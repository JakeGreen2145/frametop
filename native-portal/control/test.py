#!/usr/bin/env python3
"""CPU only, private runtime paths; --evidence optionally replays the pinned ELF."""
import argparse
import ast
import importlib
import os
from pathlib import Path
import struct
import subprocess
import sys
import tempfile

root = Path(__file__).resolve().parents[2]
p = argparse.ArgumentParser(description=__doc__)
p.add_argument("--sanitize", action="store_true")
p.add_argument("--evidence", type=Path, help="audited frame-native-ownership/verify_ownership.py")
a = p.parse_args()
out = root / "build/native-owner-cpu"
out.mkdir(parents=True, exist_ok=True)
flags = ["-std=c++17", "-g", "-O1", "-Wall", "-Wextra", "-Werror", "-pthread"]
if a.sanitize:
    flags += ["-fsanitize=address,undefined", "-fno-omit-frame-pointer", "-no-pie"]
common = [root / "native-portal/control" / f for f in ("protocol.cpp", "transport.cpp", "policy.cpp")]
for source in ("owner_test", "test", "adversarial_test", "frame_test"):
    sources = [root / "native-portal/control" / (source + ".cpp"), *common]
    if source in ("test", "owner_test"):
        sources += [root / "screens/native_portal.cpp", root / "screens/passthrough.cpp"]
    binary = out / source
    subprocess.run(["g++", *flags, *map(str, sources), "-o", str(binary)], check=True)
    # Parent TMPDIR is respected; only this owned fixture directory is removed.
    with tempfile.TemporaryDirectory(prefix="np-test-", dir=os.environ.get("TMPDIR")) as runtime:
        result = subprocess.run([str(binary)], capture_output=True, text=True, timeout=30,
                                env={**os.environ, "TMPDIR": os.environ.get("TMPDIR", runtime), "XDG_RUNTIME_DIR": runtime})
        print(result.stdout, end="", flush=True)
        print(result.stderr, end="", file=sys.stderr, flush=True)
        result.check_returncode()
        if source == "owner_test":
            # Exercise the shipped GUI/CLI schema against actual Owner replies.
            sys.path.insert(0, str(root / "display-settings"))
            parse_status = importlib.import_module("native_portal_client").parse_status
            replies = [parse_status(line) for line in result.stdout.splitlines() if line.startswith("ok portal ")]
            assert {reply["state"] for reply in replies} >= {"draining", "off", "warming", "fault"}
            print(f"PASS {len(replies)} actual owner replies through GUI/CLI schema", flush=True)
    print(f"PASS {source}", flush=True)
if a.evidence:
    binary = out / "policy-fixture"
    subprocess.run(["g++", *flags, str(root / "native-portal/control/policy_fixture.cpp"), str(common[2]), "-o", str(binary)], check=True)
    rows = subprocess.check_output([str(binary)], text=True).splitlines()
    assert len(rows) == 2560
    # Load only the already-audited emulator setup/functions, never the original
    # script's result writers. No copy or edit of proprietary native instructions.
    evidence = a.evidence.resolve()
    tree = ast.parse(evidence.read_text(), filename=str(evidence))
    prefix = []
    for node in tree.body:
        if isinstance(node, ast.Assign) and any(isinstance(t, ast.Name) and t.id == "policy_cases" for t in node.targets):
            break
        prefix.append(node)
    else:
        raise RuntimeError("evidence prefix changed; re-audit before replay")
    namespace: dict = {"__file__": str(evidence)}
    exec(compile(ast.Module(body=prefix, type_ignores=[]), str(evidence), "exec"), namespace)
    f32 = lambda x: struct.unpack("<f", struct.pack("<f", float(x)))[0]
    count = 0
    for bits in range(256):
        fields = [bool(bits & (1 << n)) for n in range(8)]
        for style in range(5):
            for owned in (False, True):
                expected = namespace["native_policy"](*fields[:7], style, suppression=fields[7], owned=owned)
                values = rows[count].split()
                actual = dict(demand=int(values[0]), construct=int(values[1]), style=int(values[2]), alpha=f32(values[3]))
                assert actual == expected, (bits, style, owned, actual, expected)
                count += 1
    print(f"PASS {count} C++ policy outputs versus actual pinned AArch64 instructions", flush=True)
