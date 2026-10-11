#!/usr/bin/env python3
# SPDX-License-Identifier: MIT
"""Pinned current-render producer islands; synthetic memory, not whole render.

Runs the actual selector call/helper/continuation, slot and texture loads.
No service/GPU calls, installations, native activation or inferred live lifetime.
"""
import argparse
import importlib
import json
import struct
import sys
from pathlib import Path
from stock_native_vectors import digest, native_bytes


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument('--compositor', type=Path, required=True)
    ap.add_argument('--python-deps', type=Path, required=True)
    ap.add_argument('--output', type=Path, required=True)
    args = ap.parse_args()
    assert digest(args.compositor) == '9b1bd5fe4463592cbc71b2517b9ceb0a0fcd66cd6885a19dca4be0784bf6c265'
    request = json.loads(Path(__file__).with_name('current-source-native-hook-request.json').read_text())
    for site in request['render_required_callbacks']:
        assert native_bytes(args.compositor, int(site['pc'], 16), 16).hex() == site['guard16']
    sys.path.insert(0, str(args.python_deps))
    unicorn = importlib.import_module('unicorn')
    a = importlib.import_module('unicorn.arm64_const')
    u = unicorn.Uc(unicorn.UC_ARCH_ARM64, unicorn.UC_MODE_ARM)
    u.mem_map(0x160000, 0x3000)
    u.mem_write(0x160000, native_bytes(args.compositor, 0x160000, 0x3000))
    camera, entry_sp, source = 0x90000000, 0x90040000, 0x90200000
    u.mem_map(camera, 0x10000)
    u.mem_map(entry_sp - 0x10000, 0x20000)
    u.mem_map(source, 0x10000)
    registers = [getattr(a, f'UC_ARM64_REG_X{i}') for i in range(31)]
    u.reg_write(a.UC_ARM64_REG_CPACR_EL1, 3 << 20)
    u.reg_write(a.UC_ARM64_REG_SP, entry_sp - 0x460)
    u.reg_write(registers[27], camera)
    u.reg_write(registers[28], 99)
    u.reg_write(registers[30], 0x123456)
    u.mem_write(camera + 0x360, b'\x01')
    u.mem_write(camera + 0x820, struct.pack('<I', 3))
    u.mem_write(camera + 0x520 + 24 * 3, struct.pack('<Q', 555))
    records = []

    def save(pc):
        assert u.reg_read(a.UC_ARM64_REG_PC) == pc
        values = [u.reg_read(r) for r in registers] + [u.reg_read(a.UC_ARM64_REG_SP)]
        saved = struct.pack('<32Q', *values)
        saved += b''.join(u.reg_read(getattr(a, f'UC_ARM64_REG_Q{i}')).to_bytes(16, 'little') for i in range(32))
        saved += struct.pack('<4Q', *(u.reg_read(getattr(a, 'UC_ARM64_REG_' + r)) for r in ['NZCV', 'FPCR', 'FPSR']), 0)
        assert len(saved) == 800
        records.append(struct.pack('<Q', pc) + saved)

    u.emu_start(0x160ca4, 0x160ca8, count=1)
    assert u.reg_read(registers[0]) == 1
    save(0x160ca8)
    u.reg_write(registers[0], camera)
    u.reg_write(registers[1], 0)
    u.reg_write(registers[2], source)
    # Both zeroed resource matrices tie at the actual native selector. Native
    # csel keeps index0. This is not a modeled helper return.
    u.emu_start(0x161bc0, 0x161be8, count=2000)
    assert u.reg_read(registers[0]) == 0 and u.reg_read(registers[28]) == 99
    save(0x161be8)
    u.emu_start(0x161be8, 0x161c74, count=200)
    assert u.reg_read(registers[0]) == camera
    save(0x161c74)
    u.emu_start(0x161c74, 0x161c78, count=1)
    assert u.reg_read(registers[24]) == 3
    save(0x161c78)
    u.reg_write(registers[21], 3)
    u.emu_start(0x161e10, 0x161e40, count=50)
    assert u.reg_read(registers[0]) == 555
    save(0x161e40)
    args.output.parent.mkdir(parents=True, exist_ok=True)
    args.output.write_bytes(b''.join(records))
    report = {'scope': __doc__, 'compositor_sha256': digest(args.compositor), 'vectors': len(records),
              'output_sha256': digest(args.output), 'selector': 'actual 161bc0 BL and 160a80 helper, zero-matrix tie selects0',
              'synthetic': 'camera/stack/source contents and schedule between independent instruction islands',
              'pcs': [hex(struct.unpack_from('<Q', r)[0]) for r in records]}
    args.output.with_suffix('.json').write_text(json.dumps(report, indent=2) + '\n')
    print(json.dumps(report, indent=2))


if __name__ == '__main__':
    main()
