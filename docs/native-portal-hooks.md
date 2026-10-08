# Native compositor metadata hooks

This launch-only observer is default-off. It only records existing native metadata. It does not call OpenVR, request camera intent, change a ticket or name, allocate renderer resources, write native policy, change blend/shaders, or read camera pixels. This cache-join repair bundle has **not been activated**. The parent owns review, any bounded live trial, and rollback.

## Immutable ARM bundle

```
/home/steamos/dev/frametop-native-lifecycle-observer/native-portal/hooks/build/bundle-893c49a8941c39f9
```

The matching local bundle is in this worktree's ignored `native-portal/hooks/build/`. Artifact files and manifest are mode 0400; the bundle directory is 0500. The manifest pins every hook source/test plus each shipped artifact. Its identity now includes source hashes, so changed tests or build sources cannot silently reuse an older manifest. This repair did not modify or activate any prior bundle. The prior bundle was used in the parent-owned live trial described below.

| Artifact | SHA256 |
|---|---|
| `libft_native_hooks.so` | `028069627207af8d0567ee401406326eabbfc16c1e258eb402bc3cee870c7df9` |
| `decode.py` | `722fa4b7ed9926623241f12939f5a865cb22efba907814813e1f930d90492666` |
| `pins.h` | `412197e1298c8b45683dc0cdbd39105fb04516e25df54e9c6fd541ae302f4e34` |
| `manifest.json` | `cadb9a20fbca62014cebb7f08f16f9a1a4207b9374397400cb765e57e415e008` |

The only supported executable remains `/opt/steamvr/bin/linuxarm64/vrcompositor`, SHA256 `9b1bd5fe4463592cbc71b2517b9ceb0a0fcd66cd6885a19dca4be0784bf6c265`. There is no discovery/fallback for another executable. Private ELF/disassembly evidence and generated artifacts stay ignored; no installed proprietary binary is committed.

## Live-reproduced repair and CPU evidence

The parent-owned `observer-live-02/metadata.bin` trial reproduced two defects in the prior bundle. Of 4064 `cache_poststore` records, 4062 attempted tag11 reads at `0x68c` or `0x68d` because x0 was a Boolean at the unchanged join. Every join also counted as a resource edge, repeatedly requesting another three-frame window. Normal admission exhausted at 3.953s and critical admission at 24.008s, before useful late camera/backend evidence. The trace reported 793709 drops, 17278 critical-budget drops and 776366 normal-budget drops, with zero scope/reentrancy errors. The repair does not claim that missing camera/backend records prove no native work occurred.

The callback now uses the exact x20-derived address and existing comparison/store-path taps. No hook moved, no pin changed, and all 55 sites remain. Budgets, defaults, policy-float triggers and camera timing are unchanged. The old trace had 25 critical policy snapshots; that is not evidence that steady policy fields require another trigger change.

Strict production RED evidence is retained in `build/cache-red.txt` and `cache-replay-red.txt`. Before editing the callback, the steady join failed its exact tag11/address assertion and the realistic schedule failed its budget assertion. Pinned instruction replay independently reached the same wrong-address assertion. Decoder RED is in `cache-decoder-red.txt`.

GREEN includes the full original suite plus four repair tests. The new 120-second 90Hz schedule uses the live trial's `SAMPLE_EVERY=600`, `WINDOW_FRAMES=3`, 21600 cache joins, 183600 resource-stamp pairs and four waits per outer invocation. It accepts 4635 normal and 9 critical records with no drops/read/scope errors, retains both-eye late camera records and final release at frame 10501, and preserves 122 deferred backend entries after outer return. This is bounded synthetic evidence, not a repaired live capture. Another 10800 malformed/unknown joins do not reopen windows or exhaust admission.

The replay test executes the private hash-pinned instructions for stable 0, stable 1, 0->1 and 1->0. Native and production-island runs match every GPR, full SIMD/FP state, continuation PC, native memory effects, acquire/release call count and original LR. Emulator stubs replace only acquire/release function bodies, not the branch/load/store path. Their register snapshots feed the real production C callback across thousands of joins. The original 330 all-site machine-state comparisons and six pthread drain schedules still pass. Local and ARM suites both report 12 tests, OK.

The fresh bundle is immutable and source/artifact hashes were compared on both hosts. The old `bundle-b430c166d75c9d23` manifest and artifacts are unchanged. The new bundle passed default-off and twice-opted-in wrong-executable `/usr/bin/true` checks on the actual SteamOS host. No activation, service/camera/pose/configuration or selected-mirror write occurred. Live confirmation remains the parent's next bounded trial, not an acceptance claim from this worker.

## Parent-only launch settings

No launcher, service, configuration or live process was changed.

| Variable | Contract |
|---|---|
| `FT_NATIVE_HOOKS_ENABLE` | Exactly `metadata-v1-9b1bd5fe`; absent or anything else is off |
| `FT_NATIVE_HOOKS_LOG` | Absolute fresh filename in an existing canonical user-owned mode 0700 directory |
| `FT_NATIVE_HOOKS_SECONDS` | 1..120, default 60 |
| `FT_NATIVE_HOOKS_PROFILE` | Absent preserves legacy schema-1 recording; exactly `lifecycle-v2` selects the new mode; other values refuse |
| `FT_NATIVE_HOOKS_SAMPLE_EVERY` | Lifecycle only, 1..10000 outer invocations, default 120 |
| `FT_NATIVE_HOOKS_WINDOW_FRAMES` | Lifecycle only, 1..8 consecutive outer invocations, default 3 |
| `LD_PRELOAD` | Parent appends the absolute bundle DSO to the existing value, preserving the existing testbench preload |

For a 120-second lifecycle trial the parent can choose `SECONDS=120`, `PROFILE=lifecycle-v2`, `SAMPLE_EVERY=600`, `WINDOW_FRAMES=3`. These are observer settings, not camera or owner authority. No settings listed here enable camera rendering. Preserve stock Vulkan loader discovery. Presence of `FT_AURORA_GATE_ENABLE`, `FT_NATIVE_CLIP_DIR` or `FT_CAMERA_BLEND_MODE` refuses installation.

Exact executable-path refusal runs **before opening the log**, including service-wide inherited preloads in vrstartup/vrcmd. Default-off and wrong-executable loads do not create a file or thread. The fresh output is mode 0600, O_EXCL/O_NOFOLLOW, a regular file with one link, in a canonical private parent. Full file/inode/hash/load checks remain separate and mandatory.

## Bounded sampling and loss contract

Legacy mode retains the previous fixed 2384-byte schema-1 records, 256-entry ring and 16384-event stopping cap. New sites are ignored by the callback in this mode. The additional installed islands still preserve native state.

Lifecycle mode uses two independently bounded queues and lifetime admission budgets:

| Class | Queue records | Accepted-record budget | What goes here |
|---|---:|---:|---|
| Critical | 128 | 4096 | Both raw external wrappers; all acquire/release entries and real returns; layer transition/acquire results; validated cache store transitions; changed or unreadable compact stock-policy snapshots |
| Sampled | 256 | 16384 | Coherent frame windows, camera/record/backend metadata, selected submission/stamp/wait/reset boundaries |

Normal exhaustion does **not** disable critical observation. Critical exhaustion also does not end normal sampling. Both stop at the duration or shutdown. A shared single-attempt try-lock preserves publication ordering. It can drop a critical event while a normal producer holds the lock, so the reserve is **not a lossless guarantee**. Ring full, contention, budget exhaustion, reentrancy, scope failures and metadata read failures remain visible. A write failure ends draining with code 21 if the file still permits a control record; absent/truncated terminal records are incomplete, not success.

The build calculates and pins the maximum successful trace size: `(16384 + 4096 + 3) * 2384 = 48,831,472 bytes`, about 46.57 MiB, independent of duration or native callback frequency. This includes prepared, installed and terminal controls. Actual callback rate and resource-edge count are not assumed bounded by this disk limit. Output exceeding either event budget is explicitly lost and counted.

Selection happens once at outer entry, never by unrelated per-event modulo. Every Nth outer invocation starts a configured consecutive-frame window. A stock-policy change or resource edge requests the **next whole window**, including when the edge occurs on another thread. The current unselected frame is not retroactively labeled complete. Critical events themselves are retained independently. All outer counters, scopes, eye masks, policy-load presence and bounded call stacks continue to update for unrecorded frames.

The selected interval stays open after outer return until the next outer entry, covering the previously observed same-thread deferred backend schedule. Backend entry carries its selection through pipeline lookup and both real returns. Wait entry carries selection and its immutable token through result/return. Other-thread events inside an open interval are observations only; they are not assigned another thread's frame. `frame_context=0` is preserved for deferred/no-scope events.

A selected window can still be incomplete because of contention, caps, deadline expiry, filtering/read failure, unmatched scopes, or work outside the interval. Require both eye records and every relevant native boundary in the decoded evidence; never interpret selection flags as proof of complete stereo or GPU lifetime. There is no claim that a finite trace proves future scheduling. Normal-budget exhaustion can remove later full windows while still retaining the final critical release.

## Exact additional sites

IDs 0..24 remain unchanged. Every new 16-byte window was read from the hash-pinned local ELF and verified against the ARM installed file through the existing identity tests. All new sites are in the production emitter's preservation tests.

| ID | Event | ELF offset |
|---:|---|---|
| 25 | `cache_poststore` | `0x9eebc` |
| 26 | `record_complete` | `0x29b548` |
| 27 | `backend_return` | `0x29f518` |
| 28 | `backend_return_cleanup` | `0x29f550` |
| 29 | `arena_reset` | `0x2987d4` |
| 30 | `submit_call` | `0x3021d4` |
| 31 | `submit_result` | `0x3021d8` |
| 32 | `submit_stamp` | `0x302208` |
| 33 | `submit_stamped` | `0x30220c` |
| 34 | `resource_stamp` | `0x310ea0` |
| 35 | `resource_stamped` | `0x310f20` |
| 36 | `wait_enter` | `0x3160b8` |
| 37 | `wait_result` | `0x316110` |
| 38 | `wait_return` | `0x316120` |
| 39 | `wait_return_error` | `0x316150` |
| 40 | `renderer_wait` | `0x7410c` |
| 41 | `renderer_wait_return` | `0x74110` |
| 42 | `policy_style` | `0x112ab4` |
| 43 | `policy_forced_initial` | `0x112ac4` |
| 44 | `policy_enable_special` | `0x112aec` |
| 45 | `policy_special_scalar` | `0x112af4` |
| 46 | `policy_ordinary` | `0x112afc` |
| 47 | `policy_forced_inactive` | `0x112b10` |
| 48 | `policy_request` | `0x112c10` |
| 49 | `policy_forced_alpha` | `0x112c50` |
| 50 | `policy_special` | `0x112c70` |
| 51 | `policy_construct` | `0x112c78` |
| 52 | `policy_suppression` | `0x112cfc` |
| 53 | `policy_forced_final` | `0x112d14` |
| 54 | `policy_base` | `0x112d2c` |

The original inventory remains outer `147a48/148414/148de0`, policy `112b58/122568/1480b8`, layer `148244/1482a8/14834c/14840c`, enqueue `161ed8`, backend/lookup `29f1e8/29f204`, external wrappers `6d6e0/6d6f0`, tracking `63920`, comparison/join `9ee98/9eeb4/a0510`, acquire `15f9e0/15fa78/15faa4`, and release `15fba8/15fbd8/15fc20`.

### Resource and recording contracts

- Acquire/release entry captures K, original SP and caller LR in a bounded TLS call stack even when publication fails. Real RET captures the unchanged raw x0 and reads K+340..+367 from that **entry K**, never restored x19. Missing/mismatched entry is flagged, not invented. Retained nonzero handles after release are metadata, not proof of activity or failure.
- `9eebc` is a common join after the optional native store, not an unconditional store completion. It reads `x20+0x25000+0x68c`, which is valid on both branch arms. On the unchanged arm, `ldrb w0` has replaced the x0 pointer with the cache Boolean. No native call or precommit observation replaces the post-commit read.
- Existing `9ee98` captures a fault-safe precompare byte in the current outer scope. Existing `9eeb4` marks the taken store path. Association requires the same layer SP, x20-derived address and Boolean w27 target, plus valid outer scope and matching post-read. A committed change also requires the store-path marker and restored owner x0. A validated unchanged join requires Boolean x0 and no marker. The comparison is consumed once, reset on outer entry, and never carried across scopes. The precompare byte is an observer read, not an assertion of an atomic native load/store transaction. Inconsistent evidence remains unknown.
- Only validated committed cache changes use critical admission and request a future whole window. Unchanged joins remain ordinary sampled metadata. Unknown joins set flag16, retain tag11 read failure when applicable, and never request a window. Fault-safe failures also increment the existing counter even outside selected windows. External/acquire/release and real store-path result edges retain their prior admission behavior.
- Enqueue retains existing K+0xc0 native528, this draw's SP+170 raw-to-Standing matrix and original packet prefix. No getter call, cold origin initialization or OpenVR initialization is added.
- Enqueue also captures R+1380 and wrapper+8, R+1388, arena R+1390 with buffer+0xb0 and size+0xb8, mapping/cursor R+1410/+1418, full u64 alignment R+328, pass-vector bounds R+15a0/+15a8, both packet UBO wrappers' +8/+18 fields, and 32-byte blend attachment at blend+8. Resource size is the u32 at +0xb8; the adjacent captured word has no declared meaning.
- Source name is read one byte at a time up to NUL or 15 bytes. A full 15-byte source prefix does not establish full source length. Recorded name captures existing pointer+160, exact u64 length+168, and at most 15 bytes from that pointer. No name/ticket rewrite occurs.
- Recorder completion uses common epilogue `29b548`, before x21/SP restore: R=x21, source=`[SP+98]`, destination=`[SP+60]+150`. This is later than the branch-specific `29b518` candidate and follows the descriptor/name snapshots. It captures packet prefix, nine descriptors and name plus renderer metadata. Source VS57/58 filters camera records. Backend retains both original stage enums; PS127 and PS130 remain distinguishable. A valid-looking pointer or matching prefix is not an authenticated generation.
- Backend return sites use captured entry packet/SP/LR, not already-restored x24. CPU returns are not submission or completion.

### Submission, wait and reuse contracts

`3021d4` is the existing resolved submit call, with command x19; `3021d8` captures raw w0 before it is moved to w20. `302208` is reached on native submit success and supplies x0 command, x1 sync object and x2 pointer to the exact u64 serial plus signed subserial byte. `30220c` follows native stamping. Header registers preserve those identities; chunks retain mutable command/resource stamp metadata separately.

`310ea0` captures resource, sync object and requested serial/subserial. `310f20` reads resource x19+10/+18/+20 after either native update path. It is not a new guessed completion function and does not equate an unstamped object with retired work.

`3160b8` captures the actual native wait's synchronization object, timeout, existing semaphore+58, and an **entry copy** of serial/subserial. `316110` captures raw VkResult before comparison; `316120/316150` capture actual native Boolean returns with that entry token after caller registers are restored. Native renderer wait call `7410c` records R and R+1388; `74110` records its returned value only if the call was observed, not when the preceding null branch skipped it.

`2987d4` records R=x22 and cursor **before** the native zero store. This locates reuse precisely. To claim a completed native timeline, require matching command/resource/sync/serial/subserial, submit result, successful native wait return, and subsequent reset ordering in the actual trial. Later mutable stamp values, elapsed time, nonnull pipeline, outer/backend return, or a null synchronization pointer cannot substitute. Untaken or unsampled boundaries leave the claim unproven. Resubmitted native commands and resources not observed in the selected interval still need their own evidence.

### Branch-specific policy input

Policy taps retain actual loaded register values at the exact paths listed above. The schema records original style, enable, special pointer/scalar, ordinary versus special branch, ordinary/construct request, four forced-state loads, suppression result and base-scalar bits. p0/p1 and S+62c..+64f come from the compact pre-feedback snapshot. This includes the copied gain and output alpha/style; no policy helper is called twice.

Tag30 carries a presence bitmap, conflict bitmap and fixed u64 slots. Decoder emits `value: null` for untaken inputs, not false/zero. Repeated values that disagree, including differing forced loads across branches, retain conflict bits. A single taken source does not establish the inactive request source. This instrumentation does not itself authorize a production PolicyInput or any counterfactual; unresolved/contradictory input must remain unresolved. Nested/unwound or concurrent unsupported schedules are not certified by these observations.

## Wire format

Both versions use fixed 2384-byte little-endian records with `struct.Struct('<4I6Q4I32Q')` headers. Schema1 magic is `0x3154464e`; lifecycle magic is `0x3254464e`. The decoder accepts both and rejects unknown magic/size, truncated records/chunks, excessive used lengths and unknown site IDs. Existing tags 1..12 keep their meaning. Tag41 adds a 16-byte `<4I>` cache association with precompare byte, target w27, post-read byte and status 0=unknown, 1=unchanged, 2=committed-change. UINT32_MAX fields decode as null. Derived tag41 is an observer classification, not native storage. All tag names are in the bundled decoder; no live process access is required.

Headers retain publication sequence, actual TLS frame, per-thread callback sequence, TID, MONOTONIC ns, owner, depth/eye/mask, original x0..x30 and original SP. Payload chunks are `<IIQ>` tag/size/original-address plus bounded bytes. High tag bit means read/association failure; failed bytes must not be interpreted as native values. Derived copied-call/policy chunks have address zero; native source chunks retain their addresses.

Flags retain 1=no outer scope, 2=ambiguous/recovered scope, 4=failed read, 8=payload refusal. Lifecycle adds 16=unmatched/invalid call or cache context, 32=critical admission class, 64=selected-window class. A changed policy record may have flag32 without64 and does not start a partial frame window.

Control site is `0xffffffff`; codes 0/1/2 are prepared/installed/drained. Existing refusal/fatal codes remain, with 20=invalid sampling configuration and 21=drain write failure. Controls still start with the exact ELF SHA. Original x0..x5 counters remain PID, total drops, read failures, filtered noncamera, reentrant, scope errors. Lifecycle x6..x12 add critical queue/contention drops, critical budget drops, normal budget drops, intentionally sampled-out callbacks, write failures, normal accepted count and critical accepted count. x13..x17 pin interval, window length, budgets and max file bytes. Reentrancy is separately counted, not silently folded into critical drops.

The drainer chooses the lower publication sequence across both queues. Closing admission, checking both queues empty, and producer admission recheck use the same ring lock. Accepted events precede the terminal record unless file writing fails. No file format claims losslessness or native transaction atomicity across separate reads.

## Preservation and verification

Startup retains exact executable/inode/hash/ELF/load-bias/private-RX mapping checks, all site guards, single-thread launch, blocked signals and all-or-none precommit permission handling. Text pages become RW without X before writes and RX afterward. Precommit failure rolls back protections; postcommit RX/postcheck failure exits190. A bounded MAP_FIXED_NOREPLACE island search never replaces another mapping. No guard was relaxed.

Islands save x0..x30, all full q0..q31, original SP, NZCV, FPCR and FPSR before C; errno is preserved. Retained BL/BLR use original native LR; CBZ/CBNZ/B.cond, B, RET and pinned ordinary instructions preserve their exact effects. New conditionals use the existing two-branch relocation encoding, not a general relocator.

Callbacks use fixed storage, TLS, atomics, one try-lock and self-only fault-safe process_vm_readv. They do not allocate, perform file I/O, block on a mutex, or call the renderer/VR. Kernel reads can fault or perturb timing; there is no zero-overhead claim. Forked children disable observation and do not join a vanished drainer. Deadline expiry does not detach branches or unmap RX islands; process exit owns cleanup. Never dlclose this observer after installation.

Verified against this source and ARM bundle:

- ARM GCC16.2.1 production DSO, native synthetic island, exact-file mapping, installation/rollback/fatal-RX fixtures, fork/errno/default metadata and deadline drain all passed.
- Production emitter generated 165 islands for all55 sites at three synthetic load biases. Unicorn executed330 cases, including both outcomes of conditional sites, BL/BLR/RET, every GPR/full SIMD/FP state, native continuation PC and native memory effects.
- Actual C callback fixtures exercise every new site and both decoders. They prove captured entry K survives restored x19, immutable wait token survives source mutation, both-eye selection, absent/conflicting policy loads, full alignment width, independent queue/budget exhaustion, cross-thread edge window triggering and backend selection through pipeline return.
- Virtual-clock 120-second fixture executes10800 outer invocations, captures219 complete stereo frames in2639 records/6,291,376 bytes, and retains final release at frame9001. This is a synthetic callback schedule, **not live stock off/on/off evidence**.
- Six real-pthread drain schedules cover both queues: publication before deadline closure and inflight producer admission after deadline or explicit stop. All passed. Legacy tests remain unchanged in behavior; the original8 tests plus4 cache repair tests report12 tests, OK, on both local CPU and ARM.
- Actual SteamOS host `/usr/bin/true` preload checks passed default-off and both opt-in wrong-executable launches, leaving the fresh log path absent. No compositor was launched.
- Local/ARM source/artifact hashes and 0400/0500 modes were read back and verified. Evidence is under ignored `build/arm-cache-repair-build.txt`, `repair-local-tests.txt`, `repair-arm-tests.txt`, `repair-host-inert.txt`, `cache-test.txt`, `cache-replay-test.txt`, `preservation-test.txt`, `native-test.txt`, `drain-test.txt`, `lifecycle-test.txt`, `emitted.json` and the bundle manifest. Prior independent approval of unchanged contracts stands. Parent confirmation should cover only the two live defects and this repair delta; no independent reviewer CLI is installed in this worker.

Rebuild only in the dedicated mirror, not a live checkout:

```sh
rsync -a --exclude build --exclude __pycache__ native-portal/hooks/ \
  frame:/home/steamos/dev/frametop-native-lifecycle-observer/native-portal/hooks/
ssh frame 'cd /home/steamos/dev/frametop-native-lifecycle-observer/native-portal/hooks && ~/.local/bin/distrobox enter dev -- python3 -B build.py'
```

`build.py` refuses non-AArch64 and performs only offline/synthetic tests. Both ARM and local replay tests need Unicorn 2.1.4 in `build/pylib` or the Python environment. The dedicated ARM container has no pip; this build downloaded the AArch64 wheel locally and extracted it only into that mirror's ignored `build/pylib`, without changing installed packages. `test_cache_replay.py` reads the private ELF from `FT_HOOK_TEST_ELF`, defaulting to the pinned installed executable, and verifies the entire SHA before emulation. Local invocation:

```sh
FT_HOOK_TEST_ELF=/home/jakeg/.hermes/work/frame-native-reuse/camera-analysis/vrcompositor \
  python3 -B -m unittest discover -s native-portal/hooks -p 'test_*.py' -v
```

No proprietary fixture is committed. Prior bundles remain usable with their original decoder/pins; do not replace their files. Parent activation and stock-camera/GPU ordering acceptance remain separate from this build.

## Reusable constraints

Keep event-budget closure separate from producer admission closure when critical evidence must survive heavy steady-state sampling. Carry entry object/token/selection through restored-register returns. Read native recorded names rather than inventing identity in pointer reuse, and keep selected windows, authenticated packet provenance and actual GPU completion as separate facts. Treat a common branch join as an edge only after proving the store path. A pointer register can become a Boolean on an unchanged path. Test both arms with actual pinned instructions and sustained realistic callback volume; a fixture that always supplies an owner pointer misses both the association error and its sampling flood. These lessons are kept here because this task only owns hooks and this document.
