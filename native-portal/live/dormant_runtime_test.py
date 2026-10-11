#!/usr/bin/env python3
# SPDX-License-Identifier: MIT
"""Actual default Runtime RED/acceptance fixture, modeled native code only."""
import argparse
import ast
import hashlib
import json
import os
from pathlib import Path
import resource
import re
import socket
import subprocess
import sys
import threading
import uuid

ROOT = Path(__file__).resolve().parents[1]
BUILD = ROOT / 'build/dormant-runtime'
SCRATCH = Path('/home/jakeg/.hermes/cache/scratch')


def main():
    global BUILD
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument('--in-scope', action='store_true', help=argparse.SUPPRESS)
    p.add_argument('--baseline-red', action='store_true')
    p.add_argument('--sanitize', action='store_true')
    p.add_argument('--current', action='store_true', help='Default Runtime first stereo and settlement')
    p.add_argument('--fixtures', type=Path, default=Path('/home/jakeg/.hermes/work/frametop-native-live-integration/native-portal/build/camera-runtime-asan'))
    args = p.parse_args()
    if not args.in_scope:
        unit = 'dormant-runtime-' + uuid.uuid4().hex + '.scope'
        try:
            return subprocess.run(['systemd-run', '--user', '--scope', '--quiet', '--unit=' + unit,
                                   '-p', 'MemoryMax=256M', '-p', 'MemorySwapMax=0', sys.executable,
                                   str(Path(__file__).resolve()), '--in-scope', *sys.argv[1:]], timeout=600).returncode
        except subprocess.TimeoutExpired:
            subprocess.run(['systemctl', '--user', 'stop', unit], timeout=15, check=False)
            raise
    resource.setrlimit(resource.RLIMIT_CORE, (0, 0))
    os.environ['TMPDIR'] = str(SCRATCH)
    if args.current:
        BUILD = BUILD / 'current'
    if args.sanitize:
        BUILD = BUILD / 'sanitize'
        os.environ['ASAN_OPTIONS'] = 'detect_leaks=1:halt_on_error=1'
        os.environ['UBSAN_OPTIONS'] = 'halt_on_error=1:print_stacktrace=1'
    BUILD.mkdir(parents=True, exist_ok=True)
    # Modeled installed inventory from literal canonical inputs, NOT either
    # collector's required-sites list. Do not import prepare or open an ELF.
    prep = ast.parse((ROOT / 'live/prepare.py').read_text())
    names = {'SOURCE_COMP', 'SOURCE_CLIENT', 'STOCK_OBSERVERS', 'ARENA', 'PASS', 'LIFECYCLE', 'EVIDENCE'}
    values = {}
    for node in prep.body:
        if isinstance(node, ast.Assign) and len(node.targets) == 1 and isinstance(node.targets[0], ast.Name) and node.targets[0].id in names:
            values[node.targets[0].id] = ast.literal_eval(node.value)
        elif isinstance(node, ast.AugAssign) and isinstance(node.target, ast.Name) and node.target.id == 'SOURCE_COMP':
            values['SOURCE_COMP'] += ast.literal_eval(node.value)
    comp = set()
    for name in ('SOURCE_COMP', 'ARENA', 'PASS', 'LIFECYCLE', 'EVIDENCE'):
        comp.update(values[name])
    comp.update(int(pc, 0) for pc in re.findall(r'\{"[^\"]+",(0x[0-9a-f]+),', (ROOT / 'hooks/pins.h').read_text()))
    manifest = json.loads((ROOT / 'live/runtime-sites.json').read_text())
    comp.update(row['relative_pc'] for row in manifest['sites'])
    client = set(values['SOURCE_CLIENT'])
    client.update(pc for _, module, pc, _, _ in values['STOCK_OBSERVERS'] if module == 'client')
    assert (len(comp), len(client)) == (200, 32), 'canonical stock/current inventory changed; reconcile rather than guess'
    inventory = ('// Modeled installation only, canonical PCs without ELF/loader execution.\n'
                 + 'static constexpr uint64_t modeled_comp_sites[]={' + ','.join(map(hex, sorted(comp))) + '};\n'
                 + 'static constexpr uint64_t modeled_client_sites[]={' + ','.join(hex(pc | (1 << 63)) for pc in sorted(client)) + '};\n')
    (BUILD / 'dormant-inventory.private.hpp').write_text(inventory)
    tree = ast.parse((ROOT / 'live/runtime_test.py').read_text())
    sources = next(ast.literal_eval(n.value) for n in tree.body if isinstance(n, ast.Assign)
                   and any(isinstance(t, ast.Name) and t.id == 'SOURCES' for t in n.targets))
    flags = ['-std=c++17', '-O0', '-g1', '-Wall', '-Wextra', '-Werror', '-pthread', '-no-pie',
             '--param=ggc-min-expand=10', '--param=ggc-min-heapsize=4096',
             '-I' + str(ROOT / 'include'), '-I' + str(BUILD), '-I/home/jakeg/.hermes/work/frame-native-pipeline-review/include',
             '-DFT_NATIVE_PORTAL_TEST_CALL']
    if args.sanitize:
        flags += ['-fsanitize=address,undefined', '-fno-omit-frame-pointer']
    private_hashes = {}
    if args.current:
        # Opaque frozen headers only. Do not import their native evidence runners.
        private_hashes = {
            'pump.private.hpp': '2619b907e95b98af18a1367aaf38b0a9463510ce63483cd5541c2d579e03d1c0',
            'camera.private.hpp': '4a48132b19672f429e6584552fa5712bd05aa8d54cc86f9bf3b3815d02b3763d',
            'selection.private.hpp': '4d70ee508d6741fa75c04c021f1272f4f1e0eba8228954fcf76b1cba305f2e60',
        }
        for name, expected in private_hashes.items():
            content = (args.fixtures / name).read_bytes()
            assert hashlib.sha256(content).hexdigest() == expected, 'frozen fixture changed: ' + name
            (BUILD / name).write_bytes(content)
        # Source-model coordinates already present in the owning fixture and
        # published native contracts, not a fresh instruction-execution claim.
        coordinates = dict(NATIVE_RUN_RETURN=0x182aa8, BOOT_DISPATCH_LR=0x6caf0,
                           BOOT_RELEASE_LR=0x2a23b4, BOOT_POOL_LR=0x3052f0,
                           BOOT_QUEUE_LR=0x2a21f0, RECYCLE_LR=0x2a9cdc,
                           STOCK_POOL_LR=0x297cb4, INITIAL_POOL_LR=0x29a8b8,
                           OUTER_LR=0x14a5cc)
        flags += ['-DFT_' + key + '=' + hex(value) for key, value in coordinates.items()]
    inputs = [p for p in ROOT.rglob('*') if p.is_file() and 'build' not in p.relative_to(ROOT).parts
              and p.suffix in ('.cpp', '.hpp', '.inc', '.h', '.py')]
    hashes = {str(p.relative_to(ROOT)): hashlib.sha256(p.read_bytes()).hexdigest() for p in inputs}
    fingerprint = hashlib.sha256(json.dumps([hashes, flags, inventory, private_hashes], sort_keys=True).encode()).hexdigest()
    stamp = BUILD / 'fingerprint'
    rebuild = not stamp.exists() or stamp.read_text() != fingerprint
    commands = []
    with (BUILD / 'compile.log').open('a') as log:
        def run(command):
            commands.append(list(map(str, command)))
            log.write(json.dumps(commands[-1]) + '\n');log.flush()
            subprocess.run(commands[-1], stdout=log, stderr=subprocess.STDOUT, check=True, timeout=180)
        objects = []
        for source in [*sources, 'live/current_runtime_test.cpp' if args.current else 'live/dormant_runtime_test.cpp']:
            obj = BUILD / (source.replace('/', '_') + '.o');objects.append(obj)
            deps = {name: digest for name, digest in hashes.items() if name == source or
                    Path(name).suffix in ('.hpp', '.h', '.inc') or
                    ('test' in source and name.endswith('_test.cpp'))}
            key = hashlib.sha256(json.dumps([deps, flags, inventory, private_hashes], sort_keys=True).encode()).hexdigest()
            objstamp = obj.with_suffix('.hash')
            if not obj.exists() or not objstamp.exists() or objstamp.read_text() != key:
                run(['g++', *flags, '-c', ROOT / source, '-o', obj])
                objstamp.write_text(key)
        exe = BUILD / 'dormant-runtime-test'
        if rebuild or not exe.exists():
            run(['g++', *flags, *objects, '-o', exe])
        stamp.write_text(fingerprint)
    rows = []
    modes = ['baseline', 'responsive', 'blocked', 'nested']
    if not args.baseline_red:
        modes += ['cold-seal', 'connect', 'engage-race']
    if args.current:
        modes = ['cold', 'warm', 'stock-render', 'update-three', 'missing-hook', 'partial-upload', 'rejected-enqueue',
                 'source-uncertainty', 'identity-migration', 'actual-return', 'no-gpu', 'no-cpu', 'no-source']
    for mode in modes:
        reader, writer = socket.socketpair();chunks = []
        def drain():
            while True:
                data = reader.recv(65536)
                if not data:
                    return
                chunks.append(data)
        thread = threading.Thread(target=drain);thread.start()
        child = subprocess.Popen([str(exe), mode], stdout=writer.fileno(), stderr=writer.fileno(),
                                 env={**os.environ, 'FT_DORMANT_FIXTURE_LEGACY': '1' if args.baseline_red else '0'})
        writer.close()
        try:
            code = child.wait(timeout=15)
        except subprocess.TimeoutExpired:
            child.kill();child.wait();raise
        finally:
            thread.join(timeout=5);reader.close()
        text = b''.join(chunks).decode(errors='replace')
        (BUILD / (mode + '.log')).write_text(text)
        complete = code == 0 and ('clean-teardown=1' in text if args.current else 'DORMANT clean-teardown=1' in text)
        returned_caller = complete and 'DORMANT returned caller=1 owner=0' in text
        fatal = next((line for line in text.splitlines() if line.startswith('ft-camera-fatal ')), '')
        fields = dict(word.split('=', 1) for word in fatal.split()[1:])
        exact_red = (code == -6 and fields.get('reason') == '0x000000000000000b'
                     and fields.get('event') == '0x000000000013a100' and fields.get('bound') == '1'
                     and fields.get('current_tid') != fields.get('owner_tid')
                     and 'DORMANT at-SIGABRT originals=0' in text and 'DORMANT bound=1 zero_work=1' in text)
        passed = returned_caller if mode != 'responsive' else complete
        engaged = 'DORMANT metadata-engaged=1' in text
        if mode in ('cold-seal', 'connect', 'engage-race'):
            passed = passed and engaged
        if args.baseline_red and mode in ('blocked', 'nested'):
            passed = exact_red
        if args.current:
            passed = code == 0 and text.count('stereo=1 pending-Off=1 exact-settlement=1 disconnects=0 clean-teardown=1') == 2
            if mode == 'missing-hook':
                passed = code == 0 and text.count('CURRENT missing-hook refused=1 clean-teardown=1') == 2
            if mode in ('partial-upload', 'rejected-enqueue', 'source-uncertainty', 'identity-migration',
                        'actual-return', 'no-gpu', 'no-cpu', 'no-source'):
                passed = (code == -6 and 'CURRENT pending cleanup negative=' + mode + ' native_cleanup=0' in text
                          and fields.get('event') == '0x000000000008b020'
                          and fields.get('reason') == '0x0000000000000005' and fields.get('joined') == '1'
                          and 'CURRENT forbidden native cleanup' not in text and 'Assertion' not in text)
        passed = passed and not any(marker in text for marker in ('ERROR: AddressSanitizer', 'runtime error:', 'ERROR: LeakSanitizer'))
        rows.append(dict(mode=mode, returncode=code, complete=complete, caller=returned_caller,
                         exact_red=exact_red, passed=passed, fatal=fields))
        print(json.dumps(rows[-1]), flush=True)
    assert hashes == {str(p.relative_to(ROOT)): hashlib.sha256(p.read_bytes()).hexdigest() for p in inputs}
    result = dict(source_model=True, native_execution=False, memory_max='256M', swap_max=0,
                  baseline_red=args.baseline_red, source_hashes=hashes, frozen_fixture_hashes=private_hashes, commands=commands, cases=rows)
    (BUILD / ('baseline-red.json' if args.baseline_red else 'acceptance.json')).write_text(json.dumps(result, indent=2) + '\n')
    return 0 if all(row['passed'] for row in rows) else 1


if __name__ == '__main__':
    raise SystemExit(main())
