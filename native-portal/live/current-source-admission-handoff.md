# Current source admission integration

Implemented header-only APIs. The legacy constructor remains unchanged compatibility mode, not production current-read authority. `current_source_admission_test.cpp` now compiles and runs the positive identity-only cold bootstrap, actual event-engine Connect completion, borrowed journal, two collector-selected borrows, backend and exact watermark settlement. Broader tests are still being added.

## Runtime call order and actual declarations

```cpp
NativeSourceAdmission provider(ctx, CurrentSourceContracts{authenticate_identity}, stock_owner);
// authenticate_identity(void*, const CameraBinding&, SourceIdentity&) noexcept
// SourceIdentity{runtime, version, client_module}. No current camera reads.
// Explicit stock_owner(void*) noexcept after Run. No construction-thread fallback.
auto eligibility = provider.bootstrap_eligible(binding);
// available is infrastructure/journal integrity, not image admission.
// needs_connect says there is no usable journal.

CurrentConnectCompletion completion(binding, version, owner_thread, runtime_id);
CurrentConnectCollector collector(gate, completion, existing_native_read_boundary);
provider.begin_current_connect(binding, completion.invocation_id());
collector.begin(retained_ticket, binding, version, 0x15f9e0, entry_saved,
                compositor_base, client_base, compositor_sha, client_sha);
// Same camera_fact/event/boundary_event routes as StockSourceCollector.
// camera_return(pc, saved) is PRE-RET candidate only.
auto invocation = collector.invocation();
// Run ONE complete existing own method bypass. No invented stock caller.
collector.complete_own_bypass(invocation, actual_return);
auto result = completion.take();
provider.finish_current_connect(binding, *result);
// Only now journal and transition receipt can permit portal token/committed use.
```

`CurrentConnectResult` and `StockSourceReceipt` are distinct nonconstructible specializations. `NativeSourceMethodCollector<Sink>` shares the existing actual field facts, native allowlist and ClientSourceBinding engine. No receipt cast or second event engine. No-history two-NewEntry success is `AuthenticatedBorrowedUse`, never local Disconnect permission. Missing result leaves the reservation outstanding. Partial actual results retain successful peers and poison later admission.

```cpp
CurrentRenderCollector render(gate, ctx, authenticate_identity, covered);
// covered(void*, ClientSourceBinding::Module, uint64_t pc) noexcept
render.begin(ticket, binding, nonreused_render_invocation, compositor_base, entry_saved);
render.capture(pc, saved); // each requested actual observer, in native order
// At 161d64 capture ORIGINAL Saved BEFORE Evidence changes upload arguments.
// Then call existing Evidence::stage/Adapter reserve, not another upload.
render.upload_reserved(existing_upload_ticket);
// At 161d68 capture actual upload continuation.
// At 161ed8 capture ORIGINAL Saved before replacement enqueue.
// INSIDE existing NativeBorrow callback, with already copied NativeBorrow:
render.with_selection(native_borrow,
    [](void* p, const CurrentRenderSelection& selection, render::NativeBorrow n) noexcept {
        return provider_from(p).borrow_ready(binding_from(p), selection, n);
    }, ctx);
// This is the existing synchronous admission before the sole replacement enqueue.
render.finish(nonreused_render_invocation, actual_returned_entry_sp_lr_saved);
// abort() on real aborted routing. Never fabricate a return or command settlement.
```

Selection has no public constructor or field setters. It contains copied identity/binding/packet and a collector-owned one-shot window. Copies become unusable when callback returns, including after collector destruction. No Saved pointer, native reader, old metadata pointer or global current-selection boolean survives. The two-argument `borrow_ready(binding,NativeBorrow)` refuses in opt-in mode.

The collector uses render frame `0x460`, x0 at pre-mov `161be8`, paired pre-load `161c74` / post-load `161c78` x24, and loaded texture `161e40` x0. `161ed8` authenticates native K, SP, x28, x21=x24=3*slot, packet pointer/target and sampled texture. Upload ticket is the existing Adapter reserve identity and must equal NativeBorrow provenance ticket.

`backend` and `settled` call only AuthenticateIdentity in this mode and compare committed packets/command/recording. Runtime retains existing SourceCommand/Command/Lifecycle exact settlement and Off barriers. `settled` is NOT a new standalone GPU completion certificate. No camera.external predicate was added. Existing frame-policy external_clear stays independent.

## Loader dependency

See `current-source-native-hook-request.json`. Current Connect uses the existing complete stock native required inventory unchanged. Current render needs the listed additional scalar observers. Slot successor `161c78` first three words are provided by Desktop's `161c74` guard; the loader lane must pin the fourth word from the authenticated ELF rather than invent it. Coverage callback must refuse until the complete canonical installed inventory is present. No build-list change, new .cpp or Runtime edit is required by these headers.
