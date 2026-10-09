#!/usr/bin/env python3
# SPDX-License-Identifier: MIT
"""CPU-only full-router build. No loader, VR runtime, camera or device activation."""
import argparse, json, os, pathlib, resource, signal, subprocess, sys
ROOT=pathlib.Path(__file__).resolve().parents[1]
sys.path.insert(0,str(ROOT/'runtime'))
from evidence_test import limits
SOURCES=['live/runtime.cpp','runtime/evidence.cpp','runtime/recording.cpp','runtime/staging.cpp',
         'binding/command.cpp','binding/lifecycle.cpp','binding/pass.cpp','render/adapter.cpp',
         'arena/bridge.cpp','control/protocol.cpp','control/policy.cpp','control/transport.cpp','src/aperture.cpp']
def main():
    parser=argparse.ArgumentParser();parser.add_argument('--sanitize',action='store_true')
    parser.add_argument('--vulkan-include',type=pathlib.Path,default=pathlib.Path('/home/jakeg/.hermes/work/frame-native-pipeline-review/include'))
    args=parser.parse_args();os.environ['TMPDIR']='/home/jakeg/.hermes/cache/scratch';resource.setrlimit(resource.RLIMIT_CORE,(0,0));cg,report=limits();build=ROOT/'live/build/runtime';build.mkdir(parents=True,exist_ok=True)
    flags=['g++','-std=c++17','-Wall','-Wextra','-Werror','-O1','-g','-pthread','-I'+str(args.vulkan_include),'-I'+str(ROOT/'include')]
    if args.sanitize:flags+=['-fsanitize=address,undefined','-fno-omit-frame-pointer','-no-pie']
    from native_caller import run_return
    native_lr=run_return()
    print('Pinned pthread thunk Run return:',hex(native_lr))
    flags+=['-DFT_NATIVE_RUN_RETURN='+hex(native_lr)]
    results=[]
    for test in ['runtime_test','runtime_owner_test','runtime_chain_test']:
        binary=build/(test+('-asan' if args.sanitize else ''))
        cmd=flags+(['-DFT_NATIVE_PORTAL_TEST_CALL'] if test=='runtime_chain_test' else [])+[str(ROOT/s) for s in SOURCES]+[str(ROOT/'live'/f'{test}.cpp'),'-o',str(binary)]
        for call in [cmd,[str(binary)]]:
            result=subprocess.run(call,capture_output=True,text=True,env={**os.environ,'TMPDIR':'/home/jakeg/.hermes/cache/scratch'})
            print(result.stdout+result.stderr,end='');results.append({'command':call,'code':result.returncode,'output':result.stdout+result.stderr})
            if result.returncode:
                (build/'verification.json').write_text(json.dumps(results,indent=2));return result.returncode
    modes=['--bootstrap-stock','--bootstrap-geometry','--worker-no-stock','--worker','--joined-cleanup',
           '--external','--foreign-external','--reject-graph','--stop-after-left',
           '--bad-stock-owner','--bad-stock-offset','--bad-stock-unreadable',
           '--bad-stock-resource','--bad-stock-capacity','--bad-geometry-offset',
           '--bad-geometry-unreadable','--bad-geometry-cursor']
    for mode,expected in [('--unsafe-cleanup',-signal.SIGABRT),('--join-error-cleanup',-signal.SIGABRT)]+[(m,0) for m in modes]:
        binary=build/('runtime_chain_test'+('-asan' if args.sanitize else ''))
        result=subprocess.run([str(binary),mode],capture_output=True,text=True)
        print(mode,result.stdout+result.stderr,end='\n');assert result.returncode==expected,(mode,result.returncode,result.stderr)
        if expected==-signal.SIGABRT:assert 'cleanup-entry' in result.stderr and 'terminate called' not in result.stderr
        results.append({'mode':mode,'expected_exit':expected,'code':result.returncode,'output':result.stdout+result.stderr})
    # Link every production owner/router component into one DSO, with no fixture.
    cmd=flags+['-fPIC','-shared']+[str(ROOT/s) for s in SOURCES]+[str(ROOT/'live/runtime-call.S'),'-o',str(build/'libft-native-router.so')]
    subprocess.run(cmd,check=True)
    subprocess.run(['nm','-D','--defined-only',str(build/'libft-native-router.so')],check=True,stdout=subprocess.DEVNULL)
    report.update(tests=results,memory_peak=(cg/'memory.peak').read_text().strip())
    (build/'verification.json').write_text(json.dumps(report,indent=2)+'\n');print(json.dumps(report|{'tests':len(results)}));return 0
if __name__=='__main__':raise SystemExit(main())
