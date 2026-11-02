# Ship cargo: the manual ship -> own-warehouse unload

**Status:** MAP-CARGO-1 U1, 2026-11-02. Static reading of the original instructions (no run of the game, no emulation: the
primitives below are straight-line register arithmetic that `tools/cargo_ref.py` transcribes and checks against hand-derived
vectors). Contract that uses this note: [`docs/sim/ship-cargo.md`](../sim/ship-cargo.md).
Earlier notes it completes: [`ship-founding.md`](ship-founding.md) 10.7 (hold record, deduction, emptied hold),
[`fire-and-ships.md`](fire-and-ships.md) (speed factor `+0x3c`), [`production.md`](production.md) 5.3 (city stock record),
[`annowelt.md`](annowelt.md) (city capacity `0x10028400`).
Labels: **D** read from disassembly; **S** by shape (call target or meaning not proven); **U** unread; **P** file or table value.
Addresses are virtual with image base `0x10000000`; the modules are different images that share that base.

Pinned inputs (SHA-256 of the files in `game/app` read for this note):

| File | SHA-256 |
|---|---|
| `AnnoWelt.dll` | `f220a878510f740c4cfb7224ade0a260e9a0fd7e99113e1201e7a37c0dfa6ae2` |
| `AnnoFrame.dll` | `dd3f36ff564d2685d10795272c4ed3c0b2da32a946c2883fb1158ff439249b91` |
| `AnnoGame.dll` | `ac721220afd27ebca5fd2ece6a8ff452289b9d7458c4e091c68adea2012db7bc` |
| `View.dll` | `e32fa8a5ff0f3f0b00ef05618daf8309c692daaa57316147eb6cb1f3637733a1` |
| `Texte.dat` | tooltip text only (section 1) |

Tools: `tools/disasm.py` style linear sweep (capstone 5.0.7, pefile 2024.8.26, the private venv of `cache/map2-founding-evidence/venv`),
vtables read with `pefile`. Listings stay in git-ignored scratch space, as in the earlier notes. Independent transcription:
`tools/cargo_ref.py`, vectors `fixtures/synthetic/sim/cargo_vectors.txt`.

## 0. Result in one paragraph

The manual unload is **one world function**, `AnnoWelt.dll 0x100ac360` (world vtable slot `+0x100`, `ret 0x10`, args
`(rec, ship, flag, city)`), reached from the ship view's "Unload goods" click through `tNetClient` slot `+0x60`. The record names
**a hold index and a goods id and a request in whole tons**; the function moves goods from that one hold into the city stock.
It has **no range test, no state test and (in multiplayer) no sender test**; the range is decided *before* the call, by the view,
with `ship slot +0xc4` = `0x1003f200`: a warehouse (`PRODTYPE 0x23`, KONTOR) whose footprint is inside a digital disc of radius 5
around the ship's truncated tile, in a city that owns one of 24 outline tiles of that disc. The target is the **city of that
warehouse** (never "a city on the island"). The arithmetic has three original quirks that lose or mint goods (section 6); the
rebuild adapts them (contract section 4).

## 1. Names the game shows (P)

`Texte.dat` `[TOOLTIP:SCHIFFTAB]`: "Load/unload ship" is the fourth ship-view tab. `[TOOLTIP:SCHIFF_HANDELALL]`: "Unload goods",
"Load goods", "Determine amount of load". `[TOOLTIP:SCHIFF_HANDELEIGEN]`: the goods categories "Resources", "Building material",
"Consumer goods", "Weapons". `[KONTORFREMD]`: "Purchase", "Sales" (the foreign-warehouse trade, excluded). `[TOOLTIP:SCHIFFMAIN]`:
"Build warehouse", "Throw cargo overboard" (a different action, section 8). The manual's "unload the ship" (`game-mechanics.md`
line 42) is this tab. `[TOOLTIP:SCHIFF_ROUTEACTION]` ("Load goods onto ship", "Unload goods from ship") belongs to the automatic
trade routes and is not read as the manual rule.

## 2. The chain, in order (D unless marked)

```
View.dll ship view (object with figure at +0x630, local player at +0x618, frame game at +0x62c, controls at +0x5d4)
   amount buttons         0x1005a5c0   [view+0x640] = 1 / 5 / 10 / 50 (jump table 0x1005a63c); constructor default 10 (0x1005a598)
   "Unload goods" click   0x1005a7f0   builds rec, city = 0x1005d8c0(), net client = [frame]+slot 0x98
        -> client slot +0x60(rec, ship = [view+0x630], flag = 1, city)                                       0x1005a8a4
   "Load goods"  click    0x1005a8d0   same call with flag = 0 (city to ship; not this package)               0x1005a9e2
   target city           0x1005d8c0   ship type group == 0x71 ? ship slot +0xc4(0, &out) : (1, &out); out[0] -> building slot +0x18 = city;
                                       city slot +0x1c = owner; accepted if owner == local player ([view+0x618]), else the relation tests (excluded)
tNetClient (single player) slot +0x60 = AnnoFrame 0x100591a0: pushes its four stack arguments unchanged and calls
   world slot +0x100                                                                                        0x100591b9
tNetClientNet (multiplayer) packs it as command 0x6a (section 4) and the dispatcher 0x1005ffd0 -> 0x1005d0a0 calls 0x100591a0
world slot +0x100 = AnnoWelt 0x100ac360   (world vtable 0x100ccdd8: +0xfc = 0x100accf0 throw overboard, +0x100 = 0x100ac360 trade)
   same-owner unload arm 0x100ac91f-0x100accde: hold slot +0x10 (0x1003b9a0), then city slot +0x80 (0x1002da50)
```

Both view handlers were read to the call; they differ only in the pushed flag (`push 1` at `0x1005a883` for unload, `push 0` at
`0x1005a9d8` for load) and in how the hold index is chosen. Which UI control property feeds `rec[0]` (goods) and `rec[1]` (hold
index) is **S**: properties `0x1bf` and `0x1b3` of the clicked control are stored into those bytes (`0x1005a86d`/`0x1005a88f`), and
the world function validates `rec[1]` against the hold count and compares `rec[0]` with the hold's goods, which fixes the meaning.

### 2.1 The record (D, built at `0x1005a86d`-`0x1005a89f`, consumed in 0x100ac360)

| Byte | Meaning | Source |
|---|---|---|
| 0 | goods id | compared with the hold's goods (`0x100ac90a`) |
| 1 | hold index | `< type slot +0x88(0)` else the call returns 0 (`0x100ac470`-`0x100ac47a`) |
| 2-3 | request, **whole tons** (u16) | `<< 8` at `0x100ac930`; the view writes `[view+0x640]` = 1, 5, 10 or 50 |
| 4-5 | unused by the unload arm (the view writes 0) | |
| 6 | 0 for a hold transfer; **1 selects the cannon arm** (section 7), which the view never sends | `cmp byte [edi+6], 1` at `0x100ac371` |

`type slot +0x88(0)` is the first `LOGIC` count of the figure type (`fire-and-ships.md` line 110: "likely the cargo holds"); the hold
accessor is ship slot `+0x84(i)` = `0x100418e0` = `ship + 0xb0 + 12*i` with **no bound check**.

## 3. Eligible ship, target and range: `ship slot +0xc4` (D)

Ship vtable `0x100c4da4` slot `+0xc4` = `0x1003f8e0` (`ret 8`, args `(mode, out)`): mode 0 or 2 -> `0x1003f200`, mode 3 -> `0x1003f740`
(a nearest-point search, not read further), any other mode returns 0 without a search. The view passes mode 0 for a ship whose
type slot `+0x50` (the `BEWOHNTYPE` class group) is `0x71` and mode 1 (= nothing) for everything else (`0x1005d8e7`-`0x1005d8f1`).
**So only ships get a warehouse, and nothing but this function limits "near".**

`0x1003f200` mode 0 (returns the number of warehouses written to `out`; `this` = ship, `[ship+0x10]` = world, `[ship+0x14]` and
`[ship+0x1c]` = the f32 position, divisor `[0x100c4b18]` = 1.0 (P), truncation `__ftol` `0x100b5f90`):

1. **Tile** `(tx, ty) = (trunc(x / 1.0), trunc(z / 1.0))` (`0x1003f254`-`0x1003f274`).
2. **Half-width table** `w[0..5]`, built at `0x1003f217`-`0x1003f24e` by a midpoint-circle recurrence from the literals 5, 0x19, 0x14, 0xa;
   executed by hand and by `tools/cargo_ref.py selftest`: `w = 5, 5, 5, 4, 3, 2` (E, the recurrence is straight-line integer code).
3. **City set** (`0x1003f297`-`0x1003f3b2`): for `k = 0..5`, the four tiles `(tx -/+ w[k], ty -/+ k)` and `(tx -/+ w[k], ty +/- k)`
   (24 tile reads; the `k = 0` pair appears twice). For each: `island = world slot +0x8c(x, y)` (null = skip); `owner = island slot
   +0xec(x, y)`; if `owner != 0xf` (none) then `city = island slot +0xf8(owner)` is added to a duplicate-free list (`0x1003f5c0`,
   capacity 32 pointers, no bound check, unreachable with 24 reads). Only the **outline** is sampled, not the interior.
4. **Rectangle** `{tx - 5, ty - 5, 11, 11}` (`0x1003f3d6`-`0x1003f3f4`).
5. **Per city in list order**: `BuildingIndex::Query` (city slot `+0x60` = `0x1002dfc0` = `city + 0x144`; the object's slot 0 =
   `0x10023f50`, `ret 0xc`, args `(mask, rect, out)`) with **mask 2** for mode 0 (mask 8 for mode 2), which selects the list at
   `BuildingIndex + 0x190` = the market list (PRODTYPE 33..64, `ListLayout::original()` list 5, mask `0x02`, already ported as
   `Island::rect_query`). Mode 2 would look for shipyards (PRODTYPE 7) and is not an unload path.
6. **Per building returned**: its footprint `{x0, y0, w, h}` from building slot `+0x78` (record order x, y, then bytes w, h); per axis
   `d = 0` if the ship tile lies in `[lo, lo + len - 1]`, else `min(|lo - p|, |lo + len - 1 - p|)` (`0x1003f47f`-`0x1003f4cf` for y,
   `0x1003f4d3`-`0x1003f518` for x). **Reject if `dy > 5`**; **reject if `dx > w[dy]`** (`0x1003f51a`-`0x1003f520`). Then the type's
   PRODTYPE (building slot `+0x74` -> type slot `+0x5c`) must be `0x23` (KONTOR; the market-class values are HAUPT 33, MARKT 34,
   KONTOR 35, MARKTKOMBI 36, `production.md` line 39) for mode 0. Matches are appended to `out` in city-list order, then query order.

The view takes `out[0]` only (`0x1005d903`-`0x1005d909`). Consequences (D): (a) the **first** qualifying warehouse in that order
decides the city; a rival's warehouse found first hides the player's own (the view then fails its owner test); (b) the **same**
function is the only "docking" notion: there is no separate dock point, no berth and no speed test.

## 4. The wire and the single-player sink (D)

`tNetClient` vtable `0x1008914c`: slot `+0x5c` = `0x100591d0` (3-argument forward to world `+0xfc`, throw overboard), slot `+0x60` =
`0x100591a0` (4-argument forward to world `+0x100`, trade). `tNetClientNet` dispatcher `0x1005ffd0`, table `0x10060388`: types `0x6a`
and `0x6b` both reach `0x1005d0a0`.

Type `0x6a` payload (`0x1005d0a0`-`0x1005d170`): `u16 [0]` ship id (`0xffff` = none -> the ship argument is 0), `u16 [2]` = island id in
the low byte (`world slot +0x20(0x1300, id)`) and **city number in the high byte** (`island slot +0xf8(n)`), `u16 [4]` -> `rec[2..3]`
(tons), `u16 [6]` -> `rec[4..5]`, `u8 [8]` -> `rec[0]` (goods), `u8 [9]` -> `rec[1]` (hold index), `u8 [0xa]`: bit 0 = flag (1 unload,
0 load), bit 1 -> `rec[6]`. The target is therefore named by **(island, city number)**, not by a building. Nothing on the receiving
side compares the packet's ship or city with the sender (`docs/re/tick-loop.md` line 92: the server queues types `0x64..0xc7`
unexamined, no sender in the turn entry). Type `0x6b` (two figure ids, a goods byte) is a different transfer and is **U**.

## 5. The unload arm of `0x100ac360` (D, instruction by instruction)

Frame: `this` = world, `[esp+0x184]` rec, `[esp+0x188]` ship, `[esp+0x18c]` flag, `[esp+0x190]` city. Registers below: `esi` = request,
`edi` = hold, `ebp` = city. Order of tests and effects:

1. `rec[6] == 1`: the cannon arm (section 7). Else:
2. `n0 = ship slot +0x78 (type) -> slot +0x88(0)`; **return 0 if `rec[1] >= n0`** (`0x100ac470`-`0x100ac47a`).
3. flag != 0 (`0x100ac492`) -> arm at `0x100ac8ea`: `edi = ship slot +0x84(rec[1])`; **return 0 unless `hold.goods` (hold slot `+4`,
   `0x10037560`: `movzx byte [hold+4]`) `== rec[0]`** (`0x100ac90a`).
4. `esi = rec.word[2] << 8` (a u16 of tons, so at most 65535 t); `esi = min(esi, hold.amount)` (hold slot `+8`, `movzx word [hold+6]`,
   **signed** `jle` compare, `0x100ac936`).
5. `edi = (0xff - stock(g) + capacity) & 0xffffff00` where `stock(g)` = city slot `+0x7c` = `0x1002da30` (**no bound check on g**) and
   `capacity` = city slot `+0x78` = `0x1002f7f0` = `[city + 0x624]` (one value for every goods); `esi = min(esi, edi)` with a **signed**
   compare and **no clamp at 0** (`0x100ac94d`-`0x100ac96b`). This is the free space **rounded up to a whole ton** when stock <= capacity.
6. `esi == 0` -> return 0 (`0x100ac96d`). A **negative** `esi` continues (section 6, defect B).
7. Owner test: `ship slot +0x1c` (`0x10043430` = `[ship + 0x144]`, the owner player) against `city slot +0x1c` (`0x1008c680` =
   `[city + 0x20]`, the owner). **Equal -> straight to `0x100accbf`**: hold slot `+0x10` (`esi`), city slot `+0x80` (`g`, `esi`), return
   `esi`. Nothing else happens on this path: no gold, no event, no message (the `0xd` notification records at `0x100acc06`-`0x100accbc`
   belong to the foreign arm only).
8. Not equal: relation `player slot +0x7c(0, other) == 3` required, then the price/tribute loop and the notifications: the **foreign
   (allied) trade arm**, excluded from MAP-CARGO-1 and not read further.

Hold take, `0x1003b9a0` (hold vtable `0x100c4f28` slot `+0x10`): `if take >= amount (signed 32-bit compare, take = arg)` then `amount = 0`
**and goods id = 0**, else `amount -= take` (stored as a word); then the hold's owner (`[hold + 8]`, the ship) is told through its slot
`+0xa0` (section 6.3). Slot `+0xc` (add) is `amount += arg` as a **wrapping word add**; slot `+0x14` takes everything and returns it.

City stock record (`production.md` line 325): 12 bytes at `city + 0x334 + 12*g`, `g < 0x38`: `u32` stock (8.8 tons), `u16` `+4` consumed,
`u16` `+6` added, `u16` `+8` removed. Add `0x1002da50` (`ret 8`, args `(g, amount)`): `g >= 0x38` -> returns 0, nothing written; else
`sum = stock + amount` (32-bit wrap); **if `capacity < sum` (unsigned) then `amount = capacity - stock` (32-bit wrap)**; `added += amount`
(word); `stock += amount`; returns `amount`. Remove `0x1002da90`: `amount = min(amount, stock)`, `removed += amount`, `stock -= amount`;
**no bound check on g**. The caller of the unload arm ignores add's return value.

Capacity: `0x10028400` (`annowelt.md` line 188) sets `[city + 0x624] = table[min(count, 124)] << 8` with the u16 table at `0x100dcc84`
(P: `0, 50, 70, 90, 110, 130, 140, 150, 160, 170 ...`) over the market-class buildings, called after every build (`0x10027c5d`), and
**when it shrinks, every stock is lowered to the new capacity** (`0x100284ac`-`0x100284c3`). One Kontor gives 50 t = 12800 units.

## 6. What this arithmetic does (D; hand-executed, each case is a vector in `cargo_vectors.txt`)

With `T = 256` and stock <= capacity (every state the original itself produces):

* transfer `x = min(request, hold amount, ceil_T(capacity - stock))`, credit `min(x, capacity - stock)`, debit `x`.
* **A. Rounding loss.** When `capacity - stock` is not a multiple of 256 and the hold has more than that, the debit exceeds the credit
  by up to 255 units: the excess is **destroyed** (vector `fractional_free_space...`: debit 256, credit 128; `one_unit_free`: debit 256,
  credit 1). The stock afterwards equals the rebuild's `stock + min(request, hold, free)`; only the hold differs.
* **B. Over-capacity stock (stock - capacity >= 256).** `edi` is negative (e.g. -256), so `esi` becomes negative, the hold's "take" is
  negative (**the hold gains 256**) and the stock is lowered to the capacity. The original's capacity recompute (section 5) keeps stock
  <= capacity, so this is reachable only from a loaded or constructed state (vector `over_capacity_by_300_original_defect`).
* **C. Goods id >= 0x38.** The hold is debited, the add is refused: **cargo destroyed**; the stock read beforehand is out of bounds (U
  contents). The rebuild's goods count is data.
* **D. u32 wrap.** `stock + request` wraps for a capacity near 2^32 (vector `u32_boundary...`: stock 155 afterwards). Unreachable in the
  original (capacity <= 900 t).

## 7. Arms that are NOT the manual warehouse unload

* **Cannon arm** (`rec[6] == 1`, `0x100ac37b`-`0x100ac452`): `ship slot +0x8c(5)` (a per-ship counter; **S**: "%d Cannons on board" in
  `[TOOLTIP:SCHIFFMAIN]`) is decremented through slot `+0x88(5, n - 1)` and **one ton** of goods `rec[0]` (`0x100`) is added to the city
  with **no ownership test**. Only the multiplayer packet bit can select it (the view builds `rec[6] = 0`). Excluded; the rebuild must
  not accept it (contract: no `rec[6]`).
* **Load arm** (flag 0, `0x100ac498`-`0x100ac8e1`): city stock is read rounded **down** to whole tons; not this package.
* **Foreign arm** (owners differ): gold, relation 3, notifications; excluded (shared visibility is not trading permission).
* **Throw cargo overboard** (`CTRL+W`): `View.dll 0x10062840` builds a record whose goods byte is `0x8e` ("all") and calls client slot `+0x5c`
  = `0x100591d0` -> world `+0xfc` = `0x100accf0`; the arm moves holds into a flotsam object (kind `0x654`). Not a transfer to a city.
* **Automatic trade-route stop** (`0x10055bb0`, node bytes `+0xc` goods, `+0xe` load/unload flag): calls the same world `+0x100`
  (`0x10055d8a`, `0x10055ee3`) with the city owning the stop tile (`island slot +0xec`/`+0xf8` at `0x10055bd3`-`0x10055c0c`) and **no range
  test** (the ship has arrived). It shows that `0x100ac360` is the shared primitive, and is excluded from the manual rule.
* **Carts and resident walkers** unload into a *building* (`0x10057bd0`, `0x10057d70`, `population.md` lines 445-524, `transport.md`) and never
  touch a ship hold.
* **Founding payment** `0x10041760` takes goods `0x29..0x2d` from the holds in index order (`ship-founding.md` 10.7); it shares the hold
  take but not the city stock.

## 8. What the original does not check (D unless marked)

| Question | Original |
|---|---|
| Sender equals ship owner | **No** (single player: the view only shows own ships; multiplayer: nothing) |
| Ship state: moving, turning, hit points, state word bits `0x02`/`0x0a`/`0x200e`, carried (has a parent) | **No test in `0x100ac360` or `0x1003f200`**; whether the view disables the tab for a moving ship is **U** |
| Range at execution | **No** (only the view, at click time, through `0x1003f200`) |
| Target is a warehouse | **No** in the world function; the view obtains it from `0x1003f200` (PRODTYPE `0x23`) |
| Same island | not tested; the city pointer decides |
| Duplicate holds | no search: the **named hold** is used; the goods byte must match it |
| Goods in the city's table | **No** for the debit (defect C) |
| Fractional request | impossible: the request is a u16 of tons; fractions arise only in hold amount and free space |
| Gold, taxes, a message, a timed event | none on the same-owner path |
| Statistics words `+6`/`+8` of the stock record | add/remove increment them (`0x1002da81`, `0x1002daaa`); consumer **U** (the balance sheet is the likely reader) |

## 9. The cargo-dependent speed hook (D)

The hold's take/add call the owning ship's slot `+0xa0` (`0x1003b9a0` end, `0x1003b950` end): `0x100427b0` recomputes
`load = 100 - 17*sum(amount of the first n0 holds with a goods id) / ((n0 + n1) * cap)` (when the owner's player value 3 is 1) and stores
`[ship + 0x3c] = percent * 0.01f` **immediately** (`0x100428b8`-`0x100428be`). Movement reads the stored value only when a segment starts
(`ship-navigation.md` 7.5-7.8). The rebuild derives it at segment start from the current holds (`docs/sim/ship-navigation.md` 12.8, 12.14);
the observable effect is the same. A transfer must therefore only change the holds.

## 10. Evidence matrix

| Item | Status | Where |
|---|---|---|
| Ship view unload -> client slot `+0x60` -> world `+0x100`, flag 1 | D | 2 |
| Record bytes 0-3, 6; wire layout of type `0x6a` | D | 2.1, 4 |
| Hold-by-index addressing, goods echo, request in whole tons | D | 5 |
| Free space rounded up to a ton, signed negative case | D | 5, 6 |
| Hold clear on empty; hold gains on negative take | D | 5 |
| City stock add/remove/clamp, goods bound, capacity source | D | 5 |
| Range = digital disc of radius 5 on the ship's truncated tile, KONTOR only, city list from the outline | D | 3 |
| Table `5 5 5 4 3 2` | E (recurrence executed) | 3 |
| First qualifying warehouse decides the city | D | 3 |
| No state, sender, range test at execution | D (absence in the two functions read) | 8 |
| Which UI property is hold index / goods | S | 2 |
| View enabling rules of the tab (moving ship, city missing) | **U** | OQ-CARGO-1 |
| Consumer of stock words `+6`/`+8`, notification records of the foreign arm | **U** | OQ-CARGO-2 |
| `0x6b`, foreign arm, cannon arm semantics | **U** (excluded) | OQ-CARGO-3 |
| Runtime capture of one manual unload | NOT_RUN | not required by the contract |
