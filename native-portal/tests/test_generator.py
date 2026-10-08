#!/usr/bin/env python3
# SPDX-License-Identifier: MIT
"""Native files stay private. Set FT_NATIVE_SHADER_SOURCE to installed/private inputs."""
import hashlib
import json
import os
from pathlib import Path
import shutil
import subprocess
import sys
import unittest

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / 'tools'))
import generate_shaders as g
SOURCE = Path(os.environ.get('FT_NATIVE_SHADER_SOURCE', '/opt/steamvr/resources/shaders/vulkan'))
BUILD = ROOT / 'build' / 'shader-tests'

def dis(path):
    return subprocess.check_output(['spirv-dis', '--raw-id', str(path)], text=True)

class GeneratorTests(unittest.TestCase):
    def test_all_eight_native_variants(self):
        manifest = g.generate(SOURCE, BUILD / 'native')
        self.assertEqual(len(manifest['shaders']), 8)
        self.assertEqual(manifest['pairs'], [[57,125],[57,126],[57,127],[58,128],[58,129],[58,130]])
        for ident, pin in g.MANIFEST['shaders'].items():
            with self.subTest(shader=ident):
                original = SOURCE / pin['filename']
                replacement = BUILD / 'native' / f'{ident}.portal.spv'
                subprocess.run(['spirv-val', '--target-env', 'vulkan1.0', str(replacement)], check=True)
                a, b = dis(original), dis(replacement)
                # Independently compare every original instruction, in order.
                original_lines = [s.strip() for s in a.splitlines() if s.strip() and not s.startswith(';')]
                replaced_lines = [s.strip() for s in b.splitlines() if s.strip() and not s.startswith(';')]
                pos = 0
                for line in original_lines:
                    # Only the block type and entrypoint may append operands.
                    match = next((j for j in range(pos,len(replaced_lines)) if replaced_lines[j]==line or
                                  ('OpTypeStruct' in line or line.startswith('OpEntryPoint ')) and replaced_lines[j].startswith(line+' ')), None)
                    self.assertIsNotNone(match, line)
                    pos = match+1
                self.assertIn('Location 3', b)
                self.assertEqual(a.count('OpImageSample'), b.count('OpImageSample'))
                self.assertEqual(a.count('OpPhi'), b.count('OpPhi'))
                self.assertEqual(a.count('NoPerspective'), b.count('NoPerspective'))
                self.assertEqual(manifest['shaders'][ident]['original_sha256'],pin['sha256'])
                self.assertEqual(manifest['shaders'][ident]['portal_sha256'],hashlib.sha256(replacement.read_bytes()).hexdigest())
                if pin['stage']=='fragment':
                    self.assertLess(b.index('OpKill'), b.index('OpImageSample'))
                    self.assertGreater(b.count('OpVectorTimesScalar'), a.count('OpVectorTimesScalar'))
                for offset in [528,544,560,576,580,584,588]:
                    self.assertIn(f'Offset {offset}', b)

    def test_mismatch_refuses_before_output(self):
        target = BUILD / 'bad-input'
        target.mkdir(parents=True, exist_ok=True)
        for record in g.MANIFEST['shaders'].values():
            shutil.copyfile(SOURCE / record['filename'], target / record['filename'])
        first = next(iter(g.MANIFEST['shaders'].values()))['filename']
        (target / first).write_bytes(b'not a native shader')
        out = BUILD / 'refused'
        if out.exists(): shutil.rmtree(out)
        with self.assertRaisesRegex(ValueError, 'hash'):
            g.generate(target, out)
        self.assertFalse(out.exists())

    def test_wrong_variant_refuses(self):
        target = BUILD / 'swapped-input'
        target.mkdir(parents=True, exist_ok=True)
        records = list(g.MANIFEST['shaders'].values())
        for record in records:
            shutil.copyfile(SOURCE / record['filename'], target / record['filename'])
        shutil.copyfile(SOURCE / records[1]['filename'], target / records[0]['filename'])
        with self.assertRaisesRegex(ValueError, 'hash'):
            g.generate(target, BUILD / 'swapped-output')

if __name__=='__main__':
    unittest.main(verbosity=2)
