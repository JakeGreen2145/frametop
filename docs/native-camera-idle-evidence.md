# Native camera idle evidence

Offline only. Pinned ELF SHA-256 `9b1bd5fe4463592cbc71b2517b9ceb0a0fcd66cd6885a19dca4be0784bf6c265`.

## Trial 19 does not identify an external owner

The retained journal has six sampled `ft-native-live` records. At frame 3, idle and can_acquire were true, but tracking was false and policy failed predicate 11. At frame 164 idle and can_acquire were false. Frame 207 had physical tracking but failed policy predicate 9. At frame 1053 tracking and policy were both true, while idle, can_acquire, adapter and capable remained false. No sampled record proves simultaneous lease readiness. The samples do not identify which camera-idle scalar failed. Neither On/warming nor a nonzero lease epoch proves native camera acquisition or portal submission.

## Pinned native lifecycle

`live/camera_idle_native.py` executes the actual `15ddc8` constructor, `15f9e0` acquire, `15f938` shared demand setter, `15fba8` release, `15faf8` shared demand clear and `15df70` destructor instructions. Shared-lock access, queue-service calls, libc memset and empty bookkeeping-container disposal are explicitly synthetic CPU boundaries. No camera, runtime or GPU is opened.

| Camera offset | Pinned meaning |
| --- | --- |
| `+349`, array byte 9 | Return byte from augmented passthrough geometry update `15e300`, stored at `160138`; release clears it. Not a reference count. |
| `+34a`, array byte 10 | Result of shared RoomView demand setter `15f938`, stored by acquire at `15f9f8`; release clears it at `15fbc0`. |
| `+350`, array bytes 16..23 | Output identifier for `/xrservice/roomview3d/meshData` connection. |
| `+358`, array bytes 24..31 | Output identifier for `/xrservice/roomview3d/meshLayout` connection. |
| `+360`, array byte 32 | Both depth-mesh queue connections succeeded. Set at `15fab8`, cleared only after both successful disconnects at `15fc48`. |
| `application+2568c` | Cached native layer activity at the existing comparison/store boundary. It is separate from queue connection state and is not a camera reference count. |

Acquire returns boolean success only when the shared demand result and connected flag are nonzero. Repeated successful acquire with queues already connected makes no further service calls. Release passes queue identifiers **by value**, never clears their slots, and returns boolean success when demand and connected flag are clear. Repeated release with connected flag zero makes no service calls. The inspected wrapper therefore has boolean/latch behavior, not a per-call reference count. Internal service reference accounting is not established by this witness.

The native sequence produces these byte/identifier tuples `[349,34a,360,350,358]`:

- Cold: `[0,0,0,0,0]`.
- Acquired: `[0,1,1,0xa001,0xa002]`.
- Successfully released, warm: `[0,0,0,0xa001,0xa002]`.
- Failed disconnect: `[0,0,1,0xa001,0xa002]`.

The current all-zero-handle guard rejects the successful released state. A production-router test reproduced that refusal. A provisional removal passed the full modeled stereo/settlement chain, but **was not retained**: the native acquire error branch supplies a counterexample. With shared demand unavailable, first queue connection failure and second queue connection success, the method leaves connected flag zero, returns failure and preserves the same nonzero identifiers. A later release returns success without disconnecting anything because connected is zero. This state aliases the released-warm tuple. Removing the handle guard based only on the three bytes would erase unknown partial/foreign ownership.

No full RoomView, safety override, external-call continuation, acquisition or release policy is changed. A safe future warm-state admission needs authenticated successful transition provenance or a separately proved queue-service ownership contract, not merely readable identifiers. The live refusal cannot yet be attributed to retained handles.

## Warm-transition admission remains blocked on camera lifetime

A successful connected Acquire followed by a successful disconnect is necessary, but the existing owner proof cannot bind that pair to a camera-object generation. This task did **not** enable warm recovery. The production cold-only predicate, pending-render guard, demand settlement, forwarding and diagnostic code are unchanged.

The precise missing facts are:

- `Evidence::identity()` issues renderer and device generations. `application_owner()` authenticates worker, context, scheduler and application at Run. Neither issues or revokes a camera-object epoch.
- The manifest does not observe camera constructor `15ddc8`, publication `8dc18`, destructor `15df70`, or resource cleanup `15e1c8`. The pinned publication stores the constructor's object at application `+25320`. The inspected destructor callers at `8df10` and `8e4e8` subsequently pass that object and size `8d0` to sized delete `4b3d0`.
- The existing `4b3d0` observer only updates known command membership in `runtime/recording.cpp`. Passing the camera to this event does not invalidate Evidence. The new router test verifies that fact; a generic delete hook's presence is not camera lifetime coverage.
- The actual pinned destructor preserves the warm tuple `[0,0,0,0xa001,0xa002]` and performs no queue-service calls in the fixture. Running the actual constructor again at that same address resets the tuple to cold. These are mapped synthetic-storage observations, not permission to inspect a destroyed native object and not a claim that concurrent destruction occurred in trial 19.

Consequently, matching camera address, renderer/device generations, Run owner, identifiers and successful return values cannot alone invalidate a certificate on camera destruction or reconstruction. A counter invented at seal or Acquire would count observations, not native object generations. The next implementation needs authenticated publication/construction and pre-destruction/reconstruction revocation, including foreign threads, or a separately proved invariant excluding those events throughout the tracked interval. That proof must precede warm admission. No camera fields should be cleared to manufacture it.

## Lifecycle replay and unmet acceptance probe

The witness exports all eleven observed method calls with exact before/after tuples, native return values and service-call lists. The runner generates ignored `camera.private.hpp` from those observations. The real router replays them through its existing native-call fake. The ARM64 methods execute in the witness, not in the C++ router process; application storage, native method invocation and Vulkan remain modeled there.

Four router modes cover the full call sequence, untouched foreign continuations, camera deletion after successful disconnect, and Release without any observed Acquire. The full sequence includes repeated Acquire/Release, failed disconnect, retry, shared-demand failure with a partial-connect alias, Release-success without disconnect, and a later genuine connected pair. Each forwarded result is preserved and each native-call fake runs exactly once. All warm states remain unsupported under the current lifetime contract.

The positive recovery requirement is an explicit reproducible failing probe, not a claimed passing feature:

```
TMPDIR=/home/jakeg/.hermes/cache/scratch FT_TEST_REQUIRE_WARM=1 \
  native-portal/live/build/runtime/runtime_chain_test --camera-native-lifecycle
```

Run that probe inside the same 650M/zero-swap scope as the suite, with core dumps disabled. It reaches `warm-recovery-required` immediately after the first proved connected Acquire and disconnect, then fails the `capable` assertion at the next ordinary seal. This distinguishes a missing recovery feature from partial-connect refusal. It is intentionally not enabled by the normal test runner.

Normal and ASan/UBSan full runners each passed 269 records with MemoryMax=650M and MemorySwapMax=0. Their recorded peaks were 270413824 and 517373952 bytes. Evidence is in ignored `live/build/runtime/warm-blocker-normal.json`, `verification.json`, `warm-blocker-normal.log`, `warm-blocker-asan.log` and `warm-required-red.log`. The positive feature requirement remains unmet; these results verify the fail-closed behavior and existing contracts only.

## Bounded diagnostics delivered

The existing owner seal mailbox now captures the already-read cache, all three checked bytes and both queue identifiers. `camera_read=1` makes the bytes and identifiers valid. The original short-circuit read remains unchanged: a nonzero cache prevents the camera array read. Cache is valid only when association predicate 7 passed. `camera_flags` packs byte 9 in bits 0..7, byte 10 in bits 8..15 and byte 32 in bits 16..23. No new raw-memory permission or native read is added.

`idle_first` is the first frame satisfying the unchanged cold-only predicate. `ready_first_ns` and `ready_last_ns` are BOOTTIME timestamps for seals with capable backend, external-clear policy, physical tracking, no fault and new-lease readiness. `lease_first_ns` is the first lease observed by the existing frame gate, **not the time of an attempted or rejected Acquire request**. Compare readiness bounds with independently recorded user On/control timestamps. Zero first-readiness distinguishes never-observed readiness from a missed earlier window; sparse polling is not an exhaustive readiness timeline.

The collector still attempts at most 64 transition lines. Advancing last-readiness time alone does not emit a line. The fixed line buffer is 3072 bytes. The existing bounded 8192-row trace now selects native acquire/release entries on owner, handoff-wait and foreign threads, recording actual camera argument `x0`, method PC, LR and TID. A separately recorded owner-forwarded result has `result_valid=1` only when the existing native thunk actually returned. Foreign passthrough calls have no observed return and remain `result_valid=0`; their incoming x0 is never presented as a result. Overflow and sink-loss reporting remain in force. No callback formatting or I/O is introduced.

## Verification

The diagnostic tests were observed red for missing camera fields and missing acquire/release trace selection. The pinned witness covers warm release, repeated calls, failed disconnect and partial-connect alias. Router cases retain refusal for unknown warm handles and each activity byte independently. Existing tests cover the complete stereo/settlement path, owner/foreign external continuations, policy/safety refusal, 64-line cap, 8192-row overflow and saturated nonblocking collector sinks.

Run with `FT_HOOK_TEST_ELF` pointing to the pinned offline ELF and `PYTHONPATH` containing the pinned Unicorn, pyelftools and Capstone dependencies:

```
systemd-run --user --wait --pipe --working-directory=/absolute/repo \
  -p MemoryMax=650M -p MemorySwapMax=0 \
  env TMPDIR=/home/jakeg/.hermes/cache/scratch \
  FT_HOOK_TEST_ELF=/private/vrcompositor PYTHONPATH=/private/pylib \
  python3 -B native-portal/live/runtime_test.py
```

Final-source verification passed 265 normal runner records under MemoryMax=650M and MemorySwapMax=0, with peak memory 267620352 bytes. A fresh final-source ASan/UBSan chain build passed nine focused modes: unknown warm handles, each of the three activity bytes, owner and foreign external calls, good and bad diagnostics, and authenticated construct policy. The exact diagnostic assertions and the partial-connection alias witness were rerun successfully. Logs and generated evidence are under ignored `native-portal/live/build/runtime/`, notably `camera-final.log`, `camera-final-asan.json` and `camera-native.json`.

CPU results do not establish a working live portal. No headset access, restart or deployment was performed.
