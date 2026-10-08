#!/usr/bin/env python3
# SPDX-License-Identifier: MIT
"""Offline bounded production tests. No VR, deployment, or package installation."""
import argparse
import json
import os
from pathlib import Path
import subprocess

p=argparse.ArgumentParser()
p.add_argument('--sanitize',action='store_true')
a=p.parse_args()
root=Path(__file__).resolve().parents[1]
build=root/'binding/build/pass'
build.mkdir(parents=True,exist_ok=True)
cg=Path('/sys/fs/cgroup')/Path('/proc/self/cgroup').read_text().strip().split('::')[1].lstrip('/')
assert (cg/'memory.max').read_text().strip()=='681574400'
assert (cg/'memory.swap.max').read_text().strip()=='0'
cmd=['g++','-std=c++17','-Wall','-Wextra','-Werror','-g','-pthread','-I'+str(root/'include')]
if a.sanitize: cmd+=['-fsanitize=address,undefined','-fno-omit-frame-pointer','-fno-pie','-no-pie']
sources=['binding/pass_test.cpp','render/adapter.cpp','arena/bridge.cpp','src/aperture.cpp','control/protocol.cpp','control/policy.cpp']
if (root/'binding/pass.cpp').exists(): sources+=['binding/pass.cpp']
out=build/('test-sanitize' if a.sanitize else 'test')
subprocess.run(cmd+[str(root/x) for x in sources]+['-o',str(out)],check=True)
subprocess.run([str(out)],check=True,env={**os.environ,'ASAN_OPTIONS':'detect_leaks=1'})
print(json.dumps({n:(cg/n).read_text().strip() for n in ['memory.max','memory.swap.max','memory.peak']}))
