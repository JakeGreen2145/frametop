#!/usr/bin/env python3
# SPDX-License-Identifier: MIT
"""Pin and merge launch sites. Writes private ELF bytes only below build/."""
import argparse
import hashlib
import json
from pathlib import Path
import re
import struct

ROOT = Path(__file__).resolve().parents[1]
PIN = '9b1bd5fe4463592cbc71b2517b9ceb0a0fcd66cd6885a19dca4be0784bf6c265'
ELF = Path('/home/jakeg/.hermes/work/frame-native-reuse/camera-analysis/vrcompositor')
CLIENT_ELF = ELF.with_name('vrclient.so')
CLIENT_PIN = 'fe967dcc0d97dab984bd9acc1dc522fcecbcf9beb794fdd7ec156334ec31e313'
SOURCE_COMP = [0x15fa78,0x15faa4,0x15fbd8,0x15fc20,0x15fa24,0x15fa28,0x15fa44,0x15fa48,0x15fae8,0x15faec,0x15fbec,0x15fbf0,0x15fc34,0x15fc38]
SOURCE_CLIENT = [0x32a270,0x32a398,0x32a39c,0x32a49c,0x32a504,
    0x32750c,0x327e54,0x3280a8,0x32813c,0x328140,0x327640,0x3276c4,0x3276c8,
    0x327e18,0x327f2c,0x327f30,0x327f70,0x327f74,0x3274d8,0x32758c,0x327590,0x3275a0,0x3275a4]
# Verified continuation16 observations. Bit order is the public v1 coverage ABI.
# These are scalar observations, not a complete command-provenance certificate.
STOCK_OBSERVERS = [
    ('command_output', 'compositor', 0x304ecc, 0, '947a41f9740000b4e00318aaae1cf597'),
    ('publication_committed', 'compositor', 0x2a23f8, 3, 'b40100b4e00314aa988b0194e00314aa'),
    ('wrapper_command', 'compositor', 0x3052e0, 3, '810000b4202840f900400391d7300094'),
    ('pool_return_call', 'compositor', 0x3052ec, 1, 'd7300094f35341a9f51340f9fd7bc3a8'),
    ('metadata_lock_result', 'client', 0x2584c4, 0, '610240f920c0403900010034601e0090'),
    ('metadata_guard_begin', 'client', 0x258438, 0, 'fd7bbda9fd030091f35301a954d03bd5'),
    ('metadata_guard_return', 'client', 0x2584f8, 5, 'c0035fd61f2003d5017c00a9cdffff17'),
    ('metadata_guard_revoke', 'client', 0x258508, 0, '41d03bd5e20300aafd7bbfa9601e0090'),
]
SOURCE_CLIENT += [pc for _,module,pc,_,_ in STOCK_OBSERVERS if module=='client']
SOURCE_COMP += [0x15fdc8,0x15fee4,0x15fee8,0x160164,0x160168,0x160184,0x160188,0x160380,0x160384,0x1605c4,0x1605c8]
ARENA = [0x161d64,0x29a14c,0x299110,0x2988b0,0x29f358,0x7fdbc,0x2987d4,0x161d68,0x29a150]
PASS = [0x2a1e18,0x2a1c18]
LIFECYCLE = [0x2a2230,0x2a2320,0x2987d8,0x298840,0x300b20,0x300c54,0x300a08,0x300a38,0x3018b0]
EVIDENCE = [0x7fda0,0x301808,0x30180c,0x3003d0,0x311648,0x4b3d0,0x305260,0x2953a0,
            0x29bd18,0x29c238,0x2966a0,0x294678,0x2fe090,0x29a528,0x161f00]
EMITTERS = {'observer':0, 'arena':1, 'pass':2, 'lifecycle':3, 'enqueue':4,
            'replace-call':5, 'return-function':6, 'observer-relocated':7, 'camera-decision':8}
RUNTIME = {'replace-call':[0x9eeb0,0xa050c,0x161ed8,0x29f200],
           'camera-decision':[0x15fbb8,0x15fb20],
           'return-function':[0x8b020,0x15e1c8,0x15df70,0x162360,0x6d6e0,0x6d6f0,0x1366b0,0x136118,0x13a100,0x15d730,0x15d198,0x15d248,0x15f938,0x15faf8,0x15fcc0,0x15f9e0,0x15fba8], 'observer-relocated':[0x4b3d0,0x142708]}

def number(v):
    if isinstance(v, bool): raise ValueError('boolean address')
    return int(v, 0) if isinstance(v, str) else int(v)

def digest(data): return hashlib.sha256(data).hexdigest()

def image(path, pin=PIN):
    data = Path(path).read_bytes()
    if digest(data) != pin: raise ValueError('unsupported module ELF hash')
    if data[:6] != b'\x7fELF\x02\x01' or struct.unpack_from('<H',data,18)[0] != 183:
        raise ValueError('not little-endian AArch64 ELF')
    off = struct.unpack_from('<Q',data,32)[0]
    size,count = struct.unpack_from('<HH',data,54)
    segments = [struct.unpack_from('<IIQQQQQQ',data,off+i*size) for i in range(count)]
    def at(pc,n):
        for typ,flags,offset,va,_,filesz,_,_ in segments:
            if typ == 1 and flags == 5 and va <= pc and pc+n <= va+filesz:
                return data[offset+pc-va:offset+pc-va+n]
        raise ValueError('site outside exact RX file span: '+hex(pc))
    return at

def kind(word):
    if word & 0xfc000000 == 0x94000000: return 1
    if word & 0xfffffc1f == 0xd63f0000:
        if (word >> 5) & 31 == 30: raise ValueError('BLR LR cannot preserve original target')
        return 2
    if word & 0xff000010 == 0x54000000 or word & 0x7e000000 == 0x34000000: return 3
    if word & 0xfc000000 == 0x14000000: return 4
    if word & 0xfffffc1f in (0xd65f0000,0xd61f0000): return 5
    if word & 0x1f000000 == 0x10000000: return 6  # ADR/ADRP, explicit relocation
    if word & 0x7e000000 == 0x36000000 or word & 0x3b000000 == 0x18000000:
        raise ValueError('unsupported PC-relative instruction')
    return 0

def core_specs():
    text = (ROOT/'hooks/pins.h').read_text()
    specs=[]
    for name,pc,word,k,guard in re.findall(r'\{"([^"]+)",(0x[0-9a-f]+),(0x[0-9a-f]+),(\d+),\{([^}]+)\}\}', text):
        specs.append(dict(name=name,pc=pc,kind=int(k),guard=bytes(int(x,0) for x in guard.split(',')).hex(),emitter='observer'))
    for emitter,pcs in [('arena',ARENA),('pass',PASS),('lifecycle',LIFECYCLE)]:
        specs += [dict(name=emitter+'_'+hex(pc),pc=pc,emitter=emitter,index=i) for i,pc in enumerate(pcs)]
    specs += [dict(name='evidence_'+hex(pc),pc=pc,emitter='observer') for pc in EVIDENCE]
    specs += [dict(name='source_'+hex(pc),pc=pc,emitter='observer') for pc in SOURCE_COMP]
    # The enqueue skip has the same restored Original/SkipOwned routes as upload.
    specs.append(dict(name='camera_enqueue_owned',pc=0x161ed8,emitter='enqueue',index=0))
    return specs

def merge(at, specs):
    merged={}
    for raw in specs:
        pc=number(raw.get('pc',raw.get('relative_pc',raw.get('offset'))))
        if pc <= 0 or pc & 3: raise ValueError('unaligned/invalid PC')
        guard=at(pc,16);word=struct.unpack_from('<I',guard)[0];k=kind(word)
        supplied=raw.get('guard',raw.get('guard_hex'))
        if supplied is not None and bytes.fromhex(supplied) != guard: raise ValueError('guard mismatch '+hex(pc))
        if 'guard_sha256' in raw and raw['guard_sha256'] != digest(guard): raise ValueError('guard hash mismatch')
        if 'word' in raw and number(raw['word']) != word: raise ValueError('word mismatch')
        if 'kind' in raw and number(raw['kind']) != k: raise ValueError('relocation kind mismatch '+hex(pc))
        emitter=raw.get('emitter','observer')
        if emitter not in EMITTERS: raise ValueError('unknown emitter '+str(emitter))
        index=number(raw.get('index',0))
        allowed={'arena':ARENA,'pass':PASS,'lifecycle':LIFECYCLE,'enqueue':[0x161ed8],**RUNTIME}
        if emitter in allowed:
            if pc not in allowed[emitter]: raise ValueError('unsupported specialized PC')
            expected=allowed[emitter].index(pc)
            if 'index' in raw and index != expected: raise ValueError('emitter index conflict')
            index=expected
        continuation=raw.get('continuation')
        row=dict(pc=pc,word=word,kind=k,guard=guard.hex(),emitter=emitter,index=index,
                 names=[raw.get('name',hex(pc))])
        if continuation is not None: row['continuation']=continuation
        if pc in merged:
            old=merged[pc]
            if old['emitter'] != emitter:
                if old['emitter']=='observer': pass
                elif emitter=='observer': row['emitter']=old['emitter'];row['index']=old['index']
                elif pc==0x161ed8 and {old['emitter'],emitter}=={'enqueue','replace-call'}:
                    # The router synchronously replaces this call and supplies LR.
                    # Its exact runtime emitter supersedes the older arena skip.
                    if emitter=='enqueue':row['emitter']=old['emitter'];row['index']=old['index']
                else: raise ValueError('conflicting emitters '+hex(pc))
            if 'continuation' in old and continuation is not None and old['continuation'] != continuation:
                raise ValueError('conflicting continuations '+hex(pc))
            if 'continuation' in old: row['continuation']=old['continuation']
            row['names']=sorted(set(old['names']+row['names']))
        merged[pc]=row
    if not merged or len(merged)>256: raise ValueError('invalid site count')
    return [merged[pc] for pc in sorted(merged)]

def stock_observer_coverage(rows, client_rows, comp_pin=PIN, client_pin=CLIENT_PIN):
    mask=0
    if comp_pin==PIN and client_pin==CLIENT_PIN:
        for bit,(_,module,pc,k,guard) in enumerate(STOCK_OBSERVERS):
            selected=client_rows if module=='client' else rows
            exact=[s for s in selected if s['pc']==pc]
            if len(exact)!=1: continue
            s=exact[0]
            if (s['word']==int.from_bytes(bytes.fromhex(guard[:8]),'little') and
                s['kind']==k and s['guard']==guard and
                s['emitter']=='observer' and s['index']==0):
                mask |= 1<<bit
    return dict(version=1,selected_mask=mask,required_mask=(1<<len(STOCK_OBSERVERS))-1,
                scope='observer-sites-only')

def prepare(elf, output, manifest=None, client=CLIENT_ELF):
    output=Path(output).resolve()
    if ROOT/'build' not in output.parents: raise ValueError('output must be below native-portal/build')
    at=image(elf);specs=core_specs();runtime=None
    if manifest:
        runtime=json.loads(Path(manifest).read_text())
        if runtime.get('elf_sha256') != PIN: raise ValueError('runtime manifest pin mismatch')
        # The router requires the complete checked-in contract, not a subset that
        # happens to relocate. Compare metadata too; bytes alone do not pin routes.
        canonical=json.loads((ROOT/'live/runtime-sites.json').read_text())
        for key,value in canonical.items():
            if key=='sites':
                actual=runtime.get(key,[])
                if sorted(actual,key=lambda s:number(s['pc'])) != sorted(value,key=lambda s:number(s['pc'])):
                    raise ValueError('runtime manifest site contract mismatch')
            elif runtime.get(key)!=value:raise ValueError('runtime manifest ABI mismatch: '+key)
        specs += runtime['sites']
    rows=merge(at,specs)
    client_pins={pc:dict(guard=guard,kind=k) for _,module,pc,k,guard in STOCK_OBSERVERS if module=='client'}
    client_rows=merge(image(client,CLIENT_PIN),[dict(name="client_source_"+hex(pc),pc=pc,emitter="observer",**client_pins.get(pc,{})) for pc in SOURCE_CLIENT])
    coverage=stock_observer_coverage(rows,client_rows)
    output.mkdir(parents=True,exist_ok=True)
    result={'schema':1,'elf_sha256':PIN,'runtime_manifest_sha256':digest(Path(manifest).read_bytes()) if manifest else None,'sites':rows,'client_sha256':CLIENT_PIN,'client_sites':client_rows,'stock_observer_coverage':coverage}
    (output/'sites.private.json').write_text(json.dumps(result,sort_keys=True,indent=2)+'\n')
    lines=['#pragma once','#include <stdint.h>','#define EXE "/opt/steamvr/bin/linuxarm64/vrcompositor"',
           '#define EXE_SHA "'+PIN+'"','#define SITE_COUNT '+str(len(rows)),
           'struct site { uint64_t offset; uint32_t word; unsigned kind; unsigned char guard[16]; unsigned emitter,index; };',
           'static const struct site sites[SITE_COUNT] = {']
    for s in rows:
        lines.append('{%s,%s,%d,{%s},%d,%d},'%(hex(s['pc']),hex(s['word']),s['kind'],','.join('0x'+s['guard'][i:i+2] for i in range(0,32,2)),EMITTERS[s['emitter']],s['index']))
    lines+=['};','#define CLIENT_SHA "'+CLIENT_PIN+'"',
            '#define CLIENT_PATH "/opt/steamvr/bin/linuxarm64/vrclient.so"',
            '#define CLIENT_SITE_COUNT '+str(len(client_rows)),
            'static const struct site client_sites[CLIENT_SITE_COUNT] = {']
    for s in client_rows:
        lines.append('{%s,%s,%d,{%s},0,0},'%(hex(s['pc']),hex(s['word']),s['kind'],','.join('0x'+s['guard'][i:i+2] for i in range(0,32,2))))
    lines+=['};', '#define FT_STOCK_OBSERVER_SELECTED_MASK UINT64_C('+hex(number(coverage['selected_mask']))+')',
            '#define FT_STOCK_OBSERVER_COMP_SHA "'+PIN+'"',
            '#define FT_STOCK_OBSERVER_CLIENT_SHA "'+CLIENT_PIN+'"',
            'static const struct site stock_observer_pins[] = {']
    for _,module,pc,k,guard in STOCK_OBSERVERS:
        word=int.from_bytes(bytes.fromhex(guard[:8]),'little')
        tagged=pc | ((1<<63) if module=='client' else 0)
        lines.append('{UINT64_C(%s),%s,%d,{%s},0,0},'%(hex(tagged),hex(word),k,
                     ','.join('0x'+guard[i:i+2] for i in range(0,32,2))))
    lines+=['};']
    (output/'loader-pins.h').write_text('\n'.join(lines)+'\n')
    return result

if __name__=='__main__':
    p=argparse.ArgumentParser(description=__doc__);p.add_argument('--elf',type=Path,default=ELF)
    p.add_argument('--output',type=Path,required=True);p.add_argument('--manifest',type=Path)
    p.add_argument('--client',type=Path,default=CLIENT_ELF)
    a=p.parse_args();r=prepare(a.elf,a.output,a.manifest,a.client);print('PASS pinned sites:',len(r['sites']))
