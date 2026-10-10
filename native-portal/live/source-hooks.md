# Installed source observations

The production loader and Runtime now route the known source observations. This is not a complete source-admission provider. Production `Runtime()` now installs the separate provider described in [native-source-admission.md](native-source-admission.md); this document covers its observation hooks. No readiness setter, alternate source queue, independent camera read, extra DMA fence, or source-command replacement was added.

## Loader and ABI

`prepare.py` pins both local ELF inputs and generates separate compositor and client site tables. The merged build has 144 compositor sites and 23 client sites. Of these, 25 compositor and 23 client sites belong to this source-observation path, including the original 15 connection sites.

At opted-in, exact-executable bootstrap, `pin_client()` verifies the canonical `/opt/steamvr/bin/linuxarm64/vrclient.so` file before loading it. It takes its base from the real loader handle and program-header enumeration, not a guessed address. Verification checks the exact file digest, ELF and program headers, every file-backed PT_LOAD page's inode/device/file offset and permissions, and every byte in the immutable RX segment. The held `dlopen` reference and open file descriptor survive until process exit. Other users' `dlclose` calls cannot unload this reference. The Runtime's module query rejects forks, missing installation, and subsequent file-identity/stat changes.

Both islands are prepared before text patching. All guards, branch ranges and writable-page transitions for both modules must succeed before either module's first instruction is replaced. Precommit failures restore RX permissions without writing instructions. Failure to restore RX after a write or during rollback terminates the trial rather than returning into partly modified native code. The existing process/fork/TTL/errno dispatcher handles both modules.

This changes bootstrap by loading the pinned client before installing callbacks. Its constructors have not been exercised in the target compositor by these tests. If loading creates another thread, the single-thread check refuses installation. There is no late, multithreaded patch fallback.

The existing emitter preserves the complete 800-byte Saved ABI, including all GPRs, SIMD lanes, SP, NZCV, FPCR and FPSR. Client events use bit 63 of the existing relative-PC argument as a module tag. The Runtime strips it before calling the binding and does not pass client PCs into compositor Evidence. No original instruction, branch, LR or native result is replaced by observer output.

## Retained Runtime invocation

`CameraOwner::observe_scope` is additive. The existing mutation signature is unchanged. The owner sets its original invocation ticket with RAII, then permits synchronous observation callbacks only on the owner thread while that invocation is active. The callback receives the same gate, original ticket and checked binding. Drain uses the barrier retained before closing; it never attempts another retain after close. Outside-scope, foreign-thread and post-unwind calls cannot obtain a scope.

`source_router.inc` creates one `ClientSourceBinding` on the authenticated owner thread and retains it for the camera owner's lifetime. Provider identity and failure history therefore survive successive native calls. An explicitly null fixture admission provider does not prevent collection of useful stock observations.

Direct original Acquire, Release and Update calls start observation before invoking the loader-issued bypass. Nested original Acquire/Release entries start under the enclosing invocation's ticket. Real RET hooks finish nested calls. A missing nested return cannot use an outer application's x0 as the camera return. Original direct-bypass returns remain an independent actual return boundary. Bootstrap calls without a retained owner are forwarded and stay unadmitted.

`SourceAdmission` now has `before`, `observe`, and `after` notifications alongside its existing receipt and settlement methods. Event notifications are forwarded only after binding acceptance. Scope failure, foreign interference or lost module integrity blocks Runtime readiness and post-call admission receipts. These notifications are observations, not authority to manufacture producer generations or settlement. There is no production implementation of `ready()` or `settled()` in this change.

## Additional exact event sites

The original compositor Connect/Disconnect and client Connect entry/new/dedup/header/insertion sites remain listed in [source-provider.md](source-provider.md).

| Module | Before-instruction PCs | Observation |
|---|---|---|
| compositor | `15fa78`, `15faa4`, `15fbd8`, `15fc20` | actual Acquire/Release RET |
| compositor | `15fdc8` | actual Update RET |
| compositor | `15fee4` / `15fee8` | layout readable acquire call/result |
| compositor | `160184` / `160188` | data readable acquire call/result |
| compositor | `160164` / `160168` | layout release call/result; result PC is also the no-release join |
| compositor | `160380` / `160384`, `1605c4` / `1605c8` | both native data release tails |
| client | `3280a8`, `32813c`, `328140` | manager read entry, inner call and return |
| client | `327640`, `3276c4`, `3276c8` | manager release entry, inner call and return |
| client | `327e18`, `3274d8` | actual inner queue-object entry |
| client | `327e54`, `32750c` | shared header accessor result before native use |
| client | `327f2c`, `327f30` | metadata creation call/result |
| client | `327f70`, `327f74` | readable slot-reference increment store and continuation |
| client | `32758c`, `327590` | readable slot-reference decrement store and continuation |
| client | `3275a0`, `3275a4` | exact metadata return call and its raw return register |

Update uses observation action `None`, not a demand transition. The binding authenticates the current camera queue IDs, provider vptr and methods, metadata/data output slots at native SP+90/+98, original caller LR, manager and inner frame depths, queue-object identity, header ID, and matching metadata handles. It captures actual increment/decrement stores and their continuations. It never independently calls cameraRead or returns a handle.

Each queue keeps distinct acquire-observed, release-observed, native result, metadata/data/queue/header identity, local matched-return and metadata-call-return fields. A failed acquire is observed but not acquired metadata. A release call's success is not producer readiness or global detach. The metadata-interface return register is preserved even when native code ignores it. In particular, local `returned` does not certify that an external imported-image registry accepted a ref/unref.

Missing manager/inner entry, ref store, header identity, metadata return or matching release invalidates the observation. Nested reconnect/migration inside an Update is deliberately not certified by this single Update scope; the original path still runs unchanged. Broader reentrant and exceptional cleanup admission remains unsupported.

## Remaining facts that block admission

1. **Producer and matching publication.** Need attributable successful per-queue Create or an authenticated intact producer generation, complete name/alias/header/mapping identity and capacity, and matching committed layout/data publication. Driver `d1380`'s attempted-once latch is not successful Create. Client Connect, header IDs, membership `header+1c`, and these read observations do not supply producer history or a header generation.
2. **Complete consumer detach.** The native Update's own paired handles are now observed. Independent waiter captures, acquisitions outside this Update, producer/consumer replacement and destruction exclusions, and all outstanding handles still need their own operational lifetime observations. Disconnect erasure is not their drain. A local new-entry branch or exact-ID dedup is not independent release authority.
3. **Original source-command attribution.** Still need each original Adapter ticket watermark associated with its sampled resource, native `29abf0` acquire and `2a1e18` reverse release, actual imported-resource registry ref/unref or initial-owner conversion results, and the relevant metadata return. This must use the same accepted native source command, renderer and timeline. Existing Command/Lifecycle remain the CPU-disposal/reset and exact GPU-completion authorities. The ordinary queue metadata return above does not settle that old command.

The local B4 review does not require a stronger external DMA fence on this unchanged path. None was added. Newer DMA/native-reuse-closure contents were not transferred or accessed. There was no SSH, Desktop access, alternate transfer, remote request, Ghidra import, device activation or new module investigation.

## CPU verification

All runners and compiler subprocesses used `MemoryMax=256M`, `MemorySwapMax=0`. Expected RED was observed for absent production source sites, absent active owner scope, absent Runtime notifications, and absent Update observation support.

- `source_loader_test.py`: real loader preparation, real mapped pinned client bytes, complete RX mutation refusal, second-module guard/range refusal, every second-module precommit page rollback, fatal second-module RX restoration failure, module-tag dispatch, and 1,170 all-register/continuation emitter comparisons. Real inert preload runs also passed. This is not a target `dlopen`/constructor activation test.
- `source_provider_test.py`: 17 existing pinned compositor/client connection tests passed.
- `source_metadata_test.py`: 11 pinned original read/release tests passed with UBSan. Includes raw metadata return preservation, missing manager/inner/ref/metadata events, header/queue/metadata mismatch and metadata-creation failure.
- `source_runtime_test.py --sanitize`: seven actual Runtime modes passed with ASan/UBSan, plus the original-ticket/closing-barrier scope test. Modes cover owner-direct, queued foreign, nested calls, missing nested return, missing module, missing insertion and provider replacement. Fresh native register/read vectors are generated for Connect, Update, Release and dedup reattachment, then host-relocated into explicitly modeled storage. The Runtime, camera owner, gate, queue and binding are production code. OS mappings, native containers, property calls and physical producer state are modeled boundaries, not readiness evidence.
- `camera_runtime_test.py --matrix`: all ten existing modes returned their expected success or intended fail-stop results after its explicit loader boundary and modeled nested RET were updated.

The private vectors and logs live under `native-portal/build/`. OpenSSL development headers were unpacked into an owned scratch directory for host loader compilation; no system package or runtime library was replaced. Full production ARM64 compilation and live activation remain parent-owned and were not performed here.
