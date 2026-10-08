#!/usr/bin/env python3
"""ARM-only local build and synthetic acceptance. Never installs or launches SteamVR."""
import hashlib, json, os, pathlib, platform, shutil, struct, subprocess
R=pathlib.Path(__file__).resolve().parent
if platform.machine()!='aarch64':raise SystemExit('Build only on ARM in distrobox dev.')
os.chdir(R);os.umask(0o077)
B=R/'build';B.mkdir(exist_ok=True);(B/'tmp').mkdir(exist_ok=True)
os.environ['TMPDIR']=str(B/'tmp')
flags=['-O2','-Wall','-Wextra','-Werror','-Wno-misleading-indentation','-mno-outline-atomics','-fno-stack-protector']
def run(args,**kwargs):
    print('$',' '.join(map(str,args)),flush=True)
    return subprocess.run(args,check=True,timeout=90,**kwargs)
run(['cc','--version'])
run(['cc',*flags,'test_emit.c','-o','build/test_emit'])
with (B/'emitted.json').open('w') as f:run(['build/test_emit'],stdout=f)
run(['cc',*flags,'-Wno-unused-function','-Wno-unused-variable','test_native.c','-pthread','-lcrypto','-ldl','-o','build/test_native'])
run(['cc',*flags,'-fPIC','-fvisibility=hidden','-ftls-model=initial-exec','-shared','-Wl,-z,relro,-z,now,-z,noexecstack','-Wl,-Bsymbolic-functions','observer.c','-pthread','-lcrypto','-ldl','-o','build/libft_native_hooks.so'])
with (B/'native-test.txt').open('w') as f:run(['build/test_native'],stdout=f,stderr=subprocess.STDOUT)
with (B/'drain-test.txt').open('w') as f:run(['python3','-B','test_drain.py'],stdout=f,stderr=subprocess.STDOUT)
# Default-off and opt-in unsupported-executable cases run /usr/bin/true, not a VR process.
directory=B/'preload-test';directory.mkdir(mode=0o700,exist_ok=True)
log=directory/'disabled.bin';bad=directory/'wrong-exe.bin'
for p in (log,bad):
    if p.exists():p.unlink()
env=os.environ.copy();env['LD_PRELOAD']=str(B/'libft_native_hooks.so');env.pop('FT_NATIVE_HOOKS_ENABLE',None);env['FT_NATIVE_HOOKS_LOG']=str(log)
run(['/usr/bin/true'],env=env);assert not log.exists()
env['FT_NATIVE_HOOKS_ENABLE']='metadata-v1-9b1bd5fe';env['FT_NATIVE_HOOKS_LOG']=str(bad)
run(['/usr/bin/true'],env=env)
data=bad.read_bytes();assert len(data)==2384 and struct.unpack_from('<4I',data)==(0x3154464e,2384,0xffffffff,10)
with (B/'native-test.txt').open('a') as f:f.write('PASS default-off-and-wrong-exe\n')
run(['readelf','-d','build/libft_native_hooks.so'])
run(['ldd','build/libft_native_hooks.so'])
run(['nm','-D','--defined-only','build/libft_native_hooks.so'])
files=['libft_native_hooks.so','test_native','test_emit','emitted.json','native-test.txt','drain-test.txt']
(B/'SHA256SUMS').write_text(''.join(hashlib.sha256((B/name).read_bytes()).hexdigest()+'  '+name+'\n' for name in files))
print((B/'native-test.txt').read_text(),end='');print((B/'SHA256SUMS').read_text(),end='')
artifacts={'libft_native_hooks.so':B/'libft_native_hooks.so','decode.py':R/'decode.py','pins.h':R/'pins.h'}
hashes={name:hashlib.sha256(path.read_bytes()).hexdigest() for name,path in artifacts.items()}
identity=hashlib.sha256(json.dumps(hashes,sort_keys=True).encode()).hexdigest()
bundle=B/('bundle-'+identity[:16])
if not bundle.exists():
    bundle.mkdir(mode=0o700)
    for name,path in artifacts.items():shutil.copyfile(path,bundle/name);(bundle/name).chmod(0o400)
    (bundle/'manifest.json').write_text(json.dumps({'schema':1,'elf_sha256':'9b1bd5fe4463592cbc71b2517b9ceb0a0fcd66cd6885a19dca4be0784bf6c265','bundle_id':identity,'artifacts':hashes,'source_sha256':{p.name:hashlib.sha256(p.read_bytes()).hexdigest() for p in R.iterdir() if p.is_file()}},indent=2)+'\n')
    (bundle/'manifest.json').chmod(0o400);bundle.chmod(0o500)
for name,digest in hashes.items():assert hashlib.sha256((bundle/name).read_bytes()).hexdigest()==digest
(B/'bundle-path.txt').write_text(str(bundle)+'\n')
print('BUNDLE',bundle)
