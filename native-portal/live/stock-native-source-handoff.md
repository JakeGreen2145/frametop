# Native stock-source fields and read scopes

## Delivered

Base `bfad7b1a9d79482345b92b4c5b8eb7270289322d`, branch `feat/native-portal-source-native-facts`.

The real stock collector now records native field facts instead of reading camera tuples at entry/RET. The real default provider folds masked path facts, with no before/after or adjacent tuple equality. Both stock NewEntry and dedup Connect remain borrowed. Full Release still needs actual two-queue raw0 and committed inactive/disconnected facts; empty Release1 cannot detach. Neutral Update does not seed warm history. Failures, sequence gaps, overlap, provider/selected-ID changes and partial work remain sticky.

The existing ENGAGED Authenticate and transaction/debt policy are unchanged for compatibility. They are NOT certified native-safe current admission by this work. The final current-readiness contract arrived during implementation. Its separately tested identity/bootstrap/current-render implementation is a follow-up, with concrete proposed API and native coordinates in `native-source-current-api-handoff.md`. Parent explicitly allowed completing this collector first. No Runtime, router, camera_owner, Evidence, loader, manifest or prepare file was changed.

## Implemented files/API

- `stock_native_facts.hpp`: six-bit known mask, exact byte width, path and per-field module/PC/method/runtime/version/sequence/ticket/binding/PID/TID/SP/LR provenance. Client is the client module base, not the manager. Unknown never implies zero.
- `stock_native_boundary.hpp`: actual native allowlist and per-direct-manager/shared-guard state. Uses the existing engine's immutable pending-call coordinates. Injected byte backend or bounded `process_vm_readv` runs only after exact native checks. No parallel queue-observation engine.
- `stock_source_receipts.hpp`: collector, retained immutable receipt, one-way transport, candidate/actual completion split.
- Narrow `source_provider.hpp/.cpp` and `source_metadata.inc` changes: explicit `NativeSelectedId` input replaces redundant mutable ID reads only in opt-in native-input mode. `native_connect_output(Saved)` captures actual client `32a628` output store; raw0 continuation is still mandatory. Default legacy engine behavior is unchanged.
- `native_source_admission.hpp`: stock-only masked fold. Existing ENGAGED `apply_observation`, current authentication and debt guards are preserved.

Construct the collector with:

```cpp
StockSourceReadBoundary backend{
    read_backend_or_null, context, covered, provider_live
};
StockSourceCollector collector(gate, transport, backend);
```

`covered(context, Module, pc)` must consult the trusted installed/pinned inventory. Missing any required callback refuses begin. `provider_live(context, scope, provider)` must prove the conditional same-original provider lifetime and loader-pinned immutable vtable without camera tuple reads. Camera retention alone is not that proof. Null/missing qualification refuses begin. `read_backend_or_null` is only a bounded copy backend; it does not decide permissions. Null selects fault-contained self-process reads.

The complete signatures and site inventory are in `stock-native-source-hook-request.json`. The required compiled set has 75 unique callbacks, including 23 additions to the old engine group, three method entries and the existing held decision. The runner compares JSON against the actual compiled inventory and tests refusal with each required site missing. This is requested coverage, not a claim that the loader installed it.

## Parent call order

1. Keep one transport for the DORMANT binding/version and explicit render owner, capacity32 unchanged. Keep separate per-caller collector instances. Do not reuse ENGAGED source-binding state.
2. Enter parent `RetainedCaller::observe_scope(const Observer&) const`. Its callback receives the existing `CameraAccessGate&`, ticket and immutable binding. The caller handle must remain alive, unmoved and unreleased through the complete native method. This grants association retention only.
3. Inside that scope call `begin(ticket, binding, version, method, entrySaved, compositorBase, clientBase, exactPins)`. Save `const auto call = collector.invocation()` immediately after successful begin, before native execution. It includes runtime/version/stream/sequence/ticket/method/SP/LR.
4. Run the unchanged complete native METHOD bypass, not only an outer service handler. Match nested calls on the parent's native-method stack. Route old engine PCs to `event(Module, pc, Saved)`. Route new field-only PCs to `camera_fact(Module, pc, Saved)`. Route new guard/manager-only PCs to `boundary_event(Module, pc, Saved)`. Do not send unsupported new PCs to the binding engine.
5. `camera_return(pc, Saved)` only captures the exact method's pre-instruction RET candidate. It performs no native read and does not publish. After the same `ft_native_portal_call_saved` actually returns, invoke `complete_own_bypass(call, actual_x0)`. Preserve raw successful zero/negative values. Never use an outer-handler result or invented caller continuation. Stale completions with reused SP/LR fail on invocation identity.
6. An actual aggregate-held decision can call `held_disconnect(0x15fbb8, Saved)` at the real method bodySP. It is not inferred from Release1. All callback false results mean history unavailable; they must not abort, skip or alter stock native execution.
7. Only after the actual method and completion callback may the retained caller handle be released. Cancellation is not completion. Missing pre-RET or actual completion remains a gap/abandonment.
8. On the authenticated render owner after all stock-original exclusion, call `fold_stock_history(transport)`. Folding reads no native memory and calls no Authenticate. Use explicit identity-only `StockOwner` for provider construction before Run handoff. There is no per-On reset.

## Read permission details

- Exact provider[0] and pinned vtable slots at matching native dispatch, qualified by `provider_live`.
- Connect header ID is exactly the native next u64 load at `32a49c`, under its new-mapping path, not the metadata shared guard.
- Camera IDs are never reread. Connect uses actual `32a628` output plus raw0; Release/Update use named native x1 selections and existing exact pair checks.
- Exact Update private caller slots `P+0x90/P+0x98`, P=entrySP-0x360, only on their successful own/property continuations. No pointee dereference implied.
- Extra metadata header/ref reads require direct-manager lock raw0 at `3280ec`/`327678`, paired selected inner invocation, guard begin `258438`, actual nonzero uint32 bool at `2584c4`, and exact guard address/bodySP/queue/provider. Guard addresses are innerSP+0x58 for Acquire and +0x48 for Return.
- `2584f8` is optional and does not revoke the guard. `258508` revokes before unlock. Manager unlock `328150`/`3276d8` separately ends map qualification. Acquire and Return require independent guards. Missing/false guard blocks added reads, never native forwarding.
- Header reads are 8 bytes. Ref reads are 4 bytes, stride8, `[header+0x68,header+0x468)`, all 128 slots. Overflow, wrong widths/addresses, count wrap and wrong paired Return remain refused. Existing engine rereads cover the retained native retry path; register-ref replacement and its `327624` observer are not needed in this version.
- The actual conditional no-lock mode has no executed qualification hook in the supplied table. It remains unavailable. No waiter `3281a8` semantics or guessed threading-global read is used.

Both Release external0 producers `15fbd0` and `15fc08`, and Update's actual committed external store at `16013c`, are supported. No missing external fact becomes zero. Legitimate external changes do not invalidate history; existing current eligibility remains separate.

## Verification

All compiler/link/test processes ran serially in user scopes with `MemoryMax=256M`, `MemorySwapMax=0` and scratch TMPDIR. No SSH, device, camera, SteamVR or Runtime activation.

```sh
python3 native-portal/live/stock_source_receipts_test.py --runtime \
  --native-vectors native-portal/build/stock-native-contract/native-vectors.bin
python3 native-portal/live/stock_source_receipts_test.py --sanitize --runtime \
  --native-vectors native-portal/build/stock-native-contract/native-vectors.bin
```

Both exited0. Each run had 32 successful capped commands, including 22 serial compiles. The real collector/default-provider tests, unchanged admission test, and complete production Runtime link/smoke passed. ASan+UBSan reported no finding. Logs are `native-portal/build/stock-source-receipts/{plain,sanitize}/results.json`.

`stock_native_vectors.py` separately verified both ELF hashes and the delivered permission-table hash, checked observer guards, executed 16 pinned field producers in Unicorn, and generated 30 full 800-byte Saved vectors. Both normal and sanitized native-contract binaries replayed those vectors through the real collector. This includes client `32a624 ->32a628` and both Release external stores. It is isolated pinned instruction execution with synthetic input memory, not whole-native-method or loader-island coverage. The full Connect/metadata/guard paths in the C++ fixtures remain explicitly modeled native schedules.

The vector generator requires explicit `--permissions`, `--compositor`, `--client`, `--python-deps` and `--output`. This run read the immutable ELFs in `/home/jakeg/.hermes/work/frame-native-reuse/camera-analysis/`, the delivered permissions under `continuation16-source-boundary-f17510461c5d/`, and the already-installed Unicorn dependency in `/home/jakeg/.hermes/work/frametop-native-native-stock-facts/native-portal/runtime/build/native-stock-facts/python`. It did not install or modify that lane. Generated vectors/evidence are in ignored `native-portal/build/stock-native-contract/`.

Tests cover cold stock new/dedup borrowing, warm external changes both ways, fresh current authentication, complete/held/partial Release, output-store fact provenance, exact guard scopes/false/missing/reused/wrong guard, another queue/provider, all ref slots, overflow/width/range, callback read revocation, real PROT_NONE reads, no camera entry/RET reads, no fold reads, missing completion, stale completion, wrong method/K/SP, replay/version/process/thread/overlap/capacity and sticky failures. New REDs observed before repairs were entry tuple reading, cross-method allowlist access, stale completion accepted at reused SP/LR, and a byte-backend call after lifetime revocation inside provider qualification. Test assertion mistakes encountered during development were corrected before counting GREEN; notably `borrow_ready` has no existing camera.external predicate, so current external refusal is tested through `connect_eligible`, not invented in borrowing.

## Remaining integration blockers

Parent owns independent delta review and Runtime routing/exclusion. Loader owns actual installation, null candidate-kind emitter resolution and byte-preservation testing. `provider_live` needs the real retained provider/module qualification, not an always-true callback. No-lock manager mode is intentionally unavailable. Current AuthenticateIdentity/bootstrap/CurrentRenderSelection remains the concrete follow-up described above, and the old broad state callback must not be called production-ready. No source/GPU/command settlement or automatic local Disconnect authority was added.
