# Current-render loader handoff

## Delivered delta

Base `0f387ff207eb52d99b80848a57bab207e374043a`. Frozen consumer
`673f1149e929aaa218716d572499b1fc919614a1`, implementation `5bd84e0` plus
`1be8d33`. The tests compile that consumer's actual
`CurrentRenderCollector::required_sites` and compare its 12 PCs with the request
and freshly generated canonical loader inventory. They also compare the request
and header bytes with the frozen Git blobs.

Added only these nine missing compositor manifest records:

| PC | Existing preparer kind | Observation |
| --- | --- | --- |
| `160be8` | 0 | Entry before native `SUB SP,SP,#0x460` |
| `160ca8` | 3 | Connected byte after native load, before CBZ |
| `160dd4` | 5 | Early pre-RET, revoke scope only |
| `161bc0` | 1 | Actual BL to selector `160a80`, native LR `161bc4` |
| `161be8` | 0 | Selector in x0 before MOV w28,w0 |
| `161c74` | 0 | Exact x0 operand before slot load, not future x24 |
| `161c78` | 0 | Actual loaded x24 with the same operand |
| `161e40` | 0 | Selected texture x0 before packet store |
| `161f30` | 5 | Normal pre-RET, revoke scope only |

Every added record uses existing ordinary observer emitter 0. Words, complete
16-byte guards and their SHA256s come from the frozen request and were checked
against the exact file-backed executable span of compositor SHA256
`9b1bd5fe4463592cbc71b2517b9ceb0a0fcd66cd6885a19dca4be0784bf6c265`.
No pin was missing or malformed. RET metadata follows the existing manifest's
numeric successor convention; actual kind5 emission returns through native x30.

The actual merged prepare inventory already contained the other three PCs.
Their complete selected rows and specialized emitters remain unchanged:

- `161d64`: arena emitter, index 0.
- `161d68`: arena emitter, index 7.
- `161ed8`: replace-call emitter, index 2.

Python counts: 200 compositor plus 32 client callbacks, 232 total. The manifest
has 166 records, previously 157. All 157 old records, top-level metadata and 191
previously selected compositor rows compare equal. The previous complete
40-command and 75-source required sets remain covered. No new renderer,
Runtime/Evidence/provider/camera-owner/build-list or emitter code was changed.

## Consumer API

Use the unchanged `ft_native_portal_installed_sites` export from
`stock_observer_coverage.h`, version `FT_INSTALLED_SITES_VERSION`, module tag zero
for this compositor set. It returns all actual selected PCs after the dual-module
transaction. Current collector `Covered(Module,pc)` must query that authenticated
inventory, not a copied required list or a V1 full mask.

Complete current set in consumer order:
`160be8 160ca8 161bc0 161be8 161c74 161c78 161d64 161d68 161e40 161ed8 160dd4 161f30`.

No additional coverage ABI or mask is needed. The 44-bit V1 diagnostic mask
`0xfffffffffff`, its record layout/version, and old camera mask `0xfffff` are
unchanged. Tests omit each new site independently while deliberately retaining
the stale generated V1 full-mask macro. Each actual installed inventory then
fails the complete current set while V1 and camera coverage remain valid.
Default-off, missing-router strong-link refusal, invalid version/tag, fork,
disabled routing and module-lifetime gates are reused unchanged.

Runtime still owns original-Saved observation before upload/enqueue argument
replacement at `161d64` and `161ed8`, ticket association, collector routing and
source admission. These loader observers do not establish source or GPU debt
settlement. Both pre-RET sites merely end the selection scope.

## Verification

First TDD run failed with exactly nine missing callbacks and 191 rather than 200
compositor sites. `--baseline-red` replays the exact base preparer, manifest and
core pins in an isolated ignored tree and reproduces that failure, exit 1.

Final normal and ASan+UBSan commands both exited 0. All compiles and tests ran
serially under MemoryMax=256M and MemorySwapMax=0, using existing dependencies.

```sh
export CPATH=/home/jakeg/.hermes/work/frametop-native-live-loader/native-portal/build/live-loader-test/deps/usr/include:/home/jakeg/.hermes/work/frametop-native-live-loader/native-portal/build/live-loader-test/deps/usr/include/x86_64-linux-gnu
export LIBRARY_PATH=/home/jakeg/.hermes/work/frametop-native-live-loader/native-portal/build/live-loader-test/deps/usr/lib/x86_64-linux-gnu
systemd-run --user --scope -p MemoryMax=256M -p MemorySwapMax=0 \
  python3 -B native-portal/live/current_render_hooks_test.py --baseline-red
systemd-run --user --scope -p MemoryMax=256M -p MemorySwapMax=0 \
  python3 -B native-portal/live/current_render_hooks_test.py --full
systemd-run --user --scope -p MemoryMax=256M -p MemorySwapMax=0 \
  python3 -B native-portal/live/current_render_hooks_test.py --sanitize
```

Verified:

- Actual dual-module host installer, full inventory and nine separately compiled
  omissions, normally and sanitized. Includes file/map authentication, exact
  patched destinations, precommit rollback, fatal postpatch failures and bounded
  inventory output. No sanitizer finding.
- 72 original-versus-selected-island comparisons across all 12 consumer sites,
  three load biases and both conditional outcomes. All GPR/SIMD registers, SP,
  NZCV/FPCR/FPSR and native memory effects match. Ordinary pad stays unspecified
  and untouched. Specialized selectors retain their existing zero initialization.
  Every full Saved record passes the real host dispatcher unchanged with errno
  restored after its deliberately clobbering observational sink.
- Actual pinned selector BL/helper/continuation with native LR; pre-MOV x0 versus
  stale x28; paired pre/post slot load; texture load and packet store at slots
  0, 3 and 15. Both connected branches execute, including false connected through
  the native restored-SP/LR epilogue and early RET without selection callbacks.
- The six existing stock prepare/coverage tests, plus ruff F checks and
  `git diff --check`. The old whole-inventory A64 suite was not needlessly rerun.

`current-render-hooks-verification.json` records exact counts, selected rows,
consumer/source pins, final source hashes and RED/GREEN log hashes. Logs are in
`/home/jakeg/.hermes/cache/scratch/current-render-hooks-*.log`.

An initial chain fixture failed because it left texture index registers patterned
and compared the retired native frame against observer scratch below restored SP.
The fixture now initializes the actual native operands and compares live native
memory, while the separate displaced-instruction test retains complete state
checks. No production emitter repair was necessary.

CPU limits: native memory contents and scheduling between bounded selection
blocks are synthetic. The sink is the compiled loader dispatcher, not Runtime.
The ARM client handle is a test token after real file/RX authentication, not a
host dlopen of ARM code. The normal loader DSO supplies Unicorn emission even
in the sanitizer run; the separate full and omission installer executables and
all their emitters are ASan+UBSan-instrumented. No Frame installation, native
activation, SSH, camera access, GPU proof or ARM build is claimed. Final Runtime
integration, independent review and inactive ARM build remain parent-owned.
