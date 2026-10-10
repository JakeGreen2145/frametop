#!/usr/bin/env python3
"""Focused CPU-only camera lifetime tests; no renderer/runtime initialization.

Run inside: systemd-run --user --scope -p MemoryMax=256M -p MemorySwapMax=0 --
    python3 native-portal/control/camera_gate_test.py [--asan]
"""
import argparse
from pathlib import Path
import subprocess

ROOT = Path(__file__).resolve().parents[1]

def main():
    p = argparse.ArgumentParser()
    p.add_argument('--asan', action='store_true')
    args = p.parse_args()
    out = ROOT / 'build' / 'camera-gate'
    out.mkdir(parents=True, exist_ok=True)
    exe = out / ('test-asan' if args.asan else 'test')
    flags = ['-fsanitize=address,undefined', '-fno-omit-frame-pointer', '-g0', '-O0'] if args.asan else ['-O2']
    subprocess.run(['g++', '-std=c++17', '-pthread', '-Wall', '-Wextra', '-Werror', *flags,
                    str(ROOT/'control/camera_gate.cpp'), str(ROOT/'control/camera_gate_test.cpp'),
                    '-o', str(exe)], check=True, timeout=90)
    subprocess.run([str(exe)], check=True, timeout=20)

if __name__ == '__main__':
    main()
