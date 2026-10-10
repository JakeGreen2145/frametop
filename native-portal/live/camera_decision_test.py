#!/usr/bin/env python3
# SPDX-License-Identifier: MIT
"""Instruction-only aggregate decisions through the real dual-module loader.

The callback selects a route and clobbers CPU registers, not native result lanes.
Runtime policy is tested separately. Services, locks and object storage are models.
"""
import ctypes
import gc
import json
import struct
import unittest

import loader_test as L
from unicorn import Uc, UC_ARCH_ARM64, UC_MODE_ARM, UC_HOOK_CODE, UC_HOOK_INTR
import unicorn.arm64_const as A

DECISIONS = {0x15fbb8: (18, 0x15fbc8, 0x97ffffd0),
             0x15fb20: (19, 0x15fb74, 0x91400801)}
REGS = ([getattr(A, f'UC_ARM64_REG_X{i}') for i in range(31)] +
        [getattr(A, f'UC_ARM64_REG_Q{i}') for i in range(32)] +
        [A.UC_ARM64_REG_SP, A.UC_ARM64_REG_NZCV, A.UC_ARM64_REG_FPCR, A.UC_ARM64_REG_FPSR])
SP = 0x70060000
CAMERA, SHARED, SERVICE, VTABLE = 0x70000000, 0x70010000, 0x70020000, 0x70021000


def state(u):
    return [u.reg_read(r) for r in REGS]


def machine(base):
    u = Uc(UC_ARCH_ARM64, UC_MODE_ARM)
    u.mem_map(base, 0x800000)
    u.mem_map(CAMERA, 0x80000)
    u.mem_write(CAMERA, bytes(i % 251 for i in range(0x80000)))
    values = ([CAMERA + 0x100 * i for i in range(31)] +
              [(i + 1) * 0x123456789abcdef123456789 for i in range(32)] +
              [SP, 0xa0000000, 0x400000, 0x8000011])
    values[30] = base + 0x790000
    for r, v in zip(REGS, values):
        u.reg_write(r, v)
    return u


class DecisionTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        L.compile_tests(L.ROOT / 'live/runtime-sites.json')
        cls.rows = json.loads((L.B / 'sites.private.json').read_text())['sites']
        cls.ids = {row['pc']: i for i, row in enumerate(cls.rows)}
        cls.dll = ctypes.CDLL(str(L.B / 'loader-test.so'))
        cls.emit = cls.dll.ft_loader_emit_site
        cls.emit.argtypes = [ctypes.POINTER(ctypes.c_uint32), ctypes.c_uint64,
                             ctypes.c_uint64, ctypes.c_uint, ctypes.c_uint64]
        cls.emit.restype = ctypes.c_size_t
        cls.pin = staticmethod(L.prepare.image(L.prepare.ELF))
        cls.report = {'instruction_routes': [], 'native_continuations': []}

    def require_sites(self):
        self.assertTrue(set(DECISIONS) <= self.ids.keys(), 'aggregate decision sites absent from actual loader')

    def code(self, pc, at, base, callback):
        out = (ctypes.c_uint32 * 512)()
        n = self.emit(out, at, base, self.ids[pc], callback)
        self.assertTrue(0 < n <= 1536)
        return bytes(out)[:n]

    def callback(self, u, pc, route, before):
        saved = u.reg_read(A.UC_ARM64_REG_X0)
        self.assertEqual(u.reg_read(A.UC_ARM64_REG_X1), self.ids[pc])
        self.assertEqual(saved, before[-4] - 800)
        self.assertEqual(list(struct.unpack('<32Q', u.mem_read(saved, 256))), before[:31] + [before[-4]])
        self.assertEqual([int.from_bytes(u.mem_read(saved + 256 + 16*i, 16), 'little') for i in range(32)], before[31:63])
        self.assertEqual(list(struct.unpack('<3Q', u.mem_read(saved + 768, 24))), before[-3:])
        ret = u.reg_read(A.UC_ARM64_REG_X30)
        # Only decisions consume the selector. Generic source observers leave the
        # padding unspecified. Neither callback writes saved x0 or LR.
        if pc in DECISIONS:
            self.assertEqual(bytes(u.mem_read(saved + 792, 8)), bytes(8))
            u.mem_write(saved + 792, struct.pack('<Q', route))
        for r in REGS:
            if r != A.UC_ARM64_REG_SP:
                u.reg_write(r, 0)
        u.reg_write(A.UC_ARM64_REG_PC, ret)

    def test_contract(self):
        self.require_sites()
        manifest = json.loads((L.ROOT / 'live/runtime-sites.json').read_text())
        canonical = {s['relative_pc']: s for s in manifest['sites']}
        for pc, (_, skip, word) in DECISIONS.items():
            self.assertEqual(self.rows[self.ids[pc]]['emitter'], 'camera-decision')
            self.assertEqual(canonical[pc]['word'], hex(word))
            self.assertEqual(canonical[pc]['continuation'], hex(pc + 4))
            self.assertEqual(canonical[pc]['selectors']['2'], hex(skip))
            bad = L.B / f'missing-decision-{pc:x}.json'
            changed = dict(manifest, sites=[s for s in manifest['sites'] if s['relative_pc'] != pc])
            bad.write_text(json.dumps(changed))
            with self.assertRaisesRegex(ValueError, 'site contract mismatch'):
                L.prepare.prepare(L.prepare.ELF, L.B / f'missing-{pc:x}', bad)
        self.assertEqual(len(self.ids), len(self.rows))
        # The later Release RET is still a source-result observer, not a decision.
        self.assertEqual(self.rows[self.ids[0x15fbd8]]['emitter'], 'observer')
        dual = self.dll.ft_loader_test_dual
        dual.argtypes = [ctypes.c_char_p]
        self.assertEqual(dual(str(L.prepare.CLIENT_ELF).encode()), 1)

    def test_missing_installed_decision(self):
        self.require_sites()
        # Bypass prepare's earlier manifest refusal to exercise real coverage on
        # deliberately incomplete compiled loader inventories, one site at a time.
        header = (L.B / 'loader-pins.h').read_text()
        for pc, (bit, _, _) in DECISIONS.items():
            out = L.B / f'coverage-without-{pc:x}'
            out.mkdir(exist_ok=True)
            lines = header.splitlines()
            removed = [line for line in lines if line.startswith('{' + hex(pc) + ',')]
            self.assertEqual(len(removed), 1)
            reduced = '\n'.join(line for line in lines if line not in removed)
            reduced = reduced.replace(f'#define SITE_COUNT {len(self.rows)}\n',
                                      f'#define SITE_COUNT {len(self.rows)-1}\n')
            (out / 'loader-pins.h').write_text(reduced + '\n')
            (out / 'loader-build.h').write_bytes((L.B / 'loader-build.h').read_bytes())
            objects = [L.B / f'emit-{i}.o' for i in range(4)] + [L.B / 'runtime-emit.o']
            mask = 0xfffff & ~(1 << bit)
            L.run(['cc', '-std=c11', '-O1', '-Wall', '-Wextra', '-Werror',
                   '-Wno-misleading-indentation', '-Wno-unused-function', '-Wno-unused-variable',
                   f'-DFT_TEST_CAMERA_MASK={mask}', '-I', out, L.ROOT / 'live/loader_test.c',
                   *objects, '-lcrypto', '-ldl', '-o', out / 'coverage-test'])
            L.run([out / 'coverage-test', L.prepare.ELF])

    def test_selected_routes(self):
        self.require_sites()
        for base in (0, 0x100000000, 0x7000000000):
            for pc, (_, skip, word) in DECISIONS.items():
                self.assertEqual(self.pin(pc, 4), struct.pack('<I', word))
                at, callback = base + 0x600000, base + 0x780000
                code = self.code(pc, at, base, callback)
                for route in (0, 2, 1, 3, 7):
                    actual, expected = machine(base), machine(base)
                    before = state(expected)
                    expected.mem_write(base + pc, self.pin(pc, 4))
                    if route == 0:
                        # Execute the pinned instruction, never a transcribed register formula.
                        expected.emu_start(base + pc, base + 0x7ffffc, count=1)
                        stop = expected.reg_read(A.UC_ARM64_REG_PC)
                    else:
                        stop = base + skip
                        expected.reg_write(A.UC_ARM64_REG_PC, stop)
                    actual.mem_write(at, code)
                    seen = []
                    def hook(u, address, size, _):
                        if address == callback:
                            self.callback(u, pc, route, before)
                            seen.append('callback')
                        elif address == stop:
                            seen.append('end')
                            u.emu_stop()
                    def trap(u, number, _):
                        seen.append('trap')
                        u.emu_stop()
                    handles = [actual.hook_add(UC_HOOK_CODE, hook), actual.hook_add(UC_HOOK_INTR, trap)]
                    actual.emu_start(at, base + 0x7ffffc, count=1000)
                    self.assertEqual(seen, ['callback', 'end' if route in (0, 2) else 'trap'])
                    if route in (0, 2):
                        self.assertEqual(state(actual), state(expected), (hex(pc), route))
                        self.assertEqual(actual.reg_read(A.UC_ARM64_REG_PC), stop)
                        for start, length in ((CAMERA, SP-800-CAMERA), (SP, 0x20000)):
                            self.assertEqual(actual.mem_read(start, length), expected.mem_read(start, length))
                    self.report['instruction_routes'].append([hex(base), hex(pc), route])
                    for handle in handles:
                        actual.hook_del(handle)
                    del actual, expected
                    gc.collect()

    def test_native_continuations(self):
        self.require_sites()
        base, callback, done, disconnect = 0x10000000, 0x10780000, 0x10790000, 0x107a0000
        # Full pinned methods after route selection, compared to an independent
        # unpatched execution with the same explicit branch decision.
        for name, entry, route, failure in (
            ('held-release', 0x15fba8, 2, False),
            ('held-clear', 0x15faf8, 2, False),
            ('final-release', 0x15fba8, 0, False),
            ('failed-disconnect', 0x15fba8, 0, True),
            ('ordinary-clear', 0x15faf8, 0, False)):
            outcomes = []
            snapshots = {}
            for patched in (False, True):
                u = machine(base)
                u.mem_write(base + 0x15f000, self.pin(0x15f000, 4096))
                u.mem_write(SHARED, bytes(0x4000))
                u.mem_write(SHARED + 0x54, b'\1')
                u.mem_write(CAMERA + 0x349, b'\7\1')
                u.mem_write(CAMERA + 0x360, b'\1')
                u.mem_write(CAMERA + 0x350, struct.pack('<QQ', 0xa001, 0xa002))
                u.mem_write(SERVICE, struct.pack('<Q', VTABLE))
                u.mem_write(VTABLE + 16, struct.pack('<Q', disconnect))
                u.reg_write(A.UC_ARM64_REG_X0, CAMERA)
                events, boundaries, service_calls, lock_calls = [], [], [], []
                observed = list(DECISIONS) + [0x15fbd8, 0x15fc20]
                starts = {}
                if patched:
                    for i, pc in enumerate(observed):
                        at = base + 0x600000 + i * 2048
                        u.mem_write(at, self.code(pc, at, base, callback))
                        u.mem_write(base + pc, struct.pack('<I', 0x14000000 | (((at-base-pc)//4) & 0x3ffffff)))
                def hook(m, address, size, _):
                    pc = address - base
                    if pc in observed:
                        if patched:
                            starts[pc] = state(m)
                            if pc in DECISIONS:
                                self.assertEqual(starts[pc], snapshots[pc])
                        else:
                            snapshots[pc] = state(m)
                            events.append(pc)
                            if pc in DECISIONS and route == 2:
                                m.reg_write(A.UC_ARM64_REG_PC, base + DECISIONS[pc][1])
                        return
                    if address == callback:
                        ident = m.reg_read(A.UC_ARM64_REG_X1)
                        event_pc = self.rows[ident]['pc']
                        self.callback(m, event_pc, route if event_pc in DECISIONS else 0, starts[event_pc])
                        events.append(event_pc)
                        return
                    if pc in (0x15fbbc, 0x15fbc8, 0x15fb24, 0x15fb74):
                        boundaries.append((pc, state(m)))
                    if address == done:
                        m.emu_stop()
                        return
                    x0 = m.reg_read(A.UC_ARM64_REG_X0)
                    if pc == 0x186a38:
                        lock_calls.append(('lock', x0)); result = 0
                    elif pc == 0x186ab0:
                        result = SHARED
                    elif pc == 0x186a40:
                        lock_calls.append(('unlock', x0)); result = 0
                    elif pc == 0x26a548:
                        result = SERVICE
                    elif pc == 0x174510:
                        result = 0
                    elif address == disconnect:
                        service_calls.append(m.reg_read(A.UC_ARM64_REG_X1)); result = int(failure)
                    else:
                        return
                    m.reg_write(A.UC_ARM64_REG_X0, result)
                    m.reg_write(A.UC_ARM64_REG_PC, m.reg_read(A.UC_ARM64_REG_X30))
                handle = u.hook_add(UC_HOOK_CODE, hook)
                u.emu_start(base + entry, base + 0x7ffffc, count=10000)
                self.assertEqual(u.reg_read(A.UC_ARM64_REG_PC), done)
                self.assertEqual(u.reg_read(A.UC_ARM64_REG_SP), SP)
                outcomes.append((state(u), events, boundaries, service_calls, lock_calls,
                                 bytes(u.mem_read(CAMERA, 0x4000)), bytes(u.mem_read(SHARED, 0x4000)),
                                 bytes(u.mem_read(SP, 0x20000))))
                u.hook_del(handle)
                del u
                gc.collect()
            self.assertEqual(outcomes[0], outcomes[1], name)
            registers, events, _, calls, locks, camera, shared, _ = outcomes[1]
            if name == 'held-release':
                self.assertEqual(registers[0], 1)  # supplied by native MOV w0,#1
                self.assertEqual(calls, [])
                self.assertEqual(locks, [])
                self.assertEqual(camera[0x349:0x34b], b'\0\1')
                self.assertEqual(camera[0x360], 1)
                self.assertEqual(shared[0x54], 1)
                self.assertEqual(events, [0x15fbb8, 0x15fbd8])
            elif name == 'held-clear':
                self.assertEqual(calls, [])
                self.assertEqual([v[0] for v in locks], ['lock', 'unlock'])
                self.assertEqual(locks[0][1], locks[1][1])
                self.assertEqual(shared[0x54], 1)
                self.assertEqual(camera[0x349:0x34b], b'\7\1')
            elif name in ('final-release', 'failed-disconnect'):
                self.assertEqual(registers[0], int(not failure))
                self.assertEqual(calls, [0xa001] if failure else [0xa001, 0xa002])
                self.assertEqual(camera[0x360], int(failure))
                self.assertEqual(shared[0x54], 0)
            else:
                self.assertEqual(shared[0x54], 0)
                self.assertEqual(calls, [])
            self.report['native_continuations'].append(dict(case=name, result=registers[0],
                service_calls=calls, lock_calls=locks, callbacks=[hex(pc) for pc in events]))

    @classmethod
    def tearDownClass(cls):
        (L.B / 'camera-decisions.json').write_text(json.dumps(cls.report, indent=2) + '\n')
        print('DECISION COUNTS', {k: len(v) for k, v in cls.report.items()})


if __name__ == '__main__':
    unittest.main(verbosity=2)
