# Population, needs, growth and taxes

**Status:** first pass (2026-10-17); house and city field inventory T32.1 (2026-10-25); house tick pseudo-code T32.2 (2026-10-25), static analysis only. Modules (GOG 2.0.0.5, image base `0x10000000` each): `AnnoGame.dll` (loader for `Data/BGruppen.dat`), `AnnoWelt.dll` (houses, cities, balance), `AnnoFrame.dll` (tax button). Tools: Ghidra 12.1.4 headless (`cache/ghidra/plus/*.c`), `objdump -d -M intel` for every constant quoted below, a scratch model (`cache/pop/model.py`, not committed) for the fixture numbers. `AnnoWelt`/`AnnoGame` have no RTTI, so class names are ours. Addresses are in the module named in each section. Behaviour is paraphrased; no code is copied.

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

The building factory `0x10024010` maps building kinds `0x61`–`0x65` to the house class: constructor `0x10077220`, vtable `0x100c9958`, 0x50 bytes, base class `0x10075c60` → `0x100734c0`. It then calls slot `+0x84` (`SetGroup`, `0x10076b90`) with group record `kind − 0x61` and increments the city's house count: `inc word [BuildingIndex + 0x46 + 2·BGruppeNr]` at `0x10024c2d` (`this` = `city+0x144`; `city+0x18a` = that word for group 0). The counter index is the first byte of the selected `BGRUPPE` record, not `kind − 0x61` for an arbitrary file order. Fields:

| Offset | Type | Meaning |
|---|---|---|
| +0x04 | u16 | index in the city's house list (**D** T32.1: slot `+0x3c` = `0x10035bf0`) |
| +0x06, +0x08 | u16 | tile position (x, y) (**D** T32.1: slot `+0x50` = `0x10071ba0`) |
| +0x0a | u16 | tick of the last population update (base constructor sets 0; Create property `0x23` stores `now`, `0x100733ac`) |
| +0x0c | u8 | flags; bits 4/5 force a bad mood (§4); bit 2 from the type (`0x10073318`–`0x10073329`); bit 3 = building switched off (**likely**, `0x10072a60`) |
| +0x10 | ptr | city (**D** T32.1: attach kind `0x1400`, `0x100732fb`) |
| +0x14 | ptr | building type (**D** T32.1: attach kind `0x700`, `0x1007330b`) |
| +0x18…+0x24 | 4 ptr | active walkers (**D** T32.1: attach kind `0x1500` into the first free slot, `0x10075b5c`) |
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
- **Consumption billing** (city slot `+0x88` = `0x1002dac0`, `(goods id, amount, ·, take)`): only for consumer goods `0x1a`–`0x28`, `billed[id] += amount`. If `take`, `min(amount, store)` is also removed from the city store. **The house tick does not call this** (T32.2: no xref from `0x10076230`). Direct `.text` callers: `0x1006b520` and `0x10070120` with take = 0, `0x100702f0` with take = 1; each has `this` = a building (`this+0x10` = city). Which class is the market is not traced (OQ-186). **Confirmed** (function and callers), **likely** (class).
- **Upgrade check** (`0x10075dd0`), **confirmed**: no upgrade if `NoUpgrade`. Otherwise evaluate the **next** group's forced needs with the current house stocks (slot through the current group's lookup; infrastructure capped at 128 here, not 160). The next group's `NahrungTab`, `BedarfTab` and `InfraTab` values must all be **> 0**. Then city slot `+0x94` (`0x1002e080`) must accept: at most 10 queued (`+0x30e8`), house kind `0x61`–`0x80`, and the city store holds the materials of the next house type. That type is `type_id − type_id mod 20 + 20` (house types come in blocks of 20 per level), checked by `0x1002c030` (`0x1002e000`).
- **Upgrade** (`0x1002e0d0` → `0x1002cbe0`), **confirmed**: removes the materials (goods `0x29`–`0x2d`: WERKZEUG, HOLZ, ZIEGEL, MAMOR, and id `0x2d`) from the store and books them in `+0x5d8`. If the type has variants (property `0x32`) it picks one with the world PRNG. It moves the residents between the per-group counters and house counts, sets the new type, and calls `SetGroup(next record)`. **Downgrade** (`0x1002e120` → `0x1002cdd0`), **confirmed** T32.2: previous type `id − id%20 − 20`, else `id − id%20 − 19`; group `− 0x118`; same variant draw and counter / count / `SetGroup` / type swap.
- **`SetGroup`** (`0x10076b90`), **confirmed**:
  - From an old group, each need's satisfaction is carried over by goods id. When the new satisfaction is ≥ 102, it is also given to the `IdSec` chain. The new stock is `Menge_new · s · E / 1536`.
  - Needs new to the house get stock 0, except slot 0 (food), which gets `Menge · E / 12` (100 %).
  - On a downgrade, needs that are new to the house start at satisfaction 102, and `E` is reset to `residents · 128`.
  - **First assignment to a non-pioneer group** (a higher house placed directly, e.g. aristocrats or scenario houses): residents = `Wohnmax`, `E = Wohnmax · 128`, and every forced need gets satisfaction `128 − (PRNG & 31)` from the world PRNG.
  - A new pioneer house keeps the constructor values: 5 residents, `E` = 640, food 100 %, everything else 0.
- **Save/load**: slot `+0x80` (`0x10077130`) writes an 8-byte header (residents as an integer, need count, version 2) and one byte per need with `min(s_j, 160)`. Slot `+0x7c` (`0x10076e60`) restores residents with `E = residents · 128` and the stocks from the satisfaction bytes (version 0/1/2 as in `annowelt.md` (f)). **Confirmed** for save and load (T32.1 re-read `0x10076e60`–`0x1007711b`).

## 6. Resident counters per city

City `+0x2ec` = counter object (vtable `0x100c3018`, reached as city slot `+0x5c`), one `i32` per group at `+4 + 8·g`. Slot 0 = residents of `g`, slot 4 = total, slot 8 = residents of group `g` **and all higher groups**, slots `+0xc`/`+0x10` = add/sub. **Confirmed.** The five groups are hard-coded here (`5 − g` loop).

## 7. Building stages (`BAUSTUFEN`, city `0x10029610`, every 2500 ticks) — **confirmed**

For each stage `i` = 0…30 not yet set in the city mask `+0x618`: if `counter.slot8(stage.group) ≥ stage.count`, set bit `i`. That is, *residents of that level or higher* ≥ the threshold: `INFRA_STUFE_1A` = 30 pioneers or better, `2A` = 25 settlers or better, `5F` = 5000 aristocrats. If the low 23 bits of the player's mask (property `0x21`) are numerically below the city mask, the city announces every infrastructure good `0x4c`–`0x5a` whose `BauFirst` is the stage just reached (text ids `0xfb`–`0x107`, e.g. WIRTSHAUS at `1B` = 80 pioneers, KAPELLE at `1C` = 125). It then ORs its mask into the player's. The player mask then gates consumption (§4: needs whose good is not yet available neither decay nor count as the lowest need).

## 8. Money: income, tax level, upkeep

- **Player money** = `i32` at player `+0x74`. The player vtable is `0x100c2238`: slot `+0x60` get (`0x1001cfc0`), `+0x64` add (`0x1001cfd0`), `+0x68` subtract (`0x1001cfe0`). No clamping, so the balance can go negative. **Confirmed.**
  - **Confirmed by running** (Wine runner, T16, 2026-10-22). A read-only value scan for the HUD gold (500000, then 499996 after upkeep) left one address. It is `+0x74` of an object whose vtable is AnnoWelt `+0xc2238`, the static address above.
  - A Citizen game holds seven such objects. In every one, `+0x08` points to the live world object.
  - Fields seen:
    - `+0x24`: name, 32 bytes, zero-padded;
    - `+0x44`: kind, 32 bytes (`HUMAN`, `CG01`, `CG02`, `VENEZIANER`, `DUMMYPLAYER`, empty for the two natives "Yaok" and "Sokkot");
    - `+0x64`: a 16-byte GUID. The human's equals the profile's `USGUID` (`docs/formats/user-dat.md`); the others are zero.
  - Money of the others at start: 10000; the two `CG` players had 9696 a few seconds in.
  - Building costs are debited at once on placement. Placements verified by money and store deltas match `Anlagen.dat` `HEAD` (playbook §14), e.g. a forester −150 gold and −2 tools. Reader: `tools/orig/gamestate.py` `players()`.
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
- **Upkeep source** (base building `0x10072a60`, on (de)activation): the building's stat kind (slot `+0x90`) loses the type's cost for the old state and gains the cost for the new state. The cost comes from building type slot `+0xcc(2, active)`, so active and inactive costs differ. **Confirmed**. That slot returns the type fields loaded from `PROD3` u16 `0x00` and `0x02` (`production.md` §2.3), so those two values are the running costs.
  - `0x02` is the cost while active and `0x00` while shut down. The in-game help agrees on all 50 buildings for which it gives both (`docs/research/manual-and-help.md` §1.2).
  - Per-building values: `docs/research/playbook.md` §14.
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

## 11. State or derived (T32.1)

Static re-read of the house constructors, `Load`/`Save`, the house tick's field writes, Create's attach/index stores, and the city counter. No game run. Addresses are AnnoWelt unless named. **This-adjustment:** house methods run with `this` = the house object (vtable `0x100c9958` installed at `[house+0]` by `0x1007722e`); city methods with `this` = the city (vtable `0x100c2f40` at `0x1002fa36`); the resident counter is the embedded object at `city+0x2ec` (vtable `0x100c3018` at `0x1002f9e3`); Create `0x10024010` has `this` = `city+0x144` (BuildingIndex; city ctor `lea ecx, [ebp+0x144]` at `0x1002f9b0`, then `mov edi, ecx` at `0x10024036`).

### 11.1 Function table (first row = `this`)

| Function | this | Why | Range read |
|---|---|---|---|
| Base ctor `0x100734c0` | house | `ecx`; writes vtable `0x100c92b8` | `0x100734c0`–`0x100734e1` |
| House-base ctor `0x10075c60` | house | `ecx`; calls `0x100734c0`; vtable `0x100c9818` | `0x10075c60`–`0x10075c88` |
| House ctor `0x10077220` | house | `ecx`; calls `0x10075c60`; vtable `0x100c9958` | `0x10077220`–`0x1007724b` |
| Load slot `+0x7c` `0x10076e60` | house | vtable `0x100c9958+0x7c` | `0x10076e60`–`0x1007711b` |
| Save slot `+0x80` `0x10077130` | house | vtable `0x100c9958+0x80` | `0x10077130`–`0x10077211` |
| SetGroup slot `+0x84` `0x10076b90` | house | `ebx = ecx`; vtable `+0x84` | `0x10076b90`–`0x10076e4e` |
| Tick slot `+0x8c` `0x10076230` | house | `esi = ecx`; vtable `+0x8c` | `0x10076230`–`0x100765af` (gate, `E`, stocks, growth, floors) |
| Index slot `+0x3c` `0x10035bf0` | house | Create after list insert (`0x10024c04`, `0x1002516d`) | `0x10035bf0`–`0x10035bf9` |
| Pos slot `+0x50` `0x10071ba0` | house | Create `0x1002467c` | `0x10071ba0`–`0x10071bb2` |
| Rot slot `+0x68` `0x10071c10` | house | Create `0x1002468a` | `0x10071c10`–`0x10071c25` |
| Height slot `+0x70` `0x10071c40` | house | Create `0x10024698` | `0x10071c40`–`0x10071c47` |
| Attach slot `+0x28` `0x10075b10` / `0x100732d0` | house | Create `0x1002465f`, `0x100246be` | `0x10075b10`–`0x10075b5f`, `0x100732d0`–`0x10073330` |
| Prop slot `+0x4c` `0x10073380` | house | Create property `0x23` → `+0x0a` | `0x10073380`–`0x100733c9` |
| City ctor (counter init) | city | `ebp`; vtable `0x100c2f40` | `0x1002f9b0`–`0x1002fa74` |
| City slot `+0x5c` `0x1002dfb0` | city | returns `city+0x2ec` | `0x1002dfb0`–`0x1002dfb6` |
| Counter get/add/sub | `city+0x2ec` | vtable `0x100c3018` | `0x10022e10`, `0x10022df0`, `0x10022e20`, `0x10022e50`, `0x10022e60` |
| Create house-count inc | `city+0x144` | `inc word [index + 2·BGruppeNr + 0x46]` | `0x10024c13`–`0x10024c3c` |
| Billing slot `+0x88` `0x1002dac0` | city | consumer goods `0x1a`–`0x28` | `0x1002dac0`–`0x1002daff` |
| City save slot `+0x58` `0x1002ebb0` | city | sweep: no `+0x2ec` / `+0x2f0` / `+0x18a` | `0x1002ebb0`–`0x1002f264` |

### 11.2 House fields

| Offset | Type | Meaning | Who writes | Saved? | Rebuild |
|---|---|---|---|---|---|
| +0x00 | ptr | vtable `0x100c9958` | ctor `0x1007722e` | no (class) | derived |
| +0x04 | u16 | list index | slot `+0x3c` `0x10035bf0` from Create after `mov [list+esi*4], house` | no | derived from the list |
| +0x06, +0x08 | u16 | tile x, y | slot `+0x50` `0x10071ba0` | no (the `PROD/ENTRY` position is the save) | building record |
| +0x0a | u16 | last population tick | ctor `0x100734ca` = 0; property `0x23` `0x100733ac` = Create `now`; tick `0x1007625e` `last += 2500` | **no** (Load does not write it) | reset on load (Open in §10: catch-up) |
| +0x0c | u8 | flags (rot bits 0–1; bit 2 from type; bits 4/5 mood force) | base ctor `and 0xc3`; rot slot `+0x68`; type attach `0x10073329` | no | building rotation + runtime flags |
| +0x0d | u8 | zeroed | `0x100734c4` | no | — |
| +0x0e | u8 | `0x82` house / `0x86` base | `0x10075c81` after `0x100734dd` | no | `byte0e` already in the sim |
| +0x0f | u8 | height | slot `+0x70` `0x10071c44` | no (ENTRY byte) | building record |
| +0x10 | ptr | city | attach kind `0x1400` `0x100732fb` | no | derived |
| +0x14 | ptr | type | attach kind `0x700` `0x1007330b` | no | derived |
| +0x18…+0x24 | 4 ptr | walkers | attach kind `0x1500` `0x10075b5c`; ctor zeros | figures (`TASK`) | figure registry, not house payload |
| +0x28 | ptr | `BGRUPPE` record | SetGroup `0x10076e43`; ctor 0 | no | registry group id (load-time data) |
| +0x2c | i16 | residents · 64 | ctor `0x140` (5·64); Load `residents<<6`; tick `VermehrTab`; SetGroup | **integer residents** in the 8-byte header (`[1]`, `0x10077161`) | **state** |
| +0x2e | u16 | `E` | ctor `0x280` (5·128); Load `residents<<7` (`0x10076f01`); tick `E·127/128 + residents` (`0x10076277`–`0x100762a9`) | **no** (Load overwrites from residents) | runtime state; **derived after load** |
| +0x30+2j | 16 × u16 | stock of need `j` | ctor zeros 32 bytes `0x10077240`; Load from `s_j`; SetGroup; Delivery `0x10076610`; tick decay | **satisfaction bytes** `min(s_j, 160)` (`0x100771ae`–`0x100771d2`), not the raw stock | **state** (save form = `s_j`; runtime = stock) |

Mood is not a field: slot `+0x9c` `0x10075fd0` computes it. The house record the original round-trips is `[1]` residents, `[2]` need count, `[3]` version 2, then `n` bytes `s_j` (`annowelt.md` (f); T32.1 Load/Save re-read). Direct callers of Save are virtual only (reloc `0x100c99d8` = vtable slot `+0x80`; OQ-185).

### 11.3 City population fields

Counter `this` = `city+0x2ec`. Ctor writes vtable `0x100c3018` and zeros **six** 8-byte group slots at `+0x2f0` (`ecx = 6`, `0x1002f9c5`–`0x1002f9db`); getters loop **five** groups (`0x10022df5`). Per group `g`: i32 residents at `[counter + 4 + 8g]` = `city+0x2f0+8g`; the four bytes after each i32 (`city+0x2f4+8g`) are zeroed by the ctor and unused by the five methods read. Slot 0 `0x10022e10` returns that i32; slot 4 `0x10022df0` sums groups 0–4 (total); slot 8 `0x10022e20` sums groups `g`…4; `+0xc`/`+0x10` add/sub that i32 (`0x10022e50`/`0x10022e60`). City slot `+0x5c` = `lea eax, [ecx+0x2ec]`. T15's `city+0x2f0` HUD match is group 0's i32 (pioneers).

| Offset | Type | Meaning | Who writes | Saved? | Rebuild |
|---|---|---|---|---|---|
| city+0x2ec | ptr | counter vtable `0x100c3018` | city ctor `0x1002f9e3` | no | embedded object |
| city+0x2f0+8g | i32 | residents of group `g` | Load `0x10076ef6`/`0x10076f31` (sub old, add new); house tick (remove/add around `VermehrTab`, `0x10076407`…); Create/upgrade/downgrade | **no**: city save `0x1002ebb0`–`0x1002f264` never reads `+0x2ec`/`+0x2f0`; city `VOLK` is unused (`annowelt.md` (g)) | **derived after load** (sum of houses); original keeps it incrementally at run time |
| BuildingIndex+0x46+2g = city+0x18a+2g | u16 | houses of group `g` | Create `inc word [index+eax*2+0x46]` `0x10024c2d` (`eax` = `BGruppeNr`) | **no** (not in `0x1002ebb0`) | **derived** from the house list |
| city+0x334+12id+0 | u32 | store amount | `WARE` load; add/remove; billing `take` | **yes** (`WARE`, tons<<8) | already `CityStock` |
| city+0x334+12id+4 | u16 | billed this booking (ids `0x1a`–`0x28`) | `0x1002dae0`; booking clears | **no** (not in `WARE`'s 8-byte entry) | **derived** / period scratch |
| city+0x334+12id+8 | u16 | amount taken when `take` | `0x1002daf2` | **no** | period scratch |
| demand | — | house slot `+0xa4` `0x100765c0` computes `Menge·E·256/44236 − stock/16` | none stored on the city | no | **derived** |

There is no separate city-wide demand or consumption sum object. Billing is per-good at store `+4`; income sums those at booking (`0x10029950`, §8).

### 11.4 Conclusion (OQ-165)

The original **keeps both** at run time: per-house `+0x2c` / stocks / `E`, and the city's per-group i32s. What it **saves** is the house record (integer residents + `s_j`). After a load it **rebuilds** `E` and stocks from that record and **patches** the city counters from each house `Load`. City totals and house counts are not an independent save channel. For the rebuild: house residents and per-need stocks (or the saved `s_j`) are **state**; city group totals and house counts are **derived** (may be cached like the original, rebuilt on load). `E` is runtime state that does not round-trip (Load sets `residents<<7`).

## 12. The house tick as pseudo-code (T32.2)

Static re-read of the house tick, mood, upgrade check, city Upgrade/Downgrade, booking, demand, delivery and `SetGroup`. No game run. AnnoWelt unless named. **This-adjustment:** every method below has `this` = the object in the first column (house vtable `0x100c9958` at `0x1007722e`; city `0x100c2f40` at `0x1002fa36`; counter `city+0x2ec` / `0x100c3018` at `0x1002f9e3`).

| Function | this | Why | Range |
|---|---|---|---|
| Tick slot `+0x8c` `0x10076230` | house | `esi = ecx`; vtable `+0x8c` | `0x10076230`–`0x100765b9` |
| Mood slot `+0x9c` `0x10075fd0` | house | `ebx = ecx`; called at `0x100763ed` | `0x10075fd0`–`0x10076224` |
| Demand slot `+0xa4` `0x100765c0` | house | `ecx`; not called from the tick | `0x100765c0`–`0x10076607` |
| Delivery slot `+0xc0` `0x10076610` | house | `ecx`; not called from the tick | `0x10076610`–`0x10076668` |
| `s_j` helper `0x10076ae0` | house | `ecx`; arg = slot `j` | `0x10076ae0`–`0x10076b27` |
| Upgrade check `0x10075dd0` | house | `ecx`; called at `0x1007649c` | `0x10075dd0`–`0x10075fc4` |
| SetGroup slot `+0x84` `0x10076b90` | house | `ebx = ecx` | `0x10076b90`–`0x10076e4e` |
| Booking `0x10029950` | city | `esi = ecx`; city tick step (e) | `0x10029950`–`0x10029aa4` |
| Billing slot `+0x88` `0x1002dac0` | city | vtable `0x100c2fc8` | `0x1002dac0`–`0x1002daff` |
| Queue slot `+0x94` `0x1002e080` | city | from upgrade check | `0x1002e080`–`0x1002e0c1` |
| Upgrade slot `+0x98` `0x1002e0d0` → `0x1002cbe0` | city | `esi`/`edi = ecx` | `0x1002e0d0`–`0x1002e118`, `0x1002cbe0`–`0x1002cdc4` |
| Downgrade slot `+0x9c` `0x1002e120` → `0x1002cdd0` | city | `esi`/`edi = ecx` | `0x1002e120`–`0x1002e168`, `0x1002cdd0`–`0x1002cf52` |
| Materials `0x1002c030` | city | goods `0x29`–`0x2d` | `0x1002c030`–`0x1002c08c` |

### 12.1 `BGruppen.dat` field map (load-time data, never state)

`formats::text` names from `docs/formats/data-text.md`. Offsets are the group record (`[BGRUPPE]`, 0x118 bytes) or the goods record (`[WAREN]`, 26 bytes) as in §1. `Priority` has no reader in this path (§1).

| File field | `formats::text` name | Offset | Reader (first site) |
|---|---|---|---|
| group index | `BGruppeNr` | group `+0` | tick `0x1007641c`; Create `0x10024c2d` |
| need count `n` | (derived from `Ware` lines) | group `+2` | tick `0x100762ad` |
| no upgrade | `NoUpgrade` | group `+1` bit 0 | upcheck `0x10075dd7` |
| no downgrade | `NoDowngrade` | group `+1` bit 1 | tick `0x10076519` |
| goods id of need `j` | `Ware` | group `+0x64+4j` | tick `0x100762ca`; mood; SetGroup |
| forced flag | `Force` | group `+0x65+4j` | mood `0x1007601d`; SetGroup `0x10076d70` |
| quantity (integer) | `Menge` | group `+0x66+4j` u16 | tick `0x10076300`; demand; `s_j`; mood |
| id → slot | (post-pass, §1) | group `+3+id` | delivery `0x1007661b`; demand `0x100765ca` |
| end of goods needs | (derived) | group `+0xcc` | mood `0x10075ffe` |
| end of infra needs | (derived) | group `+0xcd` | mood `0x10076077` |
| residents cap | `Wohnmax` | group `+0xc8` | tick `0x1007645c`; SetGroup `0x10076dac` |
| prev group's cap | `Wohnmax` | `group−0x50` = prev `+0xc8` | tick `0x10076527` |
| food table | `NahrungTab` | group `+0xcf` | mood `0x1007615c`; next group at `+0x1e7` in upcheck |
| goods table | `BedarfTab` | group `+0xe4` | mood `0x1007618e`; next `+0x1fc` |
| infra table | `InfraTab` | group `+0xf9` | mood `0x100761a8`; next `+0x211` |
| growth | `VermehrTab` | group `+0x10e` (mood −2…+2 at `+0x110+mood`) | tick `0x1007642c` |
| goods-table ptr | — | group `+0x114` | tick `0x100762c2` |
| stage gate of a good | `BauFirst` | goods `+2` | tick `0x100762d3` (player mask bit) |
| substitute good | `IdSec` | goods `+1` | delivery `0x1007664c` |
| price / tax surcharge | `Preis` / `PreisLevel` | goods `+4+2·climate` / `+0x12+2·tax` | booking `0x1002999d`–`0x100299de` |
| group `BauFirst`/`BauLast`, `Hiscore`, `Priority` | those keys | group `+0xca`/`+0xcb`/`+0xce`; unused scratch | **not read** in this path |

### 12.2 House tick (every 2500 ticks)

`s_j = min(stock_j · 1536 / (E · Menge_j), 160)` (`lea eax,[eax+eax*2]; shl 9` = `×1536`; cap `0xa0`). Divisions truncate toward zero (MSVC `cdq` / `idiv`, or `cdq; and edx, 2^k−1; add; sar` for `/128` and `/64`).

The house tick **does not** subtract from the city store (`city+0x334`). It decays **house** stocks. City-store take is billing with `take = 1` (`0x1002dae4`–`0x1002daf6`), not called from `0x10076230`. Units: delivery adds `amount·16` to stock (`0x10076638`); demand subtracts `stock/16`; booking does `(sum + 128) >> 8`. That **256 delivered units = 1 t** (store is tons≪8) stays **likely** (next: T1 / savegame pair, M2.1). Magic `0xbda21029` at `0x100765e4` is signed `/44236`.

```
# this = house; arg0 = tick
if (u16)(tick − [house+0x0a]) < 0x9c4: return          # 2500
[house+0x0a] += 0x9c4
mask = player property 0x21, second dword               # city+0x10, slot +0x1c, then +0x48(0x21)
E = E·127/128 + trunc(residents_64 / 64)                # [+0x2e], [+0x2c]; both toward 0
lowest = +∞
for j in 0 .. n−1:                                      # n = [group+2]
    if mask has bit BauFirst(goods[Ware(j)]):
        stock[j] *= 127/128                             # consumption of house stock
        lowest = min(lowest, s_j)
if lowest < 0x66 and walker_count < type.slot+0xb8(0):  # 102; walkers at +0x18…+0x24
    nvar = type.slot+0xbc(0)
    r = world.PRNG()                                    # city+0x10 slot +0x18 → +0x18 → +0xb4
    k = ((r & 1) + y + x + index) % nvar                # [+8],[+6],[+4]
    target = type.slot+0xb4(0, k)
    house.slot+0x118(0x35, target, tick)                # walker spawn; role likely
mood = house.slot+0x9c()                                # −2…+2, §12.3
g = BGruppeNr
counter = city.slot+0x5c()                              # city+0x2ec
counter.sub(g, trunc(residents_64 / 64))
residents_64 += VermehrTab[mood]                        # i8 at group+0x110+mood
counter.add(g, trunc(residents_64 / 64))
if mood >= 0:
    if residents_64 > Wohnmax·64:
        counter.sub(g, trunc(residents_64/64) − Wohnmax)
        residents_64 = Wohnmax·64
        if UpgradeCheck(): city.slot+0x98(house)        # §12.4
else:
    if NoDowngrade or [house+0x0c] & 0x30:
        floor = 0x40; remove = 1                        # 1 resident
    else:
        floor = 48 · Wohnmax(prev); remove = 0          # ¾ of lower cap, in ·64 units
    if residents_64 < floor:
        if remove == 0:
            if residents_64 < 0x40:
                counter.add(g, trunc(residents_64/64) − 64)   # D instr.; likely dead (VermehrTab)
                residents_64 = 0x40
            city.slot+0x9c(house)                       # Downgrade
        else:
            island.slot+0xcc(x, y, 0, 0, 0)             # D call; meaning likely: remove
```

### 12.3 Mood (−2 … +2)

```
# this = house
if E == 0: return 0
food = min(s_0, 128) → f = NahrungTab[(food+4)>>3]
goods = mean of min(s_j,128) over forced slots 1…(+0xcc−1);
        forced WIRTSHAUS (Ware 0x4c) in the infra range is added here too;
        128 if no forced goods → b = BedarfTab[(goods+4)>>3]
infra = min of s_j (cap 160) over forced slots +0xcc…(+0xcd−1); 160 if none
        → i = InfraTab[(infra+4)>>3]
if f, b, i all ≥ 0:
    if flags & 0x30: return flags & 0x20 ? −2 : −1      # not + shr 5 | −2
    return (f + b + i + 1) / 3                          # magic 0x55555556
else:
    if flags & 0x20: return −2
    return min(f, b, i)
```

### 12.4 Upgrade and downgrade (which building)

**Check** (`0x10075dd0`): no if `NoUpgrade`. Else score the **next** group's forced needs (`group+0x118`) through the **current** lookup; infra capped at 128. Next `NahrungTab`/`BedarfTab`/`InfraTab` (at `+0x1e7`/`+0x1fc`/`+0x211`) must all be **> 0**. Then city `+0x94`: queue count `+0x30e8` < 10, current kind in the `0x61…0x80` block (`(id−1) & ~0x1f + 1 == 0x61`), and `0x1002e000` finds the next type (`id − id%20 + 20`) and `0x1002c030` says the store covers goods `0x29`–`0x2d`.

**Upgrade** (`0x1002cbe0`): next PRODTYPE = `id − id%20 + 20` (`div 0x14` at `0x1002cc22`). Remove those materials (city `+0x84`), book them at `+0x5d8`. If the next type has property `0x32` with a non-zero count, draw the world PRNG and pick a variant. Move residents off the old group, `SetGroup(next record)`, onto the new; house counts `city+0x18a` dec old / inc new.

**Downgrade** (`0x1002cdd0`): **D** (was likely). Previous PRODTYPE = `id − id%20 − 20`; if that type is missing, try `id − id%20 − 19` (`0x1002ce21`–`0x1002ce3b`). Same variant draw. Group pointer `− 0x118`. Same counter / count / `SetGroup` / type swap.

### 12.5 Taxes (every 2000 ticks, city — not the house)

Booking `0x10029950`: if `(u16)(tick − [city+0x0a]) < 0x7d0` return; then `[city+0x0a] = tick` (not `+= 2000`). Tax = world property 10; climate = island property 7 (**likely**). Income = Σ_{id=0x1a…0x28} `(Preis[climate] + PreisLevel[tax]) · billed[id]`; billed cleared; `(income + 128) >> 8` added to player `+0x74` (slot `+0x64`). Upkeep `(stat1+stat2)/15` subtracted (unsigned). Taxes are a surcharge on billed consumer goods, not a per-head tax.

### 12.6 PRNG draws (world slot `+0xb4` = `0x100a1740`)

| # | Site | Condition | Use |
|---|---|---|---|
| 1 | tick `0x10076397` | `lowest < 102` and walkers < max | walker target index (`(r&1)+x+y+index) % nvar`) |
| 2 | upgrade `0x1002ccd3` | next type property `0x32` count ≠ 0 | variant of the next house type |
| 3 | downgrade `0x1002ce68` | previous type property `0x32` count ≠ 0 | variant of the previous house type |
| 4 | SetGroup `0x10076d7a` | first assignment and `BGruppeNr ≠ 0` and `Force` | per forced need: satisfaction `128 − (r & 31)` |

City events `0x1002ba80` also draw; they are not on this path. Walker spawn's **meaning** (resident fetch) stays **likely** (next: house slot `+0x118`). Island `+0xcc` as remove stays **likely**. Climate = property 7 stays **likely**. OQ-185 (Save dispatcher) still virtual-only (`xref --reloc 0x10077130` → `0x100c99d8` only).

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

## Verified by running (T15 stage 1, 2026-10-22)

Citizen single-player, the copy driven by `tools/orig/control.py` (zoom out, speed 400%, place
pioneer houses in the Kontor's yellow range), objects read by `tools/orig/probe.py` (vtable scan).

- **City inhabitant count is at `+0x2f0` (likely; N4).** The player's city (city vtable `AnnoWelt
  +0xc2f40`, identified by its warehouse goods = the HUD's tools 50 / wood 38) read `+0x2f0` = 27-31
  while the HUD showed ~28 pioneers, i.e. `+0x2f0` (= the `+0x2ec` sub-object slot +4, the "likely
  inhabitants" Open item) tracks the inhabitant count. It was read **while the population was
  changing**, so this is **likely**, not confirmed: read it **paused** and equal to the HUD to
  confirm. `+0x18a` = 4 = the pioneer houses placed (the "houses per level" guess holds); `+0x618`
  (stage mask) = 0x3 for the player, 0x7 for the more-grown AI cities (pop 72 / 96).
- **Decline/removal without a market: an OPEN mismatch, NOT P5 (OQ-T15-HOUSEGROW).** Houses placed
  in the Kontor's range populated (28 pioneers); over ~80 game-s (20 s at 400%) the city `+0x2f0`
  fell 31 -> 21 and `+0x18a` went 4 -> 3 (one house removed). This is **not** starvation removal
  (P5): food decays ~1/128 per 5 s update, so a negative food score takes ~97 updates and emptying
  a house ~50 more — roughly 750 game-s, not 80. And `INFRA_EVER` (threshold 0) is always reached,
  so P2 predicts growth with no deliveries; 31 in 4 houses is ~7.75/house, i.e. growth. So the cause
  of the removal is **unknown** — possibly the tool itself (a road drag across the house, or a
  misplaced click under the screen->tile offset), possibly a model rule we lack. It needs **per-house
  `+0x2c` reads** (house vtable, OQ-T15-HOUSEVT) of the same houses with and without a road to a
  market, fed into `src/sim`; only then does it become a finding. The owner's rule (houses need a
  road to a market; services reach by road; a road connects at the building entrance tile) stands as
  the game's rule, but HOW a missing connection acts on a house (no deliveries / bad mood / removal)
  is unmeasured. The city `+0x2f0`/`+0x18a` aggregate is read now; the per-house read is the fixture
  that decides it.

## Open
- T32.1 closed the house field layout; T32.2 closed the house-tick order, mood, upgrade/downgrade bodies, materials check and booking. Still open below.
- Who dispatches house Save slot `+0x80` on a world save (virtual only; OQ-185, T32.2 re-check still reloc `0x100c99d8` only). Next: `call [reg+0x80]` in world save `0x100ae700` / the building-list walk.
- The house tick does **not** take from the city store; decay is house stock only. Who bills with take = 1 is OQ-186 (`0x100702f0` and the constructors of vtables that hold `0x1006b520` / `0x10070120` / `0x100702f0`).
- Unit of the city store and of delivered amounts (is 256 = 1 t?); confirm with T1 and a savegame pair (M2.1).
- Where house flags bits 4/5 (`+0x0c & 0x30`, forced mood −1/−2) are set: fire, plague, riot? (`0x1002ba80`.)
- Which building class is the market, and the full resident or market-cart round trip (walker task `0x35`, house slot `+0x118`, then `+0xd4`, `+0xa4`, `+0xc0`).
- How infrastructure "goods" (`0x4c`–`0x5a`) reach the house stock (service radius delivery?).
- Group `BauFirst`/`BauLast`, `Wissen`, goods `BauLast`, `Hiscore`: readers not found yet.
- Island property 7 = climate (**guess** from the 0…6 price index); world property 10 = tax rate (**likely**). Whether multiplayer has any tax control at all.
- The first update of a new house: `+0x0a` starts at the placement tick in single player (the net client's `now`, stale by up to 50 ticks) and at 0 only in multiplayer and after a load, so the 16-bit difference to the current tick decides when it first runs, and a house created with 0 may catch up in several quick steps (up to 131). The rebuild uses the placement tick in both modes (`docs/sim/state.md` F1, §7.3). The house is created by `BuildingIndex::Create`, which also increments the group's house count: `kind - 0x61` selects a 0x118-byte BGRUPPE record and the FIRST BYTE of that record (BGruppeNr) is the counter index `g` (`inc word [city + 0x144 + 0x46 + 2 * g]`, Create 0x10024c13..0x10024c2d; Remove reads the same byte through building slot `+0x88`). For the shipped BGruppen.dat (PIONIER, SIEDLER, BUERGER, KAUFLEUTE, ARISTOKRATEN in file order) `g = kind - 0x61`; for another file order it is not. Corrected 2026-10-19 and in the S6 review round.
- `0x1002d3b0`, the `+800` sub-object (`0x10027400`), and the upgrade queue filler (`+0x30c0` / `0x1002d350`).
- Materials check `0x1002c030` is **D** (type slot `+0xcc(0, id)` + queued vs store `city+0x520` minus spent `+0x5d4`, ids `0x29`–`0x2d`). The house `HEAD` materials match the playbook's upgrade figures. The running-cost pair is settled (§8).
- The display formula for satisfaction in the house panel (percent = `s · 100 / 128`?), for the fixture readout.
- Walker spawn's meaning (resident fetch) and island slot `+0xcc` as house removal stay **likely** (next: house `+0x118`, island `+0xcc`).
