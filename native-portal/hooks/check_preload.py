#!/usr/bin/env python3
"""Exercise the real DSO in inert processes only. Never launch the compositor."""
import os
import pathlib
import subprocess
import sys
import tempfile


def check(library, scratch):
    library = pathlib.Path(library).resolve(strict=True)
    with tempfile.TemporaryDirectory(prefix='inert-preload-', dir=scratch) as directory:
        parent = pathlib.Path(directory).resolve()
        parent.chmod(0o700)
        log = parent / 'compositor.bin'
        env = os.environ.copy()
        env['LD_PRELOAD'] = str(library)
        env['FT_NATIVE_HOOKS_LOG'] = str(log)
        env.pop('FT_NATIVE_HOOKS_ENABLE', None)

        def inert():
            result = subprocess.run(['/usr/bin/true'], env=env, capture_output=True,
                                    text=True, timeout=10)
            assert result.returncode == 0, result
            # A loader failure can leave /usr/bin/true exiting successfully.
            assert not result.stdout and not result.stderr, result
            assert not log.exists(), 'non-target executable reserved the compositor log'
            assert not list(parent.iterdir()), 'non-target executable created a file'

        inert()
        print('PASS default-off no file', flush=True)
        env['FT_NATIVE_HOOKS_ENABLE'] = 'metadata-v1-9b1bd5fe'
        for attempt in (1, 2):
            inert()
            print(f'PASS wrong-executable attempt={attempt} no reservation', flush=True)
        print('PASS default-off-and-wrong-exe', flush=True)


if __name__ == '__main__':
    check(sys.argv[1], sys.argv[2])
