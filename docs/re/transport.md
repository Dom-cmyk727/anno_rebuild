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
- **Cells**: blocks are made on demand (`0x1000b790`) and filled from the island's tiles by island slot `+0x128` = `0x10091fa0(mode, …)`; figures pass mode 0 (`0x1003ad74`: `+0x50(island, &rect, 0, 0)`), whose cell for a tile is `table[mask] | byte << 16`: the 9-bit field at bits 16–24 of the tile record's second dword mapped through the u16 table `0x100c1af8`, and the byte at bits 8–15 moved unmapped to bits 16–23 (`0x1008ff70`, §3.1 (b)). Each tile is a 3 × 3 grid of sub-cells; path steps are counted in thirds of a tile (§4). **D** for the structure; **likely**: the 9-bit mask is the walk mask of the tile's object (every `ANLAGEN` entry has a `WALKMASK` record, P 921 of 921; the writer is not read); **open**: what the layer tables `0x100d1140` hold (likely static `.data` with image content, §3.1), so which tiles a cart (layer 3, P: figure 220 `LOGIC[0x10]` = 3) and a carrier (layer 0, figure 203) may enter (OQ-160).
- **Goals**: building slot `+0xf8` = `0x10072be0` (D): unless the building's flags bits 4/5 are set (masked with the caller's argument), its type's entrance points (rotated, plus the building's tile) go to engine `+0x70` (at most 256, `0x10002df0`). P: 326 of 921 types have an `ENTRANCE` record (8–32 bytes: 1–4 points of two f32), all on sub-cell centres (`k/6` with `k` odd); markets 1301–1305 have 2–4, the weaver 1001 two, the grain farm 811 three.
- **Order and stop**: the engine calls the callback for the goal nodes it reaches (`0x10003d90` and its loop); a callback answer 0 or slot `+0xb0` (`0x10003210`, `+0x438` = 0) stops it; slot `+0x90(node, 4)` = `0x10003030` sets the best node unconditionally for a priority ≥ 4, `+0x94` returns it. **D** for these slots; **likely**: goals are reached in order of path cost (the cart's early stop and the home walk's "first goal" only make sense so); **open**: the expansion itself (neighbour order, the cost of a step, the tie order), which is the bulk of `0x10003d90`, `0x10004890`, `0x10005890` (OQ-161).
- **The road rule (OQ-103)**: what the code requires is the layer table, which is open. The files point one way: carts use layer 3 and carriers layer 0, so their passable cells differ (D: different layers; likely: the cart's layer is the road net, as `production.md` §5.2 and the tutorial series say, and the carrier's is open land, as the series says for wool fetched without a road, V1 03:34).

### 3.1 The walking layers (T27)

Time-boxed RE of OQ-160 (`tools/disasm.py`, `tools/xref.py` on `game/app/AnnoWelt.dll`; corrected after the round v review). What is now **D**, what is **likely**, what is still **open**:

- **(a) The table pointer and where it lives. D.** Two setters compute the per-layer table pointer and store it: `0x1000dd20` writes `[engine + 0x430] = 0x100d1140 + 0x1c00·layer` (`lea edx,[eax*8]; sub edx,eax; shl edx,0xa; add edx,0x100d1140`), and `0x1000f010` writes the same value to `[+0x1030]` of its object. So each walking layer owns a `0x1c00`-byte (7168) block; five blocks are used (P: the sixth `0x1c00` bytes after them are all zero). The base-relocation table (`xref.py --reloc`) lists exactly two absolute references to `0x100d1140`, at `0x1000dd32` and `0x1000f022` inside the two setters, and **none** to any address inside the five blocks (`0x100d1141`–`0x100d9d3f`; the next referenced `.data` address is `0x100dcb44`). `0x1000dd20` is held in the vtables at `0x100c1320`, `0x100c1564` (slot `+0x80` of `0x100c14e4`, the engine at world `+0x6aa8` `+4`, §3) and `0x100c1934`; `0x1000f010` only at `0x100c1abc`, another vtable, so `[+0x1030]` is likely a field of another engine class (open).
- **The blocks hold initialised data; that they are not written at run time is likely.** D: `0x100d1140` lies in `.data` (characteristics `0xc0000040` = initialised / read / write) with non-zero image content, and no absolute reference points into the blocks (a), so a run-time write would have to go through the stored pointers `[engine + 0x430]` / `[+0x1030]`. Read so far through the pointer: the reader `0x1000daf0` (below); stores through it were not searched. So "not filled at run time" (which would correct §3's earlier "filled at run time") is **likely**; T30 step 7a settles it (a compare with the running original's memory).
- **(b) The cell value the search reads. D.** `0x10091fa0` dispatches on a mode 0–7 through the table `0x10092190`; mode 0 (the figures', §3) goes to `0x10091fba`, which calls `0x1008ff70`. Its loop `0x10090050`–`0x1009007b` reads each tile record's second dword, takes bits 16–24 (`shr 0x10; and 0x1ff`) as the index into the u16 table `0x100c1af8` (`.rdata`; its only reference is the load at `0x1009005f`), moves bits 8–15 to bits 16–23 (`and 0xff00; shl 8`) and stores `table[mask] | byte << 16` as the cell. So the layer blocks are indexed (somehow, open) by the mapped value, not by the raw 9-bit mask.
- **Image statistics of the blocks (P; their meaning open).** `0xff` bytes per block: layer 0 = 316, layer 1 = 409, layer 2 = 561, **layer 3 = 75**, layer 4 = 613; distinct byte values per block: 50, 53, 74, 42, 87. With that many distinct values an entry is not a plain yes/no flag, so the `0xff` counts do not rank the layers (with `0xff` = passable layer 3, the market cart's (P), would be the most restrictive; with `0xff` = blocked the least). They say nothing about whether the stand-in (layer 3 → roads only, every other layer → open land) has the right shape.
- **Open, and the next address.** The **meaning** of a block's entries (how a cell value indexes the `0x1c00` block and what the entry says to the search) is not decoded. The search functions `0x10003d90`–`0x10004867` and `0x10004890`–`0x1000588f` do not access `+0x430` (a displacement sweep; their `+0x42c` accesses at `0x10004831`/`0x1000483e` are a counter). The direct reader of the pointer is **`0x1000daf0`** (held in the vtable at `0x100c18b0`): it reads `[engine + 0x430]` at `0x1000db0e` and adds a u16 taken from its argument; a twin with a frame shifted by 4 bytes is `0x1000ed40` (vtable `0x100c1a30`; `+0x430` at `0x1000ed50`, `+0x434` at `0x1000ed77`). Those two and their callers are the next addresses (T30 step 2). The RE of OQ-160 stays open (the owner decided on 2026-10-24 how it is settled: T30), and the stand-in (road-flag tiles for layer 3, open land otherwise) is kept; the statistics above neither confirm nor refute it.
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
- **WALKMASK is 12 zero bytes for every named road** (37 of 37). (counts corrected in T30b.1; T30 wrote 36 and 10) 282 of 921 Anlagen entries carry 12 zero bytes; the rest carry a per-cell u32 mask (one per footprint cell). Named-road walkability therefore is **not stored per cell on the type**: it must come from a different mechanism.
- **The bridge "transition" pieces that DO carry per-cell WALKMASK:** ids 3955 (`BRUECKE`, 24 bytes `3f000000 … 3f000000`, 6 cells of 6-bit mask), 3956 (12 bytes `ff010000 00000000 ff010000`, 3 cells with `0x1ff` / 0 / `0x1ff`), 3961 (`BRUECKE`, 12 bytes `ff0f0000 … ff0f0000`), 3992 (`BRUECKE`, `07000000 07000000 05000000`), 3993 (`BRUECKE`, `3f000000` × 3). These look like the ramps/approaches where the walkable cells are a subset of the footprint.

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
| `0x10001212`–`0x10001229` | `[ecx + 0x454]` (receiver), `[ecx + 0x458]` (arg) | Stores a 4-dword buffer at `[esp + 0x14..0x24]`: `(edi, edi + 2, edx, edx + 2)` = `(x0, x1, y0, y1)` in `W + 0x430..0x434` origin-shifted sub-cell → tile coordinates. |
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

- `xref.py --reloc AnnoWelt.dll 0x100d1140`: exactly two absolute references, both inside the two setters above (`1000dd32`, `1000f022`). **No absolute reference points into any address of the five blocks (`0x100d1141`–`0x100d9d3f`)**; the next referenced `.data` address is `0x100dcb44` (`xref --reloc 0x100d1141 0x100e1000`). So any run-time write must go through the stored pointers at `[engine + 0x430]` / `[+0x1030]`.
- `xref.py 0x430 0x431` lists every instruction with the displacement `+0x430`. The one that writes `[reg + 0x430]` in the layer-table context is the setter `0x1000dd20`'s `mov [ecx + 0x430], edx` — a store that writes the table POINTER, not an entry. Every other `+0x430` match in `AnnoWelt.dll` is in a different class (integer coordinate at that offset). I did not read `AnnoGame.dll`, `AnnoScene.dll`, `AnnoDisplay.dll` for cross-module writers; the memory compare of step 7(a), when a reader exists, settles it.

**Conclusion:** the layer blocks are **initialised data**, and writes at run time, if any, go through the engine pointers (not through the base address). Writes through the pointer have not been traced; T30 step 7(a) is the straightforward way to decide (compare the DLL image with the running game's memory at those addresses).

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

### 3.5 Step 7 is BLOCKED on a figure-position reader

Step 7 of T30 calls for building the playbook opening in the running original, reading the tile records of the opening's buildings and the market cart's position tick by tick, and checking each observed tile against the decoded rule. The Wine runner is set up (`docs/testing/original-wine.md`), the opening macro runs (`tools/orig/opening.py`), and `tools/orig/gamestate.py` reads world, players and city stocks. **It has no figure-position reader**, and no decoded figure object layout in memory (the sim's `figure.hpp` has one, but the ORIGINAL's figure layout is different). The reader is a task of its own: find the figure table off `world + ?`, read figure records (position, direction, tile), export them to the harness. **Done here instead:** step 7(a) and (b) are reduced to "memory-compare the five 7168-byte blocks at `0x100d1140` with the DLL image after a scenario loads" (no figure reader needed), left as a next-task sub-step of T30b with a mem-reader call plan. Step 7(c)/(d) wait on T30b's figure reader.
