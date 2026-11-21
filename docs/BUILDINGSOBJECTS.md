# BUILDINGSOBJECTS: how every building and object is placed, shown and embedded

**Standing reference (owner, 2026-11-17):** "Write down all results in a markdown called BUILDINGSOBJECTS.md and use
that as reference always when working on buildings." Every packet that touches placement, the build menu, the
placement ghost, the info panel's preview, entrance arrows, or what a placed building does to the world reads
this file first, and updates it when it learns something new (with evidence, as below).

**Sources:**
- GOG 2.0.0.5 (`game/app`): `Anlagen.dat`, `FigurenHeader.dat`, `Data/*`, and the DLLs read by static
  disassembly.
- History Edition (`E:\SteamLibrary\steamapps\common\Anno 1503 - History Edition`): its type files, and the
  owner's recordings `he_01.mp4` and `ui_9.mp4` (pixels only).
- The repo's specs and open branches as of 2026-11-17.

Branch `task/BUILDINGS-REF-1`, base main `4dfdd9b7`. Built by four research passes (UI lane, 2026-11-17):
- part A, the per-type inventory, generated from the files;
- part B, the placement rules;
- part C, the entrance/exit arrows;
- part D, what placing does to the world.

The per-type tables of part A are generated from the user's own files and never committed: `python -I
tools/buildings_inventory.py [--game-dir DIR] [--he-dir DIR] [--out FILE]` (default `ANNO1503_GAME_DIR`,
output `cache/buildings/inventory.md`). The raw research (scripts, disassembly listings, frame crops) stays in
the worktree's `cache/bref/` (git-ignored).

**Evidence tags used throughout:** **D** = read in the code (module + address given); **P** = parsed from an
original file; **L** = likely (inferred, not read to the end); **U** = unknown (an open question). The open
questions this reference raised are OQ-728..749 in `docs/open-questions.md` (part B: 728-734, C: 735-739,
D: 740-749; BUILDINGS-REF-2, 2026-11-19: OQ-764..772, part B section 10).

**Original data and text:** the per-type field values (part A's tables) and the display names from `Texte.dat`
are not committed (repo rule: original data is read from the user's files at run time; owner rule: no
original text in git). This file keeps the method, the field meanings, findings and summary counts.

## Quick answers

0. **BUILDINGS-REF-2 (2026-11-19), the short version** (part B section 10): roads are an **L drag, long axis first, at
   most 40 tiles, every tile in own territory, no dirt over cobblestone, re-laying free**, with a neighbour reshaper that
   picks straight/corner/T/cross/end ids; the flat straight piece has **no terrain test**, so it can stand on territory
   sea tiles (HE agrees). The rebuild's sim already places land roads, but the **picture draws no runtime tile object**,
   which is why the owner saw nothing. `Occupied` is always a **building footprint** (trees, fields and stones never refuse
   in the sim, as in the original; placing removes them, demolish does not bring them back). Only buildings
   (PRODTYPE <= 192) are selectable; a click with a building armed places it.

1. **One path for everything (D, part D).** Every building, road, wall, field, tree or decoration the player
   places goes cursor → apply mode 6 → `City::Build` → `City::Place 0x100279a0`. Placement is **instant**:
   there is no building site, and the first tick waits for the class gate (500 t production and public,
   2500 t houses, 166 t towers). Costs are taken before the object is created, and **demolishing refunds
   nothing** (D + manual).
2. **Who places what (D, part B):** separate cursor classes in AnnoFrame (factory `0x10066330`):
   - the default building cursor (kind 4), which can drag up to 64 ordinary buildings;
   - road (5): an L-shaped drag of at most 40 tiles, every tile in own territory, no downgrade, free re-lay;
   - wall (0x12);
   - field/tree (0xd): a rectangle drag of at most 256 tiles; on a dry tile a field becomes its dry variant;
   - mine (0xe);
   - Kontor (0x13);
   - demolish (6).
   Coastal buildings (Kontor, fisher, shipyard, whaler) **turn themselves** until one rotation fits.
3. **Where (D, part B):** the terrain class comes from the type's flag word (`F & 3`, `F & 0x7000`):
   - ordinary land;
   - coast;
   - road/bridge (exempt from the height test);
   - slope (walls and slope roads);
   - mountain (mines and quarries);
   - single-tile crop/tree objects (`0x29`: floor of the four-corner mean ≥ 32, overlap only with objects
     whose flag has bit `0x20`; FERTILITY-1a).

   A footprint needs one owner city and at least one owned tile. Kontor and markets (PRODTYPE 33-35) claim a
   territory disc of radius 24 on neutral tiles only and never repaint owned tiles. Market 1307 has radius
   255.
4. **What the player sees while placing (D + HE pixels, parts B/C):**
   - **A valid ghost:** the building, its radius outline (GOG yellow, HE green), and for crop-input buildings
     the fertility bar (UI-PRODBAR-1).
   - **Refused ground:** a yellow footprint rectangle and no ghost.
   - **Unaffordable:** the ghost **blinks** (HE about 0.75 s on, 0.5 s off). The preview ignores cost; the
     click checks it.
5. **Entrance arrows (D + P + HE, part C):**
   - **Where they are drawn:** only in the **info panel's picture**, both in the build menu's preview and in
     a placed building's panel. **The map ghost draws no arrows**, in GOG or HE, valid or refused. The
     owner's `ui_9` clips at 3:02-4:55 show the panel picture.
   - **The data:** the arrows are the type's `OBJECTPOS/CONPO2` points, drawn with figure 1573
     `Strassenanschluss01` ("road connection"), which has 8 directions (OQ-711 resolved).
   - **Counts:** they match the owner's counts (forester 3, sheep farm 3, hunter 4, small farm 4, fisher 4,
     tanner 4; a 3-arrow mine is the salt mine 1203).
   - **What they mean in the world:** they mark the building's road connections. Carriers and carts start
     and end at the type's ENTRANCE points, and cross the footprint where the arrows sit, so the arrows are
     both entrance and exit.
   - **Placement does not check a road.** A missing road shows later as the "no road" status (icon 1571,
     OQ-720).
6. **The editions (P, part A):** the HE game loads `Anlagen.addon`, which is **byte-identical** to GOG
   `Anlagen.dat`, so all type data agrees. HE's older `Anlagen.dat` and `Anlagen.classic` differ (ruins in
   place of some houses, field costs and similar) but are not what the HE game loads.

## The rebuild's gaps, all parts (2026-11-17, main `b9b8df23`/`4dfdd9b7` plus open branches)

| # | Gap | Part | Status / owner of the fix |
|---|---|---|---|
| 1 | A market or second Kontor claims no territory (only FoundCmd does); BuildCmd claims no neutral footprint tiles; no ore discovery, reveal or Kontor id for them | B, D | sim; not yet placed |
| 2 | No storage/EK-VK recount when a market is placed; removing a market never shrinks territory or recounts | D | sim |
| 3 | In-place market/Kontor upgrade refused as `Occupied` | D | sim (O-4) |
| 4 | Fields and trees (0x29) cannot be placed: no rectangle drag, no dry-variant swap | B, D | FERTILITY-1a has the placement class; planting is still `Unsupported` |
| 5 | Walls, gates, bridges, ramps, piers, plazas, fences, monuments `Unsupported` | B, D | sim + UI |
| 6 | Road drag: no L-path, no 40-tile cap, no every-tile-own-territory rule, no no-downgrade rule, no free re-lay; no neighbour reshaping (`0x1007cc40`). Detail and packets: 10.1, rows 16-18 | B | sim + UI |
| 7 | Ghost: the window's check covers bounds, occupancy and cost only; it always draws; no yellow rectangle, no radius outline, no blinking when unaffordable | B, C | UI-GHOST-1 (proposed) |
| 8 | Ordinary buildings cannot be drag-placed (original: up to 64), and no random house variants (property 0x32) | B | UI + sim |
| 9 | Mine cursor's snap to deposit sites not modelled; gold and gem mines `Unsupported` | B | sim (OQ-728) |
| 10 | Arrows on a placed building's info-panel picture (the build preview has them on task/UI-INFO-2b, not yet on main) | C | UI-INFO-2d (re-scoped: no ghost arrows, see below) |
| 11 | "No road" status (code 3) and its icon 1571 | C | STATUS-NOROAD-1 (planned) |
| 12 | Carrier spawn uses the first entrance, while the original uses a reachable one (`0x10072b50`, unread) | C | sim |
| 13 | Ctrl auto-align missing | B, C | UI (OQ-735) |
| 14 | Well hooks on place/remove; church/university/library hooks; run planner for watchtowers and gates; fire/plague stop and stall detach on remove | D | sim |
| 15 | The figure veto (`0x10045080`) runs only at founding, not in Build | B | sim |
| 16 | **Runtime tile objects (roads, later fields and trees) never reach the picture**: a placed road is `Applied` but `NoVisualChange`; the trees under a new building stay drawn | B (10.1, 10.2) | **Settled for runtime roads & cleared scenery (S + code, ROADS-0):** `PlayRunner` diffs tracked tile objects (`kRoadFlag`) and cleared tick-0 scenery alongside `BuildingDelta`; `ScenePicture` -> `apply_island_buildings` emits `PROD2` sprites (`id, rotation, height, phase`), placed roads reach `Visible`, cleared trees disappear and stay bare on demolish. Planting crops/trees remains gap 4 |
| 17 | Road drag: L path with y first on ties, 40 tiles, every tile territory, no downgrade, same-family free, kind-9 prefix commit; hold-and-follow ghost | B (10.1) | ROADS-1, ROADS-3 |
| 18 | Road autotiling table (corner/T/cross/end, dirt = cobblestone + 20) | B (10.1 item 8) | ROADS-2 |
| 19 | The ghost hides no trees; no yellow rectangle on refusal; occupied footprints invisible | B (10.2) | UI-GHOST-2 |
| 20 | Selected building has a title but no yellow highlight; market/Kontor click does not open the warehouse | B (10.3) | UI-SELECT-1/2 |

**Consequence for open UI packets:** UI-INFO-2d item (a), "the map ghost's entrance arrows", is **dropped**.
The original draws none (part C §2), so OQ-713 closes as "no arrows on the ghost". Arrows on a placed
building's info-panel picture replace that item.



---

# Part B: placement rules

## B. Placement rules: when and where each building or object may be placed

BUILDINGS-REF-1 part B, 2026-11-17. Read-only research: no tracked file changed, nothing built, no game started.

**Sources.** GOG 2.0.0.5 `AnnoFrame.dll`, `AnnoWelt.dll`, `AnnoGame.dll`, `View.dll`, `Anlagen.dat` (static disassembly, capstone; image base `0x10000000` in every module). History Edition (HE): files only. Existing RE is cited, not redone:

- `docs/re/ship-placement.md` (SP);
- `docs/re/road-placement.md` (RP);
- `docs/re/ship-founding.md` (SF);
- `docs/re/ui.md` §2.5-2.6 and §4 on main (UI);
- `task/UI-PRODBAR-1` `docs/re/ui.md` §2.6 (UI-PLACE-1/2) and §4.1 (PB);
- `task/UI-STATUS-1` `docs/re/ui.md` §5.6 (ST);
- `docs/sim/salt-extraction.md`, `iron-tools.md` and `quarry-bricks.md` (mountain class and coverage).

**New in this pass.** These findings are marked **new**:

- the cursor vtables and the factory kinds;
- the Default cursor's check and input handler, with drag multi-placement;
- the road and wall drag path and its 40-tile cap;
- the field rectangle drag and its 256-tile cap;
- the unaffordable-ghost blink mask;
- the demolish ownership rule;
- the GOG-HE data comparison.

**Tags.**

- **D**: read in the code.
- **P**: file or pixel evidence.
- **L**: likely; inferred, not read to the end.
- **U**: unknown.

`B` is the complete cursor object (vtable `0x100883d8` for the Default cursor).

---

### 1. The cursor classes: who places what

Every placement goes through one `tCursor*` object in `AnnoFrame.dll`. The frame keeps one cursor per kind (`0x10007e50`, cache at `this + kind*4 + 0x1834`). Factory `0x10066330` constructs them. The build menu fetches kinds `0xd`, `0xe`, `4`, `5`, `6` and `0x12` once at set-up (View `0x1002b7e0`-`0x1002b8c0`, stored at view `+0x640`, `+0x63c`, `+0x644`, `+0x648`, `+0x650`, `+0x64c`). The ship menu fetches kind `0x13` (View `0x1006330b`, SF §3.1).

| Kind | Class (RTTI) | Constructor | Input handler (slot `+0x70`) | Draw (slot `+0x74`) | Places | Tag |
|---|---|---|---|---|---|---|
| 4 | `tCursorAnlageDefault` | `0x10048530` | `0x10047470` | `0x10048450` | every ordinary building: houses, workshops, farms, public buildings, markets, and coastal buildings that are not the Kontor | D (vtables, factory); L (type-to-kind map) |
| 0x17 | `tCursorAnlageDefaultEdit` | `0x10048620` | (editor) | `0x100489b0` | editor only | D |
| 5 | `tCursorAnlageStrasse` (inherits `tCursorAnlageMauer`) | `0x1004b830` | `0x1004b290` | `0x1004b7f0` | roads (`STRASSE`), drag | D; L (type map) |
| 0x12 | `tCursorAnlageMauer` | `0x10049e50` | `0x10049950` | `0x1004b7f0` | walls (`MAUER`, flag `0x1886`), drag | D; L |
| 0xd | `tCursorAnlageRohstoff` | `0x1004ac30` | `0x1004a490` | `0x1004abf0` | fields and trees (crop objects, flag `0x29`), rectangle drag | D; L |
| 0xe | `tCursorAnlageBergwerk` | `0x10046bf0` | `0x10046830` (check `0x10046300`) | `0x10046bc0` | mines; snaps to deposit-site records (SF §3.2-3.4) | D (class); **U** whether the build menu arms it for `MINE`/`STEINBRUCH` (OQ-728) |
| 0x13 | `tCursorAnlageKontor` | `0x10049400` | `0x10048e60` (validator `0x10048b30`) | `0x10049230` | Kontor founding from a ship | D (SF §3, §8, §10) |
| 6 | `tCursorAbriss` | `0x100424b0` | `0x10041e60` | `0x10042420` | demolish (pickaxe) | D |
| 0xc | `tCursorBergwerk` | `0x1004c6c0` | `0x1004c4f0` | `0x1004c690` | not fetched by the build menu (editor or scenario) | D (class); U (use) |

Each cursor also carries the `iAnnoCursorBau` interface (third vtable). Slot `+0` gives the material cost and slot `+4` the gold cost; the status bar's cost row reads both (ST). Only the Default cursor has a fourth slot `+0xc` = `0x10047170`, the preview/click check. The other cursors run their own checks inside their input handlers.

**HE.** `AnnoFrame.dll` and `AnnoFrameAddOn.dll` hold the same 24 `tCursor*` RTTI names and `iAnnoCursorBau`. Both are x64, so the code could not be compared byte for byte (P, string scan).

**Which cell arms which kind.** The cell callback in `View.dll` was not found. The type-to-kind column above is inferred from what each handler does: the Strasse cursor emits commit kind 9, the Rohstoff cursor tests PRODTYPE `0xe1`/`0xe2`. See OQ-728.

---

### 2. Placement classes and terrain tests (the island predicate)

Every cursor ends in `City::Build` (`AnnoWelt 0x100276a0`). That calls the island predicate (island slot `+0x6c` = `AnnoWelt 0x1007c790`), then the type slots (`AnnoGame` `+0x6c` `0x10008920` and `+0x68` `0x10008f90`). The class is decided by `F` = the flag word (HEAD u16 `+0x1c`, type `+0x12`).

| Class / flag word | Types (`Anlagen.dat`, P) | Terrain/height test | Result byte | Acceptance | Address | Tag |
|---|---|---|---|---|---|---|
| Ordinary land `F & 0x7017 == 0x0004` (`0x84`, `0x04`) | 352 (houses, workshops, plantations, markets, public, `ZIERDE`, `DENKMAL`, `ZAUN`, `PLATZ`, ...) | every `(fw+1)(fh+1)` vertex of the window equal and a multiple of 16 | that level `v` | `v >= 32` (threshold `trunc(1.28f/0.04f)`); PRODTYPE group `0x141` exempt | `0x10008f90`; step 7 `0x1007cbc2`-`0x1007cc26` | D + E (SP §3) |
| Coast `F & 0x7017 == 0x2007` (`0x2087`) | 30: `KONTOR` 13, `PIER` 11, `FISCHER` 3, `WERFT` 2, `WALFAENGER` 1 | for `n,m > 2`: window max `& 0xf0 == 0x30`; the shore edge (selected by rotation) all `0x30`; at least `floor(n/2)` vertices of the opposite edge `< 0x20` | `max & 0xf0` | accepted with no level test | helper `0x10008ea0` | D + E (SF §10.4) |
| Road `F & 0x7000 == 0x4000` (`0x4186`, `0x4386`, `0x4087`) | 35: `STRASSE` 13, `RAMPE`/`RAMPEHIGH`/`RAMPELOW`, `BRUECKE` 12, `PIER` 2 | none (`0x10009199 jne`) | mode 2: lowest cell minimum | accepted at once (no terrain refusal in the predicate) | `0x1007cbc2` | D + E (RP §2) |
| Slope `F & 0x7000 == 0x1000` (`0x1186`, `0x1886`, `0x1386`) | 52: `MAUER`/`MAUERUP` 41, `STRASSE` on slope 8, `BRUECKE` 3 | `mx - lo > 0x10` refuses | mode 2 | `rec[8] >= 32` | `0x10008f90` | D + E (RP §2) |
| Mountain `F & 0x7000 == 0x3000` (`0x3096`) | 12: `MINE` 8, `STEINBRUCH` 4 | `HIGHMAP` table (`2h x 2w` half-tile cells, type `+0x234`): refuse when `table[cell] <= ftol(height(sample)) - (lo & 0xff)`; plus the inherited `lo >= 32` | mode 2 `lo` | the coverage test below must also pass (bit `0x10`) | `0x10008a80` (sampler `0x10008d90`, `0x10015e90`) | D + E (salt-extraction §11, §18) |
| Coverage bit `F & 0x10` | the mountain types; `FELSEN`, `KLIPPE`, `STEIN`, `WASSERFALL`, `FLUSS`, `EISLOCH` (scenery) | per 8 x 8 world chunk: every touched chunk needs at least one footprint tile **not** covered by an existing bit-`0x10` object with its anchor bit set | - | refuse otherwise | `0x1007c955`-`0x1007cb3d` | D + E (salt-extraction §18) |
| Single-quad tile object `F & 3 == 1` (`0x29`) | 291: `ROHSTOFF` 180, `ROHSTWACHS` 81, `ROHSTDUERR` 30 (fields, trees) | floor of the four-corner mean | mean | `>= 32` | `0x10008fa1` | D (PB) |
| Ruins `0x26` | 32 `RUINE` | bit 4, mode 2 | - | (objects, not built) | - | P |

**Overlap** (type slot `+0x6c` = `0x10008920`, all rows D + E, SP §4):

| Placed group | May replace an existing object when | Tag |
|---|---|---|
| any | the existing type has flag `0x20`: trees, raw objects, dry crops, stones, ruins. These are overlappable and are overwritten | D + E |
| `1`, `0x21`, `0x61` (production, market/Kontor, houses) | same PRODTYPE and existing `word14 & 0x100` (`HEAD[0x2e] & 1`; 3 `MARKT`, 3 `KONTOR`), which is the in-place upgrade; or a group-`0x141` existing when bit `0x10` is set | D + E |
| `0x81` | existing group `0xc1` or `0x101`, and placed PRODTYPE exactly `0x81` | D + E |
| `0xc1` (roads, ramps, piers) | existing group `0xc1`; a bridge (`0xc6`) only by a bridge | D + E |
| `0x101` (walls) | existing group `0xc1` or `0x101` | D + E |
| `0x141` | existing `0x141` | D + E |
| `0x41` public, `0xa1` stalls, others | nothing but flag-`0x20` objects | D + E |

**Footprint, rotation and island bounds**

| Rule | Detail | Address | Tag |
|---|---|---|---|
| Footprint | type bytes `+6`/`+7`, swapped when the rotation is odd | `0x1007c790` jump table `0x1007cc2c` | D |
| Rotation | cursor byte `+0x46`, `& 3`; `.` turns +1 and `,` turns −1 (Default cursor); arrows `0x10030440` (−1) and `0x10030480` (+1) | UI §2.5; `AnnoFrame 0x1000798a` / `0x100079ba` | D + P |
| Centred origin | footprint centred on the pointer tile: `x0 = x - (w-1)/2`, clamped at 0, then clamped to the world size | `0x10043e20`; Kontor `0x10049340` | D |
| Island bounds | `x >= x0`, `x + fw <= x0 + W`, the same in y | `0x1007c814`-`0x1007c879` | D + E |
| One island | all records of a drag (and the footprint) on the island at the record's tile; a change of island refuses | `0x100471a7`-`0x100471c3` (world slot `+0x8c`) | D |
| Figure veto | the footprint grown by 10 is queried for classes 6/7 (scouts, pack animals, soldiers, ships); a hostile armed figure within its range of a corner, or any figure inside the rectangle or whose ship hull overlaps it, refuses | `0x10045080` (called from `0x100471f1`; road `0x1004b0ca`; wall `0x10049843`) | D (SF §11.4) |
| Blocked tile (UI only) | island slot `+0x64` (`0x1008b950`) for each footprint tile must be 0 (an object raised above the vertex heights, L) | `0x10047378`-`0x1004738c`; fields `0x1004a374` | D (call); L (meaning) |

---

### 3. Special cases

| Case | Rule | Address | Tag |
|---|---|---|---|
| **Kontor founding from a ship** | own cursor kind `0x13`; the click lies within `trunc(sqrt(dx²+dy²)) <= 12` of the ship (truncated toward zero); ship hold covers the goods and the owner's gold covers the gold; island not `LOCKED`; every footprint tile neutral (or a takeover of a weakened Kontor); the player has no city on that island yet (message `0x138`); coast rule; figure veto; at most 16 cities per island (after payment) | `0x10048b30`, `0x100414d0`, `0x10048cd0`, `0x100967e0` | D (SF §8, §10, §11) |
| Kontor from land (second Kontor in own territory) | the Default check sees PRODTYPE `0x23`: tries the cursor's rotation and then 3 more (`rec[6]+1 & 3`) until `City::Build` accepts | `0x1004741e`-`0x1004744c` | D |
| **Coastal buildings** (fisher `FISCHER` 6, shipyard `WERFT` 7, whaler `WALFAENGER` 0xf, Kontor 0x23) | same 4-rotation auto-turn: the game finds the facing whose shore edge fits; coast class terrain | `0x100473cf`-`0x100473e1` | D |
| Pier (`PIER`, `0x2087`/`0x4087`), harbour pieces | PRODTYPE 197 >= 193 has no building list; the command path refuses ordinary BuildCmd. Which cursor places piers is U | SP §5 | P; U |
| **Mines and quarries** | terrain: mountain class plus coverage. The cursor `tCursorAnlageBergwerk` additionally snaps the footprint to the nearest 6-byte site record (`x`, `y`, rotation, mask) from island slot `+0x114` (island `+0x24`) whose mask bit `(output goods − 1)` is set. No matching record means no placement. Territory: one owner city, at least one tile owned. Cost is folded into the ghost flag (`0xA0000000` when unaffordable) | `0x10046300` (records `0x100464c6`-`0x10046645`, class walk `0x100466a1`), `0x10046830`, `0x10046913` | D (SF §3.3-3.4); conflicts with OQ-552 (OQ-728) |
| Mine and ore (command path) | `City::Build` and the predicate check no deposit; a mine accepted with no ore stays idle | OQ-552 | D |
| **Roads, drag** | press stores the start tile; the path from start to the pointer is an **L**: the longer axis first (unit steps of its sign), then the other axis. At most `0x27` = 39 steps (40 tiles). Tile `i` gets a per-tile random byte (`0x1004af50`, `rand`) for its id variant (slot `+8`) | `0x1004b3c4`-`0x1004b51f` | D |
| Road per-tile check | for each record in order, stop at the first failure: same island; tile owner `!= 15` and its city's owner = local player, all one city; ids 2518/2519 checked as 2500; `City::Build`; figure veto; a dirt road (2520-2535) may not replace a cobblestone (2500-2515) | `0x1004af90`-`0x1004b15e` | D |
| Road shapes | island slot `+0x80` = `0x1007cc40` rewrites the records by neighbours (flag bit `0x800` or PRODTYPE `0x181`; ids 2516-2519, 2536/2537, 2672/2673 are special-cased): the autotiling RP U-R1 asked for | `0x1007cc40` (about 1,600 instructions, not decoded) | D (location); U (table) |
| Road cost | a tile whose existing and new ids are the same family (cobble 2500-2515 or dirt 2520-2535) is free; otherwise the cost is accumulated (`0x10043d80`), stopping at the first unaffordable tile | `0x1004b1ed`-`0x1004b263` | D |
| Road commit | release → sink kind 9 (`[B+0x58]` slot `+0x54(9, records, n, city, 0)`) with the accepted prefix, flags `0x9000000` | `0x1004b66a`, `0x1004b752` | D |
| **Walls** (`MAUER`, `0x1886`) | same L drag and 40-tile cap, commit kind 9; per tile: own territory, `City::Build`, figure veto; a tile whose existing object has flag `0x800` (a wall piece) costs nothing | `0x10049950`, `0x10049770` | D |
| Bridges, ramps | road class (`0x4386`) or slope class (`0x1386`); placement through the road cursor is L; an existing bridge accepts only a bridge | SP §4 row 5 | D (rules); L (cursor) |
| **Fields and trees** (crop objects `0x29`, PRODTYPE `0xe1`/`0xe2`) | rectangle drag from the press tile to the pointer (bounding box, inclusive), at most `0x100` = 256 tiles. Each tile: own territory and one city, `City::Build`, slot `+0x64` free, not the same crop already there. On release each record is re-checked with `0x1000000` and dropped if refused. A growing object (`0xe2`) uses id + 1. When the crop test (island slot `+0x140`) says dry, the id becomes the type's slot `+0xf8(3)` (the dry variant), and a dry tree becomes 0 (nothing). One batch commit | `0x1004a490`, `0x1004a2c0`, `0x1004aa18`-`0x1004ab8f` | D; L (meaning of `+0xf8(3)`) |
| Field cost | each `ROHSTOFF`/`ROHSTDUERR` field tile costs 5 gold (HEAD u16 `[8]`) in GOG and HE addon; 0 in HE classic | `Anlagen.dat` | P |
| Fertility bar | farms show a 0-10 bar over the ghost: the share of a disc around the centre where the crop object could stand and would not be dry. Only when the ghost is valid | `0x10047940` | D (PB §4.1) |
| Houses (multi) | the Default cursor itself supports **drag multi-placement**: the press arms a list (byte `B+0x2e5`); every move appends a record while it does not overlap a listed one (`0x10047050`) and passes the preview check, at most `0x40` = 64. On release each record is checked again with `0x8000000` and committed one by one (`0x100441f0`) | `0x100474a6`-`0x100476b1`, `0x10047726`-`0x10047845` | D |
| House variants | slot `+8` (`0x10044340`): `id + (rand & 0xff) % n * step` from type property `0x32`; with event flag 8 (CTRL, L) no random variant | `0x100475b3`-`0x100475c0` | D; L (flag 8 = CTRL) |
| CTRL alignment | with event flag 8 the cursor tries the four rotations and keeps the first whose entrance (type properties `0x461`/`0x462`) meets a road (`0x10048030`); without it the saved rotation is restored | `0x10047e80` | D (logic); L (flag 8 = CTRL; OQ-546) |
| Market / Kontor takeover snap | group `0x21` with a foreign city: `0x10043f70` searches the window for a takeover site (player property `0x1c == 1`) | `0x100473a6`-`0x100473c4` | D (call); L (meaning) |
| Decorative (`ZIERDE`, `DENKMAL`, `ZAUN`, `PLATZ`) | ordinary land class; PRODTYPE outside the building lists, so the rebuild refuses them as `Unsupported` | SP §5 | P |
| Upgrade rewrite | PRODTYPE `0x43` (67, `KAPELLE`) with input `0x52`, and PRODTYPE `0x48` (72) with input `0x50`: the record id becomes the next type when the city qualifies (`0x1002c090` / `0x1002c740`); the rewritten type is not re-validated | `0x10027805`-`0x10027874` | D |

---

### 4. Where the player may build (territory)

| Rule | Detail | Address | Tag |
|---|---|---|---|
| Owner of the footprint (Default cursor) | each footprint tile's owner nibble (island slot `+0xec`): 15 = neutral is skipped; every other tile must belong to one city; **at least one tile must be owned** (all-neutral refuses); that city's owner (slot `+0x1c`) must be the cursor's player; a drag stays in one city | `0x100472c9`-`0x1004734d` | D |
| Owner (command path) | the same rule in `City::Build` (`0x100277c8`-`0x100277e3`) and the apply call (`0x10058890`) | SP §1 | D |
| Roads, walls, fields | stricter: **every** tile must be in own territory (owner `!= 15`) | `0x1004b02e`, `0x100497f8`, `0x1004a310` | D |
| Mines | same as the Default cursor (one city, at least one owned tile) | `0x100466a1`-`0x100466ed` | D |
| Kontor founding | the reverse: every tile neutral; no own city on the island | SF §8.4 | D |
| Territory growth | `City::Place` claims a disc of radius `HEAD[0x11]` (action radius) around the footprint for PRODTYPE 33-35 (`HAUPT`, `MARKT`, `KONTOR`), painting only neutral tiles, then the footprint. `MARKTKOMBI` (36) and every other type claim only the footprint | `0x10027a53`, `0x10027a71`; `0x10085840` | D (SF §11.2) |
| Radii | Kontor 1302/1304/1306 and market 1301/1303/1305: 24; add-on Kontors 3018, 3314, 3513, 3855: 16; 3712: 20; small markets 29xx: 3; 3100-3850 markets 16 | `Anlagen.dat` HEAD `[0x11]` | P |
| Combined range | the union of the painted discs, first come first served: a tile already owned (own or rival) is never repainted, so cities on one island share a border where the claims met | `0x100853b0` (neutral-only painter) | D |
| Other players' land | a footprint touching a rival's tile refuses (one-owner rule, owner must be the player); takeover only through the Kontor path | as above | D |
| Island lock | the `LOCKED` island property refuses **founding** only (ship predicate, validator). No lock test was found in the Default, road, wall or field checks | `0x100415c1`; `0x10048c32` | D (founding); L (absent elsewhere) |
| Market reach for houses | no placement test reads market reach; houses are placed anywhere in own territory. The yellow outline drawn around the Kontor while a house is armed is display only | UI §4 (first pass) | L |
| Demolish | the target tile must belong to a city whose owner is the local player; neutral tiles only in the editor (byte `+0x842`). Rectangle drag, up to 512 objects | `0x10041940`-`0x10041a14`, `0x10041d20` | D |

---

### 5. Cost and affordability: what the player sees

| Behaviour | Rule | Address | Tag |
|---|---|---|---|
| Preview ignores cost | the preview passes flag `0x2000000`; `City::Build` then skips `0x100275d0`. The ghost/rectangle verdict (byte `B+0x2e4`) is about the site only | `0x1004754f`, `0x1002787d` | D |
| Click checks cost | the click passes `0x8000000` (no `0x2000000`): `0x100275d0` must pass, meaning goods `0x29`-`0x2d` (tools, wood, bricks, marble, `0x2d`) in the city stock (slot `+0x7c`) and gold (player slot `+0x60`) >= the type's costs. Player option 7 skips it (OQ-126.1). An unaffordable click places nothing | `0x10027890`, `0x100275d0` | D |
| Ghost of an unaffordable building **blinks** | after a valid preview the cursor sums the costs of the records listed so far plus this one against the stock (`0x10043d80`). The ghost is written into the island with flags `0x80000000` (ghost, no tile write) or, when unaffordable, `0xA0000000`. `0x1008dc10` sets the footprint bits in the chunk's ghost mask `+0x210` and, for `0x20000000`, also in the **unaffordable mask** `+0x218`. The chunk draw `0x1009e1d0` tests that mask together with a draw-context byte `+0x140c`. When both are set it copies the real tile instead of the ghost tile, so the ghost vanishes. `+0x140d` keeps the last value and a flip forces a redraw (`0x100a0098`) | `0x10047598`-`0x100475d3`; `0x1008ec90`; `0x1008e01d`-`0x1008e074`; `0x1009e2b5`-`0x1009e306`; `0x100a00a2` | D (masks, branch); **L** (that `+0x140c` toggles with time; its writer was not found, OQ-730). P: HE recording (ghost appears and disappears for lack of resources) |
| Roads and walls, partial | the drag ghost shows the affordable prefix normally and the rest with `0x20000000` (blinking). The commit sends only the prefix that is valid; cost is checked again per record on apply (OQ-043: op 6 continues, op 9 stops) | `0x1004b762`-`0x1004b7d9`; `0x10049de0`-`0x10049e39` | D |
| Mines | the same flag fold (`0xA0000000` when unaffordable) | SF §3.4 | D |
| Status-bar cost row | while a building is armed over own territory, the cost figures show; a cost above the stock blinks `0xaabbee`/`0xee8888` every 250 ms | ST (UI-STATUS-2) `0x10064114`-`0x1006419e` | D |
| **Yellow rectangle** on invalid ground | draw `0x10048450`: byte `B+0x2e4` clear (preview refused) draws the yellow empty rectangle `0x10041050` (colour `0xffff00`) and no ghost and no fertility bar; set draws the ghost `0x100443c0` | PB §4.1; `f13_place_water` | D + P |
| Nothing drawn | gates: `[B+0x48] < [B+0x50]` (non-empty clamped footprint) and the type registered | `0x10048491`-`0x100484e8` | D |
| Pointer over chrome | the input handler returns before any preview when `0x10041290` finds no map tile under the pointer (view slot `+0x94`). The ghost drawn then is the last one; whether it hides over the panel is OQ-700 | `0x1004748a`-`0x10047491`, `0x10041325` | D (gate); U (visible result) |
| Kontor ghost | the rebuild shows it only where valid (N); the original's Kontor draw `0x10049230` was not read | - | U |

---

### 6. The placement UI flow

| Step | Original | Evidence | Tag |
|---|---|---|---|
| Arm | a U cell opens the info panel and arms the building (cursor property 0 = type id); an L cell does nothing | UI §2.5, §4 | P |
| Move | the preview runs on every pointer move (flag `0x2000000`); validity goes to `B+0x2e4` | `0x100477a2`, `0x100478a7` | D |
| Rotate | `.` +1, `,` −1; arrows; the panel preview turns with the view turn plus the cursor turn | UI §2.5 | D + P |
| Press | Default: starts the record list (multi byte `B+0x2e5`) and previews the first record. Road/wall: stores the start tile. Field: stores the rectangle corner | `0x100474b3`-`0x100475ff` | D |
| Drag | Default: adds non-overlapping valid records (at most 64). Road/wall: L path (at most 40). Field: rectangle (at most 256) | sections 3, 5 | D |
| Release | each record is checked with `0x8000000` (cost included) and committed (`0x100441f0` → `tNetClient` `0x100589a0` mode 6; roads and walls kind 9); the list is cleared; **the type stays armed** (`B+0x44` untouched) | `0x10047626`-`0x100476b1` | D |
| Cancel (event 4) | cursor Reset (slot `+0x5c`), type id `B+0x44` = 0: nothing armed | `0x100476b4`-`0x100476e2`; road `0x1004b6a1` | D; L (event 4 = right button) |
| ESC | one step back: info panel → grid with nothing armed; demolish → off (an armed building stays); grid → menu closed with the building put away | UI-PLACE-2 | P |
| Right click | on the map or the panel, everything is put away and the grid shows; on the grid, the menu closes | UI-PLACE-1/2 | P |
| Cancel-X | closes the panel only; the building **stays armed** (ghost and orange cell frame) | UI-PLACE-1 | P |
| Pickaxe | toggles demolish with or without an armed building; a cell chosen while demolishing ends it | UI-PLACE-1 | P |
| CTRL | auto-align the entrance to a road (manual p15); code path `0x10047e80` | OQ-546 | D (logic); P (manual); not captured |

---

### 7. What the rebuild implements, against the original

Rebuild: `src/sim/src/placement.cpp` classes `Ordinary`, `Coast`, `Road`, `Mountain`, `Other`; `footprint_owner_rule`; `apply.cpp` Build; `src/app` `DevTool`/`DevInput`/`dev_feedback.cpp`.

| Item | Original | Rebuild | Gap |
|---|---|---|---|
| Ordinary land rule | D + E | `Class::Ordinary` (`(F & ~0x80) == 4`) | none (emulated agreement, SP §3) |
| Coast rule | D + E | `Class::Coast` via `site::coast_height_rule` | none for the rule |
| Coast auto-rotation (Kontor, fisher, shipyard, whaler) | 4 tries in the cursor | `DevTool::turn` tries 4 turns by the coast rule only | it tests only coast terrain, not the whole `City::Build` (occupancy, owner) |
| Road class `0x4000`/`0x1000` | no terrain refusal in the predicate | `Class::Road` plus adaptations R-A2 (level floor) and R-A3 (ramp/pier preserved) as rules options | intended deviations (RP) |
| Road autotiling (island slot `+0x80` `0x1007cc40`) | neighbour-dependent ids | places the commanded id; no neighbour rewrite | **gap** (U-R1 now located) |
| Road drag | L path, longer axis first, at most 40 tiles, every tile in own territory, dirt-over-cobble refused, same-family re-lay free | `DevInput::line`: a straight line along the longer axis; cap = wire limit (8,190 tiles) | **gap**: shape, cap, territory per tile, downgrade refusal, free re-lay |
| Walls | slope class, drag, free over wall pieces | slope class rule decoded; walls not placeable (`is_land_road` needs `STRASSE`; `0x1886` is `Other`) | **gap** |
| Mountain + coverage | D + E | `Class::Mountain` for `SALZSTEIN`/`EISENERZ` mines with ore evidence, and quarries | gold/gem mines `Unsupported`; the Bergwerk cursor's site-record snap is not modelled (OQ-728) |
| Crop objects `0x29` (fields, trees) | own cursor, rectangle drag, dry-variant swap | `Class::Other` → `Unsupported` | **gap** (no field or tree placement) |
| Other bit-`0x10` scenery | coverage test | `Other` → `Unsupported` | not player-placeable anyway |
| Decorative `ZIERDE`/`DENKMAL`/`ZAUN`/`PLATZ` | ordinary class | refused: no building list | **gap** (ornamental menu) |
| Piers, bridges, ramps | road/slope class; cursor U | `Unsupported` unless `STRASSE` | gap; cursor U |
| Footprint owner rule | one city, at least one owned tile | `footprint_owner_rule` | none |
| Territory claim by markets (PRODTYPE 33-35, radius 24) | `City::Place` | only the founding claims (`founding.cpp:476`); a market or a second Kontor claims nothing | **gap** (OQ-490 is now answered by SF §11.2) |
| Figure veto `0x10045080` | on every check | only in founding (`FigureInTheWay`); not in Build | **gap** |
| UI blocked-tile test (island `+0x64`) | footprint and fields | not modelled | gap (OQ-552 note) |
| Affordability | click and apply; preview ignores it; the ghost blinks | sim: `CannotAfford`; window: the verdict says "would be refused" as text; the ghost is always drawn | **gap**: no blink, no prefix display |
| Refused preview | yellow empty rectangle | the ghost sprites are drawn anyway (`make_preview`); the verdict checks only bounds, occupancy and cost, not the placement rule or territory (`dev_feedback.cpp:350`) | **gap** |
| Multi-placement of buildings (Default drag, at most 64) | yes | "a drag places a road; the selected type is not one" | **gap** |
| Random house variants (`+0x32`) and CTRL | yes | none | gap (OQ-546) |
| Fertility bar | `0x10047940` | none | gap (PB) |
| Demolish | own territory only, rectangle, at most 512 | `DevInput` demolish; sim `NoBuildingAtTile` | ownership and area cap not checked against the original (L) |
| Cancel, ESC, right click, pickaxe, cancel-X | P | implemented (UI-PLACE-1/2) | orange cell frame missing (OQ-627) |
| Upgrade rewrite | unvalidated | validated (contract A4) | intended |
| Wire-byte trust | apply trusts the client's height | sim recomputes | intended (A1) |

---

### 8. Open questions

Existing ones this note touches:

- **OQ-043** (drag continues after a failure): also note that the client sends only the prefix the cursor accepted (`0x1004af90`).
- **OQ-052**: open classes are the crop objects, walls and decorative types.
- **OQ-126.1**: player option 7, the cost waiver.
- **OQ-490**: the territory radius is answered by SF §11.2 (action radius 24, neutral-only painting). Proposed: close it and link here.
- **OQ-520**: the Kontor's facing.
- **OQ-546**: CTRL. The code path is `0x10047e80`, and event flag 8 is the likely CTRL bit.
- **OQ-552**: see OQ-728.
- **OQ-566**: the demolish cursor.
- **OQ-627**: the orange frame on the armed cell.
- **OQ-700**: the cursor over chrome.
- **OQ-T15-PLACEREFUSE**: no road adjacency test was found in any check, so a building beside an unconnected road is placed (L).
- RP **U-R1** (now located at `0x1007cc40`) and **U-R2** (still U).

Proposed new questions:

- **OQ-728.** Does the build menu arm `tCursorAnlageBergwerk` (kind `0xe`) for `MINE` and `STEINBRUCH`? If it does, a player mine can only stand on a site record whose mask holds its ore bit (`0x10046300`). That is a UI-level deposit restriction OQ-552 did not consider, because OQ-552 read the Default cursor `0x10047170`. Next: the View.dll cell callback (from `0x1002c260`) and the source of the 6-byte records (island `+0x24`, object slot `+0xc`).
- **OQ-729.** Which kind arms which cell: the per-cell rule in View.dll (PRODTYPE, flag class, or a table). Also which cursor places bridges, ramps and piers.
- **OQ-730.** The writer of the draw-context byte `+0x140c` (the blink phase) and its period. Not in AnnoWelt, AnnoFrame, AnnoGame, View, AnnoScene or AnnoDisplay by direct offset; likely a struct copy or an indexed store.
- **OQ-731.** The neighbour table of the road/wall autotiler `0x1007cc40` (ids 2500-2537, 2672/2673, PRODTYPE `0x181`, flag `0x800`).
- **OQ-732.** The meaning of event codes 1, 2 and 4 and of flag bit 8 in the cursor input handlers (L: press, release, right click, CTRL). One Wine capture would settle it.
- **OQ-733.** The dry-tile swap in the field commit: what type slot `+0xf8(3)` returns (dry variant), and whether a tree on a dry tile is really dropped (id 0, `0x1004ab61`-`0x1004ab68`).
- **OQ-734.** Whether a multi-record Default drag can overspend: each record is checked against the live stock at release, before the earlier records are applied.

### 9. GOG against HE

| Item | Result | Tag |
|---|---|---|
| Placement fields of `Anlagen.dat` (PRODTYPE, footprint, flag word, action radius, cost, HIGHMAP) | HE `Anlagen.addon` equals GOG `Anlagen.dat` for all 921 types. HE `Anlagen.dat` (909 types) differs in 7 (ruins at 510-513, 566/567, 2229). HE `Anlagen.classic`: 56 differ, for example field gold cost 0 instead of 5 | P (`cache/bref/anlcmp.py`) |
| Cursor classes | the same 24 `tCursor*` names in HE `AnnoFrame.dll` and `AnnoFrameAddOn.dll` | P |
| Code | HE modules are x64: no byte compare. The rules above are GOG; the HE recording's blinking ghost agrees with the GOG mechanism (section 5) | L |

Scripts and listings for this part are in `cache/bref/`: `vt.py`, `sweep.py`, `sweep2.py`, `anl.py`, `anlcmp.py`, the `d*.txt` listings and `w*.txt`.


### 10. Roads (UI-ROADS-1 evidence), over-building and selection (BUILDINGS-REF-2, 2026-11-19)

Research for the owner's test `ui_10.mp4` (findings 2, 3, 7, 8, 9, 10, 11). Read-only on the original (GOG 2.0.0.5
`AnnoFrame.dll`, `AnnoWelt.dll`, static disassembly, image base `0x10000000`; HE by the owner's pixels only). The
rebuild was run through scripted headless viewer sessions (`tools/test/viewer_wiring.py`, CPU renderer, the
existing `anno1503.exe`); scripts and logs are in `cache/bref2/` (git-ignored). Tags as above, plus **S** = a
scripted rebuild session, **H** = HE pixels from the owner's recording.

#### 10.1 Roads (UI-ROADS-1 evidence)

**Answer in ten lines.**

1. **Drag (D `AnnoFrame 0x1004b290`).** Press stores the start tile and shows a one-tile preview; every move recomputes
   the path from the start tile to the pointer; release commits it; the type **stays armed**. The cursor does nothing
   while no type is armed (`[B+0x44]` = 0) or when the pointer is not over a map tile (`0x10041290`).
2. **Path (D `0x1004b392`-`0x1004b51f`).** `dx`, `dy` from start to pointer. The **longer axis runs first**
   (`|dx| > |dy|`: x first; `|dx| <= |dy|`, so also on a tie: **y first**), for `max(|dx|,|dy|)` steps, then the other
   axis for the rest: an L. Total steps `|dx|+|dy|`, capped at `0x27` = 39 steps = **40 tiles**; past the cap the
   path is cut and does not reach the pointer. A tile is a 10-byte record `{id u16, x u16, y u16, rotation byte,
   byte, height u16}`; `id` is the armed type, the rotation is the cursor's.
3. **Variant (D `0x1004af50`, slot `+8`).** One random byte per tile (`0x100785ba`, CRT `rand`) is stored at
   `B+0x1f0+i`; the record id is `slot +8(id, byte)` (`0x10044340`: `id + (rand & 0xff) % n * step`, `n` and `step`
   from type property `0x32`). The same bytes are reused at release, so the placed variants equal the previewed ones.
   Dirt road 2520 has 5 flat variants (2520-2524), cobblestone 2500 has 4 (2500-2503) (P).
4. **Per-tile check (D `0x1004af90`; the commit runs it too).** For each record in path order, stopping at the first
   failure (the commit takes the valid prefix): (a) the tile lies on an island (world slot `+0x8c`), the same island as
   the first record; (b) its owner nibble (island `+0xec`) is **not 15**: every tile must be territory, not neutral;
   (c) that city's owner (city slot `+0x1c`) is the cursor's player (`[B+0x5c]`) and all tiles belong to one city;
   (d) ids 2518/2519 are checked as 2500; (e) `City::Build` (city slot `+0xa0`) accepts the record; (f) the figure veto
   `0x10045080`; (g) **no downgrade**: an existing object whose type flag word has bit `0x100` (all `STRASSE`,
   `BRUECKE`, `RAMPE`) and whose id is in 2500-2515 (cobblestone family) refuses a record whose id is in 2520-2535
   (dirt family) (`0x1004b11b`-`0x1004b141`).
5. **Cost (D `0x1004b1ac`-`0x1004b278`).** The checked records go through the island reshaper (item 7) in a dry mode that
   writes nothing and returns the full list of records it would write (the path plus rewritten neighbours). For each
   returned record: if the object now at that tile and the returned id are **both** in the cobblestone family
   (2500-2515) or **both** in the dirt family (2520-2535), the tile is **free** (re-laying a road, or a neighbour
   re-shaped); otherwise the running sum is tested against the city stock and the player's gold (`0x10043d80`, add
   `0x10043d30`) and the loop stops at the first tile that does not fit. The preview (`0x2000000`) skips cost. Costs
   per tile are the types' own: dirt road 5 gold, cobblestone 10, no goods (P); bridges 2516-2519 are in neither family
   and always pay (grand bridge 2518: 500 gold plus its goods).
6. **Water and shore (D + P + H).** The per-tile check has **no terrain test of its own** (nothing between (a) and (g)
   reads a height). `City::Build` accepts the class-`0x4000` types (the flat straight pieces 2500-2503, 2520-2524,
   2530-2533, and the `0x4386` ramps/bridges) at any terrain (RP section 2, `0x10009199 jne`), so a straight piece can be built on a
   **sea tile that is territory**. The territory disc covers sea tiles inside the island rectangle (painter `0x10085840`
   works on the island's tile grid), which is what HE shows (road mode darkens only what is outside the range, and a road
   piece stands "in the water" near the shore, `ui_10` 11:04). The class-`0x1000` pieces (corner, T, cross, end, see item
   8) need `rec[8] >= 32` and a slope of at most `0x10` (RP section 2), so those do **not** go on water. Whether a
   re-shaped piece that would fall on water is refused at commit, and how the ghost shows it, is **U** (OQ-765).
7. **Reshaping (D, island slot `+0x80` = `AnnoWelt 0x1007cc40`, about 3,000 instructions).** Called with the record list
   (n records, a mode byte, flags: `0x80` write the ghost, `0x80000000` ghost, `0x20000000` blinking). For each record
   whose type has flag `0x800` or PRODTYPE `0x181`, or when the mode byte is set, it finds the four orthogonal
   neighbours (`0x1007e8b0`, first in the list, then in the world, by direction 0-3), classifies them (`0x1007ef60`:
   strips the dirt offset `0x14`, maps variants to the family base) and appends the neighbour records whose shape
   changes to the output list (`0x1007ece0`). Bridge pieces (2516-2519, 2536/2537) have their own pass that scans up to
   20 tiles along the axis (`0x1007d917`-`0x1007dc52`). Heights of re-shaped tiles are recomputed with the type slot
   `+0x68` (`0x1007e154`). Only the structure was read; the table in item 8 is from files.
8. **The shape table (P, from the shipped scenarios; D for the family arithmetic).** Dirt id = cobblestone id + 20
   (`0x1007efcf`, `0x1007f0f3`). Over 28,233 dirt and 13,967 cobblestone tiles of 50 shipped `.sze` files
   (`cache/bref2/auto.py`; neighbour mask N=1, E=2, S=4, W=8):

   | Piece | Cobblestone / dirt ids | Class | Rotation 0 / 1 / 2 / 3 : neighbour mask |
   |---|---|---|---|
   | straight (4 or 5 cosmetic variants) | 2500-2503 / 2520-2524 | `0x4186` | 10 (E+W) / 5 (N+S) / 10 / 5; a lone tile too |
   | corner | 2505 / 2525 | `0x1186` | 6 (E+S) / 3 (N+E) / 9 (N+W) / 12 (S+W) |
   | cross | 2506 / 2526 | `0x1186` | 15 |
   | T | 2507 / 2527 | `0x1186` | 14 (no N) / 7 (no W) / 11 (no S) / 13 (no E) |
   | end | 2508 / 2528 | `0x1186` | 2 (E) / 1 (N) / 8 (W) / 4 (S) |
   | slope junction | 2509 / 2529 | special (`0x9cd`, `0x9e1` compared at `0x1007e182`) | mixed masks |
   | ramps | 2510-2515 / 2530-2535 | `0x4386`, `0x4186` (low/high) | at slopes |
   | bridges | 2516-2519, 2536/2537 | `0x4386`, `0x1386` | over water, straight |

   The dominant (id, rotation, mask) cells hold 90-100 % of each id; the rest are hand-edited or AI-placed tiles. The menu
   arms only **2520** (dirt), **2500** (cobblestone) and **2518** (grand bridge), so every other id is the reshaper's
   output, and the corner/T/cross/end tiles are the only ones it puts in the slope class.
9. **Preview and draw (D `0x1004b7f0`, `0x1004b762`).** The ghost path is written into the island with flags
   `0x80000000`; the valid, affordable prefix normally, the rest with `0x20000000` (blinks, section 5). **No radius
   outline** (only the Default cursor draws it, PB section 4.1) and **no fertility bar**. When the preview has no valid
   record and no path (`[B-0x18] == 0` and `[B-0x14] == 0`) the draw `0x1004b7f0` draws the yellow empty rectangle
   `0x10041050` (`0xffff00`) on the hovered tile.
10. **Commit (D `0x1004b5c7`-`0x1004b69e`).** Release: the ids are re-picked from the stored random bytes, the check runs
    with `0x9000000` (`0x8000000` cost, `0x1000000` apply path), and the valid prefix (n+1 records) goes to the sink
    `[B+0x58]` slot `+0x54(9, records, n, city, 0)`: commit **kind 9**, which stops at the first record the apply
    refuses (OQ-043). The type stays armed; the start tile and the active flag are cleared; new random bytes are
    drawn. Event 4 (right button) clears the armed type and runs the cursor Reset (slot `+0x5c`): nothing armed.

**Where the darkening comes from (U).** HE darkens everything outside the Kontor/market range while a road is armed. No
instruction in `tCursorAnlageStrasse` (input `0x1004b290`, draw `0x1004b7f0`) does it; it is another layer (OQ-767).

**What the rebuild has, and the exact gaps (S + code).**

| Item | Original | Rebuild | Gap |
|---|---|---|---|
| A road is placed | click/drag, tile object, charge | sim: `BuildCmd` of a land road is `Applied`, 5 gold per tile (S). **Settled by `ROADS-0` (S + code):** `PlayRunner::collect_tile_objects` diffs `kRoadFlag` tile objects (`type, rotation, height, phase`) alongside `BuildingDelta` through `BuildingMailbox` -> `ScenePicture` -> `apply_island_buildings` -> `PROD2` sprite path; a placed road renders (rot 0/1) and reaches `Visibility::Visible` (`picture.tile_objects`), while redemolishing an empty tile stays `NoVisualChange` (`tile_object_picture_test`, `viewer_wiring_roads_picture_objects`) | **G1 settled by ROADS-0** (previously `NoVisualChange`; G2-G8 remain) |
| Drag path | L, long axis first, y first on ties, 40 tiles | `DevInput::line`: a straight line along the longer axis only (the other axis is dropped); cap = wire limit 8,190 tiles | G2 |
| Hold-and-follow preview | ghost path while held | no ghost: the one-tile footprint of the hover; a drag shows nothing until release | G3 |
| Territory per tile | every tile owner != 15, one city, own player | `footprint_owner_rule`: one city and **at least one** owned tile, per record (the building rule) | G4: a road may start in the neutral sea beyond the range |
| No downgrade / free re-lay | refuse dirt over cobblestone; same family free | none: re-laying pays again; dirt over cobblestone allowed | G5 |
| Autotiling | corner/T/cross/end pieces, neighbour rewrites | none: the commanded flat straight id for every tile, rotation by the tool only | G6 |
| Terrain | none for the flat piece; slope class for shape pieces | `road_require_land_level` is **off** by default (the original behaviour); the title's `unchecked: the placement rule` is the window's own text, not a refusal | none for flat; G6 |
| Radius outline | none | none for a road | none |
| Armed after placing | yes | yes | none |
| Darkening outside the range | HE yes | `territory_view` exists, not tied to road mode | G7 (OQ-767) |
| Bridges 2518, piers, ramps | bridge scan up to 20 tiles | `Unsupported` | G8 |

**Implementation plan, small packets** (ROADS-0 first, or nothing placed can be seen).

| Packet | Part | Size | Content |
|---|---|---|---|
| ROADS-0 PICTURE-OBJECTS-1 | app | **Settled (S + code)** | The picture takes **tile-object deltas** (`SceneTileObject` with `phase` and no hardcoded type limit; `type == 0` cleared-tile markers): `PlayRunner` diffs tracked `kRoadFlag` objects and cleared tick-0 scenery alongside `BuildingDelta`, `ScenePicture` -> `apply_island_buildings` draws roads from `(id, rotation, height, phase)` with the existing `PROD2` sprite path and suppresses cleared trees under placed/demolished footprints while preserving flag-`0x10` rocks. Verified (`tile_object_picture_test`, `viewer_wiring_roads_picture_objects`): placed roads reach `Visible`, not `NoVisualChange` |
| ROADS-1 SIM-ROADS-1 | sim | 4 h | Pure helpers: `road_path(start, end, cap = 40)` (L, y first on ties), the per-tile cursor check (own territory per tile, one city, same island, no downgrade), cost with same-family free; a run command whose apply stops at the first refusal (kind 9, OQ-043). The 40 cap and the no-downgrade rule are original defaults in the config. **Implemented (D + P + S, header extension tag 2 `road_rules`):** `road_path` (`AnnoFrame 0x1004b392`-`0x1004b51f`), `footprint_all_owned_rule`, `is_road_family`/`same_road_family`/`is_road_downgrade` (`Anlagen.dat` 2500-2537: dirt gold=5, cobblestone gold=10, `BRUECKE` 198 excluded and always pays), `check_road_figure_veto` (`0x10045080`), kind-9 prefix commit in `BuildCmd` (`0x1004b5c7`) |
| ROADS-2 SIM-ROADSHAPE-1 | sim | 4-6 h | The reshaper from the item 8 table: neighbour mask to `(id, rotation)`, neighbour rewrites, dirt = cobblestone + 20, shape pieces in the slope class; checked against the 42,000-tile corpus (agreement of at least 97 %). RE-ROADS-2 (3 h) decodes `0x1007cc40` for the residual cases (OQ-764) |
| ROADS-3 UI-ROADS-1 | app | 3-4 h | Press-hold-drag with the L path in the **menu-armed** tool (the owner's route: the menu arms 2520 through `DevTool::arm`), the live ghost of the whole path, the unaffordable tail blinking, the yellow empty rectangle on refusal, no radius outline, stay armed, the 40-tile cut. Right click disarms |
| ROADS-4 UI-RANGE-1 | app | 2 h | Darken the map outside the range while a road (and every territory-bound building) is armed (OQ-767 for what to copy) |
| ROADS-5 | sim + app | 5 h (later) | Bridges 2518/2536 and ramps; the 20-tile bridge scan (OQ-766) |

#### 10.2 What may be built over, and the `Occupied` refusals

**The rule (D, section 2 "Overlap" and part D section 1.1).** Every placement ends in the island predicate `0x1007c790`
and the type slot `+0x6c` (`0x10008920`). A new object may share a tile with an existing one only when:

| Existing object on the tile | Allowed? | Evidence |
|---|---|---|
| trees, raw objects, dry crops, stones, ruins: type flag word bit `0x20` (all `ROHSTOFF` 41 = `0x29`, `ROHSTWACHS`, `ROHSTDUERR`) | **yes, for every placed type** | D (SP section 4) + P: all 291 crop and tree types carry `0x29` |
| rocks `FELSEN` (`0x8018`), cliffs `KLIPPE`, rivers `FLUSS`, waterfalls (`0x12`), `EISLOCH` | **no** (bit `0x20` clear): `Occupied`; the bit-`0x10` coverage rule applies | P (flag words read from `Anlagen.dat`) |
| another building | no, except the in-place market/Kontor upgrade (same PRODTYPE, `word14 & 0x100`) | D |
| a road, ramp or pier piece | only by a road, ramp or pier (group `0xc1`); a bridge only by a bridge; walls over walls and roads | D (section 2 table) |
| a crop field | overlappable by anything (bit `0x20`) | D (FERTILITY-1a) |

**What happens to the overwritten object (D, part D section 1.1).** `0x10082040` writes the footprint's tile records
unconditionally: the object id word is replaced and **the tree is lost with no removal call**; only a placed type with
flag `0x10` (mines, scenery) keeps a bit-`0x10` object that has its anchor bit. Nothing is stored that could bring a
tree back: **demolish** (`RemoveAt` step R8, `0x10082c30` clear arm: id 0, walk code 2, mask 0) leaves **bare ground**.
One arm (the "replacement arm" from a catalogue type, chosen by the high nibble of the `island +0x18` grid) is unread
(road-placement U-R5, OQ-768); it is the only place a restoration could hide.

**What the owner saw in HE (H).** The trees under the **ghost** vanish while the building is armed and hovering (the
preview hides the objects under its footprint; consistent with the chunk draw copying the ghost tile instead of the real
tile, `0x1009e2b5`-`0x1009e306`); after the click, the trees reappear except under the building's footprint tiles. That
is exactly: the preview hides, the placement removes the footprint's trees, the rest is untouched.

**The rebuild (S + code).** The sim agrees with the original: `placement_rule` accepts flag-`0x20` objects
(`site::overlap_with_existing`), `create_building` writes `object := type id` on the footprint (stand-in, O-5), so the
trees under a placed building are removed in the sim and are not restored at demolish. In the **window** (`ROADS-0`
settled items 1 and 3, S + code): (1) and (3) `PlayRunner` tracks cleared tick-0 scenery anchors (`type = 0` suppression
markers, preserving flag-`0x10` rocks) into `ScenePicture` -> `apply_island_buildings`, so trees under newly placed
buildings/roads and direct tree demolishes disappear and demolished footprints show bare ground (`tile_object_picture_test`,
`viewer_wiring_roads_picture_objects`); (2) the hover ghost still does not hide the trees under its footprint (`UI-GHOST-2`).

**Which tiles blocked the sheep farm (S, P, H).**

- **Static data.** Every 3x3 neighbourhood of the seven tiles on island 1 (`N_256x256_0002_2B.scp`, origin 1048,440)
  is forest: `GELAENDE` `ROHSTOFF` types 69-81, 105-119, 190/191, 411-423 (all flag `0x29`), plus `ROHSTWACHS` 1, 78, 80.
  **No rock, cliff or river lies within four tiles of any of the seven tiles** (`cache/bref2/occ.py`, `occ2.py`). The
  accepted and the refused windows are the same kind of forest, so a tree can never be the reason for `Occupied`.
- **Fresh session (S).** In a fresh `lan5` session the four tiles the owner lists as refused, placed in his order with
  the sheep farm at rotation 2, are **all `Applied`** (cost 220 gold each). The next click at `(1181,657)` is `Refused
  (Occupied)`, because the farm at `(1180,656)` covers `1180-1182 x 656-658`; `(1172,646)` is `TerrainRefused` (a
  different reason). The title then reads `would be refused: occupied in the published picture`.
- **So `Occupied` is a building footprint** (`tile_has_building` over every building grid of the island, or an earlier
  record of the same command), and the owner's refusals came from buildings placed earlier in his session.
- **Where they were (H).** Frame `ui_10` 4:52 (command #27, `at 1181,661`): the yellow 3x3 rectangle lies just left of the
  Kontor, inside its 4x4 footprint: the warehouse sprite is smaller than its footprint, so the tiles look free (L; the
  16 tiles were not measured). Frame 4:40 (command #19, `at 1180,646`): the pointer stands on a drawn farm, a true
  refusal. The owner had issued 27 commands by then, so several farms and foresters existed.
- **Deviation from the original: none in the rule; the gap is feedback.** The window draws the ghost over any tile and
  marks nothing as occupied; the original shows a **yellow empty rectangle** when the preview is refused (section 5).
  **Required:** the ghost draws the refusal (yellow rectangle), and while the tool is armed the occupied footprints
  can be outlined (UI-GHOST-2).

**Decision table for the rebuild.**

| Existing | Place | Preview | Demolish |
|---|---|---|---|
| tree, field, dry crop (bit `0x20`) | allow; **remove** the tile object (the sim does; the picture must) | hide the objects under the footprint | bare ground; nothing returns |
| rock, cliff, river (no `0x20`) | refuse, `Occupied` | yellow rectangle | n/a |
| building | refuse (`Occupied`); in-place market/Kontor upgrade is the exception (O-4) | yellow rectangle | the building's removal |
| road under a house or farm | refuse (a building is not group `0xc1`) | yellow rectangle | n/a |
| house or farm under a road | refuse (group `0xc1` only over `0xc1` or flag `0x20`) | yellow rectangle | n/a |

#### 10.3 Selection and highlight

**Answer in eight lines.**

1. **Who handles the click (D).** With **nothing armed** the active cursor is `tCursorInfo` (vtable `0x10088b30`, input
   `0x100502b0`, draw `0x10050190`); with a building, road, wall, field, mine or demolish armed, the armed cursor takes the
   click. **A click with a building armed places** (or is refused), in the original and in HE: the owner selected in HE only
   after nothing was armed.
2. **The pick (D `0x10050030`, `0x1004f7e0`).** The cursor asks the view for a pick with a mask (`0x4000` for a move,
   `0xc0000` for a click; view slots `+0x90`/`+0x94`) and gets 16-byte hits whose top nibble is the class: `4` units, `2`
   a tile hit (x, y 14 bits each), `1` the terrain point, `5` and `6` further object hits. The classifier returns 2
   (units), 4 or 5 (tiles), 10 or 1. Release (event 2) with no rubber band acts on the result:
   - **units** (class 2): selected through the player's selection; a rubber band of at least 4 pixels selects several
     (the cursor draws a yellow rectangle `0xffff00` while dragging, `0x1004f430`);
   - **a tile hit**: the tile object's type is read (island slot `+0x84(x, y, 6)`), and
     * PRODTYPE group `0x21` (markets and Kontors, 33-64) opens the warehouse/market window (message `0x17` through the
       city's view slot);
     * PRODTYPE `0x103` (259, the gate `TOR`) toggles the gate (commit kind 6 with one of four messages `0x8b9`-`0x8bc`
       by the gate's state);
     * anything else goes on to the selection.
3. **Shift or ctrl** (flag `0x4000` of the event modifiers) adds to the selection (player slot `+0xcc`); otherwise the
   selection is replaced (slot `+0xd8`): `AnnoFrame 0x10059d00`/`0x10059d40` forward to `AnnoWelt 0x1001afc0`/`0x1001b090`.
4. **The selection object (D `AnnoWelt 0x1001ae10`, `0x1001afc0`).** The player keeps one selection object of a kind:
   `2` units, `3` unit group, `4` **one tile object**, `5` several tile objects. Kinds 4 and 5 hold the clicked tile and
   the footprint rectangle (`0x10019a60`: island slot `+0x84(x, y, 6)` for the object, slot `+0x90` for the rectangle).
   Setting the selection tells the view layer (`0x10077730`), which opens the object's panel (`docs/re/ui.md` sections 4
   and 5).
5. **What is selectable (D `0x10019740`, the kind-4 validity).** The tile's object type is read (island `+0x94`) and its
   PRODTYPE group `(p - 1) & ~0x1f` must be **at most `0xa0`**: production (1-32), market block (33-64), public (65-96),
   houses (97-128), military (129-160) and market stalls (161-192); a byte table (`0x100198d0`) maps the block to a list
   mask, the same as RemoveAt's. **Roads, walls, bridges, piers, plazas, fences, monuments, ornaments, crops, trees,
   rocks and rivers (PRODTYPE 193 and up) are rejected: not selectable.** The tile's owner must not be neutral
   (`0x100197eb`).
6. **The yellow highlight (U for GOG, H for HE).** HE draws the clicked building in yellow with its panel (`ui_10`
   7:14). The GOG draw of a selected **building** was not found: the only selection-time yellow in `tCursorInfo` is the
   rubber band for units (`0x1004f430`), and its draw `0x10050190` draws order markers (texture `0x659`). Where the
   building's mark is drawn (a chunk mask like the ghost's `+0x210`/`+0x218`, the scene, or the building's own draw)
   is OQ-770. Until then the rebuild copies HE's look: a yellow footprint outline.
7. **Trees and fields (D).** Not selectable (item 5). A click on a tree or field with nothing armed selects nothing and
   drops the previous selection (S: the rebuild agrees).
8. **The "not in the tool's cycle list" message (S + code).** `DevTool::arm(type)` fails for a type the tool does not
   place. Types **104** and **708** are crop objects (`f12_info_field_104`, `_708`: farm fields), which the menu can arm
   and the sim cannot place yet (gap 4). The message is the tool refusing the **arm**, not a failed selection.

**The rebuild (S + code).** Selection exists: with the developer tool **off** and nothing armed, a left click on a placed
building selects it (S: the title reads `selected Farm out=0.00t ...`), a click on a tree or empty ground deselects, ESC
deselects (`selection.hpp`, `PointerMode::Select`). With the tool **on** (the owner's session: armed from the menu) a
click on a building is a **build attempt** (S: `Refused (Occupied)`), as in the original. The owner's "nothing happens" is
therefore the armed state. Gaps:

| Item | Original | Rebuild | Gap |
|---|---|---|---|
| Yellow highlight of the selected building | HE yes (U in GOG) | title text only | S1 |
| Panel of the selected building | View panel by PRODTYPE | the farm shows the title only | S2 (UI-INFO) |
| Click on a market block or Kontor | opens the warehouse window | selects | S3 |
| Click on a gate | toggles it | gates are not placeable | later |
| Rubber band selects units and tile objects (kind 5) | yes | none | S4 |
| Select without disarming | a click places; to select, unarm (right click or ESC) | same | none; say so in the help |
| Selectable set | production..stalls only | the city's buildings | none |
| Message for field types | n/a | `not in the tool's cycle list` | wording: "not placeable yet" |

**Packets:** `UI-SELECT-1` (app, 3 h): the yellow footprint outline (HE pixels for colour and width), the footprint from
the sim's building record, one per selected building; ESC and right click as now. `UI-SELECT-2` (2 h): a market or
Kontor click opens the warehouse panel. The GOG highlight code is OQ-770.

#### 10.4 Open questions of this pass (OQ-764 to OQ-772)

- **OQ-764.** The exact road reshaper (`AnnoWelt 0x1007cc40`, helpers `0x1007e8b0`, `0x1007ece0`, `0x1007ef60`,
  `0x1007f960`, `0x10080610`, `0x1008bae0`): when a one-neighbour tile is an end piece and when it stays a straight piece
  (the corpus has both), which neighbour rewrites it emits, what the mode byte and flag `0x10` do. The table in 10.1 item
  8 is data-derived (P), not read.
- **OQ-765.** Whether the commit (kind 9 apply) re-checks a reshaped corner/T/cross/end piece in the slope class
  (`rec[8] >= 32`) on a water tile, and what the player sees (a straight run into water that stops at the first corner).
- **OQ-766.** Bridges: the pass that scans up to 20 tiles (`0x1007d917`-`0x1007dc52`), which cursor places 2516, 2517,
  2536 and 2537 (the menu arms only 2518), and the grand bridge's ramps.
- **OQ-767.** Who darkens the map outside the range while a road is armed (HE `ui_10` 10:48-11:13), and whether GOG does
  the same; the road cursor's own code does not.
- **OQ-768.** Demolish: the replacement arm of `0x10082c30` (grid `island +0x18`): does a demolished footprint get
  anything back (a tree, a ground tile)? Evidence so far: the clear arm, bare ground.
- **OQ-769.** The exact 16 tiles of the Kontor footprint relative to its drawn sprite in the owner's session (the
  `Occupied` refusals beside the Kontor); needs the owner's command log or a headless dump of the city's building
  footprints.
- **OQ-770.** Where the original draws the yellow highlight of a selected building, and the HE look (colour, width,
  footprint or sprite outline).
- **OQ-771.** The pick masks `0x4000` and `0xc0000` and what the view's pick returns for a click over a building versus
  the ground under it (`AnnoFrame 0x10050030`, view slot `+0x94`).
- **OQ-772.** The tile-object selection's further effects: kind 5 (several), the panel hand-off (`0x10077730`), and what
  `AnnoFrame 0x10050b90` (unit selection by pick) does for ships.

---

# Part C: entrance/exit arrows

## C — Entrance/exit arrows end to end (BUILDINGS-REF-1, part C, 2026-11-17)

Read-only research. GOG binaries (`game/app`, VAs), HE files and the owner's HE videos. No build, no test,
no game started. Work files: this folder (`s/` scripts, `img/` crops, `pv/` `mg/` `v9/` `h1/` frames).

Evidence tags: **D** = disassembly (module + VA), **P** = parsed original file or pixel measurement on an
original capture/recording, **L** = inference from D/P, not read directly. Confidence: high / medium / low.

### 0. Answer in five lines

1. **Map ghost (OQ-713): no arrows, in GOG and in HE.** The arrow parts exist only in the build/info panel
   picture. Static: the only producer of arrow parts (AnnoGame `0x1000bb60`, type slot `0xa8`) is consumed
   only by View `0x1001d000` (the info-picture composer). Pixels: a valid GOG market ghost and HE tanner and
   market ghosts carry no green arrows. The manual says the same: "Green arrows ... on the information box".
2. **Meaning:** the arrows are the type's **road-connection points** (figure 1573 is `Strassenanschluss01`,
   "road connection"): the footprint-edge places where the walkable region reachable from the type's
   `ENTRANCE` sub-cells leaves the footprint. They are drawn from stored `CONPO2` data, which no sim code
   reads. The sim works with `ENTRANCE` (type slot `0x98`) and the walk mask. Carriers and carts start and
   end at the `ENTRANCE` sub-cells and cross the footprint edge where the mask opens, which is where the
   arrows are.
3. **Placed buildings:** no arrows on the map. Their **info panel picture** has them, because every
   building picture goes through the same composer (`0x1001d760` → `0x1001d000` → slot `0xa8`
   unconditionally). HE `c6_farmpanel` shows a placed farm's panel with arrows. The arrows are neither
   entrance nor exit: they mark both. Help and manual call them "building entrances".
4. **OQ-711:** 8 directions, the `tHausFigur` constructor default (AnnoDisplay `0x10005cb0`: mask
   `+0x19 = 7`, shift `+0x1a = 1`; frames per direction `+0x18 = 1` from base `0x10005390`). Frame for
   arrow direction d = ((d<<2)>>1)&7 = 2d. This settles the rebuild's assumption.
5. **Road requirement:** nothing tests it at placement (L). Instead the "no road" status (code 3, figure
   1571) comes from a runtime path search between entrance points (`0x10071f40` / `0x10072390`). Ctrl while
   placing auto-aligns the entrance to a street (manual, help); that routine is not read (OQ-735).

### 1. The build-panel preview (known, re-checked)

| # | Fact | Evidence | Tag | Conf. |
|---|---|---|---|---|
| 1.1 | Arrow data = `ANLAGEN/OBJECTPOS` sub-record `CONPO2`, 3 bytes/arrow (x3, y3, side), count = size/3 → type `+0x6b`, records → `+0x22c` | AnnoGame `0x10009e3e..0x10009edf` | D | high |
| 1.2 | Legacy `CONPOS` (2 bytes/arrow) loader keeps x and drops y; the side byte is left 0 (it writes `[+1]` twice) | AnnoGame `0x10009d96..0x10009e39` | D | high |
| 1.3 | Readers of `+0x22c`/`+0x6b` in AnnoGame: the loaders, copy/free `0x1000b438`/`0x1000b44b`, init `0x1000b5ca`, the editor recompute `0x1000d62e..0x1000d688`, and the part builder `0x1000bb66..0x1000bc5c`. **Nothing else** | `s/scan.py` regex `\+ 0x22c\]|\+ 0x6b\]` over AnnoGame | D | high |
| 1.4 | Part builder = type slot `0xa8` = `0x1000bb60`, in the two type vtables `0x10021a88` (`+0xa8` at `0x10021b30`) and `0x10021bbc` (`0x10021c64`). No direct call. The only place figure `0x625` (1573) is pushed in any GOG module | `0x1000bb82 push 0x625`; pointer search | D | high |
| 1.5 | The 0xa0/0xa4/0xa8 part trio, called as `slot(&out, rot, buf)` with the result indexing 20-byte (`a8`) or 16-byte (`a0`/`a4`) parts: **only** View `0x1001d0a4`/`0x1001d251`/`0x1001d3f7`, all inside `0x1001d000` | linear sweep of all code sections of AnnoFrame, AnnoWelt, AnnoDisplay, AnnoScene, View for `call [reg+0xa8]` followed by `lea ecx,[eax+eax*4]`: one hit (`s/ctx.py`) | D (sweep) | high |
| 1.6 | `0x1001d000` calls slot `0xa8` with no flag test, so every picture it builds carries the type's arrows | View `0x1001d3d7..0x1001d3f7` | D | high |
| 1.7 | `0x1001d000` is called only by `0x1001d760` (`0x1001d7cc`). `0x1001d760` has 9 callers: `0x10030326` (build panel, args 1,0,1,0) and `0x1003e475`, `0x1004bbe5`, `0x1004d145`, `0x1004eae5`, `0x10051785`, `0x10052df5`, `0x1006e88d`, `0x10074b03`. These are other info gadgets, each passing its own gadget ids (6/7, 5/6, 4/5, 6/7, 8/9, ...) | `s/ctx.py View.dll "call 0x1001d760"` | D | high (callers); which panel each is: L |
| 1.8 | Placement, rotation, sprite, frame and canvas as in UI-INFO-2b (`docs/re/ui.md` §2.5 on `task/UI-INFO-2b` `1a6f5ef5`): corner + third(x3) + 0.167 + half-tile side offset; direction (turn + side) & 3; figure 1573; frame 2d | AnnoGame `0x1000bbc8..0x1000bdc0`; tables `0x100217c4`, `0x10021a84`, `0x10021828` | D | high |

#### HE preview frames vs `CONPO2` (owner's `ui_9.mp4`, 2560x1440, 80 fps)

Crop: panel box (2340, 350) 200x200 at 2 fps, 2:58-5:02 (`pv/`, sheets `pv_sheet_0..2.png`). "Arrows" =
the `CONPO2` count from `Anlagen.dat` (UI-INFO-2b reader `anlagen_entrance_arrows`, `s/ui_layout_2b.py`).

| Building (type) | Shown in panel | Owner's count | CONPO2 (x3, y3, side) | ENTRANCE points (sixths) | Match | Conf. |
|---|---|---|---|---|---|---|
| Forsthaus 801 | 3:02-3:24, turned 3:07-3:11 | 3 | (3,0,3) (4,8,1) (8,4,2): **3** | (11,9): 1 | yes | high |
| Schaffarm 807 | 3:26-3:33, turned 3:29-3:32 | 3 | (3,0,3) (4,8,1) (8,4,2): **3** | 3 points | yes | high |
| Jagdhütte 805 | 3:36-3:44, turned 3:41-3:43 | 4 | (4,0,3) (0,3,0) (3,8,1) (8,3,2): **4** | 3 points | yes | high |
| Kleine Farm 809 | 3:47-3:51 | 4 | (2,0,3) (0,5,0) (4,8,1) (8,3,2): **4** | 4 points | yes | high |
| Fischerhütte 810 | 3:53-3:59 | 4 | (3,0,3) (0,3,0) (4,11,1) (8,3,2): **4** | 2 points | yes | high |
| Gerberei 1023 | 4:01-4:30 | 4 | (7,0,3) (0,6,0) (3,8,1) (8,3,2): **4** | (15,5): 1 | yes | high |
| Walfänger 821 | 4:32-4:52 | ("1 front + 2 back", spoken 4:31-4:50) | (4,0,3) (6,0,3) (9,11,1) (11,5,2): **4** | 2 points | 4 visible at 4:36 (one by the tower, three low right) | medium |
| Salzmine 1203 | 4:54-4:55 | ("that's the mine": 1 + 2) | (0,10,0) (3,11,1) (8,10,2): **3** | (9,19): 1 | 3 visible at 4:54.5 | medium |

The owner's "mine, 1 front + 2 back" fits the salt mine's 3 arrows (1203). The whaler (821) has 4. The
speech runs over both panels, so which one he counted is not certain (OQ-738). On every type the arrow
count differs from the ENTRANCE count: arrows and entrances are different data (§3).

### 2. The map ghost (OQ-713)

#### 2.1 What the GOG cursor draws

`tCursorAnlageDefault` (AnnoFrame RTTI; the vtables come from `s/vt.py`). Its virtual base's vtable
`0x1008834c` has slot `0x70` = thunk `0x100489b0` → `0x10048450`. The slots `0x5c` `0x10047010`, `0x60`
`0x10047e40` and `0x6c` `0x10047470` are its other own methods.

| # | Step | Evidence | Tag | Conf. |
|---|---|---|---|---|
| 2.1 | `0x10048450(dt, arg)`: adds dt to `[this-8]`. With byte `[this-0x14]` == 0 → `0x10041050(rect, 0xffff00, dt, arg)` only: the **yellow footprint outline** (the refused ghost) | `0x10048464..0x1004848e` | D | high (control flow); "flag = valid" L from pixels |
| 2.2 | Otherwise: type = `[this-0x2ec]->slot 0x70(id)`, radius = `type->slot 0x78`, `0x100443c0(rect, radius)` = the **action-radius outline** (diamond rows, 24-byte vertices, colour `0xffff00`), then `0x10047940(dt, arg)` | `0x10048491..0x10048513` | D | high |
| 2.3 | `0x100443c0` is the radius outline, **not** the ghost (the task's premise "0x100443c0 draws the ghost" is wrong): it builds a polyline of tile corners from the radius and hands it to a draw list (`slot 0xa4`). In GOG `p1_c_armed_1301` this is the yellow zigzag around the valid market ghost | `0x100443c0..0x1004488c`; pixels | D + P | high |
| 2.4 | `0x10047940`: only for a type whose input 0 (`slot 0xb0(0)`) is goods 0x38..0x4b; walks the radius rows and marks tiles (a field/crop overlay). No arrows. Not read further | `0x1004797f..0x100479ab` | D | medium |
| 2.5 | No `call [reg+0xa8]`, no `0x625`, no read of the arrow tables `0x100217c4`/`0x10021828`/`0x10021a84` anywhere in the cursor code `0x10041000..0x1004d000` or in any module other than AnnoGame (all modules swept) | `s/scan.py`, `cur_range.txt` | D (sweep) | high |
| 2.6 | The ghost building itself is not drawn through the arrow builder. Its own draw path (which object, which tint) was not traced to the end | — | L | — |

#### 2.2 Pixels

| Source | Time / file | Ghost | Arrows? | Measure | Conf. |
|---|---|---|---|---|---|
| GOG Wine | `anno1503-UI-PLACE-1/cache/orig/ui-place-1/d96-20261115-174705/shots/p1_c_armed_1301.png`, ghost at about (225, 210) | market 1301, **valid** (drawn building + yellow radius zigzag) | **no** | 4 "arrow green" px (g>150, r,b<90) in (160..290, 160..250); crop `img/gog_p1c_1301_crop.png` | high |
| GOG Wine | `anno1503-UI-INFO-1/cache/orig/r2_place_market_1301.png`, `r2_place_forester_801.png` | refused ghost over the Kontor: yellow footprint diamond only | no | crops `img/r2_place_*_crop.png` | high (invalid ghost only) |
| HE `ui_9.mp4` | 4:03-4:31 (tanner 1023 on the map, cursor at about (1760, 500) in 2560x1440) | tanner ghost, **blinking** (unaffordable, owner 4:21) | **no** | crop (1560, 340) 400x320 at 4 fps (`mg/`, `img/he_tanner_mapghost.png`): green count 8 with or without the ghost (background), against about 70 in the 135-px preview box with its 4 arrows | high |
| HE `he_01.mp4` | 4:08 and 4:11 (market 1301 ghost, zoomed out; `h1/`, `img/he01_market_ghost_408.png`) | market ghost (cursor on it), opaque | **no** (only the flag's teal pennant) | visual, 3x crop | medium (small zoom) |
| Manual (GOG `Manual.pdf`, Construction Menu) | "Green arrows radiating from the building on the information box indicate building entrances. ... press and hold CTRL" | — | arrows on the info box | text | high |
| Help `help/Strassenanschluss.htm` | "During construction, a building entrance is marked by arrows ... keep the [Ctrl] key pressed" | — | consistent with the panel picture, which is shown during construction | text | high |

**OQ-713 answer:** the map ghost draws **no** entrance arrows, in any rotation, valid or refused, in GOG
(D + P) and HE (P). The OQ text ("he_01 3:32-4:21 shows green arrows on the placement ghost that turn with
it") describes the panel picture, not the map ghost. Side findings for part B: the HE radius outline is
**green** where GOG's is yellow `0xffff00`. The HE ghost **blinks** when unaffordable: on 0.75 s, off
0.5 s, measured at 4 fps over 4:05-4:30 (`########..###...##...###...`).

### 3. What the arrows mean in the world

| # | Fact | Evidence | Tag | Conf. |
|---|---|---|---|---|
| 3.1 | The figure is named `Strassenanschluss01`, "road connection" | FigurenHeader 1573 | P | high |
| 3.2 | `CONPO2` is **derived data**. The editor routine `0x1000d2a0`, run only from the WALKMASK/ENTRANCE property setters `0x1000eace`/`0x1000ef5d`, reads ENTRANCE (`+0x1a8`), runs per-side edge scans (`0x1000cf80`, `0x1000d000`, `0x1000d080`, `0x1000d190`) over the walk mask, and rewrites `+0x22c`/`+0x6b`. It reproduces 181 of 185 stored records (UI-INFO-2b) | AnnoGame `0x1000d2a0..0x1000d6d9` | D | high (calls); per-side scan semantics L |
| 3.3 | The game never reads `CONPO2` outside the picture builder (1.3, 1.5). **No sim, transport or placement code uses the arrows** | sweep | D | high |
| 3.4 | The sim's connection data is `ENTRANCE` (1-4 points of two f32 per type, on sub-cell centres k/6 with k odd; 326 of 921 types). Type slot `0x98(rotation, buf)` = AnnoGame `0x100084c0` returns them rotated, 12-byte records | `docs/re/transport.md` §1.6/§3; UI-HARVEST-ICON-1 §11.6 | D | high |
| 3.5 | Carriers and market carts **start** at the home's entrance points when they stand inside its footprint, and their **goals** are each candidate's entrance points (building slot `0xf8` = AnnoWelt `0x10072be0`, at most 256; none while the building's flag bits 4/5 are set) | `transport.md` §1.3, §3; `state.md` §5.9 | D | high |
| 3.6 | A footprint is entered and left **through its own WALKMASK cells** (code 4, costs 40/15), not through any edge tile. So the edge cells where an entrance's walkable region meets the border are the only places a path crosses it. Those cells are the CONPO2 arrows (3.2) | `state.md` §5.9 item 7; 3.2 | D + L | medium-high |
| 3.7 | The market cart (layer 3) admits roads and building cells but not open ground (code 2: admission 0). So a producer whose goods the cart collects needs a road from an arrow to the market's road net. Carriers (layer 0) also walk over open ground, so a farm inside a workshop's radius needs no road | OQ-103 update T30b.8, `transport.md` §3.4.13 | D | high |
| 3.8 | "No road" status (code 3, figure 1571 `KeineStrasse`). `0x10071f40`, raised when code 1 is due: the building's own carrier (`type.slot+0xb4`) searches **from its entrance points** (`slot+0x98`) to the entrance goals (`slot+0xf8`) of candidates in its radius (markets PRODTYPE 33-64 or consumers of the input); no path → code 3. `0x10072390`, raised when code 2 is due: the candidates within ±24 tiles, nearest first, search **to this building's entrance goals** with their own figure type; none reaches it → code 3. No candidate at all → the plain code 1/2 stays | AnnoWelt `0x10071f40..0x100722b9`, `0x10072390..0x1007285a`, callback `0x100722d0` (UI-HARVEST-ICON-1 `c8da3ff2`, `docs/re/ui.md` §11.6) | D | high (control flow); self-match OQ-720 |
| 3.9 | **Placement does not require the connection** (L): no reader of CONPO2 or of the road layer is known on the build path, the help describes the missing-connection symbol on a built building, and code 3 is raised only from the status tick (`0x10072f24..0x10072f83`). Whether the build is ever refused beside a road stays OQ-T15-PLACEREFUSE | help `Strassenanschluss.htm`; 3.8; 1.3 | L | medium |
| 3.10 | Ctrl while placing auto-aligns the building (turns it) so its entrance meets an existing street | Manual, help | P (text) | high (that it exists); routine not read (OQ-735) |
| 3.11 | Spawn: a figure is created at a reachable entrance point; `0x10072b50` (which point) was not read | `transport.md` §1 line 57 | D (call) / L (rule) | medium |

**Entrance or exit?** Both. The same `ENTRANCE` sub-cells are the starts of a building's own figures and
the goals of every figure coming to it (3.5). The arrows mark where those paths cross the footprint edge
in either direction. The original's own words (manual "building entrances", help "building entrance is
marked by arrows", figure "road connection") favour **entrance / road connection**. The arrow sprite
points **outward** from its side (UI-INFO-2b: frame 0 up-right for side 0, -x).

### 4. Do placed buildings show arrows?

| Where | Arrows? | Evidence | Tag | Conf. |
|---|---|---|---|---|
| On the map, placed building | **no** | only producer 1.4, only consumer 1.5. GOG/HE map shots of placed producers (e.g. HE `fisher-he-1 .../a4_ghost_on_site.png` top-left fisher hut; GOG `p1_c` Kontor) show none | D + P | high |
| Info panel picture of a placed building | **yes** | `0x1001d760` has 8 callers besides the build panel (1.7), all through the unconditional `0x1001d000`. HE `he_01_work/c6_farmpanel.png` (placed small farm's panel) shows its arrows where CONPO2 puts them (UI-INFO-2b report) | D + P | high (HE); which GOG gadget uses which caller: L |
| Map ghost | **no** | §2 | D + P | high |

### 5. OQ-711 — the arrow figure's direction count

| # | Fact | Evidence | Tag | Conf. |
|---|---|---|---|---|
| 5.1 | `tHausFigur` (vtable `0x1003b734`, slot `0x6c` = frame selector `0x100055b0`) constructor `0x10005cb0`: `+0x19` (direction mask) = 7, `+0x1a` (shift) = 1. Base `tHausBaum` ctor `0x10005390`: `+0x18` (frames per direction) = 1, mask 0 | AnnoDisplay `0x10005cf3..0x10005cff`, `0x1000539d..0x100053a6`; RTTI via `s/vt.py` logic | D | high |
| 5.2 | Property setter `0x10005220`: id `0x12f` = direction count n → mask n-1, shift 0 if n-1 ≥ 15, 1 if 7 ≤ n-1 < 15, else 2 (case `0x100052c7`); id `0x136` = frames per direction (`0x100052a3`); id `0x13f` = mask directly, 1 read as 3 (`0x1000524b`). Getter `0x10005320` returns mask+1 for `0x12f` | AnnoDisplay | D | high |
| 5.3 | Writers of `0x13f`/`0x136`: AnnoDisplay `0x10003291` (Anlagen `BODY2` byte & 0xf → `0x13f`), `0x10003335` (`ANIM` → `0x136`/`0x138`). These are HAEUSER sprite sets, not FigurenHeader figures. AnnoGame `0x10003bda`/`0x10003d63` only **read** `0x12f` (slot `0x48`, default 8) and `0x136` (default 1) | sweep for `push 0x12f/0x136/0x13f` | D | high |
| 5.4 | So figure 1573 (a `tHausFigur`) keeps the constructor default: **8 directions**, 1 frame each; frame = ((dir16 >> 1) & 7) = 2d for arrow direction d = (turn + side) & 3 | 5.1-5.3; matches both stall shots (UI-INFO-2b, 0 px) | D + L (no override found for 1573) | high |

OQ-711 can be resolved: 8 directions by the class default, no data field. 4 of the 8 frames are used.

### 6. What the rebuild has, and the gaps

State checked on `main` `b9b8df23` and on the two branches.

| Area | Rebuild | Original | Gap |
|---|---|---|---|
| Build-panel arrows | `task/UI-INFO-2b` `1a6f5ef5`: `info_preview::compose`, CONPO2 reader, figure 1573 as layout data. **Not on main** (not an ancestor of `b9b8df23`; `src/app/src/info_preview.cpp` absent) | §1 | G1: land UI-INFO-2b |
| Placed-building info panel picture | not known to use `compose` (the branch covers the build panel) | arrows via the same composer (§4) | G2: the selected-building panel picture should get the same arrows (check against `c6_farmpanel`) |
| Map ghost arrows | none (`viewer.cpp`) | none (§2) | **no gap**. OQ-713 can close as "no arrows on the ghost". Do not add them |
| Ghost overlays | — | refused: yellow footprint outline only (`0x10041050`). Valid: building + action-radius outline (`0x100443c0`, GOG yellow `0xffff00`, HE green) + a crop-input field overlay (`0x10047940`). HE: blinking ghost when unaffordable | G3: belongs to part B (ghost); listed for completeness |
| Arrow direction count | assumed 8 (frame 2d) | 8, class default (§5) | none; update OQ-711 |
| Entrance data in the sim | `BuildingType::entrances` from `ENTRANCE` (`types.cpp:199-217`, AnnoGame `0x10009a48-0x10009aa8`); rotated `entrance_position` (`driver.cpp:211`) | same (3.4) | none |
| Transport starts/goals | `transport.cpp:491` starts = the home's entrance points, `:499` goals = the candidates' entrance points, `:645` return goals = the home's entrances (`entrance_walk_points`). `walk_search.hpp:69` exempts the entrance tile for residents | 3.5 | none known (T33 rule). The goal gate on flag bits 4/5 is described in `state.md` §5.9 |
| Spawn position | first entrance point (`driver.cpp:199-206`; fisher `driver.cpp:561-563`, "no reachable-entrance retry") | a **reachable** entrance (`0x10072b50`, unread) | G4: read `0x10072b50`; the rebuild may spawn at an unreachable entrance |
| "No road" status code 3 | **not modelled** (`production.hpp:112-116`: "Not modelled: ... checks (0x10071f40, 0x10072390) whose failure turns the message into code 3"). `Status` has 0-2. `transport.cpp:440` `home_no_road`/`goal_no_road` are 4-neighbour **diagnostics**, not the rule | §3.8 | G5: implement the two searches (entrance starts, entrance goals, radius / ±24-tile candidates); depends on OQ-720 (self-match) |
| "No road" icon (1571) over the building | not drawn | status figure (UI-HARVEST-ICON-1 §11) | G6: UI after G5 |
| Placement road check | none (`placement.cpp` has no entrance or road-adjacency test) | none known (3.9) | none (pending OQ-T15-PLACEREFUSE) |
| Ctrl auto-align | none | exists (manual, help) | G7: RE the Ctrl path in the cursor (OQ-735), then implement (input + rotation search) |
| CONPO2 in the sim registry | not loaded (UI-only data in the app) | the sim never reads it | none |
| Odd CONPO2 data | branch skips side > 3 (types 261, 1571) and ignores `CONPOS` (1542) | original: side > 3 reads past the side table, `CONPOS` drops y and the side | low. Keep as documented |

### 7. Open questions

- **OQ-713 → resolve:** no arrows on the map ghost (§2), GOG D + P, HE P. Rebuild unchanged.
- **OQ-711 → resolve:** 8 directions, `tHausFigur` constructor default (§5).
- **OQ-712** unchanged (other sprite sets of the picture).
- **OQ-720** (self-match in `0x10071f40`) is the precondition of G5.
- **OQ-T15-PLACEREFUSE** stays open (3.9: L that the build path does not test roads).
- **OQ-735:** the Ctrl auto-align. Which routine turns the cursor building to meet a street, and what does
  it test: ENTRANCE plus walk mask, or CONPO2? CONPO2 is ruled out (1.3), so it must use ENTRANCE/WALKMASK
  or the road layer. Resolve: find the key-state path in the cursor's input handler (`tCursorAnlageDefault`
  slots `0x5c` `0x10047010`, `0x60` `0x10047e40`, `0x6c` `0x10047470`); no `GetKeyState` import use in
  AnnoFrame/AnnoWelt, so the flag arrives with the input message.
- **OQ-736:** which `0x1001d760` caller is which info gadget (placed production panel, market, house, ...)
  and whether any of them hides the arrows by its own drawing. Resolve: RTTI of the classes owning
  `0x1003e475` ... `0x10074b03`, plus a GOG capture of a placed producer's panel.
- **OQ-737:** the ghost's own draw path (the object that renders the building at the cursor, its tint, and
  the blink when unaffordable in HE; GOG not checked). Part B.
- **OQ-738:** the owner's "mine 1 front + 2 back" (`ui_9` 4:31-4:50) was spoken over the whaler panel
  (821, 4 arrows) and just before the salt mine panel (1203, 3 arrows). Resolve: confirm with the owner;
  the data fit 1203.
- **OQ-739:** the semantics of the four edge scans in `0x1000d2a0` (`0x1000cf80`/`d000`/`d080`/`d190`),
  for an exact statement "arrow = border cell of the entrance's walkable region". Only needed if the
  rebuild ever regenerates arrows; the game uses the stored data.

### 8. Scripts and files (this folder)

- `s/vt.py <dll> <class>`: MSVC6 RTTI → vtables. `s/scan.py <dll> <regex>`: linear sweep of the code
  sections. `s/ctx.py`/`s/ctxp.py`: context around matches. `s/green.py`: arrow-green pixel count per frame.
  `s/ui_layout_2b.py`: copy of `tools/ui_layout.py` from `task/UI-INFO-2b` (CONPO2 reader).
- Disassembly dumps: `f48450.txt`, `f443c0.txt`, `f47940.txt`, `cur_range.txt` (AnnoFrame
  `0x10041000..0x1004d000`), `d2a0.txt`, `ctx_*.txt`, `ctxa_*.txt`, `ctx_d760.txt`.
- Frames: `v9/` (ui_9 1 fps, 1280 wide, t_n = 177 + n s), `pv/` (panel box, 2 fps, p_k = 178 + (k-1)/2 s),
  `mg/` (tanner map ghost, 4 fps from 4:03), `h1/` (he_01 4:08, 4:11). Sheets: `pv_sheet_0..2.png`,
  `img/*.png`.
- HE binaries: the arrow builder's counterpart is in `AnnoGameAddOn.dll` (`push 0x625` at `0x1000580e`,
  32-bit). Not read further; HE evidence here is pixels only.


---

# Part D: what placing does to the world

## D — What placing does to the world (BUILDINGS-REF-1, part D)

Status: read-only research, 2026-11-17. GOG 2.0.0.5 binaries (`game/app`), image base `0x10000000` for every module; module named where it is not `AnnoWelt.dll`. Nothing was run in the original.

**Evidence tags.** **D** = instructions read (by the earlier notes cited, or newly in this pass, marked *new*); **P** = parsed file data (`Anlagen.dat`, `Scenes/**`); **L** = likely: inferred from call shape, a partial read, a name, the manual or observation, not proven. **U** = unread.

**New reads of this pass** (listings in `cache/bref/dis/`, helper scripts `cache/bref/tbl.py`, `names.py`, `types_sum.py`):
`City::Place 0x100279a0` whole body (`place.txt`), `RemoveAt 0x10028fd0` whole body (`removeat.txt`) with its three jump tables, the demolish apply `AnnoFrame 0x10058e90` (`demolish_apply.txt`), the well hooks `0x1002cf60` / `0x1002d090`, building slot `+0x108` = `0x10071d10`, the post-commit helper `AnnoFrame 0x10046020`, and a P summary of all 921 `Anlagen.dat` types by PRODTYPE (`types_sum.py`).

Sources reused (not repeated here): `docs/re/ship-placement.md` (build path, `0x1007c790`, compat table, classes), `docs/re/ship-founding.md` §11-§16 (claim, city creation, tile writes, discovery, reveal), `docs/sim/city-creation.md`, `docs/re/road-placement.md`, `docs/re/transport.md` §3.4.17 (tile record), `docs/re/production.md`, `docs/re/population.md`, `docs/re/fisher.md` §2, `docs/re/forestry.md`, `docs/sim/state.md` §7.3/§8.4/§12, `docs/sim/salt-extraction.md` §11-§22, `docs/sim/quarry-bricks.md`, `docs/re/complogic.md` §11/T54; branches `task/UI-STATUS-1` 5e5a51de (annowelt.md "Island discovery word", ui.md 5.6), `task/ISLAND-CROPS-1` (insel.md `ANBAU`), `task/FERTILITY-1a` c77b5703 (ship-placement.md §3a single-quad class), `task/UI-PRODBAR-1` (production.md §4.2b, HEAD `[0x28]` crop object), `task/UI-HARVEST-ICON-1` c8da3ff2 (ui.md §11 status icons, OQ-720).

---

### 0. The one path every placement takes

Every player placement — house, workshop, Kontor, road, wall, field, monument — goes through the same three functions. The class differences are branches inside them.

| # | Step | Address | What it does to the world | Tag |
|---|---|---|---|---|
| 0 | UI cursor validation | `AnnoFrame 0x10047170` / `0x100486b0`; flags `0x2000000` preview, `0x8000000` click | none (validation only); type flag bit `0x80` set → `City::Build` check, clear → island `+0x6c` directly; Kontor/fisher/shipyard/whaler: up to 4 rotation retries | D (ship-placement.md §1) |
| 1 | Commit | `AnnoFrame 0x100441f0` → `tNetClient` apply `0x100589a0` mode 6 | queues/executes the 10-byte record(s) | D |
| 1a | Placement sound | `AnnoFrame 0x10046020` (after the commit, `0x10047694`) | type property `0x479` (the `BAUSAMPLE` record, L) played at the record's (x, height, y) through the sound object `[cursor+0xc]` slot `+0x74`→`+0x40`. Client-side, not world state | D *new* (call); L (`0x479` = BAUSAMPLE) |
| 2 | Owner of footprint | `0x10058890` | every footprint tile neutral (15) or one common owner = the commanded city | D |
| 3 | `City::Build` | `0x100276a0` | island `+0x6c` = `0x1007c790` (terrain, overlap, height byte → `rec[8]`); owner loop; upgrade rewrite PRODTYPE 0x43/0x48; affordability `0x100275d0` (unless player option 7 or flag `0x2000000`). Market block with the same PRODTYPE already at the origin → flag `0x400000` (in-place replacement allowed) | D |
| 4 | `City::Place` | `0x100279a0` | **all world effects below, in this order** | D *new* (full body) |

#### 0.1 `City::Place 0x100279a0` in order (D *new*, whole body read)

| Order | Address | Condition | Effect | Tag |
|---|---|---|---|---|
| P1 | `0x100279e6`–`0x10027a1b` | always | footprint rect `{x, y, w, h}`, w/h swapped for rotation 1/3 (jump table `0x10027eb0`) | D |
| P2 | `0x10027a28`–`0x10027a53` | PRODTYPE 33..35 (`HAUPT`, `MARKT`, `KONTOR`) | **territory disc**: island `+0xe8` = `0x10085840(rect, city, radius = type slot +0x78 = HEAD[0x11], NULL, 0)`: neutral tiles only, plus discovery stage 2 and the reveal (§1.4) | D |
| P3 | `0x10027a5d`–`0x10027a71` | **every type** (also roads, walls, fields) | **footprint claim**: same function, radius 0: every neutral footprint tile gets this city's owner nibble | D *new* (unconditional call) |
| P4 | `0x10027a77`–`0x10027ae1` | unless flag `0x2000000` (founding ship, AI) or player option 7 (free build) | **charge**: player slot `+0x68(type +0xcc(1,0))` = gold; for goods `0x29..0x2d`: amount = type `+0xcc(0,g)`, `[city+0x5d8+8(g-0x29)] += amount` (spent counter), city slot `+0x84(g, amount)` (stock remove). Charged **before** Create; no refund if Create fails (F18) | D |
| P5 | `0x10027aee`–`0x10027b13` | viewer's bit clear in city mask `+0x61c` | flag `0x40000000` (extra tile pass in P11 that sets the visibility bit 31) | D (ship-founding §13.1) |
| P6 | `0x10027b1e`–`0x10027b2d` | PRODTYPE 65 `BRUNNEN` (well) | `0x1002cf60(rect, type, 0)` **before** Create: production buildings (index mask 8) in the well's radius whose input goods 0 is a crop (`0x38..0x4b`) get island slot `+0x138(0, rect, type+0x78(1))` (farm-area update; L: the well bit the crop test multiplies by 133/100) | D *new* (structure); L (meaning) |
| P7 | `0x10027b32`–`0x10027b5b` | block start `(p-1) & ~0x1f` above `0xa0` (p ≥ 193) | **no Create**: roads, ramps, piers, bridges, plazas, fences, crops/trees, walls, gates, monuments, rocks, rivers, ruins stay **tile objects only** (byte table `0x10027ec8` is 0 for every block ≤ `0xa0`) | D *new* (table) |
| P8a | `0x10027b62`–`0x10027c33` | market block (group `0x21` = PRODTYPE **33..64**) and the origin tile already holds the same PRODTYPE | **in-place replacement** (market/Kontor upgrade): find the old building (index mask 2), island `+0x150(x,y,1)`, island-owner notify `+0xc8(0,4,rect)`, city slot `+0xc0(old, newtype)` swaps the type, city slot `+0xbc` recount; **return** (no Create, no tile write, no message) | D |
| P8b | `0x10027c36`–`0x10027ca0` | market block, no same-PRODTYPE building | `Create(rec, -1, flags, &out)` `0x10024010`; city slot `+0xbc` = `0x10028400` (storage cap `+0x624` and EK/VK slot counts from the count and highest INFRAMIN of the market block); **Kontor counter**: island property `0x11` old → old+1, building property `0xe` (KONTID, `+0x1e`) := old. Note: the compare at `0x10027b62` is on the **group** (`ecx = ((p-1)&~0x1f)+1`), so **every market-block building (markets too) takes a KONTID**; `city-creation.md` E12 "PRODTYPE exactly 0x21" should read "group 0x21" | D *new* (correction) |
| P8c | `0x10027ca2`–`0x10027cf4` | other blocks ≤ 192 | `Create`; then hooks: PRODTYPE 68 `KIRCHE` → `0x1002c920(rect, type)`, 73 `HOCHSCHULE` → `0x1002ca80(rect, type)`, 78 `BIBLIOTHEK` → `0x1002d2a0()` | D (bodies U, O-4) |
| P9 | `0x10027cf9`–`0x10027dae` | unless flag `0x4000000` (the loader); PRODTYPE 129 `WACHTURM` or 259 `TOR` | **tile fill**: a record `{id 1, x, y}` per footprint tile, island slot `+0x80(list, n, 1, 0, 0, 0x10, 0)` (the road/wall run planner `0x1007cc40`; L: joins the tower/gate into the wall run) | D *new* (call); U (body) |
| P10 | `0x10027db2`–`0x10027dc5` | always | **tile writes**: island `+0x78(rec, flags & 0x40000000)` = `0x1008ec90` → `0x10082040` (§1.1) | D |
| P11 | `0x10027dc8`–`0x10027e74` | PRODTYPE 33..35 | message type 2 `{x, y, island, city, rotation, player}` to the CompLogic manager (null without AI), message type 4 via player property `0x2c` (the player's AI object `[player+0xc]`, null for type-8 humans) | D (effect on sim state U, O-52) |
| P12 | `0x10027e84`–`0x10027e9a` | group 1 (PRODTYPE 1..32, production) or `0x81` (129..160, military) | new building slot `+0x108(0)` = `0x10071d10`: rewrites flag bit 3 of `+0x0c` (with argument 0 the stored bit ends as 1) and, on change, posts island `+0x168({type, x, y, rot}, bit)` (the blocked-flag tile effect O-26) | D *new*; L (meaning: new producers start "blocked" until their first productive step) |

`Create` `0x10024010` (state.md §7.3, D): class constructor by PRODTYPE (Appendix C), attach type, position `+6/+8`, rotation `+0x0c` bits 0-1, `byte0e` (0x82 house / 0x86 other), `byte0f` := `rec[8]` (the height the placement rule computed), `last` := stamp (property `0x23`), attach city (mine: deposit attach `0x1006e299` → `0x1006ddf0`), list insert at the first free slot, house: SetGroup `0x10076b90` + house count, public block: service counters (`0x10024e18`–`0x1002500f`).

---

### 1. What a placed object writes

#### 1.1 The island tile record (8 bytes per tile; `0x10082040`, transport.md §3.4.17, ship-founding §12.1)

| Field | Bits | Written by placement | Value | Tag |
|---|---|---|---|---|
| object id | word 0 | `0x10082447` | `rec.type` on every footprint tile | D |
| footprint offset + rotation | byte 2 | `0x1008244e`–`0x10082483` | dx, dy from the origin, rotation | D |
| growth phase / parity / anchor | byte 3 | `0x10082497`–`0x100824c5` | phase 0; bit 7 = anchor (first footprint tile in a world-aligned 8×8 block) | D |
| height (item level) | dword 2 bits 0-7 | `0x10082480` | `rec[8]` (ordinary land: window level; coast `max & 0xf0`; single quad: floor of 4-corner mean) | D |
| walk code | dword 2 bits 8-15 | `0x100825af`–`0x100825b7` | from the type's HEAD walk pair (encoder `0x100091f0`); roads 0/1/4 | D |
| walk mask | dword 2 bits 16-24 | ordinary path | type `WALKMASK` | D |
| **owner nibble (territory)** | dword 2 bits 25-28 | **not by `0x10082040`**; by the claim painters `0x100853b0` (neutral only) / `0x100854f0` (force) | city ring index of the claiming city; 15 = neutral. Read by island `+0xec` = `0x100860c0` | D (transport §3.4.17 row 25-28; ui.md 5.6) |
| harvest claim | bit 29 | kept by placement (`and 0x3fffffff`) | — | D |
| bit 30 | bit 30 | cleared | 0 | D |
| visibility | bit 31 | cleared; then set by the `0x40000000` pass `0x10082a46` if the viewer's bit is clear | per client, draw-only | D (§14.3) |

**Overwrite rule:** placed type flag `0x10` clear (every building, road, field): footprint overwritten unconditionally, the old object (tree, stone, scenery) is lost with no removal call. Placed flag `0x10` set (mines, scenery): an existing object whose type has flag `0x10` **and** whose tile has the anchor bit is kept (D, ship-founding §12.1).
**Height grid:** placement writes **no** vertex of the height grid (`island+0x14`): the building records the level in `rec[8]`, the terrain is not flattened (D bounded, ship-founding §12.3). **Ground texture (`GFXID`)**: no store into the GFX grid on this path (L: not in the `0x10082040` store list).
**Chunk redraw** counters at island `+0x94` bumped (D). Loader placements use the same writer with flag `0x4000000` (`0x10089130` → `0x10082040`, OQ-142).

#### 1.2 The building object (`Create`, per class)

| Class (PRODTYPE) | Object, size | Initial fields | Stock / workers at placement | Tag |
|---|---|---|---|---|
| Workshop (1), dual (12), weapons (17) | `0x30` / `0x38` | stocks 0, switched on, carriers 0, `last` = stamp, status time = stamp | empty, no figure | D |
| Farm family (2, 4, 8, 10), fisher (6), trapper (16) | `0x38` | raw/out 0, 5 worker slots empty | none | D |
| Mine (3) | `0x28` | `deep` from research id; ore object bound at attach | none | D (salt-extraction §22) |
| Small farm (9) | `0x48` | | none | D |
| Quarry (13), ice hole (14) | base class, vtable `0x100c2614` | no tick; the mason's worker job does the work | none | D (quarry-bricks Q1) |
| Hunter (5), shipyard (7), whaler (15), cannonworks/barracks (11, 130) | class-specific | bodies partly unread | none | D (ctor) / U |
| Market block 33-35 | `0x50` market class | KONTID := island counter (P8b) | city storage cap/EK-VK recount | D |
| 36 `MARKTKOMBI` | `0x1006c080` + vtable `0x100c2740` | | | D |
| Public (65-79) | `0x18`-`0x3c` | service counters raised | none | D |
| Houses 97-101 | `0x50` | ctor: residents_64 = 320 (5 residents), E = 640; `SetGroup` first assignment: group ≠ 0 (non-pioneer placed directly) → residents = `Wohnmax`, one world-PRNG draw per forced need (`128 − (r & 31)`) | residents move in at once | D (population.md (a)/(b)) |
| Military 129, 131 | tower classes | gate 166 | none | D |
| Market stall 161 | `0x20` | never scheduled | | D |
| p ≥ 193 | **no object** | tile record only | — | D *new* (P7) |

#### 1.3 Costs taken (P4)

Gold (HEAD u16 `[8..9]`) from the player; build goods `0x29..0x2d` (HEAD bytes `[3..6]`… type slot `+0xcc(0, g)`) from the **city's** warehouse, plus the city's per-goods "spent" counter `[city+0x5d8+8i]`. Waived by flag `0x2000000` (founding/AI pass the cost elsewhere) or player option 7 (cheat/free build, O-9). Tile objects are charged the same way (roads 5/10 gold, fields 0-6, walls 0-70; P). Charged before Create; a failing Create keeps the charge (F18). **D**

#### 1.4 City and territory

| Effect | Address | Rule | Tag |
|---|---|---|---|
| Joins the city | apply `0x100589a0` → `City::Build` | the footprint's single non-neutral owner **is** the city; at least one tile must already be owned | D |
| Footprint claim (all types) | P3, `0x10085840` radius 0 | neutral footprint tiles become the city's | D *new* |
| Territory disc (33 HAUPT, 34 MARKT, 35 KONTOR only; **not** 36 MARKTKOMBI) | P2, `0x10085840` stage 1 | Bresenham row table `T[0..R]`, R = HEAD[0x11]; neutral tiles only; rival and own tiles untouched; clipped to the island | D + E (ship-founding §11.2) |
| Radius values | `Anlagen.dat` HEAD[0x11] | Kontor 1302/1304/1306 = 24, market 1301/1303/1305 = 24, market 1307 (1×1) = 255, markets 2900-2906 = 3, 2909 = 0, 3000 = 24, Kontor 1412 = 0; no type of PRODTYPE 33 or 36 exists | P *new* |
| Discovery (stage 2) | `0x10085ac4`–`0x10085ee0` | ore objects (`OCCUR`, kind mask 1) in the radius-(R+6) table: first discovery posts world `+0xd0` codes `0xd..0x11`, sets the player's bit | D (ship-founding §16.2) |
| Reveal | island `+0x130` = `0x10080c10(cx, cy, R+8, 1, player)` | city masks / player property 7 / island `+0xaa`; only the lowest-numbered foreign city in reach is visited | D (§15.4) |
| Market/Kontor storage | city slot `+0xbc` = `0x10028400` | cap `+0x624 = u16[0x100dcc84 + 2n] << 8` (50, 70, 90, 110, 130, 140 … t), EK/VK slots 6/4, 8/6, 8/8 by highest INFRAMIN | D (§14.2, §15.2) |
| First city on an island | ship `0x10041760` (founding only) | ring slot + name draw + registration before Place | D (city-creation.md) |

#### 1.5 Figures spawned

| Class | At placement | Later | Tag |
|---|---|---|---|
| Every building | **none**: no figure write in `0x100279a0`, `0x10024010`, `0x10082040`, the claim | carriers (job 0x33), carts (0x34), workers (farm/forester 0x2a, fisher route 45, hunter 0xd, trapper), whalers are created by the building's tick via slot `+0x118` (`0x10072d20`; fisher `0x10067e60`) when the tick decides to send one | D (closure: virtual callees L) |
| Houses | residents are a number, not figures; walkers come from the house tick | | D |
| Ships | built by the shipyard tick/commands, not by placement | | L |
| Animals | wildlife sweep `0x10093480`, independent of buildings | | D (wildlife.md) |
| Type-11 human AI | registration can trigger the AI scan that places one road piece with a PRNG draw (founding, not reached for a first warehouse) | | D/L (§16.1) |

#### 1.6 Messages and sounds

| Event | Address | Receiver | Tag |
|---|---|---|---|
| Placement sound | `AnnoFrame 0x10046020` | client sound object; type property `0x479` | D *new* / L |
| Market block 33-35 placed | `0x10027e49` type 2; `0x10027e74` type 4 | CompLogic manager; player's AI object (prop `0x2c`) | D |
| Demolish | `RemoveAt 0x100292d5` type 5 `{x, y, type id, island no, city no, tile owner}` | player property `0x2c` object (AI) | D *new* |
| Discovery | world `+0xd0` codes `0xd..0x11` | AnnoFrame `tEvent` log | D |
| Blocked flag | island `+0x168` | redraw / status icon | D *new* (P12) |
| Founding refused, own city exists | message `0x138` (validator) | UI | D |

#### 1.7 Ground and terrain changes

- No terrain height change, no ground texture change on placement (§1.1). **Fields do not turn ground**: a farm's fields are separate tile objects (crop objects, PRODTYPE 226 `ROHSTWACHS`, HEAD u16 `[0x28]` of the farm names which, task/UI-PRODBAR-1) that the player plants (the "plant" button / add-on V key, L: video note V1 01:10 and the community FAQ, quoted in docs/open-questions.md near line 985) or the map places; the farm's worker harvests a ripe object back to its young predecessor (`0x10087340`, forestry.md) and the growth sweep `0x1008b5f0` advances `id + 1`. **D** for harvest/growth, **L** for the plant button's path.
- Roads write walk code 0/1/4 and mask 0 (road-placement.md §3); no neighbour tile is touched by `0x10082040` (D bounded); gates and towers call the run planner (P9).
- Wells (65) change the farm-area records of crop farms in reach (P6) — the only placement that alters fertility-related state (L for meaning).

---

### 2. Per class

| Class | PRODTYPE (P: types, footprint, flags) | Placement class / rule | Place-specific effects | Sim today (`BuildCmd`) |
|---|---|---|---|---|
| **Houses** | 97-101 (52+7+8+8+6 types, 2×2..4×4, `0x84`, gold 0..200) | ordinary land, window level ≥ 32 | footprint claim, charge, Create (SetGroup, house count, residents 5 or `Wohnmax` + draws), tile write | supported: placement rule, charge, Create, first assignment (`HouseFirstAssignment` draw, MAP-POP-CLOSE-1 C2); **gap**: neutral footprint tiles not claimed |
| **Workshops** | 1, 12, 17 (24+0+3; `0x84`) | ordinary land | Create, P12 blocked bit | supported; gaps: P3 claim, P12 |
| **Farms with fields / plantations** | 2 `PLANTAGE` (21: forester 801, grain 811, plantations), 4, 8, 9, 10 | ordinary land | Create, P12; fields are separate tile objects | building supported; fields/trees as player placements **Unsupported** (single-quad class rule exists, FERTILITY-1a, not reachable by `BuildCmd`) |
| **Forester** | 801 `PLANTAGE`, radius 5 | ordinary land | as farm; the worker cuts (job 0x2a), growth regrows | supported (forestry contract) |
| **Fisher** | 6 (3 types, `0x2087`) | coast class `(F & 0x7017) == 0x2007`, height `mx & 0xf0` | Create (fisher vtable), P12; fish fill is runtime (fisher.md §6-7) | supported (FISHER-1d) |
| **Hunter / trapper** | 5 (1), 16 (1) | ordinary land | Create, P12 | Hunter class exists (tick None/PARTIAL per hunter.md); trapper supported |
| **Mines / quarries** | 3 (8, `0x3096`), 13 (4), 14 `EISLOCH` (1, `0x92`) | mountain class `0x3000` + coverage test (flag `0x10`), HIGHMAP | Create; mine attach binds the best ore object (`0x1006ddf0`), no deposit refusal at placement | supported for mountain class (salt-extraction §19-22, quarry-bricks Q3); `EISLOCH` unsupported |
| **Public** | 65-79 (well 5, inn 3, chapel 14, church 2, bath 2, doctor, school 2, university, castle 7, gallows 3, court, fire brigade, library, pavilion) | ordinary land | Create, service counters; **well**: P6 hook before Create; **church/university/library**: P8c hooks; chapel/school: upgrade rewrite (O-28) | Create + service counters supported; hooks P6/P8c and the rewrite **missing** (O-4, O-28) |
| **Kontor / market** | 34 `MARKT` (24), 35 `KONTOR` (13, coast `0x2087`), 36 none shipped | coast (Kontor) / ordinary (market) | disc claim R = HEAD[0x11], discovery, reveal, Create, KONTID, storage/EK-VK recount, AI messages; same-PRODTYPE at origin → in-place upgrade | **first Kontor via `FoundCmd`**: disc, footprint claim, KONTID, discovery, reveal, registration all implemented (founding.cpp). **Via `BuildCmd`** (second Kontor, every market): none of disc/discovery/reveal/KONTID/recount; in-place upgrade answers `Occupied` (O-4) |
| **Military** | 129 `WACHTURM` (6, 1×1/2×2), 130 `MILITAR` (13), 131 `KANONENTURM` (1), 11 `KANONENBAU` (2) | ordinary land | Create, P12 (group 0x81), WACHTURM: P9 run planner | Create supported as classes (towers, Cannonworks; ticks unread); P9/P12 missing |
| **Shipyard** | 7 `WERFT` (2: 5×5, 8×8, `0x2087`) | coast | Create (Shipyard class), P12 | Create supported; tick unread (None) |
| **Harbour pieces** | 197 `PIER` (13, `0x2087`/`0x4087`) | coast / road class | tile object only (P7) | **Unsupported** |
| **Roads** | 193 `STRASSE` (21, `0x4186`/`0x1186`), 194-196 ramps | road class `0x4000` (no level test) / slope `0x1000` (≥ 32) | tile object, footprint claim, charge | land roads supported (MAP-ROAD-1, own level floor R-A2); ramps unsupported; **gap**: P3 claim |
| **Bridges** | 198 `BRUECKE` (15, `0x4386`/`0x1386`) | road/slope class over water | tile object | **Unsupported** |
| **Walls** | 257 `MAUER` (39), 258 `MAUERUP` (2) `0x1886`; 259 `TOR` (4, `0x84`) | slope class ≥ 32; gate ordinary | tile object; TOR: P9 planner | **Unsupported** |
| **Plazas, fences** | 199 `PLATZ` (11), 200 `GAUKLERPLATZ` (5), 201 `ZAUN` (15) | ordinary land | tile object | **Unsupported** |
| **Monuments / ornamentals** | 289 `DENKMAL` (28), 290 `TRIUMPH` (1), 291 `ZIERDE` (26) | ordinary land | tile object only (P7: no Create for p ≥ 193) | **Unsupported**; OQ-166 |
| **Nature objects** | 225-227 (291 crops/trees/fields), 321-325 rocks, 353-355 water, 385 ruins | single-quad `0x29` (mean ≥ 32, overlap only on flag `0x20` objects); scenery `0x10`-flag classes | map: loader `0x10089130` → `0x10082040` with `0x4000000`; player: crops/trees through Build/Place as tile objects (L) | map objects loaded; player planting **Unsupported**; growth sweep and harvest ported |

---

### 3. Demolish and removal

Apply `AnnoFrame 0x10058e90` (mode 6), per tile record `{x, y}`: tile owner (island `+0xec`) must be the city ([esi]+0x40), an optional client callback `[this+0x124](rect, 1)` may **abort the rest of the command** (`0x10058f5f je 0x1005918f`; L: the "no demolish with enemy units near" rule, M 27), then city slot `+0xb4` = **RemoveAt** `0x10028fd0(x, y, tick)`, then city slot `+0x78` (empty?) → island `+0xfc(city)` removes the city. After all tiles, if a rectangle was given: border-strip records `{1, x, y}` along the rectangle, island `+0x80(list, n, 1, 0, 0, 0xe, 0)` (L: re-fit road/wall variants around the cleared area). **D *new***

#### 3.1 `RemoveAt 0x10028fd0` (D *new*, whole body)

| Order | Address | Condition | Effect |
|---|---|---|---|
| R1 | `0x10028fed` | island `+0x94(x, y)` = the **tile object's type** (also for roads, walls, fields) | null → nothing |
| R2 | `0x10029011`–`0x10029051` | list mask by block (byte table `0x10029484` = 0..5 per block; targets `0x10029468`): 1..32 → 8, 33..64 → 2, 65..96 → 1, 97..128 → 4, 129..160 → 0x10, 161..192 → 0x20, ≥ 193 → 0 | — |
| R3 | `0x1002906b` | always | island `+0x90(x, y, 6, &rect)`: the object's footprint rect |
| R4 | `0x1002909d` | mask ≠ 0 | building lookup (index slot `+8`); if found: flag `0x40` → island `+0x150(x, y, 1)`, flag `0x10` → island `+0x158(x, y)` (O-5: fire / plague stop); market block: stalls (list `0x20`) in the footprint grown by 4 get slot `+0x2c(building)` (detach); **`0x100255c0` remove** (counters, house count, service counters, list remove, market-list → defeat flag) |
| R5 | `0x10029190` | market block | island owner notify `+0xc8(0, 4, rect)` |
| R6 | `0x100291aa`–`0x100291fc` | building first output goods `0x51..0x56` (table `0x10029528`) | `0x51`: `0x1002c740` else `0x1002cb20`; `0x53`/`0x54`: `0x1002c090` else `0x1002c9c0`; `0x56`: `0x1002d2a0` (O-5; L: the counterparts of the church/university/library hooks) |
| R7 | `0x10029201`–`0x100292d5` | player property `0x2c` object present | message type 5 (§1.6) |
| R8 | `0x100292ef` | always | island `+0x7c(x, y, 0)` → `0x10082c30`: **tile clear** (clear arm: id 0, byte 2 = 0, walk code 2, mask 0, height byte kept; or the replacement arm from a catalogue type when the high nibble of island `+0x18` is set — arm choice U, OQ-201, U-R5) |
| R9 | `0x100292fa`–`0x10029420` | PRODTYPE switch (byte table `0x1002954c`, targets `0x10029540`) | **65 `BRUNNEN`**: `0x1002d090(rect ±9, building, tick)` (undo of P6; L); **197 `PIER`, 257 `MAUER`, 258 `MAUERUP`**: figures in the footprint (query mask `0xf9cf`, buffer 0x80) get slot `+0x9c(slot +0x90(3, 0))` (L: units standing on a wall/pier are reset/dropped) |
| R10 | `0x10029425`–`0x10029456` | market block | city slot `+0xb8` = `0x100286c0(rect, radius = type +0x78)`: walks all 16 cities of the island and can call a **nested RemoveAt** on objects of tiles that lose their market coverage (complogic.md T54 F1); then city slot `+0xbc` recount (storage cap shrinks) |

#### 3.2 What is reverted, what refunded

| Item | Reverted? | Tag |
|---|---|---|
| Building object, list slot, derived counters, house count, service counters | yes (R4) | D |
| Tile object word / walk code | cleared or replaced (R8); the object the building overwrote (tree, stone) is **not** restored by the clear arm | D / U (replacement arm) |
| Height byte of the tile | kept | D |
| Territory (owner nibbles) | not by RemoveAt itself; only the market-block cascade R10 (L: tiles outside any remaining market range revert and orphan objects are removed) | D (call) / L (semantics) |
| KONTID counter | never decremented | D (no store) |
| Storage cap / EK-VK slots | recounted (R10) | D |
| Fire / plague on the building | stopped (R4) | D / L |
| Well bonus on farm areas | updated (R9) | D / L |
| **Gold and goods** | **no refund**: no player gold or city stock store in RemoveAt or the demolish apply; manual: "cannot be undone and gives no materials back" [M 28] | D *new* (closes U-R3 of road-placement.md) + manual |
| Figures homed in the building | not handled in the direct code (OQ-126 item 7) | U |
| City | removed when empty (city slot `+0x78` = `0x1002f7f0`, O-6) | D (call) / U (predicate) |

---

### 4. Construction time

**Placement is instant.** `City::Place` calls `Create` with the final class in the same command; no building-site object, no timer, no "under construction" state is written (D *new*: whole body read, no such branch). The building's first tick waits for the class gate (500 t production/public, 2500 t houses, 166 t towers) from `last` = the placement stamp (SP: start of the previous step; MP: 0, so up to 131 catch-up steps, state.md §7.3). Houses are occupied at once (5 residents, or `Wohnmax` for a higher group). The flag bits 4/5 of `+0x0c` that production.md guessed as "under construction" are the suspend flags read by the ticks (fisher.md §2), not set by Place. The only "not ready yet" state Place leaves is P12's blocked bit on production/military buildings (L). **D** (path), **L** (P12 meaning; no observation in the running original: NOT_RUN).

---

### 5. What the rebuild's sim does (`src/sim/src/apply.cpp` `build` → `check_record` → `place`; `create.cpp`; `founding.cpp`)

| Original effect | Sim | Status |
|---|---|---|
| Footprint owner rule `0x10058890` | `footprint_owner_rule` | done |
| Terrain rule `0x1007c790` (ordinary, coast, mountain, land road, single quad) | `placement_rule` | done for these classes; others `Unsupported` |
| Height byte → `byte0f` | derived at execution (ADAPT A1, original trusts the wire) | done (deliberate) |
| Upgrade rewrite (P: Build) | `Seams::upgrade_rewrite`, default none | **gap** (O-28) |
| Affordability, free build | step 6 | done |
| Disc claim 33..35 (P2) | only `FoundCmd` | **gap for `BuildCmd`** |
| Footprint claim of neutral tiles (P3) | only `FoundCmd` (`claim_neutral_rect`) | **gap for `BuildCmd`** (neutral footprint tiles stay neutral) |
| Charge (P4) | after Create, same state | done |
| Visibility pass (P5) | not modelled | deliberate (client-only) |
| Well hook (P6) | — | **gap** |
| p ≥ 193 tile objects (P7) | land roads only; rest `Unsupported` | **gap** (walls, gates, bridges, ramps, piers, plazas, fences, monuments, crops/trees) |
| In-place market/Kontor upgrade (P8a) | answers `Occupied` | **gap** (O-4) |
| KONTID + counter for every market-block building (P8b) | only `FoundCmd` | **gap** |
| Storage cap / EK-VK recount (city `+0xbc`) | `CityStock::capacity` fixed 50 t unless loaded | **gap** |
| Church/university/library hooks (P8c) | `Seams::on_placed`, none | **gap** (O-4) |
| Tower/gate run planner (P9) | — | **gap** |
| Tile write `0x10082040` | `place_object_record` / Create step 8 | done (overwrite rule; flag-0x10 keep rule via anchor) |
| AI messages (P11, R7) | — | not modelled (O-52; no AI) |
| Blocked bit after Place (P12) | `blocked = false` at creation | **gap** (L) |
| Mine attach | `bind_mine_deposit` | done |
| House first assignment + draws | `first_assignment_problem` / create | done (with `house_updates`) |
| Discovery + reveal | `FoundCmd` only | **gap for `BuildCmd` markets** |
| Demolish owner test, FindByTile, remove | `demolish`, `remove_building` | done |
| Demolish veto callback (`[this+0x124]`) | — | gap (L meaning) |
| RemoveAt fire/plague stop, stall detach (R4) | `Seams::on_removed`, none | **gap** (O-5) |
| Goods 0x51-0x56 handlers (R6) | — | **gap** (O-5) |
| Tile clear (R8) | clear arm stand-in; non-building objects via `clear_object_record` | partial (replacement arm U, OQ-201) |
| Well undo, wall/pier figure reset (R9) | — | **gap** |
| Market removal cascade + recount (R10) | — | **gap** (territory never shrinks) |
| City removal when empty | `Seams::city_is_empty`, default false | deliberate stand-in (O-6) |
| Refund | none | matches (D *new*) |
| Placement sound | UI | outside sim |

---

### 6. Open questions

Existing: O-4 (Place hooks), O-5 (RemoveAt tail; now partly read: R4-R9 structure), O-6, O-9, O-26 (blocked tile effect — P12 now shows the caller), O-28 (upgrade rewrite), O-52 (messages), OQ-063 (what a building leaves on its tiles), OQ-126 items 6-7, OQ-142, OQ-166 (monuments: P7 confirms Place never creates a building object for 289-291), OQ-201 / U-R5 (clear vs replacement arm), OQ-204/205 (tile bits), OQ-720 (no-road tests), road-placement U-R1/U-R2/U-R4. **U-R3 (refund) is answered: no refund (D, §3.2).**

NEW:
- **OQ-740** `city-creation.md` E12 says the Kontor counter runs "only for PRODTYPE exactly 0x21"; `0x10027b32`–`0x10027b62` compares the **group** (`((p-1)&~0x1f)+1`), so every market-block building (33..64, including markets 1301…) takes a KONTID and bumps the counter. Re-check and correct the contract.
- **OQ-741** Building slot `+0x108` = `0x10071d10` with argument 0 leaves flag bit 3 set and posts island `+0x168`: is a new production/military building really "blocked" until its first step, and what clears it? (read the class ticks' `+0x108` callers; one capture in the original of a fresh workshop's status).
- **OQ-742** Well hooks `0x1002cf60` / `0x1002d090` → island slot `+0x138`: which tile bits does `+0x138` write (the crop test's well bit), and with which radius (farm radius `type+0x78(1)`)?
- **OQ-743** City slot `+0xb8` = `0x100286c0` after a market-block removal: which tiles revert to neutral, which objects are removed by the nested RemoveAt, and are other cities' buildings affected?
- **OQ-744** Island slot `+0x80` (`0x1007cc40`) with `{id 1}` records: what the tower/gate fill (flags `0x10`) and the demolish border strip (flags `0xe`) write — wall/road variant re-fit?
- **OQ-745** RemoveAt R9 for 197/257/258: figure slot `+0x9c(slot+0x90(3,0))` — what happens to units on a removed wall or pier?
- **OQ-746** Hooks P8c (`0x1002c920`, `0x1002ca80`, `0x1002d2a0`) and R6 handlers: what state they keep (L: research/infrastructure counters for goods `0x51..0x56`).
- **OQ-747** The player "plant" action (fields/trees around a farm): which cursor and records it sends (`tCursorAnlageRohstoff`?), and whether the apply path is mode 6 Build/Place (charge, footprint claim) as the code allows.
- **OQ-748** The demolish callback `[tNetClient+0x124]`: who installs it and is it the "enemy units near" rule (M 27)? It aborts the whole remaining command.
- **OQ-749** Type property `0x479` played by `AnnoFrame 0x10046020`: confirm it is `BAUSAMPLE` and whether demolish has its own sample.


---

# Part A: per-type inventory (method, findings, counts)

## BUILDINGS-REF-1 part A: per-type data inventory

Generated by `cache/bref/inventory.py` (read-only; `python -I`). Do not hand-edit the tables.

### 1. Method

- File: GOG `game/app/Anlagen.dat` (205,470,957 bytes), `ANNO/ANLAGEN/ENTRY` (921 types). Display names (GOG `Texte.dat` `[ANLAGEN]`) appear only in the locally generated tables.
- Decoder: the repo oracle `tools/figuren.py` (`records()`, `entry()`), no second chunk reader. OBJECTPOS sub-records: 6-byte name, u16 size, data (task/UI-INFO-2b `tools/ui_layout.py` `anlagen_entrance_arrows`; AnnoGame loader 0x10009e3e..0x10009edf).
- HEAD is zero-filled to 56 bytes as the original loader does (AnnoGame 0x10009351). Offsets: `[0]`/`[1]` footprint W/H, `[3]` bricks, `[4]` wood, `[5]` tools, `[6]` marble, u16 `[8]` gold, `[0x11]` action radius, u16 `[0x1c]` flag word F, u16 `[0x28]` crop object (`docs/re/production.md` 2.4; task/UI-PRODBAR-1 production.md for `[0x28]`). PROD3 u16 `[0]` upkeep while shut down, u16 `[2]` while active (production.md 2.3). WARE = outputs, ROHSTOFF = inputs, `UNUSED` dropped (production.md 2.2).
- F classes: `F & 3` selects the terrain mode (1 = single-quad scenery), `F & 0x7000` the class test: 0 ordinary land (all window vertices equal, >= 32), 0x1000 mountain/relaxed (`mx - lo <= 0x10`), 0x2000 coast helper, 0x3000 HIGHMAP helper (mines/quarries, unread: OQ-052), 0x4000 road/bridge (exempt) (task/FERTILITY-1a `docs/re/ship-placement.md` 3, 3a; AnnoGame 0x10008f90).
- Arrows: CONPO2, 3 bytes per arrow (x3, y3, side); x3/y3 in thirds of a tile from the footprint corner at rotation 0; side 0..3 = -x, +y, +x, -y (offsets (-0.5,0), (0,0.5), (0.5,0), (0,-0.5), View.dll 0x10021828; ui.md 2.5 on task/UI-INFO-2b). Which screen side is "front" is not decoded here (L, see section 4).
- `rot` = rotation blocks of sprite set 0, zoom group 0 = frames / ANIM n (`ui_layout.anlagen_preview_frame`, render::frame_index); a 1 means the type has one picture for all four turns (L). `sets` = HAEUSER sprite sets.
- Groups: by PRODTYPE (my mapping, L). Houses PIONIER..ARISTOKRATEN; production workshops, mines, quarries, stonemason, cannon foundry; farms and fields PLANTAGE, WEIDETIER, SCHAFFARM, UNIVERSAL, JAGDHAUS, PELZJAEGER, FISCHER, WALFAENGER; public incl. market, stalls, decoration, castle/palace (SCHLOSS); military incl. walls, gates, fences; harbour KONTOR, WERFT, PIER; roads incl. bridges, ramps, squares (PLATZ); nature ROHSTOFF*, rocks, cliffs, ruins, rivers, sea; others = UNUSED and the rest.
- Evidence letters: P parsed from the file, D code address cited in the linked doc, L likely. Unknown HEAD fields (`[0x02]`, `[0x0b]`, `[0x0c]`, `[0x12]`, `[0x20]`, `[0x24/0x25/0x2e]`, `[0x32]`) stay out of the tables: O-21 / OQ-054.

Types per group: houses 81, production 46, farms and fields 38, public 146, military 80, ships and harbour 28, roads 55, nature objects 442, others 5.

HE summary: `Anlagen.addon` 921 types, 0 differing, 0 only GOG, 0 only HE; `Anlagen.dat` 909 types, 5 differing, 14 only GOG, 2 only HE; `Anlagen.classic` 894 types, 77 differing, 29 only GOG, 2 only HE; `Anlagen.gfx` 1 entry without HEAD (graphics only, not type data).

### 2. Per-type tables (generated locally, not committed)

Run `python -I tools/buildings_inventory.py` (see the top of this file). One row per type, grouped by PRODTYPE (houses 81, production 46, farms and fields 38, public 146, military 80, ships and harbour 28, roads 55, nature objects 442, others 5; 921 in GOG `Anlagen.dat`). Columns and their evidence: id P; PRODTYPE P+D; WxH P+D; flag word F P+D; F&3 D; F&0x7000 D; radius [0x11] P+D (meaning L outside market/farm); crop object [0x28] P+D; inputs and outputs P+D; gold/tools/wood/bricks/marble P+D (cost meaning L); upkeep on/off P+D; sprite sets P; rotation blocks L; arrows per side -x/+y/+x/-y P+D; raw CONPO2 (x3, y3, side) P+D.

### 2.10 Arrow anomalies and CONPOS

- 261 ~Steppenbaum02 (ROHSTOFF): CONPO2 12 bytes = `01 00 d7 00 00 01 01 00 00 02 00 00`
- 1542 ~Eisscholle_Loch01 (TIERHOEHLE): old CONPOS 4 bytes = `00 04 00 07`; OBJECTPOS sub-records ['SCHUSS', 'CONPOS']
- 1571 ~LöwenHöhle01 (STEIN): CONPO2 3 bytes = `02 00 d7`

- 184 types carry CONPO2 (852 arrow records); 326 types carry ENTRANCE; 142 carry ENTRANCE but no CONPO2: 105 ROHSTOFF, 107 ROHSTOFF, 109 ROHSTOFF, 111 ROHSTOFF, 115 ROHSTOFF, 119 ROHSTOFF, 202 ROHSTOFF, 204 ROHSTOFF, 206 ROHSTOFF, 208 ROHSTOFF, 210 ROHSTOFF, 212 ROHSTOFF, 214 ROHSTOFF, 216 ROHSTOFF, 218 ROHSTOFF, 220 ROHSTOFF, 222 ROHSTOFF, 224 ROHSTOFF, 226 ROHSTOFF, 258 ROHSTWACHS, 411 ROHSTOFF, 413 ROHSTOFF, 415 ROHSTOFF, 417 ROHSTOFF, 419 ROHSTOFF, 1542 TIERHOEHLE, 2249 MAUERUP, 2250 MAUERUP, 2251 MAUER, 3000 MARKT, 3001 PIONIER, 3002 KAPELLE, 3003 PIONIER, 3004 KAPELLE, 3005 MILITAR, 3006 MARKTSTAND, 3007 BRUNNEN, 3008 PIONIER, 3009 MARKTSTAND, 3010 HANDWERK, 3011 WEIDETIER, 3012 PLANTAGE, 3013 WEIDETIER, 3017 PIONIER, 3018 KONTOR, 3100 MARKT, 3101 PIONIER, 3102 PIONIER, 3103 PIONIER, 3104 WEIDETIER, 3105 MARKTSTAND, 3106 MILITAR, 3107 PIONIER, 3109 PLANTAGE, 3110 KAPELLE, 3200 MARKT, 3201 KAPELLE, 3202 KAPELLE, 3203 PIONIER, 3205 PIONIER, 3206 STEINMETZ, 3207 DENKMAL, 3208 PIONIER, 3209 MILITAR, 3210 MARKTSTAND, 3213 WEIDETIER, 3214 HANDWERK, 3301 MILITAR, 3302 MARKT, 3303 PIONIER, 3304 PIONIER, 3305 PIONIER, 3306 PIONIER, 3308 KAPELLE, 3309 PIONIER, 3311 MARKTSTAND, 3313 WEIDETIER, 3314 KONTOR, 3315 PLANTAGE, 3500 MARKT, 3501 PIONIER, 3505 STEINMETZ, 3506 EISLOCH, 3508 PIONIER, 3509 MILITAR, 3510 FISCHER, 3511 MARKTSTAND, 3513 KONTOR, 3515 PLANTAGE, 3516 KAPELLE, 3600 MARKTSTAND, 3601 PIONIER, 3602 MARKT, 3603 KAPELLE, 3604 PIONIER, 3605 DENKMAL, 3606 MILITAR, 3607 ZIERDE, 3608 PIONIER, 3609 PIONIER, 3610 WEIDETIER, 3613 PLANTAGE, 3614 PLANTAGE, 3700 MARKT, 3701 PIONIER, 3702 WEIDETIER, 3703 PIONIER, 3704 PIONIER, 3705 KAPELLE, 3706 PIONIER, 3707 PIONIER, 3708 ZIERDE, 3709 MARKTSTAND, 3710 MILITAR, 3711 PLANTAGE, 3712 KONTOR, 3803 FELSEN, 3804 FELSEN, 3850 MARKT, 3851 PIONIER, 3852 PIONIER, 3853 PIONIER, 3854 PIONIER, 3855 KONTOR, 3856 MARKTSTAND, 3857 WIRT, 3864 PIONIER, 3872 UNIVERSAL, 3950 MARKT, 3951 PIONIER, 3952 PIONIER, 3954 PIONIER, 3957 MILITAR, 3958 KAPELLE, 3960 PIONIER, 3969 PIONIER, 3972 PIONIER, 3973 KAPELLE, 3979 PIONIER, 3980 MARKTSTAND, 3989 KONTOR, 3992 BRUECKE
- Flag classes over all types, (F & 3, F & 0x7000): (0, 0x0000) 400, (1, 0x0000) 291, (2, 0x0000) 101, (2, 0x1000) 52, (2, 0x3000) 12, (2, 0x4000) 33, (3, 0x2000) 30, (3, 0x4000) 2
- Distinct flag words: 0x0004 8, 0x0012 39, 0x0026 32, 0x0029 291, 0x003a 29, 0x0084 344, 0x0092 1, 0x1186 8, 0x1386 3, 0x1886 41, 0x2087 30, 0x3096 12, 0x4087 2, 0x4186 15, 0x4386 18, 0x8018 48
- HEAD lengths: 36 B 5, 44 B 35, 48 B 5, 52 B 142, 56 B 734

### 3. GOG / History Edition differences

#### HE `Anlagen.addon`

- 921 types
- Types only in GOG (0): none
- Types only in HE Anlagen.addon (0): none

Types in both whose footprint, flags, arrows, costs, radius, crop, upkeep, goods or PRODTYPE differ (0):

| id | PRODTYPE (GOG) | GOG -> HE |
|---|---|---|

Other HEAD differences only (bytes listed; none of the columns above) (0): none

#### HE `Anlagen.dat`

- 909 types
- Types only in GOG (14): 525 Settler's house, 526 Settler's house, 546 Citizen's house, 584 Aristocrats, 585 Aristocrats, 1384 Palace, arch element, 1385 Main Palace, 1387 Palace gate, 1388 Hall of Generals, 1389 Lions' portal, 1390 Equestrian statue, 1391 Fountain of Venus, 1392 Palace wing, 1393 Ornamental garden
- Types only in HE Anlagen.dat (2): 510 RUINE 1x1, 512 RUINE 4x4

Types in both whose footprint, flags, arrows, costs, radius, crop, upkeep, goods or PRODTYPE differ (5):

| id | PRODTYPE (GOG) | GOG -> HE |
|---|---|---|
| 511 | PIONIER | PRODTYPE: PIONIER -> RUINE; flag word: 0x0084 -> 0x0026; CONPO2: 3,0,3 6,0,3 0,3,0 0,6,0 3,11,1 6,11,1 11,3,2 11,6,2 -> -; costs: (0, 0, 3, 0, 0) -> (0, 0, 0, 0, 0) |
| 513 | PIONIER | PRODTYPE: PIONIER -> RUINE; flag word: 0x0084 -> 0x0026; radius: 24 -> 0; CONPO2: 3,0,3 6,0,3 0,3,0 0,6,0 3,11,1 6,11,1 11,3,2 11,6,2 -> -; costs: (0, 0, 3, 0, 0) -> (0, 0, 0, 0, 0) |
| 566 | KAUFLEUTE | PRODTYPE: KAUFLEUTE -> RUINE; flag word: 0x0084 -> 0x0026; radius: 24 -> 0; CONPO2: 3,0,3 6,0,3 0,3,0 0,6,0 3,11,1 6,11,1 11,3,2 11,6,2 -> -; costs: (0, 5, 5, 4, 0) -> (0, 0, 0, 0, 0) |
| 567 | KAUFLEUTE | PRODTYPE: KAUFLEUTE -> RUINE; flag word: 0x0084 -> 0x0026; radius: 24 -> 0; CONPO2: 3,0,3 6,0,3 0,3,0 0,6,0 3,11,1 6,11,1 11,3,2 11,6,2 -> -; costs: (0, 5, 5, 4, 0) -> (0, 0, 0, 0, 0) |
| 2229 | KANONENTURM | PRODTYPE: KANONENTURM -> RUINE; footprint: (3, 3) -> (2, 2); flag word: 0x0084 -> 0x0026; radius: 14 -> 0; CONPO2: 3,0,3 0,3,0 0,8,1 8,3,2 -> -; costs: (650, 10, 12, 16, 0) -> (0, 0, 0, 0, 0) |

Other HEAD differences only (bytes listed; none of the columns above) (2): 545 (0x1a,0x1b,0x32,0x33), 1330 (0x02)

#### HE `Anlagen.classic`

- 894 types
- Types only in GOG (29): 525 Settler's house, 526 Settler's house, 546 Citizen's house, 547 Citizen's house, 584 Aristocrats, 585 Aristocrats, 1384 Palace, arch element, 1385 Main Palace, 1387 Palace gate, 1388 Hall of Generals, 1389 Lions' portal, 1390 Equestrian statue, 1391 Fountain of Venus, 1392 Palace wing, 1393 Ornamental garden, 1394 ~Prunkflagge01, 1395 Decorative flag, 1539 ~Vulkan02, 2900 ~krokodilhoehle, 2901 ~loewenhoehle, 2902 ~raeuberhoehle, 2903 ~wolfshoehle, 2904 ~raeuberhoehle_sued, 2905 ~einsiedelei, 2906 ~monstergrab, 2907 ~Spinnennetz, 2908 ~Spinnennetz_leer, 2909 ~monstergrab_franki, 3215 ~Schnitzel_Kloster01
- Types only in HE Anlagen.classic (2): 510 RUINE 1x1, 512 RUINE 4x4

Types in both whose footprint, flags, arrows, costs, radius, crop, upkeep, goods or PRODTYPE differ (77):

| id | PRODTYPE (GOG) | GOG -> HE |
|---|---|---|
| 511 | PIONIER | PRODTYPE: PIONIER -> RUINE; flag word: 0x0084 -> 0x0026; CONPO2: 3,0,3 6,0,3 0,3,0 0,6,0 3,11,1 6,11,1 11,3,2 11,6,2 -> -; costs: (0, 0, 3, 0, 0) -> (0, 0, 0, 0, 0) |
| 513 | PIONIER | PRODTYPE: PIONIER -> RUINE; flag word: 0x0084 -> 0x0026; radius: 24 -> 0; CONPO2: 3,0,3 6,0,3 0,3,0 0,6,0 3,11,1 6,11,1 11,3,2 11,6,2 -> -; costs: (0, 0, 3, 0, 0) -> (0, 0, 0, 0, 0) |
| 566 | KAUFLEUTE | PRODTYPE: KAUFLEUTE -> RUINE; flag word: 0x0084 -> 0x0026; radius: 24 -> 0; CONPO2: 3,0,3 6,0,3 0,3,0 0,6,0 3,11,1 6,11,1 11,3,2 11,6,2 -> -; costs: (0, 5, 5, 4, 0) -> (0, 0, 0, 0, 0) |
| 567 | KAUFLEUTE | PRODTYPE: KAUFLEUTE -> RUINE; flag word: 0x0084 -> 0x0026; radius: 24 -> 0; CONPO2: 3,0,3 6,0,3 0,3,0 0,6,0 3,11,1 6,11,1 11,3,2 11,6,2 -> -; costs: (0, 5, 5, 4, 0) -> (0, 0, 0, 0, 0) |
| 701 | ROHSTOFF | costs: (5, 0, 0, 0, 0) -> (0, 0, 0, 0, 0) |
| 703 | ROHSTDUERR | costs: (5, 0, 0, 0, 0) -> (0, 0, 0, 0, 0) |
| 705 | ROHSTOFF | costs: (5, 0, 0, 0, 0) -> (0, 0, 0, 0, 0) |
| 707 | ROHSTDUERR | costs: (5, 0, 0, 0, 0) -> (0, 0, 0, 0, 0) |
| 709 | ROHSTOFF | costs: (5, 0, 0, 0, 0) -> (0, 0, 0, 0, 0) |
| 711 | ROHSTDUERR | costs: (5, 0, 0, 0, 0) -> (0, 0, 0, 0, 0) |
| 713 | ROHSTOFF | costs: (5, 0, 0, 0, 0) -> (0, 0, 0, 0, 0) |
| 715 | ROHSTDUERR | costs: (5, 0, 0, 0, 0) -> (0, 0, 0, 0, 0) |
| 717 | ROHSTOFF | costs: (5, 0, 0, 0, 0) -> (0, 0, 0, 0, 0) |
| 719 | ROHSTDUERR | costs: (5, 0, 0, 0, 0) -> (0, 0, 0, 0, 0) |
| 721 | ROHSTOFF | costs: (5, 0, 0, 0, 0) -> (0, 0, 0, 0, 0) |
| 723 | ROHSTDUERR | costs: (5, 0, 0, 0, 0) -> (0, 0, 0, 0, 0) |
| 725 | ROHSTOFF | costs: (5, 0, 0, 0, 0) -> (0, 0, 0, 0, 0) |
| 727 | ROHSTDUERR | costs: (5, 0, 0, 0, 0) -> (0, 0, 0, 0, 0) |
| 729 | ROHSTOFF | costs: (5, 0, 0, 0, 0) -> (0, 0, 0, 0, 0) |
| 731 | ROHSTDUERR | costs: (5, 0, 0, 0, 0) -> (0, 0, 0, 0, 0) |
| 733 | ROHSTOFF | costs: (5, 0, 0, 0, 0) -> (0, 0, 0, 0, 0) |
| 735 | ROHSTDUERR | costs: (5, 0, 0, 0, 0) -> (0, 0, 0, 0, 0) |
| 737 | ROHSTOFF | costs: (5, 0, 0, 0, 0) -> (0, 0, 0, 0, 0) |
| 739 | ROHSTDUERR | costs: (5, 0, 0, 0, 0) -> (0, 0, 0, 0, 0) |
| 741 | ROHSTOFF | costs: (5, 0, 0, 0, 0) -> (0, 0, 0, 0, 0) |
| 743 | ROHSTDUERR | costs: (5, 0, 0, 0, 0) -> (0, 0, 0, 0, 0) |
| 745 | ROHSTOFF | costs: (5, 0, 0, 0, 0) -> (0, 0, 0, 0, 0) |
| 747 | ROHSTDUERR | costs: (5, 0, 0, 0, 0) -> (0, 0, 0, 0, 0) |
| 749 | ROHSTOFF | costs: (5, 0, 0, 0, 0) -> (0, 0, 0, 0, 0) |
| 751 | ROHSTDUERR | costs: (5, 0, 0, 0, 0) -> (0, 0, 0, 0, 0) |
| 753 | ROHSTOFF | costs: (5, 0, 0, 0, 0) -> (0, 0, 0, 0, 0) |
| 755 | ROHSTDUERR | costs: (5, 0, 0, 0, 0) -> (0, 0, 0, 0, 0) |
| 757 | ROHSTOFF | costs: (5, 0, 0, 0, 0) -> (0, 0, 0, 0, 0) |
| 759 | ROHSTDUERR | costs: (5, 0, 0, 0, 0) -> (0, 0, 0, 0, 0) |
| 811 | PLANTAGE | crop: 712 -> 713 |
| 813 | PLANTAGE | CONPO2: 3,0,3 0,3,0 5,8,1 8,4,2 -> 0,3,0 5,8,1 8,7,2 |
| 1006 | HANDWERKWAFFEN | upkeep: (30, 15) -> (30, 20) |
| 1012 | HANDWERKWAFFEN | upkeep: (40, 15) -> (30, 15) |
| 1036 | HANDWERKWAFFEN | upkeep: (35, 17) -> (50, 20) |
| 1310 | MARKTSTAND | CONPO2: 0,2,1 -> 1,2,1 |
| 1326 | GALGEN | radius: 24 -> 0 |
| 1327 | GALGEN | radius: 24 -> 0 |
| 1339 | GERICHT | costs: (600, 4, 7, 6, 0) -> (400, 4, 3, 4, 0); upkeep: (20, 0) -> (40, 0) |
| 1379 | HOCHSCHULE | CONPO2: 0,0,3 3,0,3 6,0,3 9,0,3 12,0,3 15,0,3 18,0,3 21,0,3 0,0,0 0,3,0 0,6,0 0,9,0 0,12,0 0,15,0 0,18,0 0,21,0 0,23,1 7,23,1 9,23,1 23,23,1 23,0,2 23,3,2 23,6,2 23,9,2 23,21,2 -> 0,0,3 3,0,3 6,0,3 9,0,3 12,0,3 15,0,3 18,0,3 21,0,3 0,0,0 0,3,0 0,6,0 0,9,0 0,12,0 0,15,0 0,18,0 0,21,0 0,23,1 7,23,1 9,23,1 23,23,1 23,0,2 23,5,2 23,6,2 23,9,2 23,21,2 |
| 2229 | KANONENTURM | PRODTYPE: KANONENTURM -> RUINE; footprint: (3, 3) -> (2, 2); flag word: 0x0084 -> 0x0026; radius: 14 -> 0; CONPO2: 3,0,3 0,3,0 0,8,1 8,3,2 -> -; costs: (650, 10, 12, 16, 0) -> (0, 0, 0, 0, 0) |
| 2555 | ZIERDE | CONPO2: 0,0,3 0,4,0 0,5,1 4,5,1 5,5,2 -> - |
| 3000 | MARKT | PRODTYPE: MARKT -> HAUPT |
| 3100 | MARKT | PRODTYPE: MARKT -> HAUPT |
| 3200 | MARKT | PRODTYPE: MARKT -> HAUPT |
| 3302 | MARKT | PRODTYPE: MARKT -> HAUPT |
| 3400 | MARKT | PRODTYPE: MARKT -> HAUPT; CONPO2: 0,0,3 0,3,0 3,8,1 8,3,2 -> - |
| 3401 | PIONIER | CONPO2: 3,0,3 0,3,0 -> - |
| 3402 | MILITAR | CONPO2: 7,0,3 0,4,0 14,8,2 -> - |
| 3403 | PIONIER | CONPO2: 4,0,3 10,0,3 4,11,1 10,11,1 12,11,1 14,9,2 -> - |
| 3404 | KAPELLE | CONPO2: 10,0,3 10,17,1 -> - |
| 3405 | PIONIER | CONPO2: 0,0,3 3,0,3 6,0,3 13,0,3 15,0,3 18,0,3 0,0,0 0,3,0 0,6,0 0,9,0 0,12,0 0,15,0 0,17,1 3,17,1 6,17,1 12,17,1 15,17,1 18,17,1 20,0,2 20,3,2 20,6,2 20,9,2 20,12,2 20,15,2 -> - |
| 3406 | MARKTSTAND | CONPO2: 0,3,0 0,8,1 8,3,2 -> - |
| 3407 | PIONIER | CONPO2: 0,8,0 3,8,1 8,5,2 -> - |
| 3408 | DENKMAL | CONPO2: 3,0,3 0,3,0 3,8,1 8,3,2 -> - |
| 3409 | PLANTAGE | CONPO2: 1,8,1 8,3,2 -> - |
| 3410 | HANDWERK | CONPO2: 1,0,3 6,0,3 -> - |
| 3414 | DENKMAL | CONPO2: 5,8,1 8,6,2 -> - |
| 3415 | PIONIER | CONPO2: 5,8,1 -> - |
| 3416 | WEIDETIER | CONPO2: 3,0,3 0,3,0 3,8,1 8,3,2 -> - |
| 3500 | MARKT | PRODTYPE: MARKT -> HAUPT |
| 3506 | EISLOCH | goods: ([], ['EISFISCH']) -> ([], []) |
| 3602 | MARKT | PRODTYPE: MARKT -> HAUPT |
| 3700 | MARKT | PRODTYPE: MARKT -> HAUPT |
| 3800 | FELSEN | PRODTYPE: FELSEN -> HAUPT; footprint: (4, 4) -> (4, 5); flag word: 0x8018 -> 0x0084; CONPO2: 0,6,0 11,9,2 -> -; goods: ([], []) -> ([], ['ALL']) |
| 3801 | FELSEN | PRODTYPE: FELSEN -> HANDWERK; flag word: 0x8018 -> 0x0084; CONPO2: 0,0,3 0,3,0 0,6,0 8,11,1 -> - |
| 3802 | FELSEN | PRODTYPE: FELSEN -> DENKMAL; flag word: 0x8018 -> 0x0084; CONPO2: 9,11,1 11,11,2 -> - |
| 3803 | FELSEN | PRODTYPE: FELSEN -> PIONIER; footprint: (1, 1) -> (2, 2); flag word: 0x8018 -> 0x0084 |
| 3804 | FELSEN | PRODTYPE: FELSEN -> PIONIER; flag word: 0x8018 -> 0x0084 |
| 3805 | FELSEN | PRODTYPE: FELSEN -> STEIN; footprint: (2, 2) -> (1, 1); flag word: 0x8018 -> 0x003a |
| 3850 | MARKT | PRODTYPE: MARKT -> HAUPT |
| 3950 | MARKT | PRODTYPE: MARKT -> HAUPT |
| 3953 | PIONIER | CONPO2: 3,0,3 0,6,0 3,8,1 8,3,2 -> - |

Other HEAD differences only (bytes listed; none of the columns above) (75): 501 (0x0b,0x1a,0x1b,0x32,0x33), 503 (0x1a,0x1b,0x32,0x33), 505 (0x1a,0x1b,0x32,0x33), 507 (0x1a,0x1b,0x32,0x33), 509 (0x1a,0x1b,0x32,0x33), 520 (0x0b,0x1a,0x1b,0x32,0x33), 521 (0x1a,0x1b,0x32,0x33), 522 (0x1a,0x1b,0x32,0x33), 523 (0x1a,0x1b,0x32,0x33), 524 (0x1a,0x1b,0x32,0x33), 540 (0x0b,0x1a,0x1b,0x32,0x33), 541 (0x1a,0x1b,0x32,0x33), 542 (0x1a,0x1b,0x32,0x33), 543 (0x1a,0x1b,0x32,0x33), 544 (0x1a,0x1b,0x32,0x33), 545 (0x1a,0x1b,0x32,0x33), 560 (0x0b,0x1a,0x1b,0x32,0x33), 561 (0x1a,0x1b,0x32,0x33), 562 (0x1a,0x1b,0x32,0x33), 563 (0x1a,0x1b,0x32,0x33), 564 (0x1a,0x1b,0x32,0x33), 565 (0x1a,0x1b,0x31,0x32,0x33), 568 (0x0a), 580 (0x0b,0x1a,0x1b,0x32,0x33), 581 (0x1a,0x1b,0x32,0x33), 582 (0x1a,0x1b,0x32,0x33), 583 (0x1a,0x1b), 704 (0x13), 724 (0x13), 814 (0x13,0x21,0x22,0x32,0x33), 819 (0x32,0x33), 821 (0x32,0x33), 823 (0x32,0x33), 1013 (0x13), 1029 (0x32,0x33), 1030 (0x32,0x33), 1034 (0x32,0x33), 1313 (0x02), 1315 (0x02), 1316 (0x02), 1317 (0x02), 1318 (0x02), 1330 (0x02), 1341 (0x31), 1342 (0x31,0x32,0x33), 2230 (0x14,0x15,0x16,0x32,0x33), 2231 (0x14,0x15,0x16,0x32,0x33), 2232 (0x2c,0x2d), 2246 (0x14,0x15,0x16,0x32,0x33), 2247 (0x14,0x15,0x16,0x32,0x33), 2248 (0x2c,0x2d,0x32,0x33), 2516 (0x2c,0x2d,0x32,0x33), 2517 (0x2c,0x2d), 2518 (0x2c,0x2d), 2519 (0x2c,0x2d), 2529 (0x2c,0x2d), 2536 (0x2c,0x2d), 2537 (0x2c,0x2d), 2552 (0x2c), 2564 (0x2c), 2566 (0x2c), 2567 (0x2c), 2568 (0x2c), 2569 (0x2c), 2672 (0x32,0x33), 3007 (0x31,0x32,0x33), 3862 (0x16), 3962 (0x31,0x32,0x33), 3964 (0x2c,0x32,0x33), 3965 (0x2c), 3966 (0x2c,0x32,0x33), 3967 (0x2c), 3968 (0x2c,0x32,0x33), 3971 (0x31), 3984 (0x2c)

#### HE `Anlagen.gfx`

- 1 ANLAGEN entries, none with a HEAD record: graphics data, not type data; not diffed


### 4. Arrow cross-check against the owner's HE recording

| owner (HE recording) | id | PRODTYPE | WxH | CONPO2 arrows | per side -x/+y/+x/-y | raw (x3,y3,side) | match |
|---|---|---|---|---|---|---|---|
| forester 3 | 801 | PLANTAGE | 3x3 | 3 | 0/1/1/1 | 3,0,3 4,8,1 8,4,2 | yes |
| sheep farm 3 | 807 | SCHAFFARM | 3x3 | 3 | 0/1/1/1 | 3,0,3 4,8,1 8,4,2 | yes |
| hunter 4 | 805 | JAGDHAUS | 3x3 | 4 | 1/1/1/1 | 4,0,3 0,3,0 3,8,1 8,3,2 | yes |
| small farm 4 | 809 | UNIVERSAL | 3x3 | 4 | 1/1/1/1 | 2,0,3 0,5,0 4,8,1 8,3,2 | yes |
| fisher 4 | 810 | FISCHER | 3x4 | 4 | 1/1/1/1 | 3,0,3 0,3,0 4,11,1 8,3,2 | yes |
| tanner 4 | 1023 | HANDWERK | 3x3 | 4 | 1/1/1/1 | 7,0,3 0,6,0 3,8,1 8,3,2 | yes |
| mine 1 front + 2 back | 1203 | MINE | 3x4 | 3 | 1/1/1/0 | 0,10,0 3,11,1 8,10,2 | yes |
| mine 1 front + 2 back | 1205 | MINE | 3x4 | 3 | 1/1/1/0 | 0,10,0 3,11,1 8,10,2 | yes |
| mine 1 front + 2 back | 1206 | MINE | 3x4 | 1 | 0/1/0/0 | 3,11,1 | NO |
| mine 1 front + 2 back | 1210 | MINE | 3x4 | 1 | 0/1/0/0 | 3,11,1 | NO |
| mine 1 front + 2 back | 1212 | MINE | 3x4 | 1 | 0/1/0/0 | 3,11,1 | NO |
| mine 1 front + 2 back | 1214 | MINE | 3x4 | 1 | 0/1/0/0 | 3,11,1 | NO |
| mine 1 front + 2 back | 1216 | MINE | 3x4 | 1 | 0/1/0/0 | 3,11,1 | NO |
| mine 1 front + 2 back | 1217 | MINE | 3x4 | 1 | 0/1/0/0 | 3,11,1 | NO |

Notes (L unless stated):
- The HE game loads `Anlagen.addon` (x64 AnnoGame/AnnoFrame name it; `docs/research/he-vs-gog-comparison.md` 2), which is byte-identical to GOG `Anlagen.dat` (section 3), so the recording checks the same CONPO2 bytes (P).
- Forester, sheep farm, hunter, small farm, fisher (810) and tannery (1023) match the recorded counts exactly (P).
- Mines: all eight MINE types are 3x4 with flag word 0x3096 (class 0x3000). 1203 (salt) and 1205 (ore) carry three arrows: one on the +y face at its middle (3,11) and one each on the -x and +x faces at y3 = 10, i.e. both in the last tile row next to the +y face. That is 3 arrows = the recorded 1 + 2. Reading the +y arrow as the recorded "front" and the two side arrows as the "back" pair depends on the screen orientation of side +y at the recorded turn, not decoded here (L). The other six mines (1206 gold, 1210 ore, 1212 gold, 1214 gem, 1216/1217 large ore) carry a single +y arrow (3,11,1): if the recorded mine was one of those, it is a mismatch. Which mine id the recording shows is not known to this inventory.
- 3510 (`~Eskimo_Fischer01`, FISCHER 4x3) has ENTRANCE but no CONPO2, so no arrows in the preview; it is a scenario-range entry (3xxx), not the player fisher (P).

