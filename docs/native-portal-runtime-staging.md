# Adapter-owned native staging

`runtime/staging.cpp` connects the real Evidence self-process reader to the existing Adapter and arena Bridge. It does not install a trampoline, enable a capability, attach to SteamVR or authorize rendering. The existing whole-dispatch graph retention block remains.

## Exact write ownership

The 592-byte allocation is `Adapter::Impl::Upload::staging`. No caller registers its address. A private Adapter method, callable only by Evidence, compares the requested destination with that exact live allocation and copies exactly its size. It checks the current ticket, frame, recording generation, owner PID/thread, upload phase and live authorization. Wrong destination, wrong size and a second attempt refuse before copying.

Evidence retains a writer pointer only during the synchronous `Adapter::upstream(Upload)` call. It consumes that pointer before invoking the private write method. Adapter separately opens its write window only around `Bridge::dispatch(Upload)`, and consumes it on the first attempt. Neither API returns a writable pointer. The original native call receives the resulting staging pointer through its existing saved x2 register, as before.

Bridge still copies the final 528 native bytes and appends the Adapter's immutable 64-byte suffix. It does not write native camera constants, the stock wrapper or arbitrary host storage. Adapter retains its existing check for Off/expiry **after** the write callback and **before** permitting the original BLR. An aborted copy or refused continuation keeps native pending debt. Bridge reservation accounting changes only at its original admission/reset boundaries.

Completion of the synchronous write closes write access even while the original native upload is still pending. Finish, rejection and abort release the offered staging identity. The owner-thread Adapter destructor releases any remaining offer before freeing its private allocation, without claiming native settlement or clearing charged debt. Evidence never dereferences the pointer identity it retains between events.

## Native caller contract

The preparer must verify the full ELF SHA:

```
9b1bd5fe4463592cbc71b2517b9ceb0a0fcd66cd6885a19dca4be0784bf6c265
```

The only supported owner is the serialized, paused native camera caller. The trusted trampoline supplies saved registers from the pinned instructions. These methods are not an adversarial IPC interface or arbitrary memory-registration API.

The pinned function at `0x160be8` reserves `0x460` stack bytes and assigns its camera argument x0 to x27 at `0x160c10`. At `0x160f30`, x22 becomes SP+`0x300`. The upload argument assembly at `0x161d40..0x161d64` derives x0 from K+8, x1 from K+`0x2d0`, x2 from K+`0xc0`, and x7 from renderer vtable slot `0x78`. That slot resolves to `0x2993f8`.

`Evidence::stage` supports these scopes:

| PC | Contract |
|---|---|
| `0x161d64` | Entry at the actual native BLR. Require R/K/stock/source/target and bounded caller SP. Read the stock resource through the native wrapper, check its R+`0x50` context and 528-byte size. Copy native constants into the reservation input. Reserve through Adapter, then allow the Bridge read/write callbacks only during this call. Return the nonreused Adapter ticket. Successful replacement permits the original upload BLR exactly once. |
| `0x161d68` | Immediate return. Require the exact ticket, Adapter, K and SP, unchanged eye/origin/standing input and unchanged K/stock/resource associations. Invoke Adapter's existing immediate Bridge descriptor authentication and both native descriptor resolutions. |
| `0x161ed8` | Final native enqueue. Require the same invocation, x0=R, x1=x22=SP+`0x300`, x2=`base+0x450650` and x4=`base+0x29b278`. Read the actual final 352-byte prefix, then revoke camera scope before Adapter enqueue. Adapter changes only its three owned prefix fields. Success returns `SkipOwned`, LR=`base+0x161edc`, because Adapter has already issued the replacement enqueue. Do not also execute the stock BLR. |
| `0x161f00` | Native epilogue reached with an unfinished invocation. Revoke its scope and offer, reject the frame, preserve charged debt and let the original epilogue execute. No completion is inferred. |

Only the specific K fields, 528 constant bytes, stock-wrapper resource field and stock-resource size are readable by Bridge during entry. These are call-local borrows, not new general-purpose entries in the native allocation registry. Every later native field read occurs only after the paired ticket/K/SP checks. Destroying the active stock wrapper at the existing `0x2966a0` route also revokes Evidence before native destruction.

A new invocation while one is pending, stale tickets at reused K/SP, mismatched scope, failed field reads or duplicate return poison this staging owner. Foreign-thread attempts set an atomic poison flag without racing the owner's non-atomic fields. Forked children refuse independently. Off/expiry remains the Adapter's authorization decision; neither native camera readiness nor a caller-provided eye/origin creates demand authorization.

## What the tests prove

`runtime/staging_test.inc` uses real allocated self-process storage and the actual Evidence, Bridge, Adapter, FrameGate and reservation policy. Native allocation, upload memory effects, descriptor resolution and enqueue are explicitly modeled CPU boundaries. The test calls do not execute ARM native functions in the host process.

Positive cases exercise both eye orders, exact 592-byte payloads, all preserved prefix bytes, both immediate descriptors for each eye, and closed write access after every scope. Fault cases cover destination/range/overflow/null source, replayed writes, wrong ticket, stale ticket with reused K/SP, changed K/stock/resource, wrong arguments and caller SP, duplicate return, new invocation before finish, foreign thread, fork, destruction and abort. Off/expiry inside the write callback cannot permit the native upload. Failures after admission retain the charged 640-byte reservation and pending debt.

`runtime/staging_machine.py` separately verifies the full pinned ELF, checks 19 semantic instruction guards, and executes the actual native prologue, K/SP derivation, upload/enqueue argument assembly, original BLRs and epilogue for both eye arguments. Its memory is synthetic. It does not map an ASLR host staging address into the guest. The existing upstream instruction suite remains responsible for the emitted arena islands, real upload/resolver instructions and host-copy transport boundary.

Reproduce locally, with absolute paths from this checkout:

```sh
systemd-run --user --wait --pipe -p MemoryMax=650M -p MemorySwapMax=0 \
  python3 -B "$PWD/native-portal/runtime/evidence_test.py" --machine
systemd-run --user --wait --pipe -p MemoryMax=650M -p MemorySwapMax=0 \
  python3 -B "$PWD/native-portal/runtime/evidence_test.py" --sanitize
systemd-run --user --wait --pipe -p MemoryMax=650M -p MemorySwapMax=0 \
  python3 -B "$PWD/native-portal/integration/upstream_test.py" --sanitize --machine
systemd-run --user --wait --pipe -p MemoryMax=650M -p MemorySwapMax=0 \
  python3 -B "$PWD/native-portal/integration/combined_test.py" --sanitize
```

All four commands passed in the final serial verification. Each enforced `memory.max=681574400` and `memory.swap.max=0`. Evidence normal/machine peaked at 242360320 bytes, Evidence ASan/UBSan at 245403648 bytes, upstream ASan/UBSan plus machine at 190787584 bytes, and combined ASan/UBSan at 211185664 bytes. The combined regression retained its 2000-frame/4000-upload result. Full commands and outputs are in ignored `native-portal/integration/build/runtime-evidence/staging-final-verification.json` and `staging-final-*.log`.

## Still unsupported

No installed event router proves every camera caller exit, K release/reconstruction, renderer teardown or thread handoff. The scoped implementation requires serialized owner delivery without concurrent teardown. The next integration probe must pair the pinned `0x160be8` camera entry with upload, immediate return, enqueue and all reached caller epilogues on the same native owner, routing abort before any reused caller scope. It must establish K/stock lifetime against release/reconstruction and preserve the original native continuations. Do not enable runtime installation on the strength of the synthetic schedules here.

Pass graph/cache/pipeline retention, native rendering, optical alignment, real queue submission and successful recycling through Evidence are separate obligations. The existing PassReader remains false. No policy or capability is relaxed to hide those gaps.
