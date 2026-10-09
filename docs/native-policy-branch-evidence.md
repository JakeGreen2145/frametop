# Native policy branch sampling

## Result and remaining blocker

The pinned ELF is `9b1bd5fe4463592cbc71b2517b9ceb0a0fcd66cd6885a19dca4be0784bf6c265`.
The live trial's `policy_failed=8` rejected a legitimate missing request tap.
That absence does **not** establish a false request mailbox.

All three observed masks, `0xfdf`, `0x3bdf`, and `0x1df`, contain bit 6 and
omit bit 5. Bit 6 is the native construct read at `0x112c78`. It is reached
only from the enabled **special** branch at `0x112c70`. These masks do not
identify the ordinary disabled branch. The replay asserts this distinction
across every tested input combination, not just one matching example.

`PolicySample` now requires the inputs consumed by that actual branch. With
mask `0x3bdf`, the production-router fixture advances from missing-input
predicate 8 to the unchanged special-policy exclusion, predicate 9.
`frame_policy` also still treats special policy as external and refuses owned
output. This repair alone does **not** make the observed live policy eligible
for owned stereo. No special-policy, external-demand, enable, tracking, safety,
camera-idle, background, or native-resource guard was removed.

## Pinned control flow

At `0x112aec`, a nonnull special object selects ordinary policy directly.
Otherwise `0x112af4` compares the special scalar with zero and the native
`b.hi` selects `0x112c70` for positive or unordered values. The actual branch
tap, not a guessed interpretation of the scalar, supplies `special`.

At ordinary `0x112afc`, nonzero enable reaches the request load and its tap
at `0x112c10`. Zero enable writes construct zero and bypasses request.
At special `0x112c70`, zero enable also bypasses the input load; nonzero enable
reads construct and reaches `0x112c78`. Neither special path reads request.
Therefore an absent request may be represented as an unused false input only
after branch and enable evidence. It is never an observation of that mailbox.

The pure-policy stock and owned outputs are checked against independent
execution of the pinned instructions. Existing enable admission remains
`value == 1`; required-load classification follows native `CBNZ`, including
unsupported noncanonical enable values.

## Tap inventory

| Bit | Native PC | Meaning and consumption |
| --- | --- | --- |
| 0 | `0x112ab4` | Initial style, always |
| 1 | `0x112aec` | Enable byte, always |
| 2 | `0x112aec` | Special-object pointer, always |
| 3 | `0x112af4` | Special scalar, only with null special object |
| 4 | `0x112afc` / `0x112c70` | Actual ordinary / special branch |
| 5 | `0x112c10` | Request, enabled ordinary branch only |
| 6 | `0x112c78` | Construct, enabled special branch only |
| 7 | `0x112ac4` | Initial forced read, always |
| 8 | `0x112b10` | Forced read on the no-construct path |
| 9 | `0x112c50` | Forced-alpha read when p1 is false |
| 10 | `0x112d14` | Final forced demand read when earlier demand and suppression are both false |
| 11 | `0x112cfc` | Suppression result when earlier demand is false, native low byte |
| 12 | `0x112d2c` | Base alpha when p1, forced, and construct are false |
| 13 | `0x112d90` / `0x112d74` | Gain in s1 for styles 1/2, or s2 for style 3 |

The new style-3 tap is necessary because that path never reaches the existing
s1 gain tap. Its complete manifest guard, displaced FMUL, and continuation
are pinned. The production observer emitter is executed against the original
instruction with callback register clobbers and complete GPR/SIMD/status
comparison.

The sampler clears values at each outer-frame entry. It binds every tap and
feedback to the same native SP, policy output object x19, and policy owner
x20. Repeated taps, multiple invocations, cross-call scopes, inconsistent
forced reads, and missing consumed inputs remain invalid. Skipped input
values cannot leak from a previous frame. Original output reads and exact
construct/demand/style/alpha comparisons remain in the production router.

## CPU verification

Run under `systemd-run --user --scope -p MemoryMax=650M -p MemorySwapMax=0`,
with `FT_HOOK_TEST_ELF` pointing to the pinned ELF and the existing Unicorn
and pyelftools directories in `PYTHONPATH`:

```
python3 -B native-portal/live/policy_sample_test.py
python3 -B native-portal/live/runtime_test.py
python3 -B native-portal/live/runtime_test.py --sanitize
```

The branch sampler test compares 5,120 cold/warm, pointer/scalar, boolean and
style cases, exercises 51 observed branch masks, removes each consumed tap
from representative paths, changes invocation scopes, and tests duplicate
feedback. NaN, infinity, zero gain, special-scalar unordered behavior and
unused scalar branches retain the existing comparison and admission rules.
Native lock/provider/suppression calls use explicit synthetic returns.

The router fixture covers ordinary-disabled readiness without camera demand
and the live `0x3bdf` special refusal. Existing negative predicates, real
private control transport, external-call forwarding, stereo queue/wait,
settlement and Release tests remain in the normal and sanitized runner.
Neither test loads the compositor, opens hardware, or proves visible stereo.

The full loader suite was blocked locally by missing OpenSSL development
headers and stale package URLs. The directly compiled production observer
emitter test above verifies the new site without those headers. This does not
substitute for a complete product loader build.
