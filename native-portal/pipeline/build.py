#!/usr/bin/env python3
# SPDX-License-Identifier: MIT
"""Build in an ignored directory. No install, service or VR operations."""
import argparse
import hashlib
import json
from pathlib import Path
import subprocess

HERE=Path(__file__).resolve().parent
ROOT=HERE.parent
FLAGS=['-std=c++17','-O2','-Wall','-Wextra','-Werror','-Wno-missing-field-initializers','-Wno-misleading-indentation','-pthread']


def run(args):
    print('$ '+' '.join(map(str,args)),flush=True)
    subprocess.run(list(map(str,args)),check=True,timeout=180)


def build(output, sanitizer=False):
    output=Path(output).resolve()
    if ROOT/'build' not in output.parents:
        raise ValueError('output must be below this mirror native-portal/build')
    if not (output/'bundle.hpp').is_file():
        raise ValueError('first generate bundle.py on a host with SPIR-V tools')
    run(['cc','-std=c11','-Wall','-Wextra','-Werror','-x','c','-fsyntax-only','-include',HERE/'ft_pipeline.h','/dev/null'])
    flags=FLAGS+(['-fsanitize=address,undefined','-fno-omit-frame-pointer','-g'] if sanitizer else [])
    run(['c++',*flags,'-I',output,HERE/'test_layer.cpp',HERE/'layer.cpp','-o',output/'test-layer'])
    run([output/'test-layer'])
    run(['c++',*FLAGS,'-I',output,'-fPIC','-fvisibility=hidden','-shared',
         '-Wl,-Bsymbolic-functions,-z,relro,-z,now,-z,noexecstack,-soname,libft_portal_pipeline.so.1',
         '-Wl,--version-script='+str(HERE/'exports.map'),
         HERE/'layer.cpp','-o',output/'libft_portal_pipeline.so.1'])
    (output/'VkLayer_FT_scoped_portal.json').write_bytes((HERE/'VkLayer_FT_scoped_portal.json').read_bytes())
    if (output/'vertex.owned.spv').is_file():
        # Reuse the existing exhaustive pixel oracle, not a second shader renderer.
        source=(ROOT/'tests/test_gpu.cpp').read_text()
        replacements={
            '#include "ft_aperture.hpp"':'#include "ft_aperture.hpp"\n#include "gpu_bridge.hpp"',
            '    VkPipeline pipeline;\n    ok(vkCreateGraphicsPipelines(d, VK_NULL_HANDLE, 1, &gp, nullptr, &pipeline));':
                '    VkPipeline pipelines[2];\n    for (int mode=0; mode<2; ++mode) {\n        pipeline_stock_mode = mode == 0;\n        ok(vkCreateGraphicsPipelines(d, VK_NULL_HANDLE, 1, &gp, nullptr, &pipelines[mode]));\n    }',
            '    for (const auto &test : cases()) {':
                '    for (int mode=0; mode<2; ++mode) {\n    pipeline_stock_mode = mode == 0;\n    VkPipeline pipeline = pipelines[mode];\n    for (const auto &test : cases()) {',
            '    vkDestroyPipeline(d, pipeline, nullptr);':
                '    }\n    for (auto pipeline : pipelines) vkDestroyPipeline(d, pipeline, nullptr);',
            'bool inside = contains(aq) && (test.p.flags & 1) && std::isfinite(test.p.opacity) &&\n                              test.p.opacity >= 0 && test.p.opacity <= 1;':
                'bool inside = pipeline_stock_mode || (contains(aq) && (test.p.flags & 1) && std::isfinite(test.p.opacity) &&\n                              test.p.opacity >= 0 && test.p.opacity <= 1);\n                float opacity = pipeline_stock_mode ? 1 : test.p.opacity;',
            'test.color[channel] * test.p.opacity +':'test.color[channel] * opacity +',
            '(1 - test.color[3] * test.p.opacity)':'(1 - test.color[3] * opacity)',
            'case_count, verified);\n}':'case_count, verified);\n    return 0;\n}',
        }
        for old,new in replacements.items():
            if source.count(old)!=1: raise ValueError('pixel fixture changed: '+old)
            source=source.replace(old,new)
        generated=output/'gpu.cpp'
        generated.write_text(source)
        run(['c++',*FLAGS,'-I',HERE,'-I',ROOT/'include',generated,ROOT/'src/aperture.cpp','-lvulkan','-ldl','-o',output/'test-gpu-layer'])
        fixture=output/'gpu-input';fixture.mkdir(exist_ok=True)
        for stage in ('vertex','fragment'):
            (fixture/f'{stage}.spv').write_bytes((output/f'{stage}.owned.spv').read_bytes())
    artifacts={p.name:hashlib.sha256(p.read_bytes()).hexdigest() for p in output.iterdir() if p.is_file() and (p.suffix in ('.1','.json') or p.name in ('test-layer','test-gpu-layer'))}
    (output/'artifacts.json').write_text(json.dumps(artifacts,indent=2)+'\n')
    print('PASS build explicit layer and tests; NOT installed or activated')


if __name__=='__main__':
    p=argparse.ArgumentParser(description=__doc__)
    p.add_argument('--output',type=Path,required=True)
    p.add_argument('--sanitizer',action='store_true')
    a=p.parse_args();build(a.output,a.sanitizer)
