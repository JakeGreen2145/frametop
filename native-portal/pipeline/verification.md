# Scoped pipeline verification

Executed only in `/home/steamos/dev/frametop-native-pipeline`, using its existing `dev` container for compilation and GPU execution. No live compositor, OpenVR, service, launch, configuration, camera ownership or pose operations were performed.

## Results

- Initial forwarding-only skeleton failed the actual-entrypoint test at `assert(gen)`, exit 134. This was observed in the tool transcript before implementing the scoped layer.
- A legal null-pipeline-destruction regression failed at the expected approval assertion before the fix. `null-destroy-red.log` preserves that run. The fix prevents null destruction from clearing reserved, in-flight records.
- A shader-error regression failed when teardown attempted to destroy a handle from an unsuccessful module creation. `shader-error-red.log` preserves that run. The fix records only successfully created portal modules.
- Final native AArch64 CPU entrypoint tests passed with ASan and UBSan. They used all eight exact generated native originals and all six valid pairs, plus one-bit mutations. These are fake-next-Vulkan tests, not native shader GPU execution.
- Final synthetic CPU entrypoint tests passed with ASan and UBSan through the same implementation.
- CPU coverage includes whole-batch derivative indices, mixed stock/owned entries, exact caller/stage preservation, stock module-create pointer preservation, wrong stage/name/pair/device/generation, unknown graphics/stage/fixed-function `pNext`, specialization refusal, token mismatch/nesting/cross-thread use, reentrant creation, partial error outputs, success-with-null output, approval after original-module destruction, exact pipeline destruction, bounded registry exhaustion/reclamation, device recreation, explicit revocation, module initialization failure and all eight intercepted submit/wait loss entrypoints.
- The public header compiled as C11 with warnings as errors. The versioned DSO exported the five ABI functions under `FT_PORTAL_PIPELINE_1.0` and reported SONAME `libft_portal_pipeline.so.1`.
- Real loader GPU test passed on `Turnip Adreno (TM) 750`, type 1. Stock and scoped pipelines coexisted on one device using the same original modules. The fixture checked 46 cases and 47,104 pixels, all four channels per pixel.
- Global replacement mutant failed the registry oracle with exit 8: `FAIL registry stock=1 approved=1`.
- Missing-fragment mutant failed the pixel oracle with exit 7: outside pixel x=0,y=0 red 64 instead of 0.
- Claimed-success stock-fallback mutant failed the same pixel oracle with exit 7. These failures were not loader/initialization failures or crashes.

The GPU inputs were owned synthetic shader originals processed by the production transformation, then embedded and substituted by the production layer. The real GPU test never loads native compositor shaders or native camera descriptors. It does not prove optical registration, native-wrapper extraction, live visibility, command resubmission policy or GPU lifetime integration.

## Preserved artifacts

Artifacts are ignored under `native-portal/build/` in both the isolated Frame mirror and the local task worktree. Build logs contain the exact compiler argv. `pipeline-native/hashes.json` records every original/generated binary and embedded-header hash. `artifacts.json` records build output hashes.

| Artifact relative to `native-portal/build/` | SHA256 |
|---|---|
| `pipeline-native/libft_portal_pipeline.so.1` | `f58ff9f972705f10f9b6823b687fc0bced8d86baccf131f71bdc9aec285923a7` |
| `pipeline-synthetic/libft_portal_pipeline.so.1` | `c3fe453f22064190ea7655b081f3881fdeb890d93bbd1a7a88bd073fad4ae3ca` |
| `pipeline-native/VkLayer_FT_scoped_portal.json` | `134b61dd9f633ac245ee0c9a45ad05057010e2bae94c1766c27f5c18e1471b8d` |
| `pipeline-native/bundle.hpp` | `a1c347f23eea339217fb46e2862f98c385544c65e0cd634d02989299e37f4185` |
| `pipeline-native/build-green.log` | `ce168c4bd6ba7796d6191995da9760e2d8bcfe0cbc6eea825cc51483cde73f6d` |
| `pipeline-synthetic/build-green.log` | `f1005068ffea16fe5bb1deac438af2e3db3dba663f81026341711f3d522a2f2d` |
| `pipeline-synthetic/gpu-green.log` | `b9ab2b7fd059a482e1606a215ac49d523006df2ca24976a6332e7b808ca35ab2` |
| `pipeline-synthetic/mutation-run.log` | `4d2ebfbe2c374d75696e89485cb6a2c865cb16a1fb39aaa98b210aa87c4d20f3` |
| `pipeline-synthetic/null-destroy-red.log` | `85d349dabffc291e015f93e4363844b9de84305d5884e49b90df9df1b5a3979a` |
| `pipeline-synthetic/shader-error-red.log` | `e42a837e947d9f60c9d7c534a4bfc6b3aea22b52793732390507665b8c0caf81` |

Local Vulkan development headers were absent, so compilation and sanitizers ran on the explicit ARM mirror rather than installing packages locally. A formatting-induced generated-header include-order error was repaired by making the generated header self-contained, then all final tests above were rerun. No independent reviewer tool was available in this delegated session; parent delta review remains required. No prior accepted shader work was changed or broadly reviewed.
