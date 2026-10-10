#!/usr/bin/env python3
# SPDX-License-Identifier: MIT
"""Pinned camera blend constructors and actual late camera packet assembly.
Synthetic objects/allocator/upload/memset only. No GPU or live camera execution.
"""
import hashlib
import json
from pathlib import Path
import struct
import sys
ROOT=Path(__file__).resolve().parents[1]
sys.path.insert(0,str(ROOT/'live'))
from prepare import image,ELF,PIN

def witness(out):
    from elftools.elf.elffile import ELFFile
    from unicorn import Uc,UC_ARCH_ARM64,UC_MODE_ARM,UC_HOOK_CODE
    import unicorn.arm64_const as A
    image(ELF) # Reject an unpinned ELF before executing anything.
    m=Uc(UC_ARCH_ARM64,UC_MODE_ARM);m.ctl_set_cpu_model(A.UC_CPU_ARM64_MAX)
    m.mem_map(0,0x600000)
    with ELF.open('rb') as f:
        for s in ELFFile(f).iter_segments():
            if s['p_type']=='PT_LOAD':m.mem_write(s['p_vaddr'],s.data())
    B=0x1000000;m.mem_map(B,0x200000)
    K,R,V,APP,POL,FRAME,SP=B,B+0x10000,B+0x20000,B+0x30000,B+0x60000,B+0x70000,B+0x100000
    heap=B+0x120000;stop=B+0x1f0000;allocs=[];uploads=[]
    def x(i,v=None):
        r=getattr(A,'UC_ARM64_REG_X'+str(i))
        if v is not None:m.reg_write(r,v)
        return m.reg_read(r)
    def put(p,v,fmt='Q'):m.mem_write(p,struct.pack('<'+fmt,v))
    def get(p,fmt='Q'):return struct.unpack('<'+fmt,m.mem_read(p,struct.calcsize('<'+fmt)))[0]
    def run(start,end):
        m.emu_start(start,end,count=10000)
        assert m.reg_read(A.UC_ARM64_REG_PC)==end,hex(m.reg_read(A.UC_ARM64_REG_PC))
    def hook(uc,pc,size,user):
        if pc==0x4b490:
            assert x(0)==40
            address=heap+len(allocs)*0x100;allocs.append(address);x(0,address)
        elif pc==0x4b8c0:
            assert x(2)==352;m.mem_write(x(0),bytes([x(1)&255])*x(2))
        elif pc==0x2993f8:
            assert x(0)==R and x(1)==B+0x90000 and x(2)==K+0xc0
            uploads.append(bytes(m.mem_read(x(2),528)))
        else:return
        m.reg_write(A.UC_ARM64_REG_PC,x(30))
    m.hook_add(UC_HOOK_CODE,hook)
    put(K,APP);put(K+8,R);put(R,V);put(V+0x1a8,0x294358)
    put(V+0x78,0x2993f8);put(V+0x228,0x29b278)
    m.reg_write(A.UC_ARM64_REG_SP,SP);x(0,K);x(30,stop)
    run(0x15ee20,stop);assert x(0)==1 and len(allocs)==5
    states={hex(o):list(struct.unpack('<8I',m.mem_read(get(K+o)+8,32))) for o in (0x80,0x88,0x90,0x98,0xa0)}
    assert states['0x90']==[1,1,7,0,1,7,0,15]
    assert states['0xa0']==[0,0,0,0,0,0,0,15]
    # Execute the same constructor used by Adapter::initialize for its private state.
    desc=B+0xa0000;m.mem_write(desc,struct.pack('<6I',1,1,3,1,3,0))
    x(0,R);x(1,desc);x(30,stop);run(0x294358,stop)
    private=list(struct.unpack('<8I',m.mem_read(x(0)+8,32)))
    assert private==states['0x90'] and x(0) not in [get(K+o) for o in (0x80,0x88,0x90,0x98,0xa0)]
    put(K+0x2d0,B+0x90000);put(K+0x2e8,3,'I')
    # Native method entry establishes the same candidate K consumed at upload
    # and late enqueue. Do not infer K from a fixture's chosen register alone.
    put(K+0x319,1,'B');put(FRAME,1,'I')
    m.reg_write(A.UC_ARM64_REG_SP,SP+0x460)
    x(0,K);x(1,0);x(2,FRAME);x(3,B+0xb0000);x(4,POL)
    run(0x160be8,0x160c28)
    assert x(27)==K and x(21)==FRAME and x(23)==POL
    assert m.reg_read(A.UC_ARM64_REG_SP)==SP and get(SP+0xa8)==B+0xb0000
    identity=[1,0,0,0,0,1,0,0,0,0,1,0,0,0,0,1]
    projection=[1,0,0,0,0,1,0,0,0,0,-1,-.1,0,0,-1,0]
    m.mem_write(K+0xc0,bytes((i*13)%256 for i in range(528)))
    for offset,matrix in ((0,identity),(64,identity),(128,projection)):
        m.mem_write(K+0xc0+offset,struct.pack('<16f',*matrix))
    rows=[];records=[]
    for special in (0,1):
        for alpha in (1.,.5,0.):
            for sharpen in (0,1):
                for variant in (0,1,2):
                    m.reg_write(A.UC_ARM64_REG_SP,SP)
                    put(SP+0xa8,B+0xb0000);put(B+0xb0000,1,'B')
                    put(SP+0xb8,alpha,'f');put(SP+0x140,1.,'f')
                    put(POL+0x8c,special,'B');put(POL+0x21,0,'B')
                    put(K+0x347,int(variant<2),'B');put(K+0x346,int(variant==1),'B')
                    for i,v in ((27,K),(21,FRAME),(23,POL),(22,SP+0x300),(25,sharpen),(24,0),(28,0)):
                        x(i,v)
                    run(0x161d40,0x161ed8)
                    assert x(0)==R and x(1)==SP+0x300 and x(4)==0x29b278 and x(27)==K
                    prefix=bytes(m.mem_read(x(1),352));selected=struct.unpack_from('<Q',prefix,8)[0]
                    offset=0x98 if special else (0xa0 if alpha==1 else 0x90)
                    assert selected==get(K+offset)
                    vs=struct.unpack_from('<I',prefix,0x20)[0];ps=struct.unpack_from('<I',prefix,0x30)[0]
                    assert (vs,ps)==(58 if sharpen else 57,125+3*sharpen+variant)
                    assert struct.unpack_from('<Q',prefix,0x38)[0]==get(K+0x2d0)==struct.unpack_from('<Q',prefix,0x58)[0]
                    words=bytes(m.mem_read(selected+8,32));constants=uploads[-1]
                    assert struct.unpack_from('<f',constants,0x13c)[0]==alpha
                    records.append(struct.pack('<If',special,alpha)+words+prefix+constants)
                    rows.append(dict(special=special,alpha=alpha,slot=hex(offset),vs=vs,ps=ps))
    out.mkdir(parents=True,exist_ok=True)
    (out/'native.private.bin').write_bytes(struct.pack('<I',len(records))+b''.join(records))
    report=dict(elf_sha256=PIN,construct=hex(0x15ee20),blend_constructor=hex(0x294358),caller=[hex(0x161d40),hex(0x161ed8)],states=states,private=private,cases=rows,scope='pinned instructions on synthetic objects; not live camera/GPU')
    (out/'native-witness.json').write_text(json.dumps(report,indent=2)+'\n')
    return report

if __name__=='__main__':
    result=witness(ROOT/'render/build/blend');print(json.dumps(result,indent=2))
