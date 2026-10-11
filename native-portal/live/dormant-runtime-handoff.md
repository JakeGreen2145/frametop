# Dormant Runtime and current-source integration

## Completed implementation

Worktree `/home/jakeg/.hermes/work/frametop-native-dormant-runtime`, branch `feat/native-portal-dormant-runtime`.

The actual default `Runtime()` now runs the complete CPU-tested path:

1. DORMANT originals stay on their incoming caller with retained TLS scope and no owner pump. First engagement excludes every complete original before optional reads or allocation.
2. The provider selected before pin uses identity-only current-source contracts. Authentication retains the exact camera binding, Run owner, version, gate and loader module identity, without reading mutable camera source fields.
3. Portal acquisition enters the one existing hooked native Acquire. Its actual entry supplies SP/LR, then the loader-issued complete bypass runs once. Typed CurrentConnect completion and the real source event journal finish before portal token commit. Cold no-history success remains borrowed use, not Disconnect ownership.
4. Already-scheduled Update runs through the typed CurrentUpdate collector, including actual metadata acquire/ref/return observations. It is not an added native Update.
5. Claimed render invocations retain one caller and one admission reader through the actual render RET. Capture observes original Saved before upload/enqueue replacement. The existing upload ticket is handed back to the current collector.
6. The copied current selection supplies resource index, slot, texture, K and SP to Evidence staging. Current mode never falls back to the old broad selected-slot/texture rereads. The existing Adapter NativeBorrow callback consumes `with_selection` before its sole enqueue. The scalar sample alone grants no use.
7. The unchanged Command, Lifecycle and SourceCommand retain exact GPU, CPU and sampled-source settlement authority. Native render RET revokes read scope only. Pending Off remains Draining until those proofs finish. Borrowed use performs no native Disconnect.

Unclaimed stock rendering does not open an integration selection scope. A partial or missing current method result keeps a sticky source-failure hold in the existing Backend and keeps the camera closing barrier, even when failure happened before portal token creation. It cannot be mistaken for an unused integration owner.

`runtime_current_source.inc` is included in the production identity hash. `Evidence::stage` adds a default-false copied-selection parameter; legacy callers retain their previous behavior.

## Dependencies already in this branch

The previously reviewed admission/routing commits `b447a5b` and `42539b6` remain in history, along with the reviewed stock collectors and current-source component.

This continuation applied only these delivered dependencies:

- Current-render hooks `74a26a5` as `9f702f4`.
- Reentrant selection-revocation fix `f1fe5b7` as `0a7091b`.
- Frozen hook-consumer test repair `833bdb8` as `4cf87b0`.

The owning Runtime test derives modeled installation from canonical prepare/manifest inputs, not collector-required arrays. Its asserted inventory is 200 compositor plus 32 client sites. Production still uses the actual installed-sites API.

## Verification

All compiles and tests ran serially under `MemoryMax=256M`, `MemorySwapMax=0`, with configured scratch TMPDIR. No installs, SSH, native activation or device access occurred.

```sh
python3 native-portal/live/dormant_runtime_test.py --current
python3 native-portal/live/dormant_runtime_test.py --current --sanitize
python3 native-portal/live/dormant_runtime_test.py
python3 native-portal/live/dormant_runtime_test.py --sanitize
python3 native-portal/live/dormant_inventory_test.py
python3 native-portal/live/build_inventory_test.py
```

Results:

- Current Runtime: 13/13 scenarios pass normally and with ASan+UBSan.
- Preserved dormant Runtime: 7/7 scenarios pass normally and with ASan+UBSan, including the blocked caller, nested original, actual Connect history and original-versus-first-engagement race.
- Current cold, warm, stock-render recovery and raw Update `0x103` cases each execute two complete Runtime instances in one process. Each instance produces both owned packets, encodes both eyes, submits once, observes pending socket Release, proves exact settlement, performs no Disconnect and tears down after the actual C++ join plus modeled native join callbacks.
- Warm startup seeds its journal from complete observed Update metadata pairs. Its subsequent native Acquire returns along the already-connected branch with no service Connect observations.
- Missing-current-hook refuses capability without native acquisition and still tears down normally.
- Partial upload, rejected first enqueue, partial Connect uncertainty, nonzero client-module identity migration before the second enqueue, wrong actual RET, missing GPU wait, missing CPU disposal and missing sampled-source proof all retain debt. Each reaches the exact joined-cleanup refusal: SIGABRT, reason 5, event `0x8b020`, joined 1, with zero native cleanup calls. The runner rejects assertion aborts, unexpected returned cleanup and any sanitizer finding.
- The unchanged Evidence test executable passes normally and sanitized, including its 35 staging cases. It links the current production Evidence/staging objects; no ELF analysis runner was imported.
- Both production inventory tests pass. `git diff --check` passes.

Artifacts under `native-portal/build/dormant-runtime/` contain full logs, commands and source hashes:

- `acceptance.json`, `sanitize/acceptance.json`: preserved seven-case suites.
- `current/acceptance.json`, `current/sanitize/acceptance.json`: current thirteen-case suites.
- `current/staging-red.json` and corresponding cold/warm logs: first enqueue failed before the current-selection staging connection.
- `current/stock-render-red.json`: unclaimed stock rendering poisoned later capability before the scope restriction.
- `current/source-uncertainty-red.json` and log: actual forbidden native cleanup exit 70 before the failed-current-source closing hold.
- `legacy-evidence-results.json`: both legacy Evidence executions and exact compile/link commands.
- Original `baseline-red.json` remains the earlier timeout reproduction, not current acceptance.

The runner verifies frozen private pump, camera-transition and selection headers by SHA256 and copies them into its own build directory. `--fixtures` can point to another directory containing those exact headers. The old ephemeral camera-abort coordinate JSON was absent, so the new fixture uses explicitly labeled source-model coordinates from existing contracts. It does not claim a fresh native execution result.

## Scope and remaining verification

Native bodies, native memory transitions, service responses, Vulkan operations, installed inventory and native worker-join callbacks are modeled. Real Runtime, Backend, socket protocol, CameraOwner/gate, admission, source collectors/provider, Evidence, Adapter, Command, Lifecycle, SourceCommand and pthreads execute. The late camera-slot/texture values are deliberately made unusable before enqueue to verify the new copied-selection path.

This is not GPU rendering, hardware usability, live timing, ARM64 execution or loader-island validation. Parent owns independent final-delta review, ARM build and any separately authorized headset activation. No new native-contract research dependency is claimed.
