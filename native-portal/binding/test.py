#!/usr/bin/env python3
# SPDX-License-Identifier: MIT
"""Offline only. Private metadata stays in ignored binding/build, no proprietary bytes."""
import argparse
import csv
import hashlib
import json
import os
from pathlib import Path
import subprocess

p = argparse.ArgumentParser()
p.add_argument('--evidence', type=Path, required=True,
               help='Private live03 completion analysis directory')
p.add_argument('--sanitize', action='store_true')
a = p.parse_args()
root = Path(__file__).resolve().parents[1]
build = root / 'binding/build'
build.mkdir(parents=True, exist_ok=True)
# Verify this runner actually resides inside the requested RAM/swap limits.
cgroup = Path('/sys/fs/cgroup') / Path('/proc/self/cgroup').read_text().strip().split('::')[1].lstrip('/')
assert (cgroup / 'memory.max').read_text().strip() == '681574400'
assert (cgroup / 'memory.swap.max').read_text().strip() == '0'
expected_inputs = {
    'evidence.json': '66758c0662db1d37c94a6abde73c1d36ca7cf89be63d3a1b4844bf3eac01e055',
    'camera-chains.csv': '6c8487755c90b8851a844ca05a8117865e06538b671d775b58718aa8543ad502',
    'records.jsonl': '4b483a263c2346ee24a646138069dad96260300944bd722139b7d12f0bbfae18',
}
for name, expected in expected_inputs.items():
    assert hashlib.sha256((a.evidence / name).read_bytes()).hexdigest() == expected, name
e = json.loads((a.evidence / 'evidence.json').read_text())
rows = list(csv.DictReader((a.evidence / 'camera-chains.csv').open()))
records = {r['seq']: r for r in map(json.loads, (a.evidence / 'records.jsonl').read_text().splitlines()) if 'seq' in r}
assert [f['frame'] for f in e['frames']] == [601, 602, 603, 743, 744, 745]
assert len(rows) == 6

def val(x):
    return int(x, 16) if isinstance(x, str) and x.startswith('0x') else int(x)

def bits(seq, obj, captured=False):
    r = records[seq]
    if captured:
        entry = next(c['entry'] for c in r['chunks'] if c.get('entry'))
        assert entry['valid'] and entry['selected']
        sp, lr = entry['sp'], entry['lr']
        assert val(entry['object']) == val(obj)
    else:
        sp, lr = r['sp'], r['x'][30]
    return '{' + ','.join(str(val(x)) + 'ULL' for x in (seq, r['tid'], sp, lr, obj)) + '}'

lines = ['// PRIVATE live03 metadata only. Generated; do not commit.', 'static const Chain chains[] = {']
for f, row in zip(e['frames'], rows):
    q, w, ar = f['queue'], f['wait'], f['pair'][0]['arena']
    assert int(row['frame']) == f['frame']
    for key, seq in [('submit_call',q['call']),('submit_result',q['result']),
                     ('submit_stamp',q['stamp']),('submit_stamped',q['stamped']),
                     ('wait_enter',w['enter']),('wait_result',w['result']),('wait_return',w['returned'])]:
        assert int(row[key]) == seq
    assert f['backend_through_wait_contiguous'] and q['contiguous'] and w['contiguous']
    assert val(records[q['result']]['x'][0]) == q['raw_result'] == 0
    assert val(records[w['result']]['x'][0]) == w['raw_result'] == 0
    assert val(records[w['returned']]['x'][0]) == w['boolean'] == 1
    entries = [next(c['entry'] for c in records[w[k]]['chunks'] if c.get('entry')) for k in ('enter','result','returned')]
    assert entries[0] == entries[1] == entries[2]
    assert val(entries[0]['object']) == val(q['token']['sync'])
    assert entries[0]['serial'] == q['token']['serial']
    assert entries[0]['subserial'] == q['token']['subserial']
    assert w['serial'] == q['token']['serial'] and w['subserial'] == q['token']['subserial']
    for key in ('call', 'result', 'stamped'):
        assert records[q[key]]['x'][19] == q['command']
        assert records[q[key]]['sp'] == records[q['call']]['sp']
    for key in ('arena_stamp', 'command_stamp'):
        post = f[key]['post_token']
        assert post['resource'] == f[key]['object']
        assert {k:post[k] for k in ('sync','serial','subserial')} == q['token']
        assert records[f[key]['post']]['x'][19] == f[key]['object']
    for pair in f['pair']:
        returned = next(c['entry'] for c in records[pair['returned']]['chunks'] if c.get('entry'))
        entered = records[pair['backend']]
        assert returned['valid'] and returned['selected']
        assert returned['sp'] == entered['sp'] and returned['lr'] == entered['x'][30]
        assert returned['object'] == pair['backend_address']
    values = [f['frame'], ar['renderer'], q['command'], ar['arena_resource'], ar['buffer'],
              ar['map'], ar['capacity'], ar['alignment'], q['queue'], w['semaphore'],
              q['token']['sync'], q['token']['serial']]
    line = '{' + ','.join(str(val(v)) + 'ULL' for v in values)
    line += ',' + ','.join(str(x) for x in [q['token']['subserial'], q['raw_result'], w['raw_result'], w['boolean']])
    line += ',{' + ','.join(bits(p['backend'],p['backend_address']) for p in f['pair']) + '}'
    line += ',' + bits(q['call'],q['command']) + ',' + bits(q['stamp'],q['command'])
    line += ',{' + ','.join(bits(f[k]['enter'],f[k]['object']) for k in ('arena_stamp','command_stamp')) + '}'
    line += ',' + bits(w['enter'],w['sync'],True) + '},'
    lines.append(line)
lines.append('};')
(build / 'private_chains.hpp').write_text('\n'.join(lines) + '\n')
cmd = ['g++', '-std=c++17', '-Wall', '-Wextra', '-Werror', '-g', '-pthread', '-I'+str(root/'include')]
if a.sanitize:
    cmd += ['-fsanitize=address,undefined','-fno-omit-frame-pointer','-fno-pie','-no-pie']
out = build / ('test-sanitize' if a.sanitize else 'test')
cmd += [str(root/x) for x in ['binding/test.cpp','binding/command.cpp','render/adapter.cpp','arena/bridge.cpp',
                            'src/aperture.cpp','control/protocol.cpp','control/policy.cpp']]
cmd += ['-o',str(out)]
subprocess.run(cmd,check=True)
subprocess.run([str(out)],check=True,env={**os.environ,'ASAN_OPTIONS':'detect_leaks=1'})
print(json.dumps({'memory.max':(cgroup/'memory.max').read_text().strip(),
                  'memory.swap.max':(cgroup/'memory.swap.max').read_text().strip(),
                  'memory.peak':(cgroup/'memory.peak').read_text().strip(),
                  'sources':{str(x.relative_to(root)):hashlib.sha256(x.read_bytes()).hexdigest()
                             for x in [root/'render/adapter.cpp',root/'arena/bridge.cpp',root/'binding/command.cpp']},
                  'private_inputs':{x:hashlib.sha256((a.evidence/x).read_bytes()).hexdigest()
                                    for x in ['evidence.json','camera-chains.csv','records.jsonl']},
                  'sanitized':a.sanitize,'chains':6}))
