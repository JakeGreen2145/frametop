# Initializer callback register provenance

## Outcome

The narrow source repair is ready for review against `a374d892836fa71b1458124cd35184ce407935b2`. The only production change removes the initializer-specific `x24 == x0` predicate from `NativeStockFactCollector::observe_shared`. Exact LR, retained renderer, `P=R+11e0`, literal name, caller SP/output slot, selected-object overlap, lifecycle and receipt-completeness checks remain unchanged. No native reads or Saved writes were added.

The default Runtime regression now exercises direct initializer acquisition before the initial wrapper factory and `29a528`, with native-produced `x24=base+478000`. Positive and null pop paths reach initial publication, real worker handoff, cold/warm stereo, pending Off, exact GPU/CPU/source settlement and normal joined cleanup. They run twice per process. The later known-member initializer return uses existing `LR29a8b8`; it does not erase member history before publication. The direct receipt retires at the modeled queue caller boundary before the wrapper factory, and the wrapper receipt retires at publication before Run.

All 12 Desktop-executed argument vectors now pass the actual production Runtime, covering all six roots in two configurations with positive and null descendants. Both initializer ancestry cases pass pre-publication, handoff, cold/warm stereo and exact settlement. The existing shared-chain tests also replay all 12 vectors after Run before completing stereo and settlement.

The package is `/home/jakeg/.hermes/work/desktop-compositor-re/factory-call-arguments-a374d89/`. This lane independently checked all 22 extracted payload sizes and hashes against its manifest. `factory_arguments_data.py` consumes only the hash-pinned JSON, validates counts, root identities, metadata and per-register validity, and generates an ignored private C++ header. No vendor verifier import, Unicorn execution or new binary analysis occurred on this host.

- `SAVED-VECTORS.json`: SHA-256 `b16751dfc0e5a0fa72ef81213fdc8fe36ab4f0c048de97e828b3b5348886b8c3`.
- `INTERPRETED-REPORT.md`: SHA-256 `b181dcbf76183fe7e74c1bf8daad5a520d580b7509c4050a31a0616bc813a624`.

Desktop executed the pinned producer slices. This lane replays their saved argument results into compiled production code. The surrounding native bodies, installed coverage, allocations, descendant stores and GPU/source outcomes remain models. The replay relocates documented module, stack and object coordinates to each Runtime fixture. All GPR lanes are copied, but unconstrained lanes retain their original model values and grant no native identity fact. NZCV/pad are separately seeded test lanes, not captured Desktop data. Each callback must preserve all Saved bytes. Native pool identities are relocated per invocation, so the fixture's separately allocated wrapper pool is not a claim about the native renderer object's physical layout.

The ancestry replay preserves Desktop's pre-instruction `29a528` semantics: the caller output slot is cleared, x1 is the new W, x0 and the renderer field still name the old W. The modeled displaced store occurs only after the actual observer. Terminal helper receipts retire before Run; successful stereo afterward proves collector handoff, not merely worker authentication.

## Confirmed producer and correction

Desktop's saved text contains:

```text
29a450 MOV  x0,x24
29a454 ADD  x8,sp,88
29a458 ADRP x24,478000
29a45c ADD  x1,x24,c0
29a460 BL   302790
```

Thus callback entry has `x0=P`, `x8=callerSP+88`, `x1=base+4780c0`, `x24=base+478000`, and `LR=base+29a464`. The previous fixture copied the faulty guard by assigning `x24=P`; it could not catch this failure. The replacement guard does not require the incidental name-page value in x24.

The prior Desktop contract's retained-x24 assertion is superseded by this producer sequence and Desktop's explicit correction. Source files consumed without new binary analysis:

- `desktop-contract-8a63fbdcc54b/DESKTOP-FACTORY-CONTRACT.md`, SHA-256 `8a63fbdcc54b231e8ff54faa022715e9c5133686d9ec7ff1338afb1e3a571388`.
- `desktop-contract-8a63fbdcc54b/verified-payloads/native/elf-00299f10.asm`, SHA-256 `84c85e1eba7c0a4edc3dc06e350180ab703092967fbf0590077829b0c558b772`.
- Fixed `runtime/native_stock_facts.cpp`, SHA-256 `d59158805e623193e6367f55d04b4ddee0fa2ce791039b2479d1f257528e2aa9`.

Both Desktop files are under `/home/jakeg/.hermes/work/desktop-compositor-re/live-factory-f9adb38/`.

## Root qualification table

This table is checked by the delivered executed vectors and their production replay. Addresses are module-relative; SP is the helper caller's SP. Whole-method execution and external calls are not claimed.

| Acquisition LR | Retained renderer | x0 pool | x1 name | x8 output | Status |
| --- | --- | --- | --- | --- | --- |
| `297878` | x20 | R+11e0 | base+477ae8 | SP+50 | Both executed vectors replay positive/null; x21 is the name page |
| `297b70` | x19 | R+11e0 | base+477c50 | SP+80 | Both executed vectors replay positive/null; x23 is the name page |
| `29a464` | x19 | R+11e0 | base+4780c0 | SP+88 | Corrected x24 clobber; pre-publication and post-bootstrap pass |
| `29af28` | x23 | R+11e0 | base+478388 | SP+80 | Both executed vectors replay positive/null |
| `29ca68` | x24 | R+11e0 | base+4784a8 | SP+a8 | Both executed vectors replay positive/null; x0 no longer holds the name page |
| `305330` | No R register required | [A+38] | Constructor-entry x2 | W+8 | Both executed vectors replay positive/null; x19=W, x3=A |

## Verification

All builds and child tests ran serially under the existing runner's `MemoryMax=256M`, `MemorySwapMax=0`. Existing private headers were hash-verified and copied to this worktree's ignored build directory. No dependency installation, SSH, headset operation, service activation, binary analysis or cross-worktree write occurred.

```sh
python3 native-portal/live/dormant_runtime_test.py --current
python3 native-portal/live/dormant_runtime_test.py --current --sanitize
python3 native-portal/live/dormant_runtime_test.py
python3 native-portal/live/dormant_runtime_test.py --sanitize
```

Final results: **155/155 current and 7/7 dormant, normal and ASan+UBSan. 324 passing case invocations.** All four reports have identical hashes for 228 source inputs, checked against final source bytes. No sanitizer finding. No owned runner scope remained active afterward.

Reports below are relative to `native-portal/build/dormant-runtime/`:

| Report | SHA-256 |
| --- | --- |
| `current/factory-arguments-vectors-full.json` | `68d898fb6ba81405cc4f7202cd48fbcd0842d10337de6c757061a6224188f425` |
| `current/sanitize/factory-arguments-vectors-full.json` | `d53b387bbed71433915e80bbce473b910683d437837f7b2174b7dd38941e0f4e` |
| `factory-arguments-vectors-full.json` | `39b2846c78ceba6e0002b53fbfd2f6772b3d2a39b46b29ee955cb0db6f673c90` |
| `sanitize/factory-arguments-vectors-full.json` | `815d53905cca38811168ae63b37e721b27b135b4bc5b6892d92dae31322333e9` |

Added defaults cover four initializer positive/null cold/warm combinations, seven wrong pool/renderer/slot/name/SP/LR/reentry cases with the real x24 clobber, and three initializer-path missing-settlement negatives. The latter require production SIGABRT, reason `5`, event `8b020`, `joined=1`, no fixture assertion abort and zero forbidden native cleanup. Existing selected/current aliases, incomplete scopes, known member return/reset/free and Saved/no-extra-read tests remain in the full suite.

The delivered-vector additions are 24 root/configuration/result cases and eight initializer ancestry/configuration/cold-warm/result cases. They are default acceptance, not opt-in diagnostics. Use `--factory-vectors PATH` to supply the same pinned receipt elsewhere; the original private fixture-header prerequisites remain unchanged.

### Preserved RED and one fixture issue

`current/factory-arguments-red/` retains the unmodified-base production failure, test source delta, base commit, compile log and all six focused logs. Its `acceptance.json` SHA-256 is `9760659f6d00307a19e9cb71b3ebaefd8d62c52434153148a8f6e28e5788c745`. Both post-bootstrap roots report `first_stock_pc=302790 stock_pred=1 stock_phase=0 stock_flags=1 stock_caller=29a464`. All four initial cold/warm positive/null chains fail their required capability assertion before the production fix. The first preserved initial fixture used a modeled noncurrent return before the wrapper. The final fixture tightens this order to the initializer's later `29a8b8` return; the RED artifact is intentionally not overwritten.

`current/factory-arguments-vector-red/` separately replays all 32 final vector cases after compiling the exact base commit's stock collector into a separate executable with the final fixture and other production objects unchanged. Both initializer roots fail the exact first-stock predicate for positive and null outputs, and all eight full initializer chains fail required capability. The other five roots remain healthy in both configurations and both result paths. All 32 expected baseline outcomes matched. `result.json` SHA-256 is `18b5dfb4620d9b517b5ca342a7b9195dbeb438f1494a1f62bd7f339893cb99dc`. This retrospective differential replay complements, rather than replaces, the original test-first RED.

The first complete normal run had two existing fixture mapping failures in `prebootstrap-delete` and `prebootstrap-zero`: their second Runtime failed `MAP_FIXED_NOREPLACE` at the fixed client-vtable page after the first complete settlement. Both passed an unchanged focused rerun, and the subsequent complete normal and sanitizer suites passed. Preserved evidence is `current/factory-arguments-first-full.json` and the corresponding `*-first-full.log` files. No mapping workaround or production change was made for this intermittent fixture issue.

## Live boundary

The supplied actual trial identifies `LR29a464` as the first refused shared-helper root, followed by Evidence failure at `29a528` and initialization refusal at `7fdbc`. It produced no frames and did not log x24. Desktop's producer sequence supplies the missing register provenance. These CPU tests do not establish live recovery, camera/Present eligibility, GPU rendering or headset visibility. Those remain parent-owned acceptance work outside this repair.

Reusable test lesson: derive callback arguments at the BL target after every intervening producer instruction, not at an earlier MOV. A retained-register fixture copied from the guard can make an impossible predicate look valid. Carry the corrected full invocation through publication, collector handoff and real settlement rather than stopping at a zero first-failure field.
