# B4 caller/blend admission closure

The existing shader-only aperture path had an admission gap, not an incorrect
owned blend selection. At baseline `5d03195`, `Runtime::Impl::stock` rejected the
native opaque camera attachment before `Adapter::enqueue` could clone the packet
and replace its blend pointer with the existing private source-over marker.

## Pinned native proof

Compositor SHA256:
`9b1bd5fe4463592cbc71b2517b9ceb0a0fcd66cd6885a19dca4be0784bf6c265`.
Addresses below are local ELF-relative PCs. Desktop's Ghidra addresses for these
functions include its `0x100000` image-base adjustment.

`blend_native.py` executes the actual instructions with synthetic memory and
explicit allocator, upload and memset boundaries:

- `0x160be8..0x160c28` establishes incoming camera K as x27, frame as x21, policy
  as x23 and the actual camera stack size.
- `0x15ee20` calls real `0x294358` for all five stock blend objects and publishes
  them in K. K+0x90 has attachment `{1,1,7,0,1,7,0,15}`. K+0xa0 has
  `{0,0,0,0,0,0,0,15}`. K+0x98 has `{1,9,1,0,9,1,0,15}` and remains unsupported.
- `0x161d40..0x161ed8` multiplies native alpha, stores it in the native constants,
  executes the upload call boundary, selects K+0xa0 exactly when native alpha is
  1, and K+0x90 otherwise on the ordinary path. The special path selects K+0x98.
  It constructs the final 352-byte prefix and actual enqueue arguments.
- Both sharpen branches and all three format branches yield precisely the six
  legal pairs, VS57/PS125..127 and VS58/PS128..130. Both stock constant wrapper
  fields are established by native instructions at prefix +0x38/+0x58.
- A separate call to the real `0x294358` with the Adapter's descriptor
  `{1,1,3,1,3,0}` yields the existing private source-over attachment, with a
  distinct allocation from every stock object.

The witness executes 36 cases. Synthetic setup selects supported branch inputs;
this is not a full camera-method replay, captured live frame or GPU rendering.

## Minimal production change

Only `live/runtime.cpp` changes behavior. The exact disabled attachment becomes
an additional accepted source for cloning, alongside the existing exact
source-over attachment. All other factors, operations, mask and enable values
still fail. Owned invocation and binding-identity guards are unchanged.

No Adapter, shader, FrameGate, stock blend, native cache or arena policy changes.
`Adapter::enqueue` still copies the final prefix and changes only blend +8 and
wrapper +0x38/+0x58. It always selects its privately allocated source-over marker,
including at portal opacity 1. Partial portal opacity therefore cannot borrow
native disabled blending. Parent's zero-portal-opacity guard remains unchanged.

## Reproduced RED and verified GREEN

The focused runner extracts and compiles the **exact current production**
`stock`, identity comparison and `read_self` bodies. A minimal owner shell supplies
only their referenced fields. It does not copy a separately maintained predicate
or substitute the callback's result. This shell is not the complete live router.
The actual Adapter, Bridge, FrameGate and geometry code execute against the
existing synthetic native-operation boundary.

The instruction-produced prefix, constants and source attachment feed this path.
Only the guest source-blend address is relocated to host-owned fixture storage.

Baseline RED:

```text
ordinary native alpha=1 portal opacity=0.25 VS=57 PS=125 rejected before private clone
Assertion `accepted' failed.
```

GREEN, normal and ASan/UBSan:

```text
72 positive stereo cases, 94 negative cases
zero opacity no records
private blend/prefix/528 bytes/two aligned 592 descriptors PASS
```

Positive cases vary native alpha, portal opacity, legal pair and eye recording
order. Assertions cover unchanged input/state, every preserved prefix byte,
all 528 original constant bytes, private source-over state, both stage descriptor
ranges and 64-byte offsets, and no late descriptor resolution. Negative cases
cover special blending, each attachment word, all six illegal shader cross-pairs,
invalid scope/identity/address, and owned-marker DROP without a stock lookup.
Zero portal opacity produces no upload, record or commit.

Additional ASan/UBSan regressions pass:

- `upstream_test.cpp`, including 10,000 stereo cycles and 20,000 native upload
  boundary calls with one persistent wrapper.
- `stereo_commit_test.cpp`, including the existing Adapter regression families,
  two 10,000-frame bounded-storage runs and exact submission expiry/Off checks.
- `control/frame_test.cpp`, including the parent's zero-opacity guard.

All compilers ran sequentially with `-O0 -g1`, `MemoryMax=256M`,
`MemorySwapMax=0`. The complete production `live/runtime.cpp` translation unit
also compiled with warnings as errors under that cap. No full router suite ran.

## B4 manifest and source-ID proof

`blend_shaders.py` checks all eight original file sizes and SHA256 values against
both the transferred receipt and production `tools/native-shaders.json`. It uses
the actual generator and Khronos tools, not audit-inferred IDs or a custom
validator. All eight generated variants pass `spirv-val --target-env vulkan1.0`.
The existing three generator tests also pass, including wrong-file and
wrong-variant refusal and ordered native-instruction preservation.

The production manifest's VS57 `%371` and VS58 `%385` are verified as matrix
member0 at byte0 multiplied by Location0 input extended with W=1. They precede
member1 eye-space and member2 projection multiplication. RowMajor and
MatrixStride16 decorations are checked. The generator adds exactly the three
XYZ extractions from these same IDs. This closes the missing manifest/ID proof,
not physical raw-space/optical alignment certification.

SPIRV-Tools was absent from PATH. Debian's `spirv-tools_2023.1-2_amd64.deb` was
downloaded and extracted only inside ignored `render/build/blend/tools`, with no
system install. Generated derivatives and native fixture bytes stay private in
ignored build directories.

## Reproduction

From the checkout root:

```sh
export PYTHONPATH=/home/jakeg/.hermes/work/frame-native-product-design/pylib:/home/jakeg/.hermes/work/frame-native-reuse/camera-analysis/pylib
systemd-run --user --scope -p MemoryMax=256M -p MemorySwapMax=0 \
  python3 -B native-portal/render/blend_test.py
systemd-run --user --scope -p MemoryMax=256M -p MemorySwapMax=0 \
  python3 -B native-portal/render/blend_test.py --sanitize
export PATH="$PWD/native-portal/render/build/blend/tools/usr/bin:$PATH"
python3 -B native-portal/render/blend_shaders.py \
  /home/jakeg/.hermes/work/desktop-compositor-re/shader-assets-for-checkpoint06/assets \
  /home/jakeg/.hermes/work/desktop-compositor-re/shader-assets-for-checkpoint06/manifest.json
```

Private reports are `render/build/blend/red.json`, `verification.json`,
`verification-asan.json`, `native-witness.json`, and `shader-verification.json`.

No device, SSH, activation, camera, GPU or live pipeline execution occurred.
Native ownership integration, producer synchronization, target/depth/format
compatibility and live binocular optical validation remain separate obligations.
This closes the reproduced source-blend admission gap and the missing manifest
proof. It does not certify B4 as a whole or declare the portal complete.
