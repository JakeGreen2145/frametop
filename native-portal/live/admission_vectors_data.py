# SPDX-License-Identifier: MIT
"""Frozen producer Saved data only. Never import an analysis or loader runner."""
import hashlib
import json
from pathlib import Path
import struct

PIN = 'bca7a1da598b555baec2dd8790aba8a900afaf839849850f93019b5448f06b39'


def header(path: Path) -> str:
    raw = path.read_bytes()
    if hashlib.sha256(raw).hexdigest() != PIN:
        raise ValueError('admission Saved vectors changed')
    data = json.loads(raw)
    assert len(data['vectors']) == 29 and data['instruction_checks'] == 195
    rows = []
    for vector in data['vectors']:
        for event in vector['events']:
            saved = bytes.fromhex(event['saved'])
            assert len(saved) == 800
            words = struct.unpack('<100Q', saved)
            rows.append('{' + json.dumps(vector['name']) + ',' + event['pc'] + ',{' +
                        ','.join(hex(word) + 'ULL' for word in words) + '}}')
    assert len(rows) == 63
    return ('// Frozen producer full Saved records; not newly executed native code.\n'
            'struct AdmissionVector {const char* name;uint64_t pc;uint64_t words[100];};\n'
            'static constexpr AdmissionVector admission_vectors[]={\n' + ',\n'.join(rows) + '\n};\n')
