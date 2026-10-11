# SPDX-License-Identifier: MIT
"""Read Desktop argument receipts as pinned data, never execute its verifier."""
import hashlib
import json

PIN = 'b16751dfc0e5a0fa72ef81213fdc8fe36ab4f0c048de97e828b3b5348886b8c3'
ROOTS = (0x297878, 0x297b70, 0x29a464, 0x29af28, 0x29ca68, 0x305330)


def header(path):
    raw = path.read_bytes()
    assert hashlib.sha256(raw).hexdigest() == PIN, 'Desktop argument vector pin changed'
    data = json.loads(raw)
    assert data['schema'] == 1 and data['root_count'] == 6
    assert data['Saved_vector_count'] == len(data['vectors']) == 12
    assert data['ancestry_case_count'] == len(data['initializer_ancestry']) == 2
    assert data['binary_sha256_in_retained_scan'] == '9b1bd5fe4463592cbc71b2517b9ceb0a0fcd66cd6885a19dca4be0784bf6c265'
    number = lambda x: int(x, 0)
    records = {}
    for row in data['vectors']:
        case, lr = row['case'], number(row['root']['lr'])
        assert case in (0, 1) and lr in ROOTS
        key = (case, ROOTS.index(lr))
        assert key not in records
        assert number(row['Saved']['pc']) == number(row['fixture']['module_base']) + 0x302790
        assert number(row['Saved']['x'][30]) == number(row['fixture']['module_base']) + lr
        records[key] = row
    assert set(records) == {(c, r) for c in range(2) for r in range(6)}

    def record(saved, fixture, known):
        assert len(saved['x']) == 31
        fields = [*map(number, saved['x']), number(saved['sp']),
                  *[number(fixture[k]) for k in ('module_base', 'renderer_R', 'pool_P', 'wrapper_W', 'owner_A')]]
        return '{{' + ','.join(hex(x) + 'ULL' for x in fields[:31]) + '},' + ','.join(hex(x) + 'ULL' for x in fields[31:]) + ',' + hex(sum(1 << i for i in known)) + 'U}'

    lines = ['// Generated from hash-pinned Desktop data. No native execution on this host.',
             'struct FactorySaved { uint64_t x[31],sp,base,r,p,w,a; uint32_t known; };',
             'static constexpr FactorySaved factory_saved_vectors[2][6]={']
    for case in range(2):
        lines.append('{')
        for root in range(6):
            row = records[case, root]
            statuses = row['Saved_register_status']
            assert set(statuses) == {'x' + str(i) for i in range(31)}
            known = [i for i in range(31) if statuses['x' + str(i)].startswith('native producer')]
            assert {0, 1, 8, 30}.issubset(known)
            lines.append(record(row['Saved'], row['fixture'], known) + ',')
        lines.append('},')
    lines.append('};')
    # Only these ancestry lanes are consumed as explicit producer/caller facts.
    # Other displayed lanes remain copied model values, never identity evidence.
    ancestry = [('initial_factory_Saved', (0, 1, 8, 19, 24, 30)),
                ('constructor_entry_Saved', (0, 1, 2, 8, 19, 20, 21, 22, 24, 30)),
                ('nested_acquire_Saved', (0, 1, 2, 3, 8, 19, 20, 21, 22, 24, 29, 30)),
                ('pre_29a528_Saved', (0, 1, 2, 3, 8, 19, 20, 21, 22, 24, 29, 30))]
    lines.append('static constexpr FactorySaved factory_ancestry_vectors[2][4]={')
    for case, row in enumerate(data['initializer_ancestry']):
        assert row['case'] == case
        fixture = records[case, 2]['fixture']
        lines.append('{')
        for name, known in ancestry:
            lines.append(record(row[name], fixture, known) + ',')
        lines.append('},')
    lines.append('};')
    return '\n'.join(lines) + '\n'
