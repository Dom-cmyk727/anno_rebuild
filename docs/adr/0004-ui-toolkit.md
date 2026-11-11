# ADR 0004: The UI toolkit

- **Status:** Proposed
- **Date:** 2026-11-07

## Decision

UI.2 (the original's HUD and menus, placeholders where the sim has none) draws with **our own
small widget set, drawn by the GPU compositor, using the original's fonts and `ToolGfx` textures**
(owner decision D4, `docs/plan/lan-mvp.md` §4 W5). No third-party UI library, no immediate-mode GUI:
the widget tree is a thin, data-driven layer over the existing render/app code, not a new
subsystem with its own event loop or its own GPU pipeline.

### Why this shape, not a general-purpose toolkit

The original's screens are not freely laid out: every element's position, size and source texture
is a measured fact (`data/ui/`, `docs/re/ui.md`), and UI.1.F2's own gates (`tools/ui_layout.py
mock`/`compare`) already check a rendered frame against those facts pixel by pixel. A general
widget toolkit (retained-mode or immediate-mode) would add a layout engine UI.2 does not need —
every rectangle is already known — and would duplicate the GPU atlas/sprite-draw path the
compositor already has for exactly this job (`docs/render/gpu-chunks.md` §4, the existing
`Atlas`/`atlas_alloc` RGBA8Unorm array texture and the straight-alpha sprite-instance draw,
`gpu.cpp`). Reusing that path keeps one atlas, one upload budget and one CPU-reference-vs-GPU
fidelity contract for both the world and the chrome, instead of two.

## Widgets

A widget is a **data record, not a C++ class hierarchy**: `data/ui/hud.txt` and
`data/ui/construction.txt` already describe every element (`element <name> pos X Y size W H file
<tex> access <ACCESS> tex_id <id> method <sprite-sad|inc-size|atlas-right|view-dll>`) at the
**1024 × 768 reference**. UI.2 reads the same files at runtime (through the existing
`tools/tex.py`-equivalent C++ decoder already used by `anno_dump`/the asset pipeline) and turns
each `element` into a draw call: a rectangle, a decoded texture id and, for the eight construction
categories and the 148 cells, a small per-instance index (category row, cell column/row) computed
from `construction.txt`'s own grid/category records, never a hardcoded id in `src/`.

**No coordinate lives in code.** A screen is a flat list of elements plus the handful of
*generated* ones `data/ui/` already documents as derived, not stored (the category column's eight
cells from `construction.txt`'s grid; the 148 unlocked-cell icons from the same grid plus
`cell_icon_tex_id`'s `THUMBNAIL* + 3×record_index` formula, §3.3 of `docs/re/ui.md`). Adding a
screen or an element is a `data/ui/` edit plus, where the source is not yet an existing `.tex`
decode, a new `access`/`tex_id` row — never a new C++ type.

**Anchoring** follows `docs/re/ui.md` §5's identities at the 1024 × 768 reference (right chrome
`x = W − 245`; status bar `y = H − 35`; construction grid origin `(W − 216, 305)`, stride 52;
tabs `y = 181`, stride 61 from `x = W − 245`) and the scale factor from R3 below. These are
stated as identities *at 1024 × 768*, not yet verified at other window sizes (`docs/re/ui.md` §5's
own caveat); H3 and F2.3's owner checklist close that gap.

## Input routing

SDL3 events go to the UI first, then to the world: the viewer's existing `SDL_PollEvent` loop
(`viewer.cpp`) gains a UI-dispatch call before `dev_tool_step`/the world's own key handling. A UI
widget that consumes an event (a click inside its rectangle, a key its screen binds) stops it
there; everything else falls through unchanged to the existing camera/dev-tool/world input exactly
as today. **The developer tool keeps its own key** (unchanged, per the dispatch's own rule 0.7):
it is not a UI.2 screen and this ADR does not route its toggle through the new widget dispatch.

## Text

Rendered with the original's `.zei` bitmap fonts (already parsed, `anno::formats`), from
Windows-1252 strings loaded from `Texte.dat` at runtime (never a string literal in `src/`, per
CLAUDE.md's format-spec rule and the dispatch's own "no original... UI strings... in git"). A
screen names its `Texte.dat` section/line in `data/ui/`, exactly as `construction.txt` already
names `tooltip_section` per category; the glyphs are decoded and blitted the same way an element's
texture is, through the same atlas.

## Atlases

The `ToolGfx`/`fenster`/`statusbar`/`B_*` textures UI.2 draws from are uploaded **once**, into the
compositor's existing sprite atlas (`docs/render/gpu-chunks.md` §4), not a second, UI-only atlas:
one atlas, one eviction policy (none, for chrome — UI frames are static and few compared to the
world's sprite set, so they sit in the atlas's existing "static" allocation class and are never
candidates for the "hot" class's LRU). **No texture is uploaded per frame**: a UI element's frame
id is requested once per screen-open (or once ever, for the always-on HUD), exactly like a chunk's
first-use upload; V6 measures this directly (no upload counted across idle frames).

## The command boundary

The UI is a **consumer**: it reads the published, read-only snapshot (`OpeningView`/`PlaySnapshot`,
already how the developer tool and `DevScript` read state) and sends commands through the existing
command path (`BuildCmd`, `DemolishCmd`, and whichever of `LoadCmd`/`UnloadCmd`/`FoundCmd` a screen
exposes) — the same `PlayRunner::post_command` entry every other consumer uses. **It never mutates
sim state directly**: no widget holds a reference into `World`, and V3 is a test that opening,
browsing and closing every menu without issuing a command leaves the state hash unchanged.

## Rendering

**A GPU path and a CPU reference path, compared pixel by pixel** (CLAUDE.md's own rule). This ADR
first planned the GPU path as a reuse of the existing straight-alpha sprite instance draw
(`gpu.wgsl`'s `sprite_vs`/`sprite_fs`) — wrong once implemented (W4-UI-2, 2026-11-08): that path is
structurally tied to `render::SpriteDraw`'s world-tile-projected sprite records, which cannot carry
`.tex`/`.zei` UI content or fixed screen-space positions. The GPU path actually implemented
composes the same live state into a transparent RGBA canvas on the CPU
(`anno::app::draw_live_screen_rgba`, `ui_mock.hpp`/`.cpp`) and blends it over the already-rendered
frame with a full-screen pass that reuses the existing resample/zoom post-process's vertex stage
(`gpu.wgsl`'s `scale_vs`, new `ui_overlay_fs`). `draw_live_screen_rgba` is a separate function from
the CPU path's own `draw_live_screen_xrgb` — not shared code — but over the same data/ui/ layouts,
live eligibility/counters and digit-rectangle lookups; the two are not independent implementations
of those details, so the real safety net against them drifting apart is each path's own pixel
comparison against the UI.1 oracle (`ui_mock_matches_oracle`, `live_screen_matches_oracle
--renderer gpu`), not independence. The CPU path itself decodes and blits the same elements in
plain C++ (closely related to, but not the same code as, `tools/ui_layout.py`'s own Python
`compose_screen`/`blit_rgba`, which is the **oracle**, not the shipped CPU path — the shipped CPU
renderer is new C++ in `src/render`/`src/app`, checked *against* the oracle, same as every other
CPU-vs-GPU-vs-oracle triple this project already keeps). Exact match is the target on the CPU path;
the GPU path is checked within this project's own established F2.1 tolerance (channel ≤ 48,
fraction ≤ 0.08) rather than exact match, since the full-screen blend pass's own float sampling
(not present in the CPU path at all) is a real, if tiny, source of per-pixel difference — measured
at 28 of ~200,000+ checked pixels (fraction ~0.0001–0.0002), all at sprite-corner pixels, none in
digit/grid content.

## Testing

**The oracle pixel tests, with their masks and negative controls** — already built in UI.1/UI.1.F2
(`tools/ui_layout.py mock`/`compare`, channel 48, fraction 0.08, `data/ui/masks.txt`) — are reused
as the acceptance gate for the *shipped* renderer, not just the Python oracle: H4/C4/V4 run
`compare` against the shipped app's own screenshot, not only against the oracle's own mock, so a
regression in the real renderer is caught the same way a wrong `data/ui/` fact would be. V5's
mutation checks (a wrong cell type, an accepted click on a locked cell, a click that sends no
command, a wrong displayed cost) must each fail a test, per the dispatch.

## Screen tree: bound vs. placeholder (W4-UI-2 M)

Every screen `docs/re/ui.md` section 1's "In-game" table names (19, from a running Citizen-start
endless game), checked against what `--construction-menu on` actually wires up today, not
guessed: "bound" means a real click/key reaches real engine state and draws something live;
"placeholder" means the screen has no handler and no chrome at all yet; "partial" sits between
the two. The "waits on" column separates a true sim-side gap (a feature or aggregate the engine
does not compute yet) from a UI-side one (input routing and chrome this project has not built
yet, with the sim data already available) - conflating the two would misdirect whoever picks up
the next slice.

| # | Screen | Status | What's live now | Waits on |
|---|---|---|---|---|
| 1 | HUD idle | **Bound** | Chrome (`draw_live_screen_xrgb`) and the gold counter (H2), live, every frame | - |
| 2 | Settlement / Kontor panel | Placeholder | Nothing (no click target for "Kontor" exists in `construction_handle` or anywhere else) | UI: a click-routing seam for a non-menu panel. Sim: trade sliders would read `City::trade_lists` (already modelled) once a panel exists |
| 3 | Construction menu, 8 categories | **Bound** | `ConstructionMenu` + `draw_live_screen_xrgb`, all 8 categories (the draw path does not special-case which one); 2 of 8 screenshot-proven pixel-exact against the oracle (public, roads) | - |
| 4 | Building info / placement | Partial | A grid click resolves through `ConstructionMenu::click` to `Arm`/`DevTool::arm` (selects the type; the actual `BuildCmd` is the existing dev-tool path's own next click, on the world tile); `cell_info` already computes cost live from the registry (C3's own data path). No panel chrome, no cost/upkeep numbers are drawn - the panel itself has no screen rectangle in the live window yet | UI: the panel's own drawing slice (C1/C6, independent of fonts - fonts only draw numbers once there is a panel to draw them on). Sim: OQ-563 (an unbuilt type's upkeep has no live source) |
| 5 | Demolish | Partial | The pickaxe/cancel-X click resolves `ToggleDemolish` through `ConstructionMenu::click` and a real `DemolishCmd`-path `ToolInput` | UI: a distinct demolish-mode chrome/cursor (none drawn; today it is state with no visual) |
| 6 | Help / info mode | Placeholder | Nothing | UI only: a tab click handler and its own chrome; no sim gap is known |
| 7 | Military tab (= key D's panel) | Placeholder | Nothing | UI only (same panel as 9, same D collision) |
| 8 | Options tab | Placeholder | Nothing | UI only; client-side settings, no sim data needed |
| 9 | Player list (key D) | Placeholder | Nothing - **another real key collision**: D is already bound to pan right | UI: same collision pattern as 15/17 |
| 10 | Pause overlay (key P) | Placeholder | Nothing (the viewer's own developer controls do not bind P, so the key is free; the original's parchment-over-map chrome is not drawn) | UI only |
| 11-14 | Options load/save/music/speech | Placeholder | Nothing | UI only; load/save touch the existing save/load path, not a new sim feature |
| 15 | Ship list (key S) | Placeholder | Nothing - **a real key collision**: the viewer's own developer controls already bind S to pan | UI: needs its own key or a scheme that resolves the collision (mirrors the B/Esc precedent H1 already set for the construction menu) |
| 16 | City list (key C) | Placeholder | Nothing | UI only |
| 17 | Rating (key Q, full frame) | Placeholder | Nothing - **a real key collision**: the viewer's own developer controls already bind Q to quit | UI: same collision pattern as 15. Sim: whatever statistics the rating screen shows would need their own aggregation, not yet checked for existing equivalents |
| 18 | Selected house | Partial | A left click selects a built building of the player's city and Escape deselects ("Selection" below); the selection shows in the title only, the panel is not drawn | UI: the panel's measurement (tab 975, need icons, close button) and drawing |
| 19 | Deselected right panel | Placeholder | Nothing | UI only; an empty-panel default, no sim feature needed |

Pre-game menus (main menu, singleplayer setup, `tools/orig/menus.json`) are out of this table:
`docs/re/ui.md` section 1 itself marks them "not the owner in-game test surface", and `--play`
starts past them - this ADR's whole scope is the in-game UI reachable from a running session.

## Selection: clicking a built building (W4-NEXT-2 K, row 18)

**Where it lives.** Viewer UI state (`app::SelectionState`, `selection.hpp`), never sim state: a
building id and the clicked map tile, never a screen position, so pan and zoom leave it as it
is. Selecting posts no command; a test checks the state hash is unchanged
(`inspect_building_test.cpp`, "selecting and deselecting writes no sim state", with a negative
control that posts a demolish and is seen).

**How a click resolves.** The click asks the runner to inspect the tile under the pointer
(`PlayRunner::request_inspect`, the developer inspection's own slot); the next published snapshot
answers which of the player's buildings stands there (`PlaySnapshot::inspection`, and the new
`PlaySnapshot::inspected`, the tile it answers even when nothing stands there). The answer selects
that building, or deselects on empty ground (also when the player has no city yet: the tile is
answered as empty). Every later answer for the selected tile re-checks the building's id, so a
building demolished or replaced meanwhile drops the selection. A paused runner answers a new
request by republishing (`answer_inspection_while_paused`), because a pause publishes no turn.
While the developer tool is on, its hover keeps the inspection slot, and a click still waiting
for its answer is dropped. One change for the developer tool follows: a hover while paused is now
answered by that republish, where before the old answer stayed (and `live_inspection` hid it).

**What a left click means** (`selection_step`, unit-tested in `selection_test.cpp`):

| Mode | Left click | Escape |
|---|---|---|
| Construction tool armed (Build) | the tool's (place); the selection stays | the menu's / the tool's, as today |
| Demolish mode | the tool's (demolish); a demolished selected building drops the selection at the first answer after the tool is off again (while it is on, the hover owns the answers) | as today |
| Developer tool on | the tool's | as today |
| Otherwise (Select) | a click (press and release within 4 drawable pixels) selects the building under it, or deselects on empty ground or off the map; a drag still pans | with a selection or a pending click: deselect (consumed); without one: today's behaviour (quit) |

Only with `--play --ui classic` in a session started **without** the ship-start opening (an
existing-town scenario): a ship-start session has no selection at all yet, also after a city is
founded, because its left clicks belong to the opening's ship selection for the whole session
(QUESTION K-5). `--ui dev` and `--view` are unchanged. The construction menu keeps Escape while it
is open (it sees the key first); a held Escape's auto-repeat after a deselect does not quit.

**Evidence.** The original's own capture recipe for `f15_selected_house` (`docs/re/ui.md` §7 and §9:
"place public (0,0) type 501, ESC, click the house") shows a left click on a built house selects
it after ESC left the building tool. Nothing else about the original's selection rules
(deselect, Escape, a drag, foreign buildings) has been captured; the rows below are QUESTIONs.

**Known limitation: an inspection answer is not tied to its request (F4, finding 7 of the
2026-11-11 review; corrected by G4, finding 6 of the 2026-11-11 fix review).** `PlaySnapshot::
inspected`/`inspection` echo only the tile that was asked about, never a request number;
`selection_refresh`'s own `Answer` event matches an incoming answer to `state.pending` by tile
alone. Two requests for the *same* tile in quick succession (a click, then - before that answer is
in - a second click elsewhere and back) can have the second request's answer resolve the first's
`pending`, selecting whatever the first request's tile holds even though the player's pointer and
intent have since moved on. The fix: a request number the snapshot echoes back -
`PlayRunner::request_inspect` (`play.hpp`) does not hand one out today; it only stores the tile
under a mutex (`inspect_tile_`), with no counter or identity of any kind. The fix would need to add
one (incremented per call, published in `PlaySnapshot`), and `SelectionEvent::Answer`/
`selection_step` would check it against the request the current `pending` was made with, not just
the tile. Not implemented here: F1's race (facts built from a stale frame count, not from an
untagged answer) did not need it, and no capture or test in this packet exercises two in-flight
requests for one tile.

**QUESTION K-1: what Escape does with a selection.** (a) Deselect, and quit only without a
selection - **recommended and implemented**: Escape stays the way out of every mode, one press at
a time. (b) Keep Escape as quit always, deselect by clicking empty ground only. No capture of the
original's Escape with a selection exists (W4-NEXT-2 W could take one).

**QUESTION K-2: click or drag.** The left button already pans by dragging. (a) A press and
release within 4 drawable pixels is a click (selects); farther is a drag (pans) - **recommended
and implemented** (`Viewer::kClickSlop`). (b) Pan with the right button only and give the left
button to selection alone (changes how the map feels today).

**QUESTION K-3: a click on empty ground or water.** (a) Deselect - **recommended and
implemented**. (b) Keep the selection until Escape or another building is clicked.

**QUESTION K-4: other players' buildings.** The inspection answers only the player's own city
(`inspect_building`), so a click on a foreign building deselects today. (a) Keep that until a
capture shows what the original's panel shows for a foreign building - **recommended**. (b) Select
foreign buildings too (needs a read-only query for another city).

**QUESTION K-5: selection in a ship-start session.** (a) Once the player has a city, a click
that hits no ship selects a building - **recommended**, not implemented: it needs the opening's
click routing to hand unclaimed clicks on, which this run could not test without the game files.
(b) Keep ship-start sessions without selection until the opening's UI is redesigned.

**Done, continued on the Windows PC (W4-NEXT-2 K, steps 2-4).** The panel now draws on both the CPU
and GPU paths, gated on a selected House (`draw_live_screen_xrgb`/`_rgba`'s `selected_house`
parameter, `ui_mock.cpp`), from three measured `data/ui/hud.txt` elements against
`f15_selected_house`: `pionier_bk` (id 974), `house_portrait`, and `need_icon_0` (id 727, the
first of the panel's need icons). The tab (`VIEWINFOPIONIER_TAB` id 975), the panel's other need
icons and its close button are still unmeasured (OQ-569, OQ-570): an element this packet cannot
measure is not drawn, per step 2's own rule. CPU/GPU parity (0 of 78,400 pixels differ), the UI.1
oracle (unchanged, 9 of 9 plus 3 of 3 live screens) and the pixel-level checks below (`selected_
house_pixel_check.py`) are all executed and passing; `bench_frame` before/after is in that part's
own REPORTS entry.

## QUESTION R2: the default surface in `--play`

- **(a) The original UI on, with the developer tool on its key — recommended.** It is the owner's
  chosen test surface (M3 decision, 2026-10-30, `docs/plan/lan-mvp.md` §1); every future
  build/demolish playtest should exercise the real path by default, not a developer-only one.
- **(b) The developer tool, as today, with the original UI on a switch.** Lower risk while UI.2 is
  new (a bug in the UI never blocks a playtest that only needs the dev tool), at the cost of the
  owner having to remember to switch for the tests that matter.

Until answered: **(a) implemented behind `--ui classic|dev` (default `classic`, i.e. the
recommendation)**, so the owner can get (b)'s behaviour with one flag without a second build. Noted
on the playtest card (K).

## QUESTION R3: the UI scale at 1440p and 4K

The original draws at 1024 × 768; a modern display is commonly 2560 × 1440 or 3840 × 2160.

- **Integer factors 2 / 3.** Sharp (nearest-neighbour on crisp original pixel art), but 2× of
  1024 × 768 is 2048 × 1536 (black bars on a 2560 × 1440 or wider display) and 3× is
  3072 × 2304 (same on 4K) — the chrome does not fill the screen's native resolution.
- **A fractional factor** (e.g. 2560/1024 = 2.5, 3840/1024 = 3.75). Fills the screen exactly, at
  the cost of resampling the original's pixel art (soft edges on the `.tex` sprites and the `.zei`
  glyphs) — the same trade-off `docs/re/ui.md` §5 already flags for the five in-game resolutions
  the original itself offers, just continuous instead of five discrete choices.
- **A player setting with a default.** Either of the above, exposed as a option instead of fixed;
  more implementation surface (a settings screen, a saved preference) for a decision neither of
  the two options above removes.

No screenshots at 2560×1440/3840×2160 exist yet to compare sharpness directly (this ADR is written
before H renders a frame); H3 will attach them once the HUD exists. **Recommendation, provisional
on those screenshots: the fractional factor**, so the chrome always fills the window at the cost of
some softness on text/sprite edges — consistent with how most modern games scale a fixed-reference
UI, and avoidable later behind a player setting (the third option) if the softness proves
objectionable. Implemented behind a switch (R1's "scale factor from R3" is read from one
configuration value, not hardcoded), so changing the default later is a one-line change, not a
redesign.

## What this does not decide

- **The construction menu's lock authority** (the sim's `construction_eligibility`, unchanged by
  this ADR — C2).
- **Any new sim behaviour.** This ADR adds no `src/sim` *behaviour* change; a UI.2 screen that
  needs one stops and asks, per the dispatch's rule 0.2. It does add one new read-only sim query
  (`World::city_population`, wrapping the existing `residents_at_or_above` the stage pass already
  uses, for the HUD's residents counter) — explicitly allowed by rule 0.2 itself ("a new read-only
  sim query is allowed if it changes no state, stream, hash or draw"), not an exception to this
  bullet's own rule against new behaviour.
