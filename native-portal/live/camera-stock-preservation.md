# Stock cleanup and aggregate camera demand repair

## Scope and integration dependency

This repair changes Runtime, CameraOwner and DemandArbiter. SourceAdmission stays a separate default-null interface. The test provider remains explicitly modeled. CPU, GPU and original source-consumer debt still prevent destructive native calls and cleanup.

The parent/source-hook worker owns the loader, manifest, emitter and build integration. This commit requires the two decision hooks below. It is not a deployable aggregate-wiring result without their installation and emitted-instruction tests. Runtime requires their coverage bits before admitting provider-backed portal work. This is an installation guard, not a replacement for coexistence arbitration.

No CameraOwner invocation-ticket plumbing was changed; the source worker can add its observation scope independently.

## Native decision contract

Pinned ELF SHA-256: `9b1bd5fe4463592cbc71b2517b9ceb0a0fcd66cd6885a19dca4be0784bf6c265`.

Release now means release the authenticated caller's demand, rather than release every caller's shared producer. The complete original handler and native camera method run once for the request. The actual method supplies its result. Runtime never writes an accepted-queue value into the native result, retries a camera method speculatively, or compensates for another caller's Release with Acquire.

Within the retained owner mutation, nested application/camera calls inherit the outer stock object or foreign connection identity. The native camera entry updates that caller's logical level before the producer decision. Portal tokens and committed debts are additional demand. Source readiness and queue ownership are separate facts.

Required hook 1, coverage bit 18:

- PC `0x15fbb8`, inside Release `0x15fba8`, before the call to Clear.
- Original word `0x97ffffd0`, `BL 0x15faf8`.
- Guard `d0ffff9760824d397f2a0d39c0000035`.
- Guard SHA-256 `964722f6c64a9622b86c9176bd03dce535c43ae95e6d35e846e84b792ffea1ae`.
- Normal: perform the original call with LR `base+0x15fbbc`, then continue there.
- `Saved.pad=SkipOwned`: restore saved state and branch to `base+0x15fbc8`. Do not return from the island and do not synthesize x0. That native branch supplies success, clears the native per-frame `+349` flag and executes the original epilogue. It preserves producer Set, `+34a`, `+360` and both queue connections.

Required hook 2, coverage bit 19:

- PC `0x15fb20`, inside Clear after acquiring the native lock/shared state.
- Original word `0x91400801`, `ADD x1,x0,#2,LSL#12`.
- Guard `01084091e01f00f921b0433961000034`.
- Guard SHA-256 `7ae9fa31be75d65bf77b35d08feddbf70f8f92f234c1bff459e8301620ccf290`.
- Normal: execute the displaced ADD, continue `base+0x15fb24`.
- `Saved.pad=SkipOwned`: restore saved state and branch to `base+0x15fb74`, the native lock unwind and void return. This keeps a direct Clear from erasing surviving producer demand.

Both callbacks require the authenticated owner, an active retained native call and x19 matching the bound camera. Original cleanup after the exact join remains stock and bypasses aggregate suppression. Native Set/Connect retain their existing idempotent behavior. When the final demand disappears, native Clear/Disconnect runs normally, and the same arbiter journal consumes the actual provider receipt even when a foreign caller is last. Missing or failed receipts keep integration obligations and block cleanup.

`camera_idle_native.witness(aggregate=True)` executes these pinned branches and the actual Acquire/Release public handlers. It verifies connection request markers, public reply fields, shared producer Set preservation, zero service calls for held Release, a real failed Disconnect returning a false response, and final disconnect. Service, lock and memory ownership boundaries remain CPU models. This witness does not execute the parent-owned emitted islands.

## Cleanup distinction

`DemandSnapshot.integration_obligations` records an attempted integration connection that has not fully detached. Initial Unknown service state is not an obligation created by merely binding the owner. After close has canceled new admission, drained retained calls and retired logical tokens, an unused owner may release its barrier without changing Unknown to Disconnected or Ready. Faults, committed debt and attempted unsettled integration calls still block cleanup. The original no-source test now joins and runs complete native cleanup instead of `_Exit`.

## Verification and limits

Original scratch `run_review.py` and `native_review.py` were replayed unchanged before repair. Both unused-provider modes and the overlap-release mode aborted as reported. The original eight-cycle path passed. The native A/A/R witness again confirmed that deduped Acquire adds no service reference and the first unconditional Release disconnects both queues.

After repair, the focused Runtime test preserves the overlap stimulus and original failing assertion. It reports `acquires=2 releases=0 active=1 pending=0`, checks the actual native return and performs normal cleanup. The old review fixture's subsequent attempt to draw from the externally invalidated frame is not a valid positive continuation: with the original failing assertion fixed, that later draw hits the existing guard. The focused test therefore ends the overlap scenario before that draw, rather than weakening external full-display invalidation. This is not a claim that the complete unchanged old overlap executable exits successfully.

Commands exercised under 256 MiB / zero-swap serial compilation:

```
python3 native-portal/live/camera_runtime_test.py --matrix
python3 native-portal/live/camera_runtime_test.py --sanitize --matrix
python3 native-portal/live/camera_owner_test.py --sanitize
```

The Runtime matrix includes null and unused providers, missing decision-hook coverage, stock-only multi-caller cleanup without a provider, portal-first and portal-last release orders with two foreign connections and one stock caller, direct Clear, and the existing eight full stereo CPU/GPU/source settlement cycles. Failed final Disconnect, partial provider receipts, missing GPU/CPU/source return, source mutation over debt, timeout and failed join retain their intended fail-stop guards. The standalone owner test confirms unused cleanup leaves native idle and portal readiness false.

Generated logs and result JSON are in `native-portal/build/camera-runtime` and `native-portal/build/camera-runtime-asan`; the pinned aggregate witness is `aggregate-native.json` in those directories. No device, SSH, runtime activation, GPU rendering or artifact transfer was used. The separate update-debt probe was not promoted or repaired in this round.
