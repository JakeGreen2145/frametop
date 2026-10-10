#!/usr/bin/env python3
# SPDX-License-Identifier: MIT
"""Pinned source instructions + actual merged islands -> actual Runtime replay.
GPU driver calls/format predicate are synthetic. Native acquire, vector append,
resource registration/dedup, reverse argument assembly and helper execute.
Run in a 256 MiB/zero-swap scope. No device or live loader activation.
"""
import ctypes as C,json,os,struct,subprocess,sys
from pathlib import Path
import loader_test as L
from unicorn import Uc,UC_ARCH_ARM64,UC_MODE_ARM,UC_HOOK_CODE
import unicorn.arm64_const as A
ROOT=L.ROOT;B=ROOT/'build/source-command';B.mkdir(parents=True,exist_ok=True)
PCS=[0x29f294,0x29f298,0x29ac80,0x29ac84,0x2fe72c,0x2fe730,0x2fe738,0x2fe73c,0x2a255c,0x2a2560]
BASE=0x70000000
R,CMD,W,T,I,V,P,S,REF=[0x1000000+i*0x10000 for i in range(9)]
CALLBACK=BASE+0x780000;API=BASE+0x790000

def generate():
    L.compile_tests(ROOT/'live/runtime-sites.json')
    # Full register/status differential tests exercise these same merged islands.
    import unittest
    suite=unittest.TestSuite(L.LoaderTests(n) for n in ['test_actual_merged_emitters','test_runtime_manifest_contract'])
    assert unittest.TextTestRunner(verbosity=2).run(suite).wasSuccessful()
    rows=json.loads((L.B/'sites.private.json').read_text())['sites'];ids={s['pc']:i for i,s in enumerate(rows)}
    dll=C.CDLL(str(L.B/'loader-test.so'));emit=dll.ft_loader_emit_site
    emit.argtypes=[C.POINTER(C.c_uint32),C.c_uint64,C.c_uint64,C.c_uint,C.c_uint64];emit.restype=C.c_size_t
    m=Uc(UC_ARCH_ARM64,UC_MODE_ARM);m.mem_map(BASE,0x800000);m.mem_map(R,0x90000)
    # ELF executable mapping is validated by prepare.image and manifest guards.
    at=L.prepare.image(L.prepare.ELF)
    for lo,hi in [(0x29abf0,0x29ad18),(0x29f1e8,0x29f560),(0x2fe638,0x2fe7c8),(0x3011c0,0x301350),(0x2a251c,0x2a2564)]:
        m.mem_write(BASE+lo,at(lo,hi-lo))
    def q(p,v):m.mem_write(p,struct.pack('<Q',v))
    def w(p,v):m.mem_write(p,struct.pack('<I',v&0xffffffff))
    def x(i,v=None):
        reg=getattr(A,'UC_ARM64_REG_X'+str(i))
        if v is None:return m.reg_read(reg)
        m.reg_write(reg,v)
    q(R+0x1380,W);q(W+8,CMD);q(CMD+0x738,0xc001)
    q(CMD+0x770,REF);q(CMD+0x778,REF);q(CMD+0x780,REF+32)
    q(R+0x1550,V);q(R+0x1558,V);q(R+0x1560,V+32);q(R+0x13f8,42)
    w(R+0x768,0);w(R+0xb20,-3)
    q(T+0x28,1);q(T+0x48,I);q(I+8,R+0x50);q(I+0xb0,0xcafe)
    w(I+0xc8,37);w(I+0x144,1);w(I+0x148,1);w(I+0xd0,1)
    q(P+0x80,T);q(P+0x2b0+0x80,T)
    q(BASE+0x5ae728,BASE+0x791000);q(BASE+0x791000,API)
    q(BASE+0x5ae2f0,0) # single-thread fixture lock selection only
    for pc in PCS:
        island=BASE+0x600000+ids[pc]*2048;buf=(C.c_uint32*512)()
        n=emit(buf,island,BASE,ids[pc],CALLBACK);assert n
        m.mem_write(island,bytes(buf)[:n]);m.mem_write(BASE+pc,struct.pack('<I',0x14000000|(((island-BASE-pc)//4)&0x3ffffff)))
    events=[];phase=0;apis=0
    def hook(u,pc,size,_):
        nonlocal apis
        if pc==CALLBACK:
            saved=x(0);ident=x(1);rel=rows[ident]['pc'];assert rel in PCS
            abi=bytes(m.mem_read(saved,800))
            events.append((phase,rel,abi,bytes(m.mem_read(S,0x400)),bytes(m.mem_read(I,0x180)),bytes(m.mem_read(V,32)),bytes(m.mem_read(R+0x1558,8))))
            # A real observer may clobber every volatile lane. The emitted island
            # must restore the complete Saved record, checked independently above.
            ret=x(30)
            for reg in range(31):x(reg,0xdead)
            m.reg_write(A.UC_ARM64_REG_PC,ret)
        elif pc==API:
            apis+=1;m.reg_write(A.UC_ARM64_REG_PC,x(30))
        elif pc==BASE+0x3172c8: # format predicate, explicit synthetic boundary
            x(0,0);m.reg_write(A.UC_ARM64_REG_PC,x(30))
        elif pc==BASE+0x29f29c or pc==BASE+0x2a2124:m.emu_stop()
    handle=m.hook_add(UC_HOOK_CODE,hook)
    for eye in range(2):
        phase=eye;m.reg_write(A.UC_ARM64_REG_SP,S+0x250)
        x(21,R);x(24,P+eye*0x2b0);x(20,P+eye*0x2b0+0x80);x(1,T);x(4,0xcafe);x(30,BASE+0x2a1c58)
        m.emu_start(BASE+0x29f284,BASE+0x7ffffc,count=20000)
    phase=2;m.reg_write(A.UC_ARM64_REG_SP,S+0x200)
    x(19,R);x(20,V);x(0,T);x(1,I);x(24,0xffffffffffffffff);x(23,1)
    m.emu_start(BASE+0x2a251c,BASE+0x7ffffc,count=20000)
    expected=[[0x29f294,0x29ac80,0x2fe72c,0x2fe730,0x2fe738,0x2fe73c,0x29ac84,0x29f298],
              [0x29f294,0x29f298],[0x2a255c,0x2fe72c,0x2fe730,0x2fe738,0x2fe73c,0x2a2560]]
    assert [[e[1] for e in events if e[0]==p] for p in range(3)]==expected
    assert apis==2 and struct.unpack('<Q',m.mem_read(REF,8))[0]==I
    assert struct.unpack('<Q',m.mem_read(CMD+0x778,8))[0]==REF+8 # genuine registration/dedup
    assert struct.unpack('<Q',m.mem_read(I+0xd8,8))[0]==0xffffffffffffffff
    target=B/'source-native.bin'
    with target.open('wb') as f:
        f.write(struct.pack('<Q',len(events)))
        for p,pc,*chunks in events:f.write(struct.pack('<QQ',p,pc)+b''.join(chunks))
    m.hook_del(handle);del m
    report=dict(events=len(events),api_calls=apis,source_resource_entries=1,pcs=[hex(p) for p in PCS],
                boundaries=['synthetic VkCommandBuffer/VkImage','synthetic void Vulkan driver call','synthetic format predicate'],
                native=['acquire','resource registration and dedup','texture append','reverse release','merged loader islands'])
    (B/'native-report.json').write_text(json.dumps(report,indent=2)+'\n');print(json.dumps(report))
    return target

if __name__=='__main__':
    target=generate()
    exe=ROOT/('build/camera-runtime-asan/camera-runtime' if '--sanitize' in sys.argv else 'build/camera-runtime/camera-runtime')
    subprocess.run([str(exe),'--positive'],env={**os.environ,'FT_SOURCE_NATIVE':str(target)},check=True)
    print('PASS actual Runtime + emitted pinned source callbacks + exact Command/Lifecycle retirement')
