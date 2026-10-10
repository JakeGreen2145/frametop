#!/usr/bin/env python3
"""Execute the shipped saved-state AAPCS64 call thunk; no native device calls."""
from pathlib import Path
import struct
from keystone import Ks,KS_ARCH_ARM64,KS_MODE_LITTLE_ENDIAN
from unicorn import Uc,UC_ARCH_ARM64,UC_MODE_ARM,UC_HOOK_CODE
import unicorn.arm64_const as A
source=(Path(__file__).parent/'runtime-call.S').read_text().split('ft_native_portal_call_saved:\n',1)[1].split('.size',1)[0]
code=bytes(Ks(KS_ARCH_ARM64,KS_MODE_LITTLE_ENDIAN).asm(source,addr=0x100000)[0])
u=Uc(UC_ARCH_ARM64,UC_MODE_ARM)
u.mem_map(0x100000,0x10000);u.mem_map(0x200000,0x10000)
saved=0x202000;sp=0x209000;target=0x102000;end=0x103000
x=[0x12340000+i for i in range(32)];q=[(i+1)*0x123456789abcdef123456789 for i in range(32)]
u.mem_write(saved,struct.pack('<32Q',*x)+b''.join(v.to_bytes(16,'little') for v in q)+bytes(32))
u.mem_write(0x100000,code)
u.reg_write(A.UC_ARM64_REG_SP,sp);u.reg_write(A.UC_ARM64_REG_X30,end)
u.reg_write(A.UC_ARM64_REG_X19,0xabcdef);u.reg_write(A.UC_ARM64_REG_X20,0x987654)
u.reg_write(A.UC_ARM64_REG_X0,target);u.reg_write(A.UC_ARM64_REG_X1,saved)
seen=[]
def hook(m,pc,size,user):
    if pc==target:
        assert [m.reg_read(getattr(A,'UC_ARM64_REG_X'+str(i))) for i in range(9)]==x[:9]
        assert [m.reg_read(getattr(A,'UC_ARM64_REG_Q'+str(i))) for i in range(8)]==q[:8]
        for i in range(8):
            m.reg_write(getattr(A,'UC_ARM64_REG_X'+str(i)),x[i]+0x100)
            m.reg_write(getattr(A,'UC_ARM64_REG_Q'+str(i)),q[i]+0x100)
        m.reg_write(A.UC_ARM64_REG_PC,m.reg_read(A.UC_ARM64_REG_X30));seen.append('native')
    elif pc==end:m.emu_stop();seen.append('return')
u.hook_add(UC_HOOK_CODE,hook);u.emu_start(0x100000,end+4,count=1000)
assert seen==['native','return']
assert struct.unpack('<8Q',u.mem_read(saved,64))==tuple(v+0x100 for v in x[:8])
assert [int.from_bytes(u.mem_read(saved+256+16*i,16),'little') for i in range(8)]==[v+0x100 for v in q[:8]]
assert u.reg_read(A.UC_ARM64_REG_SP)==sp and u.reg_read(A.UC_ARM64_REG_X0)==1
assert u.reg_read(A.UC_ARM64_REG_X19)==0xabcdef and u.reg_read(A.UC_ARM64_REG_X20)==0x987654
print('PASS shipped saved-state thunk: x0-x8/q0-q7 arguments, exact integer/vector results, caller/SP restoration')
