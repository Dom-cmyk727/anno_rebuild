# Who is working on what

A one-page view of the current work, for the owner and the co-developer. The task board
[`TASKS.md`](TASKS.md) holds the full instructions and [`REPORTS.md`](REPORTS.md) the executors' reports. This page only
says who is doing what, where the work is, and what it waits on.

**Last updated:** 2026-11-06, by the Codex orchestrator (state-134 prerequisite; Windows lane retained).

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

- **Now:** **close MAP-NEEDS-2 (C0–C2), then MAP-EXTRACT-1 (X0–X6), salt extraction** (TASKS.md, "First Mac — close
  MAP-NEEDS-2, then MAP-EXTRACT-1"; [review](reviews/2026-11-06-map-needs2-orchestrator.md)).
  - The MAP-NEEDS-2 candidate `5df34b38` (sheep, wool and cloth; hides and leather) is reviewed and preserved, but not
    accepted on `main`.
  - Hunting stays PARTIAL and outside this packet.
- **Branch:** `task/MAP-EXTRACT-1-main`, a new branch from the `main` that carries the dispatch and the bench fix
  `b408f311`. The candidate stays on `task/MAP-2-needs2-main` (`5df34b38`).
- **Identity:** state 135 is reserved for the salt slice. It may only be claimed after the second Mac's reviewed 134
  correction has been taken in. That prerequisite is now accepted: consume `7ba07750`, then `f406374d`,
  at a clean checkpoint while preserving owned work. Salt X1 contract approval is still required.

### Second Mac executor

- **Now:** **MAP-EXPAND-1** (TASKS.md, "Second Mac — MAP-EXPAND-1" and "Compatibility decision — founding accounts and
  salt sequencing"): settle a second island and resupply it by ship, with goods produced by the first settlement.
  - **Accepted prerequisite:** `city.accounts = local.accounts` under state134, with strict133 refusal.
    Portable `7ba07750` plus CLI-consumer correction `f406374d`; focused Debug15/15 and ASan15/15.
  - **Now:** E2 revalidation and E3–E7. Full voyage acceptance remains open.
  - No new production class, trade route, combat or UI is in scope.
- **Branch:** `task/MAP-EXPAND-1-main`.
  - It started from exact assignment `c5c7007f` (runtime baseline `303b3faf`); historical partial checkpoint `71af3846`.
  - The tested source `637da167` is the partial evidence pin.
- **Just finished:** ECON-BOOK-1, earned income and building upkeep.
  - It was accepted on 2026-11-06 ([acceptance](reviews/2026-11-06-econ-book1-acceptance.md)) and is on `main`.

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
  - **GitHub `Dom-cmyk727/anno_rebuild`:** the dispatch channel and CI.
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

## Mirror handoff for this acceptance

Windows orchestrator must run `python -I tools/orch/sync_gitlab.py` after the
state134 acceptance push and verify the resulting heads. The Mac SSH read-only
probe failed host-key verification; no trust settings were changed or GitLab
push attempted. GitHub publication does not itself establish mirror currency.
