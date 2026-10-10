#!/usr/bin/env python3
# SPDX-License-Identifier: MIT
"""Source-only Runtime tests using saved opaque headers and coordinates.

Does not import or execute native evidence generators. Every compiler/test runs
inside one verified 256 MiB, zero-swap cgroup. Outputs stay in owned scratch.
"""
import argparse
import ast
import json
import os
from pathlib import Path
import resource
import socket
import signal
import subprocess
import sys
import uuid

ROOT = Path(__file__).resolve().parents[1]
SCRATCH = '/home/jakeg/.hermes/cache/scratch'

def main():
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument('--coordinates', type=Path, required=True)
    p.add_argument('--fixtures', type=Path, required=True)
    p.add_argument('--vulkan-include', type=Path, required=True)
    p.add_argument('--build', type=Path, required=True)
    p.add_argument('--limited', action='store_true')
    p.add_argument('--reuse', action='store_true')
    p.add_argument('--sanitize', action='store_true')
    p.add_argument('--test', default='initial_binding_test')
    p.add_argument('--camera-matrix', action='store_true')
    p.add_argument('--modes', nargs='+', default=['initial-success','init-failure','foreign-stock','worker-stock','late','mismatch','nested-cleanup','cleanup-between','concurrent-cleanup','blocked-stock','aba','start-aba','exhausted','partial-bind','active-rebind','unclassified-init','repeated-init','bad-publication','rejected-run','replacement-malformed','bound-publication'])
    a = p.parse_args()
    if a.camera_matrix:
        a.test='camera_runtime_test'
        a.modes=['--absent','--no-source','--no-source-cleanup','--provider-unused','--missing-arbitration',
                 '--stock-only-multicaller','--positive','--no-gpu','--no-cpu','--no-source-return','--foreign-debt','--partial-receipt']
    if not a.limited:
        unit = 'source-initial-' + uuid.uuid4().hex
        cmd = ['systemd-run','--user','--wait','--pipe','--unit='+unit,
               '-p','MemoryMax=256M','-p','MemorySwapMax=0','--setenv=TMPDIR='+SCRATCH,
               sys.executable,str(Path(__file__).resolve()),*sys.argv[1:],'--limited']
        try:
            return subprocess.run(cmd, timeout=480).returncode
        except subprocess.TimeoutExpired:
            subprocess.run(['systemctl','--user','stop',unit+'.service'],timeout=15,check=False)
            raise
    resource.setrlimit(resource.RLIMIT_CORE,(0,0))
    cg = Path('/sys/fs/cgroup') / Path('/proc/self/cgroup').read_text().split('0::',1)[1].strip().lstrip('/')
    assert (cg/'memory.max').read_text().strip()==str(256*1024*1024)
    assert (cg/'memory.swap.max').read_text().strip()=='0'
    os.environ['TMPDIR']=SCRATCH
    assert a.build.resolve().is_relative_to(Path(SCRATCH).resolve()), 'build must stay in owned scratch'
    a.build.mkdir(parents=True,exist_ok=True)
    tree=ast.parse((ROOT/'live/runtime_test.py').read_text())
    sources=next(ast.literal_eval(n.value) for n in tree.body if isinstance(n,ast.Assign) and any(isinstance(t,ast.Name) and t.id=='SOURCES' for t in n.targets))
    coords=json.loads(a.coordinates.read_text());boot=coords['bootstrap']
    flags=['-std=c++17','-O0','-g1','-Wall','-Wextra','-Werror','-pthread','-no-pie',
           '--param=ggc-min-expand=10','--param=ggc-min-heapsize=4096',
           '-I'+str(ROOT/'include'),'-I'+str(a.fixtures),'-I'+str(a.vulkan_include),
           '-DFT_NATIVE_PORTAL_TEST_CALL','-DFT_NATIVE_RUN_RETURN='+hex(coords['run_return'])]
    for name,key in [('BOOT_DISPATCH','dispatch_lrs'),('BOOT_RELEASE','release_lr'),('BOOT_POOL','pool_lr'),('BOOT_QUEUE','queue_lr'),('RECYCLE','recycle_lr'),('STOCK_POOL','stock_pool_lr'),('INITIAL_POOL','initial_pool_lr'),('OUTER','outer_lr')]:
        value=boot[key];flags+=['-DFT_'+name+'_LR='+hex(value[0] if isinstance(value,list) else value)]
    if a.sanitize:
        os.environ['ASAN_OPTIONS']='detect_leaks=1:halt_on_error=1'
        os.environ['UBSAN_OPTIONS']='halt_on_error=1:print_stacktrace=1'
        flags+=['-fsanitize=address,undefined','-fno-omit-frame-pointer']
    objects=[]
    for name in sources+['live/'+a.test+'.cpp']:
        assert not Path(name).is_absolute() and '..' not in Path(name).parts
        obj=a.build/(name.replace('/','_')+'.o');objects.append(str(obj))
        # Reuse is explicitly local iterative compilation, not a freshness proof.
        if not a.reuse or name in ['live/runtime.cpp','binding/camera.cpp','control/camera_gate.cpp','live/camera_owner.cpp','live/'+a.test+'.cpp']:
            subprocess.run(['g++',*flags,'-c',str(ROOT/name),'-o',str(obj)],check=True,timeout=90)
    exe=a.build/a.test
    subprocess.run(['g++',*flags,*objects,'-o',str(exe)],check=True,timeout=60)
    rows=[]
    for mode in a.modes:
        import threading
        left,right=socket.socketpair();chunks=[];overflow=threading.Event()
        def drain():
            total=0
            while True:
                data=left.recv(65536)
                if not data:break
                total+=len(data)
                if total<=16*1024*1024:chunks.append(data)
                else:overflow.set()
        reader=threading.Thread(target=drain);reader.start()
        try:
            r=subprocess.run([str(exe),mode],stdout=subprocess.PIPE,stderr=right.fileno(),text=True,timeout=25)
            right.close();reader.join(timeout=3);assert not reader.is_alive()
            assert not overflow.is_set(), 'diagnostic capture exceeded bounded buffer'
            r.stderr=b''.join(chunks).decode()
        finally:
            right.close();reader.join(timeout=3);left.close()
        (a.build/(a.test+'-'+mode+'.log')).write_text(r.stdout+r.stderr)
        expected_abort=(a.test=='camera_runtime_test' and mode in ['--no-gpu','--no-cpu','--no-source-return','--foreign-debt','--partial-receipt']) or (a.test=='initial_binding_test' and mode in ['partial-bind','bound-publication']) or (a.test=='production_admission_test' and mode in ['partial-connect','partial-alias','missing-event','provider-migration','provider-loss','association-migration','bad-selection','bad-selection-joined'])
        passed=r.returncode==(-signal.SIGABRT if expected_abort else 0)
        if expected_abort:
            fatals=[line for line in r.stderr.splitlines() if line.startswith('ft-camera-fatal ')]
            passed=passed and len(fatals)==1 and 'unexpected-cleanup-return' not in r.stderr and 'forbidden-native-cleanup' not in r.stderr
            if mode in ['partial-bind','bound-publication']:
                record=dict(word.split('=') for word in fatals[0].split()[1:]) if fatals else {}
                passed=passed and int(record.get('reason','0'),0)==(8 if mode=='partial-bind' else 2)
                binding=[dict(w.split('=') for w in line.split()[1:]) for line in r.stderr.splitlines() if line.startswith('ft-camera-binding ')]
                wanted=({'partial':1,'runtime_bound':0,'internal_bound':1,'epoch_published':1,'barrier_retained':0,'join_attempts':1,'first_failure':12} if mode=='partial-bind' else {'publication_accepted':0,'publication_rejection':12,'publication_sequence':2,'runtime_bound':1})
                passed=passed and bool(binding) and all(int(binding[-1].get(k,'-1'),0)==v for k,v in wanted.items())
            else:
                record=dict(word.split('=') for word in fatals[0].split()[1:]) if fatals else {}
                passed=passed and int(record.get('reason','0'),0)==(9 if mode=='--foreign-debt' else 5)
        rows.append({'mode':mode,'exit':r.returncode,'passed':passed,'stdout':r.stdout,'stderr':r.stderr})
        print(mode,r.returncode,'PASS' if passed else 'FAIL',r.stdout, r.stderr[-1200:] if not passed else '',flush=True)
    (a.build/(a.test+'-results.json')).write_text(json.dumps(rows,indent=2)+'\n')
    return int(any(not r['passed'] for r in rows))

if __name__=='__main__':
    sys.exit(main())
