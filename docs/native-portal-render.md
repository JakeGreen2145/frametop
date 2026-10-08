# Native copied-packet render adapter

`native-portal/render/adapter.hpp` and `adapter.cpp` implement the reusable C++ adapter. They link the existing aperture implementation and control `FrameGate`. They install no hooks and have no dependency on the sibling pipeline implementation. Nothing here publishes `BackendStatus.capable`, `serialized`, or `external_clear`.

The default backend remains unsupported. Actual renderer/device lifecycle bindings, headroom evidence, native trampolines, pipeline acceptance, queue observation and completion are integration inputs, not properties established by these CPU tests.

## API and ownership

Construct `native_portal::render::Adapter` on the established native owner thread, outside render callbacks. Pass the existing `Backend`, its `FrameGate`, a `Binding`, and `Callbacks`. The adapter copies the binding and callback table. Their external context, control backend and gate must outlive it.

- `initialize()` allocates one persistent 592-byte native UBO wrapper and constructs one distinct native source-over marker. It requires an explicitly validated initialization boundary, nonzero process/renderer/device generations, every required call target, a headroom evidence ID and existing backend capability. It checks the native wrapper resource, size, initial offset sentinel and marker attachment. A failed initialization is not retried opportunistically in a draw callback.
- The owner calls `FrameGate::begin` once with authoritative policy/tracking, then passes the returned `Frame` to `latch`. The adapter keeps a value copy, including lease geometry, original deadline, revocation and origin. Do not independently call `FrameGate::eye`; `enqueue` owns those two calls.
- `enqueue(EyeInput)` receives the actual native 352-byte camera prefix, unchanged 528-byte constants, eye number and same-draw raw-to-Standing transform. It reads row-major float32 M1 at constants+64 and M2 at +128 without transposition, uses the existing aperture `compute`/`snapshot`, then calls the checked native managed-upload and recording boundaries.
- `outer_return()` seals the two ready eyes. It deliberately does not call `FrameGate::end`. Backends may run after this return. An early backend callback invalidates the complete frame.
- `backend(Recorded, Context, Registers)` authenticates copied packets and invokes the injected pipeline lookup/admission callback. Return `Portal` carries the approved native pipeline result. Return `Drop` carries zero for the existing native null-pipeline skip branch. Never retry a marked draw with a stock pipeline. `Stock` preserves the original lookup call and result, including when its label resembles a portal ticket.
- `submitted()` observes the separate final stereo queue boundary. Both CPU admissions must have succeeded. The read-only submission callback must establish actual successful submission of both exact tickets and return a nonzero immutable synchronization identity/serial bound to the original command. It must not queue new work. Under valid owner-thread/process/native lifetime ownership, the adapter saves that exact token once even if expiry or Off precedes observation. A false/missing observer or malformed token leaves native debt unresolved. Current authorization and `FrameGate::submitted` must still pass before status publication; otherwise `submitted()` returns false while retaining valid completion evidence. CPU completion or a nonnull pipeline never publishes submission.
- `poll_completion()` invokes the supplied nonblocking completion operation for that exact saved token, not a mutable resource's latest stamp. Off does not prevent polling. An unstamped native resource is not completion evidence.
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

Sequential eyes must share the audited arena/command/pass identity, and the cursor cannot regress or overlap the preceding upload. Backend admission rechecks the live arena identity, minimum cursor and immutable attachment. A native schedule that changes these fields before the deferred callback is conservatively unsupported until a separately validated ownership handoff exists.

## Tickets and native byte preservation

The clone copies every byte of the native prefix. Only three u64 fields change: blend at +8, VS UBO slot0 at +0x38, and PS UBO slot0 at +0x58. Texture, geometry, depth/raster, enum and other constant slots remain unchanged. The original camera object and its native 528-byte UBO are never altered.

The label is exactly `FTP:` followed by the 11 unpadded base64url characters encoding a full big-endian uint64. It has 15 ASCII characters and a NUL. The decoder rejects zero, wrong lengths/prefixes, invalid alphabet, noncanonical padding bits and a missing bounded terminator. The native recorder owns its copied label. Native string relocation, not the caller's buffer or recorded packet address, carries identity.

The table is preallocated at construction, up to 4096 lifetime slots, with 1024 by default. It reserves two entries before the first eye's native work. Slots never recycle, even after failure or consumption. A process-wide lock-free monotonic counter issues tickets across adapter instances, with one nonblocking CAS attempt and checked refusal before wrap. The optional first-ticket floor can only skip forward; it cannot reset that counter. Each loaded copy of this library must have one lifetime under the parent owner, not be unloaded/reloaded around retained native records.

A ticket binds process instance, renderer and device generations, frame, eye, lease epoch, revocation, origin, geometry generation, original deadline, recording generation, marker, wrapper, selected pair, entire cloned prefix and both descriptor triples. Lookup consumes a ticket once, including failed admission. Every bound value and the existing `FrameGate::accept` must pass at current BOOTTIME. Marker recognition occurs before label parsing. Unmarked stock packets forward unchanged, even with a spoofed label; invalid marked packets drop.

`Recorded` is a bounded caller-owned snapshot. Copy the actual prefix, recorded string length and at most 16 string bytes, plus all nine descriptors, from validated native memory. Descriptor entries 0 and 4 correspond to recorded+0x1c8 and +0x228. Do not treat recorded+0x288 as another portal UBO. Do not dereference an unbounded debug-string pointer in this library's place. A failed bounded read for a recognized marker must drop, never become a zeroed unmarked snapshot.

## Deliberate limits

This implementation supports one retained frame at a time. It will not replace that frame while native work is unresolved. A subsequent frame needs actual completion of the prior submission. It never overwrites a lost submission token to keep rendering. Lifetime table exhaustion refuses new owned work; expanding beyond the configured bound or recycling slots is not supported. These conservative limits can reduce throughput or halt owned rendering. They are not a continuous-rendering performance claim.

Successful CPU admission cannot retract the first eye's Vulkan command if authorization fails before the second eye. The final stereo queue owner must suppress that complete command transaction on refusal. Work already submitted cannot be revoked by this table. Resubmission of recorded Vulkan commands does not reenter `backend`; it therefore needs a separately validated submission policy. No expiry-to-last-visible guarantee is claimed.

The parent still must establish native lifecycle pins, safe initialization and cache teardown, actual recording generation and label readback, workload headroom, coherent arena reuse, actual queue/completion binding and command-buffer resubmission policy. This adapter does not prove physical camera freshness, finite visible pixels, stock RoomView coexistence, optical registration or live shader acceptance.

## Verification

Run from the checkout root:

```
python3 -B native-portal/render/test.py
python3 -B native-portal/render/test.py --sanitize
python3 -B native-portal/render/test.py --sanitize --trace /private/observer-live-01/metadata.bin
```

The last command accepts only the pinned trace SHA256 `0f2b0701eab2a02d3688133db18233655edca901fd2aed404030635f88e63be4`. It extracts fixtures under ignored `native-portal/render/build/`. No captured Valve bytes or generated fixtures belong in git. It replays all 882 captured native prefixes, constants and same-draw origins through this actual adapter, with explicitly synthetic leases/geometry, managed observations and external native operations. It is not another replay-only implementation.

Tests exercise native byte preservation and 592-byte extension layout, all six exact stage pairs, both descriptor slots, copied-record relocation, canonical tickets, process/renderer/device/recording generation mismatches, stale/duplicate/expired/revoked packets, changed origin/geometry, complete stereo readiness, transparent stock forwarding, failed native calls, checked managed overflow and absent headroom, native arena reset, process-wide ticket uniqueness, table exhaustion/wrap, queue result versus CPU completion, exact command token binding, missing completion retention, partial retirement refusal, off-thread refusal and reentrancy.

Observed RED failures included unimplemented initialization/latch, ticket reuse across owners, arena reset between eyes and before backend, missing enqueue x4, wrong queue-command acceptance and partial-retirement reactivation. Production fixes followed those failures. Normal and ASan/UBSan runs pass locally. The existing control protocol/policy/demand/socket/frame sanitizer suite also passes.

The isolated mirror `/home/steamos/dev/frametop-native-render` builds and runs the same sanitizer tests inside the existing `dev` container. `file` identifies the resulting executable as ARM aarch64. No live compositor, camera, shader, service, pose, configuration or active application deployment was touched. Independent parent integration review remains required; this subagent environment has no delegate tool or installed Codex CLI.
