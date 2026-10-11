# Camera-owner composition

`camera_owner.hpp/.cpp` composes the real `CameraAccessGate`,
`CameraNativeBinding`, `OwnerCoordinator` and `DemandArbiter`. The actual Runtime
now owns this composition; see [runtime integration](camera-runtime.md).
`CameraOwner::coverage()` remains a standalone component result. Use
`Runtime::camera_coverage()` for the loader's installed boundary mask and current
admission. Building or testing this component alone cannot enable live admission.

## Lifetime and execution

Construct on bootstrap and observe completed camera publication at `0x8dc1c`.
After Evidence authenticates Run, use `bind_handoff` with the independently
authenticated renderer/device/association identities. Direct component fixtures
may still construct and `bind` on one owner. Publish the instance to listeners
only after binding.
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
4. `finish_close` releases that barrier only when all retained callers have
   released, the queue is drained, every arbiter debt has released its ticket,
   the journal has no fault, and there is no producer demand. Attempted integration
   work must have proven native disconnect; an unused integration can relinquish
   its barrier without declaring the unknown native queues idle.
5. Retry the original cleanup boundary. The binding invokes the complete original
   method synchronously through its integration-supplied bypass callback. Join
   all users before destroying the owner object.

Unresolved integration work, partial execution, missing source proof or release failure
can leave the barrier retained indefinitely. Destruction then fails fast. There
is no force-release or timeout-based declaration of settlement. An integration
must retain the owner or take its explicit fail-stop path, not discard its debt.
A never-connected generation has no invented initial-idle certificate.

## Incoming caller lifetime only

`retain_caller(expected_epoch, expected_generation)` returns a move-only
`CameraOwner::RetainedCaller`. An empty handle means refusal. Caller authentication
and the expected identities come from the incoming binding, not an unretained
native pointer lookup. This API does not authenticate a caller by itself.

1. Retain before resolving any context, application, camera or association pointer.
2. Call `handle.binding(out)` to copy the immutable published `CameraBinding`
   through that handle's actual gate ticket. Refusal clears `out`. This exposes
   scalar identities, not permission to read a native graph.
3. Keep the handle until the complete original invocation has returned, including
   nested callbacks. Each nested independently admitted caller owns a separate
   ticket. No queue pumping or owner execution is required by this API.
4. Call `handle.release()` after the original returns. Success empties the handle.
   Failure preserves its ticket and owner accounting for an explicit retry.
   Releasing an empty or moved-from handle returns false and cannot release a
   later occupant of the reusable gate slot.

The handle is not concurrently mutable. With external synchronization it may move
or release on another thread, matching `CameraAccessGate::release` and the owner
queue. The gate's existing cleanup self-wait rules still apply. Move assignment
first releases the destination. Destruction provides RAII release; failed fallback
release or failed destination release takes `std::terminate`, never silently
forgets a ticket. All handles and callers must finish before the owning
`CameraOwner` is destroyed. Outstanding retention makes owner destruction fatal.
The handle does not extend the C++ object's storage lifetime.

Only the acquire-visible `Retained` bind stage permits admission. Immutable epoch
and generation writes precede that release publication. Initial, unbound,
clean-refused, partial-published and closed instances refuse without reading
owner-only mutable fields. A short caller-accounting mutex covers actual gate
retain/release and final barrier release. Lock order is caller accounting then
gate; no native callback runs under either lock. The gate itself serializes
admission with owner close and native cleanup. All new lock-taking entry paths
check process identity before touching inherited locks after fork.

A caller ticket keeps native cleanup nonquiescent. `finish_close` also refuses
while any caller handle remains, even if the queue and arbiter have no work. A
successful `finish_close` means only that the integration barrier was released.
It neither invokes native cleanup nor changes unknown native queues to idle.
The complete native cleanup must still run through `CameraNativeBinding::cleanup`.

This is lifetime retention only. It grants no graph-read scope, mutation
exclusion, source-arbiter access, dispatch authorization or native owner execution
permission. `Runtime::camera_route` is unchanged. Moving `0x13a100` or any other
original to the incoming thread still requires the independently established
native caller/thread and source contracts, with retention before first lookup and
release after the complete original call. These CPU tests do not repair the live
blocked-owner routing failure or establish world-space stereo output.

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

The caller-only suite needs a C++17 compiler and user systemd, no private native
binary or generated fixture. Every native callback is explicitly modeled. The
test links the actual production owner, gate, native cleanup binding and queue.
Link-time retain/release wrappers provide deterministic race scheduling and
failure injection without a production test hook. Unused `RenderDebt` methods
are link-discarded in this small suite; the full composition below covers them.

```
python3 native-portal/live/camera_caller_test.py
python3 native-portal/live/camera_caller_test.py --sanitize
```

It covers a blocked owner with an independently retained incoming invocation,
close-before-retain and retain-before-close, nested independent handles, shared
gate capacity and slot reuse, stale identities, moves, cross-thread release,
release failure and retry, RAII fail-stop, partial bind, and fork refusal with a
real inherited locked caller mutex. Condition variables represent complete
original return, not native success. Cleanup callbacks are counted separately
from integration close.

For source-only full-owner regression runs, pass an existing saved opaque header:

```
python3 native-portal/live/camera_owner_test.py --saved-idle-fixture /path/to/camera_idle_fixture.hpp
python3 native-portal/live/camera_owner_test.py --sanitize --saved-idle-fixture /path/to/camera_idle_fixture.hpp
```

This option copies the saved fixture into ignored build storage without importing
or executing a native analysis runner. It reports saved replay rather than fresh
instruction evidence. The default workflow below still generates fresh evidence.

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

## Runtime integration boundary

The merged loader now installs the camera group and Runtime pumps its retained
owner FIFO. See [runtime integration](camera-runtime.md) for exact installed PCs,
positive real-router tests and limitations. Native source/provider admission,
precise rebind association, unclassified lost-pipe identity and co-owned mutation
over committed source work remain explicit unsupported cases. No live activation
or GPU result follows from the CPU fixtures.
