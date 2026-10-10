#!/usr/bin/env python3
"""Merged production loader + all camera method routes and original bypasses."""
import ctypes,json,struct,unittest
import loader_test as L
from unicorn import Uc,UC_ARCH_ARM64,UC_MODE_ARM,UC_HOOK_CODE,UC_HOOK_INTR
import unicorn.arm64_const as A
L.compile_tests(L.ROOT/'live/runtime-sites.json')
suite=unittest.TestSuite(L.LoaderTests(n) for n in ['test_pin_and_merge_refusals','test_runtime_manifest_contract','test_build_inputs','test_actual_merged_emitters'])
assert unittest.TextTestRunner(verbosity=2).run(suite).wasSuccessful()
dll=ctypes.CDLL(str(L.B/'loader-test.so'))
emit=dll.ft_loader_emit_site;emit.argtypes=[ctypes.POINTER(ctypes.c_uint32),ctypes.c_uint64,ctypes.c_uint64,ctypes.c_uint,ctypes.c_uint64];emit.restype=ctypes.c_size_t
bypass=dll.ft_emit_camera_cleanup_bypass;bypass.argtypes=[ctypes.POINTER(ctypes.c_uint32)]+[ctypes.c_uint64]*3;bypass.restype=ctypes.c_size_t
rows=json.loads((L.B/'sites.private.json').read_text())['sites']
regs=[getattr(A,'UC_ARM64_REG_X'+str(i)) for i in range(31)]+[getattr(A,'UC_ARM64_REG_Q'+str(i)) for i in range(32)]+[A.UC_ARM64_REG_SP,A.UC_ARM64_REG_NZCV,A.UC_ARM64_REG_FPCR,A.UC_ARM64_REG_FPSR]
base=0x10000000;sp=0x70010000;at=base+0x600000;callback=base+0x780000;lr=base+0x790000
checks=[]
for ident,row in enumerate(rows):
    if row['emitter']!='return-function':continue
    pc=row['pc'];word=row['word']
    for route in ('bypass','original','return','trap'):
        buf=(ctypes.c_uint32*512)()
        size=bypass(buf,at,base,pc) if route=='bypass' else emit(buf,at,base,ident,callback)
        assert size and (route!='bypass' or size==8)
        actual=Uc(UC_ARCH_ARM64,UC_MODE_ARM);expected=Uc(UC_ARCH_ARM64,UC_MODE_ARM)
        values=[0x70002000+i*0x100 for i in range(31)]+[(i+1)*0x123456789abcdef123456789 for i in range(32)]+[sp,0xa0000000,0x400000,0x10]
        values[30]=lr
        for u in (actual,expected):
            u.mem_map(base,0x800000);u.mem_map(0x70000000,0x20000)
            u.mem_write(0x70000000,bytes(i%251 for i in range(0x20000)))
            for reg,v in zip(regs,values):u.reg_write(reg,v)
        expected.mem_write(base+pc,struct.pack('<I',word))
        if route in ('bypass','original'):expected.emu_start(base+pc,base+pc+4,count=1)
        if route=='return':expected.reg_write(A.UC_ARM64_REG_X0,0xfedcba9876543210)
        actual.mem_write(at,bytes(buf)[:size]);seen=[]
        def hook(u,address,n,_):
            if address==callback:
                assert u.reg_read(A.UC_ARM64_REG_X1)==ident
                saved=u.reg_read(A.UC_ARM64_REG_X0);assert saved==sp-800
                ret=u.reg_read(A.UC_ARM64_REG_X30)
                u.mem_write(saved+792,struct.pack('<Q',{'original':0,'return':2,'trap':3}[route]))
                if route=='return':u.mem_write(saved,struct.pack('<Q',0xfedcba9876543210))
                for reg in regs:
                    if reg!=A.UC_ARM64_REG_SP:u.reg_write(reg,0)
                u.reg_write(A.UC_ARM64_REG_PC,ret);seen.append('callback')
            elif address==(lr if route=='return' else base+pc+4):seen.append('end');u.emu_stop()
        def trap(u,n,_):seen.append('trap');u.emu_stop()
        actual.hook_add(UC_HOOK_CODE,hook);actual.hook_add(UC_HOOK_INTR,trap)
        actual.emu_start(at,base+0x7ffffc,count=1000)
        assert seen==([] if route=='bypass' else ['callback'])+['trap' if route=='trap' else 'end'],(hex(pc),route,seen)
        if route!='trap':
            assert [actual.reg_read(r) for r in regs]==[expected.reg_read(r) for r in regs],(hex(pc),route)
            native_sp=expected.reg_read(A.UC_ARM64_REG_SP)
            # SUB reserves native scratch but writes nothing. Callback save bytes
            # in that uninitialized scratch are not part of the native ABI.
            if word&0xffc00000==0xa9800000:
                assert actual.mem_read(native_sp,16)==expected.mem_read(native_sp,16),(hex(pc),route)
        checks.append([hex(pc),route]);del actual,expected
(L.B/'camera-routes.json').write_text(json.dumps(checks,indent=2)+'\n')
print('PASS merged camera method/bypass routes:',len(checks))
