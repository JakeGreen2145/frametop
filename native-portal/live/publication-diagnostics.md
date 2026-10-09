# Command publication diagnostics

## Evidence and limits

Trial 09 journal lines 224 and 299 report successful Run handoff, seals and frames, then first Evidence failure `0x2a23f4` and first stop `0x142620`. They contain neither the publication predicate nor the stop argument. This change does not claim to identify or fix that live failure. No device was accessed and no ownership, graph, stop or settlement guard was relaxed.

The CPU production-router fixture now publishes an already-begun pool member after authentic handoff to a separate Run thread, before any adapter or lease. This succeeds for both pinned callers `0x6caf0` and `0x9e6e0`. The latter is excluded from the whole-dispatch graph entry scope but still gets the separate current-transfer scope at `0x2a2320`. Therefore the different caller allowlists alone do not establish the trial's cause. A failed transfer would ordinarily invalidate at the earlier wrapper release `0x305260`, which also requires `transferred_` after Run.

`bootstrap_native.py` executes the hash-pinned main-pump prologue: incoming `x0` becomes `x20` at `0x142664`; the later `0x1426fc` loads the worker from that same context's `+0x98` before wait `0x142704`. External bodies are not modeled as successful joins. It also retains the existing actual dispatch caller, pool-pop, release and publication instruction checks.

## Reading the next trial

The existing `ft-native-live` line now includes the first **invalidating** command-publication snapshot. Ignored foreign/not-ready calls do not occupy it. It is captured before `fail()` clears the ledger, then retained through later events. All native read order, early returns and guard side effects are unchanged.

`pub_pred` is the first failed predicate. `pub_checked` and `pub_passed` use bit positions equal to these IDs. An unchecked predicate is unknown, not false.

| ID | Predicate |
| --- | --- |
| 1 | Ready |
| 2 | `x19` matches renderer |
| 3 | Previous command wrapper has been relinquished |
| 4 | Incoming wrapper is nonnull |
| 5 | Turnover phase is 5 |
| 6 | Publication SP matches turnover SP |
| 7 | Not in Run, or current transfer was captured |
| 8 | Read incoming wrapper's command |
| 9 | Read command's native owner association |
| 10 | Association equals renderer + `0x50` |
| 11 | Read native command handle |
| 12 | Native handle nonnull |
| 13 | Command exists in bounded recording ledger |
| 14 | Ledger/native handles match |
| 15 | Ledger has successful Begin |
| 16 | Add wrapper read role succeeded |
| 17 | Add command read role succeeded |
| 18 | Existing or newly issued generation is nonzero |

Failures 1 through 4 retain the original non-invalidating early-return behavior and are not latched. `pub_pred=0` means no invalidating publication has been captured.

Other fields:

- `pub_phase`: exact turnover phase before refusal.
- `pub_caller`: pinned relative dispatch LR saved at turnover entry.
- `pub_return`: publication's current LR, relative to the pinned image, or zero outside its range. These two LRs serve different purposes.
- `pub_recording`: current recording generation before refusal.
- `pub_generation`, `pub_renewal`: candidate ledger generation and reset/begin renewal phase. Meaningful only after predicate 13 passed. Zero generation on a successfully begun pending member is legal until first publication issues one.
- `pub_entries`: occupied ledger entries before invalidation clears them.
- `pub_attempt`: publication callback count at the first invalidating attempt.

`pub_flags` bit positions:

| Bit | Fact |
| --- | --- |
| 0 | Run accepted |
| 1 | Transfer captured |
| 2 | Whole-dispatch scope active |
| 3 | Lifecycle bound |
| 4 | Graph contains owned marker |
| 5 | Publication SP matches |
| 6 | Candidate ledger entry begun |
| 7 | Candidate/native handles match |

Bits 6 and 7 are meaningful only after predicate 13 passed. Native pointers and raw Vulkan handles are not logged.

The first stop PC, `stop_flags`, and relative `stop_lr` share one atomic record. Flags remain `0=context known`, `1=x0 match`, `2=x20 match`, `3=x21 match`, `4=bootstrap thread`, `5=handoff waiting`, `6=runtime_owned`. No extra native memory reads are needed. The subsequent pinned main-loop witness proved that `0x142620` is normal entry, not shutdown. The manifest and both router paths now use the authenticated shutdown store at `0x1426d4`, with context in x20 and w1 equal to 1. Historical entry-stop captures below describe the old implementation, not the repaired contract.

## Verification

Red test against the original implementation failed because `pub_pred` was absent. ASan/UBSan full-router runner passed 55 checks, including expected unsafe-cleanup aborts, production DSO linkage, both successful post-Run stock publication callers, phase/SP/read/association/null-handle/missing-member/handle-mismatch/unbegun faults, and first-stop same/foreign/unknown classification. It checks exact evaluated/pass masks and retained pre-failure generations. Existing bounded logging, journal socket and saturated-sink tests remain in that runner.

The separate Evidence ASan/UBSan runner passed both layer and no-layer fixtures. Both runners enforce `MemoryMax=650M`, `MemorySwapMax=0`. Router peak was 443912192 bytes; Evidence peak was 246190080 bytes. Reports are in ignored `live/build/runtime/verification.json` and `integration/build/runtime-evidence/verification-asan.json`.

These are CPU boundary and pinned-instruction results, not live GPU upload or stereo submission proof. The next live journal must supply the current seal association masks and first failed ID to diagnose the independent `associated=0` blocker. The main-entry stop repair does not claim live camera association or stereo success.
