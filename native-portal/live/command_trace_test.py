#!/usr/bin/env python3
# SPDX-License-Identifier: MIT
"""Assert real collector output, not a copied logger or invented live trace."""
import pathlib
import subprocess
import sys


def verify(binary):
    results = []
    for mode in ['--publication-pool-drop', '--publication-pool-live', '--repeated-cycles', '--command-trace-gap', '--command-trace-capacity']:
        result = subprocess.run([str(binary), mode], capture_output=True, text=True, timeout=5)
        assert result.returncode == 0, result.stderr
        rows = [dict(w.split('=', 1) for w in line.split()[1:])
                for line in result.stderr.splitlines() if line.startswith('ft-native-command ')]
        assert rows, 'missing bounded command transition trace'
        assert [int(r['seq']) for r in rows] == list(range(1, len(rows)+1))
        if mode == '--command-trace-capacity':
            summaries=[dict(w.split('=',1) for w in line.split()[1:])
                       for line in result.stderr.splitlines() if line.startswith('ft-native-command-summary ')]
            summary=summaries[-1];capacity=int(summary['capacity'])
            assert len(rows)==capacity and int(summary['consumed'])==capacity
            assert int(summary['attempted'])-capacity==int(summary['overflow'])>0
            assert summary['sink_lost']=='0'
            assert rows[-1]['route']=='handoff-wait' and rows[-1]['pc']=='0x300a38'
            print('command trace: real collector full bounded capacity, later records and explicit overflow PASS')
        elif mode == '--publication-pool-drop':
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
        elif mode == '--publication-pool-live':
            pub = next(r for r in rows if r['pc'] == '0x2a23f4')
            cycle = [r for r in rows if r['command'] == pub['command'] and int(r['seq']) <= int(pub['seq'])]
            assert [r['pc'] for r in cycle] == [
                '0x301808', '0x300a08', '0x300a38', '0x30180c', '0x311648',
                '0x300b20', '0x300c54', '0x300a08', '0x300a38', '0x311648',
                '0x300b20', '0x300c54', '0x300a08', '0x300a38', '0x2a23f4']
            added = cycle[3]
            for offset in [4, 9]:
                returned, reset, reset_result, begin, begin_result = cycle[offset:offset+5]
                assert returned['lr'] == '0x297cb4'
                assert returned['before_member'] == returned['after_member'] == '1'
                assert returned['begun'] == '1,0' and returned['generation'] == '0,0'
                assert returned['entries'].split(',')[0] == returned['entries'].split(',')[1]
                assert returned['before_vk'] == returned['after_vk'] == added['after_vk']
                assert reset['lr'] == begin['lr'] == '0x2a9cdc'
                assert reset_result['x0'] == begin_result['x0'] == '0x0'
                assert [r['renewal'] for r in [reset,reset_result,begin,begin_result]] == ['0,1','1,2','2,3','3,0']
                assert [r['begun'] for r in [reset,reset_result,begin,begin_result]] == ['0,0','0,0','0,0','0,1']
                assert all(r['generation'] == '0,0' for r in [reset,reset_result,begin,begin_result])
            assert cycle[4]['tid'] == added['tid'] != cycle[5]['tid'] == cycle[9]['tid'] == pub['tid']
            assert all(r['route'] == 'owner' and r['tid'] == r['owner_tid'] and r['failure'] == '0x0' for r in cycle)
            assert pub['before_member'] == pub['after_member'] == '1' and pub['accepted'] == '1'
            assert pub['generation'].split(',')[0] == '0' and int(pub['generation'].split(',')[1]) > 0
            assert pub['native_vk'] == pub['after_vk'] == added['after_vk'] == '0xc001'
            assert 'second stereo queue PASS' in result.stdout
            print('command trace: trial-11 same-C bootstrap return, real thread handoff, two reset/Begin cycles, publication and owned settlement PASS')
        elif mode == '--repeated-cycles':
            pub = next(r for r in rows if r['pc'] == '0x2a23f4')
            cycle = [r for r in rows if r['command'] == pub['command']]
            returns = [r for r in cycle if r['pc'] == '0x311648' and r['lr'] == '0x297cb4']
            pubs = [r for r in cycle if r['pc'] == '0x2a23f4']
            assert len(returns) == len(pubs) == 1 + 12 + 8
            for returned in returns:
                assert returned['before_member'] == returned['after_member'] == '1'
                assert returned['before_vk'] == returned['after_vk'] == '0xc001'
                assert returned['begun'] == '1,0' and returned['generation'] == '0,0'
            for published in pubs:
                assert published['generation'].split(',')[0] == '0'
                assert int(published['generation'].split(',')[1]) > 0
                assert published['accepted'] == '1'
            owned_begins = [r for r in cycle if r['pc'] == '0x300a38' and r['lr'] == '0x301000']
            assert len(owned_begins) == 8
            for begun in owned_begins:
                assert begun['renewal'] == '3,0' and begun['begun'] == '0,1'
                assert int(begun['generation'].split(',')[1]) > 0
                assert int(begun['flags'].split(',')[0], 16) & 64  # actual Lifecycle attached
            assert all(r['failure'] == '0x0' for r in rows)
            summaries = [dict(w.split('=', 1) for w in line.split()[1:])
                         for line in result.stderr.splitlines() if line.startswith('ft-native-command-summary ')]
            assert summaries and int(summaries[-1]['consumed']) == len(rows)
            assert summaries[-1]['attempted'] == summaries[-1]['consumed']
            assert all(summaries[-1][k] == '0' for k in ['overflow', 'identity_overflow', 'sink_lost'])
            assert '8 complete owned queue/wait/disposal/reset/Release cycles' in result.stdout
            print('command trace: repeated published stock reuse and complete interleaved Lifecycle settlement PASS')
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
