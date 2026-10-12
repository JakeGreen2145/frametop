# Native camera aperture experiment

This experiment masks the existing native camera draws. It leaves camera
acquisition, native mesh reprojection, descriptors, the 528-byte uniform buffer,
command recording and GPU retirement with SteamVR. It does not use the live
portal runtime or its copied packets and graph observers.

`generate.py` takes the eight pinned installed shaders and produces private
`<id>.original.spv` and `<id>.portal.spv` files. It reuses the aperture generator's
smooth location-3 varying and fragment rejection. The vertex shader calculates
the aperture from its existing per-eye M1 matrix and a fixed rectangle embedded
in the shader. No uniform block members, descriptors or push constants are added.

```sh
python3 native-portal/poc/generate.py \
  --source /opt/steamvr/resources/shaders/vulkan \
  --output native-portal/build/poc \
  --center 0 0 -1 --u .45 0 0 --v 0 .3 0
```

The coordinates are in the native shader's X reference frame, where
`X = M0 * mesh_position` and `eye_position = M1 * X`. The live translation/yaw
check below identifies this with raw tracking space for the tested runtime and
camera mode. The default rectangle is only an initial diagnostic placement.
It is not a calibrated desk location.

The shader derives E from `inverse(M1) * (0,0,0,1)`. With rectangle center C and
half-extent vectors U,V, it emits `inverse(mat3(U,V,C-E)) * (X-E)` without
dividing the varying by its third component. The fragment shader rejects
nonfinite coordinates, nonpositive depth and coordinates outside the rectangle.
Both eyes therefore use their own native eye position and the same rectangle.

The generator validates hashes, preserves original native instructions and runs
`spirv-as` and `spirv-val` against every generated module. Generated shaders and
assembly belong under ignored `build/`, never in version control.

## Build and launch configuration

After shader generation, build in the dev container:

```sh
~/.local/bin/distrobox enter dev -- \
  python3 native-portal/poc/build.py --output native-portal/build/poc
```

The build embeds the generated shader bytes into `libft_poc.so` and writes
`VkLayer_FT_poc.json`. Select that library through Vulkan's explicit layer
configuration. Preload only the separate `libft_poc_gate.so` when testing over
Aurora:

```sh
VK_ADD_LAYER_PATH=/absolute/repo/native-portal/build/poc
VK_INSTANCE_LAYERS=VK_LAYER_FT_poc
LD_PRELOAD=/absolute/repo/native-portal/build/poc/libft_poc_gate.so
FT_POC_AURORA=1
```

Apply these variables to the compositor launch and preserve any required
existing layer paths, layers and preload libraries. The gate checks the exact
compositor hash and original instruction, then patches one Aurora scene gate
during single-threaded startup. Other executables ignore it. Do not preload
`libft_poc.so`; its Vulkan entry points belong in the loader's layer chain.

Restart without these launch settings to restore the original compositor and
camera shaders. Do not rebuild a library while a process has it mapped. Stop
the trial first, or build into a different output directory for the next run.

The host-only helper installs these settings temporarily and restarts SteamVR:

```sh
python3 native-portal/poc/run.py enable
python3 native-portal/poc/control.py on
python3 native-portal/poc/control.py off
python3 native-portal/poc/run.py disable
```

`enable` preserves existing preload and Vulkan layer settings, and creates only
its own runtime `99-ft-poc.conf` service drop-in. It refuses an existing drop-in
instead of overwriting it. An independent user-systemd timer automatically runs
`disable` after ten minutes. `disable` sends RoomView Off, removes the owned
drop-in and restarts SteamVR with the prior settings. Both restarts interrupt VR.

The helper records an initially active Frametop desktop's command and required
environment in private `build/poc/launch-state.private.json`. After either
restart it starts the desktop unit, recreating its transient definition if it
was unloaded. It also restores pointer, gaze and power services that were
initially active. It does not change testbench poses or tracking configuration.
This remains a temporary experiment that masks stock RoomView while loaded.

This shader change alone does not activate RoomView or ensure the scene remains
visible outside the aperture. Native system-layer clears and scene fades are
separate operations. A live trial must verify both eyes and the outside region.
Replacing camera shaders also masks stock RoomView for the duration of that
diagnostic. The experiment does not promise independent portal controls or
normal stock RoomView coexistence.

## First live image

A headset compositor capture with the experiment enabled shows a finite camera
rectangle in both eye images, with different camera perspectives and the Aurora
environment visible outside each rectangle. This reaches the binocular masked
native-camera proof of concept. The camera image includes the original native
reprojection and color processing.

The capture establishes visible binocular output and outside-region scene
preservation for this Aurora trial. It does not establish optical comfort while
wearing the headset, physical registration under head motion, or coexistence
with every application and stock RoomView mode. The private capture belongs
with local trial evidence, not in the source repository.

A second On capture at the translated/yawed pose shows the aperture displaced
to the right in both eye views, with sloping upper and lower boundaries. This
is consistent with viewing the same fixed plane from the changed pose, rather
than keeping a screen-space rectangle fixed. Aurora remains visible outside
the mask. An Off capture at that pose contains only Aurora, with no residual
camera rectangle. These are visual observations from the compositor captures;
the matrix comparison below supplies the corresponding geometric evidence.

## First live matrix check

The first four native upload observations, two consecutive stereo pairs,
contained affine M1 matrices with rotation determinants within 0.0000005 of
one. Inverting the first pair placed the eye centers at approximately
`(-0.032253, -0.001032, 0.000201)` and
`(0.033408, -0.001032, 0.000199)` in the X reference frame. Their separation was
65.66 mm. This supports using the full inverse M1 for the per-eye projection
center without transposing it.

The default rectangle at z=-1 projects in front of both eyes. Its projected
centers before optional native screen warp are approximately
`(-0.09617, -0.07695)` and `(0.09785, -0.08201)` in normalized device coordinates.
The native per-eye rotations produce opposing tilted trapezoids at this stage.
Their screen appearance alone does not indicate incorrect aperture geometry.

The following controlled pose check changed the supplied standing pose to
`(0.2, 1.6, 0)` with +15 degrees yaw. The native inverse-M1 eye centers became
`(0.16828424, 0, 0.00849824)` and `(0.23171571, 0, -0.00849823)` within numerical
precision. Their midpoint was `(0.19999997, 0, 0)`, and the inter-eye vector
rotated by the commanded yaw. Both successive moved pairs agreed.

The observed raw-to-standing matrix adds 1.6 m in Y. The measured centers
therefore match the supplied standing pose after converting to raw space.
Multiplying `inverse(M1_moved) * M1_baseline` independently recovered the
15-degree Y rotation and approximately 0.2 m X translation for both eyes.
The small translation residual reflects the submillimeter baseline pose offset.

This is live matrix evidence that the fixed aperture follows world-space
projection under the tested pose change. The default center `(0,0,-1)` in raw
space corresponds to standing `(0,1.6,-1)` for this origin. It is not evidence
of worn optical comfort, physical camera registration, or behavior after a
tracking-origin reset. A desk aperture needs geometry converted through the
current raw-to-standing transform, rather than a hardcoded floor offset.
