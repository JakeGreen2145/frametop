#!/usr/bin/env python3
# SPDX-License-Identifier: MIT
"""Offline, hash-pinned portal variants. Never loads or replaces runtime modules."""
import argparse
import hashlib
import json
from pathlib import Path
import re
import subprocess

MANIFEST = json.loads(Path(__file__).with_name('native-shaders.json').read_text())
OFFSETS = (528, 544, 560, 576, 580, 584, 588)


def run(*args):
    return subprocess.check_output([str(a) for a in args], text=True, stderr=subprocess.STDOUT)


def instructions(text):
    return [line.strip() for line in text.splitlines() if line.strip() and not line.lstrip().startswith(';')]


class Assembly:
    def __init__(self, text):
        self.lines = instructions(text)
        self.next_id = max(map(int, re.findall(r'%(\d+)', text))) + 1
        self.declarations = []

    def fresh(self):
        result = f'%{self.next_id}'
        self.next_id += 1
        return result

    def declaration(self, rhs):
        for line in self.lines + self.declarations:
            if line.endswith(' = ' + rhs):
                return line.split()[0]
        ident = self.fresh()
        self.declarations.append(f'{ident} = {rhs}')
        return ident


def patch_assembly(text, stage, raw_position=None):
    """Shared implementation for pinned native inputs and owned synthetic fixtures.

    Only generate() accepts production native files and enforces their pins.
    This assembly transform deliberately has no runtime integration API.
    """
    if stage not in ('vertex', 'fragment'):
        raise ValueError('unsupported stage')
    a = Assembly(text)
    lines = a.lines
    entry = [s for s in lines if s.startswith('OpEntryPoint ')]
    if len(entry) != 1 or (' Vertex ' if stage == 'vertex' else ' Fragment ') not in entry[0]:
        raise ValueError('expected one matching entrypoint')
    function = entry[0].split()[2]
    if any('EarlyFragmentTests' in s or ' PushConstant' in s for s in lines):
        raise ValueError('unsupported execution mode or push constants')
    binding = 0 if stage == 'vertex' else 4
    buffers = [s.split()[1] for s in lines if re.fullmatch(rf'OpDecorate %\d+ Binding {binding}', s)]
    if len(buffers) != 1:
        raise ValueError('expected unique camera binding')
    ubo = buffers[0]
    if f'OpDecorate {ubo} DescriptorSet 0' not in lines:
        raise ValueError('expected descriptor set zero')
    definitions = {s.split()[0]: s.split(' = ', 1)[1] for s in lines if ' = ' in s}
    ptr = definitions[ubo].split()[1]
    if not definitions[ptr].startswith('OpTypePointer Uniform '):
        raise ValueError('expected uniform block')
    block = definitions[ptr].split()[2]
    members = definitions[block].split()[1:]
    if len(members) != 26 or f'OpMemberDecorate {block} 25 Offset 464' not in lines:
        raise ValueError('expected native 528-byte block')
    if f'OpDecorate {block} Block' not in lines:
        raise ValueError('missing Block decoration')
    storage = 'Output' if stage == 'vertex' else 'Input'
    for line in lines:
        if line.endswith(' Location 3'):
            var = line.split()[1]
            if definitions[var].endswith(' ' + storage):
                raise ValueError('aperture location already occupied')
    f32 = a.declaration('OpTypeFloat 32')
    u32 = a.declaration('OpTypeInt 32 0')
    v3 = a.declaration(f'OpTypeVector {f32} 3')
    v4 = a.declaration(f'OpTypeVector {f32} 4')
    boolean = a.declaration('OpTypeBool')
    b3 = a.declaration(f'OpTypeVector {boolean} 3')
    uf = a.declaration(f'OpTypePointer Uniform {f32}')
    uv = a.declaration(f'OpTypePointer Uniform {v4}')
    ui = a.declaration(f'OpTypePointer Uniform {u32}')
    varying_ptr = a.declaration(f'OpTypePointer {storage} {v3}')
    varying = a.fresh()
    a.declarations.append(f'{varying} = OpVariable {varying_ptr} {storage}')
    zero = a.declaration(f'OpConstant {f32} 0')
    one = a.declaration(f'OpConstant {f32} 1')
    uzero = a.declaration(f'OpConstant {u32} 0')
    uone = a.declaration(f'OpConstant {u32} 1')
    indices = [a.declaration(f'OpConstant {u32} {i}') for i in range(26, 33)]
    old_struct = f'{block} = {definitions[block]}'
    lines[lines.index(old_struct)] += f' {v4} {v4} {v4} {f32} {u32} {u32} {u32}'
    lines[lines.index(entry[0])] += f' {varying}'
    decorations = [f'OpDecorate {varying} Location 3']
    decorations += [f'OpMemberDecorate {block} {26+i} Offset {offset}' for i, offset in enumerate(OFFSETS)]
    first_type = next(i for i, s in enumerate(lines) if ' = OpType' in s)
    lines[first_type:first_type] = decorations
    code = []

    def emit(rhs):
        ident = a.fresh()
        code.append(f'{ident} = {rhs}')
        return ident

    def load_member(index, typ, ptr_type):
        ptr_id = emit(f'OpAccessChain {ptr_type} {ubo} {indices[index]}')
        return emit(f'OpLoad {typ} {ptr_id}')

    if stage == 'vertex':
        raw = f'%{raw_position}'
        if raw not in definitions or definitions[raw].split()[1] != v4:
            raise ValueError('raw position must be an existing vec4 result')
        coords = [emit(f'OpCompositeExtract {f32} {raw} {i}') for i in range(3)]
        homogeneous = emit(f'OpCompositeConstruct {v4} {" ".join(coords)} {one}')
        dots = [emit(f'OpDot {f32} {load_member(i, v4, uv)} {homogeneous}') for i in range(3)]
        q = emit(f'OpCompositeConstruct {v3} {" ".join(dots)}')
        code.append(f'OpStore {varying} {q}')
        pos = next(i for i, s in enumerate(lines) if s.startswith(raw+' = '))
        lines[pos+1:pos+1] = code
    else:
        entry_function = next(i for i, s in enumerate(lines) if s.startswith(function+' = OpFunction '))
        first_label = lines[entry_function+1]
        if not first_label.endswith(' = OpLabel'):
            raise ValueError('unexpected function parameters')
        if 'OpVariable' in lines[entry_function+2]:
            raise ValueError('entry local variables require a separately supported relocation')
        label = first_label.split()[0]
        code.append(f'{a.fresh()} = OpLabel')
        q = emit(f'OpLoad {v3} {varying}')
        nan = emit(f'OpIsNan {b3} {q}')
        inf = emit(f'OpIsInf {b3} {q}')
        invalid_components = emit(f'OpLogicalOr {b3} {nan} {inf}')
        bad = emit(f'OpAny {boolean} {invalid_components}')
        flags = load_member(6, u32, ui)
        valid_bit = emit(f'OpBitwiseAnd {u32} {flags} {uone}')
        invalid_flag = emit(f'OpIEqual {boolean} {valid_bit} {uzero}')
        bad = emit(f'OpLogicalOr {boolean} {bad} {invalid_flag}')
        components = [emit(f'OpCompositeExtract {f32} {q} {i}') for i in range(3)]
        qx, qy, qz = components
        behind = emit(f'OpFOrdLessThanEqual {boolean} {qz} {zero}')
        bad = emit(f'OpLogicalOr {boolean} {bad} {behind}')
        for component in (qx, qy):
            negative = emit(f'OpFNegate {f32} {component}')
            positive_out = emit(f'OpFOrdGreaterThan {boolean} {component} {qz}')
            negative_out = emit(f'OpFOrdGreaterThan {boolean} {negative} {qz}')
            outside = emit(f'OpLogicalOr {boolean} {positive_out} {negative_out}')
            bad = emit(f'OpLogicalOr {boolean} {bad} {outside}')
        opacity = load_member(3, f32, uf)
        # Malformed extension must not produce NaN framebuffer data.
        opacity_nan = emit(f'OpIsNan {boolean} {opacity}')
        opacity_low = emit(f'OpFOrdLessThan {boolean} {opacity} {zero}')
        opacity_high = emit(f'OpFOrdGreaterThan {boolean} {opacity} {one}')
        for condition in (opacity_nan, opacity_low, opacity_high):
            bad = emit(f'OpLogicalOr {boolean} {bad} {condition}')
        kill = a.fresh()
        code += [f'OpSelectionMerge {label} None', f'OpBranchConditional {bad} {kill} {label}', f'{kill} = OpLabel', 'OpKill']
        # A NEW entry block preserves the original first-label identity in native OpPhi.
        lines[entry_function+1:entry_function+1] = code
        outputs = [key for key, rhs in definitions.items() if rhs.startswith('OpVariable ') and rhs.endswith(' Output') and f'OpDecorate {key} Location 0' in lines]
        if len(outputs) != 1 or definitions[definitions[outputs[0]].split()[1]] != f'OpTypePointer Output {v4}':
            raise ValueError('expected one vec4 color output')
        output = outputs[0]
        stores = [i for i, s in enumerate(lines) if s.startswith(f'OpStore {output} ')]
        if not stores:
            raise ValueError('missing native color store')
        # Retain every native instruction, including each original store. Scale RGBA after it.
        for i in reversed(stores):
            code.clear()
            color = emit(f'OpLoad {v4} {output}')
            scaled = emit(f'OpVectorTimesScalar {v4} {color} {opacity}')
            code.append(f'OpStore {output} {scaled}')
            lines[i+1:i+1] = code
    first_function = next(i for i, s in enumerate(lines) if ' = OpFunction ' in s)
    lines[first_function:first_function] = a.declarations
    return '\n'.join(lines)+'\n'


def verify_preservation(original, replacement):
    """Require ordered, literal survival of native body/interface/type instructions."""
    old, new = instructions(original), instructions(replacement)
    pos = 0
    for line in old:
        appendable = ' = OpTypeStruct ' in line or line.startswith('OpEntryPoint ')
        found = next((i for i in range(pos, len(new)) if new[i] == line or appendable and new[i].startswith(line+' ')), None)
        if found is None:
            raise ValueError(f'native instruction changed: {line}')
        pos = found+1
    if sum(' = OpTypeStruct ' in s for s in old) != sum(' = OpTypeStruct ' in s for s in new):
        raise ValueError('unexpected block type changes')


def assemble_patch(original, output, stage, raw_position=None):
    text = run('spirv-dis', '--raw-id', original)
    patched = patch_assembly(text, stage, raw_position)
    asm = output.with_suffix('.spvasm')
    asm.write_text(patched)
    run('spirv-as', '--target-env', 'spv1.0', '--preserve-numeric-ids', asm, '-o', output)
    run('spirv-val', '--target-env', 'vulkan1.0', output)
    verify_preservation(text, run('spirv-dis', '--raw-id', output))


def generate(source, output):
    source, output = Path(source).resolve(), Path(output).resolve()
    if source == output or source in output.parents:
        raise ValueError('output must be separate from native inputs')
    inputs = {}
    for ident, pin in MANIFEST['shaders'].items():
        data = (source / pin['filename']).read_bytes()
        if hashlib.sha256(data).hexdigest() != pin['sha256']:
            raise ValueError(f'native shader {ident} hash mismatch')
        inputs[ident] = data
    output.mkdir(parents=True, exist_ok=True)
    # Publish a result manifest only after the entire bundle passes validation.
    result_path = output / 'portal-shaders.json'
    result_path.unlink(missing_ok=True)
    result = {'schema': 1, 'ubo_bytes': 592, 'aperture_location': 3, 'pairs': MANIFEST['pairs'], 'shaders': {}}
    for ident, pin in MANIFEST['shaders'].items():
        original = output / f'{ident}.original.spv'
        original.write_bytes(inputs[ident])
        run('spirv-val', '--target-env', 'vulkan1.0', original)
        replacement = output / f'{ident}.portal.spv'
        assemble_patch(original, replacement, pin['stage'], pin['raw_position_id'])
        result['shaders'][ident] = {'original_sha256': pin['sha256'], 'portal_sha256': hashlib.sha256(replacement.read_bytes()).hexdigest(), 'file': replacement.name, 'stage': pin['stage'], 'entrypoint': 'VS_MAIN' if pin['stage']=='vertex' else 'PS_MAIN', 'binding': 0 if pin['stage']=='vertex' else 4}
    result_path.write_text(json.dumps(result, indent=2)+'\n')
    return result


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--source', type=Path, default=Path('/opt/steamvr/resources/shaders/vulkan'))
    parser.add_argument('--output', type=Path, default=Path(__file__).resolve().parents[1]/'build'/'shaders')
    args = parser.parse_args()
    result = generate(args.source, args.output)
    print(f'PASS: {len(result["shaders"])} paired native variants validated; generated private files in {args.output}')


if __name__ == '__main__':
    main()
