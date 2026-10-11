#!/usr/bin/env python3
# SPDX-License-Identifier: MIT
"""Evaluate literal source inventories without importing native analysis runners."""
import ast
import hashlib
from pathlib import Path
ROOT = Path(__file__).resolve().parents[1]
tree = ast.parse((ROOT / 'live/runtime_test.py').read_text())
sources = next(ast.literal_eval(n.value) for n in tree.body if isinstance(n, ast.Assign)
               and any(isinstance(t, ast.Name) and t.id == 'SOURCES' for t in n.targets))
assert 'runtime/native_stock_facts.cpp' in sources, 'full Runtime tests omit the real native stock collector TU'
tree = ast.parse((ROOT / 'live/build.py').read_text())
base = next(ast.literal_eval(n.value) for n in tree.body if isinstance(n, ast.Assign)
            and any(isinstance(t, ast.Name) and t.id == 'BASE_SOURCES' for t in n.targets))
function = next(n for n in tree.body if isinstance(n, ast.FunctionDef) and n.name == 'inventory')
namespace: dict = dict(ROOT=ROOT, BASE_SOURCES=base)
exec(compile(ast.Module(body=[function], type_ignores=[]), 'build.py:inventory', 'exec'), namespace)
production = namespace['inventory']()
assert 'runtime/native_stock_facts.cpp' in production
assert not any('_red' in p for p in production), 'a RED test main would be linked into the production owner'
namespace.update(Path=Path, hashlib=hashlib)
for name in ('hash_file', 'source_hashes'):
    node = next(n for n in tree.body if isinstance(n, ast.FunctionDef) and n.name == name)
    exec(compile(ast.Module(body=[node], type_ignores=[]), 'build.py:' + name, 'exec'), namespace)
hashes = namespace['source_hashes'](production, ROOT / 'live/runtime-sites.json')
assert 'live/runtime_dormant.inc' in hashes, 'production identity omits actual dormant routing implementation'
print('Runtime CPU/production inventories and identity include dormant implementation; RED probes excluded')
