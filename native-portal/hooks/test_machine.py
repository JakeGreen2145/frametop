#!/usr/bin/env python3
"""Execute the production-emitted A64 islands, not a Python model of them."""
import json, pathlib, random, struct, sys, unittest
R = pathlib.Path(__file__).resolve().parent
sys.path.insert(0, str(R / 'build/pylib'))
from unicorn import Uc, UC_ARCH_ARM64, UC_MODE_ARM, UC_HOOK_CODE
from unicorn.arm64_const import *

class MachineTests(unittest.TestCase):
    def test_generated_native_continuations_preserve_full_state(self):
        manifest = R / 'build/emitted.json'
        self.assertTrue(manifest.exists(), 'production machine-code emitter has not been implemented/built')
        data = json.loads(manifest.read_text())
        rng = random.Random(418)
        for case in data:
            for zero in (False, True):
                with self.subTest(site=case['name'], base=case['base'], zero=zero):
                    u = Uc(UC_ARCH_ARM64, UC_MODE_ARM)
                    base, island, cb = case['base'], case['island'], case['callback']
                    u.mem_map(base, 0x800000)
                    u.mem_map(0x70000000, 0x20000)
                    pc = base + case['offset']
                    u.mem_write(pc, struct.pack('<I',case['branch']))
                    code = bytes.fromhex(case['code'])
                    u.mem_write(island,code)
                    xs = [rng.getrandbits(64) for _ in range(31)]
                    # All original memory operands in the fixture address a mapped scratch region.
                    for n in (0,1,19): xs[n] = 0x70008000
                    if zero and case['kind'] == 3: xs[0] = 0
                    xs[4] = base + 0x600000
                    xs[30] = base + 0x600100
                    sp = 0x70010000
                    qs = [rng.getrandbits(128) for _ in range(32)]
                    for i,v in enumerate(xs): u.reg_write(UC_ARM64_REG_X0+i if i<29 else [UC_ARM64_REG_X29,UC_ARM64_REG_X30][i-29],v)
                    for i,v in enumerate(qs): u.reg_write(UC_ARM64_REG_Q0+i,v)
                    u.reg_write(UC_ARM64_REG_SP,sp)
                    u.reg_write(UC_ARM64_REG_NZCV,0xa0000000)
                    u.reg_write(UC_ARM64_REG_FPCR,0x400000)
                    u.reg_write(UC_ARM64_REG_FPSR,0x8000011)
                    u.mem_write(0x70000000,bytes((i%251 for i in range(0x20000))))
                    expected = Uc(UC_ARCH_ARM64, UC_MODE_ARM)
                    expected.mem_map(base,0x800000); expected.mem_map(0x70000000,0x20000)
                    expected.mem_write(pc,struct.pack('<I',case['word']))
                    expected.mem_write(0x70000000,bytes(u.mem_read(0x70000000,0x20000)))
                    regs = [UC_ARM64_REG_X0+i for i in range(29)] + [UC_ARM64_REG_X29,UC_ARM64_REG_X30] + [UC_ARM64_REG_Q0+i for i in range(32)] + [UC_ARM64_REG_SP,UC_ARM64_REG_NZCV,UC_ARM64_REG_FPCR,UC_ARM64_REG_FPSR]
                    for r in regs: expected.reg_write(r,u.reg_read(r))
                    expected.emu_start(pc,base+0x7ffffc,count=1)
                    stop = expected.reg_read(UC_ARM64_REG_PC)
                    calls=[]
                    def intercept(uc,address,size,unused):
                        if address==cb:
                            self.assertEqual(uc.reg_read(UC_ARM64_REG_SP)%16,0)
                            ctx=uc.reg_read(UC_ARM64_REG_X0)
                            self.assertEqual(ctx,sp-800)
                            self.assertEqual(uc.reg_read(UC_ARM64_REG_X1),case['id'])
                            saved=struct.unpack('<32Q',uc.mem_read(ctx,256))
                            self.assertEqual(list(saved[:31]),xs)
                            self.assertEqual(saved[31],sp)
                            for i,q in enumerate(qs): self.assertEqual(int.from_bytes(uc.mem_read(ctx+256+16*i,16),'little'),q)
                            self.assertEqual(struct.unpack('<3Q',uc.mem_read(ctx+768,24)),(0xa0000000,0x400000,0x8000011))
                            lr=uc.reg_read(UC_ARM64_REG_X30)
                            # Hostile callback clobbers even callee-saved state. The bridge must restore it all.
                            for r in regs:
                                if r!=UC_ARM64_REG_SP: uc.reg_write(r,0)
                            uc.reg_write(UC_ARM64_REG_PC,lr)
                            calls.append(1)
                        elif address==stop:
                            uc.emu_stop()
                    u.hook_add(UC_HOOK_CODE,intercept)
                    u.emu_start(pc,base+0x7ffffc,count=500)
                    self.assertEqual(calls,[1])
                    self.assertEqual(u.reg_read(UC_ARM64_REG_PC),stop)
                    for r in regs: self.assertEqual(u.reg_read(r),expected.reg_read(r),f'reg {r}')
                    # Native instruction memory effect must match, except the bridge's dead stack scratch.
                    native_sp=expected.reg_read(UC_ARM64_REG_SP)
                    if native_sp!=sp:
                        self.assertEqual(u.mem_read(native_sp,16),expected.mem_read(native_sp,16))
                    self.assertEqual(u.mem_read(sp,0x1000),expected.mem_read(sp,0x1000))
                    self.assertEqual(u.mem_read(0x70008000,0x1000),expected.mem_read(0x70008000,0x1000))

if __name__=='__main__': unittest.main(verbosity=2)
