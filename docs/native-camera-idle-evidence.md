# Native camera idle evidence

Offline only. Pinned ELF SHA-256 `9b1bd5fe4463592cbc71b2517b9ceb0a0fcd66cd6885a19dca4be0784bf6c265`.

## Trial 19 does not identify an external owner

The retained journal has six sampled `ft-native-live` records. At frame 3, idle and can_acquire were true, but tracking was false and policy failed predicate 11. At frame 164 idle and can_acquire were false. Frame 207 had physical tracking but failed policy predicate 9. At frame 1053 tracking and policy were both true, while idle, can_acquire, adapter and capable remained false. No sampled record proves simultaneous lease readiness. The samples do not identify which camera-idle scalar failed. Neither On/warming nor a nonzero lease epoch proves native camera acquisition or portal submission.

## Pinned native lifecycle

`live/camera_idle_native.py` executes the actual `15f9e0` acquire, `15f938` shared demand setter, `15fba8` release and `15faf8` shared demand clear instructions. Shared-lock access and queue-service calls are explicitly synthetic CPU boundaries. The cold object starts as zeroed fixture storage. No camera, runtime or GPU is opened.

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
