#!/usr/bin/env python3
"""Portable real-pthread regressions, compiled from the current production C.

No copied callback implementation, OpenSSL dependency, A64 execution or live VR.
The generated prefix retains observe/drain verbatim; only the unused identity
includes and the A64 compilation guard are removed for this CPU-only test.
"""
import pathlib
import subprocess
import tempfile
import unittest

R = pathlib.Path(__file__).resolve().parent


class DrainTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        build = R / 'build'
        build.mkdir(exist_ok=True)
        cls.directory = tempfile.TemporaryDirectory(prefix='drain-test-', dir=build)
        cls.addClassCleanup(cls.directory.cleanup)
        cls.path = pathlib.Path(cls.directory.name)
        source = (R / 'observer.c').read_text().split('static int guards(', 1)[0]
        for text in (
            '#include <openssl/evp.h>\n',
            '#include "identity.h"\n',
            '#if !defined(__aarch64__) || __BYTE_ORDER__ != __ORDER_LITTLE_ENDIAN__\n'
            '#error This observer is little-endian AArch64 only.\n#endif\n',
        ):
            if source.count(text) != 1:
                raise AssertionError('Update the portable test boundary for changed includes/guard')
            source = source.replace(text, '', 1)
        (cls.path / 'observer-under-test.c').write_text(source)
        cls.binary = cls.path / 'test_drain'
        subprocess.run([
            'cc', '-O2', '-Wall', '-Wextra', '-Werror',
            '-Wno-unused-function', '-Wno-unused-variable', '-Wno-misleading-indentation',
            '-I' + str(cls.path), '-I' + str(R), str(R / 'test_drain.c'),
            '-pthread', '-Wl,--wrap=clock_gettime', '-o', str(cls.binary),
        ], check=True, timeout=30)

    def schedule(self, name):
        result = subprocess.run(
            [str(self.binary), name, str(self.path / (name + '.bin'))],
            capture_output=True, text=True, timeout=5,
        )
        self.assertEqual(result.returncode, 0, result.stdout + result.stderr)
        print(result.stdout, end='', flush=True)

    def test_publication_between_empty_observation_and_deadline(self):
        self.schedule('empty-deadline')

    def test_inflight_producer_cannot_publish_after_deadline_drain(self):
        self.schedule('inflight-deadline')

    def test_inflight_producer_cannot_publish_after_stop_drain(self):
        self.schedule('inflight-stop')


if __name__ == '__main__':
    unittest.main(verbosity=2)
