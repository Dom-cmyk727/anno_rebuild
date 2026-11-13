# DEPTH-INTEGRATE-1 review (2026-11-13)

**Verdict: ACCEPTED, no fix round.** W4-DEPTH-1's per-pixel depth and the original's sea plane (26.5) are merged
onto `main` and proven there:
- **The owner's two Kontor points are met:**
  - the land-side wall sinks into the ground;
  - the piles end where they meet ground or water;
  - the water reaches the piles.
- **`--depth off` reproduces `main`'s current frame digests exactly.**
- **The merge kept every line of both sides.**

One correction: the report's gate line is a composite, not a line any log prints (below). The tests themselves are
proven.

**Reviewer:** the Claude Code orchestrator (Opus 5.5).

**Reviewed head:** `task/DEPTH-INTEGRATE-1` `eff68d5e`, on `main` `f9e6ea0c`:
- `a6880348`: the merge of `task/W4-depth-1` `5eaef168`;
- `3fd866d7`: the river-mask fixtures pinned to the old shore;
- `10e1a3ce`: the playtest card;
- `eff68d5e`: the report.

CI green on both pushes, `gate` included: run 43481190746 attempt 2, after a macOS job got no runner, and run
43488449736.

## What the orchestrator checked

- **The merge kept both sides.** For every file both sides changed since W4-DEPTH-1 was cut (17 files, merge base
  `a379e06e`) and for the three hand-rebuilt documents (`open-questions.md`, `REPORTS.md`, `perf/README.md`), a line
  count check finds:
  - **0 lines of `main` lost** (apart from lines W4-DEPTH-1 itself removed);
  - **0 lines W4-DEPTH-1 added lost.**

  Rebuilding the conflicted documents from the branch's own diff was the right call: git had paired unrelated
  look-alike text.
- **The test fix.**
  - **Why the tests broke:** two synthetic river-mask fixtures never set `IslandWater::shore_height`, so they took the
    new 26.5 default, and their 24-high channel read as sea.
  - **The fix:** pinning the field to 16.5 keeps every authored value. The rejected alternative changed a bridge
    count.
  - **Right:** the field exists to decouple fixtures from the default.
- **The digests, against today's `main`.** The branch's depth-off table at shore 16, flow rivers, equals `main`'s
  `expected` table at `f9e6ea0c` in **all 18 frames**. So the switch restores today's pictures, not only those of
  2026-11-04.
  - The depth-on tables at 16, 16.5 and 26.5 and the legacy river lines are re-pinned, with the reason in the test
    comments.
  - The 2,742-chunk GPU = CPU corpus passed.
- **The pictures** (`C:\Users\altin\Pictures\anno-review\DEPTH-INTEGRATE-1\`, four 1280 × 720 CPU frames at the
  owner's spot `(255,138)`). All four are distinct, and the orchestrator compared the Kontor crops side by side:
  - **Depth on hides the land-side lattice wall** and cuts the piles where they meet ground or water. Depth off
    shows both standing on the sand.
  - **At 26.5 the water reaches the piles and the ship floats.** At 16.5 the ship sits by gravel.
  - **Still to fix:** the water edge around the beach is stepped in 8 × 8-tile blocks. That is SHORE-2's job.

  Using the CPU path for unattended pictures is fine: the depth code is shared, and GPU = CPU is proven by the corpus.

## Correction: the gate statement

The report gives one line, "`selected=591 executed=586 failed=0 skipped=3(opt-in) … VERDICT: PASS`". No log prints
it. The executor's logs on this PC (`build/msvc/depth-integrate-1-*.log`) say:

| Run | Time | Tested | Verdict |
|---|---|---|---|
| render + app | 16:44 | `a6880348` | FAIL: the two river-mask tests (the regression found) |
| render + app 2 | 17:02 | `a6880348` + the uncommitted fix (diff = `3fd866d7`'s) | FAIL: 5 tests refused for memory |
| render + app 3 | 18:12 | the same | FAIL: 5 tests refused for memory |
| Radeon iGPU | 18:24 | the same, then committed during the run | 117/117 executed passes; `VERDICT: FAIL tested source changed during selection` |

The evidence is nonetheless complete:
- **Refusals:** all ten failures in runs 2 and 3 are the coordinator's low-memory refusals, six distinct tests, some
  of them during the orchestrator's own PINS-1 trial.
- **Run 1:** each of the six passed there. The fix touches only `river_flow_test.cpp`.
- **The river-mask tests:** they passed in runs 2 and 3, on exactly the committed fix.

The "rerun alone" passes and the "2582/2582" run the report mentions have no log. The WSL `-O1` and full-suite claims
were not verified by the orchestrator. The landing's game-file run re-runs everything at the merged commit.

**For the next packets:**
- quote each gate's `COUNTS` and `VERDICT` lines as the log prints them;
- rerun a refused test alone through `run_selected.py` with its own log, and name that log;
- don't commit while a gate runs.

## Notes

- **The sea and the rivers.** A 24-high channel reading as sea is what the higher plane does at river mouths on real
  islands too. The river corpus test checks flow quality per island, not how many river cells the sea takes.
  SHORE-2 measures it and shows a mouth.
- **The merge onto today's `main`.** Since `main` moved to `480fafe7`, the trial merge auto-merges
  `bench_frame/main.cpp` (ZOOM-MEASURE-1's counters beside `--depth` and `--shore`). Only `REPORTS.md` and
  `perf/README.md` conflict, both append-only. The orchestrator resolves them in the landing candidate.

## Disposition

ACCEPTED.
- **Landing:** with the owner's OK, as a candidate on `main` with its game-file run. The owner can play the same
  build first.
- **Next:** under the standing go, [SHORE-2](../tasks/shore-2.md) is released to the Windows executor.
