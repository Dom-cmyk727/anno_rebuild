# Executor task board (read this first, every time)

**Who writes this file:** the orchestrator session (Claude Fable, the owner's "main" session). **Who reads it:** the executor session (Claude Sonnet with subagents). The executor never edits this file; it writes to [`REPORTS.md`](REPORTS.md). The orchestrator updates this file after every review, so re-read it at the start of every task and whenever a task ends.

Last update: 2026-10-20 (board created; status derived from `docs/PLAN.md` §7 and the local git state).

## 1. Protocol for the executor

1. **Read first, every session:** `CLAUDE.md`, this file, `docs/PLAN.md` §0.1–0.3 and §7, and the spec named by the task. The rules in `CLAUDE.md` apply unchanged (evidence only, determinism, no hardcoded limits, tests before refactors, `game/` read-only, nothing from `game/` in git).
2. **Pick the first task whose status is `READY`** and whose dependencies are `DONE`; tasks marked `PARALLEL-OK` may run at the same time as another task in a separate git worktree (`git worktree add ../anno1503-<id> -b task/<id>-<slug> main`). Never run two tasks that touch the same files in parallel.
3. **One branch per task:** `task/<id>-<slug>` from `main`. Small conventional commits (`feat(render): …`, `fix(sim): …`, `docs: …`, `test(...)`), one concern each, each ending with the attribution line the harness gives you. **Do not merge into `main` and do not push** unless the task says so. The orchestrator merges after review.
4. **Gates before you report a task as finished** (all of them; the owner's PC has 16 threads, use them):
   - `cmake --build --preset msvc` warning-free (warnings are errors).
   - `$env:ANNO1503_GAME_DIR="$PWD\game\app"; $env:ANNO_TEST_THREADS=16; ctest --preset msvc -j 16` green **and** the same without `ANNO1503_GAME_DIR` (skips must be clean skips).
   - clang-format 23.1.3 clean: `python -m pip install --target <scratch>/cfpkg clang-format==23.1.3` then `find src -name '*.cpp' -o -name '*.hpp' | xargs <scratch>/cfpkg/bin/clang-format.exe --dry-run --Werror` (a venv fails with Python 3.14 in Git Bash).
   - For `sim`/`formats`/`test_support` changes: the GCC 13.3 `-O3 -Werror` check in WSL (`Ubuntu`) with the CI's exact flags (`-O3 -DNDEBUG -std=c++20 -Wall -Wextra -Wpedantic -Wconversion -Wsign-conversion -Wshadow -Wnon-virtual-dtor -Wold-style-cast -Wcast-align -Wnull-dereference -ffp-contract=off -Werror`, doctest from `build/msvc/_deps/doctest-src`, run with and without `ANNO1503_GAME_DIR`). For `render`/`app` changes it is enough that the MSVC build is clean and you avoided the known GCC traps (`std::swap`/copy of structs holding `std::optional<std::vector<…>>` at -O3; sign conversions).
   - `tools/agree.py` where an `anno-dump` command changed.
   - A performance task also needs its `bench_frame` / `bench_sim` numbers in `docs/perf/README.md` (p50/p95/p99/max, hardware, commit).
5. **Subagents:** use them for (a) independent `PARALLEL-OK` tasks in worktrees, (b) two independent reviewers per finished task who read the diff against the spec's acceptance list, look at produced PNGs where there are any, and mutation-check the new tests (flip a constant or a branch, the tests must fail). Give reviewers the code, not your conclusion. Fix what they find before reporting. Do not spawn more than the task needs.
6. **Report** by appending a section to `REPORTS.md` in the format given there: what was built, evidence (test names, numbers, PNG paths under `C:\Users\altin\Pictures\anno-review\<id>\`), what the reviewers found and what you changed, what is left, open questions. Then stop that task; **do not start the next step of a task marked `OWNER-CHECK` until this file says the owner has looked**.
7. **Decisions you must not make:** anything that changes the game's look or feel (defaults of render styles, river tuning, rules options, limits) is the owner's (`docs/open-questions.md`, "Decisions for the owner"). Implement the faithful or stated default behind an option and list the question in your report. Obvious bugs you just fix.
8. **If blocked** (missing information, a gate that cannot pass for a reason outside the task, a conflict with `main`): write a `BLOCKED` entry in `REPORTS.md` with what you tried and move to the next `READY` task.

## 2. Current status (2026-10-20, verified by the orchestrator)

- `main` is at `fdf11c3`, **three commits ahead of `origin/main`, not pushed** (`ad06bd4` viewer river defaults, `2f7b40d` river gap patch, `fdf11c3` docs). The hand-over says the full ctest and the GCC check have not been re-run after the last refactor.
- Branch `wip/rivers-s7` holds one unfinished WIP commit (hydraulic extras, rivers.md stage S7; 33 files, +3,144/−356), never green, never pushed.
- M1 (formats, assets, render CPU + GPU, viewer) is done in C++20; the Rust crates remain as reference. M2 static RE done. **M3 sim: stages S0–S7 done** (`docs/sim/state.md` §15), S8–S16 open. M4 (net) and M5 (UI, audio, AI) not started.
- Render: enhanced water, river gap fixes, river flow stages S4–S6 done; S7/S8 open; zoom is three discrete groups switching instantly (`render::kGroups`, `Viewer::zoom` in `src/app/src/viewer.cpp:596`), which the owner finds clunky and artifact-prone, and the owner wants a larger zoom-out range.
- Working mode the owner asked for: **small steps, the owner checks each visible step in the viewer before the next.** Viewer command for the owner's checks: `build\msvc\src\app\Release\anno1503.exe --view game\app\Scenes\Islands\Dschungel\D_256x256_0045_5B.scp --game-dir game\app --water enhanced`.

## 3. Task queue

Status values: `READY`, `IN PROGRESS` (set by the orchestrator when a report says so), `REVIEW` (reported, orchestrator reviewing), `OWNER-CHECK` (waits for the owner's look), `DONE`, `BLOCKED`, `LATER`.

### T0 — Re-run the gates on `main` and push · `READY` · first
- **Goal:** make `main` safe to push and push it, so CI runs on the three local commits.
- **Do:** on `main` itself (no branch): full MSVC build, ctest with and without the game dir (16 threads), clang-format, the GCC 13 check in WSL for `src/sim`, `src/formats`, `src/test_support`. If everything is green: `git push origin main`, then watch the run with `gh run list --limit 1` / `gh run view <id>` until all jobs finish and record the result. If a gate fails: fix it in a small commit on `main` only when the fix is obvious and local (formatting, a warning); otherwise report `BLOCKED` with the output.
- **Acceptance:** all CI jobs green (`linux-gcc`, `linux-gcc-release`, `linux-clang-asan`, `windows-msvc`, `macos-clang`, `cpp-format`, Rust jobs). Report the run id.
- **Scope guard:** no functional changes.

### T1 — Smooth zoom, step 1: no popping after a zoom step · `READY` · after T0 · `OWNER-CHECK` after the step
- **Owner's complaint:** zooming in and out fast shows strange artifacts and feels clunky, like a performance gap; it must be smooth whatever the number of players or the progress of the game; and a larger zoom-out range is wanted.
- **What was measured (2026-10-20, `ANNO_VIEW_ZOOM_SCRIPT=4,12`, enhanced water):** the window thread is fine (worst busy frame 2.8 ms, 250–330 fps) but every zoom step triggers new chunk builds ("zoom group N: 1–3 chunk(s) received"), and the step is an instant 2× jump of the tile size (`kTile` = 84×42, 42×21, 20×10). So the artifacts are most likely content popping in after the jump plus the hard step itself. Not confirmed against what the owner sees.
- **Step 1 (this task):** after a zoom step, chunks of the new group that are not built yet must not show as empty/sea: draw the previous group's chunk content for that screen area, scaled by the tile ratio (2× or 0.5×), until the new chunk arrives, then swap. GPU path first (it is the viewer's default); the CPU path may keep the current behaviour if the cost is out of proportion, say so. Keep the scaling in the compositor (`src/render/src/gpu.cpp` / `gpu.wgsl`), not in the viewer. The swap must not produce a visible seam between a stand-in chunk and a built neighbour (same world alignment).
- **Before coding:** read `docs/render/iso.md` ("Scene composition", "Fidelity"), `docs/perf/README.md` ("2026-10-19 — zoom stutter"), `src/render/include/anno/render/frame.hpp`, `gpu.hpp`, `Viewer::zoom`. Also decide OQ-077 ("missing chunks: open sea or a stand-in from the nearest built zoom group") in the faithful-neutral way: stand-in from the nearest built group is this step; record the decision note in `docs/open-questions.md` OQ-077 as "implemented as stand-in, owner to confirm".
- **Scalability requirements:** no per-frame CPU work proportional to the number of chunks on screen beyond what exists; the stand-in lookup is O(visible chunks); no new fixed limits (group count stays `kGroups`-driven, nothing assumes exactly three groups; a comment where the 2× ratio is derived from `kTile`, not written as a literal).
- **Evidence to produce:** `bench_frame --zoom 6 --renderer gpu --chunks async --limits interactive` before/after (same p50/p95/worst, no new hitch lines); a PNG pair of the first frame after a zoom step before/after (`--png` with a scripted zoom, or a screenshot path) under `C:\Users\altin\Pictures\anno-review\T1\`; a doctest that composes a frame with one missing chunk of group 1 and a built group 2 and checks the stand-in pixels equal the 2×-scaled source within 1 per channel (CPU reference) and GPU = CPU within the existing tolerance.
- **Then stop** and report; the owner looks at the viewer. Steps 2–4 are T2–T4 and will be unlocked here one at a time.

### T2 — Smooth zoom, step 2: animated zoom step · `LATER` (unlock after T1 owner check)
- Animate the group switch over about 120 ms (ease-out), anchored at the cursor, by rendering the current group's content with a continuous scale factor in the compositor during the animation, then landing on the new group at scale 1. Wheel events during the animation retarget it (no queueing of steps). Duration and curve are a viewer config value with 120 ms default (owner decides the feel: OQ entry). Must not add work on the window thread beyond one uniform update; measure with the zoom script.

### T3 — Smooth zoom, step 3: continuous zoom and larger zoom-out range · `LATER`
- A continuous zoom factor on the GPU from the nearest built group (bilinear or area-averaging for scale < 1, nearest or bilinear for > 1; measure both; the owner picks the look). Zoom-out beyond group 2 by scaling group 2 further down. **Scalability warning that must be designed for:** at scale 0.25 of group 2 a 4K view covers roughly 120 chunks of 512 px; chunk textures must then be sampled from mip levels (generate mips on upload, or a downsampled chunk tier), otherwise VRAM and bandwidth grow 16×. Produce a table of visible chunks, VRAM and frame time per zoom factor in `docs/perf/README.md`. Min/max zoom are config values with defaults.

### T4 — Smooth zoom, step 4: prebuild adjacent groups in the background · `LATER`
- Keep the adjacent groups' chunks of the visible area warm (priority below the current group's, bounded memory, evicted first), so the zoom cost does not depend on the game load. Measure: zoom script with a full 8-player scene (`Single_24.sze`) must show no chunk-receive lines in the first second after a step for areas that were visible before the step.

### T5 — Finish river flow stage S7 on `wip/rivers-s7` · `READY` · `PARALLEL-OK` (worktree; touches `src/render/src/river*`, `gpu.wgsl`, `src/tools/river_qa`, `docs/render/rivers.md`)
- **Context:** `docs/render/rivers.md` sections 11, 15, 16 and the S7 acceptance list there; `docs/open-questions.md` OQ-071…OQ-090; the stage workflow that did S4–S6 is described in `docs/PLAN.md` §7 item 0(b).
- **Do:** `git worktree add ../anno1503-T5 wip/rivers-s7`; rebase onto `main` (the gap patch `2f7b40d` touched `sea.cpp`, expect small conflicts); run the four gates; review the diff against the S7 acceptance in rivers.md; fix; split into small commits on `task/T5-rivers-s7`. Everything of S7 that changes the look must sit behind the existing options with the current defaults (the owner just approved gain 4 / median speed / gap patch 8 and must not see a different picture by default).
- **Do not touch** `src/app/src/viewer.cpp` zoom code or `frame.cpp` composition (T1 owns them); if S7 needs a change there, report it and stop.
- **Evidence:** the S7 tests (`river_extras_test.cpp`) with mutation checks by the reviewers; `river_qa` sheets for `D_256x256_0045_5B`, one Nord and one Steppe island at all three groups, before/after, under `C:\Users\altin\Pictures\anno-review\T5\`; default-picture digests unchanged (the enhanced scene digests must not change when the options are at their defaults; if they must, say why).
- **Acceptance:** rivers.md S7 acceptance met, gates green, `docs/render/rivers.md` section 17 written as "S7 as built" with deviations and the owner decisions it needs (as OQ candidates, not decided).

### T6 — River gap patch: check the other river islands · `READY` · `PARALLEL-OK` (tools only, no engine code) · small
- The gap patch (`sea::patch_river_gaps`, default 8 in the viewer) was judged on the owner's canyon island only. Run `river_qa` over all 106 islands with rivers with `gap_patch_tiles` 0 and 8: connected-component counts per river network, gap pixels per group, and any river that gets *more* components or new water outside the painted bed. Add a `--gap-patch N` option to `river_qa` if missing. Deliver a table (island, components before/after, gap pixels before/after) in `docs/render/rivers.md` under OQ-068's section and contact sheets of the five worst islands under `C:\Users\altin\Pictures\anno-review\T6\`. No default changes.

### T7 — Sim stage S8: commands I · `READY` · `PARALLEL-OK` (worktree; `src/sim` only)
- **Spec:** `docs/sim/state.md` §8 (commands), §15 S8 (file list, test list, exit criterion), §4.14. Also `docs/sim/core.md`.
- **Do:** exactly the S8 stage: `command.hpp`, `codec.cpp`, `apply.cpp`; `World::clock`/`rng` private with const accessors; `Seams::ai` applies commands; migrate `sim_test.cpp:193`; the listed tests in `command_codec_test.cpp`, `command_apply_test.cpp` (round trips and fuzz, "decode rejects length and count lies", "legacy raw never carries a native tag", "set production line changes output", "rejected command leaves the hash unchanged", "issuer not owner rejected", "unknown ids rejected", "set speed host only", "line not found"). Wire format per §8.5: explicit little-endian, widened ids (no `u8` player or `u16` type ids on the engine side), bounds-checked, versioned.
- **Determinism rules are hard requirements:** integer only, PRNG draws only through `draw.hpp`, ordered containers, every new state field in the one field list (`state_fields.hpp`), `tools/lint_sim.py` passes without new allow-list lines unless each has a reason.
- **Scalability:** the codec and apply path must be O(command size); no per-command scan of all buildings/players (lookups by id through the existing slot lists).
- **Reviewers:** two, with the mutation checks and the GCC -O3 check. The `asan` preset cannot be configured in WSL (no cmake/ninja there); say in the report that ASan/UBSan is untested locally, CI's `linux-clang-asan` job covers it after the merge.
- **Exit:** S1 can be driven only through `apply` and `advance`; `docs/sim/state.md` §15 S8 gets its "as implemented" note and review log entry like S0–S7 have.

### T8 — Sim stage S9: replay and session · `LATER` (after T7 is DONE)
- Per `state.md` §15 S9 including `src/tools/sim_replay/`. Unlocked by the orchestrator.

### T9 — Sim stages S10–S15 · `LATER`
- One stage per task, in the order of §15; each unlocked after the previous is merged.

## 4. Review log (orchestrator)

(empty: nothing reported yet)
