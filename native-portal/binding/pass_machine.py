#!/usr/bin/env python3
# SPDX-License-Identifier: MIT
"""Pinned instructions versus compiled extraction. All external APIs synthetic.
No proprietary bytes/disassembly are committed; ELF and results stay private.
"""
import argparse
import base64
import ctypes as ct
import hashlib
import importlib.util
import json
from pathlib import Path
import struct
import subprocess
import sys
sys.dont_write_bytecode=True
p=argparse.ArgumentParser()
p.add_argument('--analysis',type=Path,required=True,help='Prior read-only analyzer directory')
p.add_argument('--elf',type=Path,required=True)
a=p.parse_args()
root=Path(__file__).resolve().parents[1]
build=root/'binding/build/pass';build.mkdir(parents=True,exist_ok=True)
cg=Path('/sys/fs/cgroup')/Path('/proc/self/cgroup').read_text().strip().split('::')[1].lstrip('/')
assert (cg/'memory.max').read_text().strip()=='681574400'
assert (cg/'memory.swap.max').read_text().strip()=='0'
PIN='9b1bd5fe4463592cbc71b2517b9ceb0a0fcd66cd6885a19dca4be0784bf6c265'
assert hashlib.sha256(a.elf.read_bytes()).hexdigest()==PIN
spec=importlib.util.spec_from_file_location('prior',a.analysis/'analyze.py')
b=importlib.util.module_from_spec(spec);spec.loader.exec_module(b)
b.ELF=a.elf
from unicorn import UC_HOOK_MEM_WRITE
import unicorn.arm64_const as ar
img=b.Image()
so=build/'extract.so'
subprocess.run(['g++','-std=c++17','-Wall','-Wextra','-Werror','-shared','-fPIC','-pthread','-I'+str(root/'include')]+[str(root/x) for x in ['binding/pass_test_bridge.cpp','binding/pass.cpp','render/adapter.cpp','src/aperture.cpp','control/protocol.cpp','control/policy.cpp']]+['-o',str(so)],check=True)
lib=ct.CDLL(str(so))
extract=lib.pass_extract_test
extract.argtypes=[ct.c_void_p,ct.c_size_t,ct.c_uint64,ct.c_uint64,ct.c_uint64,ct.c_uint32,ct.POINTER(ct.c_uint64)]
extract.restype=ct.c_int

class Probe(b.Probe):
    def __init__(self):
        self.events=[]; self.executed=[]; self.writes=[]; self.skip=False; self.marker=0xf00d1234
        super().__init__(img)
        self.uc.hook_add(UC_HOOK_MEM_WRITE,lambda uc,access,addr,size,val,u:self.writes.append((addr,size)))
        self.passbase=self.R+0x18000; self.cache=self.R+0x1a000
        self.wq(self.R+0x15a0,self.passbase);self.wq(self.R+0x15a8,self.passbase+3*0x98)
        self.wq(self.R+0x1478,self.cache);self.wq(self.R+0x1480,self.cache+0x80)
        self.wq(self.R+0x1380,self.R+0x4000);self.wq(self.R+0x4008,self.R+0x5000)
        self.wq(self.R+0xa78,0xd00d)
        self.wq(self.R+0x5000+0x738,0xc0ffee)
        self.wq(0x5ae2f0,0)
        self.names={}
        self.wq(self.rq(0x5aef88),self.STOP+0x1000) # synthetic vkCmdBeginRenderPass2KHR
        self.wq(self.rq(0x5aed20),self.STOP+0x1100) # synthetic vkCmdEndRenderPass
        self.wq(self.rq(0x5ae920),self.STOP+0x1200) # synthetic vkCreateRenderPass2KHR
        self.wq(self.rq(0x5ae918),self.STOP+0x1300) # synthetic vkCreateFramebuffer
    def run(self,*args,**kwargs):
        self.end=kwargs.get('stop',self.STOP)
        try: return super().run(*args,**kwargs)
        except Exception:
            print('failed PC',hex(self.uc.reg_read(ar.UC_ARM64_REG_PC)), 'recent', [hex(x) for x in self.executed[-12:]], 'events',self.events[-8:])
            raise
    def ret(self):self.uc.reg_write(ar.UC_ARM64_REG_PC,self.x(30))
    def hook(self,uc,pc,size,user):
        if pc==getattr(self,'end',self.STOP): uc.emu_stop();return
        self.executed.append(pc)
        if pc in (0x17aa50,0x17ab38):
            self.events.append(('profiling_fake',pc));self.ret();return
        if pc==0x298b60:
            self.events.append(('packet_viewport_boundary_fake',self.x(1)))
            self.wq(self.x(8),0);self.ret();return
        if pc==0x298d60:
            self.events.append(('viewport_boundary_fake',pc));self.ret();return
        if pc in (self.STOP+0x1000,self.STOP+0x1100):
            self.events.append(('vkCmdBeginRenderPass2KHR_fake' if pc==self.STOP+0x1000 else 'vkCmdEndRenderPass_fake',self.x(0)))
            self.ret();return
        if pc in (self.STOP+0x1200,self.STOP+0x1300):
            kind='vkCreateRenderPass2KHR_fake' if pc==self.STOP+0x1200 else 'vkCreateFramebuffer_fake'
            self.events.append((kind,self.x(0)));self.wq(self.x(3),0x123000+pc);self.x(0,0);self.ret();return
        if pc in (0x2a9080,0x2a9308,0x2a9518,0x2a8e28,0x2a97d0):
            # Native inline-vector allocation boundary only. Never readiness.
            self.events.append(('native_vector_allocation_fake',pc));n=self.x(1)
            self.x(0,self.heap);self.heap+=(n+15)&~15;self.ret();return
        if pc in (0x2a8d08,0x2a8d50,0x2a8d98,0x2a8cc0,0x2a8de0):
            self.events.append(('native_vector_free_fake',pc));self.ret();return
        if pc==0x2fe638:
            self.events.append(('native_image_barrier_boundary_fake',self.x(0)))
            self.ret();return
        if pc==0x29dac0 and getattr(self,'null_pipeline',False):
            self.events.append(('pipeline_lookup_boundary_fake',self.x(1),self.x(2),self.x(3)))
            self.x(0,0);self.ret();return
        if pc==0x2a1c18 and self.skip and self.rq(self.x(19)+8)==self.marker:
            self.events.append(('proposed_owned_skip',self.x(19)))
            uc.reg_write(ar.UC_ARM64_REG_PC,0x2a1c0c);return
        if pc==0x29f1e8:
            self.events.append(('actual_backend_entry',self.x(1),self.x(2),self.x(3),self.x(4)))
        super().hook(uc,pc,size,user)
    def setup(self,vs,ps,alternate):
        for eye in range(2):
            pas=self.passbase+eye*0x98; c=self.cache+eye*0x40
            target=self.R+0x10000+eye*0x400; alt=target+0x200 if alternate else 0
            for off,value in [(0,target),(8,0),(0x10,alt),(0x20,0xabc+eye)]:self.wq(pas+off,value)
            self.wu(pas+0x68,0x0101+eye*0x10000)
            self.wu(target+0x14c,37+eye)
            if alt:self.wu(alt+0x14c,97+eye)
            for off,value in [(0,0xaa000+eye),(8,0xbb000+eye),(0x10,target),(0x18,0),(0x20,alt),(0x30,0xabc+eye)]:self.wq(c+off,value)
            self.wu(c+0x38,0x0101+eye*0x10000)
            pack=self.R+0x20000+eye*0x2000
            self.wq(pas+0x50,pack);self.wq(pas+0x58,pack+3*0x2b0)
            for j,name in enumerate(('stock-before','owned','stock-after')):
                addr=pack+j*0x2b0;self.names[addr]=(eye,name)
                self.wq(addr+8,self.marker if name=='owned' else 0xabcd)
                self.wq(addr+0x10,0xa123);self.wq(addr+0x18,0xb123)
                self.wu(addr+0x20,vs);self.wu(addr+0x30,ps)
                self.uc.mem_write(addr+0x15e,bytes([int(alternate)]))
                self.wq(addr+0x160,addr+0x170);self.wq(addr+0x168,15)
                label=b'FTP:'+base64.urlsafe_b64encode(struct.pack('>Q',eye+1)).rstrip(b'=')+b'\0'
                self.uc.mem_write(addr+0x170,label)
        self.events=[];self.executed=[];self.writes=[]
    def scan(self,mode=0):
        blob=bytes(self.uc.mem_read(self.R,0x30000));buf=ct.create_string_buffer(blob)
        result=(ct.c_uint64*12)()
        reason=extract(buf,len(blob),self.R,self.R,self.marker,mode,result)
        assert buf.raw[:len(blob)]==blob
        return reason,list(result)

results=[]
# Read-only helper hit -> pass encoder -> backend prologue -> actual lookup key.
for ps in range(125,131):
    vs=57 if ps<=127 else 58
    for alternate in (False,True):
        n=Probe();n.setup(vs,ps,alternate)
        reason,r=n.scan();assert reason==0 and r[:2]==[1,2]
        for eye in range(2):
            pas,R,packet,value,fmt=r[2+eye*5:7+eye*5]
            out=n.R+0x28000
            n.events=[];n.executed=[];n.writes=[]
            assert n.run(0x2a0638,[n.R,out,out+8,pas])==1
            assert n.rq(out)==value and n.rq(out+8)==0xbb000+eye
            assert not n.events and 0x2a06dc in n.executed
            assert all(n.STACK<=address<n.STACK+0x10000 or out<=address<out+16 for address,size in n.writes)
            # Execute native argument assembly and backend prologue, stop before lookup.
            n.x(19,packet);n.x(22,pas);n.x(21,n.R)
            n.wq(n.STACK+0x10000+0x70,value)
            n.run(0x2a1cc0,stop=0x29dac0)
            assert [n.x(i) for i in range(4)]==[R,packet,value,fmt]
            assert n.x(4)==pas
            # Native pipeline key, no Vulkan pipeline creation in this slice.
            n.run(0x29dac0,[R,packet,value,fmt],stop=0x29db78)
            sp=n.uc.reg_read(ar.UC_ARM64_REG_SP)
            assert n.rq(sp+0x170)==value and n.ru(sp+0x18c)==fmt
            assert n.ru(sp+0x178)==vs and n.ru(sp+0x188)==ps
        # Whole pass encoder is ONLY the reference path after proposed commit.
        for skip in (False,True):
            n.skip=skip;n.null_pipeline=True;n.events=[]
            for eye in range(2):n.run(0x2a1928,[n.R,n.passbase+eye*0x98])
            entered=[n.names[e[1]] for e in n.events if e[0]=='actual_backend_entry']
            assert entered==[(eye,name) for eye in range(2) for name in ('stock-before','owned','stock-after') if not(skip and name=='owned')]
            assert sum(e[0]=='vkCmdBeginRenderPass2KHR_fake' for e in n.events)==2
            assert sum(e[0]=='vkCmdEndRenderPass_fake' for e in n.events)==2
            assert not any(e[0]=='native_image_barrier_boundary_fake' for e in n.events)
        results.append({'case':'warm equivalence and selective skip','vs':vs,'ps':ps,'alternate_attachment':alternate})
# Compare every cache-key component and ignored fields against the actual native
# first-match loop. Stop at its miss boundary before any materialization.
for offset,size,is_key in [(0,8,True),(8,8,True),(0x10,8,True),(0x20,8,True),(0x68,1,True),(0x69,1,True),(0x6a,1,True),(0x18,8,False),(0x6b,1,False)]:
    n=Probe();n.setup(58,130,True);pas=n.passbase
    old=int.from_bytes(n.uc.mem_read(pas+offset,size),'little')
    n.uc.mem_write(pas+offset,(old^0x10).to_bytes(size,'little'))
    reason,r=n.scan()
    assert bool(r[0])== (not is_key)
    out=n.R+0x28000
    if is_key:
        n.run(0x2a0638,[n.R,out,out+8,pas],stop=0x2a06f8)
        assert not n.events
    else:
        assert n.run(0x2a0638,[n.R,out,out+8,pas])==1
        assert n.rq(out)==r[5] and not n.events
    results.append({'case':'native cache key differential','offset':hex(offset),'is_key':is_key})
# Cold materialization, execute real native helper with fake native/Vulkan boundaries.
n=Probe();n.setup(58,130,True)
n.wq(n.R+0x1480,n.cache)
reason,r=n.scan();assert not r[0] and reason!=0
n.wq(n.R+0x1488,n.cache+0x400) # spare native cache capacity, not a fabricated cache entry
pas=n.passbase
n.wu(n.rq(pas)+0xc8,1);n.wu(n.rq(pas)+0x148,1)
n.wu(n.rq(pas+0x10)+0xc8,1);n.wu(n.rq(pas+0x10)+0x148,1)
n.wq(pas+0x18,0x123456)
out=n.R+0x28000
assert n.run(0x2a0638,[n.R,out,out+8,pas])==1
assert any(e[0]=='vkCreateRenderPass2KHR_fake' for e in n.events)
assert any(e[0]=='vkCreateFramebuffer_fake' for e in n.events)
assert any(e[0]=='native_image_barrier_boundary_fake' for e in n.events)
assert 0x2a149c in n.executed
results.append({'case':'cold helper has native barrier side effect; production refuses','events':n.events})
# Real dispatch nonzero mode no-ops. No call through encoder allowed in preparation.
n=Probe();n.setup(58,130,False);assert n.scan(1)[1][0]==0
n.run(0x2a1e18,[n.R,1]);assert not n.events
results.append({'case':'nonzero dispatch returns without native effects'})
ranges=[(0x2a0638,0x2a06f8),(0x2a10f8,0x2a1114),(0x2a1464,0x2a14a0),(0x2a1928,0x2a1da4),(0x29f1e8,0x29f208),(0x29dac0,0x29db78)]
report={'elf_sha256':PIN,'cases':results,'pins':[{'start':hex(lo),'end':hex(hi),'sha256':hashlib.sha256(img.data(lo,hi-lo)).hexdigest()} for lo,hi in ranges],'limits':{s:(cg/s).read_text().strip() for s in ['memory.max','memory.swap.max','memory.peak']},'limits_note':'CPU instruction equivalence, not native GPU execution or installed hook preservation'}
(build/'instructions.json').write_text(json.dumps(report,indent=2)+'\n')
print(json.dumps({'elf_sha256':PIN,'cases':len(results),'limits':report['limits'],'evidence':str(build/'instructions.json')},indent=2))
