# W4-NEXT-1: coordinator root cause, settlement close-out, next UI screens

**RELEASED 2026-11-10** by the owner, acting as orchestrator, to the Windows executor (Claude Sonnet 5.5 on the
owner's Windows PC). Written 2026-11-10 by the Claude Code orchestrator.

Work the parts in order: **P → C → S → U → E**. Each part ends with a REPORTS entry and a push of its branch. Re-read
this packet and your latest REPORTS entries after any context compaction.

## Why this packet

1. **The test coordinator costs every push and every game-file run.** On 2026-11-08 its tests failed four times in
   hosted CI on docs-only commits, and the same subsystem stalled the owner-rule game-file run on this PC twice
   ([review §3 and §4](../reviews/2026-11-08-w4-ui-2-merge.md)). You raised this yourself (REPORTS, "W4-UI-2 —
   QUESTION"); your option (a), a real root cause, is now the assignment.
2. **W4-SETTLEMENT-1 has three small review items left** ([review, part S](../reviews/2026-11-07-w4-main-2.md)).
   They block the owner's settlement playtest card.
3. **The owner tests through the original's in-game UI** (decision M3, `docs/plan/lan-mvp.md` §1). W4-UI-2 bound the
   HUD and the construction menu. Seventeen screens are still placeholders or partial (ADR 0004, the screen table).
   The next ones are those the owner meets in a build-and-demolish session.

## 0. Rules for this run

### 0.1 Base, branches, pushes

- **Base:** `origin/main` at `bd760031` (state134 / ECONBK1 / PRNG5 / replay4 / command1). It already contains
  W4-UI-2, W4-MAIN-2 parts S, A, B and D, and UI.1.F2.1. Verify with `git merge-base --is-ancestor`.
- **Branches, each created from that commit in its own new worktree under `E:\anno1503-worktrees\`:**
  - part C: `task/W4-coord-1`;
  - parts S and U: `task/W4-ui-3` (S first, as its own commits).
- **No merge, no rebase, no other lane's branch.** In particular don't touch `task/MAP-EXTRACT-2`,
  `fix/MAP-EXTRACT-2-ci` or their worktree `anno1503-MAP-EXTRACT-2-ci`; the orchestrator owns that consolidation, and
  it will bring state 137 to `main` later.
- **Push** each branch at every checkpoint with an ordinary push. Never push `main`, never force.

### 0.2 What this packet may change

- **Part C:** `src/test_support/**`, its tests, `.github/workflows/` only where the root cause requires it, and
  `docs/perf/README.md`. A change to the wire format or to reservation semantics bumps `wire::kCompatVersion` and is
  a QUESTION first.
- **Parts S and U:** `src/app/**`, `src/render/**` (UI drawing only), `data/ui/**`, `docs/**`, `tools/ui_layout.py`.
- **No sim behaviour changes.** The UI reads the published snapshot and sends commands through the existing command
  path. A new read-only sim query that changes no state, stream, hash or draw is allowed; name it in the report.
  Anything else in `src/sim` is a QUESTION first.

### 0.3 How to work

- **Check, don't guess** (board §1 item 12). For a UI layout, graphic or behaviour, the sources are `data/ui/`,
  `docs/re/ui.md`, `tools/ui_layout.py`, and the original files in `E:\anno1503\game\app` (read-only). For the
  coordinator, read the code and run a bounded measurement. An open doubt becomes a QUESTION or an OQ, never a
  silent choice.
- **Don't stop for a decision.** Write a QUESTION in that part's REPORTS entry (what is needed, the options, your
  recommendation with evidence), skip only what depends on it, and continue.
- **"Decisions and reasoning"** in every REPORTS entry: what you decided, the alternatives, the evidence, why.
- **Evidence contract:** read `docs/testing/evidence-contract.md` before P and before E. Write the acceptance table
  for each part before implementing it. Use `tools/test/run_selected.py` for gates. Report selected, executed,
  failed, empty and skipped separately; a refused or skipped test is not a pass.
- **Independent review:** each of C, S+U gets a read-only review subagent that sees the frozen diff and the raw
  logs, not your summary. Fix its findings and get a re-review before you report the part.
- **Host discipline:** this PC has 31 GB. Check free memory and running jobs before a build or gate; one heavy job
  at a time. Run a full suite as a detached process (review §3 says why). Never stop a coordinator that has
  clients, and never stop another session's processes.
- **Keep evidence** in the worktree's git-ignored `cache\w4-next-1\`: logs, driver scripts, mutant definitions as
  exact replacements. Keep the first failed run of anything.

## P. Preflight

1. `git fetch origin`; confirm `origin/main` is `bd760031` or a descendant. If it moved, read `STATUS.md` and the
   new top dispatch first and record the commit you start from.
2. Create the two worktrees and branches of §0.1. `git status` empty in both.
3. Read: this packet; `docs/testing/evidence-contract.md`; review
   [2026-11-08-w4-ui-2-merge](../reviews/2026-11-08-w4-ui-2-merge.md) §3–§4; review
   [2026-11-07-w4-main-2](../reviews/2026-11-07-w4-main-2.md) part S; ADR 0004 with its screen table and QUESTIONs
   R2 and R3; `docs/perf/README.md` on the coordinator.
4. Record host state (free memory, commit charge, WSL distros running, any coordinator on port 47631).

## C. The coordinator: root cause and fix

**Goal.** Name the cause of each symptom below with evidence, fix it, and show the fix holds.

**Symptoms (all in the review, with run ids and logs under `cache/main-gamefile-96de6bca/`):**

| # | Symptom | Where |
|---|---|---|
| C-1 | `anno_test_support_tests` cases fail in hosted Linux CI on docs-only commits: `F3 (D1, crash handover)`, `F3 (D1, silent)`, `F3 (D1, exact build)`, `F4 (N3)`, `O1`, `O3`, `V3`, `V4`, `H1` | review §4 table; also run 42421383045 on `task/MAP-EXTRACT-2` |
| C-2 | Test clients hold a coordinator connection, make no progress and are never refused, past `ANNO_TEST_ADMIT_WAIT_S` | review §3 attempts 1 and 4; `stall-snapshot.txt`, `stall2-snapshot.txt`, `coord-47631.journal.at-stall2` |
| C-3 | Large tests that take no reservation push free memory under the stop line, so reserving tests are refused with `pending=0` | review §3 attempt 3 |

**What is already known, to verify and not to assume:**
- An unmerged branch `origin/claude/dreamy-brahmagupta-10bjax` (`e5d0b877`, `0104011e`, `85b8a761`, `6f7faaa2`)
  moves the coordinator regression ports below Linux's ephemeral range. It was written for an earlier failure of
  `V3`. Decide with evidence whether it explains C-1 fully, partly or not at all. Don't merge the branch; if its
  change is right, re-make it on `task/W4-coord-1` and say so.
- `origin/claude/ci-fix-map-route-1` tried reserving the ports in CI and reverted it. Read why.
- For C-2 the review's hypothesis is a hang in the coordinator↔host-meter read path, with the client's deadline
  checked only when an answer arrives. It is unproven.

**Steps.**
1. **Reproduce before fixing.** For C-1, make the failures observable: the failing tests don't log the message they
   received. Add that logging first, then reproduce under load (parallel test binaries in WSL, or a dedicated CI
   run on your branch). For C-2, build a bounded reproduction with a meter that stops answering.
2. **Root cause each symptom** in `src/test_support/src/` (`coord.cpp`, `coord_server.cpp`, `budget.cpp`,
   `budget_coord.cpp`, `memory.cpp`). State the mechanism and the line.
3. **Fix.** Every wait has a deadline that fires without an answer. A fix needs a test that fails without it.
4. **C-3 is a design question, not a bug hunt.** Measure which test binaries exceed 2 GiB without a reservation
   (peak working set per CTest item from one full run), and propose the remedy as a QUESTION with numbers. Implement
   only a reservation for an existing test if that is the whole remedy.
5. **Document** the cause and fix in `docs/perf/README.md`.

**Acceptance.**
- Each of C-1 and C-2 has a named cause, a test that fails without the fix, and the fix.
- Five consecutive full CI runs on `task/W4-coord-1` with `anno_test_support_tests` clean on all three Linux jobs,
  listed with run ids. A job-level rerun does not count as one of the five.
- One full MSVC run with the game files on this PC (`ctest --preset msvc`, `ANNO1503_GAME_DIR`,
  `ANNO_GPU_ADAPTER=4090`, detached) with no stall and every refusal explained.
- If a cause cannot be proven within the part, say so plainly, keep the added diagnostics, and report what was
  ruled out. Don't call it fixed.

## S. W4-SETTLEMENT-1: the three items left

From the [part S review](../reviews/2026-11-07-w4-main-2.md):

1. **The negative step clicks instead of hovering.** `settlement_activity.script` uses `click 1130 516` with the
   building tool on and type 501 selected, so it queues a Build. Use the script language's hover verb, and show
   that the step still fails when the inspection line is wrong.
2. **No test covers `inspect_building` and its formatters** for workshop, farm, small farm, mine and inn. Add
   `anno_app_tests` cases with hand-derived expected lines, and one wrong-building control.
3. **Correct the S report:** `wsl_gate.sh` does pass `--exclude-label heavy`; the heavy tests were NOT_RUN under
   ASan. Add a correction entry in REPORTS; don't edit the old entry.

Then confirm the settlement playtest card in `docs/testing/` matches what the window does today, and say in the
report that it is ready for the owner.

## U. The next UI screens

**Goal.** In `--play`, a build-and-demolish session through the original's UI no longer hits a blank panel.

**Scope, in this order** (row numbers are ADR 0004's screen table):

| Row | Screen | Done means |
|---|---|---|
| 19 | Deselected right panel | the original's empty-panel default is drawn when nothing is selected |
| 18 | Selected house / building | clicking a built instance shows its panel with live data from the snapshot; a field the sim lacks is a labelled placeholder |
| 5 | Demolish mode | the mode has the original's cursor and chrome, not only state |
| 10 | Pause overlay (key P) | the parchment overlay is drawn and the sim clock stops and resumes through the existing path |
| — | Key collisions | S, Q and D are bound to developer controls and to original screens. Propose one scheme for all three as a QUESTION, and implement your recommendation behind the `--ui classic` surface only |
| 2 | Kontor panel, read-only | opens from a click on the Kontor; shows stock and the trade lists the sim already models; no trade commands |

Rows 6–9 and 15–17 stay placeholders. If time remains after row 2, stop and report; don't start them.

**For every screen:**
- Layout and graphics come from `data/ui/` and `docs/re/ui.md`. Where the spec is silent, check the original files
  and extend the spec in the same commit; a guess goes to `docs/open-questions.md`.
- Draw on the CPU reference path first, then the GPU path, and compare them. W4-UI-2 found the GPU overlay missing
  late (`7d65bea3`); test both from the first screen.
- Compare with the UI.1 oracle within its tolerance, and include one deliberately wrong input that the comparison
  rejects.
- Input goes through the same routing as the construction menu. No screen mutates sim state.
- Measure the frame cost with `bench_frame` before and after (PLAN §0.3).

**Owner questions still open:** ADR 0004 R2 (default surface) and R3 (UI scale). Keep both behind their switches.

**Proof (as W4-UI-2's part V):** one scripted window session that selects a building, opens the Kontor panel,
pauses, resumes and demolishes; its replay in a fresh process; pixel checks on CPU and GPU; the command-boundary
check that no screen wrote sim state. Update `docs/testing/construction-menu-playtest.md` or add a card.

## E. Final gate and report

1. Per branch: clang-format 23.1.3 over `src/`; `wsl -d Ubuntu -- bash tools/check_unix_warnings.sh`; the canonical
   gate with `tools/test/run_selected.py`; the full MSVC suite with the game files and the 4090, detached.
2. Hosted CI green on the pushed heads, with run ids. A coordinator-family failure is rerun once at job level and
   logged with both ids until part C is merged.
3. The final REPORTS entry: heads, the acceptance tables, exact counts, every failure by test name and assertion,
   NOT_RUN items, QUESTIONs, the review records, and a review-first list for the orchestrator.
4. Stop. Don't start another package.
