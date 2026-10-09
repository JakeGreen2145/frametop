#!/usr/bin/env python3
# SPDX-License-Identifier: MIT
"""Pinned native startup continuations, with explicit CPU-only external boundaries.

Do not infer the live dispatch caller from the first-invalidation diagnostic: it
contains no LR. Enumerate the renderer's real direct virtual callers instead.
"""
import hashlib
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
    # Shutdown entry keeps its incoming context x0 in x20, then obtains the
    # exact worker from context+98 at the wait. Stop at external calls; their
    # implementation/results are not modeled as successful joins.
    put(context+0x98,region+0x51000)
    x(0,context);m.reg_write(A.UC_ARM64_REG_SP,stack)
    run(0x142620,0x185128);assert x(20)==context
    run(0x1426fc,0x142704);assert x(0)==region+0x51000 and x(20)==context
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
    return {'release_lr':release_lr,'pool_lr':0x3052f0,'queue_lr':queue_lr,'dispatch_lrs':[lr for _,lr in callers]}


if __name__ == '__main__':
    print(bootstrap_native())
