# Fire, plague, volcano and ship movement (`AnnoWelt.dll`, `AnnoGame.dll`)

**Status:** first pass (2026-10-17), static analysis only. Modules: `game/app/AnnoWelt.dll` and `game/app/AnnoGame.dll` (GOG 2.0.0.5, image base `0x10000000`), a few cross-checks in `AnnoScene.dll`. Tools: Ghidra 12.1.4 headless output in `cache/ghidra/plus/*.c`, `objdump -d` for every threshold, divisor and branch polarity quoted below, small Python scripts over `FigurenHeader.dat`, `Anlagen.dat` and the 50 `.sze` scenarios (scratch in `cache/re-fire/`). Addresses are virtual addresses in `AnnoWelt.dll` unless another module is named. Behaviour is paraphrased; no code is copied.

Confidence: **confirmed** = read directly in the decompiled code and checked in the disassembly; **likely** = strong inference from code structure, data or names, not traced end to end; **guess** = hypothesis, repeated under "Open".

Names used below (ours): *world* = the object built by `0x100a3980` (vtable `0x100cc92c`; the in-game subclass uses `0x100ccdd8`); *island* = vtable `0x100ca19c` (subclasses `0x100ca530`, `0x100cac2c`); *city* = a player's settlement on one island (update `0x1002ba80`); *building type* = `Anlagen.dat` entry object in `AnnoGame.dll` (vtable `0x1002185c`); *figure type* = `FigurenHeader.dat` entry object in `AnnoGame.dll` (vtable `0x10021d10`). "Time" is the `u16`/`u32` value the world passes down each tick (see §1.3).

## 1. Randomness and time

### 1.1 The shared random table (world slot `+0xb4`)
`0x100a1740` is the game's own random source: the world holds a `u16` index at `+0xfc` and a length `+0xfe` (= `0x800` = 2048, set in the constructor `0x100a3980`). Each call first advances the index, `index = (index + 1) mod 2048`, then returns `table[index]`, a `u16` from the read-only table at `0x100cb8f8`. **Confirmed.**

The table holds exactly the first 2048 outputs of the MSVC C runtime `rand()` seeded with 1 (41, 18467, 6334, 26500, 19169, …; values 21…32757, 1991 distinct). **Confirmed** (we regenerated it with the LCG `s = s·214013 + 2531011`, output `(s >> 16) & 0x7fff`, and compared all 2048 entries).

Seeding: the world constructor calls `srand(GetTickCount())` and sets the index to `rand() & 0x7ff` (`0x100a3980`); world slot `+0x70` (`0x100a3230`), key 8, sets `index = value mod 2048`, and slot `+0x74` (`0x100a3380`) key 8 returns it; `0x100af240` (world reset) sets it to 0. `AnnoScene.dll` `0x10002e30` creates the world and, if the scene's seed field `+0x14c` is non-zero, calls `SetValue(8, seed)`; the seed comes in as an argument of the scene loaders `AnnoScene.dll` `0x100025a0` / `0x100029f0`. **Confirmed** (calls), **likely** that this seed is the multiplayer session seed (caller not traced). Whether savegames store the index was not found (Open).

Every fire, plague and volcano decision below uses this table. All machines stay in step only while they make the same number of draws in the same order; a single extra draw anywhere (e.g. code that runs only for the local player) shifts every later decision.

### 1.2 The C runtime `rand()` (not shared)
The statically linked CRT `rand` is at `0x100b68a9`, `srand` at `0x100b689c` (per-thread state). Several **figure constructors re-seed it with `GetTickCount()`**: `0x10044b60` (base of vtable `0x100c5478`, called from four other constructors), `0x1004cea0`, `0x1004f5d0`. The building damage handler `0x10093c10` does the same before it spawns collapse dust. **Confirmed.** So every CRT `rand()` result is machine-local. Users found (27 functions, list from the decompile): most are draw/idle-animation/sound slots (wake foam in the ship draw `0x1003c6b0`, idle variants `0x100332b0`, `0x10044860`, …); some are in sim-looking code: collapse dust `0x1008bae0` (effect figure 1544 `Effekt_Hauszusammenbrechen` at random footprint tiles), animal/bird spawning per 16 × 16 sector `0x10097880`, the volcano symbol's animation phase `0x10066a40`, figure logic `0x1004cf20`, `0x1004f620`. **Confirmed** (calls); whether each of them changes checksummed state is not classified (Open).

### 1.3 Time
- World tick, in-game world vtable `0x100ccdd8` slot `+0x84` = `0x100ae040`: adds the signed `dt` argument to a 64-bit accumulator (`+0x100`/`+0x104`) and sets the world time `+0x10c` = accumulator `>> 1`. Then it updates all islands with that time (`0x100a2e60` → island slot `+0xe4` = `0x1008b760`), runs the timed event queue (`0x100add90`: delayed hits, see §2.4), and three more subsystems (`+0x114` slot `+0x20`, `+0x118` slot `+0x24`, `+0x69c8` slot `+0x14`). **Confirmed.** Who supplies `dt` and in what unit is in another module (Open; tick-loop note).
- Island update `0x1008b760`: processes `chunks · elapsed / 2000 + 1` of its 8 × 8 tile chunks per call (round-robin, so each chunk roughly every 2000 time units), another pass every 15000 units, then its object manager (`[10]` slot `+0x20`), which runs the fire and plague objects of §2 (**likely**), and its cities. **Confirmed** (structure). The work done per call depends on the elapsed time, so clients must see the same sequence of `dt` values, not only the same total.
- All fire/plague/city timers compare a 16-bit difference `(u16)(now − last)`. With the thresholds below (2500, 12500) the time unit is presumably milliseconds of game time (**guess**; it is half the `dt` sum, see Open).

## 2. Fire

### 2.1 Data
- Building type, `Anlagen.dat` `HEAD` record (read by the `Anlagen.dat` `ENTRY` reader `AnnoGame.dll` `0x10009240`; 56-byte buffer, shorter records zero-filled): `u16` at `0x2c` → type `+0x70` = **maximum hit points** (slot `+0xf0`, `0x1000a500`); `u8` at `0x31` → `+0x79` (slot `+0xf4`, `0x1000a510`) = **can burn** (non-zero); `u8` at `0x0a` → `+0x7a` (slot `+0x104`) = rubble pattern used on destruction (`0x10093c10`, table `0x100ca7c0`). **Confirmed** (code), byte offsets **likely** (Ghidra stack-variable naming, values plausible: P below).
- `PRODTYPE` (type `+0x10`, slot `+0x5c`) is a number from a name table (`AnnoGame.dll` `0x100138e0`): blocks of 32, group = `(id − 1) >> 5`: 0 production (1–17), 1 market/warehouse (33–36), 2 public (65–79, `FEUERWEHR` = 77), 3 houses (97 `PIONIER` … 101 `ARISTOKRATEN`), 4 military, 5 market stand, 6 roads/places/fences/bridges, 7 raw materials, 8 walls, 9 monuments, 10 rocks, 11 water, 12 ruins. **Confirmed.**
- `FIREPOS` (`AnnoGame.dll`, same reader): `n × 12` bytes = `n` points `(f32, f32, f32)` relative to the building; count at type `+0x68` (slot `+0xfc`), points at `+0x220`, read rotated by slot `+0x100` (`0x10008660`: rotation 1 maps `(x, y, z)` → `(z, y, −x)`, etc.). **Confirmed.**
- P (`Anlagen.dat`, 921 entries): can-burn byte 0: 661, 1: 14, 2: 185, 3: 61 (houses of pioneers/settlers 3, other houses and workshops 2); `FIREPOS` on 26 entries (1–6 points, e.g. pioneer house 2, merchant house 3, chapel 6). Max hit points e.g. pioneer house 2500, settler 4000, citizen 6000, merchant 7000, aristocrat 9000, fire brigade 6000, cathedral 30000.
- Figures: 1596 `EffektFeuer01`, 1598 `EffektFeuer02` (`BEWOHNTYPE FIRE`, 151), 1610 `Effekt_Feuerrauch01`, firefighter 145 `Feuerwehr\` (`TRAEGER`, `ROUTETYPE LOESCH`), water jets 1613/1614, plague 1568 `Effekt_DiePest` (`SEUCHE`, 149), volcano symbol 1565. P.
- Building instance state bits (slot `+0x100(id, on)` sets, `+0x104(mask)` tests): id `0x97` (FIRE) ↔ bit `0x40`, id `0x95` (SEUCHE) ↔ bit `0x10`. **Likely** (id − `0x91` = bit number; the pairs are used consistently in `0x100866f0`, `0x10086c70`, `0x1002b840`, `0x10028fd0`).

### 2.2 How a fire starts
There are three triggers; all end in island slot `+0x14c` = `0x100866f0` (*StartFire(x, y)*).

1. **Random fire per city** — `0x1002ba80` (city update), every time at least 12500 units have passed since its last run (`(u16)(now − last) ≥ 0x30d4`; afterwards `last = now`, so the period drifts with the tick size). **Confirmed.**
   - Allowed unless bit `0x08` is set in world value 6 (`+0x69d0`, game options) or in the player's value 4 (`(+0x20)` slot `+0xc4(4)`). **Confirmed** (code); meaning "disasters off" **likely**.
   - Risk: `s = 5·H0 + 5·H1 + 2·(H2 + H3 + H4)`, where `Hk` are the five `u16` counters at city `+0x18a` (houses per level: the house-upgrade code `0x1002cbe0` moves one count from the old to the new level; **likely**), weights from `0x100c2ec8`. `i = min(31, s / 16)` (signed). `b = table[i]` from `0x100dcc24`:
     `255 ×10, 191, 191, 159, 159, 127, 127, 127, 95, 95, 95, 63 ×4, 31 ×8`.
     If `b ≠ 255` and `rnd % b < 4` (one table draw) → pick a house. **Confirmed.** So below `s = 160` there is never a random fire; above, the chance per check is ≈ 4/b (2.1 % … 12.9 %).
   - House choice `0x1002b840`: walk the city's house list (category `0x61`, slot `+0x24`/`+0x18` of the building index at city `+0x144`) in list order, keep houses that are not burning (bit `0x40`) and whose level byte (slot `+0x88`, byte 0) is 0 or 1 (pioneers/settlers), **stop after 64**; if any, `StartFire` at the house chosen by `rnd % count` (one draw). **Confirmed.** Faithful quirk: in a large city only the first 64 eligible houses in list order can catch a random fire.
2. **Damage** — island slot `+0xcc` = `0x10093c10` (*SetHitPoints(x, y, hp, source, …)*, subclass `0x100cac2c`). The first hit on an undamaged building only creates its damage record (no ignition test). Afterwards, when the level `L = trunc(16·hp / maxhp)` changes, the type can burn and `hp > 0`: `b = 0x100cabc0[17·(source & 0xf) + max(L, 0)]`; if `b == 0` or `rnd % b == 0` (one draw only when `b ≠ 0`) → `StartFire`. **Confirmed.** Table rows (levels 0–16):
   - source 0: all 0 (always);
   - source 1: `0 ×7, 1, 1, 1, 1, 2, 2, 2, 3, 5, 255`;
   - source 2: `0, 0, 0, 1, 1, 1, 1, 2, 2, 2, 2, 3, 3, 4, 9, 15, 255`;
   - source 3: `0, 0, 1, 1, 1, 2, 2, 3, 3, 4, 5, 7, 9, 15, 31, 255, 255`.
   Fire damage itself passes source 0, so a burning building gets a new flame at every 1/16 step (§2.3). Delayed hits from the event queue (`0x100adb80`) pass the attacker's value from its slot `+0xb0(0)` (**confirmed**); which weapons give which source class is not traced (Open).
3. **Spread** from a burning neighbour (§2.3) and **scripted disasters**: the mission-script compiler in `AnnoScene.dll` `0x100170d0` knows the disaster names `VULCAN` = 0, `DROUGHT` = 1, `FIRE` = 2, `PLAGUE` = 3, `EARTHQUAKE` = 4 (**confirmed**); `AnnoScene.dll` `0x100137d0` calls island `+0x14c`/`+0x154` (**likely** the executor; not read).

*StartFire(x, y)* `0x100866f0`, **confirmed**:
- find the building origin from the tile record (byte 2: bits 0–2 / 3–5 = offset inside the footprint), its type and rotation (bits 6–7 of the record's second byte);
- count existing flames over the footprint (island object manager slot `+8`, kind mask `0x40`);
- if the type has `n > 0` `FIREPOS` points: start at point `rnd % n` (one draw) and take the first point (cyclically) that has no existing flame within distance² ≤ `0.02778889` (`0x100ca190`, radius ≈ 0.167 world units); none free → no new flame. Point position = building origin + footprint/2 (`0.5` steps) + rotated `FIREPOS` offset, height from the item's height byte (`0x100e2d90`);
- if `n == 0`: only if no flame exists yet, one flame at the footprint centre, height + type slot `+0x58`;
- create the flame figure `1596 + 2·(rnd & 1)` (one draw);
- owner of the tile (island `+0xec`) ≠ 15: owner's city statistic 9 += 1 (`+0x70(9, 1)`), building instance state FIRE = 1, world event `+0xd0(0x1c, pos, 9, 0, owner colour)` and notification `0x10086670(…, 0xf, …)` (message 15). **Confirmed** (calls), meaning of the event/message ids **likely** (alert, minimap mark).

Random draws per `StartFire`: 2 with `FIREPOS`, 1 without.

### 2.3 Burning and spreading (flame object)
Flame objects are island objects, not walking figures: island object factory `0x10094e00` maps `BEWOHNTYPE` 151 (`0x97`) to the flame class (constructor `0x10063d10`, vtable `0x100c7640`); 149 → plague (`0x10067470`, vtable `0x100c7f18`); 153 `VULKANSYMBOL` → `0x10066a40`. **Confirmed.**

Flame update, vtable slot `+0x6c` = `0x10063b30(now)`, **confirmed**:
- runs once per 2500 units: if `(u16)(now − last) ≥ 2500` then `last += 2500` (at most one step per call; a lagging flame catches up by one step per call);
- step counter `+0x28` += 1; when it exceeds 8 (every **9th** step, 22.5 k units) → spread attempt, counter = 0:
  - `dir = (rnd & 3) · 2` (one draw) — one of the four sides of the burning building;
  - `0x10064470(dir, range = 1, mask = 0xffffffbf)`: from the middle of that side, scan outward up to `range` tiles for a building whose `PRODTYPE` group bit (`1 << ((id − 1) >> 5)`) is in `mask` — every group except 6 (roads, places, fences, bridges); returns 0 or 1 candidates (**likely**, decompile partly garbled);
  - if a candidate exists: `rnd % count` (one draw), and if its type has hit points and can burn → `StartFire` there (1–2 more draws);
- every step: building under the flame (tile = `trunc(flame x)`, `trunc(flame z)`, x87 → `__ftol`), `M` = max hp, `h` = current hp (island `+0xd0`): `d = max(1, trunc((M + h) / 180))`; if `h − d > 0` → `SetHitPoints(h − d, source 0)` (may add a flame, §2.2), else destroy: island `+0x90(x, y, 6, …)` and an event via world sub-object `+0xc4(0, 0x32, 4, …, 0, 0)`. **Confirmed** (arithmetic: `0xb60b60b7` multiply, shift 7).

Every flame on a building damages it separately, so `k` flames take `k · d` per 2500 units; with one flame a building burns down in 125–128 steps (§Fixtures). The spread rate is per flame as well.

Extinguishing — island slot `+0x150` = `0x10086c70` (*ExtinguishFire(x, y)*): removes all flames on the footprint, subtracts their count from city statistic 9, clears the FIRE state. **Confirmed.** Callers:
- the firefighter's task: task factory `0x1003b0e0`, task type `0x20` → class `0x1003b570`; its arrival action (vtable `0x100c48d8` slot `+0x10` = `0x10057420`) calls `ExtinguishFire` at the task's target tile. **Confirmed** (call); that type `0x20` is the `LOESCH` route type of figure 145 is **likely**;
- building removal `0x10028fd0` (burning → extinguish, plague → cure). **Confirmed.**
- How a fire station chooses targets (radius, dispatch timing, walking speed) was not found (Open). No fire-station coverage term appears in the random-fire risk, the damage ignition or the spread code: fire brigades only react. **Confirmed** for those three functions.

Destruction path (`0x10093c10`, hp ≤ 0): `srand(GetTickCount())`, then over the footprint's rubble pattern each tile gets a rubble object and, if `rand() % 3 == 0` (CRT), collapse dust `0x1008bae0` (CRT positions). **Confirmed.** Dust is an effect figure (**likely** visual only).

### 2.4 Plague, riots, volcano
- **Plague** (same city check `0x1002ba80`, same disable bit 8): `i = clamp((P − 500) / 256, 0, 31)` (signed division toward zero), `P` = city `+0x2ec` slot `+4` (**likely** inhabitants); `b = 0x100dcc44[i]`: `255 ×4, 223 ×4, 191 ×4, 159 ×4, 127 ×4, 95 ×4, 63 ×4, 31 ×4`; `rnd % b < 4` → `0x1002b960` picks a house without plague (bit `0x10`) and calls island `+0x154` (`0x10086ef0`, *StartPlague*: house state SEUCHE, plague object 1568, statistic 8 += 1, event `0x1b`, message 16). Plague object update `0x10067340`: same 2500-unit step and 9-step cycle; then `rnd & 7 < 2` (25 %) → island `+0x158` (`0x10087130`, cure: clear state, statistic 8 −= 1, remove object) **without resetting the counter** (the next step tries again), else `dir = (rnd & 3)·2`, `0x10064470(dir, range 2, mask 8)` (houses only), `rnd % count`, `StartPlague`. **Confirmed.**
- **Riots** (same function): merchant/aristocrat houses (`PRODTYPE` 100/101) with satisfaction slot `+0x9c` < 1 are counted, those < −1 a second time but only when the city has more than 6 such houses; `i = min(31, count / 2)`, table `0x100dcc64`, `rnd % b < 4` → revolt event `0x16`. Runs only when world value 14 has bit 2. **Confirmed** (structure), meaning **likely**.
- **Volcano** symbol update `0x10066900`: disabled by world value 6 bit 2; a countdown `+0x38` of `rnd % 9900000 + 9900000` units (the table value is < 32768, so effectively `9900000 + rnd`); at expiry a second draw, `rnd % 3 == 0` → eruption `0x10066990` (not read). The constructor also takes a CRT `rand() % 30` animation phase. **Confirmed.**
- **Drought** (`DUERRE` record, island `+0x15c` = `0x10087280` creates an area object) and **earthquake**: not read (Open).

### 2.5 Fire and determinism (ISS-033)
Ignition, spread, flame variant, damage and timers use only integer arithmetic and the shared table. The float parts are positions: flame position (`+0x10`, `+0x18`) truncated to a tile, `FIREPOS` rotation and the distance² test in `StartFire`. **Confirmed.** Desync candidates, all **guess** until tested: (a) different `dt` sequences per client (timers advance by "one step per call" and the island processes `elapsed`-proportional work); (b) any machine-local draw from the shared table; (c) x87 precision differences in the float tests (Direct3D may switch the FPU to 24-bit precision unless told otherwise); (d) CRT-`rand` figures that feed back into sim state. For the rebuild: one sim PRNG, integer positions, the 64-house cap and the list order kept, all disasters behind the "disasters off" rule.

## 3. Ships

### 3.1 Figure type data (`FigurenHeader.dat` `HEAD` and `LOGIC`)
`AnnoGame.dll` `0x1000fe00` (figure `ENTRY` reader) and the property switch `0x10010ae0` / getters (figure-type vtable `0x10021d10`), **confirmed**:

| Record, offset | Type | Stored at | Getter | Ships (P) |
|---|---|---|---|---|
| `HEAD` 0x00 | f32 | `+0x2c` | slot `+0x64(0)`, prop `0x4b5` | 0.100–0.125: **turn-step length** (§3.3) |
| `HEAD` 0x04 | f32 | `+0x10` | slot `+0x60(x)` returns `x·value`, prop `0xc` | 0.030–0.048, canoe 0.012: **base speed** (**likely**, §3.2) |
| `HEAD` 0x08 | u8 | `+0x104` | prop `0x4b6` | 0 |
| `HEAD` 0x0a | u16 | `+0xe6` | prop `0x4bd` | 1539 `EffekteKanonenkugel` (projectile figure) |
| `HEAD` 0x0c | u16 | `+0xe8` | prop `0x4bf` | 1580 `EffektSchiffskanone01` (muzzle effect) |
| `HEAD` 0x10 | f32 | `+0x108` | prop 5 | 1.55–2.35 |
| `LOGIC` 0x00–0x0c | 4 × f32 | `+0x34, +0x30, +0x38, +0x3c` (≤ 0 → 1, 1, 4, 4) | slot `+0x64(2, 1, 3, 4)` | f0 6.0–8.0 on sailing ships, 1.0 otherwise |
| `LOGIC` 0x11 | u8 | `+0xf2` as `value << 8` | slot `+0x84` | 50 (**likely** tons per hold, ×256) |
| `LOGIC` 0x12, 0x27, 0x24, 0x25, 0x28 | u8 / 8 | `+0x14, +0x18 (≥ +0x14), +0x1c, +0x24 (0 → 7), +0x28 (0 → 2)` | slot `+0x68(0, 1, 2, 4, 5)` | 8, 8, 0, 16, 5 |
| `LOGIC` 0x13, 0x14 | u8 | `+0xf6, +0xf7` | slot `+0x88(0/1)` | e.g. 1501: 4, 12; 1503: 8, 3. 0x13 is **likely** the cargo holds. 0x14 is **likely** the unit slots: the troops a ship carries, and on war machines the crews (catapult 2, cannon 1, mortar 1, siege tower 2, as in the manual; `docs/research/manual-and-help.md` §1.4) |
| `LOGIC` 0x15 | u8 | `+0x101` | slot `+0x94(2)` | 10–25 |
| `LOGIC` 0x16 | u16 | `+0xf4` | slot `+0x94(1)` | 1500–4000 |
| `LOGIC` 0x1e | u16 | `+0xea` | slot `+0x90` | 6000–14000: **maximum hit points** (§3.2) |
| `LOGIC` 0x1c | u16 | `+0xec` | slot `+0x9c` | 300 |

The scale `0.125` (`0x10021d08`) and defaults (1.0, 4.0, 7.0, 2.0 at `0x100213bc`, `0x10021d0c`, `0x10021d04`, `0x10021d00`) are **confirmed**. These offsets belong in `docs/formats/figuren.md` (OQ-023) once accepted.

Ship objects are created by the figure factory `0x1009a110` for `BEWOHNTYPE` 113 (`SHIP`): class `0x10043600` (`0x18c` bytes, vtable `0x100c4da4`). The ship's name comes from the `SHIPS` text list, index `rnd % count` (one shared-table draw per ship created; default `Newship`). **Confirmed.**

### 3.2 Movement
State (ship object): position `+0x14, +0x18, +0x1c` = world `(x, height, z)` floats; current segment: direction `+0x20, +0x24, +0x28`, remaining length `+0x2c`, speed `+0x30`, kind/flags `+0x34`; speed multipliers `+0x38` (reset to 1.0 per segment) and `+0x3c`; facing `+0x74`; segment list head `+0x54` (nodes: `+4` next, `+8` six floats + `u32` kind). **Confirmed.**

Sim step, ship vtable slot `+0x74` = `0x100428d0(now)` (same code as the base `0x1003f090`), **confirmed**:

```
t = (u16)(now − last) · 0.2                 // float
while speed ≠ 0 and t ≠ 0:
    step = min(t · speed, remaining)         // distance
    pos += direction · (step / speed)        // direction is not normalised by speed
    remaining −= step;  t −= step / speed
    if remaining ≤ 0: next segment (0x1003e930) or arrival callbacks (slots +0x174/+0x178/+0x17c)
    last = now
```

Segment start `0x1003e930` (**confirmed**): for a movement segment (kind ≥ 8) the wanted facing is `dir16(direction)` (§3.3); if it differs from the current facing by more than 2 of 16 steps, the ship first turns **one** step (toward the shorter side) as a separate phase that leaves the segment in place (so the test repeats until the difference is ≤ 2) with direction 0, length `HEAD.f0` and speed 0.01, i.e. `HEAD.f0 / 0.01` = 10–12.5 time units per 22.5°; otherwise the facing snaps to the wanted value and the ship moves with `speed = segment speed · (+0x3c · +0x38)`.

Speed factor `+0x3c`, ship slot `+0xa0` = `0x100427b0` (**confirmed** arithmetic): `load = 100`; if the owner's value 3 (`(+0x144)` slot `+0xc4(3)`) equals 1: `load = 100 − 17·Σcargo / ((n0 + n1) · cap)` with `cap` = slot `+0x84`, `n0`, `n1` = slot `+0x88(0)`, `+0x88(1)` and the cargo amounts of the first `n0` holds (integer division). `percent = (35·hp / maxhp + 65) · load / 100` with `hp` = ship `+0x6c` (signed 16-bit) and `maxhp` = slot `+0x90`; `+0x3c = percent · 0.01` (float `0x100c4da0`). So a wrecked ship sails at 65 %, a fully loaded one up to 17 % slower when the option is on (option meaning **guess**).

Segment speed: the ship draw code computes the current speed as `type.slot+0x60(1.0) · +0x3c · +0x38` (= `HEAD` 0x04 · factor) and shows the wake only above 0.033 (`0x100c4bf4`), so `HEAD` 0x04 is the base speed in world units per time unit (**likely**; the path builder that writes the segment speed was not read). With the factor 0.2 above, 1501 (0.032) moves 0.0064 tiles per input time unit.

Path: the movement is a list of straight float segments. `0x1003de40` re-plans locally: it collects other ships/units within the radius `slot +0x68(5)` (`LOGIC` 0x28 / 8 = 5.0 for ships) via `0x1003dc50`, walks the current path up to 12 world units (`0x10030660`) to get a bounding box, and runs a planner object obtained from `(+0x10)` slot `+0x20` (methods `+0x50, +0x68, +0x6c, +0x74, +0x7c` with a callback `0x10030920`, `+0x80, +0x84`). **Confirmed** (structure); the grid/graph used for open-water routing was not found (Open). Scenario routes already contain waypoints (§3.4).

Drawing (`0x1003c6b0`, slot `+0x7c`) interpolates the position with the same formula from the last sim time and adds a sine bob; colour parameter `(+0x76 << 8) | 0xC0` (`+0x76` = property `0xb`). **Confirmed.** Visual only.

### 3.3 Facing (OQ-027 d) — resolved
The scenario unit loader `0x1001d240` (records `SCHIFF` and `SOLDAT`) reads the 16-byte unit prefix, creates the unit at world `(x_f + ox, 0.0 for ships / terrain height for soldiers, y_f + oz)` from the `f32` at 0x08 and 0x0C, and passes **byte 0x05 to the unit's `SetProperty(0x16, value)`** (vtable slot `+0x4c`; `0x1001e01d`–`0x1001e041` for ships, `0x1001ddad`–`0x1001ddd1` for soldiers). **Confirmed.** It then calls slot `+0xe0(0.01, 0, 0, 0)`, which leaves the facing unchanged because no direction vector is passed (`0x10040000`). Bit 0 of byte 0x06 triggers world slot `+0x1bc(unit, 0)` (meaning open; 0 in the corpus).
- Ships: property `0x16` → `+0x74` (`0x10040310`), the **facing in 16 steps of 22.5°** (`& 0xf` arithmetic in `0x10041420`, `0x1003e930`, `0x1003fdb0`, getter slot `+0x68` = `0x10040180`; used as the draw rotation in `0x1003c4b0`). **Confirmed.**
- Soldiers: property `0x16` → `+0xe` (`0x10035ca0` via `0x10049ad0`), set from a movement vector by `0x10088b50` into **8 directions** (`0x10034c20`). **Confirmed.**
- Direction from a vector `(dx, dz)` (ships, `0x10030e20`, x87): `θ` = angle from +z toward +x in [0, 2π) built from `asin(dx/len)` with the quadrant fixed by the signs (`π − asin` for dz < 0, `+ 2π` for dx < 0 ≤ dz); `dir16 = (trunc(θ · 16 / 2π + 0.5) + 4) & 15`. So 0 = −x, 4 = +z, 8 = +x, 12 = −z (scenario `y` is world `z`). **Confirmed.**
- Soldiers (`0x10088b50`): octants by comparing `|dz|` and `|dx|` with factor 0.36 (`0x100c324c`, boundary atan 0.36 = 19.8°, not 22.5°): 0 = −x, 2 = +z, 4 = +x, 1 and 3 the diagonals with +z; **confirmed** for dz ≥ 0, **likely** 5–7 for dz < 0 (5 = (+x, −z), 6 = −z, 7 = (−x, −z)). Ship index = 2 × soldier index.
- Corpus (P): ships use 0–14 (0: 225 of 327), soldiers 0–7, consistent with 16 and 8 directions.

How `facing` plus view rotation selects the sprite frame is in `AnnoDisplay.dll` (`annodisplay.md` §4, rotation `(r + view_rotation) & 0xF`); the exact frame index for ships was not traced (Open).

### 3.4 Trade routes (`SCHIFF/PRIVAT/ROUTE`)
Ship `PRIVAT` loader `0x10042c70` (ship slot `+0x12c`), **confirmed**: `HEAD` bit 0 → route mode (`+0x148 = 3`, route restarted after load), `NAME` → `+0x168`, `VERK` → `+0x164`, `CREW` → crew units, `ROUTE` → route object at `+0x9c` (vtable `0x100c4f4c`), loader slot `+0x3c` = `0x1005cd60`.

Route content (`0x1005cd60`, `0x10056030`, corpus P over 38 routes):
- `ENTRY` records, each a 2-byte prefix `(u8 kind, u8 n)` and a body; the kind selects the node class (route slot `+0x10`):
  - kind 8, 10 bytes: waypoint, `f32 x, f32 y` at offsets 2 and 6 (978 in the corpus; spacing ≈ 15–20 tiles, so routes carry a precomputed coarse path);
  - kind 9, 6 bytes: stop, `u16 x, u16 y` (a warehouse tile; 76);
  - kind 10, 22 or 24 bytes: trade order, nested `TRADE` record (95).
- `TRADE` (12 bytes in `ANNO`, 14 in `ANNOADDON1`): `u16` 0 (unused by the loader), `u16` → node `+10` (always 50 in the corpus, **likely** amount or limit), `u16` ware → node `+0xc` through the scenario's ware remap table, `u8` → `+0xd` (0–10), `u8` → `+0xe` (0/1, **guess** load/unload), `u16 x`, `u16 y` → `+4`, `+6` (the stop's tile), add-on `u8` → `+0xf` (0/1). **Confirmed** (mapping), meanings **guess**.
- `POS` (2 bytes, once): index of the current entry; the loader walks the list to it. **Confirmed.**
- Execution (arrival, loading rules, waiting) was not read (Open). These layouts belong in `docs/formats/sze.md` once accepted.

### 3.5 Floating point in movement
Float (x87, 32-bit storage) everywhere in ship and soldier motion: `0x1003f090`/`0x100428d0` (integration), `0x1003e930` (segment start, speed product), `0x10041420` (turn step), `0x10030e20` (`sqrt`, `asin`, `__ftol` at `0x100b5f90`), `0x10088b50` (octants), `0x100427b0` (int → float speed factor), `0x1003de40`/`0x1003dc50`/`0x10030660` (avoidance distances), `0x1003c4b0` (draw only). **Confirmed.** The rebuild needs fixed-point positions and a lookup for `dir16`/`dir8`; exact float reproduction of the original is not a goal for lockstep.

## Fixture spec
Inputs → expected outputs, to check later against the original (x86 Windows, `anno-diff`) or a recorded savegame.

1. **Shared random table.** After world `SetValue(8, s)`, the next draws are `table[(s+1) mod 2048], table[(s+2) mod 2048], …` with `table` = MSVC `rand()` seed 1. `s = 0` → 18467, 6334, 26500, 19169; `s = 2047` → 41, 18467. (Static fixture; also confirms pre-increment.)
2. **Flame damage, one flame.** `M = h = 2500` (pioneer house): hit points after successive 2500-unit steps 2473, 2446, 2419, 2392, 2365; burned down after 128 steps. `M = 4000`: 3956, 3912, 3869, 3826, 3783; 127 steps. `M = 6000`: 126 steps; `M = 100`: 100 steps (minimum damage 1); `M = 30000`: 125 steps. Test: a building with no `FIREPOS` points (only one flame possible) set on fire by script; measure the time to collapse.
3. **Flames per building.** A building with `n` `FIREPOS` points: one flame at ignition, then one more at each of the first `n − 1` 1/16-hit-point boundaries crossed (source 0 always ignites); never more than `n`. The first damage step only creates the damage record (§2.2), so the first boundary that counts is 15 → 14: pioneer house 501 (2 points, 2500 hp) gets its second flame when hp drops below 2344 (step 6 with one flame: 2365 → 2338).
4. **Spread cadence.** Every flame tries to spread once per 9 steps (22,500 units) to at most one adjacent building on a random side (range 1); roads, places and fences never catch fire; a fire never spreads diagonally beyond the scanned side (**likely**, check visually).
5. **Random fire risk.** City with `H0 = 32` pioneer houses and nothing else: `s = 160`, `i = 10`, `b = 191`; per 12,500-unit check the draw `r` ignites iff `r mod 191 < 4`. `H0 = 31`: `s = 155`, never. `H0 = H1 = H2 = 20`: `s = 240`, `b = 127`. 100 houses of each level: `b = 31`. With option bit 8 set: never. Only pioneer/settler houses ignite, and only the first 64 eligible in list order.
6. **Plague risk.** `P = 1523`: `i = 3`, never; `P = 1524`: `i = 4`, `b = 223`; `P ≥ 8180`: `b = 31`. Cure chance per 9-step cycle 25 %; spread range 2, houses only.
7. **Damage ignition table.** Source class 2, level 14 → `b = 9`: ignites iff `r mod 9 == 0`; level 16 (full hp) → `b = 255`; levels 0–2 → always.
8. **Ship facing from the scenario.** Load `Scenes/0200Single/*.sze`; each ship's initial sprite direction equals its `attr`; `attr = 4` points toward +y (down the tile rows), 8 toward +x, 12 toward −y, 0 toward −x. Soldiers: `attr` 0–7 with 2 = +y, 4 = +x.
9. **`dir16` from vectors** `(dx, dz)`: (1, 0) → 8; (0, 1) → 4; (−1, 0) → 0; (0, −1) → 12; (1, 1) → 6; (1, −1) → 10; (−1, 1) → 2; (−1, −1) → 14. `dir8` (soldiers): (1, 0) → 4; (0, 1) → 2; (1, 1) → 3; (1, 0.36) → 3 (boundary is inclusive for the diagonal); (1, 0.35) → 4.
10. **Ship turning.** The turning phase does not consume the path segment, so the check repeats until the remaining difference is ≤ 2 steps, which then snaps. A galleon (1501, `HEAD.f0` = 0.125) ordered to reverse course (8 steps) turns 6 steps of 12.5 time units (75 units ≈ 375 input units at the 0.2 scale) and snaps the last 2; a course change of ≤ 2 steps (≤ 45°) has no turning phase.
11. **Ship speed factor.** Full health, option off: `+0x3c = 1.0`. `hp = maxhp / 2`: `percent = 82` → 0.82. `hp = 0`: 0.65. Option on, the first `n0` holds full (`n0·cap` cargo): `load = 100 − trunc(17·n0 / (n0 + n1))`, e.g. 1503 (`n0 = 8`, `n1 = 3`): 88 → 0.88 at full health.
12. **Route file layout.** `S01_03_01.sze` ship 6: first entries `(9, 0) stop (70, 339)`, `(10, 1) TRADE (0, 50, 30, 8, 1, 70, 339)`, then waypoints `(8, 3) (61.0, 358.0)`, `(8, 4) (61.0, 373.0)`, … (P; re-check after a save in the original keeps the same sequence).

## Open
- Unit of the world time: the world time is half the summed `dt`; who calls world slot `+0x84` and with what `dt` (frame time × game speed? fixed tick?). Decides the real-time length of 2500/12500 and the ship speed. Tick-loop note.
- Whether savegames and multiplayer sync store/send the random index (world value 8) and the scene seed; who passes the seed to `AnnoScene.dll` `0x100025a0`/`0x100029f0`.
- Fire brigade: dispatch range, timing, firefighter walking and work time; mapping of task type `0x20` to `ROUTETYPE LOESCH`; doctors for plague (task `0x21`?).
- Damage source classes (`+0xb0(0)` of attackers) per weapon (fire arrows, mortar incendiary, cannons).
- Meaning of city `+0x18a` (houses per level, **likely**) and `+0x2ec` slot `+4` (inhabitants, **likely**); player value 3 (cargo slows ships, **guess**) and value 4 bit 8 / world value 6 bits 2 and 8 (disaster switches).
- Eruption `0x10066990`, earthquake, drought (`0x10087280`, `DUERRE`).
- The open-water route planner (grid or graph), the writer of segment speeds, and route execution (load/unload rules, waiting at stops).
- Exact sprite frame for a ship facing and view rotation; soldier octants for dz < 0 in the disassembly.
- FPU precision control under Direct3D (24-bit vs 53-bit) for the float tests in fire and movement.
- Which CRT-`rand` users (§1.2) change sim state.
- `LOGIC` fields marked **guess** above and the non-ship meanings of `LOGIC` f0–f3.
