# HARNESS-GUARD-1: deletion guard, rule-sync lint, reviewed-commit check

- **Status:** draft 2026-11-28 by the orchestrator for the render lane (owner go for harness items 1-7, confirmed in
  the orchestrator session 2026-11-28, including the `.github/` change).
- **Branch:** `task/HARNESS-GUARD-1`, base origin/main after the board landing that carries harness item 1
  (commit `6ab31e05`, "docs: sync review rules ..."). Start only once `git merge-base --is-ancestor 6ab31e05 origin/main`
  succeeds.
- **Size:** one concern (landing guards), about 3 h.
- **Background:** `cache/reports/Harness engineering for agent lanes.md` in the main checkout, last section "Final
  proposal after advisor review", items 2, 3 and 7. Not in git; read it there.

## Why

On 2026-11-27 UI-PLANK-1 head `c5e6c432` cut `docs/re/ui.md` from 1715 to 196 lines and nothing mechanical noticed. Docs-only
pushes skip every build job, so no CI job looks at them at all. And AGENTS.md had drifted from CLAUDE.md on the review
rule. This packet makes both mechanical.

## What to do

1. **`tools/orch/docs_integrity.py`**, standard library only, `python -I`. Two checks, one CLI:
   `python -I tools/orch/docs_integrity.py --base <rev> --head <rev> [--repo .]`. Exit 0 = clean, 1 = a finding
   (one line per finding naming the check, the path and the numbers), 2 = a git error.
   - **Deletion guard.** `git diff --numstat -z --no-renames <base> <head>`. Per file, a finding when
     `deleted - added >= 200` **and** `deleted >= 4 * added`. Skip binary rows (`-` counts). `--no-renames` on purpose:
     a moved file counts as a deletion at its old path, so an archive move needs a trailer.
   - **Escape:** a trailer `Large-Deletion: <path> - <reason>` in any commit of `<base>..<head>`
     (`git log --format=%(trailers:key=Large-Deletion,valueonly,unfold) <base>..<head>`). The path before ` - ` must
     equal the flagged path exactly; the reason must be non-empty. No verbatim-move detector, no grandfather list.
   - **Rule-sync lint** on the files at `<head>` (read with `git show <head>:<path>`, not the working tree): fail when
     any of `AGENTS.md`, `CLAUDE.md`, `docs/testing/*.md`, `docs/orchestration/TASKS.md` contains, case-insensitively,
     `Use a review subagent` or `obtain re-review before completion`; and fail when `AGENTS.md` no longer contains the
     link target `(CLAUDE.md)`. Whole files, no "live part" parser: the orchestrator checked on 2026-11-28 that neither
     phrase occurs anywhere in those files after item 1. The phrase list lives in the script only; do not quote it in a
     scanned doc. `docs/orchestration/history/` and `docs/orchestration/tasks/` are not scanned.
   - **Self-test** `tools/orch/docs_integrity_test.py` (unittest, a throwaway git repo in a temp dir): the 200/4x
     boundary on both sides, a binary file, a path with a space and a non-ASCII path, a trailer that matches, one that
     names another path, one with an empty reason, and each lint rule failing and passing.
2. **CI job `docs-integrity`** in `.github/workflows/ci.yml`:
   - `runs-on: ubuntu-24.04`, `timeout-minutes: 5` (CI-SPEED-1's rule for a short job), `actions/checkout@v4` with
     `fetch-depth: 0`. **No `if:`**: it runs on every push and pull request, docs-only ones included.
   - **The same base as the `changes` job**, not a second rule: `BEFORE` for a push to `refs/heads/main`, otherwise
     `git merge-base refs/remotes/origin/main HEAD`. With no usable base (all-zero `BEFORE`, a missing commit), run the
     lint only and print that the guard was skipped and why. Factor the base computation into a step both jobs can use
     only if that stays simple; otherwise duplicate the few lines and say so in a comment naming the other job.
   - Steps: run the guard and lint with that base, then `python3 -I tools/orch/docs_integrity_test.py` and
     `python3 -I tools/orch/land_test.py` (neither runs in CI today).
   - **`gate`:** add `docs-integrity` to `needs`, and exempt it from the "skipped is fine when `CODE=false`" allowance
     the way `changes` is exempt, so a skipped or failed `docs-integrity` always fails `gate`. Update the comment above
     `gate`.
3. **`land.py`:**
   - Run the same check (import `docs_integrity`, base = `origin/main`, head = the candidate) before anything is
     pushed; a finding refuses with exit 1 and prints the findings and the trailer syntax.
   - **Reviewed commit (item 7):** a new required argument `--reviewed <sha>`, the commit the orchestrator reviewed.
     Refuse with exit 1 unless it resolves to the same commit as `--rev`. For a stacked bundle that is the candidate
     merge the orchestrator and the advisor reviewed, not any packet head; for a board landing it is the board head
     the orchestrator read. A push after the review changes `--rev`'s resolution and so forces a new review.
   - Update the module docstring (usage line and step 1) and `land_test.py` for both refusals.
4. **Stale string in `tools/orch/packet.py`:** the generated packet still says `AGENTS.md` is "never edit or commit
   it". Make it say only that Claude Code loads `CLAUDE.md` by itself and `AGENTS.md` carries the same rules for Codex.
5. **Docs:** one short paragraph in `docs/testing/README.md` (or next to the `land.py` notes in
   `docs/orchestration/TASKS.md` §1, wherever `land.py` usage is described) naming the guard, the trailer syntax and
   `--reviewed`.

## Validation (acceptance; record commands and outputs in the report)

- **Reproduce the report's numbers with the shipped script,** not a separate one: over the last 400 first-parent
  commits of `origin/main` before your base, run the guard on each parent..commit pair. Expected: exactly 4 findings,
  in `23885eb3` (FIXTURES-TRIM-1), `3e2bd480` (`save.cpp`), `78d32770` and `ff2b844c` (board archives). If the count
  differs, stop and report what differs and why (for example renames) before changing the rule.
- `aeb65424..c5e6c432` gives a finding for `docs/re/ui.md` (+1 −1520); `aeb65424..5a08acd2` gives none.
- The lint passes on your head and fails when you add one of the phrases to a scratch copy of AGENTS.md (do not
  commit that).
- `land.py` refusals shown with `--rev` and `--reviewed` differing, and with a guard finding, both without pushing
  (use a local throwaway branch; do not push to `land/*`).
- `tools/test/workflow_test.py` and the contract suites `docs/testing/README.md` names for a workflow change pass.
- Hosted CI on the exact head: `docs-integrity` and `gate` green. Also push one docs-only commit on the branch and
  show `docs-integrity` ran (not skipped) on it.
- Numstat of the branch against its base in the report.

## Files

- New: `tools/orch/docs_integrity.py`, `tools/orch/docs_integrity_test.py`.
- Modify: `.github/workflows/ci.yml`, `tools/orch/land.py`, `tools/orch/land_test.py`, `tools/orch/packet.py`, the
  one docs paragraph, `docs/orchestration/REPORTS.md` (report).

No change to the build jobs, the test coordinator or any game code. The orchestrator lands it with
`--allow-ci-change` after review.
