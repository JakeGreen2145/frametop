# Camera-route fatal diagnostics

This change labels the existing abort sites inside `Runtime::Impl::camera_route`.
It does not identify or fix the previously observed startup failure. Every guard
predicate, short-circuit order, refusal, and abort outcome is unchanged. Aborts
outside this function are not covered.

## Output contract

A failed guard builds a fixed-size `CameraFatalRecord` and attempts one stderr
socket send. The line starts with `ft-camera-fatal` and contains only:

- `reason`, a stable enum value identifying the abort site.
- `event`, the received callback/event ID, including in nested lambdas.
- `current_tid`, the thread executing the failed guard.
- `owner_tid`, the currently expected owner thread.
- `bootstrap_tid`, the immutable initializer thread.
- `bound`, `handoff_wait`, `stopped`, and `joined`, sampled atomic booleans.

The reason, event, and thread IDs are fixed-width hexadecimal. Booleans are `0`
or `1`. Loads are individually atomic and relaxed, not a coherent transaction.
The new logging code never reads saved-register pointers, source objects, owner-only
nonatomics, camera state, or frame data. Compile-time checks require lock-free
atomics for the sampled fields.

| Reason | Enum | Existing abort site |
| --- | --- | --- |
| 1 | `DecisionScope` | Release/Clear decision scope, owner, or camera mismatch |
| 2 | `Publication` | Publication thread, handoff, bound state, or observation refused |
| 3 | `CleanupThread` | Cleanup on a thread other than the owner or joined bootstrap |
| 4 | `CleanupOriginal` | Cleanup original call failed |
| 5 | `CleanupRefused` | Camera-owner cleanup refused |
| 6 | `WriterBinding` | Writer object does not match the retained binding |
| 7 | `NestedMethod` | Nested camera method failed |
| 8 | `WriterBeforeHandoff` | Unbound writer on a foreign thread or during handoff wait |
| 9 | `MutationDebt` | Mutation encountered camera debt or pending adapter work |
| 10 | `SubmitRefused` | Foreign mutation submission refused |
| 11 | `CompletionTimeout` | Submitted mutation did not complete within the existing deadline |
| 12 | `RetentionNotReleased` | Completion did not release retention |
| 13 | `NativeResult` | Mutation result was not executed or had no valid value |

A reason identifies the compound guard, not which term within that guard failed.
Thread and state coordinates can narrow it further without acquiring new read
permissions.

Delivery is explicitly **best effort**. `send` uses
`MSG_DONTWAIT | MSG_NOSIGNAL` once, with a fixed stack buffer no larger than 256
bytes. There is no retry, allocation, lock, wait, stdio, file write, descriptor
flag change, or fallback. Full, missing, closed, pipe, and regular-file stderr
can drop the record. A partial socket send can truncate it; concurrent ordinary
logging can interfere with readability. Absence of a record does not mean the
guard did not execute. `emit_camera_failure` preserves `errno` for direct unit
checking. Its fatal wrapper always calls `std::abort` afterward.

## Source-only verification

`startup_fatal_test.cpp` adapts the saved startup fixture. Native calls and graph
storage remain mocked boundaries. `startup_fatal_test.py` reads the existing
`SOURCES` assignment through `ast.literal_eval`, not by importing its runner.
It accepts previously saved opaque fixture headers and numeric coordinates. It
does not regenerate them, inspect external executable images, or invoke native
evidence runners.

Example using the already saved local inputs:

```sh
python3 native-portal/live/startup_fatal_test.py \
  --coordinates /home/jakeg/.hermes/cache/scratch/camera-abort-488/derived-native-coordinates.json \
  --fixtures /home/jakeg/.hermes/work/frametop-native-live-integration/native-portal/build/camera-runtime-asan \
  --vulkan-include /home/jakeg/.hermes/work/frame-native-pipeline-review/include \
  --with-helper
```

The runner puts every compilation and test inside a systemd user cgroup with
`MemoryMax=256M`, `MemorySwapMax=0`, and
`TMPDIR=/home/jakeg/.hermes/cache/scratch`. It checks the inherited cgroup limits,
disables core dumps, and gives each subprocess a timeout. Build output and
`results.json` go under `native-portal/live/build/startup-fatal` by default.
Saved fixture inputs are prerequisites, not checked-in generated evidence.

Before the production change, the tests executed 23 modeled subprocess cases
and failed specifically because the foreign writer, cleanup, and publication
aborts had no fatal record. Their exits were still `SIGABRT`.

With the change, the full focused run covers 37 subprocess cases:

- Bootstrap-owned Release and a foreign noncamera event return without a fatal record.
- Before-handoff foreign Release, cleanup, and publication each emit their own
  reason with the exact received event and thread IDs, then abort. The fixture
  asserts zero frames, uploads, and pending work before each callback.
- Each failure is exercised with an AF_UNIX stream socket, a saturated socket,
  a closed peer, missing stderr, a pipe, a full pipe, and a regular file.
- Direct helper tests cover every reason value, both values of every boolean,
  maximum-width integers, exact send flags, one send per invocation, preserved
  `errno`, and no regular-file or pipe fallback. SIGPIPE is explicitly restored
  to its default disposition so an accidental SIGPIPE would fail the exit check.

Only three reason sites are exercised through the router. The other reason IDs
have direct formatting/send coverage, not modeled reachability coverage.
The original `camera_route` body was also compared with the modified body after
normalizing the 13 diagnostic calls back to `std::abort()`. They matched byte
for byte, and all 13 reason IDs were unique.

These modeled schedules do not identify the callback that failed in the real
runtime. No whole-suite, live-camera, ARM-build, device, loader, or installed-code
verification is claimed. Independent review, ARM compilation, and any separately
authorized operator trial remain outside this change.
