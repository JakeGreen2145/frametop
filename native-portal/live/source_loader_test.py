#!/usr/bin/env python3
"""Real merged loader emission, dual-image identity and atomic installation."""
import ctypes,json,unittest
import loader_test as L

COMP={0x15fa24,0x15fa28,0x15fa44,0x15fa48,0x15fae8,0x15faec,0x15fbec,0x15fbf0,0x15fc34,0x15fc38}
CLIENT={0x32a270,0x32a398,0x32a39c,0x32a49c,0x32a504}
class SourceLoaderTests(unittest.TestCase):
    def test_actual_production_manifest_has_both_modules(self):
        r=L.prepare.prepare(L.prepare.ELF,L.B,L.ROOT/'live/runtime-sites.json')
        self.assertTrue(COMP <= {s['pc'] for s in r['sites']},'compositor source observations not installed')
        self.assertTrue(CLIENT <= {s['pc'] for s in r.get('client_sites',[])},'client source observations not installed')
    def test_client_pin_required(self):
        bad=L.B/'wrong-client.elf';bad.write_bytes(b'not the pinned image')
        with self.assertRaises(ValueError):
            L.prepare.prepare(L.prepare.ELF,L.B,L.ROOT/'live/runtime-sites.json',client=bad)

if __name__=='__main__':
    suite=unittest.defaultTestLoader.loadTestsFromTestCase(SourceLoaderTests)
    result=unittest.TextTestRunner(verbosity=2).run(suite)
    if not result.wasSuccessful():raise SystemExit(1)
    L.compile_tests(L.ROOT/'live/runtime-sites.json')
    dll=ctypes.CDLL(str(L.B/'loader-test.so'))
    dll.ft_loader_test_dual.argtypes=[ctypes.c_char_p]
    assert dll.ft_loader_test_dual(str(L.prepare.CLIENT_ELF).encode())==1
    # Reuse the all-register/SIMD/flags/continuation comparator for BOTH images.
    # Client emitter IDs are in the same global table, not donor-local IDs.
    rows=json.loads((L.B/'sites.private.json').read_text())
    rows['sites']+=rows['client_sites']
    (L.B/'sites.private.json').write_text(json.dumps(rows))
    result=unittest.TextTestRunner(verbosity=2).run(L.LoaderTests('test_actual_merged_emitters'))
    raise SystemExit(0 if result.wasSuccessful() else 1)
