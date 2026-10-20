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
