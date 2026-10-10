#!/usr/bin/env python3
# SPDX-License-Identifier: MIT
"""Pinned compositor + vrclient CPU instructions against the production binding.
OS services, allocation, native containers and header mapping are explicit fakes.
No readiness callback, device, camera service, GPU or live loader is exercised.
"""
import ctypes as C
import hashlib
import json
import os
from pathlib import Path
import struct
import subprocess
import unittest
import uuid
from elftools.elf.elffile import ELFFile
from unicorn import Uc, UC_ARCH_ARM64, UC_MODE_ARM, UC_HOOK_CODE
import unicorn.arm64_const as A

ROOT=Path(__file__).resolve().parents[1]
BUILD=ROOT/'build/source-provider'
WITNESSES=[]
SANITIZE=os.environ.get('FT_SOURCE_UBSAN')=='1'
COMP=Path(os.environ['FT_HOOK_TEST_ELF'])
CLIENT=Path(os.environ.get('FT_SOURCE_CLIENT_ELF','/home/jakeg/.hermes/work/frame-native-reuse/camera-analysis/vrclient.so'))
PINS=['9b1bd5fe4463592cbc71b2517b9ceb0a0fcd66cd6885a19dca4be0784bf6c265',
      'fe967dcc0d97dab984bd9acc1dc522fcecbcf9beb794fdd7ec156334ec31e313']
class Saved(C.Structure):
    _fields_=[('x',C.c_uint64*31),('sp',C.c_uint64),('q',C.c_uint64*64),
              ('nzcv',C.c_uint64),('fpcr',C.c_uint64),('fpsr',C.c_uint64),('pad',C.c_uint64)]
Read=C.CFUNCTYPE(C.c_bool,C.c_void_p,C.c_uint64,C.c_void_p,C.c_size_t)

def capped(cmd):
    unit='source-provider-'+uuid.uuid4().hex+'.scope'
    try:
        subprocess.run(['systemd-run','--user','--scope','--unit='+unit,
                        '-p','MemoryMax=256M','-p','MemorySwapMax=0',*map(str,cmd)],check=True,timeout=90)
    except subprocess.TimeoutExpired:
        subprocess.run(['systemctl','--user','stop',unit],check=False,timeout=15)
        raise

def compile_binding():
    BUILD.mkdir(parents=True,exist_ok=True)
    objects=[]
    flags=['-std=c++17','-O0','-g1','-Wall','-Wextra','-Werror','-pthread','-fPIC',
           '--param=ggc-min-expand=10','--param=ggc-min-heapsize=4096']
    sanitizer=['-fsanitize=undefined','-fno-sanitize-recover=all'] if SANITIZE else []
    for file in ['live/source_provider_test.cpp','live/source_provider.cpp','control/camera_gate.cpp']:
        out=BUILD/(Path(file).stem+'.o');objects.append(out)
        capped(['g++',*flags,*sanitizer,'-c',ROOT/file,'-o',out])
    capped(['g++','-shared','-pthread',*sanitizer,*objects,'-o',BUILD/'binding.so'])

class Native:
    cb=0x10000000
    camera=0x90000000
    shared=0x90010000
    manager=0x90020000
    stack=0x90040000
    done=0x90050000
    def __init__(self,dll):
        self.dll=dll;self.m=Uc(UC_ARCH_ARM64,UC_MODE_ARM);self.m.ctl_set_cpu_model(A.UC_CPU_ARM64_MAX)
        pages=set()
        for path,pin,base in zip([COMP,CLIENT],PINS,[0,self.cb]):
            with path.open('rb') as f:
                assert hashlib.file_digest(f,'sha256').hexdigest()==pin
                f.seek(0)
                for seg in ELFFile(f).iter_segments():
                    if seg['p_type']!='PT_LOAD':continue
                    lo=base+seg['p_vaddr'];hi=lo+seg['p_memsz']
                    for page in range(lo&~4095,(hi+4095)&~4095,4096):
                        if page not in pages:self.m.mem_map(page,4096);pages.add(page)
                    self.m.mem_write(lo,seg.data())
        self.m.mem_map(self.camera,0x400000)
        self.next=0x90100000;self.header={};self.names={};self.aliases={};self.current=''
        self.read_log=[];self.inject={};self.events=[];self.rejected=[];self.omit=set();self.error=None;self.reads=0
        self.put(self.manager,self.cb+0x60f108)
        self.put(self.cb+0x60f110,self.cb+0x32a270)
        self.put(self.cb+0x60f118,self.cb+0x328380)
        self.put(self.cb+0x60f170,0x90065000) # temporary control-block disposal fixture
        self.put(self.cb+0x60f178,0x90066000)
        self.put(self.cb+0x6223b8,0) # native single-thread branch; mutex boundary not tested
        self.put(0x90060000,0x90061000);self.put(0x90061000,0x90062000)
        self.put(0x90062000,0x90063000);self.put(0x90062010,0x90064000)
        self.m.mem_write(self.shared+0x18,b'\1')
        @Read
        def read(_,address,out,size):
            self.reads+=1
            try:
                            data=bytes(self.m.mem_read(address,size));self.read_log.append((address,int.from_bytes(data,"little"),size))
                            C.memmove(out,data,size);return True;
            except Exception:return False
        self.read=read;self.fixture=dll.source_fixture(read,None)
        self.hook_id=self.m.hook_add(UC_HOOK_CODE,self.hook)
    def close(self):
        self.dll.source_delete(self.fixture);self.fixture=None
        self.m.hook_del(self.hook_id)
        del self.read,self.m
        # ctypes/Unicorn callbacks can otherwise retain one mapped ELF pair per
        # unittest case until cyclic GC eventually runs, exceeding the RAM cap.
        import gc
        gc.collect()
    def put(self,p,v,n=8):self.m.mem_write(p,int(v).to_bytes(n,'little'))
    def get(self,p,n=8):return int.from_bytes(self.m.mem_read(p,n),'little')
    def x(self,i,v=None):
        reg=getattr(A,'UC_ARM64_REG_X'+str(i))
        if v is not None:self.m.reg_write(reg,v)
        return self.m.reg_read(reg)
    def string(self,p):
        out=bytearray()
        while len(out)<100:
            c=self.get(p+len(out),1)
            if not c:return out.decode()
            out.append(c)
        raise AssertionError('unterminated string')
    def alloc(self,size):
        p=self.next;self.next+=(size+31)&~15
        assert self.next<0x90400000
        return p
    def observed(self,module,pc):
        self.events.append((module,pc))
        if (module,pc) in self.omit:return
        s=Saved()
        for i in range(31):s.x[i]=self.x(i)
        s.sp=self.m.reg_read(A.UC_ARM64_REG_SP)
        change=self.inject.get((module,pc))
        if change:change(s)
        if not self.dll.source_event(self.fixture,module,pc,C.byref(s)):self.rejected.append((module,hex(pc)))
    def hook(self,uc,pc,size,_):
        try:self.step(pc)
        except BaseException as e:self.error=e;uc.emu_stop()
    def step(self,pc):
        comp={0x15fa24,0x15fa28,0x15fa44,0x15fa48,0x15fae8,0x15faec,0x15fbec,0x15fbf0,0x15fc34,0x15fc38}
        client={0x32a270,0x32a398,0x32a39c,0x32a49c,0x32a504}
        if pc in comp:self.observed(0,pc)
        if pc-self.cb in client:self.observed(1,pc-self.cb)
        if pc in (0x186a38,0x186a40,0x174510):self.x(0,0)
        elif pc==0x186ab0:self.x(0,self.shared)
        elif pc==0x26a548:self.x(0,self.manager)
        elif pc==self.cb+0x368458:self.x(0,0x90060000)
        elif pc==0x90064000: # Paths intern result, not a queue ID
            self.current=self.string(self.x(2));self.names[self.current]=10+len(self.names)
            self.put(self.x(1),self.names[self.current]);self.x(0,0)
        elif pc==0x90063000: # exact alias-property output lanes
            request=self.x(2);index=0 if self.current.endswith('meshData') else 1
            alias=0xa00000001+index;self.aliases[self.current]=alias
            self.put(self.get(request+8),alias)
            self.put(request+20,3,4);self.put(request+28,self.alias_failure if index==self.failure_queue else 0,4)
            self.x(0,0)
        elif pc==self.cb+0x8e760:self.x(0,self.alloc(self.x(0)))
        elif pc==self.cb+0x901f0:self.x(0,len(self.string(self.x(0))))
        elif pc==self.cb+0x8f6d0:self.x(0,self.alloc(self.get(self.x(1))+1))
        elif pc==self.cb+0x8f1a0:self.m.mem_write(self.x(0),bytes(self.m.mem_read(self.x(1),self.x(2))))
        elif pc==self.cb+0x329a40: # OS/shared mapping boundary, explicitly modeled
            index=0 if self.current.endswith('meshData') else 1
            header=self.alloc(0x470);self.put(header,self.aliases[self.current]+(1 if self.bad_header==index else 0))
            self.put(header+0x20,1,4);self.header[self.x(0)+0x60]=header
            self.x(0,self.connect_failure if index==self.failure_queue else 0)
        elif pc==self.cb+0x258418:self.x(0,self.header[self.x(0)])
        elif pc==self.cb+0x32aaa8: # native C++ tree allocator/insertion boundary
            tree=self.x(0);key=self.get(self.x(1));root=tree+0x10
            while self.get(root):
                node=self.get(root);old=self.get(node+0x20)
                if old==key:break
                root=node+(0x10 if key<old else 0x18)
            else:
                node=self.alloc(0x40);self.put(node+0x20,key);self.put(root,node)
                self.put(node+0x28,self.get(self.x(1)+8));self.put(node+0x30,self.get(self.x(1)+16))
                control=self.get(node+0x30);self.put(control+8,self.get(control+8,4)+1,4)
            self.x(0,node)
        elif pc in (self.cb+0x8f2d0,self.cb+0x23d580,0x90065000,0x90066000):self.x(0,0)
        elif pc==self.cb+0x23d7d8:self.x(0,0)
        elif pc==self.cb+0x328380: # Disconnect service boundary kept separate from detach proof
            self.x(0,self.disconnect_failure if self.x(1)==0xa00000001+self.failure_queue else 0)
        else:return
        self.m.reg_write(A.UC_ARM64_REG_PC,self.x(30))
    def run(self,action=1,connect_failure=0,alias_failure=0,bad_header=-1,disconnect_failure=0,failure_queue=0):
        self.connect_failure=connect_failure;self.alias_failure=alias_failure;self.bad_header=bad_header
        self.disconnect_failure=disconnect_failure;self.failure_queue=failure_queue
        assert self.dll.source_begin(self.fixture,action)
        self.x(0,self.camera);self.x(1,2);self.x(30,self.done);self.m.reg_write(A.UC_ARM64_REG_SP,self.stack)
        self.m.emu_start(0x15f9e0 if action==1 else 0x15fba8,self.done,count=30000)
        if self.error:raise self.error
        assert self.m.reg_read(A.UC_ARM64_REG_PC)==self.done
        self.result=self.x(0);valid=bool(self.dll.source_finish(self.fixture,self.result))
        WITNESSES.append({'action':action,'actual_camera_result':self.result,'valid_observation':valid,
                          'queues':[[self.value(q,k) for k in range(6)] for q in range(2)],
                          'native_events':self.events[:]})
        self.events.clear()
        return valid
    def value(self,q,field):return self.dll.source_value(self.fixture,q,field)

class BuildTests(unittest.TestCase):
    def test_production_inventory_contains_binding(self):
        import build
        self.assertIn('live/source_provider.cpp',build.inventory())

class BindingTests(unittest.TestCase):
    def setUp(self):self.n=Native(DLL)
    def tearDown(self):self.n.close()
    def test_new_native_provider_binding(self):
        n=self.n;self.assertTrue(n.run());self.assertEqual(n.result,1)
        for q in range(2):
            self.assertEqual([n.value(q,k) for k in range(4)],[1,0,0xa00000001+q,1])
            self.assertNotEqual(n.value(q,4),0);self.assertEqual(n.value(q,5),0xa00000001+q)
    def test_dedup_is_borrowed_not_another_owner(self):
        n=self.n;self.assertTrue(n.run());self.assertTrue(n.run(action=2))
        # The fake Disconnect deliberately leaves native client maps intact.
        self.assertTrue(n.run())
        self.assertEqual([n.value(q,3) for q in range(2)],[2,2])
    def test_partial_result_does_not_hide_successful_peer(self):
        n=self.n;self.assertTrue(n.run(connect_failure=7));self.assertEqual(n.result,0)
        self.assertEqual([n.value(q,1) for q in range(2)],[7,0])
        self.assertEqual([n.value(q,3) for q in range(2)],[0,1])
        self.assertTrue(n.run(action=2));self.assertEqual(n.result,1)
        self.assertEqual([n.value(q,0) for q in range(2)],[0,0])
    def test_alias_failure_is_actual_per_queue_error(self):
        n=self.n;self.assertTrue(n.run(alias_failure=7,failure_queue=1))
        self.assertEqual([n.value(q,1) for q in range(2)],[0,2]);self.assertEqual(n.result,0)
    def test_header_alias_mismatch_poisoned_despite_native_success(self):
        n=self.n;self.assertFalse(n.run(bad_header=0));self.assertEqual(n.result,1)
        self.assertEqual(n.value(0,6),0)
    def test_unobserved_inner_branch_never_newly_owned(self):
        n=self.n;n.omit={(1,0x32a504)};self.assertTrue(n.run())
        self.assertEqual([n.value(q,3) for q in range(2)],[0,0])
    def test_unobserved_service_return_refuses_pair(self):
        n=self.n;n.omit={(0,0x15fa28)};self.assertFalse(n.run())
    def test_foreign_provider_target(self):
        n=self.n
        n.inject[(0,0x15fa24)]=lambda s:s.x.__setitem__(3,s.x[3]+4)
        self.assertFalse(n.run());self.assertEqual(n.value(0,0),0)
    def test_retention_revoked_before_native_reads(self):
        n=self.n;self.assertTrue(DLL.source_begin(n.fixture,1));DLL.source_drop_retention(n.fixture)
        s=Saved();before=n.reads
        self.assertFalse(DLL.source_event(n.fixture,0,0x15fa24,C.byref(s)))
        self.assertEqual(n.reads,before);self.assertFalse(DLL.source_finish(n.fixture,1))
    def test_foreign_thread_revokes_without_native_reads(self):
        n=self.n;self.assertTrue(DLL.source_begin(n.fixture,1));before=n.reads
        self.assertFalse(DLL.source_foreign(n.fixture));self.assertEqual(n.reads,before)
        self.assertFalse(DLL.source_finish(n.fixture,1))
    def test_reattach_does_not_reuse_old_observations(self):
        n=self.n;self.assertTrue(n.run());self.assertTrue(n.run(action=2))
        n.omit={(1,0x32a398)};self.assertTrue(n.run())
        self.assertEqual([n.value(q,3) for q in range(2)],[0,0])
    def test_provider_replacement_revokes_observation_generation(self):
        n=self.n;self.assertTrue(n.run());self.assertTrue(n.run(action=2))
        n.manager=0x90021000;n.put(n.manager,n.cb+0x60f108)
        self.assertFalse(n.run())
    def test_unreadable_provider_vptr_never_observed(self):
        n=self.n;n.inject[(0,0x15fa24)]=lambda s:s.x.__setitem__(0,0x777700000000)
        self.assertFalse(n.run());self.assertEqual(n.value(0,0),0)
    def test_missing_retention_scope_refuses_before_read(self):
        n=self.n;s=Saved();self.assertFalse(DLL.source_event(n.fixture,0,0x15fa24,C.byref(s)))
        self.assertEqual(n.reads,0)
    def test_wrong_client_call_scope(self):
        n=self.n;n.inject[(1,0x32a270)]=lambda s:setattr(s,'sp',s.sp+16)
        self.assertFalse(n.run());self.assertEqual(n.value(0,0),0)
    def test_release_failure_stops_before_second_queue(self):
        n=self.n;self.assertTrue(n.run());self.assertTrue(n.run(action=2,disconnect_failure=2))
        self.assertEqual(n.result,0);self.assertEqual(n.value(0,1),2);self.assertEqual(n.value(1,0),0)

if __name__=='__main__':
    compile_binding();DLL=C.CDLL(str(BUILD/'binding.so'))
    DLL.source_fixture.argtypes=[Read,C.c_void_p];DLL.source_fixture.restype=C.c_void_p
    DLL.source_delete.argtypes=[C.c_void_p];DLL.source_begin.argtypes=[C.c_void_p,C.c_int]
    DLL.source_event.argtypes=[C.c_void_p,C.c_int,C.c_uint64,C.POINTER(Saved)]
    DLL.source_finish.argtypes=[C.c_void_p,C.c_uint64]
    DLL.source_value.argtypes=[C.c_void_p,C.c_uint,C.c_uint];DLL.source_value.restype=C.c_uint64
    DLL.source_drop_retention.argtypes=[C.c_void_p];DLL.source_foreign.argtypes=[C.c_void_p]
    result=unittest.main(exit=False).result
    report={'pins':PINS,'tests':result.testsRun,'successful':result.wasSuccessful(),'ubsan':SANITIZE,
            'scope':'Pinned compositor/client instructions; OS/container/header/Disconnect boundaries modeled. No source readiness, GPU or live hooks.',
            'queue_fields':['observed','raw_result_u32','id','entry_unknown_new_existing','header_address','alias'],
            'witnesses':WITNESSES}
    (BUILD/('ubsan-results.json' if SANITIZE else 'results.json')).write_text(json.dumps(report,indent=2)+'\n')
    raise SystemExit(0 if result.wasSuccessful() else 1)
