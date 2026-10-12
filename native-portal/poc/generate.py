#!/usr/bin/env python3
"""Generate a camera aperture driven by 64-byte vertex push constants.

Private diagnostic assets only. This does not install or activate the shaders.
"""
import argparse
import hashlib
import json
import math
from pathlib import Path
import re
import subprocess
import sys

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / 'tools'))
import generate_shaders as original


def patch(text, stage, raw_position, center, u, v, fixed=False):
    # Reuse the tested varying, clipping and native-instruction preservation.
    # Replace only the newly added extension loads with shader calculations.
    extended = original.patch_assembly(text, stage, raw_position)
    a = original.Assembly(extended)
    lines = a.lines
    definitions = {s.split()[0]: s.split(' = ', 1)[1] for s in lines if ' = ' in s}
    binding = 0 if stage == 'vertex' else 4
    ubo = next(s.split()[1] for s in lines if re.fullmatch(rf'OpDecorate %\d+ Binding {binding}', s))
    block = definitions[definitions[ubo].split()[1]].split()[2]
    members = definitions[block].split()[1:]
    f32 = a.declaration('OpTypeFloat 32')
    u32 = a.declaration('OpTypeInt 32 0')
    v3 = a.declaration(f'OpTypeVector {f32} 3')
    v4 = a.declaration(f'OpTypeVector {f32} 4')
    one = a.declaration(f'OpConstant {f32} 1')
    uone = a.declaration(f'OpConstant {u32} 1')
    zero = a.declaration(f'OpConstant {f32} 0')
    replacements = {3: one, 6: uone}
    code = []
    push_block = None

    def emit(rhs):
        ident = a.fresh()
        code.append(f'{ident} = {rhs}')
        return ident

    def constant_vec(values):
        ids = [a.declaration(f'OpConstant {f32} {float(value):.9g}') for value in values]
        return a.declaration(f'OpConstantComposite {v3} {" ".join(ids)}')

    if stage == 'vertex':
        # Vulkan handles RowMajor storage when OpLoad materializes the matrix.
        # M1 is reference-to-eye. Its inverse sends the eye origin to reference
        # coordinates. Mapping this reference to OpenVR raw/standing needs a
        # live matrix observation and is not claimed by this generator.
        matrix = members[1]
        if definitions[matrix] != f'OpTypeMatrix {v4} 4':
            raise ValueError('M1 is not a float4x4')
        for decoration in (f'OpMemberDecorate {block} 1 Offset 64',
                           f'OpMemberDecorate {block} 1 RowMajor',
                           f'OpMemberDecorate {block} 1 MatrixStride 16'):
            if decoration not in lines:
                raise ValueError('unexpected native M1 layout: ' + decoration)
        glsl = next(s.split()[0] for s in lines if '= OpExtInstImport "GLSL.std.450"' in s)
        matrix_ptr = a.declaration(f'OpTypePointer Uniform {matrix}')
        index = a.declaration(f'OpConstant {u32} 1')
        ptr = emit(f'OpAccessChain {matrix_ptr} {ubo} {index}')
        m = emit(f'OpLoad {matrix} {ptr}')
        inv = emit(f'OpExtInst {matrix} {glsl} MatrixInverse {m}')
        origin = a.declaration(f'OpConstantComposite {v4} {zero} {zero} {zero} {one}')
        e4 = emit(f'OpMatrixTimesVector {v4} {inv} {origin}')
        e3 = emit(f'OpVectorShuffle {v3} {e4} {e4} 0 1 2')
        ew = emit(f'OpCompositeExtract {f32} {e4} 3')
        ew_inverse = emit(f'OpFDiv {f32} {one} {ew}')
        eye = emit(f'OpVectorTimesScalar {v3} {e3} {ew_inverse}')
        active = one
        if fixed:
            c, uu, vv = map(constant_vec, (center, u, v))
        else:
            push_block = a.declaration(f'OpTypeStruct {v4} {v4} {v4} {v4}')
            push_ptr = a.declaration(f'OpTypePointer PushConstant {push_block}')
            push = a.declaration(f'OpVariable {push_ptr} PushConstant')
            vector_ptr = a.declaration(f'OpTypePointer PushConstant {v4}')
            annotations = [f'OpDecorate {push_block} Block']
            annotations += [f'OpMemberDecorate {push_block} {i} Offset {16*i}' for i in range(4)]
            first_type = next(i for i, line in enumerate(lines) if ' = OpType' in line)
            lines[first_type:first_type] = annotations
            vectors = []
            for i in range(4):
                member = a.declaration(f'OpConstant {u32} {i}')
                address = emit(f'OpAccessChain {vector_ptr} {push} {member}')
                vectors.append(emit(f'OpLoad {v4} {address}'))
            c, uu, vv = [emit(f'OpVectorShuffle {v3} {x} {x} 0 1 2') for x in vectors[:3]]
            active = emit(f'OpCompositeExtract {f32} {vectors[3]} 0')
        w = emit(f'OpFSub {v3} {c} {eye}')
        n = emit(f'OpExtInst {v3} {glsl} Cross {uu} {vv}')
        det = emit(f'OpDot {f32} {n} {w}')
        reciprocal = emit(f'OpFDiv {f32} {one} {det}')
        numerators = [emit(f'OpExtInst {v3} {glsl} Cross {vv} {w}'),
                      emit(f'OpExtInst {v3} {glsl} Cross {w} {uu}'), n]
        for i, numerator in enumerate(numerators):
            row = emit(f'OpVectorTimesScalar {v3} {numerator} {reciprocal}')
            dot = emit(f'OpDot {f32} {row} {eye}')
            offset = emit(f'OpFNegate {f32} {dot}')
            xyz = [emit(f'OpCompositeExtract {f32} {row} {j}') for j in range(3)]
            coefficient = emit(f'OpCompositeConstruct {v4} {" ".join(xyz)} {offset}')
            replacements[i] = emit(f'OpVectorTimesScalar {v4} {coefficient} {active}')

    # The original patch adds members 26..32. Eliminate their accesses and
    # declarations, leaving the exact original descriptor and buffer layout.
    constants = {key: int(rhs.split()[-1]) for key, rhs in definitions.items()
                 if rhs.startswith(f'OpConstant {u32} ') and rhs.split()[-1].isdigit()}
    accesses = {}
    for line in lines:
        parts = line.split()
        if len(parts) == 6 and parts[2] == 'OpAccessChain' and parts[4] == ubo:
            index = constants.get(parts[5], -1)
            if 26 <= index <= 32:
                accesses[parts[0]] = index - 26
    aliases = {}
    remove = set(accesses)
    insertion = min(i for i, line in enumerate(lines) if line.split()[0] in accesses)
    for line in lines:
        parts = line.split()
        if len(parts) == 5 and parts[2] == 'OpLoad' and parts[4] in accesses:
            aliases[parts[0]] = replacements[accesses[parts[4]]]
            remove.add(parts[0])
    output = []
    for i, line in enumerate(lines):
        if i == insertion:
            output.extend(code)
        if line.split()[0] in remove:
            continue
        if line.startswith(f'OpMemberDecorate {block} ') and int(line.split()[2]) >= 26:
            continue
        if line.startswith(f'{block} = OpTypeStruct '):
            line = f'{block} = OpTypeStruct ' + ' '.join(members[:26])
        output.append(re.sub(r'%\d+', lambda m: aliases.get(m[0], m[0]), line))
    first_function = next(i for i, line in enumerate(output) if ' = OpFunction ' in line)
    output[first_function:first_function] = a.declarations
    result = '\n'.join(output) + '\n'
    # The existing checker allows no new structs. Exclude exactly our new push
    # block declaration from that check; every original instruction must remain.
    preservation = '\n'.join(line for line in output
                             if push_block is None or not line.startswith(push_block + ' = OpTypeStruct '))
    original.verify_preservation(text, preservation)
    return result


def main():
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument('--source', type=Path, default=Path('/opt/steamvr/resources/shaders/vulkan'))
    p.add_argument('--output', type=Path, required=True)
    p.add_argument('--center', type=float, nargs=3, default=[0, 0, -1])
    p.add_argument('--u', type=float, nargs=3, default=[0.45, 0, 0])
    p.add_argument('--v', type=float, nargs=3, default=[0, 0.3, 0])
    p.add_argument('--fixed', action='store_true', help='retain the original baked rectangle diagnostic')
    args = p.parse_args()
    if not all(math.isfinite(value) for values in (args.center, args.u, args.v) for value in values):
        p.error('aperture coordinates must be finite')
    cross = [args.u[1]*args.v[2]-args.u[2]*args.v[1],
             args.u[2]*args.v[0]-args.u[0]*args.v[2],
             args.u[0]*args.v[1]-args.u[1]*args.v[0]]
    if sum(value*value for value in cross) < 1e-12:
        p.error('aperture half-extent vectors must span a plane')
    args.output.mkdir(parents=True, exist_ok=True)
    report = {'geometry_mode': 'fixed' if args.fixed else 'frametop-push-constants',
              'center_reference': args.center, 'u_reference': args.u, 'v_reference': args.v, 'shaders': {}}
    for ident, info in original.MANIFEST['shaders'].items():
        source = args.source / info['filename']
        if hashlib.sha256(source.read_bytes()).hexdigest() != info['sha256']:
            raise ValueError('native shader hash mismatch: ' + str(source))
        assembly = original.run('spirv-dis', '--raw-id', source)
        modified = patch(assembly, info['stage'], info['raw_position_id'], args.center, args.u, args.v, args.fixed)
        private_asm = args.output / (ident + '.poc.spvasm')
        target = args.output / (ident + '.portal.spv')
        (args.output / (ident + '.original.spv')).write_bytes(source.read_bytes())
        private_asm.write_text(modified)
        subprocess.run(['spirv-as', '--target-env', 'vulkan1.0', str(private_asm), '-o', str(target)], check=True)
        subprocess.run(['spirv-val', '--target-env', 'vulkan1.0', str(target)], check=True)
        report['shaders'][ident] = {'file': target.name, 'sha256': hashlib.sha256(target.read_bytes()).hexdigest()}
    (args.output / 'poc-shaders.json').write_text(json.dumps(report, indent=2) + '\n')
    print(json.dumps(report, indent=2))


if __name__ == '__main__':
    main()
