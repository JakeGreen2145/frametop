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
            '-DFT_BOOT_QUEUE_LR='+hex(bootstrap['queue_lr']),
            '-DFT_RECYCLE_LR='+hex(bootstrap['recycle_lr']),
            '-DFT_STOCK_POOL_LR='+hex(bootstrap['stock_pool_lr']),
            '-DFT_INITIAL_POOL_LR='+hex(bootstrap['initial_pool_lr'])]
    (build/'pump.private.hpp').write_text(
        'static constexpr uint64_t pump_events[][6] = {'+
        ','.join('{'+','.join(hex(v) for v in row)+'}' for row in bootstrap['pump'])+'};\n')
    flags+=['-I'+str(build),'-DFT_OUTER_LR='+hex(bootstrap['outer_lr'])]
    results=[]
    for test in ['runtime_test','runtime_owner_test','command_trace_test','runtime_chain_test']:
        binary=build/(test+('-asan' if args.sanitize else ''))
        cmd=flags+(['-DFT_NATIVE_PORTAL_TEST_CALL'] if test=='runtime_chain_test' else [])+[str(ROOT/s) for s in SOURCES]+[str(ROOT/'live'/f'{test}.cpp'),'-o',str(binary)]
        for call in [cmd,[str(binary)]]:
            result=subprocess.run(call,capture_output=True,text=True,env={**os.environ,'TMPDIR':'/home/jakeg/.hermes/cache/scratch'})
            print(result.stdout+result.stderr,end='');results.append({'command':call,'code':result.returncode,'output':result.stdout+result.stderr})
            if result.returncode:
                (build/'verification.json').write_text(json.dumps(results,indent=2));return result.returncode
    pool_faults=['lr','pool','device','handle','delete','clear','no-reset','no-begin',
                 'reset-failed','begin-failed','reset-lr']
    from camera_idle_native import witness
    results.append({'camera_native':witness()})
    modes=['--camera-unknown-warm','--camera-busy-841','--camera-busy-842','--camera-busy-864','--construct','--construct-setter','--construct-recover']+['--construct-fault-'+str(i) for i in range(1,20)]+['--policy-native-disabled','--policy-native-special','--application-direct','--application-fallback','--main-pump','--main-pump-owner','--repeated-cycles','--repeated-reset-failed','--repeated-begin-failed',
           '--repeated-owned-reset-failed','--repeated-owned-begin-failed',
           '--repeated-owned-no-wait','--repeated-owned-no-disposal',
           '--publication-pool-live','--pool-owned-current','--pool-owned-retained']+['--publication-pool-'+s for s in pool_faults]+[
           '--bootstrap-turnover','--bootstrap-bad-caller','--bootstrap-bad-renderer',
           '--bootstrap-bad-generation','--bootstrap-bad-release','--bootstrap-bad-pool',
           '--bootstrap-bad-reset','--bootstrap-bad-begin',
           '--bootstrap-stock','--bootstrap-geometry','--worker-no-stock','--worker','--joined-cleanup',
           '--external','--foreign-external','--reject-graph','--stop-after-left',
           '--bad-stock-owner','--bad-stock-offset','--bad-stock-unreadable',
           '--bad-stock-resource','--bad-stock-capacity','--bad-geometry-offset',
           '--bad-geometry-unreadable','--bad-geometry-cursor']
    for mode,expected in [('--unsafe-cleanup',-signal.SIGABRT),('--join-error-cleanup',-signal.SIGABRT),('--timeout-cleanup',-signal.SIGABRT)]+[(m,0) for m in modes]:
        binary=build/('runtime_chain_test'+('-asan' if args.sanitize else ''))
        result=subprocess.run([str(binary),mode],capture_output=True,text=True)
        print(mode,result.stdout+result.stderr,end='\n');assert result.returncode==expected,(mode,result.returncode,result.stderr)
        if expected==-signal.SIGABRT:assert 'cleanup-entry' in result.stderr and 'terminate called' not in result.stderr
        if mode=='--camera-unknown-warm':
            samples=[dict(w.split('=',1) for w in line.split()[1:]) for line in result.stderr.splitlines()
                     if line.startswith('ft-native-live ') and 'seal=1 ' in line]
            assert samples
            sample=samples[-1]
            assert sample['camera_read']=='1' and sample['camera_cache']=='0'
            assert sample['camera_flags']=='0x0' and sample['camera_handles']=='0xa001,0xa002'
            assert sample['idle']=='0' and sample['can_acquire']=='0' and sample['idle_first']=='0'
            assert sample['ready_first_ns']=='0' and sample['lease_first_ns']=='0'
        if mode.startswith('--policy-native-'):
            lines=[line for line in result.stderr.splitlines() if line.startswith('ft-native-live ') and 'seal=1 ' in line]
            assert len(lines)==1,(mode,result.stderr)
            fields=dict(word.split('=',1) for word in lines[0].split()[1:])
            special=mode.endswith('special')
            assert fields['policy_failed']==('9' if special else '0'),fields
            assert fields['policy_seen']==('0x3bdf' if special else '0x3f97'),fields
            assert fields['policy']==('0' if special else '1'),fields
        if mode.startswith('--application-'):
            sealed=[dict(word.split('=',1) for word in line.split()[1:]) for line in result.stderr.splitlines()
                    if line.startswith('ft-native-live ') and 'seal=1 ' in line]
            assert sealed and 'second stereo queue PASS' in result.stdout,(mode,result.stderr)
            for sample in sealed:
                assert sample['assoc_failed']=='0' and sample['associated']=='1'
                fallback=mode.endswith('fallback')
                assert sample['assoc_fallback']==str(int(fallback))
                order=list(range(1,17)) if fallback else [8,1,2,3,4,12,5,6,7,13,14,15,16]
                assert int(sample['assoc_checked'],16)==int(sample['assoc_passed'],16)==sum(1<<i for i in order)
        results.append({'mode':mode,'expected_exit':expected,'code':result.returncode,'output':result.stdout+result.stderr})
    for mode,predicate,failure,stop,owners in [
        ('--diagnostics-start-renderer',13,'0x29bd18','0x29bd18','0x17'),
        ('--diagnostics-start-unreadable',12,'0x29bd18','0x29bd18','0x17'),
        ('--diagnostics-start-failed',5,'0x305260','0x29bd18','0x17'),
        ('--diagnostics-stop-unrelated',0,'0x0','0x29bd18','0x0'),
        ('--diagnostics-stop-waiting',0,'0x0','0x0','0xf'),
        ('--diagnostics-stop-same',0,'0x0','0x1426d4','0xf'),
        ('--diagnostics-stop-unknown',0,'0x0','0x0','0x0'),
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
        if stop=='0x1426d4':
            assert fields['stop_flags']=='0x35'
            assert fields['stop_lr']=='0x123456'
        assert fields['owner_events']==owners and fields['frames']=='0' and fields['seals']=='0'
        assert fields['uploads']=='0,0' and fields['handoff']=='0'
        assert fields['first_refusal']=='graph-ownership' and fields['first_pc']=='0x7fdbc'
        results.append({'mode':mode,'code':result.returncode,'output':result.stdout+result.stderr})
    for suffix,predicate in [('good',0),('alternate',0),('phase',5),('sp',6),('read',9),('owner',10),('null-vk',12),('member',13),('handle',14),('unbegun',15)]:
        mode='--publication-'+suffix
        result=subprocess.run([str(binary),mode],capture_output=True,text=True,timeout=5)
        print(mode,result.stdout+result.stderr)
        assert result.returncode==0,(mode,result.stderr)
        lines=[line for line in result.stderr.splitlines() if line.startswith('ft-native-live ')]
        fields=dict(word.split('=',1) for word in lines[-1].split()[1:])
        assert fields.get('pub_pred')==str(predicate),(mode,fields)
        if predicate:
            assert int(fields['pub_checked'],16)==(1<<(predicate+1))-2
            assert int(fields['pub_passed'],16)==(1<<predicate)-2
            assert fields['pub_phase']==('4' if suffix=='phase' else '5')
            assert fields['pub_caller']=='0x6caf0' and fields['pub_attempt']=='1' and fields['pub_entries']=='2'
            assert fields['pub_recording']=='1'
            pub_flags=int(fields['pub_flags'],16)
            assert pub_flags&3==3 and not pub_flags&24 # Run+transfer, no Lifecycle or owned graph
            assert bool(pub_flags&32)==(suffix!='sp')
            if suffix=='handle':assert pub_flags&64 and not pub_flags&128
            if suffix=='unbegun':assert pub_flags&128 and not pub_flags&64 and fields['pub_generation']=='1'
        assert fields['first_evidence_pc']==('0x2a23f4' if predicate else '0x0')
        assert fields['handoff']=='1' and fields['uploads']=='0,0' and fields['queue_success']=='0'
        results.append({'mode':mode,'code':result.returncode,'output':result.stdout+result.stderr})
    for mode in ['--diagnostics-good','--diagnostics-bad']+['--association-'+str(i) for i in [1,2,3,4,5,6,7,8,9,10,11,12,14,16,17]]:
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
        failed=int(mode.rsplit('-',1)[1]) if mode.startswith('--association-') else 0 if good else 6
        fallback=failed in [4,9,10,11,17]
        if failed==17:failed=12
        order=[8,1,2,3]+([9,10,11] if fallback else [])+[4,12,5,6,7,13,14,15,16]
        checks=order[:order.index(failed)+1] if failed else order
        checked=sum(1<<i for i in checks)
        assert fields.get('assoc_failed')==str(failed),(mode,fields)
        assert int(fields['assoc_checked'],16)==checked,(mode,fields,checks)
        assert int(fields['assoc_passed'],16)==checked-(1<<failed if failed else 0)
        assert fields['assoc_owner']==str(int(failed!=8))
        assert fields['assoc_fallback']==str(int(fallback))
        assert fields['assoc_lr']==hex(bootstrap['outer_lr']) and int(fields['assoc_frame'])>0
        for key in ['associated','idle','can_acquire','adapter','capable']:
            assert fields[key]==str(int(good)),(mode,key,fields)
        assert fields['seal_block']==('none' if good else 'association')
        assert 'renderer=' not in sealed[0] and 'device=' not in sealed[0]
        results.append({'mode':mode,'code':result.returncode,'output':result.stdout+result.stderr})
    for failed in range(1,19):
        mode='--policy-'+str(failed)
        result=subprocess.run([str(binary),mode],capture_output=True,text=True,timeout=5)
        assert result.returncode==0,(mode,result.stderr)
        lines=[line for line in result.stderr.splitlines() if line.startswith('ft-native-live ') and 'seal=1 ' in line]
        assert len(lines)==1,(mode,result.stderr)
        fields=dict(word.split('=',1) for word in lines[0].split()[1:])
        assert fields['policy']=='0' and fields['associated']=='1' and fields['assoc_failed']=='0'
        sampled=failed!=18
        assert fields['policy_sampled']==str(int(sampled))
        assert fields['policy_failed']==str(failed if sampled else 0),(mode,fields)
        assert int(fields['policy_checked'],16)==((1<<(failed+1))-2 if sampled else 0)
        assert int(fields['policy_passed'],16)==((1<<failed)-2 if sampled else 0)
        seen=0x3fb7-(0x780 if failed==7 else 1 if failed==8 else 0)+(0x40 if failed==9 else 0)
        assert int(fields['policy_seen'],16)==seen,(mode,fields)
        if failed==14:assert fields['policy_actual']=='0x1' and fields['policy_expected']=='0x0'
        if failed==15:assert fields['policy_actual']=='0x2' and fields['policy_expected']=='0x0'
        if failed==16:assert fields['policy_style']=='2,1'
        if failed==17:assert fields['policy_alpha']=='0x3f800000,0x0'
        results.append({'mode':mode,'code':0,'output':result.stdout+result.stderr})
    # Feed every independently executed same-class native caller into the actual
    # router. Cross every caller with identity/reset/Begin and owned-role faults.
    for lr in bootstrap['command_pool_lrs']:
        for mode in ['--publication-pool-live','--pool-owned-current','--pool-owned-retained']+[
                '--publication-pool-'+s for s in pool_faults]:
            result=subprocess.run([str(binary),mode],capture_output=True,text=True,timeout=5,
                                  env={**os.environ,'FT_TEST_INITIAL_POOL_LR':hex(lr)})
            assert result.returncode==0,(hex(lr),mode,result.stderr)
            if mode=='--publication-pool-live':
                rows=[dict(w.split('=',1) for w in line.split()[1:]) for line in result.stderr.splitlines()
                      if line.startswith('ft-native-command ')]
                returned=next(r for r in rows if r['pc']=='0x311648')
                assert returned['lr']==hex(lr)
                assert returned['before_member']==returned['after_member']=='1'
                assert returned['begun']=='1,0' and returned['generation']=='0,0'
                assert 'second stereo queue PASS' in result.stdout
            results.append({'pool_lr':hex(lr),'mode':mode,'code':0})
    for lr in bootstrap['excluded_pool_lrs']:
        result=subprocess.run([str(binary),'--publication-pool-unsupported'],capture_output=True,text=True,
                              timeout=5,env={**os.environ,'FT_TEST_INITIAL_POOL_LR':hex(lr)})
        assert result.returncode==0,(hex(lr),result.stderr)
        results.append({'excluded_pool_lr':hex(lr),'code':0})
    print('same-class native pool caller/fault matrix and excluded returns PASS')
    from command_trace_test import verify as verify_command_trace
    results.extend(verify_command_trace(binary))
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
    for mode in ['--external','--foreign-external','--reject-graph','--stop-after-left',
                 '--repeated-cycles','--repeated-owned-no-wait','--repeated-owned-no-disposal',
                 '--repeated-owned-reset-failed','--repeated-owned-begin-failed']:
        result=subprocess.run([str(binary),mode],capture_output=True,text=True,
                              env={**os.environ,'FT_TEST_CONSTRUCT':'1'})
        assert result.returncode==0,(mode,result.stderr)
        results.append({'construct_mode':mode,'code':result.returncode,'output':result.stdout+result.stderr})
        print('construct mode',mode,'PASS')
    # Link every production owner/router component into one DSO, with no fixture.
    cmd=flags+['-fPIC','-shared']+[str(ROOT/s) for s in SOURCES]+[str(ROOT/'live/runtime-call.S'),'-o',str(build/'libft-native-router.so')]
    subprocess.run(cmd,check=True)
    subprocess.run(['nm','-D','--defined-only',str(build/'libft-native-router.so')],check=True,stdout=subprocess.DEVNULL)
    report.update(tests=results,memory_peak=(cg/'memory.peak').read_text().strip())
    (build/'verification.json').write_text(json.dumps(report,indent=2)+'\n');print(json.dumps(report|{'tests':len(results)}));return 0
if __name__=='__main__':raise SystemExit(main())
