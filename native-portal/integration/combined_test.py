#!/usr/bin/env python3
# SPDX-License-Identifier: MIT
"""Offline combined production chain; native operations/scopes remain synthetic."""
import argparse
import json
import os
from pathlib import Path
import subprocess
p=argparse.ArgumentParser();p.add_argument('--sanitize',action='store_true');a=p.parse_args()
r=Path(__file__).resolve().parents[1];b=r/'integration/build';b.mkdir(exist_ok=True)
cg=Path('/sys/fs/cgroup')/Path('/proc/self/cgroup').read_text().strip().split('::')[1].lstrip('/')
assert (cg/'memory.max').read_text().strip()=='681574400'
assert (cg/'memory.swap.max').read_text().strip()=='0'
os.environ['TMPDIR']=str(Path.home()/'.hermes/cache/scratch')
# Reuse only the upstream test fixture, not its synthetic settlement tests/main.
fixture,fixture_guard,_=(r/'render/upstream_test.cpp').read_text().partition('#ifndef FT_UPSTREAM_FIXTURE_ONLY')
assert fixture_guard, 'upstream fixture boundary missing'
assert 'struct UpstreamNative' in fixture and 'struct UpstreamFixture' in fixture
(b/'upstream_fixture.hpp').write_text(fixture)
flags=['-std=c++17','-Wall','-Wextra','-Werror','-g','-pthread','-I'+str(r/'render'),'-I'+str(r/'include')]
if a.sanitize:flags+=['-fsanitize=address,undefined','-fno-omit-frame-pointer','-fno-pie','-no-pie']
sources=['integration/combined_test.cpp','render/adapter.cpp','arena/bridge.cpp','binding/pass.cpp','binding/command.cpp','binding/lifecycle.cpp','src/aperture.cpp','control/protocol.cpp','control/policy.cpp','control/transport.cpp']
exe=b/('combined-sanitize' if a.sanitize else 'combined')
subprocess.run(['g++',*flags,*[str(r/x) for x in sources],'-o',str(exe)],check=True)
subprocess.run([str(exe)],check=True,env={**os.environ,'ASAN_OPTIONS':'detect_leaks=1'})
print(json.dumps({n:(cg/n).read_text().strip() for n in ['memory.max','memory.swap.max','memory.peak']}))
