#!/usr/bin/env python3
# SPDX-License-Identifier: MIT
"""Pinned construct publisher and caller witness. Synthetic memory, no devices."""
import ctypes as C
import json
import os
from pathlib import Path
import struct
import subprocess
from prepare import image


def verify():
    from elftools.elf.elffile import ELFFile
    from unicorn import Uc, UC_ARCH_ARM64, UC_MODE_ARM, UC_HOOK_CODE
    import unicorn.arm64_const as A
    root=Path(__file__).resolve().parents[1]
    path=Path(os.environ['FT_HOOK_TEST_ELF']);at=image(path)
    m=Uc(UC_ARCH_ARM64,UC_MODE_ARM);m.ctl_set_cpu_model(A.UC_CPU_ARM64_MAX)
    m.mem_map(0,0x600000)
    with path.open('rb') as f:
        for seg in ELFFile(f).iter_segments():
            if seg['p_type']=='PT_LOAD':m.mem_write(seg['p_vaddr'],seg.data())
    B=0x1000000;m.mem_map(B,0x200000)
    W,L,H,CTX,SP=B,B+0x10000,B+0x20000,B+0x80000,B+0x100000
    def x(i,v=None):
        r=getattr(A,'UC_ARM64_REG_X'+str(i))
        if v is not None:m.reg_write(r,v)
        return m.reg_read(r)
    def put(p,v,fmt='Q'):m.mem_write(p,struct.pack('<'+fmt,v))
    def get(p,fmt='Q'):return struct.unpack('<'+fmt,m.mem_read(p,struct.calcsize('<'+fmt)))[0]
    def run(start,end):
        m.emu_start(start,end,count=1000)
        assert m.reg_read(A.UC_ARM64_REG_PC)==end
    def boundary(uc,pc,size,user):
        if pc==0x1190f8:m.reg_write(A.UC_ARM64_REG_PC,x(30))
    hook=m.hook_add(UC_HOOK_CODE,boundary)
    # Both real Run caller sites derive x2 from worker+f88 and owner from
    # worker->context+1b0. Execute argument assembly, BL and native prologue.
    callers=[]
    put(W+0x48,CTX);put(CTX+0x1b0,L)
    for start,call in ((0x14ab54,0x14ab80),(0x14c29c,0x14c2c8)):
        x(19,W);x(0,L);m.reg_write(A.UC_ARM64_REG_SP,SP)
        run(start,call);assert x(0)==L and x(2)==W+0xf88
        run(call,0x11a214)
        assert x(23)==L and get(m.reg_read(A.UC_ARM64_REG_SP)+0x90)==W+0xf88
        callers.append(hex(call))
    m.hook_del(hook)
    cases=0
    for flag in (0,1,255):
        for ramp in (0.,.25,.75,1.):
            for override in (.125,float('nan')):
                put(L+0x600,ramp,'f');put(L+0x604,.25,'f')
                put(W+0xf88+0x1f0,flag,'B');put(W+0xf88+0x1e8,override,'d')
                m.reg_write(A.UC_ARM64_REG_SP,SP);put(SP+0x90,W+0xf88)
                x(23,L);x(0,H);run(0x11a348,0x11a3a4)
                assert x(2)==flag and x(3)==W+0xf88 and x(0)==H and x(23)==L
                first=get(H+0x4027c,'f');second=get(H+0x40280,'f')
                if not flag:assert first==1-ramp and second==.75
                elif override==override:assert first==second==override
                else:assert first!=first and second!=second
                cases+=1
    # Zeroed handles are native initialization, not a guessed Scene pointer.
    # Execute all six initialization stores then the alternate-path selector.
    x(20,H+0x40000);x(7,H+0x402b8);x(10,H+0x402e8)
    m.mem_write(H+0x402b4,bytes([0xaa])*52)
    run(0x112398,0x1123a8)
    assert get(H+0x402b4,'I')==0 and bytes(m.mem_read(H+0x402b8,48))==bytes(48)
    for handle in (0,99):
        put(H+0x402b8,handle);x(0,H);x(28,B+0x90000)
        put(B+0x90000+0x619,0,'B');run(0x12180c,0x121820)
        assert get(B+0x90000+0x619,'B')==bool(handle)
    # Manifest and emitted island preserve every register/status bit, including
    # the displaced ADD x2 that overwrites the consumed flag after our callback.
    for pc in (0x11a3a4,0x1130a8):
        sites=json.loads((root/'live/runtime-sites.json').read_text())['sites']
        site=next(s for s in sites if s['relative_pc']==pc)
        assert bytes.fromhex(site['guard'])==at(pc,16)
        build=root/'live/build/construct';build.mkdir(parents=True,exist_ok=True)
        libpath=build/'observer.so'
        subprocess.run(['cc','-shared','-fPIC','-DFT_EMITTER=0',str(root/'live/loader_emit.c'),'-o',str(libpath)],check=True)
        emit=C.CDLL(str(libpath)).ft_loader_emit_0
        emit.argtypes=[C.POINTER(C.c_uint32),C.c_uint64,C.c_uint64,C.c_uint64,C.c_uint32,C.c_uint,C.c_uint,C.c_uint64]
        emit.restype=C.c_size_t
        island,callback=B+0x180000,B+0x190000
        code=(C.c_uint32*512)();size=emit(code,island,0,pc,int(site['word'],0),0,0,callback)
        assert 0<size<=C.sizeof(code);m.mem_write(island,bytes(code)[:size])
        regs=[getattr(A,'UC_ARM64_REG_X'+str(i)) for i in range(31)]+[getattr(A,'UC_ARM64_REG_Q'+str(i)) for i in range(32)]+[A.UC_ARM64_REG_SP,A.UC_ARM64_REG_NZCV,A.UC_ARM64_REG_FPCR,A.UC_ARM64_REG_FPSR]
        for i,r in enumerate(regs):m.reg_write(r,0x100+i)
        m.reg_write(A.UC_ARM64_REG_SP,SP);m.reg_write(A.UC_ARM64_REG_NZCV,0xa0000000)
        m.reg_write(A.UC_ARM64_REG_FPCR,0x400000);m.reg_write(A.UC_ARM64_REG_FPSR,0x10)
        initial=m.context_save();run(pc,pc+4);expected=[m.reg_read(r) for r in regs]
        native_stack=bytes(m.mem_read(SP-32,32))
        m.context_restore(initial);calls=[]
        def clobber(uc,where,size,user):
            if where!=callback:return
            assert x(0)==SP-800;lr=x(30)
            for r in regs:
                if r!=A.UC_ARM64_REG_SP:m.reg_write(r,0)
            calls.append(where);m.reg_write(A.UC_ARM64_REG_PC,lr)
        h=m.hook_add(UC_HOOK_CODE,clobber);m.ctl_remove_cache(island,island+size)
        run(island,pc+4)
        assert calls==[callback] and [m.reg_read(r) for r in regs]==expected
        assert bytes(m.mem_read(SP-32,32))==native_stack if pc==0x1130a8 else True
        m.hook_del(h)
    report=dict(callers=callers,publisher_cases=cases,zero_handle_stores=6,selector_cases=2,islands=2,result='PASS')
    (build/'verification.json').write_text(json.dumps(report,indent=2)+'\n')
    return report

if __name__=='__main__':print(json.dumps(verify()))
