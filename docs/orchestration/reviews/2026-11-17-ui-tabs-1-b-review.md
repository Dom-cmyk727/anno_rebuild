# Review: UI-TABS-1 stage B (the balance pages' rows)

Reviewer: the orchestrator, 2026-11-17. Branch `task/UI-TABS-1`, head `5e6c1ad5`:
- B `dd18401f`;
- the merge of `origin/main` `09315620`, which brings stage A;
- OQ-651 and OQ-655 marked partly answered.

Packet: `tasks/ui-tabs-1.md` on that branch.

**Verdict: ACCEPTED.** It lands under the owner's standing rule.

## What was checked

**`balance_rows.cpp`:**
- **The rows:** each row cites its View.dll getter and print, and each statistic its AnnoWelt address
  (`docs/re/ui.md` 5.5).
- **City row 6:** it is `row4 - row5 - row3 - row2 + row1`, with the military bucket counted twice as the
  original does. The header's formula and the code agree.
- **The overview:** it sums the cities' population, shown sales and civil cost only, matching player getter
  indices 0..2.
- **Scaling and arithmetic:** the statistics' shown scale `15 * x / 105` comes from `SimConfig`
  (`upkeep_slices`, `stat_decay_den`), not a constant. Every sum and difference is checked for int64 overflow,
  and an overflow leaves the row empty.
- **Missing values:** a row without a source has no key, so it is never drawn as 0. `balance_text` matches
  `_itoa`, including the int64 minimum.

**The snapshot:**
- `PlayRunner::publish` copies the city accounts and the two scale numbers read-only. Nothing in `sim`
  changes.
- `accounts_live` follows `Rules::city_booking`, so the account rows stay empty under the default rules.

**Placement** is data (`hud.txt` `value`/`page`), and the text offset and cut are open as OQ-692.

**Five new questions,** OQ-691 to 695, each with its default and how to resolve it.

## Evidence

- CI green on `5e6c1ad5` (run 44023629594, head verified).
- The UI lane's focused gate on `5e6c1ad5` after a fresh msvc build: 94 executed, 94 passed, 0 skipped,
  including the required contracts.
- **The owner's viewer test,** `ui_6.mp4`, narrated:
  - the "?" city page shows 0, 0, 15, 0, 0, 0 with the balance -15 in red (2:05);
  - the overview shows 0, 0, 15 with rows 3-6 empty (2:26);
  - the clicked tab is highlighted;
  - the narration names no fault in the rows.

## Landing

Candidate `004a26f4` (tree `7ab5576f`), stacked on `4dfdd9b7`, at `refs/candidates/ui-tabs-b`. Its one
end-of-file conflict in `docs/open-questions.md` kept both sides: OQ-690, then OQ-691 to 695.

## The owner's other points from `ui_6.mp4` (not B)

- **The magnifier zoom range** (UI-MINIMAP-2) and **the status bar per island** (UI-STATUS-1, which replaces
  OQ-622's default): UI-lane packets, each with the owner's go.
- **Entrance arrows:** UI-INFO-2b, queued.
- **Minimap rotation:** VIEW-TURN, render lane.
- **The plank text:** `PLANKEGROSS`, a known gap.
