# TOWN-CI-CLOSE-1 — isolated CI portability closure

Owner authorization, 2026-11-18: fix the remaining CI blockers separately without interrupting
WILDLIFE-1a. Author: Mac orchestrator, on task/TOWN-CI-CLOSE-1 from town report
156badab35516609e92796e85a4fb0b9314ddac6. No main push, force, other-worktree edit or wildlife intake.
This explicit repair authorization supersedes the earlier town packet's no-repair scope only here.

Read CLAUDE.md, PLAN sections 0.1–0.3, evidence-contract.md and testing/README.md.
The existing town measurement/review remains preserved. No gameplay, state, PRNG, fixtures,
warning thresholds or CI matrix coverage may change to make a build green.

| Requirement | Positive proof | Negative/control | Evidence | Review |
|---|---|---|---|---|
| GCC Release freight compilation | Exact warning flags compile corrected path | Reproduce original diagnostic; compare unchanged assignment behavior | Saved compiler outputs, existing freight tests | Pending |
| Clang C++20 doctest compatibility | Real doctest includes the supported standard header | Original dependency include triggers obsolete-header failure | Minimal include/control plus test build | Pending |
| No gameplay/identity drift | Existing freight assignment, lifecycle, save/replay cases | Refusals and replacement controls retain behavior | Focused canonical Debug and sanitizer tests | Pending |
| Integration | All exact-head CI jobs pass, including corrected town GCC linkage | Preserve original failed jobs and any new failures | Run id and full head | Pending |

Work: (1) preserve CI annotations and reproduce minimal compiler failures; (2) make the smallest
portable source/dependency correction without suppressing warnings; (3) focused validation;
(4) independent read-only review per AGENTS.md, including a review subagent; (5) push this task
branch, watch exact-head CI and close remaining failures within this scope. Original files and
large logs remain private; committed evidence contains commands, hashes and compact findings only.
Use resource-limited owned containers/builds and admission control; never stop another lane.
Do not repeat the town experiment if the change has no path into its commands or results;
justify reuse explicitly. Stop at a clean reviewed CI-green candidate. Landing stays with the
Windows orchestrator. Report the cherry-pickable repair delta for other branches; do not modify them.
