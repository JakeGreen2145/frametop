#!/usr/bin/env python3
# SPDX-License-Identifier: MIT
"""Private build-time bundle, never runtime arbitrary shader configuration."""
import argparse
import hashlib
import json
from pathlib import Path
import struct
import sys
ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / 'tools'))
sys.path.insert(0, str(ROOT / 'tests'))
import generate_shaders as g
import gpu_shaders


def emit(output, source=None, synthetic=False):
    output = Path(output).resolve()
    output.mkdir(parents=True, exist_ok=True)
    if synthetic:
        gpu_shaders.build(output)
        specs = [(57, 'vertex', 'main'), (125, 'fragment', 'main')]
        data = [(i, s, e, (output/f'{s}.owned.spv').read_bytes(),
                 (output/f'{s}.spv').read_bytes()) for i, s, e in specs]
        pairs = [(57, 125)]
    else:
        manifest = g.generate(source, output)
        data = [(int(i), s['stage'], s['entrypoint'],
                 (output/f'{i}.original.spv').read_bytes(),
                 (output/s['file']).read_bytes()) for i, s in manifest['shaders'].items()]
        pairs = manifest['pairs']
    lines = ['#pragma once', '#include <vulkan/vulkan.h>', '#include <cstddef>', '#include <cstdint>',
             'struct FtBundle { uint32_t id; VkShaderStageFlagBits stage; const char* entry; const uint32_t* original; size_t original_size; const uint32_t* portal; size_t portal_size; };']
    for i, _, _, original, portal in data:
        assert original != portal
        for name, binary in [('original', original), ('portal', portal)]:
            words = struct.unpack('<'+'I'*(len(binary)//4), binary)
            lines.append(f'inline constexpr uint32_t {name}_{i}[] = {{'+','.join(hex(w) for w in words)+'};')
    lines += ['inline constexpr FtBundle ft_bundle[] = {']
    for i, stage, entry, _, _ in data:
        lines.append(f'{{{i}, VK_SHADER_STAGE_{stage.upper()}_BIT, "{entry}", original_{i}, sizeof(original_{i}), portal_{i}, sizeof(portal_{i})}},')
    lines += ['};', 'inline constexpr uint32_t ft_pairs[][2] = {'+','.join('{'+f'{v},{f}'+'}' for v,f in pairs)+'};']
    (output/'bundle.hpp').write_text('\n'.join(lines)+'\n')
    checksums = {p.name: hashlib.sha256(p.read_bytes()).hexdigest() for p in output.iterdir() if p.is_file() and p.suffix in ('.spv','.hpp')}
    (output/'hashes.json').write_text(json.dumps(checksums, indent=2)+'\n')
    print(f'PASS embedded bundle shaders={len(data)} pairs={len(pairs)} synthetic={synthetic}')


if __name__ == '__main__':
    p=argparse.ArgumentParser(description=__doc__)
    p.add_argument('--output',type=Path,required=True)
    p.add_argument('--source',type=Path,default=Path('/opt/steamvr/resources/shaders/vulkan'))
    p.add_argument('--synthetic',action='store_true')
    a=p.parse_args()
    emit(a.output,a.source,a.synthetic)
