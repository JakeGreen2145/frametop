# Preloaded pipeline dispatch regression

## Defect and fix

The live owner has a `DT_NEEDED` dependency on `libft_portal_pipeline.so.1`.
Preloading the owner can put the layer's exported `vkGetInstanceProcAddr` and
`vkGetDeviceProcAddr` ahead of libvulkan in global lookup. `vkCreateInstance`
still resolves to libvulkan. `-Bsymbolic-functions` protects references inside
the layer, not application references to its exported names.

The regression reproduces three consequences with the actual production layer
DSO and system Vulkan loader:

- Global GIPA/GDPA resolve to the layer while `vkCreateInstance` resolves to the loader.
- Creating an instance through global GIPA fails because the application has no loader layer-link chain.
- Direct loader instance creation followed by global GIPA device creation aborts in the loader. Separately, using loader GIPA for creation but global GDPA for `vkDeviceWaitIdle` skips the test layer above our layer: its call-count delta is zero instead of one.

The only production change removes the three proc-address functions from the
ELF export allowlist. The existing interface-v2 negotiation function still
returns their addresses. All five versioned `ft_pipeline_*` ABI symbols and the
SONAME remain unchanged. No layer ordering, Gamescope configuration, shader,
loader-owner architecture, or service behavior changes.

This follows [Khronos loader/layer interface-v2 negotiation](https://github.com/KhronosGroup/Vulkan-Loader/blob/main/docs/LoaderLayerInterface.md#layer-version-negotiation):
export negotiation and return pointers to the internal instance, device and
physical-device proc-address functions. This layer already refuses interface
versions below two.

## Regression runner

After generating a bundle and building it with the existing scripts:

```sh
python3 native-portal/pipeline/bundle.py --synthetic --output native-portal/build/preload-synthetic
python3 native-portal/pipeline/build.py --output native-portal/build/preload-synthetic
VK_ICD_FILENAMES=/path/to/software-icd.json \
  python3 native-portal/pipeline/test_preload.py --output native-portal/build/preload-synthetic
```

The runner compiles a loader-linked client and a client that loads libvulkan
later through `dlopen`. It runs each without preload, with direct production
pipeline preload, and with a test-only preloaded owner whose checked
`DT_NEEDED` names the production pipeline. Each combination tests symbol
identity, GIPA instance creation, full dispatch, and isolated GDPA dispatch,
with the counter layer above and below the production layer where applicable.
The owner fixture has no constructor or native hooks. The counter layer forwards
to the real loader/ICD and caches intercepted downstream functions during chain
construction. The application never links `layer.cpp` directly.

Only these test child processes suppress unrelated implicit layers. This makes
the fake stacked-layer regression deterministic; it is not a Gamescope workaround.
The runner creates no WSI surfaces, swapchains, VR sessions or service changes.
Select a software ICD explicitly when hardware access is prohibited.

## Executed evidence

Local x86-64 execution used system libvulkan 1.3.239 and the Lavapipe ICD,
reporting `llvmpipe (LLVM 15.0.6, 256 bits)`. Existing synthetic and full native
embedded headers were reused from the earlier pipeline review; no private
shader resources were added to git.

- Original exports: 36 cases, 15 failures. Both baselines pass. The transitive owner reproduces the bug when Vulkan loads later; linking the loader at process startup changes lookup order and masks that case.
- Fixed exports, synthetic bundle: 36 cases, zero failures.
- Fixed exports, full native bundle: 36 cases, zero failures. All successful dispatch cases observe exactly one physical-properties call and one device-wait call, a nonzero production pipeline generation, and successful teardown.
- Existing full-native CPU layer tests pass under ASan/UBSan, including all six shader pairs and the existing fail-closed/lifetime cases. Existing synthetic CPU tests also pass.
- `nm -D --defined-only` confirms only negotiation and the five existing versioned ABI functions are exported, apart from the version node.

Ignored evidence is under `native-portal/build/preload-synthetic/` and
`native-portal/build/preload-native/`: `build-green.log`, `green.log`,
`preload-test/results.json`; the synthetic directory additionally contains
`red.log`, `red-exports.map`, and `preload-test/red-results.json`.

This proves the preload binding defect and its repair with real loader calls.
It does not replay the exact Gamescope swapchain failure, execute native shader
pipelines, or prove that the next ARM compositor launch succeeds. The parent's
independent inert ARM probe found the same mixed symbol ownership with the real
preloaded owner. Rebuild and seal the ARM pipeline/owner bundle before repeating
that probe and any separately authorized guarded live trial. Do not replace a
DSO inside an already sealed bundle.
