#!/usr/bin/env python3
# SPDX-License-Identifier: MIT
"""Source-only stock-receipt CPU tests. Never imports native analysis runners."""
import argparse
import ast
import json
import os
import resource
import subprocess
import uuid
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
BUILD = ROOT / 'build/stock-source-receipts'


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument('--sanitize', action='store_true')
    parser.add_argument('--current-vectors', type=Path, help='replay pinned current-render producer Saved vectors')
    parser.add_argument('--native-vectors', type=Path, help='replay generated pinned producer Saved vectors')
    parser.add_argument('--runtime', action='store_true', help='also build/link complete unchanged Runtime and run its CPU smoke test')
    parser.add_argument('--vulkan-include', type=Path, default=Path('/home/jakeg/.hermes/work/frame-native-pipeline-review/include'))
    args = parser.parse_args()
    resource.setrlimit(resource.RLIMIT_CORE, (0, 0))
    scratch = os.environ.get('TMPDIR', '/home/jakeg/.hermes/cache/scratch')
    assert Path(scratch).is_dir()
    BUILD.mkdir(parents=True, exist_ok=True)
    output = BUILD / ('sanitize' if args.sanitize else 'plain')
    output.mkdir(exist_ok=True)
    results = []
    env = {**os.environ, 'TMPDIR': scratch, 'ASAN_OPTIONS': 'detect_leaks=1:abort_on_error=1'}
    flags = ['-std=c++17', '-O0', '-g1', '-Wall', '-Wextra', '-Werror', '-pthread',
             '--param=ggc-min-expand=10', '--param=ggc-min-heapsize=4096',
             '-I' + str(ROOT / 'include'), '-I' + str(args.vulkan_include)]
    sanitizer = ['-fsanitize=address,undefined', '-fno-omit-frame-pointer', '-no-pie'] if args.sanitize else []

    def run(cmd, capped=True):
        unit = 'stock-receipts-' + uuid.uuid4().hex + '.scope'
        actual = ['systemd-run', '--user', '--scope', '--unit=' + unit, '-p', 'MemoryMax=256M', '-p', 'MemorySwapMax=0', *cmd] if capped else cmd
        try:
            r = subprocess.run(actual, capture_output=True, text=True, env=env, timeout=180, check=False)
        except subprocess.TimeoutExpired:
            if capped:
                subprocess.run(['systemctl', '--user', 'stop', unit], check=False, timeout=15)
            raise
        print(r.stdout + r.stderr, end='', flush=True)
        results.append({'command': actual, 'exit': r.returncode, 'output': r.stdout + r.stderr})
        (output / 'results.json').write_text(json.dumps(results, indent=2) + '\n')
        r.check_returncode()
        return r.stdout

    objects = {}

    def compile_one(source):
        if source not in objects:
            obj = output / (source.replace('/', '_') + '.o')
            run(['g++', *flags, *sanitizer, '-c', str(ROOT / source), '-o', str(obj)])
            objects[source] = str(obj)
        return objects[source]

    def target(name, sources):
        objs = [compile_one(s) for s in sources]
        binary = output / name
        run(['g++', '-pthread', *sanitizer, *objs, '-o', str(binary)])
        # ASan reserves virtual address space independently of resident RAM.
        run([str(binary)])

    target('native-contract', ['live/stock_native_contract_test.cpp', 'live/source_provider.cpp', 'control/camera_gate.cpp'])
    inventory = run([str(output / 'native-contract'), '--inventory'])
    actual_sites = [tuple(line.split()) for line in inventory.splitlines()]
    requested = json.loads((ROOT / 'live/stock-native-source-hook-request.json').read_text())
    additions = json.loads((ROOT / 'live/admission-alternatives-hook-request.json').read_text())['new_required_sites']
    expected_sites = {(s['module'], s['pc']) for s in requested['required_callbacks']}
    expected_sites.update((s['module'], s['pc']) for s in additions if s['module'] == 'client')
    assert len(actual_sites) == len(set(actual_sites)) == len(expected_sites)
    assert set(actual_sites) == expected_sites
    if args.native_vectors:
        run([str(output / 'native-contract'), str(args.native_vectors)])
    target('stock-receipts', ['live/stock_source_receipts_test.cpp', 'live/source_provider.cpp', 'control/camera_gate.cpp'])
    target('existing-admission', ['live/native_source_admission_test.cpp'])
    target('current-admission', ['live/current_source_admission_test.cpp', 'live/source_provider.cpp', 'control/camera_gate.cpp'])
    current_inventory = run([str(output / 'current-admission'), '--inventory'])
    current_sites = [tuple(line.split()) for line in current_inventory.splitlines()]
    current_request = json.loads((ROOT / 'live/current-source-native-hook-request.json').read_text())
    expected_current = {(s['module'], s['pc']) for s in current_request['render_required_callbacks']}
    expected_current.update((s['module'], s['pc']) for s in additions if s['module'] == 'compositor')
    assert len(current_sites) == len(set(current_sites)) == len(expected_current)
    assert set(current_sites) == expected_current
    if args.current_vectors:
        run([str(output / 'current-admission'), str(args.current_vectors)])
    if args.runtime:
        tree = ast.parse((ROOT / 'live/runtime_test.py').read_text())
        sources = next(ast.literal_eval(n.value) for n in tree.body if isinstance(n, ast.Assign)
                       and any(isinstance(t, ast.Name) and t.id == 'SOURCES' for t in n.targets))
        assert 'live/runtime.cpp' in sources and 'live/source_provider.cpp' in sources
        target('runtime-smoke', sources + ['live/runtime_test.cpp'])
    print('PASS source-only stock receipts and unchanged admission' + ('; complete Runtime link/smoke' if args.runtime else ''))


if __name__ == '__main__':
    main()
