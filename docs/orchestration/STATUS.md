# Who is working on what

A one-page view of the current work, for the owner and the co-developer. The task board
[`TASKS.md`](TASKS.md) holds the full instructions and [`REPORTS.md`](REPORTS.md) the executors' reports. This page only
says who is doing what, where the work is, and what it waits on.

**Last updated:** 2026-11-07, by the Claude Code orchestrator: W4-SETTLEMENT-1 reviewed, changes requested; 18
real-file test failures found on `main`; W4-MAIN-2 released to the Windows executor. Before that: 2026-11-06, by the Codex orchestrator (MAIN-CI-1 accepted;
gameplay assignments unchanged).

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
  - **Now:** fix review items 1–8.
  - **Then, with no stop: [W4-MAIN-2](tasks/w4-main-2.md),** released 2026-11-07. Its parts:
    - **A:** repair main's 18 out-of-date real-file tests (`task/MAIN-REALFILES-1`).
    - **B:** bring the Windows lane onto main 134 (`task/W4-main-2`).
    - **C:** one W7 and the Windows host gates at that head. This replaces the old-base W7 rerun.
    - **D:** the Load adapter and the live two-island recipe (`task/W4-expand-live-1`).

    Every part ends REVIEW PENDING, for the orchestrator's review the next morning.
  - **After that:** the owner's playtest cards.
- **Found by this review, on `main` (not this package):** 18 real-file tests fail at `96775da4`. Hosted CI can't see
  them because it has no game files.
  - The strict scene loader (`f7c31ed8`) refuses `Single_01.sze`; only 11 of the 50 original scenes still load.
  - The construction-eligibility check (`d2be23aa`) stage-locks the buildings the legal workload chooses.
  - The tests are out of date. W4-MAIN-2 part A repairs them on a branch, and the orchestrator merges it after review.
- **Branch:** `task/W4-settlement-1`, head `384df19d` (pushed).
  - It is based on `task/W4-opening-2-main`: `main` `3b74201f` plus the reviewed W4-OPENING-CONTROLS-FLAKE delta.
- **Also open:**
  - `task/W4-opening-2` (`3fd68746`): W4-OPENING-CONTROLS-FLAKE steps 0–3, on the older base `88968580`. It was adopted
    onto the consolidated main as `task/W4-opening-2-main` (`11e3c687`).
  - `task/W4-depth-1` (`5eaef168`): W4-DEPTH-1-CLOSE, per-pixel sprite depth. It waits on the owner's depth check, and
    it merges into `main` as its own package.
- **Queued, not released:** W4-SHORE-2, the sea against its plane per pixel.
- **Pushed:** all four branches, on 2026-11-06.

### First Mac executor

- **Now:** **MAP-EXTRACT-1: salt contract and implementation preparation.**
  C0–C2 NEEDS corrections are reported complete locally, with review notes at
  `7b480584`; this is not a new orchestrator acceptance of that whole candidate.
  - Accepted state134 intake is present: `668ff3c6` (portable founding fix) and
    `e067d109` (current replay CLI registrations).
  - Latest observed committed checkpoint: **`c177f391`**, re-pinning NEEDS hashes
    at134 with unchanged reported numerical behavior and fresh replay results.
  - X1/X2 preparation is committed. Runtime salt work remains conditional on the
    required independent contract approval; no completed salt result is claimed.
  - Hunting remains PARTIAL and outside this packet.
- **Branch:** `task/MAP-EXTRACT-1-main`. Earlier NEEDS candidate `5df34b38` remains
  preserved on `task/MAP-2-needs2-main`; owned corrections are not merged into main.
- **Identity:** current intake134; **135 reserved for salt**. The founding dependency
  is consumed, not still waiting to be supplied.
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
