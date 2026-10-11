#!/usr/bin/env python3
# SPDX-License-Identifier: MIT
"""Execute exact Desktop-saved instruction text, never open or decode an ELF.
Only wrapper factory/constructor/pool-pop, Begin, turnover-entry and disposal
islands are executed. Allocator, Vulkan and cold pool creation are explicit
synthetic boundaries. The generated factory Saved records feed the C++ consumer.
"""
import argparse,hashlib,json,pathlib,re,struct,sys
LOCAL=pathlib.Path(__file__).resolve().parent/'build/native-stock-facts/python'
if LOCAL.exists():sys.path.insert(0,str(LOCAL))
from unicorn import Uc,UC_ARCH_ARM64,UC_MODE_ARM,UC_HOOK_CODE
import unicorn.arm64_const as A
ROOT=pathlib.Path('/home/jakeg/.hermes/work/desktop-compositor-re/continuation16-verified-collector-9f10bcd93d99')
ADDITIONAL=ROOT.parent/'continuation16-additional-native-pins-ed19c700d06d/verified-payloads/ADDITIONAL-NATIVE-PINS.json'
FILES=['pass4/ins-0x3053f8.asm','pass4/ins-0x305308.asm','pass5/ins-0x302790.asm','pass7/ins-0x304e60.asm','pass4/ins-0x300a08.asm','pass4/ins-0x2a1e18.asm','pass4/ins-0x305260.asm']
X=[getattr(A,f'UC_ARM64_REG_X{i}') for i in range(31)]
R,W,C,P,S,O,D,Q=0x1000000,0x1100000,0x1200000,0x1300000,0x1400000,0x1500000,0x1600000,0x1700000

def run(out):
    uc=Uc(UC_ARCH_ARM64,UC_MODE_ARM);words={};hashes={};pages=set()
    def map_page(p):
        page=p&~4095
        if page not in pages:uc.mem_map(page,4096);pages.add(page)
    for name in FILES:
        path=ROOT/'verified-payloads/native'/name;raw=path.read_bytes();hashes[name]=hashlib.sha256(raw).hexdigest()
        for line in raw.decode().splitlines():
            m=re.fullmatch(r'([0-9a-f]{8}): ([0-9a-f]{8}) .*',line)
            if m:
                address=int(m[1],16);code=bytes.fromhex(m[2]);assert address not in words or words[address]==code
                words[address]=code;map_page(address);uc.mem_write(address,code)
    # Parent-verified narrow full window closes the former modeled epilogue and
    # exposes the actual 304f9c/304fa0 failed-creation predecessor.
    source=json.loads(ADDITIONAL.read_text())
    window=next(v for v in source['native_source_windows'] if v['elf_start']=='0x304e60')
    raw=bytes.fromhex(window['source_bytes_hex']);assert hashlib.sha256(raw).hexdigest()==window['source_bytes_sha256']
    hashes[window['file']]=window['source_bytes_sha256']
    for i in range(0,len(raw),4):
        address=int(window['elf_start'],16)+0x100000+i;code=raw[i:i+4]
        assert address not in words or words[address]==code
        words[address]=code;map_page(address);uc.mem_write(address,code)
    for p in [R,R+0x1000,W,C,P,S-4096,S,O,D,Q,0x6ae000,0x14b490,0x410ca8,0x410cf8,0x4010d8,0x411648,0x1800000,0x1900000,0x3a23e8]:map_page(p)
    def put(p,v):uc.mem_write(p,struct.pack('<Q',v))
    def reg(i,v=None):
        if v is not None:uc.reg_write(X[i],v)
        return uc.reg_read(X[i])
    def sp(v=None):
        if v is not None:uc.reg_write(A.UC_ARM64_REG_SP,v)
        return uc.reg_read(A.UC_ARM64_REG_SP)
    def pc(v):uc.reg_write(A.UC_ARM64_REG_PC,v)
    def snapshot(p):return {'pc':p-0x100000,'sp':sp(),'x':[reg(i) for i in range(31)]}
    factory_sites={0x3053f8,0x305308,0x302790,0x304e60,0x304ec8,0x304ecc,0x3053a8,0x305490,0x305470}
    records=[];failed_records=[];other=[];calls=[];phase='factory'
    def hook(u,p,n,user):
        rel=p-0x100000
        if phase=='factory' and rel in factory_sites:records.append(snapshot(p))
        if phase=='failed_factory' and rel in factory_sites:failed_records.append(snapshot(p))
        if phase not in {'factory','failed_factory'} and rel in {0x300a08,0x300a28,0x300a38,0x2a1e18,0x2a1e3c,0x305260,0x3052e0,0x3052ec,0x311648,0x3052f0}:other.append(snapshot(p))
        if p in {0x14b490,0x410ca8,0x410cf8,0x4010d8,0x411648,0x1800000,0x404c68}:
            calls.append(hex(p));reg(0,W if p==0x14b490 else 0 if p in {0x1800000,0x404c68} else 1 if p==0x411648 else 0xdecafbad);pc(reg(30));return
        if p in {0x3a23e8,0x1900000,0x3a1e40,0x3a23b4}:u.emu_stop();return
        assert p in words, f'unmodeled native instruction {p:x}'
    handle=uc.hook_add(UC_HOOK_CODE,hook)
    try:
        put(O+0x30,0);put(O+0x38,P);put(O+0x50,D)
        put(P+0x90,Q);put(P+0xb0,Q+8);put(P+0xb8,Q);put(Q,C);put(C+0x50,P)
        reg(0,O);reg(1,0xaaaa);reg(8,S+0x98);reg(30,0x3a23e8);sp(S)
        uc.emu_start(0x4053f8,0,count=2000)
        assert [x['pc'] for x in records]==[0x3053f8,0x305308,0x302790,0x304e60,0x304ec8,0x304ecc,0x3053a8,0x305490,0x305470]
        assert struct.unpack('<Q',uc.mem_read(S+0x98,8))[0]==W
        assert reg(0)==S+0x98 and reg(0)!=W
        assert struct.unpack('<Q',uc.mem_read(W+8,8))[0]==C
        phase='failed_factory';put(O+0x30,0);put(P+0x90,Q);put(P+0xb0,Q)
        reg(0,O);reg(1,0xaaaa);reg(8,S+0x98);reg(30,0x3a23e8);sp(S)
        uc.emu_start(0x4053f8,0,count=2000)
        assert 0x304ecc in [v['pc'] for v in failed_records] and 0x304ec8 not in [v['pc'] for v in failed_records]
        assert struct.unpack('<Q',uc.mem_read(W+8,8))[0]==0
        phase='begin';put(C+0x738,0xc001);put(0x6ae000+0x3e0,0x1800000+0x100);put(0x1800000+0x100,0x1800000)
        reg(0,C);reg(19,C);reg(30,0x1900000);sp(S);uc.emu_start(0x400a08,0,count=100)
        assert [(x['pc'],x['x'][0],x['sp']) for x in other]==[(0x300a08,C,S),(0x300a28,0xc001,S-0x30),(0x300a38,0,S-0x30)]
        phase='turnover';put(R+0x1380,W);reg(0,R);reg(1,0);reg(30,0x16caf0);sp(S+0xd0);uc.emu_start(0x3a1e18,0,count=100)
        assert other[-1]['pc']==0x2a1e3c and other[-1]['x'][1]==W and other[-1]['sp']==S
        phase='dispose';put(W+8,C);put(O+0x30,W);reg(0,W);reg(19,R);reg(30,0x3a23b4);sp(S);uc.emu_start(0x405260,0,count=200)
        assert [v['pc'] for v in other[-5:]]==[0x305260,0x3052e0,0x3052ec,0x311648,0x3052f0]
        assert other[-3]['x'][0]==P+0xd0 and other[-3]['x'][1]==C and other[-3]['x'][19]==W
        assert other[-1]['x'][0]&255==1
        put(W+8,0);put(O+0x30,W);reg(0,W);reg(19,R);reg(30,0x3a23b4);sp(S);uc.emu_start(0x405260,0,count=200)
        assert [v['pc'] for v in other[-3:]]==[0x305260,0x3052e0,0x3052f0]
    finally:uc.hook_del(handle)
    out.mkdir(parents=True,exist_ok=True)
    header=['// Generated from exact saved instruction execution; private CPU fixture.','struct NativeFactoryVector {uint64_t pc,sp;uint64_t x[31];};','static constexpr NativeFactoryVector factory_vectors[]={']
    for v in records:header.append('{'+hex(v['pc'])+','+hex(v['sp'])+',{'+','.join(hex(x) for x in v['x'])+'}},')
    header.append('};')
    header.append('static constexpr NativeFactoryVector failure_factory_vectors[]={')
    for v in failed_records:header.append('{'+hex(v['pc'])+','+hex(v['sp'])+',{'+','.join(hex(x) for x in v['x'])+'}},')
    header.append('};')
    (out/'native-stock-witness.hpp').write_text('\n'.join(header)+'\n')
    result={'scope':'saved text islands only, synthetic object storage; no ELF/no hardware/no installed emitter','files':hashes,'factory':records,'failed_factory':failed_records,'other':other,'synthetic_boundaries':calls,'status':'PASS'}
    (out/'witness.json').write_text(json.dumps(result,indent=2)+'\n')
    print('native stock saved-text witness: actual hidden output, constructor/factory RET, Begin Vk/raw, turnover W and disposal C/P PASS')
if __name__=='__main__':
    ap=argparse.ArgumentParser();ap.add_argument('--output',type=pathlib.Path,required=True);run(ap.parse_args().output)
