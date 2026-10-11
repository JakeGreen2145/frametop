#!/usr/bin/env python3
# SPDX-License-Identifier: MIT
"""Hardware-free camera-owner composition. Every subprocess is capped at 256 MiB."""
import argparse
import json
import os
from pathlib import Path
import subprocess
import sys
import uuid

p = argparse.ArgumentParser(description=__doc__)
p.add_argument('--sanitize', action='store_true')
p.add_argument('--saved-idle-fixture', type=Path,
               help='reuse an opaque saved camera_idle_fixture.hpp; skip ALL native binary analysis')
a = p.parse_args()
r = Path(__file__).resolve().parents[1]
b = r.parent / 'build/camera-owner'
b.mkdir(parents=True, exist_ok=True)
scope = ['systemd-run', '--user', '--scope', '-p', 'MemoryMax=256M', '-p', 'MemorySwapMax=0']

def run(cmd, **kw):
    unit = 'camera-owner-test-'+uuid.uuid4().hex+'.scope'
    try:
        return subprocess.run([*scope, '--unit='+unit, *map(str, cmd)], check=True, timeout=120, **kw)
    except subprocess.TimeoutExpired:
        # Killing systemd-run alone can leave its scoped compiler alive.
        subprocess.run(['systemctl', '--user', 'stop', unit], check=False, timeout=15)
        raise

# Reuse the production-chain fixture's real Adapter/Command/Lifecycle, not its
# unrelated test main or a second settlement ledger. Generated files stay ignored.
upstream, fixture_guard, _ = (r/'render/upstream_test.cpp').read_text().partition('#ifndef FT_UPSTREAM_FIXTURE_ONLY')
assert fixture_guard, 'upstream fixture boundary missing'
combined = (r/'integration/combined_test.cpp').read_text().split('struct ControlWire {')[0]
assert 'struct UpstreamNative' in upstream and 'struct Combined {' in combined
render = (r/'render/test.cpp').read_text().split('static void initialization() {')[0]
assert 'struct Fake {' in render and 'struct Fixture {' in render
(b/'render_fixture.hpp').write_text(render)
(b/'upstream_fixture.hpp').write_text(upstream.replace('"test.cpp"', '"render_fixture.hpp"'))
(b/'combined_fixture.hpp').write_text(combined.replace('"build/upstream_fixture.hpp"', '"upstream_fixture.hpp"'))

if a.saved_idle_fixture:
    (b/'camera_idle_fixture.hpp').write_bytes(a.saved_idle_fixture.read_bytes())
else:
    # The warm receipt fixture is generated from actual pinned instruction execution.
    # FT_HOOK_TEST_ELF and PYTHONPATH are operator inputs; never fetch device binaries.
    witness = run([sys.executable, r/'live/camera_idle_native.py'], capture_output=True, text=True)
    rows = json.loads(witness.stdout)
    (b/'camera-idle-witness.json').write_text(json.dumps(rows, indent=2)+'\n')
    lines = ['// Generated pinned native transitions; service/lock calls are CPU fakes.',
             'static const Transition transitions[] = {']
    for t in rows['transitions']:
        calls = t['service_calls']
        assert len(calls) <= 2 and all(c[0] in ('acquire', 'release') for c in calls)
        assert len(t['service_results']) == len(calls)
        values = ['{%d,%d,%d}' % (1 if c[0]=='acquire' else 2, c[1], result)
                  for c, result in zip(calls, t['service_results'])]
        values += ['{0,0,0}'] * (2-len(values))
        lines.append('{%d,{%s},{%s},%d,%d,{{%s}}},' % (
            t['pc'], ','.join(map(str,t['before'])), ','.join(map(str,t['after'])),
            t['result'],len(calls),','.join(values)))
    lines.append('};')
    (b/'camera_idle_fixture.hpp').write_text('\n'.join(lines)+'\n')
flags = ['-std=c++17', '-O0', '-g1', '-Wall', '-Wextra', '-Werror', '-pthread',
         '--param=ggc-min-expand=10', '--param=ggc-min-heapsize=4096',
         '-I'+str(b), '-I'+str(r/'render'), '-I'+str(r/'include')]
if a.sanitize:
    flags += ['-fsanitize=address,undefined', '-fno-omit-frame-pointer', '-fno-pie', '-no-pie']
sources = ['live/camera_owner_test.cpp', 'live/camera_owner.cpp',
           'control/owner_coordinator.cpp', 'control/camera_gate.cpp', 'binding/camera.cpp',
           'render/adapter.cpp', 'arena/bridge.cpp', 'binding/pass.cpp', 'binding/command.cpp',
           'binding/lifecycle.cpp', 'src/aperture.cpp', 'control/protocol.cpp',
           'control/policy.cpp', 'control/transport.cpp']
objects = []
for s in sources:
    obj = b/(s.replace('/', '_')+'.o')
    run(['g++', *flags, '-c', r/s, '-o', obj])
    objects.append(obj)
exe = b/('camera-owner-sanitize' if a.sanitize else 'camera-owner')
run(['g++', *flags, *objects, '-o', exe])
run([exe], env={**os.environ, 'ASAN_OPTIONS':'detect_leaks=1'})
label = 'saved opaque receipt replay' if a.saved_idle_fixture else 'fresh pinned receipt replay'
print(f'camera-owner: {label} and real production composition passed; no live hooks/GPU')
