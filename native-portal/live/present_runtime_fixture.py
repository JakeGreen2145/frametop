# SPDX-License-Identifier: MIT
"""Source-only first-consumer facts and result checks. Never opens a native ELF."""
import hashlib
import json
from pathlib import Path
import struct

MODES = ['consumer-flush', 'consumer-present', 'consumer-unknown', 'consumer-coldpass']
PINS = Path('/home/jakeg/.hermes/work/desktop-compositor-re/proactive-f9adb38/lane-c-b1ec331db554/verified-payloads/NATIVE-PINS.json')
PIN_SHA = 'dbb7ae08e6b66b52b4211e2ba735bdaebb2aa196ab782ccfd20bb09ebe523f46'


def generate(build, path=PINS):
    data = path.read_bytes()
    assert hashlib.sha256(data).hexdigest() == PIN_SHA
    pins = json.loads(data)
    assert pins['sha256'] == '9b1bd5fe4463592cbc71b2517b9ceb0a0fcd66cd6885a19dca4be0784bf6c265'
    derived = {}
    mode = None
    for name, start in [('present', 0x295594), ('flush', 0x6cadc)]:
        window = pins['windows'][hex(start)]
        raw = bytes.fromhex(window['bytes'])
        assert hashlib.sha256(raw).hexdigest() == window['sha256']
        words = struct.unpack('<6I', raw)
        # Decode only retained caller argument/load/BLR words, no disassembler,
        # emulation, external native analysis, or copied production guard list.
        assert words[4] & 0xfffffc1f == 0xd63f0000  # BLR Xn
        target = (words[4] >> 5) & 31
        load = words[3]
        assert load & 0xffc00000 == 0xf9400000 and load & 31 == target
        assert (load >> 5) & 31 == target
        slot = ((load >> 10) & 4095) * 8
        assert slot == 0x250
        if name == 'present':
            assert words[0] == 0xf9400282  # LDR X2,[X20]
            assert words[1] == 0xaa1403e0  # MOV X0,X20
            assert words[2] & 0xffe0001f == 0x52800001  # MOVZ W1,imm
            mode = (words[2] >> 5) & 65535
            assert mode == 0
        else:
            assert words[0] == 0x2a1403e1  # MOV W1,W20, fixture chooses mode0
            assert words[1] == 0xf96b5260  # LDR X0,[X19,#0x56a0]
            assert words[2] == 0xf9400002  # LDR X2,[X0]
        derived[name + '_lr'] = start + 4 * 5
    assert mode is not None
    derived.update(mode=mode, slot=slot)
    header = '// Generated from hash-verified lane-C windows, modeled relocation only.\n'
    header += ''.join(f'static constexpr uint64_t consumer_{key}={hex(value)};\n' for key, value in derived.items())
    (build / 'consumer.private.hpp').write_text(header)
    (build / 'consumer-native-facts.json').write_text(json.dumps(dict(
        pin_sha256=PIN_SHA, native_sha256=pins['sha256'], derived=derived,
        native_execution=False, known_lanes=['x0=modeled renderer', 'w1=0', 'x2=modeled slot250 target', 'x30=derived LR'],
        modeled=['objects', 'SP', 'unobserved registers', 'dispatch bodies', 'whole native lifetime exclusion']), indent=2) + '\n')
    return {'consumer.private.hpp': hashlib.sha256(header.encode()).hexdigest(), 'lane-c/NATIVE-PINS.json': PIN_SHA}


def assess(mode, code, text, fatal):
    snapshots = [dict(word.split('=', 1) for word in line.split()[1:])
                 for line in text.splitlines() if line.startswith('CONSUMER mode=')]
    required = ['recorded', 'dispatch', 'packets', 'off', 'queue', 'wait', 'transfer', 'disposal', 'reset', 'joined']
    expected_runs = 2 if code == 0 else 1
    complete_tail = ([row['phase'] for row in snapshots] == required * expected_runs)
    groups = [{row['phase']: row for row in snapshots[start:start + len(required)]}
              for start in range(0, len(snapshots), len(required))]
    negative = mode in ('consumer-present', 'consumer-unknown')
    fatal_ok = (code == -6 and fatal.get('event') == '0x000000000008b020'
                and fatal.get('reason') == '0x0000000000000005' and fatal.get('joined') == '1'
                and 'native_cleanup=0' in text and 'CURRENT forbidden native cleanup' not in text
                and 'CURRENT unexpected cleanup return' not in text)
    safe_tail = fatal_ok if negative else code == 0 and text.count('clean-teardown=1 native_cleanup=') == 2
    expected = {
        'consumer-flush': dict(graph='1', pass_live='1', pass_reason='0', first_evidence='0x0', committed='1', rejected='0', lookups='2'),
        'consumer-present': dict(graph='0', pass_live='0', pass_reason='2', first_evidence='0x0', committed='0', rejected='0', lookups='0'),
        'consumer-unknown': dict(graph='0', pass_live='0', pass_reason='2', first_evidence='0x2a1e18', committed='0', rejected='0', lookups='0'),
        'consumer-coldpass': dict(graph='1', pass_live='1', pass_reason='11', first_evidence='0x0', committed='0', rejected='0', lookups='0'),
    }[mode]
    def matches(row, values):
        return all(row.get(key) == value for key, value in values.items())

    def diagnostic_group(phases):
        dispatch, packets, reset = (phases.get(name, {}) for name in ('dispatch', 'packets', 'reset'))
        return (matches(dispatch, expected) and matches(dispatch, dict(uploads='1,1', tickets='2', first_stock='0x0'))
                and matches(packets, dict(skips='0' if mode == 'consumer-flush' else '3',
                                          rejected='1' if mode == 'consumer-coldpass' else '0'))
                and matches(phases.get('off', {}), dict(pending='1', camera_debt='1'))
                and matches(reset, dict(pending=str(int(negative)), camera_debt=str(int(negative)),
                                        source_sealed=str(int(not negative)), gpu_complete=str(int(not negative)),
                                        cpu_settled=str(int(not negative)), queue_observed=str(int(not negative)),
                                        queue=str(int(not negative)), submitted_frame='0',
                                        encoded='1,1' if mode == 'consumer-flush' else '0,0',
                                        stereo_submitted='1' if mode == 'consumer-flush' else '0')))

    diagnostic = (complete_tail and safe_tail and bool(groups) and all(map(diagnostic_group, groups))
                  and 'Assertion' not in text)

    def repaired_group(phases):
        return (matches(phases.get('dispatch', {}), dict(graph='1', pass_live='1', pass_reason='0',
                        first_evidence='0x0', first_stock='0x0', committed='1', lookups='2'))
                and matches(phases.get('packets', {}), dict(encoded='1,1', skips='0'))
                and matches(phases.get('reset', {}), dict(pending='0', camera_debt='0', source_sealed='1',
                        gpu_complete='1', cpu_settled='1', queue='1', stereo_submitted='1')))

    # Off precedes queue, as in the original fixture: no new lease-visible frame
    # may be published, even though Command/Adapter certify the completed stereo.
    functional = diagnostic if mode != 'consumer-present' else (
        complete_tail and code == 0 and text.count('clean-teardown=1 native_cleanup=') == 2
        and bool(groups) and all(map(repaired_group, groups)) and 'Assertion' not in text)
    return dict(diagnostic_pass=diagnostic, functional_pass=functional, complete_tail=complete_tail,
                exact_cleanup_refusal=fatal_ok, snapshots=snapshots)
