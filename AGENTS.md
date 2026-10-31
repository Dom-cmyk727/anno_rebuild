# ANNO 1503 Rebuild — instructions for coding agents

Read [CLAUDE.md](CLAUDE.md) for the common project rules. It applies to every
coding agent, regardless of client. Keeping one copy avoids divergent hardware,
memory-admission and gameplay instructions.

Before acting, read [docs/PLAN.md](docs/PLAN.md) and the current task packet.
Executors also read [docs/orchestration/TASKS.md](docs/orchestration/TASKS.md),
except where their current packet explicitly replaces that historical board.
Only the orchestrator updates assignments; executors report their work in
[docs/orchestration/REPORTS.md](docs/orchestration/REPORTS.md).

At task start and before reporting a package complete, read and follow:

- [The mandatory test/evidence contract](docs/testing/evidence-contract.md).
- [The shared gate commands and adoption procedure](docs/testing/README.md).

Every substantive package requires independent read-only review, including the
tests and evidence that support its claims. Use a review subagent when available;
otherwise mark independent review pending. Fix actionable findings and obtain
re-review before completion.

Existing work adopts this policy at the next package checkpoint. Do not interrupt
a running gate, reset another worktree, alter its coordinator or start an unrelated
repair campaign. Preserve the assigned scope and its stopping criteria.

These tracked instructions were authorized by the owner on 2026-10-31. Historical
task-board text saying that AGENTS.md is an untracked duplicate or must never be
committed is superseded for maintaining the project instructions. This does not
authorize executors to rewrite task assignments or change gameplay policy.
