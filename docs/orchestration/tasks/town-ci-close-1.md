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
| GCC Release freight compilation | Exact warning flags compile corrected path | Reproduce original diagnostic; compare unchanged assignment behavior | Saved compiler outputs, existing freight tests | Local review approved; exact-source CI pending |
| Clang C++20 doctest compatibility | Real doctest includes the supported standard header | Original dependency include triggers obsolete-header failure | Minimal include/control plus test build | Local review approved; exact-source CI pending |
| Optional timing diagnostics | x86 GCC Release compiles the nonempty-guarded helper; public runner still prints paired samples | Original x86 compile fails; empty input cannot be indexed | Compiler controls and stats-enabled runner test | Local review approved; exact-source CI pending |
| Immutable zoom table | Existing test reads original table by const reference | Original copied table triggers x86 GCC warning; assertions unchanged | Compiler pair and existing zoom case | Local review approved |
| Native Mine decoding | Present/absent byte vectors, all strict prefixes, reload and G3 | Original hardened layout test aborts on disengaged optional; fixed 28/28 | Hardened and sanitizer gates | Local review approved |
| No gameplay/identity drift | Existing freight assignment, lifecycle, save/replay cases | Refusals and replacement controls retain behavior | Focused canonical Debug and sanitizer tests | Local review approved; exact-source CI pending |
| Integration | All exact-head CI jobs pass, including corrected town GCC linkage | Preserve original failed jobs and any new failures | Run id and full head | Local review approved; exact-source CI pending |

Work: (1) preserve CI annotations and reproduce minimal compiler failures; (2) make the smallest
portable source/dependency correction without suppressing warnings; (3) focused validation;
(4) independent read-only review per AGENTS.md, including a review subagent; (5) push this task
branch, watch exact-head CI and close remaining failures within this scope. Original files and
large logs remain private; committed evidence contains commands, hashes and compact findings only.
Use resource-limited owned containers/builds and admission control; never stop another lane.
Do not repeat the town experiment if the change has no path into its commands or results;
justify reuse explicitly. Stop at a clean reviewed CI-green candidate. Landing stays with the
Windows orchestrator. Report the cherry-pickable repair delta for other branches; do not modify them.

## Reviewed checkpoint, 2026-11-18

Frozen source `283e8f9bf1f080caad8d3d0ef659ad3ad46fa04c`: four small repair commits after
156badab. Local independent review approved; hardened 28/28 and affected Debug/ASan 56/56 each.
Full CI [44261293763](https://github.com/Dom-cmyk727/anno_rebuild/actions/runs/44261293763)
is pending at this documentation checkpoint. The Mac lane review determines acceptance once
that exact-source run is green. The REPORTS entry preserves every failed precursor and the
subsequently exposed zoom-test warning and real Mine deserialization bug. No automatic
wildlife intake or main landing is authorized.
