# Completed-wrapper sized-delete repair

## Outcome

Owned-source checkpoint against `06b4e8adbc62cf314ecfee3587d3ee195c011f94`. The complete modeled ordinary turnover reproduces the two original `0x4b3d0` refusals before the repair. The repaired default Runtime passes pre-Run turnover, real owner handoff, cold/warm stereo, pending Off and exact settlement. Source tests do not establish native execution or authorize activation.

**Native-stream validation is blocked.** Parent reported Desktop's complete instruction-stream worker failed with `content_policy_blocked`; its equivalent executable replay was stopped. No complete Saved stream was delivered. No alternate provider, new opaque extraction, native emulator run, headset trial or package installation was attempted here.

## Production change

- `runtime/wrapper_epoch.hpp` tracks one selected cleanup/replacement invocation with scalar-only wrapper allocation epochs and a consumed-retirement tombstone. Renderer/process/thread authority remains in the owning Evidence/collector. Wrapper epochs do not issue executable generations.
- `NativeStockFactCollector` and `Evidence` admit exactly one old-W sized delete after paired C-load, pool-call entry and successful actual `3052f0` return. The delete must be `4b3d0`, LR `M+2a23c0`, SP at the accepted outer body, x19=R, x0=x20=old W, and full-width x1=16. Selected-command/allocation collisions still refuse. The callback performs no native memory reads and changes no Saved lanes.
- Pool entry is not cleanup completion in either Dormant or Engaged mode. Both consume the already-installed `3052e0`, `3052ec` and `3052f0` observations. C-load/pre-call LR must be the same and one of the four delivered native alternatives. Cleanup's x20 is not constrained to W/R.
- Consuming the delete receipt retires only old-W storage authority. It does not erase C membership or settle Command, Adapter, GPU, CPU or source debt. Factory entry is the subsequent delete-return witness. A new paired constructor advances the wrapper epoch even at the identical address.
- Replacement publication requires the complete positive factory chain and committed `2a23f8`, including ordinary-path x20=0, matched C/pool and existing made/Begin/generation guards. Pre-store `2a23f4` no longer publishes in Engaged mode. A stale delete after the new constructor or publication remains fatal.
- Early, duplicate, wrong-PC, wrong-size/SP/LR/R/W/subject, missing/failed cleanup, current/factory W deletion and R/C factory collisions refuse. The retirement exception precedes generic command classification, so W deletion cannot erase the C ledger.

No SourceAdmission, Present/graph implementation, loader, manifest or hook inventory changed.

## Regression fixtures

`live/retired_wrapper_fixture.inc` is explicitly a contract model, not a native-generated event stream. It sends the complete ordinary selected-turnover callback sequence and checks all Saved bytes. Both self-read entrypoints reject attempted reads of the retired W range until a new same-address constructor establishes a fresh epoch.

The default current Runtime scenarios now perform mandatory pre-Run sized delete and same-address reconstruction. The owned settlement fixture also includes cleanup result, sized delete and complete replacement factory. Default dormant scenarios, selected-factory fixtures, legacy `bootstrap_test.inc`, stock-fact tests and the legacy Runtime turnover helper no longer skip those lifecycle events. CPU stack storage is explicitly 16-byte aligned.

The collector component test supports `FT_NATIVE_STOCK_SOURCE_MODEL` for a fully labeled source-model factory sequence when private native witness headers are unavailable. Its existing saved factory-only witness path remains separate. Neither path is described as the blocked complete native replay.

New default Runtime cases cover different-address replacement, repeated turnover before engagement and after engagement, turnover after settled owned work, all four cleanup LR alternatives, low-byte Boolean success, and paired dormant/engaged refusal cases. Missing cleanup return with committed owned work reaches the actual joined cleanup refusal with reason 5, event `8b020`, joined=1 and native_cleanup=0. No trailing fixture assertion supplies that abort.

## RED and GREEN

RED artifacts, retained in ignored task-owned build storage:

- `native-portal/build/dormant-runtime/current/retired-baseline-red.log`
- `native-portal/build/dormant-runtime/current/retired-baseline-red.json`

The baseline functional case reaches the modeled sized delete, records both first_stock_pc and first_evidence_pc as `0x4b3d0`, then fails first-stereo capability. Production edits followed this reproduction.

Final source acceptance, serial, 256 MiB RAM and zero swap:

| Suite | Normal | ASan + UBSan |
|---|---:|---:|
| Default current Runtime | 201/201 | 201/201 |
| Dormant Runtime | 7/7 | 7/7 |
| Evidence component executable | exit 0 | exit 0 |
| Stock facts component executable | exit 0 | exit 0 |
| NativeStockFactCollector component, explicit source model | exit 0 | exit 0 |

All four Runtime suite reports contain identical source hashes, verified against the final source tree. Expected fatal cases are accepted only with their production reason/event/join markers and no assertion or sanitizer error. Compiler warnings are errors. `git diff --check` passes.

Commands:

```sh
python3 native-portal/live/dormant_runtime_test.py --current
python3 native-portal/live/dormant_runtime_test.py
python3 native-portal/live/dormant_runtime_test.py --current --sanitize
python3 native-portal/live/dormant_runtime_test.py --sanitize
systemd-run --user --scope --quiet -p MemoryMax=256M -p MemorySwapMax=0 python3 native-portal/build/retired-source-tests.py
systemd-run --user --scope --quiet -p MemoryMax=256M -p MemorySwapMax=0 python3 native-portal/build/retired-source-tests.py --sanitize
```

The component runner is an ignored, task-owned helper which reuses the just-built production objects. It parses source-list literals without importing opaque-analysis runners. The tracked Runtime runner copies only its existing hash-pinned private fixtures.

`live/retired-wrapper-verification.json` records exact report hashes, final source hashes, RED identity, component results and the native-validation blocker. Full logs and reports are under `native-portal/build/dormant-runtime/`.

## Separate legacy smoke result

A supplemental compile of the older standalone `runtime_chain_test.cpp` main, without the default-current fixture, failed its bootstrap capability assertion. An isolated exact-HEAD source archive also failed that same assertion; its publication-good case failed baseline capability too. Those results are retained under `build/retired-legacy-baseline/objects/baseline-results.json` and `build/dormant-runtime/current/retired-legacy-results.json`. This is not a green legacy suite or a diagnosis of that separate baseline refusal. The opaque-importing `runtime_test.py` runner was not executed. The source checkpoint's acceptance is the default current/dormant Runtime suites and the three actual component executables above.

## Delivered native contract and review boundary

Read and consumed:

`/home/jakeg/.hermes/work/desktop-compositor-re/stock-turnover-free-06b4e8a/contract-90ecc77b3f8a/{REPORT.md,CONTRACT.json,VERIFICATION.json,PAN-VERIFIED.json}`

Report SHA256: `90ecc77b3f8ab6627fa0a33be159c623bb9468d141a689b0bafb881b7f64925e`.

The report establishes the ordinary instruction contract and confirms existing observation sites suffice. Its scalar oracle is not production or native execution. The historical live generic-free arguments were filtered out of the trace; this repair does not claim those arguments were recorded or satisfied the exemption.

Parent owns independent review, merge and any future native qualification. This commit is a source checkpoint, not an independently reviewed or activated native binocular build. The remaining native acceptance gap is the complete instruction-derived observer stream through the actual repaired runtime, including installed-site/register preservation and real same-address allocator reuse.

## Reusable test lesson

Keep wrapper storage retirement separate from command/executable retirement. Require the actual nested return and a downstream same-invocation cleanup-return witness, consume storage retirement once, then require a fresh constructor epoch before same-address publication. Include every mandatory generic free in default positive fixtures; an abbreviated cleanup-to-publication fixture can keep an unconditional free guard falsely green. If native execution is blocked, preserve the complete explicitly modeled sequence and report source GREEN separately from native validation.
