# Population, needs, growth and taxes

**Status:** first pass (2026-10-17), static analysis only. Modules (GOG 2.0.0.5, image base `0x10000000` each): `AnnoGame.dll` (loader for `Data/BGruppen.dat`), `AnnoWelt.dll` (houses, cities, balance), `AnnoFrame.dll` (tax button). Tools: Ghidra 12.1.4 headless (`cache/ghidra/plus/*.c`), `objdump -d -M intel` for every constant quoted below, a scratch model (`cache/pop/model.py`, not committed) for the fixture numbers. `AnnoWelt`/`AnnoGame` have no RTTI, so class names are ours. Addresses are in the module named in each section. Behaviour is paraphrased; no code is copied.

Confidence as in [`annodisplay.md`](annodisplay.md): **confirmed** = read in the decompiled and/or disassembled function; **likely** = strong inference, not traced end to end; **guess** = hypothesis, repeated under "Open".

Time: one **tick** = 2 ms of game time (world tick = game-ms counter `>> 1`, `AnnoWelt` `0x100ae040`; see [`tick-loop.md`](tick-loop.md) §2). All periods below are in ticks.

Short answer: every house holds a stock level per need. Every 2500 ticks (5 s) a house lets each stock decay by 1/128, rates three satisfaction scores (food, goods, infrastructure) through `NahrungTab`/`BedarfTab`/`InfraTab`, combines them into a mood from −2 to +2, and adds `VermehrTab[mood]` (in 1/64 residents) to its residents. A full house with a positive outlook upgrades if the next group's forced needs are met and the building materials are in store. Money comes from consumer goods handed to residents, at the good's price plus a tax-level surcharge (`PreisLevel`). It is booked every 2000 ticks (4 s, 15 times per game minute). Building upkeep is deducted at the same time. The house and city updates use only integers. Floats appear only when `BGruppen.dat` is loaded.

## 1. `BGruppen.dat` loader (`AnnoGame.dll`)

- Game init `0x10007400` allocates a 0x10d4-byte object (constructor `0x10015790`, vtable `0x10022058`), parses `Data\BGruppen.dat` into it (`0x10015770` → `0x10013e00`) and stores it at game `+0x5fdc`. Getter `0x100014f0` = game interface slot `+0x78`. **Confirmed.** In `AnnoWelt` the object is reached as player slot `+0x6c` (`0x10021450`, returns player `+0x10`). **Likely**, because the slot results are used with exactly this layout.
- Object vtable: slot 0 → groups (`+0x9c4`), slot 1 → stages (`+0x1054`), slot 2 → goods (`+0x4`). **Confirmed** (`0x100157c0`, `0x100157d0`, `0x100157e0`).
- The parser reads the whole file. It looks for `[`, compares the section name, then reads `Key: value` lines until the next `[`. Names in values are resolved through the enum-name dispatcher `0x100033c0`: kind `0x20` = goods (`0x10013be0`), `0x21` = stages (`0x10013d60`, table `0x10024bd0`), `0x38` = research. Integers are read with CRT `strtol` (`0x100173b2`), decimals with CRT `strtod` (`0x100175ce`), lists with `,` as separator (`0x10016360` int, `0x100163c0` double). **Confirmed.**

### Goods ids (kind `0x20`, `0x10013be0`) — **confirmed** (table read from the data section)
`1` EISENERZ … `6` MAMORSTEIN, `9` WOLLE … `0x17` HOLZKOHLE, **`0x1a` NAHRUNG, `0x1b` LEDER, `0x1c` SALZ, `0x1d` ALKOHOL, `0x1e` TABAKWAREN, `0x1f` GEWUERZE, `0x20` HEILKRAEUTER, `0x21` STOFFE, `0x22` SEIDESTOFFE, `0x23` LAMPENOEL, `0x24` KLEIDUNG, `0x25` SCHMUCK, `0x26` PELZE, `0x27` WEIN**, `0x29` WERKZEUG, `0x2a` HOLZ, `0x2b` ZIEGEL, `0x2c` MAMOR, `0x2e`–`0x35` weapons, `0x38`–`0x48` crops, animals and fish, **`0x4c` WIRTSHAUS, `0x4d` BADEHAUS, `0x4e` THEATER, `0x4f` KLINIK, `0x50` SCHULE, `0x51` HOCHSCHULE, `0x52` KAPELLE, `0x53` KIRCHE, `0x54` KATHETRALE, `0x55` PARK, `0x56` BIBLIOTHEK, `0x57` FEUERWEHR, `0x58` GERICHT, `0x59` GALGEN**, `0x60`… military units, ships. The code treats `0x1a`–`0x28` as **consumer goods** and `0x4c`–`0x5a` as **infrastructure** (§2, §5, §8). **Confirmed.**

### Goods record (`[WAREN]`, 96 × 26 bytes at object `+0x4`, index = goods id) — **confirmed**
| Offset | Type | Key | Meaning |
|---|---|---|---|
| +0x00 | u8 | `Id` | goods id (marks the slot as used) |
| +0x01 | u8 | `IdSec` | substitute / predecessor good (§5) |
| +0x02 | u8 | `BauFirst` | stage index; default 0 (the `[WAREN]` start clears the table, `0x10014c3b`) |
| +0x03 | u8 | `BauLast` | stage index; set to `0xff` by `Id` |
| +0x04 | 7 × i16 | `Preis`, `Bonus`, `Malus` | price per climate. `Preis` writes all 7 entries. `Bonus: n, A\|B…` adds `n` to the listed climates, `Malus` subtracts it. Several `Bonus` lines accumulate (GEWUERZE, TABAKWAREN). Climate index: 0 POLAR, 1 TUNDRA, 2 NORD, 3 PRAERIE, 4 DSCHUNGEL, 5 STEPPE, 6 WUESTE (table `0x10024ca0`). |
| +0x12 | 3 × i16 | `PreisLevel` | surcharge per tax level 0/1/2 (§8) |
| +0x18 | u8 | `Forsch` | research id (kind `0x38`) |

A climate name the parser does not know gives index −1, and the write then lands on `+0x02/+0x03` (BauFirst/BauLast). That is an original bug the data never triggers. **Confirmed** (code path). A `Preis` line after a `Bonus` line would overwrite the bonus, because the order of keys matters. The data always has `Preis` first.

### Group record (`[BGRUPPE]`, 6 × 0x118 bytes at object `+0x9c4`) — **confirmed**
`BGruppeNr` selects the slot by name: PIONIER 0, SIEDLER 1, BUERGER 2, KAUFLEUTE 3, ARISTOKRATEN 4 (only these five names are compared; slot 5 stays empty). The section start clears all six slots.

| Offset | Type | Key | Meaning |
|---|---|---|---|
| +0x000 | u8 | `BGruppeNr` | group index |
| +0x001 | bits | `NoUpgrade` (bit 0), `NoDowngrade` (bit 1) | `TRUE`/`FALSE` |
| +0x002 | u8 | — | number of needs `n` |
| +0x003 | 96 × u8 | — | lookup goods id → need slot; `n` = "not needed" (built after the record, below) |
| +0x064 + 4j | u8, u8, u16 | `Ware`, `Force`, `Menge` | need slot `j`: goods id, forced flag, quantity (integer, below) |
| +0x0c4 | u32 | — | bit `(id − 0x4c)` set for every forced infrastructure need |
| +0x0c8 | u16 | `Wohnmax` | residents per house at this level |
| +0x0ca / +0x0cb | u8 | `BauFirst` / `BauLast` | stage indices (use not traced) |
| +0x0cc | u8 | — | end of the goods needs = number of slots before the first infrastructure need |
| +0x0cd | u8 | — | end of the infrastructure needs (≥ `+0xcc`) |
| +0x0ce | u8 | `Hiscore` | key accepted, absent from the data (use not traced) |
| +0x0cf | 21 × i8 | `NahrungTab` | food score per satisfaction step |
| +0x0e4 | 21 × i8 | `BedarfTab` | goods score per step |
| +0x0f9 | 21 × i8 | `InfraTab` | infrastructure score per step |
| +0x10e | 5 × i8 | `VermehrTab` | growth per update for mood −2…+2, in 1/64 resident |
| +0x114 | ptr | — | goods table |

- **`Menge`** is stored as `trunc(Menge · 40.96 + 0.5)` (x87 `fmul` by the double 40.96 at `0x10022048`, `fadd` 0.5 at `0x10021cf8`, `__ftol` at `0x10014920`). **`VermehrTab`** is stored as `trunc(v · 64)`, toward zero, with no rounding (`fmul` by 64.0 at `0x10022050`, `0x10014787`). **Confirmed.** Integer equivalent for a value written with two decimals, `h` = value · 100: `Menge = ⌊(h·4096 + 5000) / 10000⌋` and `VermehrTab = sign(h) · ⌊|h|·64 / 100⌋`. Checked against the double computation for every `h` in −1000…1000 (our script, 2026-10-17). This matches `text::fixed(field, 2)` in `docs/formats/data-text.md`.
- **`Priority`** is compared with `HIGH` only, and the result goes into a scratch local that is never read (`0x10014937`–`0x100149cf`). **It has no effect in this build.** **Confirmed.**
- Post-pass per group (`0x10014ad0`–`0x10014bd0`), **confirmed**:
  1. lookup[`Ware(j)`] = `j`;
  2. if the group needs WIRTSHAUS (`0x4c`) and not ALKOHOL (`0x1d`), lookup[ALKOHOL] = the WIRTSHAUS slot (the tavern serves alcohol);
  3. for every infrastructure id `0x4c`–`0x5a` the group does not need, if its `IdSec` is needed, the id maps to the `IdSec` slot. Example: KIRCHE (`IdSec` KAPELLE) fills the pioneers' KAPELLE need.
- **Data invariants the code relies on** (**confirmed** from the formulas in §4–§6, all hold in the shipped file): NAHRUNG is need slot 0 of every group. Goods needs come before infrastructure needs. Every forced need of group `g+1` also appears in group `g`; these are the needs marked "Aufstieg !!" in the file. Otherwise the upgrade check divides by a zero `Menge`.

### Stage record (`[BAUSTUFEN]`, 32 × 4 bytes at object `+0x1054`) — **confirmed**
`Stufe: <name>, <group>, <count>` → `+0` u8 group index, `+2` u16 count, at the slot of the stage name (kind `0x21`: 0 `INFRA_EVER` … 30 `INFRA_STUFE_DENKMAL`, 31 `INFRA_STUFE_FORSCH`). `Wissen: <name>, <n>` → `+1` u8 (use not traced). The count is a resident threshold (§7).

## 2. House object (`AnnoWelt.dll`) — **confirmed** unless marked

The building factory `0x10024010` maps building kinds `0x61`–`0x65` to the house class: constructor `0x10077220`, vtable `0x100c9958`, 0x50 bytes, base class `0x10075c60` → `0x100734c0`. It then calls slot `+0x84` (`SetGroup`, `0x10076b90`) with group record `kind − 0x61` and increments the city's house count per group (`+0x18a + 2·group`). Fields:

| Offset | Type | Meaning |
|---|---|---|
| +0x04 | u16 | index in the city's house list |
| +0x06, +0x08 | u16 | tile position (x, y) — **likely** |
| +0x0a | u16 | tick of the last population update (base constructor sets 0) |
| +0x0c | u8 | flags; bits 4/5 force a bad mood (§4); bit 3 = building switched off (**likely**, `0x10072a60`) |
| +0x10 | ptr | city |
| +0x14 | ptr | building type (Anlagen data) — **likely** |
| +0x18…+0x24 | 4 ptr | active walkers sent by the house (§5) — **likely** |
| +0x28 | ptr | group record (§1) |
| +0x2c | i16 | **residents · 64** (start 5 · 64) |
| +0x2e | u16 | **consumption base `E`** ≈ residents · 128, smoothed (start 5 · 128) |
| +0x30 + 2j | 16 × u16 | **stock of need slot `j`**, in 1/16 delivered units |

**Satisfaction of slot j** (used everywhere, e.g. `0x10076ae0`): `s_j = stock_j · 1536 / (E · Menge_j)`, integer division, capped at 160 (`0xa0`). **128 = 100 %.** With `E = 128 · residents`, 100 % means `stock_j = E · Menge_j / 12`. Score-table index = `(s + 4) >> 3`, i.e. 0…20.

Hard layout limit: 16 stock slots per house, so at most 16 needs per group (data: 6, 10, 12, 11, 8).

## 3. Scheduling and order of evaluation — **confirmed**

```
world.Advance(ms)                      0x100ae040   tick = game_ms >> 1
  for each island: island.update(tick) 0x100a2e60 → island slot +0xe4 = 0x1008b760
    chunk sweeps (2000 / 15000 ticks), sub-objects, then
    cities: 0x100969d0                  round robin over 16 slots, (Δtick·16)/50 + 1 per call (all within 50 ticks)
      city.update(tick)                 city vtable 0x100c2f40 slot +0x50 = 0x1002d9a0, in this order:
        a. buildings 0x10023a50: lists +0x108, +0x88, +0xe8, +0xc8 (houses), +0xa8;
           each list round robin, (Δtick·N)/500 + 1 buildings per call → every building ≥ once per 500 ticks;
           building slot +0x8c(tick)  (house: 0x10076230)
        b. +800 sub-object 0x10027400        (every 250 ticks; not traced)
        c. stage unlock 0x10029610           (every 2500 ticks, §7)
        d. queued house upgrades 0x1002d350  (list +0x30c0, count +0x30e8)
        e. income and upkeep 0x10029950      (every 2000 ticks, §8)
        f. store statistics decay 0x1002b670 (every 2500 ticks: per-good in/out counters × 63/64)
        g. random city events 0x1002ba80     (every 12500 ticks; fire, riot, plague — not traced in detail)
        h. mood statistics 0x1002a5e0        (every ≥ 500 ticks; houses round robin over 15000 ticks;
                                              residents per mood → +0x844, advisor messages — likely)
        i. 0x1002d3b0                        (not traced)
```
All period checks compare a **16-bit** difference `(u16)(tick − last)`. The house uses `last += 2500` (it catches up in steps), the income booking uses `last = tick`.

## 4. House update (slot `+0x8c` = `0x10076230`) — **confirmed**

```
if (u16)(tick − last) < 2500: return
last += 2500
mask = player property 0x21, 2nd word        // reached-stage bitmask (§7)
E = E·127/128 + residents·64/64              // both divisions truncate
lowest = +∞
for each need slot j:
    if mask has bit BauFirst(goods[Ware(j)]):           // good already available to the player
        stock_j = stock_j·127/128                       // consumption: 1/128 per update
        lowest = min(lowest, s_j)                       // s_j capped at 160
if lowest < 102 (0x66) and walkers < type.max_walkers (type slot +0xb8):
    pick a target type with the world PRNG (type slot +0xbc/+0xb4, index from house position + PRNG bit)
    spawn walker task 0x35 (house slot +0x118)          // likely: a resident goes to fetch goods
mood = Mood()                                            // slot +0x9c, §4.1
remove residents·64/64 from the city's per-group counter; residents·64 += VermehrTab[mood]; add back
if mood < 0:
    if NoDowngrade or flags & 0x30: floor = 64 (1 resident) else floor = 48 · Wohnmax(group − 1)
    if residents·64 < floor:
        if downgradable: if residents·64 < 64 { counter adjust; residents·64 = 64 }; city.Downgrade(house)  // slot +0x9c
        else: island slot +0xcc(x, y, 0, 0, 0)          // likely: house is removed
else if residents·64 > 64 · Wohnmax:
    clamp to Wohnmax; if UpgradeCheck() (0x10075dd0): city.Upgrade(house)                                 // slot +0x98
```
- The downgrade floor is ¾ of the **lower** group's `Wohnmax`, read at `group − 0x50` = previous record `+0xc8` (`0x10076527`): settlers < 6, citizens < 11.25, merchants < 21 residents. Pioneers and aristocrats have `NoDowngrade`, so their floor is 1 resident.
- The counter adjustment at `0x10076551` adds `trunc(residents) − 64` instead of a correction of at most one resident. It looks like an original bug, reachable only if a downgradable house drops from ≥ the floor to < 1 resident in one update, which the `VermehrTab` steps (≤ 102/64) make impossible in normal play. **Confirmed** instruction, **likely** dead.

### 4.1 Mood (slot `+0x9c` = `0x10075fd0`) — **confirmed**
```
if E == 0: return 0
food   = min(s_0, 128)                                        → f = NahrungTab[(food+4)>>3]
goods  = mean over forced slots 1…(+0xcc − 1) of min(s_j, 128)
         + forced WIRTSHAUS slot (min(s,128)) also counted here; 128 if no forced goods → b = BedarfTab[(goods+4)>>3]
infra  = min over forced slots +0xcc…(+0xcd − 1) of s_j (cap 160); 160 if none   → i = InfraTab[(infra+4)>>3]
if f, b, i all ≥ 0:
    if flags & 0x30: return flags & 0x20 ? −2 : −1
    return (f + b + i + 1) / 3                                 // integer division
else:
    if flags & 0x20: return −2
    return min(f, b, i)
```
Not-forced needs (the "Aufstieg !!" ones) do not affect mood; they only matter for the upgrade check and demand. The pioneers' `BedarfTab` is ≥ 1 everywhere, so goods can never make pioneers unhappy.

## 5. Supply, demand and level change

- **Delivery** (slot `+0xc0` = `0x10076610`, `(goods id, amount)`): if the goods id maps to a slot, `stock += amount · 16`. If the good has an `IdSec`, the same amount is delivered again for the `IdSec` good. A good the house does not need returns 0. **Confirmed.** Callers include figure tasks that unload cargo into a building (`0x10057bd0`). **Likely**: the market/resident walker completes here.
- **Demand** (slot `+0xa4` = `0x100765c0`): `Menge·E·256/44236 − stock/16`. That is the amount the house still wants, up to ≈ 111 % satisfaction. **Confirmed** formula.
- **Choice between two offered goods** (slot `+0xd4` = `0x10076670`): goods already being fetched by one of the four walkers are skipped. Urgency = `128 − s` for the good itself or, recursively, the highest urgency along its `IdSec` chain (`0x10076960`, `0x100769f0`). A good is accepted only if the supplier's stock (slot `+0xa4`) exceeds `residents · Menge · 256 / 4096` and its slot `+0xc4` allows it. **Confirmed** arithmetic. **Likely** role (supplier = market or walker).
- **Consumption billing** (city slot `+0x88` = `0x1002dac0`, `(goods id, amount, ·, take)`): only for consumer goods `0x1a`–`0x28`, `billed[id] += amount`. If `take`, `min(amount, store)` is also removed from the city store. Building classes call it with take = 0 (`0x1006b520`, `0x10070120`) or take = 1 (`0x100702f0`). Which class is the market is not traced. **Confirmed** (function), **likely** (meaning).
- **Upgrade check** (`0x10075dd0`), **confirmed**: no upgrade if `NoUpgrade`. Otherwise evaluate the **next** group's forced needs with the current house stocks (slot through the current group's lookup; infrastructure capped at 128 here, not 160). The next group's `NahrungTab`, `BedarfTab` and `InfraTab` values must all be **> 0**. Then city slot `+0x94` (`0x1002e080`) must accept: at most 10 queued (`+0x30e8`), house kind `0x61`–`0x80`, and the city store holds the materials of the next house type. That type is `type_id − type_id mod 20 + 20` (house types come in blocks of 20 per level), checked by `0x1002c030` (`0x1002e000`).
- **Upgrade** (`0x1002e0d0` → `0x1002cbe0`), **confirmed**: removes the materials (goods `0x29`–`0x2d`: WERKZEUG, HOLZ, ZIEGEL, MAMOR) from the store and books them in `+0x5d8`. If the type has variants (property `0x32`) it picks one with the world PRNG. It moves the residents between the per-group counters and house counts, sets the new type, and calls `SetGroup(next record)`. **Downgrade** = `0x1002e120` → `0x1002cdd0` (not read in detail; **likely** symmetric).
- **`SetGroup`** (`0x10076b90`), **confirmed**:
  - From an old group, each need's satisfaction is carried over by goods id. When the new satisfaction is ≥ 102, it is also given to the `IdSec` chain. The new stock is `Menge_new · s · E / 1536`.
  - Needs new to the house get stock 0, except slot 0 (food), which gets `Menge · E / 12` (100 %).
  - On a downgrade, needs that are new to the house start at satisfaction 102, and `E` is reset to `residents · 128`.
  - **First assignment to a non-pioneer group** (a higher house placed directly, e.g. aristocrats or scenario houses): residents = `Wohnmax`, `E = Wohnmax · 128`, and every forced need gets satisfaction `128 − (PRNG & 31)` from the world PRNG.
  - A new pioneer house keeps the constructor values: 5 residents, `E` = 640, food 100 %, everything else 0.
- **Save/load**: slot `+0x80` (`0x10077130`) writes an 8-byte header (residents as an integer, need count, version 2) and one byte per need with `min(s_j, 160)`. Slot `+0x7c` (`0x10076e60`) restores residents with `E = residents · 128`. **Confirmed** for save. **Likely** for load (Ghidra dropped part of the function).

## 6. Resident counters per city

City `+0x2ec` = counter object (vtable `0x100c3018`, reached as city slot `+0x5c`), one `i32` per group at `+4 + 8·g`. Slot 0 = residents of `g`, slot 4 = total, slot 8 = residents of group `g` **and all higher groups**, slots `+0xc`/`+0x10` = add/sub. **Confirmed.** The five groups are hard-coded here (`5 − g` loop).

## 7. Building stages (`BAUSTUFEN`, city `0x10029610`, every 2500 ticks) — **confirmed**

For each stage `i` = 0…30 not yet set in the city mask `+0x618`: if `counter.slot8(stage.group) ≥ stage.count`, set bit `i`. That is, *residents of that level or higher* ≥ the threshold: `INFRA_STUFE_1A` = 30 pioneers or better, `2A` = 25 settlers or better, `5F` = 5000 aristocrats. If the low 23 bits of the player's mask (property `0x21`) are numerically below the city mask, the city announces every infrastructure good `0x4c`–`0x5a` whose `BauFirst` is the stage just reached (text ids `0xfb`–`0x107`, e.g. WIRTSHAUS at `1B` = 80 pioneers, KAPELLE at `1C` = 125). It then ORs its mask into the player's. The player mask then gates consumption (§4: needs whose good is not yet available neither decay nor count as the lowest need).

## 8. Money: income, tax level, upkeep

- **Player money** = `i32` at player `+0x74`. The player vtable is `0x100c2238`: slot `+0x60` get (`0x1001cfc0`), `+0x64` add (`0x1001cfd0`), `+0x68` subtract (`0x1001cfe0`). No clamping, so the balance can go negative. **Confirmed.**
- **Booking** (`0x10029950`, when `(u16)(tick − last) ≥ 2000`, then `last = tick`; **confirmed** from the disassembly):
  ```
  tax     = world property 10                       // 0, 1 or 2
  climate = island property 7                       // likely the island climate 0…6
  income  = Σ over goods 0x1a…0x28 of (Preis[climate] + PreisLevel[tax]) · billed[id];  billed[id] = 0
  income  = (income + 128) >> 8
  player.money += income
  stat0   = stat0·104/105 + income;  stat4 = stat4·104/105;  stat5 = stat5·104/105
  player.money −= (stat1 + stat2) / 15                // unsigned division
  ```
- **Tax level** = world property 10 (`+0x69cf`, getter `0x100a3380`, setter `0x100a3230`). It is set from the scene or savegame header when a scene is loaded (`AnnoScene.dll` `0x100025a0`, `0x100029f0`) and cycled `(v + 1) mod 3` by `AnnoFrame.dll` `0x10006070`. That function is reached from frame command `0x9c5c` (`0x10006e10`) and runs only when net-client slot `+0xfc` returns 0, i.e. in single-player. **Confirmed** code. `Texte.dat` has `[TOOLTIP:INFOSTADT_STEUERBAR]` "Tax rate" and the city page "Change taxes" (original file, `game/app/Texte.dat` lines 1085–1094), so property 10 is **likely** the tax rate. It is one value for the whole world, not per city. So taxes are a surcharge on the price of the goods residents receive (`PreisLevel` 0/5/10 for most goods; NAHRUNG has none). They are not a per-head tax.
- **City statistics** (getter city slot `+0x6c` = `0x1002dc70`, add `+0x70` = `0x1002dd40`, sub `+0x74` = `0x1002de40`), **confirmed**:
  - stat 0 `+0x60c` sales to residents. Smoothed; shown as `value · 15 / 105`, which at steady state is the income per 15 bookings = **per game minute**.
  - stat 1 `+0x604` and stat 2 `+0x608` are raw per-minute running costs. stat 1 = civil buildings: the house class returns 1 from slot `+0x90` (`0x10025400`). stat 2 is **likely** military ("Military costs" in the city page).
  - stat 4 `+0x5fc` / stat 5 `+0x600` are trade sales and purchases (added by the trade function `0x100ac360`), shown like stat 0.
  - stats 8, 9, 13, 14 are u16 counters.
- **Upkeep source** (base building `0x10072a60`, on (de)activation): the building's stat kind (slot `+0x90`) loses the type's cost for the old state and gains the cost for the new state. The cost comes from building type slot `+0xcc(2, active)`, so active and inactive costs differ. **Confirmed**. That slot returns the type fields loaded from `PROD3` u16 `0x00` and `0x02` (`production.md` §2.3), so those two values are the running costs. Which index means "active" is **likely** index 1, the larger value. Per-building values: `docs/research/playbook.md` §14.
- **City store**: `+0x334`, 56 × 12 bytes indexed by goods id: `u32` amount, then `u16` billed (consumer goods only, reset at each booking), `u16` added, `u16` removed (both × 63/64 every 2500 ticks). Capacity `+0x624`. Add `0x1002da50` (clamped to capacity), remove `0x1002da90`. **Confirmed** fields; the names of the two decaying counters are **likely**.

## 9. Units

- Updates: house every 2500 ticks = **5 s**; booking every 2000 ticks = **4 s** = 1/15 game minute (the 15 in the upkeep divisor and in the display scale agree); stage check 5 s; city events 25 s. **Confirmed** constants; seconds via the 2 ms tick (`tick-loop.md`).
- At a steady 100 % a house loses `stock/128` per update = `residents · Menge_int / 192` delivered units per 5 s. Per minute that is `residents · Menge_int / 16` = `2.56 · residents · Menge` units. The `256` and `4096` factors in §5 point to **256 delivered units = 1 t**. Under that reading, **`Menge` = tons per 100 residents per game minute**, and income per ton = `Preis + PreisLevel`. **Likely** (consistent across the decay, the demand, the acceptance test and the `>> 8` in the booking; the store's own unit is not traced).
- `Wohnmax` = residents per house. `VermehrTab` = residents per 5 s update (as stored, 1/64 resident). The tables are indexed by satisfaction in steps of 8/128 = 6.25 %: index 16 = 100 % (food and goods are capped there), 20 = 125 % (infrastructure only). **Confirmed.**

## 10. Floating point and determinism

- x87 only in the loader: `Menge` and `VermehrTab` (`0x10014785`, `0x10014909`), via CRT `strtod`. The stored values are integers. Our loader must use the integer formulas in §1. **Confirmed.**
- House update, mood, upgrade check, delivery, demand, stage unlock and booking are integer-only, with C truncating division (all operands non-negative in practice). **Confirmed** in the disassembly of `0x10076230`, `0x10075fd0`, `0x10075dd0`, `0x10076610`, `0x10029610`, `0x10029950`.
- Random draws in this path all use the world PRNG (world slot `+0xb4`, `tick-loop.md` §3): walker target, upgrade variant, initial satisfaction of directly placed houses, city events. **Confirmed.**
- Floats elsewhere in the city: event message positions in `0x1002ba80`, building positions (`0x10056b10`, `0x10072b50`). They are UI payload or not part of this subsystem. **Likely** harmless.
- Hard-coded limits our rebuild must turn into data (CLAUDE.md): 5 groups (names in the loader, kinds `0x61`–`0x65`, counter object); 16 needs per house; 96 goods ids; 32 stages (u32 masks, `& 0x7fffff` test); consumer goods `0x1a`–`0x28` and infrastructure `0x4c`–`0x5a` as fixed id ranges; WIRTSHAUS ↔ ALKOHOL special case; 10 queued upgrades; 16 cities per island.

## Fixture spec (to run on x86 Windows / Wine against the original)

Observables: house residents and the satisfaction shown in the house info panel (or via a debugger: house `+0x2c`, `+0x2e`, `+0x30…`), city stage mask `+0x618`, player money `+0x74`, city statistics page. Times are in ticks since the house's last update (1 tick = 2 ms game time).

| # | Input | Expected |
|---|---|---|
| P1 | Load `BGruppen.dat`, dump BGruppen object (`AnnoGame` game `+0x5fdc`) | `Menge` integers: PIONIER NAHRUNG 41, LEDER 16, STOFFE 16, SALZ 8, WIRTSHAUS 16, KAPELLE 41; SIEDLER 37, 12, 20, 8, 16, 16, 20, 41, 20, 41; BUERGER 37, 20, 10, 16, 16, 8, 16, 20, 41, 31, 41, 41; KAUFLEUTE 37, 10, 16, 16, 8, 12, 25, 16, 41, 41, 41; ARISTOKRATEN 37, 29, 12, 20, 41, 41, 41, 41. `VermehrTab`: PIONIER −16, −9, 0, 12, 32; SIEDLER −25, −12, 0, 25, 51; BUERGER −32, −16, 0, 32, 64; KAUFLEUTE −38, −19, 0, 38, 76; ARISTOKRATEN −51, −25, 0, 51, 102. Lookup: pioneers' KIRCHE → KAPELLE slot; every group with WIRTSHAUS: ALKOHOL → WIRTSHAUS slot |
| P2 | New pioneer house, no deliveries at all, `INFRA_EVER` reached | after updates 1…11: residents 5.5, 6, 6.5, 6.6875, 6.875, 7.0625, 7.25, 7.4375, 7.625, 7.8125, **8** (update 11 = 27,500 ticks = 55 s); food satisfaction 126, 125, 124, 123 … (shown ≈ 98 %, 98 %, 97 %, 96 %); mood 2, 2, 2, then 1 from update 4; no upgrade (settlers need leather, cloth, salt, tavern, chapel) |
| P3 | New pioneer house, food and leather kept at ≥ 100 % | +0.5 residents per update, 8 residents after update 6 (15,000 ticks = 30 s); upgrade only once cloth, salt, tavern and chapel also give positive settler scores and the materials of the settler house type are in store |
| P4 | Settler house, all supply cut | residents fall by 25/64 then 12/64 per update depending on mood; downgrade to pioneer when residents < 6 (not < 1) |
| P5 | Pioneer house starved until mood −2 | −16/64 per update; at < 1 resident the house is removed (`NoDowngrade`) |
| P6 | City grows past 30, 80, 125 pioneers | stages `1A`, `1B`, `1C` set within ≤ 2500 ticks (+ ≤ 500 scheduling); messages for WIRTSHAUS at `1B`, KAPELLE at `1C`. Settlers count toward pioneer thresholds |
| T1 | Island climate STEPPE, tax level 1; in one 2000-tick period residents receive 300 units NAHRUNG and 100 units LEDER | money +⌊(300·50 + 100·75 + 128)/256⌋ = **+88** (NAHRUNG 45 + 5 steppe bonus, `PreisLevel` none; LEDER 70 + 5) |
| T2 | Same at tax levels 0 / 2 | LEDER part 70 / 80 per unit → +86 / +90 |
| T3 | City with civil running costs 100/min and military 20/min, no sales | money −8 every 2000 ticks (−120 per game minute); city page "Operating costs" 100, "Military costs" 20 |
| T4 | Constant income `I` per booking for ≥ 5 game minutes | city page "Sales revenue" → `15 · I` (smoothing factor 104/105 per booking) |
| T5 | Press "Change taxes" three times (single-player) | tax level 0 → 1 → 2 → 0; in multiplayer the button does nothing |

## Open
- Unit of the city store and of delivered amounts (is 256 = 1 t?); confirm with T1 and a savegame pair (M2.1).
- Where house flags bits 4/5 (`+0x0c & 0x30`, forced mood −1/−2) are set: fire, plague, riot? (`0x1002ba80`.)
- Which building class is the market, and the full resident or market-cart round trip (walker task `0x35`, house slots `+0xd4`, `+0xa4`, `+0xc0`); who calls city slot `+0x88` with take = 0 vs 1.
- How infrastructure "goods" (`0x4c`–`0x5a`) reach the house stock (service radius delivery?).
- Group `BauFirst`/`BauLast`, `Wissen`, goods `BauLast`, `Hiscore`: readers not found yet.
- Island property 7 = climate (**guess** from the 0…6 price index); world property 10 = tax rate (**likely**). Whether multiplayer has any tax control at all.
- The first update of a new house: `+0x0a` starts at the placement tick in single player (the net client's `now`, stale by up to 50 ticks) and at 0 only in multiplayer and after a load, so the 16-bit difference to the current tick decides when it first runs, and a house created with 0 may catch up in several quick steps (up to 131). The rebuild uses the placement tick in both modes (`docs/sim/state.md` F1, §7.3). The house is created by `BuildingIndex::Create`, which also increments the group's house count: `kind - 0x61` selects a 0x118-byte BGRUPPE record and the FIRST BYTE of that record (BGruppeNr) is the counter index `g` (`inc word [city + 0x144 + 0x46 + 2 * g]`, Create 0x10024c13..0x10024c2d; Remove reads the same byte through building slot `+0x88`). For the shipped BGruppen.dat (PIONIER, SIEDLER, BUERGER, KAUFLEUTE, ARISTOKRATEN in file order) `g = kind - 0x61`; for another file order it is not. Corrected 2026-10-19 and in the S6 review round.
- `0x1002d3b0`, the `+800` sub-object (`0x10027400`), the upgrade queue filler (`+0x30c0`), and the downgrade path `0x1002cdd0` in detail.
- Running costs: `PROD3` `0x00`/`0x02` through slot `+0xcc(2, ·)` (§8). Still open: which index is active (likely 1, the larger value) and the upgrade material costs (`0x1002c030`; the house `HEAD` materials match the playbook's upgrade figures).
- The display formula for satisfaction in the house panel (percent = `s · 100 / 128`?), for the fixture readout.
