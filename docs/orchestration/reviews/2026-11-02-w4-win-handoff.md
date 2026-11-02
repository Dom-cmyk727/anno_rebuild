# Windows rendering handoff assessment, 2026-11-02

**Review pending; bounded closeout required. This is an assessment of the
submitted handoff against the assignment, not a source-code acceptance review.**

The Windows executor reports local head `9ba01f1`, with `src/` last changed at
`4ecea07`. Neither object is available in the orchestrator's Mac repository.
A fresh GitHub ref check found `task/W4-3-live-buildings` still at
`a29b07949201e3ea5bf30c800e392f337c1ec598` and main at `3352ce5`.
The Windows checkout, raw logs and new implementation were not accessible here.
No new source defect or independently verified Windows pass is asserted below.

## Immediate disposition

Publish the existing committed Windows candidate to its GitHub task branch for
review, using an ordinary non-force push after checking remote ancestry.
This is explicitly authorized by the W4-WIN-CLOSE packet; it is not a main
merge, release or acceptance of R1–R4. Keeping the only copy on Windows prevents
the orchestrator from reviewing it. Preserve the candidate and its evidence
before further corrections.

## Acceptance gaps disclosed by the executor

1. **R1/R4 do not yet have a discriminating consumer test.** The handoff says
   reverting their viewer call sites causes no test failure. The prior packet
   expressly required production-consumer coverage. A helper test with correctly
   arranged inputs cannot show that the window actually uses the helper, passes
   the publication watermark or honors the disabled-tool path. Close these at
   the actual viewer boundary with valid controls and verified mutant binaries.
2. **R2's important delay boundaries remain partly untested in the real window.**
   Removing `frame_done` or `begin_frame` and observing a timeout establishes
   that the feed matters. It does not by itself prove that `Visible` stays false
   while chunk replacement, upload or dissolve is incomplete, or after a failed
   present. Keep the positive presented-frame test and add focused adversarial
   consumer coverage; do not repeat a full matrix just for this gap.
3. **Two failures need disposition.** The whole-app WSL sanitizer gate had 175
   passes and one timeout at 3,400 seconds. An additional stalled-reclaimer
   assertion failed once. Eighty focused passes do not supersede that timeout,
   and simultaneous EA FC27 load is a hypothesis, not a diagnosis. Investigate
   the named cases with observable progress/wait state and a bounded comparison.
   Do not close them by excluding the case, increasing every timeout or retrying
   until green. A demonstrably unrelated failure may remain a separately owned
   follow-up; an unexplained lifecycle failure remains open.
4. **The final evidence tools and assertions need their own closure.** The report
   says its audit stopped at `f8bc0a6`; the later report correction and last tool
   fix were not rechecked. Watchdog status, unreadable peak-memory handling and
   capture-guard refusal were never exercised. Small public-command contract
   cases can test these without another long game run. Re-aggregate retained
   raw data with the corrected final tool; rerun a measurement only if its inputs
   are invalid or its measured behavior changed.
5. **Some provenance is irrecoverable or not yet portable.** Two negative-control
   runs used an unmutated binary and are correctly marked invalid. The 24-process
   reproduction was only in terminal output; temporary baseline build logs were
   deleted. Those claims cannot be independently reproduced from their original
   logs. Record the loss, preserve the remaining evidence and create a new narrow
   experiment only where a decision still depends on that result. Never recreate
   a historical log or reclassify an invalid control as successful evidence.

These are reported acceptance gaps, not five newly discovered implementation bugs.
The next source review may confirm fixes or identify additional concrete defects.

## Reported results retained at their actual scope

| Reported result | Scope and limit |
|---|---|
| MSVC CPU: 463 selected, 461 passed, two opt-in skips at `381b3e3` | A reported gate on that revision, not 463 executed passes or automatically a final-head gate. |
| RTX Vulkan: 106/106 at `4ecea07`; Radeon Vulkan: 106/106 at `70817ed` | Useful adapter-specific coverage. Reuse at a later head requires a dependency explanation and binary/source identities. |
| WSL GCC 13.3 / Clang 18.1.3 warnings clean | Reported portability evidence; inspect the actual command/file coverage after publication. |
| WSL ASan/UBSan affected subset: 80 passes plus shell contracts | Keep separate from the failed whole-app run. Native Windows shell-contract absence was permitted by the assignment and is not itself a defect. |
| Live command session and replay, changed command diverges at turn 60 | Promising integration evidence; inspect the actual traces/commands and their identities. It does not replace the missing R1/R4 wiring controls. |
| Continuous-zoom preview: maximum difference one on both adapters; wrong overlay rejected | Preserve the existing tolerance and relevant non-empty comparison. This is CPU/GPU agreement, not original-game visual parity. |
| Thirteen window cases, reported 500 fps and zero dropped frames | Reconcile raw rows and field meanings. Present-return cadence, trace-record loss, present failures and physical display drops are different quantities. Do not infer observed scanout from refresh metadata or API returns. |
| A real 3840×2160 drawable on a 2560×1440 monitor | Valid evidence of rendering that pixel workload if measured correctly; not evidence of native 4K display/scanout behavior. |
| OQ-525 reproduced within existing tolerance on both Vulkan adapters | The discrepancy remains named; this does not close Metal failures or establish original fidelity. |

## Scope of closure and follow-ups

The next packet closes the known repair/evidence gaps and produces a reviewable
Windows candidate. It does not restart R3's already reported allocation work,
the complete adapter matrices or the historical renderer campaign without a
dependency reason. A final code change gets affected tests, not automatic reuse
of all older passes and not an automatic full rerun.

Physical display latency, native 4K monitor testing, the full Radeon window
matrix, large-city/complex-economy/multiplayer scaling, Metal timing, OQ-525,
terrain (OQ-052), height (OQ-550), road implementation and final UI remain explicit
follow-ups. None is silently waived or labeled passed. A small obscured/failed-
present check is relevant to R2; an exhaustive obscured-window performance matrix
is not required for this closeout. Separately reviewed gameplay on the Mac
continues under MAP-NAV-2 without waiting for these unrelated rendering goals.

The independent final review must inspect the actual viewer calls, the negative
controls and the final tools, not only this report. Completion means the scoped
criteria are met and source/evidence are available for orchestrator review;
publication alone is not completion.

Independent dispatch review: `/root/review_windows_dispatch` approved this
assessment and W4-WIN-CLOSE with no findings. It checked the report/acceptance
distinction, limited publication authority, production-consumer controls,
finite failure investigation and evidence reuse. MAP-NAV-2 and the historical
suffix were confirmed byte-identical. This approval covers documentation only;
the unavailable Windows source and raw runs were not independently reviewed.
