# UI-STATUS-1: the status bar shows the island under the cursor

**Written 2026-11-17** by the UI lane on the owner's go ("Yes, write it"). Branch `task/UI-STATUS-1`, base
origin/main `09315620`, worktree `E:\anno1503-worktrees\anno1503-UI-STATUS-1`.

Status:
- Stage A is done and accepted by the UI lane (head `f8827b0a`; the section was renumbered to
  `docs/re/ui.md` 5.6 in `52222a7f`, because UI-TABS-1 B adds 5.5).
- The owner answered OQ-703 on 2026-11-17: "Like original" on all three points.
- Stage B is released 2026-11-17. Code can start now. Builds and tests wait until the orchestrator says
  the PC is free.

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

## Stage B: draw the status bar for the island under the cursor (2-4 h, code; released 2026-11-17)

The rule is `docs/re/ui.md` 5.6, with the owner's OQ-703 answer: the original on all three points. The
summary below is for orientation only; 5.6 is authoritative.
- **The point:** the map tile under the cursor, re-read every frame. What the bar shows while the mouse is
  over the panel or the bar is OQ-700: keep the last map tile, which is what the HE frames suggest.
- **Gold:** always.
- **The four materials and the residents:** only while that tile is inside the territory of a city of
  the local player. They are that city's stock / 256 (wares WERKZEUG, HOLZ, ZIEGEL, MAMOR) and its
  residents. With two own cities on one island, it is the city whose territory holds the tile.
- **A foreign city's territory:** gold, plus its residents when its `City::player_mask` has the local
  player. Never its materials.
- **Island land outside every territory, or sea:** gold only. When a building is armed, its four corner
  tiles pick the first own city among them (5.6).
- **The fertility row:** over every island the local player has discovered (`Island::players_present`
  has the local player), with or without a city. The segments come from the island's climate mask (5.6
  table: NORD 6, 8, 11 and so on, at most 4, x = 547 + 40k). Over open sea the row hides at the next
  frame; over sea inside the island's chunks it hides after 2 s; returning to land within 2 s cancels the
  hide.
- **The green bars (`STATUSBAR_BALKEN`):** the sim has no source for island property 11 (OQ-702), and
  `Terrain::fertility` is a per-tile stand-in, not property 11. So draw **no bars**: the icons without
  bars are a named placeholder (`hud.txt` comment plus OQ-702). Do not invent 50 or 100.
- The "Always show my city" fallback (R4) and the view-centre point (`centre_tile()` for the status bar)
  go. Remove them, do not keep them behind an option; there is no `rules` option (owner).

Steps:
1. **Snapshot** (`src/app/src/play.cpp`; read-only accessors, no sim change, no new sim field):
   - for the tile under the cursor, the island and the city whose territory holds it (find the
     rebuild's territory or tile-owner accessor; the minimap's `refresh_territory()` already reads
     owners);
   - per own city, its goods and population, as today;
   - per foreign city, its population and whether its `player_mask` has the local player;
   - per discovered island, its climate (find the sim's field; `SimConfig::raw_climate_mask` is the
     mask table) or the segment list.

   The window thread sends the cursor tile; the sim thread answers in the next snapshot (the pattern the
   `inspection` request uses). Measure the cost with `ANNO_PUBLISH_STATS`.
2. **Viewer** (`src/app/src/viewer.cpp`):
   - resolve the cursor's map tile (the hover pick the building ghost uses) and keep the last map tile
     while the mouse is over chrome;
   - choose the groups by the rule above;
   - turn the static `rosh_46/48/51` elements in `data/ui/hud.txt` into four data-driven slots: x from
     data, segment from the island;
   - the 2 s hide timer uses the window's clock: UI only, never sim state.
   - Both the CPU and GPU UI paths.
3. **Facts:**
   - `hud.status.tile` (x,y or `none`);
   - `hud.status.island` (index, or `sea`);
   - `hud.status.city` (island:city, or `none`);
   - `hud.counters` (`materials+residents`, `residents`, `gold`);
   - `hud.fertility` (the shown segments, or `hidden`).

   Keep `hud.counters.city` working, or rename it and update every script that reads it.
4. **Tests:**
   - Unit tests for the choice function (pure, no viewer): own territory, own island outside the
     territory, foreign city known and unknown, sea, sea within the chunks (timer), undiscovered
     island, two own cities, armed-building corners, every climate's segment list.
   - A wiring script `tools/test/viewer_wiring/ui_status_island.script` on lan5: hover own territory,
     own island outside it, a foreign island, the sea and an undiscovered island if lan5 has one (else
     say so and cover it in the unit test); read the facts.
   - Pixel checks of the bar against the GOG shots that show it (`hud_idle_running`: gold plus the NORD
     row; `r2_construction_public`, `f13_hud_cursor_map`: the counters), with live digits masked as
     today. The bars are absent by design; mask their 5 × 29 rectangles with a reason. Check the
     residents icon's x (code 400, `hud.txt` 401) and record the answer.
   - Existing tests that assumed the centre rule or "Always show my city" (find them with `git grep
     counters`) are updated, each named in the report.
   - Failure proof: the wiring case fails on the old centre-based code.
5. **The owner's test** (viewer, `--rules living`): hover his territory, his island outside the
   territory, a foreign island, the sea, and an unexplored island if there is one. Watch the fertility
   row hide 2 s after leaving land over the island's sea.

## Stage C: when the island's figures show, and what the bars mean (2-3 h, docs only; released 2026-11-17)

The owner tested B in `ui_7.mp4` (2026-11-17; report and transcript in `E:\1503_test_videos\ui_7_work\`)
and gave a direct task (2:54-5:52): "please double check that with the original and History Edition game
files", "investigate both", "examine that thoroughly". Answer each question with GOG code (module and
address) or HE frames, or open an OQ:

1. **When does an island's resource availability (the fertility row) appear?**
   - Find every writer of island word `+0xa6` (property `0xe`) and every caller of the reveal (island
     slot `+0x130` = AnnoWelt `0x10080c10`; world slot `+0xcc`); this is OQ-701.
   - Does a ship near the island reveal it? Which ships (own only? any?), what distance (tiles, chunks,
     the ship's view range from its type data?), and is it checked every tick?
   - Do the scenario's `DISCOVER` records, the start position, a Kontor, or a click on the island also
     reveal it?
   - The owner remembers 1602's "Erkunde die Insel" (explore the island). Check whether 1503 has such a
     command or text (`Texte.dat`, the ship panel's orders).
   - What does a click on an undiscovered island show (status bar, panel)?
2. **What the bars mean (OQ-702):**
   - Find who reads island property 11 (`+0xac`) besides the status bar, above all the farms' growth or
     planting (production code, `production.md` §4 "which code reads fertility" is open).
   - Is it two levels (50 / 100, as the owner remembers from 1602), and what does 50 do to a farm's
     rate?
   - Where does property 11 come from? Map placement sets `0xF009 | raw marks` (`annoscene.md` §5 step
     10). Also check save files and the scenario `ROHSTOFF` records. Is there a 0 % case?
   - Where can the rebuild read the raw marks today (`scenario_formats.hpp` `LinkRawMaterials` holds
     them: "the sim has no field for it yet")?
3. **Own resources and citizens:** re-confirm 5.6's territory rule against the owner's words ("you have
   to settle on an island to have your own resources there") and list every case: before founding,
   own territory, own island outside it, foreign city, and while a building is armed.
4. **HE recipe for the owner** for each point that needs the running game: for example, sail a ship
   towards an island never visited and note when the row appears; then compare a 50 % and a 100 %
   island. Sound stays at 0 %. The owner plays HE himself.

Write the result into `docs/re/ui.md` 5.6 (and `production.md` if a farm rule is found), update OQ-700..702
with new OQs from OQ-704 on, and add a REPORTS entry with an implementation plan for stage D. Stage D
implements, in the smallest safe pieces:
- the reveal;
- an island raw-material mask field, read from the files, for the bars;
- the bars themselves;
- any farm-efficiency rule.

Sim parts follow the orchestrator's lane rules.

## Acceptance

- A: every behaviour in the table has a GOG address or an HE frame, or is an OQ. The owner's statement is
  compared row by row. No code changes.
- B: the focused UI gate passes with every oracle-shot test executed (`live_screen_matches_oracle`,
  `ui_dynamic_draw_check`, `info_panel_gog_compare`, `selected_house_pixel_check`, `ui_panels_match_gog`,
  plus the new tests), 0 skipped. CI is green. The owner's viewer test passes.
- Evidence contract: `docs/testing/evidence-contract.md`. No original pixels or text in git.
