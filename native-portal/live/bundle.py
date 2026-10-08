#!/usr/bin/env python3
# SPDX-License-Identifier: MIT
"""Seal a content-addressed live trial bundle. Never writes a service drop-in."""
import argparse
import hashlib
import json
from pathlib import Path
import shutil

ROOT=Path(__file__).resolve().parents[1]
def sha(p):return hashlib.sha256(Path(p).read_bytes()).hexdigest()
def create(output,pipeline):
    output,pipeline=Path(output).resolve(),Path(pipeline).resolve()
    if ROOT/'build' not in output.parents:raise ValueError('output outside private build tree')
    report=json.loads((output/'build-report.json').read_text())
    if report['owner_sha256']!=sha(output/'libft_native_portal_live.so') or report['pipeline_sha256']!=sha(pipeline/'libft_portal_pipeline.so.1'):
        raise ValueError('build artifact changed before sealing')
    if sha(output/'sites.private.json')!=report['sites_sha256']:raise ValueError('site set changed')
    pin='schema=1\nbuild='+report['build_id']+'\nowner='+report['owner_sha256']+'\npipeline='+report['pipeline_sha256']+'\n'
    identity=hashlib.sha256(pin.encode()).hexdigest()
    target=output/('bundle-'+identity)
    artifacts={
        'libft_native_portal_live.so':output/'libft_native_portal_live.so',
        'libft_portal_pipeline.so.1':pipeline/'libft_portal_pipeline.so.1',
        'VkLayer_FT_scoped_portal.json':pipeline/'VkLayer_FT_scoped_portal.json',
        'sites.private.json':output/'sites.private.json','build-report.json':output/'build-report.json',
        'portal-shaders.json':pipeline/'portal-shaders.json',
    }
    contents={n:p.read_bytes() for n,p in artifacts.items()}
    contents['bundle.pin']=pin.encode()
    # Explicit layer discovery, not shader-directory replacement. Parent merges
    # existing preload/layer variables rather than overwriting unrelated tools.
    env={'FT_NATIVE_PORTAL_ENABLE':report['build_id'],'FT_NATIVE_PORTAL_BUNDLE_SHA256':identity,
         'FT_NATIVE_PORTAL_SECONDS':'60','LD_PRELOAD':str(target/'libft_native_portal_live.so'),
         'VK_ADD_LAYER_PATH':str(target),'VK_INSTANCE_LAYERS':'VK_LAYER_FT_scoped_portal'}
    contents['trial.env']=''.join(k+'='+v+'\n' for k,v in env.items()).encode()
    manifest={'schema':1,'elf_sha256':'9b1bd5fe4463592cbc71b2517b9ceb0a0fcd66cd6885a19dca4be0784bf6c265',
              'bundle_id':identity,'build_id':report['build_id'],
              'artifacts':{n:hashlib.sha256(data).hexdigest() for n,data in contents.items()}}
    contents['manifest.json']=(json.dumps(manifest,indent=2,sort_keys=True)+'\n').encode()
    if not target.exists():
        target.mkdir(mode=0o700)
        for name,data in contents.items():
            p=target/name;p.write_bytes(data);p.chmod(0o400)
        target.chmod(0o500)
    if target.is_symlink() or target.stat().st_mode&0o777!=0o500:raise ValueError('bundle directory mode')
    if set(p.name for p in target.iterdir())!=set(contents):raise ValueError('bundle inventory mismatch')
    for name,data in contents.items():
        p=target/name
        if p.is_symlink() or p.stat().st_mode&0o777!=0o400 or p.read_bytes()!=data:raise ValueError('immutable bundle mismatch '+name)
    (output/'bundle-path.txt').write_text(str(target)+'\n')
    return target
if __name__=='__main__':
    p=argparse.ArgumentParser(description=__doc__);p.add_argument('--output',type=Path,required=True);p.add_argument('--pipeline',type=Path,required=True)
    a=p.parse_args();print(create(a.output,a.pipeline))
