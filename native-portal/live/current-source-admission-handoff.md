# Current source admission integration

Header-only implementation for the Runtime lane. Initial API commit is `5bd84e0`; the final tested delta is the branch tip accompanying this handoff. The legacy constructor preserves compatibility, not production current-field-read authority. No Runtime, Evidence, camera owner, source router, loader, manifest or build-list file was changed.

## Actual provider API and call order

```cpp
NativeSourceAdmission provider(ctx, CurrentSourceContracts{authenticate_identity}, stock_owner);
// bool authenticate_identity(void*, const CameraBinding&, SourceIdentity&) noexcept
// SourceIdentity{runtime, version, client_module}. No mutable camera reads.
// bool stock_owner(void*) noexcept, explicit authenticated owner after Run.
auto eligibility = provider.bootstrap_eligible(binding);
// available: retained identity plus journal integrity and no prior pair debt.
// needs_connect: no usable journal. Neither field claims a current image.

CurrentConnectCompletion completion(binding, version, owner_thread, runtime_id);
CurrentConnectCollector collector(gate, completion, existing_native_read_boundary);
if (!provider.begin_current_connect(binding, completion.invocation_id())) refuse_owned_admission();
if (!collector.begin(retained_ticket, binding, version, 0x15f9e0, entry_saved,
                     compositor_base, client_base, compositor_sha, client_sha)) refuse_owned_admission();
const auto invocation = collector.invocation();
// ONE existing complete native METHOD bypass executes.
// Route camera_fact/event/boundary_event exactly as for StockSourceCollector.
// camera_return(pc,Saved) is PRE-RET candidate only.
const bool observed = collector.complete_own_bypass(invocation, actual_return);
auto result = completion.take();
const bool ready = observed && result && provider.finish_current_connect(binding, *result);
// Finalize journal and actual transition receipt BEFORE portal token/committed use.
```

The snippets name refusal points, not permission to cancel an already-running native method. Original forwarding and native partial failures remain unchanged. Reserve inside the authenticated original scope after the pre-call transition eligibility check. Bootstrap returns unavailable while a reservation is outstanding, so do not repeat that pre-call check after reserving.

`CurrentConnectResult`, `CurrentUpdateResult` and `StockSourceReceipt` are distinct, nonconstructible specializations. `NativeSourceMethodCollector<Sink>` shares the existing actual field facts, known masks, native read allowlist and ClientSourceBinding engine. There is no stock receipt cast, fake historical caller or second queue engine. Completion is keyed by invocation, retained ticket, process/thread, runtime/version, method and exact SP/LR.

No-history two-NewEntry success is `AuthenticatedBorrowedUse`, never local Disconnect permission. The existing legacy local-owned rule remains unchanged; opt-in mode does not invent a cold ownership witness. Missing actual completion leaves the reservation outstanding. Partial actual results preserve successful peers and sticky uncertainty. An empty already-connected return cannot seed history.

## Existing scheduled Update, not a new Update

Use `CurrentUpdateCompletion`, `CurrentUpdateCollector`, `begin_current_update(binding, completion.invocation_id())` and `finish_current_update(binding, *result)` around an actual already-scheduled native `15fcc0` own bypass. The collector API and completion order are the same. No method is invoked by the provider or collectors.

A no-read native Update is neutral and cannot make cold bootstrap wait for a future image. First warm journal creation requires both complete actual metadata/ref/Return pairs. After a good journal, unattempted q1 is optional; an attempted failed or incomplete pair is sticky failure. Observed selected IDs/provider must match that journal. No metadata pointer survives as read authority. A warm stock history can instead be sealed/folded through the unchanged stock transport, then used for current render without an extra Update.

## Current render and the existing enqueue

Construct the persistent collector on the authenticated render owner after Run. The parent keeps its real retained ticket live through the whole render invocation.

```cpp
CurrentRenderCollector render(gate, ctx, authenticate_identity, covered);
// bool covered(void*, ClientSourceBinding::Module, uint64_t pc) noexcept
render.begin(ticket, binding, nonreused_render_invocation, compositor_base, entry_saved);
render.capture(pc, original_saved); // requested actual native observers, native order
// At 161d64 capture ORIGINAL Saved BEFORE Evidence changes upload arguments.
// Then run the existing Evidence::stage/Adapter reservation, not another upload.
render.upload_reserved(existing_upload_ticket);
// At 161d68 capture actual upload continuation.
// At 161ed8 capture ORIGINAL Saved before replacement enqueue.
auto sample = render.sample();
// Optional by-value scalar transport, ONLY at this current enqueue:
// Sample{resource_index, slot, selected_texture, upload_ticket, invocation_sp, camera}.
// Feed these values into the existing EyeInput/NativeBorrow selection fields
// instead of re-reading K+0x820 or the selected texture from camera memory.

// INSIDE existing NativeBorrow callback, using its already-copied packet/context:
render.with_selection(native_borrow,
    [](void* p, const CurrentRenderSelection& selection, render::NativeBorrow n) noexcept {
        return provider_from(p).borrow_ready(binding_from(p), selection, n);
    }, ctx);
// Return that result to existing Adapter admission before its sole native enqueue.
// Actual 160dd4/161f30 RET observers route to capture(pc,Saved), closing the scope.
// Alternative authenticated parent boundary: finish(invocation, entry_sp_lr_saved).
// abort() on actual aborted routing. Neither finish nor abort certifies source debt.
```

`sample()` is scalar transport only, not a certificate or native reader. It disappears after the synchronous selection callback. Evidence's current broad selection re-reads remain a Runtime integration obligation; this lane did not edit Evidence. The optional sample return supplies the exact replacement data, so no NativeBorrow layout extension or second consumer is needed.

`CurrentRenderSelection` has no public constructor or setters. It contains copied identity/binding/packet and a collector-owned one-shot window. Copies cannot authorize another use after the callback returns or the collector is destroyed. No Saved/native-stack pointer, old metadata read token or global selection boolean survives. The two-argument `borrow_ready(binding, NativeBorrow)` refuses in opt-in mode.

The collector uses render frame `0x460`, not Update's `0x360`. At pre-mov `161be8`, it takes x0, not stale x28. Pre-load `161c74` and post-load `161c78` pair the actual slot and exact x0=K+4*index operand. Texture comes from `161e40` x0. `161ed8` matches K/SP, x28, x21=x24=3*slot, packet pointer/target, original sampled texture and existing upload ticket. Prefix changes remain limited to `[8,16)`, `[0x38,0x40)` and `[0x58,0x60)`. Both eyes must share frame/origin/command/recording/watermark and use distinct, nonreplayed tickets. A real connected0 render branch is neutral no-sample, not a permanent startup failure.

## Backend, Off and settlement

`backend` and `settled` use AuthenticateIdentity plus committed packet/command/recording provenance. They do not sample current camera state. Runtime must preserve its existing frame policy, commit/Off checks, SourceCommand sampled-use/reverse-release coverage, accepted queue token and Command/Lifecycle completion/reset proof. `settled` remains a Runtime-attested source acknowledgement, not a new standalone GPU certificate. VkCommandBuffer identity remains in the existing Command binding; NativeBorrow carries the native command and recording identity.

No camera.external predicate was added. Missing external stays unknown. Existing independent frame-policy external_clear and invalidation remain unchanged. Borrowed use never grants automatic native Disconnect. An aggregate held no-op does not mutate borrowed ownership.

AuthenticateIdentity is a trusted retained-lifetime/module/owner boundary. It must not require a fresh image or live On status during debt settlement. Bootstrap infrastructure, graph/pipeline/origin, closing/full-display and installed coverage must still be enforced by the owning Runtime at their existing admission boundaries.

## Exact loader dependency

`current-source-native-hook-request.json` lists 12 required compositor callbacks, each with exact word, 16-byte guard, Saved predicate and provenance. This is the minimal implemented set, not every table anchor. Current Connect/Update reuse the complete existing 75-site stock native inventory.

Current render requires `160be8`, `160ca8`, `161bc0`, `161be8`, `161c74`, `161c78`, `161d64`, `161d68`, `161e40`, `161ed8`, `160dd4`, `161f30`. Existing upload/enqueue sites must keep their canonical emitters. Do not install duplicate hooks. All guard bytes were checked against compositor SHA256 `9b1bd5fe4463592cbc71b2517b9ceb0a0fcd66cd6885a19dca4be0784bf6c265`. The full `161c78` guard and two epilogue guards were read from its executable PT_LOAD, not synthesized. Missing any callback refuses render begin. V1 stock coverage alone cannot grant this incremental set.

## Verification and limits

Run the owning suite with the current-vector path, once normally and once with `--sanitize`; add `--runtime` for complete production Runtime compile/link and unchanged legacy-constructor CPU smoke. All compilation/link/test processes are serial user scopes with MemoryMax=256M, MemorySwapMax=0 and designated scratch TMPDIR. No installs, SSH, camera, headset or native activation.

The tests exercise cold actual engine completion into a borrowed journal and both render-selected eyes; warm stock history without extra Update; q1-absent current Update; neutral pre-bootstrap Update/render; partial/missing Connect facts and actual completion; wrong SP/LR/method/K; every missing stock/current coverage site; selector/slot/texture; callback mutation/replay; both-eye command/recording/frame/origin/ticket/watermark mismatches; every prefix byte; identity loss; exact/idempotent settlement and stale backend rejection after reclamation. Read counters prove provider/borrow/backend/settled do not invoke the native byte backend after its memory is disabled.

`current_source_vectors.py` independently executes the pinned `161bc0` call and actual `160a80` selector helper, connected load, slot load and texture load. Its five full Saved records pass through the compiled current collector/provider and cold journal chain. Native memory contents and scheduling between instruction islands are explicitly synthetic. This is not whole-render, loader-island or hardware validation. Existing stock collector/legacy provider and complete Runtime smoke are tested separately; new Runtime consumer wiring and installed-hook verification remain with their assigned lanes.
