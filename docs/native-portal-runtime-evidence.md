# Pinned native runtime evidence

`native-portal/runtime/evidence.hpp`, `evidence.cpp` and `recording.cpp` provide an inactive native metadata extractor with a real Linux self-process reader. The first command generation now attaches to a successfully begun native command and its later renderer publication. `Evidence::observe` can support Adapter initialization and latch for that attachment. The bootstrap router composes Evidence and Bridge in the same paused allocation-return scope.

This is **not a completed rendering binding**. `PassReader::live` remains false. Whole-dispatch graph/cache/pipeline retention is not established. The Bridge write callback now supports only the current Adapter-owned staging allocation in a paired native camera scope, detailed in [runtime staging](native-portal-runtime-staging.md). The exercised PassGate path refuses rather than committing either eye. No capability, installer, hook, arena policy, pass, or command-lifecycle changes are included.

## What is implemented

- A fixed-capacity native-object read registry. Its epochs originate at the recognized renderer constructor return, not a pointer hash or frame number. Each read checks process, thread, epoch, object bounds and overflow before asking the kernel to copy bytes.
- Actual `process_vm_readv` against the current process. No `/proc/maps`, `/proc/mem`, hot file I/O, signal handler, logging, heap storage or arbitrary address registration occurs in the reader. An inaccessible page, short read, unavailable syscall or denied syscall returns false. The syscall contains memory faults; it does not establish lifetime, atomicity or native synchronization.
- Paired arena allocation entry and immediate return. The reader consumes the actual native output slot while that frame is suspended, then forgets the slot. It does not reread the cleared stack storage at initialization.
- Typed renderer/device/arena reads, a selected-memory-type coherence check, actual mapping association, wrapper resource/size/offset and the complete source-over marker attachment. `snapshot` fills `render::Observation` with diagnostic facts. Recording generation remains zero unless the complete begin/publication chain was observed.
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
| `0x301808` | Before the native command initializer call. x0=x20=C is the freshly constructed `0x7b0` object. Check C+8=D and save the caller SP. |
| `0x300a08` | Initial begin entry must have x0=x19=C, SP=initializer-call SP minus `0x40`, LR=`base+0x300ae0`. Read the actual handle at C+`0x738`. |
| `0x300a38` | Raw begin result. Require SP=begin SP minus `0x30`, x19=C, unchanged command handle, and saved caller LR at SP+8. Only low w0 zero succeeds. |
| `0x30180c` | Native initializer return must restore its saved SP and x20=C and return a nonzero low byte. Keep this begun C in bounded pending storage, not as read authority or an active generation. |
| `0x29a528` | Before the active command-wrapper store, x19=R and x1 is the native 16-byte wrapper. Its +8 must match a proved pending C and unchanged VkCommandBuffer. Register C and issue the generation from the persistent `binding::Generations`. |
| `0x7fdbc` | Require x24=R and nonzero low byte of x0, actual device/context fields, matching allocation and mapping, and initial zero cursor. Attach the layer device generation if available. |
| `0x2987d4` | Recognize the actual arena reset with x22=R. Do not issue a recording generation. |
| `0x29bd18`, `0x29c238` | Revoke before renderer teardown with x0=R. |
| `0x2fe090` | Revoke at unmap entry for the exact arena resource. No remap recovery is claimed. |
| `0x2966a0`, `0x294678` | Read the native pointer-to-slot argument at destruction entry and revoke if it names the owned wrapper or marker. |
| `0x311648`, `0x4b3d0`, `0x3003d0` | Revoke a pending C before native release, sized delete or handle clear. Release takes C in x1; the other sites take x0. If C is the published owner, revoke the provider before reading freed or reused storage. |
| `0x305260`, `0x2953a0` | Revoke the published command wrapper before its native reference release or destruction. |
| `0x300b20`, unmatched `0x300a08` | Clear recording authorization before reset or another begin, even if the eventual Vulkan call fails. No cursor/frame/address inference can restore it. |

`call_enter` and `call_return` surround an actual direct native call made by the future owner. They do not invoke the target. Supported allocation is `base+0x296c00`, R/592/0/0/0/0/0. Supported marker creation is `base+0x294358` with six words `[1,1,3,1,3,0]`. Returned objects must have the expected resource context, size, initial sentinel and all eight attachment words. These APIs are for a trusted in-process caller, not IPC input.

`initialized()` means successful initialization facts were collected. It does not by itself authorize Adapter initialization; the recording and device-generation checks must also pass. `identity().process_instance` is local PID evidence plus an independent fork check. It is not a durable cross-process identity token. Renderer epochs are never reused by this provider's process-local issuer; failed startup cannot be retried over the same evidence object.

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

1. **Initial attachment implemented; successful recycling still blocked.** The above chain now supplies the first recording generation. Pending commands can predate wrapper acquisition: `0x3053f8 -> 0x305308 -> 0x302790 -> 0x304e60` can pop an already begun pool member. Do not require begin to occur inside that later acquisition call. Keep at most 64 pending begin facts, and issue an identity for the published member rather than the most recently created member. `bind_lifecycle` constructs the actual Lifecycle with the same persistent issuer and Evidence reader. It checks the exact observation/Owner identity and forwards reset/begin/queue/disposal captures. No caller supplies a generation issuer, reader or ready flag. Missing graph retention still prevents current/retained scans and successful recycling in this binding. The CPU suite verifies binding and refusal of a premature reset, not a successful successor through this reader.
2. **Pass graph retention remains blocked.** `PassReader::read` uses the bounded reader for registered objects, but `live=false`. Decoding the dispatcher shows the current native iteration at `0x2a1f2c..0x2a1f44`, the synchronous encoder call at `0x2a24dc`, and later current-to-retained transfer at `0x2a2230`. Nonempty retained transfer calls the native mover at `0x2a22b4`; packet buffers can then be freed at `0x2a230c` before the current end store at `0x2a231c`. Those scopes do not prove attachment, warm-cache handle and pipeline retention across both committed eyes. The reviewed reader has no allocation/publication ledger for pass vectors, packet buffers, cache entries or attachment objects and no exhaustive cache/device release routing. A per-pass encoder call cannot authorize a whole-dispatch borrow. Granting reads from vector endpoints, even for real mapped process objects, would hide this gap. No such grant was added. Lifecycle uses a role-restricted reader for the four renderer vector fields and C+`0x738` only. A RED test put apparently valid pass/packet/ticket objects inside the already registered renderer allocation. The generic reader incorrectly accepted that alias as a disposal scan. The restricted reader now rejects it; known allocation byte bounds are not graph ownership.
3. **Bridge allocation-return and staging composition implemented.** `route` binds one exact Bridge to the constructed renderer, lets its existing allocator path request `0x80080`, registers the allocation result, then grants the saved eight-byte output slot only during that same `AllocationReturn` dispatch. It revokes the slot before returning. Initialization uses the captured resource after native slot clearing. `stage` now pairs the native camera upload, immediate return and final enqueue by exact K/SP and a nonreused Adapter ticket. Its call-local writer can copy exactly 592 bytes only to the current Adapter-owned allocation. Camera/constants and stock-wrapper/resource field reads are scoped to the native caller, not registered as general allocations. Real-process synthetic tests exercise both upstream replacements and immediate descriptor capture through Evidence, Bridge and Adapter. There is no arbitrary writable-address API, public staging pointer accessor, unbounded memcpy or guest remapping. The [staging contract and tests](native-portal-runtime-staging.md) distinguish these modeled calls from pinned instruction execution and the still-uninstalled live owner.
4. **Complete invalidation coverage and synchronization.** The implemented destroy/unmap events are narrow contracts, not proof every alias/release path is installed. The owner must guarantee no concurrent teardown or writes during each snapshot. Startup and render-thread handoff is unproven. Foreign threads and forked children refuse; no handoff is inferred from a pointer or TID supplied by the caller.
5. **Live identity and transport.** Runtime image verification, lifecycle installation/quiescence, device destruction/recreation observation and native hook routing are not implemented. CPU calls into the actual layer ABI use an explicitly synthetic generation export in one test and no export in the other. There is no real Vulkan device call.

Use the guarded `Evidence::observe`, not `snapshot`, for the supported first-generation observation. Do not invent a generation, turn `PassReader::live` into a Boolean lifetime bypass, or broaden the exact staging write into an arbitrary-address copy. The constructor/allocation fixtures are synthetic native event schedules, not captured startup evidence. Default capability stays unavailable.

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

The extension runs normal and ASan/UBSan binaries with and without the layer generation export. It exercises the actual Adapter with Evidence allocation callbacks, Bridge bootstrap, successful initial observation/latch and a PassGate lifetime refusal over real process-allocated pass/packet/cache/attachment arrays. It does not substitute a ready Boolean, a fake unbounded reader, or synthetic complete-scan input. The staging extension exercises stereo upload and enqueue through this reader with modeled native call effects. Pass commit and a successor generation remain unsupported here.

The instruction suite executes native command allocation/begin caller code for raw results 0, -4 and +1, checks exact x0/x19/x20, SP, stack-saved LR and VkCommandBuffer operands, and preserves native success/failure Boolean outcomes. It also retains both full preferred/fallback allocation-to-map paths and 22 selector cases. There are 47 semantic guards. Vulkan calls, debug naming and native allocation boundaries remain explicitly synthetic.

RED logs include missing initial event attachment, missing Bridge composition, publication of a non-last pooled command, unrelated pending-command release and unpaired begin incorrectly preserving authorization. Further negatives cover failed begin, wrong SP/LR/C/handle, mismatched publication, reused C, release/delete/handle clear, missing device generation and premature Lifecycle reset. Normal/sanitizer execution results and measured memory peaks are retained in ignored `verification*.json`. No test repair from `4922e49` was cherry-picked; this lane never used the old ASLR staging-mapping path. The final serial verification passed normal plus machine tests, ASan/UBSan with and without the layer export, and the unchanged combined ASan/UBSan regression with 2000 frames and 4000 upstream replacements. Enforced limits were `memory.max=681574400` and `memory.swap.max=0`; measured peak was 277356544 bytes. Full command results are in ignored `final-verification.json` and `final-*.log`. Parent review remains required.

On any SteamVR update, reject the old SHA and re-establish all event scopes and layouts before altering the pin. No product update-check change is needed until an installer actually makes this dependency reachable; that installer must add the retest/check integration required by AGENTS.md.
