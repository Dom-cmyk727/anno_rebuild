# `AnnoWelt.dll` — transport: the market cart and the workshop carrier (T24)

**Status:** first pass (2026-10-23), static analysis with `tools/disasm.py` (capstone); nothing of the disassembly is committed. Module `game/app/AnnoWelt.dll` (GOG 2.0.0.5, image base `0x10000000`); figure type data from `game/app/AnnoGame.dll`. Labels: **D** = read instruction by instruction, **likely** = inferred from the code around it or from the files, **open** = not read. Counts marked P come from the shipped files (`Anlagen.dat`, `FigurenHeader.dat`). This note continues `production.md` §4.6 and §5 and answers T24 (a)–(e).

## 1. Jobs are three-step task lists (a)

- **Factory** `0x1003b0e0` (figure slot `+0xa4`, `job − 0xd` through the byte table `0x1003b480` and the jump table `0x1003b450`): job `0x33` (`0x1003b11a`) and job `0x34` (`0x1003b16d`) both allocate `0x48` bytes and run the base constructor `0x1005d450(this, this + 0x34, this + 0x3c, this + 0x44)`. **D.** The three sub-objects are the job's **steps**; the constructor links them as a list of three 12-byte nodes at `+0x10`, `+0x1c`, `+0x28` (`{prev, next, step}`), head `+4`, tail `+8`, current `+0xc` (`0x1005d46d`–`0x1005d4e8`). **D.**
- **Main vtable** (both jobs share it: `0x100c45e8` for `0x33`, `0x100c4568` for `0x34`, equal from `+0x00` to `+0x54`): `+0x30` = `0x1005b6c0` **run**: if there is a current step, call its slot `+0xc` (start) with `(figure, t)`; a step that answers 0 cannot start: `+0x4c` = `0x1005d380` picks the next step (the tail node `+0x28` when the failing step's type is 6, 9 or 10, else none) and **run** repeats with it. `+0x34` = `0x1005d3c0` **arrival**: with no current step it starts at the head; a figure without a home (figure slot `+0x1c` = 0) aborts the job (`+0x38`, current = none); else the current step's slot `+0x10` (arrival) runs, and a step of type 9 passes its data to the next step (slot `+0x1c(1, &v)` → next slot `+0x18(1, &v)`: the target tile, `0x1005be80` / `0x1005be60`); then current = next. **D.**
- **The figure drives the job** (figure tick `0x10034760`, `production.md` §5.4 and `annowelt.md` "Unit registry"): an action ends when its remaining amount `+0x3c` reaches 0; then the tick calls, in this order, slot `+0x18c` (an action of its own), a route step (`+0x17c` = `0x100343c0` while a route `+0x50` is set), the job (`+0x178` = `0x10034380`: job **arrival**, then job **run**; a run that returns 0 deletes the job, `+0x20` = 0) or, with neither route nor job, the end path `+0x174` = `0x1003af00`. The loop goes on while time is left in the advance: a step that starts no action is finished in the same advance (the next pass finds `+0x3c` = 0 again). **D.**
- **Step types and bodies** (slot `+0` returns the type; slots `+0xc` start, `+0x10` arrival, `+0x14` abort):

| step | object | type | start | arrival | abort |
|---|---|---|---|---|---|
| A cart: find a producer and walk there | job `+0x34`, vtable `0x100c45c0` | 9 (`0x1005cae0`) | `0x1005ad10` | `0x1005f710` (returns 1) | `0x1005a760` |
| A carrier: find a source and walk there | job `+0x34`, vtable `0x100c4690` | 9 | `0x1005a530` | `0x1005f710` | `0x1005a760` |
| B load | job `+0x3c`, vtable `0x100c4668` | 10 (`0x10056320`) | `0x100551b0` | `0x100551f0` | — |
| C walk home and unload | job `+0x44`, vtable `0x100c4640` | 4 (`0x10057ad0`) | `0x10057ae0` | `0x10057bd0` | — |

  **D.** The step-type codes 7, 9, `0xb` of `production.md` §5.1 are step types of other jobs; for these two, step A has type 9 and its failure jumps to step C (`0x1005d380`: type 9 → the tail node).

### 1.1 Step A of the cart (`0x1005ad10`, the market cart, job `0x34`)
1. **Area** (D, `0x1005ad29`–`0x1005adbe`): the home building's footprint `(x, y, w, h)` (building slot `+0x78(&rect)`) and the radius `r` = the type's byte `+0x65` (type slot `+0x78` = AnnoGame `0x10007d90`, `HEAD[0x11]`, the action radius). The rectangle starts at `x + (w − 1) / 2 − r`, `y + (h − 1) / 2 − r` (division towards zero) and is `2r + 1 + (w − 1) mod 2` wide and `2r + 1 + (h − 1) mod 2` high: a square around the footprint's centre, not the footprint grown by `r`.
2. **Candidates** (D, `0x1005adc9`–`0x1005ae06`, filter `0x1005aca0`): the home city's query (city slot `+0x60`, which goes to the city's building index `+0x144`, slot `+4` = `0x10023ea0`) lists the city's buildings of the lists under mask 8 (the production list, `ListLayout::lookup_mask`) in that rectangle, so only the home city's buildings are candidates; the filter keeps a building whose `+0xa4(output goods 0)` or `+0xa4(output goods 1)` is at least 256 (1 t). The list is a 256-entry stack array with no bound test (`[ctx + 0x404]`); more than 256 candidates would overrun it (none in the shipped scenarios is known). No candidate → step A answers 0 → the job goes to step C.
3. **Goals and search** (D, `0x1005ae09`–`0x1005ae7f`): the figure's searcher (figure slot `+0xc8(0)`, §3) is set up; every candidate registers its entrance points as goals (building slot `+0xf8(figure type, 0, searcher)` = `0x10072be0`); the callback `0x1005a900` with the context `{goods, city slot +0x18, city, chosen}` is installed (searcher `+0x7c`) and the search runs inside figure slot `+0xcc(0, 4, searcher)`.
4. **The callback** (D, `0x1005a900`): the node's position → a world tile (`__ftol` of the two floats, `0x1005a914`/`0x1005a926`, then world slot `+0x170`); the object of kind 8 on that tile (none → go on). For output `g` = the building's output goods 0, then 1 (a 0 output skips the second): only if the **city capacity** (city `+0x78`) is above the **city stock** of `g` (city `+0x7c(g)`):
   - goods 1–25 (`0x1005a9b3`–`0x1005a9ca`): `cap` = the producer type's output-0 capacity (type `+0xc0` = `+0x208`), `q = 3·cap / 4` (towards zero), `score = ((avail − q) << 7) / (cap − q)` with `avail` = building `+0xa4(g)`;
   - other goods (`0x1005aa93`): `score = (avail << 7) / cap`, taken only with `avail ≥ 256`;
   - if the searcher's best score (`+0x8c`) is below `score` (strict): the node becomes the best (`+0x90(node, 4)`), the best score `score` (`+0x88`), and the context takes `g` and the building; then, if `score ≥ 128` and (goods 1–25 only) `2·cap ≥` the city stock of `g`, the search stops (`+0xb0`) and the callback answers 0.
   The callback answers 1 to go on. **The best score starts at 0 for every search** (the run `0x10003d90` writes `[+0x444] = 0`, `0x10003dbf`): a producer of goods 1–25 below 75 % fill (score ≤ 0) is never chosen, and a score of 0 is never taken. **D** (answers the "initial best score" open item of `production.md` §5.2).
5. **After the search** (D, `0x1005ae82`–`0x1005aef4`): the walk starts (figure slot `+0xcc` returns 0 when there is no path; then step A answers 0 and the job goes to step C). With a path: step A keeps the chosen building's tile (`+4`/`+6`, building slots `+0x54`/`+0x58`), the cargo (figure slot `+0x84(0)` = the cargo object at figure `+0x6c`, `0x1003b6d0`) takes the goods (cargo slot `+0`), and the building **reserves** the figure type's capacity (figure slot `+0x78` = the type, AnnoGame type slot `+0x84` = `0x1000fb50` = `+0xf2` = `LOGIC[0x11] << 8`): `reserved = building +0xb4(g, capacity)`, kept in the cargo (slot `+0x1c`). So the reserved amount is `min(capacity, avail)` (§1.4).
6. **Abort** (`0x1005a760`, D): the building on the step's tile (kind `0xa`) un-reserves the cargo's reserved amount (building `+0xb8(goods, reserved)`).

### 1.2 Step B: load (`0x100551b0` / `0x100551f0`)
- **Start** (D): a wait of the figure type's float `+0x30` (type slot `+0x64(1)` = AnnoGame `0x1000fd10`; `+0x30` = `LOGIC` f32 `[4]`, 1.0 when not above 0, `0x100101f4`–`0x10010211`) through figure slot `+0xe0` = `0x10035260` (remaining = the duration, rate 0.01 = `0x3c23d70a`, no movement, action code `0x13`). P: market cart 220 waits 1.0 (500 ticks, §4), carrier 203 waits 2.0 (1000 ticks).
- **Arrival** (D): the building of kind `0xa` on the step's tile (none → nothing, the step is done); `building +0xb8(goods, cargo reserved)` un-reserves; `amount = building +0xa8(goods, figure type capacity)` removes and returns what it took; the cargo's amount = `amount` (cargo slot `+0xc`). The load takes up to the **capacity**, not the reserved amount: what the producer made since the reservation is taken too, but never another figure's reservation (§1.4).

### 1.3 Step C: walk home and unload (`0x10057ae0` / `0x10057bd0`)
- **Start** (D): if the figure's tile (position / 1.0, `__ftol`) lies inside the home's footprint (`x ≤ fx < x + w`, `y ≤ fy < y + h`; bytes of the footprint rect, `0x10057b2d`–`0x10057b66`): answer 1 with no action (the arrival follows in the same advance). Else: the searcher with the home's entrance points as goals (`home +0xf8`), the callback `0x10054240` (the first goal reached becomes the best node and the search stops), the walk (figure `+0xcc(0, 4, searcher)`); its result is the step's answer (0 = no path: the job ends with the cargo on board, the figure takes the end path).
- **Arrival** (D): with a home and a cargo goods `g ≠ 0`: `home +0xc0(g, cargo amount)`. For a market that is the city's add (`0x1006b7d0` → city `+0x80` = `0x1002da50`, clipped at the capacity, `production.md` §5.3); what the clip leaves is lost with the figure (the cargo is not read again; the figure ends). For a workshop it is the input store (`0x100683f0`: input 1 when `g` is the type's input goods 1, else input 0; added without a clip).
- **Then** the job has no step left: run answers 0, the job is deleted, and the next pass of the figure tick takes the end path `0x1003af00` (the last walk of 0.0 inside the home's footprint, the counter release `0x1003b6e0`, the Remove; `annowelt.md`).

### 1.4 The buildings' side (D)
| class | output goods `+0xa0(i)` | available `+0xa4(g)` | reserve `+0xb4(g, a)` | unreserve `+0xb8(g, a)` | remove `+0xa8(g, a)` | add `+0xc0(g, a)` |
|---|---|---|---|---|---|---|
| workshop (`0x100c8010`), mine (`0x100c8990`) | type output i (`0x100253d0`) | `out − reserved` (`0x10068880`, `g` not read) | `reserved += min(a, out − reserved)` (`0x10068460`) | `reserved −= min(reserved, a)` (`0x10068490`) | `take = min(a, out − reserved)`, `out −= take` (`0x10068430`) | workshop: input store (`0x100683f0`); mine: none (`0x100a3930`) |
| farm (`0x100c8ae0`), fur trapper (`0x100c2bf0`) | as above | as above | as above | as above | as above, plus the fill-stage graphic (`0x1006ee80`, slot `+0x11c`) | raw store `+0x1c` (`0x1006ee70`) |
| small farm (`0x100c8d60`) | as above | per line: line 1 if `g` = output 1 (`0x1006f9a0`) | per line (`0x1006fa70`) | per line (`0x1006fad0`) | per line (`0x1006f9e0`) | `0x1006f960` |
| weapons workshop (`0x100c8150`) | as above | per line: line 1 if `g` ≠ output 0 (`0x1006a4a0`) | `0x1006a540` | `0x10068f50` | `0x1006a4e0` | `0x10068ee0` |
| market, Kontor (`0x100c86c8`, `0x100c2740`) | `0x8e` (`ALL`) for i = 0, else 0 (`0x10025470`) | the city stock of `g` (`0x10025490`) | 0, nothing (`0x100a3930`) | 0 | the city's remove (`0x1006b7f0` → city `+0x84`) | the city's add (`0x1006b7d0` → city `+0x80`) |

Fields (D): workshop, mine, farm: output `+0x18`, reserved `+0x1a` (u16, 8.8); small farm `+0x18 + 2i` / `+0x20 + 2i`; weapons workshop `+0x18 + 2i` / `+0x1c + 2i`.

### 1.5 The market tick (`0x1006b6d0`, D)
`(u16)(t − last) ≥ 500` → `last += 500`; group 0: the carts out are the non-null pointers of `+0x20`/`+0x24`; while fewer than `max_figures(0)` (type slot `+0xb8(0)`), one cart is sent: figure type `+0xb4(0, 0)` (group 0, entry 0), base slot `+0x118(0x34, type, t)`. Group 2 (`+0x28`/`+0x2c`, `max_figures(2)`): the figure is entry `v mod count(2)` of group 2, `v` from world slot `+0xb4` through city `+0x18` and `+0x18` (not read; likely a draw) — not modelled (no shipped market type has a group-2 figure: P, PROD3 of 1301–1305). P: 1301/1302 send one cart (max 1), 1303–1305 up to two; all of figure 220.

### 1.6 Sending (`0x10072d20`, D)
Refused while the building's flags bits 4/5 are set (`+0xc & 0x30`, `0x10072d26`–`0x10072d34`) or without a reachable entrance (`0x10072b50`, not read: likely the first entrance point whose tile is walkable); the registry (world slot `+0xa0`) creates the figure at that entrance point; building and figure attach to each other (slot `+0x28` both ways, the city too); the figure makes the job (`+0xa4`) and takes it (`+0xac`; that this runs step A at once is likely).

## 2. The workshop carrier (job `0x33`) (b)
- **Step A** `0x1005a530` (D): the same area as the cart (the workshop's footprint centre ± its type radius, §1.1). The candidates are the home city's buildings under the list mask **`0xa`** (not 8: the production and the market lists, `0x1005a60a`) in the rectangle that pass the filter `0x1005a500` = the workshop's slot `+0xd4(candidate, 0) > 0`.
- **Workshop slot `+0xd4(source, &goods)`** = `0x100728f0` (D): for each input `i < type +0xdc(1)` with goods `g = type +0xb0(i)`: if the source's output 0 is `0x8e` (`ALL`: a market or Kontor, §1.4) or equals `g`, and the source's `+0xa4(g) ≥ 256`, and the workshop's priority `p = +0xd8(g)` (`0x10068500`: `0x80` below 30 steps of input, `0x60` below 60, else −1, `production.md` §5.1) is above 0: `score = p + avail · 192 / cap` with `cap` = the source type's output-0 capacity (type `+0xc0`); the source's output 1 is checked the same way when it equals `g`. The best score wins (start −1) and its `g` is returned. **So the carrier fetches from the warehouse (any building whose output 0 is `ALL`) and from producers of its inputs, and only for an input with fewer than 60 steps in stock** (answers `production.md` "Open": "warehouse only, or producers"). For a market, `cap` is its type's `+0x208` (PROD3 `[4] << 8`; P: 1301–1305 hold a PROD3 of 68 bytes; whether the value is 0 there, which would divide by zero, is not checked).
- **Its callback** `0x1005a400` (D): the building of kind `0xa` on the node's tile; `v = home +0xd4(building, &g)`; if `v` is above the best score: best node, best score `v`, the context takes `g` and the building; if the building's `+0xa4(g)` is at least `3·capacity / 4` of the **carrier** (the context's `+4`, `0x1005a6a7`–`0x1005a6b3`), the search stops.
- **Then** (D): the reservation `building +0xb4(g, carrier capacity)` (a market reserves 0) and the same steps B and C as the cart (B removes up to the capacity from the source; C adds the cargo to the workshop's input store, `0x100683f0`).
- **Not read:** the workshop tick's send (`production.md` §4.1: the tick sends whenever `+0xd8` is non-zero); whether a carrier is sent at all when no input is below 60 steps (the filter then lets nothing pass and step A fails → step C at home).

## 3. The search (c)
- **The searcher is a world service**: figure slot `+0x190` = `0x10035d50` → world slot `+0x20(0xc00, 0)` = `0x100aee00` → world `+0x6aa8` (one object, made at `0x100aab35` by `0x10006310`, vtable `0x100c1424`, about `0x3074` bytes). Most of its slots forward to the engine at `+4` (vtable `0x100c14e4`). **D.**
- **Set-up for a figure** (figure slot `+0xc8` = `0x1003acc0`, D): bounds = the figure's island (`+0x1c`) through engine `+0x50` = `0x10002b80` (the island as the cell provider `+0x434`, the island rectangle as the search grid, origin `+0x4e8`/`+0x4ec`, the grid in blocks of 4 × 4 cells, `(w + 3) / 4`); the **walking layer** = the figure type's byte `+0xff` (type slot `+0xac` = AnnoGame `0x1000faf0`; `LOGIC[0x10]`, `0x1001025a`–`0x10010262`) selects the table `0x100d1140 + 0x1c00 · layer` (`.data`, engine `+0x430`, `0x1000dd20`); the **area** = the home's footprint centre and the type radius through engine `+0x54` = `0x10002a60`: a disc built with integer midpoint-circle steps (half-widths per row at `+0x468`, `r ≤ 3` widened by one cell on the outer rows, an even footprint widening the centre to two cells); cells outside the disc are blocked when a block is filled (`0x10003780`, `& 0xfc07 | 7`). The start: inside the home's footprint the type's entrance points (type slot `+0x98(rotation)` = AnnoGame `0x100084c0`, from `ANLAGEN/ENTRANCE`, below), else the figure's own position (engine `+0x68`).
- **Cells**: blocks are made on demand (`0x1000b790`) and filled from the island's tiles by island slot `+0x128` = `0x10091fa0(mode, …)`; figures pass mode 0 (`0x1003ad74`: `+0x50(island, &rect, 0, 0)`), whose cell for a tile is `table[mask] | byte << 16`: the 9-bit field at bits 16–24 of the tile record's second dword mapped through the u16 table `0x100c1af8`, and the byte at bits 8–15 moved unmapped to bits 16–23 (`0x1008ff70`, §3.1 (b)). Each tile is a 3 × 3 grid of sub-cells; path steps are counted in thirds of a tile (§4). **D** for the structure; **likely**: the 9-bit mask is the walk mask of the tile's object (every `ANLAGEN` entry has a `WALKMASK` record, P 921 of 921; the writer is not read); **open**: what the layer tables `0x100d1140` hold (likely static `.data` with image content, §3.1), so which tiles a cart (layer 3, P: figure 220 `LOGIC[0x10]` = 3) and a carrier (layer 0, figure 203) may enter (OQ-160). **Answered (T30b.3m–3o, T30b.8):** the tile byte indexes the layer block, the mapped mask supplies the geometry bits (§3.4.12), `state.md` §5.9.
- **Goals**: building slot `+0xf8` = `0x10072be0` (D): unless the building's flags bits 4/5 are set (masked with the caller's argument), its type's entrance points (rotated, plus the building's tile) go to engine `+0x70` (at most 256, `0x10002df0`). P: 326 of 921 types have an `ENTRANCE` record (8–32 bytes: 1–4 points of two f32), all on sub-cell centres (`k/6` with `k` odd); markets 1301–1305 have 2–4, the weaver 1001 two, the grain farm 811 three.
- **Order and stop**: the engine calls the callback for the goal nodes it reaches (`0x10003d90` and its loop); a callback answer 0 or slot `+0xb0` (`0x10003210`, `+0x438` = 0) stops it; slot `+0x90(node, 4)` = `0x10003030` sets the best node unconditionally for a priority ≥ 4, `+0x94` returns it. **D** for these slots; **likely**: goals are reached in order of path cost (the cart's early stop and the home walk's "first goal" only make sense so); **open**: the expansion itself (neighbour order, the cost of a step, the tie order), which is the bulk of `0x10003d90`, `0x10004890`, `0x10005890` (OQ-161). **Answered (T30b.3i–3o, T30b.8):** §3.4.7–§3.4.14, order and ties §3.4.16 (a); goals are tested in pop order (lowest score first), `state.md` §5.9 clauses 4, 5, 7.
- **The road rule (OQ-103)**: what the code requires is the layer table, which is open. **(Decoded, T30b.8: the cart's layer 3 admits roads and building cells, not open ground; the carrier's layer 0 admits roads, open ground and buildings, `state.md` §5.9 clause 2; the three OQ-103 clauses follow from it statically and wait for the running original, T30b.6.)** The files point one way: carts use layer 3 and carriers layer 0, so their passable cells differ (D: different layers; likely: the cart's layer is the road net, as `production.md` §5.2 and the tutorial series say, and the carrier's is open land, as the series says for wool fetched without a road, V1 03:34).

### 3.1 The walking layers (T27)

Time-boxed RE of OQ-160 (`tools/disasm.py`, `tools/xref.py` on `game/app/AnnoWelt.dll`; corrected after the round v review). What is now **D**, what is **likely**, what is still **open**:

- **(a) The table pointer and where it lives. D.** Two setters compute the per-layer table pointer and store it: `0x1000dd20` writes `[engine + 0x430] = 0x100d1140 + 0x1c00·layer` (`lea edx,[eax*8]; sub edx,eax; shl edx,0xa; add edx,0x100d1140`), and `0x1000f010` writes the same value to `[+0x1030]` of its object. So each walking layer owns a `0x1c00`-byte (7168) block; five blocks are used (P: the sixth `0x1c00` bytes after them are all zero). The base-relocation table (`xref.py --reloc`) lists exactly two absolute references to `0x100d1140`, at `0x1000dd32` and `0x1000f022` inside the two setters, and **none** to any address inside the five blocks (`0x100d1141`–`0x100d9d3f`; the next referenced `.data` address is `0x100dcb44`). `0x1000dd20` is held in the vtables at `0x100c1320`, `0x100c1564` (slot `+0x80` of `0x100c14e4`, the engine at world `+0x6aa8` `+4`, §3) and `0x100c1934`; `0x1000f010` only at `0x100c1abc`, another vtable, so `[+0x1030]` is likely a field of another engine class (open).
- **The blocks hold initialised data; that they are not written at run time is likely.** D: `0x100d1140` lies in `.data` (characteristics `0xc0000040` = initialised / read / write) with non-zero image content, and no absolute reference points into the blocks (a), so a run-time write would have to go through the stored pointers `[engine + 0x430]` / `[+0x1030]`. Read so far through the pointer: the reader `0x1000daf0` (below); stores through it were not searched. So "not filled at run time" (which would correct §3's earlier "filled at run time") is **likely**; T30 step 7a settles it (a compare with the running original's memory).
- **(b) The cell value the search reads. D.** `0x10091fa0` dispatches on a mode 0–7 through the table `0x10092190`; mode 0 (the figures', §3) goes to `0x10091fba`, which calls `0x1008ff70`. Its loop `0x10090050`–`0x1009007b` reads each tile record's second dword, takes bits 16–24 (`shr 0x10; and 0x1ff`) as the index into the u16 table `0x100c1af8` (`.rdata`; its only reference is the load at `0x1009005f`), moves bits 8–15 to bits 16–23 (`and 0xff00; shl 8`) and stores `table[mask] | byte << 16` as the cell. ~~So the layer blocks are indexed (somehow, open) by the mapped value, not by the raw 9-bit mask.~~ **Corrected T30b.3m (§3.4.12):** the tile byte supplies the admission/cost index; the mapped mask supplies geometric edge bits.
- **Image statistics of the blocks (P; their meaning open).** `0xff` bytes per block: layer 0 = 316, layer 1 = 409, layer 2 = 561, **layer 3 = 75**, layer 4 = 613; distinct byte values per block: 50, 53, 74, 42, 87. With that many distinct values an entry is not a plain yes/no flag, so the `0xff` counts do not rank the layers (with `0xff` = passable layer 3, the market cart's (P), would be the most restrictive; with `0xff` = blocked the least). They say nothing about whether the stand-in (layer 3 → roads only, every other layer → open land) has the right shape.
- **Open, and the next address.** ~~The **meaning** of a block's entries (how a cell value indexes the `0x1c00` block and what the entry says to the search) is not decoded.~~ **Decoded T30b.3m (§3.4.12):** through the Y+4 interface, `0x1000dd20` sets Y+0x434 to the selected layer block; `L[code]` is the admission mask, `L[0x400+code*8+dir]` the default step cost. The rest of this bullet records the earlier investigation. The search functions `0x10003d90`–`0x10004867` and `0x10004890`–`0x1000588f` do not access `+0x430` (a displacement sweep; their `+0x42c` accesses at `0x10004831`/`0x1000483e` are a counter). The direct reader of the pointer is **`0x1000daf0`** (held in the vtable at `0x100c18b0`): it reads `[engine + 0x430]` at `0x1000db0e` and adds a u16 taken from its argument; a twin with a frame shifted by 4 bytes is `0x1000ed40` (vtable `0x100c1a30`; `+0x430` at `0x1000ed50`, `+0x434` at `0x1000ed77`). Those two and their callers are the next addresses (T30 step 2). The RE of OQ-160 stays open (the owner decided on 2026-10-24 how it is settled: T30), and the stand-in (road-flag tiles for layer 3, open land otherwise) is kept; the statistics above neither confirm nor refute it.
- **(c)** What a tile *without* an object carries in those bits (water, mountain, open land) was not read here; it is the island loader's tile-record set-up (`docs/re/islands.md`, the tile record's second dword; T30 step 5). Rivers are not such tiles: they are `FLUESSE` tile objects (step 7, OQ-170).

## 4. Movement (d)
- **Time**: the figure tick turns the ticks since the figure's stamp into time units: `u16(t − stamp) · 0.2f` (`0x1003476a`–`0x10034784`, constant `0x100c3270`). An action moves `time · rate` along its remaining amount `+0x3c` and the position by `time · velocity` (`+0x30`/`+0x34`/`+0x38` added to `+0x24`/`+0x28`/`+0x2c`); a longer step is cut at the remaining amount and the rest of the time goes on to the next action in the same call (`0x100347b1`–`0x10034808`). **D.**
- **Waits**: rate 0.01 (`0x10035260`): a wait of `d` lasts `d / 0.01 / 0.2 = 500·d` ticks (the last walk's 15.0: 7500 ticks). **D.**
- **Walking**: figure slot `+0xcc` = `0x10034df0` takes the path the searcher builds (engine `+0xa4` through `0x10005890`: a list of `{direction, count}` byte pairs, `count` in thirds of a tile, `0x100c1298` = 1/3) and makes one segment per pair (`0x1002fde0`): direction 0 = −x, 1 = −x+y, 2 = +y, 3 = +x+y, 4 = +x, 5 = +x−y, 6 = −y, 7 = −x−y (table `0x1002ff20`); the length is `count / 3` tiles, times `√2` (`0x100c3130` = 1.41421354f) on a diagonal; the rate is the type's speed (type slot `+0x60(1.0)` = AnnoGame `0x1000fb10`: `HEAD` f32 `[4]` at type `+0x10`); the velocity is the rate along the axis, `rate · 0.70710677f` per axis on a diagonal. A segment is walked one tile at a time (`0x100343c0`: lengths above 1.0 are split; the height axis `+0x28` follows the terrain, display only). **D.** P: the cart 220 walks 0.016 tiles per time unit = 0.0032 per tick = 1.6 tiles per second; the carrier 203 0.012 (1.2 tiles per second). Without a path (`+0xa4` gives no step) the walk counts as done when the target is within 1/6 tile (`0x10034e96`–`0x10034ecb`, `0x100c1294`). **D.**
- **The integer design** (no float in state or in a hashed path; `docs/sim/state.md` §5.9): the figure's position stays `FixedPos` (Q24.8, D13); a walk is a list of tile waypoints; the progress along a segment is a `u32` in 2^-24 tile; the speed of a figure type is precomputed at load as `round(f32(HEAD[4]) · 0.2f · 2^24)` per tick (double precision, as the production floats, `production.md` §8); a diagonal segment is `23726566` (= round(1.41421354 · 2^24)) long. What the original does with x87 floats (the `0.2f` product rounded to f32, the per-axis velocities, the time carried between actions) is approximated: the trip time may differ from the original's by rounding (OQ-162, owner).

## 5. The figure's save record (e)
`TASK`/`ENTRY` (`0x1005b7a0` / `0x1005b9a0`, job slots `+0x3c`/`+0x40`) was not read (no save compatibility in M3); the fields T24 needs are named above from the code that uses them.

## 6. What the rebuild implements (T24 stage 1)
In `src/sim/src/transport.cpp` (`docs/sim/state.md` §5.9 has the pseudo-code):
- the market tick's send (§1.5);
- the cart's three steps (§1.1–§1.3) with the producers' reservations (§1.4) and the market's city stock;
- the **workshop carrier** (§2) for the `HANDWERK` workshops: its filter and callback (`0x100728f0`, `0x10068500`, `0x1005a400`), the reservation and the delivery into the input store. The weapons workshop's carrier (its lines, `0x1006a4a0`) keeps the D12 behaviour and is left to stage 2;
- the walk (§4) in integers;
- a **stand-in search** (OQ-160, OQ-161) over the area's tiles.

The figure type's speed, capacity, layer and load wait come from `FigurenHeader.dat` (`figure_type_from_entry`, `src/sim/src/types.cpp`). A registry without them (the synthetic registries of G1–G5) keeps the earlier default (the figure does not act; D12).

**T27 (stage 2) changed, without touching behaviour:** the tile class of the walk (0 none / 1 road / 2 building) is precomputed per type as `BuildingType::walk_class` (derived, `TypeRegistry::finish`) instead of a per-search `std::map` + layout lookup; §3.1 records the RE of the walking-layer tables (initialised `.data`, two absolute references, the reader `0x1000daf0`; the entries' meaning open, OQ-160). The walk still ignores terrain: river pieces are `FLUESSE` tile objects the stand-in classifies as open land, and open sea carries no object (OQ-170; by the owner's decision of 2026-10-24 the rule waits for T30/T33, no stand-in rule). No hash moved. The real search expansion (OQ-161) and the weapons workshop's carrier (T28) are still open.

**T29 (measurement only, 2026-10-24):** `choose_source`'s diagnostic `TransportStats` gained six T29 counters filled from the search's `parent` array (`frontier` by tile-object type id, `edge`, `home_no_road`, `goal_no_road`, `corner`, `outside_disc`); `TickContext` gained a null-by-default `transport_stats` pointer routed by `World::set_transport_stats` so the scenario-stats test accumulates every search of the run. No walk-rule change, no hash moved; the frontier type ids are T30's first tiles to check (OQ-160).

### 3.2 The named-road puzzle (T30 step 5, 2026-10-24)

**D (`tools/figuren.py` on `Anlagen.dat`, `anno-dump texte` on `Texte.dat`; `tools/xref.py`, `tools/disasm.py` on `AnnoWelt.dll`):**

- **35 types carry `0x4000` in `Anlagen.dat`'s HEAD `flags_word` (kRoadFlag):** 24 named roads, the 2 piers, and 9 BRUECKE pieces without a Texte.dat ANLAGEN name (3955, 3956, 3961, 3990-3993, 3997, 3998). (counts corrected in T30b.1; T30 wrote 36 and 10)
- **The named-road set is larger than the kRoadFlag set.** 37 entries (the two piers included) in `Anlagen.dat` have a Texte.dat ANLAGEN name among {Cobblestone street, Dirt road, Stone / Grand / Wooden bridge, Stone / Wooden pier}. Of the 37, **26 carry `0x4000`** and **11 carry `0x1000` instead of `0x4000`**: ids 2505–2508 (Cobblestone street `STRASSE`, flags `0x1186`); 2517, 2536–2537 (Stone / Wooden bridge `BRUECKE`, flags `0x1386`); 2525–2528 (Dirt road `STRASSE`, flags `0x1186`). Each `0x4186`/`0x1186` pair, and `0x4386`/`0x1386`, differs exactly in bit 14 (`0x4000`) and bit 12 (`0x1000`), and the two bits are never set together across the 921 Anlagen entries. (counts corrected in T30b.1; T30 wrote 36 and 10)
- **Named-road PRODTYPE strings:** `STRASSE` (16 ids), `RAMPE` / `RAMPELOW` / `RAMPEHIGH` (6 ids, ramps onto bridges), `BRUECKE` (10 ids), `PIER` (2 ids). The sim's PRODTYPE table (`types.cpp` `kProdtypeNames`) maps these strings to small numbers; the Anlagen file always carries the string.
- **WALKMASK is 12 zero bytes for every named road** (37 of 37). (counts corrected in T30b.1; T30 wrote 36 and 10) 282 of 921 Anlagen entries carry 12 zero bytes; ~~the rest carry a per-cell u32 mask (one per footprint cell)~~ **corrected T30b.3n (§3.4.13): WALKMASK stores three u32 subcell rows per footprint tile row; bits across each row cover three subcells per tile column.** Named-road walkability therefore is **not stored per cell on the type**: it must come from a different mechanism.
- **The bridge "transition" pieces that DO carry per-cell WALKMASK:** ids 3955 (`BRUECKE`, 24 bytes `3f000000 … 3f000000`, six subcell rows), 3956 (12 bytes `ff010000 00000000 ff010000`, three subcell rows with `0x1ff` / 0 / `0x1ff`), 3961 (`BRUECKE`, 12 bytes `ff0f0000 … ff0f0000`), 3992 (`BRUECKE`, `07000000 07000000 05000000`), 3993 (`BRUECKE`, `3f000000` × 3). These look like the ramps/approaches where the walkable cells are a subset of the footprint.

**Named-road ids by group** (reproduced by `tools/anlagen_roads.py`, T30b.1; one row per contiguous id group, checked against the tool output):

| id range | count | name | PRODTYPE | flags |
|---|---|---|---|---|
| 2500–2503 | 4 | Cobblestone street | STRASSE | 0x4186 |
| 2505–2508 | 4 | Cobblestone street | STRASSE | 0x1186 |
| 2509 | 1 | Stone pier | PIER | 0x4087 |
| 2510–2513 | 4 | Cobblestone street | RAMPE | 0x4386 |
| 2514 | 1 | Cobblestone street | RAMPELOW | 0x4186 |
| 2515 | 1 | Cobblestone street | RAMPEHIGH | 0x4386 |
| 2516 | 1 | Stone bridge | BRUECKE | 0x4386 |
| 2517 | 1 | Stone bridge | BRUECKE | 0x1386 |
| 2518–2519 | 2 | Grand bridge | BRUECKE | 0x4386 |
| 2520–2524 | 5 | Dirt road | STRASSE | 0x4186 |
| 2525–2528 | 4 | Dirt road | STRASSE | 0x1186 |
| 2529 | 1 | Wooden pier | PIER | 0x4087 |
| 2530–2533 | 4 | Dirt road | STRASSE | 0x4186 |
| 2534 | 1 | Dirt road | RAMPELOW | 0x4186 |
| 2535 | 1 | Dirt road | RAMPEHIGH | 0x4386 |
| 2536–2537 | 2 | Wooden bridge | BRUECKE | 0x1386 |

**Likely:**
- Bit `0x1000` is a visual variant of the same `STRASSE`/`BRUECKE` type that doesn't carry traffic (an entrance marker or a decorative crossing). The owner's working rule says "a road connects only at a building's specific entrance tile" (OQ-103), and these may mark non-through pieces. Not confirmed by the game's code read here.
- Real walkability per layer is decided by the walking-layer tables at `0x100d1140` (OQ-160 step 2 open; indexed by the mapped mask + byte, consumed by `0x1000daf0` whose caller stashes the resulting dwords into another object through a vtable method — the yes/no decision lives deeper).
- A group of four ids with equal flags looks like one piece shape in four orientations (orchestrator's reading, round aa); which shape the `0x1000` groups are is for T30b.6.

**Open:**
- **Whether `0x1000` marks a tile walkable or not for layer 3 (the cart) in the real rule.** The 15 172 blocked cart searches of `S01_07_01` reaching Dirt road `2527` (`0x1186`) tell us the stand-in bars it; whether the original bars it too is OQ-171. T30 step 2 (the search's expansion) answers it.
- **What bits `0x0100`, `0x0080`, `0x0004`, `0x0002` mean.** Consistent across road variants; not traced here.
- **Where bit 12 / bit 14 appear in the search's layer-table index.** The cell dword a tile gives the search (`0x1008ff70`, `cell = table[mask] | byte << 16`, §3.1 (b)) is built from a 9-bit mask and a byte of the tile record; the type's `flags_word` bit is not visibly in it. The island loader must write bit 12 / 14 into the tile record somewhere — read where (next address: the island loader's set-tile path, `0x10087b90` and `0x100878d0`, O-54).

### 3.3 The reader of the layer tables (T30 step 2, partial D)

Read `0x1000daf0` (vtable `0x100c18b0` slot `+0`, held in a constructor at `0x1000ebb0`) and its twin `0x1000ed40` (vtable `0x100c1a30` slot `+0`) instruction by instruction. Both have the same shape.

**D:**
- Signature: `(this, u32 cell_ptr)`, called by a virtual dispatch somewhere above.
- Reads **two u16s from the cell**: `p[0]` (into `ecx`, zero-extended) and `p[2]` (into `edx`).
- Reads **two tables from `this`**: `[this + 0x430]` (primary) and `[this + 0x434]` (secondary). The primary is the layer table `0x100d1140 + 0x1c00 · layer` (setter `0x1000dd20`). The secondary setter was not located for the layer-table context (every write to `[reg + 0x434]` in `AnnoWelt.dll` that was xref'd here is on another object type that uses `+0x434` as a signed i32 coordinate — a different class with the same offset).
- Builds a **16-byte stack buffer** of 4 dwords: `[primary + p[0], secondary + p[2], primary + p[0] + 8, secondary + p[2] + 8]`.
- Calls the vtable method `[edx + 0xec]` on **the object `[this + 0x858]`** with **five** arguments, pushed in the order `8`, `cell + 4`, `&stack_buffer`, `0`, `[this + 0x85c]` (so, in argument order: `([this + 0x85c], 0, &stack_buffer, cell + 4, 8)`; the thiscall `this` is `[this + 0x858]`). **Correction by the orchestrator (2026-10-24 aa, re-read of `0x1000db06`–`0x1000db4a`):** the first version of this line listed four arguments and `this + 0x85c` instead of the value stored there; the method also receives a pointer to the dword after the cell (`cell + 4`), so the yes/no may read more of the tile record than the two u16s this function indexes with.
- Returns `ret 4` (one argument).

So this function **does not itself decide walkability**; it builds a 16-byte argument from two tables (indexed by two different u16s) and hands it to a deeper virtual method.

**Likely:**
- The two tables are the walking-layer tables for **two different layers** (the search's "my layer" and some reference layer, or shot vs walk). The reader name hints at a "can-this-cell" test taking primary+secondary.
- Each 7168-byte block is indexed by a value that comes from `0x1008ff70`'s `table[mask] | byte << 16` cell: `p[0]` is the low u16 (the mapped 9-bit mask through table `0x100c1af8`, values up to `0xfff` per the T27 count) and `p[2]` is the high u16 (the byte at bits 8–15 of the tile record's second dword, so `0..0xff`). An index up to `0xfff` fits a `0x1c00`-byte block; the entries are 8 or 16 bytes wide (two 8-byte spans read 8 apart per table).

**Open and the next address:**
- **The vtable method `[edx + 0xec]`** on object `[this + 0x858]`: who implements it and what it does with the 16 bytes. That is the decision point — read it next.
- **The `+0x434` setter in the layer-table context.** `+0x430` has a dedicated setter (`0x1000dd20`); `+0x434` reads as a pointer in `0x1000daf0`, but no obvious setter was found by the sweep. Likely written through a different writer that uses a register other than a literal `0x434`, or set together with `+0x430` by a caller that stores two values. The 5 writes to `[reg + 0x434]` in `AnnoWelt.dll` (`0x100012ce`, `0x100012d6`, `0x10002b9e`, `0x1000db7d`, `0x1000db85`) are on a different class (its `+0x434` is a signed i32 clamped to 0; `0x1000db60` shows the pattern).
- **The constructor at `0x1000ebb0`** installs both vtables `0x100c18b4` and `0x100c18b0` on an object with `[this + 0x85c]` writable. Reading its callers would say what this engine class is.

**T30b.2 update (constructor tracing).** The real constructor start is `0x1000eb40` (`0x1000ebb0` is mid-instruction). It installs vtable `0x100c18b4` at `[this + 0]`, vtable `0x100c18b0` at `[this + 4]`, stores `[this + 0x85c] = eax` (a caller stack value), and stores `[this + 0x14] = edx`. It does **not** write `[this + 0x858]`. An E8 opcode sweep over all executable sections of `AnnoWelt.dll` finds exactly one direct caller: `0x100aabfd: call 0x1000eb40`, with `ecx = eax` = the result of `call 0x100b5f80` (allocation via `0x100b72c3`). No code between the allocation and the constructor call writes `[eax + 0x858]`.

Vtable verification (raw dword reads, addresses only): `0x100c18b0` slot +0 = `0x1000daf0` (the reader), `0x100c18b4` slot +0 = `0x100b4a20` (not the reader). Since `0x1000daf0` is slot +0 of `0x100c18b0` installed at `[original_this + 4]`, the reader's `this` = `original_this + 4`, so `[reader_this + 0x858] = [original_this + 0x85c]` — which **is** written by the constructor (`eax`). But `eax` = `[constructor_esp + 0x14]` reads beyond the constructor's 8 `ret` bytes into the caller's stack frame, an untraceable value statically. The vtable on `[reader_this + 0x858]` = `[eax_value]` is therefore unknown, and the method at `vtable + 0xec` (OQ-175) cannot be read statically. Chain stops at caller `0x100aabfd` → constructor `0x1000eb40` → untraceable `eax`. Next address to read: the value at `[original_this + 0x85c]` at runtime in the running original (Wine memory read, T30b.4).

**Orchestrator's read after T30b.2 (2026-10-24 ae; `tools/disasm.py`, `pefile` on `AnnoWelt.dll`). D unless marked.** The chain does not stop: at `0x1000eb91` the constructor has pushed `ebx`, `esi` and the `0x1c` of its allocation call, so `[esp + 0x14]` is its **second argument**, not a value beyond the `ret 8` bytes. Its only caller is the world's `Init` (`0x100aa910`'s body, the stores at `[esi + 0x6aa8]`, `[esi + 0x6aac]`, `[esi + 0x6ab0]`): `push esi; push edi; call 0x1000eb40` with `esi` = the world object and `edi` the byte stored at `+0x41c`. So:

- **The class (call it X, world `+0x6ab0`, size `0x864`):** primary vtable `0x100c18b4` (47 slots; `+0x50` = `0x1000db60`, `+0x68` = `0x1000dd60`, `+0x80` = the layer-table setter `0x1000dd20`, `+0x84` = `0x1000e800`), secondary vtable `0x100c18b0` at `X + 4` (slot 0 = `0x1000daf0`, whose `this` is `X + 4`). Fields, with `X` as the base: `[X + 8]` / `[X + 0xc]` a size in 8-tile blocks (bounds below), `[X + 0x430]` the layer-table pointer (setter `0x1000dd20`), `[X + 0x434]` / `[X + 0x438]` two signed origin coordinates (`0x1000db60` writes `[X + 0x434] = max(arg, 0)`; `0x1000dd60` subtracts both from a position), `[X + 0x85c]` the world, `[X + 0x860]` a kind code = `arg + 2` written by `0x1000db60`.
- **The method `[edx + 0xec]` is the world's: vtable `0x100ccdd8` (the game world, `annowelt.md`) slot `+0xec` = `0x100abdb0`** (the base vtable `0x100cc92c` holds a pure-virtual stub `0x100b649d` there). It takes five arguments (`ret 0x14`) and dispatches on the first (`[X + 0x860]`, the kind): 9 → `0x100abd10`, 10 → `0x100abc60`, 11 → `0x100abaa0`, each called with `(&rect, dest, 8)`; it returns 1 in every case. **It is not a yes/no.**
- **The handlers copy a map rectangle.** The 4-dword buffer is read as `(x0, y0, x1, y1)`; the handlers step `y` from `y0` to `y1` and `x` from `x0` to `x1` in units of 8 (the `and 0x80000007` / `sar 3` arithmetic), look a chunk up by the byte at `world + 0x130 + …` (chunk index from `[world + 0xec]`, the chunk-row width) and the pointer table at `world + 0x6530 + 4·byte`, and either copy the chunk's data into `dest` (`0x100900c0` for kind 11, `0x10090350` for kind 10) or fill `dest` with a default where no chunk exists: zero bytes for kind 11, `0x08` bytes for kind 10. Kind 9 walks 64×64 blocks (`sar 6`) through `[world + 0x6a9c]` / `[world + 0x6aa4]` and `0x100a4730`. Likely: the three kinds are three per-tile map layers, and the 8-byte stride is the tile record of §3.1 (b).
- **Consequence for §3.3 and §3.6: `0x1000daf0` is not a layer-table reader.** With `this = X + 4`, its "primary table" `[this + 0x430]` is `[X + 0x434]` and its "secondary" `[this + 0x434]` is `[X + 0x438]`: the two **origin coordinates**, not pointers. It reads the two u16 of its argument as block coordinates `(p0, p1)`, builds the rectangle `(ox + p0, oy + p1, ox + p0 + 8, oy + p1 + 8)` and asks the world to copy that 8×8 block of layer `kind` into `arg + 4`. The layer table at `[X + 0x430]` is never touched by it; the "two 8-byte spans from two tables", the "33 cell types × 8 bytes" density and the `[this + 0x858]` sub-object reading of §3.3 / §3.6 (a), (c), (d) are superseded (the counts of §3.6 (b) stand as counts). The twin `0x1000ed40` is the same shape on the `+0x6aac` object (constructor `0x10012030`, size `0x27e0`; its fields sit `0x424` lower). Readers 2 and 3 of the T30b.2 tables are therefore the same block fetch for other kinds (OQ-178 narrowed).
- **Where the rule most likely lives: slot `+0x68` = `0x1000dd60`.** It takes two `float` arguments, divides each by the constant at `0x100c15a8`, subtracts the origin `[X + 0x434]` / `[X + 0x438]` (`fisub`), converts to integers, and rejects a position outside `0 ≤ x < [X + 8] << 3`, `0 ≤ y < [X + 0xc] << 3` (jumps to `0x1000df7e`); the body continues to about `0x1000df8x` and is where the layer table `[X + 0x430]` must be consulted (no class-X method appears in the `xref.py 430` list, so the pointer is read through a register or an adjusted offset). **T30b.3 reads it to the end.** The float arguments are a figure's position (the float walk of §4), which fits a per-step "may the figure stand here" test.

### 3.3.1 The position test `0x1000dd60` is a chunk cache, not the walkability yes/no (T30b.3, 2026-10-24)

Read `0x1000dd60` to its `ret` (`tools/disasm.py` on `AnnoWelt.dll`, instruction by instruction).

**Signature (D):** `(this = X, float x, float y) → ret 8`. Called by vtable slot `+0x68` of `0x100c18b4`. The two floats are the arguments; `this` is in `ecx` (`__thiscall`); return via stack cleanup `ret 8`.

**Step by step (D unless marked; `[X + k]` means offset from `X`, after accounting for `add esi, 4` at `0x1000dd95`):**

| Address range | What it reads from `this` (X) | What it computes |
|---|---|---|
| `0x1000dd60`–`0x1000dd92` | `[X + 0x434]` (origin x), `[X + 0x438]` (origin y) | `edi = int(x / 1.0f − origin_x)`, `eax = int(y / 1.0f − origin_y)` via `fld` / `fdiv [0x100c15a8]` / `fisub [X + …]` / `call 0x100b5f90` (`__ftol`). |
| `0x1000dd98`–`0x1000ddc3` | `[X + 8]` (width in 8-tile blocks), `[X + 0xc]` (height in 8-tile blocks) | Rejects `edi < 0`, `edi ≥ [X+8] << 3`, `eax < 0`, `eax ≥ [X+0xc] << 3`; jumps to `0x1000df7e` (return) on out-of-bounds. |
| `0x1000ddc4`–`0x1000ddf0` | — | Sign-safe `x mod 8` into `[esp + 0x10]` and `y mod 8` into `[esp + 0x14]` (the `and 0x80000007` / adjust pattern MSVC emits for signed `%` by a power of two). |
| `0x1000ddf6`–`0x1000de2f` | `[X + 8]` and `[X + 0xc]` again (re-check after block-aligning) | Computes `x_block = edi / 8` and `y_block = eax / 8` (sign-correct), bounds-checks against the window size, jumps to `0x1000df7e` on out-of-range. |
| `0x1000de38`–`0x1000de41` | `[X + 0x18 + block_idx*4]` where `block_idx = y_block * 16 + x_block` | Reads chunk pointer `edi` at that slot. The Y stride is `16` (fixed, `shl eax, 4`): the chunk array has at most 16 columns, so `[X + 8]` is the usable width (`≤ 16`). |
| `0x1000de4b`–`0x1000deeb` | `[X + 0xc]`, `[X + 0x10]`, `[X + 0x14 + k]` (chunk-pool head), `[chunk + 0x14]` size, `[chunk + 0x10]` data pointer | If chunk null, allocates / takes from a chunk pool and fills a new 132-byte region of chunk data by calling `[esi + 0]` = slot `+0` of the secondary vtable `0x100c18b0` installed at `[X + 4]`, which is **`0x1000daf0` (the block fetch, §3.3 orchestrator's read)**. The block fetch copies 8×8 tile records of the chunk's block into the chunk data. **D.** |
| `0x1000deeb`–`0x1000def3` | — | Stores `edi` (chunk data pointer) into `*[esp + 0x24]` (out-param); if `edi` is null, returns (`jmp 0x1000df7e`). |
| `0x1000def9`–`0x1000df19` | — | Picks the sub-cell u16 at offset `4 + (y_mod8 * 8 + x_mod8) * 2` within the chunk data (`edi`): `edx = word[eax]`, `edx = (edx & 0x8007) | 7`, `word[eax] = edx`. **MUTATES the sub-cell: bits 0..2 are set to all-1 and bit 15 is preserved; bits 3..14 are cleared.** |
| `0x1000df1a`–`0x1000df5a` | `[X + 0x424]`, `[X + 0x428]`, `[X + 0x42c]` free-list fields | Allocates a 28-byte list node (if free list empty, `operator new`), fills its fields, decrements `[X + 0x42c]`. (Fix-up T30b.3b: the first table listed `[X + 0x420]` too; `+0x420` is not read.) |
| `0x1000df5c`–`0x1000df7d` | — | `[node + 0x10] = edi`, `[node + 0x14] = y_mod8`, `[node + 0x16] = x_mod8`, `[node + 0x18] = 0`; calls `0x1000d4e0` on `[X + 0x418]` with the node — an enqueue onto the list head at `[X + 0x418]`. (Fix-up T30b.3b: the first version wrote `[X + 0x414..0x418]`; only `+0x418` is used.) |
| `0x1000df7e`–`0x1000df85` | — | Return (`pop edi; pop esi; pop ebp; pop ebx; add esp, 0xc; ret 8`). No explicit `eax` return; the function communicates through the out-param at `*[esp + 0x24]` which is `edi` or null. |

**Does `0x1000dd60` consume the layer table `[X + 0x430]`?** **No.** A grep for `[esi + 0x4(2|3)*]` in the function shows reads of only `[X + 0x434]`, `[X + 0x438]`, `[X + 0x424]`, `[X + 0x428]`, `[X + 0x42c]` and the chunk-array at `[X + 0x18 + …]`. The layer-table pointer is **not** read in this method. The function marks a sub-cell as "set" (bits 0..2 = 1, bit 15 preserved) and enqueues the mark on a list; this looks like a **claim-this-cell / reservation** operation during a walk, not a walkability test. T30b.3's hypothesis (that this is the layer-table consumer) is **refuted**.

**The constant `0x100c15a8` = `1.0f`** (bytes `00 00 80 3f`, D via `pefile`). The two `fdiv`s are therefore no-ops mathematically. Likely: the floats are already in tile units (not world units), and the x87 pass may serve a different purpose (precision pin or NaN kill). **The orchestrator's note that the constant is "a world-unit size of a tile or a sub-cell" is wrong for this release: it is 1.0f.**

**Where is `[X + 0x430]` actually read?** A full sweep of `AnnoWelt.dll` for `[reg + 0x430]` as any memory operand (any instruction kind, excluding `esp`-based) finds **six matches** (fix-up T30b.3b: the first version listed only four because the sweep was limited to `mov`). Two are writes (`0x10006362` zero-init, `0x1000dd36` class-X's setter). Two are red herrings (`0x1000db0e` and `0x1000ed50` read `[X + 0x434]` after the `+4` offset of the secondary base — these are the ORIGIN coordinate, per §3.3 orchestrator's read). **Two treat `+0x430` as a pointer**: `0x100037cb` (`mov ecx, [esi + 0x430]; mov eax, [ecx]; call [eax + 0x128]`) inside `0x10003780`, and `0x100011e2` (`add edx, [ecx + 0x430]`) inside `0x100011c0` (which also reads `[ecx + 0x434]` the same way and later calls a virtual on `[ecx + 0x454]`). Both are inside a different (bigger) class whose constructor is `0x10006344`: it installs vtables at `[+0]`, `[+4] = 0x100c14e4`, `[+8]` (chain `0x100c159c` → `0x100c1598` → `0x100c14e0` → `0x100c14dc`), `[+0x2048] = 0x100c14dc` (overwrites). **§3.3.2 below reads `0x10003780` to its `ret` and resolves what `[eax + 0x128]` dispatches to.**

**The 47 slots of `0x100c18b4`** (D via `pefile`; `+0xbc` = `0x41000000` is a float value, not code — the vtable ends at `+0xb8`):

| Slot | Function | What it does (first-20-instruction read) |
|---|---|---|
| `+0x00`..`+0x4c` | shared stubs (`0x100b49d0`, `0x100b4a00`–`0x100b4a30`, `0x10001020`–`0x10001090`) | base-class default implementations |
| `+0x50` | `0x1000db60` | writes `[X + 0x434]`/`[X + 0x438]` with `max(arg, 0)`, `[X + 0x860] = arg + 2` (window setter) |
| `+0x58` / `+0x5c` | `0x10001240` / `0x10001250` | one-field getters (`mov eax, [reg+N]; ret`) |
| `+0x60` | `0x100afa50` | one-field getter |
| `+0x64` | `0x100b4b10` | one-field getter |
| **`+0x68`** | **`0x1000dd60`** | **the chunk-cache mark-and-enqueue (this section)** |
| `+0x6c` | `0x1000e3f0` | `sub esp, 0x20; fld [esp+…]; push ebx` (another float-position method; next read) |
| `+0x70` | `0x1000df90` | `push ecx; fld [esp+…]; push ebx` (float-position method; next read) |
| `+0x74` | `0x1000e0c0` | `sub esp, 0x70; fld [esp+…]; push ebx` (float-position method; next read) |
| `+0x7c` | `0x1000dd40` | `(ecx, arg0, arg1) → [ecx + 0x44c] = arg0; [ecx + 0x450] = arg1; ret 8` (setter) |
| **`+0x80`** | **`0x1000dd20`** | **the layer-table pointer setter: `[X + 0x430] = 0x100d1140 + 0x1c00·arg`** |
| `+0x84` | `0x1000e800` | `push ebx; push ebp; mov ebp, ecx` (writer or finaliser) |
| `+0x88` / `+0x8c` | `0x10002290` / `0x100022a0` | one-line setter / getter |
| `+0x90` / `+0x94` / `+0x98` / `+0x9c` | `0x1000e6c0`, `0x1000e740`, `0x1000e760`, `0x1000e7b0` | window-state readers / mutators |
| `+0xa4` | `0x1000e8f0` | `mov eax, 0x1018; call 0x100b5fc0` (operator new of 4120 bytes) |
| `+0xac` / `+0xb0` | `0x100026b0` / `0x100027d0` | `+0x42c` counter inc / byte setter |
| `+0xb4` / `+0xb8` | `0x10037590` / `0x10037590` | shared stubs |

**None of the 47 slots dereferences `[X + 0x430]` as a pointer.** The setter writes the field, but no reader of class X ever uses it. The real reader `0x100037cb` lives in another class whose base chain overlaps class X — read in §3.3.2.

**OQ-175 narrowed, next addresses:** the walkability "yes/no" is **not** in `0x1000dd60`; `0x1000dd60` is a reservation / mark on a cached chunk. The real `[reg + 0x430]` dereference as a pointer is **`0x100037cb` inside `0x10003780`** (slot 0 of vtable `0x100c14e0`, part of a bigger engine class with vtables at `[+4]`, `[+8]`, `[+0x2048]`; big constructor `0x10006344`). The virtual call `[*(ptr) + 0x128]` on the dereferenced object is the next address. Reading `0x10003780` to its `ret` and the vtable of the object whose address lives at `[X + 0x430]` is what T30b.3 bequeaths to a follow-up.

### 3.3.2 The real `[reg + 0x430]` reader `0x10003780`, and what it dispatches to (T30b.3b, 2026-10-24)

Read `0x10003780` to its `ret` (`tools/disasm.py` on `AnnoWelt.dll`, through `0x10003a8a`).

**Signature (D):** `(this = Y, void* arg_pt) → ret 4`. Called by vtable slot `+0` of `0x100c14e0`. The function ends at `0x10003a91` with a single `ret 4` (`pop edi; pop esi; pop ebp; pop ebx; add esp, 0x1c; ret 4`), so it takes one stack argument plus `this` in `ecx`. The "5 x 4-byte args" belongs to the inner virtual call `[eax + 0x128]`, not to the function. (Orchestrator correction round ag: the first write of this section listed `(this, arg_pt, a, b, c) → ret 0x14`, which was the pushed-arg count for the inner call misread as the function's own.) The call site at `0x100037cb` is at the top of the function.

**Phase 1 (D) — the first virtual call `[eax + 0x128]`:**
- Reads `(ax, dx) = ([arg_pt + 0x40], [arg_pt + 0x42])` (two u16 of a point / tile).
- Reads `[Y + 0x4e4]` (`ox`), `[Y + 0x4e8]` (`oy`): adds them to `(ax, dx)` → `(ox + x, oy + y)`.
- Writes a 4-dword buffer on the stack (`lea edx, [esp + 0x24]` at `0x100037b2`) with contents **`{ox + x, oy + y, ox + x + 4, oy + y + 4}`** = `(x0, y0, x1, y1)` — the SAME `(x0, y0, x1, y1)` rectangle shape §3.3 names for `0x1000daf0`'s buffer; the four writes land at `[buffer + 0]`, `[buffer + 4]`, `[buffer + 8]`, `[buffer + 0xc]` in order after each `push` adjusts `esp` (traced: `0x100037ae mov [esp + 0x28], ecx` writes `oy + y` at `buffer + 4`; `0x100037c0 mov [esp + 0x28], eax` writes `ox + x` at `buffer + 0` after `push edx` shifted `esp`; `0x100037c7 mov [esp + 0x34], ecx` writes `oy + y + 4` at `buffer + 0xc`; `0x100037d8 mov [esp + 0x34], eax` writes `ox + x + 4` at `buffer + 8` after `push edx` shifted `esp` again). (Orchestrator correction round ag: the first write of this section listed the tuple as `{ox + x + 4, ox + x, oy + y + 4, oy + y}`, which paired the +4's together but is not what the four stores actually land at.)
- Reads `[Y + 0x2038]` and `[Y + 0x203c]` (two world-level objects).
- Reads **`ecx = [Y + 0x430]`**, then **`eax = [ecx]`** = ecx's vtable, then **`call [eax + 0x128]`** with the 5-arg stack `(4, arg_pt, &buffer, [Y + 0x2038], [Y + 0x203c])`.
- **`[Y + 0x430]` is a world-like pointer, NOT the class-X layer table.** Dword at every layer-block start is `0xffffffff`-ish image data, not a valid vtable, so if the setter `0x1000dd20`'s pointer ever reached this field a crash would follow. Verified by reading the first 32 bytes of each of the five `0x100d1140 + 0x1c00 · L` blocks with `pefile`.

**What `[eax + 0x128]` is (D):** the world's vtable is `0x100ccdd8` (annowelt.md, round-ae note); its slot `+0x128` is **`0x100a52d0`**. First instructions (`disasm.py`):

```
mov eax, [ecx + 0xec]      ; world's chunk-row width
cdq / and edx, 7 / add eax, edx     ; align
mov ebx, [esp + 8]         ; arg (part of the buffer coords)
...
mov al, [edx + ebp + 0x130]  ; byte at [world + 0x130 + chunk_index]
```

Pattern matches the kind-11 / kind-10 handlers of `0x100abdb0` (round-ae note: "a byte at `world + 0x130 + …`, chunk index from `[world + 0xec]`"). So `0x100a52d0` is **a per-chunk LOOKUP** on the world's chunk array, similar family to `0x100abdb0` but for a different kind code. The virtual call `[eax + 0x128]` therefore **fetches a chunk / chunk descriptor** for the buffer rectangle — the SAME shape as the block fetch of §3.3.

**Phase 2 (D) — the position-range mark (bits 0..2 of a word):**
After the virtual call, `0x10003780` continues at `0x100037e5` and does, repeatedly, the **same `(v & 0xfc07) | 7` or `(v & 0x8007) | 7` write pattern as `0x1000dd60`**:

- `0x1000393e`: `and ebx, 0xfc07; or ebx, 7; mov word[edx − 4], bx` (per-cell loop, `inc eax; jne`).
- `0x1000396d`: `and edx, 0xfc07; or edx, 7; mov word[eax − 4], dx` (per-cell loop).
- The function sweeps a RECTANGULAR RANGE of sub-cells (bounded by `[Y + 0x454]`, `[Y + 0x458]`, `[Y + 0x45c]`, `[Y + 0x460]`, `[Y + 0x464 + k]` — a per-row bound table) and sets the low three bits of each sub-cell to `7`.

**Conclusion.** `0x10003780` is **the same shape as `0x1000dd60`**: a position-indexed reservation / mark on a cached chunk, not a walkability yes/no. It calls the world's chunk fetcher first (through the virtual `[eax + 0x128]` on `[Y + 0x430]`), then walks the resulting chunks writing `(v & … | 7)` into every sub-cell in the range. The `0xfc07` mask preserves bit 15 and bits 10–14 (vs `0x8007` in `0x1000dd60` which preserves bit 15 only); the extra preserved bits suggest a WIDER tag field, so this reservation also records something more than the three-bit flag the other one writes. The virtual call parallels `0x1000daf0`'s shape — same `(fourarg buffer, cell + 4, 8)` kind of call, same "fetch a block of tiles" dispatch.

**What `[Y + 0x430]` is (D-reasoned, not direct-D-written):** `0x100a52d0` reads `[ecx + 0xec]` and `[ecx + 0x130]` — fields the world owns (round-ae: "chunk index from `[world + 0xec]`"). So the receiver is the WORLD. **`[Y + 0x430]` holds a pointer to the world** in this class, not a layer table. The field is written through a path my sweep did not find in `AnnoWelt.dll` (no `mov [reg + 0x430], <non-zero>` on this class besides the class-X setter which belongs to class X); likely set via a struct copy, a cross-module writer, or a path my sweep missed (`add`, `lea`, or an aggregate init via `rep movs`). **Open:** the write path; a Wine memory read of this class's `[Y + 0x430]` at run time would confirm (T30b.4+).

**OQ-175 resolved as follows.** The walkability "yes/no" is **not** `[edx + 0xec]` or `[edx + 0x128]` on any of the family readers: both are **chunk / block fetchers** (`0x100abdb0`, `0x100a52d0`, round-ae and this section). Both call sites are **inputs** to a reservation-write function (`0x1000dd60` and `0x10003780`); the final yes/no decision for a walk is **not** inside the family of `[reg + 0x430]`/`[reg + 0x434]` methods on these classes. The walking-layer table at `0x100d1140 + 0x1c00 · L` is **not consumed by any `AnnoWelt.dll` method the T30/T30b sweeps found**. Three places to look next, in order of likely payoff:

1. **The pathfinder / search engine's node expander** (OQ-161, world `+0x6aa8` vtable `0x100c1424`). The search engine at `+0x6aa8` is a different class from class X (`+0x6ab0`); its vtable has not been read in full. The step cost / neighbour expansion likely consults the layer-table bits that `0x1000dd60`'s mark-write SETS on sub-cells, so the "walkable" test might be a **bit-mask read on the already-marked sub-cell** (bit 0..2 = marked, bit 15 = ?), not a direct layer-table lookup.
2. **The sub-cell bit layout.** The mark writes `(v & 0x8007) | 7` and `(v & 0xfc07) | 7`: bits 0, 1, 2 become `1`; bits 3..9 are cleared by one but not the other; bits 10..14 are cleared by one but not the other; bit 15 is preserved by both. **Bits 10..14 are the "reservation detail"** on `0x10003780`'s wider mark. What a reader does with these bits (bit 15 as "blocked"? bits 10..14 as a cost?) is the actual walkability contract. Read **`0x100011c0`** next: it is the other `[reg + 0x430]` consumer and uses `[ecx + 0x454]` through a virtual too.
3. **Cross-module reader.** `AnnoGame.dll` / `AnnoScene.dll` / `AnnoDisplay.dll` were not swept here; the layer-table pointer at `0x100d1140 + 0x1c00 · L` might be referenced only from `AnnoScene.dll`'s search engine. A `tools/xref.py --reloc <dll> 0x100d1140 0x100d9d40` sweep on each would decide.

**Still open** (OQ-175 narrowed, not closed):
- Who writes `[Y + 0x430]` on the bigger class (not caught by the `AnnoWelt.dll` sweep).
- The sub-cell bit semantics (what makes a tile walkable, read from where).
- What the layer tables at `0x100d1140` are actually read by (if they are consumed by `AnnoWelt.dll` at all, the sweep's `[reg + 0x430]` is the write/read of the pointer; the TABLE CONTENT is at `0x100d1140 + 0x1c00·L`, consumed only through that pointer, and the pointer has one setter and no readers found — **so the layer tables may not be consumed in `AnnoWelt.dll` at all**).

**Also `0x100011c0` (T30b.3c, 2026-10-24).** Read to its `ret 4` at `0x1000123d`. **Correcting the round-af nit:** `0x100011e2 add edx, [ecx + 0x430]` **is not a pointer dereference**; it is an `add` that uses the field as a 32-bit coordinate value (added to `floor(p[0] / 3)`). The subagent's round-af catch about "two pointer-reads" was slightly loose: `0x100011c0` has `[ecx + 0x430]` and `[ecx + 0x434]` as **integer origin fields** (the same shape as class X's `[X + 0x434]` / `[X + 0x438]`), not object pointers. The function **does** dereference a different field, `[ecx + 0x454]`, through a virtual call `[edx + 0x128]`.

Instruction summary (D, `disasm.py` on `0x100011c0`..`0x1000123d`, 125 bytes):

| Address range | Reads | Computes |
|---|---|---|
| `0x100011c0`–`0x100011dd` | `[esp + 0x18]` (arg `p`), `word[esi]` (= `p[0]`, u16) | `edx = p[0] / 3` via MSVC's signed `div 3` sequence (`imul edx, 0x55555556; shr eax, 31; add edx, eax`). |
| `0x100011e2` | `[ecx + 0x430]` as a 32-bit value | `edi = p[0] / 3 + [ecx + 0x430]`. |
| `0x100011ea`–`0x1000120a` | `word[esi − 2]` (= `p[2]`, u16) | `edx = p[2] / 3` same divide trick. |
| `0x1000120c` | `[ecx + 0x434]` as a 32-bit value | `edx = p[2] / 3 + [ecx + 0x434]`. |
| `0x10001212`–`0x10001229` | `[ecx + 0x454]` (receiver), `[ecx + 0x458]` (arg) | Stores a 4-dword buffer at `[esp + 0x14..0x24]`. Traced instruction by instruction with each `push` accounted for: `0x100011fa [esp + 0x14] = edi` at frame X−0x24 writes `buffer + 0 = x0`; `0x1000121e [esp + 0x1c] = edx` at frame X−0x28 (after the intervening `push 0`) writes `buffer + 4 = y0`; `0x10001225 [esp + 0x20] = edi + 2` writes `buffer + 8 = x1`; `0x10001229 [esp + 0x24] = edx + 2` writes `buffer + 0xc = y1`. Final tuple **`(x0, y0, x1, y1)`** — the SAME shape §3.3 names for `0x1000daf0` and §3.3.2 for `0x10003780`. (Orchestrator correction round ah: the first write of this section listed the tuple as `(edi, edi + 2, edx, edx + 2) = (x0, x1, y0, y1)`, which paired `edi` and `edi + 2` as neighbours in buffer order; what the four stores actually land at, after each `push` shifts the frame, is `(edi, edx, edi + 2, edx + 2) = (x0, y0, x1, y1)`. Same lesson as round ag for `0x10003780`'s buffer: the order of stores depends on where `esp` is at each store, not on the `add` arithmetic.) |
| `0x1000122d`–`0x10001232` | `[ecx + 0x454]`'s vtable, slot `+0x128` | `call [edx + 0x128]` with pushes `(6, p+4, &buffer, 0, [ecx + 0x458])` — the same chunk-fetch call shape as `0x1000daf0` and `0x10003780`, with kind = 6 (vs 8 in `0x1000daf0` and the implicit kind of `0x10003780`'s world call). |
| `0x10001238`–`0x1000123d` | — | `pop edi; pop esi; add esp, 0x10; ret 4`. One arg (`p`); no explicit return. |

**Caller / receiver class (D-partial):** `0x100011c0` is slot `+0` of vtable `0x100c129c` (`xref.py --reloc`), the SECONDARY base vtable (at `[W + 4]`) of a class whose primary vtable is `0x100c12a0` and whose constructor is at `~0x10002830` (`0x1000284d: mov [esi], 0x100c12a0; mov [esi+4], 0x100c129c`). Call it **class W**. Vtable `0x100c129c` is adjacent in memory to `0x100c18b0` (= class X's secondary); both appear to inherit from the same base `0x100c135c`. The virtual call receiver `[W + 0x458]` likely holds the world — if so, slot `+0x128` would be the same `0x100a52d0` as `0x10003780`'s receiver. **Open:** who writes `[W + 0x458]` and `[W + 0x454]`; is class W a sub-component of the bigger class or constructed separately? One more sub-task settles it; T30b.3c marks these as the next addresses.

**Pattern across the three readers now:**

| Function | this | `[+0x430]` | `[+0x434]` | Virtual-call receiver field | Dispatch slot | Divides p by |
|---|---|---|---|---|---|---|
| `0x1000daf0` | class X secondary (X+4) | origin x (`[X + 0x434]`) | origin y (`[X + 0x438]`) | `[X + 0x858]` (= world) | `+0xec` (`0x100abdb0`) | not divided |
| `0x10003780` | class Y (bigger) | world pointer | some integer (`add` only) | `[Y + 0x430]` (= world) | `+0x128` (`0x100a52d0`) | not divided |
| `0x100011c0` | class W secondary (W+4) | origin x (`[W + 0x434]`) | origin y (`[W + 0x438]`) | `[W + 0x458]` (probably world) | `+0x128` (likely `0x100a52d0`) | 3 (sub-cell → tile) |

So **three different classes** share a very similar "fetch a chunk around a point" pattern against the world's chunk-row dispatcher, each with its own origin coordinates. Each then does something with the fetched chunk (`0x1000dd60` and `0x10003780` write a reservation mark; `0x100011c0` doesn't continue past the virtual call → the fetched chunk is probably the function's purpose, with the mutation happening in the virtual's handler). **The walkability yes/no decision is still not located**: all three family readers are **chunk-fetch / coordinate-translate**, not pass/fail.

### 3.4 The writers of the layer blocks (T30 step 3, D)

**Reading guide (T30b.8, 2026-10-25): where the walk rule stands.** The rule, read from the top, is `docs/sim/state.md` §5.9 "The walk rule for T33, consolidated"; this file is its evidence. The path through the evidence is §3 (what the engine is and how a job drives it) → §3.4.7 (the loop) → §3.4.8 and §3.4.14 (the expansions) → §3.4.11–§3.4.13 (the cell, the layer block, the tile record) → §3.4.15 (the running original) → §3.4.16 (order, origin, goals). Statements older than that path that it **supersedes**: §3 "open: what the layer tables hold" and "open: the expansion itself" (answered by §3.4.12, §3.4.7–§3.4.14); §3 "likely: the 9-bit mask is the walk mask of the tile's object" (D, §3.4.13); §3.1 (b)'s "indexed by the mapped value" (the tile byte indexes, §3.4.12); §3.3 / OQ-175's "secondary table and `[edx + 0xec]` combine to a yes/no" (§3.3.1, §3.3.2, §3.4.12: the reader `0x1000daf0` is a map-block fetch, the yes/no is `L[code] & key` with the geometry bits); §3.4.3–§3.4.6's candidate readings of bit 10 as the walkability flag (it marks goal cells, §3.4.7, §3.4.16 (d)); §3.4.10's "likely a ramp, bridge, pier or entrance" for the codes `0x80..0x86` (withdrawn, §3.4.11 (f)); §3.4.15's "what `+0x4ec` / `+0x4f0` is exactly" (§3.4.16 (b)). §3.2's named-road counts stand (26 and 11). Open after T30b.8, with their next addresses: the shore and river tiles (`0x10081f51`), the finer-route validation (`0x100054ef..0x100055d5`), island slot `+0x12c`, and the Wine comparison of an observed route (T30b.5–7).

- `xref.py --reloc AnnoWelt.dll 0x100d1140`: exactly two absolute references, both inside the two setters above (`1000dd32`, `1000f022`). **No absolute reference points into any address of the five blocks (`0x100d1141`–`0x100d9d3f`)**; the next referenced `.data` address is `0x100dcb44` (`xref --reloc 0x100d1141 0x100e1000`). So any run-time write must go through the stored pointers at `[engine + 0x430]` / `[+0x1030]`.
- `xref.py 0x430 0x431` lists every instruction with the displacement `+0x430`. The one that writes `[reg + 0x430]` in the layer-table context is the setter `0x1000dd20`'s `mov [ecx + 0x430], edx` — a store that writes the table POINTER, not an entry. Every other `+0x430` match in `AnnoWelt.dll` is in a different class (integer coordinate at that offset). I did not read `AnnoGame.dll`, `AnnoScene.dll`, `AnnoDisplay.dll` for cross-module writers; the memory compare of step 7(a), when a reader exists, settles it.

**Conclusion:** the layer blocks are **initialised data**, and writes at run time, if any, go through the engine pointers (not through the base address). Writes through the pointer have not been traced; T30 step 7(a) is the straightforward way to decide (compare the DLL image with the running game's memory at those addresses).

#### 3.4.1 Cross-module `--reloc` sweep: the layer blocks are not referenced outside `AnnoWelt.dll` (T30b.3d, 2026-10-24)

**D.** `tools/xref.py --reloc` on every game DLL for the whole range `0x100d1140 .. 0x100d9d40` returns **zero hits** in each. A code sweep for each of the five base addresses `0x100d1140 + 0x1c00·L` (L = 0..4) across the same DLLs returns **zero hits** in each too.

| DLL | `--reloc 0x100d1140..0x100d9d40` | code sweep for the 5 base addresses |
|---|---|---|
| `AnnoGame.dll` | 0 | 0 |
| `AnnoScene.dll` | 0 | 0 |
| `AnnoDisplay.dll` | 0 | 0 |
| `AnnoFrame.dll` | 0 | 0 |
| `CompLogic.dll` | 0 | 0 |
| `CompLogicClassic.dll` | 0 | 0 |
| `View.dll` | 0 | 0 |
| `FileGame.dll` | 0 | 0 |

So the only references to `0x100d1140` anywhere in the shipped modules are the two inside `AnnoWelt.dll`'s setter and its twin (`0x1000dd32`, `0x1000f022`, round-ag confirmed). The data at `0x100d1140 + 0x1c00·L` is **not read by any game DLL** my sweeps have found. Combined with §3.3.1–§3.3.2 (`AnnoWelt.dll` has three chunk-fetch "layer-table-shaped" readers, each of which turns out to be a chunk-fetch on the world's chunk map, not a read of the data at `0x100d1140`), the leading hypothesis is now:

- **The layer tables at `0x100d1140 + 0x1c00·L` appear to be dead data** in the shipped build. The setter `0x1000dd20` writes a pointer to one of them into `[X + 0x430]`, and nothing reads that pointer back as data. T30's original "walking-layer tables" interpretation of OQ-160 is suspect: the walkability yes/no is **elsewhere**, likely in the sub-cell bit layout the three chunk-fetch readers set (bits 0..2 of the sub-cell u16 after `(v & 0x8007) | 7` / `(v & 0xfc07) | 7`) and a reader that **reads the sub-cell's bits** on each node expansion.
- An alternative is a run-time write to the layer tables via the stored pointer (`[X + 0x430]`); step 7(a)'s memory compare of the five blocks after a scenario loads (T30b.4) decides it. If the blocks are the SAME as the DLL image at run time, the "dead data" conclusion is confirmed; if they differ, run-time writes exist and the writer must be traced through the pointer.

**Next addresses for OQ-175 (updated):**

1. **Sub-cell bit readers.** The reservation writes set bits 0..2 of each sub-cell u16; a reader somewhere must test these bits to decide "walkable". `xref.py` for `and reg, 7` / `test reg, 7` on the sub-cell data pointer is a lead; the search engine's node expander on `world + 0x6aa8` vtable `0x100c1424` is the most likely place.
2. **Memory compare (T30b.4) as a cheap test.** Already written up as a sub-task. If the blocks don't change at run time, the "dead data" hypothesis is confirmed.
3. **The three class W/X/Y writers for their world/receiver fields.** T30b.3b/3c left open: who writes `[X + 0x858]` (world), `[Y + 0x430]` (world), `[W + 0x454]`/`[W + 0x458]` (world + another). A `rep movs`-based or struct-copy-based writer may be in play; a non-`mov`-destination sweep across the four modules would catch it.

### 3.6 OQ-175: deeper look at the layer-table family (T30 round y, 2026-10-24)

After round y I went further on OQ-175. Four new concrete findings, each with its evidence; the actual yes/no method `[edx + 0xec]` body is **still open** (needs the class of `[this + 0x858]`).

**(a) The reader is a family of at least three variants with different `this` layouts.**

| Reader (vtable, slot +0) | primary table | secondary table | sub-obj | sub-arg | index stride (lea) | pair delta |
|---|---|---|---|---|---|---|
| `0x1000daf0` (`0x100c18b0`) | `[this + 0x430]` | `[this + 0x434]` | `[this + 0x858]` | `[this + 0x85c]` | implicit `+p[0]` | +8 |
| `0x1000ed40` (`0x100c1a30`) | `[this + 0x42c]` | `[this + 0x430]` | `[this + 0x434]` | `[this + 0x438]` | implicit `+p[0]` | +8 |
| `0x1000f9d0` | `[this + 0x1054]` | `[this + 0x1058]` | `[this + 0x1030]` | — | `lea [base + i·8]` | +0x40 |

All three build a 4-dword buffer of pointers into two tables and call `[edx + 0xec]`. The first two share the pair-delta of +8 (two 8-byte spans read); the third has a +0x40 pair-delta and a `·8` index stride, so its tables hold 64-byte entries with 8-byte indexed rows. The `[+0x858]`/`[+0x85c]` pair in `0x1000daf0` is the same **conceptual field** as `[+0x434]`/`[+0x438]` in the twin — a sub-object + sub-arg, just at different absolute offsets because the two engine classes differ in size by about 0x424 bytes. The engine with vtable `0x100c1a30` is **embedded as a sub-object at `+0x23a4` of a larger engine** (constructor at `0x10012079`): `[outer + 0x23a4] = 0x100c1a30` is written inline, and the embedded sub-object's `+0x434` (full offset `+0x27d8` on the outer) holds the outer's `arg` passed to the constructor (D).

**(b) The u16 table `0x100c1af8` is a many-to-few mapper, not a byte-wise mask.** 512 u16 entries (D, size), **33 distinct outputs** (P, counted). The outputs range `0..0xfff` and are **not aligned to 8 or 16 bytes** (several like `0x6c`, `0x82`, `0xee` are odd offsets): the layer-table entry stride is **not** uniform. Instead, each distinct output is a byte offset into the 0x1c00-byte layer block, pointing at an 8-byte span the reader hands to the decision method. One output (`0xfff`) dominates with **123 of 512 masks** mapping to it — whether it means "blocked / default" is open: it is the largest of the 33 offsets, but `0xfff + 8 = 0x1007` lies well inside the `0x1c00`-byte block (the first version of this sentence said "1 byte of space", which was wrong arithmetic), so nothing about its meaning follows from its position; the method of §3.3 decides.

| cell type (byte offset in block) | masks mapping to it | first mask (9 bits) |
|---|---|---|
| `0x0000` | 21 | `0b101010101` (0x155) |
| `0x000c` | 23 | `0b101010001` (0x151) |
| `0x0060` | 23 | `0b101010100` (0x154) |
| `0x006c` | 12 | `0b101010010` (0x152) |
| `0x0300` | 23 | `0b100010101` (0x115) |
| `0x030c` | 25 | `0b100010001` (0x111) |
| `0x0801` | 23 | `0b001001111` (0x04f) |
| `0x0b61` | 7 | `0b010001110` (0x08e) |
| `0x0fff` (dominant) | **123** | `0b000000000` (0x000) |
| other 24 types | 1..20 each | — |

The 33 outputs have high nibbles only in `{0, 3, 4, 7, 8, b, c, f}` (pairs 0+3, 4+7, 8+b, c+f: each pair differs by 3, which is `0b11` in the low two bits). The 9-bit input mask reads naturally as a 3×3 sub-cell layout (centre + 8 neighbours), so the mapper looks like a **canonical form** of the 9-bit mask reduced to an equivalence class of walkability patterns. The isometric engine has 8 move directions, and the mapping compresses rotational / reflection symmetries.

**(c) The layer blocks are addressed by that mapped offset, not by raw mask or by a uniform entry size.** The reader's math `[primary + p[0]]` with `p[0]` = `0x100c1af8[mask]` means a tile's cell-type is a **byte offset into the layer-specific block**, and the 8 bytes at that offset are the per-layer walkability data for that cell-type. Since 33 cell-types occupy offsets up to `0xfff + 8 = 0x1007` inside the 0x1c00 block, there is plenty of slack between entries; the real data density is one 8-byte entry per cell-type per layer, roughly `33 × 8 = 264` bytes of actual data per 7168-byte block. The rest is slack (needed because the mapper outputs byte offsets, not indices).

**(d) The second u16 (`p[2]`) is the byte at bits 8–15 of the tile record's second dword** (`0x1008ff70` at `0x10090050`–`0x1009007b`, D §3.1 (b)). That is a different piece of per-tile data than the 9-bit mask — likely the **neighbourhood code** (a byte summarising the eight neighbours of this tile, or an orientation marker), since the `+0x40` stride in the third reader `0x1000f9d0` and the pattern of its reads suggest the secondary is a **direction-indexed 8-byte entry** (one byte per direction).

**Still open (OQ-175, with addresses):**
- The vtable method `[edx + 0xec]` body on the class pointed to by `[this + 0x858]` (`0x1000daf0`) / `[this + 0x434]` (twin) / `[this + 0x1030]` (`0x1000f9d0`). The call signature, re-read for `0x1000daf0` by the orchestrator, is `(this = sub_obj, arg0 = sub_arg, arg1 = 0, arg2 = &buffer_of_4_dwords, arg3 = cell + 4, arg4 = 8)`; whether the two twins push the same five is for T30b.2 to re-check instruction by instruction. The method must dereference the 4 pointers in `buffer` to read the two 8-byte spans from each layer table and return a boolean (walkable yes/no). Reading its body is T30b step 2.
- The class of the sub-object. No direct write to `[reg + 0x858]` was found (sweep finds zero); the constructor `0x1000eb40` writes `+0x85c` from an argument but not `+0x858`. The outer engine's constructor at `0x10012079` writes `[esi + 0x27d8] = arg` (which is the embedded engine's `+0x434`), but the sub-object's own vtable write wasn't located. The field is likely written **during a later `init` method call** on the engine, not in the constructor. Checking who else calls `[engine + something]` with the newly-allocated engine (`0x100aac23` calls `[eax + 0x80]` which is the layer-table setter `0x1000dd20`) might reach it.
- Of the 76 `call [reg + 0xec]` sites in `AnnoWelt.dll`, **four match this family's pattern** by eye (two or three dword pushes with the last being a buffer pointer): `0x1000db4a` and `0x1000ed9a` (our targets), `0x1000fa33` (the `+0x40`-stride variant), and `0x1000faed` (another variant nearby). The others look like unrelated virtual methods on different classes. T30b step 2 reads one body to decide which (likely the shared method lives on the sub-object's base class).

#### 3.4.2 Memory compare in the running game: dead data confirmed (T30b.4, 2026-10-24)

**D.** The Wine runner read the five 7168-byte blocks at `annowelt_base + 0xd1140 + 0x1c00·L` (L = 0..4) from the running game's memory and compared each byte to the matching bytes in `game/app/AnnoWelt.dll` on disk (`pefile`). The run (`tools/orig/layer_blocks.py`, two passes with the game loaded):

| Pass | After | Result (per block) |
|---|---|---|
| 1 | new-game Citizen (0 game-ms) | layer 0..4: **equal** |
| 2 | +600 244 game-ms (≈ 10 game minutes) | layer 0..4: **equal** |

All ten cells **equal** — every byte at every offset matches the DLL image. The blocks are not written at run time. Combined with §3.4.1 (zero cross-module references) and §3.3–§3.3.2 (no reader of `[X + 0x430]` as a pointer in class X's vtable; the three chunk-fetch family readers all dispatch on world chunk-row methods), **the walking-layer tables at `0x100d1140` are dead data in the shipped build**. The pointer the class-X setter writes into `[X + 0x430]` is never read as data; the data at `0x100d1140 + 0x1c00·L` is never read nor written at run time.

**Consequence for OQ-175.** The walkability yes/no is **not** in these layer blocks. The remaining theories are:

1. **Sub-cell bit reader** on the sub-cell u16 the three chunk-fetch readers (§3.3.1, §3.3.2) write with `(v & 0x8007) | 7` or `(v & 0xfc07) | 7`. A reader that tests bits 0..2 of the sub-cell must exist somewhere; the search engine on `world + 0x6aa8` vtable `0x100c1424` is the most plausible place (T30b.3e).
2. The chunk's byte array at `[chunk + 0x10]` filled by `0x1000daf0` (block fetch) and its siblings — the sub-cells themselves carry walkability bits, not the layer-table blocks.

**The run.** Written up in REPORTS.md T30b.4. The write-up includes the one scope note: pass 2's `wait_game_ms` reported `status: timeout` because the loop's deadline (10 real minutes) was reached as the last check, yet `advanced_ms = 600244 ms` shows the game clock DID advance just past 10 game minutes before the final read. The `F4` keypress to accelerate the game to 400 % appears to have been dropped by the WSLg forwarder (game clock advanced at ≈ 1× real time instead of 4×); the memory-compare result is unaffected because the game time did reach the target. The tool `tools/orig/layer_blocks.py` is a committed addition.

#### 3.4.3 The search engine's vtable `0x100c1424` and the hunt for the sub-cell bit reader (T30b.3e, 2026-10-24)

**D.** Dumped vtable `0x100c1424` (the search engine on `world + 0x6aa8`, round-ae identification). 47 slots (0x00 .. 0xfc; the slots at `+0x50..+0xbc` are the non-stub ones). The vtable is installed at `0x10006401` inside the big-class constructor `0x10006344` (`xref.py --reloc`, one hit): the SAME constructor the T30b.3b analysis named for class Y. So **the "search engine" is the big class** — class Y and the search engine are the same object with multiple vtables.

**Non-stub slot summary:**

| Slot | Function | First instructions |
|---|---|---|
| `+0x50` | `0x10004b50` | 5-arg entry (`mov edx, [esp+0x10]`, push save-regs) — likely `init(x0, y0, x1, y1, flags)` or similar |
| `+0x54` / `+0x58` / `+0x5c` / `+0x60` / `+0x64` | `0x10004c10` / `0x10004b10` / `0x10004b20` / `0x10004b30` / `0x10004b40` | tiny setter/getter group (adjacent addresses in `.text`) |
| `+0x68` | `0x10010830` | — |
| **`+0x6c`** | **`0x10010870`** | contains `and edx, 7` BUT it is MSVC's signed `mod 8` for a coordinate (same pattern as `0x1000dd60`); not a bit test |
| `+0x70` | `0x10004c70` | — |
| `+0x74` / `+0x78` | `0x10010850` / `0x10004c50` | — |
| `+0x7c` / `+0x80` | `0x10004c30` / `0x10004bf0` | — |
| **`+0x84`** | **`0x10005750`** | contains `and edx, 7` BUT it is MSVC's `mod 8` for a coordinate |
| `+0x94` / `+0x98` / `+0x9c` / `+0xa0` | `0x10004b90` / `0x10004bb0` / `0x10010810` / `0x10004bd0` | — |
| **`+0xa4`** | **`0x10005890`** | huge function: 0x1020-byte local frame; `fld [esp+0x1024]; fdiv [0x100c1138]; fisub [edi+0x4ec]` — SAME float-position input shape as `0x10003780` (float → tile coord). Likely the **search's entry** that triggers the chunk cache + walk. ~700 bytes long. |
| `+0xa8` / `+0xac` | `0x10005ff0` / `0x100062f0` | the second looks like a destructor-chain helper (`lea ecx, [esi+0x2048]; call 0x10002900; mov eax, [esi+4]; call [eax+0xac]` — forwards up the base chain) |
| `+0xb0` / `+0xb4` | `0x100010a0` / `0x100029e0` | — |
| **`+0xbc`** | **`0x10003780`** | the **chunk-cache mark function from T30b.3b** — the search engine is the class that uses `0x10003780` directly as one of its methods |

**Scan for sub-cell bit tests.** Grepped every non-stub slot's body for `and reg, 7` / `test reg, 7`: two hits (`0x10010923`, `0x1000580a`), **both** are MSVC's signed `mod 8` pattern (`cdq; and edx, 7; add eax, edx; sar eax, 3`), computing a tile's block index from a sub-tile coordinate — same pattern as `0x1000dd60` uses in §3.3.1. **No obvious bit-test on a sub-cell u16 found in this pass.** Also scanned the three "search core" functions `0x10003d90`, `0x10004890`, `0x10005890` (named by the orchestrator in round ae) for word-based tests (`word ptr`, bit masks `0x8000` / `0xfc07`): zero hits.

**Observation on the family.** The search engine's `+0xbc` slot IS `0x10003780`, which §3.3.2 identified as a reservation / mark function. So the search engine CALLS the mark function on every expansion that reaches a cell; this writes `(v & 0xfc07) | 7` into the sub-cell. The reader that TESTS these bits must exist (otherwise the write is wasted), but it isn't a direct `test reg, 7` on the chunk data in any of the slot bodies my grep covered.

**Possibilities for the walkability yes/no, from leading to speculative:**

1. **The bit test lives inside a per-cell loop that reads the chunk data differently** — e.g., `movzx eax, [edi + N]; and eax, 7` (where `edi` is the chunk pointer, not the vtable). A broader grep that doesn't anchor to a vtable slot but to the chunk-data pattern (`[edi + k]` where `edi` was set from a chunk-fetch result) is the next sweep.
2. **The test is on a different field** of the sub-cell u16 — not bits 0..2 (`| 7`), but the preserved bits (`& 0xfc07` or `& 0x8007`): bits 3..9 or bits 10..14 might encode the actual walkability, with bits 0..2 being the "visited" tag.
3. **The test lives in another DLL** (unlikely given T30b.3d's zero cross-module references for the layer blocks, but the chunk data is at a different address — the chunk `[chunk + 0x10]` is heap-allocated).
4. **The chunk-fetch handler itself** (`0x100abdb0` / `0x100a52d0` on the world) tests walkability as part of filling the chunk — the chunk the family readers get is PRE-FILTERED to only walkable cells, and the subsequent sub-cell marks are reservation tags.

**Next address:** read `0x10005890`'s body in full (likely the search-entry) and `0x10003d90`/`0x10004890` (likely the node expander and the step function). Look for a pattern like `movzx eax, word[reg+k]; and eax, 7; test ...` or a conditional branch on the sub-cell value. If still no walkability bit test, option 4 above (the chunk-fetch filters) becomes the leading theory.

**OQ-175 further narrowed, not closed.** The search engine is identified (vtable `0x100c1424`, constructor `0x10006344`, same object as class Y of §3.3.2). The simple sub-cell-bit-test grep did not find a reader. T30b.3f (the deeper read of the three search-core functions) is the natural follow-up.

#### 3.4.4 The three search-core functions: lengths, callees, and the one byte-mask test (T30b.3f, 2026-10-24)

**D.** Full disassembly of `0x10003d90`, `0x10004890`, `0x10005890` to their rets, every direct callee listed, and a widened test-family grep (`test (byte|word) ptr`, `cmp (byte|word) ptr`, `movzx`, `and|or|bt|btr|bts|xor reg, imm`) across all three bodies. The round-ak nit (§3.4.3 says "47 slots"; the actual vtable has 48 code slots, 0x00..0xbc) is noted; nothing in this pass changes with that correction.

**Function boundaries and `.rdata` references.** First `ret` of each function and the end of its basic block (next instruction aligned on a prologue or data) give:

| Function | Last `ret` | Body size | `ret imm` | `.rdata` reference (from `xref.py --reloc`) | Vtable interpretation |
|---|---|---|---|---|---|
| `0x10003d90` | `0x10004867` (`ret`) | ~2776 bytes | 0 args stacked (`__thiscall`-shaped: `ecx = this`, locals big) | `0x100c1568` | slot `+0x84` of the `0x100c14e4` sub-vtable (class Y's `[+4]` interface): offset `0x100c1568 - 0x100c14e4 = 0x84` = slot 33 of that second interface |
| `0x10004890` | `0x10004a6d` (`ret 0xc`) | ~480 bytes | 3 stack args | no `.rdata` xref; sweep found exactly one caller `0x10003e8d` (inside `0x10003d90`) | NOT a vtable method — a static helper of `0x10003d90` |
| `0x10005890` | `0x10005b49` (`ret 0x14`) | ~697 bytes | 5 stack args | `0x100c14c8` | slot `+0xa4` of the primary search-engine vtable `0x100c1424`, which §3.4.3 named as the likely search entry (same `fld [esp+N]; fdiv [0x100c1138]; fisub [this+0x4ec]` float-to-tile shape) |

Callees (from each body's `call`s, de-duplicated):

- **`0x10003d90`** (fn1, 36 direct call sites, 20 distinct targets): `0x100b5f90` (×8, likely the signed-float-to-int `__ftol` helper), `0x100064b0` (×4), `0x10017510` (×3), `0x1000b790` (×3), `0x10003aa0` (×3, a float-to-tile helper, 0xcc stack frame), `0x10004890` (fn2, ×1 at `0x10003e8d`), `0x10008540`, `0x1000d6a0`, `0x10003220`, `0x100071c0`, `0x10007770`, `0x10007b00`, `0x10007ea0`, `0x10006a40`, `0x10001480` (end-of-function cleanup).
- **`0x10004890`** (fn2): `0x100b5f80` (×1, allocator-adjacent), `0x10004a70` (×2), `0x10004ac0` (×2). The two helpers (`0x10004a70`, `0x10004ac0`) are the three tiny linked-list-splice functions sitting right after fn2 in `.text` (ending with the three `ret` instructions at `0x10004a9d`, `0x10004aaf`, `0x10004ab9` seen in the raw sweep — ordinary intrusive-list helpers, 16–57 bytes each).
- **`0x10005890`** (fn3): `0x100b5fc0` (`__chkstk`-shaped, 0x1020-byte frame probe), `0x100b5f90` (×2, `__ftol`), `0x1000c190`, `0x1000c260`, `0x10009ef0`, and one vtable call `call dword ptr [eax + 0xac]` at `0x10005b37` (slot `+0xac` on whatever `eax` points to — forwards to another object's method).

**Widened test-family grep.**

Across **all three function bodies** (3850 bytes of code total), the complete set of `test (byte|word) ptr` / `cmp (byte|word) ptr` hits is:

| Function | Address | Instruction | Shape |
|---|---|---|---|
| **`0x10003d90`** | **`0x1000476e`** | **`test byte ptr [ecx + edx*4 + 3], 4`** | **bit-2 test (mask `0x04`) on byte +3 of a 4-byte-stride array entry; the only true bit-mask test in all three functions** |
| `0x10004890` | `0x1000499a` | `cmp byte ptr [eax], 0` | byte-is-nonzero on a bare pointer (plain tag/flag check inside the linked-list helper block at `0x1000497e..0x10004a57`) |
| `0x10004890` | `0x100049b4` | `cmp byte ptr [ecx], 0` | same, companion check in the same helper block |
| `0x10004890` | `0x10004a03` | `cmp byte ptr [ecx], 0` | same, third site in the same helper block |
| `0x10005890` | `0x10005b0b` | `cmp byte ptr [ebx], dl` | byte-match inside a backward loop (`dec ebx; inc eax; cmp eax, esi; jl 0x10005b0b`) — a string/byte-run compare, not a bit test |

`movzx` hits: **none** of the kind "load a sub-cell u16 and mask it to bits 0..2". `bt/btr/bts`: zero hits in all three bodies. `and reg, imm` with small masks: fn1 and fn3 use `and reg, 0x80000003 | or reg, 0xfffffffc` (sign-preserving mod 4) and `and reg, 0x80000007 | or reg, 0xfffffff8` (mod 8), plus fn3 uses `and reg, 0x8000000f | or reg, 0xfffffff0` (mod 16) — these are MSVC's signed-mod-N expansions for coordinates, not bit tests. fn2 has one `and ecx, 0xfc07` at `0x100048b1` — the same `& 0xfc07` mask §3.4.1 noted for `0x10003780`'s sub-cell write. **Correction (round al reviewer, 2026-10-24):** the masked value IS written back to memory at `0x100048ba` (full pattern: `mov cx, [eax+2]` @ `0x100048ab` → `and ecx, 0xfc07` @ `0x100048b1` → `or ecx, 7` @ `0x100048b7` → `mov [eax+2], cx` @ `0x100048ba`) — exactly the `(v & 0xfc07) | 7` sub-cell-mark pattern §3.3.2 named for `0x10003780`. This strengthens the family connection: fn2 is not only a graph-visited helper (the three byte-flag check-and-set sites) but ALSO writes the same sub-cell visited mark `0x10003780` writes.

**`0x1000476e` in context (D).** The test sits 91 % through fn1 (`0x1000476e - 0x10003d90 = 0x9de` of ~`0xad8` bytes), inside a `esi`-walked list loop that starts at `0x1000474f`:

```
10004752  cmp   esi, ebp                     ; sentinel = ebp (saved at entry)
10004754  je    0x10004855                   ; loop exit
1000475a  xor   ecx, ecx
1000475c  mov   cx,  word ptr [esi + 0x14]   ; cx  = u16 at [record + 0x14]
10004760  lea   eax, [esi + 0x10]
10004763  xor   edx, edx
10004765  mov   dx,  word ptr [eax + 6]      ; dx  = u16 at [record + 0x16]
10004769  lea   edx, [ecx + edx*4]           ; edx = cx + dx*4   (combined 2-D index)
1000476c  mov   ecx, dword ptr [eax]         ; ecx = pointer at [record + 0x10]
1000476e  test  byte ptr [ecx + edx*4 + 3], 4
10004773  lea   ebx, [ecx + edx*4]
10004776  je    0x10004794                   ; bit clear → next step of the loop
10004778  push  eax                          ; bit set   → call deeper
10004779  mov   ecx, edi                     ; this  = edi (edi = the big class saved at entry)
1000477b  call  0x10003220                   ; a ~290-byte helper that takes this + a struct
10004780  test  eax, eax
10004782  jne   0x10004794                   ; non-zero → still just continue
10004784  mov   al,  byte ptr [edi + 0x438]  ; a byte flag on class Y near [+0x430] (world ptr)
1000478a  test  al,  al
1000478c  je    0x10004855                   ; flag clear → exit loop
10004792  jmp   0x100047e4                   ; flag set   → the "do work" branch
```

**Reading of the shape (not yet confirmed, see caveat below).** Each `esi` record carries `(pointer at +0x10, u16 at +0x14, u16 at +0x16)` — the pointer looks sized for a bulk array, the two u16s are an index pair, and the stride of 4 at `[ecx + edx*4 + 3]` reads byte +3 (the high byte in little-endian) of a `u32` entry at `edx` in that array. This matches a per-tile-with-flag-byte layout (3 payload bytes + 1 flag byte). Mask `0x04` picks bit 2 of that flag byte. The record layout `(ptr, u16, u16)` combined with the `cx + dx*4` index is also consistent with a 4-cells-wide sub-grid lookup — i.e. the chunk-and-sub-cell shape §3.4.1 described.

**Caveat (why this is a candidate, not a confirmation).** The array at `[esi + 0x10]` has NOT been identified. It could be:
1. a chunk-data pointer written by `0x10003780`'s mark path (making bit 2 of the high byte a walkability bit **outside** the `0x7` visited-tag that `0x10003780` writes — compatible with §3.4.3's "preserved bits" theory), OR
2. a flag table belonging to the `esi` record's own owner (not a sub-cell block at all), OR
3. a per-type property array (a type's walkable-terrain mask).

The `edi + 0x438` byte read right after the deeper-helper call is suggestive: it is 8 bytes past the known `[+0x430]` world pointer in class Y, which is the right neighborhood for a search-engine state flag. But none of this nails which array the primary test reads.

**fn3 (`0x10005890`) is a clean miss for a walkability test.** Its only `cmp byte ptr` is a byte-run compare at `0x10005b0b` (adjacent `dec ebx; inc eax; cmp eax, esi; jl` pattern). The rest of the function is dominated by float-coordinate munging (`fld; fdiv [0x100c1138]; fisub [edi+0x4ec]; __ftol`), integer `div 3` (via `imul 0x55555556`), and `and reg, 0xffff` masking of two 16-bit coordinates — i.e. this is the "float world position → tile and sub-cell indices" conversion, not the yes/no reader. It later calls `[eax + 0xac]` through a vtable (slot `+0xac` on another object), which may be where the yes/no actually lives.

**fn2 (`0x10004890`) is a linked-list helper of fn1.** Its three `cmp byte ptr [reg], 0` sites are in a tight block (`0x1000497e..0x10004a57`) that reads a byte flag at the head of two neighbouring nodes and sets `byte [eax], 1` / `byte [ecx], 1` when both are 0 (`c60001` and `c60101` at `0x100049b9` / `0x100049bc`) — a classic "mark both visited" step on a graph edge. The byte at offset 0 of each node is a 0/1 visited flag, not a sub-cell walkability bit.

**Updates to prior sections.**
- §3.4.3 named `0x10003d90` without a vtable slot. **D here:** its `.rdata` reference is at `0x100c1568`, in the sub-vtable `0x100c14e4` (class Y's `[+4]` interface), at slot offset `+0x84` (= slot 33 of that second interface). §3.4.3 should be amended at the end of this round.
- §3.4.3 said the vtable `0x100c1424` has "47 slots"; the raw dump shows 48 code slots (0x00..0xbc, i.e. indices 0..47). The slot count is 48, index range 0..47. Non-slot-material: nothing in §3.4.3's reasoning depends on this number.
- §3.4.3's option 1 ("a broader grep that doesn't anchor to a vtable slot but to the chunk-data pattern") is the branch this task walked. The one hit it produced (`0x1000476e`) is the current best candidate for the walkability yes/no, but the structure it reads is unidentified. §3.4.3's option 4 ("the chunk-fetch handler itself tests walkability as part of filling the chunk") remains the obvious fallback.

**OQ-175: see `docs/open-questions.md` — further narrowed, not closed.** The only byte-mask bit test in the entire 3850-byte search-core trio is `0x1000476e`'s `test byte ptr [ecx + edx*4 + 3], 4` inside `0x10003d90`. Confirming or disproving that this reads walkability data requires identifying the array behind `[esi + 0x10]` in the record list fn1 walks, which calls for a one-caller trace of **fn1 itself** (who passes `[ebp+8]` into it) and/or a Wine memory read of the array at that pointer during a running cart search. **Next address** (natural follow-up): find callers of `0x10003d90` via its `.rdata` slot `0x100c1568` (one hit in the sweep so far — the vtable itself; the actual indirect call site(s) through `[this+4][+0x84]` on class Y need a `[this+4]` sweep over AnnoWelt and the game DLLs). Fallback **T30b.3g** (world chunk-fetch handlers `0x100abdb0` / `0x100a52d0` on `AnnoWelt.dll` and the world object — if filling filters out non-walkable cells before the search sees them, that is option 4 and closes OQ-175 without a per-cell reader).

#### 3.4.5 The chunk-grid on `[Y + 4]` and bit 10 of the sub-cell `u16` as the walkability flag (T30b.3h, 2026-10-24)

**D.** Static read of fn1's loop control, fn2's prologue (the fn1 → fn2 call site), and `0x100064b0` (the chunk fetcher). The three pieces fit end-to-end and identify the array behind `[esi + 0x10]` without a Wine run.

**fn1's loop (D, `0x10004749..0x10004854`).** The list sentinel is `[edi + 0x418]` = `[this + 0x418]` on class Y (edi here is `this`, saved at `[esp+0x10]` by the prologue and reloaded earlier in the function):

```
10004749  mov   ebp, [edi + 0x418]   ; ebp = sentinel = &list head (= [Y + 0x418])
1000474f  mov   esi, [ebp + 8]       ; esi = sentinel.next
10004752  cmp   esi, ebp             ; loop while esi != sentinel  (CIRCULAR LIST)
10004754  je    exit
...
1000476e  test  byte ptr [ecx + edx*4 + 3], 4
...
10004844  mov   ecx, [edi + 0x418]
1000484a  mov   esi, [ecx + 8]       ; same reload: sentinel.next
1000484d  cmp   esi, ebp
1000484f  jne   1000475a             ; loop
```

So the list rooted at `[Y + 0x418]` is a **circular doubly-linked list** with `.next` at `+8`, and each node carries `(ptr at +0x10, u16 at +0x14, u16 at +0x16)`. The `+0xc` slot is `.prev` (set by the splice helpers in `.text` right after fn2).

**fn2's prologue is the node constructor (D, `0x10004890..0x10004907`).** fn2 is `__thiscall(this, arg0, arg1, arg2)` with `ret 0xc`. Its very first act is to write the sub-cell mark, and its very last act (before the list splice) is to write the node's three identifying fields:

```
10004890  push ebx
10004891  push ebp
10004892  mov   ebp, [esp + 0xc]     ; arg0  = base pointer
10004896  push esi
10004897  mov   esi, [esp + 0x14]    ; arg1  = x-coord (u16-wide)
1000489b  push edi
1000489c  mov   edi, [esp + 0x1c]    ; arg2  = y-coord (u16-wide)
100048a0  mov   ebx, ecx             ; ebx   = this  (= class Y)
100048a2  lea   eax, [esi + edi*4]   ; eax   = arg1 + arg2*4             (the SAME index as fn1 line 10004769)
100048a5  lea   eax, [ebp + eax*4]   ; eax   = arg0 + (arg1 + arg2*4)*4  (= 4-byte entry inside the chunk)
100048a9  xor   ecx, ecx
100048ab  mov   cx,  word ptr [eax + 2]   ; the u16 at byte +2 of that entry
100048af  xor   edx, edx
100048b1  and   ecx, 0xfc07
100048b7  or    ecx, 7
100048ba  mov   word ptr [eax + 2], cx    ; (v & 0xfc07) | 7  — the sub-cell visited mark
...
100048f8  mov   dword ptr [eax + 0x10], ebp   ; new_node.+0x10 = arg0  (the base pointer)
100048fb  mov   word  ptr [eax + 0x18], dx    ; new_node.+0x18 = 0     (score / parent / tag field)
100048ff  mov   word  ptr [eax + 0x14], si    ; new_node.+0x14 = arg1  (x)
10004903  mov   word  ptr [eax + 0x16], di    ; new_node.+0x16 = arg2  (y)
```

**So the array behind `[esi + 0x10]` is literally arg0 of fn2** — the base pointer the search engine pushed when it created the node. **The two `u16`s at `[esi + 0x14] / [esi + 0x16]` are the x,y coordinates passed to fn2.** fn1's `cx + dx*4` index is byte-identical to fn2's `arg1 + arg2*4`, with the same `*4` stride applied to the base, giving the same 4-byte entry both times.

**The fn1 → fn2 call site (D, `0x10003e74..0x10003e8d`).** Walking back from the call that creates each node, the arguments to fn2 are built out of two sub-steps:

```
10003e74  push edx                  ; y_aligned = y - (y mod 4)   (signed-mod expansion above)
10003e75  push eax                  ; x_aligned = x - (x mod 4)
10003e76  mov   eax, [esp + 0x18]   ; eax = [esp + 0x18] = this  (saved at [esp+0x10] in the fn1 prologue;
                                    ;                             +0x8 shifted after push edx; push eax)
10003e7a  lea   ebp, [eax + 4]      ; ebp = &Y + 4                 (second-interface adjusted this)
10003e7d  mov   ecx, ebp
10003e7f  call  0x100064b0          ; eax = CHUNK pointer for the 4-aligned (x,y)
10003e84  test  eax, eax
10003e86  je    skip                ; NULL chunk → no mark
10003e88  push  ebx                 ; arg2 = y_mod_4                (y - y_aligned)
10003e89  push  edi                 ; arg1 = x_mod_4                (x - x_aligned)
10003e8a  push  eax                 ; arg0 = chunk pointer
10003e8b  mov   ecx, ebp            ; this = &Y + 4
10003e8d  call  0x10004890          ; fn2 — mark + push node
```

**`0x100064b0` is a chunk fetcher on `&Y + 4` (D).** `xref.py --reloc game/app/AnnoWelt.dll 0x100064b0 0x100064b1` returns **zero** `.rdata` references — `0x100064b0` is NOT a vtable method. It is called directly as `__thiscall` on `(&Y + 4)` from six sites inside class Y's own code: `0x10003e7f`, `0x1000405c`, `0x1000424a`, `0x10004476`, `0x10004726`, `0x10005669` (all inside `AnnoWelt.dll`). Its body reads `[ecx + 4]` and `[ecx + 8]` as integer dimensions (not pointers), bounds-checks `x_chunk = x/4` and `y_chunk = y/4` against them, computes a super-block index `(y_chunk/8)*16 + (x_chunk/8)`, and returns `[ecx + 0x14 + superblock*4]` — the pointer to a chunk-level record for the super-block covering `(x, y)`. The result is the pointer fn1 later walks the sub-cells of. **`[Y + 4]` is therefore a non-polymorphic sub-object (second-interface base of class Y) with layout:**

| offset | contents |
|---|---|
| `(Y+4) + 0x00` | `0x100c14e4` (vtable, §3.4.3) |
| `(Y+4) + 0x04` | chunk-grid width in chunk units (= map-width / 4) |
| `(Y+4) + 0x08` | chunk-grid height in chunk units |
| `(Y+4) + 0x14 + superblock*4` | super-block pointer table (one entry per 32×32-tile super-block, indexed `(y_chunk/8)*16 + (x_chunk/8)` — a 128-wide grid of 32×32-tile super-blocks is the apparent default; the `16` is a hard-coded super-row stride, see note below) |

**Note on the hard-coded 16.** The super-row stride `shl esi, 4` at `0x1000650b` is a literal `*16` and is NOT read from an `[ecx + …]` field. This locks the super-block grid at **16 super-blocks wide** = **16 × 32 = 512 tiles wide** in the original. Any map at or below 512 tiles across (two of the three original scenarios the executor sampled fit easily) uses this grid; a wider map would overflow it. This is a hard-coded original-game constraint — a "no hardcoded limits" entry (`docs/open-questions.md`): the C++ port must configure the super-row stride from the chosen map size, not inherit `16`.

**Each chunk the search operates on is 4 × 4 tiles of 4 bytes each (= 64 bytes).** fn2's `[arg0 + (arg1 + arg2*4) * 4]` indexes a 16-entry table with 4-byte stride, where `arg1, arg2 ∈ [0..3]`. fn1 does the identical index with the identical stride.

**Bit 10 of the sub-cell `u16` at offset +2 is the walkability flag (strong candidate, D-on-the-shape).** fn1 tests **byte +3 bit 2** = **bit 10 of the `u16` at bytes +2..+3**. The three known marks of the family write the u16 at +2 with:

| writer | mask preserved | bits set |
|---|---|---|
| `0x1000dd60` (§3.3.1) | `0x8007` | `| 7` |
| `0x10003780` (§3.3.2) | `0xfc07` | `| 7` |
| `0x10004890` (fn2, §3.4.4) | `0xfc07` | `| 7` |

Bit 10 (= `0x0400`) is **preserved by every one of these marks**: `0x0400 & 0xfc07 = 0x0400` and `0x0400 & 0x8007 = 0`-wait, `0x0400 & 0x8007 = 0`. Only the `0xfc07` writers preserve bit 10; `0x1000dd60` CLEARS it (its mask only preserves bit 15 and bits 0..2). Consistent reading: **bit 10 is set once by the chunk-fill (likely at world-load / scenario-load, outside the search), preserved by `0x10003780` and `0x10004890` which are the search-engine marks, and used by fn1 as the walkability yes/no.** `0x1000dd60`'s wider clear is on a DIFFERENT chunk layout — on the X-class 8×8 chunks of §3.3.1 — not on the Y-class 4×4 chunks fn2 writes.

**Who sets bit 10?** `0x10003780` (§3.3.2) is `[Y]+0xbc` and `0x10004890` is called from fn1 (`[Y]+0x84` via the second interface); both do `| 7`, never `| 0x400`. So bit 10 does not get set anywhere in `AnnoWelt.dll`'s search path. It must be written at **chunk allocation / fill time** by the world's chunk-fetch handlers `0x100abdb0` / `0x100a52d0` (T30b.3g territory) or at scenario-load from `Welt.dat`. The static evidence does not fully close this — the chunk-fill side is the natural next sub-task.

**Caveats.**
1. The claim "bit 10 is the walkability bit" is now the **leading hypothesis**, supported by: (a) the only byte-mask bit test in the entire search core reads it; (b) both search marks preserve it; (c) the structural interpretation of fn2 as the node-pusher + sub-cell visitor-mark is now fully D (not inferred). What is NOT yet D: that the game ever sets bit 10 to 1 in the chunk data at run time, and that it does so based on the terrain/`Welt.dat` walkability. Those need either a Wine break-and-read at `0x1000476e` to confirm the test is `true` on walkable tiles, or a trace of the chunk-fill side.
2. The chunk-base register in fn1's test — the pointer at `[esi+0x10]` — is `arg0` passed to fn2, which is the `0x100064b0` return. But fn2 only wraps `(mark; push)`; fn1's **main-loop state transitions** might also push nodes with other base pointers via a different code path. A quick scan of fn1 for `call 0x10004890` returned exactly ONE site (`0x10003e8d`), so every node on the list was built with a chunk pointer from `0x100064b0`. **The array behind `[esi+0x10]` is a Y-class 4×4-tile chunk pointer, in every case.**

**Implications for the port.**
- The search's walkability yes/no reads **bit 10 of a `u16` at byte +2 of each 4-byte sub-cell** of a 4×4 chunk, with 32×32-tile super-blocks held in a `(Y+4)+0x14` table indexed by `(y/32)*16 + (x/32)`. This is the pathfinder's **tile-level walkability oracle** — the one the sim needs a faithful replacement for.
- `[Y + 0x418..]` is the search's **open/closed list** (circular doubly-linked), with the sentinel at `[Y + 0x418]`; nodes carry `(chunk_ptr, x_mod_4, y_mod_4, u16_score)` at `(+0x10, +0x14, +0x16, +0x18)`.
- fn2's `(v & 0xfc07) | 7` writes bits 0..2 as a **visited tag** on the sub-cell; the search re-finds visited cells by this tag (hence the three `test reg, 7` MSVC-mod-8-coord false positives of §3.4.3 — the real visited-check is this test-and-set in fn2, not a `test reg, 7`).
- The `[Y + 0x438]` byte flag read at `0x10004784` (right after the test-byte branch) is the search's **outer continue/abort flag** — the §3.4.3 observation of a "byte flag 8 bytes past the world pointer" is `+0x438` relative to class Y, not to the world. Class Y is at `world + 0x6aa8` (T30b.3e), so this is `world + 0x6ae0` — a flag on the search engine itself, not on the world.

**OQ-175 moved from "narrowed" to "answered subject to Wine confirmation".** The static read is complete; a one-shot Wine read of the chunk at `[esi + 0x10]` during a running cart search would confirm bit 10 as the walkability bit. **Next address:** T30b.3g (world chunk-fetch handlers `0x100abdb0` / `0x100a52d0` — now relevant for the ORIGIN of bit 10, i.e. who sets it, not just as a fallback). Also a very small Wine subtask: break at `0x1000476e`, snapshot `[esi+0x10]`, confirm bit 10 is set on walkable tiles and clear on blocked tiles.

#### 3.4.6 The orchestrator's read after T30b.3h (2026-10-25 an): the search's main loop, the expansion functions, the chunk fill

Re-disassembled by the orchestrator (`tools/disasm.py`, `pefile`), D unless marked. Every claim of §3.4.4 / §3.4.5 re-read here holds at the instruction level (the bit test at `0x1000476e`, fn2's prologue and mark, the call site `0x10003e74..0x10003e8d`, `0x100064b0`'s `shl esi, 4` at `0x1000650b`). What the two sections did not read changes the interpretation:

- **fn1's loop (`0x1000475a..0x1000484f`) is the search's main loop over the open list, and bit 10 is a branch selector, not the yes/no.** Per node (`+0x10` chunk pointer, `+0x14` / `+0x16` the cell's x / y inside the 4×4 chunk): if **bit 10 of the cell's u16 at +2 is set**, `call 0x10003220(node)`; a non-zero result falls through to the code path below, a zero result tests the byte `[this + 0x438]` (fn1 sets it to 1 in its prologue, `0x10003db1`) — zero → leave the loop with `eax = 0` (`0x10004855`), non-zero → the common tail. If **bit 10 is clear** (or `0x10003220` returned non-zero): `code = u16 & 0x3ff`; **codes `0x80..0x86` go through the jump table `0x10004868`** → `0x80` → `0x100047c5` (calls `0x10007b00`), `0x82` → `0x100047ba` (`0x10007770`), `0x84` → `0x100047d0` (`0x10007ea0`), `0x86` → `0x100047af` (`0x100071c0`); `0x81`, `0x83`, `0x85` and every other code → `0x100047db` (**`0x10006a40`, the default expansion**). Each is called `__thiscall` with `ecx = edi + 4` and the node pushed. The common tail `0x100047e4..0x1000484f` unlinks the node from the open list (`0x10001480`), decrements the open count `[this + 0x420]`, pushes the node on the free list `[this + 0x424]` / `[this + 0x428]` (count `[this + 0x42c]`), and loops while the list head `[[this + 0x418] + 8]` is not the sentinel. **So the walkability decision is inside the expansion functions, with `0x10006a40` the one that handles ordinary cells.** The `this` of fn1 is `Y + 4` (fn1 is slot `+0x84` of the vtable `0x100c14e4` that the constructor installs at `[Y + 4]`, `0x10006375`), so fn1's `[this + k]` is `[Y + 4 + k]`; the `this` it hands the expansion functions is 4 bytes further again (`lea ecx, [edi + 4]`; which register `edi` holds at that point is T30b.3i's first table row). Every field offset in this family has to be written as an absolute `Y + n` before it is compared with another function's.
- **`0x10006a40` (the default expansion), first 120 bytes:** `ebp = [node + 0x10]` (the chunk), `cx = [node + 0x16]` (y), `ax = [node + 0x14]` (x); `bx = [cell]` (the **low** u16 of the 4-byte cell), `bx >>= 12` (its top nibble); if `y > 0`: `test word [cell − 0x10], 0x2111` (the cell one row up — 4 cells × 4 bytes; the mask `0x2111` = bits 0, 4, 8, 13 of its low u16) → `je 0x10006c44` (skip that neighbour); else `ax = [cell − 0x10 + 2]` (the neighbour's high u16), `code = ax & 0x3ff`, `dl = (bl & 8) << 3` (`0x40` if bit 3 of the current cell's top nibble is set, else 0), **`test byte ptr [[this + 0x42c] + code], dl`** — a byte table indexed by the neighbour's 10-bit code, tested with a bit chosen from the current cell. That is the shape of a per-layer walkability table; `[this + 0x42c]` resolves to an absolute `Y` offset once `this` is known (likely `Y + 0x434`, which `0x10002b80` — the primary vtable's slot `+0x50`, `this = Y` — writes from its argument: `mov [esi + 0x434], edi` at `0x10002b9e`, then calls that object's slots `+0x50` and `+0x58` as width / height; T24 called it "the island as the cell provider"). Reading the rest of `0x10006a40` (the other three neighbours, the costs, the enqueue) is T30b.3j; what `[Y + 0x434]` points to and where its byte table comes from is T30b.3k.
- **`0x100064b0` (the chunk lookup), the part §3.4.5 did not read (`0x1000655a..0x100066a7`):** the super-block pointer `[sub + 0x14 + sb·4]` is allocated on first use (a `0x100`-byte block = 64 chunk pointers, zero-filled by `rep stosd` with `ecx = 0x40`); inside it the chunk pointer sits at `[sb_ptr + ((y_chunk & 7)·8 + (x_chunk & 7))·4]`; if that is null and the byte `[sub + 0xc]` is zero the function returns 0 (no chunk, nothing made); otherwise it allocates a **`0x44`-byte chunk** from the pool at `[sub + 0x10]` (records of `0x2000` bytes carved in `0x44` steps), writes the chunk's x and y as u16 at `[chunk + 0x40]` / `[chunk + 0x42]`, and **calls `[[sub]] (chunk)`** — slot 0 of the vtable at `[Y + 4]` (`0x100c14e4` slot 0 = `0x100b4a20`, a stub?, to be checked: the call is `mov eax, [ecx]; push edi; call [eax]` with `ecx = sub = Y + 4`) — **that call fills the 16 cells (64 bytes at `chunk + 0 .. 0x3f`) from the world**: the fill chain T30b.3k follows. So the cells' bits (the low u16 with its top nibble and the `0x2111` bits, the high u16 with the 10-bit code and bit 10) are written by the fill, and bit 10 is never set by the search itself (§3.4.5 saw that correctly).
- **`0x100a52d0` (world vtable `0x100ccdd8` slot `+0x128`, which `0x10003780` calls through `[Y + 0x430]` = the world) is "the object at tile (x, y)"**: chunk index from `[world + 0xec]` (row stride) and `x / 8`, `y / 8`; the byte at `[world + 0x130 + index]` selects an entry of the pointer table at `[world + 0x6530 + 4·byte]` (likely the islands, by number); on it slot `+0xec(x, y)` returns a 4-bit value, `0xf` meaning none; else slot `+0xf8(value)` returns an object, whose slot `+0x1c()` result is returned. It is a lookup, not a fill, so T30b.3g's premise ("pre-filter at fill time") does not hold for this function; T30b.3g is dropped.

**What this means for OQ-175 (status back to "narrowed"):** the 4-byte cell of the search engine's 4×4 chunks is the unit of the walk rule. Its low u16 carries a top nibble and the bits `0x2111` that the default expansion tests on a neighbour; its high u16 carries a 10-bit code (`0x80..0x86` select four special expansions, everything else the default) plus bit 10 (the `0x10003220` branch) and the three state bits `| 7` of the marks. The yes/no for an ordinary step is `table[code] & bit`, with the table behind `[this + 0x42c]` of the expansion's `this`. Three readings remain: the rest of `0x10006a40` (T30b.3j), the fill that writes the cells from the island's tiles (T30b.3k: the slot-0 call, the world handler kind, the copy routine, and `0x1008ff70`'s `table[mask] | byte << 16` of §3.1 (b), which now reads as "low u16 = the mapped 9-bit mask, high byte = a tile byte" — exactly the cell), and `0x10003220` with the four special expansions (T30b.3l). The named-road question (OQ-171) is answered where the fill turns a road tile's record into the low u16's bits.

#### 3.4.7 fn1 as pseudo-code: `this`, absolute `Y` offsets, the node, the loop (T30b.3i, 2026-10-25)

**D.** `tools/disasm.py` on `AnnoWelt.dll` `0x10003d90` (2800 bytes; one `ret` at `0x10004867`) and `0x10004890` (fn2, through the node writes at `0x100048f8..0x1000498a` and the `0x1c` allocation at `0x100048c6`). Jump table at `0x10004868` read as seven dwords (`pefile`). Constructor vtable writes `0x10006375` / `0x1000637c` / `0x10006401` re-read. Ranges named below; no listing of the original.

**First row — `this` and the registers (D).**

| Item | Value | Evidence |
|---|---|---|
| fn1's `this` | `Y + 4` | fn1 is slot `+0x84` of vtable `0x100c14e4` (`pefile`: dword at `0x100c14e4 + 0x84` = `0x10003d90`). The constructor writes that vtable at `[Y + 4]` (`0x10006375`). Called through that slot, `ecx` is the address of `[Y + 4]`. |
| Who holds `this` | `esi` after `0x10003d99` `mov esi, ecx`. Saved at `[esp + 0x10]` at `0x10003dad`. That slot is written once in the whole function. | Prologue `0x10003d90..0x10003dad`. |
| Where `edi` is loaded before the loop at `0x1000475a` | `0x100046cf` `mov edi, [esp + 0x10]` | Between that load and `0x1000475a`, `edi` is not reassigned. The next `edi` write in the function is `pop edi` at `0x1000485d` (epilogue). |
| What `edi` is at `0x1000474f` | fn1's `this` = `Y + 4` | Same saved slot. At `0x1000474f` the instruction is `mov esi, [ebp + 8]` (the list head); `edi` is already `this`. |
| Expansion `this` | `Y + 8` | Each expansion is `lea ecx, [edi + 4]` then `call` (`0x100047b0`, `0x100047bb`, `0x100047c6`, `0x100047d1`, `0x100047dc`). With `edi = Y + 4`, `ecx = Y + 8`. |
| `0x10003220`'s `this` | `Y + 4` | `0x10004779` `mov ecx, edi` — the same `this` as fn1, not the expansion's. |
| `0x100064b0` / fn2 `this` | `Y + 8` | Call site `0x10003e7a` `lea ebp, [eax + 4]` with `eax` = `[esp + 0x10]` after the two argument pushes = fn1's `this`; `mov ecx, ebp` at `0x10003e7d` / `0x10003e8b`. Same `lea ecx, [reg + 4]` shape at the later `0x100064b0` sites (`0x10004059`, `0x10004247`, `0x10004723`). |

So fn1's `[this + k]` is `[Y + 4 + k]`. §3.4.5's labels that wrote `[Y + 0x418]` / `[Y + 0x438]` for those fields were fn1's `[this + 0x418]` / `[this + 0x438]` named as if `this` were `Y`. The absolute values are the next table.

**fn1's fields as absolute `Y` offsets (D).**

| fn1 `[this + k]` | Absolute | Who | Role in fn1 |
|---|---|---|---|
| `+0x418` | `Y + 0x41c` | `0x10004749` load sentinel into `ebp`; `0x100047e4` / `0x10004844` again | Open-list sentinel. Circular: `.next` at sentinel `+8`. Loop while `[[Y + 0x41c] + 8] != sentinel`. |
| `+0x420` | `Y + 0x424` | `0x100047fc` load, `dec`, `0x10004806` store | Open count. Decremented in the common tail after a node is unlinked. |
| `+0x424` | `Y + 0x428` | `0x1000480c` / `0x10004821` / `0x10004838` | Free-list head. Tail pushes the unlinked node here. |
| `+0x428` | `Y + 0x42c` | `0x1000481b` | Free-list other end: written with the node when the head was null. |
| `+0x42c` | `Y + 0x430` | `0x10004831` load, `inc`, `0x1000483e` store | Free-list count. Incremented when a node is pushed on the free list. |
| `+0x438` | `Y + 0x43c` | prologue `0x10003db1` writes `1`; loop `0x10004784` tests | Continue/abort byte. Zero after `0x10003220` returned 0 → leave the loop. |
| `+0x439` | `Y + 0x43d` | prologue `0x10003db8` writes `0`; `0x10004857` reads into `al` | The value fn1 returns (zero-extended in `eax`). fn1 itself has no other write of this byte. |
| `+0x444` | `Y + 0x448` | prologue `0x10003dbf` writes `0` | Best score, same as §1.1 / §3. |
| `+0x4f0` / `+0x4f4` | `Y + 0x4f4` / `Y + 0x4f8` | `0x10003d9b` / `0x10003da2` | Count and base of the first pre-loop array (8-byte float pairs). |
| `+0x4e8` / `+0x4ec` | `Y + 0x4ec` / `Y + 0x4f0` | `fisub` at `0x10003de9` / `0x10003e03` and the later walks | Integer origin subtracted from each float after `fdiv` of `1.0f` at `0x100c1138` (`pefile`, four bytes `00 00 80 3f`). |
| `+8` / `+0xc` | `Y + 0xc` / `Y + 0x10` | `0x10003e1c` / `0x10003e39` | Bounds the first walk compares against (`[this + 8] << 2`, `[this + 0xc] << 2`). |
| `+0x574` / `+0x578` | `Y + 0x578` / `Y + 0x57c` | `0x10003f91` / `0x10003f97` | Count and base of the second pre-loop array (also 8-byte float pairs). |
| `+0x17fc` / `+0x1800` | `Y + 0x1800` / `Y + 0x1804` | `0x1000416f` / `0x10004169` | Count and base of the third pre-loop array (stride 4). |
| `+0x1c04` / `+0x1c08` | `Y + 0x1c08` / `Y + 0x1c0c` | `0x100046d3` / `0x100046d9` | Count and base of the last pre-loop array (stride `0xc`). |

fn2 is called with `this = Y + 8`. Its list fields then sit on the same absolute offsets as fn1's: `[fn2 + 0x414]` = `Y + 0x41c` (fn1's sentinel), `[fn2 + 0x41c]` incremented at `0x10004a64` = `Y + 0x424` (fn1's open count), `[fn2 + 0x420]` / `+0x424` / `+0x428` = fn1's free-list triple. That is the this-adjustment check: the two functions share the lists only after both `this` pointers are written as `Y + n`.

The free-list count at `Y + 0x430` is the same absolute offset T30b.3b named as the world pointer on `0x10003780`. That function is in two vtables (`0x100c1424 + 0xbc` and `0x100c14e0 + 0`, both `pefile`); the world-pointer read is `[0x10003780's this + 0x430]`, which is `Y + 0x430` only if that call uses the primary vtable (`this = Y`). Not resolved here; fn1's use of `Y + 0x430` as an integer count is D.

The expansion's `[this + 0x42c]` of §3.4.6, with expansion `this = Y + 8`, is `Y + 0x434`. That is the address T30b.3j must use for the byte table.

**The node record (D, size `0x1c`).**

Allocation at `0x100048c6` `push 0x1c` / `call 0x100b5f80` when the free list is empty (`[fn2_this + 0x428] == 0`); otherwise the head of that list is reused (`0x100048d4..0x100048f2`). Writes below are fn2 unless named.

| Offset | Size | Who writes | Who reads | Meaning |
|---|---|---|---|---|
| `+0` | byte | fn2 `0x1000498a` writes `0`; later `0x100049b9` / `0x10004a59` write `1` | fn2's three `cmp byte [reg], 0` sites | 0/1 flag on the node (the graph-visited byte of §3.4.4). |
| `+4` | ptr | fn2 `0x10004978` | fn2's tree walk; `0x10001480` | Tree parent (score-ordered insert). |
| `+8` | ptr | fn2 `0x1000497b` writes `0`; splice writes; fn1 tail `0x1000482a` writes `0` | Open-list `.next`; fn2 tree; `0x10001480` | Next / left. |
| `+0xc` | ptr | fn2 `0x1000497e` writes `0`; fn1 tail `0x10004827` | Open-list `.prev`; fn2 tree; `0x10001480` | Prev / right. |
| `+0x10` | ptr | fn2 `0x100048f8` = arg0 | fn1 `0x1000476c` | Chunk pointer from `0x100064b0`. |
| `+0x14` | u16 | fn2 `0x100048ff` = arg1 | fn1 `0x1000475c` | `x` inside the 4×4 chunk. |
| `+0x16` | u16 | fn2 `0x10004903` = arg2 | fn1 `0x10004765` | `y` inside the 4×4 chunk. |
| `+0x18` | u16 | fn2 `0x100048fb` writes `0` | fn2 `0x10004918` / `0x1000493c` compare during insert | Score used to place the node in the tree at `[fn2_this + 0x414]`. |
| `+0x1a` | 2 bytes | not written in `0x100048f8..0x1000498a` | not read in fn1's loop | Inside the `0x1c` allocation; no use seen in these two functions. |

**The loop as pseudo-code (D, `0x10004749..0x10004867`).** `this` in this block is `edi` = `Y + 4`.

```
sentinel = [this + 0x418]                          # 0x10004749; = [Y + 0x41c]
node = [sentinel + 8]                              # 0x1000474f
if node == sentinel: goto exit                     # 0x10004752

loop:                                              # 0x1000475a
    x = u16[node + 0x14]                           # 0x1000475c
    y = u16[node + 0x16]                           # 0x10004765
    chunk = [node + 0x10]                          # 0x1000476c
    cell = chunk + (x + y*4)*4                     # 0x10004769, 0x10004773
    if byte[cell + 3] & 4:                         # 0x1000476e; bit 10 of the high u16
        r = 0x10003220(this = edi, &node+0x10)     # 0x10004778..0x1000477b
        if r == 0:
            if [this + 0x438] == 0: goto exit      # 0x10004784..0x1000478c
            goto tail                              # 0x10004792; no expansion
    # bit 10 clear, or 0x10003220 returned non-zero:
    code = u16[cell + 2] & 0x3ff                   # 0x10004796..0x1000479a
    if code in 0x80..0x86:
        jmp table[code - 0x80]                     # 0x10004868, seven dwords
    else:
        goto default
    # table (pefile, 0x10004868):
    #   0x80 -> 0x100047c5 -> 0x10007b00(this = edi+4, node)
    #   0x81 -> 0x100047db -> default
    #   0x82 -> 0x100047ba -> 0x10007770(this = edi+4, node)
    #   0x83 -> 0x100047db -> default
    #   0x84 -> 0x100047d0 -> 0x10007ea0(this = edi+4, node)
    #   0x85 -> 0x100047db -> default
    #   0x86 -> 0x100047af -> 0x100071c0(this = edi+4, node)
    default:                                       # 0x100047db
        0x10006a40(this = edi+4, node)

tail:                                              # 0x100047e4
    0x10001480(node, sentinel+4, sentinel+8, sentinel+0xc)
                                                   # 0x100047e4..0x100047f7; unlink
    [this + 0x420] -= 1                            # 0x100047fc..0x10004806
    push node on free list [this + 0x424]/[+0x428] # 0x1000480c..0x1000482a
    [this + 0x42c] += 1                            # 0x10004831..0x1000483e
    node = [[this + 0x418] + 8]                    # 0x10004844..0x1000484a
    if node != sentinel: goto loop                 # 0x1000484d

exit:                                              # 0x10004855 — the only ret path
    return zero_extend([this + 0x439])             # 0x10004857; ret at 0x10004867
```

There is no other `ret`. The empty-list jump (`0x10004754`) and the abort jump (`0x1000478c`) both land at `0x10004855`.

**Before the loop (`0x10003d90..0x1000474f`).**

1. Prologue (`0x10003d90..0x10003dbf`): frame `0x8c`; `esi = this`; `[this + 0x438] = 1`, `[this + 0x439] = 0`, `[this + 0x444] = 0`.
2. First array (`0x10003d9b..0x10003f87`): count `[this + 0x4f0]`, base `[this + 0x4f4]`, 8 bytes per entry (two floats). Each pair is divided by `1.0f` at `0x100c1138`, then `fisub [this + 0x4e8]` / `[this + 0x4ec]`, then `__ftol` (`0x100b5f90`). In-range points go through signed-mod-4 and **the only fn2 call in fn1** (`0x10003e7f` `0x100064b0`, `0x10003e8d` fn2). Those are the start nodes: every later open-list node is a descendant of this one constructor. The same walk then calls `0x10017510`, `0x1000b790`, `0x10003aa0` and `or`s a u16 into a cell (`0x10003f73`).
3. Second array (`0x10003f91..0x1000415f`): count `[this + 0x574]`, base `[this + 0x578]`, same 8-byte float pairs and the same convert. Its `0x100064b0` at `0x1000405c` does **not** call fn2; on a non-null chunk it does `or byte [cell + 3], 4` at `0x10004068` — it **sets** bit 10 of that cell's high u16. §3.4.5's "bit 10 is never set by the search" does not hold for this pre-loop site.
4. Third array (`0x10004165..0x1000433b`): count `[this + 0x17fc]`, base `[this + 0x1800]`, stride 4. Another `0x100064b0` at `0x1000424a` with the same `or byte [cell + 3], 4` at `0x10004256`.
5. A further float walk starting `0x10004368` (`fld` of `[p]`, `[p+4]`, `[p+8]`) calls `0x10008540` and `0x1000d6a0`, then `0x100064b0` at `0x10004476` (chunk `+0x40` / `+0x42` used as the chunk's tile origin; no fn2).
6. Last array (`0x100046cf..0x10004747`): count `[this + 0x1c04]`, base `[this + 0x1c08]`, stride `0xc`. `0x100064b0` at `0x10004726` (`this = edi + 4`); on a non-null chunk, `and word [cell], mask` at `0x10004737` with the u16 from the record. Then the main loop.

**OQ-175.** The loop is now D as above. Bit 10 is a branch selector (set → `0x10003220`; clear → expand), and the pre-loop itself sets that bit on some cells. The yes/no of an ordinary step stays in `0x10006a40` (T30b.3j), with expansion `this = Y + 8` so the byte table is `[Y + 0x434]`.
#### 3.4.8 The default expansion `0x10006a40` (T30b.3j, 2026-10-25)

**D.** `tools/disasm.py` on `AnnoWelt.dll` `0x10006a40`. Two `ret 4` epilogues (`0x10006ecf` for the in-chunk right path, `0x10006f27` for the right-edge path); the `jae 0x10006ed2` at `0x10006e79` crosses the first, so the function runs to `0x10006f27`. Helpers read to their `ret`: `0x10006f30` (`ret 0xc`), `0x10006f70` (allocate / mark / tree-insert), `0x10007180` (pop a `0x1c` node), `0x1000d4e0` (score-tree insert), `0x1000b850` (neighbour chunk by direction). T30b.3i's `edi` at the call site is `Y + 4` (PR #54, `transport.md` §3.4.7); fn1 does `lea ecx, [edi + 4]`.

**`this` and absolute offsets (D).**

| Item | Value | Evidence |
|---|---|---|
| `this` | `Y + 8` | `0x10006a4e` `mov esi, ecx`. Caller: `lea ecx, [edi + 4]` with `edi = Y + 4`. |
| `[this + 0x42c]` | `Y + 0x434` | Byte table. Loaded at `0x10006a8e` and every neighbour's table test. Matches §3.4.6's "likely `Y + 0x434`". |
| `[this + 0x414]` | `Y + 0x41c` | Tree / open-list sentinel. `lea ecx, [esi + 0x414]` then `0x1000d4e0`; `0x10006f70` at `0x1000701d`. Same field as fn1's `[this + 0x418]`. |
| `[this + 0x420]` / `+0x424` / `+0x428` | `Y + 0x428` / `Y + 0x42c` / `Y + 0x430` | Free list. `0x10007180` is called with `lea ecx, [esi + 0x420]`; `0x10006f70` pops the same triple. |
| `[this + 0x444]` | — | **Not read or written** in `0x10006a40` or `0x10006f70`. The best score stays the prologue zero until some other function writes it. |

**Per neighbour (D).** Four orthogonal neighbours. No diagonal call in this function (`0x1000b850` is invoked with directions `0`, `2`, `4` only). Fail = skip that neighbour (`je` to the next). Pass = add a cost byte, mark the neighbour's high u16, allocate a `0x1c` node, insert it on the score tree.

| Dir | In-chunk cell | In-chunk mask (low u16) | Edge (`x`/`y` 0 or 3) | `dl` for `[table + code] & dl` | Cost byte | Mark on high u16 | New node |
|---|---|---|---|---|---|---|---|
| Up | `cell − 0x10` (`0x10006a81`) if `y > 0` | `0x2111` | `y == 0`: `0x10017510` then maybe `0x1000b790`; test `[nbr + x*4 + 0x30], 0x2111` (`0x10006bab`) — the neighbour chunk's row `y = 3` | `(nibble & 8) << 3` = `0x40` or `0` (`0x10006a9f..0x10006aa4`) | `[table + code*8 + 0x406]` (`0x10006ab9`) | `(v & 0xfc86) \| 0x86` (`0x10006adb..0x10006ae5`) | same chunk, `(x, y−1)`; or neighbour chunk, `(x, 3)` |
| Down | `cell + 0x10` (`0x10006c66`) if `y < 3` | `0x8444` (`0x10006c69`) | `y == 3`: `0x1000b850(chunk, 2)` (`0x10006cff`); test `[nbr + x*4], 0x8444` (`0x10006d1b`) — neighbour row `y = 0` | `(nibble & 2) << 1` = `0x04` or `0` (`0x10006c8c..0x10006c91`) | `[table + code*8 + 0x402]` (`0x10006ca5`) | `(v & 0xfc82) \| 0x82` (`0x10006cbe..0x10006cc3`) | same chunk, `(x, y+1)`; or neighbour chunk, `(x, 0)` |
| Left | `cell − 4` (`0x10006dd0`) if `x > 0` | `0x4888` (`0x10006dd0`) | `x == 0`: `0x1000b850(chunk, 0)` (`0x10006e13`); cell at `nbr + y*16 + 0xc` (`0x10006e34..0x10006e37`) — neighbour `x = 3` | in-chunk: `[table + code] & nibble`, then `test dl, 1` (`0x10006df7..0x10006dfc`); edge: `0x10006f30` with `dl = nibble & 1` (`0x10006e2d..0x10006e3b`) | via `0x10006f70` with dir-index `0` → `[table + code*8 + 0x400]` | via `0x10006f70`: low 10 bits become `(old ^ 0x80) & 0x3ff` (`0x10006fb4..0x10006fc4`) | `0x10006f70`: chunk, `(x−1, y)` or `(3, y)` |
| Right | `cell + 4` (`0x10006e94`) if `x < 3` | `0x1222` (`0x10006e8c`) | `x == 3`: `0x1000b850(chunk, 4)` (`0x10006ed2`); `0x10006f30` on that chunk (`0x10006efe`) | `(nibble & 4) << 2` = `0x10` or `0` (`0x10006e81..0x10006e84`); test in `0x10006f30` | `0x10006f70` with dir-index `4` → `[table + code*8 + 0x404]` | same helper: `(old ^ 0x84) & 0x3ff` in the low 10 bits | `0x10006f70`: chunk, `(x+1, y)` or `(0, y)` |

`0x1000b850(this, chunk, dir)` (`0x1000b85b..0x1000b884`) takes `[chunk + 0x40]` / `[chunk + 0x42]` as the chunk's tile origin, `>> 2` to chunk indices, and a jump table at `0x1000c170` on `dir`. `dir = 0` decrements the x-chunk (`0x1000b88b`). It is not `0x100064b0`; it is the neighbour-chunk fetcher.

`0x10006f30(cell, dl, mask)` (`0x10006f30..0x10006f64`): `test [cell] & mask`; then `test [table + (high u16 & 0x3ff)], dl`; returns `1` or `0`.

**Top nibble (`bx >> 12` at `0x10006a70`) — every use (D).** `bx` is the current cell's **low** u16 shifted down 12. It is not reloaded.

| Nibble bit | Value | Use | Where |
|---|---|---|---|
| 3 | `8` | `dl = 0x40` for the up table test | `0x10006a9f`, `0x10006bcd` |
| 1 | `2` | `dl = 0x04` for the down table test | `0x10006c8c`, `0x10006d3c` |
| 0 | `1` | left table test (`[table + code] & nibble` bit 0; edge `dl = nibble & 1`) | `0x10006dfa`, `0x10006e3b` |
| 2 | `4` | `dl = 0x10` for the right table test | `0x10006e81`, `0x10006eea` |

Bit 3 choosing `dl = 0x40` is the up-direction bit of this nibble. The four bits are the current cell's per-direction keys into the neighbour's admission byte.

**Costs and enqueue (D).** There are no immediate cost constants. The cost of a step is a **byte** at `[table + (neighbour_code & 0x3ff)*8 + 0x400 + dir]` (`dir` = 6 up, 2 down, 0 left, 4 right), added to `[node + 0x18]` (`0x10006ac2`, `0x10006cae`, `0x10006fb0`). The new node's `+0x18` is that sum. Enqueue is **not** fn2: `0x10007180` (or the same pop inside `0x10006f70`) takes a `0x1c` record from the free list at `[this + 0x420]`, then `0x1000d4e0` / `0x10006f70`'s tree walk inserts it at `[this + 0x414]` by comparing `+0x18`. `[this + 0x444]` is not touched.

**Pseudo-code (D).** `this` = `esi` = `Y + 8`. `node` = the stack argument (`0x10006a47`).

```
chunk = [node + 0x10];  x = [node + 0x14];  y = [node + 0x16]     # 0x10006a4b..0x10006a58
nibble = low_u16[cell] >> 12                                     # 0x10006a6b..0x10006a70
table = [this + 0x42c]                                           # Y + 0x434

# up
if y > 0:                                                        # 0x10006a74
    try_up(cell - 0x10, same chunk, (x, y-1))                    # 0x10006a81..0x10006c3f
else:
    nbr = lookup_chunk_above()                                   # 0x10006b1b..0x10006b9f
    try_up(nbr + x*4 + 0x30, nbr, (x, 3))                        # 0x10006ba5..0x10006c3f

# down
if y < 3:                                                        # 0x10006c4a
    try_down(cell + 0x10, same chunk, (x, y+1))                  # 0x10006c54..0x10006dad
else:
    nbr = 0x1000b850(this, chunk, 2)                             # 0x10006cff
    try_down(nbr + x*4, nbr, (x, 0))                             # 0x10006d15..0x10006dad

# left
if x > 0:                                                        # 0x10006db8
    if [cell-4] & 0x4888 and (table[code] & nibble) & 1:
        0x10006f70(..., dir=0)                                   # 0x10006e0e..0x10006e6a
else:
    nbr = 0x1000b850(this, chunk, 0)                             # 0x10006e13
    if 0x10006f30(nbr + y*16 + 0xc, nibble & 1, 0x4888):
        0x10006f70(nbr, (3, y), dir=0)                           # 0x10006e49..0x10006e6a

# right
if x < 3:                                                        # 0x10006e75
    if 0x10006f30(cell+4, (nibble & 4)<<2, 0x1222):              # 0x10006ea0
        0x10006f70(chunk, (x+1, y), dir=4)                       # 0x10006ec3
    return                                                       # 0x10006ecf
else:
    nbr = 0x1000b850(this, chunk, 4)                             # 0x10006ed2
    if 0x10006f30(..., (nibble & 4)<<2, 0x1222):
        0x10006f70(nbr, (0, y), dir=4)                           # 0x10006f1b
    return                                                       # 0x10006f27
```

`try_up` / `try_down` are the inlined blocks: mask on the low u16, `test [table + code], dl`, cost byte, mark, `0x10007180`, fill `+0x10/+0x14/+0x16/+0x18`, `0x1000d4e0`.
#### 3.4.9 The fill chain (T30b.3k, 2026-10-25)

**D.** `tools/disasm.py` / `pefile` / `xref.py --reloc` on `AnnoWelt.dll`. The slot-0 call at `0x10006694` does **not** fill the 16 cells and does **not** call the world. The island builder `0x1008ff70` is confirmed; it is not on that path. The loader write that would decide OQ-171 for types `2527` / `2520` was not found at the two named addresses.

**(a) Slot 0 of `[Y + 4]` (D).** Constructor `0x10006344` writes `[esi + 4] = 0x100c14e4` at `0x10006375`. `xref.py --reloc` of `0x100c14e4` is that one hit. A capstone sweep of `mov [reg + 4], imm` in `0x100c1000..0x100c2000` finds no later overwrite of `[Y + 4]`. `0x100066c0` (called first) writes a **byte** to `[esi + 4]`, then `0x10006375` replaces it with the vtable.

| Item | Value | Evidence |
|---|---|---|
| Vtable at `[Y + 4]` | `0x100c14e4` | `0x10006375` |
| Slot 0 | `0x100b4a20` | `pefile` dword |
| Body | `mov eax, 1; ret` | `0x100b4a20..0x100b4a25` |
| Fill call | `mov eax, [ecx]; push edi; call [eax]` | `0x10006694..0x10006697`, `ecx = Y + 4` (reloaded `0x1000665c` / `0x100065b1` from the saved `this`) |
| World call | none | stub has no call |

`0x1000b790` (neighbour-chunk allocate) is the same shape: `call [edx]` on `this` at `0x1000b83d`, same stub. New pool pages come from `0x1000d480` (`push 0x2008; call 0x100b5f80`): the allocator writes `[eax] = 0` and `[eax + 4] = 0` only; it does not write the `0x44`-byte records. After `0x100064b0` returns, fn1's sites at `0x10004068` / `0x10004256` / `0x10004513` **OR bit 10** (`or byte [cell + 3], 4`); they do not store a mapped mask or a tile byte.

**(b) World kind / copy routine — stopped.** There is no kind code and no `0x100abdb0` call on this path. `0x100900c0` / `0x10090350` / `0x100a4730` stay the kind-11 / 10 / 9 copies of §3.3 (`xref` from `0x100abc07` / `0x100abcf8` / `0x100abd83`). `0x100900c0` indexes dest as `*2` (u16 layer), not 4-byte Y cells.

**(c) Cell builder `0x1008ff70` (D, on the island path, not on slot 0).** Only two calls: `0x10091fcb` (mode 0) and `0x100920ff` (mode 6) inside `0x10091fa0`. `0x10091fa0` is island vtable `0x100cac2c` slot `+0x128` (reloc installer `0x10094c30`; `0x100cad54 − 0x128 = 0x100cac2c`; that vtable's slot 0 is the same stub `0x100b4a20`). Modes 0–7 at `0x10092190`: `0x10091fba` / `0x10091ffe` / `0x10091fdc` / `0x1009211a` / `0x1009215e` / `0x1009213c` / `0x100920ee` / `0x1009215e`.

Loop `0x10090050..0x1009007b`: second dword of each 8-byte tile record at `[island + 0xc]`; `mask = bits 16..24 & 0x1ff`; `cell = [0x100c1af8 + mask*2] | ((dword & 0xff00) << 8)`. Low u16 = mapped mask; bits 16–23 = the tile byte; bits 24–31 = 0.

Mapper `0x100c1af8` (512 u16, 33 distinct; every value has top nibble 0):

| Mapped | Count | `& 0x2111` | `& 0x8444` | `& 0x4888` | `& 0x1222` |
|---|---|---|---|---|---|
| `0x0fff` | 123 | `0x0111` | `0x0444` | `0x0888` | `0x0222` |
| `0x030c` | 25 | `0x0100` | `0x0004` | `0x0008` | `0x0200` |
| `0x0861` | 25 | `0x0001` | `0x0040` | `0x0800` | `0x0020` |
| `0x000c` | 23 | 0 | `0x0004` | `0x0008` | 0 |
| `0x0060` | 23 | 0 | `0x0040` | 0 | `0x0020` |
| `0x0300` | 23 | `0x0100` | 0 | 0 | `0x0200` |
| `0x0801` | 23 | `0x0001` | 0 | `0x0800` | 0 |
| `0x0000` | 21 | 0 | 0 | 0 | 0 |
| `0x00ee` | 20 | 0 | `0x0044` | `0x0088` | `0x0022` |
| `0x0770` | 20 | `0x0110` | `0x0440` | 0 | `0x0220` |
| `0x0b83` | 20 | `0x0101` | 0 | `0x0880` | `0x0202` |
| `0x0c1d` | 20 | `0x0011` | `0x0404` | `0x0808` | 0 |
| `0x006c` | 12 | 0 | `0x0044` | `0x0008` | `0x0020` |
| `0x0360` | 12 | `0x0100` | `0x0040` | 0 | `0x0220` |
| `0x080d` | 12 | `0x0001` | `0x0004` | `0x0808` | 0 |
| `0x0b01` | 12 | `0x0101` | 0 | `0x0800` | `0x0200` |
| twelve values with count 7 | `0x036c` `0x03ee` `0x077c` `0x086d` `0x08ef` `0x0b0d` `0x0b61` `0x0b8f` `0x0be3` `0x0c7d` `0x0f1d` `0x0f71` | (nibble 0) | | | |
| `0x0bef` / `0x0f7d` | 5 each | `0x0101` / `0x0111` | | | |
| `0x0b6d` | 2 | `0x0101` | | | |
| `0x0082` / `0x0410` | 1 each | 0 / `0x0010` | | | |

If this dword were the Y-class cell, the expansion's top nibble would be 0 and bit 10 of the high u16 would be 0 (bits 24–31 are 0). Codes `0x80..0x86` would be tile bytes `0x80..0x86`. **That identity is not shown:** the five `call [reg + 0x128]` sites in `AnnoWelt.dll` are class W (`0x10001232`), the twin engine (`0x10002a4e`), `0x10003220` (`0x10003453`, dest is a stack rect), `0x10003780` (`0x100037df`, world `0x100a52d0`), and `0x10003aa0` (`0x10003b17`, stack dest). None writes the 64 bytes at `chunk + 0`.

**(d) Loader / Place — stopped.** `0x10087b90` is island vtable `0x100cac2c` slot `+0xbc` (also at `0x100ca258` and `0x100ca5ec`). It is a switch on `arg0 − 7` that writes island **object** fields (`+0xac`, `+0xc0`, `+0xc1`, `+0xa6`, `+0xa8`, `+0xc3`, `+0xc6`), not the 8-byte tile records at `[island + 0xc]`. `0x100878d0` has zero code xref and zero reloc — it is not a function entry (the bytes there sit inside a larger body that starts earlier; a mid-function dump is not used as a claim). **Stop addresses:** `0x10087b90` (not the tile-dword writer) and `0x100878d0` (not an entry). The 9-bit mask and the byte at bits 8–15 of the tile record's second dword are still unread as writes. Anlagen flags for the two named types stay P (`transport.md` §3.2): `2527` `0x1186`, `2520` `0x4186`; both named-road WALKMASK records are 12 zero bytes.
#### 3.4.10 `0x10003220` and the four special expansions (T30b.3l, 2026-10-25)

**D.** `tools/disasm.py` on `AnnoWelt.dll`. fn1 calls `0x10003220` with `ecx = edi` (`edi = Y + 4`, PR #54). It calls the four special expansions with `lea ecx, [edi + 4]` (`this = Y + 8`), same as the default expansion.

**`0x10003220` (D).** `this = esi = Y + 4` (`0x10003229`). One stack argument: the node (`0x10003234`). Ends at two `ret 4` epilogues (`0x1000376a`, `0x1000377d`); the next function is `0x10003780`.

| `[this + k]` | Absolute | Use |
|---|---|---|
| `+0x574` | `Y + 0x578` | Count of a goal-point list. Zero → skip the float compare (`0x10003247`). |
| `+0x578` | `Y + 0x57c` | Goal-point array, 8 bytes each (`lea ebx, [esi + 0x578]`, end `esi + count*8 + 0x578`). |
| `+0x4e8` / `+0x4ec` | `Y + 0x4ec` / `Y + 0x4f0` | Origin added to `chunk[+0x40] + node.x` / `chunk[+0x42] + node.y`. |
| `+0x434` | `Y + 0x438` | Island. Slot `+0x12c` at `0x100033bd`; slot `+0x128` = `0x10091fa0` at `0x10003453` with mode 0 and a stack rect. |
| `+0x2040` | `Y + 0x2044` | Pushed as an argument of that island call. |
| `+0x438` | `Y + 0x43c` | Continue byte. Zero after the callback → return 0 (`0x1000374a`). |
| `+0x43c` / `+0x440` | `Y + 0x440` / `Y + 0x444` | Callback pointer and its argument (`call [esi + 0x43c]` at `0x10003737`). |

Position: `x = [chunk + 0x40] + [node + 4] + [Y + 0x4ec]`, then `fild` / `fmul [0x100c1138]` (`1.0f`) at `0x10003269`. Same for y. The float pair is compared to each goal (`fcomp`, `test ah, 0x41`). **This is the goal / arrived test**, not an expansion: it does not call `0x10006f70` / `0x10007180`.

Returns (`ret 4`):
- `eax = 0` (`0x10003774`) when the callback ran and `[Y + 0x43c]` is 0 — fn1 then leaves the loop if that same byte is 0.
- `eax = [esp + 0x10] & 0xff` (`0x10003757`, the `bl` accumulator) on the normal path. Non-zero → fn1 falls through to the expansion / unlink tail.

**The four special expansions (D).** Each `this = Y + 8`. Same table `[this + 0x42c] = [Y + 0x434]`, same enqueue helpers as §3.4.8 (`0x10006f30`, `0x10006f70`, `0x10007180`, `0x1000d4e0`, `0x1000b850`, `0x10017510`, `0x1000b790`), plus `0x10007540`. ~~Four orthogonal neighbours like the default, **minus one direction**.~~ **T30b.3m correction (§3.4.12):** the neighbour order omits the direction back to the parent; the geometry-bit selection and turn-cost arithmetic also differ from the default. T30b.3o extends the table below with bit groups and cost formulas (§3.4.14).

| Arrival / reached code | Function; ret4 epilogues | Current geometry group | Neighbours in execution order; score increments | Back edge omitted |
|---|---|---|---|---|
| **Receiver: this=Y+8** | Installed interface at `0x1000637c`; vtable `0x100c14e0`; fn1 passes Y+8 | L=[Y+0x434] | Detailed predicates, pseudo-code and helper in §3.4.14 | All four share this receiver. |
| Left /0x80 | `0x10007b00`; `0x10007e3a`, `0x10007e98` | low>>0, bits0..2 | U:C7−C6, D:C1−C2, L:C0 | R |
| Down /0x82 | `0x10007770`; `0x10007afc` | low>>3, bits3..5 | L:C1−C0, R:C3−C4, D:C2 | U |
| Right /0x84 | `0x10007ea0`; `0x100081d0`, `0x10008228` | low>>6, bits6..8 | U:C5−C6, D:C3−C2, R:C4 | L |
| Up /0x86 | `0x100071c0`; `0x10007533` | low>>9, bits9..11 | L:C7−C0, R:C5−C4, U:C6 | D |

**T30b.3o:** Cj means the **destination's** `L[0x400+8*(high16&0x3ff)+j]` byte before marking;
new score is u16(parent score + increment). These are rotations of one transition/cost rule,
but their fixed enumeration orders differ. The previous table specified order alone; §3.4.14
now decodes the keys and turn algebra too.

~~**Likely** (not D; T30b.3k did not map tile bytes to these codes): each code is a tile that has no exit in the missing direction (a ramp, bridge, pier or entrance facing the opposite way).~~ **Withdrawn by the orchestrator (2026-10-25 ao, §3.4.11 (f)):** the codes `0x80 | dir` are written by the expansion's own marks into the high u16 of a reached cell (`0x10006ae5`, `0x10006cc8`, `0x10006fc4`), so a node with such a code was reached from direction `dir` and its expansion omits the way back to the parent. Codes `0x81` / `0x83` / `0x85` still take the default expansion (`0x10006a40`); no mark produces them.

#### 3.4.11 The orchestrator's read after T30b.3k (2026-10-25 ao): the fill call goes through `[Y + 8]`, and `0x10003780` is the chunk fill

**D** (re-disassembled with `tools/disasm.py`, slots with `pefile`). T30b.3k read slot 0 of the vtable at `[Y + 4]` and found the stub `0x100b4a20`; the call it looked at does not go through `[Y + 4]`.

**(a) Class Y has four vtables (D, constructor `0x10006344`).** `[Y] = 0x100c1424` (`0x10006401`), `[Y + 4] = 0x100c14e4` (`0x10006375`), **`[Y + 8] = 0x100c14e0`** (`0x1000637c`), `[Y + 0x2048] = 0x100c14dc` (`0x10006387`; `0x100c14d8` at `0x100063e5`). The dword at `0x100c14e0` is `0x10003780` (the one `.rdata` reference to that function, `xref.py --reloc`); `0x100c14dc` is `0x10037590`. So `0x10003780` is slot 0 of a one-slot interface at `[Y + 8]`, and every method called through that interface has `this = Y + 8` (§1 item 11). T30b.3i's "two vtables, `0x100c1424 + 0xbc` and `0x100c14e0 + 0`" is the same dword seen from both sides; the installer decides: `[Y + 8]`.

**(b) `0x100064b0` runs with `this = Y + 8` (D).** fn1's call site `0x10003e76..0x10003e7f`: `eax = [esp + 0x18]` is the saved-`this` slot (`[esp + 0x10]` before the two argument pushes) = `Y + 4`; `lea ebp, [eax + 4]`; `mov ecx, ebp`. `0x100064b0` saves `ecx` at `[esp + 8]` (`0x100064c4`) and reloads it at `0x100065b1` and `0x1000665c` (`[esp + 0x10]` after `push esi` / `push edi`). Its grid dimensions `[ecx + 4]` / `[ecx + 8]` are therefore `Y + 0xc` / `Y + 0x10` (the fields fn1 reads as `[this + 8]` / `[this + 0xc]`, §3.4.7), and the super-block table `[ecx + esi*4 + 0x14]` (`0x10006513`) is at `Y + 0x1c + sb·4`. §3.4.5 wrote these with `this = Y + 4` (off by 4).

**(c) The fill call (D).** `0x10006694..0x10006697` `mov eax, [ecx]; push edi; call [eax]` with `ecx = Y + 8` (reloaded at `0x1000665c`) and `edi` = the new `0x44`-byte chunk whose origin was just written (`0x1000668c` / `0x10006690` → `[chunk + 0x40]` / `[chunk + 0x42]`, from the two u16 arguments at `[esp + 0x20]` / `[esp + 0x24]`). `[Y + 8]` = `0x100c14e0`, slot 0 = **`0x10003780`**. The 64 cells were zeroed by `rep stosd` (`0x100065a6..0x100065af`, `ecx = 0x40` dwords) before the call.

**(d) `0x10003780` is the per-chunk fill (D, `this = Y + 8`, one argument = the chunk, `ret 4` at `0x10003a91`).** `0x1000378b..0x100037d8`: a stack rect from `[chunk + 0x40] + [this + 0x4e4]` and `[chunk + 0x42] + [this + 0x4e8]` (= `[Y + 0x4ec]` / `[Y + 0x4f0]`, the origin fn1 subtracts in §3.4.7) as `(x, y, x + 4, y + 4)`; then `mov ecx, [this + 0x430]` (= `[Y + 0x438]`, the island §3.4.10 found, not a world pointer: T30b.3b read it with `this = Y`) and `call [[ecx] + 0x128]` with the pushes `4` (`0x10003799`), the chunk (`0x100037ad`), `&rect` (`0x100037b6`), `[this + 0x2038]` = `[Y + 0x2040]` (`0x100037d1`), `[this + 0x203c]` = `[Y + 0x2044]` (`0x100037de`), in callee order `(mode = [Y + 0x2044], [Y + 0x2040], &rect, dest = chunk, width = 4)`. Island slot `+0x128` is `0x10091fa0` (§3.4.9), a switch on arg0; mode 0 (`0x10091fba..0x10091fcb`) calls `0x1008ff70(island, rect, dest, width)`.

**(e) `0x1008ff70` writes 4-byte cells at stride `width` into `dest` (D).** Row pointer `lea edx, [edx + eax*4]` (`0x1008ffcd`; next rows `imul eax, [esp + 0x24]` = `width`, `lea edx, [edx + eax*4]` at `0x1009000e..0x1009001a`); tile records `lea ecx, [ecx + eax*8]` from `[island + 0xc]` (`0x10090017..0x1009002e`); inner loop `0x10090050..0x1009007b`: `cell = u16[0x100c1af8 + ((rec >> 16) & 0x1ff)·2] | ((rec & 0xff00) << 8)`, stored at `[edx − 4]`. With `dest = chunk` and `width = 4` these are the 16 cells of the Y-class chunk. **The cell of §3.4.6 is this dword at fill time:** bits 0–11 the mapped mask (the 33 values of §3.4.9, top nibble 0), bits 16–23 the tile record's byte 8–15 (the `code` of §3.4.7–3.4.8), bits 12–15 and 24–31 zero. Bit 10 of the high u16, the `| 7` marks and the top nibble are written later by the search (who sets the nibble is open, T30b.3m step 6). The second half of `0x10003780` (`0x100037e5..0x10003a8a`, enabled by the byte `[this + 0x4e3]` = `Y + 0x4eb`) marks the cells of a region bounded by `[this + 0x454..0x460]` (`Y + 0x45c..0x468`) and the byte table at `this + 0x464` (`Y + 0x46c`) with `(hi & 0xfc07) | 7`: the "disc fill" of OQ-160, to be read in full in T30b.3m.

**(f) The marks are came-from codes, not tile kinds (D for the writes; T30b.3m confirms the reading).** §3.4.8's up-mark writes the neighbour's **high** u16 (`lea eax, [ebp + eax*4 − 0xe]` = `cell_up + 2` at `0x10006a94`, `mov word [ecx], ax` at `0x10006ae5`) with `(hi & 0xfc86) | 0x86`; the down-mark `(hi & 0xfc82) | 0x82` at `[ecx + 2]` (`0x10006cc8`); `0x10006f70` writes `[ecx + 2]` with bits 0–9 replaced by `0x80 + dir` (`0x10006fb4..0x10006fc4`: `dx = (dir + 0x80) ^ hi; dx &= 0x3ff; dx ^= hi`). So the code of a cell the search has reached becomes `0x80 | dir` with `dir` the step that reached it (`0` left, `2` down, `4` right, `6` up), and a popped node whose code is `0x80..0x86` takes the matching special expansion of §3.4.10, which omits exactly the direction back to the parent (`0x86` = reached going up → no down; `0x82` → no up; `0x80` → no right; `0x84` → no left). §3.4.10's "likely a ramp, bridge, pier or entrance" is withdrawn; the odd codes `0x81` / `0x83` / `0x85` cannot be produced by a mark (`dir` is even). The `| 7` of fn2 and of the disc pass gives code 7 to start and disc cells.

**What this means for OQ-175 / OQ-171 / OQ-160.** The static chain is complete end to end: tile record (8 bytes at `[island + 0xc]`) → `0x10003780` → island `+0x128` mode `[Y + 0x2044]` → `0x1008ff70` → the chunk cell → fn1 (§3.4.7) → `0x10006a40` (§3.4.8) with the byte table `[Y + 0x434]`. Three readings remain before the Wine check: who writes `[Y + 0x434]` and `[Y + 0x2044]` (a capstone sweep of `.text` finds `mov [esi + 0x434], edi` at `0x10002b9e` next to `[esi + 0x203c]` / `[esi + 0x2040]` writes at `0x10002b92` / `0x10002b98`, and `cmp [ebp + 0x2044], 6` at `0x1000576e` in `0x10005890`): if that `edi` is a layer block `0x100d1140 + 0x1c00·layer`, the block's bytes `L[code]` are the admission bits and `L[0x400 + code·8 + dir]` the step costs, which answers §3.1's "meaning of an entry"; who sets the top nibble of the low u16 (the fill leaves it 0, and `dl = 0` admits nothing); and who writes the tile record's byte and 9-bit mask for a road, a footprint and the terrain (OQ-171, OQ-170). T30b.3m and T30b.3n; then T30b.3h2 with the predicted cells.

#### 3.4.12 The fill, the layer table and the marks (T30b.3m, 2026-10-25)

All addresses below are **AnnoWelt.dll**; D denotes the static instruction/PE reading, P the image statistics. No game-memory observation is claimed. This section supersedes the off-by-four readings in §3.4.9 and the unresolved table/nibble paragraphs of §3.4.11.

**Interface proof (D).** Fill `0x10003780`, chunk lookup `0x100064b0`, default expansion `0x10006a40`, helper `0x10006f70`, fn2 and the special expansions have `this = Y + 8`: installer `0x1000637c` writes vtable `0x100c14e0` there, with slot 0 = fill; fn1 passes `lea ecx,[edi+4]` with edi = Y+4 (§3.4.7). Setup `0x10002b80`, disc setter `0x10002a60` and layer setter `0x1000dd20` have `this = Y + 4`: their slots are respectively `0x100c1534`, `0x100c1538`, `0x100c1564`, offsets +0x50/+0x54/+0x80 in `0x100c14e4`, installed at Y+4 by `0x10006375`. Start-key helper `0x10003aa0` is directly called with fn1's saved this = Y+4 (`0x10003f5a..0x10003f6e`). Goal test `0x10003220` is Y+4 (§3.4.10). These are not interchangeable with the other engine families.

**Table A — fill fields (`0x10003780..0x10003a91`, D).**

| Fill [this+k] | Absolute | Use / address |
|---|---|---|
| +0x4e4 / +0x4e8 | Y+0x4ec / Y+0x4f0 | Add to chunk u16 origin +0x40/+0x42 (`0x1000379d..0x100037ab`). |
| +0x430 | Y+0x438 | Island, virtual +0x128 at `0x100037df`. |
| +0x2038 / +0x203c | Y+0x2040 / Y+0x2044 | Auxiliary parameter and mode (`0x100037b7`, `0x100037d2`). |
| +0x4e3 | Y+0x4eb | Disc radius/enabling byte; zero skips pass (`0x100037e5..0x100037f1`). |
| +0x454 / +0x458 | Y+0x45c / Y+0x460 | Left/right centre bounds, chunk-relative (`0x100037f7..0x10003821`). |
| +0x45c / +0x460 | Y+0x464 / Y+0x468 | Top/bottom centre bounds (`0x1000380f`, `0x1000385c`). |
| +0x464+i | Y+0x46c+i | Byte half-width table (`0x1000383b`, `0x10003902`, `0x100039fb`). |

**Table B — stack rect (D).** Let S be esp immediately after the four saved-register pushes, before `push 4` at `0x10003799`; x/y include the world origin above. The pointer taken at `0x100037b2` is S+0x1c. Store operands below use esp *at that instruction*, not a common stack frame.

| Store | Operand | Address relative to S | Value / rect offset |
|---|---|---|---|
| `0x100037ae` | esp+0x28 | S+0x20 | y / +4 |
| `0x100037c0` | esp+0x28 | S+0x1c | x / +0 |
| `0x100037c7` | esp+0x34 | S+0x28 | y+4 / +12 |
| `0x100037d8` | esp+0x34 | S+0x24 | x+4 / +8 |

Thus the callee reads `(x,y,x+4,y+4)`, exclusive right/bottom, in four consecutive dwords.

**Table C — island +0x128 arguments (D).**

| Callee argument | Push | Value |
|---|---|---|
| 0 | `0x100037de` | [Y+0x2044], mode |
| 1 | `0x100037d1` | [Y+0x2040], auxiliary parameter |
| 2 | `0x100037b6` | &rect |
| 3 | `0x100037ad` | chunk destination |
| 4 | `0x10003799` | 4, destination row width in cells |

Island vtable `0x100cac2c` +0x128 = `0x10091fa0`. Jump table `0x10092190`, modes 0..7: `0x10091fba`, `0x10091ffe`, `0x10091fdc`, `0x1009211a`, `0x1009215e`, `0x1009213c`, `0x100920ee`, `0x1009215e`. Mode 0 forwards rect/dest/width to `0x1008ff70` (`0x10091fba..0x10091fcb`). Mode 6 forwards **the same arguments**, then calls `0x100906f0` with them (`0x100920ee..0x10092109`); it does not substitute the auxiliary parameter. After four register pushes these values are esp+0x1c/+0x20/+0x24. Other modes: 1 inline at `0x10091ffe`; 2 calls `0x10090a40`; 3 calls `0x10090560`; 4/7 inline at `0x1009215e`; 5 calls `0x10090870`. Their bodies were not read for this step.

**Cell builder pseudo-code (D, `0x1008ff70..0x100900b3`).** This = island. Fields +4/+6/+8/+0xa are unsigned u16 x/y/w/h; +0xc is the 8-byte tile-record array. Slot +0x50 = `0x1008caa0..0x1008caa6` returns x; +0x54 = `0x10037570..0x10037576` returns y (pefile targets, bodies read to ret).

```text
ix0 = max(rect.left - island.x, 0)                 # 0x1008ff9f..0x1008ffad
ix1 = min(rect.right - island.x, island.w)         # 0x1008ff89..0x1008ff9b
iy0 = max(rect.top - island.y, 0)                  # 0x1008ffed..0x1008fffa
iy1 = min(rect.bottom - island.y, island.h)         # 0x1008ffe0..0x1008ffeb
row = dest + 4*(island.x + ix0 - rect.left
             + (island.y + iy0 - rect.top)*width) # 0x1008ffb9..0x1009001a
record = records + 8*(iy0*island.w + ix0)          # 0x1009001d..0x1009002e
for y in [iy0, iy1):
    for x in [ix0, ix1):                          # 0x10090047..0x1009007b
        rec = second_dword(record + 8*(x-ix0))
        row[x-ix0] = mapper[(rec >> 16) & 0x1ff] | ((rec & 0xff00) << 8)
    row += width cells; record += island.w records # 0x10090089..0x100900aa
```

A destination cell is four bytes; row stride is width*4. Mapper = 512 u16 at `0x100c1af8`, 33 distinct values (§3.4.9). At mode-0 fill: bits 0..11 = mapped mask; 12..15 = 0; 16..23 = tile-record byte 8..15 (fresh code); 24..31 = 0. Clipping leaves outside-island cells untouched, hence zero from the superblock/chunk allocation (`0x100065a6..0x100065af`); the builder does not explicitly clear them. Mode6's second pass can replace a cell with 0x0fff (`0x1009081f`), which also leaves the nibble zero.

**Disc pass pseudo-code (D).** The setter `0x10002a60..0x10002b7a` is the area/radius setup described in §1.1: argument0 is the footprint rect and argument1 the radius. Its fields +0x458/+0x45c/+0x460/+0x464/+0x468/+0x4e7, after adding 4, are **the very same** absolute fields Y+0x45c/+0x460/+0x464/+0x468/+0x46c/+0x4eb. The left/right and top/bottom centre bounds account for an even footprint's two centre tiles. The byte table is the rounded-disc half-width profile, not a per-terrain table. Writes at `0x10002a80`, `0x10002aa1`, `0x10002ab9`, `0x10002ad9`, `0x10002af1`, `0x10002b0d`, `0x10002b23`, `0x10002b69`; setup disables it at `0x10002d55`.

```text
if radius == 0: return                            # 0x100037e5..0x100037f1
# Corner comparisons select which of the two row bands need work.
# They use profile[min(radius, distance)] and chunk x/y + 3.
# flag bits 0/1 = top left/right outside; 2/3 = bottom left/right outside
flags = corner_outside_tests()                    # 0x100037f7..0x100038a5
for band in [top, bottom]:                        # 0x100038ab / 0x10003993
    if corresponding flag pair == 0: continue
    if top:
        rows = [max(0, topCentre-radius-chunk.y), min(3, topCentre-chunk.y)]
        i(row) = topCentre - chunk.y - row        # 0x100038b4..0x10003902
    else:
        rows = [max(0, bottomCentre-chunk.y), min(3,bottomCentre+radius-chunk.y)]
        i(row) = chunk.y + row - bottomCentre     # 0x1000399e..0x100039fb
    for row in rows inclusive:
        half = profile[i(row)]
        endLeft = min(4, leftCentre-chunk.x-half)
        startRight = max(-1, rightCentre-chunk.x+half)+1
        for x in [0,endLeft) and [startRight,4):
            high16(cell(x,row)) = (high16(cell(x,row)) & 0xfc07) | 7
            # 0x1000393e..0x10003948 / 0x1000396d..0x10003977
            # 0x10003a3f..0x10003a49 / 0x10003a6e..0x10003a78
```

These are cells **outside the allowed rounded area**, not its interior. PE read: L[7] = 0 in all five blocks. Therefore a disc-marked cell cannot be newly admitted by `L[code] & direction_bit`; code 7 is closed. fn2 also marks start cells code 7, closing re-entry without removing their already enqueued start nodes.

**The layer pointer and mode (D).** `0x10002b80` begins after `ret 8`/padding at `0x10002b7a` and ends `ret 0x10` at `0x10002d7c`. Direct xrefs: none; relocation slot `0x100c1534`, installed +4-interface as above. edi at `0x10002b9e` is argument0 (entry esp+4; esp+0x14 after four saved registers), the island. Its +0x434 store is **Y+0x438**, not Y+0x434. Argument2 (entry esp+0xc) is stored at this+0x2040 = **Y+0x2044** (`0x10002b98`), so it supplies the mode. Argument3 (entry esp+0x10; esp+0x1c after three pushes at `0x10002b89`) is stored at this+0x203c = Y+0x2040 (`0x10002b92`). The mode is caller-supplied; mode0/mode6 wiring is above, not inferred from the misleading comparison at `0x1000576e` (which precedes `0x10005890`).

The Y-class layer setter is **the shared `0x1000dd20`**, through +4-interface slot +0x80: `[Y+4+0x430] = 0x100d1140 + 0x1c00*arg0` (`0x1000dd24..0x1000dd36`). Thus the expansion's pointer **is a layer block**, despite earlier sections assigning the shared setter only to X. The stores `0x100012ce/0x100012d6` belong to the earlier setup `0x100012a0` (relocation `0x100c12f0`); `0x1000db7d/0x1000db85` to `0x1000db60` (relocation `0x100c1904`), where the value is a rect coordinate/clamp, not a Y layer pointer. A displacement coincidence does not identify either with Y.

The store sweep (pattern/hits in REPORTS) found only `0x10002b92/0x10002b98` for displacements 0x203c/0x2040/0x2044; there is no direct literal +0x2044 store. The two +4-interface stores resolve the actual fields. Every hit is accounted for; no additional mode writer is claimed.

**Entry meaning, now D:** `L[code]` supplies admission bits (default up/down/left/right test bits 0x40/0x04/0x01/0x10); `L[0x400 + code*8 + dir]` supplies a cost byte, default dir 6/2/0/4. Addresses `0x10006ab0`, `0x10006ab9`, `0x10006c9c`, `0x10006ca5`, `0x10006f55`, `0x10006fa7`. The mapper supplies low-u16 geometry, **not** the block index; the fresh tile byte supplies the code. Fill-time codes are 0..255, reached marks 0x80/82/84/86, start/closed code 7; the 0x3ff mask itself is not evidence that the fill produces 1024 code values.

`layer_blocks.py --disk-summary` reports P: nonzero bytes in the full 0..0x3ff lookup window = 648/645/735/411/735, of which fresh-code 0..255 admissions = **216/215/245/137/245** (layers 0..4). Do not rank layers by whole-block 0xff counts. First three nonzero codes and their eight costs (indices 0..7):

| Layer | Code: admission; costs |
|---|---|
| 0 | 0:255; (10,13,10,13,10,13,10,13). 1:255; (15,20,15,20,15,20,15,20). 2:255; (20,27,20,27,20,27,20,27). |
| 1 | Same first three as layer0. |
| 2 | 0:255; (10,13,10,13,10,13,10,13). 1:255; (20,27,20,27,20,27,20,27). 2:255; (30,41,30,41,30,41,30,41). |
| 3 | 0:255; (10,13,10,13,10,13,10,13). 1:255; (20,27,20,27,20,27,20,27). 4:255; (40,55,40,55,40,55,40,55). |
| 4 | 0:255; (50,69,50,69,50,69,50,69). 1:255; same costs. 2:255; (10,13,10,13,10,13,10,13). |

For fresh codes below 128: layer0 has admission 255 iff code mod8 is 0..5; otherwise 0. Layer3 has admission 255 exactly for {0,1,4,24,25,40,41,48,49,72,73,80,81,96,97,120,121}; all other codes below128 have 0. For codes 128..255 both layers agree: each consecutive group of eight codes has admission, in order, 0,64,16,112,4,68,28,124,1,193,17,241,7,199,31,255. This compact mapping covers every possible fresh byte; it is derived from PE reads, not an extracted table committed to git. What road/terrain kinds produce those bytes remains T30b.3n.

**Start nibble (D).** `0x10003aa0` runs with this=Y+4, two arguments = the two dwords of an 8-byte input float-position entry (`0x10003f63..0x10003f6e`). It creates a bordered 3x3 byte buffer using `0x1000d950(3,3)` (stride5, border9; helper read through `0x1000d9d3`). It converts scaled position components using global `0x100e05bc` and conversion helper `0x100b5f90` to obtain tile x/y, then island +0x128 arguments are `(mode=[Y+0x2044]+1, auxiliary=0, rect=(x,y,x+1,y+1), dest=interior buffer, width=5)` (`0x10003af3..0x10003b17`). This is the subcell mode1 for ordinary fill mode0. No value for the runtime scale global is inferred from its zero image content.

It seeds an interior byte with 10 from the fractional-position components times 3 (`0x10003b1d..0x10003b98`), then floods bytes equal to 8 up/left/right/down, writing directions 6/0/4/2 and queuing 4-byte local records (`0x10003bbe..0x10003cb6`). `0x10018740(x,y)` returns buffer+4+x+(y+1)*stride (`0x10018740..0x10018755`). Byte classification at `0x100d1128` is nonzero for direction/seed codes 0..7 and10, zero for unvisited8/border9 (P). The four edge tests set return bits only:

| Bit | Connectivity test | Address |
|---|---|---|
| 0x8000 | top row, any reached subcell | `0x10003cbc..0x10003ced` |
| 0x1000 | left column, any reached subcell | `0x10003cf5..0x10003d39` |
| 0x2000 | bottom row, any reached subcell | `0x10003d3f..0x10003d54`, helper `0x1000d730` |
| 0x4000 | right column, any reached subcell | `0x10003d5a..0x10003d6f`, same helper |

Return ax is ORed into start-cell low u16 at `0x10003f73` (also `0x1000414b`, `0x100046b5`). This answers the nibble source: four start-position-to-edge connectivity keys. The pre-loop AND (`0x10004737`) instead uses entry+8 of the 12-byte array at Y+0x1c0c (count Y+0x1c08); it clears exactly bits absent in that supplied u16. Its producer `0x100056e1` takes ax from `0x10008230` at `0x100056d7`; **open in T30b.3m, decoded in §3.4.14:** the exact meaning of that separately supplied constraint mask, not the start nibble's source.

No low-u16 propagation is missing: only start nodes need the default expansion's shift12. The reached nodes select special expansions by their came-from code. Those read the mapped low-u16 bits directly: 0x80 uses low byte (`0x10007b2e`), 0x82 shifts3 (`0x100077ae`), 0x84 shifts6 (`0x10007ed6`), 0x86 shifts9 (`0x100071f3`). Thus §3.4.10's "same as default minus a direction" describes neighbour order only; bit selection and turn costs are not identical (e.g. two cost-byte reads and subtraction `0x1000723d..0x10007253`). Their full turn-cost algebra was not decoded by T30b.3m; T30b.3o decodes it in §3.4.14.

**Store audit / came-from marks (D).** Default expansion's cell-word stores `0x10006ae5` and `0x10006c06` target up-neighbour+2; `0x10006cc8` and `0x10006d74` target down-neighbour+2. Left/right helper's only cell-word store `0x10006fc4` targets neighbour+2. All remaining word stores in these two bodies are node fields: default x/y/score at `0x10006afd`, `0x10006b09`, `0x10006b0d`, `0x10006c25`, `0x10006c29`, `0x10006c2f`, `0x10006ce1`, `0x10006ced`, `0x10006cf1`, `0x10006d93`, `0x10006d97`, `0x10006d9d`; helper `0x10007011`, `0x10007015`, `0x10007019`. No cell+0 word store. Goal test `0x10003220..0x1000377d` has no word stores; goal-flag writes `0x10004068/0x10004256/0x10004513` OR byte cell+3 with4, setting high-u16 bit10 only.

Up marks `(hi & 0xfc86)|0x86` (`0x10006adb..0x10006ae5`); down `(hi & 0xfc82)|0x82` (`0x10006cbe..0x10006cc8`). The low code bits retained by those AND masks are all set by the OR, so both replace bits0..9 outright. Left/right helper replaces those same bits with `0x80+dir` (`0x10006fb4..0x10006fc4`); dir0 pushes `0x10006e09/0x10006e60`, dir4 `0x10006eb9/0x10006f11`. All preserve high-u16 bits10..15, including goal bit10. No writer of bits11..15 was found in the inspected mode0 fill/start/goal/mark paths; they start zero, and their purpose is not assigned. Odd codes 0x81/83/85 cannot be produced by these even-dir marks. All five layers have zero admission for **every code 0x80..0x86**: a fresh byte there cannot be admitted as a neighbour, so cannot be confused with a reached mark through this path.

**Still open:** tile-record writers (T30b.3n); live cell confirmation (T30b.3h2); the separate constraint mask (`0x10008230`); complete special-expansion turn costs. This closes the block-pointer identity, entry meaning, mode writer, disc fill and start-nibble source, not the full original walk rule.

#### 3.4.13 The tile record's walk bits (T30b.3n, 2026-10-25)

**Scope/confidence.** D = static instruction reading in **AnnoWelt.dll** unless explicitly
marked **AnnoGame.dll**; P = parsed original file or PE lookup. These are predictions for the
named stage of the writer, not live cell dumps. The ordinary placement/code/mask chain is
identified. Terrain additions and the final authored coast/river mask remain conditional below.

**Record and readers (step 1).** `islands.md`, "Tile record", `annowelt.md`, "Create's use of
the record", and `formats/insel.md`, "Placed objects", distinguish the runtime record from a
six-byte file item. Runtime tiles are eight bytes at `[island+0xc]+8*(y*width+x)`, island width
u16 +8, origin u16 +4/+6. First dword: u16 type id +0; byte+2 packs footprint dx bits0..2,
dy bits3..5 and rotation bits6..7; byte+3 holds phase bits0..5 and flags6/7. Second dword:
height byte bits0..7; walk-code byte bits8..15; nine mask bits16..24; owner bits25..28; the
remaining bits are not assigned a universal meaning here. Reset `0x1008c80d..0x1008c821`
sets it to `0x1e000200`: neutral owner15, code2, mask0. Placement writes the first-dword
suboffsets/rotation at `0x10082447..0x10082483` and height/phase at `0x10082485..0x100824c5`.

| Named reader | What is actually read |
|---|---|
| `0x1008ff70`, mode0 | Second-dword bits16..24 -> mapper; bits8..15 -> high-u16 code (`0x10090050..0x10090078`, §3.4.12). |
| `0x10090a40`, mode2 | Second dword at `0x10090b3f`; mask extraction at `0x10090b45..0x10090b48`, code from ah at `0x10090b56`; bit7/connection bits select a subcell branch (`0x10090b58..0x10090bbd`). The full mode2 expansion is not decoded here. |
| `0x100900c0`, kind11 | Height grid plus **first** dword's type/packed offsets, with type height callback (`0x10090295..0x100902ee`); not a second-dword decoder. |
| `0x10090350`, kind10 | Same distinction: height grid and first-dword type/offsets (`0x1009049d..0x100904fa`); not the walk byte/mask. |
| `0x100a4730`, kind9 | Separate 64x64 summary-block data at receiver+4, copies dwords at an 8-tile sampling stride or fills0x0fff (`0x100a47b5..0x100a4885`); not `[island+0xc]` tile records. |

The request's grouping of all those copies as readers of the second dword does not hold.

**Candidate writer sweep (step 2).** Scratch script `cache/t30b3n_sweep.py` uses capstone detail
and skipdata on `.text`: a `mov reg,[reg+0xc]`, register-copy/LEA tracking, scale8, and a later
memory-destination mov/or/and/xor through the derived pointer within60 instructions. Functions
are initially split at ret+NOP/int3, then checked against instruction-aligned direct callees and
island-vtable entries: inline switch data had hidden the starts `0x10082040`, `0x10082c30`,
`0x10087880`. The first raw grouping at `0x10081ce0`/`0x10087400` was rejected. Results are
**candidates**, not an exhaustive proof of writes: stack spills/calls are not fully modelled.
The final60 run finds45 stores in11 functions; a120-instruction robustness run finds71 stores
in the same11 functions. It was not necessary to loosen an empty60 run. The vtable dump from
`0x100cac2c` stops after111 text-target slots, last+0x1b8; rows below mark candidate functions
which are slots. An installed island receiver is base I, because `0x10094c30` writes
`[ecx]=0x100cac2c`; no Y-class displacement is reused.

| Candidate start | 60-window store addresses (AnnoWelt, prefix `0x100` below) | Island slot / relevant masks |
|---|---|---|
| `0x10082040` | 82753, 82a46 | direct helper; 0x1ff, shift16, 0xfe00ffff; 82a46 is another flag |
| `0x10082c30` | 8303f,83055,831fd,83213,83221,83239,83247,83426,8343b,83459,8346e,8347c,8348f,834a1,834a6,834b7,83646,8365b,83679,8368b,83699,836ac,836be,836c3,836d4,836e6 | direct helper; packed offsets/phase and field copies, 0xff00 / 0x1ff0000 |
| `0x100853b0` | 854ca | no slot; 0x1e000000 / 0xe1ffffff (owner) |
| `0x100854f0` | 855fb | no slot; 0xe1ffffff (owner) |
| `0x10085620` | 85696,85716 | +0x100; 0xe1ffffff (owner) |
| `0x10087340` | 873a8,873ab | +0x110; type word / byte+3 bit7 |
| `0x10087880` | 878e6,87a7a,87b4b | +0xc0; bit29, phase0x3f, mask0x1ff / shift16 / 0xfe00ffff |
| `0x1008c740` | 8c818,8c81b,8c81e,8c821 | +0x178; record reset |
| `0x1008ea00` | 8ebed,8ec00 | +0x168; packed record flags / phase0x3f |
| `0x10092b40` | 92c4d | +0x198; OR0x1ff0000 |
| `0x100a03c0` | a1386 | no island slot; 0x19 / 0xf / 0x7fffffff; unclassified candidate, not used as evidence of a walk writer |

The120 run adds mask/copy writes in `0x10082040` and `0x10082c30`, including
`0x1008279d/0x100827d4/0x10082823`, `0x10082ed2`, `0x100830bd/0x100830ce`,
`0x10083281/0x10083292`, `0x100834c9`, `0x100836fd`. Manual tracing of the placer also finds
`0x100825b7/0x100825d9`, missed by the narrow pointer tracking because the pointer crosses
spills/calls. The sweep is a navigation aid; addresses tagged D below were read independently.

**Object placement (step 3, D).** Receiver **I = island base**, passed unchanged from island
slot+0x78 `0x1008ec90` at direct call `0x1008ecbd` to **`0x10082040`**. It takes a placement
record P and flags (ret8 at `0x10082b0a`): P has type u16+0, world x/y u16+2/+4,
rotation byte+6, phase byte+7 and height byte+8. Type T comes from `[I+0x1c]` slot+0x70
(`0x10082063..0x10082070`). This is a type, not the runtime building instance.

| Receiver / field | Evidence |
|---|---|
| T is **AnnoGame type base**, vtable `0x1002185c` installed at `[T]` by AnnoGame `0x1000b4ec` | No +4/+8 adjustment. |
| Type +6/+7 = dimensions; slots+0x50/+0x54 | AnnoGame `0x10007d30/0x10007d40`, HEAD[0]/[1] per `formats/anlagen.md` field table. Odd P.rotation swaps width/height (`0x10082089..0x100820cc`). |
| Type +0x12 = HEAD flags; slot+0x60 | AnnoGame `0x10007d80`; placement tests **bit0x10**, not0x1000/0x4000 (`0x10082184..0x1008218f`). |
| Type +0x10 = PRODTYPE; slot+0x5c | AnnoGame `0x10007d20`; placer has special branches for kinds3/13/198 (`0x10082645..0x10082667`) after the common footprint writes. |
| Type +0x88 = WALKMASK storage; slot+0x84 | AnnoGame `0x10007f10`; loader `0x1000990d..0x1000996f` copies **3*height u32**, not one u32 per tile. |
| Type +0x75 = base walk code; slot+0x94(rotation) | AnnoGame `0x10007dc0..0x10007ea2`; HEAD inputs/rotation below. |

Ordinary-path pseudo-code (the flags-bit0x10 overlay branch is noted separately):

```text
T = types[P.type]; (w,h) = T.dimensions, swapped for odd P.rotation
for each tile in the rotated footprint (split into 8x8 working blocks):
    write type id, packed dx/dy/rotation, phase/flags, P.height
        # 0x10082447..0x100824c5
code = T.slot94(P.rotation)                 # 0x10082503..0x10082513
wm = T.slot84()                             # 0x10082517
for each footprint tile:
    r = tile.second_dword
    r = (r & 0xffff00ff) | (code << 8)       # 0x100825af..0x100825b7
    mask = sample_rotated_3x3(wm,dx,dy,w,h,rotation)
        # 0x100825bf..0x100825c2 -> 0x1007b5e0
    r = (r & 0xfe00ffff) | ((mask & 0x1ff) << 16)
        # 0x100825c7..0x100825d9
# Kind-specific updates and refresh calls follow; do not mistake this for a live final dump.
```

The overlay branch (T.flags&0x10) still replaces the code (`0x1008236d`) but **ORs** the
sampled mask (`0x1008239d..0x100823b0`) and can preserve earlier object/terrain state. Its
sampling helper `0x1007b840` takes explicit dimensions/rotation/offsets; the ordinary helper
`0x1007b5e0` reads the packed offsets and rotation from the record just written. The repair
routine `0x10082c30` restores/copies neighbouring footprint fields and walk bits; it is not
the primary code source. The explicit mask setter at island+0xc0 `0x10087880` branch
`0x10087af8..0x10087b4b` rotates an input nine-bit mask with `0x1007ba80` and replaces the same
bits; `0x100878d0` was inside this function, not a separate entry (§3.4.9).

**Walk-code source, D (AnnoGame).** In the zero-filled56-byte HEAD buffer, the loader keeps
HEAD[0x30] in cl (`0x100093db`), loads HEAD[0x2f] in al (`0x1000940b`), and if either is nonzero
calls **`0x100091f0(first=HEAD[0x2f], shape=HEAD[0x30])`** at `0x10009426`; stores al at
T+0x75 (`0x1000942b`). If both are zero, constructor default4 (`0x1000b555`) survives.

```text
if first < 8: code = first
else:
    code = byte((first-8) | 0x80)           # 0x100091fd..0x100091ff
    if shape <= 4:
        code |= [0x50,0x48,0x58,0x78,0x10][shape]
        # jump table0x10009228, branches0x1000920d..0x10009221
```

`T.slot94(rotation)` reads T+0x75 (`0x10007dc2`). For high-bit codes with canonical connection
nibbles2/9/10/11, it rotates that four-bit pattern by one position per quarter turn, retaining
the low3 bits. Other patterns return unchanged. This is a compact description of the checked
cases, not a general rotation applied to every code: jump tables `0x10007ea8/0x10007ebc`,
branches `0x10007df6..0x10007e9c`, fallback `0x10007e9f`. No HEAD flag is an input to this getter.

**Mask sampling, D (AnnoWelt `0x1007b5e0..0x1007b837`; explicit twin
`0x1007b840..0x1007ba71`).** The mask is a **blocked-subcell bitmap**: three u32 rows per
unrotated footprint tile row, with three consecutive bits per tile column. For output subcell
(u,v) in tile(dx,dy), let X=3*dx+u, Y=3*dy+v, W=3*w, H=3*h. Read the source bit at:

- rotation0: (X,Y);
- rotation1: (W−1−Y,X);
- rotation2: (W−1−X,H−1−Y);
- rotation3: (Y,H−1−X).

Store it at bit `3*v+u` of the nine-bit result. The branches start
`0x1007b5fa/0x1007b67c/0x1007b722/0x1007b7af`; no raw file bytes are copied into the high
mask field. An all-zero WALKMASK gives0 at any rotation. Mapping table `0x100c1af8` confirms
mask0 -> low0x0fff and mask0x1ff -> low0: the geometry tests need a **nonzero** edge mask,
so the latter closes all ordinary entries (§3.4.8). This is why a nonzero admission byte alone
does not mean a whole tile is passable.

**Road predictions / connection hypothesis.** P: 2527 (`flags0x1186`), 2505 (`0x1186`) and2520
(`0x4186`) are1x1; each has HEAD[0x2f,0x30]=(0,3) and zero WALKMASK. D chain above gives
**code0 and object mask0 for all rotations**, code store `0x100825b7`, mask store
`0x100825d9`, encoder AnnoGame `0x100091f0`, getter `0x10007dc0`. On flat dry ground with no
later additions the cell is0x00000fff: both L3[0] and L0[0] are255, cost10 for each orthogonal
direction. Thus the named0x1000 roads are admitted for carts; restricting them by0x4000 is
not faithful. This is an admission result, not a claim that any road through obstructed terrain
is reachable or that placement permission ignores the type flags.

The round-ap hypothesis is **supported for the specialized high-bit codes, not a universal
ordinary-road connection rule**. All37 named roads were checked with the existing roads tool:
16 have code0, one has code1, fourteen have code4, six bridges have0xd2 at rotation0 and0xaa at
rotation1 (counts recomputed from parsed HEADs). No canonical corner/junction high-bit code
occurs among those37 named-road types. For bridge2516, inputs(10,0) encode0xd2; L[0xd2]=0x11
(left/right bits), rotation1 gives0xaa with L[0xaa]=0x44 (up/down). AnnoGame's shape selectors
1/2/3 also encode corner/three-way/four-way patterns and the getter rotates the canonical
corner/three-way cases, but that algebra does not prove a particular rendered corner/junction
road piece has such a byte. Ordinary STRASSE inputs(0,3) remain0 regardless of rotation; no
neighbour-connection pattern is supplied to the common code store.

**Terrain writer and stop (step 4).** The tile code/mask is reconstructed, not read directly
from one `.scp` byte: file HEIGHT is a corner-height grid, GFXID selects ground graphics, and
GELAENDE/PROD2/FLUESSE items go through object/type placement (`formats/insel.md`).
Reset supplies code2/mask0. The pass at island+0x16c **`0x10081ce0`** samples nine heights
(`0x10081dbf..0x10081dde`) and ORs mask bits after height comparisons (`0x10081e89`,
`0x10081f8d`); there are object-height and river-kind198 branches (`0x10081e9a..0x10081f46`),
including neighbouring edge ORs at `0x10081ff1/0x1008201d`. It leaves the code byte unchanged.
The below-water whole-mask pass at island+0x198 **`0x10092b40..0x10092c94`** computes the mean
of four corners from `[I+0x14]`; if it is below the runtime threshold
`trunc([0x100e34dc]/[0x100ca6fc])`, it ORs0x1ff0000 into the second dword
(`0x10092bf3..0x10092c4d`). This is a geometric closure, not a zero admission code.
The threshold is initialized from another runtime global (`0x1008ff00`); no numeric threshold
is guessed from a zero-initialized image global.

`0x100928a0..0x10092b32` (island+0x19c, called after placement at `0x10082ab9`) can fill
empty tiles with types2/11 based on corner height and ground data, and can rewrite a code
(`0x10092a40..0x10092a5d`). Therefore "no original placed-object item" need not mean the
runtime type word stays0. **Exact stop for a universal grass/coast/water prediction:**
`0x10081f51`, the island's height-result call, and its threshold/initialisation and load-order
chain. The three conditional no-object rows below specify their state explicitly; a generic
coast cannot be assigned one numeric mask here. A river is a FLUESSE object, not the no-object
case: type2701 has code4 and zero *object contribution*, while the overlay/height/river-edge
passes can preserve/add mask bits. A final river mask is not established here.

**Predicted cell table for T30b.3h2 (step 5).** Costs are in **up/down/left/right** order;
only the default expansion's base costs, not the turn algebra queued in T30b.3o. "Object"
rows assume no terrain additions/earlier overlays unless stated. The market entrance is the
first point of type1301 ENTRANCE, approximately(2.1666667,0.5), lying in footprint tile(2,0)
at rotation0; not decoration2550 (PRODTYPE ZIERDE). The2x2 house is type3501, tile(0,0),
rotation0; ordinary pioneer503/citizen540 are4x4 in HEAD and are not substituted silently.

| Kind and condition | Record byte = code | Record mask | Mapper low u16 | L3 admission; costs UDLR | L0 admission; costs UDLR | Writer / stop |
|---|---|---|---|---|---|---|
| Dirt road2527, flat dry | 0 | 0 | 0x0fff | 255;10,10,10,10 | 255;10,10,10,10 | `0x100825b7/0x100825d9` |
| Cobblestone2505, flat dry | 0 | 0 | 0x0fff | 255;10,10,10,10 | 255;10,10,10,10 | same |
| Road2520, flat dry | 0 | 0 | 0x0fff | 255;10,10,10,10 | 255;10,10,10,10 | same |
| Empty dry grass, retained reset code, no mask additions | 2 | 0 | 0x0fff | 0;0,0,0,0 (never used) | 255;20,20,20,20 | `0x1008c821`; final authored tile stop `0x10081f51` |
| Empty water below threshold, retained reset code after whole-mask pass | 2 | 0x1ff | 0 | 0;0,0,0,0 (unused) | 255;20,20,20,20 (geometry rejects) | `0x10092c4d`; final load-chain stop `0x10081f51` |
| Empty mixed coast with retained code2 | 2 | M, sample-dependent | mapper[M] | 0;0,0,0,0 (unused) | 255;20,20,20,20 | **stop `0x10081f51`**, no numeric M claimed |
| 2x2 house3501, object-only tile(0,0) | 4 | 0x1b6 | 0x0770 | 255;40,40,40,40 | 255;15,15,15,15 | `0x100825b7/0x100825d9`, file WALKMASK |
| Market1301 entrance tile(2,0), object-only | 4 | 0x1f0 | 0x00ee | 255;40,40,40,40 | 255;15,15,15,15 | same |
| River2701, final geometry unresolved | 4 | **stop**, overlay/terrain-dependent | **stop** | 255;40,40,40,40 | 255;15,15,15,15 | code `0x1008236d`; mask stop `0x10081f51` |
| Bridge2516 rotation0, object-only | 0xd2 | 0 | 0x0fff | 0x11;10,10,10,10 | 0x11;10,10,10,10 | `0x100825b7/0x100825d9`; AnnoGame getter |

**OQ-171:** answered for the named0x1000 roads: their code0 is admitted for layer3, independently
of the stand-in's flags test. **OQ-170 tile side:** narrowed; the proposition "water and rivers
have an admission byte zero in every layer" is false for these decoded codes (empty reset
code2 has L0=255; river2701 code4 has L0=L3=255). Terrain masks can nevertheless block entry.
The exact final coast/river values, complete authored-load order and running-original comparison
remain open. The synthetic tool tests and derived file predictions are not a Wine confirmation.

#### 3.4.14 The special expansions and the constraint mask (T30b.3o, 2026-10-25)

All addresses are preferred VAs in **AnnoWelt.dll** (base0x10000000). **D** denotes static
instruction reading, **P** a PE-derived cross-check; no live search observation is claimed.
The task requested §3.4.13, already occupied by T30b.3n's tile predictions, so that section
and its incoming links are preserved and the new note is §3.4.14. No simulation change.

**Receiver and common state.** All four expansions, `0x10007540` and `0x10008230` have
this=Y+8. Fn1 saves Y+4 then passes `lea ecx,[edi+4]`; the constructor installs
`[Y+8]=0x100c14e0` at `0x1000637c` (§3.4.12). The constraint call's saved LEA is checked
separately below. Table offsets are absolute Y offsets after that adjustment:

| Field | Meaning / evidence |
|---|---|
| **this=Y+8**, installer `0x1000637c` | Receiver of the four expansions and their helpers; not the +4 interface. |
| this+0x42c = Y+0x434 | Layer block L, loaded at `0x10007214`, `0x100077ca`, `0x10007b4d`, `0x10007eef`, `0x10007550`. |
| this+0x414 = Y+0x41c | Open score-tree sentinel pointer (`0x100073d1`, `0x10007602`). |
| this+0x41c = Y+0x424 | Open-tree count, incremented by `0x10007756`. |
| this+0x420/+0x424/+0x428 = Y+0x428/+0x42c/+0x430 | Free-list head/tail/count (`0x100075a9..0x100075df`). |
| node+0x10,+0x14,+0x16,+0x18 | Chunk pointer, u16 local x, u16 local y, u16 score. |

Let p be the current cell in its 4×4 chunk, low=u16(p), and
`Cj(n)=L[0x400+8*(u16(n+2)&0x3ff)+j]` for a destination cell n **before its mark**.
A try first requires `(u16(n)&geometry_mask)!=0`, then
`(L[u16(n+2)&0x3ff]&key)!=0`. A zero key fails. Geometry/key tests are the caller's work;
helpers `0x10006f70` / `0x10007540` do not add them implicitly.

**Which bits mean which transition.** A reached cell uses one three-bit row selected by
its arrival direction, not its own top nibble or a value fetched from its parent node.
Only the specified three bits of the shifted register are used:

- arrival L (entered from R): bits0/1/2 permit exit D/L/U;
- arrival D (entered from U): bits3/4/5 permit exit R/D/L;
- arrival R (entered from L): bits6/7/8 permit exit U/R/D;
- arrival U (entered from D): bits9/10/11 permit exit L/U/R.

Equivalently, geometry columns `0x111/0x222/0x444/0x888` are exits D/L/U/R across the
three possible other entry edges. A neighbour entered by U/D/L/R is checked for a path
to its opposite edge using `0x2111/0x8444/0x4888/0x1222`; the high bits in those tests
are the corresponding start keys. **P:** all512 mapper outputs at `0x100c1af8` have
reciprocal bit pairs(0,11),(1,7),(2,3),(4,10),(5,6),(8,9), consistent with undirected
subcell connectivity. A later constraint can clear only one directed member, so this
reciprocity must not be assumed after constraints have been ANDed in.

**Shared address/mark/enqueue rules.** In-chunk U/D/L/R addresses are p−16/p+16/p−4/p+4.
At a boundary the same direction fetches the adjacent chunk, then U→(x,3), D→(x,0),
L→(3,y), R→(0,y); its addresses are respectively chunk+4*x+0x30, chunk+4*x,
chunk+16*y+0xc, chunk+16*y. First-direction branches inline lookup/fill through
`0x10017510` / `0x1000b790`; later ones use `0x1000b850` with dir6/2/0/4.
A null chunk fetch/allocation skips that neighbour. No diagonal **cell** is enqueued.

Every successful try stores `score=u16(parent.score+increment)`, then replaces destination
high-u16 bits0..9 with `0x80|outdir`, preserving bits10..15 (including goal bit10).
It allocates a0x1c node, fills chunk/x/y/score and inserts it into the unsigned-u16 score
tree. Straight steps use `0x10006f70`; the first turn is inlined with `0x10007180` and
`0x1000d4e0`; the other turn uses `0x10007540`. The two cost indices for a turn belong
to **the same destination code**: diagonal byte minus outgoing orthogonal byte. No float,
clamp to zero or extra fixed penalty is present; low16 arithmetic is retained.

**Up arrival, code0x86 (`0x100071c0..0x10007533`).**

| Item / neighbour | Geometry/key | Increment | Evidence, in-chunk / chunk-edge |
|---|---|---|---|
| **this=Y+8**, installer `0x1000637c` | g=(low>>9)&7 at `0x100071f3` | — | Current-cell group, independent of parent fields. |
| L | 0x4888; g&1 | C7−C0 | Tests `0x10007204/0x1000733d`; costs `0x1000723d..0x10007255` / `0x1000736f..0x10007387`; marks `0x1000726b/0x1000739d`. |
| R | 0x1222; (g&4)<<2 | C5−C4 | Tests `0x10007411/0x1000746a` through06f30; turn call `0x1000748d` passes orth4,diag5. |
| U | 0x2111; (g&2)<<5 | C6 | Tests `0x100074c2/0x1000750a`; straight call `0x10007527` passes6. |

```text
# this=Y+8; p=chunk+4*(x+4*y); g=(u16(p)>>9)&7
try(L, g&1,      0x4888, u16(parent.score+C7(n)-C0(n)))
try(R, (g&4)<<2, 0x1222, u16(parent.score+C5(n)-C4(n)))
try(U, (g&2)<<5, 0x2111, u16(parent.score+C6(n)))
# shared address/mark/enqueue rules above; omit D, the way back
```

**Down arrival, code0x82 (`0x10007770..0x10007afc`).**

| Item / neighbour | Geometry/key | Increment | Evidence, in-chunk / chunk-edge |
|---|---|---|---|
| **this=Y+8**, installer `0x1000637c` | g=(low>>3)&7 at `0x100077ae` | — | Current-cell group. |
| L | 0x4888; (g>>2)&1 | C1−C0 | Tests `0x100077ba/0x100078fb`; costs `0x100077fb..0x10007813` / `0x10007938..0x10007950`; marks `0x10007829/0x10007966`. |
| R | 0x1222; (g&1)<<4 | C3−C4 | Tests `0x100079da/0x10007a33`; turn call `0x10007a56` passes orth4,diag3. |
| D | 0x8444; (g&2)<<1 | C2 | Tests `0x10007a8c/0x10007ad3`; straight call `0x10007af0` passes2. |

```text
# this=Y+8; p=chunk+4*(x+4*y); g=(u16(p)>>3)&7
try(L, (g>>2)&1, 0x4888, u16(parent.score+C1(n)-C0(n)))
try(R, (g&1)<<4, 0x1222, u16(parent.score+C3(n)-C4(n)))
try(D, (g&2)<<1, 0x8444, u16(parent.score+C2(n)))
# shared address/mark/enqueue rules; omit U
```

**Left arrival, code0x80 (`0x10007b00..0x10007e98`).** Both ret4 paths, at
`0x10007e3a` and `0x10007e98`, were read.

| Item / neighbour | Geometry/key | Increment | Evidence, in-chunk / chunk-edge |
|---|---|---|---|
| **this=Y+8**, installer `0x1000637c` | g=low&7; byte read `0x10007b2e` | — | No shift needed. |
| U | 0x2111; (g&4)<<4 | C7−C6 | Tests `0x10007b40/0x10007cd2`; costs `0x10007b7a..0x10007b9c` / `0x10007d0e..0x10007d26`; marks `0x10007ba6/0x10007d38`. |
| D | 0x8444; (g&1)<<2 | C1−C2 | Tests `0x10007c16/0x10007db1`; turn call `0x10007dd4` passes orth2,diag1. |
| L | 0x4888; (g>>1)&1 | C0 | Tests `0x10007e0b/0x10007e6f`; straight calls `0x10007e2e/0x10007e8c` pass0. |

```text
# this=Y+8; p=chunk+4*(x+4*y); g=u16(p)&7
try(U, (g&4)<<4, 0x2111, u16(parent.score+C7(n)-C6(n)))
try(D, (g&1)<<2, 0x8444, u16(parent.score+C1(n)-C2(n)))
try(L, (g>>1)&1, 0x4888, u16(parent.score+C0(n)))
# shared address/mark/enqueue rules; omit R
```

**Right arrival, code0x84 (`0x10007ea0..0x10008228`).** Both ret4 paths, at
`0x100081d0` and `0x10008228`, were read.

| Item / neighbour | Geometry/key | Increment | Evidence, in-chunk / chunk-edge |
|---|---|---|---|
| **this=Y+8**, installer `0x1000637c` | g=(low>>6)&7 at `0x10007ed6` | — | Current-cell group. |
| U | 0x2111; (g&1)<<6 | C5−C6 | Tests `0x10007ee2/0x10008017`; costs `0x10007f23..0x10007f3b` / `0x1000804f..0x10008067`; marks `0x10007f51/0x1000807d`. |
| D | 0x8444; g&4 | C3−C2 | Tests `0x100080f1/0x10008148`; turn call `0x1000816b` passes orth2,diag3. |
| R | 0x1222; (g&2)<<3 | C4 | Tests `0x100081a1/0x100081ff`; straight calls `0x100081c4/0x1000821c` pass4. |

```text
# this=Y+8; p=chunk+4*(x+4*y); g=(u16(p)>>6)&7
try(U, (g&1)<<6, 0x2111, u16(parent.score+C5(n)-C6(n)))
try(D, g&4,      0x8444, u16(parent.score+C3(n)-C2(n)))
try(R, (g&2)<<3, 0x1222, u16(parent.score+C4(n)))
# shared address/mark/enqueue rules; omit L
```

The combined table is in §3.4.10. Unlike the default start expansion's shift12, these
functions choose an incoming-edge row and charge a destination-dependent turn difference.
Their cost/bit logic is rotationally consistent; their order stays exactly as listed.

**Turn helper `0x10007540..0x1000775e` (step 3, D).** This=Y+8, seven arguments
(chunk,x,y,parent,cell,orthdir,diagonal); ret0x1c. After four saved-register pushes those
are esp+0x14,+0x18,+0x1c,+0x20,+0x24,+0x28,+0x2c. It derives the cost-row pointer
from destination high-u16&0x3ff (`0x1000754a..0x1000755e`), reads Corth and Cdiag,
subtracts and adds the parent's u16 score (`0x10007571..0x100075a1`), and marks the
cell as0x80|orthdir while preserving bits10..15 (`0x1000758f..0x100075a5`). It pops
the free list or allocates0x1c bytes, writes node fields (`0x100075e5..0x100075fe`),
then performs an inlined score-tree insert/rebalance: unsigned-u16 comparison, equal
scores take the right insertion branch (`0x10007611..0x10007639`), rotations04a70/04ac0,
count increment at `0x10007756`. It returns the incremented count, not the new node pointer.
It does not perform admission/geometry checks or write Y+0x448's selected/best record.

**Constraint helper `0x10008230..0x1000851d` (step 4, D).** This=Y+8, arguments
(cache-local tile x, tile y, other-cell pointer); ret0xc. It aligns x/y to4, locates the
4×4 chunk through the same superblock layout and reads the current cell's code byte at+2,
then the other pointer's code byte at+2 (`0x10008371..0x10008397`). Valid current-cell
lookup is a caller precondition: a null lookup path reaches a later dereference; this is
not a documented null-safe fallback. The byte codes are each increased by0x80 modulo256;
marked codes80/82/84/86 thereby become switch indices0/2/4/6.

The result is a **directed transition veto**, not an entire direction-column clear.
For recognized pairs it returns0xffff with exactly the indicated bit cleared; unmatched
pairs return the initialized **zero** (`0x10008244`, `0x10008516`), which the consumer
would use to close every low-u16 bit. Raw codes are shown below, before adding0x80:

| Current code / incoming row | Other0x80 (exit L) | Other0x82 (exit D) | Other0x84 (exit R) | Other0x86 (exit U) |
|---|---|---|---|---|
| this=Y+8; installer `0x1000637c` | Switch `0x10008520` | Result is u16 | All other *other* codes give0 | No code/high-u16 store here. |
| 0x80, row0 | 0xfffd (clear1) | 0xfffe (clear0) | 0 | 0xfffb (clear2) |
| 0x82, row3 | 0xffdf (clear5) | 0xffef (clear4) | 0xfff7 (clear3) | 0 |
| 0x84, row6 | 0 | 0xfeff (clear8) | 0xff7f (clear7) | 0xffbf (clear6) |
| 0x86, row9 | 0xfdff (clear9) | 0 | 0xf7ff (clear11) | 0xfbff (clear10) |
| Any other current code, start nibble | 0xefff (clear12) | 0xdfff (clear13) | 0xbfff (clear14) | 0x7fff (clear15) |

Switch arm ranges: current80 `0x100083b4..0x100083f7`; current82
`0x10008486..0x100084c5`; current84 `0x100083fa..0x1000843d`; current86
`0x10008440..0x10008483`; default current `0x100084c8..0x1000851d`.
Thus a reached cell loses the **one incoming→outgoing connection** used by that route;
a noncanonical/current start cell loses its corresponding top-nibble key. The reverse
move, omitted by the expansions, is not mapped to a single-bit mask here and returns0.
**P validation:** a small ignored branch interpreter checked all65,536 pairs of input
bytes against this description:1,020 nonzero masks, all others0. This is a static switch
cross-check, not execution in the original game.

**Who records/applies the constraints, and the corrected caller lead.**
Producer **`0x10004c90` has this=Y**, called directly with the wrapper's unchanged base.
Wrapper `0x10005750..0x1000588e` is primary vtable `0x100c1424` slot+0x84, installed
at `[Y]` by `0x10006401`. Its call through **Y+4** slot+0x84 at `0x10005760` is fn1;
it then obtains a selected point through Y+4 slots+0x94/+0x98/+0x9c
(targets `0x10003100/0x10003140/0x100031a0`), and passes the two resulting floats to
`0x10004c90` at `0x100057b1`. They are not blindly labelled the figure's original position.

The producer reconstructs a tile-coordinate pair list from reached marks
(`0x10004e08..0x10005094`), constructs/validates a finer subcell route (Y+0x2048 cache,
`0x100054ef..0x100055d5`), and on a failed validation chooses an adjacent pair:

```text
A = path[i]; other = tile_cell(A)             # 0x10005633..0x10005671
B = path[i+1]                               # pointer increment 0x10005678
if count >= 4: count=0; return -1            # 0x1000568f..0x100056b1
entry = Y+0x1c0c+12*count; count++           # 0x100056b4..0x100056bf
entry.x = B.x; entry.y = B.y
entry.mask = constraint(Y+8, B.x,B.y,other)  # 0x100056d0..0x100056e5
return 0
```

The call's this-adjustment is explicit: `lea ecx,[esi+8]` with esi=Y at
`0x10005662`; its value is saved at base-esp+0x30 (`0x10005665`, after two argument
pushes). At `0x100056d0` the other-cell pointer is pushed, so esp+0x34 reloads that
saved **Y+8** at `0x100056d1`; the two remaining pushes are y then x. The three
arguments and ret0xc balance. Stores are entry.y at `0x100056dd`, mask16 at
`0x100056e1`, x at `0x100056e5`; only the u16 of the third dword is consumed.
The coordinates are cache-local, matching fn1's later lookup, not unadjusted world floats.

Return1 tells wrapper05750 validation succeeded; return−1 fails; return0 resets caches
and retries fn1 (`0x100057b6..0x10005873`). Fn1 then iterates the stride12 array,
fetches each tile and applies its mask **before the expansion loop**
(`0x100046ed..0x10004747`, AND low-u16 at `0x10004737`). That gives the table a concrete
purpose: remove the failed coarse-path transition for the retry without changing the
record's high-u16 came-from/goal bits. Full subcell-route validation/reconstruction rules
are not specified by this note; the producer, call path and bit veto are established.

The task's proposed `0x10005890` caller lead does not hold. That primary+0xa4 method
extracts the subcell direction/count path; its final primary+0xac call at `0x10005b37`
resolves to `0x100062f0` (cache reset), not04c90. It is not the direct producer of the
array store at056e1. This corrects the relationship without inventing an extra caller.

**Remaining:** live cells and route confirmation (T30b.3h2), full finer-route validation
and figure/path integration (later T30b work), terrain/load stops in §3.4.13. This closes
the four expansions' local turn/key algebra and the constraint mask, not all transport.

#### 3.4.15 The live cell check in the Wine runner (T30b.3h2, orchestrator, 2026-10-25)

**P by running; the predictions it confirms are the D readings of §3.4.11–3.4.14.** Three fresh
Citizen games in the `AnnoHarness` Wine runner (silent, `tools/orig/backend.py` `WineBackend`,
`macros.new_game`; AnnoWelt at `0x580000`), read-only through the agent's `read` and `scan`.
Scripts in the orchestrator's scratch space, results in `cache/orig/t30b3h2-orch/` (git-ignored;
no cell dump is in git, only the counts below).

**What was read.** The world (`gamestate.find_world`), the search engine `Y = [world + 0x6aa8]`
(its vtable `[Y]` = AnnoWelt + `0xc1424`, checked), the chunk records reachable from the super-block
table at `Y + 0x1c` (walked heuristically: every dword of a super-block that points at a record whose
u16 pair at `+0x40` / `+0x42` is a multiple of 4), their 16 cells, and every island's 8-byte tile
records (`[island + 0xc]`, islands found by the vtable AnnoWelt + `0xcac2c`, rect at `+4..+0xa`).
At 400 % speed the engine is re-used by one search after another (`Y + 0x4ec` / `+0x4f0` and the
chunk set change every second or two), so samples were taken every second for 4–5 minutes
(16 to 42 distinct chunk sets per game), 304 chunks with at least three cells whose low word is
neither `0` nor `0xfff`.

**Result 1 — the fill rule holds (§3.4.11 (e), §3.4.13).** Each of those chunks was matched as a
4×4 template against every island's predicted cells `mapper[(rec1 >> 16) & 0x1ff]`
(`0x100c1af8`): 171 chunks match somewhere, 155 of them at exactly one place, with the low word equal
on **every** non-zero cell (the matched low words: `0xfff` 722, `0` 335, `0x60` 61, `0x801` 58,
`0x770` 52, `0x300` 44, `0xb83` 41, `0xc` 41, `0xee` 40, `0xc1d` 33, `0xb6d` 32, `0x77c` 5,
`0xbe3` 2, `0x410` 1 — thirteen of the mapper's 33 values, so an accidental match is excluded).
In the same cells the **code** (bits 0–9 of the high u16) is the record's byte 8–15 in every fresh
cell (454 cells; record codes 4 for the tree and bush types 1566–1577 and the house types 3400–3414
of the AI towns, 2 for empty ground) and one of `7`, `0x80`, `0x82`, `0x84`, `0x86` in every cell a
search had reached (2 022 cells); **no other value occurred**. Over all samples the open-ground cell
`mask 0 → 0xfff` was seen 33 813 times and the closed `mask 0x1ff → 0` 15 607 times (the below-water
closure of §3.4.13). One road tile was in a matched chunk (type 2524, record code 0, low `0xfff`,
reached with mark `0x86`): consistent with §3.4.13's prediction for roads, but one cell only (the
scripted opening failed in that game and the AI towns had few roads yet) — the road case is confirmed
for the fill, not yet measured for the cart's search (T30b.6).

~~**Result 2 — the chunk coordinates are island-relative.** For 65 of the 155 uniquely matched chunks
`world = island origin + (chunk + 0x40, chunk + 0x42)` exactly; the other 90 sit at other offsets
of the same island, which the heuristic chunk walk explains: it also returns chunk records of
earlier searches still in the pool (the engine's super-blocks keep stale records), whose island
differs. So the u16 pair at `chunk + 0x40` / `+0x42` is the chunk's tile origin **in island
coordinates**, not world coordinates, and the float positions fn1 converts (§3.4.7) are island
coordinates too; `Y + 0x4ec` / `+0x4f0` is not a world offset of the chunk origin (what it is
exactly stays open; it changes per search). For T33: the search runs per island in island tile
coordinates.~~ **Corrected by the orchestrator (2026-10-25 ay, §3.4.16 (b), `0x10002b80`):** the u16 pair at `chunk + 0x40` / `+0x42` is the chunk's origin relative to the **search origin** `Y + 0x4ec` / `+0x4f0`, which is the search rectangle clipped to the island (`max(rect.left, island.x)`, `max(rect.top, island.y)`) in world tiles. It equals the island origin only when the search covers the island's top-left corner, which is what the 65 matches at offset 0 show; the other 90 matches are chunks of a search whose origin had already changed when the origin was sampled (the engine is re-used every second or two). For T33: the cells are addressed relative to the clipped search rectangle.

**Not confirmed here:** the admission bytes and costs in a running search (that needs a figure's
observed route, T30b.5–7), the terrain passes for coast tiles beyond the two bulk cases, and the
special-expansion turn costs (static only). **OQ-175: answered.** The cell is the record's mapped
9-bit mask plus its code byte, filled on first use from the island's tile records; the search marks
it with its came-from codes; the walk rule is the layer block's admission byte and costs by that
code, with the geometry bits of the mask (§3.4.8, §3.4.12, §3.4.14).

#### 3.4.16 What T30b.8 read again for the consolidated rule: the open list's order, the grid origin, the island's subcell mode, the goal test (2026-10-25)

**D**, `tools/disasm.py` on `AnnoWelt.dll` (preferred base `0x10000000`); the address ranges read are named, no listing is reproduced. These four items were the holes that kept `state.md` §5.9 from being a rule T33 can implement; every other clause of §5.9 comes from §3.4.7–3.4.15 unchanged (re-checked against the text, not re-disassembled).

**(a) The open list pops the lowest score; equal scores pop in insertion order (D).** `0x1000d4e0..0x1000d554` (the score-tree insert used by the first turn and by the start nodes; its `this` is `Y + 8 + 0x414`, so `[this]` is the header at `[Y + 0x41c]`): the walk compares `cmp si, [node + 0x18]` as **unsigned u16** (`0x1000d4fb`) and takes the left child `[node + 8]` when the new score is **below** (`jae` not taken, `0x1000d501..0x1000d503`) and the right child `[node + 0xc]` when it is **equal or above** (`0x1000d508`). The header keeps `[header + 4]` = root, `[header + 8]` = leftmost, `[header + 0xc]` = rightmost (updates `0x1000d521..0x1000d548`: a node inserted as the left child of the leftmost becomes `[header + 8]`). fn1 takes `[sentinel + 8]` (`0x1000474f`, `0x10004844..0x1000484a`, §3.4.7), i.e. the **leftmost node = the lowest u16 score**. Equal scores always descend to the right, so a later node sits **after** the equal ones already in the tree and the earlier one pops first. The same compare and the same branch are inlined in the straight-step helper `0x10006f70` (`0x10007030..0x10007036`, the right-child tie check `0x10007050..0x10007054`) and in the turn helper `0x10007540` (`0x10007615..0x1000761b`, tie check `0x10007635..0x10007639`), so every insertion of the family orders alike. The rebalancing after `0x1000d554` was not read: a rotation does not change the in-order sequence, which is all the pop order depends on.

**(b) The grid origin `Y + 0x4ec` / `Y + 0x4f0` (D, `0x10002b80..0x10002bfa`, `this = Y + 4`).** The setup takes a rectangle (argument 1: left, top, right, bottom as four i32) and the island (argument 0): `Y + 0x4ec = max(rect.left, island x)` and `Y + 0x4f0 = max(rect.top, island y)` (island x / y = slots `+0x50` / `+0x54`, `0x10002b9e..0x10002bf8`); the right and bottom are clamped to `island x + island width` / `island y + island height` (slots `+0x58` / `+0x5c`) and the grid size in chunks is `(right − left + 3) / 4`, `(bottom − top + 3) / 4`. So the origin is the **top-left of the search rectangle clipped to the island, in world tile coordinates**; for a figure the rectangle is its island's (§3 "Set-up for a figure"), which makes the origin the island's own origin, as §3.4.15 measured (65 chunks at offset 0). This closes §3.4.15's "what `+0x4ec` / `+0x4f0` is exactly": the engine's chunk coordinates are `world tile − origin`; every float the engine takes (starts, goals) is in **world tile coordinates** and is converted by subtracting the origin (§3.4.7 pre-loop) or compared after adding it (the goal test below). The origin only fixes the chunk partition; it cannot change a result (cells outside the island are 0, §3.4.12).

**(c) The island's subcell fill, mode 1 (D, `0x10091ffe..0x100920eb`, `ret 0x14`).** Called by the start-cell helper `0x10003aa0` with `mode = [Y + 0x2044] + 1` (§3.4.12). For the one tile at the rectangle's top-left, if it lies inside the island, it reads the tile record's second dword, takes the nine mask bits `(rec >> 16) & 0x1ff` and writes **nine bytes** into the destination at row stride `width`: byte `3·r + c` = `8 + ((mask >> (3·r + c)) & 1)` (`0x10092064..0x100920e2`: `mov dl, 8`, `and bl, 1`, `add bl, dl` per bit; bit 0 → row 0 column 0, bit 1 → column 1, bit 3 → row 1 column 0, bit 8 → row 2 column 2). So a **free sub-cell is 8, a blocked one 9** (the mask is a blocked-subcell bitmap, §3.4.13). A tile outside the island writes nothing (the border byte 9 of `0x10003aa0`'s buffer stays).

**(d) The goal test `0x10003220` (D, `this = Y + 4`, one argument P = `&node + 0x10`, `ret 4` at `0x1000376a` / `0x1000377d`).** `[P]` is the chunk, `[P + 4]` / `[P + 6]` the node's u16 x / y, `[P + 8]` its score dword. The node's tile in world coordinates is `tx = u16[chunk + 0x40] + x + [Y + 0x4ec]`, `ty = u16[chunk + 0x42] + y + [Y + 0x4f0]` (`0x1000324d..0x1000329d`; both as floats times `1.0f`, `0x100c1138`). It builds a **9-bit goal mask `M`** of the sub-cells of that tile that hold a goal, from three goal lists held by the engine (the register slot `+0x70`, `0x10002df0`, takes at most 256 entrance points; the arithmetic below shows where they live):

1. **Point goals** (`0x1000329d..0x10003340`): count `[Y + 0x578]`, entries of two f32 `(gx, gy)` from `Y + 0x57c`, 256 entries ending at `Y + 0xd7c`, where the next list begins. A point lies in the tile iff `tx ≤ gx < tx + 1` and `ty ≤ gy < ty + 1` (the `fcomp` / `test ah, 0x41` pairs `0x100032be..0x100032fe`); it sets bit `col + 3·row` with `col = trunc(3·(gx − tx))`, `row = trunc(3·(gy − ty))` (`0x10003300..0x10003337`: `fmul` by `−3.0f` at `0x100c1410` and `3.0f` at `0x100c128c`, the `__ftol` `0x100b5f90`, `1 << (col − (−3·row))`).
2. **Tile goals** (`0x10003346..0x100033e5`): count `[Y + 0x1800]`, entries of two u16 `(x, y)` from `Y + 0x1804` (stride 4); a tile equal to `(tx, ty)` ORs `0x1ff − V` into `M`, `V` = the return of the island's slot `+0x12c` called with `([Y + 0x2044], tx, ty)` (`0x100033ac..0x100033d4`); that slot's body was **not read** (DRAFT: it returns the tile's blocked mask, so a tile goal is "every free sub-cell"). The jobs of §1–§2 do not register tile goals.
3. **Disc goals** (`0x100033e7..0x10003695`): count `[Y + 0xd7c]`, 12-byte entries of three f32 (centre x, centre y, radius) from `Y + 0xd80`, rasterised over the tile's sub-cells (the midpoint-circle loop `0x10003508..0x1000357a`, the sub-cells read through island mode 1, the bytes `== 8` are free). Not read beyond this shape (open; neither job of §1–§2 registers a disc goal).

Then `0x100036a8..0x10003757`: with `bl = 1` and the node cell's code `c = u16[cell + 2] & 0xff`: `c` in `0x80..0x87` → row `((c + 0x80) & 0xff) >> 1` = 0 / 1 / 2 / 3 for `0x80` / `0x82` / `0x84` / `0x86` (odd codes round down); any other code → row 4 (start cell or fresh code, `0x100036db..0x100036ed`). Row `k` of the table at `0x100c1160` is nine 4-byte entries `(u16 mask, u8 col, u8 row)` (stride `0x24` per row, `0x100036fb..0x100036fe`; P, read from the PE): the nine single-bit masks `1 << (3·row + col)` in an order that depends on the arrival direction (row 0, arrived going left: `0x20 (2,1)`, `0x04 (2,0)`, `0x100 (2,2)`, `0x10 (1,1)`, `0x02 (1,0)`, `0x80 (1,2)`, `0x08 (0,1)`, `0x01 (0,0)`, `0x40 (0,2)`; the other four rows list the same nine masks in other orders; the start row begins `0x10 (1,1)`, `0x08 (0,1)`, `0x02 (1,0)`). For each entry whose mask intersects `M` (`0x10003708..0x10003717`) the callback `[Y + 0x440]` is called as `(engine = Y + 4, &record, [Y + 0x444])` (`0x1000371c..0x10003737`) with the 16-byte record `{chunk, (x, y) dword, score dword, col, row}`; `bl &= result`; after each call the continue byte `[this + 0x438]` (= `[Y + 0x43c]`) is read, and if it is **zero** the function returns 0 at once (`0x1000373f..0x1000377d`); otherwise it returns `bl` after the last entry. With `M = 0` no callback is made and the answer is 1.

Consequence for fn1 (§3.4.7): a popped cell with bit 10 whose test answers **1** is expanded like any other; an answer **0** with the continue byte still set drops the node without expanding it and the search goes on; an answer 0 with the continue byte clear (engine slot `+0xb0`, `0x10003210`: the callback asked to stop) ends the search. The callbacks of §1.1 / §2 answer 1 to go on and 0 when they stop the search (§1.1 item 4), so for them **the goal tile is expanded unless the search stops there**, and a tile whose mask `M` has several bits is reported once per goal sub-cell in the arrival-dependent order above.

**What stays open after this read:** island slot `+0x12c`'s body (tile goals), the disc goals' details, whether the third list (`Y + 0x1804`) and the fourth pre-loop walk (`0x10004368..`, calls `0x10008540` / `0x1000d6a0`) are used by any figure job, and the finer-route validation's inner rules (§3.4.14); none is needed by the cart or the carrier as far as §1–§2 read.

### 3.5 Step 7 is BLOCKED on a figure-position reader

Step 7 of T30 calls for building the playbook opening in the running original, reading the tile records of the opening's buildings and the market cart's position tick by tick, and checking each observed tile against the decoded rule. The Wine runner is set up (`docs/testing/original-wine.md`), the opening macro runs (`tools/orig/opening.py`), and `tools/orig/gamestate.py` reads world, players and city stocks. **It has no figure-position reader**, and no decoded figure object layout in memory (the sim's `figure.hpp` has one, but the ORIGINAL's figure layout is different). The reader is a task of its own: find the figure table off `world + ?`, read figure records (position, direction, tile), export them to the harness. **Done here instead:** step 7(a) and (b) are reduced to "memory-compare the five 7168-byte blocks at `0x100d1140` with the DLL image after a scenario loads" (no figure reader needed), left as a next-task sub-step of T30b with a mem-reader call plan. Step 7(c)/(d) wait on T30b's figure reader.
