#!/usr/bin/env python3
# SPDX-License-Identifier: MIT
"""Pinned native startup continuations, with explicit CPU-only external boundaries.

Do not infer the live dispatch caller from the first-invalidation diagnostic: it
contains no LR. Enumerate the renderer's real direct virtual callers instead.
"""
import hashlib
from itertools import product
import os
from pathlib import Path
import struct


def bootstrap_native():
    from elftools.elf.elffile import ELFFile
    from unicorn import Uc, UC_ARCH_ARM64, UC_MODE_ARM, UC_HOOK_CODE
    import unicorn.arm64_const as A
    path = Path(os.environ['FT_HOOK_TEST_ELF'])
    assert hashlib.sha256(path.read_bytes()).hexdigest() == '9b1bd5fe4463592cbc71b2517b9ceb0a0fcd66cd6885a19dca4be0784bf6c265'
    with path.open('rb') as f:
        segments = [(s['p_vaddr'], s['p_memsz'], s.data()) for s in ELFFile(f).iter_segments() if s['p_type'] == 'PT_LOAD']
    m = Uc(UC_ARCH_ARM64, UC_MODE_ARM)
    m.ctl_set_cpu_model(A.UC_CPU_ARM64_MAX)
    pages = set()
    def map_range(p, n):
        for page in range(p & ~4095, (p+n+4095) & ~4095, 4096):
            if page not in pages:
                m.mem_map(page, 4096); pages.add(page)
    for p, n, data in segments:
        map_range(p, n); m.mem_write(p, data)
    region = 0x90000000
    map_range(region, 0x100000)
    R, C, W, pool, wrapper_pool, stack, app, context = [region+i for i in (0,0x2000,0x3000,0x4000,0x5000,0x10000,0x20000,0x50000)]
    def put(p, v): m.mem_write(p, struct.pack('<Q', v))
    def get(p): return struct.unpack('<Q', m.mem_read(p,8))[0]
    def x(i, value=None):
        reg = getattr(A, 'UC_ARM64_REG_X'+str(i))
        if value is not None: m.reg_write(reg,value)
        return m.reg_read(reg)
    def run(a, b, count=10000):
        m.emu_start(a,b,count=count)
        assert m.reg_read(A.UC_ARM64_REG_PC)==b, hex(m.reg_read(A.UC_ARM64_REG_PC))
    # Both release methods explicitly accept a null slot and an empty slot.
    # Execute their real branches; neither reaches a destructor or native call.
    for entry in (0x2966a0, 0x294678):
        for slot in (0, region+0x53000):
            if slot: put(slot,0)
            x(0,R);x(1,slot);x(19,0xabc);x(30,0x900ff000)
            m.reg_write(A.UC_ARM64_REG_SP,stack)
            run(entry,0x900ff000)
            assert x(19)==0xabc and m.reg_read(A.UC_ARM64_REG_SP)==stack
            if slot: assert get(slot)==0
    # Integrate the context-stop-review witness: execute normal entry/backedge
    # BEFORE quit. Opaque watchdog/log/sleep calls are CPU returns, never joins.
    pump=[];worker=region+0x51000
    def pump_hook(uc,pc,size,user):
        if pc in (0x142620,0x142694,0x1426d4):
            pump.append([pc,int(x(0)==context),int(x(0)==worker),x(1),int(x(20)==context),x(30)])
        if pc in (0x185128,0x1844a0,0x181190,0x184fa0,0x174510,0x17aa50):
            x(0,0);m.reg_write(A.UC_ARM64_REG_PC,x(30))
    hook=m.hook_add(UC_HOOK_CODE,pump_hook)
    put(context+0x98,worker);x(20,context);m.reg_write(A.UC_ARM64_REG_SP,stack)
    run(0x14286c,0x142300)
    assert pump[0][0]==0x142620 and pump[0][1] and pump[0][5]==0x142874
    assert bytes(m.mem_read(context+0x2a,1))==b'\x00'
    x(0,1);run(0x1426ac,0x142300)
    assert pump[-1][0]==0x142694 and bytes(m.mem_read(context+0x2a,1))==b'\x00'
    saved=m.context_save();m.mem_write(context+0x2b,b'\x01');x(0,context);x(30,region+0xff000)
    run(0x142300,region+0xff000);assert x(0)==0
    m.context_restore(saved);x(0,0);run(0x1426ac,0x142704)
    assert pump[-1][0]==0x1426d4 and pump[-1][3]==1 and pump[-1][4]
    assert bytes(m.mem_read(context+0x2a,1))==b'\x01'
    assert x(0)==worker and x(1)==5000 and x(20)==context
    # Never execute wait or invent join success.
    m.mem_write(context+0x2a,b'\x00\x00')
    # Derive actual outer LR and worker/context/application chain as well.
    scheduler=region+0x54000
    put(worker+0x48,context);put(scheduler+8,app);x(19,worker)
    run(0x14a5c8,0x147a48);outer_lr=x(30);assert x(0)==worker
    for fallback in (False,True):
        put(context+0x1a8,0 if fallback else app)
        # An unmapped scheduler proves the nonnull branch bypasses fallback.
        put(context+0x1c0,scheduler if fallback else 1)
        x(0,worker);m.reg_write(A.UC_ARM64_REG_SP,stack)
        run(0x147a48,0x6f9f8);assert x(19)==worker and x(0)==app
        x(19,worker);run(0x148088,0x148094);assert x(0)==app
    put(context+0x1a8,app);put(context+0x1c0,scheduler)
    put(app+0x25320,region+0x70000);x(0,app)
    run(0x6d6e0,0x15f9e0);assert x(0)==region+0x70000
    x(20,app);m.reg_write(A.UC_ARM64_REG_SP,stack)
    run(0x9ee90,0x9ee98);assert x(0)+0x68c==app+0x2568c
    m.hook_del(hook)
    # Actual pre-Run branch, including the app+60 conditional tail. The large
    # configuration routine and provider are stopped at entry, not invented GPU
    # successes. Neither branch proves which caller produced the live release.
    put(context+0x1a8,app); put(context+0x98,region+0x51000)
    x(21,context); m.reg_write(A.UC_ARM64_REG_SP,stack)
    run(0x140d98,0x13c3e8); assert x(0)==context and x(30)==0x140da0
    x(0,0); run(0x140da0,0x80018); assert x(0)==app and x(30)==0x140dac
    run(0x80018,0x140dbc); assert x(0)==region+0x51000 and x(1)==0
    put(app+0x60,region+0x52000); x(0,app); x(30,0x140dac)
    run(0x80018,0x6d9a8); assert x(0)==app and x(30)==0x140dac
    # Execute all concrete calls through this renderer's +250 dispatch slot.
    # Unrelated renderer-class slots at the same offset are deliberately absent.
    callers = [(0x6cadc,0x6caf0),(0x9e6cc,0x9e6e0),(0x15a9a8,0x15a9bc),
               (0x15ad50,0x15ad64),(0x294cfc,0x294d10),(0x294e48,0x294e58),
               (0x295594,0x2955a8)]
    put(R,0x599c38); put(app+0x256a0,R)
    for start, lr in callers:
        x(0,R); x(19,app+0x20000); x(20,R); x(22,app+0x20000); x(25,R)
        if start==0x6cadc: x(20,0)
        put(stack+0x98,R); m.reg_write(A.UC_ARM64_REG_SP,stack)
        run(start,0x2a1e18)
        assert x(0)==R and x(1)==0 and x(30)==lr
    # Execute the actual empty current/retained vector swap and wrapper release.
    # Queue/device operations are independent external boundaries, not skipped
    # instructions claimed as GPU evidence.
    put(R+0x1380,W); put(W,wrapper_pool); put(W+8,C); put(C+0x50,pool)
    put(wrapper_pool+0x30,W)
    put(pool+0xd0+0x38,pool+0x300);put(pool+0xd0+0x40,pool+0x300);put(pool+0xd0+0x48,pool+0x380)
    put(0x5ae2f0,0)  # no optional libc locking in this serialized fixture
    m.reg_write(A.UC_ARM64_REG_SP,stack); x(19,R); x(23,R+0x1400)
    put(wrapper_pool+0x50,pool);put(wrapper_pool+0x40,pool+0x80);put(wrapper_pool+0x48,pool+0x90)
    x(20,W);x(21,stack+0xb0)
    run(0x2a21e4,0x310cf8)  # command resource mark, opaque native boundary
    run(x(30),0x301430)    # native command finalization, opaque boundary
    run(x(30),0x3018b0)
    assert x(1)==C and x(0)==pool+0x90 and x(30)==0x2a21f0
    queue_lr=x(30)
    # A modeled false submit-wrapper result still reaches native disposal. Do
    # not issue a GPU certificate or let queue failure fabricate owned settlement.
    x(0,0);run(queue_lr,0x2a2230)
    run(0x2a2230,0x2a2320)
    run(0x2a2320,0x305260)
    assert x(0)==W and x(19)==R and x(30)==0x2a23b4 and get(R+0x1380)==0
    release_lr=x(30)
    run(0x305260,0x311648)
    assert x(0)==pool+0xd0 and x(1)==C and x(19)==W and x(30)==0x3052f0
    assert m.reg_read(A.UC_ARM64_REG_SP)==stack-0x30
    run(0x311648,0x2a23b4)
    assert get(wrapper_pool+0x30)==0 and get(pool+0xd0+0x40)==pool+0x308 and get(pool+0x300)==C
    run(0x2a23b4,0x4b3d0); assert x(0)==W and x(1)==0x10 and x(30)==0x2a23c0
    # Actual pre-begun pool pop and STR publication, no new Begin fabricated.
    next_c, next_w = region+0x6000, region+0x7000
    put(pool+0x90,pool+0x400); put(pool+0xb0,pool+0x408);put(pool+0xb8,pool+0x400);put(pool+0x400,next_c)
    x(0,pool);x(8,next_w+8);x(30,0x900ff000);m.reg_write(A.UC_ARM64_REG_SP,stack)
    map_range(0x900ff000,4096)
    run(0x304e60,0x900ff000);assert get(next_w+8)==next_c and get(pool+0xb0)==pool+0x400
    put(stack+0x98,next_w);x(19,R);m.reg_write(A.UC_ARM64_REG_SP,stack)
    run(0x2a23e8,0x2a23f4);assert x(0)==next_w and get(stack+0x98)==0
    run(0x2a23f4,0x2a23f8);assert get(R+0x1380)==next_w
    # Trial 11's exact stock caller. Execute the stack load, null check, native
    # C+50 pool lookup, +d0 adjustment and BL, rather than copying a guessed LR.
    put(stack+0x80,C);put(C+0x50,pool)
    m.reg_write(A.UC_ARM64_REG_SP,stack)
    m.ctl_remove_cache(0,0x600000)
    run(0x297ca0,0x311648)
    assert x(0)==pool+0xd0 and x(1)==C and x(30)==0x297cb4
    stock_pool_lr=x(30)
    # Same-class renderer command tails, including the live initializer. These
    # paths derive pool+d0 from C+50 directly, unlike generic resource returns
    # through 311058. Unwinding and wrapper destruction remain excluded.
    command_tails = [(0x29a8a4,0x29a8b8,0x88), (0x297978,0x29798c,0x50),
                     (0x297ca0,0x297cb4,0x80), (0x297d2c,0x297d40,0x80),
                     (0x29b070,0x29b080,None), (0x29cbcc,0x29cbe0,0xa8),
                     (0x29cca8,0x29ccbc,0xa8)]
    # Exhaustive direct BL scan for this immediate C+50 -> pool+d0 shape.
    # Do not add generic 311058 resource-return callers or exception tails.
    excluded_pool_lrs=[0x297a54,0x297e4c,0x29a918,0x29b250,0x29cddc,
                       0x302808,0x3052f0,0x3053f0]
    direct=[]
    add=bytes(m.mem_read(0x297cac,4))
    load=bytes(m.mem_read(0x297ca8,4))
    register_path=bytes(m.mem_read(0x29b070,12))
    for p,n,data in segments:
        for off in range(12,len(data)-3,4):
            word=struct.unpack_from('<I',data,off)[0]
            if word & 0xfc000000 != 0x94000000: continue
            imm=word & 0x3ffffff
            if imm & (1<<25): imm-=1<<26
            if p+off+4*imm!=0x311648: continue
            if (data[off-8:off]==load+add or data[off-12:off]==register_path):
                direct.append(p+off+4)
    assert sorted(direct)==sorted([lr for _,lr,_ in command_tails]+excluded_pool_lrs)
    for start, lr, slot in command_tails:
        if slot is not None: put(stack+slot,C)
        x(22,C);m.reg_write(A.UC_ARM64_REG_SP,stack)
        run(start,0x311648)
        assert x(0)==pool+0xd0 and x(1)==C and x(30)==lr
        assert m.reg_read(A.UC_ARM64_REG_SP)==stack
    # Full no-sync command return/recycle/pop cycle. The same C and Vk handle
    # survive the pool return. Real pool dispatch calls real Reset -> Begin and
    # appends C to the ready deque. Only Vulkan, RTTI and debug annotation are
    # synthetic external boundaries. This is not GPU completion evidence.
    cycle_seen=[];raw_reset=0;raw_begin=0
    allocate, begin, reset = region+0x61000, region+0x61004, region+0x61008
    for got, target, cell in [(0x5ae348,allocate,region+0x62000),
                              (0x5ae3e0,begin,region+0x62008),
                              (0x5aeb60,reset,region+0x62010)]:
        put(got,cell);put(cell,target)
    def cycle_hook(uc,pc,size,user):
        if pc in (0x301808,0x30180c,0x300a08,0x300a38,0x300b20,0x300c54,0x311648,0x2a23f4):
            cycle_seen.append((pc,x(0),x(19),x(30),m.reg_read(A.UC_ARM64_REG_SP),x(20)))
        if pc==allocate:
            assert x(0)==0x1234 and x(2)==C+0x738
            put(x(2),0xc001);x(0,0)
        elif pc==begin:
            assert x(0)==0xc001;x(0,raw_begin & 0xffffffff)
        elif pc==reset:
            assert x(0)==0xc001 and x(1)==0;x(0,raw_reset & 0xffffffff)
        elif pc==0x4cb40: # __dynamic_cast on this constructed synthetic object
            assert x(0)==C;x(0,C)
        elif pc not in (0x30b5f8,0x1744b0):return
        m.reg_write(A.UC_ARM64_REG_PC,x(30))
    hook=m.hook_add(UC_HOOK_CODE,cycle_hook)
    m.ctl_remove_cache(0,0x600000) # include already-translated pool/publication blocks
    put(C,0x59f8f8);put(C+8,R+0x50);put(C+0x10,0);put(C+0x738,0)
    put(R+0xa78,0x1234);put(pool+0x188,0xabc)
    x(20,C);m.reg_write(A.UC_ARM64_REG_SP,stack);run(0x301804,0x301810)
    assert [r[0] for r in cycle_seen]==[0x301808,0x300a08,0x300a38,0x30180c]
    assert get(C+0x738)==0xc001 and x(0)==1
    recycle=[]
    for (start,lr,slot),(raw_reset,raw_begin) in product(command_tails,[(0,0),(-4,0),(0,-4)]):
        cycle_seen.clear()
        put(pool+0xd0+0x40,pool+0x300)
        if slot is not None: put(stack+slot,C)
        x(22,C);m.reg_write(A.UC_ARM64_REG_SP,stack)
        m.ctl_remove_cache(0,0x600000)
        run(start,lr)
        assert cycle_seen[0][3]==lr
        assert get(pool+0x300)==C and get(C+0x738)==0xc001
        assert bytes(m.mem_read(C+0x28,1))==b'\x01'
        put(pool+0xd0,0x599c10);assert get(0x599c20)==0x2a9c88
        put(pool+0x90,pool+0x400);put(pool+0xb0,pool+0x400)
        put(pool+0xb8,pool+0x400);put(pool+0xc0,pool+0x600)
        x(0,pool+0xd0);m.reg_write(A.UC_ARM64_REG_SP,stack)
        run(0x311818,0x311898) # remaining sync-tree work is outside this witness
        assert get(pool+0xd0+0x40)==pool+0x300 # real input vector cleared even on failure
        reset_event=next(r for r in cycle_seen if r[0]==0x300b20)
        assert reset_event[1]==C and reset_event[3]==0x2a9cdc
        pcs=[r[0] for r in cycle_seen]
        assert pcs==[0x311648,0x300b20,0x300c54]+([0x300a08,0x300a38] if raw_reset==0 else [])
        assert get(C+0x738)==0xc001
        assert all(r[2]==C and r[5]==pool for r in cycle_seen if r[0]!=0x311648)
        good=raw_reset==raw_begin==0
        assert get(pool+0xb0)==pool+0x400+(8 if good else 0)
        if good:
            assert get(pool+0x400)==C and get(C+0x738)==0xc001
            x(0,pool);x(8,next_w+8);x(30,0x900ff000);m.reg_write(A.UC_ARM64_REG_SP,stack)
            run(0x304e60,0x900ff000);assert get(next_w+8)==C
            put(stack+0x98,next_w);x(19,R);m.reg_write(A.UC_ARM64_REG_SP,stack)
            run(0x2a23e8,0x2a23f8);assert get(get(R+0x1380)+8)==C
            assert bytes(m.mem_read(C+0x28,1))==b'\x00'
            assert [r[0] for r in cycle_seen]==pcs+[0x2a23f4]
        recycle.append({'caller':hex(lr),'reset':raw_reset,'begin':raw_begin,'ready':good,'sites':[hex(r[0]) for r in cycle_seen]})
    m.hook_del(hook)
    return {'release_lr':release_lr,'pool_lr':0x3052f0,'queue_lr':queue_lr,'dispatch_lrs':[lr for _,lr in callers],
            'excluded_pool_lrs':excluded_pool_lrs,
            'initial_pool_lr':command_tails[0][1],'command_pool_lrs':[lr for _,lr,_ in command_tails],
            'stock_pool_lr':stock_pool_lr,'recycle_lr':0x2a9cdc,'pool_cycle':recycle,
            'pump':pump,'outer_lr':outer_lr}


if __name__ == '__main__':
    print(bootstrap_native())
