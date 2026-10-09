#!/usr/bin/env python3
# SPDX-License-Identifier: MIT
"""Production attachment callback -> shipped ARM64 call thunk -> pinned constructors.
Native allocation is synthetic. Output bytes are copied into distinct self-process
fixture objects with explicit pointer relocation, never host-address guest mapping.
"""
import ctypes as C
import hashlib
import importlib.util
import pathlib
import struct
import sys
from runtime_test import ROOT

def verify(library):
    sys.path.insert(0,str(ROOT/'live/build/attachment/pylib'))
    from keystone import Ks, KS_ARCH_ARM64, KS_MODE_LITTLE_ENDIAN
    from unicorn import UC_HOOK_CODE
    import unicorn.arm64_const as A
    path=pathlib.Path('/home/jakeg/.hermes/work/frame-native-live-render-analysis/analyze.py')
    spec=importlib.util.spec_from_file_location('attachment_native_analysis',path)
    assert spec and spec.loader
    p=importlib.util.module_from_spec(spec);spec.loader.exec_module(p)
    setattr(p,'ELF',pathlib.Path('/home/jakeg/.hermes/work/frame-native-reuse/camera-analysis/vrcompositor'))
    image=p.Image()
    class Native(p.Probe):
        def hook(self,uc,pc,size,user):
            # Explicit allocator boundary. Constructor instructions remain real.
            slot=self.x(8) if pc==0x2ff910 else 0
            super().hook(uc,pc,size,user)
            if slot:self.wq(self.rq(slot)+8,self.R+0x50)
    n=Native(image)
    source=(ROOT/'live/runtime-call.S').read_text()
    body=source.split('ft_native_portal_call:\n',1)[1].split('.size ',1)[0]
    thunk=n.STOP+0x200;args=n.R+0x10000;descriptor=n.R+0x11000
    code=bytes(Ks(KS_ARCH_ARM64,KS_MODE_LITTLE_ENDIAN).asm(body,addr=thunk)[0])
    n.uc.mem_write(thunk,code)
    entries=[]
    def entered(uc,pc,size,user):
        if pc in [0x296c00,0x294358]:
            entries.append(pc)
            assert n.x(0)==n.R
            assert n.x(30)==thunk+body_instructions.index('blr x16')*4+4
    body_instructions=[line.strip() for line in body.splitlines() if line.strip()]
    n.uc.hook_add(UC_HOOK_CODE,entered)
    errors=[]
    Callback=C.CFUNCTYPE(C.c_bool,C.c_uint64,C.POINTER(C.c_uint64),C.c_uint64,C.c_uint64,C.c_uint64)
    @Callback
    def construct(pc,x,wrapper,resource,marker):
        try:
            host_renderer=x[0];values=list(x[:8]);values[0]=n.R
            if pc==0x296c00:assert values[1:]==[592,0,0,0,0,0,0]
            else:
                assert pc==0x294358
                data=C.string_at(x[1],24)
                assert struct.unpack('<6I',data)==(1,1,3,1,3,0)
                n.uc.mem_write(descriptor,data);values[1]=descriptor
            n.uc.mem_write(args,struct.pack('<8Q',*values))
            # Call the actual assembly source, including argument loads, BLR,
            # returned-register stores and callee-saved register/SP restoration.
            preserved={i:0xabc000+i for i in range(19,30)}
            for i,value in preserved.items():n.x(i,value)
            assert n.run(thunk,[pc,args,0])==1
            assert n.uc.reg_read(A.UC_ARM64_REG_SP)==n.STACK+0x10000
            assert all(n.x(i)==value for i,value in preserved.items())
            result=n.rq(args)
            assert result
            if pc==0x296c00:
                native_resource=n.rq(result+8)
                assert n.ru(result+0x18)==0xffffffff and n.ru(native_resource+0xb8)==592
                assert n.rq(native_resource+8)==n.R+0x50
                w=bytearray(n.uc.mem_read(result,0x20));r=bytearray(n.uc.mem_read(native_resource,0x128))
                struct.pack_into('<Q',w,8,resource);struct.pack_into('<Q',r,8,host_renderer+0x50)
                C.memmove(wrapper,bytes(w),len(w));C.memmove(resource,bytes(r),len(r));x[0]=wrapper
            else:
                data=bytes(n.uc.mem_read(result,0x28))
                assert struct.unpack_from('<8I',data,8)==(1,1,7,0,1,7,0,15)
                C.memmove(marker,data,len(data));x[0]=marker
            return True
        except BaseException as e:
            errors.append(repr(e));return False
    lib=C.CDLL(str(library));lib.attachment_constructor.argtypes=[Callback]
    lib.attachment_constructor(construct)
    lib.main.argtypes=[C.c_int,C.POINTER(C.c_char_p)];lib.main.restype=C.c_int
    argv=(C.c_char_p*2)(b'attachment-machine',b'recover')
    result=lib.main(2,argv)
    assert not errors,errors
    assert result==0 and entries==[0x296c00,0x294358],(result,entries)
    return {'elf_sha256':p.PIN,'thunk_source_sha256':hashlib.sha256(source.encode()).hexdigest(),
            'constructor_entries':[hex(pc) for pc in entries],
            'result':'production recovery and full stereo chain PASS with pinned constructors through shipped thunk',
            'limitations':'synthetic allocation, relocated fixture objects, modeled GPU/API boundaries; no device'}
