# Camera-owner composition

`camera_owner.hpp/.cpp` composes the real `CameraAccessGate`,
`CameraNativeBinding`, `OwnerCoordinator` and `DemandArbiter`. It does not install
hooks or replace `Runtime`. `CameraOwner::coverage()` deliberately returns
`supported=false` and `installed_boundaries=0`. Building or testing this component
cannot enable live camera admission.

## Lifetime and execution

Construct on the authenticated render owner. Observe completed camera publication
at `0x8dc1c`, then call `bind` with independently authenticated renderer, device and
association generations. Publish the instance to listeners only after binding.
One instance serves one camera lifetime. Do not reconstruct it to clear a fault.

Binding acquires a real gate ticket for the closing barrier. Both the mutation
queue and committed-demand debts use callbacks to that same gate, not another
counter. `OwnerCoordinator::submit_scoped` passes its already-retained ticket to
the executor. The old zero-argument `submit` remains compatible.

`CameraOwner::submit` captures a callback without resolving native identities.
Only its owner callback calls `gate.lookup` with the queue ticket and exposes the
borrowed `CameraBinding` and owner-only arbiter. The callback must not store native
pointers, the arbiter reference or the ticket. Incoming native/connection bindings
still authenticate caller identities and preserve real native method results.
Reentrant submissions queue behind the running callback; owner waits return
`OwnerWouldBlock`. Failed ticket release quarantines the original result and
retries only release, never native execution.

Close in this order:

1. At the actual destructive boundary, close gate admission. The native binding
   withholds the entire original cleanup method while any ticket remains.
2. On the owner, call `begin_close`. It closes demand admission and cancels only
   unexecuted queued mutations. It never blocks or invents settlement.
3. Use `drain` to observe existing obligations and reconcile final native
   disconnects. It looks up identities using the barrier acquired at bind. It
   neither acquires a new ticket after close nor bypasses gate lookup.
4. `finish_close` releases that barrier only when the queue is drained, every
   arbiter debt has actually released its ticket, the journal has no fault, and
   both queues are proven disconnected with no producer demand.
5. Retry the original cleanup boundary. The binding invokes the complete original
   method synchronously through its integration-supplied bypass callback. Join
   all users before destroying the owner object.

Unknown initial queues, partial execution, missing source proof or release failure
can leave the barrier retained indefinitely. Destruction then fails fast. There
is no force-release or timeout-based declaration of settlement. An integration
must retain the owner or take its explicit fail-stop path, not discard its debt.
A never-connected generation has no invented initial-idle certificate.

## Reusing existing render settlement

`RenderDebt` binds one debt to the recorded adapter ticket watermark. Construct it
after both immutable tickets are recorded and commit its arbiter debt before
native commitment. Keep it and the adapter alive until debt release, and do not
record a successor while it is outstanding.

It polls the actual `render::Adapter`, whose callbacks must use the existing
`binding::Command` exact queue/wait certificate and `binding::Lifecycle`
current-to-retained transfer, retained disposal and successful executable reset.
It checks the same watermark before and after the independent source callback.
It does not maintain another CPU/GPU settlement registry.

Source settlement is a separate mandatory callback. Its default is null and
refuses. Native queue handles, activity flags, successful Release, empty pass
lists and GPU completion do not establish source reuse. The production native
source-proof binding is still missing. Renderer reclamation may finish before
source settlement; the camera gate and demand debt remain retained until both do.

## Consumer reattach is not producer recreation

An intact producer can support a new compositor consumer after a complete
consumer detach. The driver attempted-once Create/recreation latch after
Deactivate is a different lifetime. The arbiter now clears its connect-attempt
latch only after genuinely observed full detach of both newly-owned queues and
zero committed debts. It preserves the same camera and integration generation.

A subsequent Connect plan sets `ActionPlan::reattach`. Before making that native
call, the executor must revalidate the intact producer generation, actual provider,
name/alias resolution and both queue headers. Missing proof must return an empty
`NotExecuted` receipt without native effects. Executed reattach receipts must
explicitly set `producer_revalidated`; otherwise the arbiter faults and cannot
admit a frame. This field is a binding contract, not proof manufactured by the
arbiter. There is no default live executor that supplies it.

Partial, failed, unobserved and borrowed outcomes do not reset the latch. Failed
native `0x15fba8` is not blindly retried. A raw Release result of one without both
actual disconnect calls cannot authorize warm idle or another Connect.

## CPU verification

Provide the existing pinned compositor ELF through `FT_HOOK_TEST_ELF`, with
Unicorn and pyelftools available in the selected interpreter or `PYTHONPATH`.
The witness validates its SHA-256 before executing native instructions.

```
python3 native-portal/live/camera_owner_test.py
python3 native-portal/live/camera_owner_test.py --sanitize
python3 native-portal/control/owner_coordinator_test.py
python3 native-portal/control/owner_coordinator_test.py --sanitize
```

Each compiler, linker, emulator and test subprocess runs in a user-systemd scope
with `MemoryMax=256M` and `MemorySwapMax=0`. GCC collection parameters and generated
fixture prefixes keep the sanitized compilation within that cap. A runner timeout
stops its exact owned scope rather than abandoning a compiler. Outputs are under
ignored `build/camera-owner`.

The composition fixture reuses the existing combined Adapter/Command/Lifecycle
fixture, including its upstream Bridge and PassGate. Native userdata operations,
service results and source-return observations remain explicitly synthetic. A
receipt shim consumes fresh `camera_idle_native.py` output. It preserves both the
aggregate native return and separately recorded raw service results. Service
ownership, newness and header generations are explicit fixture inputs, never
inferences from nonnull pointers.

Verified paths include:

- Real foreign-thread submission, retained-before-lookup owner execution, portal
  tokens and independent stock/foreign full-display levels.
- Off with real adapter/command/lifecycle debt, each absent GPU, CPU or source
  proof refusing cleanup, then final release and one forwarded original cleanup.
- A supplemental scoped-queue test without a closing barrier, proving the running
  mutation ticket alone blocks cleanup and remains usable after close. Failed
  release preserves it in quarantine until a successful release-only retry.
- Stale epoch/generation, wrong caller generation, fork refusal, reentrant queued
  callbacks and owner no-self-wait.
- Complete pinned warm disconnect versus partial-connect alias and Release-one
  with no disconnect. Negative unresolved states stay quarantined.
- Three On/commit/Off/settle/disconnect/On cycles in one camera/producer generation,
  with missing reattach proof refusing before the synthetic native boundary.
- Default-null source proof and stale adapter watermark refusal.

The first composition probe failed because the production binding was absent.
The repeat-consumer probe then failed on the old one-connect latch before the
reattach change. Regular and ASan/UBSan composition runs pass. The focused gate,
native-binding and original owner-coordinator suites also pass; the coordinator
suite passes ASan/UBSan as well. No GPU or device operation is exercised here.

## Remaining global integration

Native publication and cleanup component code exists, but the live loader does
not install this owner. Remaining coverage includes application precleanup
`0x8b020`, rebind `0x162360`, all application and connection acquire/release/lost-pipe
callers, policy acquire/release/teardown, direct shared-demand Set/Clear and bank
migration, retained backend/association lifetimes, and actual owner-loop pumping.
Existing shutdown/join guards must remain tied to the exact worker and real join
result. Native service/provider/alias/header provenance and source-settlement
callbacks must be bound before live reattach or frame admission. Full renderer
frame gates and the current shader-only aperture remain unchanged.
