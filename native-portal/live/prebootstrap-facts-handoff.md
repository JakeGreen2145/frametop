# Prebootstrap stock-fact refusal

## Outcome

A source defect is reproduced and repaired. It is a candidate explanation for live trial `live-current-c417303-unworn-02`, not proof of that trial's native event ordering.

Base: `c4173034df7740e2e47ed0c7220fcd50623eae6b`.
Branch: `fix/native-portal-prebootstrap-facts`.
Worktree: `/home/jakeg/.hermes/work/frametop-native-prebootstrap-facts`.

The exact baseline collector, linked into the actual default Runtime with the new scalar diagnostic, produced:

```text
first_stock_pc=0x4b3d0 first_evidence_pc=0x7fda0
PREBOOTSTRAP runtime bootstrap-armed=0
```

The direct real collector/Evidence/Bridge probe also produced:

```text
PREBOOTSTRAP cause observed=0 earlier_stock=0x4b3d0 first_evidence=0x7fda0 bootstrap=0 reads=1
```

The earlier unrelated callback made zero native read syscalls. The one later read was the original renderer-vtable qualification. No extra native read was introduced by the repair or diagnostic.

## Cause and repair

Runtime calls `dormant_fact_site -> stock_facts.capture -> Evidence.route/event -> stock_facts.finish`. Before `base_` existed, `capture` accepted only `0x7fda0`; any other covered observer latched its PC in `failure_`. Evidence correctly ignored unrelated events before `constructed_`. At the eventual `0x7fda0`, Evidence authenticated `x24` and its original vtable but the collector bootstrap rejected its earlier sticky failure. Evidence then latched `0x7fda0`.

The pre-base gate now observes without issuing facts while `base_ == 0`. This condition implies there cannot yet be a known member, constructor, Begin, disposal, factory or turnover scope: all mutations that establish those records are below the same gate, and `base_` is never reset. Existing PID/TID, coverage, prior-failure and active-callback checks still execute before the gate. Bootstrap still requires the actual `0x7fda0` callback and Evidence's original renderer/vtable qualification. No field, identity, generation, read permission or readiness is granted by ignoring an event. No failure is cleared.

The parent then supplied Desktop's verified retained contract, SHA-256 `6b1fa06eaa31b23e3ebb49dce013b07bc85ff73fdf3b2887ddb65bc4e8008aa6`. Its section 4 prompted a batched check of the same classification seam after R exists. Actual Runtime tests reproduced poisoning from null/unknown pool returns and a complete unrelated constructor invocation. A separate check found generic frees of retained R/W were not invalidating those dependencies.

The final patch also classifies constructor/Begin/reset descendants and pool returns as unrelated only when their command key is unknown and no owned constructor, Begin, turnover, factory, disposal or member-reset obligation exists. A new constructor additionally requires the existing device relation `x1 == R + 0x50`; all original paired SP/LR/pool/result checks still apply to qualified invocations. Runtime finishes a successfully classified unrelated observation without asking Evidence for absent scalar fields. It leaves the original Saved and native continuation untouched. Otherwise the existing guard path runs, including revocation of tracked pool-return members before validating pool/LR. Generic deletion of retained renderer/current-or-turnover wrapper identities now invalidates Evidence; the collector also covers its pending factory wrapper. No lifetime redesign or new native read was added.

`first_stock_pc` now overlays the collector's existing atomic first-failure PC in the existing `ft-native-live` logger. It participates in transition filtering and uses the existing line budget. The exported `Diagnostics` ABI is unchanged. There is no new pointer read, trace buffer or callback-thread logging.

## Tests and evidence

All native bodies, callback schedules, installed inventory and GPU outcomes in these tests are explicit source models. They compile and exercise the actual default Runtime, pin path, canonical modeled installed-PC inventory, collector, Evidence, Adapter, owner binding, lease server and stereo/settlement chain. Private fixture headers are hash-checked existing data, not newly generated opaque analysis.

The regression injects events explicitly before `bootstrap_router`:

- Unknown protected pointer and zero at global delete `0x4b3d0`.
- The later owned command address at an earlier delete, followed by its valid later lifetime.
- Unknown pointer at destruction `0x3003d0` and pool return `0x311648`.
- A complete earlier unrelated constructor/Begin/destruction lifetime.

Each case preserves Saved and makes zero `SYS_process_vm_readv` or libc `process_vm_readv` calls during these observations. Each then completes actual cold first-frame stereo, exact queue/wait, pending Off, CPU/executable/source settlement and ordinary cleanup. Each positive scenario runs two Runtime instances in one process.

Negative checks retain genuine known destruction both after renderer bootstrap and after authenticated Run/camera binding. A malformed known constructor remains sticky, as do absent coverage, nested active callback, unauthorized scalar read, wrong bootstrap callback, foreign TID and forked PID. The wrong-vtable case remains refused after replacing the bytes with the correct vtable. Existing missing-hook, partial-upload, rejected-selection, identity-migration, raw-return, missing-GPU, missing-CPU and missing-source cases remain passing negative tests. Expected SIGABRT cases require the exact production cleanup guard at `0x8b020`, reason 5, joined worker, zero native cleanup and no assertion abort.

The final post-R matrix includes unknown/null returns and an unrelated full constructor through subsequent stereo settlement, plus retained R/W frees, wrong tracked pool/LR, duplicate tracked return, wrong subject during owned disposal, and wrong subject during a tracked reset. All pass with the intended refusal or successful later chain.

Important diagnostic distinction: the qualified constructor's wrong device observation at `0x303d10` latches the collector's PC; Evidence's subsequent `0x301808` read refusal sets its stock-fact gap but does not call `Evidence::fail`. That guard test therefore expects `first_stock_pc=0x303d10`, `first_evidence_pc=0x0`, not a fabricated Evidence failure.

### RED

Before changing production code:

```sh
python3 native-portal/live/dormant_runtime_test.py --current --modes cold,prebootstrap-cause,prebootstrap-delete,prebootstrap-zero,prebootstrap-reuse,prebootstrap-destroy,prebootstrap-return,prebootstrap-constructor,bootstrap-known-delete,bootstrap-known-destroy,bootstrap-constructor-fault,bootstrap-vtable
```

Exit 1. Cold control passed. The collector-cause case returned 1 with the exact earlier-stock/later-Evidence failure above. All six prebootstrap full-chain cases hit the unarmed-allocation assertion at `runtime_chain_test.cpp:112`. Guard diagnostic checks were red because `first_stock_pc` was not yet published.

Preserved initial output: `native-portal/build/dormant-runtime/current/prebootstrap-red/`.

A later bounded confirmation compiled the exact baseline collector blob from Git and relinked it against the current actual Runtime and diagnostic/tests, with no other behavioral change:

```sh
systemd-run --user --scope --quiet -p MemoryMax=256M -p MemorySwapMax=0 \
  python3 native-portal/build/dormant-runtime/current/compare-baseline.py
```

The confirmation runner exited 0 after checking expected failures. The functional `prebootstrap-runtime-cause` child exited 1 with `first_stock_pc=0x4b3d0`, `first_evidence_pc=0x7fda0`, `bootstrap-armed=0`. Cold, known-delete and bound-known-delete controls passed. Baseline collector SHA-256: `87d44b09593319838af6a0b0a3a23d0143e05ae21215c7f824a4131224195100`.

Commands, source hash, child results and logs: `native-portal/build/dormant-runtime/current/prebootstrap-red-confirmation/`.

After Desktop's contract arrived, the additional focused RED command was:

```sh
python3 native-portal/live/dormant_runtime_test.py --current --modes postbootstrap-return,postbootstrap-null-return,postbootstrap-constructor,bootstrap-delete-renderer,bootstrap-delete-wrapper
```

Exit 1, with all five cases red before their production changes. Preserved output is under `native-portal/build/dormant-runtime/current/postbootstrap-red/`. The baseline-collector confirmation above preceded this extension and documents the initial prebootstrap seam, not a final whole-tree baseline replay.

### GREEN

```sh
python3 native-portal/live/dormant_runtime_test.py --current
python3 native-portal/live/dormant_runtime_test.py --current --sanitize
python3 native-portal/live/dormant_runtime_test.py
python3 native-portal/live/dormant_runtime_test.py --sanitize
```

All exited 0 on the final frozen source. Current suite: 38/38 normal and 38/38 ASan+UBSan. Dormant suite: 7/7 normal and 7/7 ASan+UBSan. Total 90 passing case invocations, including intended fail-stop cases. Reports were parsed programmatically for unique case names, counts, success and identical final source hashes, also compared against final files on disk. No sanitizer finding. Every compile/link/test ran serially under `MemoryMax=256M`, `MemorySwapMax=0`, with `/home/jakeg/.hermes/cache/scratch` as TMPDIR.

Reports are the four `acceptance.json` files under `native-portal/build/dormant-runtime/{,sanitize/,current/,current/sanitize/}`. The current diagnostic probe now reports `bootstrap-armed=1`; direct collector probe reports `earlier_stock=0x0 first_evidence=0x0 bootstrap=1 reads=1`.

`git diff --check` and added-line secret/shell/eval/pickle scans passed. Builds use `-Wall -Wextra -Werror`. Initial test scaffolding needed the existing Bridge memory constructor, and one diagnostic expectation was corrected to match Evidence's non-failing stock-gap path. Neither required weakening production guards. Independent review remains parent-owned; no independent approval is claimed here.

## Changed files

- `runtime/native_stock_facts.cpp/.hpp`: unowned global-observation classification and retained-owner free guards.
- `runtime/evidence.cpp`: invalidate retained R/W dependencies at generic destruction.
- `live/diagnostics.hpp`, `live/runtime.cpp`: existing atomic first-stock-failure diagnostic and unrelated-observation routing.
- `live/prebootstrap_runtime_fixture.inc`: read counters, prebootstrap regressions and preserved guard probes.
- `live/current_runtime_test.cpp`, `live/runtime_chain_test.cpp`: regression integration, modeled unrelated device constructor, and post-Run destruction controls.
- `live/dormant_runtime_test.py`: focused mode selection, read interception and exact result checks.
- This handoff.

## Remaining live discriminator

The old trial did not publish collector failure. `CommandTrace` also filters unrelated `0x4b3d0`/`0x311648` events, so their absence from that journal does not exclude this cause. Startup socket failure in trial 01 is separate and is not attributed to this defect.

For a parent-owned rebuilt trial, compare `first_stock_pc` with `first_evidence_pc`:

- An earlier nonzero stock PC identifies the collector's first refusal independently of the later Evidence symptom.
- `first_evidence_pc=0x7fda0` with `first_stock_pc=0x0` points instead to the original Evidence constructor qualification, such as the actual `x24` renderer and successful read of `[x24] == image_base + 0x599c38`, or its repeated-construction/bounds/epoch checks.
- A stock failure at `0x7fda0` points to collector bootstrap/active-scope/identity checks rather than an earlier global observer.

The precise unresolved native question is whether an unrelated covered global callback preceded renderer `0x7fda0` in trial 02, and whether that actual constructor callback supplied the expected `x24` and vtable. This patch does not establish historical event ordering, native reachability, GPU output or headset recovery. No headset, SSH, service activation, settings change or new opaque analysis was performed.
