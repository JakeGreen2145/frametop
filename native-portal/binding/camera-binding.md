# Camera publication and cleanup binding

Separate from the gate component. These pieces are CPU-tested and **not installed
in the live loader manifest**. No live admission, stock demand, renderer join,
Off or committed-frame policy is changed.

## Implemented contract

`binding/camera.hpp/.cpp` provides `CameraNativeBinding` around one
`CameraAccessGate`.

- `observe_publication(0x8dc1c, saved)` records x26 camera, x19 application and
  the x20 application bias after the actual publication store at 0x8dc18.
  It grants no epoch or pointer reads. Constructor0x15ddc8 and pre-store0x8dc18
  cannot publish. x0 is deliberately ignored as a camera identity.
- `bind(full, owner_thread, epoch_out)` supplies the independently retained
  renderer/device/association facts later. Native graphics initialization follows
  camera publication, so requiring those identities at constructor/publication
  time would be wrong. Pending facts are revoked before intervening camera
  cleanup. `publish_completed` combines these operations for callers that already
  hold all independent identities, including the CPU fixtures.
- `cleanup(0x15e1c8 or 0x15df70, saved, original, context, timeout)` matches exact
  x0, closes admission before any destructive instruction, and runs the full
  original method synchronously through the gate's callback interlock.
  Resource cleanup leaves the generation closed. Destructor return retires it.
  All locks are released before the native callback, waits, IPC or joins.
- The cleanup selector is set to Unsupported before validation. Refusal selects
  the emitter's fail-stop trap. Only actual synchronous callback return selects
  the method-return route. A null callback cannot silently continue native
  destruction. Preserve the existing context/worker exact-join fail-stop too.
- Unrelated camera cleanup invokes its original callback exactly once. A short
  in-flight count prevents first publication from overtaking unbound cleanup,
  without holding a mutex across it. This count is not a camera registry.

The owner and binding must outlive all concurrent callbacks. Keep existing
transaction tickets until every CPU, GPU and source obligation is settled.
Closing must not force a retained transaction to acquire a new ticket for its
second eye; its original ticket remains valid for lookup.

## Instruction pieces

`binding/camera-sites.json` pins the three 16-byte guards against ELF SHA-256
`9b1bd5fe4463592cbc71b2517b9ceb0a0fcd66cd6885a19dca4be0784bf6c265`.
The existing runtime emitter now supports these three PCs, but the live manifest,
loader preparation and runtime router do not install or dispatch them.

`ft_emit_camera_cleanup_bypass(out, at, base, pc)` emits the exact displaced
prologue plus a branch to the original method's PC+4. The integration's original
callback must call this bypass through its existing AAPCS64 call transport,
copy actual native outputs back, and return only when the complete method has
returned. Calling the patched original entry would recurse. Returning after an
observer callback while the original body has not run would retire too early.

The CPU instruction fixture executes the actual publication store, the new
islands, both bypasses and the compiled binding/gate. It compares original routes
and bypasses against separately executed pinned original instructions across all
GPRs, SIMD registers, SP, NZCV, FPCR/FPSR and prologue stack writes. It deliberately
clobbers callback registers. Refusal traps before the destructive continuation.
Native cleanup bodies are explicit synthetic callbacks, not GPU/IPC execution.

## Parent composition still required

The phase3 boundary inventory is an obligation list, not proof that current hooks
interlock anything. This change supplies only publication and exact camera
cleanup building blocks. Parent must compose the owner coordinator and cover
retention before context/direct-or-fallback application/camera lookup, application
precleanup0x8b020, rebind0x162360, renderer/controller/connection/lost-pipe writers,
direct shared-demand mutations, backend/association teardown and all selected
caller contracts. Unknown nested mutation/cleanup paths must remain fail-closed.

Do not enable these methods as a partial live gate. Do not weaken actual worker
join guards, invent native successful results, or treat quiescent tickets as GPU
or source reuse evidence. Shader/DMA contracts are separate Desktop work.

## Focused verification

All compilers and tests ran inside MemoryMax=256M, MemorySwapMax=0. No device,
SSH, native runtime or full router suite was used.

```sh
systemd-run --user --scope -p MemoryMax=256M -p MemorySwapMax=0 -- \
  python3 native-portal/binding/camera_test.py
systemd-run --user --scope -p MemoryMax=256M -p MemorySwapMax=0 -- \
  python3 native-portal/binding/camera_test.py --asan
systemd-run --user --scope -p MemoryMax=256M -p MemorySwapMax=0 -- \
  python3 -B native-portal/binding/camera_machine.py \
  --elf /path/to/pinned/vrcompositor --pylib /path/to/unicorn/site-packages
```

Red failures included missing components/sites, publication overtaking an
unbound destructor, and refusal leaving the stock continuation selected. Green
runs cover pending publication, delayed binding, intervening cleanup, exact-x0
classification, original foreign forwarding, drain/refusal and same-address ABA.
ASan/UBSan passed. The instruction fixture passed 11 cases plus invalid bypass
input checks. All eight preexisting runtime islands emitted byte-identical code
to base633e321 for the same coordinates.
