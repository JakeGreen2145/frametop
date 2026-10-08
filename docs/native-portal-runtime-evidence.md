# Pinned native runtime evidence

`native-portal/runtime/evidence.hpp` and `evidence.cpp` add an inactive native metadata extractor with a real Linux self-process reader. They do not complete the live adapter binding. In particular, `Evidence::observe`, `PassReader::live`, and the Bridge write callback still return false. No capability, installer, owner, hook, observer, render, arena, or binding implementation changes are included.

## What is implemented

- A fixed-capacity native-object read registry. Its epochs originate at the recognized renderer constructor return, not a pointer hash or frame number. Each read checks process, thread, epoch, object bounds and overflow before asking the kernel to copy bytes.
- Actual `process_vm_readv` against the current process. No `/proc/maps`, `/proc/mem`, hot file I/O, signal handler, logging, heap storage or arbitrary address registration occurs in the reader. An inaccessible page, short read, unavailable syscall or denied syscall returns false. The syscall contains memory faults; it does not establish lifetime, atomicity or native synchronization.
- Paired arena allocation entry and immediate return. The reader consumes the actual native output slot while that frame is suspended, then forgets the slot. It does not reread the cleared stack storage at initialization.
- Typed renderer/device/arena reads, a selected-memory-type coherence check, actual mapping association, wrapper resource/size/offset and the complete source-over marker attachment. `snapshot` fills `render::Observation` with diagnostic facts but leaves recording generation zero.
- Adapter-owned wrapper and marker registration only through paired native call entry/return, with the exact call targets and allocation arguments. Nested calls poison evidence rather than associating the wrong result.
- Generation attachment through the real optional C export `ft_pipeline_generation`. If the export is unavailable or returns zero, device generation remains zero. The CPU suite also builds without the export; no renderer counter substitutes for a device generation.
- Revocation before the recognized renderer teardown, arena unmap and owned wrapper/marker destruction. Cursor reset does not manufacture recording provenance or renew a device lifetime.

The evidence registry contains only fixed native-owned objects acquired through those event scopes. It never registers mapped arena pixels, arbitrary pass vectors, caches or packets just because their addresses are readable. The provider owns no native resource and does not allocate or destroy one. A false observation is not permission to suppress, replay or replace any native instruction.

## Event contract

The future trusted trampoline must supply actual saved registers from the hash-verified image and serialize all events and reads. This file contains no such trampoline or installation proof. `pin` checks the expected digest supplied by an external preparer and validates load-bias arithmetic; it does not hash a live executable mapping. Calling `pin` is not authorization to install or use hooks.

| Relative PC | Evidence consumed |
|---|---|
| `0x7fda0` | Return from the constructor of the native `0x16c0` renderer. R is x24. Read and check vtable `base+0x599c38`; issue the renderer epoch. |
| `0x29a14c` | Arena allocation call, x19=R, x0=R+0xec0, x8=sp+0x140. Capture the actual requested u32 size, stock `0x80000` or coordinated enlarged `0x80080`. |
| `0x29a150` | Immediate return in the same SP/R scope. Read the actual output resource at the saved slot and register its native `0x128` storage. |
| `0x29a528` | Before the active command-wrapper store. Capture x1, the native 16-byte wrapper, while x19=R. This is object publication, not command recording provenance. |
| `0x7fdbc` | Require x24=R and nonzero low byte of x0, actual device/context fields, matching allocation and mapping, and initial zero cursor. Attach the layer device generation if available. |
| `0x2987d4` | Recognize the actual arena reset with x22=R. Do not issue a recording generation. |
| `0x29bd18`, `0x29c238` | Revoke before renderer teardown with x0=R. |
| `0x2fe090` | Revoke at unmap entry for the exact arena resource. No remap recovery is claimed. |
| `0x2966a0`, `0x294678` | Read the native pointer-to-slot argument at destruction entry and revoke if it names the owned wrapper or marker. |

`call_enter` and `call_return` surround an actual direct native call made by the future owner. They do not invoke the target. Supported allocation is `base+0x296c00`, R/592/0/0/0/0/0. Supported marker creation is `base+0x294358` with six words `[1,1,3,1,3,0]`. Returned objects must have the expected resource context, size, initial sentinel and all eight attachment words. These APIs are for a trusted in-process caller, not IPC input.

`initialized()` means successful initialization facts were collected. It does not mean Adapter initialization is supported. `identity().process_instance` is local PID evidence plus an independent fork check. It is not a durable cross-process identity token. Renderer epochs are never reused by this provider's process-local issuer; failed startup cannot be retried over the same evidence object.

## Actual allocation and coherence

The reader checks the following relationships on initialization and subsequent snapshots:

- Native VkDevice is `[R+0xa78]`; physical device is `[R+0xb0]`. The embedded context D is `R+0x50`.
- `[R+0xec0]` and `[resource+8]` must both equal D.
- Actual resource memory type is the **u32** at `resource+0x58`. It must be below the **u32** type count at `R+0xf00`, and the count must be at most Vulkan's 32 memory types.
- Read that exact type's property flags at `R+0xf04+8*index`. Require HOST_VISIBLE, HOST_COHERENT and HOST_CACHED, mask `0x0e`, matching this native route. A map alone never proves coherence.
- Require nonzero backing VkDeviceMemory at `resource+0x50`, allocation bytes at `+0x60` covering the declared **u32** buffer size at `+0xb8`, and actual VkBuffer at `+0xb0`.
- `[R+0x1390]` and mapped-resource owner `[R+0x1408]` must equal the exact allocation result. Renderer mapped base `[R+0x1410]` must equal native resource mapped base `[resource+0xa0]`, with a positive native map reference count at `+0x98` and checked full capacity arithmetic.
- Retain backing memory, selected index, type table count/flags, map, buffer and allocation size as immutable initialization facts. A later nonnull replacement does not pass.

Alignment is read as u64 at R+0x328. Capacity and cursor are u32. The adjacent word at resource+0xbc is not part of the capacity. Pass bounds are structurally checked with stride `0x98` and the existing pass limit, but no read permission or lifetime certificate follows from a valid span.

The instruction suite executes the **real** native resource allocator `0x2ff910`, selector/fallback `0x2fed20` and `0x2fdb10`, allocation loop `0x2fee38`, memory allocator `0x2fdc70`, and mapper `0x2fdfb0`. External Vulkan calls are explicit synthetic traps. The preferred route selects type 0 with flags 15. The fallback route has an unsuitable type 0 and selects type 1 with flags 14. Both retain the same resource through the allocation output, backing-memory store, buffer bind and mapping output. This is not merely a separate handwritten selector test.

The suite also executes the initializer's descriptor and argument assembly at `0x29a0f4..0x29a14c`, its output-slot consume/clear/publication at `0x29a150..0x29a160`, and x24/return-byte instructions at the outer initialization boundary. Semantic instruction guards cover the exact field layouts. No proprietary ELF bytes or disassembly are committed.

## Deliberately unresolved integration

The new code is useful concrete extraction, not a replacement claim that the product can now render. These missing facts remain explicit:

1. **Initial command recording attachment.** `[R+0x1380]+8` supplies the native command object, not its executable generation. Pair actual native begin entry `0x300a08` with the raw Vulkan result at `0x300a38`, its exact SP/caller scope and command VkCommandBuffer at `C+0x738`. Bind the successful begin to the command published at `0x29a528`. Subsequent generation issuance must come from the existing successful reset/begin proof at `0x300b20/0x300c54` and `0x300a08/0x300a38`. The current evidence does not establish the first such ownership chain, so `recording_generation=0` and `observe=false` are intentional.
2. **Pass graph retention.** `PassReader::read` uses the real bounded reader for registered objects, but `live=false`. It cannot safely grant arbitrary current/retained pass storage, packet buffers, warm-cache entries or attachment objects from vector endpoints alone. Native append/move/destruction and cache teardown evidence must prove their lifetimes through both committed eyes. Process mappings do not answer this question.
3. **Bridge allocation-return composition and staging.** The provider captures the actual slot while it is live. A future combined callback must also let Bridge consume that slot in the same paused return scope. This provider deliberately does not retain a stack read capability after `event` returns. Adapter's private staging and K/stock-wrapper lifetimes are not registered here. The Bridge write callback always refuses rather than granting writes to any apparently writable address.
4. **Complete invalidation coverage and synchronization.** The implemented destroy/unmap events are narrow contracts, not proof every alias/release path is installed. The owner must guarantee no concurrent teardown or writes during each snapshot. Startup and render-thread handoff is unproven. Foreign threads and forked children refuse; no handoff is inferred from a pointer or TID supplied by the caller.
5. **Live identity and transport.** Runtime image verification, lifecycle installation/quiescence, device destruction/recreation observation and native hook routing are not implemented. CPU calls into the actual layer ABI use an explicitly synthetic generation export in one test and no export in the other. There is no real Vulkan device call.

Do not wire `snapshot` directly into `Callbacks::observe`, invent a recording generation, turn `PassReader::live` into a Boolean lifetime bypass, or replace the refused write with unbounded memcpy. The constructor/allocation fixtures are synthetic native event schedules, not captured startup evidence. Default capability stays unavailable.

## Reproduce

From this checkout, use absolute paths because a transient systemd service does not inherit the shell working directory:

```sh
systemd-run --user --wait --pipe -p MemoryMax=650M -p MemorySwapMax=0 \
  python3 -B "$PWD/native-portal/runtime/evidence_test.py" --machine
systemd-run --user --wait --pipe -p MemoryMax=650M -p MemorySwapMax=0 \
  python3 -B "$PWD/native-portal/runtime/evidence_test.py" --sanitize
```

Tests use real allocated process storage, a PROT_NONE page boundary, an unmapped page, overflow addresses, fork and another thread. They mutate selected flags/index/count, backing memory, resource/context identities, mapping/refcount, wrapper/marker fields and pass bounds. They cover the output slot being cleared before initialization, adjacent u32 capacity bytes, teardown, unmap, missing allocation proof, missing layer export, bad pin and nested-call confusion. Read failures never authorize the Adapter or PassReader.

The runner validates `memory.max=681574400` and `memory.swap.max=0`. Build output and exact command results go to ignored `native-portal/integration/build/runtime-evidence/`. The machine runner reads only the local pinned ELF and existing offline analysis dependencies. It opens no VR runtime, device, network or SSH connection. The normal and ASan/UBSan binaries are separate from the instruction emulator.

The completed normal and ASan/UBSan runs passed with and without the layer generation export. The instruction run passed 25 semantic layout guards, both complete preferred/fallback allocation-to-map paths and 22 selector cases. The unchanged production-chain `integration/combined_test.py --sanitize` also passed its 2000-frame regression. The evidence run measured memory peaks of 212250624 bytes for normal plus instruction tests and 102170624 bytes for ASan/UBSan, below the enforced limits. An independent reviewer was not available in this subagent environment; parent review is still required.

On any SteamVR update, reject the old SHA and re-establish all event scopes and layouts before altering the pin. No product update-check change is needed until an installer actually makes this dependency reachable; that installer must add the retest/check integration required by AGENTS.md.
