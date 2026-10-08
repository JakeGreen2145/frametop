# Native compositor metadata hooks

This is a launch-only, default-off observer. It does not add camera intent, call OpenVR, allocate renderer resources, edit shaders, change blend state, change Aurora, or read camera pixels. It has not been activated in the compositor. The parent alone owns any live trial and its timed rollback.

## Delivered bundle

ARM build directory:

```
/home/steamos/dev/frametop-native-hooks/native-portal/hooks/build/bundle-83051aa891b468b4
```

The matching local copy is under `native-portal/hooks/build/`. The bundle contains the shared library, decoder, exact site pins and a manifest with source and artifact hashes. Files are mode 0400 in a mode 0500 directory. All binaries, private evidence, Python dependencies and test output remain ignored under `build/`.

| Artifact | SHA256 |
|---|---|
| `libft_native_hooks.so` | `0ce3bbd029cea7eac92a7baddc3d8ad033d78e37bc0fe7cc52db10038df81c74` |
| `decode.py` | `a3e4c12712404446676d00219ae4b5ac5a32863803c381faaadeaddc58c77398` |
| `pins.h` | `fd67258a3a1439dced323658f6e15110fbebb9ef2db811bdfe3a22a80c090102` |
| ARM `test_native` | `3db73ef60fc90eee992baa8cead67ba296bf965c33a892a024ac54c1f8c3d52f` |
| ARM `test_emit` | `fa39c22b6b71eb740ae7d2900bc34217133a044d7e1ca5cae294c43da41965e3` |

Only `/opt/steamvr/bin/linuxarm64/vrcompositor` with SHA256 `9b1bd5fe4463592cbc71b2517b9ceb0a0fcd66cd6885a19dca4be0784bf6c265` is supported. There is no offset discovery or fallback for another ELF.

## Build and verify

From this isolated local checkout:

```sh
rsync -a --exclude build native-portal/hooks/ \
  frame:/home/steamos/dev/frametop-native-hooks/native-portal/hooks/
ssh frame 'cd /home/steamos/dev/frametop-native-hooks/native-portal/hooks && ~/.local/bin/distrobox enter dev -- python3 -B build.py'
rsync -a --exclude tmp --exclude 'test-log-*' \
  frame:/home/steamos/dev/frametop-native-hooks/native-portal/hooks/build/ \
  native-portal/hooks/build/
python3 -m pip install --target native-portal/hooks/build/pylib unicorn==2.1.4
python3 -B -m unittest discover -s native-portal/hooks -p 'test_*.py' -v
```

`build.py` refuses non-AArch64. It builds only in this new ARM mirror and runs synthetic tests. It reads and privately maps the installed pinned ELF for identity tests but never loads its libraries, executes its code, starts SteamVR, attaches to another process, installs anything or changes services. Its synthetic island uses a generated RET continuation, not Valve code. Compiler was GCC 16.2.1 in distrobox `dev`; flags and dynamic dependencies are in `build/arm-build.txt`. Runtime dependencies are `libc.so.6` and `libcrypto.so.3`. Default-off and opt-in wrong-executable loads also passed on the SteamOS host with `/usr/bin/true`.

Observed verification:

- ARM build and native test exits were 0. Native tests cover fresh private files, wrong executable/hash, positive pinned file-backed mapping and load-bias refusals, all-or-none byte guards, precommit protection rollback, postcommit RX-failure exit 190, bounded ring/drop counters, invalid metadata addresses, fork refusal, both-eye metadata, errno preservation, real ARM island execution and deadline draining.
- Production C emitted 75 islands covering 25 sites at three synthetic ASLR bases. Unicorn ran each twice, including both pipeline CBZ outcomes: 150 executions. It compared every x0..x30, every full q0..q31, SP, NZCV, FPCR, FPSR, continuation PC and native instruction memory effects against the original pinned instruction. The callback deliberately clobbered even callee-saved registers.
- The final Python suite reported `Ran 4 tests` and `OK`, exit 0. It also decoded real ARM-generated metadata fixtures and rejected malformed/truncated records.
- RED outputs are retained as `build/red-*.txt`; final outputs are `build/final-tests.txt`, `build/native-test.txt`, `build/arm-build.txt`, `build/pin-audit.json` and `build/SHA256SUMS`. Initial fixture errors involved an overlapping Unicorn mapping and overlapping 16-byte guard windows; both were fixed without weakening register or guard checks.

These are preservation and observer tests, not live callback-order evidence or product-renderer acceptance.

## Parent-only launch options

No launcher or service drop-in is installed. For a separately authorized compositor launch, the parent must append the absolute bundle library to the existing `LD_PRELOAD`, retaining the existing testbench preload verbatim. Do not replace `VK_LAYER_PATH`, `VK_ADD_LAYER_PATH`, `VK_INSTANCE_LAYERS`, or stock loader discovery. The observer never changes those variables.

| Variable | Required value |
|---|---|
| `FT_NATIVE_HOOKS_ENABLE` | Exactly `metadata-v1-9b1bd5fe`; absent or anything else means off |
| `FT_NATIVE_HOOKS_LOG` | Absolute, fresh filename in an already-existing canonical, user-owned mode 0700 directory |
| `FT_NATIVE_HOOKS_SECONDS` | Decimal 1..120; default 60 |
| `LD_PRELOAD` | Existing value plus `:` and the absolute `libft_native_hooks.so` path |

Do not combine with `FT_AURORA_GATE_ENABLE`, `FT_NATIVE_CLIP_DIR` or `FT_CAMERA_BLEND_MODE`. Their presence refuses installation. The native observer has no shader/layer dependency.

The output is a new mode 0600 regular file, opened with O_EXCL and O_NOFOLLOW. Existing files, symlinks, noncanonical/private-parent failures and missing directories are refused. An unusable log target causes no hooks and no file. No enabled variable causes no file and no thread. The trace stops after the duration or 16,384 accepted events, whichever comes first. A 256-record static ring drops on full/contended/reentrant paths rather than waiting. Drops are not proof of missing native calls.

The duration only disables observation and drains the ring. It does not restart SteamVR or remove installed branches. Islands remain RX until process exit and continue native pass-through. Never dlclose an installed observer or unmap its islands. Parent rollback must remove only its own launch additions and use the parent's separately authorized process-lifecycle procedure. Do not relax the single-thread startup guard if the existing preload has already started threads; treat that as a failed trial.

## Install and hot-path contract

Startup verifies the exact executable path, current executable versus installed file identity, stable whole-file hash, pinned ELF headers/program headers, load bias, private RX backing device/inode/offset and every 16-byte site window. The overlay-backed filesystem identity comes from a private read-only alias of the same pinned descriptor, not a relaxed inode/device comparison. Guards were adapted from the existing reviewed launch-only diagnostic without modifying that original.

One four-byte B replaces each site. A bounded MAP_FIXED_NOREPLACE search reserves nearby RW islands and converts them to RX after cache synchronization. Only this finite instruction set is retained: ordinary non-PC-relative instruction, BL, BLR x4, CBZ x0, B and RET. BL/BLR set x30 to the original callsite+4 before jumping to the original native target. There is no general instruction relocator and no live register used as a hidden branch scratch register.

Each island reserves an aligned 800-byte bridge frame. It saves all 31 GPRs, all 32 full SIMD registers, original SP, NZCV, FPCR and FPSR, calls the observer, restores them, then performs the retained native instruction. C preserves errno. No native pointer, result, packet or policy value is changed.

Installation requires one process thread and blocks signals. Every target page becomes RW without X before any word is committed. Failure in this preparation restores RX with original words untouched. After committing, cache synchronization, RX restoration and exact branch/page postchecks are mandatory. A postcommit failure terminates startup with exit 190 instead of continuing partially installed. The drainer starts only after a complete install; setup failure then also terminates startup. These are process-local private mappings, never disk patches.

Hot callbacks do no allocation, file I/O, IPC, renderer/VR calls or blocking locks. They use initial-exec TLS, bounded fixed-size copies, atomics, a single try-lock attempt, clock/PID/TID queries and self-only `process_vm_readv` metadata reads. The latter are fault-safe kernel copies, not arbitrary pointer dereferences. They can still perturb timing or incur page faults; no zero-overhead claim is made. A drainer does regular-file writes outside callbacks. Forked children disable inherited observation and do not join a vanished parent drainer.

## Hook inventory

IDs are zero-based in `pins.h` and are stable for schema 1.

| IDs | Offsets | Observation |
|---|---|---|
| 0, 1, 24 | `0x147a48`, `0x148414`, `0x148de0` | Outer entry, normal post-eye point, actual outer RET |
| 2, 3, 4 | `0x112b58`, `0x122568`, `0x1480b8` | Stock pre-feedback policy, completed S, stereo seal |
| 5..8 | `0x148244`, `0x1482a8`, `0x14834c`, `0x14840c` | Left early/late, right early/late callsites |
| 9 | `0x161ed8` | Camera enqueue before original BLR x4 |
| 10, 11 | `0x29f1e8`, `0x29f204` | Camera backend entry and actual pipeline lookup result |
| 12, 13 | `0x6d6e0`, `0x6d6f0` | Raw external acquire/release wrappers and original caller LR |
| 14 | `0x63920` | Original HMD pose record from incoming x3 array, stride 0x50 |
| 15, 16, 17 | `0x9ee98`, `0x9eeb4`, `0xa0510` | Activity/cache comparison and native layer transition results |
| 18, 19, 20 | `0x15f9e0`, `0x15fa78`, `0x15faa4` | K acquire entry and both real native return sites |
| 21, 22, 23 | `0x15fba8`, `0x15fbd8`, `0x15fc20` | K release entry and both real native return sites |

The shared transition result at `0x9eeb4` is reached after acquire as well as release. Do not label every event there a release. The actual RET events carry native x0 and caller LR unchanged.

At enqueue, K is original x27, eye is original x19, and packet is original x1. The captured SP is the original site SP, not the bridge's lower SP. The pinned camera prologue subtracts 0x460. Only the two return epilogues add it back; neither is on a path continuing to enqueue. The exact getter destination is therefore site-SP+0x170. The observer reads its 48 bytes at enqueue, without calling the getter. The native packet begins at camera-SP+0x300 on the inspected path; x1 remains the authoritative packet reference. `pin-audit.json` records the decoded stack adjustments and site windows.

## Event schema

The binary stream consists of fixed 2,384-byte little-endian records. `decode.py FILE` emits JSONL without contacting any process. Use `python3 -B` so a read-only bundle does not need a bytecode cache.

Header format is Python `struct.Struct('<4I6Q4I32Q')`, 336 bytes:

1. u32 magic `0x3154464e`, record size, site ID, flags.
2. u64 accepted sequence, frame context, per-thread callback sequence, Linux TID, CLOCK_MONOTONIC nanoseconds, outer owner x0.
3. u32 scope depth, current eye, eye-seen mask, payload bytes used.
4. 31 u64 original GPR values x0..x30, then original SP.

The remaining 2,048 bytes contain chunks. Each begins with `<IIQ>`: tag, byte length, original address, followed by exactly that many bytes. A tag's high bit marks a failed read; its data is zeroed and must not be treated as a native value. The observer flags are 1=no outer scope, 2=ambiguous/recovered scope, 4=failed metadata read, 8=payload bound refusal.

| Tag | Bytes | Contents |
|---|---:|---|
| 1 | 528 | Unchanged native UBO bytes at K+0xc0 |
| 2 | 48 | This draw's raw-to-Standing 3x4 at original camera-SP+0x170 |
| 3 | 352 | Original or recorded packet prefix; VS at +0x20, PS at +0x30, blend +8, VS UBO +0x38, PS UBO +0x58 |
| 4 | 216 | Nine native recorded descriptor entries at recorded packet+0x1c8; raw entries, no buffer-content reads |
| 5 | 80 | HMD device-to-absolute matrix, velocities, tracking result +0x48, valid +0x4c, connected +0x4d |
| 6 | 1632 | Stock S, copied read-only through +0x65f |
| 7 | 24 | Shared policy reread H+0x40b40..+0x40b57 |
| 8 | 2 | Policy-owner enable bytes L+0x1572/+0x1573 |
| 9 | 8 | F tracking class and native diagnostic input ID |
| 10 | 64 | F per-device copied validity bytes at +0xc38 |
| 11 | 1 | Existing layer activity cache at comparison |
| 12 | 40 | K+0x340 resource state/handle metadata; no pointed-to resources |

Site `0xffffffff` is a control record. Flags are the control code; data starts with the pinned ELF SHA256 text. `decode.py` names every refusal/fatal code. Codes 0, 1 and 2 mean prepared, installed and drained. Control x0..x5 hold PID, drops, failed reads, filtered noncamera backend calls, reentrant callbacks and scope errors. The control owner field is the image load bias.

## Interpretation and remaining gates

- The frame token is a shim monotonic outer invocation sequence, not F-pointer equality or F+4. TLS supports four nested scopes and detects missing normal cleanup on the next equal/higher-SP outer entry. Overflow stops observation. Nonlocal unwind is not intercepted. Any scope error or ambiguous flag invalidates an ordering conclusion involving that scope.
- Both eyes share the same observed outer token. `eye_seen_mask` reports callbacks already seen in that thread's current scope; it is not a rendering-validity decision. The tracking capture contains the complete HMD rigid matrix and native valid/connected/result fields. Other pose-array records are not copied; x3 retains the original array reference and F has all copied validity bytes.
- `frame_context` on a backend event means the TLS scope active when that callback ran. It is not authenticated enqueue ownership and may be zero on a deferred worker. The trace includes original/recorded packet addresses, selected shaders, both UBO references and recorded descriptor bytes for offline association. Ambiguous matches must remain ambiguous; do not infer same-draw identity from shader IDs, buffer range or current TLS alone.
- Backend callbacks are logged only when the native VS field is 57 or 58. Other backend calls increase `filtered_non_camera`; no native result changes. Pipeline result x0 can be null and the native CBZ still follows its original null branch.
- Policy tag 7 is explicitly a reread at the capture point, not a retained historical result from every preceding load. This observer captures stock policy only. It does not implement the pure counterfactual graph, ownership arbitration, acquire/release handoff or any effective intent.
- Metadata chunks are bounded observations, not an atomic snapshot across separate native objects. Concurrent native mutation can make fields inconsistent. Require the live thread/order evidence before treating a K, policy or descriptor copy as a stable ownership input.
- Accepted sequence is ring publication order. Per-thread sequence includes filtered/dropped callbacks. Cross-thread monotonic timestamps bracket callback observations, not GPU submission, causality or exposure time. Read failures, drops, reentrancy, scope errors or an incomplete terminal record require an inconclusive result for affected ordering claims.
- No resource lifetime, native intent owner, recenter epoch, simultaneous-eye backend admission, GPU completion, optical registration or renderer activation is proven. Parent still needs the bounded live trace and must stop on unexpected ordering rather than enabling the product renderer.

The full requested critical ordering hook set is included; it was not reduced to an entry-only observer. The remaining limitations are explicitly observational, not silent substitutions for a product-safe ownership adapter.
