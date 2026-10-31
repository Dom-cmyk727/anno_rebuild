# Ship founding: the first warehouse

**Status:** partial. File evidence for Kontor 1302 and the shipped help is
checked against this build. The menu path and the cursor classes are a
static trace through `View.dll` and `AnnoFrame.dll` on the GOG modules in
`game/app`. The warehouse placement callee was not opened. Nothing here was
run in the original game (`NOT_RUN_ORIGINAL`). Addresses are virtual
addresses with image base `0x10000000`.

`AnnoFrame.dll` `0x10046300` is the mine cursor `tCursorAnlageBergwerk`.
An earlier revision of this note called it the warehouse click. That
attribution is withdrawn. The warehouse handler is `0x10048e60`, and its
placement callee `0x10049340` is unread.

This note separates three layers.

| Layer | What it is |
|---|---|
| Original behaviour | What the files and this trace show. Unread callees stay unread. |
| Current rebuild | `developer_start` places a free Kontor and a whole-island owner run. It does not create a ship, and turning the flag off does not either (`src/sim_load/src/fixed_map.cpp`). |
| Multiplayer adaptation | The invariant the rebuild needs. It is not an original guarantee. |

The geometric sea check in `map2-ship-start-v2` is none of these. It is a
tool model (`docs/re/ship-navigation.md`).

## 1. Help rechecked

English help under `game/app/help/`, this build:

| File | What it says |
|---|---|
| `IH_Kontorfehlt_wie.htm` | Sail close to a coast. When building along the coast is possible, "Build warehouse" appears. Place it on the coast near the ship. The service area, shown in info mode, marks the city limits. A dirt road is built afterwards, from the warehouse toward those limits. |
| `Scout.htm` | The scout on the ship discovers deposits and can construct a main market building inland. |
| `Gesamteinzugsbereich.htm` | City limits are the combined service area of warehouses and main market buildings. Further ones expand it. |
| `H_Landweg.htm` | Land trade between players requires both to have settled on the same island. |
| `Kontor_uebernehmen.htm` | Taking over a warehouse gives the buildings in its service area, after the enemy warehouse is nearly destroyed and a new one is built on it. Not the whole island. |
| `IH_Insel_Falsch.htm` | Some climate has no city. It points the player at northern temperate zones or jungle. |
| `IH_Insel_Falsch_Polar.htm` | Settlements are not possible. A few workshops (trappers, whalers) can be placed. First settlement: temperate or jungle. |
| `IH_Insel_Falsch_Praerie.htm`, `_Steppe.htm`, `_Tundra.htm` | Limited agriculture. Settle there after a settlement already exists elsewhere. Each names local goods. First settlement: temperate or jungle. |

There is no `IH_Insel_Falsch_Wueste.htm`, and no page of this name for
`NORD` or `DSCHUNGEL`. A recommendation of temperate or jungle is not a
proof that every other climate rejects the warehouse click. Which climate
byte selects which page was not traced.

`help/Kontor.htm` describes warehouses as the founding and trade hub, with
later upgrades. It does not give the placement test.

## 2. Kontor 1302, from `Anlagen.dat`

Entry id 1302, `PRODTYPE` `KONTOR`. `HEAD` is 56 bytes. The simulation's
known layout (`docs/formats/anlagen.md`, `docs/re/production.md` §2.4):

| Field | Value |
|---|---|
| Footprint bytes 0–1 | 4 × 4 |
| Build goods bytes 3–6 | bricks 0, wood 12, tools 5, marble 0 |
| Gold, u16 at byte 8 | 350 (`0x015E`) |
| Action radius byte `0x11` | 24 |
| Type flag word, u16 at `0x1c` | `0x2087`. Bit `0x2000` is the coast flag (`production.md`, AnnoGame `0x100093bd`). Bit `0x1000` (mountain) and bit `0x4000` (road) are clear. The remaining bits of `0x2087` are not given a founding meaning here. |
| Hit points, u16 at `0x2c` | 6000 |
| `ENTRANCE` | 16 bytes, two f32 pairs |

The entrance floats are the f32 values `2.5`, `13/6`, `11/6`, `0.5`
(bytes `00002040 abaa0a40 abaaea3f 0000003f`). Existing transport notes:
entrance points lie on sub-cell centres `k/6`, and the rotated points are
what a carrier walks to (AnnoGame `0x100084c0`). Floor of each float is tile
`(2, 2)` and tile `(1, 0)` inside the unrotated 4×4. That is the type's
door data. It is not yet the rotation a founding click selects (OQ-520).

The same file, for context, not as the ship action:

| Id | `PRODTYPE` | Footprint | Goods bytes 3–6 | Gold | Flags |
|---|---|---|---|---|---|
| 1301 | `MARKT` | 4 × 3 | 0, 7, 3, 0 | 250 | `0x0084` (coast bit clear) |
| 1302 | `KONTOR` | 4 × 4 | 0, 12, 5, 0 | 350 | `0x2087` |
| 1303 | `MARKT` | 4 × 3 | 0, 12, 5, 0 | 500 | `0x0084` |
| 1304 | `KONTOR` | 4 × 4 | 0, 12, 5, 0 | 500 | `0x2087` |

Costs and the coast bit are type data. They are not the whole founding rule.

Goods numbers charged by Place, from the existing registry note
(`docs/sim/state.md`, AnnoWelt `0x10027a77` / AnnoGame `0x1000a5a0`):
`0x29` tools = `HEAD[5]`, `0x2a` wood = `HEAD[4]`, `0x2b` bricks = `HEAD[3]`,
`0x2c` marble = `HEAD[6]`, each stored `<< 8` (8.8 tons). `0x2d` is inside
the five-wide goods loop of the mine cursor (§3.4), which is not the
warehouse handler. 1302's `HEAD` does not put a fifth material in bytes
3–6. Gold is the separate u16, not one of those bytes.

The action radius 24 is the market-search radius
(`docs/re/transport.md` §1.1): a square around the footprint centre. The
help text says the warehouse service area marks city limits. This trace
does not show the city-claim walk, so radius 24 is not the answer to
OQ-490.

## 3. The ship menu and the two cursors

The ship menu and `tCursorAnlageBergwerk` were previously written as one
warehouse click. They are different classes. Each connection below names
the call or the vtable slot that shows it. A missing connection stays
missing.

### 3.1 From the ship menu to `tCursorAnlageKontor`

`Texte.dat` section `TOOLTIP:SCHIFFMAIN` contains the line "Build warehouse".
The name `MAKEKONTORICON` (`View.dll` `0x100a9170`) is copied into a menu
slot, and slot index 3 registers the callback `View.dll` `0x10063ba0`
(`0x10063966`).

`0x10063ba0` does four calls on the ship view:

1. `[esi+0x62c]` vtable `+0x78` with `[esi+0x648]`.
2. `[esi+0x638]` vtable `+0x40`, result pushed onward.
3. `[esi+0x648]` vtable `+0x60` with `(8, that result)`, then `+0x60` with `(0, 0x516)`.
4. The menu object's vtable `+0x9c` with `(0xC, 1, 0)`.

`0x516` is 1302. The parallel scout/pioneer callback `View.dll` `0x10047ae0`
pushes `0x515` (1301, `MARKT`) instead. That is a different action. The
help's "main market building" is not identified with 1301 here.

The pointer at the ship view's `+0x648` is produced earlier, while the menu
is built (`View.dll` `0x1006330b`):

```
mov ecx, [ebx + 0x62c]
push 0x13
call [vtable + 0x74]
mov [ebx + 0x648], eax
```

A null return fails that setup with `0x80004005` (`0x10063320`).

**Unknown.** The class of the object at ship-view `+0x62c` was not
identified. Nothing in this trace stores a `tFrameGame` or a
`tFrameGameFull` into that field. The virtual call is the right shape for
the slot below and is not proof that the field holds that object.

If the object is one of those two classes, the rest of the chain is direct:

| Step | Evidence | Object |
|---|---|---|
| Slot `+0x74` of `tFrameGame` (vtable `0x10086428`, locator `0x1008ed78`) and of `tFrameGameFull` (vtable `0x1008664c`, locator `0x1008ee68`) | Both slots are `AnnoFrame.dll` `0x10007e50` | The frame object, class as named |
| `0x10007e50` | The argument is the cursor kind. The cached pointer is `this + kind*4 + 0x1834`. A miss calls factory `0x10066330` and stores the result (`0x10007ed4`) | Same frame object |
| Factory kind `0x13` | `cmp edi, 0x13` at `0x10066647`, then `call 0x10049400` at `0x10066666` | Constructs `tCursorAnlageKontor` |
| Factory kind `0x0e` | `call 0x10046bf0` at `0x1006640a` | Constructs the mine cursor in §3.2. Not this menu |
| Published pointer | `lea eax, [edx+esi+4]` at `0x100666f5`, written through the out-pointer. The constructor stores vtable `0x100884bc` at that address (`0x100494bc`). The cache returns it (`0x10007ee5`) | The Kontor subobject, not the complete object |

`0x100884bc` is the vtable at complete-object offset `0x20c`. Type
descriptor `0x10096928`, name `.?AVtCursorAnlageKontor@@`. Locator
`0x1008fcd8` points at that descriptor and records offset `0x20c`.

On vtable `0x100884bc`:

- Slot `+0x60` is `0x100495a0`, which jumps to `0x10049140`. Property index
  0 stores the u16 argument at `this-0x1c8` (`0x10049150`). That address is
  complete-object `+0x44`. The constructor first writes `0x515` there
  (`0x1004949b`). A later property-0 write replaces it. The menu's
  `+0x60(0, 0x516)` is that write when this vtable is the one at
  view `+0x648`.
- Slot `+0x70` is `0x100495e0`, which jumps to handler `0x10048e60`.

The handler refuses a zero id word (`cmp word [esi-0x1c8]` at `0x10048e89`).
On the click path it calls `0x10049340` (`0x10048ed4`) with the placement
coordinates. Its call list does not include `0x10046300`.

**Unknown.** `0x10049340` was not opened. Coast, tile class, distance,
rotation and payment for the warehouse are not established. The mine
arithmetic in §3.3 is not a substitute.

### 3.2 `0x10046300` is `tCursorAnlageBergwerk`

Checked on this build, image base `0x10000000`:

| Fact | Address |
|---|---|
| Type descriptor `.?AVtCursorAnlageBergwerk@@` | `0x100968b0` |
| Locator: signature 0, offset `0x78`, type descriptor `0x100968b0` | `0x1008fa58` |
| Vtable. The dword at `0x10088160` is the locator | `0x10088164` |
| Slot `+0x70` | `0x10046dc0`: `sub ecx, [ecx-4]; jmp 0x10046830` |
| That handler calls `0x10046300` | `0x100468df` |
| Constructor, reached for factory kind `0x0e` | `0x10046bf0` |

This is static mine-cursor behaviour. It is not the warehouse click.

### 3.3 What the mine handler does

`docs/re/production.md` already names two slots this function uses. Type
slot `+0xac(i)` is output goods. Island slot `+0x114` (`0x1008bec0`) reads
island `+0x24`, the deposit field, and the deposit finder then calls slot
`+0x14` (`0x100787c0`). This function calls `+0x114` and then slot `+0xc`.
That is not slot `+0x14`. The 6-byte records are not identified as deposits,
and they are not ships.

`0x10046300` (`ret 0xc`, frame `sub esp, 0xc5c`) returns 0 when any row
below fails. The click handler `0x10046830` calls it and, on 0, does not
reach its own commit call. That commit is the mine handler's, not
`0x10048e60`.

| Condition | Address | What is shown |
|---|---|---|
| Island under the tile | `0x1004634d` | Slot `+0x8c(x, y)` on the object at `[this+8]`. Null fails (`0x1004635b`). |
| Cached island | `0x1004636e` | If the cache pointer's island is non-null and differs, fail. |
| Type | `0x10046383` | Slot `+0x70(building id)`. |
| Rotation | `0x1004638d` | Byte at the tool `+0x46`. Values above 3 skip the swap. The jump table at `0x10046770` swaps type slots `+0x50` and `+0x54`. |
| Selected record | `0x100464c6`–`0x10046645` | No record whose mask matches fails. |
| Uniform class, one non-`0x0f` | `0x100466a1` | Below. |
| Class object | `0x100466dd` | Island slot `+0xf8(class)` must be non-null. |
| Identity | `0x100466ed` | That object's slot `+0x1c` must equal `[this+0x5c]`. |
| Final accept | `0x1004673b` | The class object's slot `+0xa0` returns the function result. |

**Stack.** `0x10046314` copies entry `ecx` to `ebp`, so `ebp` is `this`.
`0x1004633f` and `0x10046340` push the two arguments of the `+0x8c` call.
`mov [esp+0x68], ebp` at `0x10046341` writes `this` eight bytes deeper than
the depth used once those arguments are gone, which is the local later read
as `[esp+0x60]`. The call at `0x1004634d` has no following `add esp`. The
next store, `mov [esp+0x68], ebx` at `0x10046357`, is the island pointer
read at `0x1004667b`. Those two `[esp+0x68]` stores are different locals
because of the two pushes. `mov ecx, [esp+0x60]` at `0x100466f0` is the
first store. The pointer is `this`. It is not uninitialized, and
`[this+0x5c]` was not given a name. It is not a player id.

**Selected record.** Each record is 6 bytes: word `+0`, word `+2`, byte
`+4` (a rotation, compared with 3 at `0x100464dc`), byte `+5`. A closer
match is saved at `0x1004660c`–`0x1004661e`: the position words and the size
bytes from type slots `+0x50`/`+0x54` after that rotation. The class walk
at `0x10046673` uses that saved rectangle. On success, `0x10046715` writes
word `+0` to the placement at `+2`, word `+2` to the placement at `+4`, and
byte `+4` to the placement at `+6`. The checked footprint and the
coordinates written back come from the selected record.

**Class.** `ebp` starts at `0x0f` (`0x10046664`). Island slot `+0xec(x, y)`:
a return of `0x0f` is skipped (`je 0x100466bd` at `0x100466aa`); the first
other value is kept (`0x100466b1`); a later other value must equal it
(`0x100466b5`); if every return was `0x0f`, the function fails
(`0x100466d3`). These three results follow from those branches and were not
run:

- Returns `0x0f`, `0x0f`, `3`, `0x0f`, `3`. The `0x0f` values are skipped,
  `3` is kept, and the later `3` agrees.
- Returns only `0x0f`. Fails at `0x100466d3`.
- Returns `2`, then `3`. Fails at `0x100466b7`.

`0x0f` is not identified with `HEIGHT` 15 or with water. The name of the
value `+0xec` returns is unread.

**Mask.** The value from type slot `+0xac(0)` is in `ecx` at
`0x100465fd`–`0x10046608`:

```
dec ecx
mov edx, 1
shl edx, cl
mov cl, [ebx+5]
test cl, dl
```

The output is decremented before the shift. `production.md` treats
`+0xac(0) == 1` as goods `EISENERZ`. After `dec` the shift count is 0 and
the mask is `1 << 0 = 1`, so record byte `+5` bit 0 is tested. A return of
4 becomes shift count 3 and mask `1 << 3 = 8`, so bit 3 is tested. Without
the decrement, a return of 1 would test bit 1 (mask 2). A failed test
skips the record (`je 0x1004662c` at `0x1004660a`).

The distance score that picks the closest record stays in this function.
It is a mine-cursor score. It is not a ship range.

### 3.4 Affordability on the mine click

After `0x10046300` returns non-zero, still inside `0x10046830`:

1. Type lookup, slot `+0x70(id)` (`0x100468fa`).
2. `AnnoFrame.dll` `0x10043d80` (`0x10046909`).
3. A flag is built from that return and the commit is `[edx+0x78]` (`0x10046928`).
4. `0x10043d30` (`0x1004692e`) adds the type's slot `+0xcc` into a five-dword buffer.

`0x10043d80` loops goods `0x29` through `0x2d`. For each, it adds the unit
argument's slot `+0xcc(0, goods)` to a dword in the cost buffer and requires
another object's slot `+0x7c(goods)` to be at least that sum. It then does
the same for gold: slot `+0xcc(1, 0)` added to the buffer's `+0x14`, compared
with slot `+0x1c` then `+0x60`. If the unit argument is null, the function
returns 1 without those compares (`0x10043d8f`).

The caller passes the building type and the pointer at `[esi−0x58]`
(`0x100468ff`). That field is on the mine cursor. It was not identified.
The Kontor handler's calls do not include `0x10043d80`, so this field is
not who pays for a warehouse.

The mine commit is not skipped when `0x10043d80` returns 0. The return is
folded into the flag (`0x10046913`):

| `0x10043d80` | Flag pushed to the mine handler's slot `+0x78` |
|---|---|
| 1 | `0x80000000` |
| 0 | `0xA0000000` |

Both values include `0x80000000`. The failure value also includes
`0x20000000`. The callee of that slot was not opened. This is not an
atomic rollback, and it is not the warehouse handler.

`0x10043d30` runs after that call and only adds the type's reported costs
into the buffer. It is not the subtraction. The Kontor handler also calls
`0x10043d30` (`0x10048fca`). Sharing that cost reader does not make the
two handlers the same predicate.

### 3.5 What is still unread for a warehouse

| Question | Status |
|---|---|
| Class of ship-view `+0x62c` | Unknown. The call is `+0x74(0x13)`. `tFrameGame` implements that slot. The store into the field was not found. |
| `0x10049340` | Unknown. This is the Kontor handler's placement callee. |
| Required ship distance | Not in the Kontor handler's opened calls. The mine window is not that distance. |
| Coastal terrain | Coast flag `0x2000` is on type 1302. The tile test that uses it was not opened. |
| Who pays | Unread on `0x10048e60`. The mine field `0x100468ff` is a different object. |
| Rotation a warehouse click writes | Unread. The mine tool's byte `+0x46` is not the warehouse door (OQ-520). |
| Scenery removal | Not in the opened Kontor calls. OQ-063. |
| City and service area | The Kontor commit callee was not opened. Help says the service area is visible in info mode after the warehouse exists. |
| Simultaneous founding order | Not in this trace. |

An AI probe in `CompLogic.dll` (`0x100154bf`) loads id `0x516` and calls
slots `+0x70` and `+0x6c` with flag `0x2000000`. It counts a returned byte
`0x30`. That is a computer-player scan. It is not the player menu, and it
does not identify `0x10049340`.

## 4. Current rebuild

`fixed_map.cpp` with `developer_start` true checks that each start island
is unique, then creates a Kontor and an owner run over the whole island
(OQ-490). `developer_start` false omits that grant. Neither path reads a
ship manifest, moves a ship, or runs `0x10048e60`. The scenario adapter
records player subrecords it does not model with `unknown_record`
(`scenario_formats.cpp`); it does not spawn the endless-start ship.

`map2-ship-start-v2` checks a 4×4 of `HEIGHT >= 16`, clear of ANLAGEN `HEAD`
rectangles, beside exterior-4 water, with a 4-connected route of at most
400 tiles and a site component of at least 256. Those are tool predicates.
They are not `0x10049340`, and they are not the mine cursor.

## 5. Rebuild invariant (adaptation, not original)

A founding command is one simulation step.

- It rechecks every predicate it is specified to enforce at execution time.
  Menu state from an earlier tick is not authority.
- It either commits city, building, payment and territorial write together,
  or it leaves the world unchanged. This atomicity is required for lockstep.
  Section 3 does not prove the original does this. The flag fold in §3.4
  belongs to the mine handler.
- Two commands in one tick that claim overlapping footprint tiles are ordered
  by seat, then by command index. The later one receives the failure result.
  The original's simultaneous order was not found.
- Repeating a command after success is a new command. It is not ignored as
  a duplicate unless a later spec defines an id. No original idempotency
  bit was found.
- The payer is an explicit account in the command (ship holds and player
  gold named separately). The original warehouse split is the unread
  Kontor callee in §3.5, not the mine field `0x100468ff`.

## 6. Fixture specifications

These are specifications for a later implementation and for an original
capture. They are not results of an original run.

**Positive, original capture.** Ship carrying at least 12 t wood, 5 t tools
and 350 gold. Use the control whose callback is `View.dll` `0x10063ba0`.
Expected if the unread Kontor callee accepts: a building 1302, a city for
that player, gold and goods reduced by the type costs. Rotation, scenery,
distance and territory radius are recorded, not predicted. The mine
cursor's class rule is not this expectation.

**Mine cursor, static only.** These rows are `0x10046300` returning 0.
They are not warehouse outcomes.

| Case | Expected at the mine boundary |
|---|---|
| No island at the tile | Return 0, the mine handler does not reach its `+0x78` |
| Footprint tiles not one shared class, or all `0x0f` | Return 0, same |
| No mask-matching record | Return 0, same |

**Mine affordability, static only.** `0x10043d80` returns 0. The mine
handler still calls its commit, with flag `0xA0000000`. Expected world
effect: unknown. This flag was not found on `0x10048e60`. The rebuild
invariant is stronger, and it applies to the warehouse command: no city
and no payment.

**Not specified.** Occupied footprint, rival territory, and how close the
ship must be. Do not treat a geometric miss, or a mine-window miss, as an
original warehouse failure.

## 7. Original-runtime capture

`NOT_RUN_ORIGINAL`. This host is the Mac map lane. The existing runner
(`docs/testing/original-wine.md`) is `wsl -d AnnoHarness` on the Windows
lane. No emulator was installed.

When that runner is available, one session is enough:

1. Load `Scenes/0010SingleEndlos/03_1024_med_256.sze`. Leave the human ship
   at its stored coordinates with its stored cargo.
2. Open the ship menu and use the control whose callback is `View.dll`
   `0x10063ba0` (building 1302). Click a coast tile next to the ship, then
   a water tile, then a tile far from the ship. The distance that matters
   is whatever `0x10049340` implements. Do not use the mine cursor's window
   as that distance.
3. Repeat with the hold totals reduced below 5 t tools, and with wood still
   12 t. Record the result. The mine goods check is not assumed to be the
   warehouse goods check.
4. For each click, record player gold, every hold, new objects (id, tile,
   rotation), city list length, footprint tile owners, and whether
   `GELAENDE` items under the footprint remain.
5. Store the log under the harness run directory. Do not copy it into git.

Until that log exists, every runtime expectation in §6 stays
`NOT_RUN_ORIGINAL`.
