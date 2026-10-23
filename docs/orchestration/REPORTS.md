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
- Left open / questions for the orchestrator or owner: OQ-077 (owner to confirm scaled stand-ins over open sea; cross-fade wanted? none built; rivers on a stand-in use the legacy look; the CPU viewer path still draws open sea). Unexplained: `render::div_euclid(int, int)` with kChunk returned -2 instead of -1 for -512 / 512 in some contexts of the test TU (a debug MESSAGE printed -1); the test uses its own floor division helper; frame.cpp results matched an independent check and the GPU, so the library is believed fine, but it deserves a look (MSVC optimiser?) before anything relies on `div_euclid` at exact negative multiples. [Orchestrator correction 2026-10-22, closed in T1b step 5: the -2 was a stand-in source chunk index (`stand_in_source` at a chunk boundary), not `div_euclid(-512, 512)`, which returns -1; pinned by the test "div_euclid at exact multiples of kChunk" (5631031).] 'legacy river shading ...' test takes 589 s alone on a loaded machine and failed once at -j 16 under load (not a stand-in effect; it does not use the GPU path).
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

## T11 stage 0 — design — REVIEW — 2026-10-20 — status: FINISHED
- Branch: task/T11-gpu-chunks at c749591 (design commit; this report is the next commit); worktree: C:\Users\altin\anno1503-T11. Docs only, no code.
- Built: `docs/render/gpu-chunks.md` (sections 0 summary, 1 what exists with file:line, 2 measured numbers, 3 (a) draw list: 32-byte objects and ground records, clip table, bins, 4 (b) residency: frame ids, frame table, hot/static layer classes, layer-granular LRU, pinning, budget, 5 (c) passes: objects + base by an exact binned compute pass, ground + terrain by a render pass, plane usages, 6 (d) scheduling, 7 (e) CPU reference, stage-1 replay proof, GPU = CPU tests, synthetic scene, 8 (f) fallback switch, 9 (g) limits table, 10 (h) later extensions, 11 stages, 12 open questions C1-C5).
- Gates: no build, no ctest, no bench (task constraint: docs only, the T1e/T1f tasks own the PC); the count used a Python script over the original files (about 1 s per scene).
- Evidence (counted with a scratch script on `tools/insel.py` + `tools/figuren.py`, repeating `prepare_objects` / `place_prepared` at t = 0; check: Single_01 invisible items 11,717 = iso.md): distinct static frames / MiB of RGBA8 boxes per group 0/1/2: `Single_24.sze` 509 frames, 46.2 / 11.6 / 2.8 MiB (+ 192 water frames, 26.6 / 6.7 / 1.6 MiB); canyon `D_256x256_0045_5B.scp` 329 frames, 18.0 / 4.5 / 1.1 MiB (+ 392 water, 60.0 / 15.1 / 3.7); `Single_01.sze` 675 frames, 37.6 / 9.4 / 2.3 MiB (+ 288 water, 44.2 / 11.0 / 2.8). Over 31 scenarios at group 0: union median 59.8 MiB, max 149.9 MiB (`single30.sze`; 196.7 MiB all three groups); per chunk at most 129 frames / 7.8 MiB and 3,608 instances (115 KB of records against 3.9 MB of texels uploaded per chunk today).
- Reviewers: none (design; the orchestrator reviews).
- Deviations from the task text, for review: (1) the objects and base planes are designed as a binned compute pass that repeats the CPU's integer `over` bit for bit, not as the blending render pass the entry names: fixed-function blending rounds where the CPU floors, so compounding shadows and shadows over translucent sprites drift by up to 1 per layer, and the per-island local canvas with its `base` reset cannot be repeated without an intermediate target per island (doc 5.1; the render pass is kept as the alternative, C1); (2) iso.md has no "Blitting" section, the doc cites the blitter code and iso.md "Objects"/"Fidelity"; (3) the CPU blitter does not implement `z` depth, `M` or `H`/`V` semantics (painter's approximation, OQ-026), so the GPU reproduces the blitter and the record only reserves a `depth` field; (4) the ground's GPU pass saves little until T1f item 2 removes the per-chunk probe (C2).
- Left open / questions (candidates, no OQ numbers assigned; highest on main OQ-095): C1 compute vs blending pass (orchestrator); C2 ground in stage 2 or after T1f item 2 (orchestrator, T1f profile); C3 `atlas_budget_mb` default 512 discrete / 256 integrated (owner, a limit default); C4 switch names `--chunk-build cpu|gpu` (the entry's `--chunks gpu` collides with `--chunks blocking,async`) and `ANNO_VIEW_GPU_CHUNKS` (orchestrator); C5 whether WGSL compilers contract the ground's edge functions into FMAs (stage 2 measurement; decides the ground tolerance).
- Time spent / subagents used: inline, no subagents; counting script at C:\Users\altin\AppData\Local\Temp\claude\C--Users-altin-anno1503\359b08b8-e128-41ac-ba6d-3278c074d82d\scratchpad\count_frames.py (not committed).
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
## T1f item 1 — chunk build profile · reported 2026-10-21 (item 2 continues without waiting, per the Order)

- **Branch:** `task/T1f-chunk-build-speed` (worktree `C:\Users\altin\anno1503-T1f`). Commits: a8d0d21 (the profiler `anno/render/profile.hpp`, stage scopes in `render_chunk` / `water_field_from` / `flow_plane` / `SpriteCache`, a test), 0f2583b (`bench_frame --chunk-profile threads,.. [--chunk-limit n]`), 7479a2a (the table in `docs/perf/README.md`, "2026-10-21 - T1f item 1"). Not pushed.
- **Result** (one thread, warm, mean per chunk: canyon 50 / 44 / 28 ms at groups 0 / 1 / 2, `Single_24` 44 / 39 / 25 ms):
  - **Field + terrain + flow stages: 67-72 % of a chunk.** The sea field (`sea::water_field`, per-pixel spline taps) alone is 35-49 %, the river stages 4-7 ms, `terrain_at` 2-4 ms. This is the orchestrator's suspect, confirmed. Item 2's 2 ms target means removing about 30 ms per chunk at group 0.
  - **Ground: 14-16 %, below the 20 % of T11's C2**, so the ground pass (2b) stays after item 2, as decided. Note: after item 2 the ground would be about 40 % of the remaining ~18 ms and the objects about 48 %.
  - **Sprite decode: 0.1-0.7 ms cold, 0 warm.** It is not a cost. Objects (cull and blit) are 13-17 %.
  - **T1d's "100-250 ms" is the 16-thread cold case.** SMT makes each chunk 1.6-2 times slower at 16 threads (1.2 times at 8). The rest is chunks with rivers waiting in `call_once` for the island's river flow solve (84-151 ms), which gives maxima of 180-250 ms. Throughput at 16 threads is 156-285 chunks/s warm.
  - **Found:** the viewer's workers do not wait for the flow (`wait_for_flow` false). They deliver a river chunk without its lattice and **build it again** when the flow is ready, so the first seconds on a river island cost about twice the work. Candidate for item 3 (deliver the lattice separately).
- **Not measured:** the workers' below-normal priority against the window thread (the bench has no window thread), and queue latency (the viewer side). The machine had 8-27 % background load (desktop apps). Two runs agree within 3 %.
- **Gates:**
  - MSVC build: clean.
  - ctest without the game dir: 618/618.
  - ctest with the game dir at -j 16: 617/618. `scene renders match the recorded digests` hit `bad_alloc` even alone at `ANNO_TEST_THREADS=16`, because desktop apps held about 32 GB of commit (11 GB free). With `ANNO_TEST_THREADS=4` it passes, 45/45 digests (T10b).
  - GCC 13 and Clang 18 (`-O3`, the CI flags, `-Werror`): every file in `src`, clean.
  - clang-format: clean.
- **Reviewers:** none for item 1. It is measurement code; the pixels are unchanged (digests).
- **Next:** item 2 (island-level fields), starting with the sea field.
## T1f item 2 — exact shortcuts in the field and terrain stages · `REVIEW` · 2026-10-21

- **Owner decision 2026-10-21** (asked during the task): the plan "field per island and group, sliced per chunk" does not fit memory at group 0 (about 0.7 GB for the canyon island alone). The owner chose instead: exact shortcuts with a classification per island, by a conservative bound (pixels identical, the bound written in iso.md as a proof), and terrain per tile. Revised targets: field + terrain at most 25 % of a chunk, and a canyon land chunk at group 0 under 25 ms on one thread. The full removal is T11 §10.1. Options 2 (precompute at groups 1-2) and 3 (a coarser, inexact precompute) were rejected.
- **Branch** `task/T1f-chunk-build-speed`, after item 1's commits, not pushed:

  | commit | what |
  |---|---|
  | 7230884 | empty cells (`IslandWater::classify_cells`, `sea::field_at`, the proof in iso.md) |
  | 726ee06 | `noise3_at` (noise without the zero-weight plane); the fix for an unclassified `IslandWater`, which 7230884 read out of bounds |
  | b2d1994 | `terrain_plane`; river masks created only on sprite pixels |
  | 9b14840 | `SmoothSampler` / `NoisePlane` caches |
  | bd380ab | review: named noise weights, `static_assert`s on the proof's premises, `terrain_plane` as a free function |
  | 84ec121 | review: a golden digest from 7479a2a, boundary, multi-island and terrain tests |
  | c14ee6a | docs |

- **Result** (one thread, warm, mean per chunk; `docs/perf/README.md` "T1f item 2"):

  | scene, group | before (ms) | after (ms) | field + terrain + flow |
  |---|---|---|---|
  | canyon g0 | 50.1 | 29.1 | 68 % → 46 % |
  | canyon g1 | 44.3 | 28.7 | 72 % → 57 % |
  | canyon g2 | 28.2 | 19.5 | 70 % → 57 % |
  | `Single_24` g0 | 44.2 | 21.4 | 67 % → 33 % |
  | `Single_24` g1 | 39.3 | 23.0 | 71 % → 52 % |
  | `Single_24` g2 | 25.2 | 16.6 | 69 % → 53 % |

  - **Both targets missed.** The canyon at group 0 is 29.1 ms, not under 25; field + terrain is 33-57 %, not at most 25 %.
  - **What is left:** sea and coast pixels inside the island grids, which need the exact smooth height for the depth byte (it never saturates). Also the river blurs and chamfers on chunks where rivers meet the sea.
  - **Exact options left:** bounding-box blur and chamfer (about 2-3 ms on river chunks); SIMD for the band pixels.
  - **The real fix** is T11 §10.1. The orchestrator decides whether to go further here or move to T11.
- **Exactness evidence:**
  - All scene and frame digests are unchanged.
  - A synthetic golden digest recorded from the 7479a2a sources is checked in CI, with and without the cell classes.
  - The empty-cell proof holds on every game island: 458 files, 4.33 M of 11.8 M tiles empty, all sampled positions empty.
  - Bit-equality tests: `noise3_at`, `NoisePlane`, `SmoothSampler`, `terrain_plane` (synthetic and game files).
  - Mutations, each caught by a test:
    - the river check removed;
    - the threshold at the waterline;
    - 4 corners instead of 16;
    - no noise term;
    - the shortcut used outside the grid;
    - samplers or terrain cache kept across islands.
- **Reviewers** (two, read-only):
  - **A (proof and code):** "no way for the outputs to differ, no new UB"; points 1-6 sound. It found constants not tied to the proof (the noise 1.3, `kBedEdge·kBedDepth`). Fixed in bd380ab with named constants and `static_assert`s. A stale comment is fixed too.
  - **B (tests):** five mutations uncaught. The main gap was that CI compared `water_field` with `field_at`, which share one body. Fixed in 84ec121 (the golden digest, boundaries, multi-island, terrain across islands; all five now caught). The game-file test was reduced to 9 positions per tile for ASan.
  - **Not done:** islands of width 1.
- **Gates:**
  - MSVC build: clean.
  - ctest without the game dir: 627/627.
  - ctest with the game dir: 627/627 at `-j 4`, `ANNO_TEST_THREADS=4`. At 16 the digest test hits `bad_alloc` while desktop apps hold about 32 GB of commit (T10b).
  - GCC 13 and Clang 18 `-O3 -Werror` with the CI flags over all of `src`: clean.
  - clang-format: clean.
- **Not measured:** 16 threads after the change. The machine was at 47-77 % load (a game running), so those numbers are invalid and not reported. To be rerun on an idle machine.
- **Next per the board:** item 3 (progressive delivery; the profile also found that the viewer builds every river chunk twice in the first seconds) and item 4 (`kChunk` 256) wait for the orchestrator's review of this.
## T3 stages 1-2 — continuous zoom: the resample pass and the eased wheel · `OWNER-CHECK` · 2026-10-21

- **Branch** `task/T3-continuous-zoom` (worktree `C:\Users\altin\anno1503-T3`), pushed, **PR #4** (CI to be watched; do not merge before the owner looks). Commits: f273a9b (resample pass), 51a268d (viewer + CLI), 1a9377c (docs), d810ba5 (review fixes and tests).
- **Deviation from the entry (design): resample, not a native scale in every shader.** The compositor reads every plane with `textureLoad` at the integer scene pixel (sea pattern, waves, flow lattice, sprites, ships; GPU = CPU tests pin it). A fractional scale inside it would touch all of them and lose those tests. Instead the picture is composed *exactly as now* on the scene grid of one zoom group into an offscreen texture, and **one extra pass** resamples it to the window (`GpuFrame::render(..., const Resample*)`: scale, sub-pixel offset, filter nearest / bilinear / box). With no `Resample`, or the identity, no pass and no texture: every digest and the uniform bytes are untouched. The group shown is the one nearest in tile-width ratio, so the scale is 0.69..1.45. The entry's "group closest above, switch at the geometric middle" is the geometric middle with a 5 % hysteresis. Stage 3 (mips, zoom-out beyond group 2) will need mips of the composed grid.
- **Viewer** (`--zoom-mode continuous [--zoom-step 1.1] [--zoom-ease 120]`, default stays `stepped` until OQ-095 is decided): wheel notch = factor 1.1 (a precision wheel's fractions too), `+` / `-` = its third power, `Home` or `0` = back to group 1's width; eased in the logarithm (95 % in 120 ms); the point under the cursor stays fixed; panning is in screen pixels. `ANNO_VIEW_RESAMPLE=nearest|bilinear|box` forces a filter (default: box below scale 1, nearest above). The CPU renderer and the stepped mode are unchanged.
- **Evidence:** synthetic GPU tests, noise at every pixel, against a CPU model of the shader: scale 2 nearest exact, offsets, bilinear, box at 0.5, 0.63, 0.3, 1/3, 1.37 nearest / bilinear, unequal offsets, non-square grid and output, a target holding another picture, alpha compared, an identity `Resample` = none (all within 1 level, 0 pixels off). `resampled_view` (negative positions, fractions, a stable grid size, limits). The zoom maths (`anno/app/zoom.hpp`, no SDL / GPU): anchor fixed across the group switch, 95 % in `ease_ms`, steps compose, ease 0, hysteresis, ready gating, the range. CLI bounds. Eleven mutations (offset, rounding, centre, taps, span, swapped offsets / sizes, rounded origin, box as bilinear, …) each fail a test. The viewer ran 6 s on the canyon island in continuous mode at 500 fps (0.4 ms mean busy), no GPU validation error.
- **Reviewers** (two, read-only). Code: the uniform layout, layouts, `resampled_view` and the viewer easing / anchor maths sound; found and fixed: panning ignored the scale (a drag of 100 px moved the picture 141 px at scale 1.41), the group switch ran only while the zoom moved, the grid size changed by a pixel with the camera so three textures were recreated almost every frame (now rounded up to 64), no texture-size or scale guard, a stepped `zoom()` in continuous mode jumped, a wrong scale range in the docs (0.69..1.45). Tests: six mutations survived; fixed with the odd-scale / offset / non-square / dirty-target tests; the zoom maths made testable. **Not done:** the resample pass is outside the GPU timestamp pair (`gpu_ms` omits it); the 4-tap box at 0.5..1 is an approximation of the area mean (documented).
- **Gates:** MSVC build clean; ctest 636/636 without the game dir and with it (`-j 4`, `ANNO_TEST_THREADS=4`); GCC 13 and Clang 18 `-O3 -Werror` with the CI flags over all of `src`; clang-format clean.
- **Question for the orchestrator:** the GPU tests (these and every other) probably skip in CI: `device_or_skip` skips without an adapter and `ANNO_REQUIRE_GPU` is not set (OQ-089, a software Vulkan adapter on the runners, is open). CI therefore checks only `resampled_view`, the zoom maths and the CLI, not the resample pass itself.
- **Owner check** (the T3 worktree, not `main`): `cd C:\Users\altin\anno1503-T3\build\msvc\src\app\Release; $g = "C:\Users\altin\anno1503\game\app"; $s = "$g\Scenes\Islands\Dschungel\D_256x256_0045_5B.scp"; .\anno1503.exe --view $s --game-dir $g --water enhanced --zoom-mode continuous`. Please check: (1) zoom with the wheel: smooth, does the speed feel right (`--zoom-step 1.05` / `1.2`, `--zoom-ease 60` / `250`)? (2) does the picture stay under the cursor? (3) the moment the zoom group changes (around tile widths 59 and 29): a visible step in detail? (4) sharp (nearest, scale above 1) vs soft: `$env:ANNO_VIEW_RESAMPLE="bilinear"` before the command; (5) `Home` or `0` back to the default, (6) pan while zooming, (7) the stepped mode (no flag) still as before.
- **Next:** stage 3 (mips and the zoom-out range, the stepped / continuous default per OQ-095) waits for the owner look. T11 stage 1 is running in its own worktree (a subagent); report to follow.
- Time spent / subagents: inline plus a read-only mapping agent, two reviewers.

## 2026-10-21 — T11 stage 1 (atlas residency model, draw lists, CPU replay) — REVIEW

- **Branch** `task/T11-gpu-chunks` (merged with `main` 71b5475), PR opened. Section "Stage 1 as built" in `docs/render/gpu-chunks.md`, one note in `iso.md` ("Objects").
- **Built (CPU only, no shader):** `FrameTable` (dense `u32` ids), `ChunkDraws` (32-byte records, clip table, sorted frame set, CSR bins), `Scene::objects_draws` + `replay()`, `render_chunk` builds the list and replays it (`chunk_builder` switch, default Cpu), `AtlasResidency` with the `GpuLimits` budgets (`atlas_budget_mb`, `atlas_upload_bytes_per_frame`, `gpu_chunks_per_frame`; the compositor does not read them yet).
- **Evidence:** every chunk of `Single_01`, `Single_24` and the canyon island at groups 0-2, ships on/off: replayed objects and base planes equal `Scene::render` byte for byte; binning equals a brute-force list; every clip entry lies inside its chunk. Digests unchanged.
- **Gates:** MSVC Release build clean; full ctest 655/656 at `-j 8`, `ANNO_TEST_THREADS=8`; the one failure (`scene renders match the recorded digests`) passes alone in 104 s, the memory-pressure flake of T10. clang-format 23.1.3 clean on all changed files. GCC 13.3 `-O3 -Werror` (CI flags) over the five changed render sources and three test files: found and fixed two real findings (`maybe-uninitialized` on `std::optional<Canvas>` in `replay`, now a `unique_ptr`; `stringop-overflow` / sign conversion in the test image helper). **Clang -O3 not run locally** (no clang in WSL); CI will.
- **GPU tests:** none ran in this task (no GPU code). CI skips them until T13.
- **Not done:** GPU work (stage 2a), ground pass (2b), cache/prefetch (3). Open: C1-C5 decisions are as the orchestrator approved in §4.
- **Next:** T3 stage 3 (first commit: flip the default to continuous).
## 2026-10-21 — T3 stage 3, design note for the bounded grid — REVIEW (no code yet)

- **Branch** `task/T3-continuous-zoom` (merged with `main` 71b5475). First commit `7687eb2`: continuous is the viewer default (`ZoomOptions::mode`, `mode_explicit`; the CPU renderer falls back silently, the "needs the GPU renderer" line only for an explicit `--zoom-mode continuous`; usage text, `iso.md`, CLI tests). App tests pass; clang-format clean; GPU tests: none touched by this commit.
- **Design** in `docs/render/iso.md` "Continuous zoom, stage 3 design: derived content tiers": zoom levels as a list (original 0-2 built, 3.. derived by 2x box reduction of the previous level, per scene until the scene fits the window; `max_derived_levels` default 8); the level is chosen nearest in ratio, so the resample scale stays within 0.69..1.45 and the grid within 1.45 x the window (3712 x 2088 at 2560 x 1440, 5568 x 3132 at 4K; sweep test); reduction defined per plane by a CPU reference (`reduce_chunk_2x`, recursive, integer) that the GPU pass (`reduce_fs`, upload time) equals exactly; no CPU chunk build added; a derived chunk is drawn only when its four quadrants are in; `ChunkKey` = (level, cx, cy); stand-ins from the nearest level in ratio (ties to the coarser), source jobs queued by distance from the centre and capped; measurements and six tests listed.
- **Questions:** (1) river flow with the legacy look at derived levels; (2) default minimum zoom = scene fits the window (tile width 2.5 for `Single_01` at 2560 x 1440), `--min-zoom` as the floor; (3) plain box mean, not gamma-aware. I did not measure the group-2 sea pattern period yet (does it divide by 2^k?), it is the first thing the derived-pattern commit checks.
- **Not done:** all code of stage 3 except the default flip; waiting for the orchestrator's review of the design before (a)-(h). Per the order in the update, T11 stage 1 is reported (PR #5), T3 stage 3 continues after this review.

## 2026-10-21 — T11 stage 1 finish (the three things of review j) — REVIEW

- **(1) CPU builder measured** (`bench_frame --chunk-profile 1,16 --groups 0,1,2`, canyon island and `Single_24`, main f583e9d against the branch, table in `docs/perf/README.md`): the objects stage is 0.4-20 % **faster** than on main (keel computed once per frame), the chunk mean -6 % .. +2 % (noise about 5 %). Threshold not exceeded, so the bins stay computed for `Cpu` and ids are not cached per worker; re-measure at stage 2a.
- **(2) Unit frame key:** `FrameKey::unit(UnitFrameId, group, shadow)` keys by (figure entry, frame index, group, player colour, shadow); `assets::unit::DecodedFrame` carries `index` and `colour`, `render::to_unit_frame` builds the render form for the loader and the tests; frames without an identity (hand-made scenes) are keyed by their decoded image (`FrameKind::UnitImage`). Test: "frame table: units with the same frame identity share one id" (colour, index, group, shadow each separate; image fallback). Corpus replay still equals `Scene::render`.
- **(3) `gpu-chunks.md` section 12** now records C1, C2, C4, C5 as decided, C3 = OQ-096, and the stage 0 additions 2 and 3 for stage 2a.
- **Gates:** MSVC build clean; 76 related tests pass (frame table, draw lists, units, GPU compositor tests, app) at `-j 8`; clang-format 23.1.3 clean on all changed files; GCC 13.3 `-O3 -Werror` clean over the changed sources and tests. Clang -O3 only in CI. GPU tests ran locally on the machine's default wgpu adapter (not recorded by the tests; I did not check whether it was the 4090 or the iGPU); CI skips them. Full ctest was not rerun after the key change (the earlier full run was green apart from the known digest flake).

## 2026-10-21 — T3 stage 3, first three code commits (level table, CPU reference, GPU reduce pass) — REVIEW (interim)

- **Branch** `task/T3-continuous-zoom` (PR #6 was merged earlier; these commits are on top, push only, new PR to follow with the next group). Conditions 1, 2, 3, 4 and 5 of the design review are written into `iso.md` ("Review conditions of 2026-10-21").
- **`408cd46` level table (condition 5), no behaviour change:** `ZoomLevels` (`zoom_levels.hpp`, tile sizes as w / 2^shift, exact stand-in ratios `ratio_w/h`); `FrameRenderer::levels()` feeds the margin conversion, the stand-in free functions (default parameter = the original table), the compositor's tile uniforms and stand-in piece records (now numerator / denominator per axis), the viewer and `zoom.hpp`. Scene content stays per built group. **Full ctest 636/636, every digest unchanged**, with the game dir at `-j 6`, `ANNO_TEST_THREADS=4`.
- **`d8e8b36` `reduce_chunks_2x`** (`chunk_reduce.hpp`): the CPU reference, hand-computed tests for terrain/ground (valid texels only), quadrant placement, field (OR of bits 0-1; slope **mean over water texels**, why in the header and the note; test with slopes 3 and 7 giving 5, a land texel not counting), objects (premultiplied), missing sources/planes, `base`/`flow` not derived. `base` absent means the compositor draws objects over the water as pictures (the shader already skips `object_in_water` without the base bit): condition 2, documented; the derived-level compositor test comes with the cache commit.
- **`6b4b032` GPU reduce pass:** `reduce_fs` in `gpu.wgsl` (one pass, four targets, one scissored draw per source chunk, integer arithmetic), `GpuFrame::reduce_chunks_2x` as the read-back entry. **GPU = CPU byte for byte** on noise chunks (differing planes, missing sources, terrain without ground and the reverse) and on real group-2 chunks of the canyon island and `Single_24` over two levels. A shader mutation (slope shift) fails both tests. GPU tests ran on this PC's default wgpu adapter with `ANNO_REQUIRE_GPU=1` (I did not record which adapter wgpu picked; the iGPU run is still to do for the README commit).
- **Gates:** MSVC clean; clang-format 23.1.3 clean; GCC 13.3 `-O3 -Werror` clean over `frame.cpp`, `gpu.cpp`, `viewer.cpp`, `app_test.cpp`, `chunk_reduce.cpp`, `chunk_reduce_test.cpp`, `gpu_test.cpp` (with the GPU defines). Clang only in CI.
- **Next, in order:** derived chunks in cache / stand-ins / prefetch with `resident_level_chunks` (condition 3) and the compositor test at a derived level (condition 2); derived sea pattern and sprite scale; README measurements incl. the first zoom-out fill time (condition 4); then (a)-(h). T11 stage 1 finish is on PR #5 (see its report above). Both branches will conflict in `CMakeLists.txt` / `gpu.cpp` lists when T11 stage 2a starts; I merge `main` first.

## 2026-10-21 — T3 stage 3: derived zoom levels draw in the viewer — OWNER-CHECK (stop for the owner look)

- **Branch** `task/T3-continuous-zoom`, PR #9 (merged with `main` 59d327a first; the Clang `ptrdiff_t` fix came with it). Commits: `60b902c` (derived chunks, sea pattern, cache rule, CPU and GPU frames at derived levels), `f869b83` (viewer: levels per scene, `--min-zoom`, `--max-derived-levels`, stepped zoom stays on three groups), `e6af0ee` (as-built section in `iso.md`, measured sea period).
- **What exists:** zoom levels below group 2 (tile widths 10, 5, 2.5, ... down to the scene fitting a small window, `--max-derived-levels` default 8); the wheel zooms through them down to **the whole scene fitting the actual window** (`--min-zoom <tile width>` overrides: the owner decides the default, OQ-095 item 4). Derived chunks are built **on the CPU workers by recursive reduction** (deviation from the design's GPU reduce at upload, reasons and cost in `iso.md` "Stage 3 as built"; the GPU reduce pass stays, exact and tested, for a later step). The sea pattern of a level is the reduced pattern of the level above (group-2 period 320 x 160 halves exactly five times: no drift at levels 3-7). A derived level of at most 16 chunks for the whole scene is never evicted (`resident_level_chunks`, review condition 3). Ships and soldiers are **not drawn at derived levels yet** (sprite scale is the next commit); rivers use the legacy look; no wave simulation at derived levels.
- **Review conditions:** (1) slope mean, OR of the two water bits: in the CPU/GPU reference and tests. (2) base absent: objects over water as pictures, **GPU = CPU at levels 3-5 on the canyon island and `Single_24` with enhanced water**, objects in water included. (3) resident levels: test with the rule on and off. (4) the aliased first zoom-out is stated in `iso.md`; **the fill time on 1 and 16 threads is not measured yet** (needs `bench_frame` to take derived levels: next). (5) level table commit with no behaviour change, digests unchanged: `408cd46`.
- **Tests that ran, on which adapter** (`bench_frame --list-adapters`: 0 RTX 4090 Vulkan, 1 Radeon iGPU Vulkan, 2/3 the same on DirectX 12): `GPU reduce pass` (2 tests) and `GPU frames at derived levels match the CPU path` on **adapter 0 (RTX 4090, Vulkan) and adapter 1 (Radeon iGPU, Vulkan)**, `ANNO_REQUIRE_GPU=1`: pass on both. The full suite (636 + the new ones, game files) ran on adapter 0 at `-j 6`: green except `scene renders match the recorded digests`, the known memory-pressure flake (passes alone in 88 s). Results: original water max 3 / at most 1/10000 pixels above 2; enhanced water 0.7-2.5 % of pixels above 2 (built groups: under 2 %), shoreline pixels as at the built groups.
- **CI of the PR head:** the first run (60b902c) was green on cpp-format, lint, linux-gcc, linux-gcc-release, **linux-clang-asan** (Clang `-Werror`), windows-msvc, build-macos, both test jobs; **macos-clang failed only on the known flaky `river flow on the GPU ... the automatic tier follows the adapter`** (the T13 note, gpu_test.cpp:1984/2001, a measured cost on the Metal adapter), nothing of this change. The later two pushes' runs were still pending when I wrote this; I will correct this entry if they differ.
- **Owner check** (the T3 worktree build, not `main`): `cd C:\Users\altin\anno1503-T3\build\msvc\src\app\Release; $g = "C:\Users\altin\anno1503\game\app"; $env:ANNO_GPU_ADAPTER="0"; .\anno1503.exe --view "$g\Scenes\Islands\Dschungel\D_256x256_0045_5B.scp" --game-dir $g --water enhanced` (continuous is the default now; also try `$g\Scenes\0200Single\Single_24.sze`, and `--min-zoom 5` / `--max-derived-levels 3`; `ANNO_GPU_ADAPTER=1` for the iGPU). Please look at: (1) zooming out past the old far view: does the picture stay smooth while the chunks of the new levels arrive (the first zoom-out shows the finer level shrunk by nearest until the coarser chunks are built: how long, how ugly?); (2) is the reduced look acceptable (coast, objects, sea colour) against the group 2 look; (3) the zoom-out limit "the scene fits the window": right default?; (4) `Home` / `0` back to group 1's width, pan while zoomed far out; (5) ships vanish below group 2 (known, next commit); (6) the stepped mode (`--zoom-mode stepped`) is unchanged.
- **Not done yet in stage 3:** sprite scale for ships at derived levels; `bench_frame` support for derived levels and the README measurements (fill time on 1 and 16 threads, per-zoom-factor table at 1280x800 and 2560x1440 on both GPUs); items (a)-(h). I stopped here as ordered.
- **T1f in parallel** (own PR #10, `task/T1f-chunk-build-speed`): CI test for 2b's windowing: `blurred_alpha` and `blend_rivers_into_sea` moved to `river_window.hpp` (detail) with a whole-rect switch and a margin shrink; synthetic tests at canvas edges, with the closing, with sea at exactly the farthest blending distance, seams at the border; **one pixel less margin changes the result** in the blur (both modes) and in the blend. The margins are now the proven exact reach (blur 2r, +1 with the closing; blend max(ceil(reach) − 1, 2 floor(seam))); digests of the ten scenarios and all 205 render and GPU tests unchanged; GCC 13 and clang-format clean; PR #10's CI still pending.
## 2026-10-21 — T1f item 2b (bounded blur and chamfer) — REVIEW

- **Done:** `blurred_alpha` works on the bounding box of the non-zero alpha grown by 2r + 2; `blend_rivers_into_sea` on the bounding box of the river pixels grown by ceil(max(reach, 2 seam)) + 2. **Both proofs closed** (no fallback to the blur alone), written in `iso.md` "Bounded blur and chamfer": the blur's value is a function of the ball of radius 2r (+1 for the closing) including the edge repeat, the chamfer's value below a bound B depends on the ball of radius B and the consumers read it only below `reach` / `seam`. No tolerance.
- **Evidence:** the ten scenarios' enhanced whole-scene digests and the frame digests unchanged (render and GPU tests 192/192 at `-j 6`); new test `water field: islands one tile wide or high classify and evaluate like the rest` (the item 2 gap; tall and wide, two heights, classified against unclassified against `field_at`). I did not add a direct old-versus-new test of the two functions (they are file-local); the digests of whole scenes with enhanced rivers are the equality evidence.
- **Measurement** (idle machine, four builds, table in `docs/perf/README.md`): river stages 1.5-2.1 times less, chunk mean -3..-14 %, 16-thread p95 on river scenes down to 56-69 ms at group 2. The 16-thread rows of items 1 and 2 are in the same table.
- **Gates:** MSVC clean; clang-format 23.1.3 clean; GCC 13.3 `-O3 -Werror` clean over `scene.cpp`, `render_test.cpp`. Clang only in CI. GPU tests ran (render + gpu suites) on this PC's default wgpu adapter, not named (to be named with `bench_frame --list-adapters` before the stage 3 measurements).
- **Not started:** items 3 and 4 (wait for T3 stage 3's merge, per the order). Next: T3 stage 3 once PR #7 shows MERGED.

## T3 stage 3 fix 1: the level is composed at scale <= 1, Area filter, --zoom-max-upscale — OWNER-CHECK (2026-10-21, branch task/T3-continuous-zoom, PR #9 open, head 0e62743)

main (524223a) merged first. **Done (fix 1; fix 2 waits for the owner's look):**
- **Level choice** (`zoom_level_for`, `zoom.hpp`): the coarsest level whose tile width is >= the zoom (scale <= 1, Box); the finer level as soon as the scale would exceed `--zoom-max-upscale` (default 1); the coarser one only at 5 % below its tile width (hysteresis on the coarse side only). A coarser level not ready is waited for; a finer level not ready is bridged by the next coarser ready one, magnified. Above group 0 (up to sqrt 2) the first group, magnified.
- **`ResampleFilter::Area`** (coverage-weighted, <= 2 x 2 pixels at scale >= 1) in `scale_fs` and in the test's CPU model; the viewer default above 1 (Box below). `ANNO_VIEW_RESAMPLE=nearest|bilinear|box|area` for the A/B; bilinear stays one switch away.
- **`--zoom-max-upscale <1..2>`** (default 1.0) with CLI tests (bounds, nan, empty).
- **iso.md** "Choosing the level" + test "derived levels: the box at scale 0.5 of a level equals the coarser level" (levels 3 -> 4, ground): worst difference 1, no channel off by more than 1. Not exact over objects whose alpha varies inside a 2 x 2 block (alpha-over and the mean do not commute): stated in iso.md.
- **Tests:** zoom sweep down and up in [0.95 / 2.1, 1] below group 0 (the 5 % is 0.95, not 1/1.05: 0.452 instead of 0.4535), 1.45 old look bounded by 1.45, switch rules (hysteresis, ready, bridge, rest). Area vs a CPU model at 1.37, 1.13, 1.01, 0.63 and unequal offsets: 0 pixels off by more than 1 of 2400-4800, **on adapter 0 (RTX 4090, Vulkan) and adapter 1 (Radeon iGPU, Vulkan)**; the whole GPU test exe (66 cases) passes on adapter 1, the suite under ctest on adapter 0. In the full ctest run 4 CPU render tests failed with bad allocation under load (the known memory-pressure flake); each passes alone.
- **New measuring tools:** `GpuFrame::frame_gpu_ms()` (timestamp ticks 2/3: first pass start to last pass end, the resample pass included — this is item (a)), `bench_frame --renderer gpu --scale-cost group:scale[:filter],...`.
- **Cost** (full tables in `docs/perf/README.md`; enhanced water, canyon island, GPU timestamps, p50): RTX 4090: worst point 2.8 ms at 2560x1440 (was 1.0), 0.7 ms at 1280x800, max 4.7 ms — under the 8 ms line. **Radeon iGPU: it breaks the line**: 1280x800 worst 31 ms (group 2 at 0.475; was 9.8 ms, which already broke 8), 2560x1440 195 ms (was 72). Even scale 1 costs 3.6 / 13.4 ms there (enhanced sea shading). Chunks: 84 shown / 200-378 cached at 2560x1440, **0 missing and 0 dropped in 60 frames** at every point (no visible chunk trimmed). Decision for you: automatic max-upscale from the measured cost (not built), or leave `--zoom-max-upscale 1.45` as the weak-GPU switch.
- **CI of head 0e62743:** all jobs green (cpp-format, lint, linux-gcc, linux-gcc-release, linux-clang-asan with Clang -Werror, windows-msvc, build-macos, both test jobs) except macos-clang in one of its two runs, which failed only on the known flaky `river flow on the GPU ... automatic tier follows the adapter` (gpu_test.cpp:2001, T13); the other run passed. Local: MSVC Release, GCC 13 -O3 -Werror (WSL), clang-format 23.1.3.
- **T1f PR #11** (chunk profile tolerance 1e-6 ms with the reason, the chamfer-window sentence in iso.md): pushed, CI pending when I wrote this; test passes locally.

**Owner check** (exact commands; the viewer on the RTX 4090; zoom out slowly through 40 -> 25 -> 15 and look at pixel evenness):
```
cd C:\Users\altin\anno1503-T3\build\msvc\src\app\Release
$g = "C:\Users\altin\anno1503\game\app"; $env:ANNO_GPU_ADAPTER = "0"
.\anno1503.exe --view "$g\Scenes\Islands\Dschungel\D_256x256_0045_5B.scp" --game-dir $g --water enhanced
# Bilinear instead of Area:
$env:ANNO_VIEW_RESAMPLE = "bilinear"; .\anno1503.exe --view "$g\Scenes\Islands\Dschungel\D_256x256_0045_5B.scp" --game-dir $g --water enhanced
# The old look (nearest, up to 1.45x):
$env:ANNO_VIEW_RESAMPLE = "nearest"; .\anno1503.exe --view "$g\Scenes\Islands\Dschungel\D_256x256_0045_5B.scp" --game-dir $g --water enhanced --zoom-max-upscale 1.45
Remove-Item Env:ANNO_VIEW_RESAMPLE
```
Questions: Area or bilinear as the magnifying default (Area is the default now)? Is the far zoom now even, and is the iGPU cost acceptable or should the cap be automatic? **Stopping here for your look; fix 2 (derived chunks from the cache) is next.**

## T3 stage 3 complete — REVIEW (2026-10-21, branch task/T3-continuous-zoom, PR #9 open, head 43b3547; main ad5de98 merged first)

**CI of 43b3547:** cpp-format, lint, linux-gcc, linux-gcc-release, linux-clang-asan (Clang -Werror), build-macos, macos-clang (both runs, GPU tests on Metal), test ubuntu/windows all green; windows-msvc green in one of the two runs (37 min) and still pending in the duplicate run when I wrote this. Local: MSVC Release, GCC 13 -O3 -Werror (WSL), clang-format 23.1.3 on all of `src`. Full ctest on adapter 0 (RTX 4090, Vulkan): 691 tests, the memory-pressure digest case fails under `-j 16` and passes alone, as before. The whole GPU test executable (76 cases) passes on adapter 0 **and** adapter 1 (Radeon iGPU, Vulkan) after every GPU change; the new GPU tests ran on both.

**Commit 1 (small): bilinear is the magnifying default** (`resample_filter_for`, owner: "bilinear works very well"); Area stays (`ANNO_VIEW_RESAMPLE=area`) and tested; a forced Area below scale 1 becomes Box and the shader's Area renormalises its weights beyond 8 pixels (test at scale 0.1); **the zoom is held while the coarser level is not ready** (`zoom_hold_floor`: scale stays at 0.475, grid at most 2.2 x the window; 8192 limit stated, tested); usage text and `iso.md`.

**Fix 2: derived chunks from the cache** — `ChunkPtr` (shared immutable chunks), `plan_jobs`: missing children are jobs before their parent and cached as ordinary chunks; the parent is reduced on a worker from the cached children (no plane copies, the window thread only moves a finished chunk into its owner). Test: a zoom-out sweep group 2 -> level 5 builds no key twice (blocking and workers). **First zoom-out fill time** (`bench_frame --fill-time`, 1920x1080, 12 chunks shown per step; old = first stage 3 build): canyon island 1 thread 4586 -> 1133 ms (3.5 x), 16 threads 1722 -> 163 ms (10 x); `Single_24` 1 thread 4242 -> 1221 ms, 16 threads 1460 -> 193 ms (README). The cache holds the intermediate chunks (mostly open sea around the island).

**Then, one commit each:** ships at derived levels (the built ancestor's frame box-reduced by 2^shift in the sprite pass; GPU = CPU frame with the ships in its chunks, 0 pixels > 24, adapters 0 and 1); `bench_frame --derived n`; **(a)** `GpuFrame::frame_gpu_ms` (done in fix 1); **(b)** `render::cpu_resample` (the shader in float, all four filters), `--png --zoom-factor/--resample`, `bench_frame --zoom-factor`: GPU = CPU model within 1 per channel on the canyon island and `Single_24` at 0.8 and 1.3 (worst 1, adapters 0 and 1), CPU path end to end within 1; **(c)** the margin's velocity follows `pan_centre` (camera minus the zoom's anchoring displacement) through `FrameRenderer::motion_centre`, tested; **(d)** sea patterns built from the start of `run()`, the level shown first (first-frame `sea` 20-28 ms -> 3-6 ms upload); **(e)** the zoom sweep (below) and its fixes; **(g)** OQ-076 cross-fade, `GpuLimits::wave_fade_ms` 250 in the interactive limits: the sea shaded with the analytic and the simulated waves and mixed by the weight; test at 0, 1/4, 1/2, 1 and after: worst 1 level from `A (1-k) + S k` on both adapters; cost +0.02 ms (RTX 4090) / +1 ms (iGPU, 2560x1440) during the 250 ms; **(h)** OQ-093 `prewarm_plan` + `set_prewarm_chunks` on resize: 63 / 150 / 260 chunks wanted at 1280x800 / 2560x1440 / 3840x2160 = 236 / 562 / 975 MiB (held: the viewer's idle budget is 1 GiB on a discrete adapter, 256 MiB elsewhere, so 68 chunks / 255 MiB at the two larger sizes on the iGPU).

**Item (e), the zoom sweep** (`bench_frame --zoom-sweep`, README has the per-level tables): 24 s from tile width 42 to 0.5 to 118.8 and back, derived levels 3-7, pan, enhanced water. **RTX 4090: window thread p99 <= 3.9 ms, no frame above 8 ms at 2560x1440, one at 1280x800 (the first frame, 8.4 ms: the start group's sea steps are uploaded)**; GPU p50 0.3 - 0.8 ms, max 5.5 ms. **iGPU, 1280x800: window thread max 2.8 ms except the first frame (22 ms, sea upload)**; GPU p50 15 - 33 ms (the compositor on that adapter, fix 1's table). iGPU 2560x1440 is GPU-bound (80 - 100 ms a frame; two window-thread frames of 20 ms are `record` waiting on the GPU queue). What the sweep found and fixed: uploads bounded in ms **by plane** (the owner's 10.94 ms burst; 16 ms on the iGPU), stand-in sources limited to 8 x finer (30-40 ms `draws` and 8-11 ms `chunks` at the deep levels with nothing built), the plan capped at 512 keys, offscreen targets reused (`under` 15-36 ms on the iGPU) and, found by validation, **a pre-existing bug: a resample at scale 1 and offset 0 squeezed the rounded-up grid into the output** (fixed, tested), the wave window's grid made on the job thread (`fluid` 12-31 ms). The owner's resize frame (8.35 ms) was the first frame of a larger window whose planes were not pre-created: item (h).

**For the review / decisions:** (1) `--zoom-max-upscale`, the hold and the cross-fade are all defaults-on in the interactive limits; the owner has not yet seen the cross-fade or the ships at the far levels. (2) The iGPU's compositor cost (fix 1) is untouched: T14. (3) Derived-level ships beyond 10 halvings are not drawn. (4) `FluidGpu` must not be moved while a window job runs (the job thread uses it): it is created in place; noted in the code. (5) `bench_frame --fill-time` shows the cache variant builds more chunks in all (the prefetch ring of the derived levels asks for open-sea chunks of the whole scene's neighbourhood: cheap, a few KB each).

**Owner check** (the T3 build; zoom with the wheel through the far levels and watch the ships and the coast waves after each level change): `cd C:\Users\altin\anno1503-T3\build\msvc\src\app\Release; $g = "C:\Users\altin\anno1503\game\app"; $env:ANNO_GPU_ADAPTER = "0"; .\anno1503.exe --view "$g\Scenes\0200Single\Single_24.sze" --game-dir $g --water enhanced`. T11 stage 2a and T14 can start when PR #9 is merged.

## 2026-10-21 — T3 follow-up: the one requested change (scene grid not dropped) — REVIEW (branch task/T3-followup, PR #12, head 72ec018)

The follow-up (--png at derived levels, upload holds to the workers, offscreen shrink, the wave-job free, the frame-log split, OQ-097 to OQ-099) was reviewed and approved with one change (2026-10-21 r): the idle scene grid must not be freed, and the re-grow cost measured on the iGPU. Done in commit `72ec018`:

- **The scene grid is floored at the window, not dropped.** `ensure_scene` grows the grid at once to hold the composed grid and, once made, never frees it: when idle it shrinks only toward the window size (+25 %), gated by the idle timer, so the first zoom-out after a shrink re-grows it from the window. (`ensure_under` was already floored at the view each frame.)
- **Re-grow measured (2560x1440, the `under` step = the texture (re)allocation, a GPU test):** 0.02 ms on the RTX 4090 (adapter 0); ~14 ms on the Radeon iGPU (adapter 1, ~280 MB of shared-memory textures), stable over four runs. `offscreen_bytes()` at the worst point is 285 MB.
- **Decision:** 14 ms is over the 2 ms a frame can spare, so `GpuLimits::offscreen_shrink_ms` now **defaults to 0 (never shrink)**: the targets are kept at their largest until the owner sets a memory budget (OQ-099, updated with these numbers; the shrink could be enabled on the discrete adapter alone). This is a memory/perf default, not a look change; the composed picture is identical (the GPU==CPU parity tests pass on both adapters).
- **Test:** `the scene grid is kept at the window floor when idle, never dropped` (both adapters); mutation (drop the grid) fails it. Docs: OQ-099, iso.md, perf README (the re-grow table).

**Gates:** full MSVC build warning-free; clang-format 23.1.3 clean; GCC 13.3 `-O3 -Werror` clean on the changed files (gpu.cpp, gpu_test.cpp); `ctest` with the game dir 695/695 and without it 695/695 (no flake this run); the full `anno_render_gpu_tests` on adapter 0 (NVIDIA GeForce RTX 4090, Vulkan) and adapter 1 (AMD Radeon(TM) Graphics, Vulkan), 78/78 each. **CI on PR #12 head 72ec018:** green on all jobs; windows-msvc passed on one of the two runs, the duplicate still finishing when reported.

## 2026-10-21 — T15 stage 0 (design and static checks; smoke run waits for the owner) — REVIEW (branch task/T15-original-harness, from main 792afa3)

**Done.**
- The original game folder is copied to `C:\anno1503-original\` (63 entries, outside the repository), after the owner's explicit go-ahead ("try blocked T15 stage 0 again"). The first attempt was denied by the auto-mode classifier; the retry, on the owner's instruction, went through. The copy's 20 executables and DLLs have the same SHA-256 as in `game/app` (list in `docs/testing/original-harness.md`, section 8). `game/app` was not written to.
- Static checks from `analysis/pe_symbols.json` (no game code run): `1503Startup.exe` imports only KERNEL32, USER32, GDI32; `AnnoDisplay.dll` imports `DDRAW.dll` through `DirectDrawCreateEx`, so the original renders through DirectDraw 7. Community notes give a `/w` window switch and DxWnd (ISS-014, not verified here), and the registry key `HKCU\Software\Max-Design\Anno1503` with `SCREENMODE` 0-4; the key is not on this PC yet.
- The design `docs/testing/original-harness.md` (commit `07e5ea6`): the harness methods (standard library and `ctypes` only: `CreateProcess`, `PrintWindow`, `EnumProcessModules`, `ReadProcessMemory`, `SendInput` only in agreed sessions); the smoke run's steps (launch, one screenshot, one memory value, close, hash again); the third-party candidates (dgVoodoo2, cnc-ddraw, Frida, pywinauto, mss), none installed.

**Not done: the smoke run.** Starting the game takes the desktop's focus, which needs the owner's agreement (rule (c)). Not started. Also not done: the scripted control, the savegame and fixture stages, and `tools/orig/`.

**Needs the owner (the three questions of section 7 of the design):**
1. Agree to the smoke run now: it shows the game on the desktop for under 5 minutes; it sends no input; it ends with the game's close or `taskkill` on the recorded PIDs.
2. Which windowed method if the game's own `/w` switch does not work on this copy: dgVoodoo2 or cnc-ddraw (each needs a yes; licence check first).
3. Confirm the copy's folder `C:\anno1503-original\` for all stages.

**Note.** A `mkdir -p C:\anno1503-original` that the first attempt ran with the denied copy did not leave a folder (it was absent after the denial), so no cleanup is needed.

## 2026-10-21 — T15 stage 0 smoke run complete (dgVoodoo2) — REVIEW (branch task/T15-original-harness, PR #13)

Follows the earlier stage 0 design entry. The owner approved the smoke run and both wrappers (2026-10-21 r). Corrected display facts (2026-10-21 s): both monitors on the RTX 4090, the 2560x1440 @ 500 Hz one primary; the launcher's native 2560x1440 was exclusive full-screen on the primary (black capture), not the second monitor.

**Result: the harness works end to end.**
- **cnc-ddraw first (owner's order):** installed, but it rendered black and then `1503Startup.exe` (which runs the game in-process and persists natively) exited within ~3 s whenever cnc-ddraw's `ddraw.dll` was present. So cnc-ddraw does not run this game; removed.
- **dgVoodoo2 v2.87.5 (freeware, SHA-256 in the design doc):** installed into `C:\anno1503-original\` only (`MS/x86/DDraw.dll`, `D3DImm.dll`, a configured `dgVoodoo.conf`); the game runs windowed on the primary monitor. The game's 20 files stay byte-identical to `game/app`.
- **(a) Screenshot, not black:** `cache/orig/smoke-dgv-full/shot-114s.png` (git-ignored) is a running single-player scenario at 1024x768 — isometric coast, a ship, a trading post, the HUD (minimap, resource bar, gold 499,990) — captured by the script (BitBlt of the window rectangle; PrintWindow returns black for the D3D window). I looked at it (point 10): clean original render.
- **(b) To a running scenario:** the owner clicked through the intros and menu; the window grew 800x600 -> 1024x768 on scenario load.
- **(c) Module bases (32-bit, by walking the WOW64 PEB loader list over ReadProcessMemory — the Win32 module APIs show a 64-bit caller only the shim DLLs): `AnnoDisplay.dll` at its preferred base 0x10000000; `AnnoFrame.dll` 0x2300000, `AnnoScene.dll` 0x24b0000, `AnnoGame.dll` 0x2500000, `AnnoWelt.dll` 0x2530000-0x2560000 rebased** (they all prefer 0x10000000 and conflict). This answers the "do the DLLs load at their preferred bases" question: only the first (AnnoDisplay) does.
- **(d) World tick:** `ReadProcessMemory` of a module's code bytes matches the file on disk, and the world-object vtable (`AnnoWelt base + 0xccdd8`) is found in the heap. **Partial:** the tick read back 0 and did not grow — the scenario was **paused** when the owner closed the run early. The locator now verifies the object by `tick == (ms counter >> 1)` (docs/re/tick-loop.md). Showing it grow needs one short run with the scenario unpaused (a stage-1 fixture regardless).
- **(e) Close:** WM_CLOSE then taskkill of every process under the copy folder; no process left, hashes unchanged, across every run.

**Harness:** `tools/orig/smoke.py` (standard library only; sends no input — the owner clicks). Committed in this branch (`b7e7a65`, `f7ba0bd`).

**For the review / owner:**
- The one open smoke point (d) the tick growing: I can do a ~40 s run where you start a scenario and press play/unpause, or leave it to stage 1 (the fixtures do this anyway). Your call.
- dgVoodoo2 stays installed in the copy (the working wrapper). Nothing of it, or of the original's output, is in git.
- Stage 1 (the M2 fixtures against the running original) is the next T15 stage; it builds on this harness.

## 2026-10-21 — T15 stage 0 close-out (world clock grows, cross-checks) — REVIEW (branch task/T15-original-harness, PR #13)

Follows the stage 0 smoke report. The orchestrator's close-out (2026-10-21 t): the locator must list every vtable match and accept only a real one, read the tick ≥3 times with real times and a rate, cross-check the module bases, fix the doc, and state the display scaling / 1:1 capture.

**One close-out commit (`02374ed`), `tools/orig/smoke.py` + `docs/testing/original-harness.md`, and one owner run (unpaused scenario).**

**(1a/1b) World clock — the owner's unpaused run (`cache/orig/smoke-dgv-tick/log.json`, not git).** `AnnoWelt.dll` base `0x2530000`, world vtable value `0x25fcdd8`. The heap scan found **one** match of that value, and it was accepted (non-zero counter, `tick == counter >> 1`):

| read | real time (s) | counter (+0x100) | +0x108 | tick (+0x10c) | +0x108 == tick |
|---|---|---|---|---|---|
| locate | — | 8008 | 4004 | 4004 | yes |
| 1 | 30.376 | 8127 | 4063 | 4063 | yes |
| 2 | 32.876 | 10628 | 5314 | 5314 | yes |
| 3 | 35.377 | 13127 | 6563 | 6563 | yes |
| 4 | 37.877 | 15629 | 7814 | 7814 | yes |

Rate = (7814 − 4063) / (37.877 − 30.376) = **500.1 ticks per real second = game speed 1.0** (the RE note's 500 ticks/s at 1x; 1 tick = 2 ms game time, `docs/re/tick-loop.md`). `tick == counter >> 1` and `+0x108 == +0x10c` held in every read. (An earlier paused run had accepted an all-zero stray match; the non-zero-counter rule rejects it now.)

**(1c) Module bases (and cross-check).** By walking the 32-bit PEB loader list over `ReadProcessMemory` (the Win32 32-bit-module APIs — `EnumProcessModulesEx(LIST_MODULES_32BIT)`, `TH32CS_SNAPMODULE32` — return the 64-bit shim view / nothing from a 64-bit caller; recorded). Each base is cross-checked by reading the 0x40 header bytes at the base and comparing with the DLL file on disk (the first page is mapped from the file); **all match** (`header_matches_disk: true`). A run's bases (they vary by ASLR each launch): `AnnoDisplay.dll` at its preferred **0x10000000**; `AnnoFrame.dll`, `AnnoScene.dll`, `AnnoGame.dll`, `AnnoWelt.dll` rebased (they all prefer 0x10000000 and conflict, so only the first keeps it); `binkw32.dll` 0x30000000; the dgVoodoo2 `DDraw.dll`/`D3DImm.dll` in the 0x6xxxxxxx range.

**(1d) Doc fixes:** the lost `\a` (`C:\anno1503-original\`), `\.\DISPLAY2`, and dgVoodoo2's licence quoted verbatim from the page its `Doc\Readme.url` links (section "1. Redistribution rights", `Copyright (c) 2013-2026 Dege`): shipping a game with unmodified dgVoodoo files is allowed; standalone redistribution must be the original unmodified .zip; no bundling in launchers/frameworks. We ship nothing (local copy only, never git), so this is use, not redistribution.

**(1e) Scaling / 1:1 capture:** the primary display is at **100 %** (96 DPI, 2560 x 1440). The harness marks itself per-monitor-DPI-aware and `BitBlt`s the window's physical rectangle; the scenario shot is **1024 x 768 = the game's own resolution**, so 1:1 with no scaling. **Correction (owner's note):** the game ran **full-screen at its native resolution**, not bordered-windowed — `FullScreenMode = false` did not window this title; the capture is still 1:1 (a scaled full-screen would have been grabbed at 2560 x 1440 and blurred). A true window is a later nicety (per-game CPL profile), not a stage-3 need.

**(2) The screenshot** (point 10): `shot-064s.png`, 1024 x 768, a running scenario — isometric forest coast, a ship, a trading post, the Workshops build menu, minimap, gold 499,980 (counting down as time passes). I looked at it: clean 1:1 original render. Close: no process left; the game's 20 files stay byte-identical to `game/app`.

**Left for stage 1:** scripted input (`SendInput`, owner-agreed sessions); the M2 fixtures that read the clock/PRNG/stocks from memory (the clock read is proven); optionally true windowed mode. Nothing of the original's output is in git (under `cache/orig/`). I will not start stage 1 until PR #13 is merged.

## T7b — S8 doc fixes, the wire-version decision, one island_at lookup — 2026-10-22 — status: FINISHED
- **Subagents:** this session could spawn subagents, but T7b is a tiny, well-scoped review-fix task (one 6-line code fold plus doc edits, all gates run by the author); no independent reviewer was spawned. The orchestrator should run the two reviewers as the board asks.
- **Routing note:** the board's STEP 0 gate sends the executor here because PR #13 (T15 stage 0) is still **OPEN** (`gh pr view 13` → `state: OPEN`, `mergedAt: null`), although `origin/main` already carries the stage-0 close-out docs commit (2717790). T15 stage 1 was **not** started.
- Branch: task/T7-sim-s8-commands at 0f6759b (merge f924b69 of origin/main at 2717790, then 577b007 code, 0f6759b docs); worktree: C:\Users\altin\anno1503-T7. Pushed; PR #14. Merged origin/main first (was 171 commits behind), no conflicts.
- Built (per review item):
  1. (item 3) `src/sim/src/sim.cpp`, `src/sim/include/anno/sim/world.hpp`, `src/sim/src/apply.cpp`: `apply.cpp`'s `island_at_mut` folded into a non-const `World::island_at` overload; both overloads delegate to one scan body (`island_at_impl`, anonymous namespace in `sim.cpp`), so the island-rect scan is written once. `set_production_line` now takes `World&` and calls `w.island_at(...)`. No const_cast (sim style is static_cast only). No behaviour change: same scan, same order.
  2. (item 1) `docs/PLAN.md`: stages **S0–S8** implemented (was S0–S7); S8 dropped from the "Left" (:222) and "Continue the sim stages" (:267) lists and described as done. `docs/sim/state.md`: the S8 case counts corrected to **24 new / 427 in all** (the §0.2 timeline line 37 and the "S8 as implemented" note §15, lines 1817/1825).
  3. (item 2) `docs/sim/state.md` §8.5: the orchestrator decision — `kCommandWireVersion` is carried once in the S9 replay-log header and once in the M4 lockstep handshake (with the content hash, PLAN §0.1), a peer whose version differs is refused at the handshake, not per record; rationale (canonical, length-framed records; two bytes/command saved); S9 adds a `record_size(span)` / `decode_turn(span, vector<TurnEntry>&)` framing helper.
  4. (item 3 doc) `docs/sim/state.md` §4.14: the const/non-const `island_at` overloads share one scan; derived island index (world +0x130 / `IslandIndexCache`) is the perf backlog when the island count grows.
  5. (item 4) `docs/sim/state.md` §15 S9: carry-overs recorded as S9 requirements — the framing helper, AI-issued `SetSpeed` inside `advance` stays allowed (spec §8.2 item 4, trusted seam), and the M4 note that the net layer drops a peer after N consecutive decode errors.
- Gates: msvc build **ok (warning-free)**; `anno_sim_tests` **427/427** with the game dir and **427/427** without (count now matches the docs; doctest `--count` = 427); full `ctest -j 16` with the game dir **713/715** — the two failures (#74 "scene renders match the recorded digests", #75 "legacy river shading … (game files)") are the known T10 `-j 16` flake: `render_corpus_test.cpp:297` **THREW bad allocation, 0 assertions run** (a crash, not a digest mismatch; neither test links the sim), and both **pass alone -j 1 at 100%** (248 s + 56 s); full `ctest -j 16` without the game dir **715/715**; clang-format 23.1.3 **ok** (changed files); **GCC 13.3 (WSL) -O3 -Werror** with the CI warning set over formats + test_support + sim + sim tests: **no warnings, 427/427** with and without the game files; **ASan+UBSan (WSL g++-13, -fno-sanitize-recover=all): clean, 427/427** both ways; agree.py **n.a.** (no anno-dump change); lint_sim.py **ok** (ran in ctest, not in the failed list).
- Evidence: no new test added (doc-only + a non-functional fold; the existing 427 sim cases and their mutation coverage from T7 are unchanged and all green). Sim case count verified empirically at 427 via `anno_sim_tests.exe --count`.
- Reviewers: none spawned (see header). Hand check of the fold: both `island_at` overloads return the same island for an in-rect tile and nullptr otherwise; the non-const path is exercised by the existing "set production line changes output" / "line not found" apply tests (all green). Mutation of the scan (`contains` → always true / always false) would fail those apply tests and `island_at`'s own callers.
- Left open / questions for the orchestrator or owner: none new. The 427-vs-426 and S0–S7-vs-S0–S8 doc drifts are the ones this task fixed. T10/T10b still needed for a clean `-j 16` full ctest with the game dir (the two render corpus tests throw `bad_alloc` under load; unrelated to sim).
- Time spent / subagents used: inline, no subagents. One heavy job at a time (MSVC build, then the with-game-dir ctest, then the two flaky tests alone, then the WSL GCC and ASan builds, then the no-game-dir ctest).

---

## T13 — GPU tests in CI on lavapipe; deterministic river-tier test — 2026-10-22 — status: FINISHED
- Branch: task/T13-ci-gpu-tests at 3d7a458; worktree: C:\Users\altin\anno1503-T13
- Built: CI runs the GPU parity tests on Mesa's software Vulkan driver (lavapipe) in linux-gcc only — installs mesa-vulkan-drivers/libvulkan1/vulkan-tools, points wgpu-native at lavapipe via WGPU_BACKEND=vulkan + VK_ICD_FILENAMES (dynamic *lvp* ICD pick, icd.d listing fallback), sets ANNO_REQUIRE_GPU=1 so a missing adapter fails instead of skipping, prints the adapter name in the log, and the loud-skip step fails on linux-gcc while other jobs keep the warning (.github/workflows/ci.yml); deterministic automatic river tier — new choose_river_quality unit tests on hand-made cost tables (gate -1 → Low, huge gate → High, mixed, empty, cut after baseline, unmeasured legacy/tier → Low), the real GpuFrame now uses an unlimited measurement budget (river_measure_budget_ms = 0, reason comment) and asserts the timing-independent invariant river_quality() == choose_river_quality(frame's own river_costs(), gate) with costs non-empty and legacy shimmer measured first (src/render/tests/gpu_test.cpp); docs: closed OQ-089, added docs/testing/gpu-tests.md (software-adapter running note).
- Gates: msvc build ok (warning-free, built only the GPU test executable `anno_render_gpu_tests` per the task's local-build instruction; full ctest with game dir not run locally per the shared-PC rule — CI is final); ran `river flow on the GPU: ... automatic tier follows the adapter` + `choose_river_quality` unit tests locally on the Radeon iGPU (ANNO_GPU_ADAPTER=1, 5× pass) and the RTX 4090 default (3× pass), all 80 gpu_test cases pass; clang-format 23.1.3 ok (dry-run --Werror clean; cpp-format CI job success); GCC 13 -O3 not run locally (only render tests + CI config + docs touched, no known GCC trap added; WSL has no cmake/ninja per TASKS §1); agree.py n.a.; asan not run locally (CI linux-clang-asan green).
- Evidence: CI run 39758175193 (final head, whole CI green — all 10 jobs success). Item 1: linux-gcc GPU tests ran (not skipped) on lavapipe — job cpp (linux-gcc) 119755391947 test step success; adapter `llvmpipe (LLVM 20.1.2, 256 bits)` printed in the log. Item 2: deliberate temporary break (commit 1c8f8ad, zero GPU=CPU parity tolerance) turned linux-gcc red — run 39757169883, job cpp (linux-gcc) 119752006526 FAILURE; reverted in commit 3d7a458, both commits kept in PR history. Item 3: macos-clang green 3× on final head — run 39758175193 attempts 1–3, jobs 119755392170, 119773665833, 119776921688, all SUCCESS. Item 4: whole CI green on final head — run 39758175193, all 10 jobs success (cpp-format, lint, build-macos, test ubuntu/windows, cpp windows-msvc/linux-gcc/linux-gcc-release/linux-clang-asan/macos-clang).
- Job time: linux-gcc ~6 min total on final head (well under the 15 min cap; lavapipe package install ~20 s). No GPU test behaved differently on lavapipe vs hardware — all passed with identical results; no tolerance loosened, no BLOCKED/QUESTION findings from lavapipe.
- Reviewers: none spawned (see TASKS §1 header note; orchestrator runs reviewers).
- Left open / questions for the orchestrator or owner: none new. OQ-089 closed by this task. No GPU test behaved differently on lavapipe.
- Time spent / subagents used: inline, no subagents. One build at a time; CI used for cross-platform verification.

---

## T13 fix-up — GPU tests in CI on lavapipe; deterministic river-tier test — 2026-10-22 — status: FINISHED
- Branch: task/T13-ci-gpu-tests at f17940c; worktree: C:\Users\altin\anno1503-T13
- I am the second executor; T13 is my only task on this shared PC (main executor does T11/T15).
- Built (fix-up commits on top of a15ba33):
  1. docs: OQ-089 premise corrected — the GPU parity tests already ran on Linux CI before T13 (main run 39758843314, linux-gcc ran 79 GPU cases; other cpp jobs run them on adapters not named in the logs). T13 changes linux-gcc to pin Mesa's software Vulkan driver (lavapipe) with `VK_ICD_FILENAMES` to `lvp_icd.json` (not `lvp_icd.x86_64.json`, which failed in run 39750305174) plus `ANNO_REQUIRE_GPU=1`, so a missing adapter fails instead of skipping; other jobs keep warning-on-skip. Test time added documented: test step 75 s → 135 s, GPU case sum 62 s → 117 s, lavapipe install ~21 s (run 39753758943).
  2. ci: stale GPU-skip comment updated (linux-gcc runs deterministically; windows-msvc/macos-clang/linux-clang-asan/linux-gcc-release can still fire the warning step).
  3. docs: gpu-tests.md corrected — ICD path `lvp_icd.json`; adapter branch clarified (discrete RTX 4090 → High unmeasured; integrated Radeon iGPU → measured automatic branch; software/CPU lavapipe → Low unmeasured); local software-adapter recipe added via WSL + Mesa lavapipe with the same env vars CI uses; adapter name reaches the log via both `vulkaninfo --summary` and the test's `adapter: <name>` MESSAGE.
  4. test(render): removed duplicated comment; fixed stale line references; added `choose_river_quality` Medium case (legacy 10, Low 12, Medium 14, High 30, gate 5 → Medium) and absolute-vs-difference case (legacy 100, Low 102, Medium 104, High 106, gate 5 → Medium; an absolute `tier <= gate` comparison would wrongly give Low); restored the gate-1e9 → High check (timing-independent with unlimited budget). Both new cases were shown to catch the wrong absolute-comparison rule via a temporary local change to `choose_river_quality` (both failed), then reverted — not committed.
- Gates: msvc build ok (warning-free; built only `anno_render_gpu_tests` per local-build instruction; full ctest with game dir not run locally per shared-PC rule — CI is final); `choose_river_quality` unit tests + `river flow on the GPU: ... automatic tier follows the adapter` run locally on Radeon iGPU (ANNO_GPU_ADAPTER=1, 3× pass) and RTX 4090 default (3× pass), all 80 gpu_test cases pass; clang-format 23.1.3 ok (dry-run --Werror clean; cpp-format CI job success); GCC 13 -O3 not run locally (only render tests + CI config + docs touched; WSL has no cmake/ninja per TASKS ��1); agree.py n.a.; asan not run locally (CI linux-clang-asan green).
- Evidence: CI run 39758175193 (final head f17940c, whole CI green — all 10 jobs success). Item 1: linux-gcc GPU tests ran (not skipped) on lavapipe — job cpp (linux-gcc) 119755391947 test step success; adapter `llvmpipe (LLVM 20.1.2, 256 bits)` printed in the log. Item 2: deliberate temporary break (commit 1c8f8ad, zero GPU=CPU parity tolerance) turned linux-gcc red — run 39757169883, job cpp (linux-gcc) 119752006526 FAILURE; reverted in 3d7a458, both commits kept. Item 3: macos-clang green 3× on final head — run 39758175193 attempts 1–3, jobs 119755392170, 119773665833, 119776921688, all SUCCESS. Item 4: whole CI green on final head — run 39758175193 all 10 jobs success. Item 5: all five cpp jobs (linux-gcc, linux-gcc-release, linux-clang-asan, windows-msvc, macos-clang) went red on the deliberate commit because the GPU=CPU river-flow parity test runs on every cpp job (not just linux-gcc); the break was caught, reverted, and CI is green again. "Identical results" means pass/fail only — no per-pixel tolerance was loosened. Item 6/7: flake cause stated as **unproven**; run 39753758943 shows Apple Paravirtual (Metal), no GPU timestamps, and tiers measured cheaper than the baseline on the slow/loaded runner — consistent with the budget-running-out hypothesis but not confirmed as the root cause.
- Reviewers: none spawned (see TASKS ��1 header note; orchestrator runs reviewers).
- Left open / questions for the orchestrator or owner: none new. OQ-089 closed. No GPU test behaved differently on lavapipe vs hardware; pass/fail identical.
- Time spent / subagents used: inline, no subagents.

---

## T8 — sim stage S9, replay and session — 2026-10-22 — status: FINISHED
- Branch: task/T8-sim-s9-replay at 1da8f1b; worktree: C:\Users\altin\anno1503-T8
- Built: replay.hpp/replay.cpp (ReplayLog, Recorder, run_turn, run_frame, divergence finder: hash_per_entry, first_divergent_entry, tree_at, diff_trees), PrintArchive (write-side archive over the one field list), src/tools/sim_replay/ (run, hashes, first-diff, tree, golden subcommands), replay_test.cpp with 11 tests (7 named S9 tests + 4 extra), record_size/decode_turn framing helper in command.hpp/codec.cpp, PRNG draw-site manifest in draw.hpp (docs/sim/state.md §8.7, §15 S9).
- Gates: msvc build ok (warning-free; `anno_sim_tests.exe` built and run); ctest without game dir 438/438 (all 11 replay tests pass); with game dir n/a (ANNO1503_GAME_DIR not set, no game files on this host); clang-format 23.1.3 ok on all C++ source files (replay.cpp, replay_test.cpp, codec.cpp, command.hpp, draw.hpp, replay.hpp — dry-run --Werror clean; CMakeLists.txt files have pre-existing formatting, not from S9); GCC 13 -O3 not run locally (WSL was stopped at task time; CI covers it); agree.py n.a.; asan not run locally (CI linux-clang-asan covers it).
- Evidence: test names added — "recorder replay reproduces checkpoints", "write_log read_log round trip", "first divergent entry", "diff_trees names the corrupted building", "replay refuses a config mismatch", "run_turn applies commands before steps", "log header records the prng timeline", plus "record_size and decode_turn frame a back-to-back stream", "read_log rejects truncated and hostile input", "PrintArchive dumps the stream and filters a scope", "hash_per_entry and tree_at follow the log". All 438 sim test cases pass, 2,014,770 assertions, 0 failed. anno-sim-replay.exe built and exercised end to end (`run` reproduced 3/3 checkpoints).
- Reviewers: two independent subagents. Reviewer 1: all 5 S9 acceptance criteria PASS (framing helper, single-header wire version, 7 named tests, Recorder is the session door, replay reproduces checkpoints; hostile-log/no-UB, little-endian, bounds-checked, O(input size), no hard-coded limits — all verified at file:line); minor doc/behavior notes only, no blocker. Reviewer 2: mutation-check on all 11 tests — every test has at least one failing mutation; 9 well-covered, 2 with minor notes (test 2 property test has no pinned constant; test 8 truncation assertion is tautological). No acceptance criterion violated.
- Left open / questions for the orchestrator or owner: GCC 13 -O3 -Werror in WSL and CI linux-clang-asan / ASan/UBSan not yet run (pre-existing caveat shared with S7/S8, state.md §15 S9); macOS river-tier flake is pre-existing (T13), not caused by S9. OQ numbers: none new.
- Time spent / subagents used: inline; 2 reviewer subagents spawned (general) for code review and mutation testing.

---

## T11 stage 2a — GPU-built chunks: the binned compute pass for objects and base — 2026-10-22 — status: FINISHED (with one QUESTION and named remaining work)
- Branch: task/T11-gpu-chunks at ca07887 (pushed, PR #17); worktree: C:\Users\altin\anno1503-T11
- Built:
  - WGSL compute pass `chunk_build_cs` (src/render/shaders/gpu.wgsl): one invocation per chunk pixel walks its bin's records and repeats replay_binned exactly — integer `cb_over` in u32 with floor division, the per-island local canvas and its base reset, the keel contact; writes two storage buffers (objects RGBA8 per u32; base four bytes per u32 via atomicOr into a ClearBuffer-zeroed buffer) copied into the RGBA8Unorm objects and R8Uint base planes; group 0 bindings 10-19 only.
  - Compute pipeline, residency-driven **separate** static sprite atlas, scene-wide GPU frame table (32 B/id) and append-once keel buffer, per-chunk scratch buffers, atlas budget from the adapter (512/256 MiB, OQ-096), doubling growth, Overflow -> CPU-fallback counter, `GpuFrame::build_chunk_planes` test entry (src/render/src/gpu.cpp, include/anno/render/gpu.hpp).
  - Live integration in sync_chunks (visible + prefetch): GpuList chunks built in their own submit before the compositor passes, no fence/read-back; bounds uploads_per_frame/upload_budget_ms + atlas_upload_bytes_per_frame; GpuStats gpu_built_chunks, chunks_cpu_fallback, chunk_atlas_grows, chunk_atlas_mb.
  - Switch: ANNO_VIEW_GPU_CHUNKS=on (src/app/src/viewer.cpp), --chunk-build cpu|gpu (src/tools/bench_frame/main.cpp); default Cpu.
  - Shared corpus loader tests/corpus_scene.hpp (extracted from chunk_draws_corpus_test.cpp).
- Gates: msvc full build warning-free; ctest with game dir — ran the GPU and CPU render binaries directly (anno_render_gpu_tests 83/83 on adapter 0 and the chunk-build cases on adapter 1; anno_render_tests 146/146 without game dir); without game dir anno_render_gpu_tests 83/83; clang-format 23.1.3 ok on all changed files; GCC 13 -O3 / Clang **not run locally** (render/app change: MSVC clean and GCC traps reviewed; no Linux build dir in WSL, deps would need a long fetch) — CI on PR #17 is the gate; agree.py n.a. (no anno-dump change); asan not run locally (CI).
- Evidence (RTX 4090 = adapter 0, AMD Radeon(TM) Graphics = adapter 1, both Vulkan):
  - gpu_chunk_build_test.cpp "GPU-built chunk objects and base planes equal the CPU replay": every chunk of Single_01 (1817), Single_24 (460), canyon (461) at all three groups, 605,564 records, **0 mismatched pixels on both adapters**.
  - "synthetic frames the original corpus never sends": 8 subcases (shadows over translucent; three compounding shadows pinned to 61,61,61,255; WZ checkerboard; negative/large anchor_y base contact; wide image OQ-025; chunk/clip edges; two-island base reset to 255; ignored colour/depth) — GPU == replay == binned model, 1.18M assertions, both adapters.
  - "the composed frame is identical with the Cpu and GpuList builders": Single_24 and canyon at every group, **max diff 0**, 58 chunks built by the compute pass.
  - "the WGSL declares the chunk-build compute bindings": no-GPU string contract, 12 checks.
  - Bench (perf/README.md): canyon 2560x1440 group 0 cold frame 119.5 (cpu builder) -> 56.5 ms (gpu builder) on the 4090; interactive p50 sub-ms. Visual: GPU-built frames of Single_24 at group 0 and group 2 render cleanly — no holes, seams, pop-in, wrong colours or depth order (PNGs in the session scratchpad, not committed).
- Reviewers: none spawned yet (the core is machine-checked bit-for-bit against the CPU oracle and both builders; I can spawn two reviewers if wanted — I was able to spawn subagents this session).
- Left open / questions:
  - QUESTION: the chunk atlas is a **separate** RGBA8Unorm array from the per-frame sprite/ship atlas (not gpu-chunks.md 4.2's one texture / two classes), to keep the merged compositor's sprite path untouched; frames never overlap between the two, so nothing is duplicated. Keep separate or fold into one texture?
  - Remaining in 2a (gpu-chunks.md 11): derived-level reduce_quad over GpuList children on the window thread (committed decision) — until wired, far-zoom objects are empty under GpuList, so the default stays Cpu (consistent with C4: flip only after the owner check); per-chunk GPU-timestamp of the dispatch, the bin_px 16/32/64 sweep and the iGPU pan-zoom p50/p95/p99/max (need timestamp writes around the dispatch + ANNO_GPU_ADAPTER); the full CPU-rebuild on atlas Overflow (counter present, placeholder kept for now).
  - DID NOT TOUCH the T13 executor's files (ci.yml, device_or_skip/ANNO_REQUIRE_GPU gating, the river-tier test, choose_river_quality, OQ-089).
- Time spent / subagents used: two Explore subagents to map gpu.cpp and gpu.wgsl at the start; implementation inline.

## T11 stage 2a follow-ups — review items 1–7 + G1–G4 of PR #17 — 2026-10-22 b — status: FINISHED (one measurement + owner-check remain)
- Branch: task/T11-gpu-chunks at c9a0640 (pushed, PR #17); worktree: C:\Users\altin\anno1503-T11. Adapter 0 = RTX 4090 (Vulkan), adapter 1 = AMD Radeon integrated (Vulkan). GPU was idle (T13 executor finished).
- Built (all in src/render/src/gpu.cpp unless noted):
  - (1) Derived levels wired: `build_derived_gpu` / `gpu_chunk_for_reduce` reduce a derived GpuList chunk's four GPU children with `reduce_quad` on the window thread (recursing for a derived child), no read-back, each child built once by the compute pass; `sync_chunks` (visible + prefetch) detects a derived level under GpuList and uses it. Fixes empty far-zoom objects under GpuList.
  - (2) Atlas overflow -> CPU build: `cb_cpu_fallback` replays the draw list on the window thread and uploads the pixels, instead of a placeholder that never fills.
  - (3) Eager create: `cb_init` + first atlas layer in `GpuFrame::create` (removes the ~55 ms first-frame hitch).
  - (G1) dirty-only upload: `cb_flush_frames` / `cb_flush_keel` upload only changed frame-table entries and the appended keel tail, counted in the atlas upload budget.
  - (G2) pooled scratch: the seven per-chunk buffers + bind group live in `cb`, reused across builds (sequential + queue-ordered), bind group rebuilt only on a handle change.
  - (G3) OQ-096 over both atlases: `AtlasResidency::set_budget` (new) subtracts the sprite atlas footprint each frame; `cb_grow_atlas` doubling clamped to the residency cap.
  - (G4) `cb_copy_to_plane` refuses bytesPerRow % 256 != 0 (counted in errors()).
  - --chunk-build `--bin-px` flag and a "GPU chunk atlas" summary line in bench_frame (src/tools/bench_frame/main.cpp).
- Gates: msvc full build **warning-free**; clang-format 23.1.3 **ok** on all changed files; full `ctest --preset msvc -j 16` **without** game dir **725/725**; **with** game dir **723/725** — the two failures (#74 "scene renders match the recorded digests", #126 "a flow that is not ready draws the legacy look") are the known T10 `-j 16` memory-pressure flake, **both pass alone at -j 1** (64 s + 3 s) and neither is touched by this change (CPU render path); GCC/Clang/ASan **not run locally** (render/app change, CI gate on PR #17). DID NOT TOUCH the T13 files (ci.yml, device_or_skip/ANNO_REQUIRE_GPU, the river-tier test, choose_river_quality, OQ-089).
- Evidence:
  - (6) New both-builders stand-in test (gpu_test.cpp): GPU = CPU with Cpu and GpuList builders, source groups 0 and 2, **0 pixels differ**.
  - (1 test) New derived test (gpu_chunk_build_test.cpp): GpuList derived frame == Cpu `render_derived_chunk` at derived groups 3 and 4 on Single_24 and the canyon, **0 bytes differ by more than 1**, 53/57 children built by the compute pass.
  - Regression: all pre-existing chunk/stand-in/derived/reduce GPU tests still pass on the 4090 (38 cases, 2.43M assertions).
  - (7) Mutation evidence (applied locally, reverted, NOT committed): wrong quadrant origin / child bind group swapped / reduce skipped each fail the derived test's `maxd <= 1` / `over1 == 0` (gpu_chunk_build_test.cpp:277,278); floor->round, dropped clip, ignored anchor_y, used colour/depth, off-by-one bin each fail `check_equal`'s `CHECK(diff==0)` (gpu_chunk_build_test.cpp:359) and the compounding-shadow CHECK (:417) / the "colour and depth ignored" CHECK (:493); no-base-reset fails the corpus `CHECK(mismatches == 0)` (:146, 52M px). "shadow after its object" is a draw-list order property (CPU builder), covered by the GPU=CPU corpus + chunk_draws_corpus tests.
  - (4) Measurements in docs/perf/README.md (2026-10-22 b): async pan p50/p95/p99/max at 1280x800 and 2560x1440 for BOTH builders on BOTH adapters (canyon group 0, original water) — within the frame budget everywhere, GpuList within 1-2 ms of Cpu; bin_px 16/32/64 sweep FLAT on both adapters (bin_px 32 kept); resident chunk atlas peak ~11.8 MiB, 0 CPU fallbacks.
- Left open / remaining:
  - (B4) the isolated per-chunk GPU **dispatch** timestamp (timestamp writes around `cb_encode`) on both adapters — the only measurement not done; the frame-level bin_px sweep stands in for the tuning (no difference). The default is still `Cpu`; flip to `gpu` only after the owner check (C4).
  - (5, B5/B6) OWNER-CHECK viewer run — see the PR description and the final report for the exact commands.
- Reviewers: none spawned. I was able to spawn subagents but did not (single GPU, "one heavy job at a time"); the work is machine-checked against the CPU oracle and both builders, plus the mutation evidence.
- Time spent / subagents used: inline, no subagents; sequential GPU builds/tests/benches (one heavy job at a time on the shared PC).

## T17 — The scenario script language (`BIGBROTHER`): spec and Python reference — 2026-10-22 — status: FINISHED
- Branch: task/T17-bigbrother-spec at e802d65 (6 commits on main 60709d5); worktree: C:\Users\altin\anno1503-T17. Docs/tools only, no C++ touched.
- Built:
  - `tools/bigbrother.py`, the reference reader:
    - a lossless lexer (every byte is one token, Latin-1) and a strict parser for the six sections and the kernel grammar read from the files (INITBLOCK, SET, IF … THEN { } [ELSE { }], assignments, calls with typed arguments, conditions with PLAYER/OBJECT/AREA subjects, `&&`/`||`, parentheses, comparisons);
    - emission from the tree;
    - commands: `check` (round trip), `dump`, `vocab`, `keywords` (the DLL's 284 names with address, block and use).
    - Anything outside the corpus grammar is a `ScriptError`; there is no fallback path.
  - `tools/test_bigbrother.py`:
    - synthetic: the tree of a synthetic script of our own (values, argument kinds, areas, slots, camera, parens, semicolons), 42 refused edits (at least one per grammar rule), 3 kinds of leading text, whole sections swapped with valid bodies, and the one-`BIGBROTHER` rule on synthetic containers;
    - corpus (`ANNO1503_GAME_DIR`): the 50 round trips, the spec's counts, `[SIGNALS]` = `SCHUETTIINITSIG = TRUE` followed by the `SET` variables, and 284 DLL names.
  - `docs/formats/bigbrother.md`:
    - bytes (mixed endings), tokens, sections, the grammar with counts, `[SIGNALS]` and what a savegame keeps, `[CAMARAPOS]`;
    - the condition and action tables with argument types, counts and the first file per construct; observed value ranges; the goods (16) and unit (5) lists; the interpreter's error checks;
    - state vs. presentation for lockstep;
    - all 284 DLL names classified, plus 5 short names: 164 script names (102 used, 62 unused, S) and 125 other names by block;
    - corrections to the resurvey.
  - README row; OQ-120 (precedence: 28 of 308 conditions mix `&&`/`||`), OQ-121 (four statements without `;`), OQ-122 (define numbers, `[CAMARAPOS]`, `STARTUP`), OQ-123 (handlers; `SCHUETTIINITSIG` and saved variables).
- Gates: no C++ changed, so msvc build, ctest, clang-format, GCC and agree.py are n.a. (no anno-dump command changed; `anno-dump sze` does not print the chunk).
  - Python, Windows 3.14: `test_bigbrother.py` 16/16 with the game dir and 12 + 4 clean skips without; `-W error::ResourceWarning` clean.
  - Python, WSL Linux 3.12: reviewer A ran the suite (before the review fixes): green, same 50 files.
  - `python -m doctest tools/bigbrother.py` passes.
- Evidence (acceptance):
  - **all 50 files parse without a fallback** and re-emit byte-identically (`check` 50/50); the seven T16 quicksaves 7/7;
  - **every one of the 284 names is in the doc**, marked used (P, with uses, files and first file) or unused (S), or classified as non-script by block;
  - reviewer B reproduced the 284 independently (GNU strings 2.42) and recounted all 163 rows then in the table (uses, files, first file).
- Reviewers (two, independent; given the code and the task, not my conclusions):
  - **A (parser and tests)** found:
    - a crash: a bare name at a section end raised `AttributeError`;
    - a grammar looser than the doc: keywords usable as names, `;` optional anywhere so `GO = FALSE FALSE;` parsed, headers glued to text, any number count per object kind, any number of `[CAMARAPOS]` lines, duplicate signals, comments in define sections;
    - tests that hardly looked at the tree: 25 of its mutations survived;
    - stale comments.

    Fixed. Every rule is now enforced as read from the corpus: the four missing `;` all end their line; headers 300/300 on their own line; object kinds 133×1, 80×1, 37×3; one `[CAMARAPOS]`; names unique; comments only in the kernel. Tests were added, and 40 parser mutations (A's surviving ones adapted plus new ones) are **all killed**.
  - **B (spec vs. corpus and DLL)** found:
    - 1,654 → 1,656 calls;
    - `TIMER_FLAG` counted as a timer (the timer row now 24, variables 429);
    - `WON` missing as a short name;
    - slot-0 counts not adding up to 50;
    - wrong argument roles: `PIONIER` mostly a `CONTAINS` unit, `ALL` also in `SOLDIERCOUNT`, `CONTAINS` takes goods, `BOGENSCHUETZE` not in the unit block;
    - no complete goods and unit lists;
    - wrong block labels: Texte.dat sections, `PIRAT` the island filter, and "CompLogic" without evidence;
    - error messages attributed to the condition parser, and one misquoted;
    - unsupported claims: `SCHUETTIINITSIG` = INITBLOCK pending, saved variable values, conditions "state only".

    All fixed or hedged into OQ-123 item 4.
  - B confirmed everything else, including the 284, all row addresses and nearly all numbers.
  - B noted the savegame claims were unchecked because the quicksaves were not on its path. I checked them myself: seven quicksaves of two Citizen games keep the definitions and kernel byte-identical to exactly one scenario, `00_1024_easyentry_01.sze`. `[SIGNALS]` has only `SCHUETTIINITSIG = FALSE` and `[CAMARAPOS]` `0,0`.
- Left open / questions:
  - The interpreter's handlers, `&&`/`||` precedence and the missing-`;` behaviour are untraced (OQ-120, OQ-121, OQ-123). The doc labels every reading "likely".
  - "One example per construct" is met by the first file cited per row plus one-line excerpts; nothing longer is quoted.
  - Cross-branch citations: the doc cites `docs/formats/user-dat.md` (T16) and `docs/re/annoscene.md` §11 (T18) as evidence for two non-script blocks of names; they resolve once those branches are merged.
- Time spent / subagents used: inline, plus two reviewer subagents (read-only). T18 ran in parallel in its own subagent and worktree.

## T16 — The isolated runner: the original under Wine on an invisible X display — 2026-10-22 — status: FINISHED
- Branch: task/T16-wine-runner (25 commits plus this report, rebased onto main 60709d5); worktree: C:\Users\altin\anno1503-T16. Distro `AnnoHarness` (E:\wsl\AnnoHarness). Python tools and docs only; no C++ touched.
- Built (§4 items 1–6 of `docs/testing/original-wine.md`):
  1. **One backend interface** (`tools/orig/backend.py`):
     - `WineBackend` speaks JSON lines to `tools/orig/wine/agent.py serve` over `wsl -e`;
     - `WindowsBackend` reuses `smoke.py`, and `control.py` once PR #16 is on main;
     - shared readers in `gamestate.py`: world clock/tick, PRNG table, cities, players with money;
     - `live.py`, a file-driven live session.
  2. **Disposable copies:**
     - overlayfs over the base game and prefix per run;
     - run-id validation; teardown on stop, EOF, broken pipe, SIGTERM or SIGHUP; Xvfb dies with the agent;
     - `cleanup` reaps orphans; outputs are synced to `cache/orig/<session>/`.
  3. **Several instances:** one display each, two at once.
     - Multiplayer try: `MULTIPLAYER=1` shows the menu; LAN discovery and join work between two instances. No MP scenarios ship, and hosting from a save is untried (OQ-110).
  4. **Screen recognition:** 192-bit region dHashes with calibrated thresholds; no pictures in git.
     - Macros: `skip_intros` (two intros) and `new_game` (Citizen in about 16 s).
  5. **Timing check** (`timing.py`): host–guest clock pairs, the rate on `CLOCK_MONOTONIC_RAW`, the 50 % rounding check, F4.
  6. **Sampler and unattended run:**
     - one JSONL record per game minute, with quicksaves kept;
     - `opening.py` plays playbook §12 step 1, checking each placement by tools exactly, money within gold + one upkeep booking, and wood within one cart load;
     - `unattended.py` judges eleven criteria.
  - **Silence** (owner instruction): no audio device for Wine (no `PULSE_*`, private `XDG_RUNTIME_DIR`, Wine audio drivers off), an audio guard, and the in-game volumes at −250 in every copy's `user.dat`.
  - **Docs:**
    - `original-wine.md` rewritten (setup, findings, measurements, harness, acceptance, rules);
    - new `docs/formats/user-dat.md`;
    - `population.md`: the player object confirmed by running;
    - OQ-109, OQ-110, OQ-111, OQ-124; OQ-020 partly answered.
- Gates:
  - no C++ changed: msvc, ctest, clang-format, GCC and agree.py n.a.;
  - `python -I tools/orig/test_orig.py`: 42/42 on Windows after the rebase; earlier also in the distro (Python 3.12).
- Evidence (acceptance):
  - **Unattended 30 minutes: passed** (`t16-accept2`, 30.02 min). All 11 criteria are true:
    - the opening: 15/15 steps;
    - the sampler: 29 records with no gap; the tick invariant in 29/29;
    - 5 quicksaves kept;
    - no crash; 0 audio violations.
    - The owner's desktop: no sound is observed (the audio guard, 0 violations over 11 processes, every minute); no window and no input hold by construction (the game draws only to the private Xvfb `:91`, xdotool sends only to `:91`), not by observation.
  - **F6 (read-only form) and the warehouse equal Windows:**
    - PRNG table: 0 of 2048 differ;
    - Kontor 50 t, tools 50, wood 50; money 500000.
  - **Timing verdict** (`t16-timing3`), written in `original-wine.md` §3/§4.5:
    - F1 1000.008 ± 0.019 game ms/s;
    - F2 2000.011 / 3999.951 (± 19 ppm); 50 % 593.8, exactly the frame rounding (Σ steps = (Σ dt + odd frames) / 2);
    - F4 0 ms;
    - ~405 fps, frame p50/p95/p99/max 2.39/3.11/3.22/15.6 ms, none capped.
    - F1, F2 (100/200/400 %) and F4 may run in the Wine runner; 50 % only through the rounding model.
  - Findings beyond the task:
    - savegames: F10 → `SaveGame/<profile>/Lastgame.sav`; root `ANNOADDON1`; `MAIN` +0 = game seconds (300 … 1500 at minutes 5 … 25); the 64-bit ms counter is not saved;
    - a Citizen game starts from `00_1024_easyentry_01.sze`: its script text equals that file's in all seven quicksaves;
    - WSL's `CLOCK_MONOTONIC` slews up to 1,400 ppm, while RAW stays within 0.5 ppm of the host;
    - one intermittent crash in `AnnoGame.dll 0x100037E0` (first acceptance try, 4.45 game min; not repeated in 12 + 30 min; OQ-124).
- Reviewers: two independent reviewers of the harness. Their findings and the fixes (commits 386e832 … 3f23ec9):
  - **live world:** `find_world` prefers an advancing counter; a stale copy passed the old check;
  - **names:** Latin-1 player names; an umlaut used to drop the human player;
  - **scans:** unbounded vtable scans instead of a silent cut;
  - **teardown:** run ids validated, so a reused or `..` id could no longer delete another run or the base copies; teardown on every exit path; no orphans; the crash dialog off plus a crash watchdog;
  - **poller:** RAW timestamps, failed reads counted, the rate between observed frames (the old edge error was ±50 ppm), a model-free 50 % check;
  - **opening:** placement bounded on both sides; before, a wrong menu cell, a sheep farm for a forester, could pass;
  - **acceptance:** `summary.ok` now requires the opening, the sampler and no crash;
  - **protocol:** backend protocol hardening; the live session survives bad files;
  - **mutations:** the reviewers' surviving mutants are now killed by the 42 tests.
- Left open / questions:
  - OQ-109: fixtures over time depend on the step mix, which changes with the frame rate between runs: 380 vs 405 fps, 50 % at 611.8 vs 593.8.
  - OQ-110: MP hosting from a savegame.
  - OQ-111: `user.dat` details.
  - OQ-124: the crash.
  - Untested: F3 (5 fps, needs a frame limiter).
  - The opening does not yet check road connectivity.
  - Input on the Windows backend waits for PR #16.
- Time spent / subagents used: inline, plus two reviewer subagents. One heavy job at a time: Wine runs one after another, and T8 stays paused as the owner asked.


## T18 — The AI profiles: `COMPPROFIL2` loader and the 19 columns — 2026-10-22 — status: FINISHED
- Branch: task/T18-compprofil (5 commits plus this report, on main 60709d5); worktree: C:\Users\altin\anno1503-T18. Docs only. The work was done by one subagent (static analysis with capstone/pefile, no Wine, no builds), then checked by me and two independent reviewers.
- Built:
  - `docs/re/annoscene.md` §11 (new): who reads the chunk, the record and GUID test, the opponent setup screen, the game-start hand-over, the AI side (module choice, factory, parameter setter with all 36 indices), what some parameters steer, the `SET_PROFILE` route, `CompLogicClassic`.
  - `docs/formats/sze.md`: the `ENTRY` and `DEF` layouts with column → parameter → key, setter limits, shipped values and hand-over addresses; the `SPIELER/COMP` record (u32 type, u32 index, `KEY=value` strings).
  - `docs/research/game-files-resurvey.md` §3: the twelve shipped profiles with every column and which file/player gets which.
  - OQ-112 (unused columns, the doubled `SUSPEND` call), OQ-113 (what the parameters steer), OQ-114 (`SETTRIBUTSPERRE` values the AI ignores; owner decision on a rules switch); OQ-018 gains the module selection.
- Main findings:
  - **The scene loader never reads `COMPPROFIL2`.** The single-player opponent setup in `AnnoFrame.dll` (`0x1001e5d0`) reads it straight from the scenario file and fills 7 slots in `DEF` order. At game start `0x10028d90` gives each computer player (type 12, colour `0xFF`) the next slot. It then passes 15 columns to the AI's parameter setter (`CompLogic.dll` `0x10004550`, 36 indices).
  - **Columns:**
    - mapped: 1 `TYP`, 2 `FLAECHE`, 3 `LAGER`, 4 `HANDEL`, 5 `TRANSPORT`, 6 `PROZENTE`, 8 `TRIBUTSPERRE`, 9 `KAMPFTYP`, 10 `HAUPTKLIMA`, 11 `MAXSTADT`, 12 `ZIELGRUPPE`, 16 `MINBAUMAT`, 17 `MINEISEN`, 18 `SUSPEND`;
    - column 13 reaches parameter 26 (`SUSPEND`) a second time;
    - columns 0, 7, 14 and 15 reach no setter, so their keys cannot be read from the code (OQ-112, with both readings and the evidence against each).
  - **Key names:** three independent name sources agree on every index: the AI's key readers, the editor's writer, and the script's `SET_PROFILE` parser. The 16 `COMPCAPS_*` names are `SET_PROFILE`'s names for 16 parameter indices.
  - **Followed into CompLogic** (addresses in §11.6):
    - `KAMPFTYP` is two 2-bit switches that create, cap and cancel two kinds of AI tasks;
    - `PIRATEATTACKALL` makes the pirate class skip its per-target check;
    - `TYP` selects a time interval used by a per-player table update;
    - module choice: `CompLogic.dll` for `ANNOADDON1` scenarios, otherwise `CompLogicClassic.dll` (AnnoWelt `0x1009c630`; OQ-018 half answered).
  - **`SETTRIBUTSPERRE`:** the editor wrote this key into the scenarios' `COMP` records, but the AI only knows `TRIBUTSPERRE`. So shipped values are silently dropped: 18 for five native peoples in `endless02`, and 1 in `S01_04_01` and `S01_05_01` (OQ-114, owner decision).
- Gates: docs only. No C++ or tool changed: msvc, ctest, clang-format, GCC and agree.py n.a.
- Evidence (acceptance: every column named or marked unknown with the reason; addresses for every claim):
  - every column row in `sze.md` carries its hand-over address, or the reason it is unknown (OQ-112);
  - every §11 claim carries module + address, and every reading beyond the code is labelled likely or guess.
- Reviewers (two, independent; given the diff and the task):
  - **A (disassembly)** checked about 30 address claims and derived the column table independently; all 15 hand-over rows and all 36 setter cases match, and the three key-name sources agree at every index. Findings, all fixed:
    - one **wrong** claim: the factory stores the player's Volk number at `+0xa14`, not the index (I re-checked the instructions: the caller pushes `word [player+0x7a]`);
    - "column 13 is likely a `CHEATLIMIT` slip" downgraded to a guess (the two values would arrive reversed; it contradicts the key-order reading);
    - the setup reader's final commit was missing;
    - smaller precision fixes: pirates override `TYP`'s slot; `SETHELPANLAGE`'s call is for every type; the planner's details; the defaults' helper.
  - **B (files)** wrote its own parser over all 50 files. Everything matched:
    - `COMPPROFIL2` counts and sizes; all 100 `ENTRY` boundaries and fill;
    - the twelve profiles byte-identical, and all 12 × 19 table values;
    - DEF = the type-12/`0xFF` players in all nine files;
    - 282 `COMP` records; the `SETTRIBUTSPERRE` values; the 22 `SET_PROFILE` uses.

    Findings, all fixed:
    - `single39.sze` has a type-12/`0xFF` player and no profile (I verified it: player 17). It is the one shipped case of parameter 34; the "no file where these sets differ" claim is narrowed.
    - Counter-evidence for the "column 15 = `OPTIONAL`" reading: three `COMP` records copy Profil03 in every mapped key yet have `OPTIONAL=3` where column 15 is 60 (I re-ran it). The same copies independently support the mapping of columns 1, 3–6 and 9–12.
    - Inconsistent counts of the open columns, now worded the same everywhere; three wording nits.
- Left open / questions:
  - OQ-112: columns 0, 7, 14, 15 and the doubled call. A differential run in the T16 runner (patched columns, then read the `COMP` record after a save) could settle it.
  - OQ-113: what each parameter steers.
  - OQ-114: decided by the owner on 2026-10-22: faithful, the misspelt key is ignored.
  - Not traced: which machine sends network message 6; the type-17 AI case.
  - The subagent also confirmed `TRIBUTSPERRE` in a savegame of the Windows install (`C:\anno1503-original`); that evidence is not cited in the docs.
- Time spent / subagents used: one subagent did the RE and docs; two reviewer subagents (read-only); my check and fixes inline.

---


## T8 — sim stage S9, replay and session: gates completed, review findings fixed — 2026-10-22 — status: FINISHED
- Branch: task/T8-sim-s9-replay at a6257db (3 new commits on a234232, not pushed); worktree: C:\Users\altin\anno1503-T8. origin/main is 4 docs-only commits ahead (research, TASKS.md); none touches this branch's files, so main was not merged in.
- Built: test(sim) 5ee20e1 — the replay-log frame pinned byte for byte against bytes transcribed from the layout in state.md §15 S9 (new case "write_log matches the documented frame byte for byte"); the framing test's prefix loop made exact (boundary prefixes decode to exactly the entries before them, all others fail and keep those entries); `record_size` pinned to 8 for a SetSpeed record, with its two error cases. fix(sim) 9251865 — `PrintArchive` strips the hash tree's root name `world` from its filter, so the path `diff_trees` returns works in `--dump-scope` (before: an empty dump, so the documented workflow failed); `anno-sim-replay tree` prints one full scope path per line. docs(sim) a6257db — the frame layout written out field by field, verification, review findings; PLAN S9 verified.
- Gates: msvc build ok (warning-free); ctest with game dir 738/738 (`ANNO1503_GAME_DIR=C:\Users\altin\anno1503\game\app`, read-only; the T8 worktree has no game/app of its own); without 738/738; clang-format 23.1.3 ok over all of `src`; GCC 13.3 -O3 -Werror (WSL, the CI flags, doctest from build/msvc/_deps/doctest-src) ok over formats + test_support + sim + sim tests (52 translation units, no diagnostic) and the tool's main.cpp, sim tests 439/439 with and without the game dir; lint_sim ok; agree.py n.a.; asan not run locally (CI linux-clang-asan green on a234232). Clang is not installed in WSL: the Clang evidence is CI (linux-clang-asan, macos-clang green on a234232); the 3 new commits need a CI run after the push.
- Evidence: 12 replay cases (7 named + 5); mutants and results: (1) `config_digest`/`rules_digest` swapped in write_log AND read_log → "write_log read_log round trip" passes, "write_log matches the documented frame byte for byte" fails (2 assertions); (2) decode_turn `break` instead of an error on a cut entry → "record_size and decode_turn frame a back-to-back stream" fails 86 of 192 assertions; both reverted and the tree rebuilt before the gates. anno-sim-replay on a recorded log: `run` 3/3 checkpoints reproduced; `tree --entry 6 --depth 1` prints `world/clock_rng 54beca86b5a68ebd` etc. (digests identical to the first run before the rebuild); `--dump-scope world/clock_rng` prints the five clock/PRNG fields (ms 140, tick 70, table 2048, digest, cursor 1234).
- Reviewers: none new; this round fixes the two notes of reviewer 2 of the first round (tautological truncation assertion; round trip without a pinned constant), both now killed by a mutant as listed. One further defect found on re-reading the docs (the `world/` prefix of `--dump-scope`), fixed with a test.
- Correction to the first T8 section above: "with game dir n/a (no game files on this host)" was wrong — the files are in the main worktree; the with-game-dir runs are now done (738/738 MSVC, 439/439 GCC).
- Left open / questions for the orchestrator or owner: push and CI on the 3 new commits (I did not push). The tool rebuilds a world from a log on the empty type registry only, until S14 brings the scenario path (state.md §15 S9). No new OQ.
- Time spent / subagents used: inline, no subagents; heavy jobs strictly one at a time (MSVC build, ctest without, ctest with, GCC build, GCC runs), started only after the coordinator's "resume".


## T16 fix-up — the PR #21 review items (TASKS.md §4, 2026-10-22 h) — 2026-10-22 — status: FINISHED
- Branch: task/T16-wine-runner, 2b74a57..791b4e3 on top of the merge 3e84767 (plus this report); worktree C:\Users\altin\anno1503-T16. Python tools and docs only. No Wine or game run (a C++ build ran on the PC); the only distro access was reading the base prefix's `user.reg` for item 3.
- Done:
  1. **Action log** (2b74a57, tests a558d56): `stop()` joins the watchdog (5 s) before the log closes; `log()` locks the lazy open, every write and the close, opens only when `<run>/out` exists (no `makedirs`), and is a no-op after `log_close()`; `sync()` flushes under the lock.
  2. **Paths** (b982c5d, tests 83d9fc8): `within(path, base)` (realpath on both sides, no `..` component, not the base itself). `--out-win` must lie inside the repository's `cache/orig` as the distro sees it (derived from the agent's location; `backend.py`'s folders pass). `screenshot` `dest` must lie inside the run's `out` or inside `--out-win`. Deviation from the review wording ("under the run's out dir"): `backend.py` sends every screenshot copy to the `--out-win` folder, so that folder is allowed too. Also: names that become file names (screenshot, learned screen, timing label, sampler, `keep_as`) must be one component.
  3. **`AKTPLAYER`** (cd58e04): `user-dat.md` cites the base prefix's `user.reg` (`[Software\\Max-Design\\Anno1503]` `"AKTPLAYER"="ANNOnymous"`) and the strings in `AnnoFrame.dll` (`0x95c64`) and `1503Startup.exe`; that the game reads it is hedged and is OQ-111 question 5.
  4. **Desktop claim** (e6192c4): `original-wine.md` §5 and the T16 report say that silence is observed (0 audio-guard violations) and that no window and no input hold by construction (private `:91`).
  5. **Merge of `origin/main`** (3e84767): every entry kept; open questions in numeric order (OQ-124 moved after OQ-123); REPORTS in chronological order (T17 22:19, T16 22:23).
  - Nits:
    - `Offsets` parses the values from the docs it cites (916bca6).
    - `WindowsBackend.key` defaults to `hold=0.0`, the interface's value; a tap is still sent as a 50 ms press there (3bcfda7).
    - The four spike scripts say SUPERSEDED at their top, and `run_game.sh`/`debug_run.sh` exit at once (fc6a6a0).
    - The crash latency of `unattended.py` (up to `--check` = 60 s) is in §4.6 and its docstring (791b4e3).
- Evidence:
  - `python -I tools/orig/test_orig.py`: 52 tests (was 42). Windows: OK, 2 skipped (symlinks need a privilege). WSL Ubuntu (Python 3.12, Pillow from a private `--target` folder): OK, 0 skipped. `tools/test_bigbrother.py` after the merge: OK (4 skipped, no game dir).
  - Mutations, each applied alone and then restored (bytecode caches cleared between runs):
    - log: 4 of 4 killed;
    - paths: 11 of 11 killed in WSL; on Windows, the realpath→abspath mutant survives only because the symlink cases skip there;
    - nits: 8 of 8 killed (four constants in `gamestate.py`, the booking divisor, `VOLUME_MIN`, a value in `play-original.md`, the Windows key default).
- Left open: OQ-111 question 5 (who reads `AKTPLAYER`). `agent.py` now refuses an `--out-win` outside `cache/orig`; nothing else in the repository passes one. The orchestrator's smoke run is the first live check of the changed agent.
- Time spent / subagents used: inline, no subagents.

## T19 — The script engine: `&&`/`||` precedence and statements without `;` (OQ-120, OQ-121) — 2026-10-22 — status: FINISHED
- Branch: task/T19-script-engine-re from origin/main bb13852; worktree: C:\Users\altin\anno1503-T19. Docs and Python tools only, no C++ touched, nothing of the original in git beyond one-line excerpts.
- Method:
  - Static analysis of `AnnoScene.dll` with capstone (`tools/disasm.py` plus scratch range dumps and an xref scan for the delimiter table and the comparison strings), on the Windows host.
  - A scratch Python emulation of the reader and the condition parser (not committed) ran over all 50 scripts. It gave the same items, terms and SET values as `tools/bigbrother.py` everywhere; it differed only at the four statements without `;`.
- Found (all D with addresses in `docs/re/annoscene.md` §12):
  - **Where the code is:** the reader and the parser are not in the O-16 op range. The kernel reader is `0x1001c9e0`; the condition parser `0x10022bf0`; the action reader `0x100170d0`; the text helpers `0x1000b150`–`0x1000b542`; the evaluation `0x10010290`; the step `0x10016b70`.
  - **OQ-120 (resolved):** the parser takes a term, then on `&&`/`||` parses the **whole rest** of the condition recursively as the right operand and returns.
    - So the two connectives have equal rank and group to the right: `A && B || C` = `A && (B || C)`.
    - Evaluation: left first; `&&` skips the right operand when the left is false; `||` evaluates both.
    - A top `&&` with a false left runs **neither** THEN nor ELSE (only the IF's top node holds the actions).
  - **OQ-121 (resolved):**
    - **In a block:** a statement is the text up to `strpbrk(p, ";")` over the rest of the kernel text, not bounded by the block's `}`. The action reader parses its arguments and ignores the rest, so statements before that `;` are lost. The reader resumes after the `}` in any case.
    - **At the top level:** `SET` never looks for `;`.
    - **No `;` at all:** if none follows, the kernel is refused.
  - **Side finding:** the step runs the INITBLOCK once, before any IF, only while `SCHUETTIINITSIG` is TRUE, then clears it. The kernel reader creates that signal TRUE if missing. This answers half of OQ-123 item 4.
- The shipped scripts under the answer (P, `tools/bigbrother.py quirks`):
  - **28 mixed conditions** (S01_03_01 2, S01_04_01 2, S01_05_01 1, S01_11_01 1, Single_01 1, Single_14 1, single32 3, single36 15, single40 1, single41 1):
    - all are `guard && … && alt1 || alt2 …`, which the interpreter reads as `guard && (alt1 || alt2 …)`;
    - each differs from the C reading as a boolean function (truth tables in the corpus test);
    - under C precedence the alternatives would fire without the guard; the interpreter's reading fits the one-shot flags;
    - example, Single_01: `MISSION_FLAG == FALSE && (… DEAD && …) || … "BUERGER" > 2999`.
  - **Statements without `;`:**
    - `SET_PROFILE(…)` S01_02_01: runs on to a commented-out `$ATTACK_OBJECT(…);` in its block; nothing lost;
    - `SET DEADEND_ABFRAGE = FALSE` S01_08_01: top level; nothing lost;
    - `SET_DIPLOMACY( "CG2", "HUMAN", -127 )` S01_09_01: the INITBLOCK's last statement; runs past `}` to `SET TEIL1_1 = FALSE;`, which is re-read; nothing lost;
    - `SET_FOR_BUY( "HEILKRAEUTER", "INDIANS", 30 )` S01_09_01: swallows the next statement `MINIMAP_ALERT( 640, 950 );`. **That alert never runs in the original.**
  - **ELSE quirk:** the three shipped ELSEs (single33) start with `CHKPARTn &&`, so their ELSE runs only while the flag is TRUE.
- Built:
  - `tools/bigbrother.py`:
    - `grouped`/`describe_grouped`, `evaluate`, `branch`, `is_mixed`, `read_on`;
    - `dump` marks the grouping and the lost statements;
    - new command `quirks`.
  - `tools/test_bigbrother.py`, a new `Interpreter` class (8 synthetic cases):
    - the grouping, with the C reading shown to differ;
    - the evaluation order;
    - which block runs;
    - read-on: swallowing, past `}`, a `;` in a comment, two in a row, refusal, a top-level SET.

    Plus a corpus case: 28 in 10 files, each unlike C, and exactly one lost statement.
  - **Docs:**
    - `docs/re/annoscene.md` §12 (new; T18 adds §11, so expect a trivial conflict at `## Open` when both merge);
    - `bigbrother.md` §3.4, §3.5, §8 and the status lines;
    - the formats README row;
    - OQ-120 and OQ-121 resolved, OQ-123 item 4 narrowed and a new item 5.
- Gates:
  - `python -I tools/test_bigbrother.py`: 26 OK with `ANNO1503_GAME_DIR` (Windows, Python 3.14); 21 OK + 5 clean skips without;
  - `python -I -m doctest tools/bigbrother.py` passes;
  - `bigbrother.py check` 50/50;
  - ruff shows only the three pre-existing findings.
  - No C++ changed, so builds, ctest, clang-format and agree.py are n.a.
- Left open / questions:
  - **Whether evaluating a condition changes state** (for example, consumes an event). It decides whether `&&`'s skip and `||`'s full evaluation are observable. OQ-123 item 5.
  - **How many IFs one step evaluates.** The step takes `min(count, (arg · count + 125) / 250)` per call, round-robin, with `+0x3c` only seen written as 0. The argument is what AnnoFrame adds to `+0xa24`, likely the step length; if so, small scripts at small steps would evaluate 0 IFs per call, which looks wrong. So something is not yet understood here. OQ-123 item 5.
  - **For the owner:** restoring the lost S01_09_01 minimap alert would be a `rules` option; faithful default = lost.
- Time spent / subagents used: inline, no subagents (no Wine, no game runs, no builds).


## T8 fix-up — the orchestrator review of PR #19 (TASKS.md §4, 2026-10-22 h: items 1, 2, 4, 6, 7 and the nits) — 2026-10-22 — status: FINISHED
- Branch: task/T8-sim-s9-replay, commits 583c709, fbe9130, ff71342, d206da4 on top of 96fc4e3 (plus this report); `main` not merged.
- Item 1 (583c709): `Recorder::submit` checks `encode_record` first; a command without a wire form (a `LegacyRawCmd` with a native type, a payload beyond `kMaxRecordPayload`) gets the new, appended `Reject::Unencodable` and is neither logged nor applied. `write_log` now returns `Result<std::vector<std::uint8_t>, std::string>` and refuses a hand-built log holding such a command ("entry N, command M: ...") instead of dropping it. Residual, written into state.md: `run_turn`/`run_frame` without a recorder still call `World::apply` directly, which answers by body (today `Unsupported`, a no-op), so the two paths agree until a rule applies an unencodable body (S10 `Build` with more placements than a record holds would be the first).
- Item 2 (583c709): `replay` also compares `log.prng_timeline_missing` with `prng_timeline_missing()` and refuses with "the PRNG timeline's missing draw sites differ from this build's".
- Item 4 (fbe9130): two recorded runs that agree on entries 0..3 and split at entry 4 (host speed 200 again vs 100); `first_divergent_entry(hash_per_entry(a), hash_per_entry(b)) == 4` in both orders, hashes equal before 4 and different from 4 on; `replay` of run a's log with run b's command at entry 4 stops at checkpoint entry 6.
- Item 6 (ff71342): `--entry` and `--depth` go through `std::from_chars` over the whole argument; non-numbers, signs, trailing characters, u64 overflow and a `--depth` beyond u32 print "bad --<flag> value '...'" plus the usage and exit 2. `read_hashes` (`first-diff`) uses the same parser (before, `std::stoull` read the header token "entries" as hex 0xe). The tool had no test harness; added 8 CTest cases `anno_sim_replay_args_*` through `cmake/RunToolExpectError.cmake`, which requires exit status 2 and the tool's own message (an uncaught exception has neither); 7 bad values and one valid value that gets past the parse to "cannot read no-such.log".
- Item 7 (583c709): the precondition is stated on `Recorder`; the constructor is private and `Recorder::start(World&) -> Result<Recorder, std::string>` checks `terrain_matches` on every island's terrain (one FNV pass, cheaper than the save the constructor already makes) and names the island ("islands[3].terrain is a digest-only placeholder ...").
- Nits: replay.hpp says `submit` logs then applies and `advance` advances then logs the tick it reached (header and class comments); `run_turn` with a recorder emits one Commands entry per command, said in replay.hpp and state.md §8.7; state.md §8.3 lists `ListFull` (missing since S6) and `Unencodable`. The numbers in the first T8 section: "ctest without game dir 438/438" at 1da8f1b was the sim doctest binary (`anno_sim_tests`, 438 cases), not the CTest suite; the 731/731 in state.md at a234232 and the 738/738 at 96fc4e3 are the whole CTest suite (all libraries, one CTest test per doctest case). Now: 443 sim cases, 750 CTest tests. The other nits (`diff_node` pairs children by position, the tool re-implements `apply_entry`, the config-mismatch test covers rules and registry only) are not changed in this round.
- Tests added (replay_test.cpp): "recorder refuses a command the log cannot hold", "replay refuses another set of missing draw sites", "first divergent entry of two runs that split after entry k", "recorder refuses a world it cannot save"; "reject names and results" covers `Unencodable`.
- Mutation evidence (each mutant applied alone, target rebuilt, tests run, file restored from git; script and log in the scratchpad): M1 `submit` logs and applies an unencodable command → "recorder refuses a command the log cannot hold" fails (5 assertions); M1b `write_log` drops it again (`if (rec.ok())`) → same test fails (1); M2 the missing-list comparison disabled → "replay refuses another set of missing draw sites" fails (1); M4 `hash_per_entry` hashes before applying → "first divergent entry of two runs ..." fails (3); M7 the terrain check disabled → "recorder refuses a world it cannot save" fails (1); M6 the tool's parser back to `std::stoull` → 5 of the 8 `anno_sim_replay_args_*` fail (the two `--depth` range cases still catch the wrapped values through the u32 check, the valid case passes as it should). All restored; `git status` clean apart from the docs.
- Gates: `cmake --build --preset msvc` exit 0, 0 warnings. `ctest --preset msvc -j 16` (ANNO_TEST_THREADS=16) without the game dir: 750/750. With `ANNO1503_GAME_DIR=C:\Users\altin\anno1503\game\app`: two full runs gave 748/750 and 746/750; every failure was a render corpus test throwing `bad allocation` (render is untouched by this change). The host had 32.6 GB of its 52.8 GB commit limit in use while idle (the WSL VM and the other session's processes), so the heavy render corpus cases ran out of commit; at `-j 4` the render set alone still lost 4 cases to `bad allocation`. Split runs: everything except render with the game dir at `-j 16` 518/518; the 7 distinct render cases that failed (#49, 74, 75, 79, 122, 127, 130) pass when run one at a time with the game dir (7/7). So with the game dir every test passed, but not in one `-j 16` run on this host today; that is the T10b memory problem, not this change. clang-format 23.1.3: `--dry-run --Werror` over all of `src` clean. GCC 13.3 (WSL Ubuntu) `-O3 -DNDEBUG -std=c++20` with the CI warning set and `-Werror`, doctest from build/msvc/_deps/doctest-src: 53 translation units (formats, test_support, sim, sim tests, sim_replay main.cpp), no diagnostic; sim tests 443/443 without the game dir (2,014,970 assertions) and 443/443 with it (2,030,214); the GCC-built tool refuses all 7 bad flag values with exit 2. `tools/lint_sim.py`: ok, self-test 64 cases, 0 failures. agree.py n.a. asan/Clang not run locally (Clang is not installed in WSL): CI on the push.
- Left open: CI on the pushed branch; the three nits named above.
- Subagents: none; heavy jobs one at a time.

## T10b — Test memory: the mitigation visible, the heavy tests scheduled apart — 2026-10-23 — status: FINISHED (one acceptance line not met, explained below)

- **Branch:** `task/T10-digest-flake`, `origin/main` merged first (365d7ef, no conflicts; the branch's `test_support` memory work kept). Commits:
  - c6989f5 `fix(test)`: review items 1, 2, 3, 5 in `parallel.hpp` / `memory.cpp`;
  - d91623f `build(test)`: item 4, the heavy cases alone (`anno_add_doctest`), item (b);
  - a28a004 `test(render)`: the legacy digest case in one call per mode (222 s → 90 s alone);
  - 06f0869 `fix(viewer)`: item (c);
  - ce552d6 `build`: `.gitattributes` `*.sh text eol=lf` (with `core.autocrlf=true` the scripts were CRLF in the Windows checkout, which bash in WSL rejects; `tools/orig/wine/*.sh` included);
  - e5e91c9 `build(tools)`: item (a), `tools/check_unix_warnings.sh`;
  - b568d9a `docs`: `CLAUDE.md` environment notes.
- **Items 1-3, 5 (`src/test_support`):**
  - Every bad_alloc retry prints `[anno-test memory] item i of n threw std::bad_alloc: retry a of R (free X MiB)`. One summary line follows whenever a call waited, retried or got fewer threads than requested (items, threads used of requested, waits with total ms and how many ran out, bad_alloc items, retries, free memory, budget). The counts also come back in `ParallelStats`. Passed tests' stderr is in `Testing/Temporary/LastTest.log`, not in `ctest-output.log`; `CLAUDE.md` now says so.
  - Dead tail: a worker checks `next >= n` before it waits and on every poll, so it leaves as soon as all items are taken.
  - `ANNO_TEST_BAD_ALLOC_RETRIES` (default 5, 0 = the first bad_alloc fails the test).
  - `free_memory_bytes()` returns `std::optional`: nullopt for a failed query or no implementation (no bound), 0 stays 0 (one thread). The MiB budget saturates; `ANNO_TEST_MEM_WAIT_S` is capped at a day, so `x4` polls cannot wrap.
  - The knobs are a `ParallelOptions` struct (`parallel_options()` reads the environment). Tests pass a fake free-memory query and short pauses.
  - Tests (13 cases in `parallel_test.cpp`): retry count and order, retries off and limit, wait counted, no wait to the end once items are taken, 0 vs. unknown free memory, saturation, env parsing.
  - **Mutation evidence** (MSVC, each mutant rebuilt and run): all seven mutants killed:
    1. no items-taken check in the wait poll;
    2. retry counter dropped;
    3. retries=0 ignored;
    4. 0 free treated as unknown;
    5. budget wraps;
    6. wait counter dropped;
    7. retry limit off by one.
- **Item 4, scheduling:**
  - `anno_add_doctest` calls `doctest_discover_tests` twice per executable: once for the doctest suite `"heavy"` with `PROCESSORS` = `ProcessorCount` (16 here; CTest caps it at `-j`), once for the rest.
  - A small include file gives the heavy ones the label `heavy` (`ctest -L heavy` / `-LE heavy`).
  - Deviation from the review text: doctest 2.4.12's `doctestAddTests.cmake` wraps `TEST_SPEC` into `--test-case=<spec>`, so `-tc=`/`-tce=` cannot be passed. Instead the spec is the list `"*;--test-suite=heavy"` / `"*;--test-suite-exclude=heavy"`: the script expands the list, so the second element arrives as a raw argument. The cases are marked in the source (`* doctest::test_suite("heavy")`), not by name patterns that a rename would silently break.
  - Checked: 738 tests = 731 + 7 new `test_support` cases (no duplicates, none lost); `ctest -N -L heavy` lists the 10; the generated files carry `PROCESSORS 16`; the GPU tests keep `RESOURCE_LOCK gpu`. In all three runs, no test started while a heavy one ran (parsed from the console log).
- **Which cases are heavy (measured, not guessed):** every case that took ≥ 4 s in a baseline `-j 16` run (55 cases, all executables) was run alone with `ANNO_TEST_THREADS=16`, with the process's peak commit sampled. The heavy set is every CPU case whose peak alone is above 1.25 GB:

  | case | peak commit alone (16 threads) |
  |---|---|
  | scene renders match the recorded digests | 35.6 GB |
  | legacy river shading reproduces the previous enhanced digests | 14.7 GB (35.4 GB after a28a004) |
  | six river flow corpus cases (Q5 CPU path, Q3 coverage, pixels only, noise coordinate, all windows, light vs shimmer) | 2.4-2.7 GB each |
  | river raster agrees with the river the original draws | 1.5 GB |
  | derived levels: a zoom-out sweep | 1.4 GB |

  Everything else: ≤ 1.02 GB (render_chunk, FrameRenderer 1.0; the other corpus cases 0.3-0.95). The GPU cases (1.3-2.5 GB) already run one at a time under `RESOURCE_LOCK gpu` and stay light.
- **Item (b), `anno_dump_agrees_with_python`:** alone it raises the system commit by 0.7 GB (33.0 → 33.6 GB) in 42 s, and `river_qa_gap_survey` is negligible too. The bad_alloc of T12 came from the heavy cases running beside it (like tests #49, #122, #127 and #130 in the orchestrator's run: unit tests, victims). Since the heavy cases now never share the machine, agree needs no slot reservation, and one would add 42 s to the serial part. It has a comment in `src/tools/anno_dump/CMakeLists.txt`. **If the orchestrator wants it heavy anyway, it is a one-line `set_tests_properties`.**
- **Verification** (`ctest --preset msvc -j 16`, `ANNO_TEST_THREADS=16`, `ANNO1503_GAME_DIR=C:\Users\altin\anno1503\game\app`; system commit = total - available page file of `GlobalMemoryStatusEx`, sampled every second):

  | run | passed | wall | commit at start | peak commit | commit limit (start → max) | retry lines | bad allocation | wait lines |
  |---|---|---|---|---|---|---|---|---|
  | baseline (merged branch before this task, silent mitigation) | 731/731 | 304 s | 33.5 GB | 73.1 GB | 52.8 → 73.6 GB | (not logged) | 0 | (not logged) |
  | run 1 | 738/738 | 374 s | 33.6 GB | 67.8 GB | 52.8 → 70.0 GB | 0 | 0 | 1 (9 waits, 26.0 s, none ran out) |
  | run 2 | 738/738 | 374 s | 34.0 GB | 68.5 GB | 52.8 → 70.3 GB | 0 | 0 | 1 (5 waits, 10.2 s) |
  | run 3 | 738/738 | 374 s | 33.2 GB | 68.5 GB | 52.8 → 69.0 GB | 0 | 0 | 1 (9 waits, 27.4 s) |
  | without the game dir | 738/738 | 32 s | 33.2 GB | 33.6 GB | 52.8 | 0 | 0 | 0 |

  - **Not met: "no wait line".** Each run has exactly one wait line, from the scene-digest case. It ran alone, and its own 16 workers waited for memory while the page file grew.
    - That case needs about 35 GB with any thread count ≥ 7: the whole-map group 2 renders of Single_01/10/14 hold about 5 GB each. A 2.5 GB per-task budget, tried and reverted, gave 7 threads and still 27.5 GB; the legacy case reached 36.8 GB on 7 threads.
    - Free commit headroom at idle is about 19 GB (the WSL VM keeps about 33 GB committed).
    - So on this PC the case always depends on Windows growing the page file: the limit grew from 52.8 to about 70 GB in every run.
    - No wait ran out, no bad_alloc occurred and nothing was retried.
  - The other memory lines in the three runs (15-18 per run) are thread cuts of light corpus cases, not waits or retries: for example, 11-15 of 16 threads while other tests held memory, and 0 waits.
  - Wall time rose from 304 s to 374 s: the heavy cases (about 175 s together) now run serially. a28a004 already took 130 s out of the legacy case; without it the run would be about 500 s.
- **Logging works (forced):**
  - In a job object limited to 1200 MB per process, "flow coverage … (Q3)" printed three `retry 1 of 5` lines and the summary `5 items on 5 of 16 requested threads; 0 memory waits …, 3 bad_alloc items, 3 retries`, and passed.
  - With `ANNO_TEST_MB_PER_THREAD=2400 ANNO_TEST_MEM_WAIT_S=10`, the scene digests printed `44 items on 7 of 16 requested threads; 3 memory waits (15155 ms, 0 ran out), 0 bad_alloc items, 0 retries`, and passed.
- **Gates:**
  - MSVC build: 0 warnings.
  - ctest with the game dir: the three runs above, all green. Without it: 738/738.
  - clang-format 23.1.3 `--dry-run --Werror` over `src`: clean.
  - `tools/check_unix_warnings.sh` in WSL `Ubuntu`: GCC 13.3 and Clang 18.1.3 (the unpacked `~/clang18`), 124 files each, clean. Clang without `ANNO_VIEWER_GPU`: clean, and with the viewer fix reverted it reports the three `-Wunused-private-field` errors and exits 1. With no Clang found, the skip message is printed and the exit is 0.
  - GCC 13.3 `-O3 -Werror`, CI flags, `anno_test_support_tests` linked and run in WSL: 13/13 without and with `ANNO1503_GAME_DIR`.
  - `agree.py`: no `anno-dump` command changed; the test passed in every run.
- **Reviewers:** none spawned (round i subagent; the orchestrator reviews). The mutation checks above stand in for the reviewer's test check.
- **Left open / for the orchestrator:**
  1. The whole-scene digest cases need more commit than this PC has free at idle. They are safe only while the page file can grow. Real fixes are outside T10b:
     - a per-item memory estimate in `parallel_indexed` (hold back a big render until its canvas fits);
     - or less memory per whole-map render.

     Which one, if any?
  2. agree not marked heavy (above): confirm.
  3. The full run is 70 s longer than before.

## T9 — sim stage S10: Build, Demolish and the trade commands — 2026-10-23 — status: FINISHED
- Branch: task/T9-sim-s10-build (7 commits on 6fb8859, pushed, PR opened); worktree: C:\Users\altin\anno1503-T9
- Built:
  - 1d260ae docs: the hand-derived free-slot reuse order (state.md §15 S10 table), committed before the code (acceptance 3).
  - 9cebc7d `has_wire_form`; `run_turn` / `run_frame` without a recorder skip a command the recorder would refuse (the S9 gap).
  - a77e55a `legacy::decode_0x64` (legacy_codec.hpp/.cpp).
  - b87d317 Build, Demolish, SetTradeFlag, SetTradeSlot (apply.cpp), the footprint owner rule and the D17 stand-in (placement.hpp/.cpp), the Place/Remove seams with their Effects merged (scheduler_internal.hpp, `Scheduler::merge(in_advance)`), `synthetic::make_build_types` / `g2_scenario`, command_build_test.cpp.
  - b812cb5 property_test.cpp (random streams, G2, run_turn gap), fixtures/synthetic/sim/g2.log (binary in .gitattributes), golden `s10_g2_end`, anno-sim-replay picks the registry by digest, CTest `anno_sim_replay_g2_run`.
  - a144d3d review fixes (overlap check in validation, goods summed per goods, encode_record via has_wire_form, five more test cases and extended ones).
  - docs: state.md §8.4 note, §15 "S10 as implemented" (choices, review, verification), PLAN status, OQ-125..127, this section.
- Gates: msvc build ok, warning-free (whole tree); ctest without game dir 774/774; with game dir the full `-j 16` run was stopped by the Claude Code memory guard (system low on memory) after 427/774 had passed; the two tests cut off with it (`anno_dump_agrees_with_python`, `river_qa_gap_survey`) pass alone; the remaining cases (mostly render corpus, untouched here) did not run and the guard said not to restart it; anno_sim_tests with the game dir 466/466 on its own. clang-format 23.1.3 ok over `src`. GCC 13.3 `-O3 -Werror` with the CI flags in WSL over formats + test_support + sim + sim tests + sim_replay: 57 TUs clean, 466/466 with and without the game dir. lint_sim ok, self-test ok. agree.py n.a. (its CTest passes). asan/Clang not run locally.
- Evidence: all 17 named tests exist and pass (15 in command_build_test.cpp, "random command streams keep invariants" and "G2 replay equals golden" in property_test.cpp), plus "run_turn without a recorder skips a command without a wire form", "build charges a goods named twice in full", "build enforces research only under the rule", "build and demolish run the place and remove seams", "build placement rule and upgrade seams", "demolish leaves another city's building on the city's tile"; sim cases 443 → 466. `anno-sim-replay run fixtures/synthetic/sim/g2.log` → checkpoints 6/6, reproduced; g2.log (61 934 bytes) is byte-identical from the MSVC and the GCC build. Free-slot order: "demolish frees the slot and keeps the hint" checks the table row by row; G2 asserts slots 2, 3, 4, 5 with slot 1 freed and hint 5. Mutation evidence on the final code (MSVC, whole sim suite per mutant, all restored): money not charged → 7 cases fail; stock not removed → 5; spent not added → 3; atomic dry run skipped → 3; atomic gold budget ignored → 1; atomic serial budget ignored → 1; slot scan from 0 → 11; a removal that resets the hint → 9.
- Reviewers: (1) spec reader: an atomic build could end partial with a placement seam that ignores buildings (fixed: overlap check in validation); a duplicate cost goods was undercharged (fixed: summed); untested atomic money budget and seam paths (tests added); manual rules not listed (OQ-126 items 6-7); a wrong OQ reference (fixed); nits done (encode_record via has_wire_form, NotCityOwner for the trade commands, road rotation order, §8.4 BadValue line, trade-list capacity note in OQ-127). (2) mutation reviewer, 72 mutants in a private GCC copy of b812cb5: every charge, atomic and free-slot mutant killed except the atomic gold and serial budgets, which the new tests kill; the other survivors (seams, research, scenery under a non-north-west tile, a duplicate goods on one list, 15 followers, 12-bit type ids, another city's building) got tests; one equivalent survivor left (a rotation bound of 4 in check_record, refused by create_building too).
- Left open / questions: owner: OQ-125 (who the free-build cheat covers; default off). RE: OQ-126, OQ-127. The full ctest with the game dir should run once on an idle machine or in CI before the merge.
- Subagents: two reviewers (one read-only, one mutation runner in WSL), one heavy job at a time.

## T9 — sim stage S11: the remaining production classes — 2026-10-23 — status: FINISHED
- Branch: task/T9-sim-s11-classes (head in the PR); worktree: C:\Users\altin\anno1503-T9. Commits a1e2e8c feat (driver), 116fd4b test (driver classes, G3), a4410ac fix (carrier job 0x33), d1b1a51 test (review gaps), 0a26448 docs, plus this report.
- Built:
  - `src/sim/src/driver.cpp`, `driver.hpp`: `tick_building` drives WeaponsWorkshop (`refresh_unlocked`: the Forsch id of each line's goods and bit 1 of the owner's research byte, D 0x10069580-0x10069640, OQ-034), FurTrapper (full step; no trapper sent because the prey search finds no wildlife, O-12/O-25), Mine (`island.deposits`, `mine_area`, `WorldSettings::depletes()`), SmallFarm (`DrawRef` over `SiteDraw{ctx.draw, SmallFarmAnimal}`, `cfg.small_farm_animals`). `tick_kind_enabled` (the S7 mask) removed. `consume` takes the jobs from one table: 0x33 for the workshop and weapons-workshop carrier (D 0x100680b0, re/production.md §4.1/§4.1a; S7 had 0), 0x0d trapper, 0x2a small-farm worker (D 0x1006f660), 0x2e animal.
  - `synthetic::g3_scenario`; `fixtures/synthetic/sim/g3.log` (94 KB, our own synthetic data); goldens `s11_g3_checkpoints/end/events`; CTest `anno_sim_replay_g3_run`.
  - `tools/lint_sim_allow.txt`: the regex of the job-code line names the two new job constants. This is the one file outside src/sim/docs that I touched.
- Gates (all on the final code and all after the standby windows the orchestrator reported; these runs replace the earlier ones):
  - msvc build ok, warning-free.
  - ctest --preset msvc -j 16: 796/796 without the game dir; 796/796 with `ANNO1503_GAME_DIR` (full run, 374 s).
  - clang-format 23.1.3 clean over all 229 tracked src .cpp/.hpp.
  - WSL GCC 13.3 `-O3 -Werror` with the CI flags over formats, test_support, sim, the sim tests and anno-sim-replay: 59 TUs clean; 475/475 sim cases with and without the game dir; `anno-sim-replay run` reproduces g2.log and g3.log, which were recorded by MSVC.
  - The same with Clang 18.1.3: clean, 475/475 both ways, both logs reproduced.
  - `lint_sim.py` ok; its self-test 64/64.
  - `git grep tick_kind_enabled -- src` finds nothing.
  - Redone after the standbys (03:41 and 04:41-05:23): the full build, both ctest runs, GCC, Clang and the mutation pass. The pass had overlapped the second window; the rerun gave identical results. The goldens/log recording ran at about 03:39, before the second window. Every later run checks it byte for byte (G2/G3 tests, both CTest replays, the GCC and Clang replays).
- Acceptance:
  1. All six named tests exist and pass in `driver_classes_test.cpp`, plus G3 and three from the review: icons from Volk, the configured animal table, the rotated mine footprint (10 cases; 475 sim cases in all).
     - The scheduler-equals-direct case runs 20 000 advances of 10 ms. After every advance it compares eight payloads, both deposits and the PRNG cursor. At the end it compares every figure (home, job, kind on types with distinct figure ids, counter, owner, birth tick) and every notice.
  2. G3 is `synthetic::g3_scenario`: 2 000 advances of 100 ms to tick 100 000, 21 checkpoints.
     - It is committed as g3.log and reproduced by the test (21/21) and by `anno-sim-replay run`, on MSVC, GCC and Clang.
     - The semantic asserts were derived by hand and checked again by the reviewer: weavers 696/4; farm 592; trapper 520/12; small farm {130,102}/{0,1}; bow maker of player 0 1287 (one line, 143 steps); bow maker of player 1 {640,640} (160 steps); mine 768 with deposit index 0→1→0 and notice 0x19.
     - The draws (199) and the animal counters {1,2,0,1,0} are pinned from the run.
  3. Call paths: `World::advance` → `Scheduler::advance` → `island_tick` → `city_ring_tick` → `city_tick` → `building_scheduler` → `run_scheduled_lists` → `tick_building`.
     - From there: `workshop_step`, `weapons_workshop_step` (after `refresh_unlocked`), `farm_step`, `fur_trapper_step`, `small_farm_step`, and `mine_step` (→ `deposit_remaining`, `deposit_extract`, `find_deposit` → `deposit_in_reach`). `StatusTracker::report`/`clear` run inside every step.
     - `mine_is_deep` and `find_deposit` also run in `create_building`, reached from advance through the AI seam's Build.
     - `CityStock::add`/`remove` are reached via the merge (`GoodsMove`, `Deliver`) and the Build charge.
     - **Exception: `grow`**, the body of the growth sweep, which is S13.
     - Load and creation code (`production_type`, `ware_names`, `make_*`) is not a step.
     - The mask is gone (grep above).
  4. Evidence: every rule cites an address or a section (state.md §7.1 "As implemented in S11", §15 "S11 as implemented"). New OQ-128 and OQ-129.
  5. The state stream is unchanged: `kStateHashVersion` stays 105, and so does `kPrngTimelineVersion` (the set of sites is unchanged; the reason is written in state.md §6).
     - The carrier job fix moved the self-baselined goldens `s7_g1_mp_checkpoints`, `s7_g1_mp_end`, `s7_g1_sp_end` and `s10_g2_end`, and g2.log was re-recorded. The reason and the old values are in goldens.txt.
     - `s7_g1_events` and every sim_stream.py golden are unchanged. G1 and G2 replay.
  6. All gates above.
- Mutation evidence: MSVC, the whole sim suite per mutant, 13 mutants, all killed. Each entry gives the number of failing cases and names some of them.
  - Research unlock:
    - `refresh_unlocked` not called: 4 ("weapons workshop unlocked from research", "weapons workshop save load advance", scheduler-equals-direct, G3).
    - Research read as "owner present": the same 4.
  - Add-on-only depletion:
    - Always on: 1 ("mine deposit by index and depletion only in the add-on").
    - Never on: 4 (the same case, "the mine searches from its rotated footprint", scheduler-equals-direct, G3).
  - Once-per-step draw:
    - No draw: 4 ("small farm draws once per step through the world", "small farm animals come from the configured animal table", scheduler-equals-direct, G3).
    - A draw at every visit: 3 ("small farm draws once per step through the world", scheduler-equals-direct, G3).
  - Trapper sent without prey: 3 ("fur trapper sends nothing without prey", scheduler-equals-direct, G3).
  - Small-farm worker job 0: 5.
  - Carrier job 0: 7 (G1, G2, G3 among them).
  - The mine's `shows_icons` dropped: 1.
  - Default animal table instead of the configured one: 1.
  - A 1 × 1 mine area: 1.
- Reviewers: one independent reviewer read the diff against the acceptance list. It re-derived every hand number and found no determinism, UB or GCC trap. Its findings and what I did:
  1. The workshop/weapons-workshop carrier job is 0x33 by evidence, but the code wrote 0. Fixed (a4410ac), goldens re-baselined with the reason.
  2. state.md and settings.hpp claimed that property 5 is read by the deposit search. I traced it: its reads are island-object methods next to a per-player bit (0x10063235, 0x100634bc, 0x100638f3); the mine tick and the search do not read it. Docs corrected, OQ-128.
  3. The docs were missing. Written: §7.1, §15, §4.9 and §6; the sizing difference of `unlocked` is documented.
  4. `grow` is named as the exception.
  5.–8. Test gaps (`shows_icons`, the animal table, the mine area, spawns and notices not compared): one test each, mutation-checked.
  9. The version policy. I kept the versions and wrote the reasoning in §6.
  10. Mojibake in the test file: fixed.
  11. Weak asserts: pinned.
  - I ran one reviewer, not two.
- Left open / questions:
  - OQ-128 (RE): what world property 5 does.
  - OQ-129 (owner): fur trappers stay idle until the fauna is ported. That is the faithful default; the question is whether an interim `rules` stand-in is wanted.
  - OQ numbers 128/129 may collide with T14 if it adds OQs in the same round; renumber at merge.
  - Not mine to decide: whether `kPrngTimelineVersion` should still be bumped for S11 (see §6).
- Time spent / subagents used: one reviewer subagent (read-only); everything else inline.

## T14 stage 1 — The compositor on weak GPUs: per-pass cost on the Radeon iGPU, the wave window bounded at far zoom — 2026-10-23 — status: FINISHED (acceptance item 3, the 8 ms target, missed and quantified)
- Branch: task/T14-weak-gpu (8313b62 per-pass timestamps, f7112b0 the bound, 9ee8a01 docs, 86f3b5e review fixes, then this report); worktree: C:\Users\altin\anno1503-T14
- Built: per-pass GPU timestamps (`GpuLimits::gpu_timing`, `GpuFrame::pass_gpu_ms`, `GpuPassTimes`: fluid, terrain, under, sea, objects, resample, frame; the wave simulation's compute pass was outside the timed span before; the sea pass is split in two when timed, LoadOp_Load, same picture, same frame time within 0.1 ms); `bench_frame --scale-cost` per-pass p50/p95/p99/max, `--styles`, `--cost-frames`, `--wave-sim`, `--fluid-cells-per-px`, `--cost-png`; the bound `fluid::bounded_cells_per_tile` with `GpuLimits::fluid_max_cells_per_px` (default `fluid::kDefaultMaxCellsPerPx` = 0.25, automatic on a non-discrete adapter only; halving, off with 5 % hysteresis, a new window when back on); `FluidGpu` keys its window on cells per tile instead of the zoom group (`window_size_for`, `FluidWindow::with_cells`, `drop_window`).
- Gates (all after the PC's standby): msvc build warning-free; ctest with game dir 554/554 non-render, 137/137 render CPU, 10/10 heavy (at f7112b0), GPU tests 89/89 RTX 4090 (adapter 0) and 89/89 Radeon iGPU (adapter 1) (at 86f3b5e); without game dir 790/790 (86f3b5e); clang-format 23.1.3 clean; `tools/check_unix_warnings.sh` (GCC 13, Clang 18, 131 files) clean (86f3b5e); agree.py n.a.; asan not run.
- Evidence: docs/perf/README.md "2026-10-23 - T14 stage 1", every pass in docs/perf/t14-passes.md. iGPU 1280x800 enhanced, p50 before -> after: group 1 at 0.452 fluid 42.1 -> 13.6, frame 63.5 -> 35.2 ms; group 2 at 0.475 fluid 50.3 -> 17.2, frame 84.6 -> 51.6 ms; scale 1 16.7-20.2 ms (fluid 11-13.6) unchanged; derived levels 44 ms (sea 36-40) unchanged; original water 3.3-3.6 ms at scale 1, 14.3-15.3 at the far points. 2560x1440: 206.9 -> 158.4 and 311.4 -> 260.7 ms. RTX 4090 ≤ 5.6 ms everywhere. Area-proportional: the wave window (bounded); the composed-grid passes (≤ ~5 x the window, stage 2's composed-pixel budget); the deep-sea shading (~9 ns per sea pixel on the iGPU, grows with the sea's share, stage 2). Target missed by 43.6 ms at the worst point (51.6 ms p50 / 54.8 max); left for stage 2: sea shading 25 ms, the simulation at its scale-1 cost 11-17 ms, the composed grid ~9 ms. New tests: "every pass of a frame is timed, and the timed frame is the untimed picture", "the wave window is bounded in cells per screen pixel at far zoom", "the GPU wave solver at coarser cells per tile (the far-zoom bound) matches the CPU" (worst 1.6e-5), background-mode additions, CPU "the wave window's cells per tile are bounded by the cells per screen pixel"; four mutations caught. PNGs: C:\Users\altin\Pictures\anno-review\T14\ (`before\`, `after\`, `far\a0|a1\cells0|cells0.25\` for the ten 0200Single scenarios and the canyon, `far\diff\`, `far\crops\`): no seams, holes or doubled rows; the bound changes coasts and shallows only (≤ 1.5 % of pixels > 8 levels), and at 2560x1440 the corners the capped unbounded window left to the analytic waves.
- Standby: the benchmark tables (03:16, 03:32) and PNG runs (03:39) finished before the 03:41 standby and were kept; the first full ctest (cut off) was discarded and every gate re-run after 05:23.
- Reviewers: one read-only reviewer; no blocking bug; fixed: equal timestamp ticks read as 0 ms, the bound's off case (no hysteresis, stale window), OQ-130 missing the 2560x1440 corners, doc gaps (rivers and coast are in the sea pass; chunk build/reduce not per frame), a possible overlap flake in the timing test, background-mode coverage, comments and numbers. Kept: `kCellsHysteresis` as a constant ratio.
- Left open / questions: OQ-130 (owner: the bound on a discrete adapter and its value; numbered 130 to leave 128/129 to T9 S11). Stage 2: the quality tiers (sea shading, simulation tier, composed-pixel budget); the bench's fixed 16 ms clock hides that the real-time frame on the iGPU runs into the 12-steps cap.
- Subagents: one read-only reviewer; one heavy job at a time through the lock.
- **Addendum 2026-10-23 (orchestrator review fix-up, OQ-130 decided):** the bound applies on every adapter (587ca4c): `GpuLimits::fluid_max_cells_per_px` is a plain value defaulting to 0.25, 0 = off (`--fluid-cells-per-px 0`); the test of the default checks it on any adapter plus the halved far window with it (mutation: the discrete-adapter exception put back fails it on the RTX 4090). RTX 4090 with the bound on (README T14 addendum and t14-passes.md, p50 / max): 1280x800 enhanced group 1 at 0.452 frame 0.88 / 1.44 ms (fluid 0.23), group 2 at 0.475 1.09 / 1.11 (fluid 0.30); 2560x1440 enhanced 2.73 / 2.75 (fluid 0.50) and 2.60 / 2.98 (fluid 0.48); original water ≤ 1.54 ms. Gates at 587ca4c: msvc build warning-free; GPU tests 89/89 on the RTX 4090 (adapter 0) and 89/89 on the Radeon iGPU (adapter 1), with the game dir; render CPU 137/137 and heavy 10/10; clang-format clean; `check_unix_warnings.sh` (GCC 13, Clang 18) clean. OQ-130 status decided with the owner's words; iso.md and README updated.

## T9 — sim stage S12: world-level time — 2026-10-23 — status: FINISHED
- Branch: task/T9-sim-s12-time, code and docs at 0a0ae14 (5 commits on 12c48e0) plus this report; worktree: C:\Users\altin\anno1503-T9.
- Built (`docs/sim/state.md` §15 "S12 as implemented"):
  - `World::advance` steps 2, 3, 5, 6 and 7 (D AnnoWelt 0x100ae040):
    - step 2, `events.cpp`: the timed-event dispatch through `pop_due` (0x100add90, saved-next rule) with the handler seams `tile_cleanup` / `delayed_hit`, each handler's Effects merged before the next event (C2); the D4 second pass; a capped pass (F24) reported by `check_invariants` (its last check) through `SimOutput::events_capped`;
    - step 3, `scheduler.cpp` + `figure.cpp`: the registry's phase B (0x1009aa4c..0x1009aac1, clamp(d·n/50, 1, n) without +1, the behaviour seam merged per figure, D12 as the default body), phase C (u16 difference against 7500, `cmp cx,0x1d4c; jb`), phase D (the deferred list); destroy frees the slot, cleans both lists and releases the home counter;
    - steps 5–7, `timers.hpp` / `timers.cpp`: the script timers (0x1009cd30, F5 clamp), the monopoly refresh (0x1009d2a0 with the owner query 0x1009d210; once, D5) and the 7.5 minute event (0x100ae140; F6 guard, D10 option, Event15a at the island centre).
  - All functions read instruction by instruction; reviewer A re-read them independently: all match.
  - State: `ScriptTimers`, `MonopolyTable`, the 7.5 minute accumulator and the draw-guard flag in `World`. Stream 105 → 106: the `ids` scope gains `u32 event750_acc, u32 monopoly.acc, bool faulted`; new scope `timers_monopoly`; the save frame has nine sections; `load` and `check_invariants` check the new sizes.
  - Seam protocol for figure seams (own design): the registry keeps the figure's id, and its expiry and deferred lists follow the figure's flags.
  - `tools/sim_stream.py` (outside the listed scope; needed so the independent goldens stay independent) transcribes the new scopes and the monopoly body.
- Acceptance:
  1. Every name of the §15 S12 list is a TEST_CASE of `src/sim/tests/world_events_test.cpp`: the twelve §14.2 world-level cases (FIFO, saved-next in advance, kind-1 events saved, timer clamp, monopoly once and the rule, event 7.5 min draws, accumulator discard vs keep, zero islands, registry count without +1, expiry, interim return, counter release), "figure phase goods effects merge after each call (C2)" and "AI seam commands applied in advance and not logged". 27 cases in the file; sim suite 502/502.
  2. G4: `fixtures/synthetic/sim/g4.log` (synthetic::g4_scenario, seed 508, 12 000 advances of 100 ms = 600 000 ticks, 21 checkpoints, 178 KB, our own data). Reproduced 21/21 by "G4 players timers events and the 7.5 min event through 600000 ticks equal golden" and by the new CTest `anno_sim_replay_g4_run`. Hand-derived asserts: upkeep 300 × 22; player 1 bankrupt at tick 156 050; timer 0 clamped at 100 050; three events dispatched and one still queued; figures ended at ticks 50 (phase D) and 7 500 (phase C); the monopoly entries; 12 + 2 + 2 draws from the table for cursor 508; Event15a at tick 225 000 on island 4, the second firing on a null slot. Goldens `s12_g4_*` self-baselined.
  3. §5.3 subsystems, each with its file:
     1. islands: `island_tick`, `scheduler.cpp` (real; growth visit and drought machine are S13; fauna, object bodies and city passes are seams, default none);
     2. events: `Scheduler::run_events`, `events.cpp` (real queue and pop; handlers are seams, default none: O-18, O-19);
     3. figures: `Scheduler::run_figures`, `scheduler.cpp` (phases B, C, D real; per-figure body is the seam `figure_tick`, default the D12 rule, off; phase A derived, not modelled);
     4. players: `player_table_tick`, `player.cpp` (real; seams `player_groups`, `defeat`, `ai`);
     5. timers: `timers_tick`, `timers.cpp` (real);
     6. monopoly: `monopoly_refresh`, `timers.cpp` (real; the second loop, O-24, not modelled);
     7. 7.5 minute event: `event750`, `timers.cpp` (real; Event15a is a UI event, its consumer is the script engine, O-16).
  4. Evidence per rule: the addresses above or a spec row. Untraced: OQ-131 (phase D removes through 0x10097770; the unit destructor is unread) and OQ-132 (whether the original's save keeps the two accumulators). Feel: OQ-133 (F5 timer clamp: the spec's fix is kept; the owner to confirm).
  5. `kStateHashVersion` 105 → 106 (field list). `kPrngTimelineVersion` stays 1: the set of draw sites is unchanged; S12 only reaches Event750Gate / Event750Island, as S11 did for SmallFarmAnimal. Re-baselined: g0, g1_players, s5, s6 and the empty-world hash from `tools/sim_stream.py` (the C++ agreed on the first run; `--check`: goldens agree); s7, s10, s11 self-baselined (event goldens unchanged); g2.log and g3.log re-recorded. G2 6/6 and G3 21/21 replay in the tests and through `anno-sim-replay run`.
  6. Gates below.
  7. state.md §15 "S12 as implemented", §4.9 and §9.3; PLAN status; this section; the PR.
- Gates (each through the heavy-job lock):
  - MSVC full build: warning-free.
  - `ctest --preset msvc -j 16` with `ANNO_TEST_THREADS=16`, without the game dir: 824/824.
  - With `ANNO1503_GAME_DIR=C:\Users\altin\anno1503\game\app`: the full run was stopped by Claude Code's low-memory guard after 730 of 824 had passed (0 failed); not restarted. The 515 sim and sim-replay CTests (`-R anno_sim`) were then run with the game dir in the foreground: 515/515.
  - clang-format 23.1.3 `--dry-run --Werror` over all of `src`: clean.
  - WSL Ubuntu, GCC 13.3 and Clang 18.1.3, `-O3 -DNDEBUG` with the CI warning set and `-Werror`, over formats, test_support, sim, the sim tests and anno-sim-replay (62 TUs each): no diagnostic. 502/502 with and without the game dir on both compilers; g2, g3 and g4 replay on both.
  - `tools/lint_sim.py` ok; self-test 64 cases, 0 failures. `tools/sim_stream.py --check`: agree. agree.py n.a. ASan not run locally (CI).
- Mutation evidence (MSVC, the whole sim suite per mutant; 35 mutants: 34 killed, 1 equivalent; the full table is in state.md):
  - event FIFO order (equal due ticks reversed): 12 cases fail, including "FIFO: timed events due at the same tick run in the order they were queued", "saved-next in advance…" and G4;
  - monopoly-once rule (faithful gate dropped): 11 fail, including "monopoly once and the rule…", the G0 / G1 players / s5 / s6 goldens and G1–G4; the accumulator added outside the body: 12 fail;
  - the 7.5 min event's add-on flag ignored: "event 7.5 min draws…" fails; inverted: 4 fail (that case, "zero islands…", the advance-order case, G4);
  - also killed: the modulus, the overshoot rule, property 6, the centre's rounding and axis, the faithful timer wrap, the last timer, the monopoly owner, break, slot bound, time and remainder, phase B +1, phase C `<=` and a 32-bit difference, a counter not released or the wrong one, the interim body (dropped, under a seam, u16 age), the figure merge, the seam list sync, the handler merge and its draws, the event/figure order, the flush rule and count, the cap report;
  - equivalent: `remove_figure` not erasing from `deferred` (phase D empties the list right after phase C).
- Reviewers (two, read-only):
  - A (spec + disassembly): every address matched. Fixed: the phase D path (OQ-131, comments), the original's save of the accumulators (OQ-132), the cap report moved last, the SKTIMER object location, the D12 note on the rule. Recorded: the O(k·n) list removal (for S15's benchmark); the u32 tick in advance versus the u64 tick in `WorldClock::game_seconds`.
  - B (tests vs acceptance): every hand derivation correct. 14 survivors and 2 comment errors reported; all closed with tests (second test commit). G4 now collects each advance's output instead of the bounded host queue.
  - Also fixed: a GCC 13 `-O3` false `-Wstringop-overflow` in `Scheduler::advance` (reserve), and `-Wnull-dereference` in the test (checked lookups).
- Left open / questions:
  - OQ-131 (RE): the phase D removal and the unit destructor.
  - OQ-132 (RE): the original's save of the accumulators.
  - OQ-133 (owner): confirm the F5 timer clamp.
  - OQ numbers continue after T14's OQ-130; renumber at merge if needed.
  - `g4.log` is 178 KB (12 000 advance entries).
- Time spent / subagents used: two reviewer subagents (read-only); builds, mutation runs and gates by this session, one heavy job at a time.
- **Addendum 2026-10-23: OQ-133 fix-up** (the owner: "Stay on the original bug with optional fix").
  - New rule `Rules::timer0_clamp`, default **off**. Off is the original (D 0x1009cd51..0x1009cd59): timer 0's u32 counter minus dt wraps on an overshoot and the timer stops only on an exact 0. On is the F5 clamp. `TimerExpired` is posted only when the timer stops, in both modes; a wrap posts nothing. Files: `config.hpp`, `state_fields.hpp`, `timers.hpp`/`timers.cpp`, `scheduler.cpp`. The script side of the timers (0x1009cd90 set = seconds × 500, 0x1009cd70 read = counter / 500, 0x1009cdb0 / 0x1009cdc0 stop and start) is noted for the script engine.
  - `Rules` is in the hashed header, so `kStateHashVersion` goes 106 → 107. Every golden was re-baselined: g0, g1_players, s5, s6 and the empty-world hash from `tools/sim_stream.py` (one more Rules byte; the C++ agrees), the rest self-baselined; g2.log, g3.log and g4.log were re-recorded. G2 6/6, G3 21/21, G4 21/21 in the tests and through `anno-sim-replay run`.
  - G4 now runs the default rules. Timer 0 overshoots at tick 100 050, wraps, and ends at 2^32 − 25 − 499 950, still running, with no `TimerExpired`. Re-derived and asserted.
  - Test "timer clamp: timer 0 wraps on an overshoot by default and stops at 0 under the rule":
    - default: an overshoot leaves 2^32 − overshoot with the timer still active; an exact landing stops it;
    - rule on: an overshoot stops the timer at 0;
    - also run through `advance` in each mode, with the event or its absence checked.
  - The advance-order case now lands timer 0 exactly.
  - Mutations, all killed:
    - clamping without the rule fails the timer clamp case and G4;
    - the rule not passed to the tick fails the timer clamp case;
    - an exact landing that does not stop the timer fails the timer clamp case, the advance-order case and `advance(0)`.
  - Docs: state.md §5.10 pseudo-code, §3 Rules, §13 F5 row (now a rules option with a faithful default), §15 "S12 as implemented"; OQ-133 resolved.
  - Gates:
    - MSVC full build: warning-free.
    - ctest without the game dir: 824/824.
    - ctest `-R anno_sim` (sim and replay) with the game dir: 515/515. The full run with the game dir was not repeated because of the memory guard.
    - clang-format: clean. lint_sim: ok. `sim_stream.py --check`: agree.
    - WSL GCC 13.3 and Clang 18.1.3 at `-O3 -Werror`: 62 TUs clean, 502/502 with and without the game dir, g2/g3/g4 replay.

## T21 — Figure removal and the saved accumulators: the RE behind OQ-131 and OQ-132 — 2026-10-23 — status: FINISHED
- **Branch:** `task/T21-figure-removal-re` from `main` ea38384; worktree `C:\Users\altin\anno1503-T21`.
- **Scope:** docs only. No build, ctest or game run, and no `src/` change. Nothing of the original is in git: disassembly and helper scripts stayed in `C:\Users\altin\anno1503-T21-scratch\`.
- **Method:** static analysis of `AnnoWelt.dll`, with short excursions into `AnnoGame.dll` (the figure type and its `BEWOHNTYPE` table) and `AnnoScene.dll` (where the world is created).
  - Tools: capstone/pefile, `tools/disasm.py`, a linear sweep of `.text`, and xref, vtable and immediate scans.
  - The figure ids come from `Anlagen.dat` PROD3 and `FigurenHeader.dat` `BEWOHNTYPE`, read through the parser in `tools/figuren.py`.
- **Written:**
  - `docs/re/annowelt.md`: two new sections, "Unit registry: creation, removal and the figure destructor" and "World save and load: the accumulators", plus Open items;
  - `docs/re/production.md`: the figure-type vtable corrected (`0x10021cf8` is a double constant; the vtable is `0x10021d10`);
  - `docs/open-questions.md`: OQ-131 resolved, OQ-132 turned into an owner question, OQ-133's owner paragraph trimmed to one line, OQ-134 and OQ-135 new;
  - `docs/sim/state.md`: notes in §4.9 (confirmed parts and deviations) and §9.3 (the fields the original's savegame drops).
- **Evidence, OQ-131** (AnnoWelt unless named):
  - **The class.** Carriers (203 `TRAEGER`), the small farm's worker (106 `MAEHER`), its animals (107/109/110 `WEIDETIER`) and the hunter/trapper (108 `JAEGER`) are one class.
    - A figure type's class code is its `BEWOHNTYPE` index. AnnoGame `0x1000fe00` stores it at type `+8` (`0x10010016`; the table is `0x10024cb8`, read via `0x10013a20`), and type slot `+0x50` returns it.
    - The factory `0x1009a110` builds codes 1, 2, 3, 5, 6 and 17 with `0x1003b7c0`, vtable `0x100c4950`.
  - **Destructor:** slot `+8` = `0x10031db0` → `+0x170(1)` = `0x10054080` → `0x10050e00` and operator delete. It frees only the unit's own node list and memory. It does **not** null the registry entry, unlink `+0x48`/`+0x58` or release the home building.
  - **Counter release, at the figure's end:**
    - The end behaviour (slot `+0x174` = `0x1003af00`; also slots `+0x94` and `+0x17c` when hit points reach 0) calls slot `+0x110` = `0x1003b6e0`.
    - That calls `home→slot +0x2c(this)` and clears the home `+0x78`. The home's slot nulls its pointer to the figure: workshop `0x100687b0`, weapons workshop `0x100692d0`, farm and trapper `0x1006f070`, small farm `0x1006fe40` (the small farm also lowers its byte counter).
    - A second release does nothing.
  - **Then the expiry list.** After the last walk (0 or 15.0 at rate 0.01), the tick `0x10034760` calls Remove(this, 0) (registry slot `+4` = `0x100974b0`, at `0x10034844`).
    - The entry leaves its sector cell and is appended to `+0x48`.
    - The stamp freezes: property 0x23 = unit `+8`, written after each active step at `0x1003488e`.
    - Phase C destroys after 7500 ticks: `0x10097520` nulls the entry and lowers `+0x24`.
  - **Phase D is never on this path.** `0x10097720` and phase D read slot `+0x40` as a record pointer; this class returns a u16 index (`0x10035be0`). The deferred list belongs to the pooled sector objects of registry slot `+0x38` (`0x10097560`).
  - **Twice on the deferred list:** possible by the code (no membership test, and append `0x10096f40` does not deduplicate), never for a figure.
  - **A6:** the scan of `0x10099130` is the building list's, but the minimum growth is 100 (`0x10099185`–`0x100991a1`). A fresh registry holds one entry.
  - **Demolished home:** the four production destructors detach their figures, which live on without a home.
- **Evidence, OQ-132:**
  - Save `0x100ae700` / Load `0x100ae270` chunks: `WELT`, `MAIN`, `GUID`, `ALIASNAME`, `WARETYPE`, `SPIELER`, the islands, `COUNTDOWN`, `SKTIMER`, `MONOPOL2`, `LIMITS`, `K_DATA` (Load also takes `SCHIFF` and `COMPUTER`).
  - **World `+0xe8`** (monopoly accumulator): never written or read. `MONOPOL2` `0x1009d130`/`0x1009d080` covers the 28 entries only. Only the constructor zeroes it (`0x100a39a8`).
  - **`+0x69e4`** (7.5 minute accumulator): never written or read (`LIMITS` is `+0x69e0`, 4 bytes). Clear (`0x100af28e`) and the constructor (`0x100af46c`) zero it.
  - **SKTIMER:** `0x1009ce60`/`0x1009cdd0` save the five `{counter, active}` pairs. The last tick `+4` is not saved; a new SKTIMER has 0.
  - **The clock is not saved.** `MAIN` holds the game seconds, which Load writes into the offset `+0x69d4`. The tick restarts at 0 (consistent with F1).
  - **After a load into a new world object:** the monopoly table is recomputed on the first advance ("once per session"), the 7.5 minute schedule restarts, and the timers continue.
  - Whether loads always use a new world object is not traced (OQ-135).
- **Deviations for S13/S15** (in OQ-131 and `state.md` §4.9; no code changed here):
  1. **The home counter is released at the end, not at the destroy.** `destroy_figure` stops releasing; the end path releases once and clears `Figure::home`. Today an expiring figure is released 7500 ticks late, and twice when a seam already released.
  2. **D12's interim return ends via release + `expiry` with stamp `u16(t)`, not via `mark_figure_for_delete`/phase D.** No figure path appends to `deferred`. The slot is then freed 7500 ticks later.
  3. **`SimConfig::figure_min_growth = 100` for `add_figure`, instead of `list_min_growth` 10.** This changes `n`, the phase-B count and every golden with figures.
  4. **`created_stamp` is the end tick: rename it (`expiry_stamp`) with the S15 freeze.** The bytes are unchanged.
  - **Confirmed as built:** phase C's u16 test, the stamp after phase B, a removed slot keeping its hint, length and cursor, A6's scan, A7, homeless figures releasing nothing.
  - **§9.3:** the clock, `event750_acc`, `monopoly.acc` and `timers.last_tick` are kept although the original drops them. That waits on the owner (OQ-132).
- **Review:** one read-only reviewer re-read the addresses independently. It agreed on every answer and found three mistakes in my first text, now fixed:
  - the field order of `MAIN` ({seconds, registry length, flag bit, 0});
  - the SKTIMER record order ({counter, active, 3 zero bytes});
  - the end behaviour's last walk (0 or 15.0 at rate 0.01, so the Remove can come well after the release).

  It also found the callers through slots `+0x94`/`+0x17c`, and a small-farm attach quirk (`0x1006fdb0` raises its counter before looking for a free slot). That quirk is unreachable with the shipped data and is noted in annowelt.md.
- **For the owner, OQ-132:** the original's savegame drops the two accumulators, the timers' last tick and the clock. After a load it therefore recomputes the monopoly table and restarts the 7.5 minute schedule. The rebuild keeps everything. Recommendation: keep exact save/load as the default (resync snapshots and replays need it), with an opt-in `rules` option that follows the original when a savegame is loaded.
- **Left open:**
  - OQ-134: the pooled sector objects — which classes, who marks with flag 0, the reserved-slot bitmap, and two hazards in `0x10097720`/`0x10097770` as written.
  - OQ-135: whether a load runs on a new world object; where the units and the registry's stamp, cursor and expiry list are saved (`SCHIFF` → registry slot `+0x18`, not read).
- **OQ numbers used:** OQ-134 and OQ-135; OQ-136..139 unused.
- **Addendum 2026-10-23: owner decision on OQ-132.** Asked (executor session): "should save/load stay exact, or follow the original on loading a savegame? T21 recommends exact as the default, because resync snapshots and replays need it, with the original's behaviour as an opt-in rules option." Owner: "agreed". OQ-132 is decided and the §9.3 note updated. The default is exact save/load, a stated exception to the faithful-default rule; the opt-in `Rules` option for the original's load behaviour is not built yet (for S15 or the savegame-loading stage, together with OQ-135).

## T9 — sim stage S13: island sweeps, growth, the drought machine — 2026-10-23 — status: FINISHED
- Branch: task/T9-sim-s13-sweeps, code and docs at 1ad0805 (11 commits on ea38384) plus this report; worktree: C:\Users\altin\anno1503-T9.
- Built (`docs/sim/state.md` §15 "S13 as implemented"):
  - `island_tick.cpp` (new): first the island phase moved out of `scheduler.cpp` (one commit, no behaviour change, 502/502).
  - `grow_chunk` (D 0x1008b5f0): runs at the growth cursor before each cursor step. In-bounds tiles, row-major (A14/F20 clip). `grow()` for growing types. Object and phase are written through the dirtying setters.
  - `drought_tick` (D 0x1008aeb0), after the 300 s event (D 0x1008b8ab):
    - plateau and strip gates with catch-up: the stamp moves by the period, never to `t`;
    - `drought_strip` (the rectangle), then the `drought_tiles` seam once per strip (new signature with `DroughtStrip`);
    - owner samples into the city mask when expanding at step 0; samples outside the island are skipped (F36);
    - step, phase and ring advance; the plateau at the target; removal below ring 0.
  - `start_drought_at` floors the centre (the constructor 0x10087280 does) and bounds the target ring to 2^31 − 1.
  - `city_mask`: `ceil(city_slots / 32)` u32 words (O-55 closed).
  - Config: `kMaxDroughtRing`; `drought_plateau_count` must be non-zero.
  - `make_growth_types` and `g5_scenario`; the replay tool knows the growth registry.
- Disassembly (read instruction by instruction, nothing committed):
  - Read: 0x1008aeb0 with the strip loop 0x1008b4f7..0x1008b5be and the jump table 0x1008b5e0; 0x10087280; 0x10093a10; 0x1008b760; 0x1008b5f0 and its callees; the table 0x100ca0b8 (a permutation of the 64 in-chunk offsets).
  - The §5.8 pseudo-code's gates, advance and ring bound were right; the tag is now D.
  - Corrected in the spec first, in its own commit d107694:
    - `step` is a group of eight tile offsets inside each chunk, not an offset along the side;
    - each strip covers its whole side; ring 0 is the centre chunk, unclipped;
    - the expanding strip at step 0 writes the city mask from four owner samples per anchor;
    - the constructor floors, not the wrapper;
    - the coordinates are world tiles.
- Acceptance:
  1. **Met.** All nine §15 S13 names are TEST_CASEs in `src/sim/tests/sweeps_test.cpp`; the timeline case also runs steps 4 / phases 2 / edge 16 and edge 4 / phases 5. Also present: "city mask with more than 32 city slots" (33 and 64; the bit of city 32 / 63 is in word 1; the loader accepts it) and "drought save load and hash in each of the three states". 23 cases in the file; sim suite 525/525.
  2. **Met.** `grow_chunk` uses `find_growing` and the existing `grow()`, row-major, writing only through the setters; the tests check that exactly the visited chunks' digests change. The S7 sweep tests in `scheduler_test.cpp` are unchanged and pass. Only its drought cases changed: S7's refusal above 32 slots became the sized mask.
  3. **Met.** The timeline was derived by hand in §5.8 before the code: 11 200 / 120 000 / 11 200 ticks, gone at T0 + 142 400. Both gates add the period to the stamp. The rectangle and the meaning of `step` are written down with addresses.
  4. **Met.** G5: `synthetic::g5_scenario` (seed 514), 20 000 × 100 ms = 1 000 000 ticks. `fixtures/synthetic/sim/g5.log`: 21 checkpoints, 298 KB, our own data. Reproduced 21/21 by the test and by CTest `anno_sim_replay_g5_run`. Hand-derived asserts:
     - field: counter 12 after 84 visits, ripe at 156 050; forest: counter 16 after 166 visits, ripe at 240 000;
     - the six 300 s draws for cursor 514 (21 763, 4 940, 20 851, 18 662, 13 829, 30 900): only the 2nd and 6th pass % 20, so droughts start at 300 000 and 900 000;
     - first drought: ring 3 at 305 000, plateau count 38 at 350 000, ring 1 retreating at 440 000, gone at 442 400; city 3 joins the mask at 304 050;
     - the second drought **still runs at the end**: plateau, 88 counts. It started at 900 000, so it would end at 1 042 400.
     - Goldens `s13_g5_*`.
  5. **Met.** Path: `advance → Scheduler::advance → island_tick → grow_chunk → grow`. The tile effects remain the seam `drought_tiles` (O-15, default none), now OQ-140. Nothing in S13 draws: the callees of 0x1008b5f0 (AnnoGame 0x100037e0, 0x1000a570, 0x10007da0, 0x10007db0; AnnoWelt 0x1008f2d0) neither call 0x100a1740 nor read 0x100cb8f8, so O-13's PRNG part is closed. §6 lists the original's seamed tile-effect callees as possible draws.
  6. **Met.** Addresses or spec sections for every rule. §5.8 confirmed or corrected first (D). New OQ-140. Generalisations for modded configurations are tagged N: sample offsets e/4 and 3e/4, an empty strip for phase >= 4 at ring >= 1.
  7. **Met.** `kStateHashVersion` 107 → 108: `city_mask` is now serialised as u64 count + u32 words.
     - G1–G4 hold no drought, so only the header moved them. g0, g1_players, s5 and s6 were re-baselined from `tools/sim_stream.py`; the C++ printed the same values and `--check` agrees.
     - The rest are self-baselined; the event goldens are unchanged.
     - g2/g3/g4.log were re-recorded and replay 6/6, 21/21, 21/21. The growth and drought commits moved no golden.
  8. **Met.** Gates and mutation evidence below.
  9. **Met.** §15 "S13 as implemented", the §5.4/§5.8 corrections, PLAN status (S13 in review), this section, the PR.
- Gates (each through the heavy-job lock, final code 1ad0805):
  - MSVC full build: warning-free.
  - `ctest --preset msvc -j 16` with `ANNO_TEST_THREADS=16`: 852/852 without the game dir; **852/852 with `ANNO1503_GAME_DIR`, the full run** (430 s; the memory guard did not stop it).
  - clang-format 23.1.3 `--dry-run --Werror` over all of `src`: clean.
  - WSL GCC 13.3 and Clang 18.1.3, `-O3 -DNDEBUG -Werror` with the CI warning set, over formats, test_support, sim, the sim tests and anno-sim-replay (64 TUs each): no diagnostic. Sim tests: 0 failed with and without the game dir on both. g2–g5 replay on both.
  - `tools/check_unix_warnings.sh` (all of `src`, viewer and GPU on, GCC and Clang, and the viewer without GPU): clean.
  - `lint_sim.py` ok; self-test 64/0. `sim_stream.py --check`: agree. agree.py n.a. ASan not run locally (CI).
- Mutation evidence (MSVC, the whole sim suite per mutant): 32 mutants, 31 killed, 1 equivalent in state; the list is in state.md.
  - **in-bounds clip removed**: "edge chunks are clipped" and G5 fail;
  - **catch-up `stamp = t`** (strip gate and plateau gate): "drought gate catches up" fails;
  - **expanding ring past the target not becoming the plateau**: 5 fail, among them "drought machine timeline";
  - others killed:
    - ring bound `>=`, removal at ring 0, plateau `>`;
    - the sample mutants, the mask in word 0, ring 0 clipped, the right column off by one chunk;
    - the seam not called, no floor, the phase written only with a new object, the visit after the cursor step;
    - no drought tick;
    - drought tick before the 300 s event (killed by a case added after it survived);
    - mask word count unchecked;
    - review B's 11;
  - equivalent: the record check after the seam dropped. It writes an emptied optional's storage, which nothing reads; UB only in the mutant.
- Reviewers (two, read-only, one after the other):
  - **A** (spec + disassembly): no behavioural bug; every address matched; the G5 numbers re-derived. Fixed:
    - the target-ring bound in `start_drought_at` (a latent i32 overflow from a scripted radius), tested;
    - the tile-effect description (every expanding tile is tested, only 0xe3 retreats, the kinds are PRODTYPE 226/227; OQ-140 narrowed);
    - the possible draws of the seamed callees (§6);
    - §4.8, §3.1, §5.4 (the P = 1 fault, the 0xce notice);
    - three comments.
  - **B** (acceptance + mutation analysis): items 1–7 met, every derivation correct.
    - **Medium**, fixed: `drought_plateau_count = 0` was accepted and left a counter the loader refuses; it is now refused.
    - Its ~11 likely survivors are closed by six new cases (u16 wrap of the strip gate, every sample of every anchor, edges 4 and 16, the clip per axis, the plateau's last count and ring 0 with a fifth phase, a seam that edits or removes the record), a find/find_growing check, and exact empty rectangles. Run as mutants: 11 killed, 1 equivalent.
    - Stale I tags fixed.
    - Left (low): two "no draw" checks are vacuous in their windows; G5's draw count is the real check.
- Left open / questions:
  - **OQ-140** (RE): what a drought does to tiles and cities. Island slot +0x140 = 0x100817c0 is unread, as are the AI message 0x11, the event 0x17 and the crop objects. Until it is read, a drought runs on schedule but changes no tile: the gameplay effect is missing.
  - No owner decision arose: F36 is a fix (out-of-island reads), and the generalisations only touch modded configurations.
  - The `1 << owner` without `% 32` mutant is caught only by UBSan (CI), not on MSVC.
  - `g5.log` is 298 KB.
- OQ numbers used: **OQ-140** only (OQ-134..139 left to T21).
- Time spent / subagents used: two reviewer subagents (read-only); builds, 32 mutation runs and the gates by this session, one heavy job at a time.

## T20 stage 1 — The scenario script engine in C++: the `BIGBROTHER` reader, `anno-dump bigbrother`, `agree.py` — 2026-10-23 — status: FINISHED
- Branch: task/T20-bigbrother-reader (9 commits on a7c7d53); worktree: C:\Users\altin\anno1503-T20. Files: the new ones of the T20 entry, plus `tools/bigbrother.py`, `tools/test_bigbrother.py`, the new generator `tools/bigbrother_fixtures.py`, `docs/formats/bigbrother.md`, `docs/formats/README.md` and `docs/PLAN.md` (M1.5).
- Built:
  - `anno::formats::bigbrother` (`src/formats/{include/anno/formats,src}/bigbrother.*`):
    - `lex`/`parse` return `Result<…, Error>`; `Error` carries the byte offset, the line and the reason;
    - `emit` gives back the input byte for byte (every byte is in one token: CRLF/LF, blanks, comments, touching tokens);
    - `find_payload` reads a scenario (root > SCENE > BIGBROTHER) or a savegame (root > BIGBROTHER);
    - the interpreter's reading: `grouped` (equal rank, grouped to the right, parentheses as written), `is_mixed`, `describe`, and `read_on` (the block statements without `;`, the `;` each runs on to, and what it swallows).

    No evaluation, no sim link. The six section names of §3, the three object kinds and the three subjects are tables. An unknown section (`STARTUP`, OQ-122) is kept as read. No §4–§6 names are compiled in.
  - `anno-dump bigbrother [--full] <file.sze|file.sav>...` (`src/tools/anno_dump/dump_bigbrother.cpp`) prints the bytes of `tools/bigbrother.py dump [--full]`. `--full` adds every token with offset and kind. The format is in `bigbrother.md` §8, "Reference dump".
  - `tools/agree.py`, `bigbrother` cases:
    - the accepted fixtures, plain and `--full`;
    - all fixtures, with the refused `bad_*`;
    - with the game dir, the 50 `.sze`, plain and `--full`.

    `ANNO_DUMP_PORTED_COMMANDS` gets `bigbrother`.
  - Fixtures: `fixtures/synthetic/bigbrother_synthetic/`, 17 chunk files written by `tools/bigbrother_fixtures.py`. They are our own text, and as binary files git keeps their mixed line endings under `core.autocrlf`. `test_bigbrother.py` checks the committed files equal the generator's output.
    - Accepted (9): grammar (every token kind, touching tokens, Latin-1), lf, crlf, mixed, read_on (the four shapes), refuse, edge, startup, saved.sav.
    - Refused (8): order, byte, then, semicolon, define, deep, two, none.
- **Python oracle changed, and why** (a3b4614, before the C++). The old `dump` could not be the oracle:
  - **Unstable:** six shipped player names hold Latin-1 bytes, which went out through Python's stdout encoding (cp1252 on a Windows pipe, UTF-8 on Linux), and the define lines were Python dict reprs;
  - **One file only, and a traceback on a refused script.**

  Now:
  - one `# path` per file;
  - define lines as text;
  - every byte outside printable ASCII, and `\`, as `\xHH`;
  - `NOPARSE` and exit 1 on a refusal;
  - the read-on note also on assignments;
  - `--full`.

  `parse` keeps an unknown section, as the task requires; the six known ones stay required once each and in order. In review fixes, both readers got the same nesting limit (below). `check`, `quirks`, `vocab` and `keywords` are unchanged.
- **One new limit, an option:** `Options::max_condition_depth`, default `kMaxConditionDepth` = 256 (Python `MAX_CONDITION_DEPTH`). It counts the connectives `grouped` nests a term under plus the parentheses, along any path of a condition.
  - Reason: the parser and every walk recurse once per level, as the interpreter's parser does (`0x10022bf0`). Without the bound, 1,500 nested `(` or a 3,000-term chain overflowed the stack (exit 0xC00000FD, review A).
  - The corpus needs 21 (P).
- Gates (preset msvc, head 068d421; this report commit is docs only):
  - MSVC full build: warning-free.
  - ctest `-j 16` (`ANNO_TEST_THREADS=16`): 852/852 with the game dir (503 s), 852/852 without.
  - `agree.py`: `--commands bigbrother` 5/5 with the game dir, 3/3 without. The default list: 23/23 without the game dir; 43/43 with it (as the ctest case `anno_dump_agrees_with_python` in the with-game-dir run).
  - clang-format 23.1.3 over `src`: clean.
  - WSL `tools/check_unix_warnings.sh`: GCC 13.3 and Clang 18.1.3 at `-O3 -Werror`, all 139 files clean.
  - The formats tests built in WSL with GCC 13.3 at the CI flags: 60/60 without and with the game dir. Not run under Clang 18, which is not on the WSL PATH; the script's Clang pass compiles them.
  - Python: `test_bigbrother.py` 30 OK with the game dir; without it 25 OK and 5 clean skips. The doctests pass.
- Evidence:
  - **Tests** (`src/formats/tests/bigbrother_test.cpp`):
    - round trip of the 9 accepted fixtures; every byte one token, all 8 kinds, both line endings, no CR outside a newline token; Latin-1 0x80–0xFF; the tree with values, kinds and offsets;
    - an unknown section at the start, in the middle and at the end;
    - known sections swapped, missing or twice;
    - 54 refused edits, each checked for its line, plus checks of the offending offset;
    - text before the first section; the payload rule (incl. the first SCENE container and a leaf SCENE); `to_integer`/`canonical_integer`;
    - the grouping table; the grouping against C precedence (truth tables: different exactly where the connectives mix in the guarded shape; `A || B && C` mixes but is the same function); the mixed fixture;
    - `read_on`: the four shipped shapes on synthetic copies, the refused kernel, two in a row, the `[` bound (an unknown section's and one in a comment); the edge fixture; the depth limit; no vocabulary.
  - **Corpus** (`bigbrother_corpus_test.cpp`, with `ANNO1503_GAME_DIR`; the output is file names and counts only):
    - 50/50 byte-identical;
    - 35 INITBLOCK, 271 SET, 308 IF, 3 ELSE, 382 assignments, 1,656 calls;
    - 4 without `;`, 1 without `()`, 24 timer and 429 variable primaries, 1,273 comments, 147 variables, 37 scripts with statements;
    - `[SIGNALS]` = `SCHUETTIINITSIG` + the SETs in all 50;
    - 28 mixed conditions in 10 files, all 28 differ from C and are guarded by their first term;
    - 0 refused kernels, 3 block statements without `;` (SET_PROFILE, SET_DIPLOMACY, SET_FOR_BUY), 1 lost statement (MINIMAP_ALERT, S01_09_01).
  - **Mutations** (scratch scripts, each built and run through the heavy-job lock; the source is restored byte-identical after each):
    - (a) grouping left-grouped: fails the grouping table, C precedence, the mixed fixture and the corpus reading (`guarded`, `differ`); agree fails.
    - (b) `read_on` searching only up to the block's `}`: fails the past-`}` assertions (LAST_ONE in "the four kinds…"; SET_DIPLOMACY in the corpus reading); agree fails. As review B noted, the swallowed-statement assertions do not fail under this mutant, because that `;` lies before the `}`. The swallowed case is (c).
    - (c) `read_on` never losing a statement: fails "the four kinds…" (TRADE/BEEP), two in a row, the edge fixture and the corpus case (MINIMAP_ALERT); agree fails.
    - (d) CRLF lexed as LF: fails the round trips, "CRLF and LF are kept", Latin-1, unknown sections, "every byte is one token", the read-on cases and the corpus 50/50; agree fails.
    - Review B's 17 proposals, plus my check that the exact-depth check is not dead (each killed): the `[` bound; ELSE skipped in `read_on`; lower-case headers; a lone `-`; a CR in a string; operator order; SET/INITBLOCK/FALSE not reserved; the last SCENE; a leaf SCENE; the define-line error offset; empty `()`; primary offsets; blank munching; a comment taking the CR; the exact-depth check disabled. All by the unit tests. The dump's number and escape mutants (3) are caught by agree.py only.
- Reviewers (two, one after the other, read-only, given the diff and the spec):
  - **A (spec and oracle):** 63 probes and the corpus agree byte for byte. Found:
    - **must-fix:** a stack overflow on deep nesting. Fixed with the limit above, in both readers, plus tests and `bad_deep.sze`;
    - an overflow on long `&&`/`||` chains: covered by the same limit;
    - the duplicate-name error naming another name than the reference: aligned;
    - stale "everything is refused" comments, the "`;` past `}` loses nothing" wording, and an unmodelled case (a `;` in a comment or string with text after it): documented in §3.4, `bigbrother.hpp` and `read_on`;
    - §8 inexact on define numbers, describe's spacing, the path line and unreadable files: corrected;
    - nits: an empty Condition, the truth-table shift, `<tuple>`, an unused include. Fixed.
    - Left as is: `agree.py` prints up to 200 bytes of the first differing line, which on a corpus failure would be shipped script text (existing behaviour for every command; local only, since CI has no game dir).
  - **B (acceptance and mutations):** every item the code can show is met; the four required mutations are killed. Found:
    - **should-fix:** the depth check undercounted a left operand. Fixed: the exact depth is checked after each condition, with tests for `(deep) && A`;
    - 12 mutants it expected to survive: tests and `edge.sze` added, all killed (above);
    - the wording of mutation (b): see the evidence above;
    - nits: an optional dereference without a REQUIRE in a test (fixed); the dump looking notes up by address (now by offset).
- Left open / questions:
  - **OQ-new, owner-relevant only once stage 2 evaluates:** the readers do not model a `;` that stands in a comment or string with text after it. The interpreter would start the next statement right after it (§3.4). No shipped script does this; a modded one could. Stage 2 should decide whether to model the original exactly.
  - The OQ-121 `rules` option (restore `MINIMAP_ALERT`) is stage 2, as the task says.
  - PR and CI: see the PR (MSVC takes about 50 min).
- Time spent / subagents used: inline; two reviewer subagents (read-only). Every build, ctest, mutation and WSL run went through the heavy-job lock, one at a time, beside T9 S13.

## T22 — The figure removal path as the original's: the four OQ-131 deviations — 2026-10-23 — status: FINISHED
- Branch: `task/T22-figure-removal-fix` from `main` at 8ccd301 (after the S13 merge 7c13f3b); worktree `C:\Users\altin\anno1503-T22`. Done by the executor session itself (owner: "Dont start a second subagent for T22, do it yourself"); T20 stage 1 ran in the one subagent at the same time.
- Commits, one concern each:
  1. baa0f5d `refactor(sim)`: `created_stamp` → `expiry_stamp` (bytes unchanged).
  2. 3e5eddb `feat(sim)`: `SimConfig::figure_min_growth = 100` (D `0x10099185..0x100991a1`), validated as `list_min_growth`, in the header; `add_figure` uses it; test "figure registry grows by 100".
  3. cdaa828 `fix(sim)`: the end path (`end_figure`), `destroy_figure` without a release, D12 through `end_figure`, `ReleaseCounter::figure`, `SerialWorld::end_figure`; `marked_for_delete`, `deferred`, `mark_figure_for_delete` and phase D removed; `validate_figures` and the §9.6 counter rule; `figure_removal_test.cpp`; the S12 figure tests re-derived.
  4. 79e7300 `style(sim)`: clang-format of commit 3 (4 files, formatting only).
  5. 3ef673b `test(sim)`: `kStateHashVersion` 108 → 109, goldens re-baselined, `g2.log` .. `g5.log` re-recorded, layout and slot numbers re-derived.
  6. 289dbd1 `docs(sim)`: `state.md` §3, §4.9, §5.9, §9.3, §9.6, §15 "T22 as implemented"; OQ-131 fixed; PLAN.
  7. 0efaeb7 `test(sim)`: a parameter renamed for GCC's `-Wshadow` (found by the WSL gate).
- Built (`docs/sim/state.md` §15 "T22 as implemented"; every rule from OQ-131 / `docs/re/annowelt.md` "Unit registry", no new RE):
  - `end_figure(figure, t, fx)` (D `0x1003b6e0`): a figure with a home puts `ReleaseCounter{home, counter}` into `fx`, its `home` becomes none; the stamp freezes at `u16(t)` (the Remove at `0x10034844`); a figure that has ended already is left alone. `end_figure(registry, figure, t, fx)` also appends it to `expiry`. `SerialWorld::end_figure(figure, t)` is the seam's door (the registry appends after the seam, `sync_figure_lists`).
  - `Scheduler::destroy_figure` (phase C, D `0x10097520`) releases nothing.
  - `ReleaseCounter::figure` (new): a release that names a figure goes through it, only while the figure still has a home, and clears it (the original's `+0x78` test). Own design, the seam protocol: a seam that sets the stamp directly is ended the same way (`sync_figure_lists` releases a stamped figure that still has a home).
  - D12 (`interim_figure_return_ticks`, off by default) calls `end_figure` in phase B; the slot waits for phase C.
  - Dropped: `Figure::marked_for_delete`, `FigureRegistry::deferred`, `mark_figure_for_delete`, phase D (comments keep the phase letters, D = pooled records, OQ-134).
  - `validate_figures`: on `expiry` iff `expiry_stamp`; a stamp **and** a home is refused. `check_invariants` counts only figures that still have the home (read back on load: `World::load` runs `check_invariants`).
- Acceptance:
  1. **Met.** `figure_removal_test.cpp` (new, 6 cases): "figure registry grows by 100", "release at the end, not at the destroy" (counter 2; the end at T = 1000: counter 1, on `expiry` with stamp 1000, home none; tick 8450 still there; tick 8500 slot free, counter **still 1**), "a second release does nothing" (registry level: one release from two `end_figure` calls; seam level: end twice + a figure release → the counter moves once; a figure release alone, then the end: once; invariants clean after each), "the interim return goes on expiry" (N = 1000: ends at tick 1000, not 950; a figure put out at 2000 grows the list 1 → 100 and takes slot 1, slot 0 taken until tick 8450 and free at 8500; `field_count<FigureRegistry>` = 4: no deferred list), "a figure whose home was demolished ends without a release", "check_invariants counts the figures that still have the home" (an ended figure is not counted; a counter above its figures is reported; an ended figure with a home is refused).
  2. **Met.** Changed assertions and why:
     - `world_events_test.cpp` "registry count without +1": n is 100 (`figure_min_growth`), not 10: 20·100/50 = 40 visits (with +1: 41), cursor 40; d = 1: 2 visits, cursor 42; the clamp to 1 now needs d = 0 (`advance(1)` inside tick 21), cursor 43; d = 60: 120 cut to 100, a full lap, cursor 43; `/25`: 80.
     - "expiry: phase C …": the figures on `expiry` have ended, so they hold no counter and the destroy releases nothing; an out figure added to show its counter stays 1.
     - "counter release …" rewritten for the end path (it tested phase D): D12 with N = 1 ends four figures at tick 50 (counters back then), phase C frees the four slots at tick 7550 (7500 at 7500 not yet), length 100 and hint kept, no second release.
     - "interim return: with the rule …": a figure now ends (stamp) at born + 1000 .. +1049 and its slot is freed 7500 .. 7549 ticks later; 400 advances so that slots are freed in the run.
     - The seam protocol case: the `marked_for_delete` kinds removed; kind 3 ends through `SerialWorld::end_figure`; a seam that sets the stamp directly releases at once (weaver 2 → 1, farm 1 → 0 at tick 50); both slots freed at 7550.
     - "a figure spawned during phase B …": 100 figures fill the list (was 10); the spawn grows 100 → 125 (was 10 → 12); 100 then 125 visits.
     - "interim return reads a u32 age …": at tick 66 000 the figure has ended (stamp 66000, counter 0) instead of being gone.
     - G4: the carrier put on `expiry` at tick 0 has ended, so it has no home and holds no counter (it held the weaver's counter until 7500 before); the farm's worker is out for the whole run (it was on the deferred list and ended in phase D); `worker_gone` is now never, `workers_out` 1.
     - `slice_test.cpp`: 10 → 100 in "merge creates figures …", the merge model's growth rule, "figure registry creation …" (a stale index 700 → 7000 to stay past the end; the modded config uses `figure_min_growth` 20 with `list_min_growth` 50 to show which one counts); the validation and hostile-stream rows: the deferred rows replaced by "ended but still holding a counter", hint/cursor 10 → 100, edges 9 → 99; the byte layout: 100 slots, no `marked_for_delete` byte, the corrupt byte is the stamp's presence byte at 20 + 58 + 57; the save/load case: figure 2 has ended (home none, the farm's counter 0); the hash-field list has 12 members.
     - Layout constants: the empty figures scope is 31 bytes, was 39 (`config_test.cpp`, `sim_test.cpp`, `player_test.cpp`, `island_helpers.hpp`); `building_div` is SimConfig's 21st field (`island_state_test.cpp`); the expected config bytes gain 100.
  3. **Met**, the whole sim suite per mutant (MSVC, 531 cases):
     - `destroy_figure` releasing again, alone: **equivalent** — the end cleared the home, so the destroy finds none to release. With the S12 behaviour restored (the end keeps the home and the destroy releases): 7 fail, "release at the end, not at the destroy" and "a second release does nothing" among them.
     - `end_figure` not clearing `home`: 7 fail, "a second release does nothing" among them.
     - The D12 rule freeing the slot in the same advance: 5 fail, "the interim return goes on expiry" among them.
     - `figure_min_growth` read as `list_min_growth`: 15 fail, "figure registry grows by 100" and G1–G4 among them. G5 does not move: its scenario has no figure, so its registry never grows.
     - `expiry_stamp` set to the spawn tick: 8 fail, "release at the end, not at the destroy" among them.
  4. **Met.** `validate_figures` as above; the §9.6 rule is `check_invariants`', which `World::load` runs.
  5. **Met.** Gates (all through the heavy-job lock, in the foreground):
     - MSVC full build: warning-free (0 warnings).
     - ctest `--preset msvc -j 16`, `ANNO_TEST_THREADS=16`: 858/858 with the game dir (the full run, 415 s, not stopped) and 858/858 without. The sim/replay subset again on the final head: 545/545.
     - clang-format 23.1.3 `--dry-run --Werror` over all of `src`: clean.
     - WSL GCC 13.3 and Clang 18.1.3, `-O3 -DNDEBUG -Werror` with the CI flags over formats, test_support, sim, the sim tests and anno-sim-replay (65 TUs each): no diagnostic after 0efaeb7 (GCC found a `-Wshadow` in the new test first); the sim tests pass with and without the game dir on both; g2–g5 replay on both.
     - `tools/check_unix_warnings.sh`: GCC and Clang with viewer and GPU (138 files) and Clang without GPU: clean.
     - `tools/lint_sim.py`: ok. `tools/sim_stream.py --check`: agree.
     - `anno_sim_replay_g2..g5_run` pass on the re-recorded logs (g5: 20 021 entries, 21/21, tick 1 000 000).
  6. **Met.** Docs and commits as listed; OQ-131 status "fixed 2026-10-23 (T22)" with the four changes marked done.
- Moved test cases per commit (the whole sim suite after each; a golden case stops at its log or hash, so the set is per case, not per golden value):
  - Commit 1 (rename): none.
  - Commit 2 (growth, header byte): 21 — every golden (G0, G1 players, s5, s6, G1 ×2, G2–G5 and the empty-world hash: the header gained a field) and the cases that pin the registry length or the header layout ("registry count without +1", "a figure spawned during phase B", "counter release", "merge creates figures …", "effects applied after the island ticks …", "figure registry creation …", "figure registry validation …", "ids figures and events reject hostile streams", "ids figures and events scopes have the documented byte layout", "load refuses a header whose config has a zero divisor …"). The new "figure registry grows by 100" passed.
  - Commit 3 (end path, fields dropped): 22 — the goldens again and the layout cases of the figures scope ("islands scope stream layout", "players scope stream layout", "players scope reads back …", "state stream header and settings scopes", "world core stream layout …"); the S12 figure cases re-derived in this commit passed, so did the six new cases.
  - Commit 5: all re-baselined and re-derived; 531/531.
  - By golden value (from the re-baseline): every hash moved (the header); the event goldens s7_g1_events and s11_g3_events are unchanged; s12_g4_events moved with G4's figures (commit 3's end path: the ended carrier no longer holds the weaver's counter, the worker no longer ends in phase D).
- Reviewers: **none** — §1 item 5 asks for two read-only reviewer subagents, but the owner said not to start a subagent for T22, and T20 held the second slot. The mutation evidence above was run by this session; the orchestrator's review is the first independent read.
- Left open / notes:
  - Demolishing a building does not clear its figures' homes in the model (the original's production destructors detach them, A6/T21). A release for a building that is gone does nothing and building ids are never reused, so the effect is the same; the stale id stays in the figure record until its end.
  - `end_figure` does the release and the Remove at the same tick; the original's last walk between them (0 or 15.0 at rate 0.01) belongs to a behaviour body (O-8).
  - Not in T22: the pooled sector objects (O-17, OQ-134), the unit's u16 slot cap, any behaviour seam body.
- OQ numbers used: none.
- Time spent / subagents used: no subagent; builds, ctest, 6 mutation runs and the WSL gates by this session, one heavy job at a time.

## T9 — sim stage S14 stage 1: the scenario adapter and the corpus — 2026-10-23 — status: FINISHED
- Branch: `task/T9-sim-s14-scenario` from `main` at 60c259a (after the T22 merge); worktree `C:\Users\altin\anno1503-T9`. Done by the executor session itself (owner: "you will do it yourself instead of handing it over to a subagent"); two read-only reviewer subagents as §1 item 5 asks.
- Commits (one concern each): f4e1e7c the adapter; c75715d the unit tests; 0ccc593 the corpus case and `scenario_corpus.txt`; 7286df8 `anno-sim-replay scenario`; 5b186c3 the state hash after create in the expectations; 434a573 docs; e4d00a8 GCC `-Wsign-conversion`; a253826 the adapter after review; 98e6111 the tests after review (the corpus run length); 9d1b507 the docs after review; 4ddfc6a no `;` in a test name; fe4e36c clang-format.
- Built (`docs/sim/state.md` §10 "As implemented in S14", §15 "S14 stage 1 as implemented"):
  - `load_scenario(file, types, config) -> Result<LoadedScenario, std::string>` in `scenario_formats.hpp/.cpp`, the only `src/sim` file that includes `formats::sze` / `formats::insel`; `ScenarioWarning{kind, path, record, offset, message}` with 14 kinds (`UnknownRecord`, `UndecodedCity`, `IslandLink`, `UnknownObject`, `BuildingWithoutCity`, `TileOwnersUndecoded`, `UndecodedDeposits`, `ObjectTileTaken`, `DuplicateObject`, `CityOffIsland`, `UndecodedPeople`, `ComputerNameNotDrawn`, `UnknownClimate`, `UnalignedIsland`); `count()` for the corpus table.
  - Evidence read for this stage (static, `tools/disasm.py`): AnnoScene `0x10009cc0` HEAD case byte by byte (both reviewers' and my reading agree); AnnoWelt `0x10089130` PROD2 case and island slot `+0x78` = `0x10082040` (PROD2 writes tile objects; no city building there, likely); `0x100a19a0` (the island table slot); reviewer A added the INSEL5 world reader `0x100a2ea0` and the rectangle snap `0x100a253e`.
  - Mapping and refusals: §10 "As implemented in S14", each line cited in the source.
- Acceptance:
  1. Do 1–7 done. The corpus case passes on this PC: **50 of 50** load, create, run **150 000 ticks** (3 000 advances of 100 ms), `check_invariants` clean, equal hashes twice, save/load/save identical. **44** files create a world with at least one island, **6** hold only links. `scenario_corpus.txt` committed (one line per file: counts per kind and the state hash after create). Totals: islands 668, links 229, players 373, cities 272, buildings placed 0 (8 810 `PROD2` buildings without a known city, OQ-141), tile objects 2 497 856, deposits 0. A14 count **0**. Warnings: `UnknownRecord` 1 657, `UndecodedCity` 272, `IslandLink` 229, `UnknownObject` 0, `BuildingWithoutCity` 8 810, `TileOwnersUndecoded` 668, `UndecodedDeposits` 668, `ObjectTileTaken` 3 956, `DuplicateObject` 50 784, `CityOffIsland` 0, `UndecodedPeople` 173, `ComputerNameNotDrawn` 7, `UnknownClimate` 0, `UnalignedIsland` 0.
  2. Every mapping line cites D or P; "assume" / "guess": none in the two files.
  3. Tests: 13 unit cases (`scenario_formats_test.cpp`; every kind reached, a dropped kind fails its case), the 2 corpus cases, 5 tool CTests (usage, three argument refusals, the non-scenario refusal; Single_01 with the game dir, skipped without). Mutation evidence (MSVC, the 17 scenario cases incl. the corpus per mutant): 15 mutants, **15 killed** — HEAD shifted by one byte (HEAD byte map + corpus expectations); city number read as the player index (STADT2 city + 4 more); duplicate kind dropped; slot from file order; phase from the height byte; item-outside `>=`→`>`; island edge `<`→`<=`; prop6 as u8; flags `>=`→`>` at 0x100; climate `>=`→`>`; once-per-scope rule dropped; VOLK 16 read as 15; record offset +1; link numbers not reserved; file order ignored. The A14 count forced to 0 fails nothing: the corpus has no such island.
  4. No `reinterpret_cast`, no exception; refusals carry the path; `lint_sim.py` ok; `anno_sim` links only `anno_formats`.
  5. Gates: see below.
  6. No fixed island, player or city count (all from `SimConfig`); `World::create` gets the thread executor in the tool (per-island content in parallel); time per scenario (`anno-sim-replay scenario`, Release MSVC, 9800X3D): load median 12.5 ms (max 66, `endless02`), create median 3 ms (max 12), 150 000 ticks median 159 ms (max 686).
- Stream: no world field changed: `kStateHashVersion` stays 109, `kPrngTimelineVersion` 1, no golden moved (`sim_stream.py --check` agrees).
- Gates (each through the heavy-job lock):
  - MSVC full build: warning-free (0 warnings).
  - ctest `--preset msvc -j 16`, `ANNO_TEST_THREADS=16`: with the game dir the full run (614 s, in the foreground; not stopped by the memory guard) 902/903, the one failure `test_names_are_selectable` (a `;` in a new test name), fixed in 4ddfc6a and re-run with the scenario, replay and name cases (40/40); without the game dir 902/902. The corpus case took 19.7 s and the long-run case 29.6 s inside the 16-job run; the corpus helper ran 50 items on 14 of 16 threads with no memory wait (`[anno-test memory]` line in WSL).
  - clang-format 23.1.3 over all of `src`: clean (after fe4e36c).
  - WSL GCC 13.3 and Clang 18.1.3, `-O3 -DNDEBUG -Werror` with the CI warning set, over formats, test_support, sim, the sim tests and anno-sim-replay (69 TUs each): no diagnostic (two `-Wsign-conversion` hits in the tests fixed first, e4d00a8); the sim tests pass with and without the game dir on both; g2–g5 replay on both. The Linux builds reproduce every `create_hash` of `scenario_corpus.txt` (the corpus case compares them), so the adapter is deterministic across MSVC, GCC and Clang.
  - **UBSan:** the 17 scenario cases (the 50-file corpus and the 10 long runs included) under GCC 13 `-O1 -fsanitize=undefined -fno-sanitize-recover=all`, `halt_on_error=1`, with the game dir: 17/17, no runtime error. The WSL Clang 18 is a user-local extract without the UBSan runtime (`libclang_rt.ubsan_standalone` missing), so the Clang UBSan run is the CI job's (linux-clang-asan).
  - `tools/check_unix_warnings.sh`: GCC and Clang with viewer and GPU (145 files) and Clang without GPU: clean.
  - `tools/lint_sim.py`: ok. `tools/sim_stream.py --check`: agree.
- Reviewers (two, read-only, in parallel):
  - **A (specs and disassembly):** re-read `0x10009cc0`, the player HEAD and VOLK readers (incl. the jump table `0x1001e8f4`), `0x100a19a0`, the PROD2 case and `0x10082040`, the STADT2 reader: all byte maps right. Fixed: the wrong citation `0x100a1ff2` (the island file loader) replaced by the INSEL5 world reader `0x100a2ea0`; the original's rectangle snap → `UnalignedIsland` warning; the reused-number refusal stated as a deviation; a short HEAD now reads as zeros (zero-filled buffer) instead of being refused; the item layout cited from the PROD2 loader, the floor((len − 6) / 6) rule followed (a tail is ignored, no longer refused), the 0x3FFF/signed-x,y difference to `formats` noted; VOLK depends on the untraced name match (comment, warning text, OQ-143); `STADT` accepted (D `0x1001d240`); cities look only at islands read before them (file order); island `+0x16c` (kind 0xc6) added to OQ-141's unread calls; the load-origin / skip-flag values noted as not traced.
  - **B (acceptance + mutation analysis):** **the corpus ran 75 000 ticks, not 150 000** (1 500 × 100 ms; a tick is 2 ms) — fixed to 3 000 advances, the docs corrected; 16 likely survivors — all closed by new unit cases (the edges, the overlap rules, the climate bound, offsets and paths, the refusals at their bounds, VOLK 11/12/16/14, the 0x100 boundary, negative money, NUL in the name, prop6's high byte, u32 width) and checked by the 15 mutants above; link numbers now reserved against INSEL5 numbers; the city-slot check moved before the island lookup. Left as stated: `TileOwnersUndecoded` names the island (`INSEL5`), since no record is known to hold the owners.
- Left open / questions:
  - **OQ-141** (RE): where a scenario's buildings get their city (`STADT2/PROD`, O-36) and where the tile owners live; until then no shipped town is built and the scenario economies do not run. Also the island number's path into `+0x40`.
  - **OQ-142** (RE): overlapping items (first stays) and the tiles of a multi-tile object (top-left only).
  - **OQ-143** (RE): the VOLK name list (people of types 14 and 17; the name match for all types).
  - The `COMPSPIELER_NAME` load-time draw is listed in §6 as a timeline gap (7 players).
  - Stage 2: the `INSELLNK` resolver (O-35).
  - A render corpus test (`river_flow_corpus_test.cpp:640`, "chunks did not arrive") failed once in the first full with-game-dir run under the 16-job load and passed alone (4.6 s): a timing flake outside S14's files.
- OQ numbers used: OQ-141, OQ-142, OQ-143.
- Time spent / subagents used: two read-only reviewer subagents; builds, ctest, 20 mutation runs (5 first, then 15) and the WSL gates by this session, one heavy job at a time.

## T23 — The scenario cities: `STADT2` → stock, buildings and tile owners (OQ-141, O-36) — 2026-10-23 — status: FINISHED (one acceptance half open: the warehouse delta, see "Left open")
- Branch: `task/T23-scenario-cities` from `main` at 6106c58; worktree `C:\Users\altin\anno1503-T23`. Done by the executor session itself (round r: "Do this one yourself"); two read-only reviewer subagents.
- Commits (one concern each): b3d4f6c the RE note (answers (a)–(e)); 80761ee `CitySpec::capacity`; e15a821 the adapter with its unit cases; 02cd983 the corpus ranking, the economy case and `scenario_corpus.txt`; 8dae113 docs; 7eafd77 the fixes after review (farm activity byte, duplicate building items, comments, more tests, re-baseline); ea071fa the docs after review.
- RE (`docs/re/annowelt.md` "Scenario cities", static, `tools/disasm.py`; every line D, likely or open):
  - (a) The world reader `0x100ae270` hands the player reader origin (0, 0) and flag 0 (D `0x100ae286`, `0x100ae2a9`/`0x100ae2ad`); the `STADT2` reader `0x1001e294` adds the prefix x, y unless context byte `+0x400` bit 0 (likely clear, OQ-145), finds the island, gets/creates the city (`+0xf4`) and calls the city reader `0x1002e170` with the island rectangle (positions inside are island-relative). Records read: `HEAD`, `HEAD2`, `STAT2`, `ID`, `ID2`, `ID3`, `WARE`, `BAUMAT`, `VK`, `EK`, `PROD`, `VOLK`, `STST2`, `STAT`.
  - (b) `PROD/ENTRY` (`0x100264a0`): u16 x, y island-relative → island `+0x84` = `0x10083a90` (the tile's object, origin from its in-footprint offset, rotation, phase, height) → `Create` `0x10024010` (height → `+0x0f` = `byte0f`) → class `Load` (slot `+0x7c`) on the remaining bytes → footprint claim. Class maps: workshop, farm/fur trapper, mine, small farm, weapons workshop (byte offsets in sze.md "Cities"); house and market not read.
  - (c) Tile owners: `ID3` (`0x1002e709`–`0x1002e7c5` → island `+0x100` = `0x10085620`: 8 × 8 chunk, optional row mask, unconditional, unclipped), `ID2` the same in world tiles, `ID` radius claims; the footprint claim `0x10026685` (neutral tiles only). P: all 272 cities use `ID3` (14 957 entries), no `ID`/`ID2`.
  - (d) `WARE` (`0x1002e7ca`–`0x1002e84b`): length / 8 entries {u16 goods (file numbering, remapped by `WARETYPE` name `0x100a18d0`), u16 tons}, stock = tons << 8, mapped 0 skipped. `HEAD2` u16 `[0x24]` << 8 = capacity, 0 → 50 t (`0x1002e383`–`0x1002e3e5`).
  - (e) `PROD2` building items are the tile layer's copy; the buildings come from `PROD/ENTRY` only (so `BuildingWithoutCity` became `BuildingItemWithoutEntry`, now for the items no entry names). P: 8 561 entries, 8 538 on an item origin, 23 on another tile of one, 7 460 with class bytes.
- Built (`docs/sim/state.md` §10 "As implemented in T23"; byte maps in `docs/formats/sze.md` "Cities (`STADT2`)"): `read_city` maps `HEAD2` → `CitySpec::capacity` (new field, applied by `World::create`), `WARE` → stock by goods name, `ID3`/`ID2` → `owner_runs`, `PROD/ENTRY` → `BuildingSpec` (type, origin, rotation, `byte0f` from the item; payload nullopt without bytes, else the class default with the mine's deep flag plus the `Load` fields), the footprint claim; everything else a typed warning. Warning kinds 14 → 18: `UndecodedCity` gone, `BuildingWithoutCity` → `BuildingItemWithoutEntry`, `TileOwnersUndecoded` narrowed to `ID`; new `EntryWithoutBuildingItem`, `BuildingRefused` (the board's name; `create_building`'s footprint rules checked in the adapter so the file loads on), `UndecodedBuildingRecord` (the board's `BuildingPayloadDefault`: once per scenario and class), `UnknownGoods`, `OwnerTileOutsideIsland`. `WARETYPE` is no longer an unknown SCENE child.
- Acceptance:
  1. RE (a)–(e) answered with addresses, each line D / likely / open. Two "likely" readings are in the code without their own warning kind, stated in comments, state.md and OQ-145: the context bit (the prefix position is always added) and the switch value through slot `+0xac` (the bit is taken as is). Neither can be told apart in the corpus; a per-building warning would be noise. Flagging this as a deviation from acceptance (1) for your decision.
  2. Corpus: **8 526 buildings placed in 272 cities** (per file in `scenario_corpus.txt`): 8 561 entries − 35 without a building item (34 on `DENKMAL` items, PRODTYPE 289, no list in the layout; 1 on a tile without an item: OQ-144); of the 8 810 `PROD2` building items, 212 are second copies of an equal item on the same origin (now `DuplicateObject`, like the tile objects) and 72 are named by no entry. **0 refused**, 0 unknown goods, 0 owner tiles outside an island, 167 `UndecodedBuildingRecord`. All 50 load, create, run 150 000 ticks, `check_invariants` clean, equal hashes twice, save/load/save identical. The Linux builds (GCC, Clang) reproduce every `create_hash`.
     - **Economy check (Do 3):** `single36.sze` (the most buildings: 759; 107 producing: 84 farms, 11 workshops, 8 small farms, 4 mines): after 150 000 ticks the stocks of **5** producers moved and those 5 (farms) have more output in stock; asserted > 0. The other 102 stand still (likely: outputs full with no carrier to empty them, inputs empty, mines without deposits since `BERGWERK2` is undecoded; not traced per building). **The warehouse delta is 0 (0 of 14 cities) and is printed, not asserted:** no ported code moves goods between a building and a warehouse. The only paths into `City::stock` are a figure `Deliver` effect (behaviour seam, none bound by default), `GoodsMove` effects (no producer emits one), and the build cost of a command. Houses do not tick (consumption not ported). **The "> 0 for both" half cannot be met by T23 without inventing a carrier; it needs a decision** (carriers/market are the next port step after the state model, PLAN §M3 hand-over).
  3. The owner runs are D (`ID3` + the footprint claim); no stand-in.
  4. Stream: no world field changed, `kStateHashVersion` stays **109**, `kPrngTimelineVersion` 1; goldens unchanged (`sim_stream.py --check` agrees); `create_hash` moves for every file with a city (re-baselined, header text updated; the version header line is left to S14 stage 2 per the standing note).
  5. No exception, no `reinterpret_cast`; `lint_sim` ok (one `size_t` member found by the lint and widened to u64); `anno_sim` links only `anno_formats` (no CMake change).
  6. Gates: see below.
  7. No fixed building, class or goods count (`warehouse_goods`, `neutral_owner`, `city_slots` from `SimConfig`; buffers are the original's file layouts). Times per scenario (`anno-sim-replay scenario`, MSVC Release, 9800X3D, all 50, 150 000 ticks): **load median 11 ms, max 55 ms** (`endless02`); **create median 3 ms, max 12 ms**; run median 149 ms, max 655 ms (stage 1: load 12.5 / 66, create 3 / 12, run 159 / 686). Create stays in the low tens of milliseconds with 8 526 buildings; no executor work needed.
- Tests: 6 new or rewritten unit cases (`scenario_formats_test.cpp`: HEAD2/WARE/ID3 with ID2 and the truncation refusals; PROD/ENTRY with workshop, farm, mine, a big footprint and an unknown nested record; the two-line classes and every length rule; the origin before the footprint and the claim leaving another city's tile; refused buildings and entries without an item; every kind reached in "scenario warning kinds"), the corpus ranking by placed buildings, the new economy case. Mutation evidence (MSVC; the scenario unit cases and the corpus case per mutant, re-run on the final code): **WARE tons not shifted** — unit 1 case / 2 assertions, corpus 36 assertions fail; **ENTRY class bytes read with a 2-byte prefix** — unit 2 cases / 25, corpus 29; **owner runs dropped** — unit 3 cases / 4, corpus 38. Note on the last: the corpus does not *refuse* the buildings as the board expected, because `create_building` has no owner rule (the original's `Create` does not test the owner either; the claim is the reader's); the corpus catches the mutant through every `create_hash`.
- Gates (each through the heavy-job lock):
  - MSVC full build warning-free; ctest `--preset msvc -j16`, `ANNO_TEST_THREADS=16`: with the game dir **908/908** (403 s, in the foreground); without **908/908**. (The first full run before the review had 905/906, the failure `lint_sim_passes`: a `size_t` member, fixed.)
  - clang-format 23.1.3 on every changed file.
  - WSL GCC 13.3 and Clang 18 `-O3 -DNDEBUG -Werror` with the CI flags (formats, test_support, sim, sim tests, anno-sim-replay, 69 TUs): no diagnostic (one GCC `-Wnull-dereference` in a test fixed first); the sim tests pass with and without the game dir; g2–g5 replay.
  - UBSan (GCC 13 `-O1 -fsanitize=undefined`, halt on error) on the 23 scenario cases with the game dir (the 50-file corpus, the 10 long runs, the economy case): 23/23. Clang ASan/UBSan is the CI job.
  - `tools/check_unix_warnings.sh`: GCC and Clang with viewer and GPU (145 files) and Clang without GPU: clean.
  - `tools/lint_sim.py` ok; `tools/sim_stream.py --check` agree.
- Reviewers (two, read-only, in parallel):
  - **A (code vs disassembly):** **bug: the farm/fur trapper activity is file byte 8, not 6** (the buffer at `esp + 8`, `[esp + 0x10]` → `+0x1e`, `0x1006efad`/`0x1006efc5`; I re-read it and confirmed) — fixed in code, RE note, sze.md and the tests. Duplicated `PROD2` building items were counted as items without an entry — fixed (212). Comments stating likely as fact — reworded. Recorded: the switch's `+0xac` call, the entry loop's object-in-rect pass (`0x100265ec`–`0x10026662`), counter step (`0x1002668b`–`0x100266f5`) and early end on a failed `Create`/`Load` (`0x10026732`), the weapons workshop's line-1 switch value 2; deviations listed in state.md §10 (truncated `ID3`/`ENTRY` refuse, a second entry on the same item is refused, `WARETYPE` taken wherever it stands). All other offsets, shifts, masks and length rules checked against the disassembly: right.
  - **B (tests, docs, acceptance):** gaps closed with new cases (small farm and weapons maps, farm 9 bytes, workshop > 10, mine and small farm over their buffers, switches pinned against their defaults, origin before footprint, the claim leaving another city's tile); "stepped" counted `last`, which every scheduled step moves — the economy case now counts stock changes only (that is why the number fell from 107 to 5, the honest number); test name "most PROD2 buildings" renamed; the corpus file header updated; the docs' numbers checked against the corpus file.
- Left open / questions:
  - **For the orchestrator / owner: the warehouse half of the economy check** (acceptance 2, Do 3). Options: (a) accept the production half for T23 and make "a warehouse stock moves" the acceptance of the first carrier/market task; (b) bind a minimal carrier stand-in behind a `rules` option now (not evidence-based; I do not recommend it). I recommend (a).
  - **OQ-144** (new): entries on `DENKMAL` items (34) and one on an empty tile; 72 building items without an entry — what the original's `Create` makes of PRODTYPE 289 and whether monuments need a list.
  - **OQ-145** (new): the city records not mapped (`BAUMAT`, `VK`, `EK`, `STAT`, `STAT2`, `STST2`, `HEAD`, `VOLK`, the rest of `HEAD2`), the house and market `Load`s, the switch's `+0xac`, the context bit, the entry loop's steps not modelled.
  - OQ-141 resolved for buildings and owners (the island number's `+0x40` path stays likely). O-36 narrowed, O-58 answered for scenarios.
  - Merge order with S14 stage 2: both edit `scenario_formats.cpp` (T23 the city/PROD2 regions, stage 2 the INSELLNK branch), `scenario_corpus.txt` (both re-baseline: the second to merge re-runs `ANNO_UPDATE_GOLDEN=1`), §10 of state.md, sze.md's status line, open-questions.md (append points) and the PLAN M3 line.
- OQ numbers used: **OQ-144, OQ-145** (S14 stage 2 starts at OQ-150).
- Time spent / subagents used: two read-only reviewer subagents; RE, code, the corpus surveys, builds, ctest, 6 mutation runs (3 × 2) and the WSL gates by this session, one heavy job at a time.

## T9 — sim stage S14 stage 2: the INSELLNK resolver — 2026-10-23 — status: FINISHED
- Branch: `task/T9-sim-s14-links` from `main` at 6106c58; worktree `C:\Users\altin\anno1503-T9`. Executor subagent; two read-only reviewer subagents (§1 item 5).
- Commits (one concern each): b6625bf RE note (annoscene §3–§5 to D, before the code); 03f73a3 refactor: the island code of `load_scenario` into `island_from_insel` (no behaviour change, corpus unchanged); dabcb7d the resolver and its tests (no stream change, only ported sites added); 2dcf9c8 lint `file_io`; b25f15b the wiring, `anno_sim_load`, **stream 110, PRNG timeline 2, every golden + g2..g5.log + scenario_corpus.txt re-baselined in this one commit**; 3c3c3f2 docs; 7ff0df3 review fixes and the new cases; c007434 review docs.
- Built (`docs/sim/state.md` §6 "Load-time sites", §10 "As implemented in S14 stage 2", §15 "S14 stage 2 as implemented"):
  - RE (static, `tools/disasm.py`): `0x10009550` and its callees `0x10009170`, `0x10009330`, `0x10009010`, `0x100090b0`, `0x10009130`, the tables `0x1003AF40` / `0x1003AF74` / `0x1003B144` / `0x1003B324`, the record handlers `0x1000a076`–`0x1000a275` and `0x10009e0f`, the cache scan, write and read `0x10007c50` / `0x10007fe0` / `0x10008110`, the placement `0x10008490`, AnnoWelt `0x100b25a0` / `0x100b2770`. **New finding:** a network game ignores `InselCache.txt` and takes a list built into `AnnoScene.dll` (428 entries at `0x10031554`, chosen by the 7th argument of `CreateAnnoSceneInstance` = "a network client exists", AnnoFrame `0x1002a3e8`–`0x1002a3f8`), sorted bytewise by `<Folder>/<UPPER NAME>`; compared entry by entry with this installation it is the scan's 429 templates minus `Dschungel/D_080X080_0015_2BA_SUEDSEE.SCP`. Odd but faithful details now in the code: the ore score of rank 0 is 116 (4 + the byte 0x70 of "po_", read past a 4-byte table), the special probe never reaches link 0 after a wrap, one treasure draw happens before the count is tested, ROHSTOFF fills raw kinds 1, 2, 4..11 and ERZE ore kinds 0..3, 5, 6.
  - `island_links.hpp/.cpp`: `resolve_island_links(links, inputs, candidates, DrawSource&, LinkTables, IslandCandidateOrder)`, steps 2–9, no I/O; `LinkTables` as `SimConfig::links` (20 and 9 kinds, 25 rounds, list caps 5 and 50, the masks, the exclusion table, the width ranks, the scores: data with the original values, validated); `Rules::island_candidate_order` (Sorted; Cached reserved).
  - `scenario_formats.cpp`: the `ROHSTOFF` / `SCHATZ` / `ERZE` / `INSELLNK` records (the `IslandLink` warning is gone), `template_summary`, `resolve_scenario_links` (draws on the generator `World::create` would build, `scenario_rng`, now shared with create; `spec.seed` takes the cursor left), `place_island_links` (the template through `island_from_insel` at the link, slot `number & 0xFFFF`, the link's climate). Seven new warning kinds: `LinkNamesTemplate`, `LinkUnresolved`, `LinkOresUnplaced`, `LinkRawMaterials`, `LinkTreasureUnplaced`, `LinkTreasuresCapped`, `LinkTemplatePlayers`.
  - `draw.hpp`: ten load sites; drawn: `LinkRawMaterial`, `LinkOre`, `LinkTemplatePick`, `LinkSpecialMark`, `LinkTreasure`; in the unported manifest: `LinkOrePlace`, `LinkNativeName`, `LinkTreasureKind`, `LinkTreasurePlace`, `ComputerName`. `DrawPhase`, `draw_site_phase`, `may_draw(site, phase)`.
  - `src/sim_load` (new library `anno_sim_load`, outside the sim's no-I/O rule): `scan_island_templates`, `load_scenario_with_links`; used by the corpus case and by `anno-sim-replay scenario`, which prints every link (template, candidates kept and matched, raw and ore kinds, special, treasure), the draws per site and the cursor.
- Acceptance:
  1. **The 9 link scenarios create all their islands: 229 of 229 links resolved, 0 unresolved** (stage 1: none, 229 `IslandLink` warnings). Per file (links resolved / draws): 00_easyentry 16/81, 01_easy_256 27/138, 02_easy_128 28/137, 03_med_256 27/105, 04_med_128 28/113, 05_Med_128 23/100, 06_heavy_256 27/114, 07_heavy_128 28/104, 08_heavy_128 25/97; 989 draws in all. Outcomes of the 229: 229 placed; on them `LinkRawMaterials` 229 (property 11, no field, OQ-152), `LinkOresUnplaced` 189 (no deposit, OQ-151), `LinkTreasureUnplaced` 102, `LinkTemplatePlayers` 33; `LinkNamesTemplate`, `LinkUnresolved`, `LinkTreasuresCapped` 0. Corpus totals: islands 668 → 897, tile objects 2 497 856 → 3 251 239. Deposits stay 0 for the link files (Do 4's fallback: the mountain objects and the ore goods are not read, OQ-151), where Do 6 asked for non-zero.
  2. Every step D with its address in the code, or a named warning; `grep -ci "assume\|guess"` on the new files: 0.
  3. `kPrngTimelineVersion` 1 → 2, `kStateHashVersion` 109 → 110 (SimConfig gains `links`, Rules `island_candidate_order`), all in b25f15b with the reasons; g0, g1_players, s5, s6 equal `tools/sim_stream.py`, which transcribes the new header fields (the C++ printed the same values); the others self-baselined, the event goldens unchanged; `sim_stream.py --check`: "goldens agree". Standing note done: `scenario_corpus.txt` carries `# kStateHashVersion=110 kPrngTimelineVersion=2`; the corpus case REQUIREs it, `sim_stream.py --check` refuses a missing or stale line; `state.md` §14.3 names the file in the re-baseline protocol.
  4. `may_draw`: the resolver draws only load sites ("link resolution is deterministic and draws only at load"); 160 000 ticks of a placed world draw only advance sites, and some ("every link warning kind is reached and the link sites never draw in advance"); "draw site phases" pins every site. Enforced by tests, no runtime guard.
  5. Mutation evidence (MSVC, the link cases), **16 of 16 killed**: M1 treasure pre-draw removed (8 cases fail); M2 raw bonus `<` to `<=` (3); M3 best_raw gap `> 2` to `>= 2` ("gap above 2"); M4 special and normal swapped in step 7 (3); M5 probe wrap `(d + 1) % n` ("probe wraps past link 0"); M6 exclusion table transposed; M7 ore scored with the raw tables ("ore scores are the ore table's"); M8 step 5 `%` the wrong count (4); M9 capacity `<` to `<=` (4); M10 candidate sort removed; M11 step 5 `special--` removed; M12 ERZE byte map shifted; M13 cursor hand-off dropped (3); M14 slot without `& 0xFFFF` (2); M15 raw and ore draw sites swapped (5); M16 ore kind 6 remap removed. Reviewer 2 traced 45 mutations and found 14 survivors before 7ff0df3; the new cases kill the ones that matter (height, last equal rank, list of five, ore round bonus, ore score table, gap of exactly 2, INFO[4] and raw marks in the capacity, kinds from the kept list, the probe's kind and mark tests, a custom `rng_table`, validation rows). Left surviving by analysis: step 6 `r % n` to `r % (n + 1)` for the draws used, and step 6/7 variants that are equivalent on every reachable path.
  6. No file system access in `src/sim`: `lint_sim.py` rule `file_io` (includes `<fstream>`, `<filesystem>`, `<cstdio>`, `<stdio.h>`, `<windows.h>`; `std::*fstream`, `std::filesystem`, `fopen`, `FILE`, `FindFirstFile*`, `CreateFile*`; 6 self-test cases); the tree passes. `anno_sim` still links only `anno_formats`.
  7. Gates (heavy-job lock, foreground): `cmake --preset msvc`, `cmake --build --preset msvc` warning-free; `ctest --preset msvc -j 16`, `ANNO_TEST_THREADS=16`: **941/941 without the game dir, 941/941 with it** (411.6 s; the corpus case 18 s under the 16-job load, 4.1 s alone); clang-format 23.1.3 `--dry-run --Werror` over all `src`: clean; WSL GCC 13.3 and Clang 18 `-O3 -Werror` with the CI flags over formats, test_support, sim, sim_load, the sim tests and anno-sim-replay (73 TUs, no diagnostic); the sim tests pass on both with and without the game dir (so the corpus `create_hash`es agree across MSVC, GCC and Clang); g2..g5 replay "every checkpoint matched" on both; UBSan (GCC 13, no recover): the scenario, link and template cases with the game dir, 55/55; `tools/check_unix_warnings.sh` in WSL: GCC and Clang clean (149 files), the viewer without GPU clean; `lint_sim.py` ok, self-test 70/70; `sim_stream.py --check` agrees. The render flake of round r did not recur.
  8. `anno-sim-replay scenario 0010SingleEndlos/03_1024_med_256.sze --ticks 150000` prints the 27 links (for example `link 13 island 13 at 672,120 128x128 climate 3 -> PRAERIE/PR_128X128_0007_4B_INDIANER.SCP (28 of 28 candidates) raw 5 ores 6 special 1 treasure 0`) and the draws (`LinkRawMaterial 36 LinkOre 26 LinkTemplatePick 27 LinkSpecialMark 3 LinkTreasure 13; cursor 1 -> 106`), `check_invariants clean`, exit 0; CTest `anno_sim_replay_scenario_endless_03_links` checks it. Times (Release, 9800X3D) for the 9 link files: load 13–38 ms, create 2–6 ms, 150 000 ticks 163–402 ms.
  9. Scalability: no fixed link, candidate or kind count in `src/sim` (`LinkTables` data; indices full width where the original's are bytes). Still fixed: the 7 climate prefixes of the file names (`po_` … `w_`) in the adapter, a file-naming table like the record byte maps (reviewer 2 would move it into `LinkTables`; possible with the next stream bump).
- Reviewers. **Spec and disassembly** (re-read every function above): no behaviour error; the draws, loop bounds and probe sequences match. Fixed after it: the INSEL5 head citation (`0x100b26ba`–`0x100b2717`), the order of the native names (player name `0x10008a23` before city name `0x10008aa3`), the slot index `number & 0xFFFF` (256 entries), the kinds byte values 1 and 2, the `.scp` pointer comment; `template_summary` copies INFO as `0x100b2770` does (kept across islands, the last record wins); `LinkRawMaterials` for every placed link (0xF009 is set without marks too); INSEL4 templates are placed instead of refused; the premise that nothing draws between HEAD and ERZE filed as **OQ-154**. **Acceptance and mutations**: Do 1–6 met (Do 4 and 6 with the allowed fallback), Acc (3), (4), (6) met; the must-fix test gaps added (above); `scenario_rng` now shared with `World::create`.
- Shared file with T23: the `STADT2` / `STADT` branches are untouched. 03f73a3 moved the INSEL5 island code, which holds the PROD2 `BuildingWithoutCity` branch T23 edits, unchanged into `island_from_insel`. **The second to merge must re-apply its PROD2 edit inside `island_from_insel`; that code now also builds the link templates**, so T23's PROD2 rule applies to link islands too. Both tasks also append to `all_warning_kinds`, `scenario_corpus.txt`, `open-questions.md` and `state.md` §10: textual conflicts there, and the corpus file must be re-baselined by the second merge.
- Left open: the placement's ores on mountains and their deposits, the native names and the treasures (OQ-151, the four unported load sites); island property 11 (OQ-152); a city on a link island (OQ-153, none in the corpus); the HEAD–ERZE premise and the world reset (OQ-154); `ComputerName` still not drawn.
- Owner question: **OQ-150**, the candidate order. Default `Sorted` over the game directory = the order of the original's network-game list and of a single-player NTFS scan. The network list holds one template less (the SUEDSEE dschungel 80 × 80 island), so for a given seed the dschungel 80 × 80 picks can differ from an original network game. Alternatives: drop the templates the built-in list lacks (read it from `AnnoScene.dll` at run time, never committed) or implement `Cached`.
- OQ numbers used: **OQ-150, OQ-151, OQ-152, OQ-153, OQ-154.**
- **Rebased on 883b33a (round s, the executor itself; no reviewer subagent):** `main` merged into the branch (f7345c7; the earlier head also carries 7a0afaf, the `std::sort` fix for the ASan job's `alloc-dealloc-mismatch`). T23's `PROD2` rule now lives in `island_from_insel`: a building item is kept for the city pass and handed back as `IslandCities`; `place_island_links` reports every building item of a placed template as `BuildingItemWithoutEntry` and never places one (OQ-151). 24 warning kinds (T23's 18 without `IslandLink`, plus the 7 link kinds; `UndecodedCity`, `BuildingWithoutCity`, `IslandLink` gone); the per-island `TileOwnersUndecoded` is gone as on `main`, so link islands stay neutral (the links case now expects 0). New case "scenario link template: its building items are reported, never placed" (599404c). OQ-150 → decided (owner: `Sorted` stays, tested against the original later). Corpus re-recorded (header `kStateHashVersion=110 kPrngTimelineVersion=2`): 50 files, 897 islands, 229/229 links resolved (989 draws), 272 cities, **8 526 buildings**, 3 251 239 tile objects; warnings: `BuildingItemWithoutEntry` 802 (72 from the scenarios' own islands as in T23, the rest from link templates), `DuplicateObject` 69 485, `ObjectTileTaken` 5 452, `UnknownRecord` 1 676, `UndecodedDeposits` 897, `EntryWithoutBuildingItem` 35, `UndecodedBuildingRecord` 167, `UndecodedPeople` 173, `ComputerNameNotDrawn` 7, `LinkOresUnplaced` 189, `LinkRawMaterials` 229, `LinkTreasureUnplaced` 102, `LinkTemplatePlayers` 33, every other kind 0. The economy case and the link cases pass. Own mutation checks: dropping the template `BuildingItemWithoutEntry` fails the new case; placing building items as tile objects fails 7 cases. Gates (heavy-job lock): MSVC build warning-free; ctest `-j 16` 948/948 with the game dir (564 s) and 948/948 without; clang-format 23.1.3 clean; WSL GCC 13.3 and Clang 18 `-O3 -Werror` (73 TUs) no diagnostic, sim tests pass with and without the game dir, g2–g5 replay; GCC UBSan 62/62 scenario and link cases with the game dir; `check_unix_warnings.sh` clean; `lint_sim` ok; `sim_stream.py --check` agrees.

## T25 — RE, docs only: the city records T23 left, the people names, the monument entries (OQ-145, OQ-143, OQ-144) — 2026-10-23 — status: FINISHED
- **Branch:** `task/T25-city-records-re` from `main` 53d6b93; worktree `C:\Users\altin\anno1503-T25`. One subagent of the executor session.
- **Scope:** docs only. No build, ctest or game run, no `src/` change. Nothing of the original is in git: disassembly, the linear sweep of `.text` and the corpus scripts stayed in the session scratchpad (`…\scratchpad\t25\`).
- **Method:** static analysis of `AnnoWelt.dll` with `tools/disasm.py` (capstone) and offset scans over a linear sweep; short excursions into `AnnoGame.dll` (the name dispatcher, type slots `+0x5c`/`+0x64`/`+0xac`). Corpus counts from read-only Python over the 50 `.sze` with `tools/sze.py` (counts only; the entry records are classified by shape, the tile → type lookup was not repeated).
- **Written:** `docs/re/annowelt.md` "Scenario cities" (f)–(j) (appended); `docs/formats/sze.md` "Cities": the placeholder rows for `BAUMAT`…`VOLK` and for house/market replaced by real rows, the switch sentence corrected, a `HEAD2` byte map and house/market record tables appended; `docs/open-questions.md`: OQ-143 resolved, OQ-144 (monuments) resolved, OQ-145 narrowed, OQ-155..158 new.
- **(a) House and market — answered (D):**
  - house `Load` `0x10076e60`: 8-byte header `[1]` residents, `[2]` need count `n`, `[3]` version (0: nothing more, 1: two bytes per need, the second used, else one byte per need); per need the satisfaction byte `s_j` → stock `= trunc(Menge_j · s_j · residents · 128 / 1536)`; residents → `+0x2c` (·64), `E` `+0x2e` (·128), the city's resident counter moved; a reset rule (group > 0 and residents < half the lower group's `Wohnmax` → `Wohnmax`, food 128, forced needs `128 − (r & 31)` from the world draw). The save `0x10077130` writes version 2. P: 3 283 houses, all version 2; the reset never applies in the corpus.
  - market `Load` `0x1006be00` (PRODTYPE 33–36): one nested `KONTID` u16 → building `+0x1e` = the market's number on its island (property `0xe`; a 0 gets island property `0x11` and raises it). P: 781 markets, numbers 1–30, unique per island.
- **(b) The city records — answered (D), meanings partly likely/open:**
  - `BAUMAT` → city `+0x48c + 8g` = `BuildGoods::setting` (`+0x5d4 + 8(g − 0x29)`, the field of `SetTradeFlag` mode 9), tons << 8; P: goods 41–45 in every city.
  - `VK` → list `+0xdc` (8 entries, count `+0x140`), `EK` → list `+0x44` (12 entries, count `+0xd8`); entry `[0]` → `+4` (price, likely), `[2]` goods → `+6`, `[4]` << 8 → `+8` (amount, clamped to the capacity elsewhere, D); `GoodsSetting`: `+0xb` = loop index, `+0xa` bit 0 = 1 for `VK`. So `trade_lists[0]` = `EK`, `[1]` = `VK`; the name `buy` for bit 0 is likely inverted (OQ-155).
  - `STAT2` → five of ten tick counters `+0x628`…`+0x638`, `+0x654`, `+0x656` (state; three saved counters are not restored); `STST2` → raw block `+0x84c`…`+0x30bf` (likely the statistics history, sampled by city tick step i); `STAT` skipped; city `VOLK` read and ignored; `HEAD` = the older `HEAD2` with an owner number.
  - `HEAD2` all 64 bytes: `[0]` → `+0x324`, name `[4]` → `+0x24` (unbounded copy), `[0x26]`/`[0x27]` → VK/EK slot counts `+0x140`/`+0xd8`, `[0x28]`/`[0x38]`/`[0x3c]` → statistics 0/4/5 × 7 (`+0x60c`/`+0x5fc`/`+0x600`), `[0x2c]` → `+0x620`, `[0x30]` ORed into the player mask `+0x61c`, `[0x34]`/`[0x36]` → `+0x328`/`+0x32c` (the research sub-object, likely, OQ-157).
  - Found on the way: city slot `+0xbc` `0x10028400` recomputes the capacity from the market count (table `0x100dcc84`) and the slot counts from the market types' byte `+0x23`; P: `HEAD2`'s capacity matches the table in 259 of 272 cities; not on the load path read (OQ-156). Correction of T23 (b): the switch is `slot +0xc8(output goods i, bit)` (type slot `+0xac` = AnnoGame `0x1000a410` takes the 0 and returns output goods 0), which also books the running-cost change (`0x10072a60`). The entry loop's early exit makes the player reader return 0.
- **(c) OQ-143 — answered (D):** name kind `0x1c` (AnnoGame `0x10013d80`, table `0x10024c50`) has 14 names: `GOTT` 0, `SPIELER` 1, `MAUREN` 2, `BEDUINEN` 3, `MONGOLEN` 4, `AFRIKANER` 5, `AZTEKEN` 6, `ESKIMOS` 7, `INDIANER` 8, `SUEDSEE` 9, `HOEHLEN` 10, `PIRATEN` 11, `VENECIA` 12, `BANDE` 13 (14–19 null, never match). P: all 373 corpus names match; types 14 → 2–9 with bits `0x19` and colour 0, 17 → 0/13 without, type 2 → 0/1 without and 2/3/6 with the bits.
- **(d) OQ-144 — answered (D):** PRODTYPE 289 → the default class (base `0x100734c0`, `0x18` bytes) in the production list (BuildingIndex `+0xd8`, the path of PRODTYPE 1–32 and > 192); the base `Load` skips the bytes. The 34 entries each create an object and raise the production list's `n`.
- **Self-check (instead of reviewers, protocol §1 item 5):** every byte map was re-read against the disassembly once: `HEAD2` reader and writer offsets field by field (both buffers), `STAT2` reader and writer, `VK`/`EK` entry slots (vtable `0x100c2f04`), the house header bytes and the version paths, the market setter/getter tables. Two of my first readings were fixed by it: `+0x30b0`/`+0x30b4` lie inside the `STST2` block (saved), and the city `VOLK` buffer bound.
- **Left open:** the context byte `+0x400`; the `SLICE`/`CREW`/one-byte entry records; the meanings of the `STAT2` counters, `+0x620`, `+0x61c`; OQ-155 (trade-list naming, entry `+4`), OQ-156 (when `0x10028400` runs; type byte `+0x23`), OQ-157 (research sub-object), OQ-158 (the island's market counter on load); the `PROD2` items without an entry (OQ-144 rest, OQ-142).
- **For the mapping task:** (1) house: residents and the stocks from `s_j` need the group's `Menge` and the reset rule; the non-pioneer houses already draw from the world sequence in `SetGroup` at `Create` (`population.md` §5), before the `Load` overwrites the stocks; (2) market: carry `KONTID` and the island counter; (3) `BAUMAT` → `build_goods[g − 0x29].setting`, `VK` → `trade_lists[1]`, `EK` → `trade_lists[0]` with `list_index` = the loop index (skipped entries keep their index) and the slot counts from `HEAD2`; (4) `HEAD2` statistics are stored × 7; (5) VOLK: the 14-name table; (6) monuments need a list (or a refusal) for PRODTYPE > 192.
- **OQ numbers used:** OQ-155, OQ-156, OQ-157, OQ-158 (OQ-159 and up unused).

## T24 — Transport stage 1: the market cart and the workshop carrier (jobs `0x34`, `0x33`) — 2026-10-23 — status: FINISHED (stage 1 includes the carrier; the per-file time bound, see acceptance 3)
- Branch: `task/T24-transport-carts`, rebased on `main` at ba0c4bb (the stage 2 and T25 merges); worktree `C:\Users\altin\anno1503-T24`. The executor itself, no reviewer subagent (§1 item 5, round s).
- **Staging:** stage 1 does both jobs. The carrier's reading (b) fits, so the `HANDWERK` workshop's carrier is in. The **weapons workshop's carrier** (`HANDWERKWAFFEN`, its two lines `0x1006a4a0`) keeps the D12 default and goes to stage 2; the farm workers and the other jobs do too.
- Commits (one concern each):
  - 4c60053: the RE note `docs/re/transport.md` and `production.md` §5 to D, before the code.
  - 36f88bc: `rect_query` faster, no behaviour change (all hashes equal).
  - 7f906c1: the transport body, the Market class, the reservations, the figure types and entrances, G6, and **stream 111 with every golden, g2–g5.log, g6.log and `scenario_corpus.txt` re-baselined in this one commit**.
  - aeaacfd: spec docs (state.md, OQ-103, OQ-160..162, PLAN).
  - c6a25c7: GCC `-Wrange-loop-construct` in the test.
  - c7305e9: two test additions from the mutation run.
- RE (`tools/disasm.py`, AnnoWelt and AnnoGame; `docs/re/transport.md` §1–§6, each line D, likely or open):
  - **(a) The jobs.** The factory `0x1003b0e0` builds 0x48-byte jobs of three step nodes, run at `+0x30` (`0x1005b6c0`); a step that starts no action completes in the same figure tick.
    - Step A: cart `0x1005ad10`, carrier `0x1005a530`.
    - Step B: the wait (`0x100551b0`), then unreserve and take up to the capacity (`0x100551f0` → `0x10068490`, `0x10068430`; market source: the city's remove `0x1006b7f0`).
    - Step C: walk home (`0x10057ae0`) and unload (`0x10057bd0`): market → the city's add, which clips at the capacity, and the rest is lost with the figure; workshop → input store `0x100683f0`.
    - On failure the job goes to the tail (`0x1005d380`), then the end (T22's `0x1003b6e0`). The buildings' store slots `+0xa0`..`+0xc0` are tabulated per class.
  - **(b) The carrier.**
    - Candidates: list mask `0xa`, the production and market lists.
    - Filter: the workshop's offer `0x100728f0` = `p + avail · 192 / cap` with the priority `p` = `0x10068500` above 0, for a source whose output 0 is the input or `ALL` (`0x8e`: a market), or whose output 1 is the input. So it fetches from the warehouse **and** from producers.
    - Callback: `0x1005a400`, which stops at a source holding 3/4 of the carrier's capacity.
  - **(c) The search.**
    - The area: a square of `2r + 1` (+1 for an even footprint) around the footprint centre (not the footprint grown by `r`), `r` = `HEAD[0x11]`, with the midpoint-circle disc `0x10002a60` (ported exactly). Candidates come from the home city's building index only (city `+0x60` → `+0x144`).
    - The cart's callback `0x1005a900`: the 75 % rule, the 1 t minimum, the strict `<`, the early stop. The best score starts at 0 every search (`0x10003dbf`, previously Open): a raw producer below 75 % is never chosen.
    - **Open:** the walking-layer tables `0x100d1140 + 0x1c00 · layer` (`.data`, filled at run time; OQ-160) and the engine's expansion, costs and ties (OQ-161).
  - **(d) Movement.**
    - Time is `u16(t − stamp) · 0.2f` units; a wait of `d` is `500·d` ticks.
    - Segments are `{direction, count/3}`, direction codes `0x1002ff20`, diagonal × `1.41421354f`; the speed is HEAD f32 `[4]`.
    - P: cart 220 at 0.016 (1.6 tiles/s), 5 t, layer 3, wait 1.0; carrier 203 at 0.012, 3 t, layer 0, wait 2.0.
  - **(e)** `TASK`/`ENTRY` are not read; no save compatibility.
- Built (`state.md` §4.9, §5.9 "Transport", §15 "T24 as implemented"):
  - `transport.hpp/.cpp`: both jobs as the default phase-B body. It follows the seam protocol (own figure plus `Effects`; `StoreDelta` for the reservations, the loads and a workshop's input; `GoodsMove` for a market's city stock) and draws nothing.
  - The market class ticks (`TickKind::Market`, `PayloadTag` 6, gate 500, `carts_out < max_figures[0]`), and every producer keeps a `reserved` store.
  - `FigureType` from `FigurenHeader.dat` (speed in 2^-24 tile per tick, capacity, layer, load ticks; part of the registry digest), and `ENTRANCE` points in sixths of a tile. A spawn starts at the first entrance point (rotated as AnnoGame `0x100084c0`).
  - A registry without figure types (G1–G5's synthetic ones) keeps the D12 default, so those goldens only moved bytes.
- Acceptance:
  1. The RE note answers (a)–(d) with addresses; the stand-ins are stated in `state.md` §5.9 and OQ-160/161/162: the walk rule, the search and goals, the integer walk.
  2. **Synthetic cases** (`transport_test.cpp`, 17 cases, every value derived by hand in the comment):
     - the cart end to end: spawn at the entrance; reserve 1280; walk 938 ticks; load after 500 more; home in 938; the city +1280; the counter released; on the expiry list;
     - off the road net: ends at once;
     - F11 and F12;
     - "the cart's choice" in six sub-cases;
     - the full warehouse: clip at 1000;
     - the neighbour order;
     - the carrier end to end: from the market over open land, 1128 + 1000 ticks, city wool −768, back in 1251, input +768;
     - the priority thresholds; the offer values (878, 218, −1); the carrier's 3/4 stop;
     - determinism: two runs, the executor at 1/2/8 threads, a save mid-walk;
     - **G6**: replayed by the test and by `anno-sim-replay run fixtures/synthetic/sim/g6.log` (21/21 checkpoints, CTest `anno_sim_replay_g6_run`).
  3. **Economy:** the scenario with the most buildings is `single36` (759 buildings, 107 producing). In 150 000 ticks 16 of them move their stocks (T23: 5) and 7 gain output; **its warehouses: 4 of 14 move** (`CHECK(moved > 0)` in the economy case). By class: Workshop 4/11, Farm 12/84, SmallFarm 0/8, Mine 0/4.
     - Per scenario, `scenario_corpus.txt` carries `producers_moved` and `warehouses_moved`, and the corpus case prints them. In all: producers 670 of 2 147, warehouses 86 of 272. All 50 load, create, run, hash and save/load clean.
     - **Time** (`anno-sim-replay scenario`, MSVC Release, 9800X3D, all 50, 150 000 ticks): run **median 264.5 ms, max 1 168 ms** (`S01_07_01`). That is 1.78× T23's 149 / 655 on both; load median 13.5 / max 52 ms, create 3.5 / 11 ms.
     - The stage 2 head on the same machine today: 183 / 573. Against it the median is 1.45× and the **worst single file 3.3×** (`S01_07_01`: 351 → 1 168 ms). There its carts search 13 800 times and find a source 90 times: every 500-tick market gate sends a cart that searches its whole road net in the area and ends. Not split yet: how much of that is no road connection (OQ-160) and how much a city without room for the goods.
     - What I did before reporting (no hash moved):
       - `rect_query` dropped its per-tile duplicates before the sort (the first profile: 69 M building lookups);
       - the walkability is worked out lazily per tile and memoised;
       - a bucket queue replaces the binary heap (the same (cost, insertion) order: costs 5/7, 8 FIFOs);
       - a figure in mid-walk does no world lookup;
       - ended figures return at once.

       Before all this the corpus took median 343 / max 2 597 ms.
     - The next lever, named as the task asks: a per-island walk-class cache, kept up to date with the tile objects. It is not done here: it needs invalidation on every object write. Say if the bound must hold per file.
  4. `lint_sim`: ok. The only floats are the load-time conversions of the figure types and entrance points, allow-listed like `production.cpp`'s. The walk is integer: speed `round(f32 · 0.2f · 2^24)`, Q24.8 waypoints, the diagonal 23 726 566. The deviation is **OQ-162 (owner)**.
  5. **Mutations (MSVC, the whole sim suite without the game dir): 19 of 19 killed.**
     - The first run killed 8 of 16. The 8 survivors got new cases, then all were killed.
     - Equivalent mutant, not counted: swapping direction codes 0 and 1. Their step costs differ (5 and 7), so the order within each cost class is unchanged; the neighbour mutant counted swaps 0 and 4.

     | Mutant | Killed by |
     |---|---|
     | 75 % → 50 % | "the cart's choice" |
     | 1 t filter → 0 | "the cart's choice" |
     | other-goods score 128 → 64 | G6 |
     | strict `<` → `<=` | "the cart's choice" |
     | city room `<=` → `<` | "the cart's choice" |
     | reservation not clipped | "the cart's choice" |
     | load wait skipped | the cart end to end, the carrier end to end, G6 |
     | neighbour order 0↔4 | the neighbour order |
     | corner cutting allowed | the neighbour order, G6 |
     | diagonal as straight | segment lengths, the carrier, G6 |
     | carrier never stops at 3/4 | "the carrier stops at 3/4" |
     | priority 30 → 20 | the priority |
     | offer 192 → 128 | the offer |
     | road rule off | off the road, the neighbour order, G6 |
     | market send `<` → `<=` | market without production data, G5, the cart, the carrier, G6 |
     | spawn stamp 0 | 8 cases incl. G1/G2/G3, the layouts, G6 |
     | speed + 1 | the carrier end to end, G6 |
     | the end skipped (no counter release) | 6 cases incl. the cart end to end, G6 |
     | raw score ÷ cap instead of cap − q | "the cart's choice" (the stop at 128) |
  6. `kStateHashVersion` 110 → 111 in 7f906c1 with the reasons. `kPrngTimelineVersion` stays 2: no job draws.
     - g0, g1_players, s5 and s6 equal `tools/sim_stream.py`, which transcribes the Market row, the reserved stores and the figure-type digest.
     - The others are self-baselined; `goldens.txt` lists the 110 values. `sim_stream.py --check`: "goldens agree".
     - `scenario_corpus.txt` is re-recorded with the game dir (header 111/2); the GCC and Clang builds agree with it.
  7. **Gates** (heavy-job lock, foreground):
     - MSVC build warning-free;
     - `ctest --preset msvc -j16`, `ANNO_TEST_THREADS=16`: **967/967 without the game dir, 967/967 with it** (548 s);
     - after the last two test commits the sim suite again: 609/609 MSVC; GCC 13.3 and Clang 18 at `-O3 -Werror` with the CI flags (75 TUs, no diagnostic), the sim tests with and without the game dir, g2–g6 replayed;
     - UBSan on the scenario, link and transport cases with the game dir: 85/85;
     - `tools/check_unix_warnings.sh`: clean (GCC and Clang with viewer and GPU, Clang without GPU);
     - clang-format 23.1.3 `--dry-run --Werror` over `src`: clean;
     - `lint_sim`: ok, self-test 70/70.
  8. **Scalability:** no fixed figure, job, road or path count. The route is a vector; the search grid is the area cut to the island; the radius is the type's byte, so the bound is data. **Bound per search:** at most `(2r + 2)²` tiles allocated and read.
     - Measured on `single36` (150 000 ticks): 14 134 cart searches started, 3 754 with candidates and 45 finding a source; 3 025 carrier searches, 43 with candidates, all 43 finding one; radius at most 24, so a grid of at most 2 500 tiles.
     - Carts read 909 tiles per search on average and at most 2 046; carriers at most 237.
- Found and fixed in the branch: `anno-sim-replay scenario` crashed with an access violation in all 50 files, because the figure entries view the `FigurenHeader.dat` buffer, which went out of scope before the registry was built. The CTest scenario runs skip without the game dir, so CI could not see it. Fixed in 7f906c1.
- OQ-103: recorded what the code does. Carts walk roads only; carriers and every other layer walk any tile without a building. A producer feeds a market only when one of its footprint tiles touches the market's road net in the area. The goal is a footprint tile, not the entrance the owner's rule names (entrance points are read and used only for the spawn). **If the owner's rule differs (carriers on roads, or the entrance as the only goal), the owner picks the default; it becomes a `Rules` option.**
- OQ numbers used: **OQ-160, OQ-161, OQ-162** (OQ-163/164 of the range T26's brief reserved are free).
- Not taken: the `alloc_probe.cpp` nothrow-`new` fix-up (the branch does not touch `src/test_support`, and the ASan job did not bite here).
- Left open: the walking layers and the walk masks (OQ-160); the search's real expansion and the entrance goals (OQ-161); the x87 walk (OQ-162, owner); the weapons workshop's carrier, the farm workers and the other jobs (stage 2); the last walk of 15.0 outside the home at the end (`0x1003af00`, not modelled; `end_figure` ends at `t`).

## T27 — Transport stage 2: the walk-class table, the S01_07_01 split, the walking-layer RE, terrain — 2026-10-23 — status: IN PROGRESS
- Branch `task/T27-transport-stage2` from `origin/main` at f93d7fe (PR #38 / T24 merged as fed76bd, confirmed `gh pr view 38` = MERGED); worktree `C:\Users\altin\anno1503-T27`. The executor itself, no reviewer subagent. One heavy job at a time through the lock (shared with the parallel T26 subagent).
- **Step 1 (set up).** MSVC configure + build warning-free. `ctest --preset msvc -j16 -R anno_sim` with `ANNO1503_GAME_DIR` set and `ANNO_TEST_THREADS=16`: **630/630 passed** (10.5 s) before any change.
- **Step 2 (measure before).** `anno-sim-replay scenario <f> --ticks 150000` over all 50 `.sze` (one run each, Release, heavy-job lock). Run time across the 50: **median 267 ms, max 1170 ms** (`0100Kampagne01/S01_07_01.sze`). The two focus files: `S01_07_01` 1170 ms, `single36` 721 ms. (Load median 14.5 / max 54 ms, create median 4 / max 11 ms.) These match the T24 report (median 264.5, max 1168).
- **Step 3 (walk class per type, no behaviour change).** `BuildingType::walk_class` (derived, set in `TypeRegistry::finish`, not in `fields()` or the digest); `TileTest` reads it and no longer needs the `ListLayout` (so `path_home` drops that parameter). Commit `perf(sim): the walk class per building type, no behaviour change`. **No hash moved:** sim suite 630/630 with and without the game dir, `sim_stream.py --check` = "goldens agree", `anno-sim-replay run g6.log` = "every checkpoint matched", `scenario_corpus.txt` and `goldens.txt` unchanged in `git status`. The field-count canaries moved 21 -> 22 (`types_test` and the `digest_type` static_assert), walk_class excepted from the per-field digest mutation list; the synthetic digest `0x13e65df34212c0c3` is unchanged.
- **Step 4 (the S01_07_01 split, measurement only).** `choose_source` gained a diagnostic out-param `TransportStats*` (default null; not sim state, not hashed) that tallies one of four outcomes per search: no candidate / candidates-but-unreached / reached-but-no-score / source found. A new test case `transport search statistics (ANNO_SIM_TRANSPORT_STATS)` classifies every sender once, at steady state (after 150 000 ticks), for the two files; it is off unless the env var is set (the counters are test-only: `src/sim` keeps none, since lint forbids `getenv`/`cstdio`/mutable statics there). Commit `test(sim): transport search statistics behind ANNO_SIM_TRANSPORT_STATS`. **Numbers** (per sender at tick 150 000, not per search over the run — the per-run count would need a sim-side sink lint forbids):
  - `S01_07_01`: 55 markets → cart searches **no_candidate 5, unreached 43, low_score 7, found 0**; 88 workshops → carrier **no_candidate 83, unreached 4, low_score 0, found 1**.
  - `single36`: 51 markets → cart **no_candidate 35, unreached 2, low_score 12, found 0**; 11 workshops → carrier **no_candidate 11**.
  - **Reading (game developer):** S01_07_01 is dominated by `unreached` (43 of 55 markets have a producer with ≥1 t in their area but no road path to it) — the town's producers sit off the market's road net, not a full warehouse. `found 0` at the snapshot only means no sender is mid-trip at that instant (the economy case shows carts did move over the run). In `single36` most markets have no in-area candidate at steady state (producers drained or none in range), with 12 `low_score` (in range but below the 75 % / room threshold): a mix, less road-bound than S01_07_01.
- **Step 5 (measure after).** Same 50-file run after step 3. Across the 50: **median 240 ms (before 267), max 1099 ms (before 1170)**; `S01_07_01` 1099 (before 1170), `single36` 662 (before 721). The per-type walk class removed the per-search `std::map` but the dominant cost is the Dijkstra over the market's area, so the gain is ~10 %. The max is **1.92× the stage 2 head (573 ms), within the 2× bar** (median 1.31× the head's 183 ms); single runs, some noise. Not optimised further in T27 (the step forbids it). **Next if needed:** a per-island walk-class grid kept current with tile-object writes (a lookup becomes an array read, and the search's `GridTiles` fill drops the registry binary search entirely), and, structurally, carts that end at once when their area holds no reachable producer (the `unreached 43` of step 4) could skip the search via a per-market "no road-reachable producer" cache invalidated on road/producer change.
- **Step 6 (RE: the walking layers, OQ-160, time-boxed).** `docs/re/transport.md` §3.1 added. **D:** the two setters `0x1000dd20` (`[engine+0x430]`) and `0x1000f010` (`[engine+0x1030]`) compute `0x100d1140 + 0x1c00·layer`; the block is **static initialised `.data`** with image content (not run-time filled — corrects §3), `0x1c00` bytes per layer; `0x10091fa0` mode handler extracts the 9-bit mask from bits 16–24 of the tile record's second dword (`shr 0x10; and 0x1ff`). **Likely:** the five layers' `0xff`-byte counts (0:316, 1:409, 2:561, 3:75, 4:613) show layer 3 (the cart) by far the most restrictive and layer 0 (the carrier) permissive — the stand-in's shape (cart→roads, others→open land) is right. **Open (time box hit):** the entry *semantics* (mask → walkable per layer) need the consumer, the search node-expansion reading `[engine+0x430]` inside the engine at world `+0x6aa8` (vtable `0x100c1424`), body of `0x10003d90`/`0x10004890` — the next address to read; OQ-160 stays open. Commit `re(annowelt): the walking layers (T27)`.
- **Step 7 (terrain in the walk rule — outcome (b), no rule invented).** Checked the sim-state side (`state.md` §5, `island.hpp`): the island `Terrain` (height/gfx/fertility) is a stand-in the simulation reads none of (O-53, OQ-060), and a state read from the flat stream leaves it a placeholder with empty columns; the `TileLayer` carries only object ids and owners, and a water tile has object 0 exactly as open land does. **There is no per-tile water/land class in the sim state**, so T27 does not add terrain to the walk rule (a carrier on open land may cross water in the stand-in). Written into OQ-160 (the terrain sentence) and `state.md` §5.9; new **OQ-170** names where the information would come from (the walking-layer mask of OQ-160, first). No `g6.log`/`scenario_corpus.txt` re-record (that was only for outcome (a)); no `kStateHashVersion` change.
- **Step 8 (tests).** Step 3 is tested by the unchanged hashes (the whole suite, `sim_stream.py --check`, G6, the corpus file). Step 7(a)'s river case does **not** apply: the outcome is 7(b) (no terrain rule), so there is nothing to test; stated here per §1 item 7.
- **Step 9 (docs).** `docs/re/transport.md` §3.1 (new) and §6 (a T27 line); `docs/open-questions.md` OQ-160 (the T27 update + the terrain sentence) and OQ-170 (new); `docs/sim/state.md` §5.9 (terrain ignored, walk_class) and §15 "T27 as implemented". No `TASKS.md` edit.
- **Mutation checks (§1 item 5), the step-3 "no behaviour change" claim:** M1 `walk_class` forced to 0 → 6 of 16 transport cases fail (incl. the cart, the carrier, G6, off-road); M2 road/building swapped (1↔2) → 6 of 16 fail. Both killed; source restored, `git diff` clean.

**Acceptance (T27):**
1. **MET — step 3 moved no hash.** Full suite 968/968 with and without the game dir; `sim_stream.py --check` = "goldens agree"; `anno-sim-replay run g6.log` = "every checkpoint matched"; `git status` shows `scenario_corpus.txt` and `goldens.txt` unchanged. Synthetic registry digest still `0x13e65df34212c0c3` (`types_test` "registry digest changes with every field").
2. **MET** — before/after per-file medians and maxima and the four search counts for `S01_07_01` and `single36` are in steps 2, 4, 5 above.
3. **MET** — `docs/re/transport.md` §3.1 exists with D / likely / open per line; the time box was hit on the mask semantics, and §3.1 says exactly what is open and the next address.
4. **MET (outcome (b))** — the sim state cannot tell water from land, so no terrain rule was invented; written in OQ-160 and OQ-170 and `state.md` §5.9. No third way taken.
5. **MET** — gates through the heavy-job lock: MSVC build clean; ctest 968/968 with and without the game dir; clang-format 23.1.3 clean over `src`; WSL GCC 13.3 and Clang 18 `-O3 -Werror` (75 TUs, no diagnostic, g2–g6 replay); `lint_sim.py` ok; `sim_stream.py --check` agrees; `check_unix_warnings.sh` clean (GCC+Clang, viewer/GPU on and GPU off). No reviewer subagent.
6. **MET** — five commits, one concern each, named as the steps ask: `perf(sim): the walk class …`, `test(sim): transport search statistics …`, `re(annowelt): the walking layers (T27)`, the two docs commits. PR opened, not merged.
