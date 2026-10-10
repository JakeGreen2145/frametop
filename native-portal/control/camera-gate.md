# Camera access gate contract

`CameraAccessGate` lives in `native_portal`, in `control/camera_gate.hpp/.cpp`.
It owns one camera generation and 64 reusable ticket slots. Epoch and ticket
issuers are process-wide monotonic saturating counters. No ticket allocation.

Coordinator ABI:

```cpp
bool retain(uint64_t epoch, uint64_t& ticket_out);
bool release(uint64_t ticket);
bool begin_close(uint64_t epoch);
bool quiescent(uint64_t epoch) const;
```

Failed retain writes zero. Unknown, stale and duplicate releases fail. Closing
rejects new tickets; existing tickets still resolve the original `CameraBinding`
through `lookup(ticket, out)`. Retain must precede every native pointer lookup.
The copied binding does not extend its ticket and cannot survive its release.
`quiescent` means no retained tickets, not proof of GPU or source settlement.
A transaction must hold its ticket through all CPU, GPU and source obligations.

`publish(binding, owner_thread, epoch_out)` requires all identities and their
independent generations. It performs no native reads. Call only after completed
publication and authenticated backend/association ownership. Closing or empty
live generations cannot be replaced. Successful synchronous Destroy callback
return is the only retirement path.

`close_and_cleanup(epoch, kind, callback, context, timeout)` immediately closes
admission. It withholds the callback until no ticket or other cleanup remains.
The gate mutex never spans the callback or its waits. Resources leaves the
original generation closed. Destroy retires it only after callback returns.
Timeout, invalid input, stale epoch and self-wait return without invoking it.
The integration must fail-stop or safely defer the destructive continuation on
such refusal, never fall through to the original free or forge a success.
Render owner, current cleanup thread and any ticket issuer cannot wait on their
own barrier. A ticket may be released by its coordinator on another thread.
The gate must outlive all tickets and cleanup callbacks; destroying it with debt
terminates. Forked copies refuse calls without touching inherited mutexes.

This is a real callback interlock, not a process-wide camera registry or stock
refcount. It is safe only when every selected native entry and destructive
boundary participates. Missing entry/cleanup coverage must keep live admission
disabled. It does not serialize native mutations, replace the existing exact join
guards, establish counted demand, or prove source reuse. Parent composition owns
those obligations with the owner coordinator and Adapter settlement callbacks.

## Verification

CPU tests use real threads and withheld cleanup callbacks, including two-ticket
CPU/transaction drain, foreign native-result forwarding, owner refusal, timeout,
pre-execution cancellation, cleanup exclusion, bounded slot reuse, and same-address
republication. No native runtime is loaded.

```sh
systemd-run --user --scope -p MemoryMax=256M -p MemorySwapMax=0 -- \
  python3 native-portal/control/camera_gate_test.py
systemd-run --user --scope -p MemoryMax=256M -p MemorySwapMax=0 -- \
  python3 native-portal/control/camera_gate_test.py --asan
```

ASan/UBSan compilation uses O0 without debug info to stay inside the 256 MiB
scope. O1 plus full debug info exceeded the cap; no memory limit was raised.
