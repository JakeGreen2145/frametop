#!/usr/bin/env python3
"""Differentially execute every emitted continuation at three image bases."""
import json,pathlib,random,struct,unittest
from unicorn import Uc,UcError,UC_ARCH_ARM64,UC_MODE_ARM,UC_HOOK_CODE
import unicorn.arm64_const as ar
R=pathlib.Path(__file__).resolve().parent
class Preservation(unittest.TestCase):
 def test_explicit_addresses_and_branch_range_refusal(self):
  import subprocess
  command=[str(R/'build/emit'),str(R/'build/pins.private')]
  good=subprocess.run(command+['0x200000000','0x200600000','0x200680000'],check=True,capture_output=True,text=True)
  self.assertEqual(len(json.loads(good.stdout)),9)
  for addresses in [('0','0x100000000','0x680000'),('0','0x600001','0x680000'),('0xffffffffffffffff','0x600000','0x680000')]:
   self.assertNotEqual(subprocess.run(command+list(addresses),capture_output=True).returncode,0)
 def test_every_register_and_original_continuation(self):
  cases=json.loads((R/'build/emitted.json').read_text());self.assertEqual(len(cases),27)
  rng=random.Random(527)
  for case in cases:
   for route in range(4):
    with self.subTest(site=case['id'],base=case['base'],route=route):
     base=case['base'];pc=base+case['offset'];sp=0x70010000
     engines=[Uc(UC_ARCH_ARM64,UC_MODE_ARM) for _ in range(2)]
     regs=[getattr(ar,f'UC_ARM64_REG_X{i}') for i in range(31)]+[ar.UC_ARM64_REG_Q0+i for i in range(32)]+[ar.UC_ARM64_REG_SP,ar.UC_ARM64_REG_NZCV,ar.UC_ARM64_REG_FPCR,ar.UC_ARM64_REG_FPSR]
     values=[rng.getrandbits(64) for _ in range(31)]+[rng.getrandbits(128) for _ in range(32)]+[sp,0xa0000000,0x400000,0x8000011]
     for i in (0,19,22,24):values[i]=0x70002000+i*0x100
     values[7]=base+0x690000;values[30]=base+0x690100
     memory=bytes(i%251 for i in range(0x20000))
     for u in engines:
      u.mem_map(base,0x700000);u.mem_map(0x70000000,0x20000);u.mem_write(0x70000000,memory)
      for r,v in zip(regs,values):u.reg_write(r,v)
     actual,expected=engines
     word=case['word']
     if route==1 and case['id']==2:word=0xeb1f03ff
     if route==2 or route==1 and case['id'] in (3,4):word=0xd503201f
     if route==3:word=0xd4200000
     expected.mem_write(pc,struct.pack('<I',word))
     if route!=3:
      expected.emu_start(pc,base+0x6ffffc,count=1)
      stop=expected.reg_read(ar.UC_ARM64_REG_PC)
     else:stop=None
     actual.mem_write(pc,struct.pack('<I',case['branch']));actual.mem_write(case['island'],bytes.fromhex(case['code']))
     callbacks=[]
     def hook(u,address,size,_):
      if address==case['callback']:
       ctx=u.reg_read(ar.UC_ARM64_REG_X0);lr=u.reg_read(ar.UC_ARM64_REG_X30)
       self.assertEqual(ctx,sp-800);self.assertEqual(u.reg_read(ar.UC_ARM64_REG_X1),case['id'])
       saved=struct.unpack('<32Q',u.mem_read(ctx,256));self.assertEqual(list(saved[:31]),values[:31]);self.assertEqual(saved[31],sp)
       for i in range(32):self.assertEqual(int.from_bytes(u.mem_read(ctx+256+16*i,16),'little'),values[31+i])
       self.assertEqual(struct.unpack('<3Q',u.mem_read(ctx+768,24)),tuple(values[-3:]))
       self.assertEqual(int.from_bytes(u.mem_read(ctx+792,8),'little'),0)
       u.mem_write(ctx+792,struct.pack('<Q',route))
       for reg in regs:
        if reg!=ar.UC_ARM64_REG_SP:u.reg_write(reg,0)
       u.reg_write(ar.UC_ARM64_REG_PC,lr);callbacks.append(1)
      elif stop is not None and address==stop:u.emu_stop()
     actual.hook_add(UC_HOOK_CODE,hook)
     if route==3:
      with self.assertRaises(UcError):actual.emu_start(pc,base+0x6ffffc,count=500)
     else:
      actual.emu_start(pc,base+0x6ffffc,count=500);self.assertEqual(actual.reg_read(ar.UC_ARM64_REG_PC),stop)
     self.assertEqual(callbacks,[1])
     for r in regs:self.assertEqual(actual.reg_read(r),expected.reg_read(r),f'reg {r}')
     self.assertEqual(actual.mem_read(0x70000000,sp-800-0x70000000),expected.mem_read(0x70000000,sp-800-0x70000000))
     self.assertEqual(actual.mem_read(sp,0x10000),expected.mem_read(sp,0x10000))
if __name__=='__main__':unittest.main(verbosity=2)
