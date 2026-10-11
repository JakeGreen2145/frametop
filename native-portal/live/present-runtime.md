# First-consumer Present regression

## Result

Functional RED, without any production repair. The actual default Runtime records both eyes, but a first dispatch from Present LR `0x2955a8` cannot open the graph scope. It performs no pipeline lookup, commits no pair and drops both owned packets. Those marker-only drops do not certify a complete rejected pair. Native queue, wait, transfer, disposal and reset observations therefore cannot settle the retained upload/camera debt. Off and the ordinary verified worker join end in production cleanup refusal, reason `5`, event `0x8b020`, `joined=1`. No native cleanup or Disconnect executes.

This branch starts at `b4a978b8106275f0e42affe056dab662dc35d99a`. Its shared-factory repair remains a provisional dependency under separate review. This report does not claim that dependency has merged. Integration baseline is `f9adb38`.

| First consumer | Dispatch graph / recording / pass-live | PassReason | Pipeline lookups | Pair committed | Owned skip mask | After exact settlement tail |
|---|---|---|---|---|---|---|
| Flush `0x6caf0` | 1 / 1 / 1 | Ready `0` | 2 | 1 | 0 | Source sealed, GPU complete, CPU settled, pending 0, clean joined teardown |
| Present `0x2955a8` | 0 / 1 / 0 | Lifetime `2` | 0 | 0 | 3 | Two tickets and camera debt retained, no Command certificate, pending 1, exact fatal cleanup refusal |
| Unknown `0xdead` | 0 / 0 / 0 | Lifetime `2` | 0 | 0 | 3 | Same retained debt, but Evidence first fails immediately at `0x2a1e18` |
| Flush with cold cached pass | 1 / 1 / 1 | ColdPass `11` | 0 | 0 | 3 | Actual two-skip rejection certified, source sealed with no samples, GPU/CPU settled, pending 0, clean teardown |

Present has no Evidence or stock-collector failure at dispatch. Its immediate Runtime refusal is Adapter `9`; actual packet skips change it to GraphOwnership `3`. During the full tail, Evidence later fails at wrapper release `0x305260`. The stock collector remains unfailed. Unknown LR's first Evidence failure stays `0x2a1e18`. The continuation is not truncated at either failure.

Both uploads are charged before the stimulus. Present and unknown keep `tickets=2`, `camera_debt=1`, `source_sealed=0`, `gpu_complete=0`, `cpu_settled=0`, `queue_observed=0`, `encoded=0,0` through reset. These zero completion flags mean unproved retirement, not a claim that native GPU work never ran. Native queue result zero is modeled and delivered even when no integration Command certificate can be created.

Off precedes the modeled queue, matching the original fixture. Consequently lease-visible `submitted_frame` remains zero in all four cases. Flush separately records `stereo_submitted=1`, accepted queue count 1, GPU completion and CPU retirement. Cold-pass rejection correctly leaves `stereo_submitted=0` despite an accepted upload-lifetime queue. These are deliberately different assertions.

## Fixture and execution

`current_runtime_test.cpp` still constructs the actual default `Runtime`, with no injected admission provider. A test-only callback in `runtime_chain_test.cpp` replaces only the continuation after its two original `record()` calls and outer return. The original continuation remains intact for every default case.

The focused prelude runs bootstrap factory receipts, actual Run handoff, all five shared roots with positive/null results, shared unselected/cleanup controls, scheduled source Update, the production control socket, current copied camera selection and real Adapter uploads/enqueues. The camera fields deliberately invalidated by the existing fixture remain invalidated. No broad late-read fallback is added.

`present_runtime_fixture.inc` delivers the first dispatch and executes both packet callbacks, a stock-packet non-suppression control, Off, queue/raw result/stamp, exact wait, graph transfer, wrapper/pool return, successor publication, retained disposal and reset/Begin. The original scenario then performs owner stop, actual host thread join, native join observations and application/camera cleanup. Fatal cases have no trailing fixture assertion that could supply SIGABRT. A forbidden cleanup exits with a distinct non-abort code.

`present_runtime_observer_test.cpp` includes unchanged `runtime.cpp` in a separate test-only translation unit. Only that object is compiled with `-fno-access-control` for read-only internal snapshots. It replaces, rather than duplicates, the ordinary Runtime object in the focused binary. It never writes Runtime internals. PassReason comes from the actual read-only scanner at the suspended dispatch; no copied guard implementation or second Evidence instance supplies it. After join it reads only the published diagnostics, not owner-thread-only readers.

The runner fingerprints all production/test `.cpp`, `.hpp`, `.h`, `.inc` and `.py` inputs, compiler flags, modeled installed inventory and private header hashes. The observer object's dependencies explicitly include `runtime.cpp`. Focused outputs, normal outputs and sanitized outputs have separate ignored directories.

Commands from the repository root:

```sh
# Functional acceptance: exits 1, only consumer-present is RED.
python3 native-portal/live/dormant_runtime_test.py --first-consumer
python3 native-portal/live/dormant_runtime_test.py --first-consumer --sanitize

# Current-outcome diagnosis: exits 0; DOES NOT make Present functional.
python3 native-portal/live/dormant_runtime_test.py --first-consumer --expect-refusal
python3 native-portal/live/dormant_runtime_test.py --first-consumer --expect-refusal --sanitize

# Preserved default acceptance, including cold/warm, unknown startup and Off debts.
python3 native-portal/live/dormant_runtime_test.py --current
python3 native-portal/live/dormant_runtime_test.py --current --sanitize
```

All runs are serial, under `MemoryMax=256M`, `MemorySwapMax=0`. Normal and ASan/UBSan default suites each pass 109 cases. Focused normal and sanitized functional suites each pass the three controls and fail Present. All four exact diagnostic cases pass in both builds. Every positive mode creates two full Runtimes in one process; every negative reaches the complete joined refusal tail. No sanitizer finding was reported.

The six result files contain identical source-hash maps. SHA256 of `json.dumps(source_hashes, sort_keys=True).encode()` is `25bb3204333c50c045441c57c66545fe0be1e4aa90afb0cc8ac8f4a1ab1170b7`.

Artifacts are under `native-portal/build/dormant-runtime/current/`:

- `acceptance.json` and `sanitize/acceptance.json`, default suite.
- `first-consumer/functional.json`, `diagnostic.json` and each `consumer-*.log`.
- `first-consumer/sanitize/`, corresponding sanitized results and full logs.
- `compile.log` in each build directory, exact compiler commands.
- `first-consumer/consumer-native-facts.json`, derived caller facts and limits.

One fixture error was found and corrected before the recorded final runs: the added stock-packet probe initially omitted SP/LR, overwriting the scope later needed for rejected-command registration. Supplying the same valid modeled callback scope restored the complete cold-pass settlement control. No production condition was weakened.

## Native pins and missing contract

Caller facts come from the supplied verified lane-C `NATIVE-PINS.json`, SHA256 `dbb7ae08e6b66b52b4211e2ba735bdaebb2aa196ab782ccfd20bb09ebe523f46`. The parent report hash was independently rechecked as `b1ec331db554aa3c49d66a933e241297b67cfd68c75d09195c714efdfdd09e09`. Native image identity is `9b1bd5fe4463592cbc71b2517b9ceb0a0fcd66cd6885a19dca4be0784bf6c265`.

The generator verifies the saved window hashes and decodes only their retained argument/load/BLR words. Present's `0x295594` window establishes renderer in x0, w1 zero, virtual slot `0x250` and return LR `0x2955a8`. Flush's `0x6cadc` window establishes x0 from its caller's renderer field, w1 from w20, slot `0x250` and LR `0x6caf0`; the fixture chooses mode zero. All tested call lanes are identical except LR. Addresses, SP, unobserved registers, native method bodies, Vulkan, allocation and installed coverage remain modeled. The saved private pump/camera/selection headers retain their existing SHA checks. No ELF analysis runner is imported or executed.

Before adding Present graph authority, Desktop must establish the containing `0x295550`, caller `0x6eee0` and Present `0x8a048` path's thread and lock/exclusion scope. The proof must cover the exact current renderer/command and every added graph reader: current and retained vectors, packet labels/descriptors, attachments, warm renderpass cache, material lookup/native pipeline object, both backend draws and the cutoff before `0x2a2230` move/free. It must exclude foreign or reentrant mutation/destruction for that interval, rather than merely authenticate slot250 or a recording generation. If the scope cannot be certified at existing callbacks, identify a synchronous entry/exit or lock-result hook that carries that exclusion through both eyes. Any hook/authority change remains parent-owned and requires explicit steering.

This fixture proves a source-level first-consumer compatibility failure and its settlement consequence. It does not prove a live native caller schedule, a whole-method native lifetime, GPU rendering or optical output. No graph allowlist, factory router, camera admission, production Runtime or header was changed. Independent integration review remains for the parent.
