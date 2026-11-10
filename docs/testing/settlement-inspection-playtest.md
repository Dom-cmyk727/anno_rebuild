# The developer inspection line (W4-SETTLEMENT-1): playtest card

For the owner's personal playtest of W4-SETTLEMENT-1's developer inspection UI
(`task/W4-settlement-1`, consolidated via W4-NEXT-1 part S). **Status: automated completion shown
on this build (W2-W7, V1-V6, `docs/orchestration/REPORTS.md`; W4-NEXT-1 part S's three review
follow-ups are fixed and pushed). Your own playtest is NOT_RUN.**

## What it is

A one-line, read-only summary of whatever building is under the pointer, shown in the window's
title bar while the developer tool is on. It reads straight from the sim's live state (household
residents/stock, production stock/activity, mine deposit, and so on — one line per payload kind,
`play.cpp`'s `DescribePayload`), not a static placeholder. This is a developer/QA surface, not
part of the original's own UI (ADR 0004's screen table does not cover it).

## Launch

```
anno1503.exe --play data\maps\lan5.txt --game-dir <game folder>
```

The developer tool is on by default in `--play` (unless `--ui classic` or a future default-surface
switch changes that — see ADR 0004 R2, still open). No extra flag is needed for the inspection
line itself.

## What to check

1. Hover any built structure (house, workshop, farm, mine, …): the title bar gains a segment
   `— <Kind> <status...>` after the existing dev-tool text, for example `— House residents=5
   stock=12,0,8,...` for a house, or `— Workshop out=3.00t in=2.00t activity=7 carriers_out=0`
   for a production building. An "Inert" building (a type with no ported tick) shows just its
   kind, no status.
2. Hover empty ground, or a tile with no building: the segment disappears at once. This must
   hold **even while paused** (key `Space`, `Viewer::toggle_pause`; the original's own parchment
   pause overlay is ADR 0004 row 10, a separate, still-placeholder screen — this is only the
   existing developer pause) — the fix for finding 3 of the W7 review (W4-NEXT-1 part S) makes
   the line track the live hover tile, not just whatever the sim last published, so a paused
   session moving the pointer off a house must clear the line immediately rather than keep
   showing it from before the hover moved.
3. Hover a building, wait a few turns without moving the pointer: the numbers update live (stock
   accumulating, activity changing) — this is reading the sim's published state each turn, not a
   snapshot taken once at hover time.
4. Move the pointer between two different buildings without an intervening empty-ground hover:
   the line switches to the new building's kind/status on the very next read, never keeps
   showing the previous one.

## Known limits

- Title-bar text only — there is no on-screen panel, border highlight, or original-UI chrome for
  this (that is ADR 0004 row 18, "Selected house / building", separately scoped and still a
  placeholder as of this card).
- Covered by `anno_app_tests` (`inspect_building_test.cpp`, hand-derived expected lines for
  workshop/farm/small farm/mine/inn, plus a no-building negative control) and by the scripted
  window session `tools/test/viewer_wiring/settlement_activity.script` (builds a house through
  the real dev tool, reads its live line, then the paused-hover-clear negative step). Neither
  proves every payload kind's formatting against the original's own numbers — only that the
  engine's own fields are read and formatted consistently; this card is where a person compares
  the numbers against what they expect from playing.
