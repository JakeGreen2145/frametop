# Aggregate camera decision islands

## Installed contract

The production manifest and dual-module loader select `camera-decision`, emitter
ID 8, for two interior sites. They append camera coverage bits 18 and 19 without
changing the existing 18 bits. Complete coverage is `0xfffff`.

| Bit | PC | Original instruction | Original continuation | SkipOwned continuation |
| --- | --- | --- | --- | --- |
| 18 | `15fbb8` | `BL 15faf8`, word `97ffffd0` | Native call LR `base+15fbbc` | `base+15fbc8` |
| 19 | `15fb20` | `ADD x1,x0,#2,LSL#12`, word `91400801` | `base+15fb24` | `base+15fb74` |

Original and SkipOwned restore the Saved GPR, SIMD, NZCV, FPCR, FPSR and stack state.
SkipOwned changes neither saved x0 nor LR. Release executes its native success
branch, clears camera `+349` and returns through the original epilogue. Clear
executes its native unlock and void return. Neither island returns directly from
the enclosing function. Selectors other than Original 0 and SkipOwned 2 trap.

The manifest retains the exact 16-byte guards and their SHA-256 pins. The merged
inventory contains 146 compositor sites and 23 client sites. `15fbd8` remains a
separate source-result RET observer. The loader still exposes exactly 17 original
method bypasses, restricted to emitter 6. Neither interior decision can be looked
up or emitted as an original method.

This change installs ABI routes only. It does not change Runtime aggregate policy,
source readiness, producer ownership, native service outcomes or release receipts.
It requires the separate aggregate Runtime repair before aggregate admission is
usable.

## CPU verification

All compilation and replay used `MemoryMax=256M`, `MemorySwapMax=0`. No headset,
SSH, Desktop, camera service, runtime activation or private-asset check-in occurred.
The host lacked OpenSSL development headers. Tests used the already extracted
headers and linker files in `$HOME/.hermes/cache/scratch/source-hooks-deps`, with
its `usr/include` and `usr/include/x86_64-linux-gnu` on `CPATH` and
`usr/lib/x86_64-linux-gnu` on `LIBRARY_PATH`. No package installation was needed.

Run each command inside the same explicit memory-limited systemd scope:

```
systemd-run --user --scope -p MemoryMax=256M -p MemorySwapMax=0 -- \
  python3 -B native-portal/live/camera_decision_test.py
systemd-run --user --scope -p MemoryMax=256M -p MemorySwapMax=0 -- \
  python3 -B native-portal/live/camera_loader_test.py
systemd-run --user --scope -p MemoryMax=256M -p MemorySwapMax=0 -- \
  python3 -B native-portal/live/source_loader_test.py
```

Observed RED before production edits:

- The new contract and instruction tests rejected both absent decision sites in
  the actual selected loader inventory.
- The updated C acceptance test failed `mask==FT_TEST_CAMERA_MASK`, because the
  original loader reported only `0x3ffff`.

Observed GREEN:

- `camera_decision_test.py`: four tests, 30 instruction routes across three image
  bases, including 12 Original/SkipOwned state comparisons and 18 invalid-selector
  traps. Every callback goes through `ft_loader_emit_site` and its real ABI thunk.
  Original-route expected state comes from independently executing the pinned
  instruction, not reproducing its register effects in Python. SkipOwned compares
  unchanged state at its declared native continuation.
- Five additional native-method cases each compare emitted-island execution with
  an unpatched pinned instruction execution using an explicitly selected branch.
  The held Release actually returns 1, clears `+349`, preserves producer Set,
  handles, `+34a` and `+360`, and performs zero service or lock calls. Held direct
  Clear performs one paired lock/unlock, preserves Set and performs zero service
  calls. Final Release disconnects both queues and returns 1. Failed first
  Disconnect makes one service call and returns 0. Ordinary Clear clears Set.
  Registers, continuation snapshots, camera/shared memory and caller memory agree.
  Native lock and service calls are modeled boundaries; the callback changes only
  the selector, never a native result. The later Release result observers also
  execute through their loader-selected islands.
- Removing either decision from the manifest is rejected. Separately compiled
  loader inventories missing bit 18 or bit 19 report `0xbffff` and `0x7ffff`,
  respectively, so neither satisfies required coverage. Both keep all 17 method
  bypasses and reject interior bypass lookup/emission.
- Existing camera-loader tests pass 1,044 merged compositor instruction comparisons
  and 68 original-method/bypass routes. The dual-module source-loader test passes
  1,182 instruction comparisons, module-tag dispatch, full client RX identity,
  second-module precommit rollback and postpatch fail-stop. These comparison counts
  overlap; they are not additive coverage totals.
- The existing eight-site `runtime_machine.py --loader ...` runner passes all 24
  selector cases under the same memory cap. It reports 112 runtime manifest guards.

Generated outputs stay under ignored build directories, including
`native-portal/build/live-loader-test/camera-decisions.json`, `camera-routes.json`
and `native-portal/live/build/runtime/loader-runtime-islands.json`. These are
instruction and loader-installation fixtures, not a live installation or proof of
production Runtime policy. The parent integration owns the Runtime policy and
source-mode matrix runs after applying this patch.
