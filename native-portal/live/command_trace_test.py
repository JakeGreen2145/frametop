#!/usr/bin/env python3
# SPDX-License-Identifier: MIT
"""Assert real collector output, not a copied logger or invented live trace."""
import pathlib
import subprocess
import sys


def verify(binary):
    results = []
    for mode in ['--publication-pool-drop', '--command-trace-gap']:
        result = subprocess.run([str(binary), mode], capture_output=True, text=True, timeout=5)
        assert result.returncode == 0, result.stderr
        rows = [dict(w.split('=', 1) for w in line.split()[1:])
                for line in result.stderr.splitlines() if line.startswith('ft-native-command ')]
        assert rows, 'missing bounded command transition trace'
        assert [int(r['seq']) for r in rows] == list(range(1, len(rows)+1))
        if mode == '--publication-pool-drop':
            pub = next(r for r in rows if r['pc'] == '0x2a23f4')
            command = pub['command']
            cycle = [r for r in rows if r['command'] == command]
            pcs = [r['pc'] for r in cycle]
            for pc in ['0x301808', '0x30180c', '0x311648', '0x300b20', '0x300c54', '0x2a23f4']:
                assert pc in pcs, (pc, cycle)
            added = next(r for r in cycle if r['pc'] == '0x30180c')
            dropped = next(r for r in cycle if r['pc'] == '0x311648')
            assert added['before_member'] == '0' and added['after_member'] == '1'
            assert dropped['before_member'] == '1' and dropped['after_member'] == '0'
            assert dropped['lr'] == '0x3052f0'
            assert pub['before_member'] == '0' and pub['failure'] == '0x2a23f4'
            assert pub['native_vk'] == added['after_vk'] == '0xc001'
            assert added['tid'] != pub['tid'] and pub['tid'] == pub['owner_tid']
            assert all(r['route'] == 'owner' for r in cycle)
            for r in cycle:
                if r['pc'] in ['0x300b20', '0x300c54']:
                    assert r['before_member'] == r['after_member'] == '0'
            print('command trace: exact create/add, pool removal, ignored reset/Begin, same-C publication refusal PASS')
        else:
            for route in ['handoff-wait', 'foreign']:
                skipped = [r for r in rows if r['route'] == route]
                assert [r['pc'] for r in skipped] == ['0x301808', '0x300a08', '0x300a38', '0x30180c']
                assert all(r['ledger_sampled'] == '0' for r in skipped)
            print('command trace: bootstrap wait and foreign callback gaps recorded without ledger reads PASS')
        results.append({'mode': mode, 'code': result.returncode, 'output': result.stdout + result.stderr})
    return results


if __name__ == '__main__':
    verify(pathlib.Path(sys.argv[1]) if len(sys.argv) > 1 else pathlib.Path(__file__).parent/'build/runtime/runtime_chain_test')
