# Factory-scope refusal reproduction and diagnostic milestone

## Status

Base `f9adb3878792cf029da6c9c4bd030bfa001a6eab`, branch `fix/native-portal-factory-scope`, worktree `/home/jakeg/.hermes/work/frametop-native-factory-scope`.

The actual default Runtime reproduces the newly reached live failure pair:

```text
first_stock_pc=0x302790 first_evidence_pc=0x311648
stock_pred=1 stock_phase=0 stock_flags=0x1 stock_caller=0x297b70
FACTORY mode=factory-direct reads=0 saved-unchanged=1 no-work=1 returned=1
```

The control omitting only the direct acquisition callback has both first-failure PCs zero. A second candidate includes the shared `304e60 -> 304ec8 -> 304ecc` output chain and produces the same refusal. The member returned at `311648/LR297cb4` was already constructed, begun, known, noncurrent, and generation zero. This is not an unknown-command return test.

**No classification repair is included.** The acquisition acceptance probes intentionally remain RED. This commit supplies a source counterexample, category audit, and bounded first-refusal discriminator while the full native factory/disposal contract is pending. No native caller allowlist, lifetime guard, command settlement, Evidence policy, hook manifest, required-site inventory, or exported Diagnostics ABI changed.

## Evidence and limits

The parent-owned trial is `frame-native-live-evidence/live-prebootstrap-f9adb38-unworn-01`. A bounded read of its first 60 command rows confirms:

- Sequence 18 publishes a known command at `29a528`, generation 1, retaining four members.
- Sequence 19 accepts a distinct known generation-zero member return at `311648/LR29a8b8`.
- Sequence 20 reaches `311648/LR297cb4` with another known, begun generation-zero member. Evidence then fails and clears its ledger.
- The collector's earlier failure is `302790`, absent from the selected command trace. The trial has 40 samples and no submitted frame.

The live trace does not contain that first helper's LR, phase, output slot, or arguments. Matching its first-PC pair does not establish the entire historical native schedule.

The parent delivered Desktop's partial native finding through A2A context `ctx-c5c6d2f13cec468b`. It reports six direct calls to `302790`, with these call/return PCs:

```text
297874/297878 297b6c/297b70 29a460/29a464
29af24/29af28 29ca64/29ca68 30532c/305330
```

The first five use renderer pool `R+11e0` and a caller-local output slot. The wrapper constructor uses `[wrapper_owner+38]` and `W+8`. For the `297aa0` caller, the native frame is `f0`, acquisition output is caller SP+80, and the normal return at `297cb0/LR297cb4` loads that same slot into x1 and uses `[C+50]+d0` as x0. Both normal and failed return callers already exist in production. This lane neither redisassembled native binaries nor independently hash-verified the forthcoming full report.

The final direct probes use those supplied scalar coordinates, but native bodies, callback scheduling, coverage, memory transitions and GPU outcomes remain explicit source models. They compile the actual default Runtime and its production source list against existing hash-checked private headers. Protected name storage is never read. The initial preserved source-only counterexample used a deliberately synthetic LR `feed` and protected output slot; it is separately labeled and is not the native-coordinate result.

## Source cause

Runtime invokes `stock_facts.capture`, then Evidence, then `finish`. After complete bootstrap, `302790` requires `factory_.phase == 2`. That scope exists only inside the selected wrapper turnover. A direct helper call reaches the same global hook without that scope and latches the collector refusal.

The later known return itself uses an already accepted caller. Because collector failure is sticky, it cannot supply the command's scalar facts to Evidence. Evidence's first failure then occurs at `311648`. Broadening `297cb4` would not address this cause.

The diagnostic patch evaluates the original six `302790` guards in their original short-circuit order. It changes only how an existing refusal is recorded.

## Same-category audit

These findings describe the current source, not permission to skip the observations.

| Sites | Current global qualification | Remaining contract requirement |
| --- | --- | --- |
| `3053f8` | After bootstrap requires selected turnover phase 5, its SP/LR and hidden output slot | Distinguish ordinary factory invocation from selected wrapper replacement without inferring R publication |
| `305308` | Requires phase 1 factory, exact W/owner/name/SP/LR | Determine unrelated constructor qualification and whether any current/retained W identity can appear |
| `302790` | Requires phase 2 wrapper constructor, SP/LR305330, W+8, name and pool | Direct renderer helpers are now proved shared; preserve existing constructor/Begin/member facts beneath them |
| `304e60` | Requires phase 3 wrapper acquisition and W+8 | Shared descendant must not require wrapper origin globally; selected invocation remains paired |
| `304ec8/304ecc` | Requires phase 4, known made/begun C, pool, W+8, then adjacent positive store | A known C outside selected publication is not an unknown object. Do not issue a generation or infer publication. Failed creation can bypass the positive store |
| `3053a8` | Requires phase 5 constructor return intent | Pre-RET intent alone never establishes completed publication |
| `305490/305470` | Require paired constructor completion/store/return-slot | Preserve slot-versus-W distinction and downstream `2a23f8` commit requirement |
| `305260` | Requires x0=current W, revokes current W/C, then requires selected turnover phase 4 | Unrelated W must not poison globally, but current/retained W destruction must not be skipped |
| `3052e0/3052ec/3052f0` | Require selected disposal phases and known C/pool/result | Evidence also treats these as selected turnover unconditionally. An unowned W can potentially return a known C. Preserve required command history independently of whether W is selected |
| `311648` | Known member revoked before pool/LR validation; unknown member ignored only with no active obligation | Do not widen existing LRs. Do not erase a known member or pretend a return settles current/retained CPU/GPU debt |

Independent callback probes cover every factory/disposal row above. Each currently refuses. They use deliberately unqualified source registers and are category probes, not proofs that every such native event is harmless. The `3052e0/ec/f0` probes fail Evidence immediately at that site; most others poison the collector first and fail Evidence at the later known return.

The full native contract must answer:

1. Which complete factory/constructor invocations are unrelated to selected R/W turnover, and which register keys reliably establish that distinction before any native read?
2. Can an unrelated W own a known C? Which disposal/load/return callbacks still revoke and preserve that C's membership, reset/Begin obligations, and later publication eligibility?
3. Which current or retained W/C destructive paths can occur outside selected turnover, including factory cleanup and failure? They must invalidate rather than disappear behind an unowned-wrapper skip.
4. Which callbacks are merely selection/output facts and which change actual command lifetime? A skip cannot settle current/retained debt or replace the existing Command/Lifecycle owner.

## Atomic diagnostic

The existing `failure_` atomic now stores one immutable receipt. No second mailbox or identity registry was added. The logger reads it once and derives both PC and detail from the same word.

- Bits 0..23: first refusal PC. The old zero-PC sentinel still reads as `UINT64_MAX` through `failure_pc()`.
- Bits 24..31: predicate. At `302790`, 1=phase, 2=SP, 3=caller, 4=output slot, 5=name, 6=pool range. Zero means this refusal has no detailed discriminator.
- Bits 32..35: actual factory phase.
- Bits 36..40: flags. Bit 0=pool matches an existing member; bit 1=output matches current W+8; bit 2=output matches factory W+8; bit 3=expected SP exists; bit 4=SP matches it.
- Bits 41..63: bounded module-relative caller, or zero if outside the module range.

The new `stock_pred`, `stock_phase`, `stock_flags`, and `stock_caller` fields use only existing owned locals and the callback's copied scalar arguments. There are no native reads, saved-frame loans, callback-thread formatting, added trace storage, or failure reset. Later failures cannot replace the first receipt. The existing 64-line/3072-byte logger bounds remain unchanged.

## Reproduction commands and results

```sh
python3 native-portal/live/dormant_runtime_test.py --current \
  --modes factory-control,factory-direct,factory-direct-chain
python3 native-portal/live/dormant_runtime_test.py --current --sanitize \
  --modes factory-control,factory-direct,factory-direct-chain
```

Both functional runners exit 1. Control passes; direct and direct-chain remain RED. Every child returns normally, preserves all Saved bytes, and records zero syscall or libc `process_vm_readv` calls during the candidate/return sequence. There are no frames, enqueues or pending integration work. No assertion-generated SIGABRT disguises the result.

Preserved evidence below `native-portal/build/dormant-runtime/current/`:

- `factory-source-red/`: original source-only counterexample before production changes.
- `factory-native-coordinates-red/`: supplied direct caller/output coordinates, all category probes, and initial diagnostic RED checks.
- `factory-diagnostics-normal/`, `sanitize/factory-diagnostics-sanitize/`: final diagnostic/control passes.
- `factory-final-red/`, `sanitize/factory-final-red/`: final source still refuses direct acquisition.

The diagnostic suite is:

```sh
python3 native-portal/live/dormant_runtime_test.py --current \
  --modes factory-control,factory-diag-phase,factory-diag-sp,factory-diag-caller,factory-diag-slot,factory-diag-name,factory-diag-pool
```

Repeat with `--sanitize`. All seven pass in both modes. Phase failure is exercised during a real selected turnover with the required factory entry absent. The other five corrupt one condition in a qualified phase-2 factory. Each sends a later differently shaped failure to verify first-receipt preservation. These tests retain owned-obligation refusal and do not depend on the proposed unowned classification repair.

## Frozen regression verification

The final milestone sources passed once through each full suite:

```sh
python3 native-portal/live/dormant_runtime_test.py --current
python3 native-portal/live/dormant_runtime_test.py --current --sanitize
python3 native-portal/live/dormant_runtime_test.py
python3 native-portal/live/dormant_runtime_test.py --sanitize
```

Current: 38/38 normal and 38/38 ASan+UBSan. Dormant: 7/7 normal and 7/7 ASan+UBSan. Together with the seven focused checks in both modes, 104 passing case invocations were counted programmatically. Expected cleanup SIGABRT cases retained exact production reason 5 at `8b020`, joined worker, and zero forbidden native cleanup. No sanitizer finding.

The full-suite reports were copied as `factory-milestone-full.json` in each of the four build directories before the subsequent focused RED run. Reports have identical final source hashes, verified against the files on disk. All builds and tests were serial under `MemoryMax=256M`, `MemorySwapMax=0`; build paths are ignored and private headers were reused without importing native-analysis runners. `git diff --check` passes.

## Changed files

- `live/factory_scope_fixture.inc`: new actual Runtime probes, control, protected-read/Saved checks, and owned diagnostic negatives.
- `live/current_runtime_test.cpp`, `live/dormant_runtime_test.py`: explicit focused-mode wiring and result checks. Pending classification probes are intentionally excluded from the default suites.
- `live/runtime_chain_test.cpp`: optional modeled pool argument for an existing command-construction helper; all old defaults unchanged.
- `runtime/native_stock_facts.cpp/.hpp`: atomic diagnostic discriminator only; no classification change.
- `live/runtime.cpp`, `live/diagnostics.hpp`: coherent single-load receipt publication through the existing bounded logger.
- This handoff.

No SSH, headset access, service/settings changes, new dependency, native-binary analysis, or independent review occurred. The full native factory/disposal contract and parent-owned review remain required before accepting a classification repair. This milestone does not claim live recovery, native output, or GPU correctness.
