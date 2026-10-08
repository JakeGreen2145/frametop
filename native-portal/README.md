# Native portal math and offline shaders

This directory builds a pure C++ aperture library and generates portal-only variants of eight pinned, user-installed SteamVR shaders. It has no runtime hooks, camera acquisition, Vulkan layer, compositor control, services, GUI, or live activation. Linking the library or running the generator does not enable a portal.

The MIT repository contains our generator, hashes, library and tests only. Valve binaries and disassembly are private generated build artifacts. Do not commit or redistribute `build/`, original shaders, patched shaders or their assembly.

## C++ integration

Link `ft-aperture` and include `ft_aperture.hpp`. All mathematics uses column vectors with row-major `Mat4::v` memory and Standing-space metres. `Box::quaternion` is `[x,y,z,w]`, Standing-from-box. Do not normalize malformed input silently.

1. Latch one authoritative box geometry and lease/origin generation for both eyes.
2. For each owned draw, supply its exact `raw_to_standing`, `raw_to_eye` M1 and `projection` M2, with valid tracking. The same draw's native constant bytes contain row-major M1 at offset 64 and M2 at 128. Convert float32 elements into `Mat4` doubles without transposing. Do not substitute an independently sampled pose or a fixed floor offset.
3. Call `compute(Draw)`. On error, drop the owned draw. The result has a zero, invalid extension. `minimum_plane_distance` defaults to 0.01 m for numerical exclusion, not a validated comfort threshold. Opacity must be finite and within 0..1.
4. Call `snapshot(native528, result)` to copy the unchanged native bytes plus the 64-byte extension. The supported upload targets are little-endian IEEE float32 systems, tested on x86-64 and AArch64. Copy into renderer-owned, frame-safe storage. This function does not allocate GPU resources or establish buffer lifetime.
5. A future owner-aware backend must select both modules from one accepted pair, bind the same 592-byte snapshot to set 0, VS binding 0 and PS binding 4, and use a separate portal pipeline identity. Keep the stock modules, 528-byte UBO and native pipeline available. Never globally swap all camera shaders.

`top_face(Box, Rectangle&)` preserves the existing box's +localY face:

```
C = box.center + R * [0, size.y/2, 0]
U = R * [size.x/2, 0, 0]
V = R * [0, 0, -size.z/2]
```

`compute` validates finite geometry, positive extents, quaternion norm, rigid orientation-preserving raw-to-Standing, affine nonsingular M1, central full-rank M2, rectangle nondegeneracy and eye-to-plane distance. M1 can contain scale or shear; its inverse is not assumed to be a transpose. Projection modes that do not send the eye origin to homogeneous x=y=w=0 are refused. One-sided requests also reject the back face. Coefficients that overflow or require float32 denormals are refused.

The raw eye is `inverse(M1) * [0,0,0,1]`. Geometry is transformed by the inverse of that draw's raw-to-Standing. The three Q rows map raw `[X.xyz,1]` to the homogeneous aperture coordinates. `evaluate` and `contains` expose the CPU reference predicate. Error codes distinguish tracking, geometry, origin, eye, projection, near-plane, facing, opacity and numerical failures. The API is allocation-free and has no global mutable state.

### Appended UBO ABI

| Byte offset | Field |
|---:|---|
| 0..527 | Native bytes, unchanged |
| 528 | float4 Q0 |
| 544 | float4 Q1 |
| 560 | float4 Q2 |
| 576 | float opacity |
| 580 | uint32 epoch low |
| 584 | uint32 epoch high |
| 588 | uint32 flags, bit 0 valid, bit 1 CPU-validated one-sided |

Total size is 592 bytes. Epoch is diagnostic identification, not a revocation clock. This library does not authenticate, arbitrate or expire leases. Those checks, stereo latching, freshness, pipeline ownership, source-over blend selection, submission suppression and GPU retirement belong to the future backend. Never treat a math success or a matching UBO size as proof of ownership.

## Shader generation

Requirements are Python 3.9+, `spirv-dis`, `spirv-as` and `spirv-val`. The generator requires the exact hashes in `tools/native-shaders.json`. A SteamVR update or swapped native variant refuses generation rather than guessing offsets.

From this directory:

```sh
python3 -B tools/generate_shaders.py
# Or read a private directory containing the same installed filenames:
python3 -B tools/generate_shaders.py --source /private/native-shaders --output build/shaders
```

Generation validates all eight original hashes before writing. Output contains `<id>.original.spv`, `<id>.portal.spv`, private patched assembly and `portal-shaders.json`. The result manifest is written only after all eight originals and replacements pass Vulkan 1.0 SPIR-V validation and structural preservation checks. Run one generator writer per output directory. A future bundle loader must verify its accepted result manifest and every replacement hash; this directory supplies no loader.

The manifest records exact original/replacement SHA256, stage, entrypoint and binding. The only supported pairs are VS 57 with PS 125, 126 or 127, and VS 58 with PS 128, 129 or 130. Output modules retain `VS_MAIN` or `PS_MAIN`.

Both vertex variants export smooth vec3 aperture Q at location 3. Sharpen's location 2 remains untouched. NoPerspective interpolation is incorrect. The VS evaluates Q from native raw X without changing M0..M3, clip position, native mesh warp, camera UVs, sharpen weights or tint.

Every PS gets a new entry block before its original first label, preserving original Phi predecessors. It discards invalid flags, NaN/Inf Q, q.z<=0 and points outside `abs(q.x)<=q.z && abs(q.y)<=q.z` before native sampling. It also rejects invalid opacity. Every original instruction remains in order, except that the entrypoint and UBO type append operands. The original final color stores remain, followed by multiplication of all four premultiplied components by portal opacity. Native alpha/fade/tint fields and exceptional black outputs are not repurposed.

## Verification

Portable CPU tests need only CMake and C++17:

```sh
cmake -S . -B build/cpu -DCMAKE_BUILD_TYPE=Debug
cmake --build build/cpu -j2
ctest --test-dir build/cpu --output-on-failure
```

On a host with the installed shaders and SPIR-V tools:

```sh
python3 -B tests/test_generator.py
# For private originals, set FT_NATIVE_SHADER_SOURCE=/private/native-shaders.
python3 -B tests/gpu_shaders.py
cmake -S . -B build/cmake -DFT_PORTAL_GPU_TESTS=ON -DCMAKE_BUILD_TYPE=Debug
cmake --build build/cmake -j2
ctest --test-dir build/cmake --output-on-failure
python3 -B tests/test_gpu_mutations.py -- build/cmake/test-gpu
```

The GPU executable refuses a CPU Vulkan device. It uses owned minimal VS/PS, processes them through the identical production assembly patcher, binds the real appended UBO at both stage bindings, and checks every offscreen pixel against the CPU oracle. Cases include unequal clip W, translation, rotation, opacity, native premultiplication/fades, invalid flags and each nonfinite Q component. Mutation controls must fail the pixel oracle for unpatched shaders, NoPerspective varyings and alpha-only opacity. A Vulkan setup error is not accepted as a successful mutation rejection.

### Isolated Frame reproduction

Do not use the default `scripts/sync.sh` destination or an active checkout. Copy this directory only into the separate test root. These commands do not activate anything:

```sh
# From the PC repository root:
ssh frame 'mkdir -p /home/steamos/dev/frametop-native-shaders'
rsync -a --exclude build --exclude __pycache__ native-portal/ \
  frame:/home/steamos/dev/frametop-native-shaders/native-portal/

# Run on Frame, inside the isolated native-portal directory:
cd /home/steamos/dev/frametop-native-shaders/native-portal
python3 -B tests/test_generator.py
python3 -B tests/gpu_shaders.py
~/.local/bin/distrobox enter dev -- bash -lc \
  'cd /home/steamos/dev/frametop-native-shaders/native-portal && cmake -S . -B build/cmake -DFT_PORTAL_GPU_TESTS=ON -DCMAKE_BUILD_TYPE=Debug && cmake --build build/cmake -j2 && ctest --test-dir build/cmake --output-on-failure'
python3 -B tests/test_gpu_mutations.py -- \
  /home/steamos/.local/bin/distrobox enter dev -- \
  /home/steamos/dev/frametop-native-shaders/native-portal/build/cmake/test-gpu
```

The verified Frame has SPIR-V tools on the host and Vulkan headers/compiler in the dev container. No package install or shell sudo is required there.

### Evidence and limits

CPU RED first failed the missing compute result. Generator RED failed all three test methods before implementation. The unpatched GPU control failed at the first outside pixel. Subsequent native ARM and x86-64 CPU runs, SPIR-V validation and Adreno GPU tests passed. `tests/verification.md` records the final measured counts and exits.

GPU pixels here are synthetic shader/UBO/interpolation/blending evidence. They are not native-camera descriptor integration, physical camera alignment, timing, optical registration, native pipeline rendering or comfort proof. Native acquisition arbitration, callback/lifetime validation and live activation remain outside this change.
