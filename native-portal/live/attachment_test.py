#!/usr/bin/env python3
# SPDX-License-Identifier: MIT
"""Focused attachment regression, CPU only, under the normal 650M/no-swap cap."""
import argparse, json, os, resource, subprocess
from runtime_test import ROOT, SOURCES
from evidence_test import limits
from native_caller import run_return
from bootstrap_native import bootstrap_native

def main():
    parser=argparse.ArgumentParser();parser.add_argument('--machine',action='store_true');parser.add_argument('--sanitize',action='store_true')
    args=parser.parse_args()
    if args.machine and args.sanitize:parser.error('run machine and sanitizer separately')
    cg,report=limits();resource.setrlimit(resource.RLIMIT_CORE,(0,0))
    os.environ['TMPDIR']='/home/jakeg/.hermes/cache/scratch'
    build=ROOT/'live/build/attachment';build.mkdir(parents=True,exist_ok=True)
    b=bootstrap_native()
    (build/'pump.private.hpp').write_text('static constexpr uint64_t pump_events[][6] = {'+
        ','.join('{'+','.join(hex(v) for v in row)+'}' for row in b['pump'])+'};\n')
    flags=['g++','-std=c++17','-Wall','-Wextra','-Werror','-O1','-g','-pthread',
        '-I/home/jakeg/.hermes/work/frame-native-pipeline-review/include','-I'+str(ROOT/'include'),
        '-I'+str(build),'-DFT_NATIVE_PORTAL_TEST_CALL','-DFT_NATIVE_RUN_RETURN='+hex(run_return())]
    for key,name in [('dispatch_lrs','FT_BOOT_DISPATCH_LR'),('release_lr','FT_BOOT_RELEASE_LR'),
        ('pool_lr','FT_BOOT_POOL_LR'),('queue_lr','FT_BOOT_QUEUE_LR'),('recycle_lr','FT_RECYCLE_LR'),
        ('stock_pool_lr','FT_STOCK_POOL_LR'),('outer_lr','FT_OUTER_LR')]:
        flags+=['-D'+name+'='+hex(b[key][0] if isinstance(b[key],list) else b[key])]
    if args.sanitize:flags+=['-fsanitize=address,undefined','-fno-omit-frame-pointer','-no-pie']
    binary=build/('attachment_test-asan' if args.sanitize else 'attachment_test')
    report_path=build/('verification-asan.json' if args.sanitize else 'verification.json')
    cmd=flags+[str(ROOT/s) for s in SOURCES]+[str(ROOT/'live/attachment_test.cpp'),'-o',str(binary)]
    subprocess.run(cmd,check=True)
    report['tests']=[]
    for mode in ['partial','recover']:
        r=subprocess.run([str(binary),mode],capture_output=True,text=True,timeout=10)
        print(r.stdout+r.stderr,end='');report['tests'].append({'mode':mode,'returncode':r.returncode,'output':r.stdout+r.stderr})
        report_path.write_text(json.dumps(report,indent=2)+'\n')
        if r.returncode:return 1
    if args.machine:
        shared=build/'attachment_test.so'
        subprocess.run(cmd[:-2]+['-fPIC','-shared','-o',str(shared)],check=True)
        from attachment_machine import verify
        report['machine']=verify(shared)
        report_path.write_text(json.dumps(report,indent=2)+'\n')
        print(report['machine'])
    print('Attachment tests PASS; memory peak', (cg/'memory.peak').read_text().strip())
    return 0
if __name__=='__main__':raise SystemExit(main())
