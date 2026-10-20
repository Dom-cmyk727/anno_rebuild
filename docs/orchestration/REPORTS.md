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
