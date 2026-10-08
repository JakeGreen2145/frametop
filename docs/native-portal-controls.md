# Native portal placement and controls

This port adds placement, manipulation and persistence to clean experimental `9b11162`. It does not acquire camera images, activate native CV, upload camera textures, or modify upstream hand cutouts. `passthrough.enabled` remains readable and writable legacy data, not camera intent. The unused pure `Occludes` compatibility query does not connect to any screen renderer.

## Native owner integration

- `screens/vr.cpp` owns `g_passthrough`. Read it directly on the VR thread, not by spawning another geometry owner.
- `PollVolumeControls` runs immediately after `RefreshPoses`, before screen/catcher releases. It samples translation and rotation on the release tick. Geometry changes while held, not only on release.
- `passthrough::TopFace(volume)` supplies the future owner's plane. Its center is `C + R*(0,h/2,0)`, basis is `R*[X,-Z,Y]`, width is `size[0]`, and length is `size[2]`. `R = Ry(yaw)*Rx(pitch)*Rz(roll)`. Validate the volume before consuming it. All units are standing-space metres and degrees.
- Owner integration should consume the current volume after the poll loop's cancellation/visibility updates. IPC setters can replace it between ticks. Do not authorize a camera session from `enabled`, control visibility, or the geometry status suffix.
- `g_portalAboveSteam` and `VolumeMenuCovered()` expose the menu preference/policy. Controls use it already. A camera owner must enforce it separately. This patch does not claim native camera composition order or pixel visibility.
- `g_paused`, fresh HMD validity, and the native owner's own tracking/lifetime checks remain separate from camera intent. Controls permit at most 100 ms of display-only HMD grace, never new input or a continuing drag.
- Changed successful owner releases call upstream `RunLayout("passthrough-capture-release")`. Do not replace that helper with the donor launcher. It resolves `<repo>/layout/ft-layout` from `<repo>/screens/build/ft-screens`, retaining `O_NOFOLLOW` logging. Deploy the layout helper beside an isolated executable's source tree.

`controls 0`, pause, lost tracking, malformed rigid poses, and menu suppression cancel unsaved edits. Explicit valid geometry setters supersede the drag instead of rolling the new geometry back. The 3D mouse's delayed `up` does not finish a real controller's drag. A stationary click or return to the press pose does not save.

## Wire and saved format

```
passthrough set <0|1> <x y z> <width height depth> <yaw> [pitch roll]
passthrough state
passthrough state3d
passthrough committed
passthrough controls <0|1|state>
passthrough layer <0|1|state>
passthrough presentation state
```

`state` retains `ok enabled x y z width height depth yaw status`. `state3d` adds pitch and roll before status. A yaw-only setter resets both to zero. Geometry status is `no-vr` without VR and `idle` otherwise; handcut readiness is deliberately not native camera readiness. Add native owner status separately.

Controls and layer replies are exactly `ok controls 0|1` and `ok layer 0|1`. Presentation returns `ok presentation <above> <menu-suppressed>`, a policy report only. The observed Frame menu key is listed in the SteamVR package retest hint in `scripts/update-check.py`.

`ft-layout passthrough-capture` accepts no additional arguments. It holds the existing layout lock, queries `state3d`, rereads raw JSON, changes only the passthrough field and uses upstream's atomic writer. Controls/layer writers follow the same lock and raw-JSON path. Startup restores placement/preferences even with auto-arrange disabled, without a camera On command. Screens, named profiles, unrelated fields, and explicit zero pitch/roll values survive portal-only writes.

Keyboard raw releases carry their original device and button to the shared release dispatcher. Key and modifier handling stays in the keyboard poll loop. Desktop presses/drags and portal gestures exclude one another across devices, because they share one catcher. A second controller cannot repoint that catcher or release the desktop owner's button.

`ft-layout passthrough-capture-release` also accepts no arguments. After taking the layout lock it queries `passthrough committed`, which has the `state3d` format but changes only on a changed owner release or a valid external setter. Delayed helpers therefore coalesce to the latest committed placement and cannot save a later cancelled drag. The camera's live `state3d` and explicit GUI/CLI `passthrough-capture` semantics are unchanged.

## Verification

Run from this checkout, never an active mirror:

```
python3 screens/test/volume-controls.py --openvr-header /path/to/verified/openvr.h --output build/native-controls-cpu
python3 -m pytest -q tests -p no:cacheprovider
python3 screens/test/volume-controls.py --sanitize --openvr-header /path/to/verified/openvr.h --output build/native-controls-sanitize
FT_VOLUME_TEST_EXE="$PWD/build/native-controls-sanitize/volume-controls-vr-tests" python3 -m pytest -q tests/test_portal_release.py
```

The native runner compiles four CPU executables, including the actual current `vr.cpp` against a header-derived OpenVR boundary. It does not initialize SteamVR. GPU/hand/keyboard boundaries cannot silently stand in for rendering. The process-launch boundary records the exact upstream helper invocation without launching persistence into user configuration.

Coverage includes all six rotated face resizes, coupled bounds, rigid carry and Euler poles, press-relative pivots, release sampling, ownership, cancellation, external setters, head-display grace, independent controls, menu readback, resource lifetime, upstream `GetComponentState` controller tips, `ReleaseStuck`, `ReleaseAwayBy`, screen reset, and shared catcher policy restoration. `TopFace` is compared against an independently composed basis.

Original port results before review fixes:

- Python portal/layout tests: 41 passed, exit 0.
- CPU geometry/translation/rotation/actual-adapter: four executables passed, exit 0, also with ASan/UBSan.
- Upstream C controller-click and relay-buttons tests: each exit 0, compiled with warnings as errors.
- Upstream pause tests: 40 checks passed; pause-buttons: 11 checks passed, each exit 0.
- Upstream keys test on this host's `dash`-backed `sh`: 49 checks passed, two failed. Its `command -v ft-steam ft-layout ft-float` assertion assumes the shell prints all operands. This host's dash prints only the first. The unchanged test passed all 51 checks with an isolated `build/bash-test-bin/sh` symlink to `/bin/bash` on PATH. No input source or system shell was changed.

The review regressions run the production keyboard poll body with only its unrelated rendering/pose boundaries stubbed, test two-device catcher ownership in both directions, and join the real native poll/command loop to Python capture through private pipes and a real private layout lock. `tests/test_portal_release.py` covers multiple delayed helpers, newer commits, external setters, and unchanged explicit live capture. Select an isolated native executable with `FT_VOLUME_TEST_EXE` if not using the default CPU output.

Review-fix verification: 49 Python tests passed, including the original 41 and eight new cases. All four CPU executables and the new regressions also passed with ASan/UBSan. The isolated ARM build linked `ft-screens` in `/home/steamos/dev/frametop-native-controls`; it was not launched. Red-phase and final logs live in ignored `build/`. No runtime, configuration, service, VR or pose changes were made. These checks do not establish headset usability, stereo pixels or optical alignment. Independent confirmation of the repair delta remains with the integrating parent.
