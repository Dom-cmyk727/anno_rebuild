# Ordinary land roads: what the original does when a road tile is built or removed

**Status:** MAP-ROAD-1 R1, 2026-11-02. Evidence for the contract [`docs/sim/road-placement.md`](../sim/road-placement.md). It links, and does not repeat,
[`ship-placement.md`](ship-placement.md) (the island function `0x1007c790`, the type slots, the compat table), [`transport.md`](transport.md) section 3.4.13/3.4.17 (the tile record and its walk code/mask),
and [`ship-founding.md`](ship-founding.md) 10.2 (acceptance step 7).
Labels: **D** read from disassembly (earlier notes, addresses quoted); **E** reproduced by emulating the original instructions (`tools/placement_emulate.py`, unicorn; not a run of the game);
**P** file evidence (`Anlagen.dat`, `Scenes/**`); **U** unread/unknown; **NOT_RUN** needs the running original.

## 1. The inventory and the chosen family (P)

`python3 tools/placement_ref.py roads <game dir>` (output kept in the report; `Anlagen.dat` SHA-256 `b2317a5c...d75`, see `ship-placement.md`):

| PRODTYPE | flag word | types | in the family |
|---|---|---:|---|
| `STRASSE` (193) | `0x4186` | 13 (2500-2503, 2520-2524, 2530-2533) | **yes** |
| `STRASSE` | `0x1186` | 8 (2505-2508, 2525-2528) | **yes** |
| `RAMPE` / `RAMPELOW` / `RAMPEHIGH` | `0x4386`, `0x4186` | 4 + 2 + 2 | no (approaches to bridges) |
| `BRUECKE` | `0x4386`, `0x1386` | 12 + 3 | no (over water) |
| `PIER` | `0x4087`, `0x2087` | 2 + 11 | no (coast) |

Every `STRASSE` type is 1x1 (HEAD bytes 6, 7), has no goods cost, a gold cost in HEAD u16 `[8..9]` (cobblestone 10, dirt road 5), HEAD walk pair `[0x2f],[0x30]` = (0, 3) and a zero WALKMASK (`transport.md` 3.2, 3.4.13).
The two flag words differ only in the class bits: `0x4000` (the road class, accepted at once by the island function) and `0x1000` (the slope class). The family is chosen by PRODTYPE `STRASSE`, then by class bits;
it is facts of the parsed type, not an id list. The ids the UI places for a drag, and any neighbour-dependent variant choice, are **U** (section 6).

## 2. What the original validates for a road (D + E)

The apply call (`AnnoFrame 0x100589a0` mode 6, `ship-placement.md` section 1) is the same for a road as for a house: `0x10058890` (every footprint tile neutral or the commanded city, at least the owner), `City::Build 0x100276a0`
(type flag word bit `0x80` is set for every `STRASSE` word, so the owner-tile loop `0x100277c8` and affordability `0x1002787d` run), `City::Place 0x100279a0` (charge, create, tile write). The island function `0x1007c790`:

* **Bounds** (step 2): `x >= x0`, `x + fw <= x0 + W`, same for y (D, E: vectors `road_out_of_island`).
* **Existing objects** (step 4, type slot `+0x6c` `0x10008920`): the placed group is `G(193) = 0xc1`; row 5 of the compat table (`ship-placement.md` section 4): an existing `PRODTYPE 0xc6` (198, `BRUECKE`) is compatible only with a placed `0xc6`; otherwise compatible iff the existing group is `0xc1`
  (a `STRASSE`, `RAMPE`, `RAMPELOW`, `RAMPEHIGH` or `PIER` piece); an existing type with flag `0x20` (trees, stones, ruins) is always compatible (E: 43,221 compared cases agree; road vectors below).
  So the original lets a road replace a road of any `STRASSE` id, a ramp or a pier piece, and scenery, and refuses a house, any group other than `0xc1`, and a bridge piece.
* **Height analysis** (step 6, slot `+0x68` `0x10008f90`): skipped on the apply path (`0x1000000`); the original then tests the **wire** byte (`ship-placement.md` section 2, E). The analysis a preview runs: `F & 3 == 1` returns the average of four corners; otherwise per cell `cellmin = min(4 vertices) & 0xf0`,
  mode `F & 3`: `0` max of cellmin, `2` min of cellmin, `3` `mx & 0xf0` (mx = raw window maximum), `1` -1; the class test `A & 0x1000004 == 4` then runs by `A & 0x7000`: **`0x4000`: no test** (`0x10009199 jne 0x100091c6`), **`0x1000`: `mx - min(cellmin) > 0x10` returns -1** (D, E).
  Both `STRASSE` words have `F & 3 == 2`: the result is the minimum cell minimum, which for the 1x1 road is `min(4 vertices) & 0xf0`.
* **Acceptance** (step 7, `0x1007cbc2`-`0x1007cc26`): class `0x4000` is accepted at once (the byte is not compared), class `0x1000` needs `rec[8] >= 32`. **So the original has no terrain refusal for a `0x4000` road at all**
  (a sea tile of the island rectangle that no city owns is neutral, `0xf`, which `City::Build` accepts): the refusal a player sees comes from the UI cursor, which is **U** here (the road cursor class was not read).

Emulation (E): `python3 tools/placement_emulate.py vectors` runs the real bytes of `AnnoWelt 0x1007c790` and `AnnoGame 0x10008f90/0x10008920` on the 74 vectors of `fixtures/synthetic/sim/placement_vectors.txt`: 72 executed, **66 compared, 0 mismatches**, 8 not compared (reference-unsupported classes: executed, original returned accept for a bridge/ramp/single-quad type, as expected for an unsupported class).
The 24 road vectors include every class/mode/slope/floor edge below; where the rebuild deliberately differs (adaptations R-A2/R-A3), the vector carries an `original` line that the emulator reproduces. The pre-existing random sweep covers the road words `0x4186`, `0x4386`, `0x1886` (`ship-placement.md` section 3).

## 3. The tile write and the clear (D)

`0x10082040` (ordinary path; `transport.md` 3.4.17): id word, dx/dy/rotation in byte 2, phase in byte 3, anchor bit, height byte (second dword low byte, `byte[P+8]`), walk code and mask. For a `STRASSE` tile the type's HEAD pair (0, 3) encodes **code 0, mask 0 for every rotation**
(`0x100825b7`, `0x100825d9`, encoder `0x100091f0`, getter `0x10007dc0`; `transport.md` 3.4.13): layers 0 and 3 admit code 0 at cost 10, so the cart's search reads a road tile as walkable and any non-road tile (reset code 2) as walkable for layer 0 only.
No neighbour tile is written by the placement function (it reads and writes only the footprint's records; `transport.md` 3.4.17 lists no neighbour store), so **a road's connection to a building is not a stored property**: it is derived by the search from the codes.
Removal: `RemoveAt 0x10028fd0` -> `0x10082c30`: the clear arm writes id 0, byte 2 = 0, code 2, mask 0 and keeps the height byte (`transport.md` 3.4.17 "Removal"). The lookup `0x1008cad0` returns the type for the id on the named tile; for a road (group `0xc1`) the list-selection switch at `0x10029014`-`0x10029051` assigns `ebx = 0`
(its constants are the groups up to `0xa0`; `0xc1` is above), so **no building list is touched and nothing is refunded**: the original has no refund for a tile object (D for the switch range; the byte table `0x10029484` was not re-read, so "ebx = 0 for group `0xc1`" is the reading of the compare against `0xa0`, **U** for the table).

## 4. Shipped placements (P)

`placement_ref.py roads`: 42,766 `STRASSE` items in `Scenes/**` (32,808 of class `0x4000`, 9,958 of class `0x1000`).

| Measure | class `0x4000` | class `0x1000` |
|---|---:|---:|
| stored height equals the rule's byte (mode 2) | 32,767 (99.87 %) | 9,956 (99.98 %) |
| differs (hand-edited scenario content) | 41 | 2 |
| byte >= 32 | 32,803 | 9,958 |
| byte < 32 (sea or beach) | **5** | 0 |
| analysis rejects (slope > 0x10) | n/a (no test) | 0 |

The byte of the rule is the byte the shipped files store, and a road below level 32 is a rarity (5 of 32,808). This corroborates the byte's meaning and is the evidence for the level floor of the adaptation R-A2; it is not a replay of the game.

## 5. Evidence matrix

| Item | Status |
|---|---|
| Apply path, owner rule, bounds | D + E (shared with `ship-placement.md`) |
| Compat rows for group `0xc1`, `0xc6` | D + E |
| Height byte of a `STRASSE` (modes 0/2/3), class `0x1000` slope test | D + E (road vectors) |
| `0x4000` accepted at once; `0x1000` needs >= 32 | D + E |
| Tile write (id, dx/dy, rot, height byte, code 0 / mask 0) | D (`transport.md` 3.4.13/3.4.17) |
| Clear arm; no refund for group `0xc1` | D for the clear arm; the refund reading is D with the table **U** |
| Shipped bytes equal the rule | P |
| Which road id the UI places; neighbour variant choice | **U** |
| The road cursor's own terrain test | **U** |
| Real road build/remove click in the running original | NOT_RUN |

## 6. Unknowns, with the next address

* **U-R1 UI id/variant choice.** The ids 2520-2524 (and 2500-2503, 2530-2533) of class `0x4000` and 2525-2528 (2505-2508) of class `0x1000` look like shapes/slope variants of one road; whether the UI places one id and the game rewrites ids by neighbours, and where, was not read. The rebuild places the id the command names and writes no neighbour. Next address: the AnnoFrame road cursor (`ui-and-placement.md`), the `0x1000`/`0x4000` choice by slope.
* **U-R2 the road cursor's terrain test.** The island function refuses nothing for a `0x4000` road; the cursor must (a player cannot build a road into the sea). The rebuild's level floor R-A2 stands in for it, evidenced by the shipped data only (section 4).
* **U-R3 the refund table** `0x10029484` (above) and the second `+0x5c` jump table `0x10029540`.
* **U-R4** a road over a ramp/pier piece: the original accepts (same group); the rebuild refuses (R-A3); what the original UI does is unread.
