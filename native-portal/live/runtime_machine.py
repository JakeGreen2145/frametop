#!/usr/bin/env python3
# SPDX-License-Identifier: MIT
"""Execute the EIGHT actual runtime islands on Unicorn AArch64; never load a DSO into VR."""
import argparse,ctypes,hashlib,json,pathlib,struct,subprocess,sys
ROOT=pathlib.Path(__file__).resolve().parents[1]
def main():
    p=argparse.ArgumentParser();p.add_argument('--elf',type=pathlib.Path,required=True);p.add_argument('--pylib',type=pathlib.Path)
    p.add_argument('--loader',type=pathlib.Path,help='Replay the actual combined loader DSO and its callback thunk')
    args=p.parse_args()
    if args.pylib:sys.path.insert(0,str(args.pylib))
    from elftools.elf.elffile import ELFFile
    from unicorn import Uc,UC_ARCH_ARM64,UC_MODE_ARM,UC_HOOK_CODE,UC_HOOK_INTR
    import unicorn.arm64_const as A
    sys.path.insert(0,str(ROOT/'runtime'));from evidence_test import limits
    cg,report=limits();data=args.elf.read_bytes();manifest=json.loads((ROOT/'live/runtime-sites.json').read_text())
    assert hashlib.sha256(data).hexdigest()==manifest['elf_sha256']
    with args.elf.open('rb') as f:
        segments=[(x['p_vaddr'],x['p_offset'],x['p_filesz']) for x in ELFFile(f).iter_segments() if x['p_type']=='PT_LOAD']
    def original(pc,n=16):
        for va,off,size in segments:
            if va<=pc and pc+n<=va+size:return data[off+pc-va:off+pc-va+n]
        raise AssertionError(hex(pc))
    for site in manifest['sites']:assert original(site['relative_pc']).hex()==site['guard']
    build=ROOT/'live/build/runtime';build.mkdir(parents=True,exist_ok=True);library=build/'runtime-emit.so'
    subprocess.run(['cc','-std=c11','-Wall','-Wextra','-Werror','-shared','-fPIC',str(ROOT/'live/runtime-emit.c'),'-o',str(library)],check=True)
    ids={}
    if args.loader:
        rows=json.loads((args.loader.parent/'sites.private.json').read_text())['sites']
        ids={s['pc']:i for i,s in enumerate(rows)}
        emit=ctypes.CDLL(str(args.loader)).ft_loader_emit_site
        emit.argtypes=[ctypes.POINTER(ctypes.c_uint32),ctypes.c_uint64,ctypes.c_uint64,ctypes.c_uint,ctypes.c_uint64]
    else:
        emit=ctypes.CDLL(str(library)).ft_emit_runtime_island
        emit.argtypes=[ctypes.POINTER(ctypes.c_uint32)]+[ctypes.c_uint64]*4
    emit.restype=ctypes.c_size_t
    base=0x10000000;island=base+0x800000;callback=base+0x900000;sp=base+0xa10000;lr=base+0xb00000
    pcs=[0x9eeb0,0xa050c,0x161ed8,0x29f200,0x15f9e0,0x15fba8,0x4b3d0,0x142708];checks=[]
    for pc in pcs:
        buf=(ctypes.c_uint32*512)();size=emit(buf,island,base,ids[pc] if args.loader else pc,callback);assert 0<size<=(2048 if args.loader else 1024)
        for selector in [0,2,7]:
            uc=Uc(UC_ARCH_ARM64,UC_MODE_ARM);uc.mem_map(base,0x1000000);uc.mem_write(island,bytes(buf)[:size]);uc.mem_write(base+pc,original(pc))
            registers=[getattr(A,'UC_ARM64_REG_X'+str(i)) for i in range(29)]+[A.UC_ARM64_REG_X29,A.UC_ARM64_REG_X30]
            values=[0x500000+i for i in range(31)];values[4]=base+0xc00000;values[30]=lr
            for reg,value in zip(registers,values):uc.reg_write(reg,value)
            vectors=[(i+1)*0x0102030405060708090a0b0c0d0e0f01 for i in range(32)]
            for i,value in enumerate(vectors):uc.reg_write(getattr(A,'UC_ARM64_REG_Q'+str(i)),value)
            uc.reg_write(A.UC_ARM64_REG_SP,sp);uc.reg_write(A.UC_ARM64_REG_NZCV,0xa0000000);uc.reg_write(A.UC_ARM64_REG_FPCR,0x400000);uc.reg_write(A.UC_ARM64_REG_FPSR,0x10)
            method=pc in (0x15f9e0,0x15fba8);word=struct.unpack('<I',original(pc,4))[0]
            adrp=pc in (0x4b3d0,0x142708)
            if method or adrp:stock=base+pc+4
            elif pc==0x161ed8:stock=values[4]
            else:
                delta=word&0x3ffffff
                if delta&(1<<25):delta-=1<<26
                stock=base+pc+delta*4
            end=(lr if method else base+pc+4) if selector==2 else stock;seen=[]
            def hook(machine,address,size,user):
                if address==callback:
                    if args.loader:
                        assert machine.reg_read(A.UC_ARM64_REG_X1)==ids[pc]
                        saved=machine.reg_read(A.UC_ARM64_REG_X0)
                    else:
                        assert machine.reg_read(A.UC_ARM64_REG_X0)==pc
                        saved=machine.reg_read(A.UC_ARM64_REG_X1)
                    assert saved==sp-800
                    assert struct.unpack('<Q',machine.mem_read(saved+248,8))[0]==sp
                    assert struct.unpack('<Q',machine.mem_read(saved+792,8))[0]==0
                    returned=machine.reg_read(A.UC_ARM64_REG_X30)
                    machine.mem_write(saved+792,struct.pack('<Q',selector))
                    if selector==2:
                        machine.mem_write(saved,struct.pack('<Q',0x707))
                        if not method:machine.mem_write(saved+240,struct.pack('<Q',base+pc+4))
                    for reg in registers:machine.reg_write(reg,0xdead)
                    for i in range(32):machine.reg_write(getattr(A,'UC_ARM64_REG_Q'+str(i)),0)
                    machine.reg_write(A.UC_ARM64_REG_NZCV,0);machine.reg_write(A.UC_ARM64_REG_PC,returned);seen.append('callback')
                elif address==end:
                    seen.append('end');machine.emu_stop()
            def interrupt(machine,number,user):seen.append('trap');machine.emu_stop()
            uc.hook_add(UC_HOOK_CODE,hook);uc.hook_add(UC_HOOK_INTR,interrupt);uc.emu_start(island,base+0xffffff,count=2000)
            if selector==7 or (adrp and selector==2):assert seen==['callback','trap']
            else:
                assert seen==['callback','end'],(hex(pc),selector,seen)
                expected=values.copy()
                if not method and not adrp:expected[30]=base+pc+4
                if adrp:
                    page=((word>>29)&3)|(((word>>5)&0x7ffff)<<2)
                    if page&(1<<20):page-=1<<21
                    expected[word&31]=((base+pc)&~4095)+page*4096
                if selector==2:expected[0]=0x707
                assert [uc.reg_read(r) for r in registers]==expected,(hex(pc),selector,'GPR')
                assert [uc.reg_read(getattr(A,'UC_ARM64_REG_Q'+str(i))) for i in range(32)]==vectors
                assert uc.reg_read(A.UC_ARM64_REG_SP)==sp-(32 if method and selector==0 else 0)
                assert uc.reg_read(A.UC_ARM64_REG_NZCV)==0xa0000000
                assert uc.reg_read(A.UC_ARM64_REG_FPCR)==0x400000 and uc.reg_read(A.UC_ARM64_REG_FPSR)==0x10
            checks.append({'pc':hex(pc),'selector':selector,'bytes':size});del uc
    report.update(elf_sha256=manifest['elf_sha256'],guards=len(manifest['sites']),loader=str(args.loader) if args.loader else None,islands=checks,memory_peak=(cg/'memory.peak').read_text().strip())
    (build/('loader-runtime-islands.json' if args.loader else 'runtime-islands.json')).write_text(json.dumps(report,indent=2)+'\n');print(json.dumps(report|{'islands':len(checks)}))
if __name__=='__main__':main()
