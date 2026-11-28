# HARNESS-GUARD-1: deletion guard, rule-sync lint, landing on a fixed commit

- **Status:** draft 2026-11-28 by the orchestrator for the render lane. Owner go for harness items 1-7, recorded in
  `docs/orchestration/TASKS.md`, "Owner rules, 2026-11-28", including the `.github/` change. EGM-reviewed, reports
  in the main checkout's `cache/egm/harness-guard-1/` (not in git).
- **Branch:** `task/HARNESS-GUARD-1`, base origin/main (it already carries harness item 1, `6ab31e05`).
- **Size:** one concern (landing guards), plus one harness-item-1 straggler in `tools/orch/packet.py`; about 3 h.
- **Background:** `cache/reports/Harness engineering for agent lanes.md` in the main checkout, subsection "Ranked
  recommendation", items 2, 3 and 7. The facts this packet relies on are repeated below, so the report is optional.

## Why

- **Large deletions go unchecked.** On 2026-11-27, commit `8ff63b70` (in UI-PLANK-1 head `c5e6c432`) cut
  `docs/re/ui.md` from 1715 to 196 lines. The head passed the orchestrator's review; the UI lane caught the cut while
  merging, and `5a08acd2` restored the file before it reached main. No CI job checks deletions on any push, and a
  docs-only push runs only the `changes` and `gate` jobs, so a check inside a build job would never see it.
- **Rule copies drift.** AGENTS.md carried its own copy of the review rule and drifted from CLAUDE.md. Harness item 1
  (`6ab31e05`) replaced the copy with a pointer to CLAUDE.md and the evidence contract; this packet keeps it from
  coming back. The current rule: for executor packets, the orchestrator's review at handover is the independent
  review, and executors start no reviewer subagents unless their packet asks. The lint bans the old wording.
- **A landing can drift from the review.** `land.py` takes `--rev`, default `HEAD`. A branch name or `HEAD` that moves
  after the review lands a commit nobody reviewed.

## Design

One script, `tools/orch/docs_integrity.py`, with two checks: the deletion guard and the rule-sync lint. It has three
call sites:
1. A CI job `docs-integrity` that runs on every push and is listed in `gate.needs`. It takes its base from a new
   `base` output of the `changes` job, so there is one base rule.
2. `land.py`, before anything is pushed, against the `origin/main` commit it already resolves.
3. Its own self-test, which runs in the same CI job, next to `land_test.py`.

`land.py` also requires `--rev` to be a full 40-hex commit id. A commit id cannot move, so the commit that lands is
the one that was reviewed.

Terms: a **candidate** is the commit `land.py` lands. A **stacked bundle** is a candidate merging several reviewed
packet heads onto main. A **board landing** is a docs-only candidate of the board branch `orch/board-next`.

**Non-goals:**
- the handover-side script and the report-only warnings (report item 8): later, after this guard proves itself;
- a parser for the "live part" of TASKS.md (see the risks);
- any change to the build jobs, the test coordinator or game code.

## Alternatives considered

- **Guard in `land.py` only:** rejected. Branch pushes would not run it, and an executor would see a finding only at
  landing.
- **Lint inside `tools/test/workflow_test.py`:** rejected. That file runs inside the cpp jobs, which docs-only pushes
  skip.
- **Other thresholds** (a percentage floor, a lower line count): rejected. Only 200 lines with a 4x ratio was
  measured. Over the 400 first-parent landings on main ending at `b5ee471c` (2026-10-28 to 2026-11-27, about four
  weeks), it flags exactly 4 commits, all intentional: `23885eb3` FIXTURES-TRIM-1, `3e2bd480` `save.cpp`, and the
  board archives `78d32770` and `ff2b844c`.
- **Per-commit comparison:** rejected. It would flag a cut that a later commit in the same range restores. Range
  endpoints net them: `aeb65424..c5e6c432` shows `docs/re/ui.md` +1 −1520 (caught), `aeb65424..5a08acd2` +1 −0.
- **A verbatim-move detector and a grandfather list:** rejected. Two of the four hits were archive moves; a trailer
  covers them without extra machinery.
- **A separate `--reviewed <sha>` argument:** rejected. The same person types both values, so equality proves
  nothing. A commit-id-only `--rev` does the job with one argument.

## Risks and limits

- **Known misses:**
  - a cut under 200 lines passes;
  - so does a cut offset by additions of more than a quarter of its size in the same file;
  - range endpoints net a cut against a later restore, on purpose;
  - validation covered first-parent landings on main, not task-branch ranges.
- **Friction:**
  - about one intentional finding a week (4 in four weeks) needs a trailer;
  - `--no-renames` makes every archive move a finding;
  - every `land.py` call must now name a full commit id.
- **Self-granted escape:** an executor can put `Large-Deletion:` on its own commits. Mitigation: the guard prints
  every trailer it honoured, in CI and in `land.py`, and the orchestrator checks each one at handover. A trailer is a
  review item, not an exemption.
- **Self-weakening:** CI runs the candidate's own copy of the script. Mitigation: `land.py` treats changes to
  `tools/orch/docs_integrity.py` and `tools/orch/land.py` like changes to `.github/`, which need `--allow-ci-change`
  after review.
- **A bug in the script blocks every landing:** the self-test runs in the same job, and backing the job out is one
  revert commit.
- **Fail-open without a base:** with no usable base, CI runs the lint only and prints that the guard was skipped.
  `land.py` always has a base.
- **Whole-file lint:** the lint scans whole files, so a later quotation of a banned phrase in a scanned file fails
  CI, by design. Quote around it.

## What to do

1. **`tools/orch/docs_integrity.py`**, standard library only, run with `python -I`.
   - **CLI:** `python -I tools/orch/docs_integrity.py [--base <rev>] --head <rev> [--no-lint] [--repo .]`.
     - Without `--base`, only the lint runs, and the script prints `guard skipped: no base`.
     - With `--no-lint`, only the guard runs. The history replay below needs it, because older commits still carry the
       old wording.
     - Exit codes: 0 = clean, 1 = at least one finding, 2 = a git error.
   - **Output, one line each:**
     - `deletion: <path> +<added> -<deleted>`;
     - `lint: <path> <rule>`;
     - `trailer: <path> - <reason>` for each honoured trailer.
   - **Python API**, for `land.py`: `check(repo, base, head, lint=True)` returns `(findings, honoured)`, two lists of
     those lines. It raises `subprocess.CalledProcessError` on a git error. The CLI is a thin wrapper around it.
   - **Deletion guard.**
     - Read `git diff --numstat -z --no-renames <base> <head>`.
     - Per file, a finding when `deleted - added >= 200` **and** `deleted >= 4 * added`.
     - Skip binary rows (counts of `-`).
     - `--no-renames` on purpose: a moved file counts as a deletion at its old path, so an archive move needs a
       trailer.
   - **Escape:** a trailer `Large-Deletion: <path> - <reason>` in any commit of `<base>..<head>`.
     - Read the trailers with `git log --format=%(trailers:key=Large-Deletion,valueonly,unfold) <base>..<head>`, and
       drop the empty lines it prints for commits without one.
     - **Matching:** each trailer names one path. A trailer matches a flagged path when its value starts with that
       exact path followed by ` - `, and a non-empty reason follows. Several trailers may cover several paths.
     - Git reads trailers only from a message's last paragraph; a trailer written mid-body is ignored, which fails
       safe.
   - **Rule-sync lint**, on the files at `<head>` (read with `git show <head>:<path>`, not the working tree):
     - **Scanned files:** `AGENTS.md`, `CLAUDE.md`, `docs/orchestration/TASKS.md`, and every `docs/testing/*.md`,
       listed with `git ls-tree --name-only <head> docs/testing/` (top level only). A scanned file missing at `<head>`
       is skipped, except AGENTS.md, whose absence is a finding.
     - **Banned phrases:** fail when a scanned file contains, case-insensitively, `Use a review subagent` or
       `obtain re-review before completion`. These are the old review rule, which harness item 1 removed. Quoting them
       here is safe because `docs/orchestration/tasks/` is not scanned; in scanned docs, quote around them.
     - **Near-matches that must not fail:** `evidence-contract.md`'s "obtains re-review" and TASKS.md's plain
       "re-review".
     - **Candidates left out:** "mark independent review pending" was only ever part of the removed AGENTS.md
       paragraph, and that paragraph is caught by the first phrase. TASKS.md keeps the R-package reviewer line as a
       marked historical record.
     - **The CLAUDE.md link:** fail when `AGENTS.md` no longer contains the link target `(CLAUDE.md)`. AGENTS.md points
       to CLAUDE.md for the shared rules; it does not copy them.
   - **Self-test** `tools/orch/docs_integrity_test.py` (unittest, using a throwaway git repo in a temp dir that holds an
     AGENTS.md linking `(CLAUDE.md)`). Cases:
     - the 200/4x boundary on both sides;
     - a binary file;
     - a path with a space, a path containing ` - `, and a non-ASCII path;
     - a trailer that matches, one that names another path, one with an empty reason, and one mid-body;
     - a run without `--base`, and one with `--no-lint`;
     - each lint rule failing and passing, including a missing AGENTS.md and a near-match that passes.
   - **Imports under `python -I`:** `-I` leaves the script's own directory off `sys.path`. In `docs_integrity_test.py`
     and `land.py`, insert `Path(__file__).resolve().parent` into `sys.path` before importing `docs_integrity`, as
     `land_test.py` does.
2. **CI** in `.github/workflows/ci.yml`:
   - **The `changes` job:**
     - Add the output line `base: ${{ steps.classify.outputs.base }}`.
     - Write `base=<sha>` to `$GITHUB_OUTPUT` whenever the base passes the all-zero check and `git cat-file -e`, even
       when the diff is empty. Write an empty value otherwise.
     - Update the header comment above `changes`: a docs-only change still runs no build or test job, but it does run
       `docs-integrity`.
   - **The new job `docs-integrity`:**
     - `needs: changes`, and **no `if:`**;
     - `runs-on: ubuntu-24.04`, `timeout-minutes: 5`;
     - `actions/checkout@v4` with `fetch-depth: 0`;
     - `env: BASE: ${{ needs.changes.outputs.base }}`.
   - **The job's steps:**
     1. Run `python3 -I tools/orch/docs_integrity.py ${BASE:+--base "$BASE"} --head HEAD`. An empty base means lint
        only, and the skip is printed.
     2. Run `python3 -I tools/orch/docs_integrity_test.py`.
     3. Run `python3 -I tools/orch/land_test.py`. `land_test.py` does not run in CI today.
   - **`gate`:**
     - **Today:** `gate` accepts a skipped job when the change is docs-only (`CODE=false`), and never accepts a
       skipped `changes`.
     - **Change:** add `docs-integrity` to `needs`, and exempt it from that allowance the same way.
     - **Comment:** the comment above `gate` gains one sentence: `changes` and `docs-integrity` must succeed on every
       change, docs-only ones included.
3. **`land.py`:**
   - **Pure refusal checks**, so `land_test.py` can test them without the network:
     - `rev_refusal(rev)`: the value must be exactly 40 lowercase hex characters.
     - `refusal(repo, sha, main_sha, allow_ci_change)`: today's ancestry and guarded-path checks, plus the guard.

     Each returns a refusal message or `None`. `main()` prints the message and exits 1.
   - **Order in `main()`:**
     1. `rev_refusal`, before the fetch.
     2. After the fetch, `git rev-parse --verify <rev>^{commit}` must print the value unchanged. A tag object id
        peels to a different commit and is refused.
     3. `refusal` runs the guard and the lint through `docs_integrity.check` (base `main_sha`, resolved from `MAIN`,
        head the candidate), after the existing ancestry and `--allow-ci-change` checks and before the `gh repo view`
        slug lookup.
     4. A git error from the check is a refusal (exit 1, with the error printed).
     5. The refusal prints the findings and the trailer syntax. The honoured trailers are printed either way.
   - **Fixed commit (item 7):**
     - `--rev` becomes required, with no `HEAD` default.
     - The refusal message names the problem and gives `git rev-parse <rev>` to get the full id.
     - Which commit to pass:
       - a single packet: the reviewed packet head, or its landing merge;
       - a stacked bundle: the candidate merge reviewed by the orchestrator and the advisor (the stronger reviewer
         model the orchestrator consults for bundles);
       - a board landing: the board head the orchestrator read.
   - **Guarded paths:** extend the `--allow-ci-change` path set from `.github/` to also cover
     `tools/orch/docs_integrity.py` and `tools/orch/land.py`. Update the module docstring (the usage line and step 1),
     the `--allow-ci-change` help text and the REFUSED message.
   - **`land_test.py`:** add a test for each refusal against a temp git repo:
     - a guard finding;
     - a `--rev` of `HEAD`, of a branch name and of an 8-hex id;
     - a change to a guarded tool without `--allow-ci-change`.
4. **Stale string in `tools/orch/packet.py`:** replace the sentence
   "Claude Code loads `CLAUDE.md` by itself; `AGENTS.md` is the same text for Codex (never edit or commit it)." with
   "Claude Code loads `CLAUDE.md` automatically; `AGENTS.md` points Codex to the same rules."
5. **Docs.** In `docs/orchestration/TASKS.md`, in the protected-main owner rule:
   - add a bullet after the `--allow-ci-change` bullet naming:
     - the guard and its rule;
     - the trailer syntax, and that only a message's last paragraph counts;
     - that the orchestrator checks every honoured trailer at handover;
     - the 40-hex `--rev`;
     - the extended guarded-path set;
   - change the usage line to `python -I tools/orch/land.py --rev <full 40-hex commit id> --name <topic>`;
   - in "What `gate` means", add that docs-only changes also run `docs-integrity`.
6. **Report:** append it to the bottom of `docs/orchestration/REPORTS.md` in the template's format, with the Numstat
   line.

## Validation (acceptance; record commands and outputs in the report)

- **Reproduce the report's numbers with the shipped script,** not a separate one.
  - **Window:** for each of the 400 first-parent commits of main ending at `b5ee471c` (the oldest is `507e41a3`), run
    `--no-lint --base <c>^1 --head <c>`.
  - **Expected:** 25 `deletion:` lines, all in exactly 4 commits. The orchestrator verified this with a throwaway
    replay of the same rule on 2026-11-28:
    - `23885eb3`: 21 fixture files;
    - `3e2bd480`: `src/sim/src/save.cpp` +6 −273;
    - `78d32770`: `REPORTS.md` and `TASKS.md`;
    - `ff2b844c`: `TASKS.md`.
  - **If the output differs,** stop and report what differs and why, before changing the rule.
- **The incident:** with `--no-lint`, `aeb65424..c5e6c432` gives `deletion: docs/re/ui.md +1 -1520`, and
  `aeb65424..5a08acd2` (the restore) gives none.
- **The lint:**
  - the self-test's lint cases pass;
  - the lint passes at your head;
  - on a local throwaway commit that adds one banned phrase to AGENTS.md, the lint fails. Never push that commit.
- **The `land.py` refusals:** the `land_test.py` cases of step 3 pass, and nothing is pushed while you check them.
- **Contract suites:** they guard the test workflow, not `ci.yml` (hosted CI checks `ci.yml`). Each must pass:
  - `python3 -I tools/test/workflow_test.py`;
  - `python3 -I tools/test/ctest_verdict_test.py`;
  - `python3 -I tools/test/run_selected.py --preflight-only`.
- **Hosted CI on the exact head:** `docs-integrity` and `gate` green. A branch is classified against main as a whole,
  so a docs-only push cannot be shown here. The orchestrator checks the first docs-only push after landing and names
  it in the review.
- **Numstat:** the branch's numstat against its base goes in the report.

## Files

- **New:** `tools/orch/docs_integrity.py`, `tools/orch/docs_integrity_test.py`.
- **Modify:** `.github/workflows/ci.yml`, `tools/orch/land.py`, `tools/orch/land_test.py`, `tools/orch/packet.py`,
  `docs/orchestration/TASKS.md` (the protected-main rule), and `docs/orchestration/REPORTS.md` (step 6).

The orchestrator lands it with `--allow-ci-change` after review.
