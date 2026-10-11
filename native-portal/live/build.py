#!/usr/bin/env python3
# SPDX-License-Identifier: MIT
"""Build the real launch owner. Does not install or activate anything."""
import argparse
import hashlib
import json
import os
from pathlib import Path
import platform
import subprocess
import sys
import prepare

ROOT=Path(__file__).resolve().parents[1]
FLAGS=['-O2','-g','-Wall','-Wextra','-Werror','-Wno-misleading-indentation','-Wno-unused-function',
       '-Wno-unused-variable','-fPIC','-fvisibility=hidden','-fno-stack-protector','-mno-outline-atomics']
BASE_SOURCES=['render/adapter.cpp','arena/bridge.cpp','binding/pass.cpp','binding/command.cpp','binding/lifecycle.cpp',
              'src/aperture.cpp','control/protocol.cpp','control/policy.cpp','control/transport.cpp',
              'live/source_provider.cpp']

def run(args):
    print('$',' '.join(map(str,args)),flush=True)
    subprocess.run(list(map(str,args)),check=True,timeout=180)

def hash_file(path):return hashlib.sha256(Path(path).read_bytes()).hexdigest()

def inventory():
    # Include new production graph/router TUs but never fixture entrypoints.
    sources=list(BASE_SOURCES)
    for directory,pattern in [('runtime','*.cpp'),('live','runtime*.cpp'),('live','runtime*.S')]:
        for path in sorted((ROOT/directory).glob(pattern)):
            if any(token in path.stem for token in ('test','machine','fixture','stub')) or path.stem.endswith('_red'):continue
            sources.append(str(path.relative_to(ROOT)))
    if 'live/runtime.cpp' not in sources:raise ValueError('missing real live/runtime.cpp; test stubs are forbidden in production')
    return sources

def source_hashes(sources,manifest):
    tracked=[ROOT/s for s in sources]+[ROOT/'live/loader.c',ROOT/'live/loader_emit.c',ROOT/'live/runtime-emit.c',
             ROOT/'arena/emit.c',ROOT/'binding/pass_emit.c',ROOT/'binding/lifecycle_emit.c',ROOT/'runtime/graph.inc',ROOT/'live/runtime_camera.inc',ROOT/'live/source_router.inc',ROOT/'live/source_loader.inc',ROOT/'live/source_metadata.inc',ROOT/'live/source_command.inc',manifest]
    tracked += [p for p in ROOT.rglob('*') if p.suffix in ('.h','.hpp') and 'build' not in p.relative_to(ROOT).parts]
    tracked += [ROOT/'live'/n for n in ('prepare.py','build.py','bundle.py')]
    return {str(p.relative_to(ROOT)) if ROOT in p.parents else str(p):hash_file(p) for p in tracked}

def pipeline_check(directory):
    p=Path(directory).resolve();lib=p/'libft_portal_pipeline.so.1'
    manifest=json.loads((p/'portal-shaders.json').read_text())
    pins=json.loads((ROOT/'tools/native-shaders.json').read_text())
    if manifest['pairs']!=pins['pairs'] or set(manifest['shaders'])!=set(pins['shaders']):raise ValueError('not full production shader bundle')
    for ident,s in manifest['shaders'].items():
        if s['original_sha256']!=pins['shaders'][ident]['sha256']:raise ValueError('native shader pin mismatch')
        if hash_file(p/(ident+'.original.spv'))!=s['original_sha256'] or hash_file(p/s['file'])!=s['portal_sha256']:
            raise ValueError('pipeline shader artifact mismatch')
    artifacts=json.loads((p/'artifacts.json').read_text())
    if artifacts.get(lib.name)!=hash_file(lib):raise ValueError('pipeline build artifact hash mismatch')
    return lib

def build(a):
    if platform.machine()!='aarch64':raise ValueError('production build must run on ARM64 inside the isolated dev container')
    output=a.output.resolve()
    if ROOT/'build' not in output.parents:raise ValueError('output must be below native-portal/build')
    sources=inventory()
    if not a.manifest.is_file():raise ValueError('missing real router runtime-sites.json')
    # Optional source production list is additive and path checked, never a stub escape hatch.
    requested=json.loads(a.manifest.read_text()).get('sources',[])
    for name in requested:
        p=(ROOT/name).resolve()
        if ROOT not in p.parents or p.suffix not in ('.cpp','.S') or any(t in p.stem for t in ('test','stub','fixture')):
            raise ValueError('invalid runtime production source')
        if name not in sources:sources.append(name)
    pipeline=pipeline_check(a.pipeline)
    prepared=prepare.prepare(a.elf,output,a.manifest,a.client)
    hashes=source_hashes(sources,a.manifest)
    identity={'schema':1,'sources':hashes,'sites_sha256':hash_file(output/'sites.private.json'),'pipeline_sha256':hash_file(pipeline)}
    build_id=hashlib.sha256(json.dumps(identity,sort_keys=True,separators=(',',':')).encode()).hexdigest()
    (output/'loader-build.h').write_text('#define FT_BUILD_ID "'+build_id+'"\n#define FT_PIPELINE_SHA "'+hash_file(pipeline)+'"\n')
    objects=[]
    for i in range(4):
        obj=output/f'emit-{i}.o';objects.append(obj)
        run(['cc','-std=c11',*FLAGS,'-DFT_EMITTER='+str(i),'-c',ROOT/'live/loader_emit.c','-o',obj])
    obj=output/'runtime-emit.o';objects.append(obj)
    run(['cc','-std=c11',*FLAGS,'-c',ROOT/'live/runtime-emit.c','-o',obj])
    # Explicitly separate acceptance executable, never included in owner objects.
    run(['cc','-std=c11',*FLAGS,'-I',output,ROOT/'live/loader_test.c',*objects,'-lcrypto','-ldl','-o',output/'loader-test'])
    run([output/'loader-test',a.elf])
    obj=output/'loader.o';objects.append(obj)
    run(['cc','-std=c11',*FLAGS,'-I',output,'-c',ROOT/'live/loader.c','-o',obj])
    for i,source in enumerate(sources):
        obj=output/f'owner-{i}.o';objects.append(obj)
        compiler=['cc'] if source.endswith('.S') else ['c++','-std=c++17']
        run([*compiler,*FLAGS,'-pthread','-I',ROOT/'include','-I',output,'-c',ROOT/source,'-o',obj])
    owner=output/'libft_native_portal_live.so'
    run(['c++','-shared','-static-libstdc++','-static-libgcc','-Wl,-z,defs,-z,relro,-z,now,-z,noexecstack',
         '-Wl,-Bsymbolic-functions','-Wl,-rpath,$ORIGIN','-o',owner,*objects,'-Wl,--no-as-needed',pipeline,'-Wl,--as-needed','-pthread','-lcrypto','-ldl'])
    syms=subprocess.check_output(['nm','-D','--defined-only',owner],text=True)
    if ' ft_native_portal_event\n' not in syms:raise ValueError('real router event ABI is not exported')
    run(['readelf','-d',owner])
    report={**identity,'build_id':build_id,'owner_sha256':hash_file(owner),'compiled_sources':['live/loader.c','live/loader_emit.c','live/runtime-emit.c',*sources],'site_count':len(prepared['sites'])+len(prepared['client_sites']),'module_site_counts':{'compositor':len(prepared['sites']),'client':len(prepared['client_sites'])}}
    (output/'build-report.json').write_text(json.dumps(report,indent=2,sort_keys=True)+'\n')
    import bundle
    target=bundle.create(output,a.pipeline)
    linked=subprocess.run(['ldd','-r',str(target/'libft_native_portal_live.so')],text=True,capture_output=True,check=True)
    print(linked.stdout,linked.stderr)
    (output/'link-check.txt').write_text(linked.stdout+linked.stderr)
    if 'not found' in linked.stdout+linked.stderr or 'undefined symbol' in linked.stdout+linked.stderr:
        raise ValueError('sealed owner has unresolved production dependencies')
    print('BUNDLE',target)
    print((target/'trial.env').read_text())

if __name__=='__main__':
    p=argparse.ArgumentParser(description=__doc__)
    p.add_argument('--elf',type=Path,default=prepare.ELF)
    p.add_argument('--client',type=Path,default=prepare.CLIENT_ELF)
    p.add_argument('--manifest',type=Path,default=ROOT/'live/runtime-sites.json')
    p.add_argument('--output',type=Path,default=ROOT/'build/live-owner')
    p.add_argument('--pipeline',type=Path,required=True)
    build(p.parse_args())
