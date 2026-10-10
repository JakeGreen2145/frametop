# Initial camera binding repair

## Contract and scope

Source-only repair against `e54f6ed00552fd72aaa4fd98924700ecb74354c2`.
Desktop's `UPDATED-SOURCE-CHECKPOINT.md` was verified with SHA256
`42328f8769c4ce171d8d5be997c34cf3e7d68ae42dc8017162e8c7691632cc20`.
Its retained-text ordinary order is completed object publication, first graphics
resource Init, listener Start, renderer Start and authenticated Run. Camera Init
can return false without preventing stock startup. The live reason-8 records do
not establish whether those publication and Init hooks actually fired.

No device, SSH, runtime service, vendor executable inspection, disassembly,
emulation, old native evidence generator, loader patch or manifest change was
used. Existing private headers and saved coordinates are opaque source-fixture
inputs. Native operations, memory writes and source-service outcomes in these
C++ tests are modeled boundaries, not proof of target execution or rendering.

## Repair

* `CameraNativeBinding` owns one mutex-protected candidate. Runtime no longer
  keeps independently authoritative plain `published_camera/application` fields.
  Candidate copies are immutable values, not native pointer-read permission.
* Only the first unbound same-camera/same-application Init, observed on bootstrap
  at `162360` with the supported direct-call return `92fdc`, preserves authentic
  completed P. It revokes resource readiness before the full original body. Only
  the actual successful return restores that separate readiness. Failure keeps
  P but cannot bind or admit the portal. Init cannot manufacture P.
* App cleanup, resource destruction, camera destruction/unwind, replacement,
  malformed completed replacement, repeated Init and unclassified Init revoke
  pending facts before forwarding. Initial Init completion checks the unchanged
  candidate version, so nested or concurrent cleanup cannot resurrect it.
* Start records the invalidation scope. Actual authenticated Run establishes R.
  Joining requires an authentic ready P, unchanged Start scope, the existing
  Evidence application/worker/context matches, and the actual render owner.
  Same-address cleanup/republication across Start or Run is rejected.
* Join evaluation occurs at Run, completed publication when already on owner,
  unbound original return on owner, and owner frame/pump boundaries. Publication
  arriving after R is a modeled ordering test, not a claimed native late path.
  There is one attempt per candidate version and at most four actual bind
  attempts per Runtime. The implementation never resets an issuer or dirty epoch.
* Clean pre-bind refusal is distinct from native epoch publication followed by
  barrier-retain failure. A partial bind retains its owner and identity and is
  permanently excluded from both retry and zero-obligation stock routing.
* Truly unbound stock writer and cleanup entries execute the loader's complete
  original bypass on the incoming thread. All native guards and results remain
  native. No owner replay, SourceAdmission permission, synthetic success, source
  flag clearing or demand-zero property-call suppression is involved.
* A bounded count of complete unbound invocations shares the join-decision mutex.
  Native calls run outside all join/binding/gate locks. No join can publish while
  an unbound original or its nested cleanup is running. The counter admits at
  most 64 simultaneous/nested invocations; overflow fails closed.
* `camera_bound` remains monotonic. Adapter construction requires camera-ready
  capability, and source/queue/debt acquisition requires the bound owner. A
  clean state before the first binding therefore has no integration obligation.
  Partial publication is explicitly excluded. Existing bound FIFO, barrier,
  charged-upload, CPU, GPU and source-debt protections remain unchanged.

## Diagnostic interpretation

`ft-camera-binding` is a fixed scalar record, currently 30 uint64 fields. Runtime
publishes it through the existing atomic-word Mailbox under the decision mutex.
The collector emits changed snapshots with a 32-line lifetime cap. A fatal route
first attempts the existing short `ft-camera-fatal` reason record, then attempts
one binding record, then aborts. Fatal formatting takes no locks and performs no
native reads. Each send is nonblocking with no fallback; records can be dropped
or coalesced. The fatal coordinates and binding mailbox are independently
sampled, not one transaction or an exhaustive event trace.

* `publication_pc/accepted/sequence/rejection` distinguish observed from accepted
  publication. Sequence counts local completed-publication observations, not
  native generations. Rejection 253 means an unauthenticated publisher thread;
  other values use the NativeBinding enum below.
* `pending/native_pending` both report presence of the single object record,
  including a record held by a bound lifetime. There is no second stale Runtime
  pointer copy. `version`, `invalidations`, `invalidation_sequence`,
  `invalidate_pc/class`, and `resource_ready` describe candidate decisions.
* `run_seen/authenticated/rejection` report actual Evidence Run decisions.
  `start_scope` is the authenticated renderer generation associated with Start,
  not a newly invented native thread-generation ID. Rejected Run predicates
  retain the existing `runtime/graph.inc` IDs.
* Match fields use **0 not evaluated, 1 false, 2 true**. They preserve
  short-circuit evaluation. No foreign native reads are added for diagnostics.
* `join_attempts`, `join_rejection`, `native_rejection`, `gate_rejection`,
  `epoch_published`, `barrier_retained`, `internal_bound`, `runtime_bound`,
  `partial` and `inflight` distinguish clean refusal from partial finalization.
* `first_failure` latches the first substantive join refusal. Normal waiting for
  Run, P, resource success or a running original does not occupy this latch.
  `join_rejection` remains the latest decision, including unevaluated matches.

Invalidation classes: 0 none, 1 initial Init/resource invalidation, 2 resource
rebind, 3 object cleanup, 4 replacement.

Join refusal IDs: 0 none, 1 no Run, 2 no publication, 3 original in flight,
4 invalidated Start/lifetime scope, 5 resources unavailable, 6 app mismatch,
7 worker mismatch, 8 context mismatch, 9 dirty published epoch, 10 retry budget
or already-attempted version, 11 native bind failure, 12 barrier retention
failure, 13 stopped.

NativeBinding rejection IDs: 0 none, 1 process, 2 site, 3 stack, 4 application,
5 camera, 6 register relationship, 7 existing/replaced or exhausted candidate,
8 cleanup in flight, 9 missing publication, 10 camera mismatch, 11 app mismatch,
12 existing epoch, 13 Gate publish refusal.

Gate publish failure IDs: 0 none, 1 process, 2 required fields, 3 owner,
4 existing state, 5 exhausted nonreused epoch issuer.

## Tests and changed expectations

`initial_binding_test.py` parses the production source list with `ast.literal_eval`
and accepts saved fixture/coordinate paths. It does not import evidence runners.
Every compiler and executable runs inside a verified 256 MiB, zero-swap cgroup
with `TMPDIR=/home/jakeg/.hermes/cache/scratch`. `--sanitize` enables ASan/UBSan
with leak checking and halt-on-error. `--reuse` is only an explicitly local
incremental build, not a source-freshness claim; run without it for a clean build.

RED was observed before production edits: P -> Init -> Run failed the actual
Runtime capability assertion, and the source-modeled pre-Run lostpipe and
recognized-worker Update terminated with SIGABRT. A later malformed replacement
probe also failed capability refusal before its pending-candidate fix.

The startup matrix exercises real Runtime, gate and coordinator with the default
production source provider: initial success/failure, foreign/pre-Run stock,
recognized-worker stock, modeled R-before-P, another-app P, nested/concurrent
cleanup inside Init, cleanup between P and Run, an original blocked across Run,
same-address ABA across Run and Start, exhausted Gate publish, failed barrier
retain, active rebind, unclassified/repeated Init, malformed publication,
rejected Run, malformed replacement and bound replacement refusal. Partial-bind negative testing checks the
actual reason-8 record and scalar discriminator, and exits with a different code
if native Update is reached or the refused hook returns.

The production admission fixture now includes the formerly omitted actual
resource-Init hook before Start/Run. Cold, warm, dedup and sustained-on modes each
run two independent default Runtime instances and eight complete cycles per
instance. Negative cases retain exact cleanup-refused reason 5 for partial
Connect/alias, missing source events, provider/association changes, charged
partial upload, and partial upload after the ordinary stop/join sequence.

The focused older camera matrix now models first Init too. Its absent/null/unused
provider, arbitration, stock multicaller, positive owned chain, missing GPU,
missing CPU, missing source return, foreign debt and partial receipt expectations
are unchanged. Missing GPU/CPU/source and partial receipt still fail at cleanup
reason 5; foreign debt still fails at mutation reason 9.

Precisely changed older startup expectations:

* `foreign-release` and `foreign-cleanup` were expected to abort while wholly
  unbound. They now execute exactly one modeled original on the calling thread,
  preserving its result, and return via the complete-original selector.
* `foreign-publication` was expected to abort unbound. It now revokes pending
  portal association and refuses publication authority without aborting stock.
  Bound/partially published replacement remains fatal.
* `bootstrap-release` remains successful, but now uses the accounted complete
  original bypass rather than an unaccounted fallthrough.
* `foreign-noncamera` remains untouched. No bound/debt negative expectation was
  changed to success.

Final result artifacts are under the parent's scratch directory, with per-mode
logs and JSON in `initial-binding-asan`, and legacy sink-helper results in
`initial-binding-fatal`. ARM compilation, independent delta review and any live
trial remain parent-owned. No live-camera success is claimed.
