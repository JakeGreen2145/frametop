#!/usr/bin/env python3
# SPDX-License-Identifier: MIT
"""Owned, minimal synthetic shaders, processed by the production assembly transform."""
from pathlib import Path
import re
import sys
sys.path.insert(0, str(Path(__file__).resolve().parents[1] / 'tools'))
import generate_shaders as g


def base(stage):
    vertex = stage == 'vertex'
    entry = 'Vertex %main "main" %index %position' if vertex else 'Fragment %main "main" %color'
    lines = ['OpCapability Shader', 'OpMemoryModel Logical GLSL450', f'OpEntryPoint {entry}']
    if not vertex: lines += ['OpExecutionMode %main OriginUpperLeft']
    lines += ['OpDecorate %array ArrayStride 16', 'OpDecorate %block Block', 'OpDecorate %ubo DescriptorSet 0', f'OpDecorate %ubo Binding {0 if vertex else 4}']
    lines += [f'OpMemberDecorate %block {i} Offset {16*i if i<25 else 464}' for i in range(26)]
    lines += ['OpDecorate %index BuiltIn VertexIndex', 'OpDecorate %position BuiltIn Position'] if vertex else ['OpDecorate %color Location 0']
    # Debug names must precede annotations in logical layout.
    if vertex: lines.insert(3, 'OpName %raw "ft_raw"')
    lines += ['%void = OpTypeVoid', '%fun = OpTypeFunction %void', '%float = OpTypeFloat 32', '%uint = OpTypeInt 32 0', '%v3 = OpTypeVector %float 3', '%v4 = OpTypeVector %float 4', '%four = OpConstant %uint 4', '%array = OpTypeArray %v4 %four', '%block = OpTypeStruct '+' '.join(['%v4']*25+['%array']), '%up = OpTypePointer Uniform %block', '%ubo = OpVariable %up Uniform', '%uv = OpTypePointer Uniform %v4', '%zeroi = OpConstant %uint 0', '%onei = OpConstant %uint 1', '%zero = OpConstant %float 0', '%one = OpConstant %float 1', '%half = OpConstant %float 0.5']
    if vertex:
        lines += ['%minus = OpConstant %float -1', '%three = OpConstant %float 3', '%px = OpConstantComposite %v3 %minus %three %minus', '%py = OpConstantComposite %v3 %minus %minus %three', '%ip = OpTypePointer Input %uint', '%index = OpVariable %ip Input', '%op = OpTypePointer Output %v4', '%position = OpVariable %op Output', '%main = OpFunction %void None %fun', '%label = OpLabel', '%idx = OpLoad %uint %index', '%xp = OpVectorExtractDynamic %float %px %idx', '%yp = OpVectorExtractDynamic %float %py %idx', '%wp = OpAccessChain %uv %ubo %zeroi', '%ws = OpLoad %v4 %wp', '%w = OpVectorExtractDynamic %float %ws %idx', '%x = OpFMul %float %xp %w', '%y = OpFMul %float %yp %w', '%z = OpFNegate %float %w', '%raw = OpCompositeConstruct %v4 %x %y %z %one', '%depth = OpFMul %float %half %w', '%clip = OpCompositeConstruct %v4 %x %y %depth %w', 'OpStore %position %clip', 'OpReturn', 'OpFunctionEnd']
    else:
        lines += ['%op = OpTypePointer Output %v4', '%color = OpVariable %op Output', '%main = OpFunction %void None %fun', '%label = OpLabel', '%cp = OpAccessChain %uv %ubo %onei', '%rgba = OpLoad %v4 %cp', 'OpStore %color %rgba', 'OpReturn', 'OpFunctionEnd']
    return '\n'.join(lines)+'\n'


def build(output, patched=True):
    output.mkdir(parents=True, exist_ok=True)
    for stage in ('vertex', 'fragment'):
        asm = output / f'{stage}.owned.spvasm'
        original = output / f'{stage}.owned.spv'
        asm.write_text(base(stage))
        g.run('spirv-as', '--target-env', 'spv1.0', asm, '-o', original)
        g.run('spirv-val', '--target-env', 'vulkan1.0', original)
        dis = g.run('spirv-dis', '--raw-id', original)
        raw = int(re.search(r'OpName %(\d+) "ft_raw"', dis).group(1)) if stage=='vertex' else None
        if patched:
            g.assemble_patch(original, output/f'{stage}.spv', stage, raw)
        else:
            (output/f'{stage}.spv').write_bytes(original.read_bytes())
    print('PASS owned fixtures: '+('same production generator' if patched else 'unpatched RED control'))


if __name__=='__main__':
    build(Path(__file__).resolve().parents[1]/'build'/'gpu', '--unpatched' not in sys.argv)
