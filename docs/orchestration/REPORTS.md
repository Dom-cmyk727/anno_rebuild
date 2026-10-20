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
