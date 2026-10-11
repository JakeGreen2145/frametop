#!/usr/bin/env python3
# SPDX-License-Identifier: MIT
"""Execute only delivered native field producers. No native services or activation.

PT_LOAD bytes are hash-pinned. Observer guards match the delivered permissions
file. Memory/entry registers are synthetic, and no whole-method ABI is inferred.
"""
import argparse
import gc
import hashlib
import importlib
import json
import struct
import sys
from pathlib import Path


def native_bytes(path, pc, size):
    with path.open('rb') as f:
        header = f.read(64)
        assert header[:6] == b'\x7fELF\x02\x01'
        phoff = struct.unpack_from('<Q', header, 32)[0]
        entsize, count = struct.unpack_from('<HH', header, 54)
        for i in range(count):
            f.seek(phoff + i * entsize)
            kind, flags, offset, vaddr, _, filesz, _, _ = struct.unpack('<II6Q', f.read(56))
            if kind == 1 and flags & 1 and vaddr <= pc and pc + size <= vaddr + filesz:
                f.seek(offset + pc - vaddr)
                return f.read(size)
    raise ValueError(f'no executable file bytes at {pc:x}')


def digest(path):
    with path.open('rb') as f:
        return hashlib.file_digest(f, 'sha256').hexdigest()


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument('--permissions', type=Path, required=True)
    ap.add_argument('--compositor', type=Path, required=True)
    ap.add_argument('--client', type=Path, required=True)
    ap.add_argument('--python-deps', type=Path, required=True)
    ap.add_argument('--output', type=Path, required=True)
    args = ap.parse_args()
    assert digest(args.permissions) == 'bc95f7474990c13630870fafd4fb56c955982e6d05209295604f0ebfd26b52d0'
    table = json.loads(args.permissions.read_text())
    for module in ('compositor', 'client'):
        assert digest(getattr(args, module)) == table['module_pins'][module]
    sys.path.insert(0, str(args.python_deps))
    unicorn = importlib.import_module('unicorn')
    A = importlib.import_module('unicorn.arm64_const')
    # observer, preceding actual producer, method, field bit, byte width,
    # field offset, native operand register, store versus load, allowed values
    producers = [
        (0x15f9fc, 0x15f9f8, 0x15f9e0, 8, 1, 0x34a, 0, True, (0, 1, 255)),
        (0x15fa60, 0x15fa5c, 0x15f9e0, 8, 1, 0x34a, 0, False, (0, 1, 255)),
        (0x15fa94, 0x15fa90, 0x15f9e0, 8, 1, 0x34a, 0, False, (0, 1)),
        (0x15fa04, 0x15fa00, 0x15f9e0, 16, 1, 0x360, 0, False, (0, 1)),
        (0x15fa8c, 0x15fa88, 0x15f9e0, 16, 1, 0x360, 0, False, (0, 1)),
        (0x15fabc, 0x15fab8, 0x15f9e0, 16, 1, 0x360, 0, True, (1,)),
        (0x15fbc0, 0x15fbbc, 0x15fba8, 16, 1, 0x360, 0, False, (0, 1)),
        (0x15fbc4, 0x15fbc0, 0x15fba8, 8, 1, 0x34a, None, True, (0,)),
        (0x15fc4c, 0x15fc48, 0x15fba8, 16, 1, 0x360, None, True, (0,)),
        (0x15fbd0, 0x15fbcc, 0x15fba8, 32, 1, 0x349, None, True, (0,)),
        (0x15fc08, 0x15fc04, 0x15fba8, 32, 1, 0x349, None, True, (0,)),
        (0x15fd30, 0x15fd2c, 0x15fcc0, 8, 1, 0x34a, 20, False, (0, 1, 255)),
        (0x15fd48, 0x15fd44, 0x15fcc0, 16, 1, 0x360, 1, False, (0, 1)),
        (0x15fd90, 0x15fd8c, 0x15fcc0, 16, 1, 0x360, 0, False, (0, 1)),
        (0x16013c, 0x160138, 0x15fcc0, 32, 1, 0x349, 0, True, (0, 1, 255)),
        (0x32a628, 0x32a624, 0x15f9e0, 2, 8, 0x350, 19, True, (11,)),
    ]
    observers = {(o['module'], int(o['pc'], 16)): o for row in table['rows'] for o in row['observers']}
    camera, stack, manager = 0x90000000, 0x90040000, 0x90020000
    reg = [getattr(A, f'UC_ARM64_REG_X{i}') for i in range(31)]
    records, evidence = [], []
    for pc, producer, method, bit, width, offset, operand, store, values in producers:
        module = 'client' if pc == 0x32a628 else 'compositor'
        path = getattr(args, module)
        guard = native_bytes(path, pc, 16)
        assert guard.hex() == observers[(module, pc)]['guard16']
        code = native_bytes(path, producer, pc - producer)
        for value in values:
            u = unicorn.Uc(unicorn.UC_ARCH_ARM64, unicorn.UC_MODE_ARM)
            u.mem_map(producer & ~0xfff, 0x2000)
            u.mem_write(producer, code)
            u.mem_map(camera, 0x1000)
            u.mem_map(stack - 0x2000, 0x3000)
            before = 0xaa if store else value
            u.mem_write(camera + offset, before.to_bytes(width, 'little'))
            x = [0] * 31
            x[19], x[20], x[23], x[30] = camera, manager, camera + 0x350, 0x123456
            sp = stack - (0x360 if method == 0x15fcc0 else 0x20)
            if module == 'client':
                sp -= 0xc0
                x[30] = 0x15fa28
            if store and operand is not None:
                x[operand] = value
            for i in range(31):
                u.reg_write(reg[i], x[i])
            u.reg_write(A.UC_ARM64_REG_SP, sp)
            u.emu_start(producer, pc, count=8)
            assert u.reg_read(A.UC_ARM64_REG_PC) == pc
            if store:
                actual = int.from_bytes(u.mem_read(camera + offset, width), 'little')
            else:
                assert operand is not None
                actual = u.reg_read(reg[operand])
            assert actual == value
            saved = [u.reg_read(r) for r in reg] + [u.reg_read(A.UC_ARM64_REG_SP)]
            for i in range(32):
                q = u.reg_read(getattr(A, f'UC_ARM64_REG_Q{i}'))
                saved += [q & ((1 << 64) - 1), q >> 64]
            saved += [u.reg_read(A.UC_ARM64_REG_NZCV), u.reg_read(A.UC_ARM64_REG_FPCR), u.reg_read(A.UC_ARM64_REG_FPSR), 0]
            assert len(saved) == 100
            records.append(struct.pack('<106Q', method, int(module == 'client'), pc, bit, actual, width, *saved))
            evidence.append({'module': module, 'pc': hex(pc), 'producer': hex(producer), 'bytes': code.hex(), 'field': bit, 'actual': actual})
            del u
            gc.collect()
    args.output.parent.mkdir(parents=True, exist_ok=True)
    args.output.write_bytes(b''.join(records))
    args.output.with_suffix('.json').write_text(json.dumps({'scope': 'isolated pinned producer execution, synthetic memory and caller inputs', 'count': len(records), 'records': evidence}, indent=2) + '\n')
    print(f'PASS executed {len(records)} pinned native field vectors from {len(producers)} producers')


if __name__ == '__main__':
    main()
