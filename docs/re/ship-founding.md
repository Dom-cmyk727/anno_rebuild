# Ship founding: the first warehouse

**Status:** fifth pass added (§10–§12, 2026-10-31); §8 is the second pass, corrected in place and by §10. The status of each rule is in §10.9. The first pass (§1–§7) is kept; §8 supersedes its "unread" rows and **corrects** two of its statements (see "Corrections" at the top of §8). File evidence for Kontor 1302 and the shipped help is
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

## 8. Second pass: the placement validator, the ship, the commit (2026-10-31)

Static only (`NOT_RUN_ORIGINAL`). Modules (PE32 x86, image base `0x10000000`; every address below is a **virtual address**, RVA = VA − `0x10000000`; file offsets were not needed). SHA-256: `AnnoFrame.dll` `dd3f36ff564d2685d10795272c4ed3c0b2da32a946c2883fb1158ff439249b91`, `AnnoWelt.dll` `f220a878510f740c4cfb7224ade0a260e9a0fd7e99113e1201e7a37c0dfa6ae2`, `AnnoGame.dll` `ac721220afd27ebca5fd2ece6a8ff452289b9d7458c4e091c68adea2012db7bc`, `View.dll` `e32fa8a5ff0f3f0b00ef05618daf8309c692daaa57316147eb6cb1f3637733a1`. Repository HEAD `62ddaf1`. Tools: `tools/disasm.py` (capstone 5.0.7, pefile 2024.8.26, private venv), plus three read-only helpers kept with the evidence (`sweep.py`, `ctx.py`, `rtti.py`). Disassembly, ledger and provenance are in `cache/map2-founding-evidence/` (git-ignored: `00-provenance.md`, `02-call-graph-ledger.md`, `*.asm`). Labels: **D** read from disassembly; **S** by shape (not proven); **U** unread.

### Corrections of earlier statements
1. The checkpoint (and a reading of §3.1) that `0x10049340` "then calls `0x10048ae0`" is **withdrawn**: the dump ran past the function's `ret 0xc` (`0x100493e7`) into the constructor `0x10049400`.
2. `0x10049340` is **not a validity test**; it returns the rotation byte and writes the centred rectangle (§8.2). §3.5 row "`0x10049340` unknown" is closed.
3. §3.5 "Who pays: unread" and "Required ship distance: unread" are answered in §8.4 and §8.3 (D).
4. Still true: `0x10046300` is the mine cursor and none of its rules are used here.

### 8.1 The path, in order (D unless marked)
`View.dll` `0x10063ba0` (building 1302, §3.1) -> cursor properties: id 1302 (property 0 at `0x10049150`, writes `this-0x1c8`), ship reference (property 8 at `0x10049187` writes complete `+0x1fc`; getter `0x100491f2`) -> every mouse event reaches handler `0x10048e60` -> `0x10049340` (centre) -> validator `0x10048b30` -> ship predicate (slot `+0xbc` = `AnnoWelt.dll` `0x100414d0`) -> on the commit event `0x100441f0` hands the record to the command sink (`[cursor+0x58]` slot `+0x54`, S: `tNetClient`, vtable `0x1008914c`, apply function `0x100589a0`, already documented in `docs/sim/state.md` §8) -> **kind 2** calls the ship's slot `+0xc0` = `AnnoWelt.dll` `0x10041760` (the founding commit). Kind 8 (new city for a player, `0x100589a0` `0x10058ca5`) and kind 7 are the city-based placements (§8.6).

### 8.2 `0x10049340` (AnnoFrame, `ret 0xc`)
Arguments: click `x`, click `y` (cursor `+0x40`/`+0x42`, §8.3), out pointer. `type` = host (`[this+0xc]`) slot `+0x70(u16 this+0x44)`. Footprint `w,h` = type slot `+0x50`, `+0x54`; rotation byte `this+0x46`: 0 and 2 keep `(w,h)`, 1 and 3 swap them (jump table `0x100493ec`); rotation above 3 or an unknown type leaves `w = h = 1`. Then, in this order: `x' = x - ((w-1)` with sign correction `)/2`, stored, and clamped to 0 if negative (`jns`); `y'` likewise with `h`. Out: `{x', y', x'+w, y'+h}` (four dwords, handler `this-0x1c4`). Returns `AL` = the rotation byte. For `w = 4` the origin is `x - 1`; for `w = 3` it is `x - 1`; `w = 1` it is `x`. The clamp is applied after the subtraction and independently per axis, so a click near the island's west or north tile edge shifts the footprint's **end** with it (`x'+w` is computed from the clamped `x'`).

### 8.3 Coordinates and the distance test (D)
- Cursor `+0x40`, `+0x42` are **u16 tile coordinates** produced by `0x10041290` from the mouse: the packed result is split as `x = (v >> 14) & 0x3fff`, `y = v & 0x3fff` (`0x10041332`–`0x10041351`). They are not floats and not island-local: world tile coordinates, the same space as the placement record and as `island` slot `+0xec(x, y)` (S: the island index subtracts the island origin itself).
- Ship position: ship class `0x10043600` (vtable `0x100c4da4`, `docs/re/fire-and-ships.md` §3): slot `+0x54` = `0x10040140` = `fld [this+0x14]` (**x**, f32 world), slot `+0x5c` = `0x10040160` = `fld [this+0x1c]` (**z**, f32), slot `+0x58` = height. The same coordinates as the scenario's unit prefix (tile units; S: the unit of the f32 is the tile).
- The reference to the ship is a **figure instance id**, not a type id: property 8 stores it, the validator resolves it through `world->slot+0xa0()->slot+8(id)` (`0x10048d2f`–`0x10048d45`; null -> reject). That the id is the ship of the open menu is S (the callback pushes the result of `[esi+0x638]` vtable `+0x40`, class unread).
- Test (`0x10048d62`–`0x10048dde`), after the figure's slot `+0x8c(0)` has `bit 1` clear (`test al, 2` -> reject; meaning **U**): `dx = trunc_toward_zero((f32)click_x - ship_x)` (x87 `fild` -> `fstp` dword -> `fsubr`, then `__ftol` `0x10077da8`, which forces round-toward-zero), `dy` likewise with `click_y` and ship z; `d2 = dx*dx + dy*dy` (32-bit `imul`/`add`); `d = trunc(fsqrt((double)d2))` via `fild`/`fsqrt`/`__ftol`; **reject if `d > 12`** (`cmp eax, 0xc; jg`). The click, **not** the centred origin, is used. Because `dx`, `dy` are integers, `d <= 12` iff `dx*dx + dy*dy <= 168`; the two truncations make the accepted region **not** a circle of radius 12 in the float position: with the ship at `x = 10.9`, a click at `x = 23` gives `dx = trunc(12.1) = 12`, accepted when `dy = 0`; at `x = 10.0` a click at `x = 23` gives `13`, rejected. The rebuild needs the ship's float position (or the exact integer truncation of `click - position`) to reproduce this; Q24.8 positions are enough for the subtraction but the truncation toward zero of a signed difference must be implemented, not a floor. Selecting the representation is **PROPOSAL** in `docs/sim/ship-founding.md`.

### 8.4 The ship predicate: slot `+0xbc` = `AnnoWelt.dll` `0x100414d0` (D)
`ret 8`, arguments `(rec, flag)`. Returns 0 at every `0x1004174a`; success returns the island's slot `+0x6c(rec, flag, 0)` (`0x1004173a`), i.e. the terrain/occupancy test (§8.5). In order:
1. **Type group** (`0x10041502`–`0x1004150d`): `type = world->slot+0x70(rec.type)`; type slot `+0x5c` (PRODTYPE) must satisfy `((p - 1) & ~0x1f) + 1 == 0x21`, i.e. PRODTYPE 33..64 (warehouse / main market group). 1302 is `KONTOR`.
2. **Goods** (`0x10041524`–`0x10041598`): for goods `g = 0x29 .. 0x2d`, `need = type.slot+0xcc(0, g)`; if non-zero, sum over the ship's hold list (`n = world->slot+0x88(0)`, hold `i` = ship slot `+0x84(i)`, goods id = hold slot `+4`, amount = hold slot `+8`) the amounts of the holds whose goods id is `g`, subtracting from `need`; fail while `need > 0` after the last hold. **Duplicate holds of one goods therefore add.** Insufficient material rejects.
3. **Gold** (`0x1004159a`–`0x100415bb`): `cost = type.slot+0xcc(1, 0)`; if non-zero, `player = [ship+0x144]`, `player->slot+0x60() < cost` rejects (**the owner of the ship pays**; `[ship+0x144]` is the ship's owner object; S that slot `+0x60` is the gold balance, consistent with `state.md` "owner.money").
4. **Island flag** (`0x100415c1`–`0x100415ff`): island at `(rec.x, rec.y)` (world slot `+0x8c`); island slot `+0x48(0x10, &out)`, i.e. `AnnoWelt.dll` `0x10094aa0`, returns `island[+0xc4] != 0` in `out[1]`; non-zero rejects. Default `0` (island reset `0x1008c7f7`, ctor `0x1008cbd3`); the only writer found is the property setter `0x10094b4e` (slot `+0x4c`); the earlier "no caller with key 0x10 was found" scan is withdrawn (§11.5): the persisted initial value is the `LOCKED` record; runtime setters are unresolved.
5. **Footprint inside the island** (`0x10041605`–`0x100416b2`): `w,h` swapped when `rec.rot` is odd; `rec.x >= island.slot+0x50`, `rec.x + w <= island.slot+0x58 + island.slot+0x50`, `rec.y >= slot+0x54`, `rec.y + h <= slot+0x5c + slot+0x54` (rectangle `x0,y0,w,h`; `docs/re/annowelt.md` STADT2 reader).
6. **Ownership of each footprint tile** (`0x100416b4`–`0x1004172b`): for every tile `(x, y)`: `o = island.slot+0xec(x, y)` (owner nibble, `0x100860c0`; `15` = neutral, `docs/re/islands.md`). If `o == 15` the tile passes. Otherwise `city = island.slot+0xf8(o)` (`0x100863e0`, `island[0x38 + 4*o]`); `owner = city.slot+0x1c()` (U body), and `0x100311b0(world, island, ship_owner, owner, rec)` must return non-zero. **`0x100311b0` returns 0 if `ship_owner == owner`** (a player may not found on its own territory) and otherwise requires (a) `owner.slot+0x48(0x1c) == 1` (U: player property 0x1c), (b) an existing building at the record's origin whose PRODTYPE equals the record's and is in 33..64 and (c) bit 8 of that building type's slot `+0xd0`. That is a **takeover of a weakened warehouse** (`help/Kontor_uebernehmen.htm`), not a founding. **A footprint with any tile owned by anybody is therefore not a legal founding site** (except a takeover). Unowned tiles of an island on which another player already has a city are allowed by this step.
7. **Terrain / occupancy**: island slot `+0x6c` = `AnnoWelt.dll` `0x1007c790` (about 1,900 instructions): **not yet read** (§8.5).

### 8.5 Terrain (SUPERSEDED by §10.2–§10.4)
The first draft of this section called `0x1007c790` "about 1,900 instructions" and unread. **Withdrawn**: the dump ran into the next function. Control-flow reachability gives **368 instructions, `0x1007c790`–`0x1007cc29`**, four `ret 0xc`, one embedded rotation table at `0x1007cc2c` (data), the next function at `0x1007cc40` (§10.1). It is read in §10.2.

### 8.6 The validator `0x10048b30` (D)
`ret 0x10`, arguments `(rec, flags, figure_slot*, report)`. Order: (1) island under `(rec.x, rec.y)` or reject; (2) `*figure_slot` already set -> must equal this island; (3) type, footprint rectangle against the island's **upper** bounds (slots `+0x50 + +0x58`, `+0x54 + +0x5c`); (4) island property `0x10` must be 0 (`0x10048c32`); (5) owner of the **origin tile** -> `city = island.slot+0xf8(owner)`:
- **city present** (tile owned): `city.slot+0x1c()` -> `0x10043f70(host, island, player, owner, rec)`: returns 0 if `player == owner`; needs `owner.slot+0x48(0x1c) == 1`; accepts if `0x10044080` finds a same-size warehouse type under the tile, else searches a window `cursor+0x48..+0x54` for a shifted position (takeover mechanics, U);
- **no city on the origin tile**: `0x10044150(rec)` must be non-zero: `island = world.slot+0x8c(x,y)`; the placing player's list `player->slot+0x84(&list, 2)` is walked, and the result is 1 only if **none of the player's list-2 objects reports `slot+0x18() == island`**; i.e. (list 2 = the player's cities, §10.5) *the player has no city on this island yet*. Otherwise reject, and with `report != 0` post message `0x138` through world slot `+0xd0` (UI only);
(6) the ship (`+0x1fc`): figure present, `slot+0x8c(0) & 2 == 0`, distance test (§8.3); (7) **rotation search** (`0x10048de7`–`0x10048e27`): up to 4 trials; each trial calls ship slot `+0xbc(rec, report)`; on 0 the record's rotation byte becomes `(rot + 1)`, wrapped to 0 above 3; the **origin `x`,`y` is not recomputed** and the footprint now uses the new rotation (swapped `w,h` for odd); 4 failures return 0; on success the accepted rotation stays in the record and `0x10045080` runs (a figure-proximity routine, §8.7) and its result is returned. The first trial uses the rotation the cursor holds; the loop does not try `(rot, rot+2, ...)` specially: the order is `r, r+1, r+2, r+3 (mod 4)`.

### 8.7 `0x10045080`, `0x100441f0`, `0x10046020` (D classification)
| Function | What it does | Class |
|---|---|---|
| `0x10045080` (AnnoFrame, `ret 0x10`, called `(rec, B+0x1c, mode, 0)`; `mode` = 1 when the counter `B+0x204` has reached 1500, else 0; the second argument is never read, the fourth, 0, disables the class-`0x61` skip) | Builds the rectangle of the record grown by 10 tiles, asks the figure registry (`world->slot+0xa0()` slot `+0x10`, up to `0x100`) for figures inside it, and for each runs a floating-point proximity comparison and creates highlight effects (type properties `0x651`/`0x63f`, scale `2.3f`); returns 1 unless a figure passed the filters (§10.5): **it can veto** | validation **and** presentation (marker effects) |
| `0x100441f0` (cursor, `ret 8`, `(rec, n)`) | Chooses a submit kind: `[this+0x20] != 0` -> kind 6 with that unit; else `[this+0x24] != 0` -> kind 2 with that unit; else `0x10044150(rec)` non-zero -> kind 8 with the player `[this+0x5c]`, otherwise kind 7 with `[this+0x1c]`; calls `[this+0x58]` slot `+0x54(kind, rec, n, obj, 0)` | command construction/submission |
| `0x10046020` (cursor, `ret 8`) | Looks up type property `0x479` and starts an effect at the placed position (floats from the record) | presentation (sound/visual) |
So the click does **not** mutate the world itself: it submits through the sink. Which of kinds 2/6/7/8 a warehouse click uses depends on the cursor fields `+0x1c`, `+0x20`, `+0x24`, whose writers are **U**. The pair ship slot `+0xbc` / `+0xc0` and the pair "kind 2 -> `obj->slot+0xc0`" make kind 2 the ship founding (S).

### 8.8 The authoritative mutation (D)
tNetClient apply `0x100589a0` kind 2 (`0x10058c52`): for each 10-byte record whose type word is non-zero: `obj->slot+0xc0(rec, [this+0x10], 0)`. For a ship, slot `+0xc0` = `AnnoWelt.dll` `0x10041760` (`ret 0xc`), **no validation inside**:
1. `type = world->slot+0x70(rec.type)`. **Gold first**: `g = type.slot+0xcc(1, 0)`; if non-zero, `[ship+0x144]->slot+0x68(g)` (subtract from the owner).
2. **Goods**, `g = 0x29 .. 0x2d` ascending: `need = type.slot+0xcc(0, g)`; for hold `i = 0 ..` (ship slot `+0x84(i)`, same enumeration as §8.4): if hold goods id `== g`: `take = min(hold.amount, need)`; hold slot `+0x10(take)` removes it; `need -= take`; stop at `need <= 0`. **Deduction order: ascending goods id, then ascending hold index; a hold gives `min(amount, remaining need)`.**
3. **City**: island at `(rec.x, rec.y)`; owner nibble of the origin tile; if `island.slot+0xf8(owner)` is non-null, `0x100312a0(ship, city, owner, rec, flags)`; non-zero ends the function. Otherwise (and when the first branch returns 0) `city = island.slot+0xf4(ship_owner, flags, -1)` (`0x100861c0`, gets or creates the player's city; `docs/re/annowelt.md` STADT2), and `city.slot+0xac(rec, flags, 0x2000000)` = `City::Place` `0x100279a0`. In Place, flag `0x2000000` **skips the city's own charge** (gold and city-stock goods, `0x10027a77`); Place then claims land for PRODTYPE 33..35 via island slot `+0xe8(...)` (`0x10027a53`, `0x10027a71`) and creates the building (`state.md` §8.4 step 7).
4. Order of effects: **gold, goods, city creation (if any), Place**. **No rollback observed in the traced path** (`0x10041760`, `0x100279a0` as read in §10.6; the callees named in §10.6 are unread); nothing re-validates inside `0x10041760` (the ship predicate ran in the click; a lockstep peer that executes the stored record does not re-run it, as `state.md` §8.1 already records for all commands).
`0x100312a0` (takeover counterpart of `0x100311b0`) and `0x100861c0`'s creation details are U beyond their call shape.

### 8.9 Territory (D for the call, U for the extent)
Place grants land through island slot `+0xe8` with the city number `[city+4]` and the type's slot `+0x78` result (`0x10027a46`–`0x10027a71`), for PRODTYPE 0x21..0x23, not the whole island. The extent (radius/shape from type slot `+0x78` or `HEAD`) is U; the developer-start loader's whole-island run is not this.

### 8.10 Resulting expedition state
No code in `0x10041760` or `Place` touches the scout or the donkey or reparents them (D for those two functions; the scenario unit loader and the tick that moves them were not re-read). The ship keeps its remaining cargo.

### 8.11 Climate, rival territory, order (D / U)
- No climate test appears in `0x10048b30`, `0x100414d0`, `0x10041760`, `0x10044150`, `0x100311b0` or in the terrain function `0x1007c790` and the type slots it calls (§10.2–§10.4); that is "no climate read in the functions inspected". Callees not read (§10.6, §10.7) could still consult the island's climate byte; the help pages' recommendation is therefore **not** shown to be only a recommendation. Unresolved (§10.9).
- **Rival territory**: any footprint tile owned by another player rejects (§8.4 step 6) except the takeover; **another player's presence elsewhere on the island does not reject**; the **placing player's own existing city on the island rejects** a founding on unowned land (`0x10044150`; S that list 2 is cities) — to be confirmed with the capture of §9.
- **Ordering of simultaneous commands**: not in this path; commands run in stream order (`state.md` §8.1).

## 9. Remaining original-runtime captures (SUPERSEDED by §10.10)
The six captures drafted in the second pass are replaced by §10.10, which ties each capture to one named unknown.

## 10. Third pass: boundaries, terrain, data flow, effects (2026-10-31)

Static, plus **emulation of two pure leaf functions of the original code on synthetic memory** (§10.4; `unicorn` 2.1.4 in the private venv; this is not a run of the game). Labels: **D** read from disassembly; **E** reproduced by emulating the original instructions; **S** by shape; **U** unread. Raw material is in `cache/map2-founding-evidence/` (git-ignored): `cfg.py` (control-flow reachability, jump tables as data), `emu.py` and `coast_model.py` (emulator harness and an independent Python model, 3000 random cases, 0 mismatches), `03-terrain-reachable.asm`, `05/06/07-*.asm`, `04-type-vtable-slots.txt`, `08-emu-slot68-1302.txt`, `09-anlagen-flags.txt`, `10-island-claim-reach.asm`, `11-island-flag10-callers.txt`, `cfg-<va>.txt` per function.

### 10.1 Function boundaries and the corrected ledger
`cfg.py` follows every branch from an entry, adds the targets of an indexed jump table (entry count from the guarding `cmp`, kept only while the target lies inside the function's own range), treats the table as data and stops at `ret`. Results (AnnoWelt unless noted; "instr" = reachable instructions):
| Function | Range | instr | Returns | Notes |
|---|---|---|---|---|
| island `+0x6c` terrain `0x1007c790` | `0x1007c790`–`0x1007cc29` | **368** | 4 (`ret 0xc`) | rotation table `0x1007cc2c` is data; `0x1007cc40` is a different function. The earlier "~1,900" figure was a linear sweep past the end: **withdrawn** |
| ship `+0xbc` `0x100414d0` | `0x100414d0`–`0x10041759` | 236 | 2 | |
| ship `+0xc0` `0x10041760` | `0x10041760`–`0x100418dc` | 147 | 1 | |
| `0x100311b0` takeover test | `…`–`0x1003129d` | 88 | 2 | |
| `0x100279a0` City::Place | `…`–`0x10027ead` | 421 | 3 | |
| island `+0xe8` claim `0x10085840` | `…`–`0x10086019` | 689 | 2 | 7-entry switch `0x1008601c` (data) |
| type `+0x6c` `0x10008920` (AnnoGame) | `…`–`0x10008a78` | 143 | 9 | |
| type `+0x68` `0x10008f90` (AnnoGame) | `…`–`0x100091d1` | 213 | 6 | |
| coast helper `0x10008ea0` (AnnoGame) | `…`–`0x10008f81` | 96 | 3 | leaf |
Workload: the terrain function was never the large item; the territory claim (689 instr + helpers) is the largest unread body.

Call graph (virtual calls resolved by vtable: island `0x100cac2c`, world `0x100ccdd8`, ship `0x100c4da4`, hold `0x100c4f28`, player `0x100c2238`, city `0x100c2f40`, all AnnoWelt; building type `0x1002185c`, AnnoGame):
```
click -> AnnoFrame 0x10048e60 -> 0x10049340 (centre) -> 0x10048b30 (validator)
   -> island +0x8c? (world) / island +0xec,+0xf8 / 0x100311b0 or 0x10043f70 / 0x10044150
   -> ship +0xbc 0x100414d0 -> type +0x5c,+0xcc; hold +4,+8; player +0x60; island +0x48(0x10); island +0xec,+0xf8 -> 0x100311b0
        -> island +0x6c 0x1007c790 -> island +0x88 0x10083c80 -> type +0x6c 0x10008920 -> type +0x68 0x10008f90 -> 0x10008ea0
   -> 0x10045080 -> world +0x158 +0x178 +0x154, 0x100454d0
   -> 0x100441f0 -> [cursor+0x58] +0x54 = 0x100589a0 kind 2 -> figure +0xc0 = ship 0x10041760
        -> player +0x68; hold +0x10; island +0xf4 0x100861c0; city +0xac 0x100279a0 -> island +0xe8 0x10085840
```

### 10.2 The island terrain predicate, `0x1007c790` (D; ret 0xc; `this` = island; args `(rec, flags, _)`)
`rec` = the 10-byte record `{u16 type, u16 x, u16 y, u8 rotation, …, byte 8}`; island fields `+4 x0`, `+6 y0`, `+8 w`, `+0xa h` (words), `+0xc` tile records (8 bytes), `+0x14` the byte height grid (stride `w+1`, `docs/re/annowelt.md` line 9), `+0x1c` the building-type registry. In order:
1. `type = registry.slot+0x70(rec.type)`. Footprint bytes `fw,fh` = type bytes `+6`,`+7`, swapped for odd rotation (jump table `0x1007cc2c`; a rotation above 3 leaves both bytes uninitialised stack, **U** whether reachable: the ship predicate and the validator only produce 0..3).
2. **Bounds** (`0x1007c814`–`0x1007c879`): `rec.x >= x0`, `rec.x + fw <= x0 + w`, `rec.y >= y0`, `rec.y + fh <= y0 + h`; else return 0.
3. `flags |= type.slot+0x60()` (= the u16 flag word at type `+0x12`; 1302: `0x2087`). If `flags & 0x400000` skip step 4–5.
4. **Existing objects** (`0x1007c8ad`–`0x1007c92a`): island `+0x88(&{x,y,fw,fh}, buf)` (`0x10083c80`) copies word 0 of every footprint tile record (the object/building type id) into `buf`; for every non-zero id `o`: `existing = registry.slot+0x70(o)`; **`type.slot+0x6c(existing, flags)` must return non-zero, else return 0** (§10.3).
5. If `flags & 0x10`: a per-8×8-chunk coverage test over tiles whose existing type has flag `0x10` and whose tile byte 3 bit 7 is clear (`0x1007c955`–`0x1007cb3d`); every covered chunk must contain a mark. **1302 does not have bit `0x10`** (`0x2087`); 129 of the 921 types do (`09-anlagen-flags.txt`).
6. Unless `flags & 0x1000000`: `index = (rec.y - y0)*(w+1) + (rec.x - x0)`; call `type.slot+0x68(heights + index, w+1, rec.rotation, flags)` (§10.3, §10.4); a negative return rejects; otherwise **`rec[8] = al`** (the height byte; 1302: `0x30`).
7. **Acceptance** (`0x1007cbc2`–`0x1007cc26`): if `flags & 0x7000` is `0x4000` (road) or `0x2000` (coast) accept at once; else if the type's PRODTYPE group is `0x141` accept; else accept iff `rec[8] >= trunc(2*G/0.04)`, where `G` is `0.04*16` computed by the static initialiser `0x1007b340` (stored as f32 `0.64`), so the threshold is exactly **32** (computed in f32 as the code does: `2*0.64f / 0.04f = 32.0`). That non-coast, non-road land types therefore need a height byte of at least 32 under the footprint (from the slot `+0x68` result).
Callers pass `flags` `0x2000000` (preview) or `0x8000000` (click); neither bit is tested here. No climate byte, owner, PRNG or figure is read in this function.

### 10.3 Building-type slots (AnnoGame, type vtable `0x1002185c`, D)
`+0x50` byte `[+6]` (footprint x), `+0x54` byte `[+7]`, `+0x5c` PRODTYPE (word `[+0x10]`), `+0x60` flag word `[+0x12]` (u16; the `0x2000` coast / `0x1000` mountain / `0x4000` road bits of `production.md`), `+0x78` byte `[+0x65]` (action radius; 1302: 24), `+0xcc(kind, goods)` cost getter (`0x1000a5a0`), `+0xd0` (`0x1000a5f0`, bit 8 used by takeover).
**`+0x6c` placement compatibility `0x10008920`** `(existing_type, flags)`; `this` = the placed type. Returns 1 at once if `existing.flags & 0x20`; returns 0 if `flags & 0x10000000`; otherwise decided by PRODTYPE groups (`((p-1)&~0x1f)+1`): for a placed type in the groups 1, 0x21, 0x61 (production, market/warehouse, public) it returns 1 if `existing.PRODTYPE == this.PRODTYPE` and existing slot `+0xd0` bit 8, or if (`this.flags & 0x10` or `flags & 0x10`) and the existing group is `0x141`; else 0 (the other groups have their own rows, all read in the listing `05-type-slot6c.asm`). **Types with flag `0x20` in `Anlagen.dat`** (file evidence, `09-anlagen-flags.txt`): `ROHSTWACHS` 81, `ROHSTOFF` 180, `ROHSTDUERR` 30, `RUINE` 32, `STEIN` 29, i.e. the growing/raw-resource objects (trees, stones, ruins) are overlappable; any other existing building or object under the footprint rejects a Kontor unless the same-PRODTYPE replacement row applies.
**`+0x68` height analysis `0x10008f90`** `(heights_ptr, stride, rotation, flags)`. If `typeflags & 3 == 1` it returns the rounded average of the four corner bytes at the pointer (small footprint types). Otherwise it scans the `(fw+1) x (fh+1)` vertex window and derives the result byte by `(typeflags | flags) & 3`: `0` = maximum over cells of the cell minimum (quantised `& 0xf0`), `2` = minimum over cells of that, `3` = the window maximum `& 0xf0`; then, if `flags & 0x1000004 == 4`, a class check by `flags & 0x7000`: **`0x2000` = the coast helper** (§10.4); the branches for no class, `0x1000` and `0x3000` (helper `0x10008a80`) are **not decoded** (no founding type uses them). Only the `0x2000` branch is needed for 1302.

### 10.4 The coast rule (types with `flags & 0x7000 == 0x2000`, low bits `0b111`), D + E
Verified by emulating `0x10008f90` and `0x10008ea0` (original instructions, synthetic memory, type bytes `w`, `h`, flag word `0x2087`) against an independent Python model, 3000 random vertex grids over `w` 1..5, `h` 1..4, all four rotations: **0 mismatches** (`coast_model.py`). Let `fw x fh` be the rotation-adjusted footprint and `V(x, y)` the height byte at vertex `(x, y)` of the `(fw+1) x (fh+1)` window whose top-left vertex is the footprint origin (`index` above). Let `n = w + 1` and `m = h + 1` use the **unrotated** type dimensions. The type is accepted with result `max V & 0xf0` iff:
- `n <= 2` or `m <= 2` (a footprint 1 tile wide or deep): accepted without any terrain test (the `max V & 0xf0` is still the result); else
- `(max V) & 0xf0 == 0x30` over the whole window, and
- the **shore edge** has every one of its `n` vertices exactly `0x30`: rotation 0 = the top row (`y = 0`), 1 = the left column (`x = 0`), 2 = the bottom row (`y = fh`), 3 = the right column (`x = fw`); and
- on the **opposite edge** at least `floor(n / 2)` of its `n` vertices are `< 0x20` (water).
Anything else returns `-1` (reject). For the 4×4 Kontor this means: the building's back edge lies on the `0x30` shoreline and its front is in the water; the footprint is not required to be land. Types of this class in `Anlagen.dat`: 1302, 1304, 1306 (`KONTOR`), the ruins/damaged warehouses 1412/1413/1415/1417 and further `KONTOR` ids 3018, 3314, 3513, 3712, 3855, 3989, plus fishermen, whalers, shipyards and `PIER`s (30 types, `0x2087`). The `0x30` and `0x20` levels are literals of the code; their relation to the water threshold of the terrain pass (`annowelt.md` line 1447) is U.

### 10.5 Veto by figures, and the own-city rule (the veto part is corrected and completed by §11.4)
- **`0x10045080` can veto** (D). The record's rectangle grown by 10 tiles is queried in the figure registry (`world.slot+0xa0()->slot+0x10(rect, 0xc0, buf, 0x100)`, up to 256 figures). The result flag starts at 1 (`0x10045165`) and is set to **0** at `0x100452d1` or `0x100453c1`; it is returned at `0x100454b4`. Two ways: (a) a figure with an owner object (`world +0x158`, `0x100a5dc0`), that is armed (`world +0x178`, `0x100a5f70`: figure class group in 108..112 with at least one occupied slot and `flags & 0x200e == 0`) and hostile to the placing player (`world +0x154`, `0x100a4df0`: different player and the player's relation kind 2 non-zero) and within a float distance of the footprint rectangle (`0x10045262`–`0x100452bc`, radius from the figure type's float property 4); (b) any other figure for which `0x100454d0` returns 1: its position lies strictly inside the footprint rectangle (integer rectangle against the f32 position), or, if it is of the class group `0x71`, its hull box (type property 5 as half-extent, truncated) overlaps the rectangle. With `mode == 0` the routine returns 0 at the first veto, with `mode == 1` it first places marker effects for up to 11 figures (presentation only; the result is the same). **The cursor's own ship is not excluded** (the second argument of `0x10045080`, `B+0x1c`, is never read). Whether the founding ship or its embarked scout appears in the queried class mask `0xc0` (**U**: class mask meaning). The two numeric sources (property 4, property 5) come from `FigurenHeader.dat` and are U.
- **Own-city rule** (D): `0x10044150` walks the player's list kind 2 (`player.slot+0x84`, `0x1001c5a0`: list head at `player + kind*8 + 0x26c`); kind 2 is the player's **cities** (the scenario reader registers a city through player slot `+0x8c(…, 2, city)`, `annowelt.md` line 182), each city's slot `+0x18` is its island (`production.md`); the rule returns 1 only if no city of the player has the target island. It applies when the origin tile has no city; if the tile is owned by any city the takeover rules of §8.4 step 6 apply instead.

### 10.6 The proven data flow of the warehouse click to the ship command (D)
Notation: `B` = the adjusted cursor base (`esi - 0x20c` in `0x10048e60`, the complete object); handler `esi` = `B + 0x20c`.
- Reset `0x10048ae0` (called by the constructor; other callers not enumerated): calls `0x10043f30` on `B`, then zeroes `B+0x1c`, `B+0x20`, `B+0x24` (three stores through `lea edx,[esi-0x1f0]`), the rectangle `B+0x48..0x54` (`esi-0x1c4`), the state word `B+0x1f0` (`esi-0x1c`), the bytes `esi-0x18` (valid) and `esi-0x17` (armed), and `B+0x1fc`'s neighbours `esi-0x10`, `esi-0xc`, `esi-8`. `B+0x1fc` itself (`esi-0x10`) is the figure id and is cleared here too.
- The validator argument "figure slot" is `esi - 0x1f0 = B + 0x1c` (`0x10048ec9`/`0x100490b9`/`0x10048f84`): `slot[0]` = **island cache** (`0x10048b7e`–`0x10048b8d` compare, `0x10048c50` store), `slot[2]` = **resolved figure object** (`0x10048d69`: `mov [eax+8], esi`, i.e. `B + 0x24`), `slot[1]` = `B+0x20`, never written by this validator.
- `B+0x1fc` = figure **instance id**, written by cursor property 8 (`0x10049187`: `[sub-0x10]`; the View callback pushes it, `0x10063ba0`); resolved at `0x10048d2f`–`0x10048d49` through `world.slot+0xa0()` then slot `+8(id)`; a null figure rejects. The figure is stored to `B+0x24` only after the island/ownership stage and the `slot+0x8c(0) & 2` test have passed, **before** the distance test; it is not cleared when the distance or rotation search fails.
- Preview events (kind 0 and 1 of `0x10048e60`) call `0x10048b30(rec, 0x2000000, B+0x1c, 0)`; kind 1 first zeroes `B+0x1c` and `B+0x20` (`0x10048e9f`). The **click** (kind 2, `0x10049057`): requires the armed byte `esi-0x17`; calls `0x10043f30` which, if `B+0x1c` is set, calls its slot `+0x68(6)` and **zeroes `B+0x1c` and `B+0x20`** (not `B+0x24`); recomputes the record with `0x10049340`; calls `0x10048b30(rec, 0x8000000, B+0x1c, 1)`, so the island cache is re-resolved and **`B+0x24` is rewritten with the figure**; on success `0x100441f0(rec, 1)` (submission), then `0x10046020` (effect), the byte `esi-0xc` set to 1; on both outcomes the armed byte `esi-0x17` and the state word `B+0x1f0` are cleared (`0x100490fe`).
- `0x100441f0` (`B` = `ecx`): `[B+0x20] != 0` -> kind 6 with that object — **not taken**, because the click zeroed it and nothing in the path writes it; else `[B+0x24] != 0` -> kind 2 with `B+0x24` as the 4th argument: **taken**, with the ship figure object; the sink is `[B+0x58]` slot `+0x54` (`tNetClient::` apply `0x100589a0`, vtable `0x1008914c`; that `B+0x58` is that object is **S**: the argument layout and the record stride match `state.md` §8, its writer was not located).
- Kind 2 (`0x10058c52`): for every 10-byte record with a non-zero type: `obj.slot+0xc0(rec, sink+0x10, 0)`. The concrete method is the **class of the figure at `B+0x1fc`**: for a ship (class `0x10043600`, vtable `0x100c4da4`) it is `AnnoWelt.dll 0x10041760`. The validator's `ship.slot+0xbc` and this commit are the same object's methods.
- Conclusion: for a ship figure the warehouse click reaches `0x10041760` by data flow (D), not by shape. That the figure at `B+0x1fc` is a ship rests on the menu callback (S) and on the fact that the same cursor with a scout figure reaches the scout's own slots (the scout menu `0x10047ae0` uses building 1301; not traced).

### 10.7 Effects, as far as read
- **Gold**: account = player object `+0x74` (dword); read by player slot `+0x60` (`0x1001cfc0`), subtracted by slot `+0x68` (`0x1001cfe0`, `sub [ecx+0x74], eax`, **no clamp**). Whose object: `[ship+0x144]` (S: the owner; the same slot `+0x68` is what City::Place calls on `[city+0x20]`). Amount: type `+0xcc(1, 0)` (u16 gold at type `+0x1c`).
- **Goods units**: cost `+0xcc(0, g)` = `byte << 8` (8.8 tons, `production.md`). A hold is a 12-byte record at `ship + 0xb0 + 12*i` (`ship +0x84(i)` = `0x100418e0`), object vtable `0x100c4f28`: byte `+4` goods id (slot `+4`), word `+6` amount (slot `+8`), **8.8 tons in a u16** (50 t = 12800). So cost and amount are in the same unit.
- **Deduction** (`0x10041760`): gold first, then goods `0x29..0x2d`, then holds ascending; per hold `take = min(amount, need)` through hold slot `+0x10` (`0x1003b9a0`): if `take < amount` the amount decreases, **else the amount becomes 0 and the hold's goods id becomes 0** (the hold is emptied and cleared). Duplicate holds are consumed in index order.
- **Check/commit asymmetry**: the check (`0x100414d0`) sums all holds of the goods; the commit never fails (it stops when `need <= 0`). Payment is complete only if the check passed.
- **City**: `0x10041760` calls `0x100312a0` (takeover) if the origin tile's owner nibble has a city, else `island.slot+0xf4(player, flags, -1)` (`0x100861c0`: allocates the city in the island ring via `0x100967e0`, registers it in the player's kind-2 list, sets player/city flags; details beyond the call shape **U**), then `city.slot+0xac(rec, flags, 0x2000000)` (Place, `0x100279a0`): the flag skips the city's own gold/goods charge (`0x10027a77`), PRODTYPE `0x21`–`0x23` additionally call the claim with the type's radius. `Place` then continues into building creation (`0x10024010`) and the hooks `0x1002c920`, `0x1002ca80`, `0x1002cf60`, `0x1002d2a0`, **unread**.
- **Territory** (superseded by §11.2, which reads and verifies it): island `+0xe8` = `0x10085840` `(rect*, city number, radius = type slot +0x78 (byte +0x65), flag byte)`. `radius = 0` paints the rectangle (helpers `0x100853b0` / `0x100854f0`); `radius != 0` builds an integer row-width table of that radius and paints a region around the rectangle with many virtual calls (`+0x1c`, `+0x40`, `+0x88`, `+0xd0`, `+0x130`, `+0x188`) that treat other cities' tiles. **Shape, extent and conflict handling: U.** The first call (types 0x21..0x23) uses the type's radius; the second, unconditional call (`0x10027a71`) passes zeros. For 1302 the radius byte is 24. This replaces the second-pass statement "extent U".
- **Scout and donkey**: no write in `0x10048b30`, `0x100414d0`, `0x10041760`, `0x100279a0` up to the unread callees above, `0x10085840`'s body as reached. That is "no direct write in the functions inspected", not "no effect on the call path": the unread callees (`0x10024010`, the four hooks, `0x100861c0`'s `0x100967e0`, the territory helpers) could touch them.
- **PRNG**: no `call [reg+0xb4]` (world PRNG slot) in any function body listed in §10.1; callee bodies named above are unread. "No draw observed in the functions read."
- **Rollback**: none observed in the traced path.

### 10.8 Island flag `0x10` (WITHDRAWN: its "no caller" scan used a defective helper; replaced by §11.5)
Getter `0x10094aa0` key `0x10` -> `island[+0xc4] != 0`; setter `0x10094b20`/`0x10094b4e` writes `+0xc4 = (value != 0)`; reset and constructor write 0 (`0x1008c7f7`, `0x1008cbd3`). Across `AnnoWelt`, `AnnoGame`, `AnnoFrame`, `View`, `CompLogic`, `CompLogicClassic`, `AnnoScene`, `AnnoDisplay` there is **no call of a slot `+0x4c` with the literal key `0x10`** within five instructions (`11-island-flag10-callers.txt`; 91 + 19 + 351 + 2466 + 16 + 14 + 14 + 41 call sites). A data-driven key (a property list read from a scenario or save) is not excluded. The flag is read at the validator (`0x10048c32`) and the ship predicate (`0x100415f2`). Producer: **U**; whether any shipped scenario sets it: **U** (settle by reading the island property list of a scenario, not by gameplay).

### 10.9 Evidence matrix
| Question | Status | Where |
|---|---|---|
| Click -> ship commit link | **D** (data flow), figure class S | §10.6 |
| Distance, truncation | D | §8.3 |
| Island bounds | D | §10.2 |
| Occupancy / scenery | D (rule), E-free; the footprint tiles' object words are overwritten (§12.1); whether the overlapped scenery objects are also removed from the island's object container is unresolved (capture D6) | §10.2–§10.3 |
| Coast / terrain, 1302 | **D + E** (original instructions emulated, model matches) | §10.4 |
| Climate | none in the functions inspected; **U** in unread callees and island-level flag | §10.7 |
| Gold account, units, deduction order, emptied hold | D | §10.7 |
| Nearby-figure veto | D that it exists; filters' class mask and numeric radii **U** | §10.5 |
| Own-city rule | D (with annowelt.md for list 2) | §10.5 |
| Island flag `0x10` | producer U | §10.8 |
| Territory shape/extent | **resolved, §11.2** | §11.2 |
| City creation details | U beyond call shape (`0x100861c0`, `0x100967e0`) | §10.7 |
| Building creation and hooks | U (`0x10024010`, 4 hooks) | §10.7 |
| PRNG / scout / donkey effects | none observed in inspected functions | §10.7 |
| Rollback | none observed in the traced path | §10.7 |

### 10.10 Original-runtime captures that would still be useful (each tied to one unknown; none replaces reading code)
1. **Scenery removal from the object container** (the tile words are overwritten, §12.1): one click with trees on part of the footprint; record whether the objects disappear and from which tiles (settles the unread part of `0x100279a0`).
2. **Founding ship vs hull box** (§10.5): place the ship so its hull overlaps the footprint rectangle; record accept/reject.
3. **Territory**: after one founding record the owner nibble of every tile within 30 tiles (settles shape and radius if `0x10085840` is not read first).
4. **Island flag `0x10`**: not a gameplay capture; read the property from a loaded island (memory read) for the endless scenario's islands.
5. **Cargo**: two holds of the same goods: before/after hold amounts and goods ids (confirms the emptied-hold clear).
These confirm or refine selected rows; they do not establish every terrain or climate rule — the terrain rule is established by reading and emulation (§10.4), the climate question by reading the remaining callees.

## 11. Fourth pass: territory, city and building creation, figures, island lock, identities (2026-10-31)

Same conventions as §10 (D read, E emulated original instructions, S by shape, U unread). New evidence files in `cache/map2-founding-evidence/`: `claim_model.py` + `13-claim-stage1-verification.txt` + `14-claim-shape-examples.txt` (territory), `12-helper-verification.txt` (coast helper hardening), `17-prng-closure.txt`, `22-climate-closure.txt`, `closure.py`, `16-create-reach.txt`, `18-island-serializer.txt`, `20-island-slot78.txt`, `21-island-10082040.txt`, `19-figtype-values.txt`, `23-locked-scan.txt`, `24-input-hashes.txt`, `cfg.py`/`ctx.py`/`sweep.py`/`rtti.py` (the helpers). Inputs (sha256): `AnnoWelt.dll f220a878…6ae2`, `AnnoGame.dll ac721220…b7bc`, `AnnoFrame.dll dd3f36ff…9b91`, `View.dll e32fa8a5…33a1`, `FigurenHeader.dat 29fa1f05…68a8`, `Anlagen.dat b2317a5c…dd75`. Repository HEAD `62ddaf1`.

### 11.0 Tool defects found and repaired, claims withdrawn
- `coast_model.py` and `emu.py` now fail visibly: the return sentinel must be reached (an instruction budget that runs out is an `Incomplete`, a Unicorn error propagates), the exit code is 0 only when every selected case executed and matched (1 mismatch, 2 incomplete or emulator error) and the counts are printed. Verified with the normal run (3,000 selected, 3,000 executed, 0 mismatched, 0 incomplete; 1,254 accepted, 1,746 rejected, seed 7, all four rotations, 20 footprint sizes including non-square), one injected mismatch (exit 1) and one budget of 20 instructions (exit 2) in `12-helper-verification.txt`.
- **`ctx.py` stopped decoding at the first undecodable byte of a section**, so every earlier negative result that used it was incomplete. This invalidates: §10.8's "no call of a slot `+0x4c` with the literal key `0x10`" (see §11.5), and the earlier "no callers of `tNetClient` slot `+0x50` with a pushed code address" search. `ctx.py` now resynchronises like `sweep.py`. `sweep.py` was not affected. No other claim in §8–§10 depended on `ctx.py`.

### 11.1 What the click pays and in which order (consolidated)
Click validation (`0x10048b30`, §8.6, §10.6) -> sink kind 2 -> ship `0x10041760`: (1) gold, (2) goods, (3) ring-slot allocation and city object (`0x100861c0`/`0x100967e0`), (4) `City::Place` (`0x100279a0`) with flag `0x2000000`: (4a) territory disc, (4b) footprint claim, (4c) building creation (`0x10024010`), (4d) Kontor counter, (4e) tile writes (`island +0x78` -> `0x10082040`), (4f) messages. Each step is detailed below.

### 11.2 Territory: island `+0xe8` = `AnnoWelt.dll` `0x10085840` (D, stage 1 E)
**Boundaries.** `0x10085840`–`0x10086019` (689 reachable instructions, returns at `0x100858c5` and `0x10086016`), the switch at `0x10085d4c`–`0x10085d5a` is `dec eax; cmp eax,0x83; ja default(0x10085eb6); movzx ecx, byte [eax+0x10086038]; jmp [ecx*4+0x1008601c]`: the byte table at `0x10086038` has 0x84 entries, all in 0..6, and the dword table at `0x1008601c` has exactly 7 targets (`0x10085d61`, `0x10085e0c`, `0x10085d9a`, `0x10085e42`, `0x10085dd3`, `0x10085e74`, `0x10085eb6`), all inside the function. No other computed branch occurs in the body. Helpers: `0x10085830` (owner normaliser: negative -> 15), `0x100853b0` and `0x100854f0` (row painters), direct `__ftol` `0x100b5f90`.
**Signature.** `(this = island, rect*, city, radius, clip*, force)`, `ret 0x14`. `rect` = `{u16 x, u16 y, u8 w, u8 h}`. `Place` builds it from the record: `x`, `y` of the record and `w`, `h` = the type's footprint bytes swapped for odd rotation (`0x100279f5`–`0x10027a1b`).
**Radius source.** `Place` calls it twice (`0x10027a53`, `0x10027a71`): (1) only for PRODTYPE `0x21`–`0x23`: `radius = type slot +0x78()` = the type byte `+0x65` = `HEAD[0x11]` (the action radius, **24 for 1302**), `city = [city+4]`, `clip = NULL`, `force = 0`; (2) for every type: `radius = 0`, `clip = NULL`, `force = 0`, i.e. the footprint rectangle.
**Painters.** `0x100853b0(this, rect, owner, clip)` (called with `force = 0`) clips the rectangle to the island rectangle (and to `clip` when non-NULL) and writes owner nibble `owner` into every tile whose current owner nibble is **15** (neutral): a tile owned by anyone is left unchanged. `0x100854f0` is the same loop **without** the neutral test (used when `force != 0`; not used by founding). No terrain, height, object or building test: water, mountain and occupied tiles are claimed like any other neutral tile.
**Radius 0** (`0x1008585a`–`0x100858b5`): `owner = normalise(city)`, then one painter call for the rectangle, then the redraw notification (`island +0x188(rect, 0xcc, 0)`).
**Radius R > 0 — stage 1, painting (`0x100858c8`–`0x10085ac4`, verified by emulation).** Let `hx = (w-1)/2`, `hy = (h-1)/2` (integer, w,h >= 1), `px = (w-1) & 1`, `py = (h-1) & 1` (1 for an even size), `cx = x + hx`, `cy = y + hy`. A table `T[0..R]` is built by a Bresenham-style integer circle loop (`0x100858d1`–`0x10085921`, then, for `R <= 3`, `T[e..R] += 1` with `e = R - (R+1)/4`): `T[k]` is the half-extent of the claim `k` rows from the footprint's centre row. For `k = 0..R` two one-tile-high rows are painted (neutral tiles only):
 - top row `y = cy - k` if `y >= island.y0`, bottom row `y = cy + py + k` if `y < island.y0 + island.h`;
 - columns `x_left = max(cx - T[k], island.x0)` through `x_right = min(cx + px + T[k] + 1, island.x0 + island.w) - 1`.
 Rows are painted for `k = 0` first (the one or two centre rows), alternating top/bottom per `k`. The shape depends on the footprint only through its centre and the parities of `w` and `h`. The table for `R = 24` is `[24,24,24,24,24,23,23,23,23,22,22,21,21,20,19,19,18,17,16,15,13,12,10,8,4]`.
 Example (R = 5, 4×4 footprint at (10,10), `#` claimed by stage 1, `F` the footprint claimed by call 2, `.` untouched; rows 6–17 shown):
 ```
  6 .........######.........
  7 ........########........
  8 .......##########.......
  9 ......############......
 10 ......####FFFF####......
 11 ......####FFFF####......
 12 ......####FFFF####......
 13 ......####FFFF####......
 14 ......############......
 15 .......##########.......
 16 ........########........
 17 .........######.........
 ```
 R = 24, 4×4 at (30,30) on a neutral island: 1,976 tiles, rows 7..56, row 7 spans x = 27..36, rows 27..36 span x = 7..56 (`14-claim-shape-examples.txt`). Exact included/excluded boundary tiles = the rows above; they are not a circle (e.g. T has the plateau 24,24,24,24,24).
 **Verification.** `claim_model.py` ports this algorithm and compares it with the original instructions (Unicorn, synthetic island, stop at `0x10085ac4`): 600 cases, radii 1..30, 36 footprint sizes, 446 with island-edge clipping, 324 with pre-owned tiles: 0 mismatches, every run reached the stop address; an injected mismatch exits 1, an instruction budget of 50 exits 2 (`13-claim-stage1-verification.txt`). Own and rival tiles are never overwritten; existing own territory is not "extended" in any other way by this stage.
**Stage 2, objects in the claimed region (`0x10085ac4`–`0x10085ee0`, D, semantics of the object properties S).** The island object container (`island +0x28`, ore/deposit/treasure objects, `annoscene.md`) is queried with the disc's bounding square grown by 6 (`slot +8(rect, 1, buf)`; no capacity argument is passed: the callee fills `buf` on the stack frame, which holds up to about 240 entries before the frame ends; the callee's own bound is U). For each returned object whose position lies in the disc (same row table: `|dy| <= R` and `T[|dy|] >= |dx|`, positions truncated) it reads the object's mask (`slot +0x78(0)`), and **if the placing player's bit is not set**, switches on the object's class (`slot +0x84() - 1`, seven arms) to post a discovery event (world event slot `+0xd0` with codes `0xd`, `0xe`, `0xf`, `0x10`, `0x11`, `0x13`, the last one also calling the object's slot `+0x64`), then in every case sets the player's bit (`slot +0x74(0, mask | bit)`). Effect: the objects (deposits, treasures) in the territory become known to the player and the first discovery raises an event.
**Tail (`0x10085ee0`–`0x10086016`).** Island `slot +0x130(cx, cy, (R+8) & 0xff, 1, playerNo)` (`0x10080c10`, 916 instructions: a chunk-wise reveal with the island mask words `+0xa6`/`+0xaa` and the same per-object discovery), then the redraw notification `slot +0x188(rect, 0xcc, 0)`. The reveal reads and writes only island words `+0xa6`/`+0xaa` directly and, through virtual calls, object masks and events; it writes no tile owner.
**Mutation order inside the claim:** stage 1 tiles (rows k = 0..R), stage 2 object masks and events, reveal, redraw. Failure behaviour: the function has no failure return other than a negative city after normalisation (impossible: normalised); allocation of the local table is on the stack.
**What is left unread inside the claim:** the object container's slot `+8` bound, the meaning of object property 0 and the seven class arms' event semantics (names), and the 916-instruction reveal's tile writes. They affect discovery state and events, not tile ownership. Category in the contract: engine requirement (discovery state) and design decision (whether the rebuild models it).

### 11.3 City and building creation on an unowned site
**Ring allocation `AnnoWelt.dll 0x100967e0`** (`this` = the island's city ring at `island+0x38`, args `(…, number, stamp, &city)`, `ret 0x10`, 142 instructions, one return). `number = -1` selects the slot: starting at `ring.cursor + 1` (`ring+0x40`, wrapping modulo 16), the first of 16 slots whose pointer is zero; none free -> returns `0x80004005` with no city (**after the payment**). Then: a city object of `0x30ec` bytes is created (`0x1002f990`); `city.slot+0x28(island)`; `ring[number] = city`; `ring.cursor = number`; `city.slot+0x3c(number)`; `ring+0x47` (highest used, signed byte) = max; `city` property `0x23` := the stamp argument; **name**: the world property `0x13` object (`island.world.slot+0x48`) yields a string list (`slot +0x5c(0x100dcea4)`); if present, **one world PRNG draw** (`island.world.slot +0xb4`, `0x100a1740`: `table[(index+1) mod 2048]`) is taken `mod` the list size and the chosen string is stored as city property `0x12` (`0x10096910`–`0x10096959`). With no list there is no draw. This is the only PRNG call in the direct-call closure of the whole founding path (`17-prng-closure.txt`; unresolved virtual callees remain, §11.8).
**Registration `0x100861c0`** (`island slot +0xf4(player, stamp, number)`, `ret 0xc`): (1) the allocation above; null player -> returns the city only; (2) `player slot +0x8c(&tmp, 2, city)` registers the city in the player's kind-2 list (the list `0x10044150` walks) and sets the city's owner; (3) the city's property-0 mask gets bit `1 << i` for every player `i` for whom `player.relation(kind 1, i) == 3` (`player slot +0x7c`, byte `[player+0x1ac+6*i+1]`); (4) island property `0xe` (word `+0xa6`) gets the player's bit; (5) island byte `[+0xb0 + number]` := player property 9; (6) island `slot +0x134(1)` (`0x100809f0`) recomputes island word `+0xaa` from the cities' masks. (7) Returns the city. Failure: only (1).
**`City::Place` `0x100279a0`** (`ret 0xc`, 421 instructions, 75 functions in the direct closure): for PRODTYPE `0x21`–`0x23` calls the claim (§11.2) twice; `flags & 0x2000000` skips the city's own gold/goods charge (`0x10027a77`); then sets the "open" flag `0x40000000` when the island's bit for the viewing player is clear (`0x10027b02`). For the market/Kontor group `0x21` (`0x10027b62`): if the origin tile already carries a building of the same PRODTYPE (island `+0x94`) the in-place replacement path runs (not the founding case); else `Create(rec, -1, flags, &out)` (`0x10027c4c`), then `city slot +0xbc`, then **the Kontor counter**: island property `0x11` (the word at island `+0xc6`, setter default 1) `old` -> `old + 1`, and the new building's property `0xe` (KONTID, `building +0x1e`) := `old` (`0x10027c63`–`0x10027c9d`). The PRODTYPE hooks `0x1002c920`, `0x1002ca80`, `0x1002cf60`, `0x1002d2a0` apply to PRODTYPE `0x44`, `0x49`, `0x41`, `0x4e` (`0x10027cc5`–`0x10027cf4`): **none runs for a warehouse** (D; the compares are on the PRODTYPE value). Tile fill for PRODTYPE `0x81`/`0x103` (`0x10027d0a`) does not apply. Then **`island slot +0x78(rec, flags & 0x40000000)`** (`0x1008ec90` -> `0x10082040`): the tile writes (below). For PRODTYPE `0x21`–`0x23`, finally two messages: a record `{type 2, x, y, city number, colour…}` posted through the player's message object (`slot +0x24`) and a record `{type 4}` (`0x10027e49`, `0x10027e74`): posted through the message manager (O-52 in `state.md`); their effect on deterministic state is not known (A4).
**`BuildingIndex::Create` `0x10024010`** (`this = city+0x144`; documented in `state.md` §7.3 and `annowelt.md` line 129): the class switch is `byte [PRODTYPE-1 + 0x1002528c]` -> `dword [idx*4 + 0x1002521c]` (28 entries, all targets inside the function); PRODTYPE 33, 34, 35 -> index 13 -> target `0x10024393`: object size `0x50`, constructor `0x1006c080` (market class, vtable `0x100c86c8`, base constructor `0x100734c0`); PRODTYPE 36 (`MARKTKOMBI`) -> index 14. The shared tail applies the record: `building slot +0x50(x, y)` stores `+6`/`+8`; `slot +0x68(rotation)` stores `+0x0c` bits 0–1; **`slot +0x70(rec[8])` = `0x10071c40` stores the byte at building `+0x0f`**; property `0x23` stamp; `slot +0x28(city)` attach; index insertion at the first free list slot; statistics entries (`0x100251..`). **byte8 -> byte0f resolved:** the building's `byte0f` is exactly the record's byte 8; the terrain function `0x1007c790` writes the height level (`0x30` for the coast class) into the **client's record** during validation, and that record is what is submitted; `Place` and `Create` do not recompute it. Therefore a warehouse founded by the original has `byte0f = 0x30`. The rebuild's rule (`state.md` §8.4: command-created buildings get 0 because the original overwrites `rec[8]` from the terrain check) is the stand-in for a terrain function that is not modelled; for founding the value must be the terrain result (§11.8).
**Tile writes `island slot +0x78` -> `0x10082040`** (868 instructions; **corrected by §12.1**: the earlier text here applied the existing-object preservation check to every placement; it belongs only to placed types that have flag `0x10`). For each footprint tile the routine writes the placed type's id into the tile's object word, updates the footprint-offset byte (dx, dy, rotation), the growth-phase bits and the anchor bit, and stores `rec[8]` in the low byte of the tile's second dword (bits 30–31 of that dword are cleared); the 8×8 chunk redraw counters are bumped (`island +0x94`, O-12). Whether an existing object survives depends on the **placed** type's flag word: bit `0x10` clear (the warehouse, `0x2087`) -> every footprint tile is overwritten unconditionally; bit `0x10` set -> the existing-object check of §12.1 applies. No separate "removal" of the replaced objects appears in the traced code.
**Effects on figures:** none of `0x10041760`, `0x100861c0`, `0x100967e0`, `0x100279a0`, `0x10024010` (class `0x50` branch), `0x10082040` or the claim writes a figure field in the instructions read; the discovery stage can raise events about objects, not figures. This is "no write found in the functions inspected"; the unresolved virtual targets (`17-prng-closure.txt` lists 85, 36, 137, 61, 29, 35, 65, 13 sites) are not shown to be free of it.
**Climate:** no read of the island climate (byte `+0xc0`, property 7) and no `push 7` property fetch occurs in the direct-call closure of any function on the path in `AnnoWelt.dll`, `AnnoFrame.dll` or `AnnoGame.dll` (`22-climate-closure.txt`); so no climate-related acceptance or effect is observed in the traced path. The help pages' "wrong island" guidance is therefore not shown to correspond to a rule; a message `0x138` is posted by the validator when the placing player already has a city on the island (`0x10048cd0`), not by a climate test.

### 11.4 Nearby figures: `0x10045080` completed (D)
- **Called** as `0x10045080(rec, B+0x1c, mode, 0)` from `0x10048e32`, `mode` = 1 once the cursor's counter `B+0x204` has reached 1,500 (a UI throttle) else 0 (the counter is reset when it does); arg 1 is never read; arg 3 is 0 (the class-`0x61` skip is off).
- **Query:** the rectangle `{rec.x - 10, rec.y - 10, rec.x + fw + 10, rec.y + fh + 10}` is asked of the figure registry slot `+0x10` (`AnnoWelt.dll 0x100985c0`): sector cells of 16×16 tiles covering the rectangle are walked; for each registered unit: position `(slot+0x54, slot+0x5c)` divided by 1.0 and truncated (`__ftol`) must satisfy `x0 <= x < x1` and `y0 <= y < y1` (half-open); the unit's state word (`slot +0x8c(0)`; ship: the word at `+0x40`) must have **no** bit of `0x0a`; its class group `(BEWOHNTYPE - 1) >> 4` must be set in the mask. **Mask `0xc0`: groups 6 and 7 = BEWOHNTYPE 97..128**, i.e. `ERKUNDER` 97 (the scout), `TRAGTIER` 98, the soldier and war-machine types 99..111 and `SHIP` 113. **The donkey's BEWOHNTYPE is `ESEL` = index 53 (group 3), so it is not queried.** Results go to a caller buffer of **256** pointers (`0x100`); the walk stops silently when the buffer is full (no overflow error, later units are not seen). That cap is the original's; the rebuild must not import it (a rule-level decision is to evaluate all matching figures; §11.8 category C).
- **Per figure** (`0x10045186`–`0x100454ae`): `owner = world +0x158(f)` (`0x100a5dc0`: for class groups 6/7 the figure's slot `+0x1c()`, i.e. its player), `usable = world +0x178(f)` (`0x100a5f70`: false if the state word has `0x200e` bits; **true for every class except BEWOHNTYPE 108..112, for which at least one of its crew slots must be occupied**), `hostile = world +0x154(player, owner)` (`0x100a4df0`: `player != owner` and `player.relation(kind 2, owner) != 0`, byte `[player+0x1ac+6*owner+2]`). 
  - If `usable && owner && hostile` (**path A**): skip when the owner's property `0x1c == 0x11`; else with `r = figure_type.slot+0x68(4)` (FigurenHeader `LOGIC[0x25] / 8`, 0 -> 7; **ships 16.0, scouts 7.0**) the figure vetoes if `(X - cx)² + (Z - cz)² <= r²` for **any of the four corner points** `(x0,y0)`, `(x1,y0)`, `(x1,y1)`, `(x0,y1)` of the footprint, with `X, Z` the figure's f32 position, corners the integers `x0 = rec.x`, `y0 = rec.y`, `x1 = rec.x + fw`, `y1 = rec.y + fh` (rotation-adjusted), arithmetic in x87 single/extended precision as written (`fild`, `fsubr`, `fmul`, `fadd`, `fcomp`; the comparison treats equality as a hit).
  - Otherwise (**path B**) `0x100454d0(f, rect, 0)`: veto if the figure's float position lies in the **half-open** rectangle `x0 <= X < x1` and `y0 <= Z < y1`; or, for a figure of class group `0x71` (BEWOHNTYPE 113..128: ships), if its hull box overlaps: with `r = figure_type property 5` (the f32 at figure type `+0x108` = `FigurenHeader.dat` `HEAD[0x10]`: **1.55 for ship type 1502**, 2.35 for 1501, 2.275 for 1503), `xmin = trunc(X - r)`, `xmax = trunc(X + r)`, `zmin = trunc(Z - r)`, `zmax = trunc(Z + r)` and the box overlaps iff `xmin < x1 && xmax > x0 && zmin < y1 && zmax > y0` (truncation toward zero).
  - A veto sets the result 0; with `mode == 0` the routine returns at the first veto, with `mode == 1` it first draws marker effects for up to 11 figures (presentation); the returned value is the same.
- **The founding ship's own participation:** nothing excludes it. The ship is a group-7 figure of the placing player: it is never hostile to itself (`player != owner` false), so it goes to path B: its hull box (half-extent 1.55 tiles for 1502) must not overlap the footprint rectangle, and its position is subject to the half-open test. **The embarked scout (group 6) is tested only by the point-in-rectangle test (path B); whether an embarked figure is registered in the sector cells or carries state-word bits `0x0a` is U** (the state word of the ship is `[+0x40]`; the scout's is class-specific).
- **Hostility needs diplomacy data the rebuild does not have** (`state.md` leaves ships/diplomacy commands as `LegacyRaw`). The relation bytes are written by the world's diplomacy command handlers (`0x100ad210`–`0x100ad640`, values 0/1/3 for kind 2) and by a bulk loader `0x1001d7ec`; the initial values for a new five-player start are therefore a scenario/setup property: U (named in §11.8 category A).

### 11.5 Island property `0x10` = the `LOCKED` record
- **Persisted representation (D):** the island serializer `AnnoWelt.dll 0x10089d80` writes, only when `island[+0xc4] != 0`, a record tagged `LOCKED` (string at `0x100dceac`) holding that byte (`0x1008a6eb`–`0x1008a71f`); the island loader tests the same tag (`0x10089c0c`) and reads the record's bytes into `island+0xc4` (`0x10089c28`–`0x10089c32`). The in-game property getter (key `0x10`) returns `+0xc4 != 0`; its setter stores `value != 0`; reset and constructor store 0.
- **Producers in shipped scenarios (file evidence):** `Scenes/**` contains 1,133 `INSEL4`/`INSEL5` islands; **128 carry a `LOCKED` record: 107 with value 1** (8 scenario files: `0100Kampagne01/S01_01_01.sze` 23 islands, `0201SingleAddOn/single30.sze` 12, `single32` 11, `single36` 21, `single39` 32, `single40` 1, `single41` 2, `Tutorial/03T_Combat_01.sze` 5), **21 with value 0** (the 21 `*_DUMMY.scp` templates, one island each). **No island of the endless scenarios (`0010SingleEndlos`, `0011SingleEndlosAddon`, `0500MultiEndlos`: islands come from `Scenes/Islands` templates) or of a non-dummy template has a `LOCKED` record** (`23-locked-scan.txt`, `locked_scan.py`).
- **Meaning required by founding:** a locked island (`LOCKED = 1`) rejects the founding click in the validator (`0x10048c32`) and the ship predicate (`0x100415f2`). Scripts or events may change it at run time through the property setter (callers exist, e.g. `AnnoWelt.dll` `0x1004df56`, `0x1006b8dd` are for other receivers; which callers target islands is U); the persisted initial value is the file's.
- **Withdrawn:** §10.8's statement "no producer found" is replaced by the above.

### 11.6 Identity links (D unless stated)
- **Command sink:** the cursor wires its collaborators by class code in a `SetObject(obj)` method (`0x10046140`): `obj.slot+0x14()` = `0xf00` stores at `B+8`, `0x1000` at `B+0x5c`, `0x1c00` at `B+0x58`. Class codes (`slot +0x14` bodies): world `0xf00` (`0x100a37c0`), player `0x1000` (`0x10020990`), island `0x1300`, city `0x1400`, ship `0x1500`. So `B+8` is the **world**, `B+0x5c` the **player**, and `B+0x58` the object of class code `0x1c00`, the object the submissions go to (its identity as `tNetClient` rests on `0x100589a0`'s record layout and `state.md` §8; the class code `0x1c00` was not matched to that vtable's `+0x14`, S).
- **Activation:** `View`'s menu calls the frame's `SetActiveCursor` (`0x10007e00`, which calls the cursor's slot `+0x5c` = Reset `0x10048ae0`).
- **Payment owner:** the ship's `SetObject` (`0x10043440`) stores an object of class `0x1000` (the player class) at `ship+0x144`, so `[ship+0x144]` is a player object (slot `+0x60` = `[player+0x74]` gold, `+0x68` = subtract). The ship's type goes to `ship+8` (class `0xd00`).
- **Selected figure:** the figure id stored in cursor property 8 comes from the menu callback's `[esi+0x638]` slot `+0x40` result (`View.dll 0x10063ba0`); the class at `View +0x638` is U (naming only): everything downstream (§10.6) is proven from the id in the cursor and the registry lookup.

### 11.7 Corrections to earlier sections
- §10.5: "armed (class group 108..112…)" is wrong: `0x178` is true for every class except BEWOHNTYPE 108..112, which additionally need an occupied crew slot. Path A uses the four corners and the figure type's range property, path B the rectangle and the hull box (§11.4).
- §10.7: territory shape/extent is resolved (§11.2); scenery tile words are overwritten (§11.3, §12.1); removal from the object container is unresolved; the PRNG draw (city name) exists (§11.3).
- §10.9 matrix: see §11.8.
- §8.4 step 6/§10.5 own-city text stands.

### 11.8 Evidence matrix after this pass
| Question | Status |
|---|---|
| Territory shape, boundary tiles, clipping, own/rival tiles, order | **D + E** (§11.2) |
| Territory side effects: object discovery masks/events, reveal radius `R+8` | D structure; object semantics S; reveal's internals U |
| City ring slot rule, registration, name draw (PRNG), counters | D (§11.3); ring-full after payment D |
| Building creation, byte0f, Kontor counter, tile overwrite of scenery | D (§11.3, branch condition corrected in §12.1) |
| Messages posted (record types 2, 4; discovery events) | D that they are posted; their effect on deterministic state U (message manager O-52) |
| Figure veto: query, mask, per-figure rules, boundaries, ship's own hull | D (§11.4); embarked-figure registration/state bits U; diplomacy initial values U |
| Island `LOCKED` | D + file evidence (§11.5) |
| Identity links | D for world/player/sink slots; sink class name S |
| Climate | no read in the direct-call closure; virtual callees unresolved |
| PRNG | one draw (city name) in the closure; virtual callees unresolved |
| Scout/donkey writes | none found in the functions inspected; scout is in the veto query, donkey not |
| Rollback | none observed in the traced path (ring-full leaves payment taken) |

## 12. Fifth pass: corrections found in review of §11 and the independent re-check (2026-10-31)

### 12.1 Tile writes: the branch depends on the placed type's flag `0x10`
`AnnoWelt.dll 0x10082040` (island `+0x78` body; entry 0x10082040, 868 instructions, listing `21-island-10082040.txt`) is re-read with control flow, not instruction presence:
1. `0x10082063`–`0x10082079`: the placed type is `registry.slot+0x70(rec.type)` and is kept in `[esp+0x20]` (`0x10082079`).
2. `0x10082184`–`0x1008218f`: `ecx = [esp+0x20]` (the placed type), `call [type+0x60]` (the flag word, `0x10007d80`), `test al, 0x10`, `je 0x100823ed`.
3. **Placed-type flag `0x10` clear -> `0x100823ed`**: the loop at `0x100823ed`–`0x100824fb` writes, for every footprint tile of the visited 8×8 block, `word[tile] := rec.type` (`0x10082447`–`0x1008244a`), the footprint bytes (`0x1008244e`–`0x10082483`), the growth-phase bits and anchor bit (`0x10082497`–`0x100824c5`), `rec[8]` and the clearing of dword bits 30–31 (`0x10082480`–`0x100824a9`). **There is no existing-object test on this path.** 1302 has flag word `0x2087` (bit `0x10` clear), so it takes this path: scenery of any type, including `STEIN`, is overwritten.
4. **Placed-type flag `0x10` set -> `0x10082195`**: the loop at `0x100821c8`–`0x100822c9` tests each tile first: `old = word[tile]` (`0x100821e6`); if `old == 0` -> write; else `existing = registry.slot+0x70(old)` (`0x100821f4`–`0x100821fa`), `call [existing+0x60]` (`0x10082201`), `test al, 0x10`, `je write` (`0x10082204`–`0x10082206`); then `test byte [edi+1], 0x80` and `jne skip` (`0x10082208`–`0x1008220c`) where `edi` is the tile address **plus two** (`lea edi,[tile + edx*8 + 2]`, `0x100821e0`), so `[edi+1]` is **tile byte 3, bit 7** (the anchor bit); a skipped tile keeps its old object word, footprint bytes, phase and anchor; the second pass of the routine (`0x100822cd`–`0x100823e2`) still runs over the whole rectangle and writes the walk code and walk mask of every tile, a skipped one included (the sim's `place_object_record` does the same).
5. The preservation test therefore concerns **existing** objects whose own type has flag `0x10` **and** whose tile has the anchor bit set, and only when the **placed** type has flag `0x10` (shipped examples of such placed types: scenery and mine types, `09-anlagen-flags.txt`: 129 types with bit `0x10`; none of the 30 coast-class types has it).
| Placed type flags | Existing object word | Existing type flag `0x10` | Tile byte 3 bit 7 | Result | Evidence |
|---|---|---|---|---|---|
| warehouse 1302, `0x2087` (bit `0x10` clear) | any | any | any | tile overwritten | `0x1008218f -> 0x100823ed`, no test on the path |
| placed type with bit `0x10` | 0 | – | – | overwritten | `0x100821ea`–`0x100821ed` |
| placed type with bit `0x10` | non-zero | clear | any | overwritten | `0x10082204`–`0x10082206` |
| placed type with bit `0x10` | non-zero | set | clear | overwritten | `0x1008220c` (`jne` not taken) |
| placed type with bit `0x10` | non-zero | set | set | **kept** | `0x10082208`–`0x1008220c` |
The `STEIN` types (flags `0x20|0x10`) are overlappable for placement (`type +0x6c`, existing flag `0x20`) and are overwritten by a warehouse; whether they would be kept under a placed type with bit `0x10` depends on the anchor bit. The sim already models the anchor bit as `TileLayer::foot_anchor` ("the first footprint tile written in a world-aligned 8×8 block", from this routine); no tile replacement is implemented by this task. The earlier statement in §11.3, the contract §6.8 and its fixture are corrected accordingly.

### 12.2 Selected-ship rejection (kept separate from nearby-figure filtering)
`AnnoFrame.dll 0x10048d54`–`0x10048d5c`: after the figure is resolved from the cursor's id (`0x10048d2f`–`0x10048d49`, null rejects), `call [figure+0x8c](0)` is tested with `test al, 2; jne 0x10048c9d` (reject). **The selected ship's state word must have bit `0x02` clear.** For the ship class the word is the ship's `+0x40` (slot `0x10041230`, key 0). The bit's meaning is not known and is not named here. The same bit is part of the nearby-figure query exclusion (`state word & 0x0a`, §11.4); excluding a figure from the proximity query does not validate that it may found. The figure pointer is stored in `B+0x24` only after this test (`0x10048d69`), so a rejected ship never reaches the submission. Regression to specify (not implemented): with every other input valid, the same command with only this bit set on the selected ship must be rejected, and with the bit clear must pass this predicate.

### 12.3 Height grid: disk samples to the runtime corner grid (`AnnoWelt.dll`)
Established for task U1 (details and files in `cache/map2-founding-evidence/25-island-reset.txt`, `26-island-19c.txt`; reading order below):
- **Allocation** (`0x1008c18a`–`0x1008c1d0`, island init): the grid pointer `island+0x14` is allocated with **`(H + 2) * (W + 1)` bytes and zero-filled** (`rep stosd`/`stosb`); `W`, `H` are the words at `island+8`, `+0xa`. Stride `W + 1`; one corner column more than the tiles and **two** corner rows more than the tile rows.
- **Copy** (island slot `+0xac`, `0x10084870`, vtable `0x100ca19c`/`0x100cac2c`): a `HEIGHT` block `{u16 x, u16 y, u8 w, u8 h, data}` is copied row by row to grid offset `(by - y0) * (W + 1) + (bx - x0)`; the right end `bx - x0 + w` is clamped to `W + 1` and the bottom end `by - y0 + h` to `H + 1` (`0x10084879`–`0x1008491a`), the source row stride is the block width (`0x10084957`–`0x1008495c`). The blocks tile the island exactly (`insel.md`, verified for the 1,112 `INSEL5` islands; the 21 `INSEL4` dummy templates are not covered by that statement), so the clamp never cuts a block; every sample `HEIGHT[x, y]` (`x < W`, `y < H`) lands in grid vertex `(x, y)`.
- **Boundary vertices:** column `x = W` (all rows) and rows `y = H` and `y = H + 1` keep the allocation value **0**. The copy writes nothing there (the clamp is to `W + 1` columns and `H + 1` rows, but the blocks do not extend beyond `W × H`); the allocation is the only other writer found.
- **After the copy** (`0x100849cf`–`0x10084a2b`): island slot `+0x19c(&{x-1, y-1, x+w+2, y+h+2})` (`0x10084150`) and the notification slot `+0x188(…, 0xc9)`. `0x10084150` **reads** the height grid (`0x10084208`: `mov esi,[edi+0x14]`; a 9×9 vertex window per 8×8 chunk, minimum and maximum) and **writes bytes 2 and 3 of the 16-byte per-chunk record at island `+0x94`** (the two stores at `0x10084323` and `0x10084331` go to `ecx = (chunk_row * [island+0x8c] + chunk_col) * 16 + [island+0x94]`, `0x100842f1`–`0x10084319`; the stored values are min/2 and max/2). It does not write the height grid. The 9×9 windows starting at 8-aligned chunk origins plausibly (S, not shown) explain the spare row beyond `H + 1`.
- **Other users of the grid pointer** (every `mov/add reg, [island+0x14]` outside the stack in `AnnoWelt.dll`, eleven sites listed in the evidence): the placement function `0x1007c790`/`0x1007cb6b`, the chunk-record builder `0x10084150`, the height-reading slots `+0xa0`, `+0xa4`, `+0xa8` (corner read), `+0xb0` (rectangle read, copies bytes out), `+0xb4`, `+0xb8`, the terrain mask passes `0x10090382`–`0x10092bc0`. No store into the grid memory was found at any of them (the byte stores near `0x100903f8` and `0x10090932` target other buffers). **This is a bounded search, not a proof that no other module writes the grid.**
- **Runtime reads by the placement predicate** use vertices `(x0..x0+W, y0..y0+H)` only (a footprint inside the island reads at most vertex column `x0+W` and row `y0+H`): the zero boundary is read, row `H + 1` is not.
- Therefore: **runtime vertex(x, y) = HEIGHT[x, y] for x < W and y < H; 0 for x = W or y = H (within the placement-reachable range).** Unresolved: none for the placement-reachable vertices; unresolved for completeness: a late writer in another module (bounded search above).

## 13. Pass 3: ledger of the remaining dependencies (2026-11-01, task MAP-2)
Produced by a read-only research subagent (scratch listings and scripts under `cache/map2-research3/`, git-ignored, derived from the original binaries); **the claims below are the research author's and were given to a separate reviewer** (record: the review section at the end of this section). Tags: ORIGINAL = instructions or file records read; DERIVED = inferred from read code plus a search with a stated bound; UNKNOWN. The earlier statement that the registration step only touches the player list, the mask, the island word and one byte (§11.3) is **incomplete**: see item 1 of the new-effects list.

### 13.1 New effects found on the always-reached registration path (`0x100861c0` → player slot `+0x8c`, kind-2 arm `0x1001c65b`)
- City property 2 := the player's colour byte (`player+0xa8`); city slot `+0x28` attaches the owner (`0x1002f3d0`). ORIGINAL (`0x1001c661`–`0x1001c68d`).
- Island property `0xb` (`+0xac`, dword) := a per-city colour record. Body of the setter not read. DERIVED.
- **World slot `+0xe8` = `0x100a1040` (436 instructions) writes bit 31 of the SECOND tile dword** over the bounding box (+7) of the player's `0x21` market footprints, for tiles whose owner nibble equals the player, then notifies `world+0x120`. The call is made unconditionally, but (verified by a second reader) it is **not always effective and at founding takes the CLEAR arm**: it returns at once if `world[0x118].slot+0xc(player).slot+0xc4(6)` is zero (`0x100a110d`; player property 6 unidentified, OPEN); it computes its direction flag from `city.slot+0xc8(0) & (1 << player)` (`0x100a1115`–`0x100a113b`) or the world byte `[world+0x69cd]`, and the registration arm has just OR-ed the player's bit into that mask (`0x1001c661`–`0x1001c678`), so the flag is non-zero and the loop at `0x100a1339` → CLEAR (`and eax, 0x7fffffff` at `0x100a1381`) runs, touching only tiles whose second dword already has bit 31 set and whose owner nibble equals the player. The SET arm (`or dword [edi], 0x80000000` at `0x100a14a5`) needs the flag zero and is itself conditional (first dword bit 31, owner nibble, a type switch excluding property values `0x45`/`0x46`). Simulation tile state; the engine's tile columns do not model bit 31 (OQ-204/OQ-205). OPEN: its checksum role and the notifier's effect.
- A type `0x19` record to the player's AI object when `[player+0xc] != 0` (humans: null). Body of the arm unread. UNKNOWN.
- `City::Place` sets flag `0x40000000` from a per-client viewer field (world `+0xf8`, getter `0x100a34b0`) when the viewer's bit in `[city+0x61c]` is clear, which adds an extra tile pass in `0x10082040` (`0x10082917`–`0x10082a89`) that sets bit 31 of the second tile dword (`or dword [ecx+2], 0x80000000` at `0x10082a46`, only if bit 31 of the first dword is set — `test byte [ecx+1], 0x80` — the pass's own comparisons match and a type switch passes), gated by world property 5, which is `[world+0x69cd] == 0` (`0x100a33ce`: a world-global byte, not a per-client one; the viewer index `[world+0xf8]` is the per-client part). A viewer-dependent tile bit is a determinism candidate. OPEN: who sets `[world+0xf8]`.

### 13.2 Ledger (item | reachability for 1302 | effects | blocks | remaining question)
1. **Initial relations / property `0x1c == 0x11`.** Table `player+0x1ac+6*other+{TRADE,MILIT,WAR}` + CREDIT word, getter `0x1001bf20`, setter `0x1001bfa0`; loader `DIPL` branch `0x1001d758` (stores `0x1001d7ec`/`0x1001d82f`/`0x1001d872`/`0x1001d8b5`); constructor zero-fills 192 bytes. Data (recounted by the verifier over the 50 `.sze`, 0 parse failures; the `.scp` templates are not in this count): 154 `DIPL` records (`0201SingleAddOn` 96, `0100Kampagne01` 34, `0011` 16, `0200Single` 4, `Tutorial` 2, `0010` 2), 1,076 sub-records (WAR 556, MILIT 282, TRADE 238), every value 3. **No endless scenario has a WAR record**; the evidence is `0010SingleEndlos` (9 files: 2 `DIPL`, TRADE only) and `0011SingleEndlosAddon` (3 files: `endless01` TRADE+MILIT among players 0..5 and 31, `endless02` TRADE only, `endless03` none); `0500MultiEndlos` and `0700Multi` contain no files, so they are no evidence. WAR appears only in `0201SingleAddOn` 530, `0100Kampagne01` 24 and `Tutorial` 2. So veto path A cannot trigger at a fresh endless start without a run-time diplomacy change. Property `0x1c` = word `player+0x7a`, the people/nation number set by `VOLK` (types 8/11/12 → 1, 15 → `0xb`, 16 → `0xc`, others → name index 0..19; only 14 names exist); **17 cannot come from the loader**; who stores 17 is UNKNOWN. Blocks: figure veto, mask E8. ORIGINAL (data, loader) / UNKNOWN (external setter).
2. **Embarked scout/ship sector registration.** Registry create `0x10097250` appends to a sector cell only when `pos != NULL` (`0x100973cb`–`0x100973d6`; the NULL-position create returns 0 on that path; the non-NULL return was not followed). The ship's reader `0x1001df31` passes a position: **the ship is in a cell**. The scout is created by the ship's `PRIVAT` loader (`0x10042c70`, `CREW`/`ENTRY` arm `0x10042e38`–`0x10042eeb`) with `pos = NULL`: **the embarked scout is not in a cell**. Ship state word `+0x40`: constructor 0; bit `0x02` written together with the hit-point field in the HP setter (`0x1003da10`, `0x1003db25`–`0x1003db89`) = "hit points reached 0" (DERIVED); bit `0x08` = activity code `[ship+0x75] == 0x13` (DERIVED); no clear of bit `0x02` found. A living stationary ship from a scenario has state 0. Blocks: figure veto. Remaining: does run-time boarding put a disembarked scout back into a cell; what is activity `0x13`.
3. **Hold notification (`0x1003b9ca`).** Hold layout `+4` u8 ware, `+6` u16 amount, `+8` owning ship; the tail calls **ship slot `+0xa0` = `0x100427b0`**, fully read: getters only, one store `fstp dword [ship+0x3c]` at `0x100428be`, a load-dependent f32 speed factor (percent of full load; 100 when the player's people number `[player+0x7a]` is not 1). No PRNG, no event. **Not presentation-only: the payment changes `ship+0x3c`** (a simulation float; its consumers are not read). Reached for both cost goods once per matching hold. Blocks: payment atomicity / ship movement state. ORIGINAL for the body of `0x100427b0` (read twice, one non-stack store); that its virtual callees are pure getters rests on the research author's reading only.
4. **Discovery/reveal and the two messages.** Claim stage 2 (`0x10085ac4`–`0x10085ee0`) calls world slot `+0xd0` (codes `0xd`, `0xe`, `0xf`, `0x10`, `0x11`, `0x13`) per object in the disc whose mask lacks the player, then object slot `+0x74(0, mask|bit)` (the player's discovery bit); code `0x13` also object slot `+0x64`. The world slot forwards to `[world+0x6ad0]` slot `+0x58`, which is AnnoFrame `tEvent` (class code `0x3800`; `0x10061010`): a script/event log, no world-PRNG draw found (its `call [edx+0xb4]` has two pushes and another receiver than the 0-argument world draw `0x100a1740`). Reveal `0x10080c10` (island slot `+0x130`): direct store `mov [ebp+0xa6], si` (players-present word), the rest through virtual calls, 916 instructions not fully read: UNKNOWN whether it writes fog tile bits. The redraw `slot +0x188(rect, 0xcc, 0)` is a chunk counter (`0x1008f2d0`: `inc byte [..+5]`), not an event; the base island vtable has a no-op. Place messages: type 2 to the CompLogic manager (`0x10027e49`; guard `test ebx,ebx; je 0x10027ea0`: null without AI) and type 4 to `[player+0xc]` (null for humans); with an AI they reach AI code that resets targets. Blocks: territory/discovery, FoundCmd/replay if AI or scripts are modelled.
5. **Remaining virtual calls.** 93 functions / 7,664 instructions in the closure of `0x10041760`, `0x100967e0`, `0x100861c0`, `0x100279a0`, `0x10085840`. Resolved by reading the call sites: see 13.1 (rows registration, `+0xe8`, viewer flag), the ship payment calls (pure getters; gold subtract `0x1001cfe0` one store), `Create` `0x10024010` (no PRNG pattern in 70 functions), the veto query `0x100985c0` (pure reads), `0x10027a8b` (skipped: the ship call always sets flag `0x2000000`), `0x10027da8` (tile fill for PRODTYPE `0x81`/`0x103`, not reached for `0x21`). **PRNG on the traced path: the only world draw is the city name (`0x10096910`)**; unresolved callees remain (the `world+0x120` notifier, the reveal body, the island property `0xb` setter, the AI arms), so "no further draw" is not proven.

### 13.3 Exact open questions
(1) does any caller store 17 in player property `0x1c`, and can a run-time war make veto path A true in an endless game; (2) does run-time boarding register a scout in a cell, what is ship activity `0x13`; (3) what does the `world+0x120` notifier do, is tile bit 31 in the net checksum; (4) which input sets world `+0xf8` (world property 5 is the global byte `[world+0x69cd] == 0`), and do clients differ on the bit-31 pass of `0x10082040`; (5) what do the reveal's virtual calls write beyond island words `+0xa6`/`+0xaa` and object masks; (6) what do the AI arms change and do they draw from the world PRNG.

### 13.4 Review of this section
A separate read-only verifier re-derived claims 1–9 from the binaries and recounted the data (its scratch: `cache/map2-verify3/`, git-ignored). Verdict on the first text: all code claims and the data counts CONFIRMED; CHANGES REQUIRED for four text corrections (the direction and gate of the tile-bit routine, the tile-dword precision, the scope of the endless-scenario evidence, the property-5 meaning), applied in this section; re-checked: APPROVED, with two wording nits applied. In this ledger "the player" of the registration arm is the player record; the city is `esi`.

## 14. Pass 4: veto residuals, remaining side effects, city-name load order (2026-11-01, task MAP-2)
Produced by three read-only research subagents (scratch under `cache/map2-research4/`, git-ignored, derived from the original binaries; nothing here has been run in the original). **The claims are the research authors'**; the points the implementation depends on are marked **[read]** where the editor also re-read the instructions (`cache/map2-founding-evidence/10045080.asm`). Tags: ORIGINAL = instructions read, DERIVED = inferred from read code with a stated search bound, UNKNOWN. Items that supersede earlier text are marked SUPERSEDES.

### 14.1 Figure veto (corrects and completes §11.4)
- **Property `0x1c` is the player's people number** (`player+0x7a`, the engine's `Player::volk`): `AnnoFrame.dll 0x10045233 cmp [esp+0x7c], 0x11` after `push 0x1c; call [player+0x48]`, getter case `AnnoWelt.dll 0x10020a58` (`mov dx,[ecx+0x7a]`). ORIGINAL. Stores to `player+0x7a`: constructor (1), the VOLK reader (1, 0xb, 0xc or the name index of the 14 shipped names), the property setter case `0x10020d06` (any word) and command slot `+0xc0` case 3 (`0x1001bdc3`). No call site with an immediate key or code stores 17 (scan over the 7 code modules; the loader cannot produce it). DERIVED: for every value the constructor or loader produces the skip never fires. UNKNOWN: a run-time write of 17 through a data-driven key/code. The engine reads `Player::volk`; the comparison value is data (`Rules::exempt_owner_property`).
- **Ship state word** (`ship+0x40`, u16, slot `0x8c(0)`): constructor 0; scenario-loaded ship: bits `0x20`, `0x100`, `0x8000` set by the init, `0x400` by the reader (and `0x20` cleared again), none of which is in the masks `0x0a`, `0x200e` or `0x02`; DERIVED (`0x10042acf`–`0x10042b74`, `0x1001de6f`, `0x1001deda`). Bit `0x08` = activity byte `[ship+0x75] == 0x13`, written only with an accepted order (`0x1003fa1a`–`0x1003fa39` and sibling sequences); producer of code `0x13` NOT found (UNKNOWN). **Bit `0x02` is not only "hit points 0"**: the HP setter sets it (`0x1003db80`–`0x1003db89`) and advancing a mode-1 order does too (`and 0xfffe; or 0x202`, `0x1003ea74`–`0x1003ea80`); no mask operation clears it; whole-word writers exist whose receiver class is unresolved. SUPERSEDES the "= hit points reached 0 (DERIVED)" gloss of the ledger: the bit is data the engine must carry, not infer. A fresh scenario ship has none of `0x02`, `0x08`, `0x04`, `0x2000`.
- **Embarked scout**: ORIGINAL that the PRIVAT loader creates it with a NULL position and the registry appends to a sector cell only for a non-NULL position (`0x100973cb`–`0x100973d6`), so it is not returned by the query. UNKNOWN: run-time disembark (no disembark routine found; the only re-insert callers `0x10036790`, `0x10048430` handle class-`0x1500` crewed machines of BEWOHNTYPE 108..112).
- **Usable** (`0x100a5f70`): false if state `& 0x200e`; true for BEWOHNTYPE outside 108..112; for 108..112 true iff some `i < n` (`n` = type slot `+0x88(1)`) has `figure.slot+0xb8(i) != 0` (n = 0 gives false). ORIGINAL (`0x100a5f84`, `0x100a5fa4`–`0x100a5fda`).
- **Path A** (`0x10045242`–`0x100452bc`): `r = LOGIC[0x25] * 0.125f`, 7.0f when the byte is 0; corners in the order (x0,y0), (x1,y0), (x1,y1), (x0,y1); `r²` computed in extended precision and stored as f32, `dx²` rounded to f32, `dz²` extended, sum compared with `fcomp` and `test ah,0x41` (≤, equality hits). The x87 precision control at run time is UNKNOWN. **Path B** (`0x10045522`–`0x10045557`): point test on the untruncated f32 position, `x0 <= X < x1`, `y0 <= Z < y1`; hull box with `__ftol` (`0x10077da8`, truncating) of `X±r`, `Z±r`, signed comparisons `xmin < x1 && xmax > x0 && zmin < y1 && zmax > y0` (`0x100455ea`–`0x100455fb`).
- **Scan extents quirk [read]**: `10045080` takes `A` (type slot `+0x50`) and `B` (slot `+0x54`); for rotation byte 1 or 3 it replaces `A` by `B` (`mov ebp, eax` at `0x100450d5`), then `x1 = x + A'`, `y1 = y + B`: **both extents are `B` for odd rotations** (the validator `0x100414d0` swaps properly). Identical for the square warehouse; reproduced by `veto::scan_extents`.
- Boundary differences of a Q24.8 port: the f32 roundings above are not reproduced; results agree where positions are multiples of 1/256, ranges and hull half-extents are exact in both and no intermediate rounding crosses the comparison. The hull half-extent 1.55 (f32 `0x3fc66666`) is not exact in Q24.8; the caller chooses the rounding.

### 14.2 Registration side effects (corrects §13.1)
- **SUPERSEDES §13.1 on island property `0xb`**: it is not a dword at `+0xac`; the in-game island setter (`0x10094b20`, case `0x10094bd9`) does `byte [island+0xb0+rec[0]] = rec[4]`, the record being {city number, owner colour} (`0x1001c6a2`–`0x1001c6c7`); the other writer is `0x100862f3` (index −1 at founding). Readers: only the chunk builder `0x1009e1d0` (ground draw records, indexed by the tile owner nibble); not an island save record. PRESENTATION (ORIGINAL; reads through the getter or a constant displacement were not enumerated).
- City property 2 (colour byte `+0x16`): two getter sites, an overview grid updater and View UI. PRESENTATION (partial).
- **City property 0 mask (`+0x61c`)**: writers `0x100861c0` (owner bit plus peers with relation 3), arm `0x1001c673`, reveal `0x10081268`, owner change, loaders; saved in HEAD2. **AUTHORITATIVE-SIM**: 54 getter sites in AnnoWelt, mostly figure code (`mask & (1 << player)` branches, e.g. `0x1003a497`, `0x10045748`, `0x10053a44`), the Place tests, `0xe8`, the reveal, AI and View. Not every site classified.
- Island `+0xa6` (players present): setter `0x10087b90`, reveal; saved in the DISCOVER record. AUTHORITATIVE-SIM. Island `+0xaa`: recompute slot `0x134`; bit *i* is set iff the number of cities with bit *i* in their mask is at least the city count; only reader the reveal early-out `0x10080c78`; DERIVED cache, not saved.
- **City slot `+0xbc` = `0x10028400`** (called at `0x10027c5d` after Create): recounts the city's PRODTYPE-`0x21` buildings; storage cap `+0x624 = u16 table [0x100dcc84 + 2*count] << 8` (count 1: `50 << 8 = 0x3200`, the constructor default), clamps stock and EK/VK entries to it, sets the slot counts `+0xd8` (EK) and `+0x140` (VK) to 6/4, 8/6 or 8/8 by the highest market level (thresholds 4 and `0xb`). AUTHORITATIVE-SIM; **missed by the pass-3 ledger**. The resulting values for the first warehouse and the constructor defaults are in §14.5.
- World `+0x120` notifier (ctor `0x100a40d0`, 256×256 byte grid pre-filled `0x90`): overview grid, reads the viewer; VISIBILITY/PRESENTATION (grid readers not enumerated).
- **Corrects §13.2 row 4 and §11.4's "null for a human"**: `[player+0xc]` is null only for type-8 humans. All nine type-11 human players of `Scenes/0010SingleEndlos` have a second SPIELER group with a `COMP` record; the reader `0x1001e3be` stores the manager at `[player+0xc]` (`0x1001e439`). The manager exists in every normal game. Place message 2 (`0x1003d7c0`, guard `0x10027df7`) reaches every AI object except type-11 ones (`cmp [esi+0xa18], 0xb; je` at `0x1000511f`); message 4 and the type-`0x19` record reach the type-11 object's handler (`0x100511b0`, unread). Resolved in §14.5 if the follow-up trace completed.

### 14.3 Tile bit 31 and the viewer
- Writers of bit 31 of the second tile dword: Place pass `0x10082a46` (gate: place flag `0x40000000` and world property 5), `0xe8` SET arm `0x100a14a5`, CLEAR arm `0x100a1381`; clears by `and 0x3fffffff` at `0x100824a7`, `0x1008227b`. Readers found: chunk builder `0x1009e1d0` at `0x1009e338`/`0x1009e5ee` (a set bit zeroes the ground gfx id or skips the tile, `0x1009e727`) and the `0xe8` routine itself. Pattern searches over 8 modules (tests/ands/ors with `0x80000000`, `[reg+7]` byte tests, sign-test patterns) found no other reader; the out-record reader `0x10083a90` masks the bit off; the island saver never writes tile dwords. **Class: VISIBILITY per client (draw only) on this evidence; completeness of the pattern search is DERIVED.**
- Viewer: world `+0xf8` is written only by world slot `0x110` (`0x100a34a0`), called from AnnoFrame `SetPlayer` `0x10009060` with the local player's index (`0x10009189`), which also sets player property 6 (`+0x294`) to 1 via slot `+0xc0` (`0x100090c1`). `0x100a1040` returns at once when the player's property 6 is 0. So clients differ in tile bytes, and a headless run (viewer 0, property 6 = 0) writes nothing. **Property 6 has a second reader in the city tick (`0x1002aec9`)**: a block `0x1002aed7`–`0x1002b24d` (after a 500-unit counter at `+0x634`) runs only when it is 1; the block is unread. UNKNOWN whether it writes simulation state or draws random numbers; it is a pre-existing question of the tick, not of founding, but the original's behaviour for the local player may differ from a headless run.

### 14.4 Cargo notification (`ship+0x3c`)
Float multiplier of the Figur base class (initialised 1.0 at `0x10040616`), getter ship slot `0x100` (`0x10041350` = `+0x3c * +0x38`), read in the step routine `0x1003de40` (`0x1003e37d`) to scale the position advance; recomputed by ship slot `0xa0` (`0x100427b0`) from the hold add/sub/clear (`0x1003b950`/`0x1003b9a0`/`0x1003b970`), the HP setter when the new HP is positive (`0x1003dc0d`), the repair tick `0x1003bd60` and the end of the figure loader (`0x10031aed`). Formula: `L = 100` unless the owner's `player+0x7a == 1`, then `L = 100 − 17·sum / ((n1+B)·A)` (sum = the amounts of holds `0..B−1` that carry a ware; `A` = type slot `0x84`, `B` = slot `0x88(0)`, `n1` = slot `0x88(1)` on `[ship+8]`; their meaning is UNRESOLVED); `V = ((35·hp)/maxhp + 65) · L / 100` (hp = word `+0x6c`, maxhp = type slot `0x90`), truncating integer divisions; `ship+0x3c = (float)V · 0.01f`. **Not serialised; a derived cache, not a pure function of holds and type data** (it also depends on HP and the people number). Authoritative only through ship movement (sailing, outside this work).

### 14.5 City-name load order
`STAEDTE` is a `Texte.dat` section (160 names in the GOG English file), world property `0x13` = pointer at `world+0x6ad4` (getter `0x100aeeb0`, setter `0x100af080`), set only by `tFrameGame::SetWorld` (AnnoFrame `0x10008e90`, `0x10008eff`), which runs **after** the scene has loaded (`0x100291af` loads, `0x100291db` calls SetWorld). During STADT2 loading the pointer is 0, so the guard `0x100968ee`–`0x100968f4` skips the draw: **no load-time name draw on the normal start path or the savegame path** (savegames restore the name from HEAD2; HEAD2 would overwrite a drawn name anyway: `0x1002e4c6`, property `0x12` writes the same field, `0x1002f5c2`). ORIGINAL. The seed is set at the scene HEAD (`0x10002ed4`) and is runtime-random (`rand()%2048+1` at start, adopted from the host by LAN clients); the savegame does not store the cursor. UNKNOWN: a second load path into an existing world (AnnoFrame `0x100584fd`, scene slot `0x100029f0`) where property `0x13` could already be set; load-time draws other than island selection and `COMPSPIELER_NAME` were not enumerated. Consequence for the rebuild: **the loader drawing nothing matches the original on the normal path**; `DrawSite::CityNameLoad` stays unported and is unreachable on that path. Fixture of hypothetical per-city draws (not used): scratch `fixture_stadt2_draws.txt`.

## 15. Pass 4, part 2: relations, city slot `+0xbc`, type-11 AI, the reveal (2026-11-01)
Two further research subagents (scratch `cache/map2-research4/followup/`, `.../reveal/`); same status as §14 (the authors' claims, to be verified by the gate review).

### 15.1 Relations
`DIPL` loader branch `AnnoWelt 0x1001d758`: each sub-record is one dword; **byte 0 = the OTHER player, byte 1 = the value**, bytes 2–3 ignored for TRADE/MILIT/WAR (`and eax,0xff` / `shr ecx,8`, `0x1d7db`–`0x1d7e9`). Stores to `[player+0x1ac+6*other+k]`: TRADE k=0 (`0x1d7ec`), MILIT k=1 (`0x1d82f`), WAR k=2 (`0x1d872`); `CREDIT` stores a u16 at `[player+0x1b0+6*other]` (`0x1d8b5`). The table is 32 × 6 bytes, zero-filled by the constructor (`0x10021820`). **Kind numbering: 0 = TRADE, 1 = MILIT, 2 = WAR.** Getter `0x1001bf20(kind, other)`; the setter `0x1001bfa0`. The loader writes only the loading player's own row, with no reverse write (the shipped data is symmetric: `endless01.sze` players 0..5 and 31 list each other with TRADE and MILIT 3; no WAR, no CREDIT). Registration `0x1008622f`–`0x1008627f`: `new_player.relation(1, p) == 3` → MILIT; the mask bit is the loop counter of the world's player list. The veto helper `0x100a4df0`: `player != owner && placing.relation(2, owner) != 0` → the placing player's WAR byte towards the owner. ORIGINAL. (Earlier text calling kind 1 "TRADE-like" or the ledger's "relation kind 1" means MILIT.)

### 15.2 City slot `+0xbc` (`0x10028400`) for the first warehouse
`n` = the city's PRODTYPE-`0x21` buildings; `level` = the maximum over them of the type byte `+0x23` (INFRAMIN); cap `+0x624 = u16[0x100dcc84 + 2n] << 8` (table 0, 50, 70, 90, 110, 130, 140, …); slot counts (`+0xd8` EK, `+0x140` VK) = (6,4) for level < 4, (8,6) for level < 11, else (8,8). Constructor defaults: `+0x624 = 0x3200` (`0x1002fac4`), EK count 6 (`0x10027590`), VK count 4 (`0x10027510`). 1302's INFRAMIN is `INFRA_EVER`, pointer-table index 0 (DERIVED): level 0, n = 1, cap 50 → `0x3200`. **Founding the first warehouse changes nothing relative to the constructor defaults** (DERIVED from the INFRAMIN reading; the later warehouses and higher-level markets do change them). `+0x624` is the per-goods stock cap in 1/256 t; the slot counts are the EK (buy) and VK (sell) trade-list sizes.

### 15.3 AI objects of type-11 humans
The type-11 AI object is in `CompLogic.dll` (vtable `0x100637f0`, factory `0x1003dc50`); `[player+0xc]` is set by the COMP reader (`0x1001e439`): non-null for type 11, null for type 8 (the five-ship start creates kind 8). Place message 4 reaches `0x100511b0` (a closure of 66 functions, 5127 instructions): AI-private island records; world access by getters; no world draw; its one setter is skipped for human types 8 and 11 (`0x10027740`). The type-`0x19` record handler `0x10005380` for type 11 runs the AI scan `0x10018860`…; if the tile predicates classify a zone mask as 3, 5, 6 or 7 it calls `0x100346c0`, which **draws the world PRNG (`0x1003478e`) and places buildings** (`city slot +0xa0`, island `+0x80`): there is no type gate, so a type-11 human can place a building and consume a draw at founding (DERIVED; whether it fires at a given founding needs the AI island record bounds and the tile predicates: UNKNOWN). Hence the engine refuses founding by any player whose kind is not 8.

### 15.4 The reveal `0x10080c10` with flags 1 (called from claim stage 2, `0x10085f31`)
Arguments: centre `cx = x + (w-1)/2`, `cy = y + (h-1)/2` (truncated; the parity terms of stage 1 are not used), radius `(R+8) & 0xff`, flags 1, the player index. Flags 2 and 4 are not reached from founding.
1. **Early out**: if island word `+0xaa` has the player's bit, nothing happens. `+0xaa` bit *i* is set iff the number of cities whose mask has *i* is at least the number of non-null ring cities (`0x10080ab0`–`0x10080ad8`). With only the new city on the island, or every other city's mask already holding the bit, the reveal is a no-op.
2. **Rectangle**: `r = R+8 = 32`; with island `x0/y0/w/h`: `xs = max(0, floor8(cx-r) − x0)`, `xe = floor8(cx+r+8) − x0` if below `x0+w` else `w`, `floor8` = truncation toward zero to a multiple of 8 on absolute coordinates; the same for y.
3. **Samples**: for `yl = ys+2, +8, …` while `yl < ye` and `xl = xs+2, +8, …` while `xl < xe`, four tiles `(xl,yl)`, `(xl+4,yl)`, `(xl,yl+4)`, `(xl+4,yl+4)` (the +4 reads are unchecked at the right/bottom edge; the port reads "no owner"). Owner nibble = bits 25–28 of the second tile dword = the ring index. A sample is kept iff its owner bit is not yet in `M` (initial `0x8000` blocks neutral), `city[owner].mask` lacks the player's bit, `|cy − y_abs| <= r` and `T[|dy|] >= |cx − x_abs|` (Bresenham half-width table for radius 32, inclusive, same algorithm as the claim incl. the `R <= 3` fix-up). Kept samples set `M |= 1 << owner`.
4. **Visit loop quirk**: if `M & 0x7fff` is 0 nothing more happens; otherwise `0x100811f3` reuses `edx` without reloading it (after the first visit `edx` = that city's bit, `0x10081304`), so **only the lowest-numbered city in M is visited** (emulated in `reveal/emu_visit.py`; likely an original defect, reproduced).
5. **Writes for the visited city *c*** (owner *O*): `c.mask |= P`; the founder's **player property 7** `|= 1 << index(O)` (`0x1008128c`–`0x1008129d`); if *O*'s type word is `0xe`, an event-log post (no state); world slot `+0xe8(c, player)` (tile bit 31, visibility); then island `0x134(1)` (`0x100809f0`): recount per-bit city counts, rewrite `+0xaa` (`or word`, 16 bits), and OR into each player *i* the property-7 mask of the owners of cities that know *i*. **Simulation state.** Widths: island `+0xa6`, `+0xaa` are words (players ≥ 16 never present/complete); city mask and property 7 dwords; shifts `shl cl` (mod 32). The engine widens these to sets (documented adaptation).
6. **Island `+0xa6`** at founding: registration `0x100862c5`–`0x100862dd` ORs the founder's bit unconditionally (setter `0x10087b90` case `0xe`, low 16 bits); the reveal does not touch it with flags 1.

### 15.5 Claim stage 2 (objects in the disc; supersedes §11.2's "same row table")
`add ebx, 6` at `0x10085ae8` makes the Bresenham table for radius **R+6 = 30**; the row limit is still `|dy| <= R` (`0x10085c9f`). Objects come from the container (`slot +8(rect, 1, buf)`, rect centre half-extent R+6). Per object class (`slot +0x84`, byte table `0x10086038`): classes 1, 3, 6, 2, 4 → world `+0xd0` codes `0xd`, `0xe`, `0xf`, `0x10`, `0x11` (only if `slot +0x88(1) != 0`); class 132 (treasure, vtable `0x100c759c`) → code `0x13`, then **`player.slot +0x64(obj.slot +0x88(0))` = gold added to the player** (`0x1001cfd0`) — a first-discovery gold gain; the treasure's mask setter `0x100639c0` starts a timer (`+0x38 = 0x5dc`, tick `0x10063830`: state 1 → 2 with 5000, then the object removes itself); every class gets `slot +0x74(0, mask|bit)`. No PRNG draw in stage 2 or the reveal body (the `0xd0`/`0xe8` callees were not read to the end). UNKNOWN: mountain classes 1–4, 6 (vtables, mask consumers); player property 7 consumers.
