#!/usr/bin/env python3
# SPDX-License-Identifier: MIT
"""Real-loader regression for a preloaded owner's production pipeline dependency.

Uses a prebuilt production DSO from build.py, not layer.cpp linked into the app.
The owner fixture models only DT_NEEDED/global scope, never native launch hooks.
No installation, WSI, VR, or device configuration. Set VK_ICD_FILENAMES to select
an isolated software ICD, or run on an explicitly approved non-VR GPU.
"""
import argparse
import json
import os
from pathlib import Path
import subprocess
from build import FLAGS, HERE, run


def main(output):
    output = output.resolve()
    if HERE.parent / 'build' not in output.parents:
        raise ValueError('isolated build directory required')
    pipeline = output / 'libft_portal_pipeline.so.1'
    if not pipeline.is_file():
        raise ValueError('run pipeline/build.py first')
    work = output / 'preload-test'
    work.mkdir(exist_ok=True)
    stack = work / 'libtest-stack.so'
    run(['c++', *FLAGS, '-fPIC', '-fvisibility=hidden', '-shared',
         HERE / 'test_stack.cpp', '-o', stack])
    manifest = {'file_format_version': '1.2.0', 'layer': {
        'name': 'VK_LAYER_FT_test_stack', 'type': 'GLOBAL',
        'library_path': str(stack), 'api_version': '1.3.0',
        'implementation_version': '1', 'description': 'Test-only dispatch counter'}}
    (work / 'VkLayer_FT_test_stack.json').write_text(json.dumps(manifest))
    owner_source = work / 'owner.c'
    owner_source.write_text('/* Test-only DT_NEEDED owner; no constructor or native hooks. */\n'
                            'void ft_test_preload_owner(void) {}\n')
    owner = work / 'libtest-owner.so'
    run(['cc', '-shared', '-fPIC', owner_source, '-Wl,--no-as-needed', pipeline,
         '-Wl,-rpath,' + str(output), '-o', owner])
    dynamic = subprocess.check_output(['readelf', '-d', owner], text=True)
    if 'Shared library: [libft_portal_pipeline.so.1]' not in dynamic:
        raise RuntimeError('fixture did not retain the production dependency')
    executable = work / 'test-preload'
    run(['c++', *FLAGS, HERE / 'test_preload.cpp', '-l:libvulkan.so.1', '-ldl', '-o', executable])
    dynamic_executable = work / 'test-preload-dynamic'
    run(['c++', *FLAGS, '-DFT_TEST_DYNAMIC_LOADER', HERE / 'test_preload.cpp',
         '-ldl', '-o', dynamic_executable])
    env = dict(os.environ)
    for name in ('LD_PRELOAD', 'VK_INSTANCE_LAYERS', 'VK_LAYER_PATH', 'VK_ADD_LAYER_PATH',
                 'VK_LOADER_LAYERS_ENABLE', 'VK_LOADER_LAYERS_DISABLE', 'VK_LOADER_LAYERS_ALLOW'):
        env.pop(name, None)
    env.update(VK_LAYER_PATH=os.pathsep.join((str(work), str(output))),
               VK_LOADER_LAYERS_DISABLE='~implicit~',
               FT_TEST_STACK=str(stack), FT_TEST_LAYER=str(pipeline),
               MESA_SHADER_CACHE_DISABLE='true', LP_NUM_THREADS='1')
    results = []
    for executable, preload in ((exe, pre) for exe in (executable, dynamic_executable)
                               for pre in (None, pipeline, owner)):
        for mode, order in (('symbols', 'above'), ('create', 'above'),
                            ('stack', 'above'), ('stack', 'below'),
                            ('device', 'above'), ('device', 'below')):
            case_env = dict(env)
            if preload:
                case_env['LD_PRELOAD'] = str(preload)
            name = f'{executable.name}-{preload.name if preload else "baseline"}-{mode}-{order}'
            result = subprocess.run([str(executable), mode, order], env=case_env,
                                    text=True, capture_output=True, timeout=60)
            log = result.stdout + result.stderr
            (work / (name + '.log')).write_text(log)
            results.append({'case': name, 'exit': result.returncode, 'log': log})
            print(f'{name}: exit={result.returncode}\n{log}', flush=True)
    (work / 'results.json').write_text(json.dumps(results, indent=2) + '\n')
    failed = [r['case'] for r in results if r['exit']]
    if failed:
        raise RuntimeError('loader/preload regressions: ' + ', '.join(failed))
    print(f'PASS {len(results)} real-loader baseline/direct/transitive preload cases')


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--output', type=Path, required=True)
    main(parser.parse_args().output)
