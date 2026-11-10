# Review: main's branch protection and the landing tool (2026-11-10)

**Package:** the owner's decision "Protect main", taken after the orchestrator's analysis of the
"agentic software factories" guide found that CI on `main` reported but did not block merges (GitHub answered
"Branch not protected").
**Author:** Claude Code orchestrator (Opus 5.5).
**Reviewer:** an independent read-only Claude subagent, given the code and settings but not the author's reasoning.
It reviewed twice: first `0ee936db`..`0d112457`, then `016d0c4c`..`d87414e4`.
**Landed:** `0ee936db`, `0d112457`, `3f0250c8`, `016d0c4c`, `3c08cd9a`, `d87414e4`, `3ecfcaa6`, `2bdffbd4`.
`main` is now `2bdffbd4`.

## What was built

- **`.github/workflows/ci.yml`:**
  - job `changes` classifies a change as docs-only or code;
  - every build job needs it;
  - job `gate` (`if: always()`) passes only if every job succeeded, or, for a docs-only change, was skipped.
- **Branch protection on `main`:** required check `gate` from GitHub Actions (app 15368), `strict` on, admins
  included, no force push, no deletion.
- **`tools/orch/land.py`:** pushes `land/<topic>`, waits for a `gate` of `ci.yml` run for a push of exactly that
  commit, pushes `main`, deletes the branch and runs `sync_gitlab.py`, which leaves `land/*` out.
- **Owner rule** at the top of [`TASKS.md`](../TASKS.md).

## Evidence

| Claim | Proof | Counterexample or negative control |
|---|---|---|
| The classifier separates docs-only from code | 33 crafted cases in a throwaway clone. A fixture guard proves each test commit really holds its path. | Must be code: a file moved from `src/` into `docs/`, a docs commit on an earlier code commit, a pull request aimed at a code branch, a tag named `origin/main`, `docsx/`, 12 Windows-unsafe paths. Must stay docs: `docs/console.md`, `docs/price$.md`. |
| `gate` fails on anything but success | 10 job-result sets in WSL Ubuntu, Python 3.12 | 7 must fail: a skipped job on a code change, failed, cancelled, `changes` failed or skipped, no or odd classification |
| `land.py` counts only the right check | `land_test.py`, 12 cases. Live lookups on `3f0250c8` and `0d112457`. | A gate from a pull request, another workflow, another commit, or with no workflow run is ignored. Skipped and neutral do not pass. A failure whose run is being re-run is pending. A quoted `.github/` path is caught. |
| A code change runs the full matrix | Run 42878281597: 11 jobs plus `gate`, all green, about 33 min | — |
| A docs-only change skips it | Run 42883373191: `code=false`, 5 jobs skipped, `gate` green in 15 s | — |
| GitHub refuses an unchecked push, admin included | `git push origin main` of `3f0250c8` was rejected: "Required status check "gate" is expected" | — |
| `strict` does not block the sanctioned path | The fast-forward pushes of `d87414e4` and `2bdffbd4` were accepted | — |
| The CI-change guard works live | `land.py --rev 2bdffbd4` without `--allow-ci-change` exited 1 and pushed nothing | — |

## Findings and dispositions

**First review: 9 findings, all fixed.**

| Severity | Finding | Fix |
|---|---|---|
| High | A pull request aimed at another branch was compared against that branch: a docs edit stacked on broken code passed in a minute | `016d0c4c` |
| Medium | `gate` approves itself | `land.py` counts only `ci.yml` push runs of the commit, and `.github/` changes need `--allow-ci-change` (`3c08cd9a`). Partly residual, see below. |
| Medium | The merge button bypasses the gate | `strict` turned on, and the rule forbids the merge button |
| Low | A Windows-unsafe docs path skipped the Windows jobs | `016d0c4c` |
| Low | A tag named `origin/main` shadowed the base | Fixed in both files |
| Low | Running `land.py` right after a re-run stopped at once | Fixed |
| Low | `[skip ci]` meant a 2-hour wait | Fixed |
| Low | One GitHub error killed the wait | Fixed |
| Low | Two wrong statements in the docs | Fixed |

**Re-review: the 9 confirmed closed, 2 new low findings, both fixed.**

| Severity | Finding | Fix |
|---|---|---|
| Low | Git quotes non-ASCII paths, so the CI guard missed them | `2bdffbd4` |
| Low | `CONIN$`/`CONOUT$` were not treated as reserved | `3ecfcaa6` |

## Residual gaps (accepted, owner informed)

- **A code change can still weaken a test.** A change to the test harness (`run_selected.py`, `ctest_verdict.py`,
  the CMake test list) can weaken what `gate` means without touching `.github/`. That is ordinary code review's job.
- **UNVERIFIED:** which check suite a check run lands in when another workflow posts it with a token. `land.py`'s
  workflow match is the defence.
- **UNVERIFIED:** paths longer than 260 characters.
- **GitHub's merge buttons are still switched on.** Turning them off is the owner's call.
- **Every code landing runs CI twice,** once on `land/` and again on `main`.
- **Coordinator-test flakes now block landings until re-run.** The `main` run of `0d112457` failed in
  `cpp (linux-clang-asan)` on `anno_test_support_tests.V3`, although the same commit had passed on `land/ci-gate`.
  This is the known family that turned about a third of `main`'s runs red. `task/W4-coord-1` (C-1) targets it.
- **No game-file run is needed.** These commits change the CI workflow, orchestration Python tools and docs, none of
  which the MSVC suite builds or runs (owner rule of 2026-11-07).
