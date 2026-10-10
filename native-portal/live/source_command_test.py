#!/usr/bin/env python3
# SPDX-License-Identifier: MIT
"""Actual Runtime source-debt negatives; original native calls never suppressed."""
import json,os,resource,signal,subprocess,sys,uuid
from pathlib import Path
resource.setrlimit(resource.RLIMIT_CORE,(0,0))
ROOT=Path(__file__).resolve().parents[1]
B=ROOT/'build/source-command';B.mkdir(parents=True,exist_ok=True)
sanitize='--sanitize' in sys.argv
exe=ROOT/('build/camera-runtime-asan/camera-runtime' if sanitize else 'build/camera-runtime/camera-runtime')
results=[]
for fault in ['missing','partial-eye','reverse','resource','command','family','image','frame','generation','record','barrier-api','registration','signal','failed-submit','range']:
    cmd=['systemd-run','--user','--scope','--quiet','--unit=source-command-'+uuid.uuid4().hex+'.scope',
         '-p','MemoryMax=256M','-p','MemorySwapMax=0',str(exe),'--missing-sampled-source']
    r=subprocess.run(cmd,env={**os.environ,'FT_SOURCE_FAULT':fault},capture_output=True,text=True,timeout=30)
    text=r.stdout+r.stderr;(B/(fault+('-asan' if sanitize else '')+'.log')).write_text(text)
    assert r.returncode==-signal.SIGABRT,(fault,r.returncode,text[-2000:])
    assert 'owned incomplete settlement stays pending' in text or 'camera-cleanup-guard: debt must fail-stop' in text,(fault,text[-2000:])
    assert 'camera-cleanup-guard: debt must fail-stop' in text,(fault,text[-2000:])
    assert 'ERROR: AddressSanitizer' not in text and 'runtime error:' not in text
    results.append(dict(fault=fault,code=r.returncode,exact_debt_guard=True,sanitize=sanitize))
    print(fault,'PASS pending debt, exact cleanup guard')
positive=[]
for name,mode,extra in [('reverse-eyes','--positive',{'FT_SOURCE_REVERSE_ORDER':'1'}),
                        ('mixed-stock-release','--positive',{'FT_SOURCE_STOCK_RELEASE':'1'}),
                        ('rejected-pair','--source-rejected',{})]:
    cmd=['systemd-run','--user','--scope','--quiet','-p','MemoryMax=256M','-p','MemorySwapMax=0',str(exe),mode]
    r=subprocess.run(cmd,env={**os.environ,**extra},capture_output=True,text=True,timeout=30)
    text=r.stdout+r.stderr;(B/(name+('-asan' if sanitize else '')+'.log')).write_text(text)
    assert r.returncode==0,(name,text[-2000:])
    assert 'ERROR: AddressSanitizer' not in text and 'runtime error:' not in text
    positive.append(dict(name=name,code=0,sanitize=sanitize));print(name,'PASS')
(B/('positive-asan-results.json' if sanitize else 'positive-results.json')).write_text(json.dumps(positive,indent=2)+'\n')
(B/('negative-asan-results.json' if sanitize else 'negative-results.json')).write_text(json.dumps(results,indent=2)+'\n')
print('PASS',len(results),'source attribution negative cases through actual Runtime')
