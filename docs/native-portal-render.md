# Native copied-packet render adapter

`native-portal/render/adapter.hpp` and `adapter.cpp` implement the reusable C++ adapter. They link the existing aperture implementation and control `FrameGate`. They install no hooks and have no dependency on the sibling pipeline implementation. Nothing here publishes `BackendStatus.capable`, `serialized`, or `external_clear`.

The default backend remains unsupported. Actual renderer/device lifecycle bindings, headroom evidence, native trampolines, pipeline acceptance, queue observation and completion are integration inputs, not properties established by these CPU tests. The pass-append correction described below is CPU integration coverage only; it installs no runtime binding.

## API and ownership

Construct `native_portal::render::Adapter` on the established native owner thread, outside render callbacks. Pass the existing `Backend`, its `FrameGate`, a `Binding`, and `Callbacks`. The adapter copies the binding and callback table. Their external context, control backend and gate must outlive it.

- `initialize()` allocates one persistent 592-byte native UBO wrapper and constructs one distinct native source-over marker. It requires an explicitly validated initialization boundary, nonzero process/renderer/device generations, every required call target, a headroom evidence ID and existing backend capability. It checks the native wrapper resource, size, initial offset sentinel and marker attachment. A failed initialization is not retried opportunistically in a draw callback.
- The owner calls `FrameGate::begin` once with authoritative policy/tracking, then passes the returned `Frame` to `latch`. The adapter keeps a value copy, including lease geometry, original deadline, revocation and origin. Do not independently call `FrameGate::eye`; `enqueue` owns those two calls.
- `enqueue(EyeInput)` receives the actual native 352-byte camera prefix, unchanged 528-byte constants, eye number and same-draw raw-to-Standing transform. It reads row-major float32 M1 at constants+64 and M2 at +128 without transposition, uses the existing aperture `compute`/`snapshot`, then calls the checked native managed-upload and recording boundaries.
- `outer_return()` seals the two ready eyes. It deliberately does not call `FrameGate::end`. Backends may run after this return. An early backend callback invalidates the complete frame.
- `backend(Recorded, Context, Registers)` authenticates copied packets and invokes the injected pipeline lookup/admission callback. Return `Portal` carries the approved native pipeline result. Return `Drop` carries zero for the existing native null-pipeline skip branch. Never retry a marked draw with a stock pipeline. `Stock` preserves the original lookup call and result, including when its label resembles a portal ticket.
- `submitted()` observes the separate final stereo queue boundary. Both CPU admissions must have succeeded. The read-only submission callback must establish actual successful submission of both exact tickets and return a nonzero immutable synchronization identity/serial bound to the original command. It must not queue new work. Under valid owner-thread/process/native lifetime ownership, the adapter saves that exact token once even if expiry or Off precedes observation. A false/missing observer or malformed token leaves native debt unresolved. Current authorization and `FrameGate::submitted` must still pass before status publication; otherwise `submitted()` returns false while retaining valid completion evidence. CPU completion or a nonnull pipeline never publishes submission.
- `poll_completion()` invokes the supplied nonblocking completion operation for that exact saved token, not a mutable resource's latest stamp. Off does not prevent polling. An unstamped native resource is not completion evidence.
- `reclaim()` releases the two resident ticket entries only after `poll_completion()` has confirmed the exact saved GPU token and a separately supplied `Callbacks::settled` confirms CPU/recording settlement for both exact ticket provenances and that same token. The callback defaults to null, which never authorizes reclamation. Both backend admissions alone are not CPU settlement. The callback must be read-only and nonblocking, and must establish that the owner has ended native CPU/recording references to the pair and settled recorded commands under its queue/resubmission policy. Native lifetime validation must pass before and after this callback. There are no timer, pointer-null, aging or Off shortcuts. Reclamation keeps the marker identity and does not publish authorization. It is idempotent until the next latch.
- `off()` revokes control authorization and ends frame admission, but retains all native objects. `retire()` additionally requires no unresolved native work and a validated CPU-settlement/cache-teardown lifecycle boundary. The normal wrapper destroy call receives a pointer to its slot. The lifecycle owner retires the marker, since this lane does not invent a missing blend-destruction ABI.

All owned callback paths reject other threads, recursion and inherited post-fork owners. Failure latches the control backend refused where appropriate. There are no hot-path allocations, locks that wait, file operations, socket operations, logs or GPU waits. Callback implementations must preserve those restrictions. A stock lookup bypasses the portal guard, so an unrelated native draw is not swallowed by an owned concurrency refusal.

Destroying the C++ adapter does not guess native teardown or free native resources. Keep the adapter, callback code and native ownership objects alive until `retire` succeeds. In particular, do not unload the containing DSO, recreate its ticket counter, or route pending records to a different owner. Lost completion deliberately retains resources.

## Exact register-call boundary

`Callbacks::call` accepts `Registers`, an explicit target plus x0..x7. It is a trampoline transport, not a guessed Valve C++ function declaration. Its Boolean reports whether the trampoline completed. It does not reinterpret a native return value as success. Preserve the surrounding callsite's remaining live registers, SP, LR and original continuation in the parent hook layer.

| Operation | Supplied register contract |
|---|---|
| Allocate, R vtable+0x60 | x0=R, w1=592, x2=0, w3=w4=w5=0, x6=0; returned x0 is wrapper |
| Blend, R+0x1a8 | x0=R, x1=six native words `[1,1,3,1,3,0]`; returned x0 is marker |
| Upload, R+0x78 | x0=R, x1=wrapper, x2=592 staging bytes; returned x0 is ignored |
| Descriptor, pinned internal resolver | x0=R, x1=24-byte output, x2=wrapper; called independently for both stage snapshots |
| Enqueue, R+0x228 | x0=R, x1=cloned prefix, x2=NUL-terminated ticket label, x4=original target |
| Wrapper destroy, R+0xb0 | x0=R, x1=pointer to wrapper slot; require slot zero after completion |

The marker attachment must read back exactly `[1,1,7,0,1,7,0,15]`. The stock-blend callback must validate the recognized stock path and RGBA write-mask policy before substitution. It must not approve arbitrary stock blend state.

Call targets come from the parent-validated pinned image and lifecycle binding. Nonzero numbers alone are not validation. `validate(Initialize)` must establish the authorized owning-thread initialization boundary and exact targets. `validate(Current)` checks that same renderer/device lifetime, including after Off for completion. `validate(Retire)` must establish stopped native admission, settled CPU references and native pipeline/cache teardown. These lifecycle sites are not proven by the current camera trace.

The pipeline callback receives immutable `Provenance` and the original lookup registers. It must perform the original lookup with exact x0..x3, then approve the actual returned native/Vulkan identities for this device generation, marker and exact patched shader pair, including cache hits. A nonnull stock pipeline is not approval. All generator-supported original pairs remain supported, exactly VS57 with PS125/126/127 and VS58 with PS128/129/130. Original enums remain unchanged.

## Managed upload contract

`Observation` is an audited, coherent native readback, not an application-owned GPU allocator. The parent reader must validate native pointer accessibility and lifetime before supplying it. Its inputs correspond to:

- Nonempty pass vector, R+0x15a0 and R+0x15a8.
- Active command wrapper at R+0x1380 and its command at +8.
- Arena resource R+0x1390, actual VkBuffer at resource+0xb0 and declared capacity at +0xb8.
- Map R+0x1410, u32 cursor R+0x1418 and native alignment R+0x328.
- Wrapper resource +8, declared size 592 and mutable offset +0x18.
- Immutable owned marker attachment and an audited recording/arena generation.

The adapter requires the pinned 0x80000 arena capacity, nonzero mapping/command/resource/buffer, power-of-two bounded alignment, aligned cursor, checked address arithmetic, checked 592-byte end and aligned next cursor. It requires an explicit headroom evidence ID and callback approval for each observed arena/cursor. The policy must reserve subsequent native workload space. There is no assumption that 592 extra bytes are universally safe.

The native upload callback executes the native copy, shader-read barrier and resource-registration path. This library never writes the GPU mapping. After that call, it checks unchanged resource identity and arena context, exact wrapper offset and expected cursor advance. It independently invokes the descriptor resolver twice and requires both exact `(buffer, old_cursor, 592)` triples. The recorder copies those into descriptor entry 0 and entry 4. Both entries are checked again at backend admission.

The comparison depends on the phase:

- Within one native upload, both actual pass bounds must remain identical, along with the command, recording generation and arena identity. Even a structurally valid append during that call refuses recording. The wrapper offset and aligned cursor advance must still match exactly.
- Between eyes and at deferred backend admission, the same pass begin may have a monotonically increasing end. Every observation must have nonzero, 8-byte-aligned bounds and a nonempty, integral span of the pinned 0x98-byte pass stride. Bounds must not wrap and the span must fit a signed 64-bit pointer difference. Pointer accessibility and allocation ownership remain the audited reader's responsibility. Arbitrary relocation, shrinking, malformed spans and resets are unsupported.
- Process/renderer/device lifetimes, command wrapper, command, recording generation, arena resource, buffer, map, capacity and alignment remain unchanged within the frame. Each backend validates the current cursor's bounds/alignment and remembers its accepted extent and cursor, so later admission cannot roll back even if it still exceeds the original upload boundary.

The binding must issue a real monotonically increasing recording generation at native reset/begin boundaries and keep it stable through both eyes and their backends. The adapter retains its generation watermark across reclamation and latches; a new frame's first upload requires a strictly newer generation. It cannot detect a reset and refill hidden between observations if the binding falsely preserves the generation. Never manufacture generations from command addresses, or freeze/normalize observed pass pointers to satisfy comparisons.

Backend authentication uses the immutable copied descriptors for each ticket, not the shared wrapper's mutable offset. The left eye remains valid when that wrapper already points at the right-eye upload. Neither pass-vector growth nor wrapper offset movement changes those saved descriptors. Relocated pass storage needs a separately audited binding before support can be added.

## Tickets and native byte preservation

The clone copies every byte of the native prefix. Only three u64 fields change: blend at +8, VS UBO slot0 at +0x38, and PS UBO slot0 at +0x58. Texture, geometry, depth/raster, enum and other constant slots remain unchanged. The original camera object and its native 528-byte UBO are never altered.

The label is exactly `FTP:` followed by the 11 unpadded base64url characters encoding a full big-endian uint64. It has 15 ASCII characters and a NUL. The decoder rejects zero, wrong lengths/prefixes, invalid alphabet, noncanonical padding bits and a missing bounded terminator. The native recorder owns its copied label. Native string relocation, not the caller's buffer or recorded packet address, carries identity.

The table is preallocated at construction, up to 4096 resident slots, with 1024 by default. The one-frame model needs only two slots. It reserves both before the first eye's native work, and rejects another latch while either resident entry remains. Even a capacity of four cannot hide an unsettled pair by admitting another frame. Only explicit `reclaim()` can recycle the pair, after exact GPU completion and CPU/recording settlement. Failed native calls, missing or malformed submission evidence, partial CPU admission, refused native ownership, and incomplete GPU or CPU settlement retain the pair and stop new work within this bound. Off alone never clears the table. There is no growing tombstone, log or history collection.

A process-wide lock-free monotonic counter issues ticket identities across adapter instances, with one nonblocking CAS attempt and checked refusal before wrap. Identities never recycle, even when resident slots do. The optional first-ticket floor can only skip forward; it cannot reset that counter. Each loaded copy of this library must have one lifetime under the parent owner, not be unloaded/reloaded around retained native records.

`State::tickets_used` means currently resident entries, either zero or two, not lifetime use. `tickets_capacity` is the fixed allocated bound and `tickets_issued` counts all identities reserved by this adapter, including failed attempts after reservation. `cpu_done` means both backend admissions succeeded. `gpu_complete` means the saved exact token completed. `cpu_settled` means explicit CPU/recording settlement and storage reclamation succeeded. The completion token and those flags remain available until a new latch. There is no implicit new lease: a new frame still needs valid explicit control authorization, including a renewed lease after expiry or Off.

A ticket binds process instance, renderer and device generations, frame, eye, lease epoch, revocation, origin, geometry generation, original deadline, recording generation, marker, wrapper, selected pair, entire cloned prefix and both descriptor triples. Lookup consumes a ticket once, including failed admission. Every bound value and the existing `FrameGate::accept` must pass at current BOOTTIME. Marker recognition occurs before label parsing. The immutable marker identity survives reclamation and retirement. A delayed old label therefore remains an owned packet and drops when its identity is no longer resident; it can never borrow a newer frame's slot or fall through to stock. As with other invalid owned packets, such a callback may invalidate the current frame. Unmarked stock packets forward unchanged, even with a spoofed label; invalid marked packets drop.

`Recorded` is a bounded caller-owned snapshot. Copy the actual prefix, recorded string length and at most 16 string bytes, plus all nine descriptors, from validated native memory. Descriptor entries 0 and 4 correspond to recorded+0x1c8 and +0x228. Do not treat recorded+0x288 as another portal UBO. Do not dereference an unbounded debug-string pointer in this library's place. A failed bounded read for a recognized marker must drop, never become a zeroed unmarked snapshot.

## Deliberate limits

This implementation supports one retained frame at a time. A subsequent frame needs exact prior-submission completion and explicit CPU/recording settlement followed by `reclaim()`. It never overwrites a lost submission token to keep rendering. Continuous completed frames reuse the same storage without a lifetime slot-exhaustion limit, but missing settlement still halts owned rendering. The finite uint64 ticket namespace still refuses before wrap. CPU-only sustained tests establish bounded storage and identity behavior, not native throughput or visible-frame continuity.

Successful CPU admission cannot retract the first eye's Vulkan command if authorization fails before the second eye. The final stereo queue owner must suppress that complete command transaction on refusal. Work already submitted cannot be revoked by this table. Resubmission of recorded Vulkan commands does not reenter `backend`; it therefore needs a separately validated submission policy. No expiry-to-last-visible guarantee is claimed.

The parent still must establish native lifecycle pins, safe initialization and cache teardown, actual recording generation and label readback, workload headroom, coherent arena reuse, actual queue/completion binding, an audited CPU/recording-settlement callback and command-buffer resubmission policy. This adapter does not prove physical camera freshness, finite visible pixels, stock RoomView coexistence, optical registration or live shader acceptance.

## Verification

Run from the checkout root:

```
python3 -B native-portal/render/test.py
python3 -B native-portal/render/test.py --sanitize
python3 -B native-portal/render/test.py --sanitize --trace /private/observer-live-01/metadata.bin
```

The last command accepts only the pinned trace SHA256 `0f2b0701eab2a02d3688133db18233655edca901fd2aed404030635f88e63be4`. It extracts fixtures under ignored `native-portal/render/build/`. No captured Valve bytes or generated fixtures belong in git. It replays all 882 captured native prefixes, constants and same-draw origins through this actual adapter, with explicitly synthetic leases/geometry, managed observations and external native operations. It is not another replay-only implementation.

Tests exercise native byte preservation and 592-byte extension layout, all six exact stage pairs, both descriptor slots, copied-record relocation, canonical tickets, process/renderer/device/recording generation mismatches, stale/duplicate/expired/revoked packets, changed origin/geometry, complete stereo readiness, transparent stock forwarding, failed native calls, checked managed overflow and absent headroom, native arena reset, process-wide ticket uniqueness, bounded-storage refusal and ticket wrap, queue result versus CPU completion, exact command token binding, missing completion retention, partial retirement refusal, off-thread refusal and reentrancy.

Observed RED failures included unimplemented initialization/latch, ticket reuse across owners, arena reset between eyes and before backend, missing enqueue x4, wrong queue-command acceptance and partial-retirement reactivation. Production fixes followed those failures. Normal and ASan/UBSan runs pass locally. The existing control protocol/policy/demand/socket/frame sanitizer suite also passes.

The continuous-use extension first reproduced the old capacity failure on frame 2 at capacity 2. `continuous_test.cpp` now runs 10000 full stereo frames at each of capacities 2 and 4 through actual latch, enqueue, deferred backend, exact submission, GPU completion and explicit reclamation. Each run issues 20000 strictly increasing uint64 identities above the high bit, holds at most two resident entries, and observes zero C++ allocations after construction. The fixtures use fixed-size arrays, injected native boundaries and BOOTTIME. Negative tests retain an unsettled pair over 1000 rejected replacement attempts, including spare-capacity cases. They cover missing settlement binding, native lifetime refusal before and during settlement, failed upload/descriptor/enqueue, partial CPU, missing/malformed queue tokens, incomplete GPU, delayed old packets after reuse/retirement and expiry/Off before or inside submission observation. R1's saved token remains exact; completing it alone never frees storage. Tests prove reuse only after settlement and a newly valid explicit lease. Normal and ASan/UBSan runs include all original R1 cases and the pinned 882-record replay. Full control protocol/policy/demand/socket/frame tests also pass in both modes.

## Pass-append regression verification

The production positive test was added and executed before changing `adapter.cpp` at baseline `1690da2c163db146e6a3e4c18de13eff9effafb2`. Normal and ASan/UBSan builds both failed the new assertion. All six live03 endpoint pairs reported `left=1 right=0 uploads=1 enqueues=1 resident=2`. With the comparison split, those same tests report `left=1 right=1 uploads=2 enqueues=2 resident=2`, then exercise deferred backend admission and exact submission/completion/reclamation.

The fixture transcribes actual pass begins/ends and command addresses for frames 601, 602, 603, 743, 744 and 745 from trace SHA256 `29705cfc2019d5c22f9cbf335a5312871d55c23e7f3be6db6beae94d3614d123`. Leases, recording generations, native operations and later backend observations are synthetic. The synthetic right cursor includes the extra left-eye portal upload. Additional same-begin appends before and between backends test supported CPU behavior; live03 did not capture backend pass bounds. No observation callback freezes pass endpoints, and the left backend sees the wrapper already holding the right offset.

Negative tests cover shrink, nonintegral stride, null/reversed/misaligned/wrapping/oversized spans, unaudited relocation, cursor reset/regression/misalignment/overflow, command or command-wrapper change, generation change/reuse/regression, arena/buffer/map/capacity/alignment replacement, and pass mutation inside the native upload. The second backend also refuses rollback from a newer accepted extent or cursor. The original R1 saved-token tests, default-null settlement refusal, never-reused ticket tests and 10000-frame runs at capacities 2 and 4 remain in the same runner.

Run each verification command above under `systemd-run --user --scope -p MemoryMax=650M -p MemorySwapMax=0` on a small build host. The pass-append change passed normal, ASan/UBSan and sanitized pinned 882-record replay locally with verified `memory.max=681574400` and `memory.swap.max=0`. This is CPU integration evidence only. No runtime deployment, remote services, hooks, queue ledger, owner or pipeline changes were made.

## Earlier verification

Before the continuous-use extension, the isolated mirror `/home/steamos/dev/frametop-native-render` built and ran the prior sanitizer tests inside the existing `dev` container. `file` identified the resulting executable as ARM aarch64. No live compositor, camera, shader, service, pose, configuration or active application deployment was touched. The continuous-use extension was executed locally in a 650 MiB MemoryMax, zero MemorySwapMax scope. An independent peer approved that earlier production delta in a static-only review against its baseline contract. The peer could not read the checkout and did not independently run the tests. That review does not cover the pass-append correction. Independent parent integration review remains required; no live deployment or target rendering validation is claimed.
