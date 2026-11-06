# Who is working on what

A one-page view of the current work, for the owner and the co-developer. The task board
[`TASKS.md`](TASKS.md) holds the full instructions and [`REPORTS.md`](REPORTS.md) the executors' reports. This page only
says who is doing what, where the work is, and what it waits on.

**Last updated:** 2026-11-06, by the Codex orchestrator (MAP-ROUTE-1 dispatch and observed Mac checkpoints); Windows lane and the `main` CI note by the Claude Code orchestrator, 13:30.

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

**`main`'s C++ CI is red** on all five build jobs (MSVC, GCC, GCC release, Clang ASan, macOS Clang). It has been red
since `944cec89` (2026-11-05 22:38, the consolidated baseline); the last green run was `cafa74d5`. On `e0bb6be7`:
- `src/sim/tests/integration_identity_test.cpp:27`: `std::filesystem::path` is used without `<filesystem>` (MSVC, GCC
  and Clang);
- `src/sim/src/script_runtime.cpp:146`: `-Werror=null-dereference` (GCC release);
- macOS: `economy_loop_fresh_process`, `economy_loop_changed_command_fresh_process`, `script2_synthetic_fresh_process`
  and `script2_original_fresh_process` fail.

A WSL GCC 13 build with `-Werror` also stops in `small_farm_worker_test.cpp` (`std::function` without `<functional>`,
`-Wrange-loop-construct`), `unit_import_test.cpp:414/423` (`-Wrange-loop-construct`) and `scenario_test.cpp`
(`-Wsign-conversion`). That build compiled the Windows executor's live worktree, not a clean checkout, so treat this list
as a lead. Every task branch based on `main` since `944cec89` inherits these failures.

### Windows executor

- **Now:** **W4-SETTLEMENT-1.** A developer inspection UI for live building, production and household state, built and
  proven in the real window.
  - The owner dispatched it in chat on 2026-11-06. It is not on the board yet.
  - **Done:** W2–W5, with the MSVC regression gate green.
  - **Now:** the W7 WSL ASan/UBSan gate is **BLOCKED** by the red `main` above; the branch's base `3b74201f` is red
    on CI too.
    - WSL now runs from E:, and the gate script follows the new WSL disk rules (`6b327759`).
    - The first full run (`w7scoped`, 08:51) failed in every lane on the required `picture_matrix_contract`
      ("aggregate of a missing trace: exit 127, expected 2"). The cause is not yet found.
    - The app-lane rerun `w7app1` was voided and stopped.
    - The rerun `gatecheck2` was meant to use a clean checkout at `743ccf9c`. It reused a CMake cache that the voided
      run had left, so it built the live worktree. It is not evidence of the clean head. The gate script needs a
      fix: it must reconfigure when the cache belongs to another source folder.
  - **Then:** the review and the owner's playtest card.
- **Branch:** `task/W4-settlement-1`, head `f1dc0baf` (pushed).
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
- **W4-SETTLEMENT-1's playtest card,** once W7 is back.
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
