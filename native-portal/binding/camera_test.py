#!/usr/bin/env python3
# SPDX-License-Identifier: MIT
"""Focused camera binding CPU tests. Run inside a 256 MiB, zero-swap scope."""
import argparse
from pathlib import Path
import subprocess

ROOT=Path(__file__).resolve().parents[1]

def main():
    p=argparse.ArgumentParser();p.add_argument('--asan',action='store_true');args=p.parse_args()
    out=ROOT/'build/camera-gate';out.mkdir(parents=True,exist_ok=True)
    exe=out/('binding-test-asan' if args.asan else 'binding-test')
    flags=['-fsanitize=address,undefined','-fno-omit-frame-pointer','-g0','-O0'] if args.asan else ['-O2']
    subprocess.run(['g++','-std=c++17','-pthread','-Wall','-Wextra','-Werror',*flags,
                    *[str(ROOT/x) for x in ['control/camera_gate.cpp','binding/camera.cpp','binding/camera_test.cpp']],
                    '-o',str(exe)],check=True,timeout=90)
    subprocess.run([str(exe)],check=True,timeout=20)

if __name__=='__main__':main()
