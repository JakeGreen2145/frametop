# Stock observer loader handoff

## Delivered scope

Initial component commit: `f5fbe0a14c1b1185b296687ddaf26c72ab9119c8`.
Complete implementation commit: `b3e93cc65dbefb29006d09e838873dbe7bd90d91`.
Cherry-pick the initial component and complete implementation commits in that order;
the following documentation-only commit records these immutable IDs.
Base: `33877a86468b2a4422c1014d81414e4136f46b3c`.

The canonical dual-module transaction now selects 191 compositor and 32 client
sites, 223 total. All 40 callbacks requested by command collector `9147ee5` and
all 75 callbacks requested by source collector `0bd4fb0` are present. The latter
includes old engine callbacks, method entries, held decision, field facts and
manager/guard boundaries. Optional `2a1f64` and `2584f8` are installed too.

Added 35 compositor and 9 client sites. All 122 old runtime-manifest records and
old top-level metadata remain byte-for-byte equivalent as parsed JSON. Existing
specialized emitters, invalidation/debt hooks, method bypasses, module pins,
RX authentication, rollback and the legacy camera mask are unchanged.

This component installs observation delivery, not Runtime consumer wiring or
native readiness. Current render-selection hooks from the subsequent readiness
contract are deliberately not added without that consumer's final request.
No headset, SSH, runtime activation, package installation or new opaque analysis.

## Stable inventory API for both collectors

Include `live/stock_observer_coverage.h`:

```c
size_t ft_native_portal_installed_sites(uint32_t version, uint64_t module_tag,
                                      uint64_t *pcs, size_t capacity);
```

Use version `FT_INSTALLED_SITES_VERSION`, currently 1. Module tag is zero for
compositor or `FT_STOCK_OBSERVER_CLIENT_TAG`, bit 63, for client. Returned entries
are exactly the module-tagged relative callback PCs emitted by the selected
merged loader table. Compositor entries are ordinary relative PCs.

The function returns the required count. Null output or insufficient capacity
writes nothing. Invalid version/tag, fork, disabled routing, absent islands or
unavailable retained dual-module lifetime returns zero. Both modules must have
committed before any inventory is available. The function does not return a
consumer's required-site list, a constant mask or a guessed inventory.

Example for the command collector:

```cpp
uint64_t installed[256];
const size_t n = ft_native_portal_installed_sites(
    FT_INSTALLED_SITES_VERSION, 0, installed, 256);
if (!n || n > 256 || !facts.install_coverage(installed, n)) {
    // Refuse optional dormant admission. Preserve native stock execution.
}
```

The source collector's `Covered(Module, relative_pc)` should consult this actual
inventory, tagging client PCs before comparison. Keep the installed-module and
same-process lifetime qualification. Never copy either collector's `required_sites`
as production installation evidence.

A separate diagnostic API remains:

```c
int ft_native_portal_stock_observer_coverage(uint32_t version,
    struct ft_stock_observer_coverage_v1 *out, size_t size);
```

Its 40-byte V1 record has `version`, `size`, `installed_mask`, `required_mask`,
`compositor_base`, `client_base`. The header names 44 added-site bits; full mask
is `0xfffffffffff`. Those bits describe the new observations only. Use the actual
inventory API for each collector's COMPLETE required set, including old sites.
This mask is separate from the unchanged 20-bit camera mask `0xfffff`.

Generation and runtime both check exact module/PC/word/kind/guard/emitter/index.
A missing site clears its bit even if a generated full-mask macro is stale.
Old generated headers without coverage metadata return unsupported. Unknown
record versions/sizes do not write the output; unavailable installed state returns
zero and clears the correctly sized record. These are installation facts, not
lock success, provider lifetime, graph authority, source readiness or GPU proof.

## Native contracts preserved

- `304ecc` has both positive and failed-creation predecessors. The new `304ec8`
  pre-store observer plus adjacent `304ecc` authenticates the positive path.
  Executed `304f9c/304fa0` writes zero and reaches the join with unproven x22;
  the negative witness emits no `304ec8` callback.
- `3053a8` and `305470` are pre-RET intent. The actual `305490` factory post-store
  and `2a23f8` committed renderer publication discharge the paired returns.
  `305470.x0` is the hidden output SLOT, not W. The complete pinned factory,
  wrapper constructor, hidden-output bridge and ready-pool pop execute in the
  test, including their actual nested returns and copied SP/LR relationships.
- `3052ec` retains native target `311648` and LR `3052f0`. Its actual caller
  continuation is observed. Null C means no call; only the paired nonnull path's
  literal low-byte result 1 is successful pool return. No GPU settlement follows.
- Metadata `258438` captures guard and outer entry SP/LR. `2584c4` captures literal
  uint32 x0 before overwrite, guard=x19 and inner LR=2584c4. `2584f8` restores
  outer SP/LR but does not unlock. `258508` revokes before unlock. Direct manager
  lock-result and pre-unlock observers are also installed.
- New source field callbacks need `camera_fact`; manager/guard callbacks need
  `boundary_event`. Do not forward them blindly into the old engine dispatcher.
  False/missing lock or exception observations deny extra reads/admission, never
  rewrite native stock execution. Update trace helpers are not a camera mutex.
- All added hooks use existing ordinary emitter 0. The source table's
  `16013c.kind_candidate=0` is not authoritative: its actual ADRP word requires
  kind 6 and the existing relocated ordinary-emitter tail. Full-state execution
  verifies that relocation. Null candidate kinds were likewise classified by the
  existing preparer and exercised, not assigned invented emitter behavior.

The complete 800-byte Saved record, including unspecified ordinary-observer pad,
passes unchanged through the compiled dispatcher to the host observational sink.
Client callbacks retain bit 63. No emitter source or Runtime dispatch was edited.

## Pin provenance and resolved issue

Tests read the parent-verified primary/guard package, additional native windows,
and frozen source request. Whole ELF hashes and RX file spans are checked using
the existing immutable local `vrcompositor` and `vrclient.so`.

The delivered factory window ended at `305498` exclusive, so it supplied only
8 of the 16 guard bytes for `305490`. After requesting the missing guard, the
exact requested RX16 was checked directly in the existing whole-hash-authenticated
ELF, without decompilation, xrefs or inferred bytes. Its first 8 bytes match the
supplied window. The complete guard is `f0ffff179b19f597f40300aa05000014`, SHA256
`9ab5bcb9c6f6209dc8d234c3fbf88a5c0a65fa83b3f4b4bf9b430bfd78b4ee57`.
`HOOK-PINS-REQUEST.json` records that resolution. No required V1 pin is missing.

## Verification

Test suites ran serially in 256 MiB / zero-swap user scopes. Existing private
OpenSSL headers were reused read-only after the first compile found no system
`openssl/evp.h`. No package was installed.

```sh
export CPATH=/home/jakeg/.hermes/work/frametop-native-live-loader/native-portal/build/live-loader-test/deps/usr/include:/home/jakeg/.hermes/work/frametop-native-live-loader/native-portal/build/live-loader-test/deps/usr/include/x86_64-linux-gnu
export LIBRARY_PATH=/home/jakeg/.hermes/work/frametop-native-live-loader/native-portal/build/live-loader-test/deps/usr/lib/x86_64-linux-gnu
systemd-run --user --scope -p MemoryMax=256M -p MemorySwapMax=0 \
  python3 -B native-portal/live/stock_observer_test.py --sanitize
systemd-run --user --scope -p MemoryMax=256M -p MemorySwapMax=0 \
  python3 -B native-portal/live/loader_test.py
systemd-run --user --scope -p MemoryMax=256M -p MemorySwapMax=0 \
  python3 -B native-portal/live/source_loader_test.py
systemd-run --user --scope -p MemoryMax=256M -p MemorySwapMax=0 \
  python3 -B native-portal/live/camera_loader_test.py
```

All exit 0:

- Six prepare/inventory/coverage tests and six instruction/API tests.
- 264 new-site comparisons across three load biases and conditional outcomes.
  All GPR/SIMD registers, SP, NZCV/FPCR/FPSR, native memory effects and callback PC
  match the original. Host dispatcher preserves every Saved byte and errno.
- Actual metadata accessor/release paths with false/true lock outcomes and both
  TLS paths; committed stores; null/false/true wrapper pool returns; failed pool
  creation bypass; actual nested wrapper/factory returns and publication.
- 44 separately compiled missing-site inventories, with stale full-mask macros,
  each refuse full added-site coverage. Exact postpatch PC inventory readback,
  bounded-copy redzones, version/tag/refusal checks and old-header compatibility.
- ASan/UBSan loader and all emitter builds pass file/map authentication,
  installation, rollback, inventory and coverage checks.
- Existing loader suite: 1314 emitter comparisons and 24 specialized routes.
  Existing dual-module suite: 1506 comparisons. Camera suite: 68 routes.
- Exact RX/file identity, every precommit page rollback and fatal postpatch/rollback
  checks retained. Default-off/wrong-executable real inert preload checks pass.
- `ruff check --select F` and `git diff --check` pass.

RED was observed for initial missing observers/coverage, each extension, missing
factory continuation, and missing installed-inventory export before repair.
Logs are under ignored `native-portal/build/live-loader-test/final-*.log`;
initial RED is `/home/jakeg/.hermes/cache/terminal-output/out-1791737384-839985-ad70.log`.

CPU models are explicit. Object memory, allocation, naming/resource service
boundaries, metadata lock results and opaque pool-return result are synthetic.
The ARM client DSO is not dlopened on x86; only the retained handle token is
modeled after real file/RX verification and installer execution. The host sink
is not Runtime. No host test establishes installed Frame or GPU coverage.

## Remaining parent work

Wire the real Runtime to both inventory consumers and dedicated fact routes,
retain exact per-invocation provenance and provider qualification, then perform
the inactive ARM build. Current identity/bootstrap/render selection is the
separate upcoming consumer contract; this V1 component makes no claim for its
unused groups. Independent review and any activation remain parent-owned.
No independent reviewer tool or coding-review CLI was available in this lane.
