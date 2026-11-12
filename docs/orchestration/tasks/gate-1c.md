# GATE-1c: a gate can end green for the right reasons

**Status:** RELEASED 2026-11-12 by the Claude Code orchestrator, under the owner's standing go for the Windows lane, to
**the Windows executor**. Step 3 of the queue on the board ([`TASKS.md`](../TASKS.md), "Current work").

**Base:** `origin/orch/land-gate-1b` at this packet's release commit, branch `task/GATE-1c`, in a new worktree. About
2–3 hours. That branch is `main` plus the GATE-1b merge and a one-line Rust fix, and lands on `main` with the owner's OK.
The fix matters: since Rust 1.100.0 reached the hosted runners on 2026-11-12, `lint` fails on every branch without it.
If `main` has moved past this base when you push, merge `origin/main` in; never rebase or force-push.

**Rules:** the owner rule of 2026-11-12 on the board. One concern, no reviewer subagents (the orchestrator reviews
your handover), one REPORTS entry of about 60 lines, READY FOR REVIEW. A flake is closed by its mechanism, never by
reruns.

## Why

The [project analysis](../reviews/2026-11-12-project-analysis.md) §2.1 and §2.2 step 3 (findings D3, D4): two kinds
of test kept every recorded gate red. The [GATE-1a review](../reviews/2026-11-12-gate-1a-review.md) saw the first one
pass at `-O1`, and the GATE-1b gate saw it pass again. That points to timing, not a fix. The
[GATE-1b review](../reviews/2026-11-12-gate-1b-review.md) found a third way to end green for a wrong reason (item 3).

## Do

1. **The flow-build test** (`src/render/tests/river_flow_corpus_test.cpp`, the test case at about line 1322, `a flow
   build that fails leaves the legacy look, final, built once (game files)`).
   - **Cause first.** Show the mechanism with evidence: the analysis and the review of 2026-11-07 suspect that its
     "settled" check looks at the visible chunks only, while `update_chunks` also counts margin and neighbour-group
     chunks, so one of those can still arrive inside the 60-frame loop. Reproduce it: it failed in every `-O0` gate, so
     an `-O0` build, or load beside an `-O1` one, should show it. Log which chunk arrives and when.
   - **Then fix the test, not the timing:** wait for every chunk `update_chunks` counts, or count only what the test's
     claim is about. Do not raise a sleep, a frame count or a retry count. If the evidence shows a product bug instead,
     stop and report it with the evidence.
   - **Proof:** the fixed test passes repeatedly under the condition that made it fail (state the condition and the
     count), and a deliberately broken variant of its claim still fails.
2. **The named-adapter GPU tests** (`src/render/tests/play_gpu_test.cpp`, `play: GPU matches the CPU path on one scene`
   and `..., on the Radeon iGPU`). With `ANNO_REQUIRE_GPU` set and the named adapter absent (the lavapipe lane), they
   `FAIL` today. Make a missing **named** adapter a classified skip even then, and keep the failure when
   `ANNO_REQUIRE_GPU` is set and there is no GPU at all. Check that `tools/test/ctest_verdict.py` classifies the new
   skip (`gpu` or `platform`) and say which.
3. **A `--select` that matches nothing fails again** (`tools/test/run_selected.py`). Since GATE-1b, a call with a
   `--require-test` and a `--select` that matches no test ends `VERDICT: PASS` with `selected=0`; before, it failed
   with "selection contains no tests". The orchestrator reproduced it with the workflow fixture
   (`--select '^no_such_test$' --require-test pass`: rc 0). Fail when the selection matched nothing *before* the
   required contracts were taken out; keep the PASS when it matched only required contracts. Add that case to
   `tools/test/workflow_test.py`, next to GATE-1b's two.

## Proof

- The flow-build evidence and repeat counts above.
- The GPU tests: their outcome with the adapter present (on this PC: the RTX 4090 and the Radeon iGPU, MSVC Release),
  absent under `ANNO_REQUIRE_GPU=1` (WSL lavapipe) and absent without it.
- `python -I tools/test/workflow_test.py` passes, with the new case and its counts.
- **One full WSL gate** at your pushed head, in a separate clean checkout, **with `--gpu-lane`**: it must end green, with
  the two GPU cases classified as skips and `sanitizer_lines` 0.
- **Hosted CI green** on the head, `gate` included. Read CI after every push; a known coordinator flake (`F3`, `C3`) is
  confirmed from the job log and rerun alone.

**Not yours:** the coordinator flakes (COORD-FLAKE-1), the MSVC game-file run (the orchestrator).

## Report

READY FOR REVIEW, about 60 lines: the flow-build mechanism with its evidence, the fix, the repeat counts, the GPU skip
behaviour on the three setups, the empty-selection fix, the gate and CI, NOT_RUN, Decisions and reasoning, QUESTIONs.
