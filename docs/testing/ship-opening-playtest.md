# Ship-start opening: playtest card

For the owner's personal playtest of W4-OPENING-1 (`task/W4-opening-1`). **Status: automated completion
shown on this build; your own playtest is NOT_RUN.** Nothing here is the full game: no menus, no network,
no trade routes, no combat. The window title is the HUD (there is no text renderer yet).

## What it is

Five expeditions, one ship each (original loadout: 30 000 gold, wood 50 t, food 10 t, tools 50 t and 30 t),
no city, at peace, shared information, on **candidate A** (`data/maps/candidates/ship-start-v2/lan5-ship-a.json`,
an explicitly named playtest input; it is not the installed map and `lan5.txt` is unchanged). You are one seat.
The other four ships are drawn (blue ring when you select one) but cannot be ordered.

## Launch

You need the original game folder (the one with `Anlagen.dat`) and a build of this branch
(`cmake --preset msvc`, `cmake --build build/msvc --config Release`).

```powershell
.\tools\play_ship_opening.ps1 -GameDir E:\anno1503\game\app            # seat 0, speed 200
.\tools\play_ship_opening.ps1 -GameDir E:\anno1503\game\app -Seat 2    # another seat
.\tools\play_ship_opening.ps1 -GameDir E:\anno1503\game\app -Resume .\saves\quicksave-seat0.sav
```

or directly: `anno1503.exe --ship-start <manifest.json> --seat 0 --speed 200 --game-dir <game folder>`.
`-Speed 100` is the original multiplayer pace (seat 0's first island takes about 3 minutes at 100, 90 s at 200).
Another seat is another window: start a second process with `-Seat N`; this is a local playtest, not
networked play.

## Controls

Kept as they were: `W A S D` / arrows pan, mouse wheel zoom (`+` `-` `0`), right-drag pans, `Space` pauses, `E`
and `R` water, `B` the building tool, `Esc` / `Q` quit (Esc first cancels what is in progress).

| Do | Effect |
|---|---|
| left click a ship | select it (yellow ring = yours, blue = someone else's: inspect only) |
| left click empty sea | deselect |
| right click open sea | order the selected ship there (one order per click; land and outside the world are refused locally) |
| right drag / left drag | pan; a drag never selects and never orders |
| `H` | stop the selected ship |
| `F` | founding mode: the warehouse sprite follows the pointer; `Tab` cycles the type; there is no manual turn (as in the original, the warehouse turns to the coast by itself; `T` does nothing here); left click sends the founding (one click, one command) |
| `U` | unload mode: `Tab` next hold, `-` / `+` (or `Page Down` / `Page Up`, or `[` `]`; in this mode `-` / `+` set the quantity instead of zooming, the wheel still zooms) quantity −1 / +1 ton (`Shift`: 10), `Enter` sends exactly that hold and quantity to your nearest warehouse (left click one of your warehouses to choose it) |
| `B` | building tool: `N` / `P` type (`Shift+N` / `Shift+P`: ten at a time), `T` rotate, `X` demolish, left click place; it follows your founded city |
| `F5` | quick-save (written off the window thread to `saves\quicksave-seat<N>.sav`) |
| `Esc` | cancel the gesture, then the mode, then the selection, then quit |

Founding (`F`): the warehouse is drawn where you would found it, and only where the sim would build it. It must stand on a coast:
the sim tries the four turns and builds at the first whose land edge is the beach and whose opposite edge is in the water, so the
building and its landing stage face the shore by themselves (no manual turn, as in the original). The title says `facing the shore`
or, where no turn fits, `not here` with the sim's reason (`NoRotationFits`, `OutOfRange`, ...).

**Picture fault fixed (2026-11-03, `task/iso-handedness-W4`; still present on `task/W4-opening-1` until that merges):** our renderer drew the whole world as the mirror image of the original
(the original's camera puts world +x to the lower left, ours to the lower right; found from the original's camera code and checked on
original towns). Until that renderer fix lands, every turned building looks wrong, the Kontor included: its piles stand along the shore
and its land-side wall faces the water, although the sim placed and turned it exactly as the original's own scenario Kontors are.

`Space` pauses the sim and the water; the title then starts with `PAUSED (Space resumes)`.

On the GPU the opening starts with enhanced water (`E` toggles): moving ships raise a bow wave and leave a wake.

The title (the window's caption: it is cut off at the window's width, so what you need comes first) shows, in this order:
the mode, the last message, the **latest command's own state** and the real refusal, the unload / founding advisory, the
pointer's tile (sea/land, to aim), then your gold, the selected ship (id, seat, tile, cargo by hold), your city and its store,
and last the window's own name and renderer. A
move reads `queued → accepted, planning → sailing → arrived (shown)`; acceptance is not arrival, and "shown"
only after a frame with that tick was delivered. A refusal names the sim's reason (`StockFull`, `OutOfRange`, ...).
The founding and unload numbers are advisory ("would move 5 t"): the sim decides when the command runs.

## A walk through (seat 0; every coordinate was driven the same way by the automated session)

1. Left click your ship (it opens centred). The title says `ship #1 seat 0 (yours) ... cargo`.
2. Zoom out with the wheel, pan with the right button until the pointer reads `(257,134) sea` (north-west, about
   550 tiles from the start; island 0), and right click. The title says `move #1: sailing`. About 90 s at speed 200.
   When it says `arrived (shown)` the ship is beside a bay of the first island.
3. `F`, move the pointer to the coast tile `(255,138) land`, click. `found #n: accepted: warehouse #1`;
   your gold drops by 350 and wood 12 t and tools 5 t leave the ship; a city (a name) appears in the title.
   A founding the sim refuses (a water tile, too far, steep ground) changes nothing: try another click.
4. `U`: the title shows `hold [0] HOLZ request 38 t: in range; would move 38 t`. `Enter`; then `Tab`, `Enter` for each
   hold. The fourth (tools) is limited by the 50 t store: `moved 5 t of 30 requested (limited)`; sending it again is
   refused `StockFull`.
5. `Esc` twice, `B`, press `N` / `Shift+N` until the title says `build 1001 (3x3, 300 gold)` (HANDWERK) or `501`
   (PIONIER), click land near the warehouse (for example `(251,137)` and `(250,141)`). Gold 29 350 after HANDWERK.
6. `F5`, quit, and later start with `-Resume .\saves\quicksave-seat0.sav`.

## Known limits (say so rather than guess)

- A developer playtest UI: title-bar HUD, no text overlay, no sounds, no minimap, no ship names.
- The sprite for a ship's facing is the figure's frame `dir16` (the original's facing-to-frame mapping is "assumed"
  in `docs/render/iso.md`); sails stay furled; scout and donkey stay embarked.
- Terrain and ownership are not checked by the founding advisory (the sim refuses), so a founding click may be
  refused. Island objects (treasure, mountains) are not modelled by the sim yet.
- One local participant per window; another seat is a separate process. No network, no trade routes, no AI.
- Resume is a launch option (no in-game load); a save from other game data or another map is refused with its reason.
- Sailing the first 550 tiles takes minutes; this is the sim's pace, not a rendering limit.
- The title bar is the HUD and a window caption is cut off at the window's width (about 190 characters were
  there with nothing selected): the command state, the advisory and the pointer come first, so a long cargo
  line is what gets cut. **With the building tool on (`B`), the tool's own line (`build 1001 (3x3, 300 gold)`,
  the placement verdict, `command #n`) comes last, after about 300 characters: maximise the window
  (double-click its caption) before step 5 or you will not see it.** Widen the window to read the stores.
- You see every ship of the world, whatever the map's visibility policy says (candidate A shares information; a
  map that hides ships is not honoured by this window yet).
- The window uses the GPU the system picks (the RTX 4090 on this PC); there is no switch for another adapter.
- Where a ship is drawn and which way it faces is looked at in screenshots, not asserted by a test; the pointer
  picks a ship by the same projection the sprite is drawn with.
- Saves go to `saves\` in the checkout (`ANNO_SAVE_DIR` changes it), not to the folder you started from.
- Verified here: the production window on the RTX 4090 (Vulkan) and on the CPU renderer, replayed in fresh
  processes. Not verified: native 4K, a Radeon window, physical display latency, five-player scale, late-game
  load, original-runtime fidelity of the ship sprites' facings.
