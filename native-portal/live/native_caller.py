#!/usr/bin/env python3
# SPDX-License-Identifier: MIT
"""Derive Run's return PC by executing the pinned native pthread thunk."""
import hashlib
import json
import os
from pathlib import Path
import struct


def run_return():
    from elftools.elf.elffile import ELFFile
    from unicorn import Uc, UC_ARCH_ARM64, UC_MODE_ARM
    import unicorn.arm64_const as A
    elf = Path(os.environ.get('FT_HOOK_TEST_ELF', '/home/jakeg/.hermes/work/frame-native-reuse/camera-analysis/vrcompositor'))
    assert hashlib.sha256(elf.read_bytes()).hexdigest() == '9b1bd5fe4463592cbc71b2517b9ceb0a0fcd66cd6885a19dca4be0784bf6c265'
    with elf.open('rb') as stream:
        segments = [(s['p_vaddr'], s.data()) for s in ELFFile(stream).iter_segments() if s['p_type'] == 'PT_LOAD']
    def read(address, size):
        for start, data in segments:
            if start <= address and address + size <= start + len(data):
                return data[address-start:address-start+size]
        raise AssertionError(hex(address))
    base, worker, stack = 0x70000000, 0x90000000, 0x91000000
    run = struct.unpack('<Q', read(0x5a1428 + 0x18, 8))[0]
    assert run == 0x14e5a8
    machine = Uc(UC_ARCH_ARM64, UC_MODE_ARM)
    for address in [base+0x182000, base+0x14e000, base+0x5a1000, worker, stack]:
        machine.mem_map(address, 4096)
    machine.mem_write(base+0x182a94, read(0x182a94, 0x14))
    machine.mem_write(worker, struct.pack('<Q', base+0x5a1428))
    machine.mem_write(base+0x5a1428+0x18, struct.pack('<Q', base+run))
    machine.mem_write(stack+0x38, struct.pack('<Q', worker))
    machine.reg_write(A.UC_ARM64_REG_SP, stack)
    machine.emu_start(base+0x182a94, base+run, count=5)
    assert machine.reg_read(A.UC_ARM64_REG_PC) == base+run
    assert machine.reg_read(A.UC_ARM64_REG_X0) == worker
    return machine.reg_read(A.UC_ARM64_REG_X30)-base


if __name__ == '__main__':
    print(json.dumps({'native_run_return': hex(run_return())}))
