# Runtime camera integration

## Implemented executable path

`Runtime::Impl` owns `CameraOwner` for the whole process camera lifetime. The
production build links `camera_owner.cpp`, `camera.cpp`, `camera_gate.cpp` and
`owner_coordinator.cpp`. `runtime_camera.inc` is part of the actual Runtime
translation unit and build identity, not a separate demonstration owner.

The merged `runtime-sites.json` installs these 18 boundaries:

- Completed publication `8dc1c`, using x26 camera and x19 application.
- Precleanup/destruction `8b020`, `15e1c8`, `15df70`, and conservative rebind `162360`.
- Prelookup application trampolines `6d6e0`, `6d6f0`.
- Connection handlers `1366b0`, `136118`, lost-pipe `13a100`.
- Policy paths `15d730`, `15d198`, `15d248`.
- Native acquire/release `15f9e0`, `15fba8`, Set/Clear `15f938`, `15faf8`, and update `15fcc0`.

The loader emits a separate original-method bypass at offset 1536 in each of
17 method islands. It validates the whole group before patching. Runtime uses
those bypasses synchronously, then selects the original method's return route.
The saved-state AAPCS64 transport supplies x0-x8 and q0-q7 and captures integer
and vector results. The original native return is never replaced with a queue
acknowledgement or guessed success.

`Runtime::camera_coverage()` reports the loader's actual installed mask. All 18
bits are `0x3ffff`. Its `supported` result additionally requires current Runtime
capability. Standalone `CameraOwner::coverage()` deliberately cannot claim loader
installation. CPU fixtures do not install native executable bytes.

## Lifetime and queue behavior

Publication is recorded on bootstrap before graphics initialization. Only the
existing Evidence authentication of the actual Run call transfers the pending
publication to the worker and binds the real gate. The closing-barrier ticket is
acquired at bind. There is no manufactured Run publication event and no relaxed
owner check. Run's actual incoming LR is derived by executing the pinned thread
thunk, which yields `182aa8`.

A listener reserves a bounded FIFO slot and retains its gate ticket before its
callback can look up the camera or dereference the application trampoline. The
suspended caller keeps its native request arguments alive until the real result
returns. Owner-direct calls reserve in the same FIFO. They never self-wait or
overtake an earlier retaining slot. Runtime callbacks pump the owner FIFO.
Nested native paths stay inside the original retained invocation and still
check application/camera/context receiver identity.

No queue or gate mutex spans native execution, IPC, GPU operations or joins.
A listener that cannot complete within 250 ms takes the explicit fail-stop path.
Cleanup closes admission and cancels unexecuted work before draining. The full
original cleanup is withheld until the barrier can really release. Exact native
join success is still required on bootstrap cleanup; timeout and failed join
remain fail-stop even if native code cleared its handle and TID.

Logical stock and foreign demand are distinct from portal tokens. Connection
handler return=1 is not acquisition success: the pinned handler updates its
connection's request marker, which is read only after retained execution. That
marker is logical intent, never a service ownership or detach certificate.
Unclassified lost-pipe identity stays conservative. Native writer calls that
could invalidate already-recorded source work are withheld with fail-stop while
render/source debt exists. General co-owned source mutation during that window
is not claimed as supported.

## Existing settlement, explicit provider boundary

`RenderDebt` binds the actual Adapter ticket watermark before deferred pair
commitment. It uses the existing Command and Lifecycle certificates. The gate
barrier survives Off and remains held through exact GPU completion, retained
record disposal, executable reset and original source-consumer settlement.
Backend has a separate pending hold for this retained camera debt so Adapter
CPU/GPU reclamation cannot briefly report completed Off while source debt remains.

`SourceAdmission` is the one explicit, construction-time provider interface.
The default production Runtime passes null and stays unadmitted. There is no
exported source-ready setter. A provider must authenticate readiness, actual
per-queue outcomes/ownership and reattach revalidation. Partial service outcomes
and aggregate Release=1 without real disconnect evidence do not gain authority.

The source callback does **not** demand a new DMA fence. B4's current replacement
keeps the original sampled resources, renderer and native command. The missing
production binding must prove that preserved native source-consumer path and
provider facts, not invent stronger producer guarantees or treat app+25668 VSYNC
as a GPU fence. The fixture's source return and provider/header facts are clearly
modeled inputs, not native evidence.

Rebind `162360` is installed and conservatively closes a bound generation. Its
specific receiver-to-camera association remains unresolved. Runtime's binding
association field denotes the authenticated scheduler/application relation, not
a claimed `162360` receiver identity. A new camera lifetime cannot be constructed
over old debt. Live replacement and unknown association changes remain unsupported.

## Reproducible CPU verification

Use the existing pinned ELF via `FT_HOOK_TEST_ELF` and the existing Unicorn /
pyelftools directories in `PYTHONPATH`.

```
python3 native-portal/live/camera_install_test.py
python3 native-portal/live/camera_runtime_test.py --matrix
python3 native-portal/live/camera_runtime_test.py --sanitize --matrix
python3 native-portal/live/camera_owner_test.py --sanitize
systemd-run --user --scope -p MemoryMax=256M -p MemorySwapMax=0 \
  python3 native-portal/live/camera_loader_test.py
systemd-run --user --scope -p MemoryMax=256M -p MemorySwapMax=0 \
  python3 native-portal/live/camera_call_test.py
```

The runtime runner serially compiles and executes under 256 MiB with zero swap,
including ASan/UBSan. Its positive path uses the real private control server,
bootstrap publication, a real worker handoff, foreign application requests,
nested original calls, eight complete stereo/Off/settlement/reattach cycles,
modeled exact native join and bootstrap cleanup. Negative cases omit publication,
provider admission, GPU completion, CPU disposal or source return; omit one
service receipt; attempt a source-invalidating writer over debt; or report a
native join timeout/error. Each intended fail-stop is identified before SIGABRT.

The merged loader test executes all selected original routes against independently
executed pinned instructions, plus all camera return/refusal/bypass routes. It
also tests actual installation rollback, RX protection, exact ELF pinning, fork,
TTL and bypass address publication. Its native service and GPU boundaries remain
modeled. The assembly-thunk test requires the existing `keystone` Python package.

This host lacked OpenSSL development headers. Verification used extracted Debian
`libssl-dev_3.0.17-1~deb12u2_amd64.deb` headers in the ignored build directory with
the installed `libcrypto.so.3`, through `C_INCLUDE_PATH` and `LIBRARY_PATH`. No
system package or live runtime was changed.

No headset, SSH, native runtime activation, live source admission, GPU rendering,
or ARM64 production deployment is claimed. The old unrestricted router fixture
suite predates mandatory camera publication/provider admission; the focused
camera runtime matrix is the verified new entry path, not a claim that every old
fixture mode has been migrated. Independent integration review remains parent-owned.
