# Native passthrough work-in-progress checkpoint

Development stopped at the owner's request. This branch preserves the latest combined source based on `ef750c6df41c9a8e364e439890578f3542877d9b`; it is not a release or an activation recommendation.

## Goal and implemented work

A finite, binocular, world-space desk passthrough portal that coexists with the current scene and reuses the native acquisition, calibration, color conversion and reprojection path.

The checkpoint combines desktop controls/placement, shader masking, native hook and pipeline integration, camera/source admission, command and wrapper lifetime tracking, committed stereo and pending-Off settlement, and the dispatch-wide graph adapter. Wrapper retirement and admission changes have been integrated; the graph adapter is implemented but has an unresolved review blocker.

## Known blocking issues

1. **G1: optional graph rejection can abort ordinary native work.** If initial graph capture rejects an over-capacity pass vector, preparation exclusion remains armed. A subsequent ordinary nonzero optimizer call then aborts before forwarding. Independent comparison reproduced baseline forwarding versus the new abort. The fix has NOT been applied. Revoke optional preparation/read/use rights when initial capture fails before any added lookup/backend admission, while preserving root lifetime retention and all actual command/source debt. Do not bypass mutation guards after preparation was admitted.
2. **Present is still deliberately disabled.** Its first-consumer functional regression remains failing. Additional authenticated outer-invocation/selector/return provenance is needed; a return-address allowlist alone is insufficient.
3. **Present can add another semaphore signal.** The existing one-signal submission subset needs a separately verified extension that identifies the unique owned first native timeline signal and preserves all foreign slots unchanged.
4. **Cold resource lookup remains unsupported.** The implementation accepts the qualified already-cached provider subset. Conditional analysis of the cold getter does not itself provide the required live pre-call identity/registration/lifetime receipts.

Further observer additions must also reconcile the current 253-site inventory with the Runtime's 256-entry buffer. Do not silently omit required observers to fit.

## Verification at the checkpoint

Recorded combined source-model results include:

- 236 current Runtime cases and 7 dormant cases, normal and ASan/UBSan.
- 42 graph-scope cases and 4 first-consumer diagnostic cases in both modes.
- Complete modeled wrapper cleanup, sized delete, fresh same-address allocation epoch, replacement publication and settlement crossing cases.
- Separate repaired collector/Evidence consumption of an instruction-derived turnover stream: 34 normal and 34 sanitized cases, including every omitted event and stale-retirement negatives.
- Earlier isolated physical-GPU masking tests passed on the headset, but used synthetic shader inputs rather than native camera frames.

These results do **not** supersede G1 or establish live binocular passthrough. Expected-refusal diagnostic tests are not functional acceptance. No successful on-headset binocular portal has been confirmed, and this combined checkpoint has not been activated.

## Private inputs and handoff

Raw device captures, runtime binaries, proprietary shader inputs, generated native evidence packages, build outputs and private operational notes are not included in this publication. Some native tests require separately held, hash-pinned runtime fixtures; existing machine-specific fixture paths need configuration on another machine.

Component handoffs and verification metadata remain under `native-portal/live/`. Start with `combined-graph-handoff.md`, `graph-scope-handoff.md`, and `combined-retirement-handoff.md`. Read their scope and historical-status qualifications rather than treating every recorded test as hardware evidence.

No implementation repair, additional device trial, upstream merge, or release was performed for this checkpoint publication.
