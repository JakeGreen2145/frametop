# One-shot attachment startup prerequisite

The trial-14 pre-pool-failure samples show native eligibility with `adapter=0`
and `policy=0`. This is sufficient to reproduce a one-shot attachment trap in
the production router. It does not establish that every live blocker is repaired.

## Cause and repair

At the seal, `status.capable` describes graph/pipeline/origin/demand eligibility.
It does not include `external_clear`. The router previously constructed an
Adapter whenever that partial capability was true. `Adapter::initialize()` sets
its `attempted` latch before testing its stricter backend prerequisite, which
requires `capable && serialized && external_clear && !fault`. An unsupported
policy sample therefore returned before either native constructor. The retained
optional Adapter then prevented another attachment when policy became supported.

The runtime now checks the effective Backend status before constructing the
Adapter. This only defers an attempt before any private resource or Adapter
exists. It does not reset an Adapter, relax policy, or retry partial native work.
Independent revocation/fault overlays remain part of the effective status check.
Bootstrap graph ownership, one-shot initialization and partial-failure retention
are unchanged. The separate policy seen-bit and command-pool caller repairs are
outside this change.

## Verification

`live/attachment_test.py` builds the production router, Adapter, Evidence and
private control server, sharing the existing chain fixture. Its recovery case
starts with unsupported policy, verifies zero constructor calls, restores valid
policy, verifies exactly one call to each constructor, then runs the real
private-lease/stereo/queue/wait/disposal/reset/release fixture. Native GPU/API
boundaries remain explicitly modeled. Its partial case fails the blend callback
after wrapper construction, removes that fault, and proves repeated eligible
frames do not retry either constructor.

Baseline recovery failed with `capable=0 constructors=0,0`. The repair passes
both cases and ASan/UBSan. The existing `live/runtime_test.py` suite also passed,
including its production DSO build, diagnostic collector, journal sockets and
saturated sink checks.

With `--machine`, the constructor callback executes the pinned `0x296c00` and
`0x294358` instructions through `live/runtime-call.S`, assembled by Keystone.
The witness checks arguments, BLR return PC, SP/callee-saved restoration,
592-byte wrapper metadata, sentinel offset, and native source-over conversion.
Native allocator outputs are synthetic. Constructor bytes are copied into
separate host fixture objects with explicit pointer relocation, then authenticated
by production Evidence call-return and Adapter observation. No host ASLR address
is mapped into the guest. No device, Vulkan execution, or live visibility is
claimed.

Run from a capped scope, with the pinned private ELF and existing Python deps:

```sh
systemd-run --user --wait --pipe -p MemoryMax=650M -p MemorySwapMax=0 \
  env FT_HOOK_TEST_ELF=/path/to/pinned/vrcompositor \
  PYTHONPATH=/path/to/product/pylib:/path/to/camera-analysis/pylib \
  python3 -B /absolute/checkout/native-portal/live/attachment_test.py
```

Use `--sanitize` separately. For `--machine`, provide `keystone-engine==0.9.2`
on PYTHONPATH or in ignored `native-portal/live/build/attachment/pylib`.
Like the existing instruction fixtures, the machine witness imports the private
hash-checking analysis definitions from the agent work directory. Reports and
binaries stay in `native-portal/live/build/attachment/`.
