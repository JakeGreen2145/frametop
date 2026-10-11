# Admission-alternative observer handoff

Base `6674da53a3d319f9de360655a0f6ed17182a66de`, branch `feat/native-portal-admission-hooks`.

## Delivered

Canonical preparation now selects 205 compositor and 37 client sites, 242 total.
The exact 232 previous selected rows compare equal, including names, continuations,
words, guards, kinds, emitter selection and indices. Old manifest records and
all top-level metadata are unchanged.

Added ordinary observer emitter 0, index 0:

| Module | ELF PCs | Kinds |
| --- | --- | --- |
| client | `3280e0 32766c 328148 3276d0 327e60` | all 3 |
| compositor | `160ca0 160db0 160e34` | all 3 |
| compositor | `1621c4` | 4 |
| compositor | `160e60` | 0 |

Diagnostic-only client `328170` remains absent. Existing emitter implementations,
call/store observers, byte authentication and production installer are reused.
No new mask ABI or hardcoded global authority was introduced. Established V1 and
camera masks remain unchanged.

Consumers must use `ft_native_portal_installed_sites`, version 1, with compositor
tag 0 and client tag `0x8000000000000000`. Returned client PCs carry that tag.
A continuation address is not an installed callback. The API remains qualified
by the completed dual-module transaction and existing module lifetime gates.

`admission-hooks-verification.json` exports the exact canonical added set, tagged
callback PCs, source/proof hashes, derived counts and log hashes. It is the parent
comparison input for the consumer's actual required arrays and hook request.
Production build identity already hashes both modified production inputs,
`prepare.py` and `runtime-sites.json`; no discovery change was needed.

## Verification

All builds and instruction tests ran serially under MemoryMax=256M and
MemorySwapMax=0. No packages were installed.

- Initial TDD run failed with exactly the ten missing sites. `--baseline-red`
  reproduces this against the exact base preparer and manifest, exit 1.
- `admission_hooks_test.py --full` and `--sanitize` exited 0. Each runs the real
  preparer, host installer and full plus ten independently compiled missing-site
  inventories. Every omission is reported by the actual dual-module installed-PC
  API despite the deliberately stale full V1 mask. ASan/UBSan reported no finding.
- Both modules reject a modified first instruction byte and guard-tail byte at
  every selected site before any patch-phase protection change. Precommit page
  rollback, postpatch fail-stop, exact patch destinations, bounded output,
  invalid version/tag, fork, disabled routing and client lifetime refusal pass.
- 195 selected-island comparisons execute the new original pinned instructions
  at three load biases. B.cond uses all 16 NZCV combinations; CBZ uses zero, one
  and a high-only value to distinguish W/X width. Both branch destinations,
  GPR/SIMD/SP/LR/flags, memory and unspecified Saved padding match. Complete Saved
  records cross the actual host dispatcher with errno and bytes preserved.
- 29 paired native block vectors cover manager GOT entry/exit and conditional
  original calls, mode0 latest reason and literal return3 with stale outputs,
  native mode/connected bypass, retained load/FP ramp/store, negative/zero exits,
  positive continuation and the actual B.GT clamp predecessor/store/join.
- Arbitrary arrival at `1621c4` with stale s0=1 leaves the output slot unchanged.
  Its callback is observational, not a positive predecessor or admission proof.
- `loader_test.py` passed five tests including 1398 merged instruction replays.
  `stock_observer_test.py --full` passed existing instruction/call/store witnesses,
  44 missing-site inventories and old-header refusal. `current_render_hooks_test.py
  --full` passed its nine omission cases and actual selection chains.
- Ruff F checks and `git diff --check` passed. No static secret, shell-injection,
  eval/exec or pickle finding in the added code. Independent review remains
  parent-owned and was not performed in this worker.

Logs and private generated binaries remain in this worktree's ignored
`native-portal/build/`. Full Saved vectors are in
`build/live-loader-test/admission-native-vectors.json` with hash in the report.

To reproduce, export the existing dependency paths, then run the commands above
under `systemd-run --user --scope -p MemoryMax=256M -p MemorySwapMax=0`:

```sh
export CPATH=/home/jakeg/.hermes/work/frametop-native-live-loader/native-portal/build/live-loader-test/deps/usr/include:/home/jakeg/.hermes/work/frametop-native-live-loader/native-portal/build/live-loader-test/deps/usr/include/x86_64-linux-gnu
export LIBRARY_PATH=/home/jakeg/.hermes/work/frametop-native-live-loader/native-portal/build/live-loader-test/deps/usr/lib/x86_64-linux-gnu
```

The first build lacked OpenSSL development headers until those existing paths
were supplied. This was test setup, not a product regression. `loader_test.py`
already locates the existing Unicorn package. Environment exports must reach the
same process as `systemd-run`.

## Parent integration obligations

- Reconcile consumer required arrays/request with the exact exported ten-site set.
- `dormant_runtime_test.py:70` still asserts `(200,32)`. It is consumer-owned and
  intentionally untouched. Reconcile it to the derived `(205,37)` inventory.
  The owning older loader fixture's count checks were updated here.
- Run the combined default Runtime proof. In particular, only the source
  predicate may authenticate ramp/clamp predecessors, recognized noGrant and
  manager lock/bypass state. These observers create no ownership permission.
- Perform independent review and the combined ARM build before activation.

Native objects, clock, pthread and guard dependencies in block tests are explicit
CPU fixtures. The host ARM-client handle is a fixture token after real file/RX
mapping authentication. Unicorn uses normal emitted code; separate sanitized
installer executables include sanitized emitters. No source policy, graph,
factory, native service, camera/GPU lifetime, headset, SSH, activation or new
opaque native analysis is claimed.
