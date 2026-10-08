#!/usr/bin/env python3
# SPDX-License-Identifier: MIT
"""Offline generated pass islands, exact pinned instruction continuations."""
import argparse
import importlib.util
import json
from pathlib import Path
import struct
import subprocess
import sys
sys.dont_write_bytecode=True
p=argparse.ArgumentParser();p.add_argument('--analysis',type=Path,required=True);p.add_argument('--elf',type=Path,required=True);a=p.parse_args()
root=Path(__file__).resolve().parents[1];build=root/'binding/build/pass';build.mkdir(parents=True,exist_ok=True)
cg=Path('/sys/fs/cgroup')/Path('/proc/self/cgroup').read_text().strip().split('::')[1].lstrip('/')
assert (cg/'memory.max').read_text().strip()=='681574400' and (cg/'memory.swap.max').read_text().strip()=='0'
assert (root/'binding/pass_emit.c').exists(), 'missing concrete pass instruction islands'
spec=importlib.util.spec_from_file_location('prior',a.analysis/'analyze.py')
assert spec and spec.loader
b=importlib.util.module_from_spec(spec);spec.loader.exec_module(b);b.ELF=a.elf
img=b.Image()
import unicorn.arm64_const as ar
from unicorn import Uc,UC_ARCH_ARM64,UC_MODE_ARM,UC_HOOK_CODE
pins=[(0x2a1e18,3),(0x2a1c18,0)]
file=build/'private-pass-pins.txt'
file.write_text(''.join(f'{off:x} {struct.unpack("<I",img.data(off,4))[0]:x} {kind}\n' for off,kind in pins))
exe=build/'pass-emit'
subprocess.run(['cc','-std=c11','-Wall','-Wextra','-Werror',str(root/'binding/pass_emit.c'),'-o',str(exe)],check=True)
rows=json.loads(subprocess.check_output([str(exe),str(file)]))
count=0
for row in rows:
    for route in (0,1):
        uc=Uc(UC_ARCH_ARM64,UC_MODE_ARM);base=row['base'];at=row['island'];pc=base+row['offset']
        uc.mem_map(base,0x700000)
        uc.mem_map(0x20000000,0x20000)
        uc.mem_write(at,bytes.fromhex(row['code']))
        uc.mem_write(pc,struct.pack('<I',row['branch']))
        regs=[0x2000+i*0x100 for i in range(31)]
        regs[19]=0x20010000;regs[1]=route if row['id']==0 else regs[1]
        uc.mem_write(regs[19]+0x2a8,b'\x7b')
        for i,val in enumerate(regs):uc.reg_write(getattr(ar,f'UC_ARM64_REG_X{i}'),val)
        vectors=[(i+1)*0x11111111111111112222222222222222 & ((1<<128)-1) for i in range(32)]
        for i,val in enumerate(vectors):uc.reg_write(getattr(ar,f'UC_ARM64_REG_Q{i}'),val)
        sp=0x20008000;uc.reg_write(ar.UC_ARM64_REG_SP,sp)
        flags={ar.UC_ARM64_REG_NZCV:0xa0000000,ar.UC_ARM64_REG_FPCR:0x400000,ar.UC_ARM64_REG_FPSR:0x8000001}
        for reg,val in flags.items():uc.reg_write(reg,val)
        target=(base+(0x2a2564 if route else 0x2a1e1c)) if row['id']==0 else base+(0x2a1c0c if route else 0x2a1c1c)
        calls=[]
        def hook(uc,address,size,user):
            if address==target:uc.emu_stop();return
            if address!=row['callback']:return
            saved=uc.reg_read(ar.UC_ARM64_REG_X0);lr=uc.reg_read(ar.UC_ARM64_REG_X30)
            assert struct.unpack('<31Q',uc.mem_read(saved,248))==tuple(regs)
            assert struct.unpack('<Q',uc.mem_read(saved+248,8))[0]==sp
            calls.append(address)
            if row['id']==1:uc.mem_write(saved+792,struct.pack('<Q',route))
            for i in range(31):uc.reg_write(getattr(ar,f'UC_ARM64_REG_X{i}'),0xdead+i)
            for i in range(32):uc.reg_write(getattr(ar,f'UC_ARM64_REG_Q{i}'),0xbad+i)
            for reg in flags:uc.reg_write(reg,0)
            uc.reg_write(ar.UC_ARM64_REG_PC,lr)
        uc.hook_add(UC_HOOK_CODE,hook)
        uc.emu_start(pc,target,count=1000)
        assert uc.reg_read(ar.UC_ARM64_REG_PC)==target and len(calls)==1
        expected=list(regs)
        if row['id']==1 and not route:expected[0]=123 # displaced ldrb executes exactly once
        assert [uc.reg_read(getattr(ar,f'UC_ARM64_REG_X{i}')) for i in range(31)]==expected
        assert [uc.reg_read(getattr(ar,f'UC_ARM64_REG_Q{i}')) for i in range(32)]==vectors
        assert uc.reg_read(ar.UC_ARM64_REG_SP)==sp
        assert all(uc.reg_read(reg)==val for reg,val in flags.items())
        count+=1
(build/'islands.json').write_text(json.dumps({'cases':count,'installed':False,'generated':rows},indent=2)+'\n')
print(f'{count} generated pass island continuation/preservation cases passed; not installed')
