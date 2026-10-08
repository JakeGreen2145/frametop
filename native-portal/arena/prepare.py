#!/usr/bin/env python3
"""Generate private pins from an exact ELF; never patch/install any process."""
import hashlib, json, pathlib, struct
PIN='9b1bd5fe4463592cbc71b2517b9ceb0a0fcd66cd6885a19dca4be0784bf6c265'
ELF=pathlib.Path('/home/jakeg/.hermes/work/frame-native-reuse/camera-analysis/vrcompositor')
R=pathlib.Path(__file__).resolve().parent
SITES=[('upload',0x161d64,2),('allocation',0x29a14c,1),('admission',0x299110,0),
       ('ubo_classifier',0x2988b0,0),('geometry_classifier',0x29f358,0),
       ('initialized',0x7fdbc,0),('reset',0x2987d4,0),('upload_return',0x161d68,0),
       ('allocation_return',0x29a150,0)]
def verify_hash(data):
    if hashlib.sha256(data).hexdigest()!=PIN: raise ValueError('unsupported compositor ELF hash')
def image(path=ELF):
    from elftools.elf.elffile import ELFFile
    import io
    data=path.read_bytes();verify_hash(data)
    return ELFFile(io.BytesIO(data))
def at(elf,address,size):
    for s in elf.iter_segments():
        if s['p_type']=='PT_LOAD' and s['p_vaddr']<=address and address+size<=s['p_vaddr']+s['p_filesz']:
            offset=address-s['p_vaddr'];return s.data()[offset:offset+size]
    raise ValueError(hex(address))
def audit(elf):
    # Decode every aligned executable word, including regions where a linear
    # disassembler stops at embedded data. Report direct branches, not alias proof.
    targets={a for _,a,_ in SITES}|{0x2990c8,0x299208,0x2993f8}
    refs={hex(a):[] for a in sorted(targets)}
    for s in elf.iter_sections():
        if not s['sh_flags']&4:continue
        data=s.data()
        for off in range(0,len(data)-3,4):
            w=struct.unpack_from('<I',data,off)[0];pc=s['sh_addr']+off
            bits=0;imm=0
            if w&0x7c000000==0x14000000:bits=26;imm=w&0x3ffffff
            elif w&0xff000010==0x54000000 or w&0x7e000000==0x34000000:bits=19;imm=(w>>5)&0x7ffff
            elif w&0x7e000000==0x36000000:bits=14;imm=(w>>5)&0x3fff
            if bits:
                if imm&(1<<(bits-1)):imm-=1<<bits
                target=pc+imm*4
                if target in targets:refs[hex(target)].append(hex(pc))
    for s in elf.iter_sections():
        if s['sh_type']=='SHT_RELA':
            for rel in s.iter_relocations():
                if rel['r_addend'] in targets:refs[hex(rel['r_addend'])].append('reloc:'+hex(rel['r_offset']))
    return refs

def generate():
    elf=image();manifest=json.loads((R/'manifest.json').read_text())
    if manifest['elf_sha256']!=PIN:raise ValueError('manifest pin mismatch')
    refs=audit(elf)
    if refs!=manifest['direct_predecessors']:raise ValueError('branch predecessor audit mismatch')
    lines=[]
    for name,address,kind in SITES:
        word=struct.unpack('<I',at(elf,address,4))[0]
        if hashlib.sha256(at(elf,address-16,36)).hexdigest()!=manifest['contexts'][name]:raise ValueError('site context mismatch')
        lines.append(f'{address:x} {word:x} {kind}\n')
    build=R/'build';build.mkdir(exist_ok=True,mode=0o700)
    (build/'pins.private').write_text(''.join(lines))
    (build/'audit.json').write_text(json.dumps(refs,indent=2)+'\n')
    return build/'pins.private'
if __name__=='__main__':print(generate())
