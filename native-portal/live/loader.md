# Guarded launch owner

This directory builds a launch-only AArch64 compositor preload, not an app-side
activation path. The runtime router owns render policy. The loader owns the
single-threaded patch transaction and immutable build identity. Nothing here
installs a service, changes `/opt`, restarts SteamVR, or opens a camera.

## Build on the parent's isolated Frame mirror

Merge the loader and router commits first. The router must supply
`live/runtime.cpp`, `live/runtime-call.S`, and `live/runtime-sites.json`.
The production build refuses a missing router or unresolved export. Test stubs
are compiled only into separately named loader acceptance artifacts.

From `/home/steamos/dev/frametop-native-live-trial` inside distrobox `dev`:

```sh
systemd-run --user --scope -p MemoryMax=650M -p MemorySwapMax=0 \
  python3 -B native-portal/live/build.py \
  --elf /opt/steamvr/bin/linuxarm64/vrcompositor \
  --manifest native-portal/live/runtime-sites.json \
  --pipeline native-portal/build/live-pipeline \
  --output native-portal/build/live-owner
```

The `--elf` path must be visible inside that container. An exact private copy is
also accepted. Its required SHA256 is
`9b1bd5fe4463592cbc71b2517b9ceb0a0fcd66cd6885a19dca4be0784bf6c265`.
The installed executable path at activation remains exactly
`/opt/steamvr/bin/linuxarm64/vrcompositor`.

The pipeline directory must be the existing production `pipeline/bundle.py`
and `pipeline/build.py` output. It must contain all eight pinned shaders and
six shader pairs, `portal-shaders.json`, `artifacts.json`, the explicit layer
JSON and `libft_portal_pipeline.so.1`. No synthetic shader bundle or global
shader-directory replacement is accepted. If rebuilding the layer is needed:

```sh
python3 -B native-portal/pipeline/bundle.py \
  --source /opt/steamvr/resources/shaders/vulkan \
  --output native-portal/build/live-pipeline
python3 -B native-portal/pipeline/build.py \
  --output native-portal/build/live-pipeline
```

`build.py` compiles the actual production router, runtime Evidence/recording/
staging/graph translation units, bindings, Adapter, control server and ARM call
bridge. It links the layer with `--no-as-needed`, `$ORIGIN` lookup and `-z defs`.
It runs the separate C loader test, including a synthetic native ARM island,
then checks real exported symbols and `ldd -r` on the sealed owner. No absent
router, absent layer or undefined symbol is a successful build.

The output includes `build-report.json`, `sites.private.json`, `link-check.txt`,
`bundle-path.txt` and a content-addressed `bundle-<full hash>` directory. Bundle
files are mode 0400, its directory is mode 0500, and every file is read back.
The executable contains a source/site/pipeline build ID. `bundle.pin` binds that
ID to the final owner and layer hashes without a self-hash cycle.

## Parent-only activation and rollback

Read the exact sealed bundle's `trial.env`. It specifies:

- `FT_NATIVE_PORTAL_ENABLE`, the compiled 64-character build ID. No default opt-in.
- `FT_NATIVE_PORTAL_BUNDLE_SHA256`, the exact `bundle.pin` hash.
- `FT_NATIVE_PORTAL_SECONDS=60`, accepted range 1 through 120.
- `LD_PRELOAD`, the immutable owner path.
- `VK_ADD_LAYER_PATH`, the immutable bundle directory.
- `VK_INSTANCE_LAYERS=VK_LAYER_FT_scoped_portal`.

The compositor also needs its normal `XDG_RUNTIME_DIR`. The loader calls the
strong `ft_native_portal_start(base, elf_sha256, XDG_RUNTIME_DIR)` export after
all text pages return to RX and before returning to native startup. This starts
the existing private control server. There is no render-on action in the loader.
Normal native intent/control-client policy remains required for owned draws.

Only the parent should merge those values into a temporary, uniquely named
`steamvr.service.d` drop-in for the authorized trial. Preserve existing preload
and Vulkan layer entries. Do not put the variables in shell-wide startup files
or the desktop app. Remove conflicting `FT_NATIVE_HOOKS_ENABLE`,
`FT_AURORA_GATE_ENABLE`, `FT_NATIVE_CLIP_DIR`, and `FT_CAMERA_BLEND_MODE` opt-ins
for the trial. The loader refuses any of these being present, even with an empty
value. Keep the prior service environment and selected desktop command for
rollback. Do not modify or replace the existing observer bundle.

**TTL ends the trial process, not just routing.** The first hooked callback at or
after the BOOTTIME deadline exits the compositor with status 190. Silently
turning the router off could forward an already-recorded owned packet as stock.
A separate parent watchdog must remove the temporary drop-in before normal
restart, including when native code stalls and no further callback arrives.
Postpatch RX failure, rollback-protection failure, or router-start failure also
exits with status 190. Precommit failure does not install any hook words.

Rollback is removal of only that trial drop-in, daemon-reload and the parent's
already-authorized restart. Verify the actual executable/environment, control
socket absence and restored desktop separately. These tools do not perform
rollback or any live service operation themselves.

## Router ABI and site manifest

```cpp
extern "C" void ft_native_portal_event(
    uint64_t relative_pc, native_portal::arena::Saved *saved) noexcept;
extern "C" bool ft_native_portal_start(
    uint64_t base, const char *sha256, const char *runtime_directory) noexcept;
```

`Saved` is the existing 800-byte x0..x30/SP/Q0..Q31/NZCV/FPCR/FPSR/pad ABI.
The loader also exports `uint64_t ft_native_portal_image_base(void)`.
No callback reaches router state in a fork child. There is no event log, worker
queue or allocation in the loader callback.

Manifest example:

```json
{
  "elf_sha256": "9b1bd5fe4463592cbc71b2517b9ceb0a0fcd66cd6885a19dca4be0784bf6c265",
  "sites": [
    {"name": "renderer-created", "pc": "0x7fda0", "emitter": "observer"},
    {"name": "upload", "pc": "0x161d64", "emitter": "arena"},
    {"name": "enqueue", "pc": "0x161ed8", "emitter": "enqueue"}
  ]
}
```

Supported emitters are `observer`, `arena`, `pass`, `lifecycle`, and `enqueue`.
The specialized emitters only accept their audited PCs and infer local indices.
Optional numeric `kind`, `word`, `guard` hex and `guard_sha256` values must match
the pinned ELF. Optional `sources` is an additive list of production `.cpp` or
`.S` paths relative to `native-portal`, never tests, fixtures or stubs.

Preparation unions the existing observer/core Evidence sites, arena sites,
pass sites and lifecycle sites with the runtime manifest. It sorts and
coalesces each PC, retaining the specialized emitter rather than installing two
branches at an overlap. Conflicting specialized emitters, guard bytes, kinds or
explicit continuation annotations are refused. All overlapping 16-byte windows
are checked before *any* text write. Core emitters preserve native BL/BLR LR,
all registers and flags. Lifecycle ADRP relocation includes the real load bias.
Unsupported literal loads and test-bit branches are refused rather than copied
with a false PC. Generated private guards never enter git.

## Offline verification

On this worker, with the existing Unicorn/ELF Python dependencies:

```sh
systemd-run --user --scope -p MemoryMax=650M -p MemorySwapMax=0 \
  python3 -B native-portal/live/loader_test.py
systemd-run --user --scope -p MemoryMax=650M -p MemorySwapMax=0 \
  python3 -B native-portal/live/bundle_test.py
```

Pass `--manifest path/to/runtime-sites.json` to the first command to include the
router's additional sites. Host loader tests use explicitly separate callback
stubs. Bundle tests use disposable, labelled synthetic host DSOs. Neither can
serve as a production build. The test runner refuses an uncapped cgroup.

The host lacked OpenSSL headers. They were extracted from Debian's
`libssl-dev_3.0.17-1~deb12u2_amd64.deb` into ignored
`native-portal/build/live-loader-test/deps`, with no system installation.
For that host only, prefix the above commands with:

```sh
export CPATH="$PWD/native-portal/build/live-loader-test/deps/usr/include:$PWD/native-portal/build/live-loader-test/deps/usr/include/x86_64-linux-gnu"
export LIBRARY_PATH="$PWD/native-portal/build/live-loader-test/deps/usr/lib/x86_64-linux-gnu"
```

Verified baseline results: 89 merged sites, 32 text pages, 720 actual generated
instruction replays across three load biases and both conditional outcomes.
All supported arena/pass selector routes preserve full machine state. Tests
exercise exact ELF hash/map identity, missing/mismatched pins, default-off and
wrong-executable real preload, fork and TTL, every precommit protection failure,
fatal postpatch/rollback failures, immutable bundle readback and loaded owner/
layer hash acceptance and tamper refusal. These are CPU/loader results, not live
stereo pixels or optical alignment evidence. The final ARM owner link and live
activation belong to the parent's isolated Frame trial.
