#!/usr/bin/env python3
# SPDX-License-Identifier: MIT
"""Pinned graph sites through canonical prepare and the actual CPU installer."""
import json
import subprocess
import sys
import unittest
import admission_hooks_test as T
import loader_test as L

T.BASE='c37911c77a140e295e1e085e11f4e9fce3b8aaf2'
REQUEST=L.ROOT/'live/graph-hook-request.json'
PCS=[int(s['pc'],0) for s in json.loads(REQUEST.read_text())['required_sites']]
T.PINS={('compositor',pc):{} for pc in PCS}

class GraphHooks(unittest.TestCase):
    def test_canonical_additions(self):
        old=T.keyed(T.baseline());new=T.keyed(L.prepare.prepare(L.prepare.ELF,L.B,T.MANIFEST))
        self.assertEqual(len(old),242)
        self.assertEqual(set(new)-set(old),set(T.PINS))
        for key,row in old.items():self.assertEqual(new[key],row)
        old_manifest=json.loads(subprocess.check_output(['git','show',T.BASE+':native-portal/live/runtime-sites.json'],cwd=L.ROOT))
        current=json.loads(T.MANIFEST.read_text())
        self.assertEqual({k:v for k,v in old_manifest.items() if k!='sites'},{k:v for k,v in current.items() if k!='sites'})
        for pc in PCS:
            row=new['compositor',pc]
            self.assertEqual(row['emitter'],'observer')
            self.assertEqual(row['kind'],5 if pc in (0x2a2498,0x2a2564) else 3 if pc in (0x299a60,0x2988a8) else 0)
        self.assertEqual(T.baseline()['stock_observer_coverage'],L.prepare.prepare(L.prepare.ELF,L.B,T.MANIFEST)['stock_observer_coverage'])

if __name__=='__main__':
    result=unittest.TextTestRunner(verbosity=2).run(unittest.defaultTestLoader.loadTestsFromTestCase(GraphHooks))
    if not result.wasSuccessful():raise SystemExit(1)
    if '--full' in sys.argv:
        L.compile_tests(T.MANIFEST)
        T.installed_matrix('--sanitize' in sys.argv)
        result=unittest.TextTestRunner(verbosity=2).run(unittest.TestSuite([L.LoaderTests('test_actual_merged_emitters')]))
        if not result.wasSuccessful():raise SystemExit(1)
        L.run([sys.executable,'-B',L.ROOT/'live/graph_hooks_machine.py'])
