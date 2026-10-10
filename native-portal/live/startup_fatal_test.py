#!/usr/bin/env python3
# SPDX-License-Identifier: MIT
"""Source-only, modeled tests. Read saved fixture data, never evidence runners.

Every compiler and test inherits a systemd 256 MiB / zero-swap cgroup.
No external executable images are inputs. Only our freshly compiled tests run.
"""
import argparse
import ast
import json
import os
from pathlib import Path
import resource
import signal
import socket
import subprocess
import sys
import tempfile
import uuid

ROOT = Path(__file__).resolve().parents[1]
SCRATCH = '/home/jakeg/.hermes/cache/scratch'
PREFIX = 'ft-camera-fatal '


def run_child(exe, mode, sink, strict_drop=False):
    """Install sink only at the callback, after ordinary fixture setup."""
    left = right = file = None
    fd = -1
    try:
        if sink in ('socket', 'saturated', 'closed-peer'):
            left, right = socket.socketpair(socket.AF_UNIX, socket.SOCK_STREAM)
            fd = right.fileno()
            if sink == 'saturated':
                right.setsockopt(socket.SOL_SOCKET, socket.SO_SNDBUF, 4096)
                while True:
                    try:
                        right.send(b'x' * 4096, socket.MSG_DONTWAIT | socket.MSG_NOSIGNAL)
                    except BlockingIOError:
                        break
            if sink == 'closed-peer':
                left.close()
                left = None
        elif sink in ('pipe', 'full-pipe'):
            read_fd, fd = os.pipe()
            file = os.fdopen(read_fd, 'rb', buffering=0)
            if sink == 'full-pipe':
                os.set_blocking(fd, False)
                while True:
                    try:
                        os.write(fd, b'x' * 4096)
                    except BlockingIOError:
                        break
                os.set_blocking(fd, True)
        elif sink == 'file':
            file = tempfile.TemporaryFile(dir=SCRATCH)
            fd = file.fileno()
        result = subprocess.run([str(exe), mode, str(fd)], pass_fds=(fd,) if fd >= 0 else (),
                                capture_output=True, text=True, timeout=5)
        text = ''
        if right:
            right.close()
            right = None
        if left:
            left.settimeout(1)
            chunks = []
            while True:
                part = left.recv(4096)
                if not part:
                    break
                chunks.append(part)
            text = b''.join(chunks).decode()
        if sink in ('pipe', 'full-pipe'):
            os.close(fd)
            fd = -1
            assert file is not None
            text = file.read().decode()
        if sink == 'file':
            assert file is not None
            file.seek(0)
            text = file.read().decode()
        # The full router has an unrelated ordinary diagnostic collector. Only
        # the isolated helper promises that absolutely no other output exists.
        if strict_drop and sink in ('pipe', 'file'):
            assert text == '', (sink, text)
        if sink in ('full-pipe', 'saturated'):
            assert set(text) <= {'x'}, (sink, text)
        return result, text
    finally:
        if left:
            left.close()
        if right:
            right.close()
        if sink in ('pipe', 'full-pipe') and fd >= 0:
            os.close(fd)
        if file:
            file.close()


def fields(line):
    return {k: int(v, 0) for k, v in (word.split('=') for word in line.split()[1:])}


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--coordinates', type=Path, required=True)
    parser.add_argument('--fixtures', type=Path, required=True)
    parser.add_argument('--vulkan-include', type=Path, required=True)
    parser.add_argument('--build', type=Path, default=ROOT / 'live/build/startup-fatal')
    parser.add_argument('--limited', action='store_true', help=argparse.SUPPRESS)
    parser.add_argument('--with-helper', action='store_true')
    args = parser.parse_args()
    if not args.limited:
        unit = 'source-fatal-' + uuid.uuid4().hex
        cmd = ['systemd-run', '--user', '--wait', '--pipe', '--unit=' + unit,
               '-p', 'MemoryMax=256M', '-p', 'MemorySwapMax=0',
               '--setenv=TMPDIR=' + SCRATCH, sys.executable, str(Path(__file__).resolve()),
               *sys.argv[1:], '--limited']
        try:
            return subprocess.run(cmd, timeout=300).returncode
        except subprocess.TimeoutExpired:
            subprocess.run(['systemctl', '--user', 'stop', unit + '.service'], timeout=15, check=False)
            raise
    resource.setrlimit(resource.RLIMIT_CORE, (0, 0))
    # Verify the actual inherited cgroup, not only command-line limit requests.
    cg = Path('/sys/fs/cgroup') / Path('/proc/self/cgroup').read_text().split('0::', 1)[1].strip().lstrip('/')
    assert (cg / 'memory.max').read_text().strip() == str(256 * 1024 * 1024)
    assert (cg / 'memory.swap.max').read_text().strip() == '0'
    os.environ['TMPDIR'] = SCRATCH
    args.build.mkdir(parents=True, exist_ok=True)
    tree = ast.parse((ROOT / 'live/runtime_test.py').read_text())
    sources = next(ast.literal_eval(n.value) for n in tree.body if isinstance(n, ast.Assign)
                   and any(isinstance(t, ast.Name) and t.id == 'SOURCES' for t in n.targets))
    assert all(isinstance(s, str) and s.endswith('.cpp') and not Path(s).is_absolute()
               and '..' not in Path(s).parts for s in sources)
    coords = json.loads(args.coordinates.read_text())
    boot = coords['bootstrap']
    flags = ['-std=c++17', '-O0', '-g1', '-Wall', '-Wextra', '-Werror', '-pthread', '-no-pie',
             '--param=ggc-min-expand=10', '--param=ggc-min-heapsize=4096',
             '-I' + str(ROOT / 'include'), '-I' + str(args.vulkan_include),
             '-I' + str(args.fixtures), '-DFT_NATIVE_PORTAL_TEST_CALL',
             '-DFT_NATIVE_RUN_RETURN=' + hex(coords['run_return'])]
    for name, key in [('BOOT_DISPATCH', 'dispatch_lrs'), ('BOOT_RELEASE', 'release_lr'),
                      ('BOOT_POOL', 'pool_lr'), ('BOOT_QUEUE', 'queue_lr'), ('RECYCLE', 'recycle_lr'),
                      ('STOCK_POOL', 'stock_pool_lr'), ('INITIAL_POOL', 'initial_pool_lr'), ('OUTER', 'outer_lr')]:
        value = boot[key]
        flags += ['-DFT_' + name + '_LR=' + hex(value[0] if isinstance(value, list) else value)]
    objects = []
    for name in sources + ['live/startup_fatal_test.cpp']:
        obj = args.build / (name.replace('/', '_') + '.o')
        subprocess.run(['g++', *flags, '-c', str(ROOT / name), '-o', str(obj)], check=True, timeout=90)
        objects.append(str(obj))
    exe = args.build / 'startup-fatal-test'
    subprocess.run(['g++', *flags, *objects, '-o', str(exe)], check=True, timeout=60)
    rows = []
    failures = []
    cases = [('bootstrap-release', None), ('foreign-noncamera', None),
             ('foreign-release', None), ('foreign-cleanup', None), ('foreign-publication', None)]
    for mode, reason in cases:
        for sink in (['socket'] if reason is None else
                     ['socket', 'saturated', 'closed-peer', 'missing', 'pipe', 'full-pipe', 'file']):
            result, text = run_child(exe, mode, sink)
            assert result.returncode == (0 if reason is None else -signal.SIGABRT), (mode, sink, result)
            assert ('returned' in result.stdout) == (reason is None), (mode, sink, result.stdout)
            records = [fields(line) for line in text.splitlines() if line.startswith(PREFIX)]
            if sink == 'socket' and reason is not None:
                if not records:
                    failures.append(f'{mode}: missing fatal diagnostic, process still SIGABRT')
                else:
                    assert len(records) == 1, records
                    coordinates = fields(next(line for line in result.stdout.splitlines() if line.startswith('coordinates ')))
                    record = records[0]
                    assert record == {**coordinates, 'reason': reason, 'owner_tid': coordinates['bootstrap_tid'],
                                      'bound': 0, 'handoff_wait': 0, 'stopped': 0, 'joined': 0}, record
                    assert record['current_tid'] != record['owner_tid']
            else:
                assert not records, (mode, sink, records)
            rows.append({'mode': mode, 'sink': sink, 'exit': result.returncode, 'records': records})
    if args.with_helper:
        helper = args.build / 'camera-fatal-helper-test'
        subprocess.run(['g++', *flags, str(ROOT / 'live/camera_fatal_test.cpp'), '-Wl,--wrap=send', '-o', str(helper)], check=True, timeout=60)
        for mode in ('emit', 'abort'):
            for sink in ['socket', 'saturated', 'closed-peer', 'missing', 'pipe', 'full-pipe', 'file']:
                result, text = run_child(helper, mode, sink, strict_drop=True)
                assert result.returncode == (0 if mode == 'emit' else -signal.SIGABRT), (mode, sink, result)
                records = [fields(line) for line in text.splitlines() if line.startswith(PREFIX)]
                expected = (13 if mode == 'emit' else 1) if sink == 'socket' else 0
                assert len(records) == expected, (mode, sink, records)
                if sink == 'socket':
                    for number, record in enumerate(records, 1):
                        assert record == {'reason': number, 'event': 2**64-1, 'current_tid': 2**64-1,
                                          'owner_tid': 0, 'bootstrap_tid': 123, 'bound': int(bool(number&1)),
                                          'handoff_wait': int(bool(number&2)), 'stopped': int(bool(number&4)),
                                          'joined': int(bool(number&8))}, record
                rows.append({'helper': mode, 'sink': sink, 'exit': result.returncode, 'records': records})
    report = {'modeled_source_only': True, 'memory_max': 256 * 1024 * 1024, 'swap_max': 0,
              'cases': rows, 'failures': failures}
    (args.build / 'results.json').write_text(json.dumps(report, indent=2) + '\n')
    print(json.dumps({'cases': len(rows), 'failures': failures, 'report': str(args.build / 'results.json')}), flush=True)
    assert not failures, failures
    return 0


if __name__ == '__main__':
    sys.exit(main())
