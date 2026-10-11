#!/usr/bin/env python3
# SPDX-License-Identifier: MIT
"""Execute canonical loader-selected stock islands, not a donor emitter.

Real pinned ELF bytes and saved Desktop instruction witnesses. Native objects,
TLS resolver, virtual lock/unlock and host Runtime sink are explicit CPU models.
"""
import ctypes
import gc
import json
import re
import struct
import unittest
import loader_test as L
from stock_observer_test import EXPECTED, HANDOFF
from unicorn import Uc, UC_ARCH_ARM64, UC_MODE_ARM, UC_HOOK_CODE
import unicorn.arm64_const as A

REGS=[getattr(A,'UC_ARM64_REG_X'+str(i)) for i in range(31)]+[
    getattr(A,'UC_ARM64_REG_Q'+str(i)) for i in range(32)]+[
    A.UC_ARM64_REG_SP,A.UC_ARM64_REG_NZCV,A.UC_ARM64_REG_FPCR,A.UC_ARM64_REG_FPSR]
DATA=0x70000000
SP=DATA+0x10000

def values(base):
    return [DATA+0x2000+i*0x100 for i in range(31)]+[
        (i+1)*0x123456789abcdef123456789 for i in range(32)]+[SP,0xa0000000,0x400000,0x8000011]

def emulator(base, vals):
    u=Uc(UC_ARCH_ARM64,UC_MODE_ARM)
    u.mem_map(base,0x800000);u.mem_map(DATA,0x20000)
    u.mem_write(DATA,bytes(i%251 for i in range(0x20000)))
    for reg,v in zip(REGS,vals):u.reg_write(reg,v)
    u.reg_write(A.UC_ARM64_REG_TPIDR_EL0,DATA+0x18000)
    return u

def putq(u,addr,value):u.mem_write(addr,struct.pack('<Q',value))

class Machine:
    def __init__(self):
        self.rows=json.loads((L.B/'sites.private.json').read_text())
        self.sites=[('compositor',s) for s in self.rows['sites']]+[('client',s) for s in self.rows['client_sites']]
        self.ids={(m,s['pc']):i for i,(m,s) in enumerate(self.sites)}
        self.dll=ctypes.CDLL(str(L.B/'loader-test.so'))
        self.emit=self.dll.ft_loader_emit_site
        self.emit.argtypes=[ctypes.POINTER(ctypes.c_uint32),ctypes.c_uint64,ctypes.c_uint64,ctypes.c_uint,ctypes.c_uint64]
        self.emit.restype=ctypes.c_size_t
        self.observe=self.dll.ft_loader_test_observe
        self.observe.argtypes=[ctypes.c_uint,ctypes.c_void_p,ctypes.c_uint64]
        self.at={'compositor':L.prepare.image(L.prepare.ELF),'client':L.prepare.image(L.prepare.CLIENT_ELF,L.prepare.CLIENT_PIN)}

    def install(self,u,module,pc,base,callback):
        ident=self.ids[module,pc];row=self.sites[ident][1]
        island=base+0x600000+ident*2048
        out=(ctypes.c_uint32*512)();size=self.emit(out,island,base,ident,callback)
        assert 0<size<=2048
        assert row['emitter']=='observer' and row['index']==0
        u.mem_write(island,bytes(out)[:size])
        u.mem_write(base+pc,struct.pack('<I',0x14000000|(((island-base-pc)//4)&0x3ffffff)))

    def callback(self,u,expected=None):
        ctx=u.reg_read(A.UC_ARM64_REG_X0);ident=u.reg_read(A.UC_ARM64_REG_X1)
        module,row=self.sites[ident];tag=(1<<63) if module=='client' else 0
        record=bytes(u.mem_read(ctx,800))
        assert struct.unpack_from('<Q',record,248)[0]==ctx+800
        if expected is not None:
            assert list(struct.unpack_from('<32Q',record))==expected[:31]+[expected[-4]]
            for i in range(32):assert int.from_bytes(record[256+i*16:272+i*16],'little')==expected[31+i]
            assert struct.unpack_from('<3Q',record,768)==tuple(expected[-3:])
        host=ctypes.create_string_buffer(record,800)
        assert self.observe(ident,host,tag|row['pc'])==1
        assert bytes(host)==record # includes unspecified ordinary-observer pad
        lr=u.reg_read(A.UC_ARM64_REG_X30)
        for reg in REGS:
            if reg!=A.UC_ARM64_REG_SP:u.reg_write(reg,0)
        assert bytes(u.mem_read(ctx,800))==record
        u.reg_write(A.UC_ARM64_REG_PC,lr)
        return module,row['pc'],record

    def witness(self,module,relative,provider_delta=0):
        # Parse delivered text only, then check every saved word against the
        # whole-hash/RX-authenticated ELF. A changed witness byte fails here.
        text=(HANDOFF/'verified-payloads'/relative).read_text()
        rows=[(int(pc,16)-provider_delta,bytes.fromhex(word)) for pc,word in
              re.findall(r'^([0-9a-fA-F]{8}): ([0-9a-fA-F]{8}) ',text,re.M)]
        assert rows
        for pc,data in rows:assert self.at[module](pc,4)==data,(module,hex(pc))
        return rows

class StockMachineTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):cls.m=Machine()

    def test_every_primary_and_guard_instruction(self):
        checks=0
        for base in (0,0x100000000,0x7000000000):
            for module,pc,pin in EXPECTED:
                for zero in (False,True):
                    vals=values(base);vals[30]=base+0x790100
                    word=L.prepare.number(pin['word'])
                    if pin['kind']==3:vals[word&31]=0 if zero else DATA+0x8000
                    if pc==0x2584c4:vals[0]=int(not zero);vals[30]=base+pc
                    actual=emulator(base,vals);expected=emulator(base,vals)
                    expected.mem_write(base+pc,self.m.at[module](pc,4))
                    expected.emu_start(base+pc,base+0x7ffffc,count=1)
                    stop=expected.reg_read(A.UC_ARM64_REG_PC);callback=base+0x780000
                    self.m.install(actual,module,pc,base,callback);seen=[]
                    pad=bytes(actual.mem_read(SP-8,8))
                    def hook(u,address,n,_):
                        if address==callback:
                            result=self.m.callback(u,vals)
                            self.assertEqual(result[2][792:],pad)
                            seen.append(result[:2])
                        elif address==stop:u.emu_stop()
                    h=actual.hook_add(UC_HOOK_CODE,hook)
                    actual.emu_start(base+pc,base+0x7ffffc,count=1000)
                    self.assertEqual(seen,[(module,pc)])
                    self.assertEqual(actual.reg_read(A.UC_ARM64_REG_PC),stop)
                    self.assertEqual([actual.reg_read(r) for r in REGS],[expected.reg_read(r) for r in REGS])
                    self.assertEqual(actual.mem_read(DATA,SP-800-DATA),expected.mem_read(DATA,SP-800-DATA))
                    self.assertEqual(actual.mem_read(SP,0x10000),expected.mem_read(SP,0x10000))
                    native_sp=expected.reg_read(A.UC_ARM64_REG_SP)
                    if native_sp!=SP:self.assertEqual(actual.mem_read(native_sp,16),expected.mem_read(native_sp,16))
                    actual.hook_del(h);del actual,expected;gc.collect();checks+=1
        print('PASS primary/guard full Saved+pad/dispatch/native instruction comparisons:',checks)

    def test_committed_store_and_wrapper_pool_call(self):
        base=0x100000000;callback=base+0x780000;outer=base+0x7a0000
        # Exact instructions already delivered in COMMAND-SCOPES, not new
        # disassembly. Check a producer store before observing its registers.
        anchors=json.loads((HANDOFF/'COMMAND-SCOPES.json').read_text())
        words={int(a['elf'],0):bytes.fromhex(a['bytes']) for scope in anchors for a in scope['anchors']}
        for pc in (0x304ec8,0x304ecc,0x2a23f4,0x2a23f8):
            self.assertEqual(self.m.at['compositor'](pc,4),words[pc])
        for start,site in ((0x304ec8,0x304ecc),(0x2a23f4,0x2a23f8)):
            vals=values(base);vals[30]=outer
            # Null prior wrapper chooses the real CBZ path after publication.
            if site==0x2a23f8:vals[20]=0
            u=emulator(base,vals);u.mem_write(base+start,words[start]);self.m.install(u,'compositor',site,base,callback)
            seen=[]
            def hook(m,address,n,_):
                if address==callback:
                    event=self.m.callback(m,vals);seen.append(event)
                    if site==0x304ecc:
                        self.assertEqual(struct.unpack('<Q',m.mem_read(vals[21],8))[0],vals[22])
                    else:self.assertEqual(struct.unpack('<Q',m.mem_read(vals[19]+0x1380,8))[0],vals[0])
                    m.emu_stop()
            h=u.hook_add(UC_HOOK_CODE,hook);u.emu_start(base+start,base+0x7ffffc,count=1000)
            self.assertEqual(len(seen),1);u.hook_del(h);del u;gc.collect()
        # Full delivered wrapper-destructor instructions. The pool operation is
        # an explicit opaque native boundary; its LR and real native RET are
        # checked, but it is NOT a queue/GPU/settlement or installed RET proof.
        code=self.m.witness('compositor','native/pass4/ins-0x305260.asm',0x100000)
        wrapper=DATA+0x2000;owner=DATA+0x3000;command=DATA+0x4000;pool=DATA+0x5000
        for nonnull in (False,True):
            outputs=[];traces=[]
            for installed in (False,True):
                vals=values(base);vals[0]=wrapper;vals[30]=outer
                u=emulator(base,vals)
                for pc,data in code:u.mem_write(base+pc,data)
                putq(u,wrapper,owner);putq(u,wrapper+8,command if nonnull else 0);putq(u,owner+0x30,wrapper);putq(u,command+0x50,pool)
                putq(u,base+0x5ae2f0,0) # delivered global conditional-lock flag
                # Model opaque pool body with an actual A64 RET instruction.
                # The RET word is authenticated from the delivered destructor.
                u.mem_write(base+0x311648,self.m.at['compositor'](0x3052fc,4))
                if installed:
                    for pc in (0x3052e0,0x3052ec):self.m.install(u,'compositor',pc,base,callback)
                events=[];native=[]
                def hook(m,address,n,_):
                    if address==callback:events.append(self.m.callback(m))
                    elif address==base+0x311648:
                        native.append((m.reg_read(A.UC_ARM64_REG_X0),m.reg_read(A.UC_ARM64_REG_X1),m.reg_read(A.UC_ARM64_REG_X30)))
                        self.assertEqual(native[-1],(pool+0xd0,command,base+0x3052f0))
                    elif address==base+0x3052f0:native.append(('actual-return' if nonnull else 'null-branch',m.reg_read(A.UC_ARM64_REG_SP)))
                    elif address==outer:m.emu_stop()
                h=u.hook_add(UC_HOOK_CODE,hook);u.emu_start(base+0x305260,base+0x7ffffc,count=2000)
                self.assertEqual(u.reg_read(A.UC_ARM64_REG_PC),outer)
                self.assertEqual(u.reg_read(A.UC_ARM64_REG_SP),SP)
                self.assertEqual(bytes(u.mem_read(owner+0x30,8)),bytes(8))
                if installed:
                    self.assertEqual([e[1] for e in events],[0x3052e0]+([0x3052ec] if nonnull else []))
                    r=struct.unpack_from('<32Q',events[0][2]);self.assertEqual((r[1],r[19],r[31]),(command if nonnull else 0,wrapper,SP-0x30))
                outputs.append([u.reg_read(r) for r in REGS]);traces.append(native)
                u.hook_del(h);del u;gc.collect()
            self.assertEqual(outputs[0],outputs[1]);self.assertEqual(traces[0],traces[1])
        print('PASS committed producer stores and pinned destructor null/pool-call/native-return paths')

    def test_metadata_actual_entry_lock_return_revoke(self):
        base=0x100000000;callback=base+0x780000
        tls_fn=base+0x790000;lock_fn=tls_fn+0x100;unlock_fn=tls_fn+0x200;outer=base+0x7a0000
        code=self.m.witness('client','native/client-guard/elf-00258438.asm')
        code+=self.m.witness('client','native/client-guard/elf-00258508.asm')
        guard=DATA+0x2000;owner=DATA+0x3000;lock=DATA+0x4000;vtable=DATA+0x5000;tls=DATA+0x18000
        for raw in (0,1):
            for flag in (0,1):
                traces=[];final=[]
                for installed in (False,True):
                    vals=values(base);vals[0]=guard;vals[30]=outer
                    u=emulator(base,vals)
                    for pc,data in code:u.mem_write(base+pc,data)
                    putq(u,base+0x624268,tls_fn);putq(u,guard,owner);putq(u,guard+8,0)
                    putq(u,owner+8,lock);u.mem_write(owner+0x30,bytes([flag]));putq(u,lock,vtable)
                    putq(u,vtable,lock_fn);putq(u,vtable+0x28,unlock_fn);putq(u,tls+0x20,0)
                    if installed:
                        for pc in (0x258438,0x2584c4,0x2584f8,0x258508):self.m.install(u,'client',pc,base,callback)
                    events=[];native=[]
                    def hook(m,address,n,_):
                        if address==callback:events.append(self.m.callback(m))
                        elif address in (tls_fn,lock_fn,unlock_fn):
                            lr=m.reg_read(A.UC_ARM64_REG_X30)
                            if address==tls_fn:m.reg_write(A.UC_ARM64_REG_X0,0x20)
                            else:
                                native.append((address,m.reg_read(A.UC_ARM64_REG_X0),lr))
                                self.assertEqual(m.reg_read(A.UC_ARM64_REG_X0),lock)
                                m.reg_write(A.UC_ARM64_REG_X0,raw if address==lock_fn else 0)
                            m.reg_write(A.UC_ARM64_REG_PC,lr)
                        elif address==outer:m.emu_stop()
                    h=u.hook_add(UC_HOOK_CODE,hook)
                    u.emu_start(base+0x258438,base+0x7ffffc,count=2000)
                    self.assertEqual(u.reg_read(A.UC_ARM64_REG_PC),outer)
                    # Native overwrites lock result with owner+30 regardless of
                    # success. Observational hooks must preserve that behavior.
                    self.assertEqual(u.reg_read(A.UC_ARM64_REG_X0),flag if not flag else 0x20)
                    self.assertEqual(u.reg_read(A.UC_ARM64_REG_SP),SP)
                    after_begin=[u.reg_read(r) for r in REGS]
                    u.reg_write(A.UC_ARM64_REG_X0,guard);u.reg_write(A.UC_ARM64_REG_X30,outer)
                    u.emu_start(base+0x258508,base+0x7ffffc,count=2000)
                    self.assertEqual(u.reg_read(A.UC_ARM64_REG_PC),outer)
                    traces.append(native)
                    final.append((after_begin,[u.reg_read(r) for r in REGS],bytes(u.mem_read(guard,16)),bytes(u.mem_read(tls+0x20,8))))
                    if installed:
                        self.assertEqual([e[1] for e in events],[0x258438,0x2584c4,0x2584f8,0x258508])
                        regs=[struct.unpack_from('<32Q',e[2]) for e in events]
                        self.assertEqual((regs[0][0],regs[0][30],regs[0][31]),(guard,outer,SP))
                        self.assertEqual((regs[1][0],regs[1][19],regs[1][30],regs[1][31]),(raw,guard,base+0x2584c4,SP-0x30))
                        self.assertEqual((regs[2][30],regs[2][31]),(outer,SP))
                        self.assertEqual((regs[3][0],regs[3][30],regs[3][31]),(guard,outer,SP))
                    u.hook_del(h);del u;gc.collect()
                self.assertEqual(traces[0],traces[1]);self.assertEqual(final[0],final[1])
        print('PASS pinned metadata entry/lock-result/return/revoke: false/true and both native TLS paths')

if __name__=='__main__':unittest.main(verbosity=2)
