#!/usr/bin/env python3
# SPDX-License-Identifier: MIT
"""Hash-verified offline island generation. Private ELF bytes remain in build/."""
import hashlib, json, pathlib, struct, subprocess, sys
sys.path[:0]=['/home/jakeg/.hermes/work/frame-native-reuse/camera-analysis/pylib','/home/jakeg/.hermes/work/frame-native-product-design/pylib']
from elftools.elf.elffile import ELFFile
R=pathlib.Path(__file__).resolve().parent;B=R/'build';B.mkdir(exist_ok=True)
elf=pathlib.Path('/home/jakeg/.hermes/work/frame-native-reuse/camera-analysis/vrcompositor')
sha=hashlib.sha256(elf.read_bytes()).hexdigest()
assert sha=='9b1bd5fe4463592cbc71b2517b9ceb0a0fcd66cd6885a19dca4be0784bf6c265'
with elf.open('rb') as f:
 e=ELFFile(f)
 def data(at,n):
  for s in e.iter_segments():
   if s['p_type']=='PT_LOAD' and s['p_vaddr']<=at and at+n<=s['p_vaddr']+s['p_filesz']:
    f.seek(at-s['p_vaddr']+s['p_offset']);return f.read(n)
  raise ValueError(hex(at))
 pins=[0x2a2230,0x2a2320,0x2987d8,0x298840,0x300b20,0x300c54,0x300a08,0x300a38,0x3018b0]
 words=[struct.unpack('<I',data(at,4))[0] for at in pins]
 (B/'lifecycle-pins.txt').write_text(''.join('%x %x %d\n'%(at,word,3 if at in (0x300c54,0x300a38) else 0) for at,word in zip(pins,words)))
 # Enumerate real direct common-queue predecessors from executable sections.
 from capstone import Cs,CS_ARCH_ARM64,CS_MODE_ARM
 cs=Cs(CS_ARCH_ARM64,CS_MODE_ARM);refs=[]
 for section in e.iter_sections():
  if section['sh_flags']&4:
   for at,n,mn,op in cs.disasm_lite(section.data(),section['sh_addr']):
    if mn in ('b','bl') and op=='#0x3018b0':refs.append(at)
 assert refs==[0x297924,0x297c6c,0x29a4f0,0x29b040,0x29cb9c,0x30551c],refs
 reloc={r['r_offset']:r['r_addend'] for s in e.iter_sections() if s['sh_type']=='SHT_RELA' for r in s.iter_relocations()}
 assert reloc[0x59f918]==0x300b20
 (B/'lifecycle-pins.json').write_text(json.dumps({'elf_sha256':sha,'queue_routes':[hex(x) for x in refs],'reset_vtable':'0x59f918','pins':[{'pc':hex(at),'sha256':hashlib.sha256(data(at,4)).hexdigest()} for at in pins]},indent=2)+'\n')
subprocess.run(['cc','-std=c11','-Wall','-Wextra','-Werror',str(R/'lifecycle_emit.c'),'-o',str(B/'lifecycle-emit')],check=True)
with (B/'lifecycle-emitted.json').open('w') as f:subprocess.run([str(B/'lifecycle-emit'),str(B/'lifecycle-pins.txt')],stdout=f,check=True)
root=R.parent
subprocess.run(['g++','-std=c++17','-Wall','-Wextra','-Werror','-shared','-fPIC','-pthread','-I'+str(root/'include'),*[str(root/x) for x in ['binding/lifecycle_machine.cpp','binding/lifecycle.cpp','binding/command.cpp','render/adapter.cpp','src/aperture.cpp','control/protocol.cpp','control/policy.cpp']],'-o',str(B/'lifecycle-machine.so')],check=True)
print('Pinned ELF, six queue routes, reset vtable, nine generated observation islands')
