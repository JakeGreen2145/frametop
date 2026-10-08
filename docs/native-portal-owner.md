# Native portal owner and control adapter

This module implements the ft-screens owner, a real private compositor control server, and CPU policy/demand/frame adapters. It does **not** install compositor hooks, change Valve camera state, start a camera reader, or claim that native rendering works. A default `Backend` is unsupported. Nothing in ft-screens starts a server or advertises a synthetic camera.

## Commands and ownership

The existing `ft_vr_command` dispatcher handles:

```
passthrough portal on
passthrough portal off
passthrough portal status
```

All return `ok portal ` and bounded ABI-1 JSON with exactly `abi`, `wanted`, `state`, `reason`, `epoch`, `submitted_frame`, and `camera_freshness`. Freshness is always `unknown`. States match `docs/native-portal-settings.md` in the integration tree. On/Off acknowledge intent without waiting for IPC or camera startup. Off stops this owner's request, never claims that Valve or another application's camera is off.

The JSON epoch is the ft-screens intent generation. It increases for accepted On/Off commands and does not reset when asynchronous socket teardown completes. The separately issued compositor lease epoch remains internal to the binary transport. Worker replies are tagged with the intent that initiated them, so a late reply cannot become proof for a newer On. An immediate On/Off/status reply cannot contradict its `wanted` field.

`native_portal::Owner` lives in ft-screens. GUI disconnection has no effect. Intent is not persisted and never inferred from `Volume.enabled`. Geometry, controls, layer preference, and their existing commands are unchanged. At the end of the real `ft_vr_poll`, after cancellation and release processing, the owner publishes live `g_passthrough` center, size, and `Ry(yaw)*Rx(pitch)*Rz(roll)` quaternion. It does not consume a committed-release snapshot. Paused, invalid/disconnected/non-Running_OK HMD tracking, and native menu suppression stop owned rendering. The above-Steam preference still affects controls only; native camera output stays suppressed during menus until a separate ordering adapter is proven. Native shutdown closes the owned connection before OpenVR teardown.

The worker publishes at 50 ms intervals with a 250 ms TTL. Its sender timestamp belongs to the VR-thread geometry sample, not the worker's heartbeat. A stalled VR tick therefore cannot renew stale geometry indefinitely. Tracking/pause recovery reacquires while native intent remains wanted. Missing/unsupported backends and revoked transport epochs require another explicit On; they do not silently fall back to a camera process. After a menu suppression, an old pre-menu submission cannot make status active again.

## Private socket

`Server(runtime, backend, clock)` runs one control thread and owns only:

```
$XDG_RUNTIME_DIR/frametop-native-portal/control.sock
```

The runtime and subdirectory must be owned by the effective UID and mode 0700. The server creates a mode-0600 `AF_UNIX/SOCK_SEQPACKET` socket. Both peers check `SO_PEERCRED`, UID, and `/proc/PID/stat` start identity. The server detects dead/zombie owners even when another process still holds a passed socket descriptor. This authenticates a process connection, not an executable pathname or a native camera refcount. Same-UID processes are inside the local trust boundary.

There are eight bounded connection slots, one lease writer, and a 5 ms control poll. Idle/nonwriter clients do not renew a lease. Existing socket, ordinary-file, and symlink collisions are refused, not unlinked. Stop removes only the exact socket inode created by this server. Render callbacks perform no socket, `/proc`, file, logging, or allocation work.

Requests implement the exact 128-byte little-endian `FTNPORT1` offsets from `frame-native-product-design/CONTROL-ABI.md`. There is no native-struct wire cast. The parser rejects trailing/truncated data, reserved bytes, unknown operations/flags, nonfinite geometry, centers outside +/-100 m, dimensions outside 0.05..5 m, opacity outside 0..1, or quaternion squared norm more than 0.0001 from one. It never repairs a malformed quaternion. Acquire has epoch/origin zero. Its response supplies the real backend origin generation; no fabricated origin is used.

All requests carry a strictly increasing nonzero sequence. Updates require an assigned matching epoch, acknowledged origin, nondecreasing geometry generation, and identical geometry when reusing a generation. TTL is 1..250 ms. Checked CLOCK_BOOTTIME deadlines use the minimum of sender+TTL and receipt+TTL, with future sender clocks rejected. The server expires before handling queued updates. EOF, dead/reused owner PID, replay, invalid packets, changed origin, native fault, or raw external handoff revokes owned rendering. A new explicit acquire gets a new epoch. An old release cannot affect a newer epoch; same-epoch release is idempotent.

### Reply record

Replies are also exactly 128 bytes, zero-filled and little-endian:

| Offset | Field |
| ---: | --- |
| 0 | `FTNSTAT1`, 8 bytes |
| 8, 12 | u32 ABI=1, bytes=128 |
| 16, 20 | u32 state, reason, in header enum order |
| 24, 32, 40 | u64 lease epoch, last sequence, geometry generation |
| 48, 56 | u64 observed origin generation, last submitted stereo frame |
| 64 | u64 server process-instance diagnostic identity |
| 72 | u32 capability, 0 unsupported or 1 explicitly admitted adapter |
| 76..127 | reserved zero |

This version carries capability, not a pretend pinned build hash. The hook adapter must check its immutable build/version manifest before publishing capability 1. The server identity is a process-instance diagnostic, not a security token. A status request is lease-scoped and cannot adopt a foreign owner's connection.

## Hook integration points

Everything here is in namespace `native_portal`, separate from `ft::portal` aperture math. Link `control/protocol.cpp`, `control/transport.cpp`, and `control/policy.cpp` with `-pthread`; `frame.hpp` is header-only. Files ending in `_test.cpp`, `test.cpp`, and `policy_fixture.cpp` are executable test fixtures, not library sources. No shader or hook files are changed.

1. On the proven native owning thread, construct a `Backend`, a `DemandAdapter(&backend)`, and a `FrameGate(backend)`. Start `Server` outside render callbacks. The default backend remains unsupported until build/ABI/ordering/resource checks pass.
2. Feed `BackendStatus` from that single native callback owner. `capable`, `serialized`, `external_clear`, and a nonzero monotonic origin generation are evidence inputs, not settings to force on. Derive the origin from validated same-draw raw-to-Standing transforms. Publish submitted-frame fields only after real stereo submission. Metadata-only observations are not submission.
3. Call `DemandAdapter::establish(ordering_proven, native_idle)` only with actual native K/cache settlement evidence. Request=false is not idle evidence. Unknown existing raw consumers refuse new leases. Failed handoff debt persists until this explicit settlement; callback concurrency permanently refuses the backend.
4. Capture the native policy inputs without changing stock feedback. `pure_policy` reproduces the verified graph, including style, forced/special paths, suppression, and native alpha. `frame_policy` retains stock and effective outputs separately. Stock/full-view and safety inputs win. It does not rerun the native policy function or touch its feedback/timer/event writes.
5. `FrameGate::begin` latches one immutable lease/geometry/policy for a monotonic outer-frame token. It requires conservative tracking/safety admission. Bind its lease/origin/frame tag to the demand adapter before the owned layer transition. If binding fails, drop the owned frame.
6. `DemandAdapter::layer` merges stock and owned activity and calls the supplied original native transition only when required. It records source, epoch/origin/frame debt, forwarding, and the observed result. `NativeOutcome.success` is a native result only when `forwarded=true`. The real ABI wrapper must retain its actual return value separately if it is not a Boolean; the callback Boolean means observed success, not a guessed Valve signature.
7. Wrap raw external acquire/release through `DemandAdapter::external`. It invalidates the backend **before** forwarding the actual call, with the same arguments carried in the callback context. It never swallows a genuine external release. Only the extra P-only falling edge after handoff is suppressed; a genuine stock falling edge is forwarded. An atomic revocation generation prevents an old lease reviving if native idle settlement occurs between control polls. New leases remain refused while old owned transition debt is unsettled.
8. Call `FrameGate::eye` for both camera callbacks using the actual observed origin. `accept` refuses mismatched, duplicate, incomplete, expired, revoked, or previous-frame packets. If backend acceptance occurs before both eyes validate, that whole owned frame is invalidated, not half displayed. Check again at the real final submission boundary, then call `submitted` only after success. The captured frame's original deadline cannot borrow a later lease renewal.
9. `background_permission` returns stock permission unless the exact admitted portal-only counterfactual permits restoration. On a disagreement between actual native permission and the predicted effective graph, the hook must revoke the owned frame. No global Aurora exception, native fade override, camera-setting toggle, or mailbox saved-value restoration is provided.

`FrameGate` and the demand ledger deliberately require all native callbacks on the same established thread and reject recursion. Deferred/off-thread callbacks are **unsupported**, not serialized through a speculative mutex. The raw external call still forwards if ordering fails; the bridge is permanently refused. A layer wrapper that cannot enter must preserve the original stock path, not synthesize a successful result. The future ABI wrapper must audit this fallback before activation.

The atomic-word mailbox uses lock-free u64 payload words and sequentially consistent sequence operations. Reads take at most three attempts; publication admission never waits. Speculative reads cannot create C++ non-atomic data races. These snapshots do not retract already submitted GPU commands. Native resource retirement and in-flight exposure bounds remain the hook/backend's responsibility.

## Verification

Local RED was observed for missing control/frame APIs, the unimplemented native command, pre-menu submission reuse, failed-handoff ledger clearing, and asynchronous GUI state/epoch inconsistencies. Focused regression tests passed after their fixes.

```
python3 -B native-portal/control/test.py --sanitize
python3 -B native-portal/control/test.py --sanitize \
  --evidence /home/jakeg/.hermes/work/frame-native-ownership/verify_ownership.py
python3 -B screens/test/volume-controls.py --sanitize --openvr-header /path/to/pinned/openvr.h
```

The optional evidence test reads only the audited emulator setup/function prefix and executes pinned AArch64 policy instructions. It never runs the evidence script's output writers. All 2,560 C++ graph outputs matched the actual native instruction fixture.

The socket tests use actual private Unix connections, bounded fake clocks, EOF, dead owner with an inherited live socket, competing writers, malformed/oversized packets, replay, exact TTL expiry, origin change, handoff/failure, and inode/path collision refusal. CPU frame tests cover incomplete stereo, origin mismatch, expired/old lease packets, stock priority, and explicit off-thread refusal. Snapshot stress checks cross-thread coherent payloads. The actual vr.cpp adapter tests use pinned-header OpenVR fakes and private backends, with no real VR initialization or public `@ft_screens` connection.

ARM builds and both ASan/UBSan suites also passed in the new isolated mirror `/home/steamos/dev/frametop-native-owner`, with `FRAME_REPO` explicitly set. `screens/build/ft-screens` was verified as AArch64. No active checkout, service, installed runtime, user layout/configuration, pose, or camera was changed.

These are CPU/transport/native-build results. They do not establish real callback scheduling, native camera acquisition, shaders, GPU resource lifetime, optical registration, exposure freshness, or headset pixels. Parent integration and separately authorized activation remain required.
