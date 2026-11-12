# GATE-1b: each required test runs once

**Status:** RELEASED 2026-11-12 by the Claude Code orchestrator, with the owner's go, to **the Windows executor**.
Step 2 of the queue on the board ([`TASKS.md`](../TASKS.md), "Current work").

**Base:** `origin/main` at this packet's release commit. Branch `task/GATE-1b`, in a new worktree. About 2–3 hours,
much of it the proof gate.

**Rules:** the owner rule of 2026-11-12 on the board. One concern, no reviewer subagents (the orchestrator reviews
your handover), one REPORTS entry of about 60 lines, READY FOR REVIEW.

## Why

The [project analysis](../reviews/2026-11-12-project-analysis.md) §2.1 and §2.2 steps 2 and 4 (finding D2):
`tools/test/run_selected.py` runs every `--require-test` contract alone (about lines 146–164), then again inside the
selection run (about line 179). On UNIX it also adds `picture_matrix_contract` to every call (about line 143), so the
WSL gate runs that contract in each of its five lanes. The [GATE-1a review](../reviews/2026-11-12-gate-1a-review.md)
has the current lane times.

## Do

1. **A required contract runs once.** In `run_selected.py`, leave the tests that already ran as required contracts out
   of the selection run. Their verdict rows are in the log already, and the README says their outcomes don't count in
   the selection's totals. A selection made only of required contracts must still end with a clear verdict, not
   "selection contains no tests".
2. **The automatic contract can be switched off per call.** Add an option that skips the automatic
   `picture_matrix_contract` (the default stays as it is for every other caller). `tools/test/wsl_gate.sh` runs it in
   the first lane only.
3. **Test the workflow.** Extend `tools/test/workflow_test.py`: a test that is both required and selected runs once,
   and the new option leaves the automatic contract out.
4. **Say which run covers what** in `docs/testing/README.md`: hosted `linux-gcc` runs the GPU parity tests on
   lavapipe without game files; the Windows game-file run covers the real adapters; `wsl_gate.sh --gpu-lane` is for
   debugging, not a package gate.

## Proof

- **One full WSL gate after the change,** at your pushed head, in a separate clean checkout, default lanes, no
  `--gpu-lane`.
- **Compare it with the GATE-1a gate** (`~/anno-gate-logs/anno1503-GATE-1a-gatecheck/` in Ubuntu, 4,958 s of lanes):
  every lane's seconds side by side; the same tests by name with the same verdicts; each required contract executed
  once per gate; `picture_matrix_contract` once; `sanitizer_lines` 0.
- `python -I tools/test/workflow_test.py` passes, with classified counts in the report.
- **Hosted CI green** on the head, `gate` included. Read CI after every push. A known coordinator flake (`F3`, `C3`):
  confirm it from the job log, rerun that job only, and say so.

**Not yours:** the flow-build test and the GPU platform skips (GATE-1c), the coordinator flakes (COORD-FLAKE-1), the
MSVC game-file run (the orchestrator).

## Report

READY FOR REVIEW, about 60 lines: what changed, the lane table against GATE-1a, the run-once evidence, the workflow
test counts, CI, NOT_RUN, Decisions and reasoning, QUESTIONs.
