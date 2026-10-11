#!/usr/bin/env python3
# SPDX-License-Identifier: MIT
"""Source-only CPU runner; no analysis-runner imports, devices or installs."""
import argparse,ast,json,os,pathlib,resource,subprocess,sys
ROOT=pathlib.Path(__file__).resolve().parents[1]
SOURCES=['runtime/evidence.cpp','runtime/recording.cpp','runtime/staging.cpp','binding/lifecycle.cpp','binding/command.cpp','binding/pass.cpp','render/adapter.cpp','arena/bridge.cpp','src/aperture.cpp','control/protocol.cpp','control/policy.cpp','control/transport.cpp']
def main():
    ap=argparse.ArgumentParser();ap.add_argument('--baseline',action='store_true');ap.add_argument('--sanitize',action='store_true');ap.add_argument('--runtime-smoke',action='store_true');ap.add_argument('--vulkan-include',type=pathlib.Path,default=pathlib.Path('/home/jakeg/.hermes/work/frame-native-pipeline-review/include'));a=ap.parse_args()
    resource.setrlimit(resource.RLIMIT_CORE,(0,0))
    cg=pathlib.Path('/sys/fs/cgroup')
    for line in pathlib.Path('/proc/self/cgroup').read_text().splitlines():
        if line.startswith('0::') and (cg/line[3:].lstrip('/')/'memory.max').exists():cg=cg/line[3:].lstrip('/')
    assert (cg/'memory.max').read_text().strip()=='268435456'
    assert (cg/'memory.swap.max').read_text().strip()=='0'
    suffix='red' if a.baseline else 'asan' if a.sanitize else 'green'
    build=ROOT/'runtime/build/stock-facts'/suffix;build.mkdir(parents=True,exist_ok=True)
    env=dict(os.environ,TMPDIR='/home/jakeg/.hermes/cache/scratch',ASAN_OPTIONS='detect_leaks=1:halt_on_error=1',UBSAN_OPTIONS='halt_on_error=1:print_stacktrace=1')
    flags=['g++','-std=c++17','-Wall','-Wextra','-Werror','-O0','-g0','-pthread','--param=ggc-min-expand=5','--param=ggc-min-heapsize=4096','-I'+str(ROOT/'include')]
    if not a.baseline:flags+=['-DFT_STOCK_FACTS_API','-DFT_STOCK_COLLECTOR_API']
    if a.sanitize:flags+=['-fsanitize=address,undefined','-fno-omit-frame-pointer','-no-pie']
    report={'scope':'actual Evidence/Adapter source models, no native ABI/hardware proof','commands':[]}
    def call(cmd):
        p=subprocess.run(cmd,text=True,stdout=subprocess.PIPE,stderr=subprocess.STDOUT,env=env)
        print(p.stdout,end='',flush=True);report['commands'].append({'argv':cmd,'returncode':p.returncode,'output':p.stdout})
        report['memory_peak']=(cg/'memory.peak').read_text().strip();(build/'results.json').write_text(json.dumps(report,indent=2)+'\n')
        if p.returncode:raise SystemExit(p.returncode)
    objects=[]
    for src in SOURCES:
        obj=build/(src.replace('/','_')+'.o');call(flags+['-c',str(ROOT/src),'-o',str(obj)]);objects.append(str(obj))
    for test in ['stock_facts_test','evidence_test']:
        obj=build/(test+'.o');call(flags+['-c',str(ROOT/'runtime'/f'{test}.cpp'),'-o',str(obj)])
        exe=build/test;call(flags+objects+[str(obj)]+(['-Wl,--wrap=syscall'] if test=='stock_facts_test' else [])+['-o',str(exe)])
        call([str(exe)])
    obj=build/'evidence-no-layer.o';call(flags+['-DEVIDENCE_NO_LAYER','-c',str(ROOT/'runtime/evidence_test.cpp'),'-o',str(obj)])
    exe=build/'evidence-no-layer';call(flags+objects+[str(obj),'-o',str(exe)]);call([str(exe)])
    if a.runtime_smoke:
        # Source-list parsing only. Never import runtime_test's analysis runners.
        tree=ast.parse((ROOT/'live/runtime_test.py').read_text())
        sources=next(ast.literal_eval(n.value) for n in tree.body if isinstance(n,ast.Assign) and any(isinstance(t,ast.Name) and t.id=='SOURCES' for t in n.targets))
        extra=[];runtime_flags=flags+['-I'+str(a.vulkan_include)]
        for src in sources:
            if src in SOURCES:continue
            obj=build/(src.replace('/','_')+'.o');call(runtime_flags+['-c',str(ROOT/src),'-o',str(obj)]);extra.append(str(obj))
        obj=build/'runtime_test.o';call(runtime_flags+['-c',str(ROOT/'live/runtime_test.cpp'),'-o',str(obj)])
        exe=build/'runtime-smoke';call(runtime_flags+objects+extra+[str(obj),'-o',str(exe)]);call([str(exe)])
if __name__=='__main__':main()
