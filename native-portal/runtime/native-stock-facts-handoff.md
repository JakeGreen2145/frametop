# Native stock command facts consumer

## Status

`NativeStockFactCollector` supplies the dormant `StockFactReader` from copied native registers, not process memory. It owns 64 bounded command records and bounded construction, Begin, turnover, disposal and factory records. It does not issue recording generations. Evidence remains the sole issuer.

The CPU component reaches creation, initial publication, pending pool reuse, repeated turnover, engagement and the existing real Adapter/PassGate/Command/Lifecycle settlement chain. Factory callbacks come from executing the Desktop-saved instruction windows. Bootstrap, command allocation/creation scheduling, reset scheduling, Vulkan outcomes, installation coverage and the remaining integration boundaries are explicitly modeled. No whole-Runtime native installation, camera provider readiness, GPU or headset result is claimed.

Default Production and ENGAGED paths are unchanged. No Runtime/router, source provider, camera owner, canonical manifest, preparer or loader file was modified. Parent integration must add this translation unit and route the events before enabling dormant admission.

## Correction that supersedes the earlier handoff

The final `continuation16-additional-native-pins-ed19c700d06d/RETURN-CONTRACTS.json` supersedes the unconditional positive `304ecc` interpretation in the earlier COMMAND-SCOPES table.

* Failed creation executes `304f9c: str xzr,[x21]`, then `304fa0: b 304ecc`. Therefore `304ecc` alone cannot establish C from x22. This consumer requires the actual `304ec8` pre-store callback and adjacent `304ecc`, with matching native invocation, SP, LR, x22 and x21. No output-slot read repairs a missing predecessor.
* `3053a8` and `305470` are pre-RET observers. The former is discharged by the actual `305490` post-store continuation. The latter is discharged by the actual committed `2a23f8` publication in the same turnover. Pre-RET alone supplies no publication fact.
* `305470.x0` is the hidden output slot address, not W or success. The collector retains W from the constructor, verifies the native store result at `305490` using x22=W and x20=slot, verifies the restored factory SP/LR and returned slot address, then matches the actual committed W at `2a23f8`. The stack slot is never read.
* `3052f0` accepts only a paired nonnull pool call with low byte of w0 equal to one. A zero result is failure. The null-C bypass is NO_CALL, not success. Entry `311648` revokes Begin immediately but does not establish this result.
* D/P constructor stores remain provisional until the actual `301804` caller continuation. `301808`/`30180c` and the actual Begin load/raw result independently establish successful command creation. Constructor return is not allocation or Begin success.

The saved-text witness now executes the full supplied pool-pop window, including both the positive store and failed-creation bypass. Its former modeled pool epilogue is no longer needed.

## Parent API and ordering

Declare the collector before Evidence so it outlives the stored callbacks:

```cpp
runtime::NativeStockFactCollector facts;
runtime::Evidence evidence;

// Obtain this list from the authenticated installed loader transaction.
// Do NOT use required_sites() itself as production evidence of installation.
if (!facts.install_coverage(installed_pcs, installed_count)) refuse_optional_admission();
if (!evidence.begin_dormant_stock(facts.reader())) refuse_optional_admission();
// Existing exact image pinning and bootstrap follow.
```

For each selected compositor fact event, synchronously on the authenticated native owner:

```cpp
const bool captured = facts.capture(pc, saved);
const bool consumed = evidence.event(pc, saved); // or existing Evidence::route at bootstrap Bridge sites
facts.finish();                                // always, before native resumes
// Failure denies optional admission, not original native execution.
```

Collector-only sites legitimately have no Evidence event handler. Do not interpret that false `consumed` value as a native failure. Keep existing mandatory invalidation/debt routing. A collector refusal is sticky and the new `StockFactReader::complete` callback prevents `engage_stock` from bypassing it. Never suppress a stock call or fabricate its result to make observation succeed.

The parent must serialize this synchronous sequence and must not retain Saved or queue these callbacks for later replay. Only the synchronous scalar callback can retrieve the current event's exact eight-byte values. `finish()` clears those answers. Historical SP, LR, output and object addresses are comparison values, never read permission. The collector performs no `read_self`, native pointer dereference, allocation or graph access.

After actual Evidence Start/Run authentication and the parent's exclusion of old native callers, call `facts.handoff(evidence)` on the new owner. It checks the exact initialized renderer/device generations, current member and actual Evidence application owner before adopting the new TID. This is not an independent Run authenticator or caller barrier. A real second-thread handoff is tested. Fork and foreign callback delivery refuse; foreign refusal only writes the atomic sticky fault.

Evidence sends the collector its already-qualified bootstrap values through the new optional `StockFactReader::bootstrap` callback:

* `7fda0`: image base, process instance, R and renderer generation, after the existing native vtable qualification.
* `29a528`: W/C/Vk from the existing initializer borrow, after the existing D/Vk/member checks. The collector contributes P from completed construction. This preserves the inherited initial bootstrap scope; it does not add a factory or pointer-walk claim.
* `7fdbc`: the already-qualified VkDevice and actual pipeline-layer device generation. No new device read is added.

These callbacks require the matching active native event. Initial wrapper borrowing remains inherited from the existing qualified initializer. Post-bootstrap wrapper publication uses the complete native factory path below. A missing initial device generation remains unsupported.

After exclusion and `finish()`, use the existing one-way call:

```cpp
const auto current = evidence.stock_command_facts();
if (!evidence.engage_stock(evidence.identity(), current.generation)) refuse_optional_admission();
```

Once ENGAGED, stop routing this dormant collector. Preserve all existing Production/ENGAGED graph, lifecycle, queue, source and teardown paths. Do not reconfigure or reinitialize Evidence to return to DORMANT.

## Native event sequence

| Phase | Actual native evidence |
|---|---|
| Construct C | `301800` C=x0=x20, D=x1, P=x2=x19; post `303d10` D=x1/C=x19; post `303d70` P=x22/C=x19; actual `301804` at the original call SP |
| Create and Begin | `301808`; `300a08` entry C/SP/LR; `300a28` actual Vk=x0; paired `300a38` raw uint32 zero; actual `30180c` low-byte success |
| Turn entry | `2a1e18` records R/mode/SP/actual enumerated LR only; `2a1e3c` copies the actual W load. No future W/C load is requested at entry |
| Optional C branch | `2a1f64` validates x20=C and x2=W only if reached. Its absence never invents a load or prevents the supported bypass |
| Native transfer | `2a2230` then `2a2320`, same R/SP. These are native phase observations, not graph disposal certificates |
| Dispose current W | `305260` immediately revokes W without requiring future C/P facts. `3052e0` supplies C=x1/W=x19; `3052ec` supplies C=x1/P=x0-d0; `311648` entry invalidates Begin; `3052f0` supplies the paired successful pool result |
| Factory | `3053f8` entry owner/name/hidden output/SP/LR; `305308` constructor W/owner/name; `302790` acquire P/name/output; `304e60` pool entry; mandatory `304ec8` positive store predecessor and paired `304ecc` post-store |
| Retire and publish | `3053a8` pre-RET; `305490` actual positive factory output store; `305470` pre-RET with x0=slot; `2a23f8` actual committed W. `2a23f4` remains pre-store and cannot publish |
| Stock reuse | Exact existing unpublished stock-return LR set or paired turnover return; `300b20` invalidation; paired raw-zero `300c54`; actual Begin entry/load/raw-zero sequence. Only then can Evidence renew publication using its existing issuer |
| Revocation | Known command deletion at `3003d0`/`4b3d0` clears the record before reuse. Current/open construction destruction is sticky refusal. Renderer teardown and unsupported owned wrapper destruction refuse |

For dormant reads, the only values supplied are the current callback's R+1380, W+8, C+8, C+50 and C+738 facts as required by that phase. Each output is exactly eight bytes. Constructor facts, prior successful Vk identity and pool identity can be compared by value while their native member remains valid. Nothing grants reads of R, W, C, pool vectors, arena resources or saved stack slots. There is no `S+8` stack request at the raw Begin callback and no dormant R+a78 request.

Unpublished stock pool returns retain the existing exact LR set `29798c,297cb4,297d40,29a8b8,29b080,29cbe0,29ccbc`. These entry observations preserve membership and invalidate Begin, not a pool-success or GPU certificate. The following paired reset/Begin remains mandatory. Turnover's wrapper-return success is separately checked at `3052f0`.

## Exact install inventory

`HOOK-REQUEST.json` is the machine-readable contract, with source-word references, available Desktop RX16 pins and the full required set. `required_sites()` contains 40 unique mandatory consumer/revocation sites. Nineteen are absent from the base `33439d7` runtime manifest:

```
2a1e3c 2a23f8 300a28 301800 301804 302790 303d10 303d70
304e60 304ec8 304ecc 3052e0 3052ec 3052f0 305308 3053a8
3053f8 305470 305490
```

`2a1f64` is additional optional branch validation, not necessary to reconstruct the supported path. If installed and delivered, mismatches refuse.

The newly supplied eleven-site pin package includes that optional observer. The four earlier compositor observers plus the ten required sites from that package are still not the complete consumer inventory. The remaining required observer boundaries are `302790`, `304e60`, `304ec8`, `3053f8`, and `305490`. Their exact instructions are in the supplied saved windows; the parent loader lane must authenticate their RX16 guards and relocation, then install them. Do not treat a continuation address or a consumer constant as an installed hook. No missing guard was invented here.

Existing bootstrap, constructor/Begin/reset/delete, retained-phase, renderer teardown and owner coverage must remain intact. Parent-owned actual loader coverage must be supplied before collector admission. The CPU fixture deliberately models this coverage and tests omission of each required PC independently.

## Verification

From this worktree, serial compilation under the unchanged 256 MiB / zero-swap cap:

```sh
systemd-run --user --scope -p MemoryMax=256M -p MemorySwapMax=0 \
  python3 -B native-portal/runtime/native_stock_facts_test.py
systemd-run --user --scope -p MemoryMax=256M -p MemorySwapMax=0 \
  python3 -B native-portal/runtime/native_stock_facts_test.py --sanitize
systemd-run --user --scope -p MemoryMax=256M -p MemorySwapMax=0 \
  python3 -B native-portal/runtime/stock_facts_test.py --sanitize --runtime-smoke
```

The witness uses Unicorn 2.1.4, installed only under ignored `runtime/build/native-stock-facts/python`. A normal Python venv was unavailable because this host lacks ensurepip; the private target install succeeded. No system package, headset or SSH operation was used. The witness imports no analysis runner and opens no vendor ELF. Its exact text/window hashes and generated Saved vectors remain in `runtime/build/native-stock-facts/{green,asan}/witness.json`.

`native_stock_facts_test.py --red` extracts only the tracked native-portal baseline from exact commit `33439d7bc553cf3fde85179cae778d1beaed2091` into owned build scratch. It runs the initial-Begin regression against that original code and is expected to abort at the future-load assertion. The same probe runs successfully in the current normal/sanitizer suites. Additional observed RED failures covered unscoped bootstrap assignment, missing positive-store provenance and unpaired factory name arguments before their repairs.

The positive collector test performs no self-process reads after qualified bootstrap through repeated dormant turnover and engagement, with protected historical graphs and tracked arena ranges. Factory success and failure vectors originate in pinned instruction execution; test relocation changes synthetic object identities, not observed argument lanes or native SP/LR relationships. The separately executed Begin, turnover-load and disposal islands confirm their register/stack contracts. Constructor scheduling and Vulkan/reset outcomes remain explicit source models.

Negatives cover each missing installed site, incomplete constructors, wrong native device/pool/name/C/W/SP/LR, missing store or return, failed-creation `304fa0` bypass, raw reset/Begin failures, stale Vk, unbegun publication, post-callback reader use, fork/foreign thread, protected graphs, bounded capacity and repeated same-address deletion/reconstruction. Existing stock, Evidence, no-layer and default Runtime smoke tests are retained, not truncated. Sanitizer and command logs plus source hashes are summarized in `native-stock-facts-test-results.json`.

Independent review is parent-owned. This lane has no reviewer delegation tool or installed coding-review CLI and does not claim independent approval. Actual loader emitter execution, installed callback delivery, full Runtime source-provider readiness and hardware acceptance remain integration work.
