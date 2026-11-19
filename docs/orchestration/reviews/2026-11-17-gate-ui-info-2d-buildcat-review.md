# GATE-RUN ui-info-2d-buildcat — failure review

**Verdict: CHANGES REQUESTED for candidate acceptance.** The Mac execution result is FAIL,
not a green landing gate. Reviewer: Mac orchestrator (Orchestrator II Mac), read-only inspection.
Candidate `95897a55bc939a185766b7207e69d098997cdb1d`, tree
`d65099256a598f7ce82d37ea303b81ff87088542`; the detached worktree remains clean.
Final execution receipt reviewed: the full-gate failure and completed warning check are retained.
No source repair or test rerun was requested. The executor reports all owned gate jobs ended.

## Verified result

Release configured/built with viewer and GPU enabled. Canonical full game-file run, four test
jobs, admission enabled, required assets present:

| Selection | Selected | Executed passes | Failed | Empty | Skipped | Incomplete | Sanitizer lines |
|---|---:|---:|---:|---:|---:|---:|---:|
| Required picture contract, separate | 1 | 1 | 0 | 0 | 0 | 0 | 0 |
| Full suite | 2861 | 2805 | 2 | 0 | 54 | 0 | 0 |

Skips: 0 required assets, 10 platform, 31 opt-in, 10 GPU, 3 other. The canonical reader
independently reproduced FAIL and retained `GATE_END` status 1 / CTest return 8.
No sanitizer diagnostic is not a sanitizer-campaign pass: this is Release.

Both `viewer_wiring_ui_magnifier_range` and `viewer_wiring_ui_magnifier_range_stepped`
fail with `no frame was drawn within 30000 ms`. Retained stderr shows the scene loaded,
a drawable was created and Apple M5 Max / Metal initialized before the dev-script failure.
Registration requests the GPU path and serializes it with the GPU resource lock. The scripts
require frame progress; this result does not reach a proof of their magnifier behavior.
It does not establish whether the cause is frame scheduling, startup work, visibility or another
path. Do not replace the failure with a GPU-unavailable skip or extend the timeout without diagnosis.

## Skip-policy mismatch

Compared with ui-tabs-b: 42 identical skips, two adapter-message spelling changes, ten new skips.
The ten new entries comprise one Windows-only DXGI probe, one opt-in texture-ceiling probe and
eight additional named-adapter GPU cases. Source inspection confirms the eight require unavailable
RTX/Radeon adapters; they are not evidence that Metal has no functioning device:

- RENDER-DIRECT-1d questions 1 and 4, the Radeon resident-pair/heap probes (two).
- Atlas lan5 byte/sample checks, RTX and Radeon (two).
- Atlas endless02 resident-set checks, RTX and Radeon (two).
- Atlas VRAM/upload-readback timing cases, RTX and Radeon (two).

Their full exact names/reasons are in the retained `skip-comparison.json` and inventory.
This explains their cause but does not meet the packet's two-GPU-skip acceptance condition.
The Windows orchestrator must reconcile that stale expectation with the new registrations and
the target-adapter evidence. No skip policy is silently broadened here.

## Provenance and bounded follow-up

All evidence below is relative to the candidate gate worktree, under
`cache/gate-run-ui-info-2d-buildcat/` (raw logs remain local, not published):

| Artifact | SHA-256 |
|---|---|
| full-01.log | c9c9a11348cea9b24cd4e49259a2545381b122c08843da3b51c2bf2cb8a9f4a4 |
| full-01.log.inventory.json | 885fbc667f2d3bf0148974f625b4d794d327c010b0fcab15b679039d971d7133 |
| outcomes-artifacts/1084/run.stderr.txt | 68498a87d1518f9855931bce29892df518cf5f43218ff05307986d2da31cb7d6 |
| outcomes-artifacts/1085/run.stderr.txt | 6d2356a0f66a7fb263d07213ccce6fa2c65d415129fcc5b2d2fb01cc44a54c8a |
| warnings-01.log | 83c24b4da897e3110ac33012d5a0cc158ba7e3d099aca948d3350d516bd9edf8 |

The public GitHub Actions lookup by exact head returned zero workflow runs when independently
checked. **CI run: none found**, not green; landing validation remains the Windows orchestrator's.
The warning script finished once, exit 0, in 206.936 seconds. Independently reconciled 1,054
project compile invocations and two rejected shadow-warning controls. Both script compiler labels
used Apple Clang 21; Bash 3 mapfile, BSD xargs capacity and CMake's c2y-warning suppression were
documented launch adaptations. Native Windows/GCC 13/Clang 18 remain NOT_RUN. Three duplicate-library
link warnings were retained; this is not an entirely warning-free build claim. The final evidence
manifest's artifact hashes were independently checked without rerunning the suite.

Hand off the two failed window cases to the Windows orchestrator for assignment to the owning
UI/render lane: diagnose the first absent-frame boundary on this source, preserve the initial
failure, then run only the repair's affected tests/lanes. No full repeat of the passing suite on
the unchanged source is requested. No Mac engine/coordinator repair is added to wave 0.

**Next Mac packet:** MAC-HEAVY-1b is RELEASED; the final gate report is received and the executor
reports gate/warning work ended. It starts from main, not this failed candidate, and only repackages earlier
evidence. This keeps TOWN-START-1's gameplay measurement independent of the viewer failure.


## Owner/Windows follow-up, 2026-11-19

The owner forwards Windows' skip-policy reconciliation: the TEN named-adapter
GPU skips above are now expected (two play and eight atlas/RENDER-DIRECT).
DXGI stays platform and the texture-ceiling probe opt-in. The old two-GPU-skip
expectation is superseded; no tests were rerun to reinterpret these receipts.
The two magnifier failures remain FAIL and are not covered by this allowance.

The owner confirms the display was **unlocked and awake** during the original
run. Locked/asleep/SSH is therefore not an established explanation. Window
coverage/focus and surface-acquire state remain to be established.
[MAC-GPU-WINDOW-1](../tasks/mac-gpu-window-1.md) releases four unchanged-candidate
case invocations, two display-context pairs, with retained stderr/traces and
explicit hidden-versus-back-off attribution or an honest unresolved verdict.
No code change or whole-suite repeat. Windows owns any later harness/display
requirement policy. This follow-up does not retroactively accept candidate 10.
