#!/usr/bin/env python3
# SPDX-License-Identifier: MIT
"""Production source selection only; no ELF, native runner or device access."""
import ast
from pathlib import Path
import unittest

ROOT = Path(__file__).resolve().parents[1]


def inventory():
    tree = ast.parse((ROOT / 'live/build.py').read_text())
    assignment = next(n for n in tree.body if isinstance(n, ast.Assign)
                      and any(isinstance(t, ast.Name) and t.id == 'BASE_SOURCES' for t in n.targets))
    function = next(n for n in tree.body if isinstance(n, ast.FunctionDef) and n.name == 'inventory')
    namespace = {'ROOT': ROOT, 'BASE_SOURCES': ast.literal_eval(assignment.value)}
    exec(compile(ast.Module(body=[function], type_ignores=[]), str(ROOT / 'live/build.py'), 'exec'), namespace)
    return namespace['inventory']()


class ProductionInventoryTests(unittest.TestCase):
    def test_red_probe_not_in_production_owner(self):
        self.assertNotIn('runtime/native_stock_begin_red.cpp', inventory())

    def test_actual_collector_and_runtime_are_linked(self):
        sources = inventory()
        for required in ('runtime/native_stock_facts.cpp', 'runtime/evidence.cpp',
                         'runtime/recording.cpp', 'runtime/staging.cpp', 'live/runtime.cpp'):
            self.assertIn(required, sources)
        self.assertEqual(len(sources), len(set(sources)))


if __name__ == '__main__':
    unittest.main(verbosity=2)
