# Combined admission and wrapper retirement reconciliation

## Scope and status

Neutral reconciliation in `integration/native-portal-combined-retirement`, based on
`c37911c77a140e295e1e085e11f4e9fce3b8aaf2`, continuing the existing cherry-pick of
`ec0ef702b29a138f1539a8131139cfb308d4384d` from parent
`06b4e8adbc62cf314ecfee3587d3ee195c011f94`.

Side A preserves recognized no-grant outcomes, guarded/deferred reads, retained
render admission and canonical loader inventory without extra native calls.
Side B retires old wrapper storage only after actual nested cleanup return and
one matched sized delete, then requires a fresh constructor epoch and committed
replacement publication without settling unrelated debt.

The incoming retired-wrapper owned-source review is now PASS, as delivered and
verified by the parent. This candidate still needs the parent's combined merge
confirmation review. CPU/source tests are not native validation or live
permission. No native analysis, emulation, SSH, Frame build or activation occurs.

## Conflict decisions

Line references identify the original conflicted file, before marker removal.
All six hunks are disjoint-intent; both sides are retained. No design-answer tie
or superseded production behavior was found.

- `current_runtime_test.cpp:110-115`: preserve per-scenario admission counters and
  fault reset plus retired-fault classification, with one connection baseline.
- `current_runtime_test.cpp:169-181`: retain engaged/settled retirement callbacks
  and union warm selection, including both empty-warm and retired-warm modes.
- `current_runtime_test.cpp:188-194`: perform the scheduled native update once,
  preserving literal empty result 2, then route missing cleanup return to its
  committed-owned-debt path rather than the generic source-negative path.
- `current_runtime_test.cpp:266-272`: union retired/bound refusal markers and
  missing-admission-hook refusal markers. Neither negative oracle is removed.
- `current_runtime_test.cpp:302-308`: union source-negative classification with
  retired-owned-missing-return and all prior negative cases.
- `dormant_runtime_test.py:326-332`: union source-negative and missing-retirement
  fatal checks. Keep actual SIGABRT, reason 5, event 8b020, joined 1,
  native_cleanup 0, and rejection of assertion/sanitizer failures.

## Textually clean dependencies

`runtime_chain_test.cpp` retains Side A's single owned update, captured s2/s8
lanes and native render scalar sequence, plus Side B's complete cleanup return,
sized delete, replacement factory and committed 2a23f8. The additions affect
separate points in the callback chain. Review must check no native call or
cleanup was duplicated. Preserve the fixed-vtable reservation before allocating
anonymous protected pages in `source_map`, and all source-input hashing,
including .inc and .py files. Production merges are left unchanged.

## Outside-marker test additions

The conflict union alone has a warmed no-grant plus retained-render case, but
not the same combination on cold startup. Add `empty-cold-retained` alongside
`empty-warm-retained`, using the existing empty-result and retained-render
branches without changing native call count or production logic. Both retain
mandatory pre-Run same-address retirement and the complete owned settlement
turnover. These two narrow test edits in the current runner inventory and
`runtime_chain_test.cpp` make the requested crossing explicit. All original
case names and negative checks remain.

## Fixture mapping failure and dependent repair

The first complete normal run passed 223/225. Two cases aborted in the second
Runtime's fixed-vtable mmap assertion after their first Runtime completed its
intended behavior. A temporary failure-only `/proc/self/maps` probe and serial
stress reproduced this after 43 processes. `mmap` returned EEXIST, and the
process heap `10306000-106cb000` covered fixed address `1060f000`. This is heap
growth into the just-unmapped fixture range, not a native admission or retirement
refusal. Initial logs, probe log and stress report remain in ignored build storage.

Required outside-marker fixture repair: reserve the fixed source-vtable page
before constructing the first current Runtime and retain that address reservation
across both Runtime lifetimes. A function-local RAII mapping frees it at process
exit. Reset its contents per source fixture, keep the anonymous protected page
per fixture, and preserve fixed-before-anonymous ordering. No MAP_FIXED overwrite,
production change, weakened assertion or negative-oracle exclusion is used.
The temporary maps diagnostic is removed. This affects only
`dormant_source_fixture.inc` and the current fixture's startup call.

## Final CPU/source verification

All executions were serial, with MemoryMax 256 MiB and MemorySwapMax 0.

| Suite | Normal | ASan/UBSan |
| --- | ---: | ---: |
| Combined default current Runtime | 225/225 | 225/225 |
| Dormant default Runtime | 7/7 | 7/7 |
| Evidence, stock facts, source-modeled NativeStockFactCollector | 3/3 | 3/3 |
| Native contract, stock receipts, existing admission, current admission | 4/4 | 4/4 |

Production source-inventory tests pass, 2/2. After the fixture reservation repair,
500 serial subprocesses alternating the two initially failing modes all returned
0. Each subprocess constructs two actual default Runtimes. The failed original
run and failure-only maps probe remain separate RED evidence, not a green run.

The original focused seam run passed 11/11, including cold and warm no-grant plus
retained render with mandatory same-address wrapper retirement. Both crossing
cases subsequently pass in both complete final suites. Protected stale-page and
before-read oracles remain enabled. Each current suite includes 17 expected
fatal cases with actual SIGABRT, reason 5, event 8b020, joined 1, native_cleanup 0,
and no fixture-assertion or sanitizer failure.

The runner inventories were extracted by AST from the frozen A and B commits and
the combined runner. A has 178 cases, B has 201, their intersection has 155 and
their union has 224. All union cases survive, with only `empty-cold-retained`
added, for 225. No negative is dropped. Canonical installed coverage remains
205 compositor plus 37 client sites, total 242. The source-contract executables
report exactly 80 source and 17 render requirements, including all ten admission
additions. No new hook or production source was introduced by reconciliation.

All four final Runtime reports contain the same 235 source hashes, including
all .inc and .py inputs, and match the final source bytes. Component tests reuse
those freshly compiled production objects and record their hashes. Frozen
private inputs are copied only into this candidate's ignored
`native-portal/build/combined-inputs/`, verified against the supplied hashes.
No other worktree's outputs were run or overwritten.

`combined-retirement-verification.json` records exact final source hashes,
input hashes, union inventories, case counts, fatal outcomes and report hashes.
Full reports are in this candidate's ignored `build/dormant-runtime/`,
`build/combined-components/` and `build/stock-source-receipts/`. Logs use
`build/combined-*.log`. The component helper is
`build/combined-components.py`, using the actual owning test sources and the
explicit `FT_NATIVE_STOCK_SOURCE_MODEL` path, not native witness generation.

Reproducible commands from the candidate root:

```sh
python3 -B native-portal/live/dormant_runtime_test.py --current \
  --fixtures native-portal/build/combined-inputs \
  --factory-vectors native-portal/build/combined-inputs/SAVED-VECTORS.json \
  --admission-vectors native-portal/build/combined-inputs/admission-native-vectors.json
# Repeat with --sanitize. Dormant runs omit --current and the input flags.
python3 -B native-portal/live/dormant_runtime_test.py
python3 -B native-portal/live/dormant_runtime_test.py --sanitize
python3 -B native-portal/live/stock_source_receipts_test.py
python3 -B native-portal/live/stock_source_receipts_test.py --sanitize
systemd-run --user --scope --quiet -p MemoryMax=256M -p MemorySwapMax=0 \
  python3 -B native-portal/build/combined-components.py
# Repeat the component command with --sanitize.
systemd-run --user --scope --quiet -p MemoryMax=256M -p MemorySwapMax=0 \
  python3 -B native-portal/live/build_inventory_test.py
```

## Ordering and review boundary

The scheduled update remains one router event. Its hooked entry delegates once
to its separate loader-issued bypass, and the returned raw x0 is preserved.
Borrowed source Acquire/Return counts, no Disconnect, two-eye upload/encoding,
queue success and settlement assertions all survive. No extra Update, Acquire,
Return or cleanup call was added by the reconciliation.

Retirement sends C-load, pre-call, pool entry and the actual 3052f0 return before
one sized delete. The replacement helper sends the factory/constructor sequence
once and publishes only at committed 2a23f8. Pre-store 2a23f4 is still explicitly
observed, not treated as committed. The owned missing-return case exits its
model without calling a duplicate cleanup or using a trailing assertion; the
real joined production cleanup supplies its reason-5 refusal.

Parent-delivered retired-wrapper review:
`/home/jakeg/.hermes/cache/scratch/retired-review-ec0ef702/REVIEW.md` and
`review.json`, PASS for the frozen incoming owned-source delta. This does not
approve the combined conflict resolutions or the fixture mapping repair.
The parent retains the final bounded merge confirmation review.

The legacy standalone Runtime smoke's pre-existing bootstrap capability failure
remains documented in the incoming handoff. It was not rewritten, rerun or
claimed green. No complete native replay through the repaired production
Runtime has been delivered here. The authorized same-provider native retry is
parent-owned; reported generator progress is not final native proof. No opaque
analysis, alternate native execution, package installation, SSH, Frame build or
activation was performed. These results authorize none of those operations.
