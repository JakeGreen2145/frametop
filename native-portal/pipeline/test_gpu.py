#!/usr/bin/env python3
# SPDX-License-Identifier: MIT
"""Real-loader GPU positives and three compiled production-layer mutants."""
import argparse
import json
import os
from pathlib import Path
import subprocess
from build import FLAGS, HERE, run


def environment(directory):
    env=dict(os.environ)
    for name in ('LD_PRELOAD','VK_INSTANCE_LAYERS','VK_LAYER_PATH','VK_ADD_LAYER_PATH',
                 'VK_LOADER_LAYERS_ENABLE','VK_LOADER_LAYERS_DISABLE','VK_LOADER_LAYERS_ALLOW'):
        env.pop(name,None)
    env.update(VK_ADD_LAYER_PATH=str(directory),FT_TEST_LAYER=str(directory/'libft_portal_pipeline.so.1'),MESA_SHADER_CACHE_DISABLE='true')
    return env


def main(output):
    output=output.resolve()
    if HERE.parent/'build' not in output.parents: raise ValueError('isolated build directory required')
    executable=output/'test-gpu-layer'
    positive=subprocess.run([str(executable),str(output/'gpu-input')],env=environment(output),text=True,capture_output=True,timeout=90)
    (output/'gpu-green.log').write_text(positive.stdout+positive.stderr)
    print(positive.stdout+positive.stderr, end='')
    if positive.returncode: raise RuntimeError(f'GPU positive failed {positive.returncode}')
    source=(HERE/'layer.cpp').read_text()
    mutations={
        'global-replacement': ('if(!scope.token){auto result=d->create(h,cache,n,c,a,out);observe(d,result);return result;}',
            'if(!scope.token)scope={fresh(),d->generation,h,57,125,false,false};'),
        'missing-fragment': ('stages[i][fi].module=pf;', '/* MUTANT missing fragment substitution */'),
        'accept-stock-fallback': ('stages[i][vi].module=pv;stages[i][fi].module=pf;',
            '/* MUTANT accepts stock handles while recording claimed replacements */'),
    }
    results={}
    for name,(old,new) in mutations.items():
        # Match tokens independent of clang-format whitespace, retaining all other code.
        positions=[i for i,ch in enumerate(source) if not ch.isspace()]
        compact=''.join(source[i] for i in positions)
        target=''.join(old.split())
        if compact.count(target)!=1: raise ValueError('mutation target changed: '+name)
        start=compact.index(target)
        mutated=source[:positions[start]]+new+source[positions[start+len(target)-1]+1:]
        directory=output/name;directory.mkdir(exist_ok=True)
        mutant=directory/'layer.cpp';mutant.write_text(mutated)
        run(['c++',*FLAGS,'-I',HERE,'-I',output,'-fPIC','-fvisibility=hidden','-shared',
             '-Wl,-Bsymbolic-functions,-z,relro,-z,now,-z,noexecstack,-soname,libft_portal_pipeline.so.1',
             mutant,'-o',directory/'libft_portal_pipeline.so.1'])
        (directory/'VkLayer_FT_scoped_portal.json').write_bytes((output/'VkLayer_FT_scoped_portal.json').read_bytes())
        result=subprocess.run([str(executable),str(output/'gpu-input')],env=environment(directory),text=True,capture_output=True,timeout=90)
        log=result.stdout+result.stderr
        (directory/'gpu-red.log').write_text(log)
        expected=8 if name=='global-replacement' else 7
        marker='FAIL registry' if expected==8 else 'FAIL case='
        if result.returncode!=expected or marker not in log:
            print(log);raise RuntimeError(f'{name}: expected oracle refusal {expected}, got {result.returncode}')
        diagnostic=next(line for line in log.splitlines() if marker in line)
        results[name]={'exit':result.returncode,'diagnostic':diagnostic}
        print('PASS rejected '+name+': '+diagnostic)
    (output/'gpu-mutations.json').write_text(json.dumps(results,indent=2)+'\n')
    print('PASS real-loader GPU original/portal coexistence and all three mutation oracles')


if __name__=='__main__':
    p=argparse.ArgumentParser(description=__doc__);p.add_argument('--output',type=Path,required=True)
    main(p.parse_args().output)
