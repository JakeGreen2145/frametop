#!/usr/bin/env python3
# SPDX-License-Identifier: MIT
"""Delta-only current-render inventory and actual selected-island tests."""
import io
import json
import os
from pathlib import Path
import subprocess
import sys
import tarfile
import unittest
import loader_test as L

BASE = '0f387ff207eb52d99b80848a57bab207e374043a'
CONSUMER = Path('/home/jakeg/.hermes/work/frametop-native-current-source-admission')
CONSUMER_COMMIT = '673f1149e929aaa218716d572499b1fc919614a1'
MANIFEST = L.ROOT/'live/runtime-sites.json'
REQUEST_PATH = CONSUMER/'native-portal/live/current-source-native-hook-request.json'
REQUEST = json.loads(subprocess.check_output(['git', 'show', CONSUMER_COMMIT+':native-portal/live/current-source-native-hook-request.json'], cwd=CONSUMER))
PINS = {int(s['pc'], 0): s for s in REQUEST['render_required_callbacks']}
BASE_MANIFEST = json.loads(subprocess.check_output(['git', 'show', BASE+':native-portal/live/runtime-sites.json'], cwd=L.ROOT))
OLD_ROWS = L.prepare.merge(L.prepare.image(L.prepare.ELF), L.prepare.core_specs()+BASE_MANIFEST['sites'])
NEW = sorted(set(PINS)-{s['pc'] for s in OLD_ROWS})


def compiled_required():
    # Read immutable Git objects, not a sibling worktree that may advance or
    # contain a reviewed repair. Preserve its files and compile the pinned tree.
    directory=L.B/'current-consumer';directory.mkdir(parents=True,exist_ok=True)
    snapshot=directory/'source'
    archive_bytes=subprocess.check_output(['git','archive',CONSUMER_COMMIT,'native-portal'],cwd=CONSUMER)
    with tarfile.open(fileobj=io.BytesIO(archive_bytes)) as archive:
        for entry in archive.getmembers():
            relative=Path(entry.name)
            assert not relative.is_absolute() and '..' not in relative.parts and relative.parts[0]=='native-portal'
            if entry.isdir():continue
            assert entry.isfile()
            stream=archive.extractfile(entry);assert stream is not None
            target=snapshot/relative;target.parent.mkdir(parents=True,exist_ok=True)
            target.write_bytes(stream.read())
    for name in ('current-source-native-hook-request.json','current_source_selection.hpp'):
        path=snapshot/'native-portal/live'/name
        assert path.read_bytes()==subprocess.check_output(['git','show',CONSUMER_COMMIT+':native-portal/live/'+name],cwd=CONSUMER)
    source=directory/'required.cpp'
    source.write_text('#include "current_source_selection.hpp"\n#include <cstdio>\nint main(){for(auto pc:native_portal::live::CurrentRenderCollector::required_sites)std::printf("%llx\\n",(unsigned long long)pc);}\n')
    binary=directory/'required'
    L.run(['c++','-std=c++17','-O0','--param','ggc-min-expand=5','--param','ggc-min-heapsize=4096','-I',snapshot/'native-portal/live',source,'-o',binary])
    required=[int(s,16) for s in subprocess.check_output([binary],text=True).split()]
    assert len(required)==len(set(required))==REQUEST['render_required_callback_count']==12
    assert set(required)==set(PINS)
    (L.B/'current-required.h').write_text('static const uint64_t current_required[] = {'+','.join(hex(pc) for pc in required)+'};\n')
    return required


class InventoryTests(unittest.TestCase):
    def test_compiled_consumer_matches_actual_selection(self):
        required=compiled_required()
        result=L.prepare.prepare(L.prepare.ELF,L.B,MANIFEST)
        rows={s['pc']:s for s in result['sites']}
        self.assertEqual(set(required)-rows.keys(),set(),'current consumer has missing installed callbacks')
        for pc,pin in PINS.items():
            row=rows[pc]
            self.assertEqual(row['word'],int(pin['word'],0))
            self.assertEqual(row['guard'],pin['guard16'])
            self.assertEqual(row['kind'],L.prepare.kind(row['word']))
            if pc in NEW:self.assertEqual((row['emitter'],row['index']),('observer',0))

    def test_old_records_and_complete_v1_consumers_unchanged(self):
        current=json.loads(MANIFEST.read_text())
        self.assertEqual({k:v for k,v in current.items() if k!='sites'}, {k:v for k,v in BASE_MANIFEST.items() if k!='sites'})
        records={s['pc']:s for s in current['sites']}
        for s in BASE_MANIFEST['sites']:self.assertEqual(records[s['pc']],s)
        r=L.prepare.prepare(L.prepare.ELF,L.B,MANIFEST)
        rows={s['pc']:s for s in r['sites']}
        for s in OLD_ROWS:self.assertEqual(rows[s['pc']],s)
        self.assertEqual(r['stock_observer_coverage']['selected_mask'],0xfffffffffff)
        self.assertEqual(len(L.prepare.STOCK_OBSERVERS),44)
        from stock_observer_test import StockObserverTests
        StockObserverTests('test_complete_consumer_inventories').test_complete_consumer_inventories()
        from admission_hooks_test import PINS as ADMISSION_PINS
        added_comp={pc for module,pc in ADMISSION_PINS if module=='compositor'}
        added_client={pc for module,pc in ADMISSION_PINS if module=='client'}
        self.assertEqual(len(r['sites']),len({s['pc'] for s in OLD_ROWS}|set(NEW)|added_comp))
        self.assertEqual(len(r['client_sites']),32+len(added_client))


def installed_matrix(sanitize=False):
    header=(L.B/'loader-pins.h').read_text()
    flags=['-std=c11','-O1','-g','-Wall','-Wextra','-Werror','-Wno-misleading-indentation','-Wno-unused-function','-Wno-unused-variable','-fPIC']
    root=L.B/('current-sanitize' if sanitize else 'current-installed');root.mkdir(exist_ok=True)
    if sanitize:flags+=['-fsanitize=address,undefined','-fno-omit-frame-pointer','-no-pie']
    objects=[]
    for i in range(5):
        obj=root/f'emit-{i}.o';objects.append(obj)
        source=L.ROOT/'live'/('runtime-emit.c' if i==4 else 'loader_emit.c')
        L.run(['cc',*flags,*([] if i==4 else ['-DFT_EMITTER='+str(i)]),'-c',source,'-o',obj])
    for omitted in [None]+NEW:
        directory=root/('full' if omitted is None else hex(omitted));directory.mkdir(exist_ok=True)
        lines=header.splitlines()
        if omitted is not None:
            matches=[s for s in lines if s.startswith('{'+hex(omitted)+',')];assert len(matches)==1
            lines.remove(matches[0]);idx=next(i for i,s in enumerate(lines) if s.startswith('#define SITE_COUNT '))
            lines[idx]='#define SITE_COUNT '+str(int(lines[idx].split()[-1])-1)
        # V1 full-mask macro deliberately unchanged, since this is a separate set.
        (directory/'loader-pins.h').write_text('\n'.join(lines)+'\n')
        (directory/'loader-build.h').write_bytes((L.B/'loader-build.h').read_bytes())
        (directory/'current-required.h').write_bytes((L.B/'current-required.h').read_bytes())
        source=directory/'main.c'
        source.write_text('#define main original_main\n#include "loader_test.c"\n#undef main\nint main(int argc,char**argv){assert(argc==3);original_main(2,argv);return ft_loader_test_dual(argv[2])==1?0:1;}\n')
        binary=directory/'test'
        L.run(['cc',*flags,'-DFT_TEST_CURRENT_COVERAGE='+str(int(omitted is None)),'-DFT_TEST_CURRENT_OMITTED='+hex(omitted or 0),'-I',directory,'-I',L.ROOT/'live',source,*objects,'-lcrypto','-ldl','-o',binary])
        L.run([binary,L.prepare.ELF,L.prepare.CLIENT_ELF],env={**os.environ,'ASAN_OPTIONS':'detect_leaks=1:abort_on_error=1','UBSAN_OPTIONS':'halt_on_error=1'})
    print('PASS current installed inventories: full +',len(NEW),'independent omissions; sanitizer=',sanitize)


def baseline_red():
    """Replay exact base preparer and manifest in an isolated ignored tree."""
    import importlib.util
    root=L.B/'current-red/native-portal'
    for name in ('live/prepare.py','live/runtime-sites.json','hooks/pins.h'):
        path=root/name;path.parent.mkdir(parents=True,exist_ok=True)
        path.write_bytes(subprocess.check_output(['git','show',BASE+':native-portal/'+name],cwd=L.ROOT))
    spec=importlib.util.spec_from_file_location('current_baseline_prepare',root/'live/prepare.py')
    assert spec is not None and spec.loader is not None
    module=importlib.util.module_from_spec(spec);spec.loader.exec_module(module)
    result=module.prepare(module.ELF,root/'build/result',root/'live/runtime-sites.json')
    missing=set(compiled_required())-{s['pc'] for s in result['sites']}
    print('Actual base prepare missing current callbacks:',','.join(hex(pc) for pc in sorted(missing)),flush=True)
    assert not missing, 'RED: base canonical loader lacks current-render observers'


if __name__=='__main__':
    if '--baseline-red' in sys.argv:baseline_red();raise SystemExit(0)
    result=unittest.TextTestRunner(verbosity=2).run(unittest.defaultTestLoader.loadTestsFromTestCase(InventoryTests))
    if not result.wasSuccessful():raise SystemExit(1)
    if '--full' in sys.argv or '--sanitize' in sys.argv:
        L.compile_tests(MANIFEST)
        installed_matrix('--sanitize' in sys.argv)
        L.run([sys.executable,'-B',L.ROOT/'live/current_render_hooks_machine.py'])
