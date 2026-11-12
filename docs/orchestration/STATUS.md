# Who is working on what

A one-page view of the current work, for the owner and the co-developer. The task board
[`TASKS.md`](TASKS.md) holds the full instructions and [`REPORTS.md`](REPORTS.md) the executors' reports. This page only
says who is doing what, where the work is, and what it waits on.

**Last updated:** 2026-11-12, later, by the Claude Code orchestrator:
- **MAP-INTEGRATE-2 ACCEPTED** ([review](reviews/2026-11-12-map-integrate-2-review.md)): salt, iron → tools,
  quarry → bricks and automatic freight on `main` as **state 138**, merged as `c75cddff`.
  - **The code is fit for `main`;** the 13 h 20 min it took were not worth it. The main causes were the packet's size
    and the one-number save format, not the executor.
  - **Owner decisions, 2026-11-12:** a manual ship command cancelling the automatic route is the original's
    behaviour (no switch); executor packets get smaller and run no reviewer subagents of their own
    ([`TASKS.md`](TASKS.md), "Owner rule, 2026-11-12").
  - **Next:** one small packet at a time, first [SIM-LIMITS-1](tasks/sim-limits-1.md) for the Windows executor, when
    the owner says go. The queue is on the board under "Current work".
- **W4-NEXT-2 part H landed on `main` as `3eef75a5`** (accepted after round H4,
  [part H review](reviews/2026-11-12-w4-next-2-part-h-review.md)). Docs only; `main` stayed healthy.

Earlier on 2026-11-12, by the Claude Code orchestrator:
- **W4-NEXT-2 landed on `main` as `7babb1f2`** (the owner's OK; `gate` passed). Its code is the merge `c944aff7`,
  whose game-file run passed before the push, so `main` is healthy at `7babb1f2`.
- **W4-NEXT-2 part H reviewed:** changes requested, small; round H4 dispatched and accepted.

Earlier, 2026-11-11 night, by the Claude Code orchestrator:
- **W4-NEXT-2 ACCEPTED after fix round G** ([acceptance](reviews/2026-11-11-w4-next-2-acceptance.md)).
  - **Both branches land together** with the owner's OK: `task/W4-ui-4` `b483c4de` and `task/W4-shared-list-1`
    `b2808533`, merged on `main` as `c944aff7`.
  - **The race is closed by construction:** the window title and the selection facts now read one state, checked
    against the code.
  - **The orchestrator's game-file run of the merge** (RTX 4090): 2,390 selected, 2,378 passed, 0 failed, 12
    classified skips. The three selected-house tests passed 60 of 60 beside it, and the negative control failed at
    its bound, as it must.
  - **Two Low docs findings**, fixed in part H after the landing:
    - the plain sprite-sad figures do reproduce (they are per pixel), only the grey-key ones don't;
    - three of the shared list's five scenarios are not endless games.

Earlier that evening, by the Claude Code orchestrator:
- **W4-NEXT-2's fix round F reviewed: changes requested again**
  ([fix review](reviews/2026-11-11-w4-next-2-fix-review.md)).
  - **Fixed:** the HUD click-through, the demolish test, the population tool (step 10, `house_updates` opt-in,
    ordered checkpoints, invariants) and the C-3 table in git.
  - **Not closed: the selected-house race (P1).** It is narrower, but the orchestrator's game-file gate of the merge
    failed that test at `-j 16`. The code shows why: a refresh F1 added updates the selection without the window
    title, so a fact and the title can disagree for one tick.
  - **Smaller:**
    - no REPORTS entries for F1–F4 and F7;
    - an unjustified 30 s test bound;
    - a HUD-click test step that can't fail reliably;
    - one wrong sentence in ADR 0004;
    - `population` exits 0 on a broken invariant.
  - **Grey-key needs no owner answer.** The UI-1-F2 scripts define it, and with it the committed 17.8 measures 32.07,
    so 17.8 is marked as not reproduced.
  - **Fix round G** goes to the Windows executor by message (the packet's part G). Both branches then land together.
  - **The orchestrator's run of the merge:** 2,390 selected, 2,376 passed, 1 failed (the race), 12 classified skips.
    The 20× repeat of the three selected-house tests beside it passed 60 of 60, and the negative control failed at
    its bound, as it must.

Earlier on 2026-11-11, by the Claude Code orchestrator:
- **W4-NEXT-2 reviewed: changes requested** ([review](reviews/2026-11-11-w4-next-2-review.md)).
  - **`task/W4-ui-4` is not accepted yet.** Its new selected-house window test races the sim (P1), and a click on the
    HUD selects the building behind it (P2).
  - **`task/W4-shared-list-1` is accepted with findings,** to be fixed in the same round. The population tool ran
    with `house_updates` off, so "no growth" was guaranteed (OQ-571), and it steps 100 ms where play mode steps 10.
  - **Fix round F** goes to the Windows executor (the packet's part F). Both branches then land together.
- **[MAP-INTEGRATE-2](tasks/map-integrate-2.md) released to the Mac executor** (owner request), in parallel with
  W4-NEXT-2 on Windows.
  - **What it does:** brings the three finished Mac packages onto `main` under one new identity, **state 138**:
    salt (135), iron tools, quarry and bricks (137), and automatic freight (136).
  - **Also:** the shared folder on the Mac (receive-only), OQ-556 from the original's code, and a measured list of
    what still stops a town from progressing.
  - **Branch:** `task/MAP-INTEGRATE-2` from `main`.
  - **Afterwards, the orchestrator:** reviews it, runs the Windows game-file run, and lands it.
- **MAP-EXTRACT-2's T5 fix ACCEPTED** by the orchestrator, as the owner had asked
  ([review](reviews/2026-11-11-map-extract-2-t5-fix.md)). The two-reserver reload test executes with the game files
  and catches the unsaved-field mutant: 2 of 4,296 assertions fail with the mutant, all pass without it.

Before that, 2026-11-10 late evening, by the Claude Code orchestrator:
- **W4-NEXT-1 ACCEPTED and merged into `main`** as `fc126fbe` (part C) and `2c4dc295` (parts S, U, E)
  ([acceptance review](reviews/2026-11-10-w4-next-1-acceptance.md)). All nine interim findings are dispositioned.
  - **Fixed:** the `--view` key regression and the macOS SIGPIPE.
  - **Reported, not proven:** C-2's link to the recorded stall stays UNPROVEN.
  - **Five findings, none blocking.** The main one: the GPU pixel check for rows 5 and 10 was not run; W4-NEXT-2
    part P runs it.
  - **`main` at `2c4dc295` is healthy:** CI green, 12 of 12 jobs. Game-file run: 2,355 executed and passed, 0
    failed, 12 classified skips.
- **W4-NEXT-2 starts now** ([packet](tasks/w4-next-2.md)), from `main` at `2c4dc295` or later.
- **The board is slimmed (owner request):** `TASKS.md` 975 KB → 63 KB and `REPORTS.md` 2.1 MB → 435 KB. Closed
  history moved verbatim to [`HISTORY-2.md`](HISTORY-2.md) and [`REPORTS-1.md`](REPORTS-1.md); `TASKS.md` says what
  moved, what of it is still open, and which owner decisions the moved text records.
  Executors still append their reports at the bottom of `REPORTS.md`.
- **`main` is protected (owner decision):** GitHub accepts a push to `main` only for a commit on which the CI job
  `gate` passed; no force push or deletion, admins included. Land with `python -I tools/orch/land.py` (owner rule in
  [`TASKS.md`](TASKS.md), top). Docs-only changes pass `gate` in about a minute.
- **Earlier that evening:** the W4-NEXT-1 interim review requested changes
  ([review](reviews/2026-11-10-w4-next-1-interim.md)), and W4-NEXT-2 was released: the selected-building panel,
  captures for the missing UI evidence, test memory (C-3), and the co-developer's shared test list measured in our sim
  and in the original.
- **MAP-EXTRACT-2:** the first Mac's run was interrupted at `ce14b264`. Its two red CI causes are fixed on
  `fix/MAP-EXTRACT-2-ci` (`a5a3bca3`, CI 10/10). Its game-file run failed 19: the 18 real-file tests `main` repaired
  in `5e7b13c6` (not on that branch), and the known `F4 (N4)` cold-cache defect. Consolidation into `main` waits for
  the owner's decisions on A6 and OQ-556.
- **The co-developer's share is now also a git mirror:** `E:\open1503-share\to-codev\anno1503.git`, every GitHub
  branch, updated by `tools/orch/sync_gitlab.py` with the GitLab mirror after every push.

Earlier on 2026-11-10, by the owner acting as orchestrator: **W4-NEXT-1 released to the Windows executor**
([packet](tasks/w4-next-1.md)), base `origin/main` `bd760031`. It takes the proposed coordinator follow-up (CI
flakes and two stalls on this PC, [review](reviews/2026-11-08-w4-ui-2-merge.md)) as part C, closes the three
W4-SETTLEMENT-1 review items left as part S, and builds the next in-game UI screens (ADR 0004 rows 19, 18, 5, 10,
2, plus the S/Q/D key-collision QUESTION) as part U. Branches: `task/W4-coord-1` (C), `task/W4-ui-3` (S, U).

Before that, 2026-11-09 morning, by the Claude Code orchestrator: **W4-UI-2 accepted and merged into `main` as
`96de6bca`**. The game-file run at `96de6bca`, made before the push, passed: 2,344 executed, 0 failed, 11 classified
skips. The overdue game-file run at `5e7b13c6` is recorded. A coordinator follow-up is proposed for the CI flakes and
two stalls on this PC ([review](reviews/2026-11-08-w4-ui-2-merge.md)). Before that, 2026-11-07 evening: W4-UI-2
released to the Windows executor and
MAP-EXTRACT-2 to the first Mac, both at the owner's request ([UI packet](tasks/w4-ui-2.md),
[mine packet](tasks/map-extract-2.md)); two owner-dispatched research notes checked and merged
(`docs/re/mine-deposit-followups.md`, `docs/re/complogic-ai-behavior.md`); the mine note answers the first Mac's
open save-load question. Earlier that afternoon: W4-MAIN-2 parts S, A, B and D reviewed; part
A (main's 18 real-file tests) merged into `main` as `5e7b13c6`, game-file run pending; the first Mac's faithful mine
default recorded (`task/MAP-EXTRACT-1-x2`, REVIEW PENDING). Earlier on
2026-11-07: W4-SETTLEMENT-1 reviewed, changes requested; 18 real-file test failures found on `main`; W4-MAIN-2 released
to the Windows executor; new owner rule on game-file runs for `main`. Before that: 2026-11-06, by the Codex
orchestrator (MAIN-CI-1 accepted; gameplay assignments unchanged).

**Who updates it:** every orchestrator, in the same commit as each dispatch, acceptance or change of lane. Executors do
not edit it; their reports go into `REPORTS.md` on their task branch.

## The people and sessions

| Who | Role |
|---|---|
| Owner | decides scope and "feeling" defaults, playtests, releases tasks |
| Co-developer | works on the project directly (from 2026-11-06) |
| Codex orchestrator | wrote the board's top dispatch, ran the two Mac lanes and the main consolidations; last on the board 2026-11-06 |
| Claude Code orchestrator | reviews and lands every lane on `main` (the Mac lane too since 2026-11-07); host and infrastructure on the Windows PC |
| Windows executor | Claude Code on the owner's Windows PC (RTX 4090): rendering, the live window, the opening |
| Mac executor | from 2026-11-11 one session on the owner's Mac: MAP-INTEGRATE-2 |
| First Mac executor | until 2026-11-10, the MAP executor: gameplay in the headless sim (supply, needs, production chains) |
| Second Mac executor | until 2026-11-06, gameplay integration: scripting, economy, expansion, routes |

## Current work

Canonical identity on `main`: **state134 / ECONBK1 / PRNG5 / replay4 / command1**.
The [founding prerequisite](reviews/2026-11-06-founding134-acceptance.md) and
[headless manual expansion](reviews/2026-11-06-map-expand1-acceptance.md) are accepted.
Live-window and target-host acceptance are separate.

**MAIN-CI-1 is CLOSED.** All ten GitHub CI jobs pass at repair source
`92e4817b`: all five C++ platform/configuration jobs, cpp-format and four Rust
jobs. [Acceptance, failures, exact counts and intake notes](reviews/2026-11-06-main-ci1.md).
The previous red-main condition since `944cec89` is repaired; the owner’s CI hold
before consolidation is satisfied. No further gameplay has been consolidated.

**By the owner rule of 2026-11-07 (TASKS.md, owner rules), `main` is healthy at `7babb1f2`** (the W4-NEXT-2 landing):
its code is the merge `c944aff7`, whose game-file run passed on the Windows PC before the push: 2,390 selected, 2,378
executed and passed, 0 failed, 12 classified skips ([acceptance](reviews/2026-11-11-w4-next-2-acceptance.md)). The
landing commit adds only docs. The docs-only commits after it need no run.
- **The run before it, at `2c4dc295`** (the W4-NEXT-1 merge): 2,367 selected, 2,355 executed and passed, 0 failed, 12
  classified skips, 800 s, no stall ([acceptance review](reviews/2026-11-10-w4-next-1-acceptance.md)).
- **The run before it, at `96de6bca`** ([review](reviews/2026-11-08-w4-ui-2-merge.md) §3): 2,355 selected; 2,344 executed and passed, 9 of them on a one-at-a-time rerun after memory refusals; 0
  failed. Skips: 6 opt-in, 2 platform, 3 symlink rows without the Windows privilege; 0 required-asset.
- **The run before it:** `5e7b13c6`, the 18 real-file repairs of W4-MAIN-2 part A. It ran on 2026-11-07 but was
  recorded only now (review §1): 2,320 of 2,321 passed. The one memory refusal passed alone.
- **How to run it on this PC:**
  - Run it as a detached process. Claude Code's background-task memory limit stops it otherwise.
  - Close large applications first.
  - Watch for coordinator stalls, and rerun any refused item one at a time.

  Review §3 has the details.

Every later code push to `main` needs its own game-file run on the Windows PC.

Compiler portability, asset-bound replay prerequisites, independently attributed
state134 economy pins, a metadata-test port collision and two dangling test-fixture
statistics pointers were corrected. No gameplay identity/default or production
coordinator change. The final source is independently reviewed and published.

Consumers adopt the portable repairs only at an owned checkpoint; do not overwrite
lane-specific132/133/135/136 identities with main134 test pins. The private GitLab
mirror still requires the owner/Windows sync after main publication.

### Windows executor

- **2026-11-11:** a Linux session started W4-NEXT-2 on `claude/dreamy-brahmagupta-10bjax`, head `15f01fa4`, CI green
  at `8b807827`.
  - **What it built:** K's selection logic (title bar only, no panel) and M's peak-memory launcher.
  - **Left NOT_RUN:** everything that needs this PC.
  - **Owner instruction:** the Windows executor continues on `task/W4-ui-4` from `15f01fa4`. See the packet's
    "Continuation on the Windows PC".
  - **The mirrors:** that session force-pushed its branch. Nothing was lost (every old commit is on `main`). The owner
    approved one forced update of that branch on GitLab and the share (2026-11-11). Both are at `15f01fa4` again, all
    103 branches equal; the no-force rule stands.
- **Now: free** for the next dispatch.
- **Finished: W4-NEXT-2 part H and round H4** (2026-11-12; head `1f1d3889`), **accepted**
  ([part H review](reviews/2026-11-12-w4-next-2-part-h-review.md)):
  - **H1:** the two sprite-sad normalizations and G6's conclusions corrected;
  - **H2:** the shared list's scenario folders, and what the New Game list holds as the next attempt's first check;
  - **H3:** report;
  - **H4:** OQ-565's and OQ-567's own figures measured with the calibrated method, every sprite on screen; F6's unit;
    the line references; an independent review.
- **Finished: W4-NEXT-2's fix round G** (2026-11-11; heads `b483c4de` and `b2808533`), from `2138773e` and
  `e69b036d`, **accepted** ([acceptance](reviews/2026-11-11-w4-next-2-acceptance.md)):
  - **G1:** the selection facts and the window title from one state (the P1 closed);
  - **G2–G5:** a HUD-click step that can fail, the pixel check's bound, ADR 0004's sentence, `population`'s exit
    code;
  - **G6:** grey-key defined (its calibration conclusion is part H's finding 1);
  - **G7, G8:** the next path for the original's column (part H's finding 2); the missing F entries;
  - **G9:** review and gates.
- **Finished: W4-NEXT-2's fix round, part F** (2026-11-11; heads `2138773e` and `e69b036d`), from `f1f772b4` and
  `593cdd3f`:
  - **F1–F3:** the selected-house session waits on state (the race narrowed, not closed); HUD clicks stay off the
    world; the demolish-while-selected step uses `local demolish`;
  - **F5:** the population tool's step, rules and checkpoints fixed; OQ-571 re-measured;
  - **F6–F8:** part W's report (sprite-sad calibration not reached), the C-3 table in git, one attempt at the
    original's column (still NOT_RUN);
  - **F9:** review and gates.
- **Finished: W4-NEXT-2 parts P → K → W → M → X → E** (2026-11-11), from `main` at `2c4dc295`:
  - **P:** preflight, plus the GPU pixel check that W4-NEXT-1 left NOT_RUN;
  - **K:** row 18, select a building;
  - **W:** captures in the original for OQ-565/566/567, then rows 19 and 2;
  - **M:** C-3 test memory;
  - **X:** the co-developer's shared test list.
- **Done: [W4-NEXT-1](tasks/w4-next-1.md), ACCEPTED 2026-11-10 and merged** as `fc126fbe` (`task/W4-coord-1`
  `2cf6f4c2`, part C) and `2c4dc295` (`task/W4-ui-3` `c7ad492f`, parts S, U, E)
  ([acceptance](reviews/2026-11-10-w4-next-1-acceptance.md), [interim](reviews/2026-11-10-w4-next-1-interim.md)).
  - **Part C:** the coordinator's port flakes and the bound on the client's wait.
  - **Part S:** the three W4-SETTLEMENT-1 items, which close out W4-SETTLEMENT-1 below. Its playtest card is
    `docs/testing/settlement-inspection-playtest.md`.
  - **Part U:** rows 5 and 10, and `--ui classic|dev`.
- **Before that:** **W4-SETTLEMENT-1.** A developer inspection UI for live building, production and household state, built and
  proven in the real window.
  - The owner dispatched it in chat on 2026-11-06. It is not on the board yet.
  - **Done:** W2–W5; the MAIN-CI-1 intake (`b7be2a1d`, accepted); a complete W7 run (`w7final2`).
  - **Reviewed 2026-11-07: CHANGES REQUESTED** ([review](reviews/2026-11-07-w4-settlement-1-w7.md)).
    - The W7 run's raw logs were deleted with its gate worktree. Three lanes fail and three sim tests never ran.
    - Code findings: one clang-format blocker; `wait turns` still races; the inspection line can show the wrong building;
      the gate script exits 0 on failed lanes and keeps its logs inside the tested worktree.
    - `picture_matrix_contract` passes in all five lanes.
  - **[W4-MAIN-2](tasks/w4-main-2.md),** released 2026-11-07. Its parts are
    [reviewed](reviews/2026-11-07-w4-main-2.md), except C, which is still running:
    - **S:** the W7 review fixes, `task/W4-settlement-1` at `8532c450`.
      - Findings 1–8 are fixed and finding 9 partly.
      - Two small follow-ups: the negative step should hover instead of click; formatter tests for
        `inspect_building`.
      - The report wrongly says the WSL gate runs the heavy tests.
    - **A:** main's 18 real-file tests, `task/MAIN-REALFILES-1` at `8175df79`. **Accepted, merged into `main` as
      `5e7b13c6`.** Its report gives a wrong cause for 13 render refusals; correction requested.
    - **B:** the Windows lane on main 134, `task/W4-main-2` at `85c4aeae`.
      - The merge is correct. Its MSVC run has 4 memory refusals and the known `F4 (N4)` cold-cache defect.
      - Acceptance waits for C.
    - **C:** the WSL gate `w4main2c1` at the B head. At 15:10: app, sim and formats PASS with no sanitizer reports;
      the render lane was running. C2–C4 are not reported yet.
    - **D:** the Load adapter, `task/W4-expand-live-1` at `9fd9c216`. **Changes requested.**
      - The pushed head lacks the scripted `l` key that its own window test needs. It sits uncommitted in the
        worktree, so the reported passes ran on a dirty tree.
      - The "pending Load" test applies the Load before saving.
      - The card's recipe replay is still wanted, as its own checkpoint.
  - **C1 at 17:24:**
    - the CPU render lane ended FAIL on the known `flow build ... built once` case, 184 of 185 executed;
    - the lavapipe GPU lane had run for about two hours and was still running.
  - **[W4-UI-2](tasks/w4-ui-2.md): DONE, accepted and merged into `main` as `96de6bca` on 2026-11-09**
    ([review](reviews/2026-11-08-w4-ui-2-merge.md)).
    - The independent review requested changes; they were fixed in `b3d8793d`, and the focused re-review approved them.
    - CI passed 10/10 at `d470825f`. The game-file run at the merge commit passed.
    - It was released 2026-11-07 by the Claude Code orchestrator at the owner's request. It builds the original's
      in-game UI in the live window, the owner's chosen test surface (decision M3):
    - **F:** UI.1.F2.1 closed, the graphics of the build-test surface;
    - **R:** ADR 0004, with owner questions on the default surface and the UI scale;
    - **H, C:** the HUD, and the construction menu with placement and demolish through the real command path;
    - **M:** every other menu, as placeholders;
    - **V, K, E:** proof, a playtest card, the final gate.

    It changes no sim behaviour.
  - **After that:** the owner's playtest cards, the new UI card among them.
- **Found by the W7 review, on `main`:** 18 real-file tests failed at `96775da4`; repaired by part A, merged as
  `5e7b13c6`. Play mode still refuses 39 of the 50 original scenes; view mode (`--view`) still opens them.
- **Branch:** `task/W4-settlement-1`, head `8532c450` (pushed; the reviewed W7 head was `384df19d`).
  - It is based on `task/W4-opening-2-main`: `main` `3b74201f` plus the reviewed W4-OPENING-CONTROLS-FLAKE delta.
- **Also open:**
  - `task/W4-opening-2` (`3fd68746`): W4-OPENING-CONTROLS-FLAKE steps 0–3, on the older base `88968580`. It was adopted
    onto the consolidated main as `task/W4-opening-2-main` (`11e3c687`).
  - `task/W4-depth-1` (`5eaef168`): W4-DEPTH-1-CLOSE, per-pixel sprite depth. It waits on the owner's depth check, and
    it merges into `main` as its own package.
- **Queued, not released:** W4-SHORE-2, the sea against its plane per pixel.
- **Pushed:** all four branches, on 2026-11-06.

### Mac executor (from 2026-11-11)

- **Now: free.** The next Mac packet comes from the queue on the board, one at a time.
- **Finished: [MAP-INTEGRATE-2](tasks/map-integrate-2.md)** (2026-11-12; head `55ce7e71`), **accepted**
  ([review](reviews/2026-11-12-map-integrate-2-review.md)), 13 h 20 min. Released 2026-11-11, parts
  S → P → I0–I5 → R → N → E.
  - **Inputs (frozen):**
    - `fix/MAP-EXTRACT-2-ci` `a5a3bca3`: 135 plus 137, CI 10/10;
    - `claude/ci-fix-map-route-1` `49ffcee6`: 136, CI green.
  - **Branch:** `task/MAP-INTEGRATE-2` from `origin/main`.
  - **Identity:** **state 138**, reserved. It refuses 134 to 137 explicitly. PRNG5, replay4, command1 and ECONBK1
    stay only if the combination proves they may.
  - **The shared folder:** the Mac joins `open1503-share` as **Receive Only**. The PC side is done (2026-11-11): the
    device `Mac` is added and the folder is shared with it. Still to do: the owner accepts the PC and the folder on
    the Mac.
  - **Still the owner's:** A6 (rebind a mine at load). The combination keeps option B, as implemented.
- The two sections below are the Mac lanes' record up to their final candidates.

### First Mac executor

- **2026-11-10: the run was interrupted at `ce14b264`; the Claude Code orchestrator owns the consolidation.** All
  parts were reported, with a final review ACCEPT with conditions. Its CI was red for two reasons: an iron test that
  asserted after its own skip, and an MSVC C4805 warning. Both are fixed on `fix/MAP-EXTRACT-2-ci` (`a5a3bca3`; CI run
  42804157700 passed 10/10). Its game-file run on this PC: 2,402 executed and passed, 19 failed (the 18 real-file tests
  `main` repaired in `5e7b13c6`, and the known `F4 (N4)` cold-cache defect, which passed alone). Still open: the owner's
  decisions A6 (rebind a mine at load) and OQ-556; an independent review of the T5 fix; the merge with `main`, which
  renumbers state 137.
- **[MAP-EXTRACT-2](tasks/map-extract-2.md), released 2026-11-07 by the Claude Code orchestrator at the owner's
  request.** It runs on the new branch `task/MAP-EXTRACT-2`, from `2f32437a`.
  - **A:** the mine's lifecycle in the original: the save-load finding re-checked, load order, the ore object's
    getter, the owner question on rebinding at load. Docs and tests only; identity stays 135.
  - **T:** the tools chain (iron mine, charcoal burner, smelter, toolsmith) up to tools spent on a later building.
  - **Q:** the quarry: research and contract first, then bricks and marble if the contract closes.
  - **G:** gold and gems, inventory only.
  - **E:** the final gate and review, then stop.
  - **Identity 137** is reserved for T and Q. It builds on 135 and does not include the second Mac's 136.
- **Before that: MAP-EXTRACT-1, salt extraction, with the faithful mine default. Candidate complete; REVIEW
  PENDING.** The executor's last checkpoint: 2026-11-07, `2f32437a`; it reported the lane idle that evening.
  - **The faithful default** replaces policy B; the owner decided it on 2026-11-07.
    - A mine at a legal site with no ore of its ware in reach is accepted, charged and created unbound. It stays idle
      and never searches again.
    - A mine built or rebuilt where ore is in reach binds it.
    - `Reject::NoDepositInReach` stays reserved and is never produced.
    - A read-only query, `mine_site_deposit_in_reach`, lets a host warn the player before sending the command.
    - Commits: code `c03589d7`, review P3s `e90eabb3`.
  - **Orchestrator spot-check** of the original `AnnoWelt.dll` (read-only, 2026-11-07). Two of the three cited byte
    claims are independently confirmed:
    - the attach at `0x1006e299` overwrites the deposit lookup's return value before returning;
    - the mine step `0x1006def0` jumps past its search at `0x1006df59` when the mine's deposit field (+0x24) is empty.

    The third, that the placement predicate `0x1007c790` reads no ore, was not re-checked.
  - **Executor gates on `e90eabb3`:**
    - Debug canonical: 1,694 selected, 1,688 executed, 0 failed, 6 opt-in skips;
    - focused ASan: 52 selected, 51 executed, 0 failed;
    - GCC 13.3 compile of the touched test, in an arm64 container.
  - **CI:** run 42368776369 at `9a094b05` and run 42373041769 at `bd725d4e` passed all ten jobs each (checked).
  - **A CI failure, now recorded in the branch's REPORTS (`2f32437a`).** Run 42360315619 at `fa100f8f`, a docs-only
    commit, failed in
    `linux-gcc-release` on `anno_test_support_tests.V3: the predecessor journal states ...`, at `budget_f5_test.cpp:998`:
    `JOURNAL-REFUSED` was expected, and six assertions failed.
    - The same code passed at `5939efc7` and `9a094b05`. The test comes from `main`, not from this package.
    - Probable cause: the coordinator test ports lie inside Linux's ephemeral range. The unmerged fix is
      `claude/dreamy-brahmagupta-10bjax` (`e5d0b877`). This is not proven, because the test does not log the message
      it received.
  - **NOT_RUN:** MSVC and native Windows/Linux with the original files, Release, the full sanitizer campaign, GOG
    versus History Edition.
  - **Answered in the original since then (research note, checked by the orchestrator, on `main`):**
    [`docs/re/mine-deposit-followups.md`](../re/mine-deposit-followups.md).
    - The original never saves a mine's deposit binding; its 10-byte mine record has no such field.
    - Each construction, at placement and at load alike, runs the deposit lookup again. The mine step repeats it when
      its bound deposit is spent or comes up short.
    - So a reloaded mine binds whatever the lookup finds at load. The branch keeps the saved binding, and an unbound
      mine stays unbound across a reload.
    - Still open: the loader's chunk order, and what the lookup filters on.
  - **Still open in the original:** whether the AI uses the lookup before placing; the quarry questions; the
    discovery-mask getter.
  - Hunting remains PARTIAL and outside this packet.
- **Next for MAP-EXTRACT-1 (separate from the new packet):** the acceptance review and the state135 consolidation into
  `main`, then the game-file run on the Windows PC (owner rule). Changes that review asks for go onto
  `task/MAP-EXTRACT-1-x2` first, then merge into `task/MAP-EXTRACT-2`.
- **Branch:** `task/MAP-EXTRACT-1-x2`, head `2f32437a` (a docs-only closeout after `bd725d4e`), clean tree; the lane
  is idle. It contains `task/MAP-EXTRACT-1-main`
  (`3d9dbafe`) and the earlier NEEDS candidate `5df34b38` (`task/MAP-2-needs2-main`). None of these is merged into
  `main`.
- **Identity:** state135 (`kStateHashVersion = 135`). The faithful default changes no state field, stream or hash.
- **Publication:** ordinary push of the owned task branch at every checkpoint and
  report. Never force or push main; existing merge/rebase/other-lane boundaries remain.

### Second Mac executor

- **Completed and accepted on main:** **MAP-EXPAND-1 E0–E7**, local headless scope.
  Two founded islands, two new-wood freight cycles, receiving construction,
  exact ledgers, five full native continuations and honest/changed fresh replay.
- **Branch:** `task/MAP-EXPAND-1-main`, published checkpoint `a4617184`;
  tested source `ca257614`. Main integration source `2e2ce074` preserves current134.
- **Verification:** executor Debug75/75 and combined ASan78/78 (seven required
  contracts separately each); new main build and short controls4/4. Long evidence
  is reused by exact dependency, not represented as another main voyage run.
- **Now dispatched:** **MAP-ROUTE-1 R0–R7**, automatic freight between owned
  warehouses: original contract/reference, typed route state and commands,
  scheduler/cargo integration, interruption/recovery, real production-backed
  cycles, save/replay, bounded scale and independent package/final reviews.
- **Successor:** `task/MAP-ROUTE-1-main`, to be created in the existing owned
  checkout from accepted main `e0bb6be7`. This is an assignment, not a claim that
  execution has started. Preserve published expansion branch `a4617184`.
- **Identity:** state136 reserved for route state; starts from134 and does not
  include first Mac's reserved135 salt. Orchestrator owns combined integration.
  Existing feature defaults and PRNG5 remain unchanged. Essential unresolved
  original predicates or a timeline/schema conflict stop dependent work.
- **Outcome:** at least two automatic freight cycles of newly produced wood,
  without per-stop commands from the test driver. First Mac's salt work stays
  separate; no foreign trade, wildlife, viewer/UI or target-host claims.
- **Windows handoff:** [manual expansion consumer card](../testing/manual-expansion.md).
  Load UI/text wiring and a live playtest remain Windows work after its current
  checkpoint; no automatic new dispatch or W7 interruption is implied.
- **Publication:** ordinary owned task-branch pushes at checkpoints; never force
  or push main. Existing merge/rebase/other-lane boundaries remain.

The first Mac section records its last observed checkpoint, not live job state.
Its later work is not accepted merely because expansion is complete.

## Waiting on a run

- None. **`main` is healthy at `7babb1f2`** (the W4-NEXT-2 landing; its code is the game-file-tested merge
  `c944aff7`). The counts and the earlier runs are under [Current work](#current-work). Part H is docs only and needs
  no run.

## Waiting on the owner

- **On the Mac, accept this PC and the `open1503-share` offer** in Syncthing, as Receive Only (MAP-INTEGRATE-2
  part S, step 4).
- **OQ-568:** may the play runner's `stop()` drop a command posted during an artificial turn delay? Only tests set
  such a delay. The executor recommends (a), leave it; the orchestrator agrees and would add (c), document it in
  `PlayRunner::stop()`.
- **When to add the C-3 reservations:** 18 tests above 1 GiB take no memory reservation (W4-NEXT-2 part M). The
  recommendation is a reservation sized from each test's measured peak, as a separate task.
- **A6, rebind a mine at load** ([packet](tasks/map-extract-2.md) A6). The executor recommends B, keep the saved
  binding. MAP-INTEGRATE-2 keeps B until the owner answers.
- **The W4-UI-2 playtest card** (part K): the HUD and construction menu in the live window, on `main` from
  `96de6bca`.
- **ADR 0004 questions R2 and R3:** the default surface (`--ui classic|dev`) and the UI scale.
- **The coordinator follow-up** ([review](reviews/2026-11-08-w4-ui-2-merge.md) §4) is assigned: W4-NEXT-1 part C,
  to the Windows executor, root cause and fix, then 5 consecutive clean CI runs.
- **The depth check** for W4-DEPTH-1 (the playtest card on `task/W4-depth-1`).
- **The opening playtest** in the live window (W4-OPENING-1, accepted 2026-11-05).
- **W4-SETTLEMENT-1's playtest card,** once review item 3 (the inspection line) is fixed.
- **Whether the developer tool may place buildings of later stages** than the start allows. Main's eligibility rule
  refuses them, as the original does. W4-MAIN-2 adds no "unlock all" switch without the owner's word.
- **Optional:** overriding the River Q5 floor of 0.45 for `D_256x256_0045_5B` (W4-MAIN-1 review D1).
- **Optional:** whether the enhanced water is lit from the top left again (OQ-026).

## Where to look

- **Repositories:**
  - **GitLab `gitlab.com/fuitable/open1503`, private:** the co-developer's repository, a mirror of every GitHub branch
    (`tools/orch/sync_gitlab.py`). It is updated after each push to `main` and each executor push.
  - **GitHub `Dom-cmyk727/anno_rebuild`, public:** the dispatch channel and CI. Visibility is a separate owner
    decision. No original files, decoded assets, listings or credentials may be published there.
- **Task branches:** every executor pushes its own task branch at each checkpoint (owner rule, 2026-11-06). The branch
  names are the ones above.
- **`main`** is the canonical, reviewed state. It is protected: only a commit on which the CI job `gate` passed can be
  pushed to it, with `tools/orch/land.py`.
- **[`TASKS.md`](TASKS.md):** the owner rules, the open dispatches that live on the board, §1 (the protocol) and a
  section saying what moved out. New dispatches are packet files in [`tasks/`](tasks/).
- **[`REPORTS.md`](REPORTS.md):** the executors' reports, newest at the end. Reports still on a task branch appear here
  when that branch is merged.
- **[`HISTORY.md`](HISTORY.md), [`HISTORY-2.md`](HISTORY-2.md) and [`REPORTS-1.md`](REPORTS-1.md):** the board's
  closed history (2026-10-28 and 2026-11-10) and the older reports, moved out verbatim.
- **[`reviews/`](reviews/):** one file per orchestrator review or acceptance.
- **[`docs/PLAN.md`](../PLAN.md):** milestones and status; §7 holds the latest status notes.

## Mirror coordination

The owner reports the formerly stale private GitLab mirror is now current.
After each new main push and task-branch checkpoint, the owner or Windows Claude
Code orchestrator runs `python -I tools/orch/sync_gitlab.py` and verifies heads
(`tools/orch/land.py` runs it after landing on `main`; landing branches
`land/*` are not mirrored).
Only ordinary pushes are allowed, never force or deletion. Another host may run
it if its `gitlab` remote has working SSH access. This Mac's earlier read-only
probe failed host-key verification; no trust setting was changed. A GitHub push
alone does not prove that its new commit has already reached GitLab.
