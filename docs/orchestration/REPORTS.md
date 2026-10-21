# Executor reports (append-only, newest at the bottom)

The executor session appends one section per task here when it finishes, gets blocked, or has a question the orchestrator must answer. The orchestrator reads this file, reviews the branch, and updates [`TASKS.md`](TASKS.md). Keep entries factual: names of tests, numbers, paths. No prose about intentions.

Template:

```
## <task id> — <title> — <date> — status: FINISHED | BLOCKED | QUESTION
- Branch: task/<id>-<slug> at <commit>; worktree: <path or none>
- Built: <one line per concern, with the files>
- Gates: msvc build <ok/fail>; ctest with game dir <n/n>; without <n/n>; clang-format <ok>; GCC 13 -O3 <ok/not run + why>; agree.py <ok/n.a.>; asan <ok/not run>
- Evidence: <test names added>; <bench numbers before/after>; <PNG paths>
- Reviewers: <what each found, what was changed>
- Left open / questions for the orchestrator or owner: <list; reference OQ numbers>
- Time spent / subagents used: <brief>
```

---

## T0 — Re-run the gates on `main` and push — 2026-10-20 — status: FINISHED
- Branch: main at 7d24231 (pushed, `5b3ceb2..7d24231`); worktree: none
- Built: nothing functional (scope guard held)
- Gates: msvc build ok (warning-free); ctest with game dir 594/594 on the second full run, without 594/594; clang-format 23.1.3 ok (`src`); GCC 13 -O3 not run: `git diff origin/main..main` has no change under `src/sim`, `src/formats`, `src/test_support` (only render, app, docs), and the board requires it for those only; agree.py n.a.; asan not run locally (CI job covers it)
- Evidence: CI run 39415500496, all 10 jobs success (lint, test windows/ubuntu, build-macos, cpp windows-msvc, cpp-format, cpp linux-gcc, linux-gcc-release, linux-clang-asan, macos-clang)
- Reviewers: none (gate task)
- Left open / questions: first full ctest run with the game dir at -j 16 had one failure, `anno_render_tests.scene renders match the recorded digests …` (output not captured); it passed alone (97 s) and in two further full runs (the second took ~173 s for that test under load). Suspect a load-dependent flake; not reproduced, no code change made.
- Time spent / subagents used: inline, no subagents

## T1 — Zoom stand-in chunks (OQ-077) — 2026-10-20 — status: FINISHED
- Branch: task/T1-zoom-standin at 236a71f; worktree: C:\Users\altin\anno1503-T1 (10 commits on 7d24231, not pushed, not merged)
- Built: nearest-zoom-group chunks drawn scaled in the GPU compositor for visible chunks not on the GPU yet (src/render/src/gpu.cpp, shaders/gpu.wgsl, include/anno/render/gpu.hpp); CPU reference `stand_in_source`, `stand_in_spans`, `stand_in_spans_in`, `stand_in_cover`, `stand_in_chunk` and retention of source chunks (src/render/src/frame.cpp, frame.hpp); `bench_frame --stand-ins on|off`, `--zoom-dump <dir>`, viewer `ANNO_VIEW_STAND_INS=off` (src/tools/bench_frame/main.cpp, src/app/src/viewer.cpp); docs iso.md "Stand-in chunks", OQ-077, perf/README.md. Default: on (`GpuLimits::stand_ins`); CPU path `FrameRenderer::stand_ins` off so digests are unchanged.
- Review fix (new in this round): the source group is now chosen per part, not per chunk. `stand_in_cover` splits a part whose source chunk is missing in the nearest group by the source chunks of the next group (and so on), merges joined parts of one source chunk, and is shared by the CPU reference and the GPU, so both choose alike. Only what no group covers is open sea.
- Gates: msvc build ok (no warnings); ctest with game dir 598/599 in the full -j 16 run (the one failure, 'legacy river shading reproduces the previous enhanced digests (game files)', is the known load-only kind: rerun alone it passed in 589 s); without game dir 599/599; clang-format 23.1.3 ok; GCC 13 -O3 not run (only render and app touched, nothing from the known traps added); agree.py n.a.; asan not run
- Evidence: tests in gpu_test.cpp: "stand-in spans tile the destination chunk and map by the tile ratio of kTile", "CPU reference: a missing chunk shows the scaled chunks of the nearest built group", "GPU stand-in chunks equal the CPU reference (synthetic)", and new: "stand-in cover chooses the source group per part" (all target groups, hole filled by the next group, disjoint parts tile the chunk, nothing available gives open sea), "GPU stand-in with a hole in the nearest group equals the CPU reference, and the real chunk replaces it" (target groups 0, 1, 2; within 1 per channel; next frame draws no stand-in and equals the stand-ins-off frame). Mutations killed (reviewer and implementer): shifted pixel centre in shader and CPU, scale, swap condition, default switch, tie order, CPU y offset, CPU scale swap, CPU missing condition. Bench (bench_frame --zoom 6 --renderer gpu --chunks async --limits interactive 2560x1369 enhanced, 01_1024_easy_256.sze, RTX 4090, Ryzen 9800X3D, three alternated runs each, load 13-38 % at start with other tasks building, medians p50/p95/p99/max ms): group 0 before 0.4/1.8/2.9/8.6, after 0.5/2.1/5.3/19.2; group 1 before 0.4/1.6/2.0/5.5, after 0.6/2.2/2.7/3.5; group 2 before 0.4/2.0/3.4/7.0, after 0.7/2.7/9.1/19.1. Run-to-run spread (e.g. after group 2 max 3.5-34.4) is larger than the differences; the quietest after run (0.5/2.1/3.2/4.5, 0.5/1.8/2.6/3.2, 0.5/1.6/2.4/3.5) matches the before runs. Same binary, --stand-ins off vs on (4 alternated runs, load 19-82 %): p50 off 0.5-0.8 (median 0.6), on 0.5-0.8 (median 0.5); p95 median off 2.4 / on 2.0; p99 off 7.6 / on 4.0: no cost found from the feature. A small cost of 0.1-0.3 ms in p50 versus the main build can neither be excluded nor confirmed on this loaded machine. Stand-ins drawn on 43 of 52 (group 0 start) and 34 of 48 (group 1 start) frames with missing chunks; first frames after a zoom step: 8 of 9 and 6 of 8. PNGs: C:\Users\altin\Pictures\anno-review\T1\ (first_frame_after_zoom_out_before/after.png clear pair; all_before, all_after folders; made before the per-part fix, so the half-sea cut-offs seen in the review PNG should now be filled where another group has the chunk; not regenerated).
- Reviewers: two independent reviewers ran on the earlier state. Finding 1 (medium, one group per chunk leaves half-sea stand-ins): valid, fixed (stand_in_cover, per part, CPU and GPU, tests above). Finding 2 (medium, thin test coverage): partly valid, fixed for the partial-source case, the swap to the real chunk, and target groups 0 and 2; not covered: water objects in shallow water (`object_in_water` rescale) and sea field cover (`sea_fs` with chunk_local) have no pixel test, stated in iso.md. Mutation survivor 'remove the !real check on sources': rejected as an equivalent mutant: GPU chunks that are not real exist only for visible keys of the current group and `sync_chunks` drops them on a group change, so a source chunk of another group is always real; the check is a guard, noted in iso.md. Bench not reproduced (medium): valid, re-measured with repeats, spread and load, wording changed from 'no regression' to 'within noise, small cost not excluded'. Low findings: CPU path per-frame allocation (CPU reference, off by default, documented), shader overflow path (comment added in gpu.wgsl), piece stride 256 (already documented as the WebGPU guarantee), nearest-neighbour quality (OQ-077, T3), CHECK-then-index crash in the CPU test (not changed, the failure is still a failure).
- Left open / questions for the orchestrator or owner: OQ-077 (owner to confirm scaled stand-ins over open sea; cross-fade wanted? none built; rivers on a stand-in use the legacy look; the CPU viewer path still draws open sea). Unexplained: `render::div_euclid(int, int)` with kChunk returned -2 instead of -1 for -512 / 512 in some contexts of the test TU (a debug MESSAGE printed -1); the test uses its own floor division helper; frame.cpp results matched an independent check and the GPU, so the library is believed fine, but it deserves a look (MSVC optimiser?) before anything relies on `div_euclid` at exact negative multiples. 'legacy river shading ...' test takes 589 s alone on a loaded machine and failed once at -j 16 under load (not a stand-in effect; it does not use the GPU path).
- Time spent / subagents used: no subagent tool available in this session; the reviewer findings were supplied by the orchestrator; no T2 work started

## T10 — Reproduce the digest-test flake under load — 2026-10-20 — status: FINISHED (mitigation, root cause NOT established)
- Branch: task/T10-digest-flake at 9f42def; worktree: C:\Users\altin\anno1503-T10 (3 commits on 7d24231 base, not pushed, not merged). `docs/orchestration/*` untouched; the diff against current `main` shows docs deletions only because the branch is older than `main`.
- Built:
  - `src/test_support/include/anno/test/parallel.hpp`, `src/test_support/src/memory.cpp`: `test_threads()` is bounded by free memory / `ANNO_TEST_MB_PER_THREAD` (default 1024). `parallel_indexed` waits for memory before each item (all workers but the first) and re-runs an item after `std::bad_alloc` serially after the pool, up to 5 times.
  - `CMakePresets.json`: every test preset writes `build/<preset>/Testing/ctest-output.log` (`outputLogFile`, `outputOnFailure` kept). `CLAUDE.md` build line updated.
  - `src/test_support/tests/parallel_test.cpp`: 5 test cases.
  - Review fixes in 9f42def:
    - The bound now measures what `bad_alloc` depends on. On Windows `free_memory_bytes()` is min(`ullAvailPhys`, `ullAvailPageFile`), i.e. free RAM and commit-limit headroom. On Linux it is still `MemAvailable`.
    - `ANNO_TEST_MB_PER_THREAD` is parsed strictly. Empty or non-numeric falls back to the default, and only an explicit `0` switches the bound off. Test: "memory_budget_bytes ignores an empty or non-numeric value".
    - `std::bad_array_new_length` is deterministic and is no longer retried. Test: "parallel_indexed does not retry std::bad_array_new_length".
    - The wait is configurable (`ANNO_TEST_MEM_WAIT_S`, default 120 s, 0 = never wait) and no longer a magic constant. A comment says the pool's own workers count as used memory, so a pool may wait for itself; the wait is bounded and only delays an item.
    - A comment says `f` must have no side effects outside its return value (true for all current callers), because an item may run twice.
- Root cause (reviewer finding 1, accepted): not established.
  - The original failure was a digest-test failure whose output was lost. None of the 6 reproduction runs before the first fix reproduced it. The only failure ever captured was a `bad_alloc` (`render_corpus_test.cpp:294`), seen after the first fix.
  - "Memory" is therefore an inference. A bad_alloc is raised by the commit limit, not by free RAM; the machine showed 87-90 GB commit charge against 31 GB RAM.
  - A data race in the parallel scene renderer is not excluded. It was only not observed in 6 + 4 runs plus the 4 runs below, all of which produced matching digests.
  - This is a mitigation plus a permanent log, not a diagnosis. If the digest test fails again, `ctest-output.log` will now hold the output; a digest mismatch (as opposed to a crash or bad_alloc) must go to BLOCKED unfixed.
- Evidence (peak commit charge, GB, sampled every 2 s; the machine was shared with T1/T6/T7 builds):
  - Before (the three runs before the fix, from the previous session's logs, recomputed by the reviewer): commit max 90.58 / 89.02 / 87.43; free RAM minimum 0 in all three; samples below 1 GB free 104 of 750, 56 of 643, 37 of 589.
  - After (final runs on 9f42def, game dir): run 1 max 81.0 (217 samples, free-RAM minimum 0, 3 samples below 1 GB), run 2 max 87.0 (271 samples, minimum 0, 2 below), run 3 max 73.0 (304 samples, minimum 0, 6 below). Without game dir: max 44.0, minimum free 3 GB. The CSVs are final_mem1.csv, final_mem2.csv, final_mem3.csv and final_mem_nogame.csv in the scratchpad directory.
  - Reading (reviewer finding 2, accepted): the commit-charge figures cannot be attributed to the fix. Other worktrees run on the same PC, the machine still hit 0 GB free in every run, and a drop from about 90 to about 73-87 GB is within noise. The per-process peak of the test binary was not measured. The only supportable statements are that no run failed and that the bound plus retry changed nothing observable except that failures did not occur.
  - Full runs: 599/599 x3 with the game dir at `-j 16` and `ANNO_TEST_THREADS=16` (total 487 s, 623 s, 703 s; the `legacy river shading` test passed each time), and 599/599 without. Logs: final_game1.log, final_game2.log, final_game3.log and final_nogame.log in the scratchpad directory, each written with `--output-log`.
  - The `bad_alloc` string in those logs matches only the new test's name, so no `bad_alloc` occurred.
- Reviewers: the orchestrator's findings, handled as follows:
  - 1 (root cause not established, commit limit vs free RAM): valid. The bound now uses commit headroom, and this report states it is a mitigation.
  - 2 (before/after not attributable): valid, stated above.
  - 3 (wait throttles on the pool's own usage, hardcoded constants): valid. Constants are now configurable and the wait is documented; the self-wait remains by design and is bounded.
  - 4 (empty `ANNO_TEST_MB_PER_THREAD` turns the bound off): valid, fixed and tested.
  - 5 (`bad_array_new_length`, side effects): valid, fixed, tested and commented.
  - 6 (no CTest `PROCESSORS`/`RESOURCE_LOCK`): not changed. `doctest_discover_tests` applies properties per executable, so a lock would serialize all render tests. This is a known remaining risk: heavy corpus tests can still start together at `-j 16`. Offer: add per-test properties through a custom discovery step if the orchestrator wants it.
  - 7 (verified OK): nothing to change.
- Left open / questions: root cause of the original digest failure (memory vs race) is unknown; recommend keeping the output log and treating any future digest mismatch as BLOCKED. The CTest resource property (finding 6) is open. ASan/UBSan not run locally (CI `linux-clang-asan` covers it after the merge).
- Time spent / subagents used: inline, no subagents; the machine was under load from T1/T6/T7 throughout. No other process was killed.

## T7 — Sim stage S8: commands, codec, World::apply — 2026-10-20 — status: FINISHED
- Branch: task/T7-sim-s8-commands at ed59370; worktree: C:\Users\altin\anno1503-T7
- Built: command codec (src/sim/include/anno/sim/command.hpp, src/sim/src/codec.cpp); World::apply (apply.hpp, apply.cpp); World::clock/rng private with const accessors; AI seam via PlayerHooks::ai and Scheduler::run_ai (scheduler.cpp); kCommandWireVersion in tools/lint_sim.py; docs/sim/state.md S8 paragraph. No state-stream change (kStateHashVersion 105, goldens unchanged). Finalisation adds one commit: ed59370 test(sim): turn entry prefix and mutation fuzz around the 8-byte header (src/sim/tests/command_codec_test.cpp).
- Gates: msvc build ok; ctest with game dir 618/618; without 618/618; clang-format 23.1.3 ok; GCC 13 -O3 -Werror ok (427 sim cases, with and without game files); agree.py n.a. (no anno-dump change); asan ok locally in WSL (g++ 13.3 -fsanitize=address,undefined -fno-sanitize-recover, 427/427 with and without game files; CI linux-clang-asan still to run after merge)
- Evidence: 23 new cases (command_codec_test.cpp 7 + 1 added now = 8; command_apply_test.cpp 16), 427 sim cases in total. New case: "command wire: turn entry prefixes and mutations" (every prefix of every sample turn entry is an error; all 256 values of the first 64 bytes of each entry re-encode canonically; sizes 0..8 rejected). Full ctest: first run of the with-game-dir pass needed 623 s total (render test "legacy river shading reproduces the previous enhanced digests (game files)" 621 s under load) and passed; the two render failures seen earlier under load did not recur this run.
- Reviewers: reviewer 1 (5 low findings, all accepted as low): (a) island_at_mut is O(islands), left, documented in state.md and noted as perf backlog; (b) switch_line bounds checks on line_on unreachable while check_invariants holds, left, defensive; (c) AI SetSpeed from inside advance allowed by spec 8.2 item 4, not changed (list below); (d) shared-worktree process issue: reviewer tree is clean now (git status empty after both reviewers); (e) GCC and ASan unverified there, now run here and green. Reviewer 2 (1 medium, 5 low): MEDIUM turn-entry short input 4..7 bytes untested: fixed by ed59370 (test added). Honest limit: the mutation `size() < 8` to `< 7` still survives the new test on MSVC Release, because a 7-byte input then fails inside decode_record instead of being caught by the bounds check; the difference is only the span precondition violation, which needs ASan/debug-iterator to be observable (WSL ASan run does not exercise the mutation, it was not applied there). The guard itself is correct and the test pins every prefix size as an error. Low findings rejected or left: AI draw-record bookkeeping (saw_ai_draw does not pin double counting) - left, covered indirectly by the ordering test; Reject::UnknownType unreachable with a consistent world - left; neutral_owner, count_fits overflow guard and decode_record header-size mutants judged equivalent/redundant.
- Left open / questions for the orchestrator or owner: (1) should an AI-issued SetSpeed be refused inside advance? Currently allowed (validated like any command); a rules decision, not made here. (2) Optional hardening tests (switch_line short line_on via WorldTestAccess, UnknownType injection, Ai draw-count equality) not added; candidates for S10. (3) island lookup O(islands) per SetProductionLine: S10/perf backlog. No OQ numbers added.
- Time spent / subagents used: inline, no subagents; most of the time was the loaded 618-test ctest run (623 s).

## T6 — River gap patch: check the other river islands — 2026-10-20 — status: FINISHED
- Branch: task/T6-gap-patch-survey at ace2309; worktree: C:\Users\altin\anno1503-T6
- Built: `river_qa --gap-patch N` and a per-island `edge px` column (src/tools/river_qa/main.cpp, d32ff03); `python tools/river_qa.py gap-patch` survey with table, regression flags and contact sheets (tools/river_qa_gap.py, tools/river_qa.py, tools/test_river_qa_gap.py, ctest `river_qa_gap_survey`, 7d2765a); after review: per-island header separator fixed to 11 cells and a final stderr line `scenes: tried N loaded N failed N with-river N` (cf56303); sheets ranked by growth of water outside the painted bed, coverage report, edge px in the summary, per-island growth % in the table, absolute exe path, 9 new tests (d85eeed); rivers.md 13.1 and OQ-068 rewritten from the rerun (ace2309). No engine code, no defaults changed.
- Gates: msvc build ok (0 warnings); ctest with game dir 592/595 on the first run, the 3 failures (anno_render_tests: "scene renders match the recorded digests", "legacy river shading reproduces the previous enhanced digests", "the light of the flow shading against the legacy shimmer") threw "bad allocation" under load and pass alone with -j 1 (109.6 s, 434 s, 4.9 s), so 595/595; without game dir 595/595; clang-format 23.1.3 ok (main.cpp); GCC 13 -O3 not run (tools change only: size_t counters and one printf); agree.py n.a.; asan not run (tools and docs only)
- Evidence: tools/test_river_qa_gap.py 23 tests pass with and without ANNO1503_GAME_DIR (3 skip without); survey rerun over 444 island templates: tried 444, loaded 444, failed 0, with river pieces 72 (the 21 dummy islands: 0 with river pieces); no regression by the rules; gap px 94 -> 0 (g0), 0 -> 0 (g1, g2); E comps 363 -> 339 (g1; 8 islands improve, 64 unchanged, none worse); edge px 87,702 -> 56,359 (g0), 26,083 -> 19,738 (g1), 10,297 -> 9,420 (g2); water outside the painted bed, share of original river px: g0 6.5 -> 17.5 %, g1 5.8 -> 15.3 %, g2 4.7 -> 11.4 %, largest single island 19.5 % (N_128x128_0005_1B_Piraten g0); absolute extra px grows 2.7x at g1 overall, up to 11.6x (N_256x256_0007_2B 3,125 -> 36,127). Sheets (7) in C:\Users\altin\Pictures\anno-review\T6\: N_128x128_0005_1B, N_128x128_0049_1B_Indianer, D_256x256_0003_4B, N_128x128_0010_1B, N_128x128_0007_1B, N_256x256_0007_2B, N_256x256_0016_3B (`<island>_i0_gap-patch-8.png`). The sheets of N_256x256_0007_2B and N_256x256_0016_3B show a few-tile water tail at the open river ends (source end, lower-left end) in addition to the bank fringe. Docs: docs/render/rivers.md 13.1, docs/open-questions.md OQ-068.
- Reviewers: reviewer 1 (6 findings) and reviewer 2 (7 findings, about 35 mutations). Fixed: unrepresentative worst-island ranking (now regressions, remaining gap px, growth share, surplus comps; rule stated in the doc; two extra sheets for the largest absolute growers), unverified coverage (stderr coverage line, parsed and cross-checked between the two runs, reported), growth hidden behind the binary flag (growth % column and summary lead), edge px not shown or tested, untested edge px, --gap-patch validation, glue code (run_per_island with a fake river_qa, gap_patch with and without cache), mutation survivors in badness, worst_islands dedupe signature, count, summary split/lost, table separator, relative --exe path, per-island header separator. Rejected: stray-edit finding (no code change needed, tree clean, committed code correct); max(before['o'],1) mutation (equivalent); full re-run of the mutation pass (not done).
- Left open / questions for the orchestrator or owner: OQ-068 owner decision: keep gap_patch_tiles 8, choose a smaller value, or deepen only the real gap cells; the patch also adds a few-tile tail at open river ends (see sheets N_256x256_0007_2B, N_256x256_0016_3B). The task text's "106 islands" was not reproduced: 72 templates carry river pieces; the 50 scenarios (.sze) place 112 river islands in 36 scenarios (group 0, measured once). The survey covers templates only, not placed instances (whether they match was not checked). EXTRA_GROWTH_FRACTION 0.25 has no evidence and cannot fire (max 19.5 %). Under load the three large render corpus tests can throw bad allocation with 16 parallel jobs while another task runs; they pass alone.
- Time spent / subagents used: about 3 h wall clock (survey rerun about 25 min, sze scan 6.5 min, full build and ctest under load about 25 min); no subagents.

## T1b: Stand-ins, the unattributed hitch, the artifacts, the evidence (finalize) - 2026-10-20 - status: QUESTION

- Branch: task/T1-zoom-standin at f6be695, worktree C:\Users\altin\anno1503-T1. Not pushed, not merged. `docs/orchestration/*` untouched.
- **Process note for the orchestrator:** the reviewer findings in this step were produced by workflow subagents. This executor session did not spawn any agents. Nothing in the findings was run through an extra reviewer after the fixes.
- **Not done: the idle-machine bench table (step 4).** The owner's game `FC27` was running the whole time (CPU 50-82 %, minimum over two minutes of polling 50 %). I did not stop it because it is the owner's process. Every number below is from a loaded machine, and the load is written next to it. Closing the game and repeating the commands in `docs/perf/README.md` ("T1b finalize") is all that is needed.
- **Not done: the owner's 26-39 ms frames are still not reproduced or explained.** After steps 1-3 and this step the worst frame was 2.0-4.4 ms in most runs. Acceptance ("no busy frame above 8 ms after a zoom step") holds in 29 of 30 acceptance rows from step 2 (one 12.9 ms frame, not reproduced). In this step's table, 11 of 60 runs had a max above 8 ms (4 of 30 with stand-ins off, 7 of 30 on), both modes, on a machine at 57-82 % load.

### What was found and changed, per step

**Step 1: instrumentation (earlier commits ecf9763, 46c1829, 94c7aee).** Every window-thread step of a GPU frame is timed and printed in the `frame hitch:` line, together with "sum" and "busy minus sum".
- **Hitch-line sum proof, stated honestly.** The zero is *by construction*. The steps are laps of one clock, so they add up to the frame. `sync scan` and `release` are residuals that absorb whatever is left. A reviewer was right to call it not evidence. It shows only that no interval inside `render()` is unnamed. `docs/perf/README.md` now says so explicitly.
- **Independent checks added or already present.**
  - The viewer's wall clock: busy = `draw()` start to end minus the surface-acquire wait.
  - The gap between frame starts, the end-to-start time between frames, and the time in window events.
  - New in this step: the doctest "the steps of a GPU frame add up..." wraps `render()` in an outside stopwatch. `total_ms` must be within 0.05 ms above and 25 ms below the call's own time.
- **Not covered by the sum:** the surface acquire, SDL present and the display (see OQ-094).

**Step 2: fixes the numbers named.** The first three were done earlier; this step adds the last two.
- Texture creation for the first uploads into a zoom group (10-19 ms, `create` 10-17 ms for 2-3 chunks, in about 4 of 6 runs). A background thread now pre-creates `prewarm_chunks` (24, about 96 MB) chunks' planes.
- Background threads run below normal priority. Evidence is weak: one frame in 8 runs on a loaded machine. Kept because it is cheap and has no look-and-feel stake.
- A chunk replaced by its rebuild is freed on the reaper thread.
- **New in this step:** the pre-create thread stops when the device refuses a texture instead of putting a null plane into the pool (commit 37d01c5). This was a valid reviewer point.
- **New in this step:** commit 985a268 fixes the viewer ignoring `--water enhanced`. `run_viewer` took no water argument, so the owner's check command always started in original water. Observations the owner made "in enhanced mode" therefore came from pressing `E` (or the zoom script's `enhanced` token). This was an obvious bug and is fixed.
- **Rejected reviewer findings, with reasons:**
  - "Planes freed beyond `kPoolPlanes` still go to the window thread": true and documented, `planes_freed` was 0 in every run, so no change is justified without a measured case.
  - "The pre-create count is not derived from the view size": this is OQ-093, an owner decision (it is a `GpuLimits` field with a documented reason, so it does not break the no-hardcoded-limits rule).
  - "Mutation `(void)h` in `take_warm` untested": all chunk planes are `kChunk` x `kChunk`, so the mutation cannot be caught through the public API. The `h` comparison is defence against a future second plane size.
- **The reviewer's 30-minute stall in the prewarm tests is not reproduced.** I re-ran both mutations on the test "pre-created chunk planes are taken by the first uploads, and an early end joins the thread".
  - Removing `++planes_warm_taken` makes it fail in under 1 s (`CHECK( st.planes_warm >= 1 )`).
  - Setting the refill target to 0 makes it fail after the test's own 10 s bounded wait (`warm_planes_ready() == 10`).
  - Every wait in those tests is bounded to 10 s, and the unmutated test passes alone in 0.8 s. The stall was therefore somewhere else, probably the whole suite on a loaded machine.
- Still named, not fixed: `wgpuQueueSubmit` / `finish` at 8-14 ms in single enhanced frames (cause not found, GPU or driver work behind the submit). One `fluid` step at 9.4 ms in 40 runs. In this step's runs one off-mode run had a `finish` of 10 ms with nothing else slow, which points to a descheduled thread on the loaded machine. The first frame of the program (sea upload 24-31 ms, wave simulation creation 27-34 ms) is excluded from the acceptance.

**Step 3: artifacts (measured, not fixed; owner decides).** From the step-3 entry in `docs/perf/README.md` plus this step.
- (a) **Rivers on a stand-in flip from legacy shimmer to flow shading when the real chunk lands: real, small.** River pixels move by 2.5-8.6 (sum over channels) against 1-3 for the animation alone, and about 4,000 pixels of 60 million differ by more than 30 on whole-island renders. Open as OQ-091. Not fixed because it needs the same change in the GPU shader and in the CPU reference.
- (b) **The wave window is rebuilt after a zoom: real, visible on open sea.** It is in flight for 9-25 frames (14-48 ms) at every step. On the zoom-out to group 2 a diamond of animated waves appears and is gone about 5 frames later. It also appears with `ANNO_VIEW_STAND_INS=off`. Open as OQ-076.
- (c) **Blocky nearest-neighbour stand-ins next to sharp built chunks: the most visible one.** Frame 0 after a zoom into group 0 has 16 % of pixels differing from the settled frame by more than 60. The blocky areas stay 25-69 ms where chunks have to be built. With stand-ins off the same frames show open sea (65 % differ at frame 0), so the stand-ins are the smaller artifact. Open as OQ-092.
- Evidence PNGs: C:\Users\altin\Pictures\anno-review\T1b\ (a_*, b_*, c_*).

**Step 4: evidence (see above).** Loaded-machine table, `D_256x256_0045_5B`, enhanced, GPU async interactive, `bench_frame --zoom 6 ... --zoom-frames 480 --stand-ins off|on`, one binary, five alternated runs per resolution. Load at the start of each run: 57-82 % (1280x800) and 62-74 % (2560x1440). Medians of p50 / p95 / p99 / max in ms; in brackets, runs of five with max above 8 ms.

| res | group | stand-ins off | stand-ins on |
|---|---|---|---|
| 1280x800 | 0 | 0.4 / 1.7 / 3.5 / 6.4 [0] | 0.5 / 2.1 / 3.5 / 5.7 [1, max 15.9] |
| 1280x800 | 1 | 0.4 / 1.6 / 2.9 / 6.2 [2, max 9.5] | 0.5 / 1.7 / 2.8 / 5.2 [0] |
| 1280x800 | 2 | 0.4 / 1.8 / 3.0 / 5.8 [0] | 0.5 / 1.7 / 3.1 / 6.0 [2, max 10.9] |
| 2560x1440 | 0 | 0.5 / 2.0 / 3.6 / 6.6 [0] | 0.8 / 2.6 / 4.5 / 7.7 [2, max 12.8] |
| 2560x1440 | 1 | 0.6 / 1.9 / 3.1 / 7.3 [2, max 9.0] | 0.7 / 2.4 / 3.5 / 6.5 [1, max 8.3] |
| 2560x1440 | 2 | 0.6 / 1.9 / 3.3 / 6.9 [0] | 0.7 / 2.4 / 3.4 / 6.7 [1, max 10.5] |

- **Real but small cost:** p50 is +0.1 ms at 1280x800 and +0.1-0.3 ms at 2560x1440 with stand-ins on.
- **Earlier p99/max regression** (group 0 2.9/8.6 to 5.3/19.2, group 2 3.4/7.0 to 9.1/19.1): **not reproduced.** Here p99 is 2.8-4.5 ms, and large maxima occur with stand-ins off too. It is "not reproduced under load", not "explained". An idle run is still needed to close it.
- **PNG pairs regenerated** after the per-part fix, in C:\Users\altin\Pictures\anno-review\T1\, same binary, before = stand-ins off, after = on:
  - `v2_first_frame_after_zoom_in_g0_{before,after}.png`: 56 % of pixels differ by more than 30; before is open sea, after is stand-ins.
  - `v2_first_frame_after_zoom_out_g1_{before,after}.png`: 22 % differ.
  - `v2_first_frame_after_zoom_out_g2_{before,after}.png`: 0 % differ, because the group-2 chunks were already built and a revisit is unaffected.
  - I viewed one of the after frames (zoom-in to group 0): it shows the full island, not black or empty.

**Step 5: `div_euclid` note.** Closed by reconstruction only: the original run's message no longer exists. The -2 is a stand-in source chunk (`stand_in_source` at a chunk boundary), not `div_euclid` of -512 / 512. The test `render_test.cpp` "div_euclid at exact multiples of kChunk" (commit 5631031) pins the library function. The phrase "believed fine" in `REPORTS.md` line 36 (the T1 entry) is still there. I may not edit `docs/orchestration/*`, so the orchestrator must replace it with the stand-in-source explanation.

### Open questions for the owner
- **OQ-091** (existing): rivers on a stand-in. Keep legacy look (default), scale the source chunk's flow lattice, or cross-fade.
- **OQ-092** (existing): blocky stand-ins. Keep nearest neighbour (default), bilinear, or cross-fade (T2/T3 territory).
- **OQ-076** (existing): wave window rebuilt after a zoom. Cross-fade, snap, or hold.
- **OQ-093** (new): how many pre-created planes (24, about 96 MB, tuned for 2560x1369 at one group). Options: derive from the view and a VRAM budget, or expose it in the viewer config.
- **OQ-094** (new, needs the owner's machine): hitches in the display path. The monitor is on the AMD iGPU while rendering runs on the 4090, so the presented frame crosses GPUs, a path `bench_frame` cannot see. The `frame hitch:` line separates "waiting for the display" from "busy".

### Owner-check
Close FC27 first if you want the idle benchmark rerun, and watch the viewer without anything else running.

Exact viewer command (now really starts in enhanced water):
`build\msvc\src\app\Release\anno1503.exe --view game\app\Scenes\Islands\Dschungel\D_256x256_0045_5B.scp --game-dir game\app --water enhanced`
(Use the build from `C:\Users\altin\anno1503-T1` at f6be695.)

To see hitch details: set `ANNO_VIEW_HITCH_MS=8` and paste the `frame hitch:` lines. Read "busy" against "waiting for the display" (OQ-094).

Isolating switches:
1. `ANNO_VIEW_STAND_INS=off`: chunks that are not built yet show as open sea instead of stand-ins.
2. `--water original`: removes the river and wave-window differences. The blockiness stays.

**Five-choice question: what do you see while zooming in enhanced mode?**
1. Blank or flashing patches.
2. Hard rectangular cut-offs or seams.
3. Blurry areas turning sharp.
4. Shimmering or flickering water or rivers.
5. Other (describe).

Please say which, with and without each switch. Mapping to the measured causes: 3 is the blocky stand-ins (c); 4 is the river flip (a) or the wave window (b); 1 is open sea if stand-ins are off or no group covers the area; 2 can be a stand-in boundary.

### What is left
- The idle-machine bench table (blocked by the running game).
- The owner's 26-39 ms frames: still unexplained on this machine.
- The orchestrator edit of `REPORTS.md` line 36.
- The owner's artifact decision (five-choice question above).
- Then `OWNER-CHECK`.

## T1c — Stand-ins: review fixes, idle bench, owner check — 2026-10-20 — status: FINISHED (OWNER-CHECK)
- Branch: task/T1-zoom-standin at 740fee8 (9 commits on f6be695: 80e531c, 60925a6, b659659, eeb78cd, c891cf9, 7298490, f3e0067, bb4cbe2, 740fee8); worktree: C:\Users\altin\anno1503-T1. Not pushed, not merged.
- **Process note for the orchestrator:** done inline in one session, one build or benchmark at a time, no subagents and no independent reviewers (the owner asked for no parallel tasks while benchmarks and tests run). The orchestrator should run the reviewers.
- Built, per board item:
  1. HIGH priority inversion: the pre-creation thread runs at normal priority and creates one plane per quiet frame (token from `render()` when nothing was uploaded and no placeholder was made; unpaced only until the first `render()`); `ANNO_VIEW_PREWARM=off` added, `ANNO_BACKGROUND_PRIORITY=normal` already existed. Test: the existing pre-created-planes test extended (a busy frame gives no refill for 300 ms, each quiet frame gives exactly one plane); mutation (token also on busy frames) fails it.
  2. HIGH 37d01c5: the refused-texture detection now compares `ref->errors` (uncaptured-error counter) before and after each creation and stops at the first change; no error scope; perf README claim corrected. No unit test (the device cannot be made to refuse a 512 x 512 texture).
  3. MEDIUM retain: `FrameRenderer::stand_in_retain` (testable), empty unless `GpuLimits::stand_ins` (set by `render()` via `retain_stand_in_sources`) or the CPU switch is on, and only for visible chunks that are not built. Test "stand-in sources are kept only for visible chunks that are not built, and only when asked"; mutation (always on) fails it.
  4. MEDIUM placeholders: **not changed, with evidence.** T1b's own numbers: 12-18 placeholders in the first frame cost 0.13-0.32 ms in total (docs/perf/README.md, T1b step 2). Removing them also changes pixels: a placeholder draws the sea pattern through the chunk shader; with none drawn the test "stand-in chunks show the open sea (game files)" failed (CHECK max <= 1 got 38). Reverted. A slab would need a new chunk bind group layout for no measured gain. Reasoned in the perf README.
  5. MEDIUM VRAM: `GpuLimits::idle_plane_budget_mb` (256, reason in the header) bounds pooled plus pre-created planes; the thread's target is `prewarm_chunks * 5` minus the pool, within the budget; the viewer sizes `prewarm_chunks` from the window in chunks (not below 24; a later resize is not followed: OQ-093). Test "pre-created chunk planes stay within the idle plane budget"; mutation (budget + 99) fails it.
  6. MEDIUM docs: OQ-094 now says `present` is inside busy (display-path stalls can land there) and that the hitch line's own console write lands in the next frame's gap.
  7. LOW hypothesis: **fixed without being able to confirm it on the owner's console.** `zoom group N: k chunk(s) received` was written per frame after present (outside every step); it is now counted and printed once in the periodic report ("chunk(s) received in this interval: N"). My runs write to a pipe, so the console cost itself was not reproduced; the owner can compare. Viewer run with `ANNO_VIEW_ZOOM_SCRIPT=4,12` (enhanced, quiet machine): after the first interval (28.6 ms worst, program start) worst frames 0.8-5.8 ms.
  8. LOW: `take_warm` waits for the (brief) lock instead of try_lock, the thread's scarcest-format pick uses per-format counters instead of `count_if` under the lock; test "GPU stand-ins carry the sea field: cover 0 shows the open water, cover 255 the sea" (checkerboard field, original water, `sea_fs` through `chunk_local` on the stand-in; mutation `cover = 255u` in gpu.wgsl fails it); the `base` plane note added to `stand_in_chunk` (source-group rows, copied texel by texel; the GPU samples it alike).
- Gates (sequential, nothing else running): msvc build warning-free; ctest -j 16 with `ANNO1503_GAME_DIR` and `ANNO_TEST_THREADS=16`: 602 of 605 first, the three failures (`chunk corpus`, `scene renders match the recorded digests`, `legacy river shading reproduces the previous enhanced digests`) were `bad allocation` (log kept in the session scratchpad, `t1c_game.out`), all three pass alone with `-j 1` (68 s, 337 s, chunk corpus fast); without the game dir 605/605; clang-format 23.1.3 over `src` clean; GCC 13 not run (render/app only, no known trap added); agree.py n.a.; asan not run. **The bad_alloc under -j 16 is the T10 problem: the memory bound of branch `task/T10-digest-flake` is not on this branch.**
- Evidence: quiet-machine table in docs/perf/README.md ("T1c idle-machine bench"): 24 rows (2 resolutions x 3 groups x stand-ins on/off x prewarm on/off), five rounds, load 2-36 % (36 of 40 runs below 20 %: quiet, but not the "below 10 %" the task asked for), medians of max 2.0-3.1 ms; 119 of 120 runs have max <= 8 ms, the miss is 12.6 ms (2560x1440 group 1, stand-ins off, prewarm on; not reproduced in the other four runs of that configuration). The T1b loaded-machine regression (group 0 p99/max 5.3/19.2, group 2 9.1/19.1) is not present: it was load. Stand-ins cost: p50 equal, p95 +0.2 ms at 2560x1440 groups 0-1. Prewarm shows no difference in this steady-state bench (it starts after the pool is ready); its effect is the first uploads into an uncached group, for the owner to A/B. PNGs not regenerated in T1c (the per-part-fix pairs exist under `Pictures\anno-review\T1\v2_*`).
- Left open / questions: OQ-091, OQ-092, OQ-076 (now decided by the owner, T1d), OQ-093 (prewarm size / resize). T10b is needed before this branch can pass a full `-j 16` ctest reliably.
- Owner check (first build that starts in enhanced water from the command line), in one line: `cd C:\Users\altin\anno1503-T1\build\msvc\src\app\Release; $g = "C:\Users\altin\anno1503\game\app"; .\anno1503.exe --view "$g\Scenes\Islands\Dschungel\D_256x256_0045_5B.scp" --game-dir $g --water enhanced`. Zoom fast with the wheel. Isolating switches: `$env:ANNO_VIEW_STAND_INS="off"`, `$env:ANNO_VIEW_PREWARM="off"`, `--water original`. What to look at: the hitch lines (`frame hitch:`) in the terminal while zooming, and which of the artifact kinds remain.
- Time spent / subagents used: inline, no subagents.

## T1d — Remove the zoom artifacts (pan while zooming) — 2026-10-20 — status: FINISHED (OWNER-CHECK); items 3 and 4 not done (conditional)
- Branch: task/T1d-zoom-artifacts at b0588c5 (7 commits on a079abd: a7ca41d, 383753a, 3b70c3b, 3edb140, 6ef2400, b0588c5, plus the docs); worktree: C:\Users\altin\anno1503-T1d. Not pushed, not merged.
- **Process note for the orchestrator:** done inline in one session, one build or benchmark at a time (owner's instruction), no subagents and no independent reviewers; please run the reviewers. Mutation checks were done by hand on the new tests (listed below).
- **Reproduced first** (`bench_frame --zoom 60 --zoom-pan 4,2,400 --pop on`, new options): with the T1c code a pan of about 1600 px/s while zooming gives open sea (a visible chunk with no source in any zoom group) in 9-86 of 1600 frames, runs up to 325 ms, and single frame pairs where 14-30 % of the pixels jump by more than 40 levels. New instrumentation: `GpuStats::open_sea_chunks` / `open_sea_built` / `chunks_fading` / `chunks_prefetched`; bench `--zoom-pan dx,dy[,frames]`, `--pop on|off`, `--prefetch n`, `--fade ms`.
- **Cause, measured:** a chunk takes 100-250 ms on one worker thread (cold frame of 12 chunks 118-247 ms with 16 threads); of the open-sea chunks of the pan runs almost none were built on the CPU, so the limit is build latency and queue order (the queue was in key order, not by nearness; neighbouring groups were only built once the current one was nearly done and were never uploaded, so a zoom step had no stand-in source for the part of the new view that was off screen).
- Built, per board item:
  5. **Margin of pre-built chunks** (`FrameRenderer::plan_margin`, `lookahead_rect`, `ChunkWorkers::build_ms`, `GpuLimits::prefetch_chunks` 96 / `prefetch_per_frame` 1): margin of one chunk grown in the direction of motion by speed x (2 x build time + 0.1 s), cap `max_lookahead_chunks` 3; the view a zoom step would show in each neighbouring group, with the same margin, always queued behind the current group and nearest-first; built chunks uploaded when no visible chunk waits. Tests: "lookahead_rect ...", "adjacent_keys ...", "prefetch: built chunks of the neighbouring groups are uploaded ... up to the cap"; mutations (cap `>` instead of `>=`, sign of the right margin) fail them.
  1. **Cross-fade on every swap** (`GpuLimits::chunk_fade_ms` 150 interactive, 0 elsewhere; piece record `transparent`, gpu.wgsl, ground and sea pipelines blend): a chunk uploaded while in view dissolves in over the stand-in or the open sea; rebuilds of real chunks do not fade. Test "a chunk that replaces the open sea dissolves in over chunk_fade_ms of the animation clock" (start = open sea, mid strictly between, end = chunk, fade 0 = at once; mutation of the start time fails it). Note in iso.md: from open sea the dissolve is an ease-in (about opacity squared) because both passes blend; over a stand-in it is linear.
  2. **Seams:** no alignment defect found. Test "a stand-in lines up with the chunk it stands in for (a smooth world, no shift)": ramp world, all group pairs; error within one source pixel, average shift at most half a source pixel, which is exactly the tie at the 2x ratio of groups 0 and 1 (the destination centre lies on a source pixel boundary and the rule picks one side: a quarter of a destination pixel). Mutation of the sampling rule (+2 instead of +0) fails it. The cut-offs seen while panning are open sea (items 1 and 5).
  3. Rivers on a stand-in and 4. wave window: **not done**, as the board made them conditional: the owner reports rivers "perfect", the wave window costs 14-48 ms per step and was not reported, and the pop metric has no jump left that is not a chunk swap. OQ-091 / OQ-076 stay open.
- Gates (sequential): msvc build warning-free; ctest -j 8 with `ANNO1503_GAME_DIR`: 3 of the render corpus tests threw `bad allocation` (the T10 problem again: `scene renders match the recorded digests`, `the noise coordinate follows the geometry on real rivers`, `river pixels on the GPU match the CPU path ...`), all pass alone with -j 1 (log in the session scratchpad, `t1d_game.out`, `t1d_rerun.out`); without the game dir 611/611; clang-format 23.1.3 clean; GCC 13 not run (render/app only); asan not run.
- Evidence (docs/perf/README.md, "T1d"; table of 24 rows, three runs per cell, load 0-29 %, mostly 4-15 %): no pan: open sea 0 frames (T1c 0-15), pop worst 10-18 % -> 0.5 % (1280x800) / 1.4 % (2560x1440); pan 4,2: open sea 0 frames in all 36 runs (T1c 9-48 frames median, runs up to 325 ms), pop worst 12-31 % -> 1.3-2.7 %, pairs above 0.5 % 51-89 -> 9-18; window-thread max 4.3-5.9 ms in every median cell (acceptance 8 ms), p99 at most 3.8 ms, p95 +0.1-0.2 ms. `Single_24.sze` (largest scene, earlier run, 1280x800, pan 4,2): open sea 63/82/87 frames -> 7/0/21, max <= 6.6 ms. **Stated threshold:** at most 3 % of pixels jump between two frames of one group with a 1600 px/s pan, 1.5 % without pan. Four single-frame outliers of 130-667 ms in no-pan runs (two per configuration, with the `--pop` read-back) are in the "worst max" column; not caused by T1d.
- Not explained / not reproduced: the 4.2 ms event-handling frame (`SDL_SetWindowTitle` per zoom step costs 0.03 ms, worst 0.09; a wheel event does no per-event work that grows with anything; `group_ready` is a map lookup) and the `queue submit` spikes of 3.9-5.0 ms (GPU/driver work behind the submit; none above 6 ms here).
- Left open / questions for the owner: a pan of about 5000 px/s (12,6 px per frame) still shows open sea (hundreds of frames): chunk builds of 100-250 ms cannot keep up with a view crossing 1280 px in a quarter of a second; faster chunk building is the remaining lever (a new task, not a margin). Memory: the margin and the neighbouring groups' views stay in the CPU cache (4 MB per chunk) and up to 96 prefetched chunks on the GPU (about 384 MB), both bounded by config values with reasons (`max_lookahead_chunks`, `prefetch_chunks`); the owner should confirm the 150 ms dissolve (`chunk_fade_ms`) feels right. T10b is still needed for a green `-j 8` ctest with the game dir.
- Owner check (one line): `cd C:\Users\altin\anno1503-T1d\build\msvc\src\app\Release; $g = "C:\Users\altin\anno1503\game\app"; .\anno1503.exe --view "$g\Scenes\Islands\Dschungel\D_256x256_0045_5B.scp" --game-dir $g --water enhanced` — pan with the mouse while zooming with the wheel. What to report: which of the four artifact kinds remain (blank/flashing patches; hard cut-offs or seams; blurry turning sharp; shimmering water or rivers); if it still pops at your normal pan speed, say how fast you move.
- Time spent / subagents used: inline, no subagents.

## T1e — Pan while zooming: the T1d review findings — 2026-10-20 — status: FINISHED (OWNER-CHECK)
- Branch: task/T1d-zoom-artifacts at fa1a6ba (12 commits on b0588c5: one per item 1-9, two for the reviewers' findings, one docs); worktree: C:\Users\altin\anno1503-T1d. Not pushed, not merged.
- Built, per item:
  1. `FrameRenderer::margin_keys()`; the compositor prefetches the margin of the group shown before the neighbouring groups' views (same cap and per-frame limit). A fade starts only over a placeholder an earlier frame drew (`it != end && !real`). **Defect found and fixed on the way:** with the placeholder replaced, nothing drew the open sea under a dissolve, so a fade from open sea started from the clear colour, not the sea pattern. The placeholder is now kept for the fade (`fade_under`) and drawn under the chunk. bench: `--zoom 0 --zoom-pan` is a plain pan; it counts the dissolves after the first second.
  2. Fade clock: `GpuFrame::set_fade_clock`, a steady clock by default. The test holds the animation clock still.
  3. Option 2 of the board: opaque ground and sea pipelines again, with blending variants only for the fading draws (they come last, so one switch per pass). No cost measurement: the cost is the pre-T1d one by construction.
  4. `FrameRenderer::shown_over_stand_in`, set by GpuFrame before update_chunks: the visible chunks still drawn over a stand-in (not uploaded yet, or fading). Their sources are retained. The path without workers passes the retain to its trim as well.
  5. Tests: "a chunk dissolves in over its stand-in…" (spare_chunks 0; the stand-in equals the CPU reference; within 2 levels at 1/4 = smoothstep 0.15625 and at 1/2; exact at the end); lookahead_rect at 1001 px/s, left+down, and the height cap; "the margin follows the view's velocity through update_chunks…" (an injected clock via `FrameRenderer::clock`; leading side, shrink after a stop, speed kept across a zoom step).
  6. Velocity reset when the centre moves by more than the view's size on an axis (test: a wide view, each axis).
  7. `open_sea_chunks` per uncovered area, `GpuStats::open_sea_px` (clipped to the view; includes the holes under a dissolving chunk).
  8. Counted and measured numbers in the `max_lookahead_chunks` comment; `Chunk::bytes`, `FrameRenderer::cached_bytes`; `--lookahead-chunks` in the viewer (default 3, at most 16, cli test) and in bench_frame.
  9. Nits: the pan-period comments, a CAS loop for `build_ms_`, the adjacent_keys doc, `replace_chunk` (prefetch pools the old planes). Deviation: `resident` is not a maintained counter. It is recounted over the margin and neighbour lists (their length, not every GPU chunk), only in frames that may prefetch. The prefetch_chunks comment now says that GPU chunk memory is bounded by the CPU cache.
- Gates: msvc build warning-free; ctest without the game dir 616/616; with the game dir and 16 threads, 614/616: `scene renders match the recorded digests` and `rendered river pixels are the flow shading of the baked lattice…` threw `bad allocation` (T10), and both pass alone with -j 1. clang-format 23.1.3 clean over src; GCC 13 not run (render/app only); asan not run; agree.py n.a.
- Evidence (docs/perf/README.md, "T1e"):
  - **Plain pan**, 0 dissolves after the first second and 0 open sea:
    - canyon 1280x800 at 1600 px/s;
    - canyon 2560x1440, groups 0 and 2, at 1600 and 3200 px/s;
    - Single_24 2560x1440, groups 0 and 2, at all three speeds.
  - **Plain pan, only exception:** the canyon at 5000 px/s, group 0 (53 open-sea frames, 151 dissolve frames). This is build latency: nothing was built on the CPU.
  - **Frame time:** max ≤ 6.5 ms everywhere. The prefetch uploads cost p95 at 1280x800 (0.4 → 1.7 ms).
  - **Pan while zooming, open sea by area** (1280x800 canyon): 74-201 of 1600 frames show some open sea, up to 18 % of the view in one frame and up to one zoom interval long. T1d's "0 frames" was chunk-wise only.
    - Not a T1e regression: a temporary T1d-order build gave 210-212 frames / 12-15 M px against 130-240 / 4-8 M px with T1e.
    - All of it is unbuilt sources (T1f/T11).
  - **Single_24, 2560x1440, zoom every 60 frames:** up to 27 % of the view.
  - **CPU cache on Single_24:** peak 0.44-0.71 GB. `--lookahead-chunks` 2 vs 3 makes no measurable difference.
  - **Mutation checks** (all failed as they should):
    - stand-in retain off;
    - retain without the fading keys;
    - jump threshold ×100;
    - in one build: margin/neighbour order swapped, a linear fade, the placeholder under the dissolve not drawn, and the covered parts not clipped to the view.
- Reviewers (two, read-only, no builds):
  - **A (code):** all items done. Found:
    - stale docs: fixed in iso.md and the README;
    - a possible starvation of the margin by the shared cap at 2560x1440: not seen in 12 bench runs, see above;
    - the dissolving chunk's holes not counted: fixed;
    - bench `--lookahead-chunks` unbounded (int overflow in lookahead_rect): bounded to 16;
    - the cap no longer bounding residency: documented;
    - the counter deviation: stated above.
  - **B (tests):**
    - margin-before-neighbours order untested: the cap now equals the margin and neighbours are built, so a swap fails;
    - the curve was checked only at 1/2: 1/4 added;
    - no rebuild-without-fade test: added;
    - the clip to the view not exercised: the test now picks the part past the edge and adds stand-ins off;
    - the jump test was square: now a wide view;
    - CLI bounds: added.
    - Its claim that the open-sea dissolve test cannot see `fade_under` is contradicted by the run before the fix (start check 0 of 63,667 pixels) and by the mutation.
    - Not done: a real-clock test for the default fade clock (would be time-based); a sea-field (cover > 0) dissolve case.
- Left open / questions:
  - (a) The open sea by area during pan-while-zoom is the next lever after T1e. It is not fixable by the margin and needs faster chunk builds (T1f items 2-3, T11). The orchestrator may want an acceptance number per area for T1f/T11.
  - (b) The owner should confirm the 150 ms dissolve.
  - (c) The wave window rebuilds per zoom step for about 85 ms (bench transients). This is pre-existing (OQ-076).
  - (d) T10b is still needed for a green `-j 16` with the game dir.
- Also done this session: **T11 stage 0** (design) by a subagent in worktree `C:\Users\altin\anno1503-T11`, branch `task/T11-gpu-chunks` at f3546f7 (`docs/render/gpu-chunks.md`, report appended on that branch). Open questions C1-C5 are in the doc; C1 (compute pass instead of a blending render pass, for exact integer compositing) needs the orchestrator. T1f's profile was not started: it needs builds and benchmarks, and T1e owned the machine.
- Owner check: `cd C:\Users\altin\anno1503-T1d\build\msvc\src\app\Release; $g = "C:\Users\altin\anno1503\game\app"; .\anno1503.exe --view "$g\Scenes\Islands\Dschungel\D_256x256_0045_5B.scp" --game-dir $g --water enhanced`. Please check:
  - plain scrolling as smooth as on `main`, with no fading at the screen edges;
  - pan while zooming: which of the four artifact kinds remain;
  - pause (Space) while chunks arrive: they still appear.
- Time spent / subagents used: inline for T1e; two read-only reviewer subagents; one subagent for T11 stage 0.
- **Owner check result (2026-10-21):** "the blue squares still appear, overall it improved a bit." The blue squares are the open sea of unbuilt chunks (finding (a)): expected after T1e, the lever is T1f/T11. The owner did not comment on the 150 ms dissolve or on pause. The viewer log shows no steady-state hitch: 500 fps (the 500 Hz cap), with a busy time of at most 3.7 ms. There were only one-off frames: the first frame's sea at 28.9 ms (start-up), a 93 ms gap of window events with a 9 ms draw prologue (the window was resized to 2560x1369), and a few 8-10 ms gaps that were spent waiting for the display.
  - Follow-up from the owner: "fade feels fine, chunks appear while paused. terrain seems a bit behind in rendering while zooming in and out." The 150 ms dissolve and the pause are confirmed. Terrain lagging during a zoom step matches the build latency of the new group's chunks: stand-ins show until the real chunks arrive, plus the wave-window rebuild of about 85 ms per step (finding (c), OQ-076). T1f/T11 address the first and T3 (continuous zoom) changes the shape of the second. The T1e owner check is complete.

## T12 — CI on `main` red: render warnings-as-errors · `REVIEW` · 2026-10-21

- **Branch:** `task/T12-ci-render-warnings` (2bdd73c, 208932f), pushed. **PR #2** (not merged). **CI run 39552937214 on the PR: all 10 jobs green** (linux-gcc, linux-gcc-release, linux-clang-asan, macos-clang, windows-msvc, cpp-format, build-macos, both Rust test jobs, lint).
- **Built:**
  1. `stand_in_chunk` (`frame.cpp`): the ships line now takes the pointer and guards it (GCC -O3 `-Wnull-dereference`). The plane check (`any`) and the copy loop dereferenced the same lookup unguarded and are guarded too. Comment: the cover's sources are cached by construction.
  2. `gpu_test.cpp:2466`: `size_t * int` now casts both factors.
  3. **Three more errors hidden behind the first one**, in `bench_frame/main.cpp` from T1d/T1e: two `size_t * int` in the pop metric, and two `Row{...}` initialisers that left trailing members out (`-Wmissing-field-initializers` on GCC and Clang). The rows are now filled by member.
- **Test:** new "the CPU stand-in chunk: none without a source, the ships flag from its sources". It checks for no stand-in with an empty cache. With sources whose `ships` is false (the non-default value), it checks the stand-in has `ships == false`, so a dropped copy shows. It is a CPU test that runs in CI without a GPU.
- **Gates (exact commands):**
  - MSVC: `cmake --preset msvc; cmake --build --preset msvc`, 0 warnings.
  - ctest without the game dir: 617/617.
  - ctest with the game dir at `-j 16`: 614/617. Failing: `scene renders match the recorded digests`, `legacy river shading reproduces the previous enhanced digests` and `anno_dump_agrees_with_python`. All three pass alone with `ctest -R`. This is the load/memory problem of T10b (`agree` is new on that list).
  - clang-format 23.1.3 on the changed files: clean.
  - GCC: in WSL, `g++-13` with the CI's flags (`-O3 -DNDEBUG -std=c++20 -Wall -Wextra -Wpedantic -Wconversion -Wsign-conversion -Wshadow -Wnon-virtual-dtor -Wold-style-cast -Wcast-align -Wnull-dereference -ffp-contract=off -Werror`), compiling only (`-c -o /dev/null`) **every `.cpp` under `src/`** (106 files). Includes come from `src/*/include`, and the deps (wgpu-native, SDL3, doctest) from `build/msvc/_deps`. It ran with `-DANNO_WITH_VIEWER -DANNO_VIEWER_GPU -DANNO_BENCH_GPU`, because without them the viewer and GPU branches are not compiled. Clean.
  - Clang: the same files and flags with Ubuntu's `clang++-18`. WSL has no clang and no sudo, so the `.deb` packages were unpacked into `~/clang18` (`apt-get download clang-18 libclang-cpp18 libllvm18 libclang-common-18-dev libclang1-18 llvm-18-linker-tools libgc1 libobjc4 libobjc-13-dev` + `dpkg -x`). Clean.
  - The script is `gcc_check.sh` in the session scratchpad. Worth turning into `tools/check_unix_warnings.sh` for §1 point 9 if the orchestrator wants it.
- **Found on the way, not changed:** compiled *without* `ANNO_VIEWER_GPU`, Clang reports three unused private fields in `viewer.cpp` (`surface_failures_`, `reconfigure_`, `present_error_shown_`, `-Wunused-private-field`). Every CI configuration builds the viewer with wgpu, so CI does not hit it. A viewer build without wgpu on Clang would fail. A small `[[maybe_unused]]` or `#ifdef` fix is possible if wanted.
- **Housekeeping:** the T1d worktree was removed (`git worktree remove`). The branch is kept until CI on `main` is green.
- **Reviewers:** none spawned. The change is mechanical, about 40 lines, and CI is the acceptance.