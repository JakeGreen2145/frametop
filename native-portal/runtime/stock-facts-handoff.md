# Stock command facts component

## Delivered and not activated

Evidence now has an explicit pre-pin DORMANT stock mode and a one-way ENGAGED boundary. Default construction keeps the existing production behavior. No Runtime, camera owner, source admission, hook manifest, emitter or installer was changed.

The actual Evidence/Adapter CPU test runs creation and successful Begin, a pending pool return and recycle before Run, repeated current-wrapper turnover and pool reuse, then engagement. It continues through actual stereo uploads, PassGate preparation, both backend admissions after Off, exact Command submission/wait certificates, current/retained disposal, successful old-command reset/Begin, Adapter reclamation and `release_lifecycle`. Native operations and Vulkan results in that test are modeled, not executed vendor instructions.

Protected historical graph pages and intercepted `process_vm_readv` calls show that dormant turnover does not touch those pages or the arena resource/backing. Exported pass and general arena readers refuse in dormant mode, including partial bootstrap. The separate existing bootstrap/allocation and native classifier scopes remain supported. Classifiers do not consult command/pass bounds in dormant mode. Their scoped fields are revoked at return.

There is no native-qualified dormant collector in this commit. The partial Desktop native findings delivered during this task are not promoted into proof. `StockFactReader` is the explicit unsupported gate: a null callback or a missing scalar latches a fact gap and refuses engagement. The test's `modeled_scalar` callback is deliberately a SOURCE MODEL of field availability. Do not install it in Runtime or replace it with an unrestricted `read_self` forwarder.

## Parent API

Before `pin`, on the Evidence owner:

```cpp
Evidence evidence;
Evidence::StockFactReader facts{collector, exact_native_scalar_fact};
if (!evidence.begin_dormant_stock(facts)) { /* refuse configuration */ }
// Existing pin/bootstrap/native event delivery follows.
```

The callback signature is:

```cpp
bool exact_native_scalar_fact(void *collector, uint64_t event_pc,
                              uint64_t field_address, uint64_t &value) noexcept;
```

Every request is exactly eight bytes. `event_pc` is the actual consumer event, not an arbitrary retrieval timestamp. The address identifies the requested scalar; it does not grant permission to read that address or dereference the returned value. Supply a native-local observed scalar or an exact authenticated by-value fact whose association remains valid at that event. Refuse any missing, unsupported, stale or mismatched fact. Keep the producer's actual capture PC, native invocation, constructor/factory result, member lifetime and pairing provenance in the collector. Do not defer Saved, stack pointers, a callback output reference, or a native read until owner progress resumes. Callback/user lifetime must cover Evidence, and the callback must not reenter Evidence.

`stock_command_facts()` returns only the current published wrapper, command, Vk handle, recording generation, pool and Begin status. It performs no native read and grants no read capability. `unsupported_stock_pc()` reports the first failed scalar request and stays sticky. Existing `first_failure_pc()` and publication diagnostics remain available.

After the parent has excluded conflicting native work and optional integration readers, on the authenticated Run owner:

```cpp
const auto current = evidence.stock_command_facts();
const bool engaged = evidence.engage_stock(evidence.identity(), current.generation);
```

This operation performs no native reads. It requires exact renderer/device identity, authenticated Run, an actually published begun member, the existing recording issuer, and no open creation, reset/Begin, turnover, retained phase, graph scope, lifecycle, staging call, resource creation or sticky uncertainty. It does not implement the parent's caller/admission interlock. Only then may the parent attempt the existing command owner, graph, Adapter, staging and pipeline paths under their native lifetime scopes. Do not retry one-shot Adapter construction before eligibility.

No API returns ENGAGED to DORMANT. Off, settled debt and released Lifecycle do not reset the mode. Configuration is allowed only before pinning, so it cannot discard an existing Lifecycle, partial upload, submitted command or owned allocation.

`graph_event` returns false while dormant. Mandatory history still goes through `event`. False stock-history/collector results mean unavailable integration evidence, not permission to suppress or fabricate native execution. The parent must preserve stock continuation while withholding engagement. This commit does not change Runtime's response to such results.

## Publication boundary and loader work

Dormant mode ignores the pre-store `2a23f4` observer without reading or advancing publication. It consumes `event(0x2a23f8, saved)` only after the actual native publication store. The consumer checks the same renderer/SP/turnover sequence, successful Begin, member/Vk identity and recording authority. Default Production and ENGAGED keep their existing `2a23f4` route unchanged.

The `2a23f8` consumer ABI used by the source fixture is `x19=R`, `x0=W`, and the unchanged turnover SP. This is an explicit fixture assumption until the final native table and emitted observer verify the saved lanes. Do not synthesize this event by renaming a pre-store callback. If the new observer is absent, turnover remains incomplete and engagement refuses.

Parent-owned installation inventory:

- Add and authenticate the committed `2a23f8` observer, its displaced instruction, digest and saved-register continuation. Route it to Evidence only at the real boundary.
- Desktop's partial `304ecc` finding supplies `C=x22`, `W=x21-8` without a wrapper dereference. Pair it with the actual constructor/factory returns before using it as the `W+8` fact. A recorded interior constructor store alone is not a successful factory result. The final constructor/factory return coordinates were not supplied to this lane.
- Reconcile the proposed `3052e0` and `3052ec` disposal facts with the current reducer's `305260` entry and `311648` return sequence. Their exact lanes and pairing are not supplied here. Do not claim they are installed, and do not backfill an earlier failed event with a later observation. If entry facts cannot be supplied safely, retain the unsupported gate and add a native-coordinate-derived reducer entry in the parent integration.
- `311648` entry does not establish pool-lock exclusion. The fact callback must not read C/pool memory there merely because the event is on the owner thread. Use authenticated by-value command/pool/handle provenance captured at a proved boundary, or refuse.
- Keep existing create/Begin/reset/delete, current/retained and bootstrap observations complete. Do not drop an unsupported event and later engage from a readable pointer.
- Runtime opt-in, caller exclusion, source receipt folding and the new native collector are separate parent work. The blocked-owner scheduling test is not repaired by this component alone.

## Exact scalar request inventory

R is the authenticated renderer, W the native wrapper selected at the event, C its exact known Recording member, S the actual native SP. All entries below are **consumer fact requests**, not native read grants. Existing source guards are listed separately from unresolved native authority.

| Consumer PC | Address/value requested | Bytes each | Existing source association and pairing | Native authority status |
|---|---|---:|---|---|
| `301808` | `C+8` device context | 8 | `x0=x20=C`, unique member, creation SP; must equal `R+50` | Existing constructor observation. Collector must authenticate the actual construction borrow. |
| `300a08`, initial Begin | `C+738` VkCommandBuffer | 8 | `C=creation`, `x0=x19=C`, SP relative to constructor, LR `300ae0` | Existing paired Begin. No inference from publication. |
| `300a38`, initial result | `S+8` saved LR, `C+738` | 8 | Exact paired result SP, x19, raw Vk result and unchanged handle | Stack value must be captured in this invocation before resume. |
| `30180c` | No field request | 0 | Real constructor return, x20, SP, successful paired Begin and returned boolean | Register facts only. |
| `29a528`, initial publication | `R+a78`, `C+50` through fact callback | 8 | Native wrapper publication, known successful member | New pool/device requests require collector qualification. Existing initial W/C reads and the final `7fdbc` bootstrap checks are unchanged, not blanket dormant loans. |
| `2a1e18` | `R+1380`, `W+8`, `C+8`, `C+738`, `R+a78`, `C+50` | 8 | Exact renderer, mode zero, enumerated native caller, expected SP; published begun member and same generation/pool | Narrow source requirement only. No arena, map, pass, attachment or cache request. Prefer prior authenticated wrapper association rather than chasing W. |
| `2a2230`, `2a2320` | No field request | 0 | Same R and turnover SP, ordered native phases | Observed transition only. Does not set `transferred_` or certify owned disposal. |
| `2987d8`, `298840` | No field request | 0 | Same R, paired retained SP | Blocks engagement while open; does not certify owned disposal. |
| `305260` | `R+1380`, `W+8`, `C+8`, `C+738` | 8 | Exact old W/C/generation, LR `2a23b4`, turnover SP, native current slot cleared | Unqualified as a new dormant read scope. Later native disposal facts must be reconciled, not assumed available here. |
| `311648`, turnover return | `C+50`, `C+738` | 8 | Exact C/W, pool+d0 argument, LR `3052f0`, wrapper-return SP | **No entry lock proof.** Requests must be satisfied by authenticated scalar provenance or refused. |
| `311648`, unpublished stock return | `C+8`, `C+50`, `C+738` | 8 | Existing exact stock LR set, known begun generation-zero member, no current/retained/Lifecycle role, pool argument | Same exclusion warning. Wrong provenance is sticky, not a silent ledger deletion. |
| `300b20`, `300c54`, `300a08`, `300a38`, recycle | `C+738`, plus `C+8` and `C+50` for the known pool member | 8 | Paired reset/Begin SP, entry LR `2a9cdc`, x19=C, x20=pool, actual raw success | Requires the actual recycler invocation or exact valid facts. Reset revokes Begin; only paired success restores it. |
| `2a23f4`, dormant | No request; not accepted as committed publication | 0 | Pre-store callback | Does not issue a generation. |
| `2a23f8`, dormant | `W+8`, `C+8`, `C+738`, `R+a78`, `C+50` | 8 | Actual committed wrapper, completed turnover, known Begin/member/handle, unchanged pool if previously known | New observer required. Wrapper identity should come from paired native constructor/factory facts such as the partial `304ecc` capture. Saved-lane ABI is not verified here. |
| `3003d0`, `4b3d0`, unsupported wrapper destruction | No new pointer reader | 0 | Existing exact identity invalidation and role exclusions | Must remain installed before free/reconstruction. |

The existing initial arena provenance checks at `7fdbc`, allocation output borrow, owner Start/Run association and call-local UBO/geometry classifier inputs were not re-proved natively in this lane. Those remain separate inherited obligations. `StockFactReader` cannot authorize pipelines, graph capture, staging, command callbacks or general allocation reads.

## Tests and reproducibility

Run from this worktree, serial compilers, no installs:

```sh
systemd-run --user --scope -p MemoryMax=256M -p MemorySwapMax=0 \
  python3 -B native-portal/runtime/stock_facts_test.py --runtime-smoke
systemd-run --user --scope -p MemoryMax=256M -p MemorySwapMax=0 \
  python3 -B native-portal/runtime/stock_facts_test.py --sanitize --runtime-smoke
```

The runner verifies the cgroup cap, uses the designated TMPDIR, compiles one translation unit at a time with low GCC collection thresholds, and imports no analysis runner. The optional Runtime smoke build parses the production SOURCES literal and uses the already provisioned Vulkan include tree. No saved native fixture headers are needed for these tests. No source-prefix extractor or preprocessor guard was changed.

TDD failures were observed before the corresponding repair: legacy turnover read protected graph/backing; an unpaired native phase still allowed engagement; pool identity was absent from the scalar result; a refused collector was bypassed; and partial-bootstrap bridge readers still exposed renderer memory. Raw command/output records stay under `runtime/build/stock-facts`. The committed `stock-facts-test-results.json` records the RED and final normal/sanitizer results and source hashes.

The positive source chain includes pending pool recycling before Run and repeated post-Run reuse. Negatives cover wrong native owner/device/pool, wrong caller/SP, missing current-transfer phase, unknown or unbegun members, stale Vk handle/generation, failed reset/Begin, unauthorized pool return, current-member reset, unsupported collector and wrong engagement identity. It also checks dormant classifier/reset behavior, incomplete retained phases, early reader denial, one-way engagement and preserved Off debt through real Command/Lifecycle settlement.

Normal and ASan/UBSan builds execute the new stock fixture, the existing Evidence suite, the no-layer Evidence variant and the actual default Runtime smoke test. This is not the full Runtime camera/source integration suite, native hook ABI validation, hardware verification, or native collector approval. Independent review is left to the parent; this subagent had no delegate tool or installed coding-review CLI and does not claim an independent approval.
