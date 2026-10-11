#!/usr/bin/env python3
# SPDX-License-Identifier: MIT
"""Pinned stock observers through canonical prepare and the selected loader.

Private ELF files are read-only. No native runtime or hardware is activated.
"""
import ctypes
import json
from pathlib import Path
import unittest
import loader_test as L

HANDOFF = Path('/home/jakeg/.hermes/work/desktop-compositor-re/continuation16-verified-collector-9f10bcd93d99')
MANIFEST = L.ROOT/'live/runtime-sites.json'
PRIMARY = json.loads((HANDOFF/'HOOK-PINS-AND-SAVED.json').read_text())['sites']
GUARDS = json.loads((HANDOFF/'verified-payloads/verification/GUARD-SCOPE-PINS.json').read_text())['sites']
EXPECTED = [(s['module'], L.prepare.number(s['pc']), s) for s in PRIMARY]
EXPECTED += [('client', s['pc'], s) for s in GUARDS]
ADDITIONAL_ROOT=Path('/home/jakeg/.hermes/work/desktop-compositor-re/continuation16-additional-native-pins-ed19c700d06d')
additional_bytes=(ADDITIONAL_ROOT/'ADDITIONAL-NATIVE-PINS.json').read_bytes()
assert L.prepare.digest(additional_bytes)=='af02e3f4a09bfa8dcfb8278729d3bb3555bf59eb4154fd4877dcbe271675392d'
ADDITIONAL=json.loads(additional_bytes)
EXPECTED += [('compositor',s['pc'],s) for s in ADDITIONAL['pins']]
# The delivered full, independently pinned window supplies ALL sixteen bytes.
# This is not a guard inferred from a single word. Positive predecessor coverage
# is needed because failure304fa0 reaches304ecc without storing x22.
window=next(w for w in ADDITIONAL['native_source_windows'] if w['elf_start']=='0x304e60')
window_bytes=bytes.fromhex(window['source_bytes_hex'])
assert L.prepare.digest(window_bytes)==window['source_bytes_sha256']
guard=window_bytes[0x304ec8-0x304e60:0x304ec8-0x304e60+16]
EXPECTED.append(('compositor',0x304ec8,dict(pc=0x304ec8,word=int.from_bytes(guard[:4],'little'),kind=0,guard=guard.hex())))
for pc in (0x3053f8,0x302790,0x304e60):
    witness=next(w for w in ADDITIONAL['native_source_windows'] if int(w['elf_start'],0)<=pc and pc+16<=int(w['elf_start'],0)+w['size_bytes'])
    data=bytes.fromhex(witness['source_bytes_hex']);assert L.prepare.digest(data)==witness['source_bytes_sha256']
    offset=pc-int(witness['elf_start'],0);guard=data[offset:offset+16]
    EXPECTED.append(('compositor',pc,dict(pc=pc,word=int.from_bytes(guard[:4],'little'),kind=0,guard=guard.hex())))
SOURCE_REQUEST=Path('/home/jakeg/.hermes/work/frametop-native-source-native-facts/native-portal/live/stock-native-source-hook-request.json')
SOURCE_REQUEST_SHA='62ad5f9d3d989e3e266a3410938a46410a22b7d663b34341d71e4b63517bf7e6'
assert L.prepare.digest(SOURCE_REQUEST.read_bytes())==SOURCE_REQUEST_SHA
SOURCE_CONTRACT=json.loads(SOURCE_REQUEST.read_text())
for s in SOURCE_CONTRACT['new_callbacks']:
    module=s['module'];pc=int(s['pc'],0)
    if any(m==module and p==pc for m,p,_ in EXPECTED):continue
    # Candidate kinds are not an emitter contract. In particular 16013c is
    # ADRP, kind6, despite kind_candidate=0 in the source permissions table.
    word=int(s['word'],0)
    EXPECTED.append((module,pc,dict(pc=pc,word=word,kind=L.prepare.kind(word),guard=s['guard16'])))
# The requested305490 window contained only eight bytes. The complete RX16 was
# checked directly in the existing whole-hash-authenticated ELF, without analysis.
EXPECTED.append(('compositor',0x305490,dict(pc=0x305490,word=0x17fffff0,kind=4,
    guard='f0ffff179b19f597f40300aa05000014')))
FULL_MASK=(1<<len(EXPECTED))-1

class StockObserverTests(unittest.TestCase):
    def test_canonical_selected_observers(self):
        result = L.prepare.prepare(L.prepare.ELF, L.B, MANIFEST)
        for module, pc, pin in EXPECTED:
            rows = {s['pc']: s for s in result['client_sites' if module=='client' else 'sites']}
            with self.subTest(module=module, pc=hex(pc)):
                self.assertTrue(pc in rows, 'verified observer missing from canonical selected install')
                s = rows[pc]
                self.assertEqual((s['word'], s['kind'], s['guard'], s['emitter'], s['index']),
                                 (L.prepare.number(pin['word']), pin['kind'], pin['guard'], 'observer', 0))

    def test_versioned_coverage_generated_from_selection(self):
        result = L.prepare.prepare(L.prepare.ELF, L.B, MANIFEST)
        self.assertIn('stock_observer_coverage', result, 'no truthful installed-observer coverage contract')
        coverage = result['stock_observer_coverage']
        self.assertEqual(coverage['version'], 1)
        self.assertEqual(coverage['selected_mask'], FULL_MASK)
        self.assertEqual(coverage['required_mask'], FULL_MASK)
        self.assertEqual(coverage['scope'], 'observer-sites-only')
        self.assertEqual(sorted((m,pc) for _,m,pc,_,_ in L.prepare.STOCK_OBSERVERS),
                         sorted((m,pc) for m,pc,_ in EXPECTED))

    def test_missing_or_wrong_selected_site_has_no_bit(self):
        import copy
        r = L.prepare.prepare(L.prepare.ELF, L.B, MANIFEST)
        for bit, (_, module, pc, _, _) in enumerate(L.prepare.STOCK_OBSERVERS):
            key = 'client_sites' if module=='client' else 'sites'
            for fault in ('missing', 'duplicate', 'word', 'kind', 'guard', 'emitter', 'index'):
                changed = copy.deepcopy(r)
                s = next(s for s in changed[key] if s['pc']==pc)
                if fault=='missing':changed[key].remove(s)
                elif fault=='duplicate':changed[key].append(s.copy())
                elif fault=='guard':s['guard']='00'*16
                elif fault=='emitter':s['emitter']='return-function'
                else:s[fault] ^= 1
                with self.subTest(pc=hex(pc), fault=fault):
                    c=L.prepare.stock_observer_coverage(changed['sites'],changed['client_sites'])
                    self.assertEqual(c['selected_mask'], FULL_MASK ^ (1<<bit))
        for comp,client in [('wrong',L.prepare.CLIENT_PIN),(L.prepare.PIN,'wrong')]:
            self.assertEqual(L.prepare.stock_observer_coverage(r['sites'],r['client_sites'],comp,client)['selected_mask'],0)

    def test_actual_prepare_rejects_changed_private_pin(self):
        # Authenticate real RX bytes, then perturb one supplied guard at a time.
        for module,pc,pin in EXPECTED:
            at=L.prepare.image(L.prepare.CLIENT_ELF,L.prepare.CLIENT_PIN) if module=='client' else L.prepare.image(L.prepare.ELF)
            row=dict(pc=pc,kind=pin['kind'],word=pin['word'],guard=pin['guard'],emitter='observer')
            self.assertEqual(L.prepare.merge(at,[row])[0]['pc'],pc)
            guard=str(row['guard'])
            row['guard']=('01' if guard[:2]!='01' else '00')+guard[2:]
            with self.subTest(module=module,pc=hex(pc)),self.assertRaises(ValueError):
                L.prepare.merge(at,[row])

    def test_complete_consumer_inventories(self):
        r=L.prepare.prepare(L.prepare.ELF,L.B,MANIFEST)
        available={('compositor',s['pc']) for s in r['sites']}|{('client',s['pc']) for s in r['client_sites']}
        source={(s['module'],int(s['pc'],0)) for s in SOURCE_CONTRACT['required_callbacks']}
        self.assertEqual(len(source),SOURCE_CONTRACT['required_callback_count'])
        self.assertEqual(source-available,set())
        command=json.loads(Path('/home/jakeg/.hermes/work/frametop-native-native-stock-facts/native-portal/runtime/HOOK-REQUEST.json').read_text())
        required={('compositor',int(pc,0)) for pc in command['required_consumer_sites']}
        self.assertEqual(len(required),command['required_consumer_count'])
        self.assertEqual(required-available,set())

    def test_no_manifest_is_not_full_stock_coverage(self):
        r=L.prepare.prepare(L.prepare.ELF,L.B)
        client_mask=sum(1<<bit for bit,(_,module,_,_,_) in enumerate(L.prepare.STOCK_OBSERVERS) if module=='client')
        self.assertEqual(r['stock_observer_coverage']['selected_mask'],client_mask)
        self.assertNotEqual(client_mask,FULL_MASK)


def compiled_missing_site_matrix():
    """Compile missing inventories, retaining a deliberately stale full macro.

    This proves the exported API inspects the selected table rather than trusting
    a generated assertion. These inventories are test-only, never launch inputs.
    """
    import sys
    header=(L.B/'loader-pins.h').read_text()
    build=(L.B/'loader-build.h').read_text()
    objects=[L.B/f'emit-{i}.o' for i in range(4)]+[L.B/'runtime-emit.o']
    flags=['-std=c11','-O1','-Wall','-Wextra','-Werror','-Wno-misleading-indentation',
           '-Wno-unused-function','-Wno-unused-variable','-fPIC']
    checks=[]
    for bit,(_,module,pc,_,_) in enumerate(L.prepare.STOCK_OBSERVERS):
        directory=L.B/f'stock-missing-{bit}';directory.mkdir(exist_ok=True)
        lines=header.splitlines()
        matches=[line for line in lines if line.startswith('{'+hex(pc)+',')]
        assert len(matches)==1
        lines.remove(matches[0])
        count='CLIENT_SITE_COUNT' if module=='client' else 'SITE_COUNT'
        old=next(line for line in lines if line.startswith('#define '+count+' '))
        lines[lines.index(old)]='#define '+count+' '+str(int(old.split()[-1])-1)
        (directory/'loader-pins.h').write_text('\n'.join(lines)+'\n')
        (directory/'loader-build.h').write_text(build)
        dll=directory/'missing.so'
        L.run(['cc',*flags,'-DFT_TEST_STOCK_MASK='+hex(FULL_MASK^(1<<bit)),'-I',directory,
               '-shared',L.ROOT/'live/loader_test.c',*objects,'-lcrypto','-ldl','-o',dll])
        # A separate process unloads each DSO and all private mappings promptly.
        L.run([sys.executable,'-c','import ctypes,sys; d=ctypes.CDLL(sys.argv[1]); d.ft_loader_test_dual.argtypes=[ctypes.c_char_p]; assert d.ft_loader_test_dual(sys.argv[2].encode())==1',dll,L.prepare.CLIENT_ELF])
        checks.append([module,hex(pc),hex(FULL_MASK^(1<<bit))])
    directory=L.B/'stock-old-header';directory.mkdir(exist_ok=True)
    (directory/'loader-pins.h').write_text('\n'.join(line for line in header.splitlines() if not line.startswith('#define FT_STOCK_OBSERVER_SELECTED_MASK'))+'\n')
    (directory/'loader-build.h').write_text(build)
    dll=directory/'old.so'
    L.run(['cc',*flags,'-DFT_TEST_STOCK_MASK=0','-I',directory,'-shared',L.ROOT/'live/loader_test.c',*objects,'-lcrypto','-ldl','-o',dll])
    L.run([sys.executable,'-c','import ctypes,sys; d=ctypes.CDLL(sys.argv[1]); d.ft_loader_test_dual.argtypes=[ctypes.c_char_p]; assert d.ft_loader_test_dual(sys.argv[2].encode())==1',dll,L.prepare.CLIENT_ELF])
    (L.B/'stock-missing-coverage.json').write_text(json.dumps(checks,indent=2)+'\n')
    print('PASS compiled missing-site inventories:',len(checks),'and old-header refusal')

def sanitized_loader():
    directory=L.B/'stock-sanitize';directory.mkdir(exist_ok=True)
    flags=['-std=c11','-O1','-g','-Wall','-Wextra','-Werror',
           '-Wno-misleading-indentation','-Wno-unused-function','-Wno-unused-variable',
           '-fsanitize=address,undefined','-fno-omit-frame-pointer']
    objects=[]
    for i in range(4):
        obj=directory/f'emit-{i}.o';objects.append(obj)
        L.run(['cc',*flags,'-DFT_EMITTER='+str(i),'-c',L.ROOT/'live/loader_emit.c','-o',obj])
    obj=directory/'runtime-emit.o';objects.append(obj)
    L.run(['cc',*flags,'-c',L.ROOT/'live/runtime-emit.c','-o',obj])
    binary=directory/'stock-sanitize'
    L.run(['cc',*flags,'-I',L.B,L.ROOT/'live/stock_observer_sanitize.c',*objects,'-lcrypto','-ldl','-o',binary])
    L.run([binary,L.prepare.ELF,L.prepare.CLIENT_ELF])

if __name__=='__main__':
    import sys
    full='--full' in sys.argv or '--sanitize' in sys.argv
    result=unittest.TextTestRunner(verbosity=2).run(unittest.defaultTestLoader.loadTestsFromTestCase(StockObserverTests))
    if not result.wasSuccessful():raise SystemExit(1)
    if full:
        L.compile_tests(MANIFEST)
        dll=ctypes.CDLL(str(L.B/'loader-test.so'));dll.ft_loader_test_dual.argtypes=[ctypes.c_char_p]
        assert dll.ft_loader_test_dual(str(L.prepare.CLIENT_ELF).encode())==1
        L.run([sys.executable,'-B',L.ROOT/'live/stock_observer_machine.py'])
        compiled_missing_site_matrix()
    if '--sanitize' in sys.argv:
        L.prepare.prepare(L.prepare.ELF,L.B,MANIFEST)
        sanitized_loader()
