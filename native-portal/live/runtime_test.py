#!/usr/bin/env python3
# SPDX-License-Identifier: MIT
"""CPU-only full-router build. No loader, VR runtime, camera or device activation."""
import argparse, fcntl, json, os, pathlib, resource, signal, socket, subprocess, sys
ROOT=pathlib.Path(__file__).resolve().parents[1]
sys.path.insert(0,str(ROOT/'runtime'))
from evidence_test import limits
SOURCES=['live/runtime.cpp','runtime/evidence.cpp','runtime/recording.cpp','runtime/staging.cpp',
         'binding/command.cpp','binding/lifecycle.cpp','binding/pass.cpp','render/adapter.cpp',
         'arena/bridge.cpp','control/protocol.cpp','control/policy.cpp','control/transport.cpp','src/aperture.cpp']
def main():
    parser=argparse.ArgumentParser();parser.add_argument('--sanitize',action='store_true')
    parser.add_argument('--vulkan-include',type=pathlib.Path,default=pathlib.Path('/home/jakeg/.hermes/work/frame-native-pipeline-review/include'))
    args=parser.parse_args();os.environ['TMPDIR']='/home/jakeg/.hermes/cache/scratch';resource.setrlimit(resource.RLIMIT_CORE,(0,0));cg,report=limits();build=ROOT/'live/build/runtime';build.mkdir(parents=True,exist_ok=True)
    flags=['g++','-std=c++17','-Wall','-Wextra','-Werror','-O1','-g','-pthread','-I'+str(args.vulkan_include),'-I'+str(ROOT/'include')]
    if args.sanitize:flags+=['-fsanitize=address,undefined','-fno-omit-frame-pointer','-no-pie']
    from native_caller import run_return
    native_lr=run_return()
    print('Pinned pthread thunk Run return:',hex(native_lr))
    flags+=['-DFT_NATIVE_RUN_RETURN='+hex(native_lr)]
    from bootstrap_native import bootstrap_native
    bootstrap=bootstrap_native();print('Pinned bootstrap continuations:',bootstrap)
    flags+=['-DFT_BOOT_DISPATCH_LR='+hex(bootstrap['dispatch_lrs'][0]),
            '-DFT_BOOT_RELEASE_LR='+hex(bootstrap['release_lr']),'-DFT_BOOT_POOL_LR='+hex(bootstrap['pool_lr']),
            '-DFT_BOOT_QUEUE_LR='+hex(bootstrap['queue_lr'])]
    results=[]
    for test in ['runtime_test','runtime_owner_test','runtime_chain_test']:
        binary=build/(test+('-asan' if args.sanitize else ''))
        cmd=flags+(['-DFT_NATIVE_PORTAL_TEST_CALL'] if test=='runtime_chain_test' else [])+[str(ROOT/s) for s in SOURCES]+[str(ROOT/'live'/f'{test}.cpp'),'-o',str(binary)]
        for call in [cmd,[str(binary)]]:
            result=subprocess.run(call,capture_output=True,text=True,env={**os.environ,'TMPDIR':'/home/jakeg/.hermes/cache/scratch'})
            print(result.stdout+result.stderr,end='');results.append({'command':call,'code':result.returncode,'output':result.stdout+result.stderr})
            if result.returncode:
                (build/'verification.json').write_text(json.dumps(results,indent=2));return result.returncode
    modes=['--bootstrap-turnover','--bootstrap-bad-caller','--bootstrap-bad-renderer',
           '--bootstrap-bad-generation','--bootstrap-bad-release','--bootstrap-bad-pool',
           '--bootstrap-bad-reset','--bootstrap-bad-begin',
           '--bootstrap-stock','--bootstrap-geometry','--worker-no-stock','--worker','--joined-cleanup',
           '--external','--foreign-external','--reject-graph','--stop-after-left',
           '--bad-stock-owner','--bad-stock-offset','--bad-stock-unreadable',
           '--bad-stock-resource','--bad-stock-capacity','--bad-geometry-offset',
           '--bad-geometry-unreadable','--bad-geometry-cursor']
    for mode,expected in [('--unsafe-cleanup',-signal.SIGABRT),('--join-error-cleanup',-signal.SIGABRT)]+[(m,0) for m in modes]:
        binary=build/('runtime_chain_test'+('-asan' if args.sanitize else ''))
        result=subprocess.run([str(binary),mode],capture_output=True,text=True)
        print(mode,result.stdout+result.stderr,end='\n');assert result.returncode==expected,(mode,result.returncode,result.stderr)
        if expected==-signal.SIGABRT:assert 'cleanup-entry' in result.stderr and 'terminate called' not in result.stderr
        results.append({'mode':mode,'expected_exit':expected,'code':result.returncode,'output':result.stdout+result.stderr})
    for mode,predicate,failure,stop,owners in [
        ('--diagnostics-start-renderer',13,'0x29bd18','0x29bd18','0x17'),
        ('--diagnostics-start-unreadable',12,'0x29bd18','0x29bd18','0x17'),
        ('--diagnostics-start-failed',5,'0x305260','0x29bd18','0x17'),
        ('--diagnostics-stop-unrelated',0,'0x0','0x29bd18','0x0'),
        ('--diagnostics-stop-waiting',0,'0x0','0x142620','0xf'),
        ('--diagnostics-run-lr',3,'0x0','0x0','0x1f')]:
        result=subprocess.run([str(binary),mode],capture_output=True,text=True,timeout=5)
        print(mode,result.stdout+result.stderr)
        assert result.returncode==0,(mode,result.stderr)
        lines=[line for line in result.stderr.splitlines() if line.startswith('ft-native-live ')]
        fields=dict(word.split('=',1) for word in lines[-1].split()[1:])
        assert fields.get('first_owner_pred')==str(predicate),(mode,fields)
        expected_pc='0x14e5a8' if mode=='--diagnostics-run-lr' else '0x140dbc' if predicate else '0x0'
        assert fields['first_owner_pc']==expected_pc
        assert fields['first_evidence_pc']==failure and fields['first_stop_pc']==stop,(mode,fields)
        assert fields['owner_events']==owners and fields['frames']=='0' and fields['seals']=='0'
        assert fields['uploads']=='0,0' and fields['handoff']=='0'
        assert fields['first_refusal']=='graph-ownership' and fields['first_pc']=='0x7fdbc'
        results.append({'mode':mode,'code':result.returncode,'output':result.stdout+result.stderr})
    for mode in ['--diagnostics-good','--diagnostics-bad']:
        result=subprocess.run([str(binary),mode],capture_output=True,text=True)
        print(mode,result.stdout+result.stderr)
        assert result.returncode==0,(mode,result.stderr)
        lines=[line for line in result.stderr.splitlines() if line.startswith('ft-native-live ')]
        sealed=[line for line in lines if 'seal=1 ' in line]
        assert sealed,(mode,'missing production seal diagnostics',result.stderr)
        assert len(sealed)==1,(mode,'unchanged frames must not produce log spam',sealed)
        fields=dict(word.split('=',1) for word in sealed[0].split()[1:])
        assert fields['seal_pc']=='0x1480b8' and fields['graph']=='1' and fields['initialized']=='1'
        assert fields['handoff']=='1' and fields['origin_ok']=='1' and fields['pipeline']=='1'
        assert fields['tracking']=='1' and fields['policy']=='1'
        assert fields['first_refusal']=='graph-ownership' and fields['first_pc']=='0x7fdbc'
        for key in ['first_owner_pc','first_evidence_pc','first_stop_pc']:
            assert fields[key]=='0x0',(mode,key,fields)
        assert fields['first_owner_pred']=='0'
        good=mode.endswith('good')
        for key in ['associated','idle','can_acquire','adapter','capable']:
            assert fields[key]==str(int(good)),(mode,key,fields)
        assert fields['seal_block']==('none' if good else 'association')
        assert 'renderer=' not in sealed[0] and 'device=' not in sealed[0]
        results.append({'mode':mode,'code':result.returncode,'output':result.stdout+result.stderr})
    # Real journal-style socket output, plus saturated sink: neither changes
    # stderr flags nor blocks owner callbacks or collector teardown.
    for saturated in [False,True]:
        reader,writer=socket.socketpair()
        try:
            writer.setsockopt(socket.SOL_SOCKET,socket.SO_SNDBUF,4096)
            before=fcntl.fcntl(writer.fileno(),fcntl.F_GETFL)
            if saturated:
                while True:
                    try:writer.send(b'x'*4096,socket.MSG_DONTWAIT)
                    except BlockingIOError:break
            result=subprocess.run([str(binary),'--diagnostics-good'],stdout=subprocess.PIPE,stderr=writer.fileno(),timeout=5)
            assert result.returncode==0
            assert fcntl.fcntl(writer.fileno(),fcntl.F_GETFL)==before
            writer.close();reader.settimeout(1);chunks=[]
            while True:
                part=reader.recv(4096)
                if not part:break
                chunks.append(part)
            output=b''.join(chunks)
            if not saturated:assert b'seal_block=none graph=1 initialized=1' in output,output
            results.append({'mode':'diagnostics-journal-saturated' if saturated else 'diagnostics-journal','code':0})
        finally:
            reader.close();writer.close()
    # Link every production owner/router component into one DSO, with no fixture.
    cmd=flags+['-fPIC','-shared']+[str(ROOT/s) for s in SOURCES]+[str(ROOT/'live/runtime-call.S'),'-o',str(build/'libft-native-router.so')]
    subprocess.run(cmd,check=True)
    subprocess.run(['nm','-D','--defined-only',str(build/'libft-native-router.so')],check=True,stdout=subprocess.DEVNULL)
    report.update(tests=results,memory_peak=(cg/'memory.peak').read_text().strip())
    (build/'verification.json').write_text(json.dumps(report,indent=2)+'\n');print(json.dumps(report|{'tests':len(results)}));return 0
if __name__=='__main__':raise SystemExit(main())
