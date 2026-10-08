import importlib.util, pathlib, struct, unittest
R=pathlib.Path(__file__).resolve().parent
class DecoderTests(unittest.TestCase):
    def test_real_metadata_fixture(self):
        fixture=R/'build/metadata-fixture.bin'
        self.assertTrue(fixture.exists(),'native metadata roundtrip fixture is missing')
        spec=importlib.util.spec_from_file_location('decode',R/'decode.py');module=importlib.util.module_from_spec(spec);spec.loader.exec_module(module)
        with fixture.open('rb') as f:events=list(module.records(f))
        self.assertEqual([e['eye'] for e in events],[0,1])
        self.assertEqual([e['eye_seen_mask'] for e in events],[1,3])
        for e in events:
            self.assertEqual([len(bytes.fromhex(c['hex'])) for c in e['chunks']],[528,48,352])
            self.assertTrue(all(c['valid'] for c in e['chunks']))
    def test_real_native_fixture_and_truncation(self):
        self.assertTrue((R/'decode.py').exists(),'event decoder is not implemented')
        spec=importlib.util.spec_from_file_location('decode',R/'decode.py');module=importlib.util.module_from_spec(spec);spec.loader.exec_module(module)
        fixture=R/'build/control-fixture.bin'
        self.assertTrue(fixture.exists(),'fetch real ARM fixture first')
        with fixture.open('rb') as f:records=list(module.records(f))
        self.assertEqual(len(records),1);self.assertEqual(records[0]['control'],'refuse-executable')
        import io
        with self.assertRaises(ValueError):list(module.records(io.BytesIO(fixture.read_bytes()[:-1])))
        malformed=bytearray(fixture.read_bytes());struct.pack_into('<I',malformed,4,1)
        with self.assertRaises(ValueError):list(module.records(io.BytesIO(malformed)))
if __name__=='__main__':unittest.main(verbosity=2)
