# Start/Run lifetime repair verification

Base: `e26d10a16fa316d660fd47ac01648c894dad7133`.
Scope: the two source-modeled stale-join schedules in the independent review. Desktop's `publication-run-join-10-42328f8769c4/UPDATED-SOURCE-CHECKPOINT.md` remains the contract. No native late-publication path is claimed.

## Repair

Runtime saves the already-authenticated Start application for revocation only. Matching application cleanup invalidates this scope even without P. A bounded set of saved application scalars records complete unbound destructive invocations. Start authentication and its snapshot share the existing join mutex with invocation entry/return, so matching destruction already in flight leaves Start permanently revoked. No lock spans an original call.

Camera/resource destruction is attributed through existing pending publication scalars, not native pointer reads. First Init classification, candidate invalidation counters, partial/bound epochs and CPU/GPU/source debt gates are unchanged. No pointer polling, epoch reset, native success substitution or blanket writer invalidation was added.

## Results

The unchanged original `straddle_start_review.cpp` and `run_review.py` were copied into owned red/green scratch and linked with actual Runtime production objects. The runner's absolute fixture include is byte-identical to this worktree's fixture.

| Original mode | Base | Repair |
| --- | --- | --- |
| `initial-success` | exit 0 | exit 0 |
| `late` | exit 0 | exit 0 |
| `late-aba` | capability assertion abort; bound=1, attempts=1, capable=1 | exit 0; bound=0, attempts=0, capable=0 |
| `cleanup-straddles-start` | capability assertion abort; bound=1, attempts=1, capable=1 | lifetime assertions pass; bound=0, attempts=0, capable=0; later fixture count assertion abort |

The original straddle fixture calls Init twice but its final line 156 expects one. It was neither truncated nor edited. Consequently its **complete unchanged replay is not green**, despite fixing the reported lifetime failure. The tracked regression retains the false-capability assertion, expects both original Init calls and runs full normal teardown. Changing production forwarding to satisfy the original erroneous count would violate the stock contract.

Both tracked negative schedules failed at the capability assertion before the repair. The final fresh ASan/UBSan build passed all **24 modes**, including initial-success, late, blocked-stock, unrelated-cleanup, init-failure, partial-bind, aba and start-aba. The new unrelated-cleanup positive blocks another application's cleanup across Start and Run; joining remains excluded until return, then succeeds. Both repaired negative modes also verify pending/resource-ready P, zero join attempts/epoch/barrier, unchanged stock Update result and normal teardown. Expected partial-bind and bound-publication aborts retained their existing fatal-reason checks.

All compiler/probe children ran under verified `memory.max=268435456`, `memory.swap.max=0`, with `TMPDIR=/home/jakeg/.hermes/cache/scratch`. `git diff --check` passed. No binary inspection, emulator, device, SSH, restart, loader or provider changes were used. Parent owns independent confirmation, ARM build and live trials.

## Reproduction

From the owning worktree:

```sh
python3 native-portal/live/initial_binding_test.py \
  --coordinates /home/jakeg/.hermes/cache/scratch/camera-abort-488/derived-native-coordinates.json \
  --fixtures /home/jakeg/.hermes/work/frametop-native-live-integration/native-portal/build/camera-runtime-asan \
  --vulkan-include /home/jakeg/.hermes/work/frame-native-pipeline-review/include \
  --build /home/jakeg/.hermes/cache/scratch/start-lifetime-repair-green --sanitize
```

Full logs and result JSON are in `start-lifetime-repair-red/` and `start-lifetime-repair-green/` under that scratch directory. Original review files remain in `initial-binding-independent-spec-e26d/`. Hashes of original and replay copies match:

- `straddle_start_review.cpp`: `deb57fafbdb3c9acc847b03752c54687b307b35d3234cf8d6b193ef10f6c7b29`
- `run_review.py`: `cd7264c97219aec56b0e55f2e8b093bda3957d4872fd8f1ef13715fab6f24f6d`
