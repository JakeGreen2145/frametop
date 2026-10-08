# Native arena reservation

`native-portal/arena/` implements the fixed reservation and upstream upload bridge. It is inactive. Nothing adds it to the renderer, observer, preload constructor, product build, or deployment scripts.

## Implemented

- `policy.hpp` keeps stock-equivalent cursor B and owned extra D. At alignment 64, a 592-byte replacement charges B by 576 and D by 64. Normal UBO and geometry writes charge their aligned size to B. B cannot exceed 524288, D cannot exceed 128, and native cursor must equal B+D. At most two substitutions are accepted between genuine arena resets.
- `bridge.cpp` replaces an authenticated 528-byte upstream upload with one native 592-byte upload. It reads exactly 528 completed bytes from K+0xc0 into owned staging and appends the offered 64 bytes. It checks the stock wrapper's 528-byte resource and the persistent owned wrapper's 592-byte resource. It never passes K+0xc0 to a 592-byte copy and never changes K's wrapper.
- The original register-call instruction invokes native `0x2993f8`. Unowned upstream calls forward once. Third or repeated owned invocations skip the upload, invalidate their descriptor result, and never fall back to a stock upload. The integration must also drop their later owned enqueue.
- The allocation bridge changes only w1 at the native allocator call, from 0x80000 to 0x80080. Its immediate return hook captures the actual output resource. Initialization at 0x7fdbc requires that same resource, exact returned capacity, nonzero map and VkBuffer, alignment 64, zero cursor, successful native initialization, and binding-supplied coherence evidence. It does not rewrite a late-attached resource size.
- Common append admission checks 64-bit arithmetic before the native copy. Native copy, barrier registration, masks, alignment and cursor store remain native. The bridge charges before copying and never refunds an interrupted or cancelled write. Only the actual reset instruction clears counters, not queue submission or packet disposal.
- Both native classifiers accept the enlarged arena range while preserving UINT32_MAX as the own-buffer sentinel. Geometry still validates its extent against the current cursor, with an additional checked-width guard before the original 32-bit instructions.
- `emit.c` generates real AArch64 islands using the existing `hooks/emit.h` save/restore generator unchanged. The callback receives the 800-byte saved register context and site ID. Four restored continuations select original instructions, replacement instructions, owned upload skip, or an unsupported-state BRK. There is no invented C++ declaration for the native upload ABI.

Unsupported native stock overflow is not a recoverable dropped upload. Once the recognized reservation route is active, an accounting/context failure chooses BRK before copy, not the native log-and-copy path. This is deliberately fatal unsupported-state behavior, not a production recovery contract. Before recognized successful initialization, portal capability stays unavailable and stock instructions remain unchanged.

## Pins and scope

The only supported ELF is:

```
/home/jakeg/.hermes/work/frame-native-reuse/camera-analysis/vrcompositor
SHA256 9b1bd5fe4463592cbc71b2517b9ceb0a0fcd66cd6885a19dca4be0784bf6c265
```

The implementation follows all arena sections of `/home/jakeg/.hermes/work/frame-native-transaction-feasibility/REPORT.md`. `manifest.json` pins the full ELF, context hashes, direct branch/relocation predecessors, and imported offline fixture source hashes. `prepare.py` rejects changed hashes or predecessor sets. It scans every aligned executable word rather than stopping linear disassembly at embedded data. This audit does not prove the absence of arbitrary indirect callers or memory aliases.

| Site ID | Address | Register scope / effect |
|---|---|---|
| Upload | 0x161d64 | R=x0, K=x27, original x7 upload target; replace x1/x2 only for the offered owned call |
| Allocate | 0x29a14c | R=x19, allocator=x0, output slot=x8; change w1 only for the armed instance |
| Admission | 0x299110 | R=x19, size=w20, source=x25, output=x21; checked common accounting |
| UboClassify | 0x2988b0 | R=x0, offset=w4; expanded threshold in w3 |
| GeometryClassify | 0x29f358 | R=x21, packet=x24, offset=w3; expanded threshold in w0 |
| Initialized | 0x7fdbc | R=x24, native successful result in low byte of x0 |
| Reset | 0x2987d4 | R=x22; clear accounting immediately before the real zero store |
| UploadReturn | 0x161d68 | offered K=x27; snapshot native wrapper offset and arena descriptor fields |
| AllocationReturn | 0x29a150 | R=x19; capture native allocation output before its stack slot goes out of scope |

A Bridge has no global state and starts unarmed. `arm` binds one renderer, renderer lifetime, owner thread, allocator and ELF base. It must happen before the native allocation. Other renderer addresses follow their original instructions, even with different lifetimes or threads. A bound lifetime/thread mismatch cannot borrow another instance's reservation. The caller must serialize `arm`, `offer`, `dispatch` and descriptor consumption on its established native owner thread. The class does not infer provenance from a shader or camera-looking buffer.

`offer` requires a caller-authenticated full ticket and immutable 64-byte portal suffix. It pins one persistent owned wrapper across resets. The binding must retain the matching frame, eye, lease, recording generation and marker in the reviewed adapter. A returned `Descriptor` contains actual native resource/offset fields plus the offered ticket. Consume it before offering the next eye and preserve it in that ticket. Reading the shared wrapper later is not a substitute.

## Verification

Run from any directory with the exact absolute path:

```sh
systemd-run --user --wait --pipe \
  -p MemoryMax=650M -p MemorySwapMax=0 \
  python3 -B /home/jakeg/.hermes/work/frametop-native-arena-reservation/native-portal/arena/test.py
```

The runner checks the cgroup limits, uses disk-backed owned scratch for compiler temporaries, and produces only ignored `arena/build/` artifacts. It uses the already-provisioned offline Unicorn, Capstone and pyelftools packages. No ARM host, runtime, camera, Vulkan device, SSH or network operation is involved.

Executed coverage:

- Original-instruction RED controls fail both full-budget admission and enlarged UBO classification. The runner retains this expected failure in `build/red.log`.
- C++ policy and production bridge tests pass under ASan and UBSan. They cover maximum-width arithmetic, full stock budget, sentinel/high offsets, wrong alignment, persistent wrapper identity, unrelated renderer preservation and conservative failed-write accounting.
- Fourteen Unicorn integration tests execute generated islands, the real C++ Bridge via a test-only ctypes ABI, and the pinned native upload/common append/descriptor/geometry instructions. The boundary witness ends at 524416 after two substitutions and all remaining original stock capacity. It performs exactly two 592-byte copies plus the remaining stock copy, with three native barrier-boundary calls and no native diagnostic.
- Two emitter tests cover address/range refusal and all four continuation choices at all nine sites at three image bases. Differential execution verifies GPRs, all SIMD registers, NZCV, FPCR, FPSR, LR, SP and relevant native memory effects after a hostile callback clobbers registers.
- Regression tests reject lookalike allocation resources, wrong native initialization result/capacity/map/coherence/alignment, wrong thread/lifetime, duplicate descriptor revival and wrong stock wrapper size. A stopped native copy retains its charge until the real cursor reset.

The native allocator, memcpy and barrier external boundaries are explicitly synthetic in Unicorn. Native allocation-call register ABI and output identity are tested; this does not prove a real Vulkan mapping or GPU execution. `build/verification.json` records executed source hashes and measured cgroup memory limits/peak. Generated instruction words and private ELF data are ignored, never committed.

## Parent integration delta, not applied here

The reviewed `render`, `control`, `pipeline`, `binding` and `hooks/observer.c` files are unchanged. The following changes belong to their owner:

1. Bind all nine islands as one coordinated prepared set before native renderer allocation. Route the exact saved-context callback into its lifetime-owned Bridge. Keep the existing patch quiescence, page permissions, instruction-cache and teardown contracts. There is intentionally no installer in this directory.
2. Provide actual memory/coherence evidence to `Memory::coherent`. This callback is mandatory and fails closed when missing. The current tests supply synthetic evidence. A mapped pointer or the 0x80080 size field alone is not sufficient evidence for a live binding.
3. Split `Adapter::enqueue` preparation from publication. Move ticket reservation, aperture computation, lease/origin validation and upload debt admission ahead of 0x161d64. Offer the persistent wrapper, owned staging and 64-byte suffix to Bridge. The hook copies the final 528 native bytes only after native constant generation has completed.
4. At 0x161d68, save the returned descriptor, ticket and exact before/after arena observations. In `Adapter::enqueue`, replace the current upload call block at lines 296..308 with consumption of that prepared result. Keep observation validation, original ticket/marker carrier and both descriptor checks. Do not issue another 592-byte upload at 0x161ed8. Missing or invalid prepared evidence means DROP for an already-owned invocation, never stock fallback.
5. Replace sampled `headroom_evidence` admission with the recognized fixed reservation route. Do not reset Bridge at submit or queue disposal. Keep command/recording generations distinct from this arena-reset interval.
6. Preserve the parent transaction policy. An accepted pre-encoding stereo commit may finish after Off, with Off pending until actual settlement. This arena implementation neither publishes completion nor changes submission, resubmission, settlement or ticket-recycling policy. It gives no finite Off latency guarantee.

Until these binding/adapter changes and real coherence evidence exist, do not advertise production capability. The deliverable here is compiled native policy/bridge code and executable island generation with instruction tests, not a deployed or end-to-end native portal.

## Update check

Any SteamVR update that changes the ELF hash disables preparation. Re-establish allocation, both append predecessors, both classifiers, reset and initialization identities before changing the pin. The parent installer must add the existing repository update-check integration when it first makes this dependency reachable. No unverified fallback is provided.
