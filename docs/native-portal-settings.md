# Native portal settings and IPC

This change adds the Native portal tab to current upstream Display Settings. It does not implement, start, install or emulate a native camera backend. The parent native owner must implement the contract below in `ft-screens`. An older binary remains usable for placement and controls, but its native portal status is shown as unavailable or unknown.

## Backend contract

Transport is the existing Linux abstract `AF_UNIX`, `SOCK_DGRAM` address `@ft_screens`. Each request is one UTF-8 datagram without a newline. The client autobinds a private abstract reply address and connects to the server, so replies from another sender are not accepted. Each query uses a fresh socket; delayed replies cannot satisfy another operation.

Commands, exactly:

```
passthrough portal on
passthrough portal off
passthrough portal status
```

Every successful command returns the exact prefix `ok portal ` followed by one JSON object. Example status after releasing the portal request:

```json
{"abi":1,"wanted":false,"state":"off","reason":"portal-owned request released","epoch":1,"submitted_frame":0,"camera_freshness":"unknown"}
```

Required fields:

| Field | ABI 1 contract |
| --- | --- |
| `abi` | Integer `1`, not a JSON boolean. |
| `wanted` | Boolean native portal intent. Not legacy placement `enabled`. |
| `state` | One of the states below. |
| `reason` | Plain string, at most 400 characters, no control characters. Explain unavailable, suspended, suppressed and fault states. |
| `epoch` | Nonnegative unsigned 64-bit integer identifying the native intent/lifetime generation. Do not decrease it between command acknowledgement and immediate readback. |
| `submitted_frame` | Nonnegative unsigned 64-bit integer. Submission evidence only. |
| `camera_freshness` | Exactly `"unknown"`. ABI 1 does not establish camera exposure freshness. |

Accepted states:

- `unavailable`: backend unsupported or unavailable. Intent may be false or true; report actual native intent.
- `off`: portal-owned request released, `wanted=false`. Not a claim that all cameras or Valve Room View stopped.
- `warming`: `wanted=true`, native startup has not reached submission.
- `active-submitted`: `wanted=true`, native submission acknowledged. The GUI says "Native portal active/submitting".
- `suspended-external`: `wanted=true`, another owner prevents this portal from submitting.
- `suppressed-menu`: `wanted=true`, native menu suppression applies.
- `draining`: `wanted=false`, native teardown is in progress.
- `fault`: report actual intent and reason. Do not substitute `off` for uncertain cleanup.

Replies are limited to 4096 bytes. Duplicate JSON keys, malformed UTF-8/JSON, unknown ABI/state, boolean numeric fields, contradictory intent/state, extra trailing JSON and oversized replies fail closed to unknown. Unknown additional object fields are allowed for forward compatibility, not used as camera proof.

On and Off must return an acknowledgement immediately after accepting or rejecting intent. Do not wait for camera readiness or drainage to finish. The client validates acknowledgement intent, sends `passthrough portal status`, then requires matching intent and an epoch not older than the acknowledgement. Both requests share one absolute monotonic deadline, 1 second by default. A generic `ok` is not acknowledgement. A timeout, rejection, malformed acknowledgement or conflicting readback is an unconfirmed operation, never false camera-off evidence. The client does not retry state-changing commands automatically.

The native process owns intent. It must not depend on GUI lifetime, a QProcess, launcher stdin, camera reader authorization, calibration cache, color fallback or exposure timing profiles. Native shutdown, ownership and rendering policy remain backend responsibilities. `wanted` is deliberately not saved and opening settings never restores it from disk.

## CLI

No Qt dependency is required for the CLI:

```
python3 display-settings/native_portal_client.py status
python3 display-settings/native_portal_client.py on
python3 display-settings/native_portal_client.py off
```

The first command is read-only. On and Off change native portal intent. Success prints the validated JSON readback and exits 0. Transport/protocol or unconfirmed-command failure prints an error to stderr and exits 1. A valid `unavailable` or `fault` status is still a successful status query; callers must examine the state. `--timeout` accepts a positive finite duration up to 5 seconds. `--socket @private-test-name` selects an isolated test server; normal production defaults to `@ft_screens`.

## Settings behavior

`native_portal_settings.py` is a Qt adapter, not an owner. Socket work and lock waits run on a worker thread, with results delivered through a queued Qt signal. One queue serializes commands, including rapid On/Off/On. Off while warming does not wait for active submission. Polling coalesces while work is in flight. A generation fence prevents a query started before a new command from overwriting that command's UI intent. Invalid replies clear current knowledge rather than keeping a stale "Off" label.

`PortalSettings.qml` is the shipped Qt Quick component embedded in the unchanged Kirigami settings shell. The native tab appears only for the `screens` backend, and `FT_DISPLAY_PAGE=passthrough` opens it. A separate "Request portal Off" button permits an explicit Off even when state is unknown and the switch cannot represent it. Closing settings stops polling only. It sends no Off and does not claim to stop camera hardware.

Independent placement and preference controls remain available without native-camera support:

- Show controls toggles resize handles, edges and the grab bar, independent of intent and legacy `enabled`.
- "Keep portal controls visible with Steam menus" stores `portal_above_steam`. Its default remains false. The label deliberately does not promise native camera composition above Steam. The native owner must separately apply its menu policy and report `suppressed-menu` when appropriate.
- Width, height, depth, X, Y, Z, yaw, pitch and roll retain the existing geometry limits. Field commits query current `state3d` before changing only the edited component. Focused or edited QML drafts survive polling while other fields update. Legacy `enabled` is preserved, not exposed as a camera switch or an unsupported desktop cut-through feature.
- Place at desk reads `head`, places the center 0.65 m forward and 0.55 m below it, and adopts head yaw. Size, legacy enabled, pitch and roll survive.
- Geometry setters validate the native legacy-yaw acknowledgement and full `state3d` readback. Existing native replies print six significant digits, so comparison allows their rounding. Geometry renderer suffixes do not become native camera status.
- Offline placement edits save preferences without claiming they applied live. Controls and layer setters require exact acknowledgement and readback. Saved and live states are shown separately.

Writes use the existing shared `ft_layout.LOCK_PATH`, raw-JSON `_write_layout_field` helper and atomic writer. Lock acquisition is nonblocking with a bounded worker-side retry, so a busy layout process cannot stall queued Off indefinitely. Only the owned top-level placement/preference field is replaced. Raw screens, named profiles and unknown keys survive. Invalid roots or validation failures are not overwritten. No native intent is persisted. Startup restore remains the existing layout implementation, with no added native On dispatch.

## Verification

Tests bind random private abstract socket names and redirect layout, lock and conf paths to test directories. No test contacts real `@ft_screens`, launches the active settings app or modifies user configuration. Fake datagrams exercise the actual client and Qt adapter; they do not stand in for a native backend implementation.

Local isolated Qt setup, if needed:

```
uv venv build/qt-venv
uv pip install --python build/qt-venv/bin/python PySide6 pytest
QT_QPA_PLATFORM=offscreen QT_QUICK_BACKEND=software \
  build/qt-venv/bin/python -m pytest tests -q -p no:cacheprovider \
  --basetemp build/pytest-native-all
```

This host lacked `libEGL.so.1`. Debian `libegl1`, `libopengl0`, `libgl1`, `libglx0` and `libglvnd0` packages were downloaded and extracted only under ignored `build/qt-libs`; `LD_LIBRARY_PATH` pointed at that private extraction. No host package installation was performed.

Actual local result: **95 passed, 1 skipped**. This includes the production Qt Quick component, keyboard-driven On/Off, numeric draft/readback behavior, bounded protocol failures, serialized commands, stale polls, raw JSON preservation and existing layout regressions. The single skip is the full Kirigami shell because those KDE modules are absent locally. It is not counted as a GUI pass.

The new isolated Frame mirror is `/home/steamos/dev/frametop-native-settings`, not the active checkout. Its distrobox dev interpreter supplies PySide6/Kirigami. A private `build/qt-venv` with system site packages supplies pytest. Execute there:

```
QT_QPA_PLATFORM=offscreen QT_QUICK_BACKEND=software NATIVE_PORTAL_KIRIGAMI=1 \
  build/qt-venv/bin/python -m pytest \
  tests/test_native_portal_client.py tests/test_native_portal_settings.py \
  -q -p no:cacheprovider --basetemp build/pytest-native-qt
```

Actual Frame dev result: **55 passed, zero skipped**, including the complete shipped Kirigami window opening directly on Native portal, both Qt implementations, private-socket CLI On/Off/status, and every native settings test. Local red-phase logs and final execution logs are in ignored `build/`. No services, runtime overrides, active app, native source or user configuration were changed.

These results do not establish native camera activation, stereo pixels, world registration, fresh exposures, composition above Steam or comfort. Those require the parent backend implementation and separately authorized live verification.
