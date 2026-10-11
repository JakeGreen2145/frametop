#!/usr/bin/env python3
# SPDX-License-Identifier: MIT
"""Serial bounded CPU collector/component tests. No binary analysis or devices."""
import argparse,io,json,os,pathlib,resource,subprocess,tarfile
from stock_facts_test import ROOT,SOURCES

def main():
    ap=argparse.ArgumentParser();ap.add_argument('--red',action='store_true');ap.add_argument('--sanitize',action='store_true');a=ap.parse_args()
    resource.setrlimit(resource.RLIMIT_CORE,(0,0))
    cg=pathlib.Path('/sys/fs/cgroup')
    for line in pathlib.Path('/proc/self/cgroup').read_text().splitlines():
        if line.startswith('0::') and (cg/line[3:].lstrip('/')/'memory.max').exists():cg=cg/line[3:].lstrip('/')
    assert (cg/'memory.max').read_text().strip()=='268435456'
    assert (cg/'memory.swap.max').read_text().strip()=='0'
    build=ROOT/'runtime/build/native-stock-facts'/('red' if a.red else 'asan' if a.sanitize else 'green');build.mkdir(parents=True,exist_ok=True)
    source_root=ROOT
    if a.red:
        raw=subprocess.run(['git','archive','33439d7bc553cf3fde85179cae778d1beaed2091','native-portal'],cwd=ROOT.parent,check=True,stdout=subprocess.PIPE).stdout
        with tarfile.open(fileobj=io.BytesIO(raw)) as archive:
            for member in archive.getmembers():
                path=pathlib.PurePosixPath(member.name)
                assert not path.is_absolute() and '..' not in path.parts and path.parts[0]=='native-portal'
                if member.isdir():continue
                assert member.isfile()
                dest=build/'baseline'/path;dest.parent.mkdir(parents=True,exist_ok=True)
                stream=archive.extractfile(member);assert stream is not None
                dest.write_bytes(stream.read())
        source_root=build/'baseline/native-portal'
        (source_root/'runtime/native_stock_begin_red.cpp').write_bytes((ROOT/'runtime/native_stock_begin_red.cpp').read_bytes())
    flags=['g++','-std=c++17','-Wall','-Wextra','-Werror','-O0','-g0','-pthread','--param=ggc-min-expand=5','--param=ggc-min-heapsize=4096','-I'+str(source_root/'include'),'-DFT_STOCK_FACTS_API','-DFT_STOCK_COLLECTOR_API']
    if a.sanitize:flags+=['-fsanitize=address,undefined','-fno-omit-frame-pointer','-no-pie']
    env=dict(os.environ,TMPDIR='/home/jakeg/.hermes/cache/scratch',ASAN_OPTIONS='detect_leaks=1:halt_on_error=1',UBSAN_OPTIONS='halt_on_error=1:print_stacktrace=1')
    report={'scope':'actual collector and Evidence component; modeled coverage and native register delivery','commands':[]}
    def call(cmd):
        p=subprocess.run(cmd,text=True,stdout=subprocess.PIPE,stderr=subprocess.STDOUT,env=env)
        print(p.stdout,end='',flush=True);report['commands'].append({'argv':cmd,'returncode':p.returncode,'output':p.stdout});report['memory_peak']=(cg/'memory.peak').read_text().strip()
        (build/'results.json').write_text(json.dumps(report,indent=2)+'\n')
        if p.returncode:raise SystemExit(p.returncode)
    objects=[]
    if not a.red:
        call(['python3','-B',str(ROOT/'runtime/native_stock_witness.py'),'--output',str(build)])
        flags+=['-I'+str(build)]
    sources=SOURCES+([] if a.red else ['runtime/native_stock_facts.cpp'])
    for src in sources:
        obj=build/(src.replace('/','_')+'.o');call(flags+['-c',str(source_root/src),'-o',str(obj)]);objects.append(str(obj))
    for test in (['native_stock_begin_red'] if a.red else ['native_stock_begin_red','native_stock_facts_test','stock_facts_test','evidence_test']):
        obj=build/(test+'.o');call(flags+['-c',str(source_root/'runtime'/f'{test}.cpp'),'-o',str(obj)])
        exe=build/test;call(flags+objects+[str(obj)]+(['-Wl,--wrap=syscall'] if test in {'native_stock_facts_test','stock_facts_test'} else [])+['-o',str(exe)]);call([str(exe)])
if __name__=='__main__':main()
