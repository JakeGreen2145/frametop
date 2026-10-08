# SPDX-License-Identifier: MIT
"""Offline production adapter runner. Never loads VR or touches active builds."""
import argparse
from pathlib import Path
import subprocess
import hashlib
import importlib.util
import struct
import sys
import os
os.environ.setdefault("TMPDIR", str(Path.home()/".hermes/cache/scratch"))
sys.dont_write_bytecode = True
p = argparse.ArgumentParser()
p.add_argument('--sanitize', action='store_true')
p.add_argument('--trace', type=Path, help='Private pinned metadata trace; never committed')
a = p.parse_args()
root = Path(__file__).resolve().parents[1]
build = root / 'render' / 'build'
build.mkdir(exist_ok=True)
out = build / ('test-sanitize' if a.sanitize else 'test')
cmd = ['g++', '-std=c++17', '-Wall', '-Wextra', '-Werror', '-g', '-pthread', '-I'+str(root/'include')]
if a.sanitize:
    cmd += ['-fsanitize=address,undefined', '-fno-omit-frame-pointer', '-fno-pie', '-no-pie']
cmd += [str(root/x) for x in ['render/stereo_commit_test.cpp','render/adapter.cpp','arena/bridge.cpp','src/aperture.cpp','control/protocol.cpp','control/policy.cpp','control/transport.cpp']]
cmd += ['-o', str(out)]
subprocess.run(cmd, check=True)
args = [str(out)]
if a.trace:
    assert hashlib.sha256(a.trace.read_bytes()).hexdigest() == '0f2b0701eab2a02d3688133db18233655edca901fd2aed404030635f88e63be4'
    spec = importlib.util.spec_from_file_location('decode', root / 'hooks' / 'decode.py')
    assert spec is not None and spec.loader is not None
    decoder = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(decoder)
    with a.trace.open('rb') as stream:
        events = [e for e in decoder.records(stream) if e.get('event') == 'camera_enqueue']
    assert len(events) == 882
    fixture = build / 'private-fixtures.bin'
    with fixture.open('wb') as stream:
        stream.write(struct.pack('<I', len(events)))
        for i, e in enumerate(events):
            assert e['eye'] == i % 2
            for tag, size in [(3, 352), (1, 528), (2, 48)]:
                chunk = next(c for c in e['chunks'] if c['tag'] == tag)
                assert chunk['valid']
                data = bytes.fromhex(chunk['hex'])
                assert len(data) == size
                stream.write(data)
    args.append(str(fixture))
subprocess.run(args, check=True)
