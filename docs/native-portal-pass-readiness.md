# Native pass readiness, inactive warm-cache subset

## Outcome

The new `binding/pass*` code extracts both owned recorded packets from the actual full pass vector and feeds the existing `Adapter::prepare`. It supports the proved native cache-hit path only. It does not call the pass encoder or materialize a cold native pass early.

`PassGate` performs extraction, both reviewed pipeline lookups, re-extraction and one-shot commit at dispatcher entry. Its packet callback suppresses only the exact owned marker when the dispatch did not commit. Stock packets keep their original path. Off after commit does not cancel the second eye.

This is not a complete production binding. Nothing installs these islands, publishes capability, supplies a live lifetime reader, or activates a portal. Cold passes and potentially mutating pre-dispatch work remain unsupported. The existing render, control, arena and command implementations are unchanged.

## Evidence and exact arguments

Pinned private ELF:

```
/home/jakeg/.hermes/work/frame-native-reuse/camera-analysis/vrcompositor
9b1bd5fe4463592cbc71b2517b9ceb0a0fcd66cd6885a19dca4be0784bf6c265
```

Read the transaction-feasibility report section 1 and both live03 binding/completion reports before changing this contract. The tests load the existing analyzer definitions without executing its report writers. Private generated pin words and emulator results stay in ignored `native-portal/binding/build/pass/`.

| Native boundary | Extraction or continuation |
|---|---|
| `0x2a1e18` | x0=R, w1=mode. Nonzero mode keeps the original branch to `0x2a2564`. |
| `R+0x15a0/+0x15a8` | Full native pass vector, stride `0x98`. |
| `pass+0x50/+0x58` | Recorded packet span, stride `0x2b0`. Empty passes require no prepared pass. |
| `0x2a198c -> 0x2a0638` | Original pass helper produces the two values at encoder `sp+0x70/+0x78`. |
| `R+0x1478/+0x1480` | Native pass cache, stride `0x40`. Extract the first exact matching entry. |
| `0x2a0668..0x2a06f0` | Cache-hit comparisons and native output loads. No calls on this path. |
| `0x2a1cc0..0x2a1cdc` | Packet byte `+0x15e` selects alternate target at pass `+0x10` when nonnull, otherwise primary target at pass `+0`. Read target `+0x14c` as **u32**, not u64. |
| `0x2a1cec -> 0x29f1e8` | x0=R, x1=actual packet, x2=first native pass-cache value, w3=selected attachment format, x4=pass. |
| `0x29f200 -> 0x29dac0` | Backend prologue leaves x0..x3 unchanged for pipeline lookup. |
| `0x29db1c/0x29db24` | Actual native pipeline-key stores consume that x2 value and w3 format. |
| `0x2a1c18 -> 0x2a1c0c` | Owned-only skip before packet-local work. The original increment/loop retains stock packets and normal pass begin/end. |

The exact native cache key compares these fields:

- entry `+0x10/+0x18/+0x20/+0x30` against pass `+0/+8/+0x10/+0x20`;
- low 24 bits of entry `+0x38` against pass `+0x68`;
- output value and framebuffer from entry `+0/+8`.

Pass `+0x18` and byte `+0x6b` are not part of the native lookup key. The differential tests mutate every compared field and both ignored fields. The extractor requires both native output handles nonzero. It never substitutes the pass pointer, a stack address, packet count, or caller-supplied ready flag for the first native output.

Both eyes may have different passes, render-pass values and attachment formats. All six supported shader pairs remain supported: VS57 with PS125..127 and VS58 with PS128..130.

## Why cold preparation remains blocked

The complete native `0x2a0638` helper is not a pure query. The executed cold fixture reaches native render-pass/framebuffer creation, then call `0x2a149c -> 0x2fe638` on the alternate image. That is a native image-barrier boundary using the active command. The fixture executes the actual helper instructions while explicitly faking native vector allocation and external Vulkan dispatch. It does not execute real Vulkan.

Moving that helper to dispatch entry can move a barrier across preceding native operations, queries and passes. Calling the complete `0x2a1928` encoder would additionally begin a render pass and encode packet work. Neither is used by production preparation.

The implemented subset requires **every nonempty pass**, including stock-only passes, to be a warm cache hit. A cold stock pass causes owned-only refusal, not suppression of that stock pass. Native stock materialization still runs at its original location. This conservative refusal also covers cold variants that might individually avoid the witnessed barrier; their complete early-execution/lifetime contract has not been proved.

Preparation also refuses:

- optimizer flag `R+0x15d0` set, because `0x29ce00` can process the pass graph;
- nonempty pre-dispatch lists at `R+0x1570/+0x1578` or `R+0x1588/+0x1590`;
- owned special-state packets with byte `+0x2a8` or auxiliary object `+0x180` set;
- owned graphics passes without byte `+0x68`, or alternate packets without byte `+0x69`.

These restrictions avoid unproved mutation/diagnostic paths. They are not claims that those native paths are invalid for stock work.

## Production contract and bounded failure

`PassReader` has only checked memory-read and native-lifetime operations. There is no materialization callback and no caller-provided readiness Boolean. Its owner must already authenticate the process, pinned image/load bias, renderer, device, recording generation and immutable owned marker. It must serialize and retain the pass graph, packets, attachment objects, native cache handles and resulting pipelines through both committed eyes. Default-null readers refuse.

`extract_passes` reads copied prefixes, exact 15-character inline ticket carriers and all nine descriptor snapshots. It recognizes ownership by exact immutable marker first. Unmarked packets with ticket-looking labels remain stock. The Adapter then authenticates both tickets, prefixes, generations and descriptors before either pipeline callback runs. Labels are never arbitrary pointer reads.

Storage/work limits are 64 passes, 1024 total packets and 128 cache entries. The scanner uses fixed automatic storage and retains only two copied owned records. An invalid span, read failure, overflow, duplicate, partial pair or limit produces no readiness and no owned commit. The packet gate tests the exact marker independently, so reaching a scan cap does not exempt later owned packets from suppression. No native vector entry is erased and no label is freed.

After the two pipeline callbacks, preparation scans again and compares the exact command, packets, descriptors, pass addresses and lookup arguments. A changed result latches Adapter refusal before commit. This catches reentrant callback mutation; it is not a substitute for serialized native lifetime ownership.

Lookup registers expose the four actual arguments x0..x3. Unused lanes x4..x7 are canonical zero in the Adapter transport. The native backend-to-Adapter binding must apply the same canonicalization while separately retaining the actual pass association. It must return the Adapter's cached pipeline, not run `0x29dac0` again after commit. This lane does not install that backend binding or change the reviewed Adapter.

The two generated islands use the existing audited save/restore emitter. The dispatch island preserves the displaced CBNZ. The packet island initializes saved offset 792 to zero, calls the owner-bound `PassGate`, then either restores the original LDRB continuation or skips to the unchanged increment. It restores all general registers, all SIMD registers, SP, LR, NZCV, FPCR and FPSR before either continuation. The generator only writes offline fixture bytes.

A live owner must not revoke/free borrowed memory while a dispatch still runs. A failed memory read is not proof that an unknown packet is owned, and cannot justify suppressing stock. Such a broken native lifetime remains unsupported, not a recovery or deployment claim.

## Verification

Strict red tests were executed before the extractor, gate and islands existed. A separate regression failed when a pipeline callback changed the second native pass-cache value after extraction. Adding post-lookup re-extraction made it pass.

The full serial regression run succeeded under one `MemoryMax=650M`, `MemorySwapMax=0` unit. Observed peak was 240713728 bytes. It ran:

- New production extraction/gate tests, normal and ASan/UBSan. These link the actual Adapter, FrameGate, packet authentication and policy implementations.
- 23 pinned instruction cases, including both attachment paths for all six shader pairs, exact key-field differentials, a cold-helper barrier witness and nonzero dispatch.
- 12 generated-island continuation/preservation cases across three load bases. The synthetic callback deliberately clobbers all caller register classes and flags.
- Existing complete render suite under ASan/UBSan, including both 10000-frame bounded-recycling cases and pre-encoding commit/Off tests.
- Existing command binding suite under ASan/UBSan against the pinned six live03 chains.

The new tests also exercise different eye passes, empty passes, stock interleaving, ticket-lookalikes, partial/duplicate/malformed packets, descriptor/prefix/generation mismatches, invalid/capped spans, cold stock, failed second pipeline, Off before preparation and Off after commit.

Production extraction is compared with actual native helper cache hits, actual packet argument assembly, the backend prologue and native pipeline-key instructions. Whole-pass encoder execution is a **reference test only**, after synthetic commit or with the proposed owned skip. Native viewport/profiling/Vulkan boundaries and pipeline results are explicitly synthetic. Tests prove CPU control flow and argument equivalence, not driver pipeline creation, live cache lifetime, headset pixels, or a finite Off-to-last-visible bound.

Reproduce from this checkout, using absolute paths because user systemd services do not inherit shell cwd:

```sh
W="$PWD"
A=/home/jakeg/.hermes/work/frame-native-live-render-analysis
E=/home/jakeg/.hermes/work/frame-native-reuse/camera-analysis/vrcompositor
systemd-run --user --wait --pipe \
  -p MemoryMax=650M -p MemorySwapMax=0 \
  --setenv=TMPDIR=/home/jakeg/.hermes/cache/scratch \
  python3 -B "$W/native-portal/binding/pass_test.py" --sanitize
systemd-run --user --wait --pipe \
  -p MemoryMax=650M -p MemorySwapMax=0 \
  python3 -B "$W/native-portal/binding/pass_machine.py" --analysis "$A" --elf "$E"
systemd-run --user --wait --pipe \
  -p MemoryMax=650M -p MemorySwapMax=0 \
  python3 -B "$W/native-portal/binding/pass_island_test.py" --analysis "$A" --elf "$E"
```

The existing render suite additionally needs the shown explicit TMPDIR environment for its private IPC fixture. An initial run without it failed constructing the fixture path; the final run above corrected the runner environment without editing existing tests. The emulator uses an explicit stop hook because a reused Unicorn translation block can otherwise pass a newly selected stop address.

## Remaining integration blockers

1. Prove a legal cold-pass preparation split that preserves native barrier/query order, or keep the warm-only capability restriction. Do not replace this with count flags or execute the encoder twice.
2. Supply and verify the actual owner-thread memory/lifetime reader, retained native cache/pipeline lifetime and recording context. Synthetic callbacks do not close this obligation.
3. Bind canonical x0..x3 backend mediation to the reviewed cached Adapter result and associate each native pass. The new islands are generated and tested, not installed or connected to a live callback registry.
4. Keep the separate command resubmission, CPU disposal, GPU completion and teardown requirements. This extractor neither settles nor cancels already executable native work.

No SSH, installer, runtime deployment, GPU device, custom renderer, camera reader or CV path was used.
