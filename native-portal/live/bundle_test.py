#!/usr/bin/env python3
# SPDX-License-Identifier: MIT
"""Immutable-bundle tests with explicitly synthetic host DSOs, never a product build."""
import ctypes
import hashlib
import json
import os
from pathlib import Path
import shutil
import subprocess
import tempfile
import unittest
import bundle
import prepare
ROOT=Path(__file__).resolve().parents[1]
B=ROOT/'build/live-loader-test'
def sha(p):return hashlib.sha256(Path(p).read_bytes()).hexdigest()
def run(args):subprocess.run(list(map(str,args)),check=True,timeout=180)

class Bundles(unittest.TestCase):
    def test_sealing_readback_and_runtime_pin(self):
        with tempfile.TemporaryDirectory(prefix='synthetic-bundle-',dir=B) as temporary:
            out=Path(temporary);pipeline=out/'pipeline';pipeline.mkdir()
            try:
                # Deliberate synthetic boundaries. Production build.py rejects
                # this directory because it has no real shader inventory.
                layer=pipeline/'libft_portal_pipeline.so.1'
                source=out/'synthetic-layer.c';source.write_text('#include <stdint.h>\nuint64_t ft_pipeline_generation(void*p){(void)p;return 0;}\n')
                run(['cc','-shared','-fPIC',source,'-Wl,-soname,libft_portal_pipeline.so.1','-o',layer])
                (pipeline/'VkLayer_FT_scoped_portal.json').write_bytes((ROOT/'pipeline/VkLayer_FT_scoped_portal.json').read_bytes())
                (pipeline/'portal-shaders.json').write_text('{"test_only_synthetic":true}\n')
                prepare.prepare(prepare.ELF,out)
                (out/'loader-build.h').write_text('#define FT_BUILD_ID "'+('a'*64)+'"\n#define FT_PIPELINE_SHA "'+sha(layer)+'"\n')
                source=out/'synthetic-router.c';source.write_text('#include <stdint.h>\n#include <unistd.h>\n_Bool ft_native_portal_start(uint64_t b,const char*s,const char*d){(void)b;(void)s;(void)d;_exit(89);}\nvoid ft_native_portal_event(uint64_t pc,void*p){(void)pc;(void)p;_exit(89);}\n')
                owner=out/'libft_native_portal_live.so'
                run(['cc','-std=c11','-O1','-shared','-fPIC','-DFT_LOADER_TEST','-Wl,-z,defs','-Wl,-rpath,$ORIGIN','-I',out,
                     ROOT/'live/loader.c',source,*[B/f'emit-{i}.o' for i in range(4)],'-Wl,--no-as-needed',layer,'-lcrypto','-ldl','-o',owner])
                report={'build_id':'a'*64,'owner_sha256':sha(owner),'pipeline_sha256':sha(layer),'sites_sha256':sha(out/'sites.private.json'),'test_only_synthetic':True}
                (out/'build-report.json').write_text(json.dumps(report))
                sealed=bundle.create(out,pipeline);self.assertEqual(bundle.create(out,pipeline),sealed)
                manifest=json.loads((sealed/'manifest.json').read_text())
                for name,hash_ in manifest['artifacts'].items():self.assertEqual(sha(sealed/name),hash_)
                self.assertEqual(sha(sealed/'bundle.pin'),manifest['bundle_id'])
                dll=ctypes.CDLL(str(sealed/'libft_native_portal_live.so'))
                check=dll.ft_loader_test_bundle_ok;check.restype=ctypes.c_int
                old=os.environ.get('FT_NATIVE_PORTAL_BUNDLE_SHA256')
                try:
                    os.environ.pop('FT_NATIVE_PORTAL_BUNDLE_SHA256',None);self.assertEqual(check(),0)
                    os.environ['FT_NATIVE_PORTAL_BUNDLE_SHA256']='0'*64;self.assertEqual(check(),0)
                    os.environ['FT_NATIVE_PORTAL_BUNDLE_SHA256']=manifest['bundle_id'];self.assertEqual(check(),1)
                    (sealed/'bundle.pin').chmod(0o600);self.assertEqual(check(),0)
                    (sealed/'bundle.pin').chmod(0o400);self.assertEqual(check(),1)
                    sealed.chmod(0o700);self.assertEqual(check(),0)
                    sealed.chmod(0o500);self.assertEqual(check(),1)
                    # Mutate a checked manifest byte, not an executing DSO.
                    pin=sealed/'bundle.pin';before=pin.read_bytes();pin.chmod(0o600);pin.write_bytes(before.replace(b'schema=1',b'schema=2'));pin.chmod(0o400)
                    self.assertEqual(check(),0)
                    with self.assertRaises(ValueError):bundle.create(out,pipeline)
                    pin.chmod(0o600);pin.write_bytes(before);pin.chmod(0o400);self.assertEqual(check(),1)
                finally:
                    if old is None:os.environ.pop('FT_NATIVE_PORTAL_BUNDLE_SHA256',None)
                    else:os.environ['FT_NATIVE_PORTAL_BUNDLE_SHA256']=old
                print('PASS synthetic immutable bundle/hash/loaded-owner-and-layer pin acceptance and tamper refusals')
            finally:
                for p in out.glob('bundle-*'):
                    if p.is_dir():p.chmod(0o700)

if __name__=='__main__':unittest.main(verbosity=2)
