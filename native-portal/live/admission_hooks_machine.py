#!/usr/bin/env python3
# SPDX-License-Identifier: MIT
"""Real selected islands versus pinned instructions. Objects/services synthetic.

No collector admission predicate, camera lifetime, GPU or complete Runtime proof.
The clamp callback is post-store, not permission to trust arbitrary arrivals.
"""
import gc
import json
import re
import struct
import unittest
import admission_hooks_test as T
import stock_observer_machine as S
from unicorn import UC_HOOK_CODE
import unicorn.arm64_const as A


class AdmissionMachineTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.m=S.Machine();cls.vectors=[];cls.instruction_checks=0
        cls.words={}
        for module in ('client','compositor'):
            path=T.PROOF/'verified-payloads'/f'{module}-closure.asm'
            rows={int(pc,16):bytes.fromhex(word) for pc,word in re.findall(r'^([0-9a-f]{8}): ([0-9a-f]{8}) ',path.read_text(),re.M)}
            assert rows
            for pc,data in rows.items():assert cls.m.at[module](pc,4)==data
            cls.words[module]=rows

    @classmethod
    def tearDownClass(cls):
        (T.L.B/'admission-native-vectors.json').write_text(json.dumps(dict(
            scope='Pinned native blocks through actual loader-selected emitters and dispatch; synthetic objects, lock/guard/clock dependencies; no consumer policy proof',
            instruction_checks=cls.instruction_checks,vectors=cls.vectors),indent=2)+'\n')

    def test_all_displaced_instructions_and_branch_outcomes(self):
        checks=0
        for base in (0,0x100000000,0x7000000000):
            for (module,pc),pin in T.PINS.items():
                word=int(pin['word'],0)
                cbz=pin['kind']==3 and word&0x7e000000==0x34000000
                conditions=[(v,0xa0000000) for v in (0,1,1<<32)] if cbz else [(1,n<<28) for n in range(16)] if pin['kind']==3 else [(1,0xa0000000)]
                destinations=set()
                for value,nzcv in conditions:
                    vals=S.values(base);vals[30]=base+0x790100;vals[-3]=nzcv
                    if cbz:vals[word&31]=value
                    actual=S.emulator(base,vals);expected=S.emulator(base,vals)
                    expected.mem_write(base+pc,self.m.at[module](pc,4))
                    expected.emu_start(base+pc,base+0x7ffffc,count=1)
                    stop=expected.reg_read(A.UC_ARM64_REG_PC);destinations.add(stop-base)
                    callback=base+0x780000
                    self.m.install(actual,module,pc,base,callback);events=[]
                    pad=bytes(actual.mem_read(S.SP-8,8))
                    def hook(u,address,n,_):
                        if address==callback:
                            event=self.m.callback(u,vals);events.append(event[:2])
                            self.assertEqual(event[2][792:],pad)
                        elif address==stop:u.emu_stop()
                    h=actual.hook_add(UC_HOOK_CODE,hook)
                    actual.emu_start(base+pc,base+0x7ffffc,count=1000)
                    self.assertEqual(events,[(module,pc)])
                    self.assertEqual(actual.reg_read(A.UC_ARM64_REG_PC),stop)
                    self.assertEqual([actual.reg_read(r) for r in S.REGS],[expected.reg_read(r) for r in S.REGS])
                    self.assertEqual(actual.mem_read(S.DATA,S.SP-800-S.DATA),expected.mem_read(S.DATA,S.SP-800-S.DATA))
                    self.assertEqual(actual.mem_read(S.SP,0x10000),expected.mem_read(S.SP,0x10000))
                    actual.hook_del(h);del actual,expected;gc.collect();checks+=1
                if pin['kind']==3:self.assertEqual(len(destinations),2,(module,hex(pc)))
        type(self).instruction_checks=checks
        print('PASS selected new islands all conditional outcomes, GPR/SIMD/SP/LR/NZCV/FPCR/FPSR/padding/memory:',checks)

    def block(self,name,module,base,start,stops,sites,setup,boundary,verify):
        outputs=[]
        for installed in (False,True):
            vals=S.values(base);vals[30]=base+0x790100
            u=S.emulator(base,vals)
            for pc,data in self.words[module].items():u.mem_write(base+pc,data)
            setup(u)
            callback=base+0x780000;events=[];trace=[]
            if installed:
                for pc in sites:self.m.install(u,module,pc,base,callback)
            def hook(m,address,n,_):
                if address==callback:events.append(self.m.callback(m))
                elif address-base in stops:m.emu_stop()
                else:boundary(m,address-base,trace)
            h=u.hook_add(UC_HOOK_CODE,hook)
            u.emu_start(base+start,base+0x7ffffc,count=5000)
            stop=u.reg_read(A.UC_ARM64_REG_PC)-base
            self.assertIn(stop,stops)
            verify(u,events,trace,installed,stop)
            # Observer frames below the minimum native SP are scratch.
            outputs.append(([u.reg_read(r) for r in S.REGS],bytes(u.mem_read(S.DATA,0xd000)),bytes(u.mem_read(S.SP,0x10000)),trace,stop))
            if installed:self.vectors.append(dict(name=name,module=module,base=hex(base),start=hex(start),stop=hex(stop),events=[dict(pc=hex(pc),saved=record.hex()) for _,pc,record in events],native_boundaries=trace))
            u.hook_del(h);del u;gc.collect()
        self.assertEqual(outputs[0],outputs[1],name)

    def test_native_ramp_store_clamp_and_join(self):
        camera=S.DATA+0x1000
        for base in (0,0x100000000,0x7000000000):
            for retained,now,ramp in ((0.,10.,None),(10.,9.9,-.4),(10.,10.,0.),(10.,10.125,.5),(10.,10.25,1.),(10.,10.5,2.)):
                def setup(u):
                    u.reg_write(A.UC_ARM64_REG_X27,camera)
                    u.mem_write(camera+0x328,struct.pack('<d',retained))
                def boundary(u,pc,trace):
                    if pc==0x185528:
                        trace.append([hex(pc),hex(u.reg_read(A.UC_ARM64_REG_X30)-base)])
                        u.reg_write(A.UC_ARM64_REG_D0,int.from_bytes(struct.pack('<d',now),'little'))
                        u.reg_write(A.UC_ARM64_REG_PC,u.reg_read(A.UC_ARM64_REG_X30))
                def verify(u,events,trace,installed,stop):
                    expected=[0x160db0]
                    if ramp is not None:expected+=[0x160e34]
                    if ramp is not None and ramp>1:expected+=[0x1621c4]
                    if ramp is not None and ramp>0:expected+=[0x160e60]
                    if installed:
                        self.assertEqual([e[1] for e in events],expected)
                        for _,pc,record in events:
                            if pc==0x160db0:self.assertEqual(struct.unpack_from('<d',record,256)[0],retained)
                            if pc==0x160e34:self.assertAlmostEqual(struct.unpack_from('<f',record,256+32)[0],ramp,places=5)
                            if pc==0x1621c4:self.assertEqual(struct.unpack_from('<f',record,256)[0],1.)
                    if ramp is not None:self.assertAlmostEqual(struct.unpack('<f',u.mem_read(S.SP+0x140,4))[0],min(ramp,1.),places=5)
                    self.assertEqual(stop,0x160db4 if ramp is None or ramp<0 else 0x161f34 if ramp==0 else 0x160e64)
                self.block(f'ramp-{retained}-{now}','compositor',base,0x160da8,{0x160db4,0x161f34,0x160e64},
                    [0x160db0,0x160e34,0x1621c4,0x160e60],setup,boundary,verify)
        print('PASS native retained load/FCMP/ramp/store/clamp B.GT predecessor/common join, zero and negative refusal paths')

    def test_arbitrary_clamp_arrival_is_not_store_evidence(self):
        # A callback alone cannot certify its predecessor. Preserve the stale s0
        # and untouched output here; only the source consumer may reject/grant.
        base=0x100000000
        sentinel=struct.pack('<f',.125)
        def setup(u):
            u.reg_write(A.UC_ARM64_REG_S0,int.from_bytes(struct.pack('<f',1.),'little'))
            u.mem_write(S.SP+0x140,sentinel)
        def verify(u,events,trace,installed,stop):
            self.assertEqual(bytes(u.mem_read(S.SP+0x140,4)),sentinel)
            if installed:
                self.assertEqual([e[1] for e in events],[0x1621c4,0x160e60])
                self.assertEqual(struct.unpack_from('<f',events[0][2],256)[0],1.)
        self.block('arbitrary-clamp-arrival-no-store','compositor',base,0x1621c4,{0x160e64},[0x1621c4,0x160e60],setup,lambda *args:None,verify)

    def test_native_mode_load_and_connected_bypass(self):
        camera=S.DATA+0x1000
        for mode in (0,1):
            for connected in (0,1):
                base=0x100000000
                def setup(u):
                    u.reg_write(A.UC_ARM64_REG_X27,camera)
                    u.reg_write(A.UC_ARM64_REG_X0,base+0x450750)
                    u.reg_write(A.UC_ARM64_REG_X1,S.SP+0x200)
                    u.reg_write(A.UC_ARM64_REG_X4,S.SP+0x200)
                    u.mem_write(camera+0x343,bytes([mode]));u.mem_write(camera+0x360,bytes([connected]))
                def verify(u,events,trace,installed,stop):
                    self.assertEqual(stop,0x160cac if mode and connected else 0x160d70)
                    if installed:
                        self.assertEqual([e[1] for e in events],[0x160ca0]+([0x160ca8] if mode else []))
                        self.assertEqual(struct.unpack_from('<Q',events[0][2],7*8)[0],mode)
                self.block(f'mode-{mode}-connected-{connected}','compositor',base,0x160c68,{0x160cac,0x160d70},[0x160ca0,0x160ca8],setup,lambda *args:None,verify)

    def test_manager_entry_exit_got_modes_and_original_calls(self):
        base=0x100000000;manager=S.DATA+0x1000
        for acquire in (False,True):
            entry,first,last,ret=(0x3280a8,0x3280e0,0x328148,0x32816c) if acquire else (0x327640,0x32766c,0x3276d0,0x3276f0)
            for got in (0,base+0x777000):
                def setup(u):
                    u.reg_write(A.UC_ARM64_REG_X0,manager);u.reg_write(A.UC_ARM64_REG_X1,99)
                    S.putq(u,base+0x6223b8,got);S.putq(u,manager+0x78,0)
                def boundary(u,pc,trace):
                    if pc in (0x8fbb0,0x8f600):
                        self.assertEqual(u.reg_read(A.UC_ARM64_REG_X0),manager+8)
                        trace.append([hex(pc),hex(u.reg_read(A.UC_ARM64_REG_X30)-base)])
                        u.reg_write(A.UC_ARM64_REG_X0,0)
                        u.reg_write(A.UC_ARM64_REG_PC,u.reg_read(A.UC_ARM64_REG_X30))
                def verify(u,events,trace,installed,stop):
                    self.assertEqual(u.reg_read(A.UC_ARM64_REG_X0),4) # missing ID is NOT clean empty imagery
                    self.assertEqual([int(t[0],0) for t in trace],[0x8fbb0,0x8f600] if got else [])
                    if installed:
                        self.assertEqual([e[1] for e in events],[first,last])
                        self.assertEqual(struct.unpack_from('<Q',events[0][2])[0],got)
                        self.assertEqual(struct.unpack_from('<Q',events[1][2],21*8)[0],got)
                        self.assertEqual(struct.unpack_from('<Q',events[0][2],248)[0],S.SP-(0x50 if acquire else 0x40))
                self.block(f'manager-{acquire}-got-{got}','client',base,entry,{0x790100},[first,last],setup,boundary,verify)

    def test_native_mode0_latest_reason_and_literal_return(self):
        base=0x100000000;queue=S.DATA+0x1000;header=S.DATA+0x3000
        for latest in (0x80,3):
            metadata=S.DATA+0x5000;data=S.DATA+0x5010
            def setup(u):
                u.reg_write(A.UC_ARM64_REG_X0,queue);u.reg_write(A.UC_ARM64_REG_X1,metadata)
                u.reg_write(A.UC_ARM64_REG_X2,data);u.reg_write(A.UC_ARM64_REG_X3,0)
                u.mem_write(header+0x14,struct.pack('<I',latest))
                S.putq(u,metadata,0x12345678);S.putq(u,data,0xabcdef00)
            def boundary(u,pc,trace):
                if pc in (0x258500,0x258578,0x258508):
                    trace.append([hex(pc),hex(u.reg_read(A.UC_ARM64_REG_X30)-base)])
                    if pc==0x258578:u.reg_write(A.UC_ARM64_REG_X0,header)
                    u.reg_write(A.UC_ARM64_REG_PC,u.reg_read(A.UC_ARM64_REG_X30))
            def verify(u,events,trace,installed,stop):
                self.assertEqual(stop,0x790100 if latest==0x80 else 0x327f00)
                if latest==0x80:self.assertEqual(u.reg_read(A.UC_ARM64_REG_X0),3)
                self.assertEqual(struct.unpack('<Q',u.mem_read(metadata,8))[0],0x12345678)
                self.assertEqual(struct.unpack('<Q',u.mem_read(data,8))[0],0xabcdef00)
                if installed:
                    self.assertEqual([e[1] for e in events],[0x327e60])
                    self.assertEqual(struct.unpack_from('<Q',events[0][2],19*8)[0],latest)
                    self.assertEqual(bool(struct.unpack_from('<Q',events[0][2],768)[0]&(1<<30)),latest==0x80)
            self.block(f'mode0-latest-{latest}','client',base,0x327e18,{0x790100,0x327f00},[0x327e60],setup,boundary,verify)


if __name__=='__main__':unittest.main(verbosity=2)
