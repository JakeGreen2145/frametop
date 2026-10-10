# Production native source admission

`Runtime()` now owns and installs `NativeSourceAdmission`. `ft_native_portal_start` constructs that default. `Runtime(SourceAdmission*)` remains an explicit fixture constructor; an explicit null still disables admission. The provider is compiled into the production Runtime translation unit and included in the production header hash inventory.

This is conditional borrowing of the exact original native enqueue. It is not a new camera reader, import owner, producer-generation certificate or DMA synchronization implementation.

## Separate contracts

- `connect_eligible` authenticates the retained camera gate binding, loader-retained pinned modules, owner thread/process and bidirectional application/camera/renderer/device association. It accepts an authentic cold connection before publication. Warm IDs require the retained journal of actual native calls or a complete original Update observation, not merely nonzero IDs or clear flags.
- `borrow_ready` accepts one by-value offer immediately before the Adapter's sole replacement enqueue. The offer includes the nonreused upload invocation ticket, HMD eye, explicit Adapter pair watermark, original command/recording, native SP/camera, actual selected resource-camera index and ring slot, selected wrapper, final native 352-byte prefix and replacement prefix. It never calls Frame, Read, Update or another native acquisition.
- `transition_eligible` covers action/lifetime independently of fresh images. Off, exact Command/Lifecycle settlement and closing do not wait for publication. A Disconnect requires the original locally owned native connection. Borrowed USE can retire locally without calling Disconnect or asserting global native idle.

The old cold-idle scalar remains diagnostic only. Production readiness comes from the authenticated contract. `DemandAdapter::establish_native_path` does not assert native idle, erase current connection debt or recover unknown handoffs/failures. It preserves eligibility across connected frames that have not produced geometry yet. Existing policy, tracking, geometry, native lifetime and pending-debt gates remain separate.

## Exact current selection

The existing `161d64` upload and return still authenticate the same invocation, camera, native constants and stock UBO. At the final `161ed8` callback, Evidence captures the actual current selection made by the original native code:

- `x28` is the resource-camera index, not the HMD eye.
- The original current slot comes from camera `+820+4*index`.
- At enqueue, `x21` and `x24` both contain `3*slot`.
- The selected original sampled wrapper is camera `+520+384*index+24*slot` and must equal final prefix `+80`.

The supported bounds are two resource cameras and sixteen ring slots. Final prefixes remain equal except blend `+8` and the two owned UBO wrapper fields `+38/+58`. All sampled fields `+80..+f8` survive unchanged. The callback refuses before the replacement enqueue. An owned refusal drops rather than forwarding stock, and already charged native upload debt remains pending.

Backend admission matches the actual prepared packet to the retained ticket, final replacement prefix, original command and recording. Source observations remain the existing ten actual native hooks. Two eyes sharing the same image/frame use the native idempotent acquire, not two invented acquire barriers.

## Honest connection and use dispositions

`AuthenticatedBorrowedUse` is separate from queue ownership. A warm Acquire can return one without making any Connect calls. The receipt preserves both `observed=false` fields; no per-queue calls are invented. Exact-ID dedup remains Borrowed, not NewlyOwned. The arbiter permits only local consumer use and can relinquish its closing barrier after local debt settles without claiming global native idle. Retiring the last borrowed token revokes that use permission without changing queue facts. The next On requires a fresh original native receipt, so an intervening stock detach cannot leave a new connection mislabeled Borrowed.

`NativeConnection` is deliberately narrower than the older `NewlyOwned` contract. It requires an original retained camera Acquire that began cold, or after this journal's complete observed native Disconnect, followed by both actual successful NewEntry call chains and the actual native result. It permits only the matching original camera Disconnect obligation. It does not claim exclusive manager ownership, driver Create, producer repair, independent waiter drain or an immutable service generation. `header_generation` remains zero. The exact Disconnect results and original IDs must match before this local connection obligation clears.

The production provider preserves raw partial results, including a failed first queue and successful peer, while refusing use/detach authority. Failed, missing, mismatched and migrated observations latch uncertainty. It never retries a partial Connect based on idle-looking flags.

A Release returning one without Disconnect is not detach. The one distinct supported case is the actual already-installed aggregate decision at `15fbb8` selecting the native held no-op continuation. That event preserves the existing connection and all raw empty queue observations. It grants no detach certificate. Stock Release/Clear continuations and native response bookkeeping remain unchanged.

## Existing settlement, no second GPU authority

The provider retains one immutable pair before Adapter reclamation. `source_command` now stores the explicit ticket-to-watermark association at that same callback. Ticket IDs are process-global and nonreused; the watermark is an Adapter-local cumulative count. They may differ, including in a second Runtime in the same process.

Actual sampled calls, reverse release and registration coverage must be frozen before native submission. Only the existing Command acceptance/token/exact wait and Lifecycle packet disposal/executable reset can settle that pair. The provider's settlement acknowledgment is idempotent if gate-ticket release must be retried. Rejected pairs still need the existing exact two-skip upload-command settlement; no sample or reverse release is manufactured for them.

No source registry, extra queue, extra GPU fence or separate GPU-truth ledger was added. Missing GPU wait, CPU disposal/reset, source callbacks or module/association lifetime keeps debt. Closing does not erase it.

## Executed CPU verification

`production_admission_test.py` constructs the real default Runtime and uses the production provider, not ModeledProvider. Native call storage, graphics allocation, command publication/disposal and Vulkan returns remain explicit fixtures. The source manager observations, metadata observations and final selection coordinates are generated by executing the pinned compositor/client instructions.

Positive scenarios cover cold no-frame eligibility, first native Connect, original Update, exact selection, both eyes, private-socket Off and exact settlement. Cold, warm and dedup each run eight complete cycles twice in one process. The second instance proves ticket IDs are not pair watermarks. Warm and dedup execute zero native Disconnects. A separate connected/no-publication frame followed by first geometry preserves the same native connection instead of clearing demand eligibility or acquiring again. Aggregate portal-first, portal-last and held Clear cases retain original native flags/results and finish normal joined cleanup.

Negative scenarios cover partial Connect, partial alias failure, missing inner insertion, provider migration/loss, camera-to-application migration and a wrong current-selection tuple. Refused selection performs zero enqueues and retains charged upload debt. Each negative reaches its explicitly marked cleanup refusal rather than hiding unresolved lifetime with `_Exit`.

The pure provider unit adds partial receipt preservation, distinct zero header generation, duplicate borrow refusal, wrong-watermark refusal, idempotent settled acknowledgment and unobserved Release-one refusal. Its scope data is synthetic, separately from production Runtime tests.

The existing 19 camera Runtime cases, seven source Runtime cases and 15 source-command negatives remain regression requirements. `source_command_native.py` generates actual merged-island sampled-use/acquire/reverse events. Replaying its `source-native.bin` through the new default-production executable validates the same exact Command/Lifecycle chain with the real provider. Normal and ASan/UBSan runs are separate; compilation and emulation are serial with 256 MiB and zero swap.

```sh
export FT_HOOK_TEST_ELF=/home/jakeg/.hermes/work/frame-native-reuse/camera-analysis/vrcompositor
export PYTHONPATH=/home/jakeg/.hermes/work/frame-native-product-design/pylib:/home/jakeg/.hermes/work/frame-native-reuse/camera-analysis/pylib
systemd-run --user --scope -p MemoryMax=256M -p MemorySwapMax=0 \
  python3 native-portal/live/production_admission_test.py
# Repeat with --sanitize, then the unchanged camera/source-command matrices.
python3 native-portal/live/camera_runtime_test.py --matrix
python3 native-portal/live/source_command_test.py
systemd-run --user --scope -p MemoryMax=256M -p MemorySwapMax=0 \
  python3 native-portal/live/source_command_native.py
FT_SOURCE_NATIVE="$PWD/native-portal/build/source-command/source-native.bin" \
  systemd-run --user --scope -p MemoryMax=256M -p MemorySwapMax=0 \
  native-portal/build/camera-runtime/production-admission cold
```

The loader witness requires OpenSSL development headers/linker input. On this host the existing extracted SDK under the source-command worktree was reused read-only through `CPATH`/`LIBRARY_PATH`; no host packages were installed. Generated private vectors and logs stay in ignored build directories.

## Remaining validation and inherited assumptions

This lane did not run an ARM64 production build, install/activate a compositor, access hardware or SSH. Parent owns the sealed ARM64 build and live cold/warm On/Off acceptance, native call reachability, camera-visible output and normal shutdown.

Producer/ICP visibility and native source-ring reuse are inherited assumptions of the unchanged Valve path, not new proof from successful Connect, metadata return, VSYNC, source barriers or these CPU tests. An unobserved prior stock acquire, aliased wrappers lacking complete attribution, split source commands, reimport/repair and additional or deferred readers remain outside this supported contract.

Primary caller reconciliation is `NATIVE-SOURCE-RECONCILIATION.md`, SHA256 `40cbd518309f6ab06fbd0bbcc64a1c57107bc36f4280d78c47a3f68df6093a11`. No blanket native ownership or lifetime gate was removed.
