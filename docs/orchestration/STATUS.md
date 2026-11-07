# Who is working on what

A one-page view of the current work, for the owner and the co-developer. The task board
[`TASKS.md`](TASKS.md) holds the full instructions and [`REPORTS.md`](REPORTS.md) the executors' reports. This page only
says who is doing what, where the work is, and what it waits on.

**Last updated:** 2026-11-07 evening, by the Claude Code orchestrator: MAP-EXTRACT-2 released to the first Mac at the
owner's request ([packet](tasks/map-extract-2.md)); two owner-dispatched research notes checked and merged
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
| Codex orchestrator | writes the board's top dispatch; runs the two Mac lanes and the main consolidations |
| Claude Code orchestrator | reviews the Windows lane and moves `main` for it; host and infrastructure on the Windows PC |
| Windows executor | Claude Code on the owner's Windows PC (RTX 4090): rendering, the live window, the opening |
| First Mac executor | the MAP executor: gameplay in the headless sim (supply, needs, production chains) |
| Second Mac executor | gameplay integration: scripting, economy, expansion |

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

**By the owner rule of 2026-11-07 (TASKS.md, owner rules), `main` is "CI green, game-file run pending" at
`5e7b13c6`.** That rule counts `main` as healthy only once its game-file tests have run on the Windows PC.
- **What failed:** 18 out-of-date real-file tests, reproduced at `96775da4`. They broke after the strict scene loader
  (`f7c31ed8`) and the construction-eligibility check (`d2be23aa`).
- **The repair:** W4-MAIN-2 part A (`task/MAIN-REALFILES-1`). It was [reviewed and accepted](reviews/2026-11-07-w4-main-2.md)
  and merged into `main` as `5e7b13c6` on 2026-11-07. On its own branch all 18 pass in the full MSVC run with the game
  files.
- **Still to do:** the owner-rule run at `5e7b13c6` itself. It runs once the Windows lane's WSL gate has freed the PC.
  A full run with `-j 16` hits memory refusals on this PC; see the review.

Every later code push to `main` needs its own game-file run on the Windows PC.

Compiler portability, asset-bound replay prerequisites, independently attributed
state134 economy pins, a metadata-test port collision and two dangling test-fixture
statistics pointers were corrected. No gameplay identity/default or production
coordinator change. The final source is independently reviewed and published.

Consumers adopt the portable repairs only at an owned checkpoint; do not overwrite
lane-specific132/133/135/136 identities with main134 test pins. The private GitLab
mirror still requires the owner/Windows sync after main publication.

### Windows executor

- **Now:** **W4-SETTLEMENT-1.** A developer inspection UI for live building, production and household state, built and
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
  - **After that:** the owner's playtest cards.
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

### First Mac executor

- **Now: [MAP-EXTRACT-2](tasks/map-extract-2.md), released 2026-11-07 by the Claude Code orchestrator at the owner's
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

- **The game-file run at `5e7b13c6`** (owner rule): the full MSVC suite with the game files and the RTX 4090. The
  Claude Code orchestrator runs it on the Windows PC once the Windows lane's WSL gate has finished.

## Waiting on the owner

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
- **`main`** is the canonical, reviewed state, and CI runs on every push to it.
- **[`TASKS.md`](TASKS.md):**
  - the top dispatch is the newest;
  - each task's section holds its full instructions;
  - "Other lane dispatches" holds the per-lane notes.
- **[`REPORTS.md`](REPORTS.md):** the executors' reports, newest at the end. Reports still on a task branch appear here
  when that branch is merged.
- **[`reviews/`](reviews/):** one file per orchestrator review or acceptance.
- **[`docs/PLAN.md`](../PLAN.md):** milestones and status; §7 holds the latest status notes.

## Mirror coordination

The owner reports the formerly stale private GitLab mirror is now current.
After each new main push and task-branch checkpoint, the owner or Windows Claude
Code orchestrator runs `python -I tools/orch/sync_gitlab.py` and verifies heads.
Only ordinary pushes are allowed, never force or deletion. Another host may run
it if its `gitlab` remote has working SSH access. This Mac's earlier read-only
probe failed host-key verification; no trust setting was changed. A GitHub push
alone does not prove that its new commit has already reached GitLab.
