# Current source API follow-up, not activated

The current field-fact commit deliberately preserves the existing ENGAGED `Authenticate(State&)` behavior. That callback is NOT declared production-safe by stock history or by this commit. `CURRENT-SOURCE-READINESS-CONTRACT.md` is now authoritative for the next provider/Runtime change. Its exact scope is different from the stock collector and needs a separately tested current-startup result and current-render selection.

## Proposed exact provider interface for the next commit

Names and fields below are the intended integration interface, not declarations implemented in this commit. Parent can assign the follow-up without another native-coherence investigation.

```cpp
struct SourceIdentity {
    uint64_t runtime, version, client_module;
};
using AuthenticateIdentity = bool(*)(void*, const CameraBinding&, SourceIdentity&) noexcept;
struct CurrentSourceContracts { AuthenticateIdentity authenticate; };
// Explicit opt-in constructor. Existing constructor keeps ENGAGED compatibility.
NativeSourceAdmission(void*, CurrentSourceContracts, StockOwner);

struct BootstrapEligibility {
    bool available;       // identity/infrastructure/journal integrity, not an image
    bool needs_connect;   // no complete journal; one existing original only
};
BootstrapEligibility bootstrap_eligible(const CameraBinding&) noexcept;
bool begin_current_connect(const CameraBinding&, uint64_t invocation) noexcept;
bool finish_current_connect(const CameraBinding&, const CurrentConnectResult&) noexcept;

bool borrow_ready(const CameraBinding&, const CurrentRenderSelection&,
                  render::NativeBorrow) noexcept;
```

`CurrentConnectResult` must be a separately typed, collector-issued completion carrying the same native fields, exact invocation and actual binding-engine outcome as a stock receipt. It must never be synthesized from a `StockSourceReceipt` or inserted into the stock transport. Factor shared method observation instead of copying its engine. Begin reserves the one current original attempt, preserves sticky failure, and never sets `before_local_` without separate genuine cold evidence. Finish requires completed own bypass, two actual successful queue outcomes and observed active/connected path before journal/portal use. Without genuine cold evidence its disposition is borrowed, even with two NewEntry results.

`CurrentRenderSelection` is a by-value, collector-issued synchronous certificate, not a struct whose booleans arbitrary callers can set. It contains `SourceIdentity`, retained binding/ticket, process/owner, parent/render invocation, entrySP/currentSP/callerLR, the current render-connected observation, resource index, ring slot, selected texture, returned upload invocation/ticket, and exact command/recording/frame/eye/borrow ticket/watermark. The native render collector must validate the following before issuing it.

- Begin is `160be8`, bodySP is entrySP minus `0x460`. Never use Update's `0x360` frame or its returned metadata pointers.
- Connected is the native `160ca4` load observed at `160ca8`.
- Selector comes from the actual `161bc0` call. At `161be8`, Saved is pre-instruction, so x0 is the selector result BEFORE `mov w28,w0`. Do not sample pre-instruction x28 there as the new result.
- Slot at `161c74` is also pre-load. Observe its actual successor or use the delivered exact scoped selected operand; do not mislabel pre-load x24 as the new slot. This is an implementation hook choice for the loader, not authority to read any camera tuple.
- Texture is the `161e38` load observed at `161e40` x0 before its packet store.
- At `161ed8`, require x27=K, x28<2, x21=x24=3*slot, slot<16 and exact selected texture at copied packet `+0x80`.
- Match existing `161d64`/`161d68` upload invocation/return and existing immutable borrowed-packet provenance. Prefix differences remain restricted to `[8,16)`, `[0x38,0x40)`, `[0x58,0x60)`.

No camera.external predicate is added to packet borrowing. Frame-policy `external_clear` remains the existing independent gate. Unknown camera external stays unknown. Subsequent current Update may omit q1 if unattempted; first-history warm journal seeding still requires both actual pairs. Failed attempted metadata is not optional omission.

Backend/settled in the opt-in mode must call identity-only authentication and compare previously committed packet/command provenance. They must not sample current camera fields after use. Existing Command/Lifecycle/reverse-release/submit/Off debt remains unchanged.

## Parent call order

Use the new parent `RetainedCaller::observe_scope(const Observer&) const` while its real handle stays live, unmoved and unreleased. The observer supplies the exact existing gate ticket and immutable binding. This is identity retention only, not provider/header/read authority.

For the current stock collector, the implemented order is documented in `stock-native-source-handoff.md`. For the follow-up, reserve bootstrap/current Connect inside that same retained method scope, run the one existing complete native-method bypass, finalize actual result/journal, then allow committed portal use. Let original Update/rendering run at their native scheduling points. At the single original enqueue, pass current by-value render selection alongside the already-copied packet; consume/revoke it synchronously. Do not add a second source consumer or re-run Update to obtain observations.
