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
