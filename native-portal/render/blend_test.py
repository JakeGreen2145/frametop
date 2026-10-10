#!/usr/bin/env python3
# SPDX-License-Identifier: MIT
"""Focused actual callback + Adapter regression, 256 MiB and zero swap.
The source extraction isolates exact production function bodies, not a copied
policy. The surrounding owner shell and external native operations are modeled.
"""
import argparse
import hashlib
import json
import os
from pathlib import Path
import resource
import subprocess
from blend_native import witness,ROOT

def function(text,signature):
    start=text.index(signature);brace=text.index('{',start);depth=1;end=brace+1
    while depth:
        depth+=(text[end]=='{')-(text[end]=='}');end+=1
    return text[start:end]+'\n'

def main():
    parser=argparse.ArgumentParser();parser.add_argument('--sanitize',action='store_true')
    a=parser.parse_args();resource.setrlimit(resource.RLIMIT_CORE,(0,0))
    cg=Path('/sys/fs/cgroup')/next(l[3:] for l in Path('/proc/self/cgroup').read_text().splitlines() if l.startswith('0::')).lstrip('/')
    limits={n:(cg/n).read_text().strip() for n in ('memory.max','memory.swap.max')}
    assert limits=={'memory.max':'268435456','memory.swap.max':'0'},limits
    build=ROOT/'render/build/blend';build.mkdir(parents=True,exist_ok=True)
    native=witness(build)
    live=(ROOT/'live/runtime.cpp').read_text();evidence=(ROOT/'runtime/evidence.cpp').read_text()
    bodies={
        'stock':function(live,'static bool stock('),
        'identity':function(live,'bool same(const Identity &'),
        'read_self':function(evidence,'bool bounded(')+function(evidence,'bool read_self('),
    }
    for name,body in bodies.items():(build/(name+'.private.inc')).write_text(body)
    flags=['g++','-std=c++17','-O0','-g1','-Wall','-Wextra','-Werror','-pthread','-I'+str(ROOT/'include'),'-I'+str(build)]
    if a.sanitize:flags+=['-fsanitize=address,undefined','-fno-omit-frame-pointer','-fno-pie','-no-pie']
    binary=build/('blend-asan' if a.sanitize else 'blend')
    sources=['render/blend_test.cpp','render/adapter.cpp','arena/bridge.cpp','src/aperture.cpp','control/protocol.cpp','control/policy.cpp','control/transport.cpp']
    cmd=flags+[str(ROOT/s) for s in sources]+['-o',str(binary)]
    env=dict(os.environ,TMPDIR=str(Path.home()/'.hermes/cache/scratch'))
    subprocess.run(cmd,check=True,env=env)
    result=subprocess.run([str(binary),str(build/'native.private.bin')],text=True,stdout=subprocess.PIPE,stderr=subprocess.STDOUT,env=env)
    print(result.stdout,end='')
    report=dict(limits=limits,memory_peak=(cg/'memory.peak').read_text().strip(),
        source_sha256={s:hashlib.sha256((ROOT/s).read_bytes()).hexdigest() for s in sources+['live/runtime.cpp','runtime/evidence.cpp']},
        body_sha256={k:hashlib.sha256(v.encode()).hexdigest() for k,v in bodies.items()},
        native_cases=len(native['cases']),command=cmd,returncode=result.returncode,output=result.stdout)
    path=build/('verification-asan.json' if a.sanitize else 'verification.json')
    path.write_text(json.dumps(report,indent=2)+'\n')
    print('report',path,'memory peak',report['memory_peak'])
    return result.returncode
if __name__=='__main__':raise SystemExit(main())
