# Owned demand in native construct-room-view mode

The pinned compositor remains `9b1bd5fe4463592cbc71b2517b9ceb0a0fcd66cd6885a19dca4be0784bf6c265`.

## Admission

The construct branch is not an external camera owner. Its demand input is the
policy owner's byte at +0x918, loaded at 0x112c74. The setter at 0x1130a8
writes `camera.enableConstructRoomView`. Shared +0x40b45 is prior published
output, not this demand input. Ordinary mode still consumes shared +0x40b49.

This change supports only a positively identified normal construct publication:

* The new read-only observer at 0x11a3a4 sees the actual consumed override flag
  in w2 after the publisher's branch. Any nonzero flag is unsupported, including
  a noncanonical boolean or an override that happens to equal the normal fade.
* Executed callers 0x14ab80 and 0x14c2c8 derive x2 from worker+0xf88 and the
  policy owner from worker->context+0x1b0. The native prologue saves that input
  at SP+0x90; 0x11a394 restores it into x3. The observer authenticates those
  identities against the already established Run owner.
* With flag zero, 0x11a348..0x11a398 publish clamped inverse ramp values from
  owner+0x600/+0x604 into shared+0x4027c/+0x40280. The observer compares both
  actual published floats to their native SIMD results. The selector must be
  finite in (0,1], and the second scalar finite in [0,1]. No ramp is changed.
* The publication authorizes only the next outer invocation and is consumed
  once. At 0x112b58, the acquired native shared object at SP+0x60 must match,
  the owner must match x20, the consumed selector bits and both current fade
  values must match, and shared+0x402b4 plus all six handles at +0x402b8 must
  be zero. Reads occur inside the original callback's native lock scope.

The six zero handles are proved by the initializer at 0x112398..0x1123a4.
The consumer at 0x12180c..0x12181c sets the alternate-background flag when
handle zero is nonzero, and 0x1219b8..0x1219f4 copies all six handles and their
metadata. The installation API and the meaning of nonzero override inputs
remain unclassified and unsupported. They need not be guessed to authenticate
the actual zero-override/no-alternate path. This is not a generic Aurora bypass.

Missing publication, stale or consumed publication, unreadable fields,
changed identities/scalars, active override, and any alternate handle or owner
PID preserve stock behavior. No publication means startup remains unsupported
until a real normal publisher event occurs.

## Stock behavior and ownership

`normal_construct` is a separate fail-closed policy input. Special mode alone
still refuses owned output. Stock construct, request, forced demand, existing
activity, enable, tracking, safety, and finite alpha/gain checks still win.
The native pure-policy output stays separate from its owned-demand
counterfactual. Only the owned frame copy changes construct/style/alpha.
No setting, shared mailbox, native policy feedback, or fade state is written.

The local construct setter now revokes admission like the existing ordinary
setter, without changing its registers or native continuation. Actual external
camera acquire/release calls retain their existing forwarding and handoff rules.

The existing background decision at 0x9f404 still requires an exact owned frame
and agreement with the native effective decision. Permission is restored only
when stock would permit the background and owned demand alone suppressed it.
Finite aperture, immutable packet markers, warm stereo commit, exact queue/wait,
and separate GPU and recording settlement are unchanged. Release remains
pending until both kinds of debt settle.

## CPU verification

Use the existing dependency paths and `FT_HOOK_TEST_ELF`, under a systemd scope
with `MemoryMax=650M` and `MemorySwapMax=0`:

```
python3 -B native-portal/live/construct_native.py
python3 -B native-portal/live/policy_sample_test.py
python3 -B native-portal/control/test.py --sanitize
python3 -B native-portal/live/runtime_test.py --sanitize
```

`construct_native.py` executes both real caller argument sequences and native
prologues, 24 publisher cases, zero-handle initialization and alternate selection,
and both new emitted islands against displaced originals with register/status
clobbers. Its memory objects and input-work call boundary are synthetic.

The router fixture covers the supported construct path through two stereo
submissions, background permission, unchanged stock memory, private Release and
settlement, unsupported startup followed by recovery, local stock demand,
producer/identity/scalar/handle faults, external calls, immutable marker rejection,
committed stereo after stop, and missing or failed settlement proofs. The branch
sampler separately compares 5,120 native stock and owned policy executions.

The full loader test remains blocked on this host by missing OpenSSL development
headers. Manifest preparation and the directly compiled production islands are
verified separately. No headset, SSH, camera, compositor process, or GPU was
accessed. This commit is CPU-tested implementation, not evidence of live stereo.
