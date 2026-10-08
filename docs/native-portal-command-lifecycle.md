# Native command lifecycle evidence

`native-portal/binding/lifecycle.{hpp,cpp}` adds an inactive, fixed-storage lifetime observer beside `binding::Command`. It can supply the real `render::Callbacks::settled` signature. It returns false until the exact successful submission has completed, both native packet-vector dispositions have been observed, and a successful reset has invalidated that old executable command generation.

There is no installer, preload constructor, live capability publication, Vulkan call, reset request or renderer change. Production callbacks remain unconnected. The only existing-source change is a friend declaration in `Command` so the lifecycle observer can check its immutable owner, tickets, completion certificate and thread ownership without inventing another submission authority.

This document supplements `native-portal-binding.md`. That document describes the original null-settlement binding. Its historical test results do not establish the new lifecycle facts.

## Pinned capture

The offline preparer refuses any ELF other than SHA256 `9b1bd5fe4463592cbc71b2517b9ceb0a0fcd66cd6885a19dca4be0784bf6c265`. The private ELF and generated instruction bytes stay outside version control. Reaudit every site after a SteamVR update before considering activation.

| Site | Saved register contract and effect |
|---|---|
| `0x2a2230`, CurrentEnter | x19 is R. Scan both complete native pass vectors before current-to-retained transfer. Require both exact ticket labels and the immutable marker in current, neither in retained. |
| `0x2a2320`, CurrentPost | x19 is the same R; entry SP must match. Require empty current and both exact tickets in retained. The empty-retained route swaps vectors at `0x2a2700`; it does not free the packets here. |
| `0x2987d8`, RetainedEnter | x22 is R, after the actual cursor store at `0x2987d4`. Capture retained-vector base and require both exact tickets. The cursor store itself establishes no settlement. |
| `0x298840`, RetainedPost | x22 and entry SP must match. Require the same retained-vector base with its end cleared, and no owned tickets in current. Native traversal has destroyed packet names and released packet buffers before this point. |
| `0x300b20`, ResetEnter | x0 is old C. Read actual C+0x738 through the checked reader. Capture the immutable old generation, SP and LR. Require the exact Command GPU-completion certificate already present. |
| `0x300c54`, ResetResult | x19 is C; SP is reset-entry SP minus 0x20. Raw w0 zero invalidates old executable contents. Nonzero does not, even though all native lists have already been cleared. |
| `0x300a08`, BeginEnter | Independently capture x0, not x19. Reset restores x19 and tail-branches here. Entry SP/LR must equal the captured reset entry. |
| `0x300a38`, BeginResult | SP is begin-entry SP minus 0x30. Raw w0 zero issues a new never-reused command generation. A failed begin leaves old reset invalidation valid but issues no new generation. |
| `0x3018b0`, QueueEnter | x1 is C before the native stack decrement, EndCommandBuffer, signal preparation and queue call. Match its actual handle and exact generation. A second or unknown owned-generation observation permanently marks unsupported debt. |

`lifecycle_emit.c` uses the existing `hooks/emit.h` whole-context save/restore implementation unchanged. The current-post displaced ADRP needs a relocated absolute target, which this emitter supplies after restoring registers. The register-preservation test first failed on x26 with an unrelocated ADRP, then passed after the correction. Result sites preserve both native CBNZ outcomes. All observation islands continue the original instruction path, including unsupported cases. They contain no production BRK recovery or proposed early cancellation return. The offline emitter currently prepares its fixed zero-based Unicorn mapping, not an ASLR-aware production installer.

The preparer enumerates and verifies the six direct predecessors of the common queue routine: `0x297924`, `0x297c6c`, `0x29a4f0`, `0x29b040`, `0x29cb9c`, and tail `0x30551c`. It also verifies reset's vtable relocation `0x59f918`. This does not prove coverage of arbitrary indirect or foreign Vulkan callers.

## Ownership and settlement

Construct one `Generations` issuer for the renderer lifetime and keep it across native C/handle address reuse. It issues increasing identities, refuses wraparound and refuses a second claim of an already-bound generation. Do not recreate it to clear debt. The lifecycle record has no clear, reset, rebind or recycle operation. A successor uses a distinct Command and lifecycle record after external retention rules permit replacing the old record. Initial generation attachment remains the native owner's obligation; pointer values and frame numbers are not generation identities.

Capture calls are synchronous on the Command's established process and owner thread. `tid` identifies the native capture thread; the capture's generation is the immutable bound generation, not a fresh lookup through mutable `R+0x1380`. No API accepts a delayed asynchronous event replay as authenticated evidence. The checked reader must provide coherent, nonblocking reads under valid native ownership. No live reader is installed by this lane.

The scanner walks real pass bounds at R+0x15a0/+0x15a8 and R+0x15b8/+0x15c0, stride 0x98, and packet bounds at pass+0x50/+0x58, stride 0x2b0. It checks marker at packet+8 and the full 15-character FTP ticket through native string pointer/length at +0x160/+0x168. Tickets are those already authenticated and admitted by Command's two backend scopes. Missing, duplicated, truncated, unreadable or wrong-marker tickets refuse the proof. Scanning is bounded to 64 passes and 128 total packets per vector; exceeding either cap retains debt and refuses settlement. A post-point alone, recognizable-label absence, empty resource lists or changed active command is insufficient.

The native current transfer is verified through three instruction paths: empty-retained vector swap, nonempty retained append with spare capacity, and nonempty retained append requiring allocation. The latter two run the actual native move/copy helper. Synthetic allocator and delete boundaries supply memory and record releases; the native traversal, inline labels, transfer, cleanup loops and end-pointer stores execute from the pinned ELF. Interleaved stock packets remain on the same native paths. This establishes CPU reference disposition for the supported, authenticated current/retained containers. It does not prove that an unobserved foreign native subsystem holds no additional packet aliases.

`settled` also calls the existing Command submission/completion predicates with both complete provenances and the exact immutable Completion. Wrong frame, token, command, generation, descriptor, device lifetime or ticket cannot borrow these facts. Command resubmission debt and lifecycle unsupported debt both prevent settlement. Invalid native ownership prevents publication. Off and expiry do not erase already-observed queue success, completion or disposal debt.

Successful reset, not begin success, ends the old executable generation. Failed reset preserves debt even after both CPU lists are empty. Successful reset followed by failed begin can settle the old generation once both CPU dispositions and exact completion exist, while `next_generation` stays zero. These are separate facts, not one success Boolean from the reset wrapper.

The integration exercises all six valid orderings of current disposition, retained disposition, exact GPU completion and successful reset, subject to current preceding retained and completion preceding reset. Missing facts remain pending. Native reset itself has no exact GPU wait in this function. An observed reset before the saved exact completion marks the ledger unsupported permanently, even if a later wait succeeds. The observer does not prevent that native reset from running. Activation would still need a justified native schedule or stock-preserving guard, not a claim that a refused observation cancelled work.

## Adapter integration and limits

The C++ fixture uses actual FrameGate, Command, Adapter and injected native functions. It executes enqueue, stereo prepare/commit, both deferred backends, native submission certificate, exact completion, Off and `Adapter::reclaim`. The lifecycle callback clears real pending debt and resident tickets only after the complete proof. It is not a fake `settled=true` callback. Off before queue observation still preserves the successful token and permits eventual settlement without publishing submitted status.

The accepted contract is committed Off with potentially unbounded settlement delay. No deadline cancellation or Off-to-last-visible latency bound is claimed. Unknown or repeated submission is unsupported, not permission to suppress a mixed command or its signal preparation. A refusal never authorizes fabricated Vulkan results.

Still outside this lane:

- Live coherent memory reader, native hook installation and exhaustive callback delivery.
- Live attachment of owned portal labels and command/recording generations. The historical six live03 chains contain stock camera records and no reset/disposal observations.
- A production disposition for unsupported resubmission and proof of reset ordering on every native schedule.
- Native cache/device teardown and all external packet aliases. Neither successful old-generation settlement nor a CPU fixture authorizes freeing the persistent marker or wrapper.
- GPU execution, pixels, headset visibility, finite shutdown timing and runtime activation.

A missing required entry/post, malformed scope, partial pair, queue failure, failed reset or unknown generation keeps the callback false. Do not enable capability from these CPU results alone.

## Initial runtime attachment

The inactive `runtime/Evidence` now captures initial native command creation, raw begin result and wrapper publication and owns a persistent `Generations` issuer. Its `bind_lifecycle` connects that issuer to the actual Lifecycle after exact Owner/observation checks. Pending pool members do not consume active identities; publication chooses the successfully begun member. Reset or an unmatched begin revokes observation before native mutation.

This reader still cannot supply current/retained graph lifetime. Its suite verifies initial binding and premature-reset refusal, not a successful successor or full settlement through real native-owned pass buffers. The successful recycling fixtures in this document still use their separately labeled synthetic memory reader. See [runtime evidence](native-portal-runtime-evidence.md).

## Verification

Run from this checkout with absolute script paths:

```sh
systemd-run --user --wait --pipe \
  -p MemoryMax=650M -p MemorySwapMax=0 \
  python3 -B "$PWD/native-portal/binding/lifecycle_test.py"

# Repeat with --sanitize for ASan/UBSan on the actual C++ integration.
```

The runner verifies cgroup memory.max 681574400 and memory.swap.max 0, runs the unchanged Command/Adapter regression suite, compiles the lifecycle integration with warnings as errors, checks the pinned ELF and direct routes, and runs six Unicorn test methods. The ASan/UBSan option instruments the Command/Adapter/lifecycle C++ integration; the Python/Unicorn C ABI test library remains uninstrumented.

Instruction coverage includes raw reset results 0, -4 and positive nonzero, successful reset with failed begin, both CPU transfer families, delayed CPU disposal, premature reset, repeat queue observation without suppression, and x0..x30, q0..q31, SP, NZCV, FPCR and FPSR preservation against original instructions at every island. Vulkan, allocator and delete boundaries are explicitly synthetic. The tests assert that failed reset empties lists without invalidating executable debt, rather than granting settlement from those lists.

The tested x86-64 lifecycle record is 248 bytes and its shared generation issuer is 32 bytes. The measured disposal/reset/settlement path makes zero C++ heap allocations. Final normal and sanitizer runs both exited zero under the enforced 650M RAM and zero-swap limits; their full outputs and measured peaks are retained below.

`native-portal/binding/lifecycle-verification.log` contains actual normal and sanitizer runner output. Ignored `binding/build/` also retains the initial missing-lifecycle RED, ADRP preservation RED, nonempty-disposal RED, generation-reclaim RED, generated private pins and emitted code. No SSH, SteamVR, native device or camera was opened.
