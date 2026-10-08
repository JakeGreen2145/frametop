#!/usr/bin/env python3
import pathlib, unittest
R=pathlib.Path(__file__).resolve().parent
class RuntimeAcceptance(unittest.TestCase):
    def test_arm_native_runtime_and_refusals(self):
        report=R/'build/native-test.txt'
        self.assertTrue(report.exists(), 'native observer and runtime acceptance are not implemented')
        text=report.read_text()
        for result in ('PASS fresh-private-file','PASS pinned-file-map','PASS identity-refusal','PASS transaction-and-rollback','PASS ring-and-metadata','PASS native-bridge','PASS deadline-drain','PASS default-off-and-wrong-exe'):
            self.assertIn(result,text)
if __name__=='__main__':unittest.main(verbosity=2)
