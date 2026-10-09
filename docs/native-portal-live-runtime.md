# Bounded native stereo runtime

`native-portal/live/runtime.cpp` is the compositor-side owner/router, not a replacement native compositor or a fixture backend. It calls the pinned compositor through `runtime-call.S`, uses the real `ft_pipeline_*` ABI, and serves the existing private control protocol consumed by `screens/native_portal::Owner`.

## Loader contract

- ELF SHA-256: `9b1bd5fe4463592cbc71b2517b9ceb0a0fcd66cd6885a19dca4be0784bf6c265`.
- Call `ft_native_portal_start(base, exact_sha, runtime_directory)` on the native initializer thread before constructor/allocation callbacks. Late attachment is unsupported. The directory is the existing private runtime-directory parent accepted by `Server`; it is not the socket filename.
- Deliver `extern "C" void ft_native_portal_event(uint64_t relative_pc, native_portal::arena::Saved *saved) noexcept`. `Saved` is exactly 800 bytes. Deliver relative native PCs, not emitter-local IDs.
- Install **every** entry in `native-portal/live/runtime-sites.json`, including native publication/Run, real join-result, early cleanup, command-pool publication, and native external acquisition/release events. Validate all original guards before replacing any overlapping instruction. Unknown callbacks preserve the native saved state.
- Reuse existing arena, pass and lifecycle emitters with their existing local IDs and selector meanings. In particular pass selector `1` is not arena selector `1` or `2`.
- `runtime-emit.c` supplies eight additional exact islands through `ft_emit_runtime_island(out, island_address, base, relative_pc, event_callback_address)`. Output capacity is 1024 bytes. It calls the exported event ABI directly. Four call replacements use selector `2` to resume PC+4; two camera-method entries use selector `2` to return through the original saved LR. Two observer ADRPs are relocated, not copied. Invalid selectors trap. There is no universal Skip operation.
- Keep the router, pipeline layer and islands loaded for process lifetime. Runtime stop is admission stop, **not** permission to unload or free pending native references.

The native call transport passes x0–x7 and an independent x8 indirect-result pointer, preserving AAPCS64 callee-saved registers. The OpenVR origin sample uses the existing initialization token and `IVRSystem_026` interface, its checked slot-14 return ABI, a rigid finite transform, and a second token read. It does not call VR_Init or establish readiness from an assumed device state.

## Native authority and supported scope

Initialization is observed at the actual constructor/allocation/initialization events. Worker publication and the native thread-start call publish the handoff; the actual `0x14e5a8` Run entry must match the worker, native TID/status, native caller LR, context, scheduler, application, renderer and device. No allocation, graph, or render authority is granted by a caller-provided `live=true` flag.

`runtime/graph.inc` borrows current pass arrays, packet arrays, inline names, attachment formats and native pass-cache entries by role. It rejects role aliasing with registered native allocations, bounds traversal, and requires the real mode-zero dispatcher caller. Existing PassGate restrictions still reject cold pass state, optimizer/mutation-list activity and unsupported owned packets. Preparation and cached encoding share the same dispatch scope through both eyes and intervening stock packets. Borrowing closes **before** native current-to-retained transfer. Transfer and retained disposal use separate synchronous scopes.

If whole-graph admission fails, the actual packet hook can only classify its immutable owned marker. It drops owned packets and leaves stock packets alone; that narrow classification is not pipeline authority or a CPU-disposal certificate. Unprovable settlement remains pending.

The runtime observes policy inputs and the original output before cloning the per-frame policy. It uses the existing pure-policy, frame gate and demand arbitration, checks real camera-idle state, and patches only the owned per-layer view. It does not rewrite shared policy feedback or unconditionally override the native background flag. Raw camera methods on the owner thread are forwarded once with their native result preserved. Foreign-thread raw methods keep the original continuation; an atomic notice revokes admission immediately and transfers debt accounting to the owner without replaying the call or inventing its outcome.

Native command wrappers may turn over while a prior executable remains pool-owned. The router retains the exact old executable role for its lifecycle, follows the actual successor wrapper publication, and pairs native reset/begin outcomes for pool commands. Issued generations are claimed monotonically; the currently published command need not be the last globally issued generation when another pool member just finished resetting. A stale bounded UBO-wrapper offset after an arena reset is readable metadata only: fresh Bridge upload/descriptor proofs remain mandatory.

## Stock classifier callback scope

Stock UBO classification at `0x2988b0` also runs during startup before any owned lease or camera staging invocation. The pinned caller at `0x29b3bc` returns to `0x29b3c0`. The Bridge needs the native wrapper's resource pointer and that resource's size even when neither object belongs to an owned upload. Geometry classification at `0x29f358` similarly needs the suspended native packet's geometry size.

`Evidence::arena_event` authenticates the existing owner, renderer, generation and Bridge binding, then copies only those exact scalar fields for the duration of Bridge dispatch. UBO input additionally checks the native wrapper offset and resource device context. Geometry input checks the native packet offset. The Bridge retains its allocation/map/coherence, capacity, cursor, reservation and fatal-refusal guards. These temporary values are unavailable to the graph reader and disappear before the native callback returns. They grant no staging writes, ownership, settlement or lease authority.

The baseline `3c52ee7` production router passes the entire two-frame chain on an actual worker thread when stock classifier calls are omitted. Its existing `Evidence::owner_event` already transfers the Bridge TID and generation issuer. The same baseline fails ordinary stock UBO classification on both bootstrap and worker threads, and separately fails stock geometry classification. This reproduces a missing callback read scope, not a stale Bridge thread. Adapter creation is deferred until the worker establishes graph/origin capability, and Command construction occurs on the queue owner; no bootstrap identity rewrite is needed for either.

The extended CPU chain keeps a charged stock append across the real thread handoff, exercises stock classifiers before any lease, rejects malformed Run identities and foreign-thread callbacks, then completes stereo uploads, encoding, queue/wait, pending Release, disposal/reset and a second recording. Separate negatives retain Unsupported on wrong resource ownership, changed offsets, unreadable fields and invalid bounds; a reset cannot heal refusal. Exact baseline reproduction and normal/sanitized verification logs are retained locally under `native-portal/live/build/runtime/`. The normal and ASan/UBSan router suites, sanitized Evidence suite with pinned instruction checks, and arena instruction/preservation suite pass under 650 MiB with zero swap. This is CPU evidence only; the live failure's exact guard branch and post-fix headset output still require parent-side verification.

## Shutdown side effect — explicit parent review required

This bounded-trial composition intentionally **aborts the compositor before `0x1381b8` cleanup** if it has created owned native state and cannot prove the shutdown worker's successful join. It pairs the actual `0x142704` worker wait, exact pthread handle at `0x183f78`, raw join result at `0x183f7c`, and native wait return at `0x142708`. Zero TID/handle alone is insufficient: the native path can clear them after a join error.

This is an explicit fail-stop gate for the unsupported timeout/error path, not a claim that stock teardown already excludes Run, not a fabricated join, not a speculative blocking join inside native locks, and not queue cancellation. The native fatal watchdog is unchanged. Review and accept this side effect before installing the complete manifest. Normal joined shutdown retains its original continuation.

`ft_native_portal_stop()` inhibits new commits. Already committed stereo finishes both cached encodings and keeps real GPU/CPU debt. Lease Release remains pending until actual queue/stamp, successful GPU wait, both CPU ownership dispositions and executable invalidation are observed. Private native wrapper/marker storage is bounded and retained until process exit rather than destroyed speculatively.

## Build and evidence

Production ARM link inputs are:

```
live/runtime.cpp live/runtime-call.S
runtime/evidence.cpp runtime/recording.cpp runtime/staging.cpp
binding/command.cpp binding/lifecycle.cpp binding/pass.cpp
render/adapter.cpp arena/bridge.cpp
control/protocol.cpp control/policy.cpp control/transport.cpp
src/aperture.cpp
```

Use C++17, pthread, PIC, `native-portal/include`, and Vulkan SDK headers for the real pipeline header. `graph.inc` is included by `evidence.cpp`. **Make the actual `libft_portal_pipeline.so.1` a retained DT_NEEDED dependency with `-Wl,--no-as-needed /absolute/path/to/libft_portal_pipeline.so.1`**, using the same canonical DSO as the Vulkan layer manifest and an appropriate runtime search path. Alternatively preload that exact DSO globally before the router. Merely letting Vulkan later `dlopen` a local layer cannot resolve the router's already-null weak ABI imports. The pipeline must expose generation/begin/end/approved; missing exports or device-generation evidence refuse capability. Compile `runtime-emit.c` into the loader's emitter tool, not as a substitute runtime. Test files must never enter the production link.

CPU verification commands (no device or loader activation):

```
systemd-run --user --wait --pipe -p MemoryMax=650M -p MemorySwapMax=0 \
  python3 -B /absolute/repo/native-portal/live/runtime_test.py --sanitize

systemd-run --user --wait --pipe -p MemoryMax=650M -p MemorySwapMax=0 \
  python3 -B /absolute/repo/native-portal/live/runtime_machine.py \
  --elf /private/pinned/vrcompositor --pylib /private/python/dependencies
```

The first runner links every production component into one DSO and tests real private lease transport, origin transport, two staged uploads, pass preflight, both cached backends, queue/stamp/wait, CPU transfer/disposal/reset, pool turnover and a second stereo queue. It also checks graph refusal without stock suppression, owner and foreign external calls, stop between committed eyes, successful join, timeout/no-join abort, and join-error abort. Native functions, native graph writes, pthread and Vulkan outcomes in the CPU chain are **explicit modeled boundaries**, not live GPU evidence. The island runner executes emitted AArch64 and checks exact pinned guards, all saved GPR/SIMD/status registers, stock and selected continuations, and invalid selectors.

`ft_native_portal_diagnostics(out, sizeof(*out))` returns a fixed-size mailbox snapshot: both upload/encoding counts, successful owned native queue count, submitted frame, pending debt, origin, initialization/capability/stop state, and refusal code. It copies no camera pixels and performs no callback logging.

ARM link, full loader reconciliation, actual headset rendering and live GPU completion are separate parent-side checks. This work does not install or activate anything on the device.
