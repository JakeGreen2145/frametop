# Native admission alternatives

## Outcome

Implemented in the production source collectors, admission provider and Runtime event routes. The default `Runtime()` constructor is exercised, not an injected admission facade. Its existing `dormant_configure` constructs `NativeSourceAdmission(CurrentSourceContracts)`. Its existing own-method path publishes only after `ft_native_portal_call_saved` completes and the collector matches the pre-RET candidate and literal result.

Base: `6674da53a3d319f9de360655a0f6ed17182a66de`.
Producer dependency: original `35a2d779916012bc3ebe77b903ec49c89a621f89`, cherry-picked as distinct commit `801bb1966217da143c2a9c98f582745a7688a814`. Parent reported its independent review PASS. The supplied review hash was checked. Consumer review remains parent-owned.

The combined canonical inventory is 205 compositor plus 37 client sites, 242 total. Consumer required sets grow from 75 to 80 source sites and 12 to 17 render sites. All 87 old consumer requirements remain; the exact ten additions match the producer's module/PC/word/kind/guard records. Diagnostic-only `328170` is absent. Runtime tests now derive the canonical inventory without injecting requested PCs. The installed-sites API and native services in those tests remain explicit CPU models, not a live installation.

## Production changes

- `source_metadata.inc`, `source_provider.hpp/.cpp`: explicit `RecognizedNoGrant`, distinct from a real grant and unsupported raw results. The header pointer is associated by value before status. The header-ID read is deferred to actual property success, including the legacy collector path. Recognized mode0/latest80 never reads or clears stale metadata/data output slots and cannot contain property/ref work. The q1 empty path still requires the actual q0 observation. Actual successful grants still require their corresponding Returns.
- `stock_native_boundary.hpp`, `source_router.inc`: track weak-GOT mode as Unknown, ActuallyLocked or NativeBypassObserved. Bind entry, actual lock result, inner return, paired exit mode and clean caller completion to provider/ID/queue/SP. Bypass allows scalar observation only. Additional header/ref reads still require actual manager lock plus independent successful shared guard. Changed, unpaired or missing mode/guard scopes fail closed.
- `native_source_admission.hpp`: completed recognized empties are neutral, preserve literal results and existing journal/debt, and create no readiness, ownership or cleanup obligation. q1 empty plus paired q0 does not seed first warm history. Missing q1 with successful q0 alone cannot seed history. Raw4/5/7, pending grants, missing retirement and prior uncertainty remain closed.
- `current_source_selection.hpp`: explicit ConnectedGate versus RetainedTimeContinuation. Require same render invocation/K/SP, actual nonzero retained-time branch, finite positive native ramp or authenticated clamp and common join. Preserve selector/slot/texture/packet/upload ticket, one-shot borrow and prior borrowed journal. Copy actual native s8 opacity without reconstructing the factor. Existing settlement is unchanged.

No factory, Present graph, emitter, GPU/CPU/source settlement authority, public scalar ABI or original native return lane was changed by the consumer delta. No extra native Acquire, Return, Update or Disconnect was introduced.

## Saved ABI correction

The production `arena::Saved` layout is `unsigned char q[32][16]`. Native s2 is `q[2]` low32 and native s8 is `q[8]` low32. The report's flattened two-u64 notation would call those q[4] and q[16]. An initial source-model fixture accidentally agreed with that wrong indexing. The frozen producer scalar replay reproduced the failure; both production indexing and fixtures are corrected. The requested native v2/v8 facts and ten-site hook contract did not change.

`admission_vectors_data.py` validates the producer vector SHA and parses JSON as data only. It emits a private header containing the 29 vectors and 63 full Saved records. Runtime tests use captured manager GOT values, latest/NZCV, mode/connected and time/ramp/clamp scalar words. K/SP, binding, unrelated native bodies and services remain explicitly modeled. This is not a claim that the consumer executes complete native methods or replays all 29 native blocks.

Frozen copy: `native-portal/build/admission-frozen/admission-native-vectors.json`.
SHA256: `bca7a1da598b555baec2dd8790aba8a900afaf839849850f93019b5448f06b39`.
No producer worktree or output was modified.

## Verification

Final merged-source executions, serial, MemoryMax 256 MiB and MemorySwapMax 0:

| Suite | Normal | ASan/UBSan |
| --- | --- | --- |
| Default Runtime current suite | 178/178 | 178/178 |
| Dormant Runtime suite | 7/7 | 7/7 |
| Source contract/admission executables | four passed | four passed |

The four unit executables are `native-contract`, `stock-receipts`, `existing-admission`, and `current-admission`. Their runner also verifies both required-site inventories. The recorded 16 commands per run include compilation and inventory queries, not 16 independent test cases. Production source inventory checks pass separately.

New default-Runtime cases cover cold empty updates, manager bypass, warmed empty retained rendering, q1 empty/q0 grant, retained connected0 and clamped paths, protected stale output/header pages with zero additional read attempts, all ten independently omitted installed PCs despite copied full coverage masks, and raw4/5/7/unpaired/missing-retirement debt. Positive cases traverse first stereo, pending Off, exact settlement and clean teardown with zero Disconnects. Negative source cases terminate at the real cleanup guard: SIGABRT, reason5, event8b020, joined1, native cleanup0. Fixture assertions are not accepted as those negative results.

Unit cases additionally cover first-history partial refusal, positive later complete history, unchanged prior one-eye debt, callback revocation, packet/slot/identity mismatches, absent warm history, negative/zero/nonfinite ramps, missing continuation/clamp, invalid opacity and neutral actual no-use RET.

Commands:

```sh
python3 native-portal/live/stock_source_receipts_test.py
python3 native-portal/live/stock_source_receipts_test.py --sanitize
python3 native-portal/live/dormant_runtime_test.py --current
python3 native-portal/live/dormant_runtime_test.py
python3 native-portal/live/dormant_runtime_test.py --current --sanitize
python3 native-portal/live/dormant_runtime_test.py --sanitize
```

Runners establish the memory scopes themselves. Current Runtime also requires the existing hash-pinned private headers and the frozen admission vector JSON above. It accepts `--fixtures`, `--factory-vectors`, and `--admission-vectors` paths. No analysis runner is imported to generate these inputs.

`admission-alternatives-verification.json` records final source/input/result hashes, counts and new-case outcomes. Full results are in ignored `build/dormant-runtime/{current/,current/sanitize/,sanitize/,}acceptance.json` and `build/stock-source-receipts/{plain,sanitize}/results.json`. Logs are `build/alternatives-*-final.log`.

An intermediate repeated-Runtime fixture failed its fixed-vtable mapping assertion after adding an anonymous protected page. The fixture now reserves its fixed mapping first. The final normal and sanitized suites pass. Earlier RED, SIMD RED and mapping-failure logs remain in this worktree's ignored build directory.

## Integration boundary

- Consumer hook request: `admission-alternatives-hook-request.json`. Exact ten-site payload unchanged; status reflects the merged producer dependency.
- Parent should cherry-pick the consumer commit after its producer dependency, reconcile overlapping test fixtures, and independently review the consumer delta.
- Producer loader/instruction verification is unchanged and parent-reviewed. It was not rerun merely to repeat its 1398 instruction comparisons.
- A combined ARM build and any live activation remain parent-owned. No headset, SSH, native camera service, new opaque analysis, package installation or live activation occurred here.
- No queue-map lifetime/exclusion authority was established for manager bypass. A successful shared lock alone still cannot authorize added queue/header reads there. Provider7 effects remain uncertain.
