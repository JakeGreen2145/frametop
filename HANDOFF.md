# Retained incoming camera caller lifetime

## Identity and scope

- Worktree: `/home/jakeg/.hermes/work/frametop-native-caller-retention`
- Branch: `feat/native-portal-caller-retention`
- Exact base: `27e908b4c8086a9d09b9f9475902220e1cc1e18b`
- Delivery head: the commit containing this handoff, obtainable with `git log -1 --format=%H -- HANDOFF.md`. The final delivery message also records its full SHA.
- Component-only source work. No headset, SSH, runtime activation, native binary analysis or private binary data added. Parent integration files and plans were not changed.

## Implemented contract

`CameraOwner::retain_caller(expected_epoch, expected_generation)` returns an empty or move-only `RetainedCaller`. The incoming integration must authenticate the caller and expected identities before calling. The handle retains the actual production gate before any native pointer lookup. `binding(out)` retrieves only the immutable `CameraBinding` through that exact ticket, without queue pumping or owner execution. Keep the handle through the complete original invocation, including nested callbacks.

Only the acquire-visible `Retained` stage admits callers. Epoch/generation and gate scalar publication precede that stage. Initial, unbound, clean-refused, partial and closed states refuse. New methods check process identity before inherited locks. They do not read the owner thread ID, queue, arbiter or mutable owner-only fields. The caller-accounting mutex joins retain/release to final barrier release; gate admission remains the existing gate's decision. No locks are held across native execution and these methods execute no native operations.

`release()` returns true only after releasing its exact ticket and owner count. A false result preserves a live handle and accounting for retry. Empty/moved-from/double release cannot release another ticket. Synchronized moves and releases across threads remain supported. RAII and move assignment fail-stop on failed fallback release. Outstanding handles make owner destruction fatal; the handle does not extend the C++ owner's storage lifetime. Join users and finish every handle before owner destruction.

`finish_close()` cannot report `Closed` or release its integration barrier while caller handles remain. Existing queue/debt/source/native-disconnect checks are unchanged. Successful integration close does not itself execute native cleanup or assert globally idle camera state. Real cleanup remains a separate complete original through `CameraNativeBinding::cleanup`.

This API grants no graph-read scope, mutation exclusion, source-arbiter authority, source readiness or native-thread permission. Public documentation is in `native-portal/live/camera_owner.md`.

## Changed files

- `native-portal/live/camera_owner.hpp`
- `native-portal/live/camera_owner.cpp`
- `native-portal/live/camera_owner.md`
- `native-portal/live/camera_caller_test.cpp`, new actual-component tests
- `native-portal/live/camera_caller_test.py`, new capped serial runner
- `native-portal/live/camera_owner_test.py`, optional saved opaque fixture mode
- `HANDOFF.md`

No changes to Runtime routing, gate implementation, FIFO, arbiter, Renderer or source provider. No new transitive production link dependencies.

## Verification

All commands below ran from this worktree with `TMPDIR=/home/jakeg/.hermes/cache/scratch`. Compiler/test subprocesses were serial and capped with user systemd `MemoryMax=256M`, `MemorySwapMax=0`. GCC was Debian 12.2.0. New and full-owner runners stop their exact owned scope on timeout. No run timed out.

### TDD evidence

1. New caller tests against the original API failed compilation because `retain_caller` did not exist. Log: `build/camera-caller/red-missing-api.log`.
2. With declared handle methods and deliberately empty admission, the real threaded positive failed at `assert(call)` inside `blocked_owner_original_lifetime`. Log: `build/camera-caller/red-behavior.log`. No stub remains.
3. Actual production retention and close integration passed the tests below.

### Commands and results

```sh
python3 native-portal/live/camera_caller_test.py
python3 native-portal/live/camera_caller_test.py --sanitize

python3 native-portal/live/camera_owner_test.py --saved-idle-fixture /home/jakeg/.hermes/work/frametop-native-live-integration/build/camera-owner/camera_idle_fixture.hpp
python3 native-portal/live/camera_owner_test.py --sanitize --saved-idle-fixture /home/jakeg/.hermes/work/frametop-native-live-integration/build/camera-owner/camera_idle_fixture.hpp

systemd-run --user --scope --unit=camera-caller-gate-check -p MemoryMax=256M -p MemorySwapMax=0 -- python3 native-portal/control/camera_gate_test.py
systemd-run --user --scope --unit=camera-caller-binding-check -p MemoryMax=256M -p MemorySwapMax=0 -- python3 native-portal/binding/camera_test.py
python3 native-portal/control/owner_coordinator_test.py
python3 native-portal/control/owner_coordinator_test.py --sanitize

git diff --check
```

All exited zero. ASan/UBSan runs reported no sanitizer findings. Logs are under `build/camera-caller/`: `green.log`, `sanitize.log`, `owner-regression.log`, `owner-sanitize.log`, `gate-regression.log`, `binding-regression.log`, `coordinator-regression.log`, `coordinator-sanitize.log`.

Caller tests cover:

- Real incoming listener retention/binding while the owner does not pump, a condition-variable-delayed complete modeled original, owner close refusing fresh callers, cleanup suppressed, original return/release, and genuine barrier close followed by counted cleanup.
- Gate retain paused before versus after actual acquisition, proving both close orderings without scheduler sleeps.
- Nested independent handles, the gate's real shared capacity, repeated slot reuse, stale epoch/generation, moves, double release and cross-thread release.
- Failed release retaining both accounting and binding for retry, with test-only linker wrapping at the production gate boundary.
- Refused partial bind, acquire-visible publication, and fork refusal while an actual caller mutex is inherited locked.
- Exact fail-stop owner destruction, handle destruction and failed destination move-release. A dedicated terminate exit marker distinguishes these from unrelated assertion aborts; native cleanup counters never manufacture expected aborts.

Every new native callback and identity is explicitly modeled. The new suite needs no private fixture or ELF. It compiles the real owner, gate, native cleanup binding and coordinator, link-discarding only unused RenderDebt methods. The full-owner regression separately exercises actual Adapter/Command/Lifecycle and CPU/GPU/source debt protections. Its optional saved fixture branch copies opaque saved bytes without importing or executing native analysis. This run is saved receipt replay, not fresh pinned instruction execution.

Saved header SHA-256:

`689fd3fc215a0dfc2eb37b366de6d7bb9f3c55aceff439b4161e0b51d095d76b`

### Verified source SHA-256

```text
3f0bacc463c78a9b482ed81c7297eea60c0af2fc9754ce6bbc22eb8e22c3eee7  native-portal/live/camera_owner.hpp
ab0132c589f6a44550ee68b8a6d229730f3e42c32d32d9bcf3962e6919d4697d  native-portal/live/camera_owner.cpp
a560a317532550fa7ac8a3045b9f237f1400d8ee91128c178788b929b82b5cef  native-portal/live/camera_caller_test.cpp
8e2fde7807b93f956a11b84e1fc258c940b7bc6f0df41d99859daad081a2855c  native-portal/live/camera_caller_test.py
2bfe22ef0ef5da0bde4d07048ed8cd90fc8368e0198f22726d0931b592112601  native-portal/live/camera_owner_test.py
4cff0f6c7e254a5037dd0e0263ff14a3510d2ea179064ff4a207fb32ec424281  native-portal/control/camera_gate.hpp
055d67b7a1da9d630ab60a5cc52aed84c0f88369324aebd40653019d6815a61c  native-portal/control/camera_gate.cpp
f33cc3c731227ddab5a49a676b88efb1e0870650436a6f69e86800f95b6e049f  native-portal/binding/camera.hpp
8131e8113eae70674b48c4c070204043b82ffc6e7c444acf546ac33f1d39eed8  native-portal/binding/camera.cpp
4fa4708365ac9a5f56c4c1c3e379714b814ee7d71edc5bd417b5d55e03915d42  native-portal/control/owner_coordinator.hpp
94bda2ac1844f45bedd2a2405a54f7f38aa28037f8ae1a3b59c423515b7d3baa  native-portal/control/owner_coordinator.cpp
```

## Review and remaining integration

Delta self-review and added-line security scan found no blocking issue. Source review checked release/acquire publication, lock order, close/retain interlock, exact ticket ownership, retry accounting, inherited-lock refusal and unchanged native debt protections. An independent review was not available within this subagent; parent integration should review this small production delta.

`Runtime::camera_route` and the 250 ms queue timeout are unchanged. No `0x13a100` or other original handler moved to its incoming thread. The existing `blocked_owner_test.*` Runtime reproduction was not rerun or repaired by this component change. After Desktop establishes the native thread/source contract, Runtime must authenticate the incoming caller, acquire this handle before its first pointer lookup, keep it through the complete original, preserve actual native outputs and source accounting, then explicitly release or fail-stop/retry. This component alone does not make original-thread mutations safe and does not certify live stereo portal output.
