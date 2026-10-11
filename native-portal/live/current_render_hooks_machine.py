#!/usr/bin/env python3
# SPDX-License-Identifier: MIT
"""Current delta through actual loader emitters and pinned selection blocks.

Native object/stack contents and the schedule between blocks are synthetic.
No Runtime policy, native service, GPU, or complete render execution is modeled.
"""
import ctypes
import gc
import struct
import unittest
import current_render_hooks_test as T
import stock_observer_machine as S
from unicorn import UC_HOOK_CODE
import unicorn.arm64_const as A


class Machine(S.Machine):
    def install(self,u,module,pc,base,callback):
        ident=self.ids[module,pc]
        island=base+0x600000+ident*2048
        out=(ctypes.c_uint32*512)();size=self.emit(out,island,base,ident,callback)
        assert 0<size<=2048
        u.mem_write(island,bytes(out)[:size])
        u.mem_write(base+pc,struct.pack('<I',0x14000000|(((island-base-pc)//4)&0x3ffffff)))


class CurrentMachineTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):cls.m=Machine()

    def test_displaced_instruction_full_state(self):
        checks=0
        for base in (0,0x100000000,0x7000000000):
            for pc,pin in T.PINS.items():
                for zero in (False,True):
                    vals=S.values(base);vals[30]=base+0x790100
                    vals[-3]=0x40000000 if zero else 0xa0000000
                    word=int(pin['word'],0);kind=T.L.prepare.kind(word)
                    if kind==2:vals[(word>>5)&31]=base+0x790000
                    if kind==3:vals[word&31]=0 if zero else S.DATA+0x8000
                    actual=S.emulator(base,vals);expected=S.emulator(base,vals)
                    expected.mem_write(base+pc,self.m.at['compositor'](pc,4))
                    expected.emu_start(base+pc,base+0x7ffffc,count=1)
                    stop=expected.reg_read(A.UC_ARM64_REG_PC);callback=base+0x780000
                    self.m.install(actual,'compositor',pc,base,callback);seen=[]
                    pad=bytes(actual.mem_read(S.SP-8,8))
                    row=self.m.sites[self.m.ids['compositor',pc]][1]
                    def hook(u,address,n,_):
                        if address==callback:
                            event=self.m.callback(u,vals);seen.append(event[:2])
                            self.assertEqual(event[2][792:],pad if row['emitter']=='observer' else bytes(8))
                        elif address==stop:u.emu_stop()
                    h=actual.hook_add(UC_HOOK_CODE,hook)
                    actual.emu_start(base+pc,base+0x7ffffc,count=1000)
                    self.assertEqual(seen,[('compositor',pc)])
                    self.assertEqual(actual.reg_read(A.UC_ARM64_REG_PC),stop)
                    self.assertEqual([actual.reg_read(r) for r in S.REGS],[expected.reg_read(r) for r in S.REGS])
                    self.assertEqual(actual.mem_read(S.DATA,S.SP-800-S.DATA),expected.mem_read(S.DATA,S.SP-800-S.DATA))
                    self.assertEqual(actual.mem_read(S.SP,0x10000),expected.mem_read(S.SP,0x10000))
                    actual.hook_del(h);del actual,expected;gc.collect();checks+=1
        print('PASS all 12 selected emitters, full Saved/pad/errno/native memory/register parity:',checks)

    def block(self,base,start,stop,sites,setup,verify):
        results=[]
        for installed in (False,True):
            vals=S.values(base);vals[30]=base+0x790100
            u=S.emulator(base,vals)
            u.mem_write(base+0x160000,self.m.at['compositor'](0x160000,0x3000))
            # Explicitly synthetic zero matrices select0 in the actual helper.
            u.mem_write(S.DATA,bytes(0x10000))
            setup(u)
            callback=base+0x780000;events=[];calls=[]
            if installed:
                for pc in sites:self.m.install(u,'compositor',pc,base,callback)
            def hook(m,address,n,_):
                if address==callback:events.append(self.m.callback(m))
                elif address==base+0x160a80:
                    calls.append(m.reg_read(A.UC_ARM64_REG_X30))
                    self.assertEqual(calls[-1],base+0x161bc4)
                elif address==base+stop:m.emu_stop()
            h=u.hook_add(UC_HOOK_CODE,hook)
            u.emu_start(base+start,base+0x7ffffc,count=10000)
            self.assertEqual(u.reg_read(A.UC_ARM64_REG_PC),base+stop)
            if installed:self.assertEqual([e[1] for e in events],sites)
            verify(u,events,installed,calls)
            # Observer save areas below live SP are scratch, not native objects.
            live_sp=max(S.SP,u.reg_read(A.UC_ARM64_REG_SP))
            results.append(([u.reg_read(r) for r in S.REGS],bytes(u.mem_read(S.DATA,0xd000)),bytes(u.mem_read(live_sp,S.DATA+0x20000-live_sp)),calls))
            u.hook_del(h);del u;gc.collect()
        self.assertEqual(results[0],results[1])

    def test_pinned_selector_slot_texture_chains(self):
        camera=S.DATA+0x1000;source=S.DATA+0x8000
        for base in (0,0x100000000,0x7000000000):
            def setup(u):
                u.reg_write(A.UC_ARM64_REG_X0,camera);u.reg_write(A.UC_ARM64_REG_X1,0)
                u.reg_write(A.UC_ARM64_REG_X2,source);u.reg_write(A.UC_ARM64_REG_X27,camera)
                u.reg_write(A.UC_ARM64_REG_X28,99);u.reg_write(A.UC_ARM64_REG_X24,87)
                u.reg_write(A.UC_ARM64_REG_X10,S.DATA+0xa000)
                u.mem_write(camera+0x820,struct.pack('<I',3))
            def verify(u,events,installed,calls):
                self.assertEqual(calls,[base+0x161bc4])
                self.assertEqual(u.reg_read(A.UC_ARM64_REG_X24),3)
                if installed:
                    r={e[1]:struct.unpack_from('<32Q',e[2]) for e in events}
                    self.assertEqual((r[0x161be8][0],r[0x161be8][28]),(0,99))
                    self.assertEqual(r[0x161c74][0],camera)
                    self.assertNotEqual(r[0x161c74][24],3) # no future slot fact before LDR
                    self.assertEqual((r[0x161c78][0],r[0x161c78][24]),(camera,3))
            self.block(base,0x161bc0,0x161c7c,[0x161bc0,0x161be8,0x161c74,0x161c78],setup,verify)
            for slot in (0,3,15):
                def setup_texture(u):
                    u.reg_write(A.UC_ARM64_REG_X27,camera);u.reg_write(A.UC_ARM64_REG_X21,slot)
                    u.reg_write(A.UC_ARM64_REG_X24,slot);u.reg_write(A.UC_ARM64_REG_X28,0)
                    S.putq(u,camera+0x520+24*slot,0x12345000+slot)
                def verify_texture(u,events,installed,calls):
                    self.assertEqual(u.reg_read(A.UC_ARM64_REG_X0),0x12345000+slot)
                    self.assertEqual(struct.unpack('<Q',u.mem_read(S.SP+0x380,8))[0],0x12345000+slot)
                    if installed:self.assertEqual(struct.unpack_from('<Q',events[0][2])[0],0x12345000+slot)
                self.block(base,0x161e10,0x161e44,[0x161e40],setup_texture,verify_texture)
        print('PASS actual selector BL/helper/LR, pre-MOV x0, paired slot and selected texture blocks at three bases')

    def test_connected_load_and_no_sample_return(self):
        camera=S.DATA+0x1000
        for base in (0,0x100000000,0x7000000000):
            for connected in (0,1):
                def setup(u):
                    u.reg_write(A.UC_ARM64_REG_X27,camera)
                    u.mem_write(camera+0x360,bytes([connected]))
                    S.putq(u,S.SP+8,base+0x790100)
                def verify(u,events,installed,calls):
                    self.assertEqual(calls,[])
                    if installed:
                        self.assertEqual(struct.unpack_from('<Q',events[0][2])[0],connected)
                        if not connected:
                            r=struct.unpack_from('<32Q',events[-1][2]);self.assertEqual((r[30],r[31]),(base+0x790100,S.SP+0x460))
                self.block(base,0x160ca4,0x160cac if connected else 0x790100,
                           [0x160ca8] if connected else [0x160ca8,0x160dd4],setup,verify)
        print('PASS actual connected load, true continuation and false no-sample epilogue/RET')


if __name__=='__main__':unittest.main(verbosity=2)
