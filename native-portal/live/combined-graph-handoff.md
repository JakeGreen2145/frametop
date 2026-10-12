# Neutral graph / retirement integration

Candidate only; parent merge confirmation and graph independent review remain required.

## Intents

A (`ae04e5180b6fb0f76b43997f3b2112561ee84956`) preserves complete native wrapper retirement, storage epochs, committed publication, admission, and all unrelated debt.
B (`e345e8d9239c83433cbb8080f8f4a3d4da183e60`) adds a dispatch-wide existing reader/CameraOwner loan, matched backend/end boundaries, pre-original exclusion and disposal preflight without enabling Present or cold lookup.

## Cherry-pick 384850a856c95f709cc42a792e167e4ab0d8042e

All four marker decisions are disjoint-intent:
- current_runtime_test.cpp original 140: retain repeated retirement and run shared-factory probes for either shared-chain or first-consumer.
- current_runtime_test.cpp original 195: generic negative path excludes both first-consumer and owned missing retirement return.
- current_runtime_test.cpp original 308: retain retirement negative classification and conditional consumer negative classification.
- dormant_runtime_test.py original 214: retain all retirement modes, then select the independent first-consumer inventory when requested.

Dependent fixture work is required before final acceptance: the incoming consumer fixture has an older minimal turnover. Preserve A's cleanup/free/factory/post-store sequence while adopting B's paired entry/body/backend stack and root-loan boundaries. Do not revert process-lifetime vtable reservation.

## Cherry-pick 0ebab69b10a9f9cbac48a18db6746ec969ef23c9

- current_runtime_test.cpp original 114: disjoint; retain retirement classification and add resource_map before source_map.
- runtime_chain_test.cpp original 974: decompose into superseded minimal publication (retain A's complete replacement_factory) and disjoint root-loan end callback (retain B's restored E/LR callback). Change cleanup/factory B arguments outside markers to the paired source_stack+0xa40, matching E-0xd0; fixed 0x8fff30 cannot match B's dispatch E.
- stock_facts_test.cpp original 279: superseded minimal cleanup/publication (retain A's full loaded-W, cleanup return, sized free and constructor/committed publication); disjoint 2987d4 scalar anchor retained before retained disposal.

Outside-marker dependent fixture edits, identified before changes:
- present_runtime_fixture.inc: replace obsolete minimal cleanup and pre-store-only publication with the existing complete Chain helpers at paired B. Test via Flush, cold rejection, Present diagnostic, and all graph adversaries. Native calls are not duplicated.
- runtime_chain_test.cpp: adapt only the complete cleanup/factory helper arguments in the owned dispatch to paired B; retain existing stock turnover scopes outside that dispatch.
- Production recording.cpp/evidence.hpp clean merge combines A's WrapperEpoch/committed publication with B's graph-scope check and authority fields; no additional production edits planned.

Verification of these dependent changes is recorded below for the final candidate, including the backend scope correction. Intermediate picks establish provenance, not acceptance.

## Remaining cherry-picks and recovery checkpoint

`8861a8b87a1b3681fd221c6a13d523b8503b8fdf` completed as `2ae042a` without markers. Keep its exact E-0x830 backend entry and additional 0x90 prologue displacement at cached forwarding. `e345e8d9239c83433cbb8080f8f4a3d4da183e60` completed as `bfd2404f8d776523829c454bf162505d74b05b98`, also without markers. No cherry-pick was replayed on recovery.

The recovered dirty edit was one `MERGE-SP-PROBE` print in `present_runtime_fixture.inc`, not the SP restoration described in the interrupted summary. Its bytes and checkpoint HEAD were preserved under `/home/jakeg/.hermes/cache/scratch/graph-resume-bfd2404/`. The old consumer failure and a fresh reproduction remain in `build/combined-graph-consumer-red/` and `build/combined-graph-alignment-red/`.

## Final dependent fixture decisions

These are outside-marker integration edits, separate from the four completed cherry-picks. No production permission or guard changed.

- `source_command_fixture.inc`, source-stack declaration: disjoint dependency. Add `alignas(16)` to the shared modeled stack. First-consumer-only fields change Chain's layout and previously put its byte-array stack at address modulo 16 equal to 8. The complete `WrapperEpoch::begin` correctly refused it at `305260`. Preserve that production check. A separate aligned dispatch buffer repaired retirement but broke the source-acquire/backend relative SP contract; that failed attempt is retained in `build/combined-graph-separate-stack-red/`, and is not the final implementation.
- `present_runtime_fixture.inc`, entry/body/backend coordinates: define E once from the aligned shared source stack, then use E, E-0xd0 and E-0x830 consistently. Keep the backend's separate 0x90 adjustment. Remove the temporary diagnostic print. Graph probes use local Saved values and did not overwrite the fixture Saved state.
- `present_runtime_fixture.inc`, complete retirement tail: keep the already-merged cleanup-result, sized-free, replacement-factory and committed post-store helpers. Add read-only test checkpoints after retirement and publication. Optional `FT_GRAPH_REUSE_W=1` reconstructs the replacement wrapper at the old W address with a new C and recording, only after the modeled sized free.
- `present_runtime_observer_test.cpp`, those checkpoints: assert the real admission graph and CameraOwner caller remain retained after preparation cutoff and throughout wrapper retirement/publication. Assert changed C and recording, and exact W reuse when requested. These are observations and assertions, not state writes. All original 42 graph schedules remain unchanged; wrapper reuse is a separate crossing control.

The complete current fixture retains the parent's fixed-vtable RAII reservation, no-grant/retained-source modes, all retirement negatives, and single native Update/cleanup/factory call ordering. No new production file edits were needed during recovery.

## Final source acceptance

All compilation and child executions were serial, with MemoryMax 256 MiB and MemorySwapMax 0. CPU/source models do not establish native ABI reachability, hardware output or activation permission.

| Suite | Normal | ASan/UBSan |
| --- | ---: | ---: |
| Combined default current Runtime | 236/236 | 236/236 |
| Dormant default Runtime | 7/7 | 7/7 |
| Original graph schedules | 42/42 | 42/42 |
| First-consumer diagnostics | 4/4 | 4/4 |
| Same-W reuse, distinct pipelines, stock interleaving, full settlement | 1/1 | 1/1 |
| Evidence, stock facts, source-modeled native stock collector | 3/3 | 3/3 |
| No-layer Evidence and stereo commit/recycling | 2/2 | 2/2 |
| Native contract, stock receipts, existing/current admission | 4/4 | 4/4 |

Production source-discovery tests passed 2/2. Each current suite retains 17 fatal negatives, all actual SIGABRT with reason 5, event 8b020, joined 1 and native cleanup 0, not fixture assertions. Graph conflicts retain their exact reason/event oracles, including cached-forwarding refusal rather than forbidden continuation exit 73. Flush and cold-pass rejection settle through ordinary cleanup.

Present functional probes exit 1 in both modes. Each child traverses the complete tail and reaches actual production SIGABRT, reason 5, event 8b020, joined 1, native cleanup 0. Diagnostic success is not functional acceptance. Unknown-caller refusal remains separate.

## Inventory and byte reconciliation

The frozen case inventories were evaluated from their runner AST list-building statements only, without importing analysis runners. Base A has 225 cases; graph B has 189. Their intersection is 178, union 236. The final runner equals that union exactly. All eleven graph omission cases are present and pass in both modes.

The canonical inventory is 216 compositor and 37 client sites, total 253. The actual C++ graph required-site array equals the eleven-row request; every added manifest row matches its word, guard, digest, kind and emitter. All old manifest rows and metadata are unchanged. `prepare.py` and `hooks/pins.h` match both frozen sides exactly, and the complete manifest matches frozen graph B. Compiled source and render contract inventories report 80 and 17 unique required sites, all contained in the combined canonical sets.

All 15 touched production blobs equal their appropriate frozen side or an exact clean three-way merge. The three overlapping production files are `runtime/evidence.cpp`, `runtime/evidence.hpp` and `runtime/recording.cpp`; the verification artifact records each choice and hash. No reset or replacement of the parent's wrapper epochs occurred. The already-verified, unchanged native emitters and CPU installer were not rerun; this acceptance freshly exercises default Runtime's modeled actual-site omissions, not native installation.

Every final Runtime report carries the same 245 source hashes, including .inc and .py files, and matches the final source bytes. Component checks reuse those verified compiled production objects and record their hashes. Frozen private input identities match the graph handoff. `combined-graph-verification.json` records the union, exact merge checks, canonical sets, source/input identities and 18 report hashes.

## Reproduction and evidence

Run from the candidate root. The ignored helpers and all outputs belong to this worktree.

```sh
python3 -B native-portal/build/combined-graph-accept.py
systemd-run --user --scope --quiet -p MemoryMax=256M -p MemorySwapMax=0 \
  python3 -B native-portal/build/combined-components.py
systemd-run --user --scope --quiet -p MemoryMax=256M -p MemorySwapMax=0 \
  python3 -B native-portal/build/combined-extra.py
# Repeat the two component commands with --sanitize.
python3 -B native-portal/live/stock_source_receipts_test.py
python3 -B native-portal/live/stock_source_receipts_test.py --sanitize
systemd-run --user --scope --quiet -p MemoryMax=256M -p MemorySwapMax=0 \
  python3 -B native-portal/live/build_inventory_test.py
python3 -B native-portal/build/combined-graph-audit.py
```

The acceptance helper passes explicit `--fixtures`, `--factory-vectors`, `--admission-vectors`, `--resource-facts` and `--consumer-pins` under `build/combined-graph-inputs/`. It builds first-consumer explicitly before using the graph runner's `--skip-build`, avoiding that runner's other-tree defaults. Final report copies are in `build/combined-graph-final/`; original failures remain separate. Logs use `build/combined-graph-*.log`.

## Remaining gates

Candidate remains provisional for the parent's independent graph review and bounded merge confirmation. Main integration at `ae04e5180b6fb0f76b43997f3b2112561ee84956` was not modified.

Present still needs the authenticated normal SignalPresentFence outer invocation tying retained R at `2955a4` to the binding and recording, including root caller provenance, selector branch, nested SP and return/abort boundary. An LR allowlist alone remains insufficient. No speculative cold IVRResources getter or arbitrary provider/name permission was added.

The parent's authorized Desktop retries completed; the older handoff's native-proof-blocked wording is historical. The delivered 20-event retirement consumer result and 34 PASS cases are separate evidence at `/home/jakeg/.hermes/cache/scratch/turnover-ec0ef702-consume/REPORT.md`, not a full repaired Runtime/native replay or hardware proof. Complete native integration, ARM build and activation remain parent-owned. This recovery performed no SSH, headset access, new opaque analysis, package install or activation.
