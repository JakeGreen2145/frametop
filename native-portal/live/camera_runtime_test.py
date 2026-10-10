#!/usr/bin/env python3
"""Real Runtime CPU integration, serial compiles capped at 256M/zero swap."""
import json, os
from pathlib import Path
import subprocess, sys, uuid, signal, resource
resource.setrlimit(resource.RLIMIT_CORE,(0,0))
ROOT=Path(__file__).resolve().parents[1]
sys.path[:0]=[str(ROOT/'live'),str(ROOT/'runtime')]
from native_caller import run_return
from bootstrap_native import bootstrap_native
from camera_idle_native import witness
from runtime_test import SOURCES
sanitize='--sanitize' in sys.argv
B=ROOT/('build/camera-runtime-asan' if sanitize else 'build/camera-runtime');B.mkdir(parents=True,exist_ok=True)
def run(cmd,check=True):
    unit='camera-runtime-'+uuid.uuid4().hex+'.scope'
    try:return subprocess.run(['systemd-run','--user','--scope','--quiet','--unit='+unit,
        '-p','MemoryMax=256M','-p','MemorySwapMax=0',*map(str,cmd)],check=check,timeout=180)
    except subprocess.TimeoutExpired:
        subprocess.run(['systemctl','--user','stop',unit],timeout=15);raise
boot=bootstrap_native()
flags=['-std=c++17','-O0','-g1','-Wall','-Wextra','-Werror','-pthread',
       '--param=ggc-min-expand=10','--param=ggc-min-heapsize=4096',
       '-I'+str(ROOT/'include'),'-I/home/jakeg/.hermes/work/frame-native-pipeline-review/include',
       '-I'+str(B),'-DFT_NATIVE_PORTAL_TEST_CALL','-DFT_NATIVE_RUN_RETURN='+hex(run_return())]
if sanitize:flags += ['-fsanitize=address,undefined','-fno-omit-frame-pointer','-fno-pie','-no-pie']
for name,key in [('BOOT_DISPATCH','dispatch_lrs'),('BOOT_RELEASE','release_lr'),('BOOT_POOL','pool_lr'),
                 ('BOOT_QUEUE','queue_lr'),('RECYCLE','recycle_lr'),('STOCK_POOL','stock_pool_lr'),('INITIAL_POOL','initial_pool_lr'),('OUTER','outer_lr')]:
    value=boot[key];flags+=['-DFT_'+name+'_LR='+hex(value[0] if isinstance(value,list) else value)]
(B/'pump.private.hpp').write_text('static constexpr uint64_t pump_events[][6] = {'+','.join('{'+','.join(hex(v) for v in row)+'}' for row in boot['pump'])+'};\n')
rows=witness()
(B/'camera.private.hpp').write_text('struct CameraTransition {uint64_t pc,before[5],after[5],result;};\nstatic constexpr CameraTransition camera_transitions[] = {'+','.join('{'+hex(t['pc'])+',{'+','.join(map(str,t['before']))+'},{'+','.join(map(str,t['after']))+'},'+str(t['result'])+'}' for t in rows['transitions'])+'};\n')
objects=[]
for src in SOURCES+['live/camera_runtime_test.cpp']:
    obj=B/(src.replace('/','_')+'.o');objects.append(obj)
    inputs=[ROOT/src,*ROOT.rglob('*.hpp'),ROOT/'live/runtime_camera.inc',ROOT/'live/runtime_chain_test.cpp']
    if not obj.exists() or any(p.stat().st_mtime>obj.stat().st_mtime for p in inputs):
        run(['g++',*flags,'-c',ROOT/src,'-o',obj])
exe=B/'camera-runtime'
run(['g++',*flags,*objects,'-o',exe])
if '--matrix' in sys.argv:
    results=[]
    for mode,expected in [('--absent',0),('--no-source',0),('--positive',0)]+[(m,-signal.SIGABRT) for m in
        ['--no-gpu','--no-cpu','--no-source-return','--foreign-debt','--partial-receipt','--timeout','--join-error']]:
        unit='camera-runtime-'+uuid.uuid4().hex+'.scope'
        result=subprocess.run(['systemd-run','--user','--scope','--quiet','--unit='+unit,
            '-p','MemoryMax=256M','-p','MemorySwapMax=0',str(exe),mode],capture_output=True,text=True,timeout=30)
        text=result.stdout+result.stderr;(B/(mode[2:]+'.log')).write_text(text)
        assert result.returncode==expected,(mode,result.returncode,text[-1500:])
        assert expected==0 or 'camera-cleanup-guard' in text,(mode,text[-1500:])
        assert 'ERROR: AddressSanitizer' not in text and 'runtime error:' not in text
        print(mode,result.returncode);results.append(dict(mode=mode,code=result.returncode,sanitize=sanitize))
    (B/'results.json').write_text(json.dumps(results,indent=2)+'\n')
elif '--build-only' not in sys.argv:run([exe,*[a for a in sys.argv[1:] if a not in ('--build-only','--sanitize')]])
