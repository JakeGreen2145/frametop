#!/usr/bin/env python3
# SPDX-License-Identifier: MIT
"""Execute pinned native policy branches; replay real registers into production sampling.

Synthetic objects and lock/provider/suppression returns only. No runtime loading,
GPU, camera or live claims. Native instructions never leave ignored build output.
"""
import ctypes as C
import hashlib
import itertools
import json
import math
import os
from pathlib import Path
import struct
import subprocess
import sys

ROOT=Path(__file__).resolve().parents[1]
sys.path.insert(0,str(ROOT/'runtime'))
from evidence_test import limits
TAPS=(0x112ab4,0x112ac4,0x112aec,0x112af4,0x112afc,0x112b10,
      0x112c10,0x112c50,0x112c70,0x112c78,0x112cfc,0x112d14,0x112d2c,0x112d74,0x112d90)


def main():
    from elftools.elf.elffile import ELFFile
    from unicorn import Uc,UC_ARCH_ARM64,UC_MODE_ARM,UC_HOOK_CODE
    import unicorn.arm64_const as A
    cg,cap=limits()
    path=Path(os.environ['FT_HOOK_TEST_ELF'])
    pin=hashlib.sha256(path.read_bytes()).hexdigest()
    assert pin=='9b1bd5fe4463592cbc71b2517b9ceb0a0fcd66cd6885a19dca4be0784bf6c265'
    m=Uc(UC_ARCH_ARM64,UC_MODE_ARM);m.ctl_set_cpu_model(A.UC_CPU_ARM64_MAX)
    m.mem_map(0,0x600000)
    with path.open('rb') as f:
        for seg in ELFFile(f).iter_segments():
            if seg['p_type']=='PT_LOAD':m.mem_write(seg['p_vaddr'],seg.data())
    B=0x1000000;m.mem_map(B,0x200000)
    L,S,H,SP=B,B+0x10000,B+0x20000,B+0x100000
    def put(p,v,fmt='B'):m.mem_write(p,struct.pack('<'+fmt,v))
    def get(p,fmt='I'):return struct.unpack('<'+fmt,m.mem_read(p,struct.calcsize('<'+fmt)))[0]
    def x(i,v=None):
        r=getattr(A,'UC_ARM64_REG_X'+str(i))
        if v is not None:m.reg_write(r,v)
        return m.reg_read(r)
    def registers():return [m.reg_read(A.UC_ARM64_REG_SP),x(19),x(20),x(0),x(1),x(2),x(3),x(23)]+[m.reg_read(getattr(A,'UC_ARM64_REG_S'+str(i))) for i in range(3)]
    trace=[];suppress=0
    def hook(uc,pc,size,user):
        if pc in TAPS:trace.append((pc,registers()))
        if pc in (0x186ab0,0x186950):x(0,H);m.reg_write(A.UC_ARM64_REG_PC,x(30))
        elif pc==0x186a38:m.reg_write(A.UC_ARM64_REG_PC,x(30))
        elif pc==0x5e5d8:x(0,suppress);m.reg_write(A.UC_ARM64_REG_PC,x(30))
    m.hook_add(UC_HOOK_CODE,hook)
    def native(fields,style,ptr=0,warm=0.,base=.37,gain=.63,special_scalar=None,owned=False):
        nonlocal suppress
        enabled,request,construct,special,forced,p0,p1,suppress=fields
        trace.clear();m.mem_write(S,bytes(0x1000))
        for i in range(31):x(i,0)
        for i in range(3):m.reg_write(getattr(A,'UC_ARM64_REG_Q'+str(i)),0)
        m.reg_write(A.UC_ARM64_REG_SP,SP)
        put(L+0x1572,enabled);put(L+0x918,construct or owned);put(L+0xa2c,style,'I');put(L+0x158c,gain,'f')
        put(H+0x40b49,request or owned);put(H+0x4027c,float(special) if special_scalar is None else special_scalar,'f')
        put(H+0x402b8,ptr,'Q');put(H+0x40b50,forced);put(H+0x40b4c,base,'f')
        put(S+0x1e0,p0);put(S+0x1e1,p1);put(S+0x648,warm,'f')
        x(0,L);x(1,S)
        m.emu_start(0x112a40,0x112b58,count=2000)
        assert m.reg_read(A.UC_ARM64_REG_PC)==0x112b58
        return list(trace),registers(),[get(S+0x62c,'B'),x(23),get(S+0x630),get(S+0x648)]
    build=ROOT/'live/build/policy';build.mkdir(parents=True,exist_ok=True)
    libpath=build/'sample.so'
    subprocess.run(['g++','-std=c++17','-Wall','-Wextra','-Werror','-O1','-g','-fPIC','-shared','-pthread',
                    str(ROOT/'live/policy_sample_fixture.cpp'),str(ROOT/'control/policy.cpp'),'-o',str(libpath)],check=True)
    lib=C.CDLL(str(libpath));Words=C.c_uint64*11;Out=C.c_uint64*19
    lib.sample_tap.argtypes=[C.c_uint64,C.POINTER(C.c_uint64)]
    lib.sample_finish.argtypes=[C.POINTER(C.c_uint64),C.c_uint,C.c_uint,C.POINTER(C.c_uint64)]
    def sampled(t,final,p0,p1,reset=True):
        if reset:lib.sample_reset()
        for pc,v in t:lib.sample_tap(pc,Words(*v))
        out=Out();lib.sample_finish(Words(*final),p0,p1,out);return list(out)
    manifest=json.loads((ROOT/'live/runtime-sites.json').read_text())
    sites={row['relative_pc']:row for row in manifest['sites']}
    for pc in TAPS:
        assert sites[pc]['guard']==bytes(m.mem_read(pc,16)).hex()
    # The new style-3 gain tap uses the production observer emitter. Compare
    # its displaced FMUL with the original instruction, with callback clobbers.
    emitter=build/'observer.so'
    subprocess.run(['cc','-shared','-fPIC','-DFT_EMITTER=0',str(ROOT/'live/loader_emit.c'),'-o',str(emitter)],check=True)
    emit=C.CDLL(str(emitter)).ft_loader_emit_0
    emit.argtypes=[C.POINTER(C.c_uint32),C.c_uint64,C.c_uint64,C.c_uint64,C.c_uint32,C.c_uint,C.c_uint,C.c_uint64]
    emit.restype=C.c_size_t
    island,callback=B+0x180000,B+0x190000
    buf=(C.c_uint32*512)();size=emit(buf,island,0,0x112d74,int(sites[0x112d74]['word'],0),0,0,callback)
    assert size and size<=C.sizeof(buf);m.mem_write(island,bytes(buf)[:size])
    regs=[getattr(A,'UC_ARM64_REG_X'+str(i)) for i in range(31)]+[getattr(A,'UC_ARM64_REG_Q'+str(i)) for i in range(32)]+[A.UC_ARM64_REG_SP,A.UC_ARM64_REG_NZCV,A.UC_ARM64_REG_FPCR,A.UC_ARM64_REG_FPSR]
    for i,r in enumerate(regs):m.reg_write(r,0x100+i)
    m.reg_write(A.UC_ARM64_REG_SP,SP);m.reg_write(A.UC_ARM64_REG_NZCV,0xa0000000)
    m.reg_write(A.UC_ARM64_REG_FPCR,0x400000);m.reg_write(A.UC_ARM64_REG_FPSR,0x10)
    initial=m.context_save();m.emu_start(0x112d74,0x112d78,count=1)
    expected=[m.reg_read(r) for r in regs];m.context_restore(initial);callbacks=[]
    def clobber(uc,pc,size,user):
        if pc!=callback:return
        assert x(0)==SP-800 and x(1)==0
        lr=x(30)
        for r in regs:
            if r!=A.UC_ARM64_REG_SP:m.reg_write(r,0)
        callbacks.append(pc);m.reg_write(A.UC_ARM64_REG_PC,lr)
    h=m.hook_add(UC_HOOK_CODE,clobber);m.ctl_remove_cache(island,island+size)
    m.emu_start(island,0x112d78,count=1000)
    assert callbacks==[callback] and [m.reg_read(r) for r in regs]==expected
    m.hook_del(h);m.reg_write(A.UC_ARM64_REG_FPCR,0);m.reg_write(A.UC_ARM64_REG_FPSR,0)
    counts=dict(cases=0,missing=0,scope=0,scalar=0,islands=1)
    masks=set();enabled_by_mask={};special_by_mask={};representatives=[]
    for bits,style,ptr,warm in itertools.product(range(256),range(5),(0,1),(0.,.81)):
        fields=[int(bool(bits&(1<<i))) for i in range(8)]
        t,final,result=native(fields,style,ptr,warm)
        got=sampled(t,final,fields[5],fields[6]);assert got[0] and not got[1],(fields,style,hex(got[2]))
        assert got[8:12]==result,(fields,style,got,result)
        _,_,owned=native(fields,style,ptr,warm,owned=True)
        assert got[12:16]==owned,(fields,style,ptr,got,owned)
        # A pointer selects ordinary policy even if the scalar says special.
        special=bool(fields[3] and not ptr)
        external=special or bool(fields[4]) or bool(fields[1] if fields[0] and not special else fields[2] if fields[0] else 0) or bool(result[1])
        assert bool(got[16])==bool(fields[0] and not external)
        enabled_by_mask.setdefault(got[2],set()).add(fields[0])
        special_by_mask.setdefault(got[2],set()).add(got[6])
        if got[2] not in masks:
            masks.add(got[2]);representatives.append((t,final,fields,result))
        counts['cases']+=1
    for t,final,fields,result in representatives:
        for i in range(len(t)):
            got=sampled(t[:i]+t[i+1:],final,*fields[5:7])
            assert not got[0] or got[1],('missing consumed input accepted',hex(t[i][0]),fields,hex(got[2]))
            counts['missing']+=1
        for idx in (0,1,2):
            for at in range(len(t)):
                changed=[(pc,list(v)) for pc,v in t];changed[at][1][idx]+=8
                got=sampled(changed,final,*fields[5:7]);assert got[1]
                counts['scope']+=1
            bad=list(final);bad[idx]+=8
            assert sampled(t,bad,*fields[5:7])[1]
        assert sampled(t+t,final,*fields[5:7])[1]
        assert sampled([],final,*fields[5:7],reset=False)[1]
    # Cold/warm/sticky prior values must not fabricate unexecuted observations.
    for special_scalar in (-1.,0.,1.,float('nan')):
        for base,gain in ((float('nan'),.63),(.37,float('nan')),(float('inf'),.63),(.37,0.)):
            for p1,style in itertools.product((0,1),(1,3,4)):
                fields=[0,1,1,0,0,0,p1,0]
                t,final,result=native(fields,style,base=base,gain=gain,special_scalar=special_scalar)
                got=sampled(t,final,0,p1);assert got[0] and not got[1]
                alpha=struct.unpack('<f',struct.pack('<I',got[11]))[0]
                native_alpha=struct.unpack('<f',struct.pack('<I',result[3]))[0]
                # Runtime equality rejects NaNs, including native style-3's
                # unordered .5 result versus pure_policy's preserved NaN.
                ready=not got[6] and not got[7] and alpha==native_alpha
                if math.isnan(alpha) or math.isnan(native_alpha):assert not ready
                assert not got[16] # disabled remains disabled, even if loads bypassed
                counts['scalar']+=1
    # Preserve the existing value==1 enable admission without treating other
    # nonzero native bytes as proof that request was bypassed.
    for enabled,request in itertools.product((2,255),(0,1)):
        fields=[enabled,request,0,0,0,0,0,0]
        t,final,result=native(fields,1)
        got=sampled(t,final,0,0);assert got[0] and not got[1] and not got[3] and not got[16]
        omitted=[row for row in t if row[0]!=0x112c10]
        assert not sampled(omitted,final,0,0)[0]
    # All live masks include construct bit 6: enabled SPECIAL branches, not
    # request=false reads or ordinary disabled branches. Existing special-policy
    # exclusion is independent and remains a blocker after sampling is repaired.
    live_masks={0xfdf,0x3bdf,0x1df}
    assert live_masks<=masks,(live_masks,masks)
    assert all(enabled_by_mask[mask]=={1} and special_by_mask[mask]=={1} for mask in live_masks)
    report={'pin':pin,**cap,**counts,'branch_masks':len(masks),'live_masks':sorted(live_masks),'memory.peak':(cg/'memory.peak').read_text().strip()}
    (build/'verification.json').write_text(json.dumps(report,indent=2)+'\n');print(json.dumps(report,indent=2))

if __name__=='__main__':main()
