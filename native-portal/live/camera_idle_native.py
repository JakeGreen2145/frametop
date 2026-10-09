#!/usr/bin/env python3
# SPDX-License-Identifier: MIT
"""Pinned camera acquire/release instructions; shared-lock/service calls are CPU fakes."""
import hashlib, json, os, struct
from pathlib import Path

def witness():
    from elftools.elf.elffile import ELFFile
    from unicorn import Uc, UC_ARCH_ARM64, UC_MODE_ARM, UC_HOOK_CODE
    import unicorn.arm64_const as A
    path=Path(os.environ['FT_HOOK_TEST_ELF'])
    assert hashlib.sha256(path.read_bytes()).hexdigest()=='9b1bd5fe4463592cbc71b2517b9ceb0a0fcd66cd6885a19dca4be0784bf6c265'
    m=Uc(UC_ARCH_ARM64,UC_MODE_ARM);m.ctl_set_cpu_model(A.UC_CPU_ARM64_MAX)
    pages=set()
    def map_(p,n):
        for page in range(p&~4095,(p+n+4095)&~4095,4096):
            if page not in pages:m.mem_map(page,4096);pages.add(page)
    with path.open('rb') as f:
        for s in ELFFile(f).iter_segments():
            if s['p_type']=='PT_LOAD':map_(s['p_vaddr'],s['p_memsz']);m.mem_write(s['p_vaddr'],s.data())
    camera,shared,service,vtable,stack,done,acquire,release=[0x90000000+i for i in (0,0x10000,0x20000,0x21000,0x40000,0x50000,0x51000,0x52000)]
    map_(camera,0x60000)
    def put(p,v):m.mem_write(p,struct.pack('<Q',v))
    def x(i,v=None):
        r=getattr(A,'UC_ARM64_REG_X'+str(i))
        if v is not None:m.reg_write(r,v)
        return m.reg_read(r)
    put(service,vtable);put(vtable+8,acquire);put(vtable+16,release)
    m.mem_write(shared+0x18,b'\1')
    calls=[];transitions=[];fail_release=False;fail_acquire=0
    def hook(uc,pc,size,user):
        if pc in (0x186a38,0x186a40,0x174510):x(0,0)
        elif pc==0x186ab0:x(0,shared)
        elif pc==0x26a548:x(0,service)
        elif pc==0x4b8c0: # constructor's libc memset, not a camera operation
            m.mem_write(x(0),bytes([x(1)&255])*x(2))
        elif pc==0x186858: # empty native bookkeeping container disposal
            assert x(0)==camera+0x4e0
        elif pc==acquire:
            assert x(1) in (camera+0x350,camera+0x358)
            handle=0xa001+(x(1)-camera-0x350)//8
            if handle!=fail_acquire:put(x(1),handle)
            calls.append(['acquire',handle]);x(0,1 if handle==fail_acquire else 0)
        elif pc==release:
            calls.append(['release',x(1)]);x(0,1 if fail_release else 0)
        else:return
        m.reg_write(A.UC_ARM64_REG_PC,x(30))
    m.hook_add(UC_HOOK_CODE,hook)
    def run(pc):
        before=state();start=len(calls)
        x(0,camera);x(1,2);x(30,done);m.reg_write(A.UC_ARM64_REG_SP,stack)
        m.emu_start(pc,done,count=10000)
        assert m.reg_read(A.UC_ARM64_REG_PC)==done
        if pc in (0x15f9e0,0x15fba8):
            transitions.append({'pc':pc,'before':before,'after':state(),
                                'result':x(0),'service_calls':calls[start:]})
        return x(0)
    def state():
        b=bytes(m.mem_read(camera+0x340,40))
        return [b[9],b[10],b[32],*struct.unpack('<QQ',b[16:32])]
    run(0x15ddc8) # actual constructor, not an assumed zeroed object
    rows=[['cold',state()]]
    assert state()==[0,0,0,0,0]
    assert run(0x15f9e0)==1;rows.append(['acquired',state()])
    assert state()==[0,1,1,0xa001,0xa002]
    before=len(calls);assert run(0x15f9e0)==1 and len(calls)==before # no per-call refcount increment
    assert run(0x15fba8)==1;rows.append(['released-warm',state()])
    assert state()==[0,0,0,0xa001,0xa002] # release takes handles BY VALUE, never clears slots
    before=len(calls);assert run(0x15fba8)==1 and len(calls)==before
    assert run(0x15f9e0)==1;rows.append(['reacquired',state()])
    fail_release=True;assert run(0x15fba8)==0;rows.append(['failed-release',state()])
    assert state()==[0,0,1,0xa001,0xa002]
    fail_release=False;assert run(0x15fba8)==1
    # A failed first connection with successful second connection does not set
    # +360. Retained handle slots make this byte-identical to released-warm.
    m.mem_write(shared+0x18,b'\0');fail_acquire=0xa001
    assert run(0x15f9e0)==0
    rows.append(['partial-connect-failure-alias',state()])
    assert state()==[0,0,0,0xa001,0xa002]
    before=len(calls);assert run(0x15fba8)==1 and len(calls)==before
    rows.append(['release-after-partial-no-disconnect',state()])
    # Finish with a genuine connected acquire/release, then execute destruction.
    # Inspection after destruction is ONLY a mapped CPU fixture observation.
    m.mem_write(shared+0x18,b'\1');fail_acquire=0
    assert run(0x15f9e0)==1 and run(0x15fba8)==1
    warm=state();before=len(calls)
    run(0x15df70)
    assert state()==warm and len(calls)==before
    rows.append(['destroyed-retains-warm-scalars',state()])
    run(0x15ddc8)
    assert state()==[0,0,0,0,0]
    rows.append(['same-address-reconstructed',state()])
    return {'states':rows,'transitions':transitions,'service_calls':calls,
            'boundary':'CPU service/lock/libc/empty-container fakes; native method bytes executed; no live camera or allocator lifetime proof'}
if __name__=='__main__':print(json.dumps(witness(),indent=2))
