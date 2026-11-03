# The forester chain (`AnnoWelt.dll`, `AnnoGame.dll`): building 801, figure 102, the harvest job

**Status:** DRAFT for `docs/re/forestry.md` (MAP-ECON-1 E1-FINAL, research only, 2026-11-03; supersedes the E1 draft: §2.1 draw, §2.11 residual, new §2.13, §4.2-§4.4 and §7 are corrected per the orchestrator's review REVIEW-CLOSE2). Static analysis with `tools/disasm.py`, `tools/xref.py`, `tools/figuren.py`, `tools/insel.py` (private venv: Python 3.14.6, pefile 2024.8.26, capstone 5.0.7, unicorn 2.1.4); one emulation harness over the original bytes on synthetic memory. No original code is copied; behaviour is paraphrased, pseudocode is ours. Addresses are virtual addresses (image base `0x10000000` in both modules).

**Modules and data (sha256, full list in `SOURCE-IDENTITY.txt`):**
`AnnoWelt.dll` `f220a878510f740c4cfb7224ade0a260e9a0fd7e99113e1201e7a37c0dfa6ae2`, `AnnoGame.dll` `ac721220afd27ebca5fd2ece6a8ff452289b9d7458c4e091c68adea2012db7bc`, `Anlagen.dat` `b2317a5c24b8c9fe796cf40f3fedaefcfa30ad3f185df9e69b2dca6a03d8dd75`, `FigurenHeader.dat` `29fa1f0528fb337d9a61e8b73169281fd05bb27efa6df65638580a4c36c468a8`; 465 `Scenes/Islands/*/*.scp` (sorted-hash digest `67032d8a…834a`). `AnnoFrame.dll` was **not** needed: the path never leaves `AnnoWelt`/`AnnoGame`.

**Evidence labels.** **D** = disassembly (module + address), **E** = executed: the real bytes of the named function run in unicorn on synthetic memory with stub methods for the objects it calls (`tools/emu_forester.py`; this is a harness, not the game), **P** = parsed from a shipped file, **A** = adaptation proposal (ours, not the original), **U** = unknown (next address given), **NOT_RUN** = needs the running original. Nothing here is stated as fact without one of D/E/P.

## 0. Answer in short

The forester (801) is a `PLANTAGE` farm. Its worker (figure 102, route type `MAEHER`) runs **job `0x2a`**, the same generic *harvest-a-tile-object* job as the grain mower. One trip: pick the nearest reachable unclaimed tile whose object's output goods is `BAUM` within an 11 × 11 square, claim it, walk there, wait 13.0 (6 500 ticks), turn the tile object into its predecessor id (`id − 1`, the young tree) with a cleared growth phase, carry **1 t (256) of `BAUM`** home and add it to the farm's raw stock. Regrowth is not a planting job: the cut tree *is* the young stage, and the existing growth sweep (`0x1008b5f0`) ripens it again. The farm converts raw → `HOLZ` at 32 raw (0.125 t) per step for 14 wood (0.0547 t), so one trip makes 0.4375 t of wood.

## 1. Files (P)

### 1.1 `Anlagen.dat` entry 801
`VERSION2`, `HEAD` 56 bytes, `PRODTYPE "PLANTAGE"`, `PROD3` 68 bytes, `INFRAMIN "INFRA_EVER"` (no research record: available from the start), `WARE "HOLZ","UNUSED"`, `ROHSTOFF "BAUM","UNUSED"`, `ENTRANCE` 8 bytes, `BAUSAMPLE`, `WALKMASK/SHOTMASK/OWNMASK` 36 bytes, `FIREPOS`, `OBJECTPOS`.

| Field | Bytes (offset) | Value | Meaning / consumer |
|---|---|---|---|
| HEAD[0], [1] | `03 03` | 3 × 3 | footprint |
| HEAD[3..6] | `00 00 02 00` | brick 0, wood 0, **tools 2**, marble 0 | build cost (`AnnoGame 0x1000a5a0`) |
| HEAD[8..9] | `96 00` | 150 | gold cost |
| HEAD[0x10] | `05` | 5 | stage count `S` (unused for a farm) |
| HEAD[0x11] | `05` | **5** | action radius `r` (type `+0x65`, slot `+0x78` = `0x10007d90`) |
| HEAD[0x1c..1d] | `84 00` | `0x0084` | flag word |
| HEAD[0x2c..2d] | `b8 0b` | 3000 | maximum hit points |
| PROD3 `00`, `02` | `04 00`, `0c 00` | 4, 12 | running costs (off, on) |
| PROD3 `04` | `05` | cap0 = 5 t → `+0x208` = **1280** | output capacity (8.8) |
| PROD3 `08`, `0c` | f32 `18.0`, `18.0` | `T0` = `T1` = 18 s per ton | |
| PROD3 `10`, `14` | f32 `2.25`, `0.0` | `I0` = 2.25, `I1` = 0 | |
| PROD3 `20` | f32 `2.046875` | display rate | display only (does not equal the 3.28 t/min the step rate gives) |
| PROD3 `28..2a` | `01 02 00` | group 0: **1 figure type, max 2 out**, start 0 | `max_figures(0)` = 2 |
| PROD3 `32` | u16 `102` | figure list[0] = 102 | |
| ENTRANCE | f32 `(1.8333, 1.5)` | one entry point | send prerequisite (§2.3) |

Load-time conversion (D `AnnoGame 0x10009627`–`0x10009717`, E-checked arithmetic, `docs/re/production.md` §2.3): `q0 = 256/18 = 14.2222…`; `rate0 = ftol(q0 + 0.5) = 14`; `need0 = ftol(q0 · 2.25) = 32` (exactly 32 at 24-, 53- and 64-bit mantissa: the product is the rational 32; checked with exact rounding emulation; no OQ-032 sensitivity here). Same method reproduces 811 (37/80) and 824 (26/25) of the documented values.

### 1.2 `FigurenHeader.dat` figure 102
`BEWOHNTYPE "MAEHER"`, **`ROUTETYPE "MAEHER"`**, `PATH "Holzfäller\"`, `HEAD` 20 bytes `00 00 00 00 | 0a d7 23 3c | …` (f32[1] = **0.01** = walking speed), `LOGIC` 44 bytes:

| LOGIC | Value | Type field (loader `AnnoGame 0x1000fe00`, `LOGIC` branch `0x1001019e`–`0x100103a4`) |
|---|---|---|
| f32 `[0]` = 5.0 | → `+0x34` | not used on this path (U-7) |
| f32 `[4]` = **13.0** | → `+0x30` | the cut/load wait (type slot `+0x64(1)` = `0x1000fd10`) |
| f32 `[8]`, `[0xc]` = 4.0, 4.0 | → `+0x38`, `+0x3c` | not used on this path (U-7) |
| byte `0x10` = **2** | → `+0xff` | walking layer (slot `+0xac`) |
| byte `0x11` = **1** | `<< 8` → `+0xf2` = **256** | carry capacity (slot `+0x84` = `0x1000fb50`) |

For comparison (P): the grain mower 101 has layer 2, capacity 3 t, wait 9.0; carrier 203 layer 0, 3 t, wait 2.0; market cart 220 layer 3, 5 t, wait 1.0. In the rebuild's `FigureType`: `speed_q24` = round(0.01 · 0.2 · 2^24) = 33 554 (one tile per 500 ticks), `capacity_88` = 256, `layer` = 2, `load_ticks` = 13 × 500 = **6 500**.

### 1.3 Tile objects (P)
Entry ids whose `WARE[0]` is `BAUM` (goods 70): 105, 107, 109, 111, 113, 115, 117, 119 (8); 202, 204, …, 238 (the even ids, 19); 241, 243, 245, 247, 251, 255, 257, 259, 261 (9); 405, 407, …, 423 (10): **46 entries**, all `PRODTYPE ROHSTOFF`. For 44 of them entry `id − 1` is `ROHSTWACHS` with `S = 5`, `k = 2`, `T0` = 480 s (growth period `P` = 24 000, `P/2` = 12 000 ticks, 20 flips). The two exceptions (hazards, §4.5): **259** (`id − 1` = 258 is `ROHSTWACHS` with `S = 1`: it never regrows) and **261** (`id − 1` = 260 is `ROHSTOFF`: the harvest fails every time).

The island files hold 677 684 `GELAENDE` items with `WARE` = `BAUM` over 465 `.scp` files (P, `trees2.py`; 10 of them id 259, 7 id 261), so *existing eligible trees* are plentiful. 1 553 further items are young trees (`ROHSTWACHS` whose successor is a `BAUM` tree).

## 2. The trace (D unless marked)

### 2.1 The farm tick `AnnoWelt 0x1006ea90` (forester path)
Read instruction by instruction; the production half is `docs/re/production.md` §4.2 (confirmed there). What matters for the forester:
- Gate `(u16)(t − [+0xa]) ≥ 0x1f4`, `[+0xa] += 0x1f4`; `[+0x22]` switched on; `[+0x1e]` activity `· 255 >> 8`; flags `[+0xc]` bits 4/5 suspend.
- `bl` = `max_figures(0)` (type slot `+0xb8`); `cmp al, 1` is true only for a single-worker farm. **801 has `bl` = 2**, so the "worker out" skip (`0x1006eb2e`) never applies and the dispatch (`0x1006ed78`) runs at the end of **every** step (`cmp [esp+0x1c], 1; ja 0x1006ed8b`), whether the step produced, was starved (status 1) or was full (status 2).
- **World draw (D, order pinned in E1-FINAL, `0x1006ed78`-`0x1006ee2d`):** the dispatch is reached only after the gate `(u16)(t - [+0xa]) >= 0x1f4` (`0x1006eaa7`, early return `0x1006ee58` otherwise), the switched-on byte `[+0x22]` (`0x1006eac0`, return when 0) and the suspend flags (`0x1006eaf1`, return when bits 4/5 are set): those three exits leave **without a draw**. After them the order is: (1) `max_figures <= 1` and a worker out/production this step -> skip (`0x1006ed78`-`0x1006ed85`); (2) count the non-null worker slots, `count >= max_figures` -> return (`0x1006edac`); (3) the rate test `384/rate0 >= raw/need0` -> else return (`0x1006ede4`); (4) **the draw** (`0x1006edf3`-`0x1006ee06`: city `+0x18`, island `+0x18`, world `+0xb4` = `0x100a1740`: `[world+0xfc] = ([world+0xfc]+1) mod [world+0xfe]`, result discarded); (5) the send `0x10072d20` through building slot `+0x118` (`0x1006ee2d`), which may refuse (§2.3, §2.13). **One draw per ELIGIBLE dispatch attempt** = every step that passes (1)-(3); whether the send is then refused or a figure is created does not change the draw count. A full output store (status 2) and a starved step (status 1) do not stop an eligible attempt for a two-worker farm (`max_figures` > 1 never takes exit (1)). This changes the PRNG stream of every farm-family fixture and golden (the fur trapper's own tick was not read: U-13).
- Dispatch (`0x1006ed8b`–`0x1006ee56`): count the non-null worker slots `+0x24 … +0x34` (5); stop if `count ≥ max_figures`; `rate0` = type slot `+0xe0(0)` (14), `need0` = `+0xe4(0)` (32); **send iff `384 / rate0 ≥ raw / need0`** (both signed integer divisions, `raw` = u16 `[+0x1c]` **after** this step's consumption). For 801: `384/14 = 27`, so a worker is due while `raw / 32 ≤ 27`, i.e. `raw ≤ 895` (3.5 t − 1).
- The send: `figure type = type.slot+0xb4(0, 0)` (list[0] = 102); **job id = figure type slot `+0x54`**; `building.slot+0x118(job, type, t)`; if a figure came back, `figure.slot+0x84(0)` is the cargo object and `cargo.slot+0(type.slot+0xb0(0))` sets its goods to the farm's **input 0 = `BAUM` (70)** (`0x1006ee06`–`0x1006ee56`).

### 2.2 Figure-type slot `+0x54` = the route type (job id)
`AnnoGame` figure-type vtable `0x10021d10` (installed at `0x10011129`); slot `+0x54` = `0x1000fae0` = `mov eax, [ecx+0xc]; ret`. The loader `0x1000fe00` writes `[type+0xc]` from the `ROUTETYPE` record (branch `0x10010029`–`0x100100d9`, store `0x100100cf`: names matched through game slot `+0x48` kind `0x3b` = `0x10013b40`, the table of 80 string pointers at `0x10024fb8`). From that table (read, D+P): 1 `WALKDEST`, 2 `WALKFOLLOW`, 3 `WALKANLAGE`, 11 `ATTACK`, 12 `JAEGER`, 21 `FLIGHT`, 31 `HELP`, 32 `LOESCH`, 33 `SEUCHE`, 41 `ERNTE`, **42 (`0x2a`) `MAEHER`**, 43 `MAEHER_SPEZ`, 44 `STEINMETZ`, 45 `FISCHER`, 46 `WEIDETIER`, 51 `TRAEGER`, 52 `MARKTLER`, 53 `EINKAUF`, 54 `PARK`, 61 `SPECIAL` … So figure 102's `ROUTETYPE "MAEHER"` gives **job `0x2a`**, which the small farm's worker already uses (`docs/re/production.md` §4.5, `0x1006f660`). All 24 `PLANTAGE`/`UNIVERSAL` buildings whose first figure is a `MAEHER` route use it (P, `routes.py`): the forester 801, grain farm 811, plantations 808, 812–823, …, small farm 809.

### 2.3 Sending (`0x10072d20`) and the entrance prerequisite (`0x10072b50`)
`0x10072d20(job, type, t)`: refused while flags bits 4/5 are set (`(f >> 1 & 0x10) | (f & 0x10)`); `0x10072b50` must succeed: it asks the building type for its entrance points of the building's rotation (`type slot +0x98(rotation & 3, &buf)`, `AnnoGame 0x100084c0`) and fails when the type has none; it does **not** test whether the entrance tile is walkable. The registry (world slot `+0xa0`, then its slot 0) creates the figure at the first entrance point (floats `x + e.x`, `y + e.y`); building and figure attach to each other (`+0x28` both ways, the city too); `figure.slot+0xa4(job)` makes the job (factory `0x1003b0e0`) and `figure.slot+0xac(job)` takes it. 801's single entrance (1.8333, 1.5) lies inside its 3 × 3 footprint.

### 2.4 The job object (factory `0x1003b0e0`, job `0x2a`)
Byte table `0x1003b480` / jump table `0x1003b450` (read in full, D): `0x29`, `0x2a` → `0x1003b255`: `new(0x48)`, base constructor `0x1005c350(this, this+0x34, this+0x3c, this+0x44)`, then
- step **A** at `+0x34`, vtable `0x100c44c0`: type 7 (`0x1005a030`), **start** `0x10058ae0`, arrival `0x1005f710` (returns 1, nothing else), abort `0x100545c0`, `+0x1c` = `0x1005be80` (output: step `+4` → caller);
- step **B** at `+0x3c`, vtable `0x100c4858`: type `0xb` (`0x10054750`), **start** `0x100553e0`, **arrival** `0x10054530`, abort `0x100545c0`, `+0x18` = `0x1005be60` (input: sets step `+4`);
- step **C** at `+0x44`, vtable `0x100c4640`: the cart's/carrier's "walk home and unload" (type 4, start `0x10057ae0`, arrival `0x10057bd0`), `docs/re/transport.md` §1.3;
- main vtable `0x100c4468`: `+0x30` run `0x1005b6c0` (shared with the cart's job), `+0x34` **arrival** `0x1005c260`, `+0x38` abort `0x1005b740`, `+0x4c` "next step after a failed start" `0x1005e280`, `+0x08` destructor `0x100b4a30` (only frees memory).

**Job arrival `0x1005c260(figure)`** (the harvest/repeat lead of the packet; D, whole function read):
1. If `figure.slot+0x1c()` (the home) is null: job abort (`+0x38` → current step's abort), current = none, return. **A figure whose home was destroyed therefore aborts at its next arrival event.**
2. If there is no current step: current = head (step A); return.
3. Otherwise run the current step's arrival; then by the step's type:
   - type 7 (A): `step.out(1, &v)` → `next.in(1, &v, 0)` (the claimed tile (u16 x, u16 y) at step `+4`/`+6` moves to step B); current = next;
   - type `0xb` (B): `cargo = figure.slot+0x84(0)`, `capacity = figure.slot+0x78().slot+0x84()` (type `+0xf2`), `amount = cargo.slot+8()` (u16 at cargo `+6`); **if `capacity > amount` the job restarts at step A** (`current = &list head node +0x10`), else goes to the next step (C);
   - otherwise: current = next.
4. The job run `0x1005b6c0`: start the current step; if the start answers 0, `+0x4c` (`0x1005e280`) names the next step: **if the failed step is type 7 (A), step C; otherwise none** (job ends).

Cargo object (figure `+0x6c`, vtable `0x100c4ae4`, D): byte goods at `+4`, u16 amount at `+6`, slot `+0` set goods, `+4` goods, `+8` amount, **`+0xc` amount `+=` x** (u16 add), `+0x14` flush.

### 2.5 Step A start `0x10058ae0` (find a tree, claim it, walk)
1. `home = figure.slot+0x1c()`. Footprint `(x, y, w, h)` from `home.slot+0x78(&rect)`; radius `r` = the home type's byte `+0x65`.
2. Search square: `x0 = x + (w−1)/2 − r`, `y0 = y + (h−1)/2 − r` as **u16 words**, width `2r + 1 + ((w−1) mod 2)`, height `2r + 1 + ((h−1) mod 2)` (identical to the market cart's, `docs/re/transport.md` §1.1). 801 at `(x, y)`: `(x−4, y−4, 11, 11)`. **A value below 0 wraps to ≥ 0xFFFF…**: for a building with `x + (w−1)/2 < r` the square starts beyond the island and nothing is found (E-checked: `tools/emu_forester.py` "wrapped rect.x").
3. `cargo = figure.slot+0x84(0)`; `island = home.slot+0x18().slot+0x18()`; **`n = island.slot+0x10c(&square, mode 0, cargo.goods, &buffer)`** (§2.6). `n = 0` → step A answers 0.
4. The figure's searcher (figure slot `+0xc8(0)`, `docs/re/transport.md` §3): `searcher.slot+0x78(buffer, n)` registers the `n` (u16 x, u16 y) pairs as **tile goals** (`0x10002e20` appends to the engine's tile-goal list at `+0x1800`, count at `+0x17fc`), `searcher.slot+0x7c(callback 0x10058a40, &ctx)`, run through figure slot `+0xcc(0, 4, searcher)`. Walking layer = the figure type's byte `+0xff` (**2** for 102); the search is confined to the disc of radius `r` around the home's footprint centre (`docs/re/transport.md` §3, §3.4).
5. **Callback `0x10058a40(searcher, node)`**: node position → tile `(x, y)` (`__ftol` of the floats); `island.slot+0xc8(x, y, 0)` (§2.7) non-zero → go on (answer 1); zero → `ctx.x, ctx.y` = the tile, `searcher.slot+0x90(node, 4)` makes it the best node, and since `ctx.best` is 0 and the node's cost is ≥ 0 the search is stopped (`+0xb0`) and the callback answers 0. **So the first goal node popped (lowest u16 score, FIFO among equals, `docs/sim/state.md` §5.9 clauses 5, 7) that is not claimed wins.**
6. If the walk exists: step `+4`/`+6` = the tile; **`island.slot+0xc0(x, y, 0, 1)` claims it** (§2.7); the step answers 1. No path: answers 0 and the claim is not set.

### 2.6 Island slot `+0x10c` mode 0: the candidate list (`0x10092320`, E-checked)
Arguments `(rect*, mode, goods, out)`, `ret 0x10`. The rectangle is clipped to the island rectangle (`[island+4]`, `+6` origin; `+8`, `+0xa` size; unsigned compares, so a wrapped `x0` yields nothing). Mode 0 scans **rows from the top (y outer), columns from the left (x inner)** over `records + (row · width + col) · 8` (`[island+0xc]`); for each tile with object id `i`:
- the type `T = registry.slot+0x70(i)` (registry `[island+0x1c]`) exists (id 0 → none),
- **`T.slot+0xac(0)` (the type's `WARE[0]`) equals `goods`**,
- and **bit 29 (`0x20000000`) of the record's second dword is clear** (`test [rec+4], 0x20000000`).
It appends `(island.x0 + col, island.y0 + row)` as two u16 and returns the count. There is no owner/city test, no phase test and no distance test: young trees are excluded only because their type's `WARE[0]` is `UNUSED`. The original buffer is the caller's 4096-byte stack area (1024 pairs); a 11 × 11 square holds at most 121 tiles, so it never overflows for 801.
Modes 1 (random sampling through `__ftol`, `0x100925a8`–`0x10092750`) and 2 (chunk object lists, `0x10092472`–`0x100925a3`) exist and are not used by step A (it pushes mode 0, `0x10058bbb`).

### 2.7 The claim: island slots `+0xc8` / `+0xc0` (modes 0 only; E-checked)
`0x10087400` (read) mode 0 returns bit 29 of the record's second dword of tile `(x, y)`; `0x10087880` (write) mode 0 sets or clears exactly that bit (`or`/`and 0xdfffffff`), nothing else. The sim's owner nibble is bits 25–28 of the same dword (`docs/sim/state.md` §9), so bit 29 is the next bit up.

### 2.8 Step B: the cut (start `0x100553e0`, arrival `0x10054530`)
- **Start**: `duration = type.slot+0x64(1)` (LOGIC f32 `[4]`, **13.0**, replaced by 1.0 when not above 0) → figure slot `+0xe0(duration, action 7, t, 0)` = `0x10035260`: remaining = duration, rate 0.01 (`0x3c23d70a`), no movement; the market cart's load uses the same call with action `0x13`. Duration in ticks: `13.0 · 500` = **6 500** (inherited conversion, not re-shown here; the unit is the one already used for the cart's 1.0 = 500, `docs/re/transport.md` §1.2, OQ-162).
- **Arrival** (the wait is over), in this order: the figure's slot-`+0x18` object (`0x10035c00` returns `[fig+0x18]`; by use the world) → `island = obj.slot+0x8c(x, y)` (the island covering the tile; U-9); **`island.slot+0xc0(x, y, 0, 0)` clears the claim**; **`island.slot+0x110(x, y)`** (§2.9); if it returns 0 the arrival answers 0 and the cargo is untouched; otherwise **`cargo.slot+0xc(0x100)`: the cargo gains 256** and the arrival answers 1.
- **Abort** `0x100545c0` (shared by A and B): clears the claim at the step's own `(x, y)`.

### 2.9 Island slot `+0x110` = `0x10087340`: the cut itself (E-checked)
```
cut(x, y):
    rec = tile(x, y);  id = rec.u16
    if id == 0: return 0
    T = registry.slot+0x70(id - 1)               # the PREDECESSOR id
    if T is null or T.slot+0x5c() != 0xe2: return 0     # PRODTYPE 226 = ROHSTWACHS
    rec.u16 = T.slot+0x40()                      # the type's u16 id ([T+4]) = id - 1
    rec.byte3 &= 0x80                            # growth counter and parity cleared, bit 7 kept
    island.slot+0x188(pos, 0xcf, 0)              # redraw notification
    return 1
```
It does **not** re-check the tile's `WARE`, the goods, ownership or the claim. A mature tree `N` with `N − 1` of production type `ROHSTWACHS` becomes tree `N − 1` (the young stage, e.g. 105 → 104) with phase 0 (bit 7 preserved), and the growth sweep (`docs/re/production.md` §4.3) ripens it again after `S · 2^k` = 20 flips of `P/2` = 12 000 ticks. Planting is *this* write; there is no separate planting action on the forester path (U-6 for any other planter).

### 2.10 Step C and the raw receiver
Step C (`0x10057ae0` / `0x10057bd0`, `docs/re/transport.md` §1.3): if the figure stands inside the home's footprint it needs no walk; otherwise a search to the home's entrance points. Arrival: with a home and cargo goods ≠ 0, `home.slot+0xc0(goods, cargo amount)`. For the farm family that slot is **`0x1006ee70`: `[+0x1c] += amount` as a u16, no clip, no goods test**. The job then has no step left; the figure takes the end path `0x1003af00` (release of the home counter through `0x1003b6e0`, then Remove, `docs/re/annowelt.md`).

## 2.11 Tile goals and trees on walking layer 2 (U-1 closed at tile level; `tools/walk_tree_check.py`)
**Tile-goal acceptance (D).** Tile goals are the (u16 x, u16 y) pairs of §2.5 step 4, appended by engine `0x10002e20` to the list at `+0x1800` (count `+0x17fc`). The search pre-loop (`0x10004165`–`0x1000433b`) turns each into a grid cell and sets **goal bit 10** (`or byte [cell+3], 4`, `0x10004256`). When such a node is popped, `0x10003220` builds the goal sub-cell mask for the node's tile: for every tile goal equal to that tile (`0x10003346`–`0x100033e7`) `mask |= 0x1ff - island.slot+0x12c(mode, x, y)`, with `mode` = `[engine+0x2040]` = the third argument of the set-up `0x10002b80` (0 for figures, `docs/re/transport.md` §3); island `0x100921b0` for mode not 3/6 returns `(record.dword4 >> 16) & 0x1ff`, the tile's **blocked** 9-bit mask (`0x10092262`). So the goal sub-cells of a tile goal are the tile's **free** sub-cells; the callback runs per goal sub-cell in the arrival-ordered table (state.md §5.9 clause 5). A tile with at least one free sub-cell is an acceptable goal; the callback `0x10058a40` reads only the tile.

**Codes and masks of the BAUM trees (P + D rule, tables from `AnnoWelt.dll`).** Walk code = rule of `0x100091f0` from `HEAD[0x2f], HEAD[0x30]`; mask = `WALKMASK` sample (1 x 1 footprint: the three u32 rows, 3 bits each, row-major, bit 4 = centre); layer block = `0x100d1140 + 2 * 0x1c00`; `tools/walk_tree_check.py` over all 46 mature ids and their `id - 1`:

| code | blocked mask9 | mapper (`0x100c1af8`) | ids (mature and young) |
|---|---|---|---|
| 2 | 0x000 | 0x0fff | 16: 201, 203 … 227, 228, 256 |
| 2 | 0x010 (centre) | 0x0fff | 68: 104–112, 114–119, 202, 204 … 226 (even), 229–238, 240, 241, 243, 246, 247, 250, 251, 254, 255, 257, 404–423 |
| 2 | 0x012 | 0x0fff | 245 |
| 2 | 0x0ba (large trees) | 0x0b6d | 113, 242, 244, 258 |
| 2 | 0x0be | - | 260 (young predecessor of 261, not a candidate) |
| 4 | 0x000 | 0x0fff | 259, 261 |

Layer 2: `L[2] = L[4] = 0xff` (admission, all four directions), step costs `C_0..C_7 = 30, 41, 30, 41, 30, 41, 30, 41` for both codes, i.e. the same as empty land (code 2, mask 0). For every code/mask in the table the entry test of clause 4 (`mapper & geom` and `L[code] & key`) passes from all four neighbours (the script prints it). **Consequence (D from the DLL tables, using the repo's decoded entry rule):** a mature or young tree tile is enterable on layer 2 exactly like open ground and its free sub-cells are the goal; the large trees (113 …) block a cross of five sub-cells and keep the four corner sub-cells free (free mask 0x145). So a worker can path to a tree tile whenever an adjacent cell is open. Residual **U-1b** (not decoded anywhere in the repo): the finer sub-cell route validation (`0x100054ef`-`0x100055d5`, `0x10004c90`, state.md §5.9 clause 6) can veto or retry a route after the tile-level search found a goal. **It can change ACCEPTANCE and SELECTION, not only timing:** a goal the tile-level walk reaches may be refused by the omitted stage, in which case the original would continue with the next goal (a different tree, or none and a failed step A), and the claim in step A is set only after the walk exists (§2.5 step 6). A rebuild that reuses the existing deterministic tile-level walker for the first slice inherits this gap: `complete = true` from that walker proves only that the tile-level search found a route, not that the omitted stage would have passed. The gap is recorded as an inherited limitation, not closed here; no new fine-pathfinding is part of E1. The trees' codes are 2 (not 4), so they are not building cells.

## 2.12 Registry keying and the `id - 1` check (U-10 closed)
The Anlagen type loader is **`AnnoGame 0x10001d10`** (chunk names `0x1002416c`, `"ANLAGEN"` `0x10001d72`, `"ENTRY"` `0x10001db6`; found by the second reviewer). Per ENTRY it reads the 8-byte header (flag, id) (`0x10001dee`), looks the id up with game slot `+0x70` (`0x10001e03`) and **creates an object of kind `0x700` only if the lookup is null** (`0x10001e15`), then `obj.slot+0x28(game)` (`0x10001e2e`), `game.slot+0x24(obj, id)` (`0x10001e41`; `0x10003260` stores `[game + id*4 + 0x214] = obj` for kind `0x700` and returns `id`) and `obj.slot+0x3c(id)` (`0x10001e49`; `0x1000a670`: `[obj+4] = id`). Game slot `+0x70` (`0x100037e0`) reads `[game + id*4 + 0x214]`; type slot `+0x40` (`0x1000a660`) returns `[obj+4]`. So **the registry is keyed by the ENTRY id and `[T+4]` equals the key** (D). No duplicate ids exist among the 921 entries (P, reviewer: no ADDON/CLASSIC collisions). `AnnoGame 0x1000b6e0` (earlier cited here by mistake) is the **`HAEUSER`** sub-record loader, not the type loader. `N - 1` is the previous ENTRY id. P (`tools/walk_tree_check.py`): all 46 mature ids have an entry `id - 1`; for 45 its PRODTYPE is `ROHSTWACHS` (the `0xe2` test passes); the exception is **261** (`260` is `ROHSTOFF`); 259's `258` is `ROHSTWACHS` with `S = 1` (a stump). `0xe2` = ROHSTWACHS: name function `0x100138e0` maps codes `0xe1..0x100` through the table at `0x1002444c + code*4`; code `0xe2` is `0x100247d4`, the pointer to "ROHSTWACHS" (reviewer, D).

## 2.13 Spawn, first job run, end and the home counter (D; E1-FINAL, bounded to the forester's common path)
Read in this packet (`tools/disasm.py`, bounded): `0x10072d20`-`0x10072ded`, `0x100334f0`, `0x10034380`, `0x10034760`/`0x10034790`+ (via `transport.md` §4.3), `0x10035ce0`, `0x1003b7c0`, `0x1003af00`, `0x10034940`, `0x1003b6e0`, `0x1003b730`, `0x1003b780`, `0x1006f070`, `0x10074540`, `0x100349c0`, `0x100343c0` (to the arm that calls `+0x110`), `0x1005b740`, `0x100545c0`, `0x1006ed78`-`0x1006ee5f`, and the first lines of `0x1006ea90`. Figure class vtable `0x100c4950` (read with a pefile dword reader): slot `+0x28` = `0x1003b730`, `+0x74` = `0x10034760`, `+0x94` = `0x100349c0`, `+0xa4` = `0x1003b0e0` (job factory), `+0xac` = `0x100334f0`, `+0x110` = `0x1003b6e0`, `+0x168` = `0x10034940`, `+0x174` = `0x1003af00`, `+0x178` = `0x10034380`, `+0x17c` = `0x100343c0`. Farm class vtables (`0x100c2bf0`, `0x100c8ae0`, `0x100c8c0c`; found by `xref.py --reloc` on `0x1006f070`): slot `+0x28` = `0x10074540`, `+0x2c` = `0x1006f070`, `+0xc0` = `0x1006ee70`, `+0x118` = `0x10072d20`.

1. **The send creates and attaches; it does not run the job (D).** `0x10072d20`: refuse on flag bits 4/5 (`0x10072d2d`-`0x10072d36`); entrance prerequisite `0x10072b50`; registry create with slot `-1` (`0x10072d87`..`0x10072d91`), result `< 0` -> return the null pointer (`0x10072de5`, nothing attached, no counter change); on success: **building slot `+0x28(figure)`** (`0x10072da0`) = `0x10074540`: for the farm class (`0x1500`) the figure pointer goes into the first free of the five worker slots `+0x24`..`+0x34` (`0x1007458c`; all five full answers -1) - **this is the counter raise**; figure slot `+0x28(building)` (`0x1003b730`: home pointer `+0x78`, footprint box `+0x64`..`+0x69`); figure slot `+0x28(city)`; `job = figure slot +0xa4(job id)` (`0x10072dce`, factory `0x1003b0e0`); if non-null, **figure slot `+0xac(job)` = `0x100334f0` is `mov [ecx+0x20], eax; ret 4`: it stores the job and nothing else** (`0x10072dde`-`0x10072de5`). No search, no claim and no step starts during the send.
2. **Constructor (D).** `0x1003b7c0` -> `0x10035ce0`: remaining `+0x3c` = `0x3a83126f` (0.001f), rate `+0x40` = 0.001f, route `+0x50` = 0, job `+0x20` = 0 (until step 1 stores it), flags `+6` = 8, stamp `+8` = 0 until the registry create sets it (`annowelt.md`; the rebuild sets `stamp = t` of the send, `Scheduler::merge`).
3. **First job run is the figure's first tick with elapsed time (D reading, `transport.md` §4.3 steps 1-9).** `time = u16(t - stamp) * 0.2f`; `time == 0` returns without writing the stamp (same advance as the send: nothing happens). With `time >= 1.0` (5 ticks) the 0.001/0.001 micro-step is used up, remaining reaches 0 and the dispatch finds no flag, no route and a job: figure slot `+0x178` = `0x10034380` runs the job arrival (`+0x34` = `0x1005c260`: no current step -> current = step A) and then the run (`+0x30` = `0x1005b6c0`: step A start `0x10058ae0`). **The first search and the first claim therefore happen in phase B of the registry visit that first has >= 5 elapsed ticks (in practice the next advance, 50 ticks), not in the farm tick that sent the figure, and are ordered among the other figures by registry order.** If the run answers 0 the job is deleted (`0x10034397`-`0x100343a6`).
4. **The end dispatch releases the home counter at once (D).** With no flag, no route and no job the tick calls figure slot `+0x174` = `0x1003af00`, which in this order: `slot +0x168(1)` = `0x10034940` (if a job object still exists: job abort `+0x38` -> `0x1005b740` -> the current step's abort, then the job is deleted; none exists on the normal path); computes whether the figure's tile is inside the home box (`+0x64`..`+0x69`) and sets remaining `+0x3c` = 0.0 inside, **15.0 outside** (`0x100c1128`, `0x100c221c`; the tile scale `[0x100e0f58]` is a runtime-initialised data cell, 0.0 in the file image: U-14); rate `+0x40` = 0.01, flags `(f & ~1) | 0x202`; then **slot `+0x110` = `0x1003b6e0` immediately** (`0x1003af96`): own list cleared, job deleted if any, and `home slot +0x2c(figure)` = `0x1006f070` (matches the pointer among the five worker slots and writes 0, `0x1006f0ae`), `[figure+0x78]` = 0. So **the release happens at the end dispatch, before the (up to 15.0 long) last walk**, which only delays the Remove / stamp freeze (`annowelt.md`). A second release finds neither home nor pointer and does nothing.
5. **Same visit or next visit.** After the job is deleted the tick's inner loop re-enters with the leftover time (`transport.md` §4.3 step 9) and the end dispatch runs in the same visit when that leftover is non-zero; with exactly 0 it runs at the figure's next visit. The rebuild's transport body ends the figure in the same call (`Done: end_figure(fig, t)`); that is the inherited adaptation, and the difference is at most one visit.
6. **Effect on future sends and on the PRNG.** The farm counts the non-null worker slots (`0x1006ed8b`-`0x1006edac`), so a worker blocks a further send exactly from its attach (farm tick of the send) until its end dispatch (or the other releases below). Because every eligible attempt draws (§2.1), the tick at which a counter is released decides whether the next 500-tick farm step is an eligible attempt, and so how many world draws are made: the first-job and release timing are PRNG-timeline facts, not display details. For a treeless forester: send at step n (counter 1), first visit >= 5 ticks later: step A fails (no goals), step C needs no walk inside the footprint, delivery of 0, job end, release - all inside one or two registry visits, long before step n+1 (500 ticks), so each step is an eligible attempt (one draw) and the counter is back to its previous value at the next step.
7. **Other ways a forester worker ends (bounded; not a combat or figure-lifecycle campaign).**
   - *Hit points to 0* (`0x100349c0`, figure slot `+0x94`): calls `slot +0x168(1)` first (`0x100349f5`), then sets the end state and calls `+0x110` (`0x10034a82`). **The job abort runs, so a claim the job holds at that moment (the tree tile during the walk to the tree or the wait, the `0x100545c0` abort; not during the walk home, step C, where the claim was already released at the cut) is cleared, then the counter is released.** This corrects the E1 statement that a killed worker leaks its claim.
   - *Route-terminal arm of the route step* (`0x100343c0`, node kind `[node+0x1c] & 0xf` other than 0 and 3, tail at `0x10034504`-`0x1003457c`): sets the end flag and calls `+0x110` without `+0x168`; `0x1003b6e0` deletes the job object through its destructor (a free, no abort). A claim held by such a worker would stay set. Whether the route builder (`0x10034df0`) produces these node kinds for the forester's tile search is **U-5b**; this note does not claim it happens.
   - *Home destroyed* (farm destructor `0x1006f0c0`): detaches; the figure's next arrival aborts (§2.4 step 1).
   - *Abort is unconditional (D):* `0x100545c0` clears the claim bit at the step's stored tile (u16 x, u16 y) with `island slot +0xc0(x, y, 0, 0)`; step B's arrival (`0x10054530`) clears the bit before the cut. Neither tests who set the bit; the original stores one bit and no owner.

## 3. Units and the conversion (D + P, hand arithmetic in `docs-sim-forestry.md` §5)
| Quantity | Value | Unit |
|---|---|---|
| tree cut | `push 0x100` | 256 = 1 t of `BAUM` (8.8) per harvest |
| worker capacity | 256 | one harvest fills it, so no repeat |
| farm raw stock `+0x1c` | u16, 8.8 | tons of `BAUM` |
| consumption per full step | 32 | 0.125 t of `BAUM` |
| output per full step | 14 | 0.0547 t of `HOLZ` |
| one trip | 256 raw = 8 full steps | **112 = 0.4375 t of `HOLZ`** |
| nominal ratio | `I0` = 2.25 t BAUM per t HOLZ | actual 32/14 = 2.2857 (the rounding of `rate0`) |
| step period | 500 ticks (1 s at normal speed) | |

Wood is **not** conserved across the conversion: 1 t of `BAUM` yields 0.4375 t of `HOLZ`. The raw stock is not a warehouse good (goods 70 > 0x37), it never leaves the farm and is not visible to the market.

## 4. Behaviour of the lifecycle (D)

### 4.1 Search order and bounds
Square `(x−4, y−4, 11, 11)` around a 3 × 3 forester, listed row-major, clipped to the island; the walk search is confined to the disc of radius 5 around the footprint centre (corner tiles of the square outside the disc are listed as goals but cannot be entered); the *winner* is the first unclaimed goal tile the search pops (cheapest route first, FIFO on ties), not the nearest by straight line.

### 4.2 Duration and repeat
Per trip: (send in the farm tick) + first visit (>= 5 ticks, §2.13) + walk out + 6 500 ticks + walk home. After the delivery the job ends and the end dispatch releases the home counter at once (§2.13 item 4); the farm's next step (>= 500 ticks later) is an eligible attempt whenever `workers_out < 2` and `raw/32 <= 27`. With no eligible tree, or no path, step A fails and the job goes straight to step C: the worker (standing inside the footprint) delivers 0 and ends within its first figure visits, so a treeless forester sends, spawns and discards a worker about once per step (D; the cost is a search per step and one world draw per step).

### 4.3 Cancellation and removal
- **Farm switched off / suspended:** the tick returns before the dispatch (`0x1006eac0`, `0x1006eaf1`): no send and no draw; a worker already out completes its trip (no job step looks at the farm).
- **Output full (status 2) does NOT stop an eligible dispatch of the two-worker forester** (`max_figures` 2: exit (1) of §2.1 is never taken; `forester_vectors.txt` `step 64 1280 0 -> 64 1280 2 1`, `draws ... 64,1280,0 -> 3`). It stops sends only when the rate or count test fails (`raw/32 > 27`, or two workers out). The E1 draft sentence "only new sends stop" for a full store is withdrawn: a full store stops production, not the dispatch.
- **Home destroyed while the worker is out:** the farm destructor detaches the figure (`docs/re/annowelt.md`, `0x1006f0c0`); at the worker's next arrival event (end of its current walk or wait) the job aborts (§2.4 step 1): the claim at the step's tile is cleared **unconditionally** (§2.13 item 7), **no tree is cut**, the cargo (and a tree's worth of waiting) is lost.
- **Target changes during the walk:** nothing re-checks the tile until B's arrival (after the 13.0 wait). At B's arrival the claim is cleared (unconditionally) and then the cut runs; a tile that no longer cuts (`cut` answers 0) leaves the cargo empty, so the job restarts at step A (`capacity 256 > amount 0`) and searches again.
- **Placement over the tile:** placement clears second-dword bits 29-31 (`0x100824a7`): the worker keeps walking to a tile whose claim is gone; another worker may then select and claim it; the first worker's arrival or abort clears the bit regardless of who set it (the cross-worker clear is a consequence of D facts, reachable in principle; no original run of it exists: NOT_RUN).
- **Figure killed by hit points:** the job is aborted first (`0x100349f5` -> `0x10034940` -> `0x1005b740` -> `0x100545c0`) and then released (`0x10034a82`): the claim is cleared. **Other end paths are not all read:** the route-terminal arm of `0x100343c0` ends a figure without the abort (§2.13 item 7); the remaining indirect `+0x110`/`+0x174` callers (about 30 sites, many on other classes) are unread. This note therefore asserts no general leak of the original; it asserts exactly the traced transitions.

### 4.4 Contention
The claim is a per-tile bit shared by every farm on the island. Worker 1's step A claims its tile only after its search succeeded; a later search excludes it. Two workers of the same farm cannot choose the same tree while the bit stays set: figures are visited one at a time (registry order) and each write is immediate in the original, so the second search of a pass sees the first claim. The bit is released at the cut arrival, at an abort, by placement (bits 29-31), or - for the unread paths of §4.3 - possibly never. Because the bit has no owner, a claim dropped by placement can be re-taken by a second worker and then cleared by the first worker's late arrival/abort (§4.3); that is a property of the traced code, not a measured behaviour (NOT_RUN-1).

### 4.5 Hazards found in the data (P + D)
- **Tree 261** (7 items in the island corpus): `cut` always answers 0, B restarts A, and the nearest unclaimed such tile is chosen again, so a worker can loop between "walk, wait 13.0, fail" on it forever while it is the first goal popped.
- **Tree 259** (10 items): cut → 258 with `S = 1`, no growth period: a permanent stump that is no longer a candidate.
- **`GRAS`/field objects:** a tile of another ripe resource whose predecessor is `ROHSTWACHS` is cut the same way if it were chosen as a goal; the candidate filter prevents that for goods ≠ `BAUM`.

## 5. Emulation (E): `tools/emu_forester.py`
Runs the real bytes of `0x10087340`, `0x10087400`, `0x10087880` and `0x10092320` (mode 0) on a synthetic island (8 × 8 tiles at (100, 200)), with unicorn and five tiny stub methods for the registry/type/island-notification objects (`reg70`, `prod5c`, `id40`, `ware_ac`, `ret0c`): harvest of 105 (phase `0x45`/`0xC5` → 104 with `0x00`/`0x80`), 259 → 258, 261 unchanged and 0 returned, id 0 and young id 104 unchanged, 713 → 712 (no goods test), the claim bit set/cleared with the other 31 bits preserved, candidate order and filters (claimed, young, other goods, outside the rectangle, clipped rectangle, wrapped `x0`). All 13 checks pass (log in `research-log.md`). These are hand-written expectations; the stubs mirror the documented getters (`+0x5c` = `[T+0x10]`, `+0x40` = `[T+4]`, `+0xac(i)` = `[T+0x206+i]`); that `[T+4]` is the registry key = the ENTRY id is settled in section 2.12 (loader `0x10001d10`; the earlier citations `0x10009bdd`… and `0x1000b6e0` were wrong and are withdrawn). "105 becomes 104" is therefore D/E with id = entry id.

## 6. Corrections owed to older notes
- `docs/re/production.md` §5.1 calls step types 7/`0xb` "harvest"/"repeat": 7 is find-and-walk (A), `0xb` the cut with the capacity-restart rule. `docs/re/transport.md` §1.6: `0x10072b50` only needs the type to have entrance points (not walkability). The step-A callback takes (searcher, node, ctx).
- `docs/re/transport.md` §3.4.17 names `0x10087880` as the candidate writer of bit 29: confirmed (claim bit); placement clears bits 29–31 (see the sim contract).
- `docs/re/production.md` §4.2 / §5.1 / Open: the farm worker's job is **`0x2a`** (route type `MAEHER`); the harvest amount is **256 per tile object**; a cut produces `id − 1`. `docs/sim/state.md` §4.9 (`Figure::job`: "farm-family workers 0 = unknown (O-8)") and `src/sim/src/driver.cpp` `send_figure_job` (`default: return 0`) are superseded for the `MAEHER` route.
- `docs/re/production.md` §4.3 "harvest amount not traced": closed here (§2.8, §2.9).
- `docs/re/annowelt.md` (End of a figure): add that slot `+0x174` = `0x1003af00` first calls slot `+0x168(1)` = `0x10034940` (job abort + delete when a job exists), that the home release is inside the same call before the last walk, and that the hit-point path `0x100349c0` also calls `+0x168(1)` before `+0x110`. `docs/re/transport.md` §1.x: the figure's send (`0x10072d20`) stores the job through `0x100334f0` and starts nothing; the first job run is the figure's first tick with >= 5 elapsed ticks (§2.13 here).
- E1 draft statements withdrawn by E1-FINAL: "a killed figure leaks its claim" (the hit-point path aborts the job); "output full stops sends" (it does not for the two-worker forester); "one draw per dispatch that passes the rate test" is now stated as one draw per eligible attempt (§2.1) including the count test.

## 7. Unknowns (U) and not-run items

| # | Question | Needed for | Next address / action |
|---|---|---|---|
| U-1 | **Closed at tile level** (section 2.11): tile goal = the tile's free sub-cells; trees are code 2 (259/261 code 4), enterable on layer 2 like open ground. Residual U-1b: the finer sub-cell route validation (clause 6) is undecoded for every figure | route veto only | `0x100054ef`–`0x100055d5`, `0x10004c90` |
| U-2 | **Closed (D, §2.13)**: the send stores the job only (`0x100334f0`); the first search/claim run at the first registry visit with >= 5 elapsed ticks (D reading of `0x10034760`). Residual: the stamp value written by the registry create (`0x10097250`, argument pushed at `0x10072d7e`) was taken from `annowelt.md`, not re-read | first-wood timing, PRNG | `0x10097250` (stamp argument) |
| U-3 | **Closed for the release (D, §2.13 item 4)**: the counter is released inside `0x1003af00` before the last walk. Residual: the run-time value of the tile scale `[0x100e0f58]` used by the inside-footprint test (0.0 in the file image), which only decides 0.0 versus 15.0 for the Remove delay | Remove/stamp delay only | `0x100e0f58` writers |
| U-4 | **Closed**: placement clears second-dword bits 29-31 (`mov ebp,0x3fffffff` `0x10082442`, `and eax,ebp` `0x100824a7`); the clear arm preserves them (`docs/re/transport.md` §3.4.17) | | |
| U-5 | **Narrowed (D)**: hit points to 0 aborts the job (claim cleared) and releases. **U-5b**: the route-terminal arm of `0x100343c0` (node kinds other than 0 and 3) ends without the abort; whether the forester's tile-goal route can end in such a node, and the other ~30 indirect `+0x110`/`+0x174` callers, are unread | exact set of claim-leaving paths | `0x10034df0` (route builder node kinds), callers of `+0x110` |
| U-6 | A planter anywhere else (does anything plant `ROHSTWACHS` besides the island loader and the cut?) | sustainable-economy scope (not E1) | search writers of island slot `+0x94` object ids with a `ROHSTWACHS` type; `Wachstum.dat` consumers (`0x10084370`) |
| U-10 | **Closed** (section 2.12): loader `0x10001d10`, `0x10003260`, `0x1000a670` | | |
| U-11 | Route invalidated mid-walk with a claim held (the walker's stop/re-route handling `0x10034df0`, `0x100343c0` kinds 0/3): not traced | claim leak | `0x10034df0`, `0x100343c0` |
| U-9 | What the figure's `[+0x18]` object and its slot `+0x8c(x, y)` are (the island lookup by tile) | none for single-island cases | `0x10035c00` callers, slot `+0x8c` of that object |
| U-7 | LOGIC f32 `[0]` (5.0), `[8]`, `[0xc]` (4.0) of figure 102 (type `+0x34`, `+0x38`, `+0x3c`) | none known on this path | callers of type slots `+0x64(0)`, `+0x64(2)`, `+0x64(3)` |
| U-12 | What the cut does to a tile whose object was replaced by placement between selection and arrival (the id is not re-checked; `id - 1` must be `ROHSTWACHS`) | enhanced revalidation scope | island `0x10087340` callers' contexts |
| U-13 | Whether the fur trapper's own tick (`fur_trapper_step`, vtable `0x100c8c0c`) has the same world draw | fur trapper PRNG | its tick, `0x1006f0c0` neighbourhood |
| U-14 | Run-time value of `[0x100e0f58]` | 0.0 vs 15.0 Remove delay | writers of `0x100e0f58` |
| U-8 | The farm's status/blocked-flag side effects (`0x10072ed0`, `0x10072fb0`) for a starving forester | UI only | covered by the existing farm status code |
| NOT_RUN-1 | Original runtime: a forester in the running game delivers 1 t of BAUM per trip, waits 13 s, and the cut tile record goes `105 → 104`, phase 0, bit 29 set during the trip and clear after | confirmation of §2.8/§2.9 | `tools/orig/gamestate.py tile_record` before/after a worker's cut (the harness exists, `docs/re/transport.md` §3.4.17 "What one sample reads"); needs the Windows/Wine runner |
| NOT_RUN-2 | Original runtime: the worker count (2) and the first-wood time | timing canary | same harness, sample the farm's `+0x1c`/`+0x18` and workers |
