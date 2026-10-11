#!/usr/bin/env python3
# SPDX-License-Identifier: MIT
"""Source-only actual Runtime regression. Never import native-evidence runners.

Default: expected-abort verification GREEN only on exact current failure.
--regression: product acceptance RED until a paused owner no longer blocks an
outer-only original. The modeled caller baseline is not a repaired product.
"""
import argparse
import ast
import hashlib
import json
import os
from pathlib import Path
import resource
import signal
import socket
import subprocess
import sys
import threading
import uuid

ROOT = Path(__file__).resolve().parents[1]
BUILD = ROOT / 'build/blocked-owner-dispatch'
SCRATCH = Path('/home/jakeg/.hermes/cache/scratch')
DEFAULT_FIXTURES = Path('/home/jakeg/.hermes/work/frametop-native-live-integration/native-portal/build/camera-runtime-asan')
DEFAULT_COORDS = SCRATCH / 'camera-abort-488/derived-native-coordinates.json'


def digest(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--in-scope', action='store_true', help=argparse.SUPPRESS)
    parser.add_argument('--regression', action='store_true')
    parser.add_argument('--fixtures', type=Path, default=DEFAULT_FIXTURES)
    parser.add_argument('--coordinates', type=Path, default=DEFAULT_COORDS)
    args = parser.parse_args()
    if not args.in_scope:
        unit = 'blocked-owner-' + uuid.uuid4().hex + '.scope'
        command = ['systemd-run', '--user', '--scope', '--quiet', '--unit=' + unit,
                   '-p', 'MemoryMax=256M', '-p', 'MemorySwapMax=0',
                   sys.executable, str(Path(__file__).resolve()), '--in-scope', *sys.argv[1:]]
        try:
            return subprocess.run(command, timeout=600, check=False).returncode
        except subprocess.TimeoutExpired:
            subprocess.run(['systemctl', '--user', 'stop', unit], timeout=15, check=False)
            raise
    resource.setrlimit(resource.RLIMIT_CORE, (0, 0))
    os.environ['TMPDIR'] = str(SCRATCH)
    BUILD.mkdir(parents=True, exist_ok=True)
    # Literal data only. These existing runners import vendor inspection modules.
    tree = ast.parse((ROOT / 'live/runtime_test.py').read_text())
    sources = next(ast.literal_eval(node.value) for node in tree.body
                   if isinstance(node, ast.Assign) and any(
                       isinstance(t, ast.Name) and t.id == 'SOURCES' for t in node.targets))
    fixtures = {}
    for name in ['camera.private.hpp', 'pump.private.hpp', 'selection.private.hpp']:
        origin = args.fixtures / name
        fixtures[str(origin)] = digest(origin)
        (BUILD / name).write_bytes(origin.read_bytes())
    fixtures[str(args.coordinates)] = digest(args.coordinates)
    coords = json.loads(args.coordinates.read_text())
    flags = ['-std=c++17', '-O0', '-g1', '-Wall', '-Wextra', '-Werror', '-pthread', '-no-pie',
             '--param=ggc-min-expand=10', '--param=ggc-min-heapsize=4096',
             '-I' + str(ROOT / 'include'),
             '-I/home/jakeg/.hermes/work/frame-native-pipeline-review/include', '-I' + str(BUILD),
             '-DFT_NATIVE_PORTAL_TEST_CALL', '-DFT_NATIVE_RUN_RETURN=' + hex(coords['run_return'])]
    for name, key in [('BOOT_DISPATCH', 'dispatch_lrs'), ('BOOT_RELEASE', 'release_lr'),
                      ('BOOT_POOL', 'pool_lr'), ('BOOT_QUEUE', 'queue_lr'), ('RECYCLE', 'recycle_lr'),
                      ('STOCK_POOL', 'stock_pool_lr'), ('INITIAL_POOL', 'initial_pool_lr'), ('OUTER', 'outer_lr')]:
        value = coords['bootstrap'][key]
        flags.append('-DFT_' + name + '_LR=' + hex(value[0] if isinstance(value, list) else value))
    inputs = [p for p in ROOT.rglob('*') if p.is_file() and 'build' not in p.relative_to(ROOT).parts
              and p.suffix in ('.cpp', '.hpp', '.inc', '.h')]
    source_hashes = {str(p.relative_to(ROOT)): digest(p) for p in inputs}
    fingerprint = hashlib.sha256(json.dumps([source_hashes, fixtures, flags], sort_keys=True).encode()).hexdigest()
    stamp = BUILD / 'build-fingerprint.txt'
    rebuild = not stamp.exists() or stamp.read_text() != fingerprint
    commands = []
    with (BUILD / 'compile.log').open('a') as log:
        def compile_one(command):
            commands.append(list(map(str, command)))
            log.write(json.dumps(commands[-1]) + '\n');log.flush()
            subprocess.run(commands[-1], stdout=log, stderr=subprocess.STDOUT, check=True, timeout=180)
        objects = []
        for source in [*sources, 'live/blocked_owner_test.cpp']:
            obj = BUILD / (source.replace('/', '_') + '.o')
            objects.append(obj)
            if rebuild or not obj.exists():
                compile_one(['g++', *flags, '-c', ROOT / source, '-o', obj])
        exe = BUILD / 'blocked-owner-test'
        if rebuild or not exe.exists():
            compile_one(['g++', *flags, *objects, '-o', exe])
        stamp.write_text(fingerprint)
    rows = []
    for mode in ['caller-baseline', 'responsive', 'blocked']:
        # Production fatal diagnostics use nonblocking send(stderr). A pipe loses
        # that marker. Drain this real socket concurrently, including stdio text.
        reader, writer = socket.socketpair()
        chunks = []
        def drain():
            while True:
                data = reader.recv(65536)
                if not data:
                    break
                chunks.append(data)
        thread = threading.Thread(target=drain)
        thread.start()
        child = subprocess.Popen([str(exe), mode], stdout=writer.fileno(), stderr=writer.fileno())
        writer.close()
        try:
            code = child.wait(timeout=15)
        except subprocess.TimeoutExpired:
            child.kill();child.wait();raise
        finally:
            thread.join(timeout=5)
            reader.close()
        text = b''.join(chunks).decode(errors='replace')
        (BUILD / (mode + '.log')).write_text(text)
        prelude = 'bound=1 barrier_retained=1 run_authenticated=1 frames=0 uploads=0 pending=0 portal_requests=0 original_calls=0' in text
        fatal_lines = [line for line in text.splitlines() if line.startswith('ft-camera-fatal ')]
        fatal = dict(word.split('=', 1) for word in fatal_lines[0].split()[1:]) if fatal_lines else {}
        returns = [line for line in text.splitlines() if line.startswith('SOURCE-MODEL returned ')]
        returned = dict(word.split('=', 1) for word in returns[0].split()[2:]) if returns else {}
        valid_return = (code == 0 and prelude and returned.get('original_calls') == '1'
                        and returned.get('value') == '0xdecafbad' and returned.get('preserved_lr') == '1'
                        and 'clean-teardown=1' in text and not fatal)
        exact_timeout = (code == -signal.SIGABRT and prelude and not returns
                         and fatal.get('reason') == '0x000000000000000b'
                         and fatal.get('event') == '0x000000000013a100'
                         and fatal.get('bound') == '1'
                         and fatal.get('current_tid') != fatal.get('owner_tid')
                         and 'at-SIGABRT original_calls=0' in text)
        if mode == 'caller-baseline':
            verified = valid_return and returned.get('original_on_caller') == '1' and returned.get('selector') == '0'
        elif mode == 'responsive':
            verified = valid_return and returned.get('original_on_owner') == '1' and returned.get('selector') == '2'
        else:
            verified = exact_timeout
        accepted = valid_return and (mode != 'blocked' or (
            returned.get('original_on_caller') == '1' and returned.get('selector') == '2'))
        rows.append(dict(mode=mode, child_returncode=code, prelude=prelude, fatal=fatal,
                         returned=returned, exact_completion_timeout=exact_timeout,
                         expected_behavior_verified=verified, product_acceptance=accepted))
        print(json.dumps(rows[-1]), flush=True)
    assert source_hashes == {str(p.relative_to(ROOT)): digest(p) for p in inputs}
    expected_verified = all(r['expected_behavior_verified'] for r in rows)
    product_passed = rows[-1]['product_acceptance']
    result = dict(source_model=True, production_changed=False, native_execution=False,
                  memory_max='256M', memory_swap_max=0, source_hashes=source_hashes,
                  fixture_sha256=fixtures, commands=commands, cases=rows,
                  expected_abort_verification='GREEN' if expected_verified else 'FAIL',
                  regression='GREEN' if product_passed else 'RED',
                  limitations='Native functions, publication/Init/Start/Run and loader coverage are modeled. No Vulkan, SteamVR, vendor instruction execution or live timing proof. Queued/NotExecuted follows submit success, paused sole owner and zero original calls, not a private-slot snapshot.')
    (BUILD / 'results.json').write_text(json.dumps(result, indent=2) + '\n')
    if args.regression:
        print('PRODUCT REGRESSION:', result['regression'])
        return 0 if product_passed and all(r['expected_behavior_verified'] for r in rows[:-1]) else 1
    print('EXPECTED-ABORT VERIFICATION:', result['expected_abort_verification'])
    return 0 if expected_verified else 1


if __name__ == '__main__':
    raise SystemExit(main())
