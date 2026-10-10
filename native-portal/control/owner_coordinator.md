# Render-owner coordination

`owner_coordinator.hpp/.cpp` is a production component, now composed with the
real camera gate and native binding in [CameraOwner](../live/camera_owner.md).
Neither is wired into `live/runtime.cpp` or the legacy `DemandAdapter`. Renderer and
CameraControl APIs remain unchanged. Keep native admission closed until the
parent integration verifies every writer and destructive boundary from the
checkpoint06 contract. CPU tests below do not establish that coverage.

## Binding order

1. Construct one `OwnerCoordinator` and one `DemandArbiter` on the authenticated
   render owner for a real retained native epoch and integration generation.
   Never reconstruct them with the same identities to clear failure history.
2. Supply `Retention{context, retain, release}` from the generation gate's real
   destruction interlock. `retain(context, epoch, ticket)` returns a real ticket;
   `release(context, ticket)` reports its actual release. Null callbacks reject.
   These callbacks must not throw. False retain must acquire nothing, and true
   retain must produce a nonzero ticket. The coordinator does not create epochs.
3. Authenticate the native caller/connection at the incoming boundary, before
   submitting. Capture stable identities, not pointers obtained by an unretained
   lookup. Resolve native objects only inside the retained mutation callback.
4. Marshal all demand mutations and native action executors through `submit_scoped`.
   Its callback receives the actual already-retained ticket for gate lookup. The
   zero-argument `submit` remains compatible but cannot itself supply that ticket.
   `DemandArbiter` is owner-only; its direct API is not a replacement for the
   queue's retained scope. `reconcile` supplies a typed plan to the binding.
   The executor must additionally verify native caller coverage, exact queue
   membership/ownership and intact producer-generation readiness before a call.
5. Pump from the actual owner event loop. Foreign callers may `wait`; the owner
   gets `OwnerWouldBlock` instead of waiting for itself. Reentrant submissions
   are queued behind the current mutation. The owner must not synchronously wait
   for an RPC handler which is itself waiting for this queue.
6. Copy completion with `take` to reclaim the slot. Completed but untaken
   results consume capacity. A failed gate release quarantines that slot and
   preserves its real native result. `retry_release` retries only gate release,
   never the native mutation.

Submission acceptance and logical caller changes are not native success.
`Result.native` keeps `Executed`, `NotExecuted`, or `Failed`, plus the exact native
value and its validity. Zero/false remains an executed zero/false. A cancelled,
stale, full, unsupported or redundant request has no invented native value.
Callers must handle a nonzero submission ID even when its rejection is Retention,
which records a contract-violating retain callback that returned a ticket on
failure. That ticket remains accountable until release succeeds.

## Demand and queue outcomes

`portal_ready` and `frame_admissible` are demand/connection prerequisites only.
They do not replace source ready/reuse provenance, native frame retention, shader
compatibility, tracking, safety, or the renderer's existing frame gates. The
binding must prove those independently before registering and recording a frame.

- Caller identity is kind, uint64 ID, and uint64 connection generation. Stock and
  foreign levels are idempotent; portal tokens are separate, monotonically issued
  identities. A disconnected caller remains tombstoned; its higher generation
  can reuse the same slot after its old tokens/debts settle. Different caller IDs
  consume the bounded generation's caller bank rather than evicting tombstones.
- `stock_full_display` uses only legacy stock/foreign levels. Portal-only demand
  cannot turn on full RoomView. Producer demand also includes committed debt.
- Queue state starts Unknown, not presumed native idle. Each executed plan needs
  independent data/layout observations, exact IDs/header generations, raw values,
  and NewlyOwned/Borrowed/Unknown ownership. Aggregate success alone is inadequate.
- Partial or unknown outcomes latch a fault and retain the journal and caller
  intent. There is no automatic retry, rollback or success inferred from native
  latches. Native Release success with unproved per-queue disconnect is not idle.
- Disconnect selects only confirmed newly-owned memberships. A mixed selection
  is not permission to use an aggregate native call that also releases borrowed
  memberships. Such a binding must return NotExecuted until a supported exact
  operation exists. Borrowed membership never authorizes a portal frame here.
- Another consumer connect is supported after complete observed disconnect of
  both newly-owned queues and zero committed debts, in the same camera/producer
  generation. The next plan has `reattach=true`. Its executor must revalidate the
  intact producer, provider, alias/name resolution and both headers BEFORE native
  execution, or return empty NotExecuted without effects. An executed receipt
  must explicitly set `producer_revalidated`; missing proof faults admission.
  Producer Create/recreation after driver Deactivate is a different lifetime and
  is not authorized here. Partial, failed, borrowed and unobserved disconnects do
  not reset the attempt latch or authorize a blind native Release retry.
- A NotExecuted receipt promises no native effects and carries no native values
  or queue observations. Contradictory receipts latch Unknown/fault. Failed
  receipts can still preserve whatever native effects were actually observed.

## Off and closing

`commit` registers a distinct debt and retains another real gate ticket before
native commitment. Its binding-supplied settlement callback must authenticate all
original CPU/executable, exact GPU, and source-reuse obligations. There is no
cancel-committed operation. Off closes fresh token/frame admission but keeps the
old producer demand until settlement and gate release both succeed.

Use `DemandArbiter::close` on the owner and close the gate's new admission at the
real precleanup boundary. Keep an already-retained closing barrier alive while
existing committed work and final native disconnects finish on the owner.
`OwnerCoordinator::close` cancels only unexecuted tasks and refuses new submissions;
it does not perform native destruction, poll debts, or manufacture a closing
barrier ticket. Drive the remaining settlement under that retained barrier,
without waiting on the owner thread or holding queue/gate mutexes. Destruction
must honor the gate even when native calls fail or source settlement is unknown.

Only after that original-generation barrier is genuinely settled may the binding
destroy the coordinator. Join users first. Queue destruction with outstanding
retention, or arbiter destruction with committed debt, fails fast. Keep faulted
native journals with their original resource owner; discarding a journal is not
recovery or evidence of native cleanup.

The per-generation limits are declared in the header. Identifiers never wrap or
reuse a freed slot's identity. Mutating the arbiter directly during an executor or
settlement callback is rejected; submit a follow-up instead. Its `close` returns
false on wrong-thread or reentrant use, so callers can distinguish nonexecution.

## CPU verification

Run from the repository root:

```
python3 native-portal/control/owner_coordinator_test.py
python3 native-portal/control/owner_coordinator_test.py --sanitize
```

Each compiler, linker and test process runs in its own user-systemd scope with
`MemoryMax=256M` and `MemorySwapMax=0`. Sanitizers use O0/g1 to stay within that
limit. No SDK, device, SSH, activation or native binary is used.

The positive test runs the production coordinator/arbiter on a real render
thread, with listener-thread submissions and a real mutex-protected in-memory
destruction interlock. It interleaves portal, stock and foreign requests, preserves
committed debt across Off, and executes modeled connect/disconnect callbacks.
Separate tests cover cancellation during execution/retention, FIFO, full/stale
queues, callback and capture-destruction reentrancy, null/failed retention,
borrowed/partial/lying native receipts, exact caller revocation, bounds, and
nonreused token/debt storage over repeated cycles. The interlock and native/source
results are explicitly modeled. The separate [CameraOwner fixture](../live/camera_owner.md)
uses the actual lane-A gate and native binding, plus Adapter/Command/Lifecycle
settlement and pinned warm-transition receipts. Actual global native writer
coverage remains integration work.
