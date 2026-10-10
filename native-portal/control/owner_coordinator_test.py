#!/usr/bin/env python3
"""Focused hardware-free coordinator test; every compiler/test has a 256 MiB cap."""
import argparse
from pathlib import Path
import subprocess

root = Path(__file__).resolve().parents[2]
parser = argparse.ArgumentParser(description=__doc__)
parser.add_argument("--sanitize", action="store_true")
args = parser.parse_args()
out = root / "build/native-owner-coordinator"
out.mkdir(parents=True, exist_ok=True)
scope = ["systemd-run", "--user", "--scope", "-p", "MemoryMax=256M", "-p", "MemorySwapMax=0"]
flags = ["-std=c++17", "-g", "-O1", "-Wall", "-Wextra", "-Werror", "-pthread",
         "--param=ggc-min-expand=10", "--param=ggc-min-heapsize=4096"]
if args.sanitize:
    flags += ["-O0", "-g1", "-fsanitize=address,undefined", "-fno-omit-frame-pointer", "-no-pie"]
objects = []
for stem in ("owner_coordinator", "owner_coordinator_test"):
    obj = out / (stem + ".o")
    subprocess.run([*scope, "g++", *flags, "-c", str(root / "native-portal/control" / (stem + ".cpp")), "-o", str(obj)], check=True, timeout=90)
    objects.append(str(obj))
binary = out / "owner_coordinator_test"
subprocess.run([*scope, "g++", *flags, *objects, "-o", str(binary)], check=True, timeout=90)
subprocess.run([*scope, str(binary)], check=True, timeout=30)
