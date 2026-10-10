#!/usr/bin/env python3
# SPDX-License-Identifier: MIT
"""Three pinned camera islands plus compiled binding. CPU-only, no live loader."""
import argparse
import ctypes
import hashlib
import json
from pathlib import Path
import struct
import subprocess
import sys

ROOT=Path(__file__).resolve().parents[1]

def main():
    p=argparse.ArgumentParser()
    p.add_argument('--elf',type=Path,required=True)
    p.add_argument('--pylib',type=Path,required=True)
    args=p.parse_args()
    sys.path.insert(0,str(args.pylib))
    sys.path.insert(0,str(ROOT/'live'))
    from prepare import image
    from unicorn import Uc,UC_ARCH_ARM64,UC_MODE_ARM,UC_HOOK_CODE,UC_HOOK_INTR
    import unicorn.arm64_const as A
    at=image(args.elf)
    manifest=json.loads((ROOT/'binding/camera-sites.json').read_text())
    for row in manifest['sites']:
        raw=at(int(row['pc'],0),16)
        assert raw.hex()==row['guard']
        assert hashlib.sha256(raw).hexdigest()==row['guard_sha256']
        assert struct.unpack_from('<I',raw)[0]==int(row['word'],0)
    out=ROOT/'build/camera-gate';out.mkdir(parents=True,exist_ok=True)
    subprocess.run(['cc','-shared','-fPIC','-Wall','-Wextra','-Werror',str(ROOT/'live/runtime-emit.c'),'-o',str(out/'camera-emit.so')],check=True)
    subprocess.run(['g++','-std=c++17','-pthread','-shared','-fPIC','-Wall','-Wextra','-Werror','-O0',
                    *[str(ROOT/x) for x in ['control/camera_gate.cpp','binding/camera.cpp','binding/camera_machine.cpp']],
                    '-o',str(out/'camera-binding.so')],check=True)
    emit=ctypes.CDLL(str(out/'camera-emit.so')).ft_emit_runtime_island
    emit.argtypes=[ctypes.POINTER(ctypes.c_uint32)]+[ctypes.c_uint64]*4;emit.restype=ctypes.c_size_t
    bypass=ctypes.CDLL(str(out/'camera-emit.so')).ft_emit_camera_cleanup_bypass
    bypass.argtypes=[ctypes.POINTER(ctypes.c_uint32)]+[ctypes.c_uint64]*3;bypass.restype=ctypes.c_size_t
    probe=ctypes.CDLL(str(out/'camera-binding.so')).camera_binding_probe
    probe.argtypes=[ctypes.c_uint64,ctypes.c_void_p,ctypes.c_int];probe.restype=ctypes.c_int
    base=0x10000000;island=base+0x800000;callback=base+0x900000;sp=base+0xa10000;lr=base+0xb00000
    app=0x20000000;camera=0x30000000
    registers=[getattr(A,'UC_ARM64_REG_X'+str(i)) for i in range(29)]+[A.UC_ARM64_REG_X29,A.UC_ARM64_REG_X30]
    checks=[]
    for row in manifest['sites']:
        pc=int(row['pc'],0)
        buf=(ctypes.c_uint32*512)();size=emit(buf,island,base,pc,callback)
        assert 0<size<=2048,('missing camera emitter',hex(pc),size)
        for route in ((0,2,3) if pc==0x8dc1c else (-1,0,2,3)):
            stream=(ctypes.c_uint32*512)()
            code_size=bypass(stream,island,base,pc) if route==-1 else emit(stream,island,base,pc,callback)
            assert code_size==(8 if route==-1 else size)
            uc=Uc(UC_ARCH_ARM64,UC_MODE_ARM);uc.ctl_set_cpu_model(A.UC_CPU_ARM64_MAX)
            pages=set()
            def map_(addr,n):
                for page in range(addr&~4095,(addr+n+4095)&~4095,4096):
                    if page not in pages:uc.mem_map(page,4096);pages.add(page)
            for addr,n in [(base+pc-4,32),(island,4096),(callback,4096),(sp-4096,8192),(lr,4096),(app+0x25320,8)]:map_(addr,n)
            uc.mem_write(island,bytes(stream)[:code_size]);uc.mem_write(base+pc-4,at(pc-4,32))
            values=[0x500000+i for i in range(31)]
            values[0]=0x90000 if pc==0x8dc1c else camera
            values[19]=app;values[20]=app+0x20000;values[26]=camera;values[30]=lr
            for reg,value in zip(registers,values):uc.reg_write(reg,value)
            vectors=[(i+1)*0x0102030405060708090a0b0c0d0e0f01 for i in range(32)]
            for i,value in enumerate(vectors):uc.reg_write(getattr(A,'UC_ARM64_REG_Q'+str(i)),value)
            uc.reg_write(A.UC_ARM64_REG_SP,sp);uc.reg_write(A.UC_ARM64_REG_NZCV,0xa0000000)
            uc.reg_write(A.UC_ARM64_REG_FPCR,0x400000);uc.reg_write(A.UC_ARM64_REG_FPSR,0x10)
            method=pc!=0x8dc1c;end=lr if method and route==2 else base+pc+4;seen=[]
            def hook(machine,address,n,user):
                if pc==0x8dc1c and address==base+pc:
                    assert struct.unpack('<Q',machine.mem_read(app+0x25320,8))[0]==camera
                    machine.reg_write(A.UC_ARM64_REG_PC,island)
                elif address==callback:
                    assert machine.reg_read(A.UC_ARM64_REG_X0)==pc
                    saved=machine.reg_read(A.UC_ARM64_REG_X1);assert saved==sp-800
                    raw=bytes(machine.mem_read(saved,800));assert struct.unpack_from('<Q',raw,248)[0]==sp
                    assert struct.unpack_from('<Q',raw,792)[0]==0
                    host=ctypes.create_string_buffer(raw);assert probe(pc,host,route)==1
                    machine.mem_write(saved,host.raw[:800]);ret=machine.reg_read(A.UC_ARM64_REG_X30)
                    for reg in registers:machine.reg_write(reg,0xdead)
                    for i in range(32):machine.reg_write(getattr(A,'UC_ARM64_REG_Q'+str(i)),0)
                    machine.reg_write(A.UC_ARM64_REG_NZCV,0);machine.reg_write(A.UC_ARM64_REG_FPCR,0);machine.reg_write(A.UC_ARM64_REG_FPSR,0)
                    machine.reg_write(A.UC_ARM64_REG_PC,ret);seen.append('callback')
                elif address==end:seen.append('end');machine.emu_stop()
            def interrupt(machine,number,user):seen.append('trap');machine.emu_stop()
            uc.hook_add(UC_HOOK_CODE,hook);uc.hook_add(UC_HOOK_INTR,interrupt)
            uc.emu_start(base+pc-4 if not method else island,lr+4092,count=2000)
            trapped=route==3 or (not method and route==2)
            assert seen==([] if route==-1 else ['callback'])+['trap' if trapped else 'end'],(hex(pc),route,seen)
            if not trapped:
                expected=values.copy();expected_sp=sp
                if route==2:expected[0]=0x707
                elif method:
                    expected_sp-=32
                    assert bytes(uc.mem_read(expected_sp,16))==struct.pack('<QQ',values[29],values[30])
                else:expected[0]+=0x330
                assert [uc.reg_read(r) for r in registers]==expected,(hex(pc),route,'GPR')
                assert uc.reg_read(A.UC_ARM64_REG_SP)==expected_sp
                assert [uc.reg_read(getattr(A,'UC_ARM64_REG_Q'+str(i))) for i in range(32)]==vectors
                assert uc.reg_read(A.UC_ARM64_REG_NZCV)==0xa0000000
                if route<=0:
                    original=Uc(UC_ARCH_ARM64,UC_MODE_ARM)
                    original.mem_map((base+pc)&~4095,4096);original.mem_map(sp-4096,4096)
                    original.mem_write(base+pc,at(pc,4))
                    for reg,value in zip(registers,values):original.reg_write(reg,value)
                    for i,value in enumerate(vectors):original.reg_write(getattr(A,'UC_ARM64_REG_Q'+str(i)),value)
                    for reg,value in [(A.UC_ARM64_REG_SP,sp),(A.UC_ARM64_REG_NZCV,0xa0000000),(A.UC_ARM64_REG_FPCR,0x400000),(A.UC_ARM64_REG_FPSR,0x10)]:original.reg_write(reg,value)
                    original.emu_start(base+pc,base+pc+4,count=1)
                    for reg in registers+[A.UC_ARM64_REG_SP,A.UC_ARM64_REG_NZCV,A.UC_ARM64_REG_FPCR,A.UC_ARM64_REG_FPSR]+[getattr(A,'UC_ARM64_REG_Q'+str(i)) for i in range(32)]:
                        assert uc.reg_read(reg)==original.reg_read(reg),(hex(pc),route,'original equivalence',reg)
                    if method:assert bytes(uc.mem_read(sp-32,16))==bytes(original.mem_read(sp-32,16))
                    del original
                assert uc.reg_read(A.UC_ARM64_REG_FPCR)==0x400000 and uc.reg_read(A.UC_ARM64_REG_FPSR)==0x10
            checks.append({'pc':hex(pc),'route':route,'bytes':code_size})
            del uc
    invalid=(ctypes.c_uint32*4)()
    assert bypass(invalid,island,base,0x8dc1c)==0
    assert bypass(None,island,base,0x15df70)==0
    assert bypass(invalid,island+1,base,0x15df70)==0
    assert bypass(invalid,0x900000000,base,0x15df70)==0
    report={'pin':manifest['elf_sha256'],'checks':checks,
            'boundary':'Actual publication store and emitted islands; real compiled gate/binding; cleanup body callback synthetic; no native GPU/IPC or live coverage claim.'}
    (out/'camera-machine.json').write_text(json.dumps(report,indent=2)+'\n')
    print('camera native guards/islands: '+str(len(checks))+' cases passed; all GPR/SIMD/status and completed store checked')

if __name__=='__main__':main()
