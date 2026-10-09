# Bootstrap command-wrapper turnover

The startup rejection in trial 07 happened before Run. A valid renderer and its
initial published recording must survive stock dispatch turnover without gaining
owned draw or GPU settlement authority.

## Pinned instruction facts

`bootstrap_native.py` verifies the compositor SHA-256 and executes:

- `140d98 -> 13c3e8`, with the native context argument and return `140da0`.
- `140da0 -> 80018`, the zero `app+60` continuation to `140dbc`, and the nonzero
  conditional tail to `6d9a8`.
- Concrete virtual calls through this renderer's `+250` slot. Their incoming
  dispatcher return PCs are `6caf0`, `9e6e0`, `15a9bc`, `15ad64`, `294d10`,
  `294e58`, and `2955a8`.
- The dispatch submit-wrapper chain through `3054c8` to `3018b0`, whose incoming
  LR is `2a21f0`. Resource marking and finalization are opaque CPU boundaries.
- The actual empty-vector transfer, `R+1380` clear, wrapper release, nested
  executable pool return, pre-begun pool pop, stack-output clear and publication.
  Release LR is `2a23b4`, not the old fixture's `2a2338`. Nested pool-return LR is
  `3052f0`, at the dispatch SP minus `0x30`.

The captured first-invalidation PC does **not** contain the dispatch caller LR.
This test does not claim to identify which of those callers ran in trial 07.
The configuration routine and external provider behind `app+60` are not replayed
as if they were known native/GPU successes. Native instruction execution over
synthetic objects and modeled router callbacks remain separate evidence.

## Admission contract

One bounded turnover record binds dispatch SP, renderer-owned command, wrapper,
VkCommandBuffer, issued recording generation and native pool. Transfer entry and
post must pair before the exact wrapper release. The wrapper read role is then
revoked. The nested pool return must match the captured command/pool/caller/SP;
publication requires an independently observed successful Begin for the selected
pool member. The old submitted generation cannot be republished unchanged.

Before Run, this applies only without owned resources, staging, native call or
Lifecycle debt. It grants no graph borrow or queue/completion certificate. Run
handoff refuses an unfinished turnover. Unsupported pre-Run reset/Begin latches
Evidence invalidation. Existing owned disposal/reset/completion requirements
remain in force after Run; bootstrap handling cannot settle owned work.

## Verification

Run `live/runtime_test.py --sanitize` and `runtime/evidence_test.py --sanitize`
using absolute paths in a user systemd scope with `MemoryMax=650M` and
`MemorySwapMax=0`. Supply the pinned ELF through `FT_HOOK_TEST_ELF` and the local
Unicorn/pyelftools paths through `PYTHONPATH`.

The regression failed against the original production code before the repair.
The repaired production router passes initialization, bootstrap submit observation
without an owned certificate, disposal/pool publication, actual second-thread Run
handoff, stereo staging/encoding, exact queue/wait, pending Release, disposal/reset
and a second stereo queue. Direct Evidence tests reject changed owner, caller,
command handle, SP, pool, missing/duplicate events, old-generation reuse and owned
bootstrap resources. Existing startup diagnostics remain enabled and tested.

No device was accessed and nothing was activated. Live stereo is not established
by these CPU tests.
