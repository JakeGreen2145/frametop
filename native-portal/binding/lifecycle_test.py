#!/usr/bin/env python3
# SPDX-License-Identifier: MIT
import argparse, os, pathlib, subprocess
p=argparse.ArgumentParser();p.add_argument('--sanitize',action='store_true');a=p.parse_args()
r=pathlib.Path(__file__).resolve().parent;root=r.parent;b=r/'build';b.mkdir(exist_ok=True)
cg=pathlib.Path('/sys/fs/cgroup')/pathlib.Path('/proc/self/cgroup').read_text().strip().split('::')[1].lstrip('/')
assert (cg/'memory.max').read_text().strip()=='681574400'
assert (cg/'memory.swap.max').read_text().strip()=='0'
subprocess.run(['python3','-B',str(r/'test.py'),'--evidence','/home/jakeg/.hermes/work/frame-native-live03-completion']+(['--sanitize'] if a.sanitize else []),check=True)
# Reuse the unchanged integration fixture with only its entry-point renamed.
(b/'lifecycle_fixture.hpp').write_text((r/'test.cpp').read_text().replace('int main() {','int binding_original_main() {').replace('std::puts("binding tests passed");','std::puts("binding tests passed"); return 0;'))
cmd=['g++','-std=c++17','-Wall','-Wextra','-Werror','-Wno-misleading-indentation','-g','-pthread','-I'+str(r),'-I'+str(root/'include')]
if a.sanitize:cmd+=['-fsanitize=address,undefined','-fno-omit-frame-pointer','-fno-pie','-no-pie']
cmd += [str(r/'lifecycle_test.cpp'),str(r/'command.cpp')]
if (r/'lifecycle.cpp').exists():cmd+=[str(r/'lifecycle.cpp')]
cmd += [str(root/x) for x in ['render/adapter.cpp','src/aperture.cpp','control/protocol.cpp','control/policy.cpp']]
cmd += ['-o',str(b/'lifecycle-test')];subprocess.run(cmd,check=True)
subprocess.run([str(b/'lifecycle-test')],check=True,env={**os.environ,'ASAN_OPTIONS':'detect_leaks=1'})
subprocess.run(['python3','-B',str(r/'lifecycle_prepare.py')],check=True)
subprocess.run(['python3','-B',str(r/'lifecycle_machine.py')],check=True)
print('memory.max='+ (cg/'memory.max').read_text().strip())
print('memory.swap.max='+ (cg/'memory.swap.max').read_text().strip())
print('memory.peak='+ (cg/'memory.peak').read_text().strip())
