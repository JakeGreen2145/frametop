#!/usr/bin/env python3
"""Default production constructor, pinned observations, serial 256M/zero swap."""
import os, runpy, sys
from pathlib import Path
ROOT=Path(__file__).resolve().parents[1]
sys.path.insert(0,str(ROOT/'live'))
g=runpy.run_path(str(ROOT/'live/source_runtime_test.py'))
B=g['B']; run=g['run']; flags=g['flags']
from production_vectors import generate
generate(g,B/"admission.private.hpp")
obj=B/'production-admission.o'
run(['g++',*flags,'-c',ROOT/'live/production_admission_test.cpp','-o',obj])
objects=[o for o in g['g']['objects'] if 'camera_runtime_test.cpp' not in str(o)]
exe=B/'production-admission'
run(['g++',*flags,obj,*objects,'-o',exe])
for mode in ["cold","warm","dedup","sustained-on","--aggregate-portal-first","--aggregate-portal-last","--aggregate-clear"]:run([exe,mode])
unit=B/"native-source-admission-unit"
run(["g++",*flags,ROOT/"live/native_source_admission_test.cpp","-o",unit]);run([unit])
import subprocess, signal, json
results=[]
for mode in ['partial-connect','partial-alias','missing-event','provider-migration','provider-loss','association-migration','bad-selection']:
    result=subprocess.run(['systemd-run','--user','--scope','--quiet','-p','MemoryMax=256M','-p','MemorySwapMax=0',str(exe),mode],capture_output=True,text=True,timeout=30)
    text=result.stdout+result.stderr;(B/('production-'+mode+'.log')).write_text(text)
    assert result.returncode==-signal.SIGABRT and 'production-cleanup-guard:' in text,(mode,result.returncode,text[-2000:])
    assert 'ERROR: AddressSanitizer' not in text and 'runtime error:' not in text
    results.append(dict(mode=mode,returncode=result.returncode));print('PASS default production refusal:',mode)
(B/'production-negatives.json').write_text(json.dumps(results,indent=2)+'\n')
