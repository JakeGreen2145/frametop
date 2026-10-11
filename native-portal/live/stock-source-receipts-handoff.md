# Stock source receipt handoff

## Delivered scope

Branch `feat/native-portal-stock-source-receipts`, base `33877a86468b2a4422c1014d81414e4136f46b3c`. The implementation is a production-usable collector/transport and a fold API on the real default `NativeSourceAdmission`. It is not installed in Runtime. No edits to Runtime, camera_owner, source_router, Evidence, recording, graph, native hooks or GPU/source-command settlement.

Files:
- `live/stock_source_receipts.hpp`, header-only collector, immutable receipt and fixed transport.
- `live/native_source_admission.hpp`, owner-only folding, shared journal transition helper, unchanged current-owner observation interface.
- `live/stock_source_receipts_test.cpp`, modeled events through actual `ClientSourceBinding`, gate and provider.
- `live/stock_source_receipts_test.py`, serial source-only build and test runner.
- This handoff.

## Parent wiring

1. Own one `StockSourceTransport(binding, version, render_thread_id, nonzero_runtime_id)` for the whole DORMANT binding. Its nonreused stream ID distinguishes different transport lifetimes as well. It must outlive all collectors. Capacity is 32 complete invocations before the one-way seal. Overflow is terminal, not drop-oldest.
2. Own a persistent `StockSourceCollector(gate, transport, {read, state, context})` per owning caller instance, for example a bounded TLS slot keyed by Runtime/PID/version. Do not share the ENGAGED `source_binding` instance. Every invocation calls `begin(actual_ticket, immutable_binding, version, method_pc, saved_entry, comp_base, client_base, verified_digest_pins)` before the unchanged complete original.
3. Minimal **RetainedCaller addition required from parent**: a synchronous callback exposing its existing `CameraAccessGate&`, original retained `uint64_t ticket`, and immutable binding only while that caller handle remains alive. No new retain, guessed ticket, mutable CameraOwner field, or owner queue operation. This lane did not edit or bypass RetainedCaller.
4. Route stock source module/PC events to this exact caller collector only under authenticated DORMANT attribution. `event` forwards the complete current Saved by const reference to the existing binding engine. `camera_return` accepts only the method's existing actual RET sites with restored entry SP and LR. No outer x0, inferred success, deferred native read or native method call exists in this component. A matching real RET still records actual completion after a previous observation refusal; it never clears the failure. `cancel` records invalidity, not completion; destruction with no matched RET records an abandoned gap.
5. Only call `held_disconnect(0x15fbb8, saved)` when the actual aggregate decision really selected the native held no-op. Ordinary empty Release-one is not sufficient.
6. Under the parent's all-stock-original exclusion, on the authenticated render owner, call `native_source.fold_stock_history(transport)`. This seals and drains in begin order. It refuses active/missing/abandoned history and poisons the provider on transport failure. The lower-level `seal` / `take` / `fold_stock_receipt(binding, version, receipt)` API is available, but the caller must check transport failure and maintain the same exclusion through folding/engagement. `take` cannot expose records before seal. An incomplete earlier invocation cannot be overtaken. Concurrent independent camera invocations conservatively invalidate the stream rather than inventing a serialization certificate; no native call waits for another invocation or consumer.
7. **Provider created before Run handoff**: use the optional third constructor argument `StockOwner`, an identity-only `bool(void*) noexcept` predicate checking Runtime PID and authenticated render TID, with no native reads. The two-argument constructor pins stock-fold ownership to its construction thread. Existing ENGAGED methods still use the existing Authenticate boundary unchanged. Tests cover both construction-thread ownership and explicit later render-owner identity.
8. Fresh `connect_eligible` / `borrow_ready` still calls the real current Authenticate boundary. Parent must move that boundary behind its engagement token/exclusion. Historical state never replaces current authentication. Do not delete `source_foreign`, suppress unsolicited ENGAGED poisoning, reset the provider per call, or allow this component to decide native routing.

Collector false means history unavailable. Parent must preserve the unchanged DORMANT native original and its results; it must not turn the collector's false into a stock abort or fake native return.

## Native read contract is still required

Read `SOURCE-CONTRACT.md` sections 3/5/7, `CLIENT-COLLECTOR-SCOPE-SUPPLEMENT.md`, and `EARLY-API-DECISION.md` from the supplied Desktop handoffs. No binary analysis or native runner import was performed here.

`StockSourceReadBoundary` is deliberately required, with no default self-process reader. Both callbacks receive the current binding, real ticket, version, begin sequence, method, module/PC, Runtime identity and synchronous Saved. They must validate the exact permitted address/byte range and current native owner/lock scope before copying. The temporary `StockSourceReadScope*` in the collector exists only during the synchronous `event` call and is cleared before returning. Receipts contain values, never a Saved pointer, callback, read span, or outstanding native loan.

Specifically, the engine's Update rereads at `327e54/32750c` and `327f70/327f74/32758c/327590` are not authorized merely by camera retention or successful Acquire/Return. The Desktop contract says `2584c0` invokes the native lock and `2584c4` ignores its bool. Parent needs a scoped **actual lock-result observation**, paired to the concrete accessor/native invocation and revoked at `258508`, or a different proved current-read boundary. Those hooks are not installed by this lane. Missing or false lock results must cause the read callback to refuse and keep engagement unavailable, not stop stock. The tests model this witness explicitly and reject absent/false witnesses. They do not prove or install that native witness.

The before/after State callback needs its own exact current-method proof for camera IDs at `+350/+358`, active `+34a`, connected `+360`, external `+349`, module identity and association. It cannot reuse the broad owner Authenticate callback on a foreign caller. Gate retention does not prove field independence. Keep the queue-object shared_ptr, acquired metadata handle/slot reference, and copied history distinct. No producer generation, membership-generation ownership, metadata-return settlement, new Acquire, new Return, or native waiter cancellation is inferred.

**Ownership reconciliation:** despite the ambiguous EARLY-API-DECISION line 18, a foreign historical NewEntry Connect is never a portal-owned NativeConnection here. Both new and exact-ID dedup stock Connect establish borrowed history only. Only a subsequent genuinely current owner Connect through the existing path can gain local Disconnect authority. Full observed historical Disconnect can establish the existing detached-history distinction, not global manager/waiter/producer drain.

## Verification

From this worktree:

```sh
python3 native-portal/live/stock_source_receipts_test.py --sanitize --runtime
```

Exit 0 with AddressSanitizer and UndefinedBehaviorSanitizer. Every compile/link/test runs serially under a `MemoryMax=256M`, `MemorySwapMax=0` user scope with scratch TMPDIR. The runner parses only the literal Runtime `SOURCES` assignment with Python AST. It never imports an evidence/native-analysis runner. Logs are in ignored `native-portal/build/stock-source-receipts/sanitize/results.json`.

Verified:
- Actual modeled full stock new and dedup Connect, ordered independent caller Connect/Release/Connect, and first-history warm Update through real paired metadata acquire/ref-store/return events become eligible for fresh authenticated borrowed use.
- Folding invokes neither historical native reads nor Authenticate. Tests clear all modeled native memory, disable its read boundary, and check all read counters remain unchanged across fold. Fresh readiness rejects missing authentication and does not reuse the old snapshots.
- Per-queue raw zero/nonzero results and partial successful peers survive in receipts; partial history cannot gain capability or recover from later cold/On-like scalars.
- Exact-ID alias checks, malformed after-state, early-out Acquire, empty Release without Disconnect, held Release, full/failed Release, missing metadata Return, duplicate queue IDs, missing/false lock scope, missing/incorrect method RET/SP/LR, nesting, wrong binding/version, replay/out-of-order records, unexplained between-call state mutation, overlap, capacity overflow, foreign caller/fold and fork refusal.
- Cancellation, actual completion and abandoned invocation are separately counted. None clears sticky invalidity.
- Existing `native_source_admission_test.cpp` is unchanged and passes, including locally owned transitions, literal partial failures, two-eye borrowing and settlement watermarks.
- Complete unchanged Runtime and its production source list compile and link; existing `runtime_test.cpp` passes with `runtime: default stock continuation, exact pin, unsupported ownership, bounded real-memory queue payload parsing PASS`.

TDD RED observations before the corresponding fixes:
- Initial compile failed because `stock_source_receipts.hpp` was absent.
- `failed_warm_history` failed because incomplete warm metadata history incorrectly folded successfully.
- The independent-caller test failed because a foreign caller could select itself as another transport's owner and reach the provider fold. The provider now checks its own immutable owner boundary first.
- A complete Connect with contradictory inactive after-state incorrectly folded successfully.
- A complete warm Update with identical queue IDs incorrectly folded successfully.
- Completion-status test did not compile before the status API and completed-versus-abandoned accounting existed.

The focused RED compile command was the same runner once present. Earlier RED/GREEN used `systemd-run --user --scope -p MemoryMax=256M -p MemorySwapMax=0 g++ -std=c++17 -O0 -g1 -Wall -Wextra -Werror -pthread --param=ggc-min-expand=10 --param=ggc-min-heapsize=4096 native-portal/live/stock_source_receipts_test.cpp native-portal/live/source_provider.cpp native-portal/control/camera_gate.cpp -o native-portal/build/stock-source-receipts/test`, followed by that binary.

## Link delta and remaining work

**No new translation unit or link dependency.** The collector calls the existing `source_provider.cpp` and `camera_gate.cpp`; all implementation additions are inline. Production `live/build.py` and `live/runtime_test.py` already register those sources. Direct source-provider/metadata tests continue using the existing binding source. No runner consumer needs a new object. Full Runtime source-list link was exercised as above.

This is component GREEN plus unchanged Runtime build/smoke, **not DORMANT Runtime integration GREEN**, native read-table proof, installed hook coverage, native lock-result capture, GPU proof or headset testing. Parent still owns TLS/routing, all-originals exclusion, retained-ticket callback, identity-only stock-fold owner predicate, native field/lock scope integration, and combined Runtime positive/negative acceptance. Independent pre-merge review remains parent-owned; no independent reviewer was available within this subagent's local tool set.
