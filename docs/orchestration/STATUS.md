# Who is working on what

A one-page view of the current work, for the owner and the co-developer. The task board
[`TASKS.md`](TASKS.md) holds the full instructions and [`REPORTS.md`](REPORTS.md) the executors' reports. This page only
says who is doing what, where the work is, and what it waits on.

**Last updated:** 2026-11-06, by the Codex orchestrator (owner publication rules and observed Mac checkpoints; Windows lane retained).

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
The [founding-account prerequisite](reviews/2026-11-06-founding134-acceptance.md) is accepted; full expansion remains open.

### Windows executor

- **Now:** **W4-SETTLEMENT-1.** A developer inspection UI for live building, production and household state, built and
  proven in the real window.
  - The owner dispatched it in chat on 2026-11-06. It is not on the board yet.
  - **Done:** W2–W5, with the MSVC regression gate green.
  - **Now:** the W7 WSL ASan/UBSan gate. The first try was blocked when C: filled up; WSL now runs from E:. The gate
    script `tools/test/wsl_gate.sh` was first updated to the new WSL disk rules (TASKS.md, W4-OPENING-CONTROLS-FLAKE
    step 0).
  - **Then:** the review and the owner's playtest card.
- **Branch:** `task/W4-settlement-1`, head `6b327759`.
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

- **Now:** **MAP-EXPAND-1, E5 continuation/replay work and remaining closeout.**
  - The narrow134 founding prerequisite is accepted on main: `7ba07750` plus
    `f406374d`, focused Debug15/15 and ASan15/15.
  - Latest observed committed checkpoint: **`bdb77ec7`**. Its report records
    independent E4 approval at `4fa38e69`: two actual freight cycles completed;
    E5 released. This is executor evidence, not final orchestrator acceptance.
  - CLI companion `f406374d` is adopted locally as `46ec410a`.
  - Full continuation/fresh-process replay, combined voyage sanitizer execution
    and E7 acceptance remain open in that checkpoint.
- **Branch:** `task/MAP-EXPAND-1-main`, from assignment `c5c7007f` (runtime baseline
  `303b3faf`). Historical `71af3846` / tested `637da167` remain partial evidence;
  they no longer describe the latest work.
- **Already finished:** ECON-BOOK-1, accepted on main; no new production class,
  automatic trade route, combat or UI is assigned to expansion.
- **Publication:** ordinary push of the owned task branch at every checkpoint and
  report. Never force or push main; existing merge/rebase/other-lane boundaries remain.

These are observed committed checkpoints, not live job status or confirmation that
those task heads are already on GitHub. Executors may be working beyond them.

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
