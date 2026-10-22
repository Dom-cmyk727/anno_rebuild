# Driving the original as a player: the play layer of `tools/orig/control.py` (spec)

**Status:** spec, 2026-10-22, orchestrator; owner request 2026-10-22: use the manual, the guides and the videos so the tool can run the game. It extends T15's harness (`tools/orig/`, PR #16). The player knowledge it relies on is [`../research/playbook.md`](../research/playbook.md); the safety rules are those of `original-harness.md` and the PR #16 review (B1).

**For now the original runs only on the owner's PC,** from the copy at `C:\anno1503-original\` and only in a session the owner agreed to. (Running it on a cloud VM, Windows or Linux/Wine, is a later topic.)

## 1. Goal

Claude can start a game and play it with verified actions: found a settlement, place buildings and roads, plant fields, grow the colony through the levels, run ships on trade routes, buy and sell at the Kontor, research, and handle diplomacy.

Every action is checked against the game's own state, read from memory. A screenshot alone is not proof. This serves two purposes:
- **The M2 fixtures,** which need set-ups the original can only reach by playing: a weaver supplied with wool, houses with and without a road to a market, taxes over time.
- **Later, `anno-diff`:** the same commands played in the original and fed into `src/sim` through the S9 replay (T8), with the two states compared.

## 2. Architecture (three layers)

1. **Input and safety** (`control.py`, exists, plus B1):
   - `SendInput` keys, clicks, drags and wheel;
   - before **every** input: `GetForegroundWindow() == game window`, and the target point lies inside the client rect;
   - abort when the cursor is not where the tool left it (the owner took the mouse);
   - `HWND_NOTOPMOST` restored on exit; one instance only;
   - every action appended to `cache/orig/<session>/actions.jsonl` with time, kind, target and window title.
2. **Perception** (`state.py`, new; extends `probe.py`): read-only state from memory, with screenshots only for UI states memory can't show yet.
   - **Done (PR #16):**
     - the world: tick, speed (rate);
     - the city: the store per good `+0x334 + 12g`, capacity `+0x624`, inhabitants `+0x2f0`, houses per level `+0x18a`, stage mask `+0x618`.
   - **Needed, in order:**
     1. **The hovered tile and the camera** (RE by read-only memory diffing; see §3). This is the key to placing buildings.
     2. **Player money** (player `+0x74`; the player object's vtable, OQ-T15-PLAYERVT).
     3. **The city's building lists** (the building index at city `+0x144`; the lists `+0x108`, `+0x88`, `+0xe8`, `+0xc8` = houses, `+0xa8`; `population.md` §3), each building's type, tile, status and stock. With these a placement is verified and production is read.
     4. **Per-house residents** (house `+0x2c`; the house vtable, OQ-T15-HOUSEVT).
     5. **The Kontor's trade settings** (sell/buy flag and list index per good, city `+0x334 + 12g + 0xa/0xb`; `production.md` §5.3) and **ships** (figures: position, cargo, route) once located.
   - A vtable-scan hit counts only after a plausibility check (fields in range, reachable from the world or island). Freed objects keep their vtable (review N1).
3. **Actions** (`play.py`, new): macros built from the playbook's UI flows. Each macro:
   - reads the state;
   - acts;
   - **verifies the expected change**, with a timeout;
   - returns success or a named failure;
   - logs one line.
   
   No macro "assumes" success.

## 3. Placing at a tile (the crux, closed loop)

Blind clicking on the isometric map failed in PR #16, because of trees, entrances and a constant screen-to-tile offset. Replace it with a loop:

1. **RE (read-only):** find the variables for the **tile under the cursor** and the **camera** (scroll position, zoom group, rotation). Use memory diffing with `probe.py`:
   - move the mouse by one tile with `SendInput` and scan for two u16 values that follow it;
   - scroll with the arrow keys for the camera.
   
   Record the module, RVA and offsets in a new RE note `docs/re/view.md`, with the evidence.
2. **`goto_tile(x, y)`:** centre the camera near the target (keys or minimap), then move the mouse until the hovered tile equals `(x, y)`. Use a coarse projection first (zoom group and tile size), then step by single tiles. Give up after N steps.
3. **`place(type, x, y, rotation, align)`:**
   - open the construction menu (`B`), pick the category and the building (§4);
   - set the rotation with `.` / `,`;
   - `goto_tile`; hold **Ctrl** when `align` is set (snap to the road); click; release; Esc.
   - **Verify:** the city's building list has a new entry of that type at that tile, and the money and materials fell by the type's cost (`Anlagen.dat` `HEAD`).
   - On failure, report why: not unlocked, outside the city's area, no money or materials, blocked, enemy near.
4. **`road(path)`** drags segments tile by tile. Verify the road tiles in the island grid, or the gold spent (5 per tile for dirt roads).
5. **The entrance rule (owner):** placement knows each type's entrance tiles. Read them from `Anlagen.dat` (the `ENTRANCE` record is undecoded; that decoding is part of this task) or measure them once per type. The road is then laid to an entrance tile, not to any edge.

## 4. Menus and panels (screen layer)

The 1024 × 768 HUD map from PR #16 (`ui-and-placement.md` §1-§2) becomes a table in `play.py`:
- the four main tabs;
- the eight construction categories;
- the building grid positions per category and level;
- the info-bar fields;
- the panel layouts for Kontor, ship, house, school, shipyard and diplomacy.

Record the grid once per civilisation level, by screenshot with the owner, and keep it as data. The menu shows "?" for locked buildings and dark icons for "soon", so the macro checks the unlock in the stage mask before trying.

**Required panel flows** (the playbook gives the steps): Kontor buy and sell (add a good; set the stock-limit and price sliders), ship load and unload (1 / 5 / 10 / 50 t buttons), trade-route editing (route points, four actions each, anchor), material lock, Zzzz (stop/start), the plant button (or V in the add-on), school research, diplomacy treaties.
- **Sliders:** drag, then read the value back from memory; adjust until it equals the target.
- **Diplomacy treaty icons cycle states** (a wrong click can break a treaty), so read the treaty state before and after, and abort on an unexpected change.

## 5. Strategy layer (Claude plays)

- **The opening from the playbook (§12) as a script:**
  1. wood, food, cloth and leather;
  2. 30 houses with stands;
  3. tavern and chapel → settlers.
  
  Each step is a macro with a verified result.
- **A monitor loop every few game seconds (read-only):**
  - stocks and their trend per good;
  - inhabitants per level;
  - money and balance;
  - events: fire, plague, "?" overload, "raw material missing".
  
  Alarms (a stock falling below N minutes of consumption, by the `Menge` table; a fire; a negative balance trend) hand control to Claude. Claude decides and acts through macros.
- **Speed and saves:**
  - play at F5–F7, drop to F8 or pause (P) to think (no orders while paused);
  - quick save (F10) at calm points (the quirk: a tavern's alcohol and cart cargo are not saved);
  - every fixture starts from a named save.

## 6. Stages and acceptance

| Stage | Content | Acceptance |
|---|---|---|
| A | B1 safety, action log, single instance, `quicksave`/`quickload`, the screen table for the HUD and the construction menu | owner present; one session where a key goes to another window is refused and logged |
| B | RE: hovered tile and camera (`docs/re/view.md`); `goto_tile`; player money; city building list | the cursor lands on 10 random tiles on two zoom groups and two rotations; money and building counts equal the HUD |
| C | `place`, `road`, `demolish`, `stop`/`start`, `plant`, `lock_materials`, with entrance tiles | each macro verified 10× from a quick save, with failure reasons named |
| D | The opening script, Citizen endless game: from the start to the first 50 settlers without a hand on the mouse | log, screenshots at milestones, state curve; re-run from the same save reaches the same milestones (times may differ) |
| E | Kontor trade, ship load/unload, one automatic trade route to a second island, scout to a deposit | a route delivers 3 cycles; a buy order fills from the Venetians |
| F | Research and diplomacy (trade treaty with an AI) | treaty state read before and after |
| G | The M2 fixtures on top (PR-F2 weaver, houses with and without a road to a market, taxes) | numbers into the RE notes, "verified by running" |
| later | Combat, multiplayer, cloud runs | — |

## 7. Rules

- Read-only memory; no writes (OQ-T15-WRITE stays an owner question).
- Never type into anything but the game window (B1).
- One action at a time. Owner-agreed sessions only.
- Outputs only in `cache/orig/<session>/`; nothing of the original in git.
- No third-party tool without the owner's yes; standard-library `ctypes` only, as today.
- Every mechanics claim the tool's runs produce goes into the RE notes only with its measurement. A guess goes to `open-questions.md`, as the PR #16 review asked.
