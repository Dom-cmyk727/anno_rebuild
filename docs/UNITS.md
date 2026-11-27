# UNITS: every moving figure of the original, as a reference

**Standing reference (owner, 2026-11-17):** "a concise report that is double checked on gog and HE data
and code containing ALL ingame units that will be used as reference for the project called UNITS.md".
Every packet that touches a figure (ship, soldier, siege engine, scout, worker, carrier, resident, animal,
bird, fish, effect) reads this file first and updates it when it learns something new, with evidence.
It does for figures what `docs/BUILDINGSOBJECTS.md` does for buildings.

**Sources:**
- GOG 2.0.0.5 (`game/app`): `FigurenHeader.dat`, plus `AnnoGame.dll` and `AnnoWelt.dll` read by static
  disassembly.
- History Edition (HE, Steam): `FigurenHeader.dat`, `AnnoGameAddOn.dll` and `AnnoWeltAddOn.dll` (32-bit,
  with RTTI class names; called HE86, as in `docs/re/forestry.md` §8).
- The repo's RE notes, which this file indexes rather than repeats:
  - `docs/re/`: ship-navigation, ship-cargo, ship-founding, ship-placement, ship-trade-routes,
    fire-and-ships, wildlife, hunter, fisher, whaler, transport, population §14, annowelt, annogame,
    original-unit-import;
  - `docs/sim/original-unit-import.md`.

**Inventory (never committed):** `python -I tools/units_inventory.py [--game-dir DIR] [--he-dir DIR]
[--out FILE]`. It reads `ANNO1503_GAME_DIR` by default and writes `cache/units/inventory.md`:
- one row per figure type, with every decoded `HEAD`/`LOGIC` field;
- a summary by BEWOHNTYPE;
- the GOG/HE record diff.

The per-figure values are the original's data and stay out of git. This file keeps the method, the field
map, the findings and the counts.

**Tags:** **D** = read in the code (module + address); **P** = parsed from a file; **S** = structural
comparison (constants, field offsets, call slots), as defined in `docs/re/forestry.md` §8; **L** = likely,
inferred from value patterns across the 368 types and not read in a consumer; **U** = unknown, an open
question. Addresses are GOG unless marked HE86. Branch `task/UNITS-REF-1`; open questions OQ-751..754.

## Quick answers

1. **368 figure types, 50 BEWOHNTYPEs in use (P).** GOG and HE have the same ids. The gameplay records
   (`HEAD`, `BEWOHNTYPE`, `ROUTETYPE`, `FORSCHTYPE`, `WARE`, `BAUWARE`, `LOGIC`, `SCHUSSPOS`, `OBJECTPOS`) are
   **byte-identical in all 368**. Only `PATH`/`VERSION2` of 370-376 and 453 differ, plus graphics packing
   (`docs/research/he-vs-gog-comparison.md` §3.2).
2. **A figure's class is chosen by its BEWOHNTYPE code (D).** The code is 1-based, index + 1 into the
   AnnoGame name table. The figure factory (`AnnoWelt 0x1009a110`, HE86 `0x100a1e80`) switches on it, and
   the case structure is the same in both editions. Part C maps each code to its class, with HE's class
   names: for example 113 → `tBewohnerSchiffPlayer` and 99-109, 111 → `tBewohnerSoldat`.
3. **Who moves them:**
   - **Player and AI command:** ships (113, plus whaler 178), the land army (codes 98-111), scouts (97) and
     pack donkeys (53).
   - **Not player-controlled:** everything else moves by a job or a sweep:
     - workers and carriers: production jobs (`ROUTETYPE` = job, forestry §2.2);
     - residents: job `0x35`;
     - wildlife: the island fauna sweep;
     - fish, whales, dolphins, birds: their own class ticks;
     - effects: spawned by fire, combat and events.
4. **Every military and ship type has `ROUTETYPE UNUSED` (P).** Their walks come from commands, not from a
   data job. How a player or AI order becomes a route for soldiers is not traced (OQ-753). Ship orders are
   traced (`docs/re/ship-navigation.md`).
5. **Combat numbers sit in `LOGIC` (D for the loader, L for the meanings, OQ-751):** `[0x1e]` = maximum hit
   points (ships 6000-14000, soldiers 520-1500, siege tower 3000), `[0x1c]` = attack strength (ships 300,
   cannon 400, mortar 600), `[0x12]/8` = attack reach in tiles, `[0x24]/8` = minimum reach (artillery 2-3),
   `[0x25]/8` = sight/action range (the rebuild already uses it for founding), and `[0x16]` = gold price.
6. **Birds land on ships (D).**
   - **Which birds:** only bird type **513** perches.
   - **Search:** at a decision, with chance 1 in 10 (`rand() % 10 == 5`), it scans ±10 tiles for objects of
     list mask `0x80` and takes the first whose class code is **113 (SHIP)** (`0x1004d1fd`-`0x1004d2a8`).
   - **Perch:** it follows the ship's position and facing (`0x1004d7b0`).
   - **Leaving:** it lets go **when the ship's state `+0x8c(0)` has bit 1 or 2 set** (`0x1004cb61`-`0x1004cb8b`),
     i.e. when the ship starts doing something. That is "birds land on ships that stay still".
   - **Randomness:** the original uses the CRT `rand()` here, so the result is cosmetic and
     non-deterministic.
   - HE86 has the same logic (S; Part D8).
7. **The rebuild models workers, carts, residents, ships and the imported scout/donkey crew.** It models
   **no land army, no combat, no wildlife, birds, fish, whales or effects**. Its BEWOHNTYPE table knows 21 of
   the 57 names, so MAEHER, FISCHER, ZIVILIST, HIRSCH and the others load as 0 (gap table).

## The rebuild's gaps (main `4dfdd9b7`)

| # | Gap | Part | Status |
|---|---|---|---|
| 1 | `kBewohn` (`src/sim/src/types.cpp` ~616) maps 21 names; 36 more (MAEHER 1 … TAENZER 130, the animals 49-66, birds 81-82, effects 145-178) load as 0 | B, C | sim: complete the table from Part C (data, not a limit) |
| 2 | No land army: soldiers 98-111, their commands, formation, combat, hit points, healing (MEDICUS), thief (DIEB 19) | D2-D3 | out of scope so far (`docs/sim/original-unit-import.md` Scope); figure veto only |
| 3 | No combat for ships either: cannons, projectile 1539, muzzle 1580, damage and sinking | D1 | `fire-and-ships.md` §3 has the reads; not modelled |
| 4 | No wildlife: animals 49-51 (`wildlife_sweep` seam only), hunter inert, fur trapper never sends | D7 | `wildlife.md`, `hunter.md`; OQ-572..586 |
| 5 | No birds, fish, dolphins, whales (whaler inert) | D8, D9 | not modelled |
| 6 | No effects or symbols: flames, smoke, water jets, plague, arrows, explosions, ore and treasure symbols, flags, sounds | D10 | not modelled (state.md O-12) |
| 7 | Scout and donkey: imported with the ship, no land movement | D4 | `original_units.cpp` |
| 8 | Dancers/jugglers (TAENZER 130, job `0x41`) and the firefighter (route `LOESCH`) | D5, D6 | not modelled |
| 9 | The `LOGIC`/`HEAD` field meanings are only partly settled (OQ-023, OQ-751) | B | RE |

# Part A: the inventory (P)

**Method.** `tools/units_inventory.py` reads every `FIGUREN/ENTRY` with the repo's reference reader
(`tools/figuren.py records()`). It decodes `HEAD` and `LOGIC` exactly as the loader does (Part B) and
compares the two editions record by record.

**Counts by BEWOHNTYPE** (figure types per name):
- **Ships and boats:** SHIP 12, WALFAENGER 1, EISSCHOLLE 2.
- **Army:** SCHWERT 25, LANZE 7, KAVALARIE 4, BOGEN 2, GEWEHR 2, MEDICUS 2, KANONIER 2, ARMBRUST 1,
  MUSKETIER 1, BEDIENUNG 1, MOERSER 1, BELAGERTURM 1, KATAPULT 1, DIEB 1.
- **Scouts and pack animals:** ERKUNDER 2, TRAGTIER 5, ESEL 1.
- **Workers and carriers:** MAEHER 18, WEIDETIER 13, TRAEGER 16, JAEGER 1, FISCHER 1, STEINMETZ 1,
  MARKTLER 1, SCHIFFWARE 2.
- **Townsfolk:** ZIVILIST 55, TAENZER 15.
- **Animals:** HIRSCH 29, RAUBTIER 7, WAL 1, FISCH 6, DELPHIN 1, VOGEL 8.
- **Effects, symbols and projectiles:** PFEIL 19, EXPLOSION 22, BLINK 5, FIRE 4, FLAGGE 11, ERZSYMBOL 13,
  SOUND 34, PRODSYMBOL 4, SCHATZ 2, SEUCHE 1, VULKANSYMBOL 1, WALSYMBOL 1, WASSERSTRAHL 1, JOKE 1.

Total: 368.

**Notes:**
- No shipped type uses HASE, KROKODIL, PELIKAN, VERKAEUFER or VULKAN, although the code table names them
  and HASE and VULKAN have classes.
- The names and the meanings in Part D are the original identifiers, not display text. The display names
  come from `Texte.dat` and are not committed.
- **Leftover fill in the data (P):** `HEAD` u32 `[0x10]` holds `0xCCCCCCCC` (MSVC's debug fill) in many
  non-ship types. The loader copies it raw to `+0x108`, which is the hull half-extent for ships
  (`fire-and-ships.md` §3.1). A reader must use `+0x108` only where a consumer does (ships, OQ-751).

# Part B: the figure-type record (`AnnoGame 0x1000fe00`, type vtable `0x10021d10` slot `+0x58`; D)

Read for this file instruction by instruction. **HE86's loader (`AnnoGameAddOn 0x10012460`) writes the
same type fields with the same rules (S), in SSE instead of x87.** All products are by 0.125 and exact,
so no value differs.

**HEAD** (16 or 20 bytes):

| byte | → field | getter | meaning |
|---|---|---|---|
| f32 `0x00` | `+0x2c` | `+0x64(0)` | turn-step length (`fire-and-ships.md` §3.1), L |
| f32 `0x04` | `+0x10` | `+0x60(x)` = x · speed | walking speed (D, `production.md` §5.4) |
| u8 `0x08` | `+0x104` | - | U |
| u16 `0x0a`, `0x0c` | `+0xe6`, `+0xe8` | - | projectile and muzzle/impact figure ids (values in the PFEIL/EXPLOSION id ranges; ship 1539/1580, D in `fire-and-ships.md` §3.1) |
| u32 `0x10` | `+0x108` | property 5 | ship hull half-extent (D for ships); fill elsewhere |

**Other records:**
- `BEWOHNTYPE` → `+8`: the class code, found by name through game slot `+0x48` kind `0x1f` for i < 0xc1 and
  returned by slot `+0x50`.
- `ROUTETYPE` → `+0xc`: the job id (forestry §2.2).
- `FORSCHTYPE` → `+0x102`: the research that unlocks the type (getter `+0x8c`; kind `0x38`, 65 names).
- `WARE` → `+0x100`: the goods the figure is (getter `+0x80`).
- `BAUWARE` → `+0xfc + i`: goods needed to build it (getter `+0x98(i)`).
- `OBJECTPOS` holds `SCHUSS`, `FLAGGE`, `FIRE` and `SHOTWI` point lists (counts `+0xee`, `+0xef`, `+0xf0`,
  `+0xf1`). `SCHUSSPOS` is the older form of `SCHUSS`. They are the muzzle, flag and fire anchor points.

**LOGIC** (28-44 bytes, read into a zeroed 44-byte buffer):

| byte | → field | getter | meaning (tag) |
|---|---|---|---|
| f32 `0x00`, `0x04`, `0x08`, `0x0c` | `+0x34`, `+0x30`, `+0x38`, `+0x3c` (≤ 0 → 1.0, 1.0, 4.0, 4.0) | `+0x64(2)`, `+0x64(1)`, `+0x64(3)`, `+0x64(4)` | `[4]` = job wait (D, forestry §2.8); others U |
| u8 `0x10` | `+0xff` | `+0xac` | walking layer (D) |
| u8 `0x11` | `+0xf2` = v << 8 | `+0x84` | carry capacity; ships 50 t per hold (D/L) |
| u8 `0x12` | `+0x14` = v/8 | `+0x68(0)` | attack reach, tiles (L: melee 0.5-0.9, archers 8-9, artillery 9-10, ships 8) |
| u8 `0x13`, `0x14` | `+0xf6`, `+0xf7` | `+0x88(0)`, `+0x88(1)` | ships: holds (2-8) and cannons (2-12) (L; `ship-founding.md` §14.1 reads 0x13 as holds, 0x14 as unit slots) |
| u8 `0x15` | `+0x101` | `+0x94(2)` | ships 10-25, soldiers 1-5: cannon capacity (`ship-founding.md`) / crew cost (U) |
| u16 `0x16` | `+0xf4` | `+0x94(1)` | gold price (L: ships 1500-4000, soldiers 90-220, artillery 300-400, tower 700) |
| u16 `0x18` | `+0xe4` = v & `0xfbff` | `+0x70` | flags; bit 10 is cleared at load (U) |
| u16 `0x1a` | not stored | - | - |
| u16 `0x1c` | `+0xec` | `+0x9c` | attack strength (L) |
| u16 `0x1e` | `+0xea` | `+0x90` | maximum hit points (L; the scout HP reader `0x100498e0`, `ship-navigation.md` §7.8) |
| u8 `0x20`-`0x22` | `+0xf9`-`+0xfb` | `+0x94(0, i)` = v << 8 | amount of `BAUWARE[i]` in tons (L: ships wood/cloth/rope 15/8/5 … 40/15/10) |
| u8 `0x24` | `+0x1c` = v/8 | `+0x68(2)` | minimum reach (L: catapult 3, cannon 2, mortar 3, else 0) |
| u8 `0x25` | `+0x24` = v/8 (0 → 7.0) | `+0x68(4)` | sight/action range (D in the rebuild's founding read: ships 16, scouts 7) |
| u8 `0x26` | `+0xf8` | `+0x88(2)` | other crew (`ship-founding.md` §14.1) |
| u8 `0x27` | `+0x18` = max(`[0x12]`, v)/8 | `+0x68(1)` | upper reach (U) |
| u8 `0x28` | `+0x28` = v/8 (0 → 2.0) | `+0x68(5)` | avoid radius (`ship-navigation.md`) |
| u8 `0x29` | `+0x103` | `+0xb0(0)` | U |

The L meanings come from value patterns over all 368 types (`cache/units/inventory.md`). The consumers
that would make them D are OQ-751.

# Part C: BEWOHNTYPE code → runtime class (D; factory `0x1009a110`, HE86 `0x100a1e80`)

**Code table (D+P):**
- The names come from AnnoGame (GOG table ending at `0x10024550 + 0x8ac`; HE86 `0x100d0920` region).
- The table is **identical in GOG and HE86**. Codes come in blocks of 16:
  - workers 1-7, carriers 17-19, residents 33, land animals 49-54, water animals 65-66, birds 81-82;
  - scout and army 97-111, ship 113, seller/dancer 129-130;
  - symbols 145-155, projectiles and effects 161-164, special ships 177-178, `WALKDEST` 193.
- The factory reads code − 1 into a byte table and a jump table: GOG `0x1009a7e8`/`0x1009a774`, HE86
  `0x100a2508`/`0x100a2494`.

**Code → class** (the HE86 size is mostly GOG + 4, because the figure base is 4 bytes longer in HE86, forestry §8.0):

| code(s) | name(s) | GOG ctor → vtable | size | HE86 class |
|---|---|---|---|---|
| 1-3, 5, 6, 17 | MAEHER, WEIDETIER, JAEGER, STEINMETZ, MAEHERSPEZ, TRAEGER | `0x1003b7c0` → `0x100c4950` | `0x7c` | `tBewohnerProduktion` |
| 4 | FISCHER | same, then `0x100cb4d8` | `0x7c` | `tBewohnerFischer` |
| 18 | MARKTLER | same, then `0x100cb1b0` | `0x80` | `tBewohnerProdKarren` |
| 130 | TAENZER | same, then `0x100cb344` | `0x7c` | `tBewohnerTaenzer` |
| 7 | SCHIFFWARE | `0x100444a0` → `0x100c5160` | `0xfc` | `tBewohnerSchiffWare` |
| 19 | DIEB | `0x1004a4e0` → `0x100c5804` | `0x9c` | `tBewohnerSoldatDieb` |
| 33 | ZIVILIST | `0x10054030` → `0x100c6a98` | `0x88` | `tBewohnerZivilist` |
| 49, 51 | HIRSCH, HASE | `0x10052c10` → `0x100c66fc` | `0xbc` | `tBewohnerWildTierNew` |
| 50 | RAUBTIER | `0x10053740` → `0x100c68e0` | `0xbc` | `tBewohnerWildTierWolf` |
| 53 | ESEL | `0x10037e70` → `0x100c3a30` | `0xa8` | `tBewohnerEsel` |
| 54 | WAL | `0x1004f410` → `0x100c5f98` | `0x90` | `tBewohnerWal` |
| 65 | FISCH | `0x1004f5d0` → `0x100c6140` | `0x84` | `tBewohnerWasser` |
| 66 | DELPHIN | `0x1004fe20` → `0x100c62d0` | `0x8c` | `tBewohnerWasserDelphin` |
| 81 | VOGEL | `0x1004cea0` → `0x100c5de0` | `0xb0` | `tBewohnerVogel` |
| 97 | ERKUNDER | `0x100374d0` → `0x100c3758` | `0xcc` | `tBewohnerErkunder` |
| 98-105, 107 | TRAGTIER, SCHWERT, BOGEN, ARMBRUST, LANZE, MUSKETIER, KAVALARIE, GEWEHR, BEDIENUNG | `0x10049c30` → `0x100c561c` | `0x98` | `tBewohnerSoldat` |
| 108, 109, 111 | KANONIER, MOERSER, KATAPULT | same | `0xac` | `tBewohnerSoldat` (the size has room for the crew, ship-founding §14.1) |
| 106 | MEDICUS | `0x1004a660` → `0x100c59e8` | `0x98` | `tBewohnerSoldatMedicus` |
| 110 | BELAGERTURM | `0x1004afb0` → `0x100c5bd8` | `0xb4` | `tBewohnerSoldBelagerTurm` |
| 113 | SHIP | `0x10043600` → `0x100c4da4` | `0x18c` | `tBewohnerSchiffPlayer` |
| 152 | VULKAN | `0x10039c10` → `0x100c3f70` | `0xd8` | `tBewohnerPfeilVulkan` |
| 161 | PFEIL | `0x100392e0` → `0x100c3ddc` | `0xd8` | `tBewohnerPfeil` |
| 162 | EXPLOSION | `0x100382a0` → `0x100c3c08` | `0x74` | `tBewohnerExplosion` |
| 163 | BLINK | `0x10032300` → `0x100c3274` | `0x34` | `tBewohnerBlink` |
| 164 | WASSERSTRAHL | `0x1003a2b0` → `0x100c4108` | `0x33c` | `tBewohnerPfeilWasser` |
| 177 | EISSCHOLLE | `0x100446a0` → `0x100c52e4` | `0x100` | `tBewohnerSchiffEisscholle` |
| 178 | WALFAENGER | `0x10044150` → `0x100c4fc0` | `0x10c` | `tBewohnerSchiffWalfaenger` |
| 52, 82, 129, 145-151, 153-155, other | KROKODIL, PELIKAN, VERKAEUFER, the symbols, FIRE, SOUND, JOKE | default arm (`0x1009a757`): no figure object | - | - |

**Notes on the table:**
- The symbols are island objects, created by a different factory (`0x10094e00`, fire-and-ships §2.3;
  whaler §2): ore 145, treasure 146, whale 147, production 148, plague 149, flag 150, fire 151, volcano
  153.
- HE86 also has classes no factory code creates: `tBewohnerWildTier` (the old one, OQ-580),
  `tBewohnerSimpl`, and the bases `tBewohnerBoden` and `tBewohnerSchiff`.

# Part D: units by group

Each subsection gives identity, control, movement, life cycle, combat, what the player sees, GOG/HE, and
the rebuild. Where a topic is settled elsewhere, the line points there.

### D1. Ships (code 113; whaler 178; ice floe 177)
- **Identity (P):** 12 SHIP types. The playable ones are 1501-1508: three trade sizes, three war sizes and
  two pirate hulls (their `WARE` names say which). 1509 is a second `HANDELSCHIFF2` entry (role U). 343/391/1510 are old or
  empty entries (`ship-navigation.md` §9.1). All have `ROUTETYPE UNUSED`, speed 0.030-0.048, 50 t holds,
  hit points 6000-14000 and attack 300 (L, Part B).
- **Control (D):**
  - Player commands come through the ship command path (`ship-navigation.md` §§1-7: orders, route engine
    `[world+0x6ab0]`, stepper `0x100428d0`).
  - Automatic trade routes: `ship-trade-routes.md`.
  - AI ships: CompLogic (`complogic-ai-behavior.md` part B; the pirate class `0x10063a00` and its attack
    walk `0x10055a30` are partly read).
- **Movement:** water layer, 16 directions (`0x10030e20`), avoid radius (Part B `[0x28]`).
- **Life cycle:**
  - built at a shipyard (WERFT; costs are `BAUWARE` + `[0x20..0x22]` tons + `[0x16]` gold, L), or placed by
    a scenario (`sze.md`);
  - it carries crew and land units (12 slots `+0x110..+0x13c`, `fire-and-ships.md` §3.2) and founds
    settlements (`ship-founding.md`);
  - sinking and capture are not read.
- **Combat:** cannons fire projectile 1539 with muzzle 1580 (`fire-and-ships.md` §3.1). The hold count
  limits cannons (goods `0x35`, `ui-ships-diplomacy.md` §2.2). The damage rule is not read (OQ-751).
- **What the player sees:** the ship panel and diplomacy are in `ui-ships-diplomacy.md` (OQ-660..669).
- **Whaler 178** (figure 1511): `tRouteWalfaenger`, `whaler.md`. **Ice floe 177** (1553/1554): a drifting
  ship-class object, not player-controlled.
- **GOG/HE:** data identical (P); factory arm identical (S). The ship tick was not compared (x64 or HE86).
- **Rebuild:** `ship*.cpp`, `route_*.cpp`, `freight.cpp`, `founding.cpp`, `original_units.cpp`. No combat.

### D2. Land army (codes 98-109, 111; thief 19)
- **Identity (P):**
  - foot soldiers: SCHWERT 25 types (most without `FORSCHTYPE`, e.g. 800-822; role U), LANZE 7, BOGEN 2, ARMBRUST 1,
    GEWEHR 2, MUSKETIER 1;
  - cavalry: KAVALARIE 4;
  - support: MEDICUS 2 (healer, class `tBewohnerSoldatMedicus`), BEDIENUNG 1 (gun crew), TRAGTIER 5 (pack
    animals, soldier class);
  - DIEB 1 (thief, `tBewohnerSoldatDieb`).

  The types with a `FORSCHTYPE` (research gate) are 701-706, 708-712, 716 and 724 (e.g. 704 needs
  `WAFFEN_FERTIGUNG_01`); this is L for "the recruitable line".
- **Values (L, Part B):** hit points 520-1500, attack 30-200, reach 0.5-0.9 tiles (melee) or 7-9 tiles
  (archers, muskets), price 90-220 gold, `BAUWARE` one or two weapon/armour goods of 1 t each.
- **Control:** player and AI commands. All have `ROUTETYPE UNUSED`, and the factory gives them the
  `tBewohnerSoldat` family. **The command path (selection, move, attack, formation) and the soldier tick are
  not traced (OQ-753).** Scenario units: `SOLDAT` records (`sze.md` "Units"). Facing has 8 directions
  (`0x10088b50`, fire-and-ships §3.3).
- **Life cycle:** recruited at a military building with goods and gold (L from `BAUWARE`/`[0x16]`), carried
  by ships; how one dies is not read.
- **GOG/HE:** data identical (P); factory arms identical (S); class names from HE86.
- **Rebuild:** not modelled (the figure veto only, `figure_veto.cpp`).

### D3. Siege and artillery (codes 108-111)
- **Identity (P):** KANONIER 710 (field cannon) and 1662 (speed 0, likely a tower's gun, L), MOERSER 711, KATAPULT
  706, BELAGERTURM 712 (siege tower, class `tBewohnerSoldBelagerTurm`).
- **Values (L):** hit points 1100 (3000 for the tower); attack 400-600 (1000 for the tower gun); reach 9-10
  tiles with a minimum of 2-3; layer 1.
- **Crew:** a crewed machine works only with an occupied crew slot (`ship-founding.md` §14.1). The crew is
  BEDIENUNG 713 (L).
- **Control and combat:** as D2 (OQ-753, OQ-751).
- **Rebuild:** the figure veto only.

### D4. Scout, donkey (codes 97, 53)
- **Identity:** ERKUNDER 510 (no hit points, role U) and 717 (`PIONIER`: 10 t capacity, carries `WERKZEUG`); ESEL 718.
- **Control:** player command. The tick is `0x10035f50` (`ship-navigation.md` §7.8) and the HP reader is
  `0x100498e0`. Allowed roles: ship → crew scout → held donkey
  (`docs/sim/original-unit-import.md`).
- **Rebuild:** imported with the ship (`original_units.cpp`); crew position mirrored
  (`ship_mover.cpp`); no land movement.

### D5. Workers and carriers (codes 1-7, 17, 18; jobs from `ROUTETYPE`)
All take class `tBewohnerProduktion` or a subclass (Part C), are sent by their building, and end after
delivering (forestry §2.13).

| who | figures | job | docs | rebuild |
|---|---|---|---|---|
| mowers, forester, small-farm worker, plantation workers | MAEHER 18 types (101, 102, 106, …) | `0x2a` | forestry.md, production.md | `harvest.cpp` |
| grazing animals of farms | WEIDETIER 13 (107, 109, 110, …) | `0x2d`/`0x2e` | forestry §8.5 | `harvest.cpp` |
| stonemason | 400 | `0x2c` | quarry-bricks | `mason.cpp` |
| fisher | 142 (`tBewohnerFischer`) | `0x2d` route 45 | fisher.md | `fisher.cpp` (gated) |
| hunter | 108 | `0x0d` (`tRouteJaeger`) | hunter.md | inert |
| workshop and inn carriers | TRAEGER (203, 204, 606, …) | `0x33` | transport §2, population §18 | `transport.cpp`, `inn.cpp` |
| firefighter | 145 (route `LOESCH`) | `0x20` (`tRouteFeuer`) | fire-and-ships §2.1 | not modelled |
| market cart | 220 (`tBewohnerProdKarren`) | `0x34` | transport §1 | `transport.cpp` |
| ship goods (crates) | SCHIFFWARE 1619/1620 | - | - | not modelled |

GOG/HE: identical (forestry §8.2 and the factory here).

### D6. Residents and dancers (codes 33, 130)
- **Residents:** ZIVILIST, 55 types, house walkers on job `0x35` (`tRouteEinkauf`). Read in
  `population.md` §14; rebuild `resident.cpp`, `resident_job.cpp`.
- **Dancers/jugglers:** TAENZER, 15 types (`tBewohnerTaenzer`, job `0x41` `tRouteTaenzer`). Not modelled.

### D7. Wildlife (codes 49-51; fauna sweep)
- **Identity (P):** deer/game HIRSCH, 29 types (`WARE FLEISCHTIER`/`PELZTIER`); predators RAUBTIER, 7 types
  (attack 170).
- **Classes:** 49 and 51 → `tBewohnerWildTierNew`, 50 → `tBewohnerWildTierWolf`.
- **Movement and life cycle:** created and removed by the island fauna sweep (slot `+0x194`, `0x10093480`;
  HE86 `0x10048370`, identical by structure). See `wildlife.md` §§1-10 and `hunter.md`; OQ-572..586.
- **Rebuild:** not modelled (seam only).

### D8. Birds (code 81)
- **Identity (P):** 8 VOGEL types (512, 513, 518-522, 525).
- **Class:** `tBewohnerVogel` (vtable `0x100c5de0`). Its own slots are `+0x7c` tick `0x1004ca50`, `+0x174`
  decision `0x1004cf20` and `+0x178` `0x1004cce0`. It also has the shared flying-object helpers
  `0x10044ad0`, `0x10044bd0` (animation step, capped at 10 000) and `0x10044f10`/`0x10044f40` (property
  get/set).
- **Spawning:** birds come from the per-sector animal spawner `0x10097880`. It looks up type 513 at
  `0x10097ad4` and creates `rand() % 3` (0-2) of them, behind a flag test (D; CRT `rand`, fire-and-ships
  §1.2).
- **Landing on ships (D):**
  - **The decision** (slot `+0x174`, `0x1004cf20`) runs when the bird's current action ends:
    - with the flight-path byte `+0x8c` set, it continues the path (`0x1004de70`);
    - in mode 3 (`+0x55`) it resets to mode 0;
    - otherwise it drops any perch (`+0x88 = 0`) and draws `r = rand() % 10` (`0x1004cfae`).
  - When `r == 5` and the bird is type 513, it queries the world grid: a box of ±10 tiles around itself,
    list mask `0x80`, up to 36 objects (`0x1004d1ec`-`0x1004d265`).
  - It takes the first object whose class code (`+0x50`) is `0x71` = **113, SHIP**, stores it at `+0x88`
    and flies to it (`0x1004d7b0`, `0x1004d9c0`).
  - Each tick while perched, the bird follows the ship's position and facing (`0x1004d7b0` reads ship
    slots `+0x54/+0x58/+0x5c` and `+0x68`, with a perch offset `+0xa0/+0xa8` rotated by the tables
    `0x100c3148`/`0x100c3158`).
  - It **lets go when the ship's state `+0x8c(0)` has bit 1 or 2 set** (`test al, 6`, `0x1004cb87`).
  - The search itself has no idle test: a bird can pick a moving ship and drop it at the next tick.
  - Every new decision drops the perch, so a perch lasts one bird action, and is renewed only by another
    1-in-10 draw.
  - Which ship state the bits mean, and how long a bird action lasts, is OQ-752.
- **GOG/HE (S):** HE86 has the same constants and the same branches: type `0x201` at `0x1002252a`, mask
  `0x80` at `0x1002256f`, code `0x71` at `0x10022592`, `test al, 6` at `0x10022154`.
- **Rebuild:** not modelled. It is cosmetic (CRT `rand`), so it belongs to the view/effects layer, never to
  the sim's PRNG.

### D9. Fish, dolphin, whale (codes 65, 66, 54)
- FISCH 6 types (`tBewohnerWasser`), DELPHIN 1200 (`tBewohnerWasserDelphin`), WAL 1202 (`tBewohnerWal`,
  15 t; whale symbol 1559).
- Fishing: `fisher.md` (the fish lifetime is not decoded). Whaling: `whaler.md`, OQ-591..595.
- Dolphins: only the class is known here. Rebuild: none.

### D10. Effects, projectiles, symbols (codes 145-178)
- **Moving effects:**
  - arrows, cannonballs and harpoons: PFEIL, 19 types, `tBewohnerPfeil`;
  - volcano bombs: 152 `tBewohnerPfeilVulkan`;
  - water jets: 164 `tBewohnerPfeilWasser`, figures 1613/1614;
  - explosions: EXPLOSION 22, `tBewohnerExplosion`;
  - flashes: BLINK 5, `tBewohnerBlink`.
- **Island objects** (no figure class; `0x10094e00`): flags, fire, plague, ore/treasure/whale/production
  symbols, sounds. See `fire-and-ships.md` §2.
- Which unit uses which projectile is in each type's `HEAD [0x0a]/[0x0c]` (Part B).
- Rebuild: none.

### D11. Pirates, traders, natives
- These are AI players, not figure types: player kinds natives 14, PIRATEN 15, VENECIA 16 (`annoscene.md`).
- Pirates use the ship hulls 1507/1508 (P: `WARE PIRATSCHIFF`).
- No figure type is a trader or a native person. VERKAEUFER (129) has a code but no class and no data entry
  (Part C).
- How the free trader's and natives' ships are chosen and ordered is CompLogic and is not traced
  (OQ-754).

# Part E: GOG against HE

- **Data (P):** all 368 ids are in both editions. The gameplay records are byte-identical. `PATH`/`VERSION2`
  differ in 370-376 and 453, and graphics differ in packing (he-vs-gog §3.2). Run the generator with
  `--he-dir` to reproduce this.
- **Code (S, HE86):**
  - **Figure-type loader** (`0x1000fe00` / `0x10012460`): same field map.
  - **Code table:** identical.
  - **Factory** (`0x1009a110` / `0x100a1e80`): same cases and same constructors per code. Object sizes are
    +4 in HE86 for the `tBewohnerBoden` family.
  - **Bird logic** (`0x1004cf20` / `0x100222a0`, `0x1004ca50` / `0x10021ff0`): same constants and branches.
  - **Worker and cart chain:** identical (forestry §8.2).
- **Not compared:** the HE x64 module's unit code, the soldier and ship ticks, combat.

# Part F: open questions

**Registered:**
- **OQ-751:** consumers of the `LOGIC`/`HEAD` fields whose meanings are L or U in Part B (hit points,
  attack, reach, price, flags, `[0x15]`, `[0x27]`, `[0x29]`, f32 `[0]`/`[8]`/`[0xc]`, `HEAD [0x08]`).
- **OQ-752:** the bird perch: what ship state bits 1 and 2 of `+0x8c(0)` are, how long a bird action
  lasts, and why only type 513 perches.
- **OQ-753:** the land army's command path and tick (player UI → command → route, AI CompLogic; formation,
  attack, healing, thief).
- **OQ-754:** how free traders', pirates' and natives' ships and figures are created and controlled.

**Older open questions this file touches:** OQ-023 (figure fields), OQ-572..586 (wildlife), OQ-580 (old
WildTier), OQ-591..595 (whaler), OQ-660..669 (ship UI).
