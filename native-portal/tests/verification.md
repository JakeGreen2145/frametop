# Verification record

All Frame compilation and GPU execution used `/home/steamos/dev/frametop-native-shaders`. No active checkout, prototype, runtime hook, service, setting, pose or camera ownership state was changed.

## Observed results

| Check | Result |
|---|---|
| Initial C++ skeleton against aperture tests | Expected exit 1 at missing successful compute result |
| Initial generator skeleton | Expected exit 1, 3 failed test methods |
| Float32 underflow regression before fix | Expected exit 1 for an unrepresentable Q2 coefficient |
| Float32 denormal regression before fix | Expected exit 1 for unsupported denormal Q2 |
| x86-64 CPU, GCC 12.2, ASan + UBSan | Exit 0, 60,130 checks, including 20,000 ray cases |
| x86-64 sanitizer CTest | Exit 0, 1/1 tests passed |
| Native AArch64 CPU, GCC 16.2.1 | Exit 0, the same 60,130 checks and 20,000 ray cases |
| Native AArch64 CTest with GPU enabled | Exit 0, 2/2 tests passed |
| Installed-shader CLI generator | Exit 0, all 8 pinned variants generated |
| Generator Python suite | Exit 0, 3/3 methods, all 8 variant subtests |
| Original native SPIR-V Vulkan 1.0 validation | 8/8 distinct originals passed |
| Generated native SPIR-V Vulkan 1.0 validation | 8/8 distinct replacements passed |
| Ordered original-instruction/interface preservation | 8/8 replacements passed |
| Owned synthetic SPIR-V generation/validation | Both VS and PS passed through the production patcher |
| Offscreen GPU | Exit 0, Turnip Adreno 750, 23 cases, 23,552 checked pixels |
| GPU mutation suite | Exit 0, 3/3 bad shader controls rejected by actual pixel comparisons |

Every native shader input matched the exact SHA256 in `tools/native-shaders.json`. The generator tests independently compared every original instruction in order, allowing only appended UBO-type and entrypoint operands. They checked appended offsets, unchanged sampling/Phi/NoPerspective instruction counts, early discard, opacity multiplication and replacement hashes. Tampered bytes and a different valid native variant were refused before successful generation.

## GPU controls

The unpatched baseline failed with exit 7 at outside pixel x=0,y=0. It wrote red 64 where the clear target required 0.

The paired NoPerspective mutation failed with exit 7 at x=3,y=1 in the unequal-clip-W case. It wrote red 64 instead of 0. This proves the fixture detects the interpolation error rather than testing a constant-W special case.

The alpha-only opacity mutation failed with exit 7 at x=8,y=8 in the opacity-zero case. It wrote red 64 instead of 0. The correct implementation multiplies every premultiplied RGBA component.

The 23 passing cases cover uniform/unequal clip W, zero/quarter/full opacity, translated and rotated Q, behind/zero/crossing eye plane, invalid/one-sided flags, diagnostic epoch fields, NaN and Inf in each Q component, invalid opacity, native black and native fade. Each case checks all 1,024 pixels and all four channels against a CPU reference with a one-byte UNORM tolerance. The target has premultiplied blue and alpha 0.25 so alpha errors remain observable after source-over blending.

## Scope

These are synthetic shader/UBO/interpolation/blending checks, not camera optical proof. Native camera descriptors were not constructed, native camera pipelines were not rendered, and no live activation was attempted. Ownership arbitration, lease policy, native callback lifetime, tracking freshness, physical registration and comfort remain integration gates.

Local sanitizer compilation and real Frame output underpin this record. No independent reviewer tool was available in this subagent session; parent review remains separate from these executed tests. Reproduction commands are in `../README.md`.
