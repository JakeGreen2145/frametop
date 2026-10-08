#!/usr/bin/env python3
# SPDX-License-Identifier: MIT
"""Local CPU-only runner. Never loads a VR/Vulkan runtime or touches a device."""
import argparse,json,os,pathlib,subprocess,sys
from typing import Any
R=pathlib.Path(__file__).resolve().parent
B=R.parent/'integration'/'build'/'runtime-evidence'

def limits():
    cg=pathlib.Path('/sys/fs/cgroup')
    # Services here expose their cgroup path through /proc/self/cgroup.
    for line in pathlib.Path('/proc/self/cgroup').read_text().splitlines():
        if line.startswith('0::'):
            candidate=cg/line[3:].lstrip('/')
            if (candidate/'memory.max').exists(): cg=candidate
    result={n:(cg/n).read_text().strip() for n in ['memory.max','memory.swap.max']}
    assert result=={'memory.max':'681574400','memory.swap.max':'0'},result
    return cg,result

if __name__=='__main__':
    ap=argparse.ArgumentParser();ap.add_argument('--sanitize',action='store_true');ap.add_argument('--machine',action='store_true');a=ap.parse_args()
    cg,cap=limits();report: dict[str,Any]=dict(cap);B.mkdir(parents=True,exist_ok=True)
    env=dict(os.environ,TMPDIR='/home/jakeg/.hermes/cache/scratch')
    suffix='-asan' if a.sanitize else ''
    exe=B/('evidence-test'+suffix)
    cmd=['g++','-std=c++17','-Wall','-Wextra','-Werror','-O1','-g','-pthread','-I'+str(R.parent/'include')]
    if a.sanitize:cmd+=['-fsanitize=address,undefined','-fno-omit-frame-pointer','-no-pie']
    sources=['runtime/evidence.cpp','runtime/recording.cpp','runtime/evidence_test.cpp',
             'binding/lifecycle.cpp','binding/command.cpp','binding/pass.cpp',
             'render/adapter.cpp','arena/bridge.cpp','src/aperture.cpp',
             'control/protocol.cpp','control/policy.cpp','control/transport.cpp']
    cmd +=[str(R.parent/p) for p in sources]+['-o',str(exe)]
    report['commands']=[]
    no_layer=cmd[:-1]+[str(exe)+'-no-layer','-DEVIDENCE_NO_LAYER']
    exit_code=0
    for c in [cmd,[str(exe)],no_layer,[str(exe)+'-no-layer']] + ([[sys.executable,'-B',str(R/'evidence_machine.py')]] if a.machine else []):
        p=subprocess.run(c,text=True,stdout=subprocess.PIPE,stderr=subprocess.STDOUT,env=env)
        print(p.stdout,end='');report['commands'].append({'argv':c,'returncode':p.returncode,'output':p.stdout})
        exit_code=p.returncode
        if exit_code:break
    report['memory.peak']=(cg/'memory.peak').read_text().strip()
    (B/('verification'+suffix+'.json')).write_text(json.dumps(report,indent=2)+'\n')
    raise SystemExit(exit_code)
