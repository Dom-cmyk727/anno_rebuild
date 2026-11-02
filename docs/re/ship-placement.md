# Ordinary-building placement: terrain, height byte, call path, growth store

**Status:** MAP-BUILD-1 package B, 2026-11-02. Static reading plus **emulation of the original instructions** (not a run of the
game: the world, registry and type objects are synthetic). Contract that uses this note: [`docs/sim/ship-placement.md`](../sim/ship-placement.md).
Earlier sections it completes: [`ship-founding.md`](ship-founding.md) 10.2-10.4 (the terrain predicate and the coast rule) and 12.1-12.3 (tile writes, vertex grid).
Labels: **D** read from disassembly; **E** reproduced by emulating the original instructions; **P** file evidence; **U** unread; **NOT_RUN** needs the running original.
Addresses are virtual with image base `0x10000000`; the modules are different images that share that base.

Pinned inputs (SHA-256 of the files in `game/app` read for this note):

| File | SHA-256 |
|---|---|
| `AnnoWelt.dll` | `f220a878510f740c4cfb7224ade0a260e9a0fd7e99113e1201e7a37c0dfa6ae2` |
| `AnnoGame.dll` | `ac721220afd27ebca5fd2ece6a8ff452289b9d7458c4e091c68adea2012db7bc` |
| `AnnoFrame.dll` | `dd3f36ff564d2685d10795272c4ed3c0b2da32a946c2883fb1158ff439249b91` |
| `CompLogic.dll` | `39b622b4a62d9814068619fd4254a675ed1a2a88d6b2594cb6a969c0216d11fa` |
| `Anlagen.dat` | `b2317a5c24b8c9fe796cf40f3fedaefcfa30ad3f185df9e69b2dca6a03d8dd75` |

Tools (this repository): `tools/placement_ref.py` (independent pure-Python rule, vector checker, registry classification, real-island corpus check),
`tools/placement_emulate.py` (unicorn 2.1.4, pefile 2024.8.26; the private venv of `cache/map2-founding-evidence/venv`), vectors
`fixtures/synthetic/sim/placement_vectors.txt`. Raw listings stay in git-ignored `cache/`.

## 1. The normal build path, in order (D)

```
UI cursor (AnnoFrame; check with 4 args rec, flags, {island, city}, commit: body 0x10047170 and its sibling 0x100486b0, **S**: the call sites below are a virtual call [cursor+0xc]; that this slot is 0x10047170 for the ordinary-building cursor was not matched through RTTI, the flags come from the call sites)
   preview (mouse move)  0x10047470 ... [cursor+0xc](rec, flags = 0x2000000, ...)        0x10047547
   click                 0x10047637 ... [cursor+0xc](rec, flags = 0x8000000, ..., 1)     0x1004766f
        -> type flag word bit 0x80 SET (every ordinary building): island +0xec per footprint tile (one owner), island +0xf8 -> city,
           group 0x21 only: 0x10043f70; then City::Build  city slot +0xa0 = AnnoWelt 0x100276a0 (rec, flags)       0x100473ef
           (PRODTYPE 0x23, 6, 7, 0xf = MARKTKOMBI, FISCHER, WERFT, WALFAENGER: up to 4 tries with rec[6]+1 mod 4, UI convenience, 0x1004741e-0x1004744c)
        -> bit 0x80 CLEAR: island slot +0x6c directly (rec, flags, 0)                                                  0x1004722f, 0x10048708
   commit 0x100441f0:  sink [B+0x58] slot +0x54 = tNetClient apply 0x100589a0 (mode 6, rec, count, city, flags = 0)    0x1004420f
apply 0x100589a0 mode 6 (single player and the network turn both end here, docs/re/tick-loop.md l.86), per 10-byte record:
   type word non-zero and footprint owner 0x10058890 == the commanded city                                            0x10058a2c-0x10058a3b
   copy rec -> City::Build(copy, flags | 0x1000000)   [0x10058a4e]  -> if non-zero -> City::Place(copy, [this+0x10], flags)  [0x10058a7f]
City::Build 0x100276a0 (ret 8):
   type = registry slot +0x70(rec.type); p = type slot +0x5c
   group((p-1)&~0x1f)+1 == 0x21 and the object at (rec.x, rec.y) (city slot +0x94) has PRODTYPE p  ->  f = 0x400000                0x100276d9-0x1002770d
   island slot +0x6c = AnnoWelt 0x1007c790 (rec, f | flags, 0): zero returns 0                                         0x10027721
   every footprint tile: island +0xec owner is 0xf or this city, else return 0                                         0x100277c8-0x100277e3
   unless flags & 0x4000000: PRODTYPE 0x43 with type +0xac(0) == 0x52 and 0x1002c090() != 0, or PRODTYPE 0x48 with +0xac(0) == 0x50 and
        0x1002c740() != 0:  rec.type = type slot +0x40() + 1   (the upgrade rewrite; the rewritten type is NOT validated again)    0x10027805-0x10027874
   unless flags & 0x2000000: unless player option 7: affordability 0x100275d0 must be non-zero                          0x1002787d-0x1002789a
City::Place 0x100279a0: charge, Create (rec byte 8 -> building byte 0x0f), tile writes (ship-founding.md 12.1)
```
`flags` of the apply call is `0` from the UI commit (`0x10044200`: `push 0`); the network decode may supply payload flags (docs/sim/state.md F13).
So the island function sees, in the two situations:

| Situation | flags passed to `0x1007c790` | `0x1000000` | `0x400000` |
|---|---|---|---|
| UI preview | `0x2000000` | clear | only the group-0x21 same-PRODTYPE case |
| UI click validation | `0x8000000` | clear | same |
| apply (server and single player), UI-sourced | `0x1000000` | **set** | same |

The bits `0x2000000`, `0x4000000`, `0x8000000` are tested only by `City::Build`/`Place`, never inside `0x1007c790`, `0x10008920` or `0x10008f90`. Other callers of island slot `+0x6c` found by a linear sweep with
a `push 0` third argument (classified by call shape only, not entered): the ship predicate `0x1004173a`, a second figure predicate `0x100362ba` (same shape as the ship's, calls `0x100311b0`),
AI probes in `CompLogic.dll` (`0x1001553c`, `0x10027f50` with `0x2000000`; `0x1002ba94`, `0x1002c84d`, `0x1002cdb9`), `AnnoFrame` `0x1004882a`, `0x1004c4bf`. None of them is a client apply.

## 2. The island function `0x1007c790`, as it matters here (D, E)

Steps 1-7 of `ship-founding.md` 10.2 stand. What the normal path adds:

* **`0x1000000` skips step 6** (`0x1007cb48 test edx, 0x1000000; jne 0x1007cbc2`): the height analysis is **not run** and `rec[8]` is **not written**; step 7 then tests the byte that is already in the record. For the apply call that byte is **the wire byte of the client** (E, `tools/placement_emulate.py`, house 2x2 at (3,3), flat 0x10 (sea), apply flags `0x1000000`):

  | rec byte 8 on the wire | original answer | byte left |
  |---|---|---|
  | `0x40` | accepted (1) | `0x40` |
  | `0x00` | refused (0) | `0x00` |

  With the preview flag `0x2000000` the same site is refused with byte `0x10`. A sloped window (one vertex `0x31` inside a `0x30` plateau) is accepted by the apply call with wire byte `0x30` and refused by the preview. **So the original server trusts a client-supplied height for the `>= 32` test and stores it as the building's `byte0f`.** It is a property of this build, not a rule to copy (contract, adaptation A1).
* **`0x400000` skips steps 4 and 5** (existing-object and coverage tests) and goes to the `0x1000000` test (`0x1007c89a`, `0x1007cba7`).
* The registry lookup of step 1 is not checked (`0x1007c7c0`, `0x1007c7c3`): an id the registry does not know yields a null type and the function faults (E: the emulated call faults on a tile id missing from the registry, `unknown_object_id_unsupported`). It cannot be reached by an accepted command.
* Footprint bytes are stored as bytes; the existing-object buffer is a 0x12c-byte stack frame (`0x3c`..`0x12c` = at most 120 ids) with no length check: a type whose footprint exceeds 120 tiles overflows it in the original. No shipped type is that large; not reproduced (no hard limit in the port).
* The static globals read by step 7 are zero in the image until the module's initialisers run: `0x1007b340` stores `f32(0.04f * 16.0f) = 0.64f` at `0x100e2d88`, `0x1007b3a0` stores `0.64f + 0.64f = 1.28f` at `0x100e339c` (E: the emulator runs both and reads `1.2799999713897705`). Acceptance is `rec[8] >= ftol(1.28f / 0.04f)`. With the globals at zero the test accepts everything (observed in a first emulation run, then corrected); the exact quotient in extended precision is `32`, so **the threshold is 32** (confirms 10.2).

## 3. Type slot `+0x68`, the ordinary-land branch (D, E)

Full listing read: `AnnoGame.dll 0x10008f90`-`0x100091d1` (213 instructions). With `F = flag word (+0x12)`, `A = F | caller flags` (the fourth argument), vertex window `V(x, y)`, `0 <= x <= fw`, `0 <= y <= fh` (`fw x fh` the rotation-adjusted footprint; `rotation & 1` selects the swap, `0x10008fe9`):

* `F & 3 == 1` (`0x10008fa1`): returns `trunc((V00 + V10 + V01 + V11) / 4)` of the four vertices at the pointer (floor for non-negative sums); no class test, no window. Scenery objects (flag word `0x29`).
* Otherwise for each cell `(x, y)` of the `fw x fh` footprint: `cellmin = min(V(x,y), V(x+1,y), V(x,y+1), V(x+1,y+1)) & 0xf0`; `mx = max` over **all** vertices of the window (raw); `lo = min cellmin`, `hi = max cellmin`.
* Result by `A & 3` (`0x100090e0`-`0x10009109`): `0` -> `hi`; `2` -> `lo`; `3` -> `mx & 0xf0`; `1` -> `-1`.
* If `A & 0x1000004 == 4` (`0x10009111`-`0x10009116`) the class test follows, by `A & 0x7000`:

  | `A & 0x7000` | test (D) | result when it fails |
  |---|---|---|
  | `0x0000` (ordinary land) | `mx - lo > 0` | `-1` |
  | `0x1000` | `mx - lo > 0x10` | `-1` |
  | `0x2000` | coast helper `0x10008ea0` (`ship-founding.md` 10.4) | `-1` |
  | `0x3000` | helper `0x10008a80`: **U** (below) | `-1` |
  | any other value | no test (`0x10009199 jne 0x100091c6`) | - |
* `0x1007c790` calls slot `+0x68` once (`0x1007cb91`), and only when `0x1000000` is clear, so `A` never contains `0x1000000` inside the analysis. Other callers of slot `+0x68` were not searched.

**The ordinary-land rule, derived:** class `0`, mode `0` with the class test on means `mx <= lo`. Since `lo <= (cell minimum) <= every vertex <= mx`, every vertex of the window equals one value `v`, and `lo = v & 0xf0 = v`: **all `(fw+1)(fh+1)` vertices are equal and the value is a multiple of 16; the result is `v`**. Step 7 then accepts iff `v >= 32` (so `v` in `{0x20, 0x30, ..., 0xf0}`, in the shipped data up to 208 = `0xd0`). Modes 2 and 3 give the same `v` in that case. A window that touches the island's east column or south row reads the zero boundary vertices (`ship-founding.md` 12.3) and is therefore refused unless the whole window is zero (then `v = 0 < 32`, refused).

**Verification (E).** `tools/placement_emulate.py` runs the original `0x1007c790` (AnnoWelt, with its own island slot `+0x88` and `_ftol`) on a synthetic island whose type objects, registry and tiles are fake; every type slot call (`+0x50 +0x54 +0x5c +0x60 +0x68 +0x6c +0xd0`) executes the real `AnnoGame.dll` bytes in a second unicorn instance. It is compared with the independent `tools/placement_ref.py`:

| Run | Cases executed | Compared | Mismatches | Not compared |
|---|---|---|---|---|
| `random 25000 11`, mixed islands (flat, near-flat, random; 23 types; bounds, objects, flags `0x2000000`, `0x8000000`, `0x1000000` with wire bytes, `0x400000`, `0x10000000`, `0x10`) | 24,572 | 21,214 | **0** | 3,358 reference-unsupported, 428 not runnable (class `0x3000` reads a null table pointer) |
| same, window flattened first (accepts common) | 24,207 | 22,007 | **0** | 2,200 + 793 |
| `vectors` (50 cases of `fixtures/synthetic/sim/placement_vectors.txt`) | 48 | 45 | **0** | 3 unsupported executed + 2 that fault |

Outcomes seen in the compared mixed run: 5,740 accepts and 15,474 refusals (BOUNDS 8,689, TERRAIN 4,007, LOWLEVEL 1,493, OCCUPIED 1,285). **Negative control:** the reference with threshold 31 instead of 32 disagrees with the original in 47 of 2,549 compared cases of the same seed. A first run with the static globals still zero disagreed (140 mismatches) until the initialisers were run; that is how the initialiser dependency in section 2 was found.
Domain of the agreement: footprints 1..5 x 1..4, islands 2..12 tiles square, levels `{0, 0x10, 0x1f, 0x20, 0x2f, 0x30, 0x31, 0x3f, 0x40, 0x50, 0x60}`, all four rotations, flag words in the registry of the tool (`0x84, 0x2087, 0x1886, 0x4386, 0x29, 0x8018, 0x26, 0x94, 0x4186, 0x3a, 0x3096, 0x4`). Not covered: heights above `0x60` in random islands, windows larger than 5x4, class `0x3000`.

**Real-file check (P).** `tools/placement_ref.py corpus` evaluates the terrain analysis on every `PROD2` item of every island in `Scenes/**` (items carry the placed height in `state >> 8`, `docs/formats/insel.md`): for the 25,358 items with flag word `0x84` or `0x04`, **25,299 (99.77 %) equal the stored height**, 12 differ and 47 are refused by the rule (59 stored placements violate the strict flat rule, e.g. a single vertex `0x40` in a `0x30` plateau; these are hand-edited scenario contents loaded without the check, not systematic). For the coast class `0x2087`: 1,955 equal, 50 refused. Class `0x1000`/`0x4000` items (walls, roads): 14,882 / 33,562 equal, 90 / 729 differ (those use other modes; not a rule of this packet). `GELAENDE`: single-quad scenery (`0x29`) equals in all 4,162,409 items. This is corroboration of the byte's meaning on shipped content, not a replay of the game.

### Open here (U)

* **Class `0x3000`** (12 types: 8 `MINE`, 4 `STEINBRUCH`; flag word `0x3096`). `0x10008a80` reads the type's table at `+0x234` (the `HIGHMAP` record, 16 entries in `Anlagen.dat`) and the float helpers `0x10008d90`, `0x10016d60`, comparing a stored byte with `ftol(...) - (arg & 0xff)`. The narrow question: what does the loader store at type `+0x234` (HIGHMAP bytes per type, their layout) and what do `0x10008d90`/`0x10016d60` compute. Not needed for a starter building. **Unsupported, explicit.**
* **Flag bit `0x10` coverage test** (`0x1007c955`-`0x1007cb3d`, 117 + 1 types): not decoded. **Unsupported, explicit.**
* The meaning of the flag-word bits `0x08`, `0x40`, `0x80`, `0x8000` in the terrain function: **not read** (`0x80` selects the UI branch above). They do not change the terrain result.
* Upgrade rewrite targets: the original does not validate the rewritten type; the port must (contract A4).
* Original-runtime capture of a real build click (preview byte, wire byte, building byte `0x0f`): **NOT_RUN**.

## 4. Type slot `+0x6c` (placement compatibility), all rows (D, E)

`0x10008920`, `this` = placed type `P`, argument 1 = existing object's type `X`, argument 2 = `A`. Whole listing read (`05-type-slot6c.asm`, 143 instructions). Groups are `G(p) = ((p-1) & ~0x1f) + 1`:

1. `X.flags & 0x20` -> **1** (growing/raw objects, ruins, stones: overlappable).
2. `A & 0x10000000` -> 0.
3. `G(P) in {1, 0x21, 0x61}` (production, market/warehouse, houses): `X.prodtype == P.prodtype` and `X.word14 & 0x100` -> 1; else if `(P.flags | A) & 0x10` is clear -> 0; else 1 iff `G(X) == 0x141`.
4. `G(P) == 0x81`: 1 iff `G(X) in {0xc1, 0x101}` and `P.prodtype == 0x81` exactly.
5. `G(P) == 0xc1`: `X.prodtype == 0xc6` -> `P.prodtype == 0xc6`; else `G(X) == 0xc1`.
6. `G(P) == 0x101`: `G(X) in {0xc1, 0x101}`. `G(P) == 0x141`: `G(X) == 0x141`.
7. Every other placed group (notably `0x41` public and `0xa1` stalls) -> 0.

`X.word14` is the type word at `+0x14` (slot `+0xd0`, `0x1000a5f0`). Its source is the `HEAD` loader `0x10009432`-`0x10009449`: `word14 = (HEAD[0x2e] << 8) | (HEAD[0x25] & 1)`, so **bit 8 = `HEAD[0x2e] & 1`** (D; the earlier text "needs bit 8 of slot +0xd0, not read" is resolved). In `Anlagen.dat` it is set for 3 of 24 `MARKT` and 3 of 13 `KONTOR` types and no other PRODTYPE (`tools/placement_ref.py classify`'s loader). Step 4 of the island function calls this once per non-zero object id under the footprint (id 0 is skipped) and refuses at the first 0 (E: 43,221 compared cases agree).

## 5. Registry classification (all 921 types of `Anlagen.dat`, P + D)

`python3 tools/placement_ref.py classify`. Flag word `F` is the u16 at `HEAD` offset `0x1c` (type `+0x12`). Support predicate for the **terrain/height rule** of a normal `BuildCmd`:

* **ordinary land:** `F & 0x7017 == 0x0004` (engine predicate `(F & ~0x0080) == 0x0004`, see `docs/sim/ship-placement.md` R3) -> mode 0, class test on, class 0, no coverage test; result `v` of section 3.
* **evidenced coast:** `(F & 0x7017) == 0x2007` -> `ship-founding.md` 10.4 (helper and result `mx & 0xf0`), the same predicate as `coast_class_supported`.
* everything else: not decided, **Unsupported**.

| Class | Condition | Types | Flag words | Reachable by `BuildCmd` (PRODTYPE 1..192) | Result byte |
|---|---|---|---|---|---|
| **ordinary land** | `F & 0x7017 == 0x0004` | **352** (`PIONIER` 52, `DENKMAL` 28, `ZIERDE` 26, `HANDWERK` 24, `MARKT` 24, `PLANTAGE` 21, `MARKTSTAND` 18, `ZAUN` 15, `KAPELLE` 14, ...) | `0x0084` x 344, `0x0004` x 8 | **254** (groups 0x61: 81, 1: 67, 0x41: 44, 0x21: 24, 0x81: 20, 0xa1: 18) | `v`, accepted iff `v >= 32` |
| **evidenced coast** | `F & 0x7017 == 0x2007` | **30** (`KONTOR` 13, `PIER` 11, `FISCHER` 3, `WERFT` 2, `WALFAENGER` 1) | `0x2087` x 30 | **19** (`KONTOR` 13, `FISCHER` 3, `WERFT` 2, `WALFAENGER` 1) | `mx & 0xf0`; coast class accepts without the 32 test |
| tile object, single quad | `F & 3 == 1` | 291 | `0x0029` | 0 | average |
| road, bridge, ramp, pier on road | `F & 0x7000 == 0x4000` | 35 | `0x4186 x15, 0x4386 x18, 0x4087 x2` | 0 | accepted at once |
| wall, road on slope | `F & 0x7000 == 0x1000` | 52 | `0x1886 x41, 0x1186 x8, 0x1386 x3` | 0 | rule decoded, building not implemented |
| mountain/quarry helper | `F & 0x7000 == 0x3000` | 12 (`MINE` 8, `STEINBRUCH` 4) | `0x3096` | **12** | **U** (section 3) |
| overlay with coverage bit | `F & 0x10` (not above) | 117 (`FELSEN` 48, `KLIPPE` 30, `STEIN` 29, `WASSERFALL` 5, `FLUSS` 4, `EISLOCH` 1) | `0x12, 0x3a, 0x92, 0x8018` | 1 (`EISLOCH`) | unsupported |
| ruins | bit 4 with mode 2, class 0 | 32 (`RUINE`) | `0x0026` | 0 | unsupported (object, not a build) |

Total 921. The 98 ordinary-land types with PRODTYPE 0 or >= 193 (`DENKMAL`, `ZIERDE`, `ZAUN`, `PLATZ`, ...) and the 11 coast types of that range (`PIER`) have no building list and are refused earlier by the existing `Unsupported` rule (`docs/sim/state.md` §8.4 step 1). The eight `0x0004` types have PRODTYPE `UNUSED`, `MEER`, `TIERHOEHLE`.

**Two real, affordable ordinary types** (both class "ordinary land", `F = 0x0084`, `PRODTYPE` group `0x61` / `0x01`):
* `501` `PIONIER`, footprint 4 x 4, no gold, 3 wood (bytes 3..6 = `00 03 00 00`, gold u16 = 0): a house.
* `1001` `HANDWERK`, footprint 3 x 3, 300 gold, 4 wood, 3 tools (`00 04 03 00`): the weaver of `production.md` §2.4 (300 gold, 3 tools, 4 wood).
(Also `801` `PLANTAGE`, 3 x 3, 150 gold, 2 tools.) On the shipped islands such buildings stand on plateaus at level `0x30` (P: the real placements of `endless02.sze` island 1: `1001` at (81,129), `501` at (124,153), `801` at (91,109), all windows entirely `48`, stored height 48); the registry cost fields are `HEAD` bytes `[3..6]` and u16 `[8..9]`.

## 6. Growth store (OQ-NAVC-2) (D)

`AnnoWelt.dll 0x1008b5f0` (island growth visit, args `x, y, t`; walks an 8 x 8 block of 8-byte tile records, `esi = tile + 3`). All stores of the function and of the notification it calls:

| Store | Address | Target |
|---|---|---|
| `mov [esi], cl` (toggle bit 6 of byte 3) | `0x1008b681` | tile byte 3 |
| `mov [esi], dl` (phase `+1` in bits 0..5) | `0x1008b6a6` | tile byte 3 |
| `inc word [esi-3]` (object id `+1`: the "replacement") | `0x1008b6c6` | tile word 0 |
| `and byte [esi], 0x80` (phase cleared, anchor bit kept) | `0x1008b6ca` | tile byte 3 |
| notification `island +0x188(rect, 0xce)` = `0x1008f2d0`, case 0xce | `0x1008b709` / `0x1008f41e` | `inc byte [chunk_record + 5]` of the 16-byte record at island `+0x94` |

The other calls of the visit look like type-slot getters from their call shape (callees not entered) (`+0x70` lookup, `+0xe8`, `+0x80`, `+0x7c`). **Nothing writes the second dword of the tile record (`tile + 4`), whose low byte is the item height.** The object is replaced **in place** by incrementing the id; there is no separate replacement callee. Therefore the item height byte **remains** across growth stages (D). This is exactly what `grow_chunk` (`island_tick.cpp`) does. OQ-NAVC-2 is resolved for the growth visit; a different writer (the loader `0x1007aa40` and the building write `0x10082040`, `ship-founding.md` 12.1) is not affected. Bounded: only the visit and its notification were read; other tick passes (fauna sweep `0x10093480`, drought) were not searched for tile `+4` stores.
`docs/sim/ship-navigation.md` 12.13 limitation (1) can therefore be restated as "growth leaves the byte (D)"; that file belongs to the evidence-writer commit `2aba1fc` and was not edited here.

## 7. Evidence summary

| Item | Status |
|---|---|
| Normal path, flags (`0x2000000` / `0x8000000` UI, `0x1000000` apply) | D |
| Apply skips the height analysis and trusts the wire byte | D + E |
| Ordinary-land rule and result byte | D + E (0 / 43,221 mismatches) + P (99.77 % of shipped placements) |
| Threshold 32 and initialisers | D + E |
| `+0x6c` compatibility rows; `word14` source | D + E |
| Coast rule | D + E (`ship-founding.md` 10.4; here again inside the full function) |
| Class `0x3000`, flag bit `0x10` coverage | U, explicit unsupported |
| Growth leaves the height byte | D (bounded) |
| Real build click in the running original | NOT_RUN |
