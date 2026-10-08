#!/usr/bin/env python3
"""Replay private pinned instructions, including the Boolean-x0 no-store arm.
No ELF bytes/trace fixture are shipped. Native acquire/release bodies are not run;
the emulator stubs only their return, and verifies calls and original LR exactly.
"""
import hashlib
import json
import os
import pathlib
import struct
import sys
import tempfile
import subprocess
import unittest
from test_cache_join import R, compile_callback
sys.path.insert(0,str(R/'build/pylib'))
from unicorn import Uc, UC_ARCH_ARM64, UC_MODE_ARM, UC_HOOK_CODE
from unicorn.arm64_const import *

ELF=pathlib.Path(os.environ.get('FT_HOOK_TEST_ELF','/opt/steamvr/bin/linuxarm64/vrcompositor'))
XS=[UC_ARM64_REG_X0+i for i in range(29)]+[UC_ARM64_REG_X29,UC_ARM64_REG_X30]
REGS=XS+[UC_ARM64_REG_Q0+i for i in range(32)]+[UC_ARM64_REG_SP,UC_ARM64_REG_NZCV,UC_ARM64_REG_FPCR,UC_ARM64_REG_FPSR]
CACHE=0x4002568c

class CacheReplayTests(unittest.TestCase):
    def test_pinned_branch_and_production_callback(self):
        elf=ELF.read_bytes()
        self.assertEqual(hashlib.sha256(elf).hexdigest(),'9b1bd5fe4463592cbc71b2517b9ceb0a0fcd66cd6885a19dca4be0784bf6c265')
        cases=json.loads((R/'build/emitted.json').read_text())
        base=cases[0]['base'];sites={base+c['offset']:c for c in cases if c['base']==base and c['id'] in (15,16,25)}
        sequences={}
        for before,target in ((0,0),(0,1),(1,1),(1,0)):
            outcomes=[]
            for patched in (False,True):
                u=Uc(UC_ARCH_ARM64,UC_MODE_ARM);u.mem_map(base,0x800000);u.mem_map(0x40000000,0x80000)
                for start,size in ((0x9ee90,0x30),(0xa050c,8)):
                    u.mem_write(base+start,elf[start:start+size])
                if patched:
                    for address,c in sites.items():
                        self.assertEqual(elf[c['offset']:c['offset']+4],struct.pack('<I',c['word']))
                        u.mem_write(address,struct.pack('<I',c['branch']));u.mem_write(c['island'],bytes.fromhex(c['code']))
                for i,r in enumerate(REGS):u.reg_write(r,0x1200+i)
                u.reg_write(UC_ARM64_REG_SP,0x40040000);u.reg_write(UC_ARM64_REG_X20,0x40000000)
                u.reg_write(UC_ARM64_REG_X26,0x40060000);u.reg_write(UC_ARM64_REG_X27,target)
                u.reg_write(UC_ARM64_REG_NZCV,0xa0000000);u.reg_write(UC_ARM64_REG_FPCR,0x400000);u.reg_write(UC_ARM64_REG_FPSR,0x8000011)
                u.mem_write(CACHE,bytes([before]));u.mem_write(0x40065320,struct.pack('<Q',0x40054000))
                u.mem_write(0x40062090,struct.pack('<Q',0x123456789))
                calls=[];snapshots=[]
                def snapshot(id,values,sp):
                    snapshots.append((id,values[0],values[20],values[27],sp,u.mem_read(CACHE,1)[0]))
                def hook(uc,address,size,unused):
                    if address==base+0x9eec0:uc.emu_stop();return
                    if address in (base+0x15f9e0,base+0x15fba8):
                        lr=uc.reg_read(UC_ARM64_REG_X30)
                        self.assertEqual(lr,base+(0xa0510 if target else 0x9eeb4))
                        calls.append((address,lr));uc.reg_write(UC_ARM64_REG_X0,1);uc.reg_write(UC_ARM64_REG_PC,lr);return
                    if patched and address==cases[0]['callback']:
                        ctx=uc.reg_read(UC_ARM64_REG_X0);values=struct.unpack('<32Q',uc.mem_read(ctx,256))
                        snapshot(uc.reg_read(UC_ARM64_REG_X1),values,values[31]);lr=uc.reg_read(UC_ARM64_REG_X30)
                        for r in REGS:
                            if r!=UC_ARM64_REG_SP:uc.reg_write(r,0)
                        uc.reg_write(UC_ARM64_REG_PC,lr)
                    elif not patched and address in sites:
                        snapshot(sites[address]['id'],[uc.reg_read(r) for r in XS],uc.reg_read(UC_ARM64_REG_SP))
                u.hook_add(UC_HOOK_CODE,hook);u.emu_start(base+0x9ee90,base+0x7ffffc,count=2000)
                self.assertEqual(u.reg_read(UC_ARM64_REG_PC),base+0x9eec0)
                self.assertEqual(len(calls),int(before!=target))
                self.assertEqual(snapshots[-1][1],0x40025000 if before!=target else before)
                self.assertEqual(u.mem_read(CACHE,1)[0],target)
                outcomes.append(([u.reg_read(r) for r in REGS],bytes(u.mem_read(0x40000000,0x3fce0)),bytes(u.mem_read(0x40040000,0x40000)),calls,snapshots))
            self.assertEqual(outcomes[0],outcomes[1],f'full native/island branch differential {before}->{target}')
            sequences[before,target]=outcomes[0][-1]
        with tempfile.TemporaryDirectory(dir=R/'build',prefix='cache-replay-') as d:
            p=pathlib.Path(d);binary=compile_callback(p);lines=[]
            for branch in [(0,0)]*2000+[(0,1)]+[(1,1)]*2000+[(1,0)]+[(0,0)]*4:
                lines.append('0 40050000 40000000 0 40040000 0\n')
                for id,x0,x20,x27,sp,byte in sequences[branch]:lines.append(f'{id} {x0:x} {x20:x} {x27:x} {sp:x} {byte}\n')
                lines.append(f'24 0 0 0 40040000 {branch[1]}\n')
            (p/'trace').write_text(''.join(lines))
            result=subprocess.run([str(binary),'replay',str(p/'trace')],capture_output=True,text=True)
            self.assertEqual(result.returncode,0,result.stdout+result.stderr)
            print(result.stdout,end='');print('PASS four pinned branch paths: full register/memory/continuation and upstream call/LR differential')

if __name__=='__main__':unittest.main(verbosity=2)
