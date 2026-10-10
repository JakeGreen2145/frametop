#!/usr/bin/env python3
"""Fresh pinned vectors through the real Runtime; every subprocess 256M/0 swap."""
import ctypes as C,json,os,runpy,subprocess,sys
from pathlib import Path
import source_provider_test as S
S.compile_binding();dll=C.CDLL(str(S.BUILD/'binding.so'))
dll.source_fixture.argtypes=[S.Read,C.c_void_p];dll.source_fixture.restype=C.c_void_p
dll.source_delete.argtypes=[C.c_void_p];dll.source_begin.argtypes=[C.c_void_p,C.c_int]
dll.source_event.argtypes=[C.c_void_p,C.c_int,C.c_uint64,C.POINTER(S.Saved)]
dll.source_finish.argtypes=[C.c_void_p,C.c_uint64]
dll.source_value.argtypes=[C.c_void_p,C.c_uint,C.c_uint];dll.source_value.restype=C.c_uint64
from source_metadata_test import Native
n=Native(dll);events=[];calls=[];original=n.observed
def observed(module,pc):
    event=dict(module=module,pc=pc,x=[n.x(i) for i in range(31)],sp=n.m.reg_read(S.A.UC_ARM64_REG_SP))
    n.read_log.clear();original(module,pc);event['reads']=n.read_log[:];events.append(event)
    assert len(event['reads'])<=16
n.observed=observed
for action in [1,0,2,1]:
    first=len(events)
    if action==0:assert n.read_cycle();result=1
    else:assert n.run(action=action);result=n.result
    events.append(dict(module=0,pc=0x15fdc8 if action==0 else 0x15faa4 if action==1 else 0x15fbd8,
                       x=[result]+[0]*30,sp=n.stack,reads=[]))
    calls.append(dict(pc=0x15fcc0 if action==0 else 0x15f9e0 if action==1 else 0x15fba8,first=first,last=len(events),result=result))
n.close()
# Existing checked-in runner provisions the complete real Runtime and dependencies.
argv=sys.argv;sys.argv=['camera_runtime_test.py','--build-only']+(['--sanitize'] if '--sanitize' in argv else [])
g=runpy.run_path(str(S.ROOT/'live/camera_runtime_test.py'));sys.argv=argv
B=g['B'];run=g['run'];flags=g['flags']
(B/'source-vectors.json').write_text(json.dumps(dict(events=events,calls=calls),indent=2)+'\n')
lines=['struct SourceRead {uint64_t address,value;unsigned size;};',
       'struct SourceEvent {unsigned module;uint64_t pc,x[31],sp;unsigned read_count;SourceRead reads[16];};',
       'static const SourceEvent source_events[] = {']
for e in events:
    reads=','.join('{%s,%s,%d}'%(hex(a),hex(v),size) for a,v,size in e['reads'])
    lines.append('{%d,%s,{%s},%s,%d,{%s}},'%(e['module'],hex(e['pc']),','.join(map(hex,e['x'])),hex(e['sp']),len(e['reads']),reads))
lines+=['};','struct SourceCall {uint64_t pc;unsigned first,last;uint64_t result;};','static const SourceCall source_calls[] = {']
lines+=['{%s,%d,%d,%d},'%(hex(c['pc']),c['first'],c['last'],c['result']) for c in calls];lines+=['};']
(B/'source.private.hpp').write_text('\n'.join(lines)+'\n')
objects=[o for o in g['objects'] if 'camera_runtime_test.cpp' not in str(o)]
for src in ['live/source_runtime_test.cpp']:
    obj=B/(src.replace('/','_')+'.o');run(['g++',*flags,'-c',S.ROOT/src,'-o',obj]);objects.append(obj)
exe=B/'source-runtime';run(['g++',*flags,*objects,'-o',exe])
for mode in ['positive','foreign','nested','missing-nested-return','missing-module','missing-event','provider-rebind']:run([exe,mode])
# Small actual owner-scope test uses the same production objects, no helper copy.
obj=B/'source-scope.o';run(['g++',*flags,'-c',S.ROOT/'live/source_scope_test.cpp','-o',obj])
# Scope fixture does not contain or call Runtime/native thunks.
run(['g++',*flags,obj,*[o for o in g['objects'] if 'camera_runtime_test.cpp' not in str(o) and 'live_runtime.cpp' not in str(o)],'-o',B/'source-scope'])
run([B/'source-scope'])
print('PASS source runtime/native vectors and active owner scopes; no live native service, readiness or GPU claim')
