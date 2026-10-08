#!/usr/bin/env python3
"""Real callback cache association and live-rate sampling regressions."""
import pathlib
import subprocess
import tempfile
import unittest

R = pathlib.Path(__file__).resolve().parent

def compile_callback(p):
    source = (R/'observer.c').read_text().split('static int guards(', 1)[0]
    for text in ('#include <openssl/evp.h>\n', '#include "identity.h"\n',
                 '#if !defined(__aarch64__) || __BYTE_ORDER__ != __ORDER_LITTLE_ENDIAN__\n#error This observer is little-endian AArch64 only.\n#endif\n'):
        source = source.replace(text, '', 1)
    (p/'observer-under-test.c').write_text(source)
    subprocess.run(['cc','-O2','-Wall','-Wextra','-Werror','-Wno-unused-function',
                    '-Wno-unused-variable','-Wno-misleading-indentation','-I'+str(p),
                    '-I'+str(R),str(R/'test_cache_join.c'),'-pthread',
                    '-Wl,--wrap=clock_gettime','-o',str(p/'test')],check=True)
    return p/'test'

class CacheJoinTests(unittest.TestCase):
    def run_case(self, case):
        with tempfile.TemporaryDirectory(dir=R/'build',prefix='cache-') as d:
            binary=compile_callback(pathlib.Path(d))
            result=subprocess.run([str(binary),case],capture_output=True,text=True)
            self.assertEqual(result.returncode,0,result.stdout+result.stderr)
            print(result.stdout,end='')

    def test_boolean_x0_unchanged_join_is_not_an_edge(self):
        self.run_case('association')

    def test_unknown_join_never_requests_a_window(self):
        self.run_case('unknown')

    def test_live_rate_120s_budget_and_late_camera(self):
        self.run_case('schedule')

if __name__=='__main__':unittest.main(verbosity=2)
