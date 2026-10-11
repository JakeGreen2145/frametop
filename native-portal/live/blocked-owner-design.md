# Blocked render-owner callback: source reproduction and repair boundary

Base: `a0434e89bcea87dd201c3718da369a8bf6dca17d`.
Production routing is unchanged. This is a design for reconciliation, not an approved patch or a native safety proof.

## Reproduced result

`python3 native-portal/live/blocked_owner_test.py --regression` exits **1**, product regression RED.
`python3 native-portal/live/blocked_owner_test.py` verifies the expected current failure separately.
Artifacts are under `native-portal/build/blocked-owner-dispatch/`, including actual child logs, compiler commands and `results.json`.

The fixture includes the existing `Chain` with the default production `Runtime()` and its real `NativeSourceAdmission`. It compiles the complete literal production `SOURCES` list from `runtime_test.py`, parsed as data, without importing that runner. It reuses saved private header constants and saved callback coordinates. Its startup is modeled completed publication -> first Init -> renderer initialization -> worker publication/Start -> authenticated Run on a real second thread. No ELF loading, vendor instruction execution, Vulkan or SteamVR occurs.

A condition variable holds that owner after authenticated bind. A separate listener enters `Runtime::event(0x13a100, Saved&)`. The modeled original has no nested camera operation, records its actual executing TID and call count, and returns a distinctive value. The connection-reference storage is valid for the complete call. This is explicitly a source model of an outer-only handler, not evidence that any particular native LostPipe branch is camera-free or cleanup-safe.

| Case | Real child result | Original calls | Meaning |
| --- | --- | --- | --- |
| Original-on-caller modeled baseline | 0 | 1, on listener | Original can return while owner remains paused; bypasses Runtime routing deliberately |
| Responsive owner, actual Runtime | 0 | 1, on owner | Real route, FIFO execution, exact modeled result and return selector work when pumped |
| Paused owner, actual Runtime | SIGABRT, Python `-6` | 0 at SIGABRT | Exact production `CompletionTimeout`, reason 11, event `0x13a100`, bound foreign caller |

The first two controls perform owner stop, real C++ join, modeled exact native join events, original stock cleanup and camera destruction, then destroy the Runtime normally. They do not `_Exit` around its barrier. The failing case observes the production abort with an async-signal-safe count marker. There is no trailing `assert(false)` and no test-generated abort substituted for the guard.

Each case asserts bound lifetime retention, authenticated Run, zero frames/uploads/pending, no portal control request and no native Acquire/Release before the stimulus. Zero render/camera debt follows this executed prelude and the absence of a portal token, not a claim that all native obligations disappeared. The bound barrier is explicitly asserted **present**.

Queued/NotExecuted is a source-level deduction, not a new sampled private-slot dump: the exact timeout is reachable only after a nonzero submission at `runtime_camera.inc:363-368`; the listener cannot execute inline; the sole owner is blocked before any pump; nothing closes or cancels the queue; the original-call count is zero. `OwnerCoordinator::submit_scoped` publishes Queued at `control/owner_coordinator.cpp:73-79`, and only the owner can change it to Running at `100-114`. No slot is assigned a historical native originating PC by this test.

The runner drains a socket-backed stderr concurrently because the real fatal marker uses nonblocking `send`, not stdio. Compilation and child execution are serial in a user systemd scope with `MemoryMax=256M`, `MemorySwapMax=0`, low GCC GC thresholds and the configured scratch TMPDIR. Headers are copied into the isolated ignored build directory; parent files are read-only. Source hashes and opaque-fixture hashes are saved. This run is not sanitizer coverage.

## What the source establishes

1. All three outer handlers, including LostPipe, are in `writer_pc`, alongside app wrappers and actual camera methods. Every bound foreign writer is currently captured by reference into the owner queue. The callback is not limited to the camera mutation. See `live/runtime_camera.inc:90-97,305-375`.
2. `camera_original` explicitly disallows a foreign execution thread and changes owner-only `camera_active` and source-observer state. Merely calling it on the listener is not a split implementation. See `runtime_camera.inc:101-111`.
3. Source observation runs **before** camera routing. A bound foreign source observation irreversibly marks `source_foreign`; it does not borrow an owner's ticket. `ClientSourceBinding` and `NativeSourceAdmission` are owner-scoped. See `live/runtime.cpp:435-438`, `live/source_router.inc:19-68`, `live/source_provider.cpp:16-46` and `runtime_camera.inc:113-127`.
4. Current nested calls inherit one owner-only `CameraAggregateScope`; outer request/reply bookkeeping remains in the original. Camera methods produce their actual native results, including aggregate-held native return/unlock paths. See `runtime_camera.inc:48-89,248-261,314-356`.
5. A gate ticket retains lifetime, not exclusive access to live camera fields. Gate retain/lookup is thread-safe, but `CameraOwner::retained_binding` and `observe_scope` are owner-only; the latter exposes only the currently executing mutation's retained ticket. `CameraOwner` retains a barrier at bind, before any On. See `live/camera_owner.cpp:31-37,54-79` and `control/camera_gate.cpp:50-73`.
6. Native cleanup is already installed and closes the exact bound epoch. It does not have a debt-free foreign fast path. Bound cleanup requires owner or successfully joined bootstrap, drains genuine obligations and then uses the real destructive gate. See `runtime_camera.inc:286-303,422-450`, `binding/camera.cpp:71-121`, `control/camera_gate.cpp:91-113`.

## Recommendation after Desktop clarification: B first, source protocol blocked

The parent's mid-task Desktop clarification supersedes the initial preference for A. **Recommend B, a bound-but-never-engaged stock-equivalent mode, before considering A.** No fresh opaque callee export is needed merely to preserve unchanged stock scheduling if complete integration exclusion is proved. Do not treat the engaged A native proof gates below as prerequisites for an entirely unused, correctly excluded B mode.

I did not establish a narrow complete B protocol in this source baseline, so I have not implemented a production bypass. This is a source-integration blocker, not a request for another headset trial or more opaque decompilation. The exact missing part is lossless exclusion of the already-installed mandatory native observers, including their paired state across callback returns, while preserving later readiness. A mutex around first portal On or an active-original counter alone cannot supply that protocol.

### Concrete B protocol and proposed change locations

Use one monotonic admission state machine, owned by Runtime, under a short mutex: `Unused -> Engaged`, plus terminal `Closing/Invalidated`. Track complete stock originals, admitted integration readers/calls, and association version. No transition back from Engaged on Off. No lock spans native code.

1. At `runtime_camera.inc:286-305`, after the pre-bind route, attempt B admission for the complete bound original. Reserve/count under the same lock used by first integration entry; retain and validate the immutable camera binding before native pointer lookup. Require Unused, unchanged association, no admitted readers or cross-callback scopes, no Adapter native allocation attempt/pending work, no source borrow/history transaction, no Command/Lifecycle/source-command obligation, and no Retaining/Queued/Running/Releasing/Quarantined mutation or capture. A stale mailbox is not this proof. The bind barrier remains real and live.
2. Execute the loader's complete original exactly once on the incoming thread. Use bounded TLS keyed by Runtime, PID and association version so every nested installed camera/app/source hook inherits stock mode rather than independently queuing or touching owner-only observer state. Complete stock mode lasts through the native return, not just entry/epilogue admission. Do not peek at connection `+0x86`.
3. Make first integration entry atomic with B admission. It must precede the **first conflicting reader/observer or native resource attempt**, not merely portal token creation. Actual source locations include `runtime.cpp:106-130,300-342,435-438,512-527,622-712,727-808` and `runtime_camera.inc:113-150,324-409,422-450`. If Unused originals are active, do not admit those operations. Once Engaged, retain all existing queue/debt/fail-stop routing.
4. On complete original return, validate that its association version was not revoked, release the exact retained handle and decrement the original count under the admission protocol. A nested cleanup or replacement invalidates the version before forwarding; its return cannot resurrect admission. Bound destruction and exact native join restrictions remain separate.
5. Owner-only outstanding scopes need explicit authoritative inspection, not a foreign read of `Impl` optionals. Add a non-reading scope/admission contract in `Evidence`, `CameraOwner/OwnerCoordinator`, source binding and Adapter integration before claiming Unused. For readers whose scopes end before returning from one callback, reserve/release in the same admission protocol. For paired scopes, hold the reservation across both events and exclude B until the terminal event, or prove those scopes independent. Failure is conservative and must not grant stock passthrough over a partial upload.
6. A useful implementation must show an original with nested native hooks running in B and then actual default-provider capability/first portal admission recovering after its complete return. This must include a command turnover and warm source transition during the original, not only an outer-only no-op. There is no such verified recovery implementation in this branch.

### Exact source blockers

| Required property | Current source conflict | What is missing |
| --- | --- | --- |
| Exclude all conflicting integration reads for complete original | Runtime runs `source_route` first and owner maintenance/graph/Evidence observation unconditionally, even without a lease, `runtime.cpp:435-438,512-527` | One atomic read/stock admission protocol covering these paths; current `camera_join_mutex` protects only joining/publication |
| No already-admitted cross-callback observer | `Evidence::graph_event` captures graph addresses and keeps `dispatch_/graph_scope_` until later native boundaries, `runtime/graph.inc:98-119`; `recording_event` holds `creation_`, Begin state and `turnover_` across callbacks, `runtime/recording.cpp:4-99` | Exhaustive, owner-published idle/reservation facts and independent-scope proof, not `camera_debt==0` or `queue_active==false` |
| Preserve required observations while stock runs | Complete command creation/Begin/publication and turnover require native reads at their actual suspended boundaries, `recording.cpp:32-99,101-128` | Passing them through needs a demonstrated independent native read scope. Skipping them loses the chain. Deferring Saved pointers alone cannot preserve stack/object lifetime |
| Recover rather than permanently refuse | `Evidence::fail` clears spans/recordings and latches `failed_`, `runtime/evidence.cpp:55-60,98-117`; publication requires known begun member and full turnover | A proved recoverable pause/resume scheme or explicit independent observer class. No existing reattach-from-current-pointers API supplies equivalent authority |
| Preserve usable source readiness after nested stock | Foreign source hooks latch `source_foreign`, `source_router.inc:58-64`; source binding is owner-only; default provider tracks complete paired queue/metadata history and sticky uncertainty, `native_source_admission.hpp:14-90` | Stock-mode observation policy that neither races owner state, silently drops needed history, nor clears uncertain history. Nested passthrough alone does not establish warm recovery |
| Preserve native scheduling | Waiting an owner callback until a foreign original completes would add a new native-thread dependency | Source does not prove that waiting cannot obstruct stock's own worker/IPC progress. No mutex across native calls is necessary but not sufficient |

This does **not** prove B impossible. It shows why I cannot safely implement it as a bounded routing-only change. Gating only optional frame admission is incomplete; gating every event by dropping it fails future authority; blocking every event risks changing the scheduling B is intended to preserve. The next bounded source task is to classify mandatory observers and prove their independent scopes or design recoverable no-read pauses, then test positive recovery through actual Runtime. No production policy should change before that proof.

## Deferred A source map: caller outer scope, owner mutation envelope

A remains a concrete seam, but is **not recommended for implementation now**. It keeps the existing renderer, Adapter, owner queue, source observer and native cleanup gate. Its limited outcome is that an outer invocation which never reaches a camera mutation no longer needs render-owner progress merely to execute its original. It does not make an active nested camera Release safe or bounded while the owner is blocked.

The following map answers the requested source review. It is not a second implementation proposal to pursue in parallel. The current fail-stop is retained.

### Exact proposed source edits

#### 1. Add a caller lifetime scope, not an owner mutation on the caller

In `live/camera_owner.hpp/.cpp`, add a small explicit retained-caller handle API, for example `retain_caller(epoch, generation, handle)` and `release_caller(handle)`. The handle contains a nonreused gate ticket and the immutable `CameraBinding`. Validate process, immutable epoch/generation, successful gate retain, then gate lookup, in that order. Do not expose `DemandArbiter`, `current_ticket_`, `active_`, or owner-only provider state to a listener. Closing races must refuse before native lookup. A failed release keeps the real ticket accounted for and cannot be reported as completion.

The existing bind barrier remains. Caller tickets prevent cleanup from freeing the generation while the original outer invocation or nested call is suspended. They do not grant graph reads, connection-object lifetime, mutation authority or GPU settlement. `finish_close` cannot make native cleanup pass while a caller ticket survives, because the gate remains nonquiescent. Preserve self-wait refusal if an outer invocation reenters destructive cleanup.

In `live/runtime_camera.inc`, introduce a bounded RAII thread-local outer context stack keyed by `Impl*`, PID, gate epoch/generation and retained handle. Store entry PC and caller attribution as immutable scalars. Do not turn the existing owner-only `camera_active`, `camera_caller`, `camera_aggregate` or `source_observing` into shared cross-thread fields.

Retain **before** dereferencing the connection reference, app.camera, context.application or scheduler fallback. At outer ingress, only scalar comparisons against the immutable retained binding are allowed until the handle is valid. If caller attribution requires the connection-reference load, do it within this retained scope and only once its own native connection lifetime is proved. A camera gate does not independently retain a connection object. Never peek at connection `+0x86` to predict whether the native handler will mutate the camera.

#### 2. Split only the proven outer handlers before the writer queue path

At `runtime_camera.inc:305`, separate `0x1366b0`, `0x136118`, `0x13a100` from mutations. Execute their **complete** loader-provided original bypass synchronously on the incoming caller, inside the retained TLS scope. Add a dedicated `camera_outer_original` helper that invokes `ft_native_portal_call_saved` without touching owner-only activity/source state. Do not remove the owner check from `camera_original` globally.

After the complete original returns, preserve its result lanes, release its caller scope, and set `Saved.pad=SkipOwned`. This is the existing return-function emitter's method-return route. Do not return relocated-original while a stack-local scope has already expired, and do not replace the handler's return with an inner bool. Keep every native guard, scheduler/list/shared/fence/focus effect, response bit, connection marker and cleanup action in the original body.

For the first bounded change, keep stock mode entrypoints `0x15d730/0x15d198/0x15d248` on their current route. They are not proved equivalent outer wrappers by this fixture. A later change needs their own recovered envelopes, not an expanded allowlist inferred from common descendants.

#### 3. Queue the whole app/camera envelope at the nested boundary

Keep app wrappers `0x6d6e0/0x6d6f0` intercepted **before their native app.camera load**. When they occur under the new TLS scope, capture the scope's authenticated `Caller` by value and queue the complete wrapper using the current bounded FIFO. The nested `Saved&` stays synchronously alive until actual completion and retention release. Its worker callback validates held generation/binding, then runs the existing `CameraAggregateScope`, debt check and original wrapper. The wrapper's camera method continues to route through `camera_method` on the owner.

This is safer than letting a foreign app wrapper dereference the camera and queuing only Disconnect. Keep the complete `0x15fba8` Release envelope, including bank clear and actual native return branches. Keep direct camera entries `0x15f9e0/0x15fba8/0x15f938/0x15faf8/0x15fcc0` on the existing retained owner queue. Inherit attribution from TLS where authenticated; direct calls without an outer scope keep their existing classification. Nested app wrappers must not become independent demand callers just because execution moved between threads.

Capture the TLS attribution explicitly into the queued mutation; the owner's TLS is not the listener's TLS. Owner nested execution continues using `camera_aggregate/camera_caller` as today. Preserve distinct per-connection identity rather than collapsing all handlers to application or context.

Leave `camera_debt || adapter->state().pending` at the actual mutation boundary. Do not charge an outer-only handler as a mutation merely because it can reach one on another branch. Conversely, do not forward destructive inner work over any partial upload, source, CPU or GPU obligation. The guard remains required even if this prevents an active handler from completing.

FIFO linearization remains the real mutation submission, including direct-owner work through that same queue. An earlier Retaining slot cannot be overtaken. Restoring native concurrent outer scheduling necessarily stops serializing all outer side effects in queue-arrival order; native analysis must approve that change. It is not acceptable to claim that the old total ordering of complete handlers is preserved. Never reserve an unready queue head and let the owner self-wait behind it.

#### 4. Preserve actual native continuations and source observation

Do not change `runtime-call.S` or the existing method/decision emitter classes as the first design step. Current source provides complete-original bypasses and correct selector distinction:

- `live/loader.c:39-55` exposes only installed emitter-6 original bypasses and all 20 camera boundaries.
- `live/runtime-emit.c:36-57,69-87,100-120` distinguishes complete method return from interior `15fbc8` no-op and `15fb74` unlock continuations.
- `live/runtime-sites.json:1440-1517` describes app wrapper/outer handler return-function routing.
- `live/runtime-call.S:30-64` transports x0-x7, x8 indirect results and q0-q7 through a synchronous complete call while preserving the suspended callback's saved return address.

Keep native source events and `source_begin/finish` on the owner inside the queued app/camera call. Preserve the actual return-hook set `15fa78/15faa4`, `15fbd8/15fc20`, `15fdc8` in `source_router.inc:49-65`. An outer result must not complete a missing nested return. Do not suppress the global foreign-observation poison to make the design appear usable. If an original outer callee reaches a source event outside the classified nested envelope, that is a missing boundary/proof gate, not an ignorable event.

Desktop identifies the relevant LostPipe sequence as guarded `6d6f0 -> whole 15fba8 -> 13a3a8`, with the native ignored bool and later `+0x86` clear preserved. This source-only test does not execute or certify that sequence. Revalidate the shipped loader-selected bypass, callback Saved ABI and all real return branches independently before installation.

#### 5. Leave lifecycle and pending Off truth intact

Keep publication/first Init/Start/Run joining, unbound-original counters, destructive overlap invalidation, bound cleanup thread restrictions and exact worker join unchanged. Caller scope admission must fail once closing starts; its complete original must return before its ticket can release. Do not reinterpret No On as unbound or call `camera_unbound_original` after a published epoch.

Keep the existing `RenderDebt` and Command/Lifecycle settlement as the only completion authority. `camera_debt=0` can precede a charged upload; an Adapter pending state can survive zero camera debt. A stopped lease, drained listener queue or completed metadata Return does not settle source command references. No alternate source/GPU registry or early Off acknowledgement is part of this design.

## B exclusion inventory: bound-but-never-engaged is not a lease predicate

Such a mode is possible only with an explicit one-way admission transition and read exclusion, not `if (!portal_token || !camera_debt) original()`.

A candidate mode would have to count **complete** stock originals under the same lock as first portal admission, retain their native lifetimes, close new passthrough admission, wait for existing originals and integration readers to leave, revalidate state, then irreversibly enter serialized integration mode. Closing must not overtake any active original. Resetting to this mode after Off would require real source/CPU/GPU settlement, not just token loss.

Current source prevents treating this as a local branch:

- Bound barrier and cleanup obligations exist immediately at Run, `camera_owner.cpp:54-64,91-109`.
- Before On, the owner already reads context/app/scheduler/camera/renderer association and camera state during seal, `runtime.cpp:622-676`. `camera_ready` invokes mutable provider eligibility. A lease-only exclusion does not cover these readers.
- `source_authenticate` reads app.camera, camera owner, renderer/device and active/connected/ID scalars, `runtime_camera.inc:113-127`. The gate prevents destruction, not concurrent field mutation.
- Bound foreign source events currently poison admission, while `ClientSourceBinding` is owner-only and persists across calls, `source_router.inc:19-68`, `source_provider.cpp:16-46`. Dropping observation or resetting the provider on a later On would discard partial-connect and warm-history uncertainty.
- Adapter construction is possible when capable before a lease, `runtime.cpp:671-702`; private allocations, Evidence graph/recording readers and source tracking must be audited separately from portal tokens.
- Adapter staging and source borrowing occur before dispatch-time `camera_commit_debt`, `runtime.cpp:727-742`, `runtime_camera.inc:129-138,411-420`. The private source command's exact sampled-use and reverse-release proofs survive Off, `source_command.inc:19-29,62-163`.
- Committed pass readiness, native queue success, exact GPU completion, CPU disposal/reset and source settlement are independent facts. `RenderDebt::settled`, `camera_owner.cpp:119-131`, deliberately requires all of them. `camera_maintenance:431-449` also retains partial-upload debt before camera debt exists.

A correct dormant passthrough needs a complete source exclusion and observation contract, beyond execution routing. If that contract establishes unchanged stock execution without overlapping integration work, Desktop's clarification allows B without demanding new opaque native callee proof. That contract has not been established here. B is the recommended first candidate; its current blocker is mandatory-observer preservation and verified recovery.

## External proof gates and present stop decision

The Desktop report supplied by the parent is `bounded-owner-wait-export-13-c7bf964217fb/BOUNDED-OWNER-WAIT-EXPORT.md`. Its lines 83-105 match this source baseline and delimit what is not yet proved. Its selected acquire path encodes `UINT64_MAX` with uncapped retry. It supplies no 250ms service contract and does not prove the actual driver deadlock cause or historical queued-slot PCs.

The following external gates apply to the deferred **engaged A split**, not to an entirely unused B mode with complete stock-equivalence exclusion:

1. **Complete outer native lifetime/lock contract.** Prove caller-thread execution of all preserved outer effects cannot invalidate objects retained/read by the renderer while its acquire is blocked. Account upstream caller locks and unresolved `1071f8/1398c8/60570/605d8/accessor` semantics. Ending local list/shared/worker lock scopes before `6d6f0` is not enough. The camera gate alone does not retain every scheduler, connection, shared target or fence resource.
2. **Connection and app-selection attribution.** Establish validity of the suspended connection reference and exact context -> primary app or scheduler fallback while a caller ticket exists. Prove all relevant camera mutations enter the installed wrappers/methods, including error, shutdown and reentrant paths. Do not infer safety by reading `+0x86` off-thread.
3. **Actual ABI and continuation equivalence.** Execute the shipped loader-selected outer bypass and nested `6d6f0 -> 15fba8 -> 13a3a8` path with Saved registers, indirect/SIMD results and every return branch checked. Confirm no untransported stack arguments or nonlocal exits defeat the synchronous scope, and that source hooks pair within the nested retained call. This fixture tests only the existing C++ boundary model.
4. **Active mutation progress.** A real nested mutation still queues behind the blocked owner. There is no proved finite pump/service bound, and camera/source service calls can themselves block. Do not raise the timeout, synthesize native success, cancel the capture then return, or add speculative acquire/wake/cleanup changes. Supporting arbitrary active callbacks requires a separately proved native progress/exclusion contract.

No safe complete repair has been established in this task. Pursue B's source-only exclusion/recovery proof first. Do not implement A for engaged operation, and do not treat this CPU reproduction as native progress or cleanup proof. Keep the current production guard while the mandatory-observer source blocker remains unresolved.

## Acceptance tests after reconciliation, not claims of coverage here

- Run this exact outer-only regression GREEN without changing its model or deadline. Keep responsive and original-on-caller controls separately labeled.
- Execute original public handlers and LostPipe shutdown/inactive/active paths, including their non-camera effects, through installed emitter/bypass tests.
- Use concurrent handlers, direct wrappers and direct camera methods. Verify inherited connection attribution, true false/zero/nonzero nested results, native response/marker writes, FIFO Retaining head exclusion and reentrancy behavior.
- Pause nested mutation, close concurrently, and verify stack/capture lifetime, no orphaned request, no fabricated Executed result, exact retention release and real cleanup refusal.
- Exercise all source RET branches, missing RET, partial Connect/Disconnect and unsolicited foreign source events without erasing provider history.
- Repeat zero-On joined cleanup, partial-upload-before-camera-debt cleanup refusal, both-eye committed work, queue/wait/disposal/reset/source settlement and repeated pending Release. No changes to truthful pending-Off behavior.

Only the first source model and its two controls were executed in this task. The rest remain required before enabling the proposed split.
