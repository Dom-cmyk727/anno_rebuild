# UI-STATUS-1: the status bar shows the island under the cursor

**Written 2026-11-17** by the UI lane on the owner's go ("Yes, write it"). Branch `task/UI-STATUS-1`, base
origin/main `09315620`, worktree `E:\anno1503-worktrees\anno1503-UI-STATUS-1`. Stage A is released with
this packet. Stage B waits for the UI lane's review of A; if A's code read disagrees with the owner's rule
below, it also waits for the owner's choice.

## Why

In his viewer test `ui_6.mp4` (2026-11-17, 3:04-4:13; transcript and report in
`E:\1503_test_videos\ui_6_work\`), the owner stated the original's rule first-hand and asked for it ("Make
sure to implement this"):

- Hover over an island where you have your Kontor and settlement: the status bar shows that island's
  goods, its citizens and the island's resource availability.
- Hover over another island: only the gold shows.
- An island you have not explored shows none of this.

Today the rebuild does something else. UI-WINDOW-1 fix round R4 (2026-11-14) shows the city on the island
under the **view centre**, else the local player's **first** city ("Always show my city", the owner's
default then, OQ-622). The owner now replaces that default. See `src/app/src/play.cpp` (the `cities` list
published for the status bar) and `src/app/src/viewer.cpp` (`centre_tile()`, `counters_city`, the facts
`hud.counters` and `hud.counters.city`).

The History Edition agrees on the cursor. The 2026-11-14 HE video `HE_UI_1.mp4` was analysed in
`E:\anno1503-worktrees\he-ui-captures\he_ui_1_video\HE_UI_1-analysis.md` §(c) "Bottom status bar", with
the visibility timeline at 0.25 s steps:
- The materials and residents counters show only over an island with the player's Kontor.
- Three fertility icons, each with a green bar, show over islands, **including one without a Kontor**
  before the founding (00:52-01:41).
- Only the gold shows over the sea.
- The counters hide exactly when the cursor leaves the island (02:04.5-02:05.5).

This differs from the owner's "another island: only the gold" for the fertility icons. Stage A settles it
from the code.

On the GOG shots, the three `STATUSBAR_ROSHTOFFE` icons sit on `hud_idle_running`, before any founding.
`data/ui/hud.txt` lines 16-18 draw them today as **static** elements (segments 6, 8 and 11), the same three
on every island.

Owner rules:
- Code is read in the GOG DLLs; the History Edition is the picture reference (pixels only).
- The original UI, bit-identical, is the base.
- Small steps; the owner tests each.
- Evidence ranks: files and disassembly first, then the owner, then community sources.
- Where the code and the owner's statement disagree, the owner picks the default (a feel matter).

## What is NOT in this packet

- The **cost line**: the second row of small numbers under gold and materials while a building is armed,
  with blue meaning not affordable (HE analysis §(c)). This becomes UI-STATUS-2. Stage A only notes where
  that code is.
- Tooltips over the status bar (`TOOLTIP:STATUSBAR`, OQ-541): they belong with the tooltip packet.
- New sim rules. If a source is missing (fertility per island, discovery per player), the snapshot does
  not invent one: B raises a QUESTION and draws nothing for that group.

## Stage A: read the original's rule (2-3 h, docs only)

1. **The GOG code.**
   - Find the status bar's update and draw: AnnoFrame.dll around `0x10063000..0x10064850` (OQ-622's
     pointer), `CreateAnnoStatusbarInstance`, `tEventStatusbar`, `iAnnoDrawEventStatusbar`
     (`docs/re/ui.md` §1 symbol list). Note module + address per finding.
   - **Which point counts:** the tile under the cursor, the last map tile the cursor was over (when it is
     over the panel or the bar), or the view centre. When is it updated (every frame, or on mouse move)?
   - **Which island and city:** what is shown over the sea, over an island with the local player's city,
     over an island with only another player's city, over an island with no city, and with two cities
     of the local player on one island.
   - **The groups:** gold (always?); the four `STATUSBAR_BAUMAT` counters and `STATUSBAR_EINWOHNER`
     (which stock and which population); the `STATUSBAR_ROSHTOFFE` slots.
     - For ROSHTOFFE: which of the 12 segments, how many at most, their order, and where the x positions
       come from (`hud.txt` measures 547/587/627).
     - What `STATUSBAR_BALKEN` (the green bar) shows, and its length rule.
   - **The exploration condition:** which flag the code tests before showing an island's figures.
     Candidates in the rebuild are the island's `discovered` mask (`src/sim/include/anno/sim/island.hpp`,
     around line 416) and the player's discovered owners (`player.hpp` around line 128). Confirm which one
     the original reads, by address.
2. **The History Edition.** Re-check the 2026-11-14 HE timeline against the code read: the hide at
   02:04.5, the 20 s gap at 01:43.5-02:03.25 that is still unexplained, and the fertility icons before the
   founding. Crops are in `he_ui_1_video\` (`c_sb_stack.png`, `crops\statusbar_*`). Give a frame and a
   confidence per point. If a point needs a new HE capture, write a short recipe for the owner (the owner
   plays HE himself; sound stays at 0 %). Do not start HE yourself.
3. **Write it down.**
   - Add a new section "The status bar's island" to `docs/re/ui.md`, with the addresses and one evidence
     row per behaviour.
   - Update OQ-622: RESOLVED or PARTLY ANSWERED, with what is still open.
   - New questions get new OQ numbers. Check `docs/open-questions.md` on main and the open branches
     first: UI-TABS-1 holds up to OQ-695, so start at OQ-700.
   - Put the owner's 2026-11-17 statement next to the code's rule in a table. Mark every difference
     "owner decision needed", above all the fertility icons over an island without the player's city.
4. **Report** in `docs/orchestration/REPORTS.md` with a "Decisions and reasoning" list, and stop. B starts
   on the UI lane's go.

## Stage B: draw the status bar for the island under the cursor (2-4 h, code; after A's review)

Written in full when A is reviewed. The plan from today's knowledge, which A may change:

1. **Snapshot** (`play.cpp`, read-only accessors, no sim change):
   - Per island the local player has discovered (by A's flag): its rectangle, its fertility slots for the
     status bar (A's mapping from `Island::fertility`) and the bar values.
   - The local player's cities with goods and population, as today.
   - No per-frame allocation beyond today's `cities` list. Measure with `ANNO_PUBLISH_STATS`.
2. **Viewer:**
   - Resolve the point A names (the cursor tile, or the last map tile) to an island.
   - Show the groups by A's rule: gold always; BAUMAT + EINWOHNER for the local player's city on that
     island; ROSHTOFFE + BALKEN as A decides and the owner confirms.
   - The static `rosh_46/48/51` elements in `hud.txt` become data-driven slots: positions stay data, and
     the segment comes from the island.
   - The "Always show my city" fallback goes, by the owner's 2026-11-17 decision. Add no rules option
     unless A or the owner asks for one.
3. **Facts:**
   - `hud.status.island` (index, or `sea`, or `undiscovered`).
   - `hud.counters` and `hud.counters.city`, as today.
   - `hud.fertility` (the shown segments).
4. **Tests:**
   - Unit tests for the island choice: sea, own city, foreign city, no city, undiscovered, two own
     cities if A says how.
   - A wiring script `tools/test/viewer_wiring/ui_status_island.script`: hover an own island, a foreign
     one, the sea and an undiscovered one; read the facts.
   - A pixel check of the bar against the GOG shots that show it (`r2_construction_public`,
     `hud_idle_running`, `f13_hud_cursor_map`), with live digits masked as today.
   - A failure proof: the wiring case fails on today's centre-based code.
5. **The owner's test:** in the viewer, hover over his own island, a foreign island, the sea and an
   unexplored island. Side by side with HE where A recorded a capture.

## Acceptance

- A: every behaviour in the table has a GOG address or an HE frame, or is an OQ. The owner's statement is
  compared row by row. No code changes.
- B: the focused UI gate passes with every oracle-shot test executed (`live_screen_matches_oracle`,
  `ui_dynamic_draw_check`, `info_panel_gog_compare`, `selected_house_pixel_check`, `ui_panels_match_gog`,
  plus the new tests), 0 skipped. CI is green. The owner's viewer test passes.
- Evidence contract: `docs/testing/evidence-contract.md`. No original pixels or text in git.
