# Command pool transition trace

The original trace diagnosed trial 10's `pub_pred=13`. Trial 11 then captured the missing identity and its first invalidation. The repair below preserves only that exact unpublished stock pool-return path. No device was accessed for the repair; live stereo after the repair remains unverified.

## Pinned native result

`bootstrap_native.py` executes the hash-pinned command initializer and a complete no-synchronization-owner pool cycle on synthetic objects:

- Initial allocation/Begin reaches `301808 -> 300a08 -> 300a38 -> 30180c`.
- `311648` appends the same C to the returned vector and sets its pool flag. C and its VkCommandBuffer survive. No command destruction is executed.
- The real `311818` vector loop calls the pool vtable's `2a9c88`. That function calls C's reset method with LR `2a9cdc`, executes `300b20 -> 300c54 -> 300a08 -> 300a38`, and appends C to the ready deque only on success.
- `304e60` pops that same C, then the real wrapper publication instructions publish it.
- Failed reset skips Begin and failed Begin prevents ready-deque insertion. In both failures the returned vector still clears. Empty native storage is not settlement.

Vulkan allocation/reset/Begin results, RTTI and debug annotation are explicit fake external boundaries. The test stops before the remaining synchronization-tree work. It does not certify GPU completion, synchronized pool reuse, concurrent ownership, or owned recording disposal.

This proves that the generic pool-return site is not executable destruction. The original ledger nevertheless removed a noncurrent member there. Trial 11 confirmed that loss, rather than an unseen creation or foreign-thread observation gap, for the command that later failed publication.

## Trial 11 repair

The private `live-stereo-11/command-cause.json` records a contiguous trace prefix 1 through 183 with no overflow, identity loss or sink loss. Later buffer overflow does not erase this complete prefix. For one exact C and unchanged VkCommandBuffer:

- Sequences 54 through 57 observe creation, paired successful Begin and ledger insertion on the bootstrap thread, with generation zero.
- Sequence 58 observes `311648` with LR `297cb4` and x0 equal to C's pool plus `d0`. The old observer erases that unpublished member, although native code returns it to the pool without destroying it.
- The authenticated worker handoff succeeds. Sequences 119 through 122 observe real successful reset/Begin on that same C, but the old ledger ignores them.
- Sequence 143 returns C through `297cb4` again. Sequences 170 through 173 repeat successful reset/Begin. Sequence 179 publishes the same C and handle, passes device and turnover checks, then fails predicate 13 because membership was lost at sequence 58.

The pinned witness now executes the actual caller at `297ca0`, including its stack-slot C load, C+50 pool lookup, +d0 adjustment, BL and complete native pool return. It derives LR `297cb4` independently of the C++ guard. It also verifies x19=C and x20=pool throughout the reset/Begin callbacks from the real pool recycler.

The repair retains only a known, never-published, generation-zero, fully begun member returned by that exact caller. It rechecks native D, nonnull bounded pool, exact x0=pool+d0 and unchanged VkCommandBuffer. Current and retained command roles and the Lifecycle-owned command cannot take this exception. Return clears Begin authorization, keeps generation zero and adds no general read role. Unknown commands remain unknown; delete and handle-clear still revoke membership.

On Run, renewal must observe the real reset entry/result and Begin entry/result with paired stack coordinates and raw Vulkan success. The retained stock pool, D, C, Vk handle, recycler LR and native x19/x20 association must still match. Failed or missing reset/Begin cannot authorize publication. Only the existing successful wrapper-publication path issues a generation. A persistent published bit prevents a later generation-zero reset state from masquerading as a never-published stock command. Owned completion, current/retained disposal, reset settlement and pending Release remain Lifecycle-controlled.

The production-chain fixture reconstructs these affected-command events across a real bootstrap-to-worker thread handoff and two pool recycles. It does not replay every unrelated journal event. Addresses, stacks, native memory writes and Vulkan boundaries are modeled; the caller and pool machinery are separately executed from the pinned ELF. After publication, the same fixture completes owned stereo, exact queue/wait, disposal/reset, pending Release and a second stereo queue. Trace assertions verify the precise affected-C sequence, unchanged membership and handle, Begin revocation/renewal, and generation issuance only at publication.

Negative fixtures cover wrong pool, caller LR, D and handle; sized delete and handle clear; absent or failed reset/Begin; wrong recycler LR; and the unchanged invalid-return unknown-C case. Separate current/retained owned-command attempts keep pending Release draining. The existing publication, handoff, queue and settlement tests remain enabled.

## Trace contract

The existing collector emits `ft-native-command` lines. Each line includes:

- Monotonic reservation sequence, callback PC, native LR, native SP, actual thread and router owner thread.
- Route: `owner`, `handoff-wait`, `foreign`, or `owner-event`.
- Native register arguments and the command identity resolved by an existing authorized read, when available.
- Before/after ledger membership, Vk handle, generation, renewal stage, Begin state, entry count, recording, creation, turnover phase and evidence flags.
- Event result and sticky first evidence failure.

Evidence flags are bits 0 through 6: pinned, constructed, ready, failed, Run, current transfer, Lifecycle present. `accepted=0` is the existing event return, not necessarily an invalidation; consult `failure` and the state transition.

`ledger_sampled=0` explicitly means no ledger was touched. Handoff-wait and foreign callbacks capture registers only. They cannot obtain read authority. For wrapper publication, C and Vk are copied from the existing short-circuit checks; tracing performs no additional native-memory read. A C value of zero at an earlier failed check means unresolved, not native null.

Generic pool returns and sized deletes are filtered to command addresses already observed by the trace or present in the ledger. Unknown Reset/Begin events are retained and enroll their register-supplied C for later deletion comparison. This filter is diagnostic identity comparison only. It never adds ledger membership or native read permissions.

The append-only buffer holds 1024 immutable records, with 128 remembered command identities. Concurrent writers reserve once and publish with release/acquire atomics. They never wait, allocate, format, open files or write output. The existing collector formats and emits through its nonblocking sink. Startup records wait for a seal, invalidation or collector teardown so their burst cannot precede the primary capability/refusal line.

`ft-native-command-summary` reports attempted/consumed records, capacity, overflow, identity overflow and lost sink writes. At most 1024 event lines and 64 summaries are attempted. No slot is overwritten or recycled. Sequence gaps or nonzero loss counters make the trace incomplete. Sink loss cannot be repaired, and abrupt process death before collection can lose records.

Raw native addresses are deliberately present for equality comparisons in private trial evidence. Keep those logs private. The exported Diagnostics ABI and existing public capability fields are unchanged.

## Interpreting a private trace

Find the failing `2a23f4` row and compare its `command` with earlier rows:

1. A successful `30180c` insertion followed by `311648` membership 1 to 0, later ignored Reset/Begin and failed publication identifies dropped pool membership.
2. Creation/Begin on `handoff-wait` or `foreign` identifies the exact ownership observation gap without treating it as authorized.
3. Reset/Begin with no preceding creation identifies an unseen creation path or a trace gap. Check loss counters before concluding.
4. `3003d0` or `4b3d0` before publication distinguishes genuine invalidation/address reuse from mere pool return.

Do not grant a generation from any of these diagnostics. A repair still needs exact successful Begin/reset and, for owned recordings, unchanged disposal, completion and retention-debt proofs.

## CPU verification

`live/runtime_test.py` now includes the trace tests and the native pool witness. The production router fixture demonstrates insertion, noncurrent pool removal, ignored reset/Begin and same-C/same-Vk publication refusal through actual collector output. This is a modeled failing schedule, not a historical trial replay. A separate real-thread fixture observes bootstrap handoff-wait and foreign events without accessing the owner's ledger.

The append-log test covers concurrent writers plus a reader, immutable first records, exact storage capacity and overflow behavior, identity overflow, unrelated-delete filtering and format bounds. The existing full worker chain, bad owner/caller/reset/Begin/publication cases, exact queue/wait and pending Release tests remain unchanged and pass. Normal and ASan/UBSan runs pass, including the actual saturated journal-socket tests. The Evidence ASan/UBSan suite and its pinned instruction/staging tests also pass.

Run locally with the pinned ELF and Python dependencies, never on a device:

```sh
systemd-run --user --scope -p MemoryMax=650M -p MemorySwapMax=0 \
  env PYTHONPATH=/home/jakeg/.hermes/work/frame-native-product-design/pylib:/home/jakeg/.hermes/work/frame-native-reuse/camera-analysis/pylib \
  FT_HOOK_TEST_ELF=/home/jakeg/.hermes/work/frame-native-reuse/camera-analysis/vrcompositor \
  TMPDIR=/home/jakeg/.hermes/cache/scratch \
  python3 native-portal/live/runtime_test.py --sanitize
```

Repair results are in ignored `native-portal/live/build/runtime/pool-return-{red,green,asan,evidence}.log`. The new successful-publication fixture first failed on the original code, then passed after the repair. `runtime_test.py` also links the complete production router DSO on the CPU host. An ARM64 production bundle was not built or activated in this task.
