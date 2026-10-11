# Stock observer loader handoff

## Current scope

Work in progress on `feat/native-portal-stock-observer-hooks`, based on
`33877a86468b2a4422c1014d81414e4136f46b3c`. No headset, SSH, runtime activation,
package installation or new opaque binary analysis.

The canonical loader now selects the five primary continuation16 observers and
all three supplied metadata guard lifecycle candidates. Actual prepare hashes
both private ELF files, checks their RX spans and validates the supplied bytes.
The old 122 runtime-manifest entries and all old top-level metadata are unchanged.
Current merged inventory is 160 compositor sites and 27 client sites.

This is NOT yet complete dormant-command provenance coverage. The parent is
requesting additional exact 16-byte guards from Desktop. The native collector
worker owns reducer positions, invocation pairing and the scalar receipt API.
The requested additional PCs are all absent from the current selected inventory:

`2a1e3c 2a1f64 303d10 303d70 301800 301804 300a28 305308 3053a8 305470 3052f0`.

Do not engage from the primary-observer mask alone. An interior output store is
not a successful constructor/factory receipt, and `3052ec` does not establish a
completed pool return. The CPU witness reaches `3052f0`, but there is no installed
observer there yet. No new guard has been inferred from a single instruction.

## Coverage API, pending additional inventory

`live/stock_observer_coverage.h` exports:

```c
int ft_native_portal_stock_observer_coverage(uint32_t version,
    struct ft_stock_observer_coverage_v1 *out, size_t size);
```

Version 1 currently has a 40-byte record with `version`, `size`, `installed_mask`,
`required_mask`, `compositor_base`, `client_base`. Named bits 0 through 7 correspond
to the rows below. This contract remains local to this unmerged component while
the complete command collector inventory is being reconciled.

| Bit | Module | PC | Callback fact before displaced instruction |
|---:|---|---|---|
| 0 | compositor | `304ecc` | C=x22, W=x21-8 after actual hidden-output store |
| 1 | compositor | `2a23f8` | R=x19, W=x0 after R+1380 store |
| 2 | compositor | `3052e0` | C=x1, W=x19; null path preserved |
| 3 | compositor | `3052ec` | pool container=x0, C=x1, W=x19; original BL and LR=3052f0 |
| 4 | client | `2584c4` | literal uint32 x0 lock result, guard=x19, inner LR=2584c4 |
| 5 | client | `258438` | guard=x0, accessor entry SP and outer caller LR |
| 6 | client | `2584f8` | normal accessor return, restored entry SP and outer LR |
| 7 | client | `258508` | matching guard=x0 release entry, revoke before native release |

Client callback PCs carry bit 63, unchanged from the existing dual-module loader.
No emitter was added or changed. The selected ordinary emitter handles all eight.
Callbacks must leave the entire 800-byte Saved record unchanged, including pad.
Pad is unspecified for ordinary observers, not an initialized decision selector.

The generated mask comes from exact merged module/PC/word/kind/guard/emitter/index
matches. The exported API independently checks the selected tables, pinned module
hash strings and existing retained dual-module lifetime gate. A missing or changed
site clears its own bit even if the generated full mask is stale. Missing old-header
coverage metadata returns unsupported. The legacy camera mask remains `0xfffff`.
Return 1 means a valid coverage snapshot, not complete coverage or readiness.
Unknown versions or sizes do not write the caller's output. Unavailable installed
module state returns 0 and a zero record.

## Tests completed for the initial eight observers

All compilations ran serially in a user scope with `MemoryMax=256M` and
`MemorySwapMax=0`. Existing private OpenSSL headers were reused read-only after
the first compilation reported missing system `openssl/evp.h`:

```sh
export CPATH=/home/jakeg/.hermes/work/frametop-native-live-loader/native-portal/build/live-loader-test/deps/usr/include:/home/jakeg/.hermes/work/frametop-native-live-loader/native-portal/build/live-loader-test/deps/usr/include/x86_64-linux-gnu
export LIBRARY_PATH=/home/jakeg/.hermes/work/frametop-native-live-loader/native-portal/build/live-loader-test/deps/usr/lib/x86_64-linux-gnu
systemd-run --user --scope -p MemoryMax=256M -p MemorySwapMax=0 \
  python3 -B native-portal/live/stock_observer_test.py --full
systemd-run --user --scope -p MemoryMax=256M -p MemorySwapMax=0 \
  python3 -B native-portal/live/loader_test.py
systemd-run --user --scope -p MemoryMax=256M -p MemorySwapMax=0 \
  python3 -B native-portal/live/source_loader_test.py
systemd-run --user --scope -p MemoryMax=256M -p MemorySwapMax=0 \
  python3 -B native-portal/live/camera_loader_test.py
```

Observed RED before production changes: all eight exact sites absent and generated
coverage absent. The original terminal log is
`/home/jakeg/.hermes/cache/terminal-output/out-1791737384-839985-ad70.log`.

Current GREEN results:

- Five prepare/coverage tests, including every missing, duplicate, wrong-word,
  wrong-kind, wrong-guard, wrong-emitter and wrong-index site; both wrong module
  pins; actual RX bytes against all delivered guards; no-manifest partial coverage.
- Three instruction suites. 48 primary/lifecycle single-instruction comparisons
  across three load biases and both conditional outcomes. All GPRs, Q registers,
  SP, NZCV, FPCR, FPSR and native memory effects agree with the original instruction.
  Every real emitted Saved record traverses the compiled loader dispatcher to an
  explicitly observational host test sink, with full 800-byte equality and exact
  module-tagged callback PC.
- Saved metadata accessor/release instructions executed with both raw lock results
  and both native TLS paths. Entry SP/outer LR, inner lock LR, actual accessor RET
  and revoke identity are recorded. False lock results leave stock execution unchanged.
- Delivered producer stores precede their callbacks. The pinned wrapper destructor
  preserves null and nonnull paths, BL target, native LR and native return. Its pool
  body is explicitly modeled, not a queue or GPU settlement proof.
- Eight separately compiled missing-site inventories all refuse full coverage,
  retaining the old camera mask. An old generated header refuses the new API.
- Existing loader suite: 1128 emitter comparisons and 24 specialized runtime routes.
  Existing dual-module suite: 1290 emitter comparisons. Existing camera suite:
  68 original/bypass/return/trap routes.
- ASan/UBSan loader and all emitter builds passed the real host file/map,
  single-module and dual-module installation, rollback and coverage tests. Run
  `stock_observer_test.py --sanitize` for the combined reproduction.
- Existing exact file/map/RX checks, second-image precommit rollback and fatal
  postpatch/rollback checks passed. Real inert preload remains default-off and
  rejects wrong executable/opt-in. No ARM DSO was dlopened on this x86 host; the
  CPU coverage fixture explicitly models only the retained handle token.

## Parent integration obligations

Keep Runtime, Evidence, source ownership and activation outside this component.
Require the complete final loader inventory before enabling the new dormant
feature, then require actual paired per-invocation scalar facts. Do not interpret
historical pointers or a global last-seen lock bool as native read authority.
Lock false, missing, overlap, exception or abandoned accessor scopes withhold
integration but cannot rewrite native stock results. Metadata API success is not
lock success. Update's trace-context helpers do not provide a camera mutex.

Final full Runtime consumption, inactive ARM build, independent review and any
live activation remain parent-owned. No independent reviewer tool or local
coding-review CLI was available in this subagent session.
