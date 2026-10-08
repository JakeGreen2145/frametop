#!/usr/bin/env python3
# SPDX-License-Identifier: MIT
"""Inactive production integration, CPU-only; run with MemoryMax=650M, zero swap."""
import argparse,hashlib,json,os,pathlib,subprocess,sys
R=pathlib.Path(__file__).resolve().parents[1]
B=R/'render/build'
p=argparse.ArgumentParser();p.add_argument('--sanitize',action='store_true');p.add_argument('--trace',type=pathlib.Path);p.add_argument('--machine',action='store_true');a=p.parse_args()
def limits():
 line=next(x for x in pathlib.Path('/proc/self/cgroup').read_text().splitlines() if x.startswith('0::'))
 cg=pathlib.Path('/sys/fs/cgroup')/line[3:].lstrip('/')
 values={n:(cg/n).read_text().strip() for n in ('memory.max','memory.swap.max','memory.peak')}
 assert values['memory.max']=='681574400' and values['memory.swap.max']=='0',values
 return values
limits();B.mkdir(exist_ok=True);os.environ['TMPDIR']=str(pathlib.Path.home()/'.hermes/cache/scratch')
logs=[]
def run(args,name):
 result=subprocess.run(list(map(str,args)),cwd=R,capture_output=True,text=True,timeout=180)
 text=result.stdout+result.stderr;(B/(name+'.log')).write_text(text);print(text,flush=True)
 logs.append({'command':list(map(str,args)),'exit':result.returncode,'log':name+'.log'})
 assert result.returncode==0,name
flags=['-std=c++17','-Wall','-Wextra','-Werror','-g','-pthread','-I'+str(R/'include')]
sources=[R/x for x in ('render/adapter.cpp','arena/bridge.cpp','src/aperture.cpp','control/protocol.cpp','control/policy.cpp','control/transport.cpp')]
args=[sys.executable,'-B',R/'render/test.py']
if a.sanitize:args+=['--sanitize']
if a.trace:args+=['--trace',a.trace]
run(args,'upstream-existing'+('-asan' if a.sanitize else ''))
sanitizer=['-fsanitize=address,undefined','-fno-omit-frame-pointer','-fno-pie','-no-pie'] if a.sanitize else []
exe=B/('upstream-test-asan' if a.sanitize else 'upstream-test')
run(['g++',*flags,*sanitizer,R/'render/upstream_test.cpp',*sources,'-o',exe],'upstream-build'+('-asan' if a.sanitize else ''))
run([exe,*([B/'private-fixtures.bin'] if a.trace else [])],'upstream-cpu'+('-asan' if a.sanitize else ''))
# Preserve the original duplicate-upload witness as an explicit expected RED.
redexe=B/('upstream-red-asan' if a.sanitize else 'upstream-red')
run(['g++',*flags,*sanitizer,R/'render/upstream_red_test.cpp',*sources,'-o',redexe],'upstream-red-build'+('-asan' if a.sanitize else ''))
red=subprocess.run([str(redexe)],capture_output=True,text=True,timeout=30)
redtext=red.stdout+red.stderr
assert red.returncode!=0 and 'native camera uploads=2, late Adapter uploads=2' in redtext and 'native_uploads + f.fake.uploads == 2' in redtext,redtext
(B/('upstream-red-asan.log' if a.sanitize else 'upstream-red.log')).write_text(redtext)
print('Expected legacy RED: two stock uploads plus two late uploads, assertion fails',flush=True)
if a.machine:
 run([sys.executable,'-B',R/'arena/test.py'],'upstream-arena')
 run(['g++',*flags,'-fPIC','-shared',R/'integration/upstream_abi.cpp',*sources,'-o',B/'upstream-machine.so'],'upstream-machine-build')
 work=pathlib.Path('/home/jakeg/.hermes/work')
 os.environ['PYTHONPATH']=os.pathsep.join(str(work/x) for x in ('frame-native-reuse/camera-analysis/pylib','frame-native-product-design/pylib'))
 run([sys.executable,'-B',R/'integration/upstream_machine.py'],'upstream-machine')
report={'limits':limits(),'commands':logs,'source_sha256':{str(f.relative_to(R)):hashlib.sha256(f.read_bytes()).hexdigest() for folder in ('render','arena','integration') for f in (R/folder).iterdir() if f.is_file()}}
(B/('upstream-verification-asan.json' if a.sanitize else 'upstream-verification.json')).write_text(json.dumps(report,indent=2)+'\n')
print(json.dumps(report['limits']))
