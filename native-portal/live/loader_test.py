#!/usr/bin/env python3
# SPDX-License-Identifier: MIT
"""Host-only loader execution and production AArch64 instruction replay."""
import argparse
import ctypes
import gc
import importlib.util
import json
import os
from pathlib import Path
import random
import struct
import subprocess
import sys
import unittest
import prepare

ROOT=Path(__file__).resolve().parents[1]
B=ROOT/'build/live-loader-test'
# Reuse existing installed instruction-test dependencies, never install packages.
for p in ['/home/jakeg/.hermes/work/frame-native-reuse/camera-analysis/pylib','/home/jakeg/.hermes/work/frame-native-product-design/pylib']:
    if Path(p).exists():sys.path.append(p)
from unicorn import Uc,UcError,UC_ARCH_ARM64,UC_MODE_ARM,UC_HOOK_CODE
import unicorn.arm64_const as ar

def run(args,**kw):
    print('$',' '.join(map(str,args)),flush=True)
    return subprocess.run(list(map(str,args)),check=True,timeout=180,**kw)

def compile_tests(manifest=None):
    cg=Path('/sys/fs/cgroup')/Path('/proc/self/cgroup').read_text().strip().split('::')[1].lstrip('/')
    assert (cg/'memory.max').read_text().strip() in ('268435456','681574400')
    assert (cg/'memory.swap.max').read_text().strip()=='0'
    result=prepare.prepare(prepare.ELF,B,manifest)
    (B/'loader-build.h').write_text('#define FT_BUILD_ID "'+('a'*64)+'"\n#define FT_PIPELINE_SHA "'+('b'*64)+'"\n')
    flags=['-std=c11','-O1','-g','-Wall','-Wextra','-Werror','-Wno-misleading-indentation','-Wno-unused-function','-Wno-unused-variable','-fPIC']
    objects=[]
    for i in range(4):
        obj=B/f'emit-{i}.o';objects.append(obj)
        run(['cc',*flags,'-DFT_EMITTER='+str(i),'-c',ROOT/'live/loader_emit.c','-o',obj])
    obj=B/'runtime-emit.o';objects.append(obj)
    run(['cc',*flags,'-c',ROOT/'live/runtime-emit.c','-o',obj])
    run(['cc',*flags,'-I',B,ROOT/'live/loader_test.c',*objects,'-lcrypto','-ldl','-o',B/'loader-test'])
    run([B/'loader-test',prepare.ELF])
    run(['cc',*flags,'-I',B,'-shared',ROOT/'live/loader_test.c',*objects,'-lcrypto','-ldl','-o',B/'loader-test.so'])
    # Actual production constructor with an explicitly named test-only boundary.
    (B/'loader-preload-stub.c').write_text('#include <stdint.h>\n#include <unistd.h>\n_Bool ft_native_portal_start(uint64_t b,const char*s,const char*d){(void)b;(void)s;(void)d;_exit(89);}\nvoid ft_native_portal_event(uint64_t pc,void*p){(void)pc;(void)p;_exit(89);}\nuint64_t ft_pipeline_generation(void*p){(void)p;_exit(89);}\n')
    cmd=['cc',*flags,'-DFT_LOADER_TEST','-I',B,'-shared','-Wl,-z,defs',ROOT/'live/loader.c',*objects,'-lcrypto','-ldl']
    missing=subprocess.run(list(map(str,cmd+['-o',B/'missing-router.so'])),capture_output=True,text=True)
    assert missing.returncode and 'ft_native_portal_event' in missing.stderr
    (B/'missing-router-link.txt').write_text(missing.stderr)
    run(cmd+[B/'loader-preload-stub.c','-o',B/'loader-preload-test.so'])
    for opt in [None,'wrong','a'*64]:
        env={k:v for k,v in os.environ.items() if not k.startswith('FT_NATIVE_PORTAL_')}
        env['LD_PRELOAD']=str(B/'loader-preload-test.so')
        if opt:env['FT_NATIVE_PORTAL_ENABLE']=opt
        for _ in range(3):run(['/usr/bin/true'],env=env)
    print('PASS real inert preload default-off/wrong opt-in/noncompositor, strong-link missing-router refusal')
    return result

class LoaderTests(unittest.TestCase):
    def test_pin_and_merge_refusals(self):
        at=prepare.image(prepare.ELF);specs=prepare.core_specs();rows=prepare.merge(at,specs)
        self.assertEqual(rows,prepare.merge(at,specs+specs))
        for entry in [dict(pc=0x161d64,guard='00'*16),dict(pc=0x161d64,kind=0),dict(pc=0x161d65),
                      dict(pc=0x161d64,emitter='unknown'),dict(pc=0x161d64,emitter='arena',index=1)]:
            with self.assertRaises(ValueError):prepare.merge(at,specs+[entry])
        with self.assertRaises(ValueError):prepare.merge(at,[dict(pc=0x161d64,continuation=1),dict(pc=0x161d64,continuation=2)])
        bad=B/'bad.elf';bad.write_bytes(b'not the pin')
        with self.assertRaises(ValueError):prepare.image(bad)
        with self.assertRaises(FileNotFoundError):prepare.image(B/'missing.elf')

    def test_runtime_manifest_contract(self):
        manifest=ROOT/'live/runtime-sites.json'
        runtime=json.loads(manifest.read_text())
        result=prepare.prepare(prepare.ELF,B,manifest)
        rows={s['pc']:s for s in result['sites']}
        for s in runtime['sites']:
            self.assertEqual(rows[s['relative_pc']]['emitter'],s['emitter'])
        self.assertEqual(rows[0x161ed8]['emitter'],'replace-call')
        self.assertNotIn(0x142620,rows) # main pump entry is not shutdown
        self.assertEqual(rows[0x1426d4]['emitter'],'observer')
        self.assertEqual(rows[0x1426d4]['kind'],0)
        self.assertEqual(rows[0x1426d4]['continuation'],'0x1426d8')
        bad=B/'bad-runtime.json'
        for mutation in ['missing','duplicate','emitter','continuation','abi','guard']:
            changed=json.loads(manifest.read_text())
            if mutation=='missing':changed['sites'].pop()
            elif mutation=='duplicate':changed['sites'].append(changed['sites'][0])
            elif mutation=='abi':changed['runtime_emitter']['callback_abi']='saved,id'
            else:changed['sites'][0][mutation]='invalid'
            bad.write_text(json.dumps(changed))
            with self.subTest(mutation=mutation),self.assertRaises(ValueError):
                prepare.prepare(prepare.ELF,B,bad)
        with self.assertRaises(ValueError):
            prepare.merge(prepare.image(prepare.ELF),[dict(pc=0x147a48,emitter='return-function')])

    def test_build_inputs(self):
        import build
        sources=build.inventory()
        self.assertIn('live/runtime-call.S',sources)
        hashes=build.source_hashes(sources,ROOT/'live/runtime-sites.json')
        for name in ['live/runtime-call.S','live/runtime-emit.c','runtime/graph.inc']:
            self.assertEqual(hashes[name],build.hash_file(ROOT/name))

    def test_runtime_selector_continuations(self):
        run([sys.executable,'-B',ROOT/'live/runtime_machine.py','--elf',prepare.ELF,
             '--loader',B/'loader-test.so'],env={**os.environ,'PYTHONPATH':os.pathsep.join(sys.path)})

    def test_actual_merged_emitters(self):
        dll=ctypes.CDLL(str(B/'loader-test.so'))
        emit=dll.ft_loader_emit_site
        emit.argtypes=[ctypes.POINTER(ctypes.c_uint32),ctypes.c_uint64,ctypes.c_uint64,ctypes.c_uint,ctypes.c_uint64]
        emit.restype=ctypes.c_size_t
        rows=json.loads((B/'sites.private.json').read_text())['sites']
        rng=random.Random(419)
        regs=[getattr(ar,f'UC_ARM64_REG_X{i}') for i in range(31)]+[ar.UC_ARM64_REG_Q0+i for i in range(32)]+[ar.UC_ARM64_REG_SP,ar.UC_ARM64_REG_NZCV,ar.UC_ARM64_REG_FPCR,ar.UC_ARM64_REG_FPSR]
        count=0
        for base in [0,0x100000000,0x7000000000]:
            for ident,s in enumerate(rows):
                island=base+0x600000+ident*2048;callback=base+0x780000;pc=base+s['pc'];sp=0x70010000
                output=(ctypes.c_uint32*512)();n=emit(output,island,base,ident,callback)
                self.assertTrue(0<n<=2048,(hex(s['pc']),s['emitter']))
                code=bytes(output)[:n]
                routes=range(4) if s['emitter'] in ('arena','enqueue') else range(2) if s['emitter']=='pass' and s['index']==1 else [0]
                for route in routes:
                    for zero in [False,True]:
                        with self.subTest(pc=hex(pc),route=route,zero=zero):
                            values=[0x70002000+i*0x100 for i in range(31)]+[rng.getrandbits(128) for _ in range(32)]+[sp,0 if zero else 0xa0000000,0x400000,0x8000011]
                            values[30]=base+0x790100
                            if s['pc']==0x32758c:values[24]=8 # native slot-byte offset, not a pointer
                            if s['kind']==2:values[(s['word']>>5)&31]=base+0x790000
                            if s['kind']==3 and s['word']&0x7e000000==0x34000000:values[s['word']&31]=0 if zero else 0x70008000
                            actual,expected=[Uc(UC_ARCH_ARM64,UC_MODE_ARM) for _ in range(2)]
                            for u in (actual,expected):
                                u.mem_map(base,0x800000);u.mem_map(0x70000000,0x20000)
                                u.mem_write(0x70000000,bytes(i%251 for i in range(0x20000)))
                                for r,v in zip(regs,values):u.reg_write(r,v)
                            word=s['word'];trap=False
                            if s['emitter'] in ('arena','enqueue'):
                                if route==1 and s['index']==2:word=0xeb1f03ff
                                if route==2 or route==1 and s['index'] in (3,4):word=0xd503201f
                                if route==3:trap=True
                            if s['emitter']=='pass' and s['index']==1 and route:word=0xd503201f
                            expected.mem_write(pc,struct.pack('<I',word))
                            if not trap:
                                expected.emu_start(pc,base+0x7ffffc,count=1);stop=expected.reg_read(ar.UC_ARM64_REG_PC)
                                if s['emitter']=='pass' and s['index']==1 and route:stop=base+0x2a1c0c
                            else:stop=None
                            branch=0x14000000|(((island-pc)//4)&0x3ffffff)
                            actual.mem_write(pc,struct.pack('<I',branch));actual.mem_write(island,code)
                            calls=[]
                            def intercept(u,address,size,_):
                                if address==callback:
                                    ctx=u.reg_read(ar.UC_ARM64_REG_X0);lr=u.reg_read(ar.UC_ARM64_REG_X30)
                                    self.assertEqual(ctx,sp-800);self.assertEqual(u.reg_read(ar.UC_ARM64_REG_X1),ident)
                                    self.assertEqual(list(struct.unpack('<32Q',u.mem_read(ctx,256))),values[:31]+[sp])
                                    for i in range(32):self.assertEqual(int.from_bytes(u.mem_read(ctx+256+16*i,16),'little'),values[31+i])
                                    self.assertEqual(struct.unpack('<3Q',u.mem_read(ctx+768,24)),tuple(values[-3:]))
                                    if s['emitter'] in ('arena','enqueue') or s['emitter']=='pass' and s['index']==1:
                                        self.assertEqual(bytes(u.mem_read(ctx+792,8)),bytes(8));u.mem_write(ctx+792,struct.pack('<Q',route))
                                    for r in regs:
                                        if r!=ar.UC_ARM64_REG_SP:u.reg_write(r,0)
                                    u.reg_write(ar.UC_ARM64_REG_PC,lr);calls.append(1)
                                elif stop is not None and address==stop:u.emu_stop()
                            handle=actual.hook_add(UC_HOOK_CODE,intercept)
                            if trap:
                                with self.assertRaises(UcError):actual.emu_start(pc,base+0x7ffffc,count=700)
                            else:
                                actual.emu_start(pc,base+0x7ffffc,count=700);self.assertEqual(actual.reg_read(ar.UC_ARM64_REG_PC),stop)
                            self.assertEqual(calls,[1])
                            for r in regs:self.assertEqual(actual.reg_read(r),expected.reg_read(r),f'reg {r}')
                            self.assertEqual(actual.mem_read(0x70000000,sp-800-0x70000000),expected.mem_read(0x70000000,sp-800-0x70000000))
                            self.assertEqual(actual.mem_read(sp,0x10000),expected.mem_read(sp,0x10000))
                            native_sp=expected.reg_read(ar.UC_ARM64_REG_SP)
                            # A displaced SUB SP allocates uninitialized scratch, not a
                            # native store. Compare new-frame bytes only when the
                            # displaced instruction actually writes them (STP).
                            if native_sp!=sp and s['word'] & 0x3b000000 == 0x29000000:
                                self.assertEqual(actual.mem_read(native_sp,16),expected.mem_read(native_sp,16))
                            actual.hook_del(handle);del actual,expected;gc.collect();count+=1
        print('PASS actual merged loader emitter instruction replays:',count)

if __name__=='__main__':
    p=argparse.ArgumentParser();p.add_argument('--manifest',type=Path,default=ROOT/'live/runtime-sites.json');a=p.parse_args()
    compile_tests(a.manifest)
    unittest.main(argv=[sys.argv[0]],verbosity=2)
