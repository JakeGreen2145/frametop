# Native one-command certificate binding

`native-portal/binding/command.{hpp,cpp}` is an inactive C++17 observer for one retained stereo pair and one native command generation. It is not linked into a deployed renderer, installs no hooks, and publishes no portal capability. The existing render adapter, controls, pipeline and observer are unchanged.

## Contract

A fresh `binding::Command` has no owner and returns no certificate. A caller may bind it once, outside draw work, to exact process, renderer and device lifetime identities, recording and command generation, native C, VkCommandBuffer, arena resource, buffer, mapping, capacity, alignment, queue, synchronization resource S and semaphore. The future native owner must issue never-reused generation identities and establish their lifetimes. This library does not invent them from pointer values or frame numbers.

There is no reset, clear, recycle or CPU-settlement API. Reusing storage requires a fresh owner after external proof that the old lifetime is disposed. Destroying this observer is not that proof and must not be used to discard unresolved work.

Each event carries a copied owner and captured invocation identity, native thread, entry stack, entry return address and entry object. Result/post events carry the entry identity, not their current volatile register values. Parent queue and batch-stamp scopes remain explicit. The future integration must capture these facts at audited native boundaries. This observer does not authenticate arbitrary caller-supplied snapshots or read native memory itself.

### Backend and queue

1. Capture each exact immutable `render::Provenance` at the authenticated adapter pipeline callback, under the same native command generation. Pair the subsequent actual backend return with its entry scope and successful adapter admission. Both eyes must finish before a queue certificate is possible. Backend return is not CPU disposal.
2. At queue entry copy the supported VkSubmitInfo and timeline extension into `SubmitSnapshot`. It requires readable, supported data, submit count one, command count one, the owned VkCommandBuffer, signal count one, matching timeline-value count, exact device/queue/semaphore and native packed timeline value, with no fence. The future reader must bounds-check the actual arrays and pNext chain before declaring them supported. No pointer is dereferenced in the binding.
3. Observe raw VkResult separately. Only zero permits certification. A prepared native S stamp cannot replace this result. Once a queue attempt has been retained, even a malformed result scope with raw zero leaves conservative success debt. Invalid payload or later stamp failure cannot erase that debt.
4. Pair the successful batch stamp to that queue invocation. Require actual post-stamps on both owned arena A and native command C, with exact S, serial and signed subserial, followed by the matching batch post-state. Another resource stamped with the same token is insufficient.
5. Expose the immutable certificate through `submission` or its callback-compatible static thunk. Both complete ticket provenances must match, including descriptor snapshots. Output is untouched on failure. Frame comes from the retained adapter provenance, never from a serial heuristic.

`timeline_value` reproduces unsigned serial shift and signed subserial extension before OR. The synthetic sign-boundary tests are separate from live03, whose six camera chains all have subserial one.

### Native wait observation

`wait_enter`, `wait_result` and `wait_return` retain one bounded wait transaction. Only exact captured scope/token, owned semaphore, raw VkResult zero and native Boolean one yield the GPU certificate. Other resources, other values, partial triples, nonzero results and unstamped fast returns do not. Failed observations retain storage; this narrow observer is not a general retry or wait registry.

`completed` and its static thunk only read the saved certificate under the still-valid owner. They never call Vulkan, wait, allocate, query mutable current command state, log, or publish authorization. The owner must call `invalidate_lifetime` before native lifetime revocation. Off stops new backend admission but does not discard a real queue observation for an already-recorded pair, even when Off preceded queue entry. Lifetime revocation forbids publication while still retaining conservative raw-success debt from a previously captured attempt.

### Resubmission and retention

A second observed queue entry makes resubmission explicitly unsupported, refuses future admission and prevents the binding from certifying completion for the repeated execution. The original successful token remains immutable, and recording/success debt remains resident. This return value is an observer refusal, not permission to suppress vkQueueSubmit, report a fabricated failure, reset native storage or cancel a prepared native timeline. All native queue routes still require an audited external mediation and cancellation policy before activation.

An adapter may already have cached GPU completion before a repeated queue is observed. This observer cannot retract that cached fact. The permanently null settlement callback prevents reclamation regardless. Do not confuse that retention safeguard with safe cancellation or a production resubmission policy.

## Adapter integration

The real CPU fixture bridges a containing native test owner through `render::Callbacks::user` to the binding's callback-compatible `submission_callback` and `completed_callback`. The existing callback user pointer is shared by all native operations, so a real owner needs equally small forwarding thunks rather than replacing the other callbacks' user pointer with a `Command *`.

The fixture executes production `FrameGate`, `Adapter::latch`, stereo enqueue, deferred backend admission, `submitted`, `poll_completion`, `off` and `reclaim`. `Callbacks::settled` remains its default null. Both resident tickets survive exact GPU completion, Off, expiry, duplicate observations and bounded refusal loops. No authoritative CPU settlement is manufactured.

The fixed record is 1504 bytes on the tested x86-64 build. The complete CPU enqueue-through-completion path performs zero C++ heap allocations after initialization, measured with overridden allocation operators. There is no per-frame storage growth, socket, file output or global registry in production binding code. Scope nesting, off-thread calls and forked inherited owners are refused.

## Exact live03 fixtures and limits

The test runner takes the private completion analysis directory explicitly. It SHA256-pins `evidence.json`, `camera-chains.csv` and `records.jsonl`, cross-checks the six joined rows and captured result/entry identities, then generates only the required metadata into ignored `binding/build/private_chains.hpp`. No proprietary packet, shader, ELF or instruction bytes are copied into the repository.

Frames 601, 602, 603, 743, 744 and 745 use their exact observed native C/A/S, arena identity, semaphore, serial/subserial, backend scopes, queue/batch/resource stamp scopes and wait scopes. The runner independently checks raw queue result zero and wait result zero/Boolean one against the records. It verifies actual resource post-stamps before normalizing their equal token values. Sequence numbers identify fixture invocations, not a proposed live authenticator.

These are stock camera chains. Live03 did not record actual VkSubmitInfo payload bytes, VkCommandBuffer handles, device/process lifetime identities, command generations or portal tickets. Those fields and the actual modeled VkSubmitInfo/timeline input are explicitly synthetic in the CPU fixture. The copied adapter packets, geometry, native operation returns and workload authorization are synthetic too. The binding's predicate checks execute real C++ against those modeled inputs; no live submission-payload readback is claimed.

This branch uses structurally valid fixed synthetic pass bounds for the adapter integration because the sibling lane owns the pass-generation comparison repair. It does not replay the six growing live pass extents through this unchanged adapter. Generation-aware pass repair must be combined separately. No hooks, native capture, GPU execution, native memory readback, disposal/reset proof, workload headroom certification, label relocation proof or active portal integration was added here.

Before any production binding, repeat the native boundary audit against the installed SteamVR image after every update. Specifically verify backend generation attachment, actual queue payload, owned packet labels, both recorded-vector disposals, old-C/VkCommandBuffer reset/destruction, all queue/resubmission routes, safe cancellation, device/cache teardown and workload headroom. Arena-reset entry and empty native lists are not settlement proof.

## Verification

From this checkout, use absolute paths because a transient user service starts in the user home:

```sh
systemd-run --user --wait --pipe --unit=command-binding-test-unique \
  -p MemoryMax=650M -p MemorySwapMax=0 \
  python3 -B "$PWD/native-portal/binding/test.py" \
  --evidence /home/jakeg/.hermes/work/frame-native-live03-completion

# Same command with a fresh unit name and --sanitize for ASan/UBSan.
```

The runner verifies actual cgroup `memory.max=681574400` and `memory.swap.max=0`, compiles with C++17 and warnings as errors, runs the binding integration plus the unchanged render adapter suite, and prints source/input hashes and peak memory.

Executed evidence in ignored `native-portal/binding/build/`:

- `red3.log`: behavioral RED with declarations/no-op implementation, failed at owner binding before implementation.
- `green1.log`: initial GREEN for the complete queue/stamp/wait and adapter integration suite.
- `red-sync.log`: focused regression RED, a different S incorrectly passed queue entry when only the semaphore matched.
- `green-sync.log`: GREEN after pinning S to the immutable owner.
- `red-late.log` and `red-invalidated.log`: focused RED cases for Off before queue observation and revocation before raw-result observation. Both fixes preserve debt without authorizing work.
- `normal-final.log` and `sanitize-final.log`: final normal and ASan/UBSan runs, both exit zero. Peak memory was 173330432 and 222236672 bytes respectively.

Coverage includes both complete provenance mutations, owner/generation/native-handle/scope mutations, malformed modeled submit counts and arrays, exact command and arena post-state, signed subserials, queue/wait errors, missing Vulkan result, unrelated resource waits, token replacement, duplicate Off, expiry/Off before and during the adapter observer, missing settlement, unsupported resubmission, bounded failure retention, nested scopes, off-thread and fork refusal, and zero allocation checks. The original adapter R1 regressions also pass.

Independent review remains with the parent integration lane. No reviewer subagent or external coding CLI was available in this worker, so these runs are not labeled independent approval.
