# The construction menu (UI.2): playtest card

For the owner's personal playtest of W4-UI-2's construction menu, HUD counters and building
info/placement panel (`task/W4-ui-2`). **Status: automated completion shown on this build (V1-V6,
`docs/orchestration/REPORTS.md`). Your own playtest is NOT_RUN.**

## What it is

The in-game construction menu (ADR 0004, `docs/adr/0004-ui-toolkit.md`), the first slice of
UI.2's "every menu, placeholders where the sim has no feature yet": the HUD bar (gold/goods/
residents counters), the 8-category construction menu, placement and demolish through it, and
the building info/placement panel's chrome. It replaces the bare developer picker as the
construction surface in `--play`.

## Launch

```
anno1503.exe --play data\maps\lan5.txt --construction-menu on --layout data\ui --renderer cpu --game-dir <game folder>
```

`--renderer cpu` is not optional for this playtest: the menu/HUD overlay only draws on the CPU
path today (see "Known limits"), and `--renderer`'s own default is `auto`, which picks the GPU on
this machine - launch without it and the menu would appear to not exist at all.
`--construction-menu on` requires `--play`; `--layout` defaults to `data/ui` and rarely needs
setting. The window starts at generous developer test resources (10,000,000 gold, 190 tons each
of tools/wood/bricks/marble) unless overridden (`ANNO_DEV_GOLD`/`ANNO_DEV_GOODS_TONS` env vars -
testing-only knobs, not a normal launch option).

## Controls

| Do | Effect |
|---|---|
| `B` | opens the menu on the "public" category (only while it is not already open - `B` while open is the developer tool's own cycle key, unchanged) |
| `Esc` | closes the menu (only while it is open - otherwise quits the window, unchanged) |
| Click a category tab (right-hand column) | switches to that category's grid |
| Click an unlocked grid cell | arms that type for placement - the usual click-to-build on the map follows, unchanged from the developer picker |
| Click a locked grid cell (greyed/padlocked) | nothing happens - no type is armed, no command is sent |
| Click the pickaxe (bottom-right of the grid) | toggles demolish mode (same `X` key the developer tool already has) |
| Click the X (bottom-right of the info/placement panel) | closes the panel / cancels demolish mode (shares the pickaxe's own pixel rectangle - the two are never shown together, so a click there resolves to whichever one currently applies) |

## A walk-through

1. `B`: the menu opens on "public". The grid shows thumbnails for unlocked types and a padlock
   for locked ones.
2. Click an unlocked cell (e.g. the pioneer house, top-left): the info/placement panel replaces
   the grid - its frame, rotate arrows and an (empty, see "Known limits") upkeep slot. The grid
   is gone until the panel closes.
3. Click a buildable spot on the map: the usual placement preview and commit, unchanged. The
   panel stays open.
4. Click the X: the panel closes, back to the grid.
5. Click the pickaxe, then click the building you just placed: it is demolished (the usual
   confirmation/feedback, unchanged).
6. Click a different category tab: its own grid draws (thumbnails, locks, the same as step 1).
7. Try an unaffordable item (needs `ANNO_DEV_GOODS_TONS=0` at launch, or having spent the test
   stock down some other way): arming it still works - cost is not checked until you click the
   map - and placing it refuses the usual way (title shows the refusal), exactly like any other
   build attempt.

## The HUD bar

Gold is always shown (idle and with the menu open). The four build-material counters and the
resident counter are shown only with the menu open (the idle HUD never shows them - matches the
original's own documented behaviour, OQ-538). All five are plain decimal numbers, white, at the
status bar's own measured slots.

## Known limits (say so rather than guess)

- **GPU rendering: none of this draws.** The HUD/menu/panel overlay is CPU-renderer only today;
  `--renderer gpu` (or the default `auto`, which picks GPU on this machine) draws the world but
  no UI chrome at all - this is why the launch command above pins `--renderer cpu`. Verified, not
  assumed: a live GPU screenshot showed 100% of the oracle's opaque pixels missing.
- **The info/placement panel shows no numbers or name.** The cost row and the building's name
  have no confirmed pixel position in the original (OQ-561 - an exhaustive search found none);
  rather than guess one, this build draws the panel's frame, arrows and the upkeep icon's own
  slot and nothing else there. The slot sits empty (upkeep itself has no live source for an
  unbuilt placement either - OQ-563).
- **Text colour is a legibility default, not confirmed against the original.** OQ-031: what the
  original actually draws for a bitmap font's three pixel levels is still open. This build uses
  plain white: readable, not verified to match.
- **Digit alignment inside the HUD's measured slots is a reasoned default, not confirmed either**
  (OQ-564): left-aligned, vertically centred by the font's own glyph height. The *positions*
  (which rectangle) are real sprite-sad-measured evidence; where the text starts *within* one is
  not.
- **Other menus are placeholders or absent.** `docs/adr/0004-ui-toolkit.md`'s own screen-tree
  table (section "M") lists all 19 in-game screens: only the HUD and this construction menu are
  bound; the Kontor panel, help/military/options tabs, the player/ship/city lists, pause and the
  rating screen have no handler at all yet. Two real key collisions exist if any of those are
  wired later without resolving them first: `S`/`Q`/`D` are already the developer viewer's own
  pan/quit controls.
- **R3 (UI scale) is still an open owner question.** The menu only responds at exactly 1024 x 768
  drawable pixels; at any other window size every menu click falls through unchanged (never a
  guessed scale).
- **No network, no AI, no trade routes** - unchanged project-wide limits, not specific to this
  feature.
- Verified here: `viewer_wiring_construction_full_proof`, `viewer_wiring_construction_unaffordable`,
  `construction_session_replay`, `viewer_wiring_construction_menu_arm`, `ui_mock_matches_oracle`,
  `live_screen_matches_oracle`, and the construction-menu/play unit tests (`anno_app_tests`), all
  on this build. Not verified: a live, hands-on playtest by the owner.
