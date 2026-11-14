# ZOOM-MEASURE-1 review (2026-11-13)

**Verdict: ACCEPTED, no fix round.** The failure the owner saw is now a number:
- **The sweep:** `bench_frame --zoom-sweep` counts, per frame, the readiness state the window itself reports. The
  sweep profiles now match how the owner plays.
- **The contract** (`tools/test/zoom_sweep_contract.py`) fails today on every real profile, as it should.
- **The "everything resident" control** reaches 0 on every readiness counter, so the criterion is reachable.
- **The 0.7 s hitch** has a confirmed mechanism.

**Reviewer:** the Claude Code orchestrator (Opus 5.5).

**Reviewed head:** `task/ZOOM-MEASURE-1` `a5e180e0`:
- `f7e23dca`: the counters, modes and contract;
- `738b0f71`: a Clang `-Werror` fix;
- the REPORTS entry.

Changed: `src/tools/bench_frame/main.cpp`, `tools/test/zoom_sweep_contract.py` (new), `docs/perf/README.md`. No
renderer behaviour change. CI green, `gate` included (run 43436856818 and later).

## What the orchestrator checked

- **The counters come from `GpuStats`,** the source of the window's own per-second line: `chunks_missing`,
  `stand_in_parts`, `chunks_fading`, `open_sea_chunks`, `fluid_fade` and `fluid_windows`. No readiness logic is
  duplicated.
- **The new files go through the real window's loaders.** `.txt` fixed maps and `.json` ship-start manifests are read
  with `app::load_play` and `app::load_opening`.
- **The contract** parses one summary line. It fails on any non-ready frame, stand-in, dissolve, open-sea draw, or
  busy frame above 2 ms. It excludes a stated four warm-up frames through `--zoom-sweep-skip`, not through a looser
  threshold. It exits 77, a skip, without the game files.
- **The baseline,** on the RTX 4090 at 2560 × 1369 with enhanced water:

  | Run | Non-ready frames | Stand-in draws | Dissolves | Frames > 2 ms |
  |---|---|---|---|---|
  | `lan5` legs | 431 of 12,000 | 26,616 | 13,304 | 329 |
  | wheel and zoompan | 593–881 | up to 45,485 | | |
  | pan | 57 | | | |
  | original water | 537 | | | |
  | 1280 × 800 | 84 | | | |
  | iGPU | 10 | | | |

  - Wheel and zoompan are worse than legs, and pan is far better: misses come from zoom changes.
  - Original water still misses, so RC1, chunk readiness, is the main cause, not the water.
  - The iGPU misses little, but its worst frame is 37.1 ms, an offscreen target resize.

  This matches `zoom-smoothness.md` §3.
- **The hitch, measured on a real window:** 106 ms, of which 96.7 ms is inside the `SDL_PollEvent` loop during a
  window drag. That is the modal-loop mechanism the research named. A restore from maximised spent 16 ms in `surface
  configure`.
- **The subset of the matrix:** 8 primary runs plus 3 spot checks, not all 32 combinations. That is reasoned in the
  report, and every factor the packet named is shown to matter. Accepted.

## Notes for ZOOM-SMOOTH-1 (no fix round)

1. **Register the contract in CTest as an opt-in test,** skipped with exit 77 unless an environment variable asks for
   it. Then it doesn't turn `gate` red. ZOOM-SMOOTH-1 reports its numbers against this baseline, and the RENDER-DIRECT
   packets make it pass.
2. **The control's busy frames:** 2 of 1,980 frames are above 2 ms with nothing loading, which is driver submission
   jitter (RC4).
   - **Readiness stays a hard 0.**
   - **The busy-time criterion** should state how it treats a frame the application did not cause, for example the
     busy time measured before the submit as against after it. Decide it on evidence, with a measurement. Don't
     simply raise the threshold.

## Disposition

ACCEPTED. It lands with the next landing, on the owner's OK. Under the standing go, and in the owner's order ("Yes,
depth first"), [DEPTH-INTEGRATE-1](../tasks/depth-integrate-1.md) is released to the Windows executor.

## Game-file run of the landing candidate

**The candidate:** `orch/land-zoom-measure-1` at `480fafe7`. It contains ZOOM-MEASURE-1, FISHER-RE-1c and the
board up to `ea680b6f`, merged on `main` `f9e6ea0c` with the owner's OK (2026-11-13).
- The merge conflicts were only appends in `REPORTS.md`.
- FISHER-RE-1c's merge base was a virtual one (its branch had merged FISHER-RE-1b), so its `REPORTS.md` was built as
  `main`'s file plus FISHER-RE-1c's one section. Every earlier section on its side was checked to be on `main`
  already.

**The run:** in the run worktree on this PC, the canonical call with the game files, `ANNO_GPU_ADAPTER=4090` and 16
jobs, 18:45–19:00.
- **2,574 tests selected, 2,543 executed, 4 refused, 0 other failures.**
- **27 skipped:** 21 opt-in, 3 platform, 3 other, the same as `f9e6ea0c`'s run.
- **The four refused,** all at 18:45, for low memory: about 2 GB free, below the coordinator's stop line, while the
  Windows executor's lanes were running too.
  - chunk draw lists;
  - `render_chunk`;
  - the river flow corpus;
  - the water field.

  Under the owner's test rule they did not run. Rerun alone (`--jobs 1`) at 19:09 with about 15 GB free: **4
  selected, 4 executed, 0 failed,** `VERDICT: PASS`.
- **Logs** (`cache/orch-land-zoom-measure-1/`):
  - `canonical-gate.log`, sha256 `e64aec0b…`;
  - `rerun-refused.log`, sha256 `953a6711…`.

So every test of the candidate passed.
