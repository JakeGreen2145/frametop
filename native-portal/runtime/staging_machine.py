#!/usr/bin/env python3
# SPDX-License-Identifier: MIT
"""Pinned camera caller instructions; synthetic memory, no host/guest remapping.
The C++ suite separately models the native calls over real self-process storage.
"""
import importlib.util
import pathlib
import struct
import sys
sys.dont_write_bytecode=True
WORK=pathlib.Path('/home/jakeg/.hermes/work')
spec=importlib.util.spec_from_file_location('native_analysis',WORK/'frame-native-live-render-analysis/analyze.py')
assert spec and spec.loader
p=importlib.util.module_from_spec(spec);spec.loader.exec_module(p)
setattr(p,'ELF',WORK/'frame-native-reuse/camera-analysis/vrcompositor')
img=p.Image() # hashes entire pinned ELF
# Assert the owner/argument derivation, not invented C++ native declarations.
guards={
    0x160be8:('sub','sp, sp, #0x460'),
    0x160c10:('mov','x27, x0'),
    0x160f30:('add','x22, sp, #0x300'),
    0x161d44:('add','x2, x27, #0xc0'),
    0x161d4c:('ldr','x0, [x27, #8]'),
    0x161d54:('ldr','x1, [x27, #0x2d0]'),
    0x161d60:('ldr','x7, [x7, #0x78]'),
    0x161d64:('blr','x7'),
    0x161d68:('ldr','x0, [sp, #0xa8]'),
    0x161e30:('ldr','x2, [x27, #0x2d0]'),
    0x161e34:('str','x2, [sp, #0x338]'),
    0x161e3c:('str','x2, [sp, #0x358]'),
    0x161ebc:('ldr','x0, [x27, #8]'),
    0x161ec0:('mov','x1, x22'),
    0x161ecc:('add','x21, x21, x21, lsl #1'),
    0x161ed4:('ldr','x4, [x4, #0x228]'),
    0x161ed8:('blr','x4'),
    0x161f2c:('add','sp, sp, #0x460'),
    0x161f30:('ret',''),
}
for address,expected in guards.items():
    instruction=next(img.cs.disasm(img.data(address,4),address))
    assert (instruction.mnemonic,instruction.op_str)==expected,(hex(address),instruction.op_str)
assert img.reloc[0x599c38+0x78]==0x2993f8
assert img.reloc[0x599c38+0x228]==0x29b278
for eye in (0,1):
    n=p.Probe(img);K=n.R+0x8000;W=n.R+0x9000;options=n.R+0xa000
    n.wq(n.R,0x599c38);n.wq(K+8,n.R);n.wq(K+0x2d0,W)
    n.uc.mem_write(K+0x319,b'\1');n.wu(options,1)
    entry_sp=n.STACK+0x10000
    # Execute actual native prologue: K is original x0, not a caller registry.
    n.run(0x160be8,[K,eye,options,n.R+0xb000,n.R+0xc000],stop=0x160c24)
    sp=n.uc.reg_read(p.ar.UC_ARM64_REG_SP)
    assert sp==entry_sp-0x460 and n.x(27)==K and n.x(19)==eye
    n.uc.emu_start(0x160f30,0x160f34,count=1)
    assert n.x(22)==sp+0x300
    n.uc.mem_write(sp+0xb8,struct.pack('<f',.5))
    n.uc.mem_write(sp+0x140,struct.pack('<f',.25))
    n.uc.emu_start(0x161d40,0x161d64,count=20)
    assert [n.x(i) for i in (0,1,2,7,27)]==[n.R,W,K+0xc0,0x2993f8,K]
    assert bytes(n.uc.mem_read(K+0x1fc,4))==struct.pack('<f',.125)
    # Original BLR transfers exactly once, with the original return PC.
    n.uc.emu_start(0x161d64,0x2993f8,count=1)
    assert n.x(30)==0x161d68
    n.uc.emu_start(0x161d68,0x161d6c,count=1)
    assert n.x(0)==n.R+0xb000 and n.uc.reg_read(p.ar.UC_ARM64_REG_SP)==sp
    n.uc.emu_start(0x161ebc,0x161ed8,count=10)
    assert [n.x(i) for i in (0,1,2,4,22,27)]==[n.R,sp+0x300,0x450650,0x29b278,sp+0x300,K]
    n.uc.emu_start(0x161ed8,0x29b278,count=1)
    assert n.x(30)==0x161edc
    n.uc.emu_start(0x161f00,n.STOP,count=20)
    assert n.uc.reg_read(p.ar.UC_ARM64_REG_SP)==entry_sp
    assert n.uc.reg_read(p.ar.UC_ARM64_REG_PC)==n.STOP
print(f'staging machine: pinned SHA {p.PIN}; {len(guards)} semantic guards; both eye arguments; native K/SP, upload and enqueue operands/BLR/returns preserved (synthetic storage)')
