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
    if args.sanitize:
        BUILD = BUILD / 'sanitize'
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
    assert (len(comp), len(client)) == (200, 32), 'canonical observer inventory changed; reconcile rather than guess'
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
    inputs = [p for p in ROOT.rglob('*') if p.is_file() and 'build' not in p.relative_to(ROOT).parts
              and p.suffix in ('.cpp', '.hpp', '.inc', '.h')]
    hashes = {str(p.relative_to(ROOT)): hashlib.sha256(p.read_bytes()).hexdigest() for p in inputs}
    fingerprint = hashlib.sha256(json.dumps([hashes, flags, inventory], sort_keys=True).encode()).hexdigest()
    stamp = BUILD / 'fingerprint'
    rebuild = not stamp.exists() or stamp.read_text() != fingerprint
    commands = []
    with (BUILD / 'compile.log').open('a') as log:
        def run(command):
            commands.append(list(map(str, command)))
            log.write(json.dumps(commands[-1]) + '\n');log.flush()
            subprocess.run(commands[-1], stdout=log, stderr=subprocess.STDOUT, check=True, timeout=180)
        objects = []
        for source in [*sources, 'live/dormant_runtime_test.cpp']:
            obj = BUILD / (source.replace('/', '_') + '.o');objects.append(obj)
            if rebuild or not obj.exists():
                run(['g++', *flags, '-c', ROOT / source, '-o', obj])
        exe = BUILD / 'dormant-runtime-test'
        if rebuild or not exe.exists():
            run(['g++', *flags, *objects, '-o', exe])
        stamp.write_text(fingerprint)
    rows = []
    modes = ['baseline', 'responsive', 'blocked', 'nested']
    if not args.baseline_red:
        modes += ['cold-seal', 'connect', 'engage-race']
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
        complete = code == 0 and 'DORMANT clean-teardown=1' in text
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
        rows.append(dict(mode=mode, returncode=code, complete=complete, caller=returned_caller,
                         exact_red=exact_red, passed=passed, fatal=fields))
        print(json.dumps(rows[-1]), flush=True)
    assert hashes == {str(p.relative_to(ROOT)): hashlib.sha256(p.read_bytes()).hexdigest() for p in inputs}
    result = dict(source_model=True, native_execution=False, memory_max='256M', swap_max=0,
                  baseline_red=args.baseline_red, source_hashes=hashes, commands=commands, cases=rows)
    (BUILD / ('baseline-red.json' if args.baseline_red else 'acceptance.json')).write_text(json.dumps(result, indent=2) + '\n')
    return 0 if all(row['passed'] for row in rows) else 1


if __name__ == '__main__':
    raise SystemExit(main())
