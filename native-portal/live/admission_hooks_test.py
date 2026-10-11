#!/usr/bin/env python3
# SPDX-License-Identifier: MIT
"""Canonical admission-alternative observers. No source-policy authority."""
import importlib.util
import json
import os
from pathlib import Path
import subprocess
import sys
import unittest
import loader_test as L

BASE = '6674da53a3d319f9de360655a0f6ed17182a66de'
CONTRACT_PATH = Path('/home/jakeg/.hermes/work/frame-native-product-ops/admission-closure-hook-contract.json')
PROOF = Path('/home/jakeg/.hermes/work/desktop-compositor-re/proactive-closure-f9adb38/lane-b-ff2c967309a9')
CONTRACT = json.loads(CONTRACT_PATH.read_text())
assert L.prepare.digest((PROOF/'REPORT.md').read_bytes()) == CONTRACT['native_contract_report_sha256']
assert L.prepare.digest((PROOF/'HOOK-INVENTORY.json').read_bytes()) == CONTRACT['input_inventory_sha256']
PINS = {(s['module'],int(s['pc'],0)):s for s in CONTRACT['new_required_sites']}
assert len(PINS) == len(CONTRACT['new_required_sites']) == 10
MANIFEST = L.ROOT/'live/runtime-sites.json'


def keyed(result):
    return {(module,s['pc']):s for module,key in [('compositor','sites'),('client','client_sites')] for s in result[key]}


def baseline():
    root=L.B/'admission-baseline/native-portal'
    for name in ('live/prepare.py','live/runtime-sites.json','hooks/pins.h'):
        target=root/name;target.parent.mkdir(parents=True,exist_ok=True)
        target.write_bytes(subprocess.check_output(['git','show',BASE+':native-portal/'+name],cwd=L.ROOT))
    spec=importlib.util.spec_from_file_location('admission_baseline_prepare',root/'live/prepare.py')
    assert spec is not None and spec.loader is not None
    module=importlib.util.module_from_spec(spec);spec.loader.exec_module(module)
    return module.prepare(module.ELF,root/'build/result',root/'live/runtime-sites.json')


class InventoryTests(unittest.TestCase):
    def test_exact_additions_and_old_rows_preserved(self):
        old=keyed(baseline());new=keyed(L.prepare.prepare(L.prepare.ELF,L.B,MANIFEST))
        self.assertEqual(set(new)-set(old),set(PINS),'canonical selected observers missing')
        for key,row in old.items():self.assertEqual(new[key],row)
        self.assertTrue(all((s['module'],int(s['pc'],0)) not in new for s in CONTRACT['excluded_diagnostic_only']))
        old_manifest=json.loads(subprocess.check_output(['git','show',BASE+':native-portal/live/runtime-sites.json'],cwd=L.ROOT))
        current=json.loads(MANIFEST.read_text())
        self.assertEqual({k:v for k,v in old_manifest.items() if k!='sites'}, {k:v for k,v in current.items() if k!='sites'})
        records={s['pc']:s for s in current['sites']}
        for s in old_manifest['sites']:self.assertEqual(records[s['pc']],s)
        for key,pin in PINS.items():
            row=new[key]
            self.assertEqual((row['word'],row['kind'],row['guard'],row['emitter'],row['index']),
                             (int(pin['word'],0),pin['kind'],pin['guard16'],'observer',0))
        result=L.prepare.prepare(L.prepare.ELF,L.B,MANIFEST)
        self.assertEqual(result['stock_observer_coverage'],baseline()['stock_observer_coverage'])

    def test_guard_word_kind_hash_refusals(self):
        for (module,pc),pin in PINS.items():
            at=L.prepare.image(L.prepare.CLIENT_ELF,L.prepare.CLIENT_PIN) if module=='client' else L.prepare.image(L.prepare.ELF)
            self.assertEqual(at(pc,16).hex(),pin['guard16'])
            self.assertEqual(L.prepare.digest(at(pc,16)),pin['guard_sha256'])
            row=dict(pc=pc,word=pin['word'],kind=pin['kind'],guard=pin['guard16'],guard_sha256=pin['guard_sha256'],emitter='observer')
            self.assertEqual(L.prepare.merge(at,[row])[0]['pc'],pc)
            for field,value in [('word',int(pin['word'],0)^1),('kind',pin['kind']^1),('guard','00'*16),('guard_sha256','0'*64)]:
                with self.subTest(module=module,pc=hex(pc),field=field),self.assertRaises(ValueError):
                    L.prepare.merge(at,[{**row,field:value}])


def installed_matrix(sanitize=False):
    header=(L.B/'loader-pins.h').read_text()
    root=L.B/('admission-sanitize' if sanitize else 'admission-installed');root.mkdir(exist_ok=True)
    flags=['-std=c11','-O1','-g','-Wall','-Wextra','-Werror','-Wno-misleading-indentation','-Wno-unused-function','-Wno-unused-variable','-fPIC']
    if sanitize:flags+=['-fsanitize=address,undefined','-fno-omit-frame-pointer','-no-pie']
    objects=[]
    for i in range(5):
        obj=root/f'emit-{i}.o';objects.append(obj)
        source=L.ROOT/'live'/('runtime-emit.c' if i==4 else 'loader_emit.c')
        L.run(['cc',*flags,*([] if i==4 else ['-DFT_EMITTER='+str(i)]),'-c',source,'-o',obj])
    results=[]
    for omitted in [None]+list(PINS):
        directory=root/('full' if omitted is None else omitted[0]+'-'+hex(omitted[1]));directory.mkdir(exist_ok=True)
        lines=header.splitlines()
        if omitted is not None:
            module,pc=omitted
            matches=[s for s in lines if s.startswith('{'+hex(pc)+',')];assert len(matches)==1
            lines.remove(matches[0]);count='CLIENT_SITE_COUNT' if module=='client' else 'SITE_COUNT'
            idx=next(i for i,s in enumerate(lines) if s.startswith('#define '+count+' '))
            lines[idx]='#define '+count+' '+str(int(lines[idx].split()[-1])-1)
        # Leave the old all-mask macro full: only actual module-tagged PCs count.
        assert [s for s in lines if s.startswith('#define FT_STOCK_OBSERVER_SELECTED_MASK')] == [s for s in header.splitlines() if s.startswith('#define FT_STOCK_OBSERVER_SELECTED_MASK')]
        (directory/'loader-pins.h').write_text('\n'.join(lines)+'\n')
        (directory/'loader-build.h').write_bytes((L.B/'loader-build.h').read_bytes())
        required=[pc|((1<<63) if module=='client' else 0) for module,pc in PINS]
        (directory/'admission-required.h').write_text('static const uint64_t admission_required[]={'+','.join('UINT64_C('+hex(pc)+')' for pc in required)+'};\n')
        source=directory/'main.c'
        source.write_text('#define main original_main\n#include "loader_test.c"\n#undef main\nint main(int argc,char**argv){assert(argc==3);original_main(2,argv);return ft_loader_test_dual(argv[2])==1?0:1;}\n')
        omitted_tagged=0 if omitted is None else omitted[1]|((1<<63) if omitted[0]=='client' else 0)
        binary=directory/'test'
        L.run(['cc',*flags,'-DFT_TEST_ADMISSION_COVERAGE='+str(int(omitted is None)),'-DFT_TEST_ADMISSION_OMITTED='+hex(omitted_tagged),'-I',directory,'-I',L.ROOT/'live',source,*objects,'-lcrypto','-ldl','-o',binary])
        L.run([binary,L.prepare.ELF,L.prepare.CLIENT_ELF],env={**os.environ,'ASAN_OPTIONS':'detect_leaks=1:abort_on_error=1','UBSAN_OPTIONS':'halt_on_error=1'})
        results.append(dict(omitted=None if omitted is None else [omitted[0],hex(omitted[1])],complete=omitted is None))
    (root/'results.json').write_text(json.dumps(results,indent=2)+'\n')
    print('PASS admission installed full + independent omissions:',len(results),'sanitizer=',sanitize)


if __name__=='__main__':
    if '--baseline-red' in sys.argv:
        missing=set(PINS)-keyed(baseline()).keys()
        print('Actual base prepare missing:',sorted((module,hex(pc)) for module,pc in missing),flush=True)
        assert not missing, 'RED: canonical dual-module admission observers absent'
        raise SystemExit(0)
    result=unittest.TextTestRunner(verbosity=2).run(unittest.defaultTestLoader.loadTestsFromTestCase(InventoryTests))
    if not result.wasSuccessful():raise SystemExit(1)
    if '--full' in sys.argv or '--sanitize' in sys.argv:
        L.compile_tests(MANIFEST)
        installed_matrix('--sanitize' in sys.argv)
        L.run([sys.executable,'-B',L.ROOT/'live/admission_hooks_machine.py'])
