#!/usr/bin/env python3
# SPDX-License-Identifier: MIT
"""Pinned instruction witnesses; all external Vulkan calls are synthetic traps.
No ELF bytes, disassembly or generated words are written outside ignored build/.
"""
import hashlib, importlib.util, json, pathlib, struct, sys
sys.dont_write_bytecode=True
ROOT=pathlib.Path(__file__).resolve().parent
WORK=pathlib.Path('/home/jakeg/.hermes/work')
spec=importlib.util.spec_from_file_location('prior_evidence_probe',WORK/'frame-native-live-render-analysis/analyze.py')
assert spec is not None and spec.loader is not None
p=importlib.util.module_from_spec(spec);spec.loader.exec_module(p)
setattr(p,'ELF',WORK/'frame-native-reuse/camera-analysis/vrcompositor')
img=p.Image()  # verifies the entire ELF SHA256, not a filename or build ID

class Allocation(p.Probe):
    def __init__(self,flags):
        super().__init__(img)
        self.calls=[];self.indices=[];self.created=0;self.bound=0;self.mapped=0
        self.manager=self.R+0xec0;self.cfg=self.R+0x7000;self.slot=self.R+0x8000
        self.types=self.R+0x9000;self.label=self.R+0xa000;self.vector=self.R+0xb000
        self.wq(self.manager,self.R+0x50);self.wq(self.R+0xa78,0x1234)
        self.wq(self.manager+0x28,self.types);self.wq(self.manager+0x30,self.types+64)
        # Index 0 deliberately lacks coherence in fallback cases. Index 1 is
        # the actually selected fallback type, not a fixed first-table entry.
        for i,f in enumerate([flags if flags&1 else 2,flags]):
            self.wu(self.types+32*i,i);self.wq(self.types+32*i+8,f)
        self.uc.mem_write(self.cfg,bytes([1,1,1])+bytes(69))
        self.wu(self.cfg+0x34,0xffffffff);self.uc.mem_write(self.label,b'arena\0')
        self.wq(0x5ae2f0,0)  # no synthetic pthread runtime
        for i,got in enumerate([0x5aea28,0x5ae488,0x5ae8f0,0x5ae370,0x5ae3f0]):
            cell=self.R+0xc000+8*i;self.wq(got,cell);self.wq(cell,self.STOP+0x200+4*i)
    def ret(self):self.uc.reg_write(p.ar.UC_ARM64_REG_PC,self.x(30))
    def hook(self,uc,a,size,user):
        if a==0x2ff910:return # execute REAL resource allocator, not prior fake
        if a==self.STOP+0x200: # vkCreateBuffer
            assert self.x(0)==0x1234 and self.rq(self.x(1)+24)==0x80080
            self.wq(self.x(3),0xb00f);self.created=self.x(3)-0xb0;self.x(0,0);self.calls.append('create');self.ret();return
        if a==self.STOP+0x204: # vkGetBufferMemoryRequirements2
            assert self.rq(self.x(1)+16)==0xb00f
            self.wq(self.x(2)+16,0x81000);self.wq(self.x(2)+24,64);self.wu(self.x(2)+32,3)
            self.calls.append('requirements');self.ret();return
        if a==self.STOP+0x208: # vkAllocateMemory, inspect real VkMemoryAllocateInfo
            info=self.x(1);assert self.ru(info)==5 and self.rq(info+16)==0x81000
            self.indices.append(self.ru(info+24));assert self.x(3)==self.created+0x50
            self.wq(self.x(3),0xfeed);self.x(0,0);self.calls.append('allocate');self.ret();return
        if a==self.STOP+0x20c: # vkBindBufferMemory
            assert [self.x(i) for i in range(4)]==[0x1234,0xb00f,0xfeed,0]
            self.bound=self.created;self.x(0,0);self.calls.append('bind');self.ret();return
        if a==self.STOP+0x210: # vkMapMemory
            assert [self.x(i) for i in range(5)]==[0x1234,0xfeed,0,(1<<64)-1,0]
            assert self.x(5)==self.bound+0xa0
            self.wq(self.x(5),self.R+0x20000);self.mapped=self.bound;self.x(0,0);self.calls.append('map');self.ret();return
        if a==0x3002a8: # std::vector append external storage only
            v=self.x(0);begin=self.rq(v)
            if not begin:self.wq(v,self.vector);self.wq(v+8,self.vector);self.wq(v+16,self.vector+0x100)
            end=self.rq(v+8);self.wq(end,self.rq(self.x(2)));self.wq(v+8,end+8);self.ret();return
        if a==0x180110: # debug-string formatting external boundary
            dst=self.x(8);self.wq(dst,dst+16);self.wq(dst+8,0);self.uc.mem_write(dst+16,b'\0');self.ret();return
        if a in (0x30b598,0x30b5c8,0x4bef0):self.ret();return
        if a==0x4b190: # external handle conversion, unused fd=-1 descriptor
            assert self.x(0)==0xffffffff
            self.x(0,0);self.ret();return
        super().hook(uc,a,size,user)
    def allocate(self):
        self.x(8,self.slot)
        try:
            self.run(0x2ff910,[self.manager,0x80080,0x11,self.label,self.cfg,0])
        except Exception:
            print('allocator stopped at',hex(self.uc.reg_read(p.ar.UC_ARM64_REG_PC)), 'calls',self.calls)
            raise
        resource=self.rq(self.slot)
        assert resource==self.created==self.bound
        assert self.rq(resource+8)==self.R+0x50
        assert self.rq(resource+0x50)==0xfeed
        assert self.ru(resource+0x58)==self.indices[-1]
        assert self.rq(resource+0x60)==0x81000
        assert self.ru(resource+0xb8)==0x80080 and self.rq(resource+0xb0)==0xb00f
        mapped=self.run(0x2fdfb0,[resource]);assert self.mapped==resource
        assert mapped==self.rq(resource+0xa0)==self.R+0x20000 and self.ru(resource+0x98)==1
        return {'selected_index':self.indices[-1],'calls':self.calls,'same_resource_through_allocate_bind_map':True}

# Text expectations are semantic layout guards decoded from hash-pinned bytes.
# Mutating one expected operand below would fail even on the right ELF.
guards={
 0x7fd88:('mov','x0, #0x16c0'),0x7fdb0:('mov','x0, x24'),0x7fdbc:('and','w23, w0, #0xff'),
 0x299f40:('ldr','x0, [x19, #0xa78]'),0x29a154:('ldr','x21, [sp, #0x140]'),
 0x29a158:('str','xzr, [sp, #0x140]'),0x29a15c:('str','x21, [x19, #0x1390]'),
 0x29a1ac:('str','x21, [x26, #8]'),0x29a1b8:('str','x0, [x26, #0x10]'),
 0x29a528:('str','x1, [x19, #0x1380]'),0x305474:('mov','x0, #0x10'),
 0x2fec0c:('ldr','w0, [x22, #0x40]'),0x2fec1c:('add','x20, x22, #0x44'),
 0x2fec44:('add','x20, x20, #8'),0x2fde14:('str','w1, [x20, #0x58]'),
 0x2fedc4:('strb','wzr, [sp, #0x38]'),0x2feeb0:('bl','#0x2fdc70'),
 0x2ff924:('mov','x0, #0x128'),0x2ffa58:('stp','w24, w1, [x19, #0xb8]'),
 0x2ffb9c:('mov','x6, x19'),0x2ffbac:('bl','#0x2fee38'),
 0x2fe01c:('add','x5, x19, #0xa0'),0x2fe030:('ldr','x1, [x19, #0x50]'),
 0x2fe03c:('ldr','x0, [x7, #0xa28]'),0x300a24:('ldr','x0, [x0, #0x738]'),
}
for pc,expected in guards.items():
    ins=list(img.cs.disasm(img.data(pc,4),pc));assert len(ins)==1
    assert (ins[0].mnemonic,ins[0].op_str)==expected,(hex(pc),ins[0].mnemonic,ins[0].op_str,expected)
results={}
for flags in [15,14]:results[str(flags)]=Allocation(flags).allocate()
assert results['15']['selected_index']==0 and results['14']['selected_index']==1
# Actual selector refuses missing properties for both configurations.
selector=[]
for cfg in [bytes([1,1,1]),bytes([0,1,1])]:
    for flags in [0,1,2,3,6,7,10,11,14,15,31]:
        z=Allocation(flags);z.uc.mem_write(z.cfg,cfg)
        z.wq(z.manager+0x30,z.types+32);z.wq(z.types+8,flags)
        req=z.R+0xd000;z.wu(req+32,1)
        selected=z.run(0x2fdb10,[z.manager,z.cfg,req])
        expected=(flags&14)==14 and (cfg[0]==0 or flags&1)
        assert bool(selected)==bool(expected),(cfg,flags,selected)
        selector.append({'preferred':bool(cfg[0]),'flags':flags,'selected':bool(selected)})
# Execute the initializer's real descriptor/argument assembly before its BL.
z=p.Probe(img);R=z.R;sp=z.STACK+0x10000
z.x(19,R);z.x(20,R+0xec0);z.x(25,sp+0x140);z.x(26,sp+0xf0)
z.run(0x29a0f4,stop=0x29a14c)
assert [z.x(i) for i in [0,1,2,4,5,8]]==[R+0xec0,0x80000,0x11,sp+0xf0,0,sp+0x140]
assert bytes(z.uc.mem_read(sp+0xf0,3))==bytes([1,1,1])
# Execute real caller's output consume/clear/publication instructions, not a copy
# of their semantics. The stack output is gone by the time init succeeds.
z=p.Probe(img);R=z.R;slot=z.STACK+0x10000+0x140;resource=R+0x8000
z.x(19,R);z.wq(R+0x1390,0);z.wq(slot,resource)
z.run(0x29a150,stop=0x29a160)
assert z.rq(slot)==0 and z.rq(R+0x1390)==resource
# Execute constructor's context/manager initialization operands as islands.
z.x(19,R);z.x(24,R);z.x(0,0xdeadbeef)
z.run(0x7fdb0,stop=0x7fdb4);assert z.x(0)==R
z.x(0,0x101);z.x(24,R);z.run(0x7fdbc,stop=0x7fdc0);assert z.x(23)==1 and z.x(24)==R
out={'elf_sha256':p.PIN,'layout_guards':len(guards),'allocation_paths':results,'selector_cases':selector,
     'source_sha256':{str(f):hashlib.sha256(f.read_bytes()).hexdigest() for f in [pathlib.Path(__file__),WORK/'frame-native-live-render-analysis/analyze.py',ROOT/'evidence.cpp',ROOT/'evidence.hpp']},
     'output_slot_consumed_and_cleared':True,'external_boundaries':'synthetic Vulkan, debug strings, libc allocation and selector vector storage; no native GPU calls',
     'unproven':'native object destruction coverage, initial command generation, pass graph lifetime, runtime image/transport, owner thread handoff'}
B=ROOT.parent/'integration'/'build'/'runtime-evidence';B.mkdir(parents=True,exist_ok=True)
(B/'instruction-results.json').write_text(json.dumps(out,indent=2)+'\n')
print(json.dumps(out,indent=2))
