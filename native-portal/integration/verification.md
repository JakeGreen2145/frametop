# Host-staging transport regression

## Failure and repair

Baseline `3f1fdaaf588171d3e139df351dce56d40cfa2790` mirrored a real C++ staging pointer into Unicorn at the same numeric address. Its `host_pages` set tracked only mappings created by that mirror, not the native fixture's existing mappings. The inherited memcpy boundary does not create mappings.

A fresh-process probe reproduced the original three failing machine cases on run 87. The attempted host pages were `0x10170000`, `0x102ac000`, and `0x10299000`, inside the existing guest region `0x10000000..0x103fffff`. All failed with `UC_ERR_MAP`. This is host ASLR address-space overlap, not a production arena failure or a split-page bookkeeping failure. Ignoring the mapping exception would overwrite native guest objects.

The test fixture now records the exact Bridge host write and matches it to the replacement's unchanged source pointer and 592-byte length. At the already-synthetic libc memcpy boundary it reads that host allocation directly and writes only the native destination in Unicorn. It does not map or overwrite the source's numeric guest address. Provenance is consumed once and cleared on another upload or native cursor reset. Unregistered stock copies still use the inherited guest-to-guest memcpy boundary. A mismatched native source or size fails an assertion rather than guessing ownership.

No native C++ code, emitted island, pinned instruction fixture, or original machine test assertion changed. The seven-method `Tests` class is AST-identical to baseline. Native admission, copy arguments, cursor accounting, both immediate resolver calls, descriptor authentication, late-upload refusal, unsupported traps and lifetime/debt assertions still execute.

## Deterministic regression

`test_upstream_transport.py` uses the real compiled Adapter/Bridge dispatch, then installs sentinel-filled guest objects at the actual returned host staging addresses before transport. The ordinary guest fixture is relocated away from host heap addresses to make the intentional alias independent of ASLR. Host pointers and payloads are not relocated or substituted.

Both recording orders verify the exact native memcpy destination, original host source address, all 592 uploaded bytes, all sentinel guest pages, both immediate descriptors and completion. A later guest memcpy at the same numeric source must read guest bytes, not a stale host registration. Separate tests alter the actual memcpy source/size and reset an interrupted native upload before reusing its address as guest memory.

The full `upstream_test.py --machine` runner includes these three transport regressions after the original seven machine cases.

## Executed verification

All runs were local and offline under `MemoryMax=650M`, `MemorySwapMax=0`. The verification scope reported `memory.max=681574400`, `memory.swap.max=0`, and peak `248745984` bytes.

| Check | Result |
|---|---|
| Deterministic alias test against exact baseline Python blob, unchanged current compiled native code | Both eye-order subcases fail `UC_ERR_MAP`, three fresh processes |
| Original seven machine tests after repair | 100 fresh processes pass |
| Three focused transport regressions | 10 fresh processes pass |
| Full upstream `--sanitize --trace ... --machine` | Pass, including 882 captured-record replays, 10,000 upstream stereo cycles, expected legacy RED, arena 14 machine and 2 preservation tests, original 7 machine and new 3 transport tests |
| Combined `--sanitize` | Pass, including 2,000 complete frames, 4,000 upstream uploads, resident <=2, one wrapper and zero hot allocations |
| Diff whitespace and original test-class AST comparison | Pass |

The C++ sanitizer executables use ASan/UBSan. The ctypes-loaded instruction-test DSOs remain unsanitized, as in the existing runner. Captured trace inputs and pinned instruction execution do not prove live GPU rendering or headset behavior. No SSH, runtime hook, camera, service or live compositor operation was used.

## Reproduction and retained evidence

From this checkout, with the existing private pinned fixtures available:

```sh
systemd-run --user --scope -p MemoryMax=650M -p MemorySwapMax=0 \
  python3 -B native-portal/integration/upstream_test.py --sanitize \
  --trace /home/jakeg/.hermes/work/frame-native-live-evidence/observer-live-01/metadata.bin --machine
systemd-run --user --scope -p MemoryMax=650M -p MemorySwapMax=0 \
  python3 -B native-portal/integration/combined_test.py --sanitize
```

For focused tests, set `PYTHONPATH` to the existing `frame-native-reuse/camera-analysis/pylib` and `frame-native-product-design/pylib` directories under `/home/jakeg/.hermes/work`, then run `native-portal/integration/test_upstream_transport.py` within the same memory-limited scope.

Ignored local evidence lives in `integration/build/`:

- `mapping-probe.log` records the natural ASLR failure.
- `verify_host_transport.py` contains the exact bounded verification commands. Its RED path loads the baseline Python source from the immutable git blob into the test module without changing tracked files.
- `host-alias-baseline-red-{1,2,3}.log` records deterministic baseline failures.
- `host-machine-green-*.log` and `host-transport-green-*.log` record fresh-process repetitions.
- `host-upstream-full-sanitize.log` and `host-combined-sanitize.log` record the full runs.
- `host-transport-verification.json` records all 115 subprocess commands, exits and memory limits.
