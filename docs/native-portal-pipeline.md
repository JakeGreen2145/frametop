# Scoped native portal pipeline layer

This explicit Vulkan layer preserves the application's original shader modules. It substitutes paired portal VS/PS modules only during a trusted, checked native pipeline-lookup scope. It does not change blend state, discover native wrapper offsets, install a service, or enable itself.

The renderer adapter must authenticate the recorded ticket and immutable native source-over blend marker before opening a scope. Shader identity alone is not ownership. The native blend marker supplies a separate native pipeline-cache key. This layer neither constructs nor rewrites that blend object.

## In-process ABI version 1

Header: `native-portal/pipeline/ft_pipeline.h`. DSO SONAME: `libft_portal_pipeline.so.1`. Export version: `FT_PORTAL_PIPELINE_1.0`.

| Function | Contract |
|---|---|
| `ft_pipeline_generation(device)` | Returns the live, nonzero layer generation only when every embedded portal module was created successfully. Zero means unavailable or revoked. Bind this value to the renderer's own generation at initialization. |
| `ft_pipeline_begin(device, generation, vertex_id, fragment_id)` | Returns a never-reused, thread-bound scope token. Requires a current generation and one exact generated pair. Zero means DROP, not permission to perform an unscoped owned lookup. |
| `ft_pipeline_approved(device, generation, pipeline, vertex_id, fragment_id, out)` | Returns 1 only for an observed successful paired substitution. `out` may be null. Otherwise it receives an ABI-tagged record containing generation, pair IDs, original and portal module handles, and the actual `VkPipeline`. It is zeroed on refusal. |
| `ft_pipeline_end(token)` | Only the matching thread/token clears its scope. Returns 0 for poisoned scopes. A mismatched token poisons an existing scope without clearing it. A nested begin also poisons the outer scope. |
| `ft_pipeline_revoke(device, generation)` | Permanently revokes the matching device generation and every approval. Stale revocation cannot affect a newer generation. Normal Vulkan device recreation is required to recover. |

`ft::pipeline::Scope` supplies noncopyable C++ RAII begin/end. No IPC or untrusted capability-token interpretation exists. These functions are for already trusted code in the same process. Do not expose them to an external client or treat a token as a ticket authenticator.

The renderer wrapper must follow this sequence on every owned lookup, including native cache hits:

1. Authenticate the recorded packet ticket, blend marker, original pair, renderer/device generation, descriptors, and deadline.
2. Begin the scope with the saved layer generation and expected original pair. On zero, drop without calling native lookup.
3. Call the original native lookup with unchanged native arguments.
4. Extract its actual `VkPipeline` through a separately checked native-wrapper contract. This layer does not guess the native object layout.
5. Query approval while the scope remains active. An unknown or nonzero-but-unapproved handle is DROP. Never retry as stock.
6. End the matching token on every exit path. For a C wrapper, require both successful approval and a clean end. Use RAII or an equivalent cleanup guard for exceptions.

Querying without a scope supports diagnostics and cache-registry tests; it does not authenticate a draw. Approval is a snapshot, not a lifetime lease. The renderer still owns Vulkan external synchronization, device lifetime, pipeline lifetime, submission admission, and expiry. Do not unload the DSO with live devices or native callbacks.

The supported original pairs are the six entries in `tools/native-shaders.json`: VS57 with PS125/126/127, and VS58 with PS128/129/130. Their names are exactly `VS_MAIN` and `PS_MAIN`. All eight originals remain available unchanged.

## Substitution and refusal

`bundle.py` calls the existing production shader generator. That generator pins installed original bytes, validates all original and generated SPIR-V, preserves original instructions, and appends the existing 528-to-592-byte interface at binding 0 VS, binding 4 PS and smooth location 3. The private build header embeds each exact original and its own generated replacement. There is no runtime arbitrary-file loader or hash-only recognition shortcut. Module recognition compares the complete original byte array and length after forwarding the application's original shader-create call unchanged.

Device creation prepares the portal modules once through the next Vulkan dispatch. Any missing portal module makes scopes unavailable. Shader-create and original module-destroy calls retain the application's handles and allocation callbacks. Portal modules have their own null allocator and remain device-owned until device destruction.

An unscoped `vkCreateGraphicsPipelines` forwards the exact input pointer, complete batch, cache, allocation callbacks, outputs and result. No camera exception exists on that path.

Inside a scope, an entry containing either recognized original camera module is an owned candidate. Both stages must match the scope's exact original pair, stage type, entrypoint and device. Entries containing no recognized camera modules remain unrelated stock entries and receive no approval. This permits mixed native batches without treating unknown shaders as portal content.

For accepted candidates the layer copies only each graphics create structure and its two stage structures. It changes the two module handles and the copied `pStages` pointer. It retains layout, render pass, raster/depth/blend state, derivative flags, base pipeline handle/index, stage order, entrypoint pointers and all other caller bytes. The complete original batch order reaches the next function in one call. Native derivative indices therefore keep their original meaning.

Owned candidates refuse any graphics, stage, or referenced fixed-function-state `pNext`, any stage flags, specialization info, unsupported stage count/type, unrecognized/missing partner, wrong pair or generation. No extension allowlist has been verified for the pinned native path. Native graphics flags are preserved, not rewritten.

The scope is poisoned on an owned refusal, nested/reentrant creation, wrong device, downstream error, or success with a null owned output. Approval is published only after `VK_SUCCESS`, nonnull owned outputs, successful paired substitution and a still-valid generation/scope. On a scoped batch failure, every nonnull result in that transaction is destroyed with the original allocator and all outputs are cleared. This includes stock entries in a failed mixed batch because derivative relationships must not escape a partially accepted transaction. Existing pipelines from earlier successful transactions remain intact. There is no stock fallback.

Registry storage is reserved before forwarding. Bounds are 16 instances, 16 devices, 1,024 recognized original modules per device, 4,096 approved pipelines per device and 64 pipeline entries per scoped batch. Fixed copies live on the stack. Device storage and portal modules are allocated at device creation, not on the creation interception path. Short mutex-protected scans are bounded; this is not a measured nonblocking real-time guarantee. Recognition-table exhaustion permanently revokes that generation. Registry exhaustion rejects the new transaction before the downstream call.

`vkDestroyPipeline` removes the exact device/handle record before forwarding. Destroying `VK_NULL_HANDLE` does not clear in-flight reservations. Destroying original shader modules removes future recognition but correctly retains approval of already-created pipelines. Device destruction and explicit revocation clear all approvals. Observed `VK_ERROR_DEVICE_LOST` from module/pipeline creation, `vkQueueSubmit`, core/KHR `vkQueueSubmit2`, queue/device idle, fence waits and core/KHR semaphore waits revokes the generation. The native renderer must call `ft_pipeline_revoke` on reset and on loss observed through any other Vulkan operation. Revocation does not itself destroy a device or pretend that submitted work has retired.

## Isolated build and test

No CMake, render adapter, hooks, controls, shader generator, screens, or launch configuration is changed by this lane. The committed explicit-layer manifest is only a template copied beside a private DSO. There is no installer and no implicit-layer registration. Do not copy it to a loader search directory or add compositor launch variables without the parent's separate integration authorization.

The executed Frame mirror is `/home/steamos/dev/frametop-native-pipeline`. Shader tools are on the host; C++/Vulkan headers and the GPU driver are in its existing `dev` container.

```sh
cd /home/steamos/dev/frametop-native-pipeline
python3 -B native-portal/pipeline/bundle.py \
  --output native-portal/build/pipeline-native
python3 -B native-portal/pipeline/bundle.py --synthetic \
  --output native-portal/build/pipeline-synthetic

$HOME/.local/bin/distrobox enter dev -- sh -c '
  set -e
  cd /home/steamos/dev/frametop-native-pipeline
  ulimit -c 0
  python3 -B native-portal/pipeline/build.py \
    --output native-portal/build/pipeline-native --sanitizer
  python3 -B native-portal/pipeline/build.py \
    --output native-portal/build/pipeline-synthetic --sanitizer
  python3 -B native-portal/pipeline/test_gpu.py \
    --output native-portal/build/pipeline-synthetic
'
```

The native build tests actual exported loader/layer entrypoints through synthetic loader link structures and a fake next Vulkan boundary. It separately exercises every generated native pair and rejects one-bit-different originals. C11 header compilation verifies the C ABI. ASan and UBSan cover the CPU entrypoint tests.

The GPU build uses an explicitly synthetic bundle, made by the same production assembly transform and embedded by the same bundle builder. The real loader negotiates and loads the same production layer implementation as a DSO. Its instance is created through a proc-address function obtained from an explicit `dlopen("libvulkan.so.1")` handle, avoiding self-interposition. No layer implementation symbols are linked into the GPU executable.

`build.py` reuses the existing `tests/test_gpu.cpp` renderer and pixel oracle through checked, exact source substitutions in ignored build output. Both a stock and a scoped portal pipeline coexist on one real device using the same original shader-module handles. All 23 existing cases are read back for each pipeline. The stock oracle ignores portal flags/opacity and remains full-view; the scoped oracle retains clipping, unequal-W interpolation and premultiplied opacity checks. Every case checks 1,024 pixels and all four channels. A CPU Vulkan device is explicitly refused as GPU evidence.

`test_gpu.py` compiles three mutants from the actual layer and requires an oracle failure rather than a crash or loader failure:

- Global replacement must fail the stock approval registry oracle.
- Missing fragment substitution must fail the outside-aperture pixel oracle.
- Accepting stock modules while claiming replacement success must fail the same pixel oracle.

Private originals, replacements, headers, DSOs, executables and logs are under the existing ignored `native-portal/build/`. They are not redistributable source artifacts and are not committed.

## Evidence and limits

See `native-portal/pipeline/verification.md` for the executed results and artifact hashes. These tests prove scoped substitution, original/portal coexistence, registry behavior and synthetic GPU output. They do not prove native wrapper extraction, live camera rendering, physical registration, GPU arena retirement, command-buffer resubmission policy, or last-visible expiry. No native camera modules were loaded into the real GPU fixture. No OpenVR, live compositor, runtime service, HMD pose, setting or launch state was accessed or changed.

The parent still must integrate the checked native-wrapper extraction and packet authenticator, review this delta independently, and perform separately authorized native acceptance. A nonnull native lookup result never bypasses approval.

### Reusable implementation lessons

- Preserve one whole downstream batch when changing individual shader stages. Splitting it silently changes derivative-index meaning.
- Publish pipeline approval only after the observed downstream result, and reserve bounded registry slots before that call.
- Treat legal null destruction as a no-op for bookkeeping too. Clearing records by a zero handle can erase in-flight reservations.
- Separate generated-native recognition tests from synthetic real-GPU tests. Passing one does not establish the other.
