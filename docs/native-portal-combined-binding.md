# Combined native binding reconciliation

This reconciliation keeps the upstream upload, warm-pass extraction, command certificate and lifecycle implementations unchanged. It installs nothing and does not publish capability.

## Link dependency repair

The combined candidate failed `binding/lifecycle_test.py --sanitize` while linking its prerequisite command test. `Adapter::upstream` requires `arena/bridge.cpp`; the independent binding runners had not added that source. The same omission affected the pass runner and the two shared libraries used for instruction tests.

All five owning build lists now link Bridge:

- `binding/test.py`, including its source-hash report
- `binding/lifecycle_test.py`
- `binding/lifecycle_prepare.py`
- `binding/pass_test.py`
- `binding/pass_machine.py`

The existing render and upstream runners already linked Bridge. The repository-wide Adapter-source search found no other owning build list needing repair. The retained `integration/build/missing-bridge-red.log` repeats the expected undefined references without Bridge; this is a link failure, not a production behavior regression. All three lane intents coexist. No production policy, marker authentication, stock continuation or queue/disposal rule was weakened.

## Combined production-chain regression

`native-portal/integration/combined_test.py` compiles `combined_test.cpp` against the actual Adapter, Bridge, PassGate extractor, Command, Lifecycle, FrameGate and private control transport. It derives an ignored header from the existing upstream native-operation fixture rather than copying its implementation. The new fixture replaces the old fixture's submission/completion/settlement callbacks with Command and Lifecycle forwarding callbacks. It never sets the inherited synthetic settlement Boolean.

Each recorded eye calls `reserve_upload`, the actual Bridge upload/admission/return path and `enqueue(ticket, eye)`. `legacy_test_upload` is false. The Bridge produces the authenticated descriptor result and the Adapter immediately resolves/copies its two descriptors. The recorded packets, including all descriptor snapshots and actual Adapter-issued labels, feed the production pass scanner. Actual warm cache entries and attachment formats in bounded synthetic native memory produce the lookup registers; no caller-authored `ready=true` or `pair_input` constructs the combined pair.

PassGate performs both pipeline preparations and the one-shot commit. Each deferred backend uses its extracted packet/pass/lookup and the saved provenance in an exact Command scope. Queue result, both resource post-stamps, exact wait entry/result/return, current-to-retained entry/post, retained destruction entry/post and old executable reset entry/result remain separate observations. Only that complete Lifecycle proof permits `Adapter::reclaim`.

Coverage:

- All four independent recording/encoding order combinations. Real private-socket Release between backend eyes reports Draining. The second committed eye still completes. Successful queue evidence survives Off without publishing submitted status. GPU-only and CPU-disposal-only progress remain Draining; successful old-command reset plus reclamation finally yields Off on repeated same-epoch Release.
- Seven retained-debt cases: failed queue result, unobserved queue result, missing GPU completion, missing CPU disposal, failed reset, repeated queue observation and unknown command-generation queue observation. No refused observation cancels native work or synthesizes queue success.
- A known cold stock-only pass refuses both owned packets before any pipeline lookup. The exact stock packet is not skipped and its Adapter stock forwarding still executes.
- Either pipeline failure admits neither eye and retains the already-created upload debt.
- 2000 consecutive fully settled frames through one persistent Adapter and Bridge. The loop observes 4000 upstream replacements, at most two resident tickets, one wrapper allocation and zero hot C++ allocations. It retains one generation issuer across command-address reuse and replaces Command/Lifecycle storage only after proved old-generation settlement. Queue/reclaim leave arena accounting at 1280; the separate native cursor-reset callback clears it for the next frame.

## Facts and limitations

The combined fixture's native memory, Vulkan handles/results, pipeline results, process/device identities, initial recording attachment and synchronous event scope coordinates are synthetic. Its portal labels come from the real Adapter and its command generations from the real persistent Generations issuer. These are not captured live03 portal submissions. The separate existing command suite still checks the six pinned historical queue/stamp/wait chains and labels its uncaptured fields synthetic.

The combined CPU fixture models the native vector swap and destruction memory transitions, then submits their exact entry/post observations to Lifecycle. It does not claim to execute those native instructions itself. The separately rerun lifecycle instruction suite executes the pinned swap, append, destruction and reset paths. The separately rerun upstream and pass instruction suites execute their own pinned boundaries. These remain separate evidence layers, not one whole-compositor emulator.

Lease Off and `Command::off` are different operations. Calling the latter between committed eyes would stop the second Command admission. The combined fixture therefore retains native command admission through both committed eyes and invokes Command::off afterward. A future live owner must preserve this scheduling contract; wiring Release directly to Command::off or lifetime invalidation is unsupported. No new live owner or callback registry is installed here.

Cold-pass materialization remains unsupported. Unknown/repeated submission remains conservative retained debt, not a cancellation policy. Live lifetime readers, all external packet aliases, cache/pipeline retention, exhaustive native callbacks and reset ordering still require integration proof. No GPU execution, headset pixels, optical alignment, finite Off latency or end-to-end screens-owner/native-compositor activation is established. The real screens-owner/GUI parser tests run independently in the control suite.

## Reproduce

From this checkout, run each command serially inside a user service with `MemoryMax=650M`, `MemorySwapMax=0` and short disk-backed TMPDIR:

```sh
systemd-run --user --wait --pipe \
  -p MemoryMax=650M -p MemorySwapMax=0 \
  --setenv=TMPDIR=/home/jakeg/.hermes/cache/scratch \
  python3 -B "$PWD/native-portal/integration/combined_test.py" --sanitize
```

Repeat without `--sanitize`. The complete reconciliation run also executes these commands under the same limits:

```sh
python3 -B native-portal/integration/upstream_test.py --trace /home/jakeg/.hermes/work/frame-native-live-evidence/observer-live-01/metadata.bin
python3 -B native-portal/integration/upstream_test.py --sanitize --trace /home/jakeg/.hermes/work/frame-native-live-evidence/observer-live-01/metadata.bin --machine
python3 -B native-portal/binding/lifecycle_test.py
python3 -B native-portal/binding/lifecycle_test.py --sanitize
python3 -B native-portal/binding/pass_test.py
python3 -B native-portal/binding/pass_test.py --sanitize
python3 -B native-portal/control/test.py
python3 -B native-portal/control/test.py --sanitize
python3 -B native-portal/binding/pass_machine.py --analysis /home/jakeg/.hermes/work/frame-native-live-render-analysis --elf /home/jakeg/.hermes/work/frame-native-reuse/camera-analysis/vrcompositor
python3 -B native-portal/binding/pass_island_test.py --analysis /home/jakeg/.hermes/work/frame-native-live-render-analysis --elf /home/jakeg/.hermes/work/frame-native-reuse/camera-analysis/vrcompositor
```

Use absolute script paths inside systemd services. Upstream runs the existing render suite, including 882 trace records and both 10000-frame capacity tests. Lifecycle runs the existing command suite and six pinned instruction test methods. Upstream machine mode runs 14 arena instruction tests, two preservation tests and seven upstream instruction tests. Pass machine mode reports 23 cases; its island suite reports 12 continuation/preservation cases. Control reports four native test executables and 11 actual owner replies accepted by the GUI/CLI schema. The C++ tests run normal and ASan/UBSan builds; Python/Unicorn shared instruction-test libraries remain uninstrumented.

The final serial run passed all 12 success-expected runner invocations and retained one expected linker RED. Verified limits were memory.max=681574400 and memory.swap.max=0; the final measured memory.peak was 364716032 bytes.

Exact commands, exits and full logs live in ignored `native-portal/integration/build/verification.json` and adjacent named logs. No private ELF, trace or generated instruction bytes are committed. No SSH, deployment, runtime activation or hardware access occurred.
