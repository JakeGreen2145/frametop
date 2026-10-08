#!/usr/bin/env python3
"""Local CPU-only build. Must run under the documented memory-limited scope."""
import hashlib,json,os,pathlib,subprocess,sys
R=pathlib.Path(__file__).resolve().parent;B=R/'build'
def limits():
    line=next(x for x in pathlib.Path('/proc/self/cgroup').read_text().splitlines() if x.startswith('0::'))
    cg=pathlib.Path('/sys/fs/cgroup')/line[3:].lstrip('/')
    result={n:(cg/n).read_text().strip() for n in ('memory.max','memory.swap.max','memory.peak')}
    assert result['memory.max']=='681574400' and result['memory.swap.max']=='0',result
    return result
limits();os.umask(0o077);B.mkdir(exist_ok=True)
os.environ['TMPDIR']=str(pathlib.Path.home()/'.hermes/cache/scratch')
sys.dont_write_bytecode=True
# Reuse already-provisioned offline analysis dependencies. No network/install/runtime.
WORK=pathlib.Path('/home/jakeg/.hermes/work')
paths=[WORK/'frame-native-reuse/camera-analysis/pylib',WORK/'frame-native-product-design/pylib']
sys.path[:0]=[str(p) for p in paths]
os.environ['PYTHONPATH']=os.pathsep.join(map(str,paths))
import prepare
pins=prepare.generate()
red=subprocess.run([sys.executable,'-B',str(R/'test_machine.py'),'--red'],capture_output=True,text=True,timeout=90)
(B/'red.log').write_text(red.stdout+red.stderr)
assert red.returncode==1 and 'FAILED (failures=2)' in red.stderr, red.stdout+red.stderr
print('Original-instruction RED controls: both required behaviors fail as expected',flush=True)
def run(args,**kw):
    print('$',' '.join(map(str,args)),flush=True)
    return subprocess.run(list(map(str,args)),cwd=R,check=True,timeout=90,**kw)
flags=['-std=c++17','-Wall','-Wextra','-Werror','-O1','-g','-fno-omit-frame-pointer']
for name,src in [('policy',['test_policy.cpp']),('bridge',['test_bridge.cpp','bridge.cpp'])]:
    run(['g++',*flags,'-fsanitize=address,undefined','-no-pie',*src,'-o',B/name]);run([B/name])
run(['g++',*flags,'-fPIC','-shared','bridge.cpp','test_abi.cpp','-o',B/'test_bridge.so'])
run(['cc','-std=c11','-O2','-Wall','-Wextra','-Werror','-Wno-misleading-indentation','emit.c','-o',B/'emit'])
with (B/'emitted.json').open('w') as f:run([B/'emit',pins],stdout=f)
run([sys.executable,'-B','test_machine.py'])
run([sys.executable,'-B','test_preservation.py'])
report={'elf_sha256':prepare.PIN,'limits':limits(),'source_hashes':{p.name:hashlib.sha256(p.read_bytes()).hexdigest() for p in R.iterdir() if p.is_file()}}
(B/'verification.json').write_text(json.dumps(report,indent=2)+'\n')
print(json.dumps(report['limits']))
