# Settlement of a fully recorded rejected stereo pair

This extends the combined binding contract. It does not relax the committed stereo contract or authorize early cold-pass materialization. No runtime provider, installed hooks, capability publication or deployment is added.

## The failure

The combined production-chain regression originally recorded both uploads and packets, rejected a dispatch containing a cold stock pass, and correctly skipped both owned packets. Stock rendering continued. The command certificate required two backend admissions, so the uploaded resources could never obtain a queue token through Adapter. Lifecycle also required both backend returns before observing disposal. Even an actual successful stock queue, GPU wait, packet destruction and command reset could not reclaim the two tickets.

The first regression was run before the production change. It failed at `CombinedNative::queue`, at the production `Lifecycle::QueueEnter` assertion, after both actual owned skips. This was a behavior failure, not a compile failure.

## A distinct rejection transaction

The original pre-encoding path still requires both ready eyes, both pipeline resolutions, one commit and both backend admissions. After commit, neither Off nor a later per-eye readiness check can discard the second eye. A committed pair or a pair with one encoded eye cannot enter rejection settlement.

The new path requires all of the following:

1. Both uploads and both packet enqueues completed through Adapter and the outer recording returned. Partial uploads, failed descriptor capture and an unrecorded eye are not covered.
2. The bounded production pass scanner completed at dispatch entry. It found exactly two distinct owned packets, with valid labels, supported packet layouts and live recording ownership. It found no optimizer or other unclassified pre-dispatch mutation. A cold cache is the only readiness failure that can still produce `suppression_scan`. Warm scans may also supply this proof when pipeline preparation fails.
3. PassGate retains the original entry snapshot, not a later scan that might conceal an initially incomplete scan. Adapter authenticates the pair against its immutable tickets, prefixes, copied owned descriptors and the exact command that registered the uploads. The pass and packet addresses are fixed. Arming rejection does not mark either packet skipped.
4. At each actual owned packet gate, PassGate scans the bounded dispatch again and matches the current packet and pass. Adapter checks the exact prefix, label and all nine copied descriptors against the saved snapshot. Each eye must take this DROP path exactly once. Neither backend has encoded an owned draw.
5. Only after both actual skips does the default-null `Callbacks::rejected` binding receive the exact upload command and both full provenances. The integration must call `Command::rejected` synchronously in that command's native owner scope. Command rejects foreign ownership, different recording generations, mismatched pair provenance, invalid buffer/range descriptors, earlier backend admission and duplicate registration. It stores the pair once; it does not mark backend admission or return bits.

All of this uses fixed Adapter/Command storage and bounded scans. A missing callback, missing skip, malformed ticket, changed descriptor, stale lifetime, duplicate skip or incomplete/unclassified entry scan retains debt. Marker-based stock routing is unchanged. A ticket-looking stock label cannot authorize its suppression.

## Rejection is not completion

Native image barriers and registered resource references can still use the command and arena without an owned draw. Stock work and prepared signals must still run. A rejection callback is never permission to cancel native queue execution or manufacture a successful return.

The rejected pair needs the same downstream facts as a committed pair:

- The actual native queue call with the exact registered command, VkCommandBuffer, device, queue, synchronization resource, semaphore and timeline value.
- A separately observed successful raw queue result, then both exact resource post-stamps and the command post-stamp.
- The paired native wait entry, successful raw result and true native return for that immutable token.
- The complete current-to-retained transfer of both tickets, the complete retained disposal, and successful reset of the old executable command generation.

Lifecycle accepts either two backend returns or the explicit no-encoding rejection certificate as the source of the immutable pair. Its vector scans, transfer/destruction pairing, exact GPU guard, reset result and generation issuance are unchanged. A failed reset keeps the debt despite empty CPU vectors. A later successful reset can settle it after Off. Failed begin does not undo an already successful old reset, and only successful begin issues the next generation.

`Adapter::submitted()` observes and saves the rejected command's exact token but returns false. `queue_observed` records native lifetime evidence; `cpu_done` and `stereo_submitted` remain false, and FrameGate does not advance `submitted_frame`. Neither `gpu_complete` nor `cpu_settled` becomes true merely because a pair was rejected. GPU completion alone cannot reclaim tickets or clear pending Off. Repeated or unknown-generation queue observations retain debt and cannot replace the saved token.

After exact completion and lifecycle settlement, `reclaim()` frees the two bounded ticket entries and clears render-pending debt. It does not reset arena reservation accounting. The separate native cursor-reset observation still does that. If existing demand and lease intent remain valid, the next recording may retry normally without a new On. If the stock helper has made its pass warm, ordinary two-pipeline preparation and commit may succeed.

## Stock pass ordering

PassGate never calls the cold helper early. During a rejected dispatch, stock passes keep their native materialization, barrier, rendering and queue positions. Cold stock may appear before, between or after the owned passes. Cache values may consequently become warm between skip observations. Suppression authentication compares immutable packet/pass identity, not a pipeline lookup result that was unavailable in the cold snapshot.

The combined CPU fixture models the native cache update at the stock pass position. It does not manufacture a full-camera draw to warm a pass. Separate pinned instruction tests execute the native cold helper and observe its render-pass/framebuffer creation and image-barrier boundary. Those tests are evidence for the native side effect, not GPU execution.

## Verification

The combined runner links production Upstream Bridge, Adapter, PassGate, FrameGate, Command and Lifecycle. Its settlement callbacks forward to Command/Lifecycle; the inherited synthetic `settled_ok` Boolean remains false.

Coverage includes:

- The original cold-stock refusal through actual queue, wait, disposal, reset and reclamation.
- Cold stock before, between and after owned passes, both recording orders, and private-socket Release between the actual skips. Repeated Release stays Draining until exact settlement, then reports Off.
- Either pipeline preparation failure, including second-eye failure, with zero owned backend admissions.
- Failed or missing queue results, missing GPU or CPU proof, failed reset and successful reset retry, repeated submission, unknown command generations and immutable-token preservation.
- Missing, malformed and duplicate skip observations; changed owned and other copied descriptors; lost lifetime; initially incomplete or post-preparation unclassified scans that later look valid; absent rejection binding; one encoded eye; foreign command/pair provenance; and a partially recorded pair. These retain debt.
- 2000 alternating rejected and warm frames through one persistent Adapter. Rejections alternate between cold stock and failed second-pipeline preparation. Every next warm frame reuses the existing valid intent. Every frame requires real Command/Lifecycle proof; rejected frames leave `submitted_frame` unchanged. Resident tickets never exceed two, one wrapper is allocated, and the hot C++ allocation count stays zero.
- The existing 2000 fully committed frame loop and committed stereo ordering/Off regressions.

The separate upstream machine run also passes its 14 arena instruction tests, two preservation tests and seven upstream instruction tests without changing those fixtures.

The final verification runs the combined suite normally and with ASan/UBSan, sanitized upstream/render regressions with 882 pinned metadata records, sanitized Command/Lifecycle and control suites, sanitized pass tests, 23 pass instruction cases, 12 generated pass-island continuation/preservation cases and six lifecycle instruction tests. These run serially under `MemoryMax=650M` and `MemorySwapMax=0`. Full commands, exits and output are retained in ignored `native-portal/integration/build/rejected-verification.json` and its adjacent logs.

Reproduce the focused suite from the checkout:

```sh
systemd-run --user --wait --pipe \
  -p MemoryMax=650M -p MemorySwapMax=0 \
  --setenv=TMPDIR=/home/jakeg/.hermes/cache/scratch \
  python3 -B "$PWD/native-portal/integration/combined_test.py" --sanitize
```

## Remaining limits

Partial upload, failed immediate descriptor capture, unrecorded pairs, one-eye encoding, unclassified mutations and scans beyond the fixed bounds are still unsupported retained debt. This extension does not invent a broad cancellation or retry policy for them. Live integration must preserve serialized dispatch ownership, native resource/cache lifetimes, all packet aliases and the exact callback scopes; it must forward native stock queue work even when an observation is refused.

The combined fixture uses synthetic native memory transitions, Vulkan results and scope coordinates. Separate pinned instruction tests establish selective packet skipping, helper effects, unchanged native common-queue forwarding and actual CPU disposal/reset behavior. They do not make the combined fixture a whole-compositor emulator. No live GPU execution, headset pixels, finite Off latency or installed runtime activation is claimed. No arena or instruction-fixture implementation was changed in this work.
