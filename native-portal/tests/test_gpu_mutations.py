#!/usr/bin/env python3
# SPDX-License-Identifier: MIT
"""Prove the pixel oracle rejects unpatched, affine-varying, and alpha-only mutants.

Run on the shader-tool host. Pass the executable command after --, for example
  python3 -B tests/test_gpu_mutations.py -- /path/to/test-gpu
The runner command receives one additional argument, the private fixture directory.
"""
import argparse
from pathlib import Path
import re
import subprocess
import sys
import gpu_shaders as fixture

g = fixture.g
ROOT = Path(__file__).resolve().parents[1]


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('runner', nargs=argparse.REMAINDER)
    args = parser.parse_args()
    runner = args.runner
    if runner and runner[0] == '--': runner = runner[1:]
    if not runner: runner = [str(ROOT/'build'/'cmake'/'test-gpu')]
    output = ROOT/'build'/'gpu-mutations'
    for mutant in ('unpatched', 'noperspective', 'alpha-only'):
        directory = output/mutant
        fixture.build(directory, patched=mutant!='unpatched')
        if mutant!='unpatched':
            stages = ('vertex','fragment') if mutant=='noperspective' else ('fragment',)
            for stage in stages:
                binary = directory/f'{stage}.spv'
                text = g.run('spirv-dis','--raw-id',binary)
                if mutant=='noperspective':
                    match = re.search(r'OpDecorate (%\d+) Location 3',text)
                    assert match
                    text = text.replace(match.group(0), match.group(0)+f'\nOpDecorate {match.group(1)} NoPerspective')
                else:
                    # Replace only the generated premultiplied multiply with alpha-only.
                    match = re.search(r'(%\d+) = OpVectorTimesScalar (%\d+) (%\d+) (%\d+)',text)
                    assert match
                    result,v4,color,opacity = match.groups()
                    float_type = re.search(r'(%\d+) = OpTypeFloat 32',text).group(1)
                    next_id = max(map(int,re.findall(r'%(\d+)',text)))+1
                    alpha,scaled = f'%{next_id}',f'%{next_id+1}'
                    text = text.replace(match.group(0), f'{alpha} = OpCompositeExtract {float_type} {color} 3\n{scaled} = OpFMul {float_type} {alpha} {opacity}\n{result} = OpCompositeInsert {v4} {scaled} {color} 3')
                assembly = binary.with_suffix('.mutant.spvasm')
                assembly.write_text(text)
                g.run('spirv-as','--target-env','spv1.0','--preserve-numeric-ids',assembly,'-o',binary)
                g.run('spirv-val','--target-env','vulkan1.0',binary)
        test = subprocess.run(runner+[str(directory)],text=True,capture_output=True,timeout=60)
        if test.returncode!=7 or 'FAIL case=' not in test.stderr:
            print(test.stdout+test.stderr)
            raise RuntimeError(f'{mutant} must fail the pixel oracle, not merely crash: exit={test.returncode}')
        print(f'PASS rejected {mutant}: '+test.stderr.strip())
    print('PASS three GPU mutation controls')


if __name__=='__main__':
    main()
