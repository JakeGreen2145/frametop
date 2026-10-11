#!/usr/bin/env python3
# SPDX-License-Identifier: MIT
"""Default Runtime graph guards. Injected schedules are adapter tests, not native races."""
import argparse
import hashlib
import json
import os
from pathlib import Path
import resource
import socket
import subprocess
import sys
import threading
import uuid
ROOT=Path(__file__).resolve().parents[1]

def main():
    p=argparse.ArgumentParser(description=__doc__)
    p.add_argument('--in-scope',action='store_true');p.add_argument('--sanitize',action='store_true')
    p.add_argument('--skip-build',action='store_true');a=p.parse_args()
    if not a.in_scope:
        unit='graph-runtime-'+uuid.uuid4().hex+'.scope'
        try:return subprocess.run(['systemd-run','--user','--scope','--quiet','--unit='+unit,'-p','MemoryMax=256M','-p','MemorySwapMax=0',sys.executable,str(Path(__file__).resolve()),'--in-scope',*sys.argv[1:]],timeout=600).returncode
        except subprocess.TimeoutExpired:
            subprocess.run(['systemctl','--user','stop',unit],check=False,timeout=15);raise
    resource.setrlimit(resource.RLIMIT_CORE,(0,0))
    build=ROOT/'build/graph-runtime'/('sanitize' if a.sanitize else 'plain');build.mkdir(parents=True,exist_ok=True)
    args=[sys.executable,str(ROOT/'live/dormant_runtime_test.py'),'--in-scope','--first-consumer','--expect-refusal','--fixtures',str(ROOT/'build/graph-inputs'),'--admission-vectors',str(ROOT/'build/graph-inputs/admission-native-vectors.json'),'--factory-vectors',str(ROOT/'build/graph-inputs/SAVED-VECTORS.json'),'--resource-facts',str(ROOT/'build/graph-inputs/resource-STATIC-FACTS.json')]
    args += ['--consumer-pins',str(ROOT/'build/graph-inputs/consumer-NATIVE-PINS.json')]
    if a.sanitize:args+=['--sanitize']
    if not a.skip_build:
        with (build/'build.log').open('w') as log:subprocess.run(args,stdout=log,stderr=subprocess.STDOUT,check=True,timeout=420)
    exe=ROOT/'build/dormant-runtime/current/first-consumer'
    if a.sanitize:exe=exe/'sanitize'
    exe=exe/'dormant-runtime-test'
    guards=[0x2983c8,0x298750,0x29ce00,0x29b278,0x299a60,0x2993f8,0x2990c8,0x2988a8,0x29bd18,0x29c238,0x2a1e18,0x2a2870,0x15fcc0,0x15fba8,0x8b020]
    cases=[('action-'+x,{'FT_GRAPH_ACTION':x},None) for x in ['hold','pump','close','unmatched','alternate-return','early','disposal','unrelated','missing-return']]
    cases += [('guard-'+hex(pc),{'FT_GRAPH_ACTION':'guard-'+hex(pc)[2:]},pc) for pc in guards]
    cases += [(x,{'FT_GRAPH_ACTION':x},0x4b3d0) for x in ['free-r','free-w','free-c','free-p0','free-p1','second-free']]
    cases += [('missing-backend-return',{'FT_GRAPH_ACTION':'missing-backend-return'},0x2a2230)]
    cases += [('distinct-stock',{'FT_GRAPH_ACTION':'hold','FT_GRAPH_DISTINCT':'1','FT_GRAPH_STOCK':'1'},None)]
    cases += [(x,{'FT_GRAPH_ACTION':x},None) for x in ['retained-wrong-post','retained-duplicate-pre']]
    cases += [('provider-'+x,{'FT_GRAPH_PROVIDER':x},None) for x in ['missing','vptr','slot0','slot2','name','pointer']]
    cases += [('optimizer-stock',{'FT_GRAPH_OPTIMIZER':'1'},None)]
    results=[]
    for name,env,pc in cases:
        r,w=socket.socketpair();chunks=[]
        def drain():
            while True:
                data=r.recv(65536)
                if not data:return
                chunks.append(data)
        t=threading.Thread(target=drain);t.start()
        child=subprocess.Popen([str(exe),'consumer-flush'],stdout=w.fileno(),stderr=w.fileno(),env={**os.environ,'TMPDIR':'/home/jakeg/.hermes/cache/scratch','FT_GRAPH_PROBE':'1','FT_GRAPH_DISPOSAL':'1','ASAN_OPTIONS':'detect_leaks=1:halt_on_error=1','UBSAN_OPTIONS':'halt_on_error=1:print_stacktrace=1',**env});w.close()
        try:code=child.wait(timeout=15)
        except subprocess.TimeoutExpired:child.kill();child.wait();raise
        finally:t.join(timeout=5);r.close()
        text=b''.join(chunks).decode(errors='replace');(build/(name+'.log')).write_text(text)
        fatal=next((line for line in text.splitlines() if line.startswith('ft-camera-fatal ')),'')
        fields=dict(x.split('=',1) for x in fatal.split()[1:])
        action=env.get('FT_GRAPH_ACTION')
        if pc is not None:
            expected_reason=5 if pc==0x8b020 else 9
            passed=code==-6 and int(fields.get('reason','0'),0)==expected_reason and int(fields.get('event','0'),0)==pc and ('GRAPH conflict before-native' in text or action=='missing-backend-return') and 'GRAPH forbidden original continuation' not in text
        elif action in ('early','missing-return','retained-wrong-post','retained-duplicate-pre') or 'FT_GRAPH_OPTIMIZER' in env:
            passed=code==-6 and int(fields.get('reason','0'),0)==5 and int(fields.get('event','0'),0)==0x8b020 and fields.get('joined')=='1' and 'native_cleanup=0' in text
            if action=='early':passed=passed and 'GRAPH early last-use=1 debt-retained=1 no-settlement=1' in text
        else:
            passed=code==0 and text.count('clean-teardown=1 native_cleanup=')==2
            if 'FT_GRAPH_PROVIDER' in env:passed=passed and text.count('phase=packets')==2 and all('lookups=0' in s and 'skips=3' in s for s in text.splitlines() if 'phase=packets' in s)
        passed=passed and not any(s in text for s in ['Assertion','ERROR: AddressSanitizer','runtime error:','ERROR: LeakSanitizer','CURRENT forbidden native cleanup'])
        results.append(dict(name=name,code=code,passed=passed,fatal=fields));print(json.dumps(results[-1]),flush=True)
        (build/'results.json').write_text(json.dumps(dict(source_model=True,native_race_claim=False,memory_max='256M',swap_max=0,binary_sha256=hashlib.sha256(exe.read_bytes()).hexdigest(),cases=results),indent=2)+'\n')
    return 0 if all(c['passed'] for c in results) else 1
if __name__=='__main__':raise SystemExit(main())
