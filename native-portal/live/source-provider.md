# Source-provider binding and admission boundary

This change implements `ClientSourceBinding`, the native provider observation part. It does **not** supply a working `SourceAdmission`, install new hooks, or enable frames. The production source list compiles it. `Runtime` construction and the original camera command path are unchanged.

The distinction is intentional. Returning `ready=true` after these connection observations would still fabricate producer publication and source-consumer retirement. There is no replacement always-false `SourceAdmission` class either.

## Implemented observations

The binding takes the existing camera gate's original retained ticket before any native read. It consumes unmodified saved registers at the actual compositor and vrclient instructions, not inferred camera activity bytes. It authenticates:

- the pinned client vptr `60f108` and exact Connect `32a270` / Disconnect `328380` target used by the native camera;
- the camera, manager, queue name argument, output slot or disconnect ID, caller return PC and stack scope;
- the new-entry branch, native returned header ID against the resolved alias, reader-map insertion return and actual Connect result;
- the native exact-ID dedup branch separately as `ExistingEntry`, never another counted owner;
- each raw service result independently, including first-queue failure followed by a successful second queue and Release returning one without calling either service.

A missing event does not grant ownership. A failed/unreadable/mismatched header, foreign callback, wrong scope, provider replacement, binding replacement or base change revokes this observer. Each invocation clears previous call observations. Reattachment does not inherit a prior branch result. Only fixed scalar loads at native dereference sites are allowed. No client queue offsets are inferred from the server queue implementation.

`Observation.valid` means that the observations received in this invocation were paired under the retained camera scope. It is not a successful pair, a readiness predicate, a detach proof or a settlement certificate. An empty fast-path invocation can be valid with both `observed=false`. `QueueCall.id` denotes a successful Connect output or the original Disconnect argument. Zero after failed Connect does not assert that native cleared its retained output slot. `NewEntry` describes the local manager insertion path, not exclusive portal release authority. The header pointer and ID are call-local facts, not an immutable header/mapping generation.

## Exact hook ABI needed by the runtime owner

The current `SourceAdmission::transition(binding, plan, actual)` is post-call only. It cannot recover the inner observations after the original method has returned.

Use the original owner-queue gate ticket to call `begin(ticket, binding, action, compositor_base, client_base, compositor_digest, client_digest)` **before** the original camera call. Forward the following saved-register events unchanged, then call `finish(actual_camera_return)`. The caller must retain the ticket throughout. No extra camera Acquire/Release is allowed for observation.

| Module | Before-instruction PC | Meaning |
|---|---|---|
| compositor | `15fa24` / `15fa28` | data Connect call / result |
| compositor | `15fa44` / `15fa48` | layout Connect call / result after data success |
| compositor | `15fae8` / `15faec` | layout Connect call / result after data failure |
| compositor | `15fbec` / `15fbf0` | data Disconnect call / result |
| compositor | `15fc34` / `15fc38` | layout Disconnect call / result |
| vrclient | `32a270` | exact Connect entry, original manager/output/name and caller LR |
| vrclient | `32a398` | dedup branch, require native compared IDs to be equal |
| vrclient | `32a39c` | new-object path |
| vrclient | `32a49c` | actual header accessor return, immediately before native header-ID load |
| vrclient | `32a504` | reader-map insertion returned |

At compositor Connect calls, `x19=camera`, `x0=provider`, `x1=out-slot`, `x2=name`, `x3=target`. Disconnect uses `x1=id`, `x2=target`. Inner client events have `SP=callerSP-0xc0`, `x20=manager`, `x23=out-slot`, `x19=alias`. These registers come from executed pinned instructions in the tests.

The loader must authenticate the **live client image and lifecycle**, not just receive the expected digest string. As with existing `runtime::Evidence`, digest parameters are a trusted preparer contract, not cryptographic proof supplied by the caller. This change does not extend the current compositor-only loader or claim register-preserving emitters for the new client sites. Adding those hooks without the remaining admission facts would not enable a sound provider.

## Exact remaining boundary

The allowed handoffs establish static native behavior, but the current runtime supplies no authenticated observation interface for these remaining facts:

1. **Intact producer and committed queue generation.** Driver `d1380` uses the separately supplied server-side `IVRBlockQueue_005`. Its attempted-once latch is not successful Create. Need attributable successful per-queue creation or an authenticated intact-generation contract, name/alias/header/mapping identity, capacity, and actual successful matching layout/data publication. Client Connect and the header ID loaded here cannot provide that history. Membership `header+1c` and queue IDs must not be manufactured into `header_generation`.
2. **Readable metadata obligations and full consumer detach.** Need the actual original camera read acquisitions and matching releases, with queue and metadata-handle identity. Existing static client contracts identify read-side `3280a8` / `327640` and inner `327e18` / `3274d8`. Disconnect map erasure alone does not drain captured waiter references or outstanding readable handles. Do not populate `producer_revalidated` from successful Release or from this observer's valid flag.
3. **Attribution to the preserved source-command lifetime.** Need an observation that associates each original Adapter ticket watermark with its native sampled-resource use, ordinary `29abf0` acquire, `2a1e18` reverse release, relevant actual inner registry ref/unref or initial-owner conversion, and metadata return. The post-only `settled(binding, watermark)` interface currently receives no such attribution. RPC `226/228` success and import success are not the inner registry results. Existing Command/Lifecycle remain the independent authorities for the original accepted command, exact timeline and CPU disposal/reset.

These are missing operational observations and integration contracts, not a request to strengthen every Valve error path. B4 caller review section 5 supersedes the older speculative requirement for an additional external DMA fence on this unchanged native path. This implementation creates no source copy, queue, DMA fence, ownership registry, shader path or custom camera stream. App `+25668` remains VSYNC.

The newer DMA and native-reuse-closure contents were not available under the local transfer permission. No Desktop access, SSH, alternate transfer, substitute remote request or duplicate bulk analysis was attempted. Their conclusions are not guessed here. The runtime must keep `SourceAdmission` absent until the remaining facts can be supplied and wired.

## Evidence and verification

Primary delivered inputs:

- `CHECKPOINT-06-EVIDENCE.zip`, SHA256 `072cd636fab516b7afec48a18ba9d5a9a69f9201eee045ddce840a388d4c93aa`, selected static facts, client provider/transport and server ownership reports. Only selected members were copied into the owned ignored build directory.
- Delivered checkpoint02 `vrclient-independent/elf-0032a270.asm`.
- Pinned local compositor SHA256 `9b1bd5fe4463592cbc71b2517b9ceb0a0fcd66cd6885a19dca4be0784bf6c265` and client SHA256 `fe967dcc0d97dab984bd9acc1dc522fcecbcf9beb794fdd7ec156334ec31e313`.
- Delivered `B4-CALLER-COMPATIBILITY.md`, section 5.

Run from the checkout with the already provisioned `PYTHONPATH` and `FT_HOOK_TEST_ELF`:

```sh
systemd-run --user --scope -p MemoryMax=256M -p MemorySwapMax=0 \
  python3 native-portal/live/source_provider_test.py
systemd-run --user --scope -p MemoryMax=256M -p MemorySwapMax=0 \
  env FT_SOURCE_UBSAN=1 python3 native-portal/live/source_provider_test.py
```

Both runs passed **17 tests**, including the positive production binding against pinned compositor and client instruction execution. RED was observed for the absent implementation, provider replacement and missing production build inventory. Compilation used first-party warnings as errors. Generated actual call observations and test results are in `native-portal/build/source-provider/{results,ubsan-results}.json`.

The fixture models OS/shared mapping, property service, allocator, C++ tree insertion, temporary disposal and Disconnect boundaries. It executes the original camera functions, client Connect path, actual ID search/dedup branches, header comparison, success/error branches and output writes. Those modeled service boundaries are not production readiness or lifetime evidence. The production observer and real camera gate are compiled and exercised. No hardware, GPU, camera API, deployment, native loader activation or full production ARM64 build was performed.

The first expanded suite reached the 256 MiB limit because callback cycles retained old emulators. Releasing each Unicorn hook and ctypes callback before collecting the fixture fixed the runner; both final suites completed under the unchanged cap and zero swap. On a SteamVR update, recheck both module hashes and rerun these witnesses before any loader integration. Existing exact pins reject other builds.
