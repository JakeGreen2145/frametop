#!/usr/bin/env python3
# SPDX-License-Identifier: MIT
"""CPU-only caller lifetime against production owner/gate/cleanup. No native ELF."""
import argparse
import os
from pathlib import Path
import subprocess
import uuid


def main():
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument('--sanitize', action='store_true')
    args = p.parse_args()
    root = Path(__file__).resolve().parents[1]
    out = root.parent / 'build/camera-caller'
    out.mkdir(parents=True, exist_ok=True)
    scope = ['systemd-run', '--user', '--scope', '-p', 'MemoryMax=256M', '-p', 'MemorySwapMax=0']

    def run(command):
        unit = 'camera-caller-'+uuid.uuid4().hex+'.scope'
        try:
            subprocess.run([*scope, '--unit='+unit, *map(str, command)],
                           check=True, timeout=120,
                           env={**os.environ, 'ASAN_OPTIONS': 'detect_leaks=1'})
        except subprocess.TimeoutExpired:
            subprocess.run(['systemctl', '--user', 'stop', unit], check=False, timeout=15)
            raise

    flags = ['-std=c++17', '-O0', '-g1', '-Wall', '-Wextra', '-Werror', '-pthread',
             '--param=ggc-min-expand=10', '--param=ggc-min-heapsize=4096',
             '-ffunction-sections', '-fdata-sections']
    if args.sanitize:
        flags += ['-fsanitize=address,undefined', '-fno-omit-frame-pointer', '-fno-pie', '-no-pie']
    objects = []
    for source in ['live/camera_caller_test.cpp', 'live/camera_owner.cpp',
                   'control/camera_gate.cpp', 'control/owner_coordinator.cpp', 'binding/camera.cpp']:
        obj = out / (source.replace('/', '_')+'.o')
        run(['g++', *flags, '-c', root/source, '-o', obj])
        objects.append(obj)
    exe = out / ('camera-caller-sanitize' if args.sanitize else 'camera-caller')
    # Discard only unused RenderDebt methods, whose complete Adapter composition
    # remains covered by camera_owner_test.py. No production implementation copied.
    run(['g++', *flags, *objects, '-Wl,--gc-sections',
         '-Wl,--wrap=_ZN13native_portal16CameraAccessGate7releaseEm',
         '-Wl,--wrap=_ZN13native_portal16CameraAccessGate6retainEmRm', '-o', exe])
    run([exe])


if __name__ == '__main__':
    main()
