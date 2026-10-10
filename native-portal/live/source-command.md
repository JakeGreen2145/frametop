# Native sampled-source command attribution

Runtime now binds the portal's existing Adapter ticket watermark to the original native sampled records and their reverse release. It does not create a source reader, copy, queue, camera Acquire/Release, DMA fence or source-slot writer. `Command` remains the only GPU completion authority; `Evidence` and `Lifecycle` remain the CPU disposal/reset authorities.

## Attribution and settlement

`source_command.inc`, included in the production Runtime, keeps bounded observations for one outstanding Adapter pair. It does not export pointer registration or grant future reads from a remembered address.

1. At the existing dispatch/debt boundary, capture the Adapter-issued watermark and Evidence-issued command, VkCommandBuffer, renderer/device and recording generation.
2. For each admitted eye, use the Adapter-authenticated immutable packet prefix and native packet address. Every nonnull sampled-texture slot in packet `+80..+f8` must execute the actual `29f294` call and `29f298` return within that eye's encoder scope. Recording order and encoding order are independent.
3. Authenticate texture `+28` shared-source selection, texture `+48` native image, image `+8` native device owner, image `+b0` VkImage and renderer `+13f8` native frame marker. The native device owner is renderer `+50`, not the Vulkan device handle in `Identity.device`.
4. Observe `29ac80` acquire, the actual `2fe72c` Vulkan barrier and `2fe730` return, `2fe738` resource-registration call and `2fe73c` return, then `29ac84`. Require the exact command/handle, layouts, stages/access, native FOREIGN or EXTERNAL family and range. At sample return, require that exact texture in the native renderer `+1550` vector.
5. Observe reverse release at `2a255c`, the same barrier/registration events, then `2a2560`. Require the exact vector element, image, current native frame, command generation, reversed families/layouts and matching subresource range. Ignore unrelated stock releases without changing their Saved record or continuation.
6. Freeze coverage before the actual queue call. Source observations arriving after submission cannot repair its coverage. Only the existing accepted Command certificate, exact successful wait, both packet disposals and successful executable reset can retire the attributed source debt. The camera owner keeps its separate pending hold if source evidence is missing after Adapter reclamation.

Off does not erase committed observations or issue new native work. The positive chain observes reverse release and queue/wait/disposal after a real private-socket Release returns Draining. A matching source barrier is not completion, and a prepared timeline point is not acceptance. Failed submits and mismatched signals remain debt.

The existing fully rejected pair is distinct. Both authenticated actual packet skips can prove that this pair encoded no source sample. It still needs the original upload command's exact queue, wait, disposal and reset certificates. Partial-eye rejection or absent skip proof cannot take this path.

All native pointer reads require the existing retained CameraAccessGate binding and current Evidence whole-dispatch scope before dereference. The array contains observations, not a reader whitelist. Completion checking makes no source-pointer reads. Native source vectors and shared status0 remain untouched.

## Hook contract

Ten observer sites were added to `runtime-sites.json`, with exact words, 16-byte guards and guard hashes from compositor SHA256 `9b1bd5fe4463592cbc71b2517b9ceb0a0fcd66cd6885a19dca4be0784bf6c265`:

| PCs | Scope |
|---|---|
| `29f294`, `29f298` | Actual packet texture sample helper call/return |
| `29ac80`, `29ac84` | Shared-image acquire helper call/return |
| `2fe72c`, `2fe730` | Actual vkCmdPipelineBarrier call/return |
| `2fe738`, `2fe73c` | Native command resource registration call/return |
| `2a255c`, `2a2560` | Original reverse release helper call/return |

They use the existing generic observer emitter and 800-byte Saved ABI. No method replacement or skip route was added. The merged loader contract requires all sites; the new include participates in the production build hash.

## Executed tests

The initial RED ran the actual Runtime with a provider claiming settlement but no sampled-source observations. The old path incorrectly completed the final watermark. The new path retains debt and reaches the existing cleanup fail-stop. Additional RED probes exposed and fixed mismatched reverse subresource coverage and accidental rejection of unrelated stock release returns. The rejected-pair positive first remained pending, then passed with its existing exact no-read certificate.

`source_command_native.py` executes pinned AArch64 instructions and the actual merged loader islands. It runs the original shared-image acquire, native vector append, real `3011c0` registration and dedup, and reverse release argument assembly and helper. It emits 16 callbacks for both eye samples and the reverse release. Those Saved records and native memory snapshots, relocated to fixture objects, feed the actual compiled Runtime. The private lease, Adapter, CameraOwner, Command, Lifecycle and exact retirement chain all run. Eight complete cycles retire successfully.

The format predicate and void Vulkan driver call are explicit synthetic boundaries. Command handles, GPU acceptance/waits, native graph publication/disposal/reset and the admission provider are modeled by the existing Runtime fixture. The source helper, registration/dedup and merged observer instructions are actually executed, not reconstructed return values. These CPU tests do not establish live GPU behavior, ICP/kernel visibility or optical correctness.

The bounded matrix covers missing samples, partial eye, missing reverse release, wrong resource/command/family/image/frame/generation/record/range, omitted barrier or registration event, wrong signal and failed submission. Positive variants cover reversed encoding order, mixed stock source releases and exact rejected-pair retirement. The pre-existing 19 camera Runtime cases and seven source Runtime cases remain required regression checks. Run normal and ASan/UBSan variants separately, with serial compiles capped at 256 MiB and zero swap.

```sh
export FT_HOOK_TEST_ELF=/home/jakeg/.hermes/work/frame-native-reuse/camera-analysis/vrcompositor
export PYTHONPATH=/home/jakeg/.hermes/work/frame-native-product-design/pylib:/home/jakeg/.hermes/work/frame-native-reuse/camera-analysis/pylib
python3 native-portal/live/camera_runtime_test.py --matrix
python3 native-portal/live/source_command_test.py
systemd-run --user --scope -p MemoryMax=256M -p MemorySwapMax=0 \
  python3 native-portal/live/source_command_native.py
python3 native-portal/live/source_runtime_test.py
```

For sanitizer runs, add `--sanitize` to the camera, source-command, native-source and source Runtime runners. Loader compilation needs OpenSSL development headers/library. On the implementation host these were extracted to owned ignored `build/source-command/ssl`, without installing system packages; its include and library directories were supplied through `CPATH` and `LIBRARY_PATH`. Generated ELF excerpts, binary event vectors, logs and reports stay under ignored build directories.

## Remaining admission boundary

Production `Runtime()` still has no `SourceAdmission`. This change implements useful native attribution, not an always-false provider facade or a claim of complete frame admission. It deliberately leaves the existing `ready`-before-Connect bootstrap contract unchanged pending Desktop's caller reconciliation. No driver Create instrumentation, global waiter registry or global imported-image registry was added.

The remaining interface decision is whether the unchanged native draw needs a separate `nativePathAdmission` prerequisite for Connect, followed by actual native frame readiness, rather than the current shared `ready()` predicate. `transition` must still represent actual per-queue native outcomes. `settled(binding, watermark)` remains an independent provider/consumer fact and cannot bypass Runtime's attributed Command/Lifecycle chain. Metadata return is not GPU completion.

Supported source coverage is bounded to the current same-command external/foreign native draw whose acquire and registration are observed in this owned dispatch. A texture first acquired by an unobserved earlier stock draw, aliased wrappers without complete attribution, split commands or extra deferred readers remain unsupported. No universal producer-generation or kernel/ICP coherency claim follows. These limitations retain debt; they do not change stock native reuse or add requirements to every Valve firmware path.

The delivered `NATIVE-REUSE-CLOSURE.md`, SHA256 `d2440ec57a25d3fc493d593839efd9f07d3a66baea8691c87f7e33dedc7290fd`, and archive SHA256 `421b312b69385f58c7f394732396a92773c0cc07787692768b111083acd3de22` supply the acquire/reverse/accepted-command distinctions. Selected original assembly was extracted only into owned ignored storage. The approved DMA reconciliation report was also read. Historical consent-blocked text in those reports is superseded by the authorized delivery. App `+25668` is still VSYNC, and the original age/status0 store is not reclassified as an all-errors GPU retirement proof.

No device, SSH, runtime activation, ARM64 production build or kernel/firmware collection occurred in this lane. Parent owns production build and admission integration.
