# The small farm supply contract (MAP-SUPPLY-1 S1, 2026-11-05)

Frozen before any engine change. Evidence marks: **D** read in the disassembly (module and address given), **P** read from an
original file, **ADAPT** a disclosed engine adaptation, **U** unread. Modules: `AnnoWelt.dll` sha256
`f220a878510f740c4cfb7224ade0a260e9a0fd7e99113e1201e7a37c0dfa6ae2`, `AnnoGame.dll` sha256
`ac721220afd27ebca5fd...` (hashes re-checked on this Mac from the main checkout's `game/app`). Independent reference:
`tools/small_farm_ref.py` with `fixtures/synthetic/sim/small_farm_vectors.txt` (47 vectors, hand-derived, contract test
`tools/small_farm_ref_contract_test.py`); the engine's existing kernel runs the `gate`, `line` and `tick` records
(`src/sim/tests/small_farm_vectors_test.cpp`, 32 records agree; `credit`, `pick` and `cut` belong to S2/S3). This document is the
contract the S2-S5 packages implement; no behaviour here is guessed.

## 1. The registry records (P, `Anlagen.dat`, `FigurenHeader.dat`)

| item | value |
|---|---|
| building 809 | `PRODTYPE UNIVERSAL` (9), `WARE ALKOHOL, NAHRUNG` (goods 29, 26), `ROHSTOFF KARTOFFEL, GRAS` (goods 68, 69; `docs/re/production.md` section 7 numbers the crops 56-72) |
| mapping | **verified from the files, not assumed:** line 0 = potatoes -> alcohol, line 1 = grass -> food (the WARE and ROHSTOFF records are read in the same order) |
| rates | output per step 13 and 4, input per step 128 and 50, capacity 1024 (4 t) per line, `max_figures(0)` = 3, figure list `106` (PROD3 u16 at 0x32), action radius `HEAD[0x11]` = 3 (search square 7 x 7 for the 3 x 3 footprint) |
| worker figure 106 | `ROUTETYPE MAEHER` = route 42 = job `0x2a`; `LOGIC` load wait f32 7.0 (3500 ticks), carry capacity byte 0x11 = 3 t (768), walking layer 2 |
| animal figures | 107, 109, 110 `ROUTETYPE WEIDETIER` = route 46 = job `0x2e`; wait 10.0 / 13.0 / 10.0 (5000 / 6500 / 5000 ticks), capacity 3 t, layer 4. 108 is a hunter (`JAEGER`), not in the table |
| tile objects, potatoes | 709 `ROHSTOFF`, `WARE[0] KARTOFFEL`; predecessor 708 is `ROHSTWACHS` (PRODTYPE 226). Growth: 5 stages, shift 2, P 15000 -> 20 flips of 7500 = **150 000 ticks** (registry, `docs/re/production.md` 4.3) |
| tile objects, grass | 2, 21, 23, 25, 27, 69, 71, 73, 79, 81, 83, 355 (`WARE[0] GRAS`); every predecessor (1, 20, 22, 24, 26, 68, 70, 72, 78, 80, 82, 354) is `ROHSTWACHS`. P 10200 (10250 for 82): **102 000 ticks** (354: 30 000) |
| three unit domains | the **warehouse** counts 8.8 tons (1 t = 256); the farm's raw and output stocks are the same 8.8 units but u16 (`+0x24/+0x26`, `+0x18/+0x1a`); the **figure cargo** is a u16 amount (`+6`) raised by 256 per cut. One cut = 1 t; one worker or animal trip = 3 cuts = 768 |

**Where the resources exist (P, 515 island and scenario files).** Grass objects: 1 279 352 placed objects in 445 files. Potato
objects (709 only; a further 218 young 708 are placed): 2 388 in **16** scenario files (the add-on `0201SingleAddOn/single31-34, 38-41`, the campaign
`S01_02/03/04/07/10/12`, `Single_24`, `Tutorial/02Trade01`; 18 files hold potatoes or a farm 809, `endless02` and `S01_08_01` only the farm); the files that place both a farm 809 and potatoes include `S01_07_01`
(11 farms, 115 + 90 young potatoes) and `single33` (16 farms, 478). In the Tutorial town of K0-K3 (island 3, player 2) two small
farms stand at (212,250) and (211,240). Counted in the farm's own search window (radius 3, footprint 3 x 3: the 7 x 7 square from `x-2` to `x+4`, `y-2` to `y+4`; one window for every count in this document) they have **28 and 11 ripe grass objects** (0 and 12 young) and **no potato object**; the neighbouring island 7 holds a farm at (261,344) with **32 potatoes** in the same window and others with at most 7 grass. (S1 first quoted 50 and 33 and 4 potatoes from an 11 x 11 window: that was wrong for this radius.) The search is further confined to the disc of radius `r` around the footprint centre (`docs/re/forestry.md` 2.5): a corner tile of the square can be a goal and still be unreachable, so the counts are upper bounds. S4 selects its scenario from the corrected numbers. **U (OQ-491):** nothing traced puts a potato object on an island
that has none; planting by the player or a building is not found (`docs/re/forestry.md` 2.9: the cut is the only write; a young
object grows back, but a missing crop never appears). The first real alcohol loop therefore needs a scenario that already
holds potato objects.

**What the numbers mean (derived, arithmetic only).** A full step of line 0 consumes 128 raw (0.5 t of potatoes) and makes 13
(0.05 t of alcohol); of line 1, 50 raw and 4. One t of alcohol therefore needs 10 t of potatoes (10 cuts), one 3 t Inn load needs
30 cuts. A cut tile is young again for 150 000 ticks (potatoes) or 102 000 (grass). The 32 potatoes near (261,344) are about one
load of alcohol per regrowth cycle: S4/S5 must claim finite production unless the regrowth is shown.

## 2. The trace

### 2.1 The tick (D, `AnnoWelt.dll 0x1006f660`, read instruction by instruction in this packet)
Gate `(u16)(t - [+0xa]) >= 0x1f4` (`0x1006f66c-0x1006f67a`), then `[+0xa] := [+0xa] + 0x1f4` as a **word** (`0x1006f683-0x1006f68a`):
one step per call however late, so a farm that is behind runs at every call until the stamp catches up. Both switches
(`+0x2f`, `+0x30`) clear: return after the stamp. Flags bits 4/5 (`(f >> 1 & 0x10) | (f & 0x10)`, `0x1006f69b-0x1006f6a9`):
both activities age (`a * 255 >> 8`) and return, whatever the switches. Line 0: age, line step `0x1006f440`; **worker due iff made0 == 0
and byte [+0x2a] < 1 and 384 / rate0 >= raw0 / need0** (`0x1006f739-0x1006f779`, signed `idiv`), then `this+0x118(job 0x2a,
type+0xb4(0,0), t)` and the new figure's cargo goods := `type+0xb0(0)` (input 0, potatoes; `0x1006f7a1-0x1006f7c2`). Line 1: age, line
step, then **one world draw** (`0x1006f810-0x1006f838`, `and eax, 7` into the u16 table `0x100c8d48` = 107 110 107 109 107 109 107 109), then
`limit[kind-106] <= counter[kind-106] - (made1 != 0)` returns (limits `0x100c8d59` = 1 2 0 1 1 0 0 32), `max_figures(0) <= sum of counters 1..4` returns,
`384 / rate1 >= raw1 / need1` sends `this+0x118(job 0x2e, world +0x6c(kind), t)` and sets the figure's cargo goods := `type+0xb0(1)` (input 1, grass;
`0x1006f8f7-0x1006f920`). Line step (`docs/re/production.md` 4.5): capacity first (it reads output 0's capacity, `call [type+0xc0]` with no line argument, for BOTH lines; 809 has 4 t on both, so the shipped data cannot tell), `q = (raw << 7) / need` capped at 128, below 64 starved,
`used = q * need >> 7`, `made = q * rate >> 7`. The existing kernel (`production.cpp small_farm_step`) agrees with all 32 `gate`, `line` and `tick` vectors.

### 2.2 Attach, counters, release (D)
Attach (`0x1006fdb0`, vtable `0x100c8d60` slot `+0x28`): a figure (class `0x1500`) raises byte `[+0x2a + (figure type id - 106)]` (the table byte `0x100c8d58` = 106) **first**, then takes the first
free of five pointer slots `+0x34..+0x44`; with all five full it returns -1 and the counter **stays raised** (`0x1006fdf4-0x1006fe16`, `docs/re/annowelt.md` side finding); any other class goes to the base attach (`0x100732d0`: city 0x700 / island 0x1400). Detach (`0x1006fe40`,
slot `+0x2c`) lowers the same byte and clears the slot. So counter 0 is the worker type 106 and counters 1..4 are the animal types 107..110; the engine's explicit
`home_counter` (0 for the worker, kind - 106 for an animal) is the same index on the shipped data. **ADAPT:** the original indexes by the figure type id, the engine by an explicit counter
(a mod with a worker type other than 106 would differ; it must say so in the type data, S3).

### 2.3 The shared job (D, `0x1003b0e0`; `docs/re/forestry.md` 2.4-2.10)
Job `0x2a` (and `0x29`) -> case `0x1003b255`, main object vtable (offset +0x00) `0x100c4468`, step A at `+0x34` with vtable `0x100c44c0`; job `0x2e` (and `0x2d`) -> case `0x1003b314`, main vtable `0x100c4340`, step A at `+0x34` with vtable `0x100c4398`.
**Both share** the run `0x1005b6c0`, the arrival `0x1005c260`, the abort `0x1005b740`, the failed-start rule `0x1005e280`, step B (type `0xb`: wait
`LOGIC f32[4]` then the cut, start `0x100553e0`, arrival `0x10054530`: clear the claim, `island +0x110` cut, cargo `+= 256` only when the cut changed the tile)
and step C (walk home, `home +0xc0(goods, amount)`). They differ in step A's start: worker `0x10058ae0`, animal `0x10059210`. The two functions are
identical instruction for instruction except one stack word: the search context field that the callback `0x10058a40` compares is **0 for the worker and
`0x14` (20) for the animal** (`0x10058bfd` vs `0x1005932d`). The callback: node position -> tile; a **claimed tile** (`island +0xc8`) answers 1 (go on); an
**unclaimed goal** stores its tile in the context, makes the node the best (`searcher +0x90(node, 4)`, unconditional), and then answers 1 only if
`limit > node score` (the u16 at `node+8`, slot `+0xa0` = `0x10003200`), else stops the search (`+0xb0`). Result: **the worker takes the first unclaimed goal; the
animal takes the first unclaimed goal whose score is 20 or more, else the last unclaimed goal reached.** Goals come from `island +0x10c` mode 0 (rows from the top, then
columns): tiles in the home's square whose type's `WARE[0]` equals the **figure's cargo goods** and whose record bit 29 is clear; the cargo goods is input 0
(potatoes) for the worker and input 1 (grass) for the animal, set by the farm tick. After a found walk `island +0xc0(x, y, 0, 1)` claims the tile. After the
cut, the job repeats step A while `capacity > amount` (`0x1005c260`), so a trip is 3 cuts of 3 different tiles. The credit at home is
**SmallFarm slot `+0xc0` = `0x1006f960`: `slot = (type+0xb0(1) == goods) ? 1 : 0; raw[slot] += amount` as a u16, no clip, no further goods test** (a goods that is not input 1 goes
to raw 0, the potato slot). The end path (`0x1003af00`, release of the counter through the detach above) is the forester's (`docs/re/forestry.md` 2.13).

### 2.4 The tile cut (D, `0x10087340`, `docs/re/forestry.md` 2.9)
`id -> id - 1` when the predecessor type is PRODTYPE 226 (`ROHSTWACHS`), `phase &= 0x80`; nothing else. The growth sweep (`docs/re/production.md` 4.3, island scan `0x1008b5f0`)
brings the young object back (`id + 1`) after `S * 2^k` parity flips of `P / 2` ticks: 150 000 (potatoes) and 102 000 (grass) ticks, with the documented lost-flip caveat only
below `P / 2 < 2000` (not the case here).

## 3. Audit of the engine against the trace

| link | original | engine | state |
|---|---|---|---|
| tick, gate, switches, suspension, line step | 2.1 | `small_farm_step` (production.cpp), 32 vectors agree | **implemented** |
| one draw, order, kind table, limits, sum limit | 2.1 | same; `SmallFarmAnimals` is data with the original defaults | **implemented** |
| spawn request, counter raise | attach 2.2 | `consume` (driver.cpp): job 0x2a / 0x2e, `home_counter` 0 / kind-106, release in scheduler | **implemented** (ADAPT 2.2) |
| cargo goods of the new figure (input 0 / input 1) | 2.1 | set only for the `Farm` class (`consume`, driver.cpp); none for a `SmallFarm` worker or animal | **incomplete** (S2/S3) |
| harvest state of the worker | job 0x2a state machine | `harvest_job(job) && Farm payload` only; `harvest_tick` clears a non-Farm home (harvest.cpp) | **incomplete** (S2) |
| tile candidates, claim, cut, growth | 2.3, 2.4 | `harvest.cpp`, the growth sweep, `harvest_cuttable`, reused as is | **implemented** for trees/grain; potatoes/grass untested |
| credit to the raw slot | `0x1006f960` | `StoreDelta{Input, 0}`: always raw 0 of a Farm | **incomplete** (S2: slot by goods, u16 add) |
| animal tile choice (limit 20, last/first >= 20) | 2.3 | the worker's first-goal rule only | **incomplete** (S3) |
| animal wait, capacity, walking layer | LOGIC | `FigureType` has `load_ticks`, `capacity_88`, `layer`; animal figures spawn with no job logic | **incomplete** (S3) |
| output pick-up by the market cart | docs/re/production 5.2 | `transport.cpp` knows SmallFarm outputs, `store_line` (`0x1006f9a0`), reserved lines | **implemented**, not run end to end (S4) |
| kind -> figure type (`world +0x6c(kind)`) | 2.1 | the engine uses the figure id equal to the kind | **U** (body of the slot unread; the shipped figures 107/109/110 exist with route 46) |
| node score units for the limit 20 | `node+8` | the walk search's score; whether it is the same quantity as the u16 at `node+8` is not shown | **U** (S3 must give a distinguishing case) |
| fine route / walk time | the finer route stage `0x100054ef..` unread | the disclosed walk adaptations of the forester and the house walkers | **ADAPT**, not original |
| first planting of potatoes | none found | none | **U** (OQ-491) |

## 4. Decisive predicates and their distinguishing vectors

| predicate | primary address | vector that separates it from its neighbour |
|---|---|---|
| gate is `>=` 500 on a 16-bit difference | `0x1006f675` | `G-499` / `G-500`; `G-stale` (65 736 -> 200 does not run) |
| the stamp moves by 500, not to `t` | `0x1006f683` | `G-behind` (1000 -> 500) |
| suspension ages both lines even if one is off | `0x1006f69b` | `T-suspended` |
| the worker needs `made0 == 0` | `0x1006f739` | `T-made` |
| the worker needs `counter0 < 1` | `0x1006f742` | `T-worker-out` |
| `384/rate0 >= raw0/need0` (integer divisions, `>=`) | `0x1006f777` | `T-full-3839` (29 sends) / `T-full-3840` (30 does not) |
| one draw per step with line 1 on, before the limits | `0x1006f810` | `T-kind-limit` (refused, draws 1), `T-fresh` |
| kind = table[draw & 7] | `0x1006f838` | `T-draw-9` (9 -> 1 -> 110), `T-draw-3`, `T-draw-2` |
| a producing step counts one animal fewer | `0x1006f85c` | `T-limit-made` vs `T-kind-limit` |
| sum of counters 1..4 against `max_figures(0)` | `0x1006f892` | `T-sum-limit` |
| `384/rate1 >= raw1/need1` | `0x1006f8c9` | `T-grass-4849` / `T-grass-4850` |
| credit slot is "goods == input 1", else slot 0 | `0x1006f96a-0x1006f989` | `C-potato`, `C-grass`, `C-other`, `C-wrap` (u16 add, no clip) |
| worker stops at the first unclaimed goal | `0x10058ab6` (`limit 0`) | `P-worker-first`, `P-worker-zero` |
| animal continues while `20 > score`, stops on the first unclaimed goal >= 20 | `0x10058ab6` with `0x1005932d` | `P-animal-far`, `P-animal-edge` (score 20 stops), `P-animal-claimed`, `P-animal-near` |
| the cut needs a `ROHSTWACHS` predecessor | `0x10087340` | `K-potato`, `K-grass`, `K-not-growing` |

## 5. Ownership and invariants S2/S3 must keep

1. A figure's home counter is raised once at creation and released exactly once at its end (the existing scheduler path); the counter is the figure's, never recomputed.
2. A claim (bit 29 of the tile's second dword) is owned by one figure from the found walk until its cut arrival or abort, and is cleared on every end path (the forester's corrected end path).
3. The credit goes to the **home's** raw slot by the rule of 2.3, to the home's city only (two owners with the same behaviour credit their own farm), as a u16 add; a home gone before the arrival loses the cargo as history (counted, not stock).
4. Output stock is the farm's own (`out`, `reserved`); only the market cart takes it, and a reservation is not extra goods.
5. A save taken at any boundary (walking, waiting, returning, ended) reloads and continues identically; no new state without an independent expected-bytes vector and a new identity: **128 / timeline 5 is published**, so new fields need a bump (stream 129).

## 6. Acceptance table

| package | accepts when |
|---|---|
| S2 | worker with a SmallFarm home: cargo goods input 0, harvest state attached, candidates = potato tiles, claim/cut/regrowth, credit by `C-*` vectors, end paths (home destroyed, ended during work/return, full stock, simultaneous claims), reload after each, a second goods/id/owner fixture, exact debit/credit and draws |
| S3 | animal figures 107/109/110 with input 1 cargo, the limit-20 choice (`P-animal-*` against the engine), the same cut/credit, per-kind counters and the sum limit, refused sends, line gating and output-full behaviour, removal with figures out, partial delays, reload mid-trip |
| S4 | a real scenario with both resources in reach (a campaign or add-on file of section 1, or island 7), farm outputs collected by the cart, Inn and food stall supply to at least three houses, ledger with every unexplained term zero, a producer-disabled comparison, finite or repeated production stated as measured |
| S5 | interruption and recovery by public commands, two owners, reload and fresh-process replay at crop work, grazing, production, cart and resident cargo, operation counts against unrelated farms |
| S1 reviewer | re-derive 2.1-2.4 from the named addresses, check that the vectors follow from the disassembly (not from the engine), and the potato/grass/GRAS/KARTOFFEL records |

## 7. Unknowns, isolated

- OQ-491: the first appearance of potato objects (no planter traced).
- U: `world +0x6c(kind)` body; node-score units; the fine-route stage; whether a modded worker type other than 106 changes the counter index.
- Nothing above blocks the crop line (worker) or the animal line from being implemented with the stated adaptations; the loop cannot be claimed original in walking time.

## 8. S2 delivered: the line-0 worker (2026-11-05)

**Change.** `consume` (driver.cpp): a `SmallFarm` worker is spawned with job `0x2a` (the literal, D `0x1006f78d`), cargo goods = input 0 and the
harvest state (docs/sim/forestry.md 2); `harvest.cpp` accepts a `SmallFarm` home and credits it with `input_slot_for(type, goods)` (driver.cpp: slot 1 when the
goods equal input goods 1, else slot 0; D `0x1006f960`); `World::harvest_problem` accepts the payload. Nothing else: the candidate search, the claim, the cut, the
growth, the end path and the counter release are the forester's, unchanged. No new field, no new draw site. No 809-only branch: nothing names 809, 106 or a goods id.

**Identity.** Because 128 / timeline 5 was published, the change of evolution of every world with a small farm is a new identity: `kStateHashVersion` **129**, timeline 5
(no draw semantics change). `tools/sim_stream.py` is the independent transcription: `g0/g1/s5/s6` goldens were written from it and the C++ prints agree
(`s6_g1_create a02546982dc950c9`, `s6_g1_step100 394e59171e7d58c2`; `empty_world 9970bb137ac3bfcf`); the other goldens, the recorded logs, `ship_holds`, `cargo_rules_bytes`, the stage
digest and `scenario_corpus.txt` were re-recorded; the saves of 127 and 128 are refused with their own text. The 128 records are preserved in git history
(`a50aec3a`) and in the S1/K3 evidence under `cache/`.

**Behavioural pins that moved (S01_07_01, 11 small farms and 115 + 90 potatoes; legitimate: the workers now cut fields, the objects change walk codes and routes).**
spawn draws / searches 2287 -> 2282, purchases 612 -> 608, bought 35026 -> 34729, deliveries 550 -> 547, delivered 104100 -> 103388, candidates scored 21146 -> 21097,
walk expansions 1750834 -> 1743166, transitions 8 -> 9, HouseVariant draws 8 -> 9, the upgrade 509 -> 520 now lands on 522 (draw 21945 -> 22619, index 0 -> 2), the first
downgrade's variant draw 1338 -> 19882 and its counters by one (1665 -> 1664 and so on). The Tutorial K3 upgrade itself (tick 55000, E 668, the stocks, 501 -> 522) is
**unchanged**; only its six replay checkpoint hashes moved. They moved with the version word at least; whether the Tutorial small farms' workers contribute (both have grass but no potatoes in reach, so their workers fail the search and return) is not separated from the version word's effect and is not claimed.

**Evidence.** `src/sim/tests/small_farm_worker_test.cpp`: (1) three ripe fields -> three cuts, 768 credited to raw 0, six full steps (78 made), counter released, 119 gate draws in the 12 000 advances (60 000 ticks; the 120th gate falls 5 ticks after the loop ends, a fencepost of the test loop); (2) failed search; (3) the home destroyed with the worker out, before the first cut and with one cut carried (the carried amount is lost as history, no claim left); (4) two farms
contending for two fields; (5) another type id (9331), goods (KARTOFFEL) and owner (island 1, player 1) with the other owner's field untouched; every case ends with `check_invariants`
and a save/load that reproduces the hash. (6) Original data: the Tutorial scenario's island-7 farm (261,344) cuts 3 of its 32 potatoes (709 -> 708) and makes 78 alcohol by tick 16 800.
Mutants (scratch checkout, legal control first, `cache/map2-supply1/S2mut/`): home check without SmallFarm, credit slot always 0, always 1, cargo goods not set, harvest state not attached,
validator refusing a SmallFarm home: 6 of 6 killed.

**Adaptations and limits (named, not original).** The credit is a u32 add clamped at the u32 maximum (the original's raw stock is a u16 add that wraps; identical below 65536, vector `C-wrap`);
walking and the cut wait use the engine's integer move model (the fine route is unread); the worker's capacity comes from the figure type (3 t for 106 on the shipped data; the synthetic cases use it too).
Not done in S2: animals (S3); the `Rules::house_updates` default stays false. A field replaced under the worker (the cut fails, nothing is credited, the claim is cleared, the worker goes on) and the reload at each walking/waiting/returning phase with a 2000-tick continuation are tested for the small farm (review P3). The full-stock case is open: the raw stock is a u32 here and cannot fill, and the original's u16 wrap is the named adaptation; a mod with a small raw capacity is not covered. The slot rule applies to the SmallFarm payload only (`harvest_credit_slot`), so a Farm type with two input goods cannot lose a credit into a store it lacks (unit test).
