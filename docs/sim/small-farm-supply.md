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

## 9. S3 delivered: the animals (2026-11-05)

**Change.** `consume` (driver.cpp) spawns the animal (job `0x2e`, kind from the draw table) with cargo goods = input 1 and the harvest state (D `0x1006f8ed-0x1006f920`);
`animal_job` / `harvest_state_job` (figure.hpp) let the shared body and the validators accept job `0x2e`; the body's step-A callback now follows D `0x10058a40` exactly:
an unclaimed goal is taken (the search result is replaced), and the search goes on only while `limit > score` (`harvest_search_goes_on`), with `limit` = 0 for the worker and
`kAnimalSearchLimit` = 20 for the animal (`harvest_search_limit(job)`; D `0x10058bfd` vs `0x1005932d`). The pure form `harvest_pick_tile` runs the S1 `pick` vectors. The credit,
the claim, the cut, the growth, the end path and the counter release are shared with the worker; the credit goes to raw 1 because the animal's cargo goods are input 1 (the real merge path,
asserted in the pasture test: raw 1 and NAHRUNG, raw 0 and ALKOHOL untouched). Farm-class sheep farms (807, 820, ... with `ROUTETYPE WEIDETIER`) are **not** given the animal state: the body
is wired to the small farm's animals only, as scoped; whether those farms graze the same way is open (they send a job-46 figure with no body, unchanged).

**Score units.** *Same field* (D): the callback compares `limit` with the u16 at `node+8` (`0x10003200`); the node pointer it receives is the open-list node plus 0x10 (`0x10004760`),
so `node+8` is the list node's word at `+0x18`, the field the open list orders by (`0x10004918`: `cmp di, [ecx+0x18]`). *Same scale* (inference, not shown): the engine's `SearchVisit::score` is its ordering score
and its increments are the decoded ones through the T33 port (docs/sim/state.md 5.9); that the port reproduces the original's absolute values is as strong as that port. **On the shipped tables the stop at 20 fires** (engine run,
original `WalkTables`, kinds 107/109/110 on layer 4, ground code 2 costs 10 straight / 13 diagonal): the ripe grass in reach of the Tutorial farm (212,250) has scores 10, 13, 16, 19, 20, 23, 26, ... 36 (9 distinct values),
the search goes on past 10..19, takes the goal of score **exactly 20** (`20 > 20` is false) and stops; the first animal's target is (212,248), where the worker's rule would have taken the first goal popped. The pasture test with the
synthetic rig (every cost 1, scores at most 7) proves only that the callback's use of the search equals the pure function on a unit-cost window (the animal ends at the last goal popped, (118,112), where the worker takes (114,111));
it says nothing about the original scale. (An earlier version of this section claimed the stop never fires on shipped data; that was a unit-cost artefact of the rig and is withdrawn.)

**Evidence** (`small_farm_animal_test.cpp`, `small_farm_vectors_test.cpp`; the sanitizer run is the 95-case selection of `cache/map2-supply1/gates/S3_asan.log`, 95 of 95 passed; the S3 reviewer's own ASan run was a 68-case subset). (1) Pasture: five ripe grass fields, no grain: 5 cuts, 1280 credited and delivered to raw 1, all five fields young, 102 NAHRUNG made
(credit worked off: raw 1 left = 1 < 25, `out = 4 * (used / 50)` within one tail step), kinds only 107 / 109 / 110, every animal carries GRAS, all counters released, never more than 3 animals out.
(2) The animal's first target equals `harvest_pick_tile(20, visits)` computed from a second run of the SAME walk search (not independent of the engine's search) from the home's entrance; on the unit-cost rig the target is (118,112) and the worker's pick would be (114,111).
(3) The home destroyed with animals out, before a cut and carrying: nothing delivered, the carried amount lost as history, no claim left, `check_invariants` and a reload with a 2000-tick continuation. (4) Reload at each of the
walking, waiting and returning phases, each with the continuation. (5) Limits: at most 3 animals out and per kind at most limit + 1 (the producing-step allowance of D `0x1006f855-0x1006f86b`, which the kernel vectors pin).
(6) Original data and original cost tables: the Tutorial town's farm (212,250), 28 ripe grass objects in its window and no potatoes. Asserted (pinned): first target (212,248) at exact score 20, scores 10..36 (9 values), and at tick 26 085 exactly 9 of the 28 fields cut (28 -> 19), 40 NAHRUNG made, all three kinds seen, line 0 untouched. The `>=` mutant now dies on the pinned target and score.
Mutants (scratch checkout, legal control first, `cache/map2-supply1/S3mut/`): cargo goods not set, harvest state not attached, limit 0, `>=` instead of `>`, animal not a harvest-state job, credit slot always 0,
pick never stops: 7 of 7 killed; re-run after the review fixes (`S3mut/mut2.log`, legal control first): 7 of 7 killed again, the `>=` mutant now also on the engine's real-table run (`stop_score == 20`).

**Identity.** No new field and no new draw site: 129 / timeline 5 is amended in place, because 129 is NOT published (S2 and S3 travel together, nothing is pushed). **129 as of `ab40919b` (S2 only) is superseded and was never published**; its values, for the record:
G3 `s11_g3_checkpoints d904dd576bbf45c8`, `s11_g3_end e69a0b509c99bf74`; the Tutorial K3 replay checkpoints `bedae9dd2e1a44cd 1de337941cadec47 28cf97e36e3b0e21 e85b6fb7cce9be38 9c76d456ea5de184 c52f3a9fe3df8357`; `scenario_corpus.txt` as committed at `ab40919b`.
The pins that S3 moved on top of it (`c5507ee4`): G3 `fe8975dd37cf07fd` / `b23878ae3170fbd9` (golden and log), the K3 checkpoints `72d54f5e636dcb8d f4e803347579882c c3357e0790ca2f42 cafe715ca879e80f 217c16e0d244b685 0a4278ce6f071e93`,
and 15 lines of `scenario_corpus.txt`. The S01_07_01 canary did not move for the animals. **If 129 is ever pushed before another incompatible change, that change is 130.** The g0/g1/s5/s6 goldens are reproduced by the independent
`tools/sim_stream.py`; the G3 / s11 goldens and the corpus are NOT (no new field or draw site, so the transcription has nothing to add): they are the engine's own values.

**Not done / limits.** (A small-farm type without input goods 1 sends no animal: no invented goods fallback.) Walking, graze waits and the fine route are the engine's integer model (as for the worker); the Farm-class sheep farms and the hunter (108) are untouched; refusal of a send (no figure created)
is covered by the kernel's `fail=` vectors only at reference level and by the scheduler's existing release path, not by an engine test of a refused spawn (the stand-in spawn cannot fail for a typed small farm); the Farm-class sheep farms are not wired to the animal body. The output of both lines is collected by the market cart in S4.

## 10. S4: warehouse, Inn and households on original data (2026-11-05)

**Slice claimed.** `Tutorial/02Trade01.sze`, island 7, city 1 (player 0), **imported** buildings: nothing is built by command in the main run, no stock is granted, nothing is relocated or seeded,
no stage flag is touched. The town (survey of all 515 files in `cache/map2-supply1/S1/s4_survey.txt`; 49 city rows hold small farms): four small farms 809 at (261,344) (32 potato objects in its 7 x 7 window, no grass),
(259,352) (neither), (307,349) and (292,333) (grass only), one Inn 1335 at (301,351), one food stall 1310 at (305,339), market halls and a Kontor, 13 houses (139 residents). **Initial inventory of the alcohol path:** warehouse alcohol 0,
Inn stock 0, every farm output and raw stock 0, no carrier or cart out; 8 of the 13 houses hold no tavern goods. Warehouse food 17 408 (68 t). `Rules::house_updates` is true in the test world only. Nothing else claims
this slice: the Tutorial island-3 town has grass but no potatoes (its food line works, S3), and no other file was run. A fresh public-command farm build was not attempted (the scenario's own farms already stand with their fields; a
BuildCmd farm would need a legal site with potatoes in reach, which is the same inventory).

**Ledger at tick 150 000** (before any cut field can have regrown, 150 000 ticks after the first cut; `small_farm_chain_test.cpp`, every term observed on its own, every unexplained term asserted zero; diagnostic ledgers added for it:
`ForestryStats::small_farms`, `ResidentStats::merged_taken_by_city`, `inn_taken_by_building`). **Potatoes:** 24 fields cut (the window's ripe count 32 -> 8) = 6144 = raw left 0 + used 6144 + carried 0. **Alcohol (one conservation identity over the whole path):**
made 624 (48 full steps x 13) = 312 still in the farm + 0 in the warehouse + 0 in the Inn + 312 sold by the Inn + 0 in a cart or carrier (the warehouse held up to 312 at one sample: a cart delivered a load and the Inn's carrier fetched it from
the store, so both the cart path and the carrier path ran). **Food:** made 1183 = 270 still in the farms + 913 loaded; the warehouse 17 408 -> 17 250 = +913 loaded - 314 in carts - 757 taken by the stall for the residents of this city.
The food ledger is per CITY and goods (`merged_taken_by_city`, scheduler.cpp 4), not per stall building; the town has three market buildings besides the stall. Reservations are inside the farm outputs and are not counted twice; ended figures' cargo is history (the in-flight sums read living figures of this island only). Households: tavern goods reached **4** of the 8 houses that held none at the start; food reached
**8** of 13. **Pinned values** (24 cuts, 624 made, 312 sold, store 17 250, 43 ripe increases) depend on the advance(100) stepping schedule of the test: the S4 reviewer measured 390 sold (instead of 312) at tick 150 000 with a different chunking of the same total time, i.e. chunking changes trajectories (a known property of the integer figure model; no document states it, this is the measurement). A replay or checkpoint schedule must use the same stepping.

**Causal control (producer removed).** The same town with the four farms demolished by `DemolishCmd` at tick 0 (a supported public command, issued by the owner), 150 000 ticks: the Inn never receives anything (stock 0, sold 0, warehouse alcohol 0), the food store only falls,
17 408 -> 16 654 = exactly the 754 the stall took (nothing comes in), against 17 250 with the farms. Alcohol: every unit sold was produced by the farm after the start (nothing existed at the start anywhere on its path), and without the farm the Inn sells nothing. **Food: the residents bought 754 units without the farms and 757 with them** - the farms did not change household purchases within 150 000 ticks because 17 408 units were in stock; the food result is only that the farms **added 596 units to the store** (17 250 vs 16 654). "Newly produced goods were needed" is therefore shown for the alcohol / tavern goods, not for food.

**Food dependence with a small store (CLOSE item 2, 2026-11-05; test "supply chain CLOSE-2: with a food store of 1 t ...", `small_farm_chain_test.cpp`).** The same town and the same 150 000 ticks in both arms, with the city's food store set to 256 raw (1 t) at tick 0 in both arms. That setting is a **named measurement setup** (a direct write to the store through the test access), not a game command: no supported public command drains a city store without a ship at a warehouse, and the Tutorial scenario ships with 17 408. Result: **with the farms the stall bought 756 units and the store ended at 99; without them (the four farms demolished by `DemolishCmd`) it bought 237 and the store ended at 19** (256 - 237, nothing comes in). The households bought about three times the store's content with the farms, so the surplus above the 1 t that was there came from food the small farms' animals made (the farms made 2 496 raw of food in that run). **Claim this supports:** in this town, with the store small enough that it cannot cover the purchases, the farms' food keeps the households supplied and removing the farms stops the purchases when the store is gone. It does not show a balanced economy, nor that a full-sized store would run out on its own.

**Repeated cycles.** A 600 000-tick run of the same town: the potato fields were cut and **grew back** (the ripe count rose 43 times, minimum 7 of 32), and 24 576 raw potatoes (96 t) were used or held against the 8192 (32 t) the fields held ripe at the start, so tiles were cut again after the 150 000-tick regrowth.
This shows the cycle in one town for 600 000 ticks; it is **not** a claim of a balanced or infinite economy: the Inn's sales (2064 raw units in that run) and the food balance were not driven to equilibrium, and no settlers upgrade is claimed or asserted.

**Provenance.** The sanitizer log's source revision and binary hash are in `cache/map2-supply1/gates/S4_asan.log.provenance`; all four mutant definitions are in `S4mut/mutants_all.py`; the first chain run that failed on the wrong Inn id and on the closed-alcohol assumption is `cache/map2-supply1/S4_run1.log`. **Mutants** (scratch checkout, legal control first, sha256-restored; `cache/map2-supply1/S4mut/`): no behaviour was added in S4 (only diagnostics), so they cut the links the chain depends on: the small-farm food line not collectable (`store_line`), a small farm without an output store (nothing is picked up), the credit slot always 0, and the Inn-sold diagnostic dropped: 4 of 4 killed by the chain test (the first run of the fourth did not compile, `mut_first.log`; rewritten, `mut2.log`).

**Limits.** One town, imported farms; potato availability is the scenario's (OQ-491); the resident side is the existing (partly ADAPT) walker model; the Inn's carriers take alcohol from the farm and from the warehouse alike (the existing `pick`), which this ledger
does not separate. Windows/Linux, sanitizer coverage and replay of this run are S5/S6.

**Checks run for S4.** Debug: the chain test (about 30 s) with the small-farm and harvest cases, 59 of 59. ASan/UBSan (`cache/map2-supply1/gates/S4_asan.log`): 70 cases selected by name (supply chain, small farm, harvest, farm dispatch, G3, G1, state_hash, the S01_07_01 canary), 70 of 70 passed, 0 sanitizer reports; coverage of the changed files:
`driver.cpp` (the small-farm ledger) and `scheduler.cpp` (the two resident diagnostics) are reached by the chain, canary and G cases, `transport.hpp` is header-only. `lint_sim` ok; no state or stream change.

## 11. S5: interruption, owners, reload, replay, operation counts (2026-11-05)

Code: `small_farm_resilience_test.cpp`, `island7_town.hpp` (the shared island-7 fixture), diagnostic counters `ForestryStats::search_tiles_scanned / search_candidates / search_popped`.
Everything below uses the advance(100) schedule; chunking changes trajectories (section 10).

**Interruption and recovery (Tutorial island 7, imported farm).** Two identical worlds run to tick 150 000, where the Inn has sold 312 units made by the potato farm and the houses hold 13 537 units of tavern goods. The farm is demolished by `DemolishCmd`
(owner's command, both worlds). Gap 150 000 -> 270 000: the two worlds stay hash-identical; nothing more is sold (312), the Inn is drained (0), nothing is in carts or carriers, the warehouse holds no alcohol, and the households' own stock falls 13 537 -> 8188 (it is initial inventory and decays by itself).
At 270 000 the farm is rebuilt by `BuildCmd` (type 809 on the same site, accepted) in one world; the control never rebuilds. To 510 000: control sold 312 (flat), households 1 957; rebuilt run **sold 936**, households **9 726**, the new building (a new id, new counters) made alcohol
from potatoes its own worker cut, and **9 houses** received tavern goods after the rebuild (the control none). The households' dependence is the difference between the two runs, not the rise of the warehouse. Food is not used for this test: 17 408 units in stock mask an interruption of the food line (stated, not worked around).
Pinned: 312, 13 537, 8 188, 936, 9 726, 1 957.

**Two owners (focused synthetic world, one island, two cities).** Player 0 owns a market and a farm at the end of a road; player 1's farm stands beside the road nearer to the market. Both farms hold 960 units of output (a disclosed preset: KORN is a raw good, so a cart takes it only above 75 % fill,
docs/re/production.md 5.2; production lines off). The cart takes the owner's farm only: its output 960 -> 0 and its warehouse 0 -> 960; the rival farm keeps 960, was never loaded (`loaded_from` has no entry), and the other city's warehouse stays 0. The worker's credit to its own home is in the S2 two-island case and every worker case; the Inn's carrier uses the same
source query, so the same city filter. The first fixture attempt moved nothing (the output was below 75 %), which is why the preset is 960.

**Reload at the boundaries of the real chain.** In the uninterrupted island-7 run the first steps at which each condition holds were found, the save was loaded there, the loaded world ran 3000 ticks, and the uninterrupted world's hash at that tick matched, six times:
a worker waiting at a field (tick 800), an animal waiting at a field (1 450), raw potatoes in the farm (14 100), a market cart carrying goods (8 600), a house walker carrying purchases (8 050), Inn stock above zero (95 700) (the boundary ticks are pinned).

**Replay (fresh process included).** `Recorder` over the interruption scenario (demolish at 150 000, rebuild at 270 000, 7 checkpoints to 430 000, 8609 entries): replays from bytes in a fresh world, 7 of 7 (checkpoint hashes pinned in the test). Two logs with one changed legal command: the demolition naming the other farm (259,352) diverges at the first checkpoint
after it (190 000; 110 000 and 150 000 matched); the rebuild one tile east diverges at 310 000 (the checkpoints through 270 000 matched). **Fresh process:** `anno-sim-replay`, built as its own target, sha256 `17d1b18753f0dd6109c7d6919e096e184b97678543062273da4797d6c8fd66d9`, `ANNO1503_GAME_DIR` set: the genuine log reproduces (exit 0, 7/7),
the changed-demolish log diverges at entry 3803 (expected 137fc9ad91424c61, got 83cedd67b2bb9aff, exit 1), the changed-rebuild log at entry 6206 (expected 833edcef11749aab, got a7151ec5811f0809, exit 1) (`cache/map2-supply1/S5logs/fresh_process.txt`).

**Operation counts.** One farm with two fields and 0, 7 and 21 unrelated farms without fields in reach, 8000 steps each (deterministic counters, no timing): the work of the farm that has fields is unchanged (2 cuts, 28 walk nodes popped, 3 candidates); each extra search examines exactly the farm's 7 x 7 = 49-tile window
(scanned = 49 x searches in all three runs: 76 636 / 1 564, ...), never the island, and the walk search is not started when the window holds no candidate. Searches grow linearly with the farms (1 564, 12 750, 35 122). **Release timing samples: NOT_RUN** (no Release build on this host in this packet). No multiplayer-scale claim.

**Ship-founded settlement smoke: not attempted** (bounded result; measured in section 12).

**S5 mutants** (scratch checkout, legal control first, sha256-restored, `cache/map2-supply1/S5mut/`): the building query ignoring the city (a rival city's farm becomes a source) is killed by the two-owner case (the rival's output was loaded); the harvest search scanning from the island origin instead of the window is killed by the operation-counter case
(the first run of the first mutant did not compile, `mut_first.log`; rewritten, `mut2.log`). **ASan/UBSan** (`cache/map2-supply1/gates/S5_asan.log`, provenance beside it): 71 cases selected by name (resilience, supply chain, small farm, harvest, farm dispatch, G3, state_hash), all passed, 0 sanitizer reports; the changed files are the test fixtures plus
`harvest.cpp` (the three search counters), reached by the operation-counter and chain cases.

## 12. Ship-founded smoke: the bounded measurement (S6, 2026-11-05)

**(a) Data scan, repaired (CLOSE item 1, 2026-11-05; `cache/map2-supply1/S1/s7_ship_scan.cpp`, output `s7_ship_scan.txt`; the first S6 scan was unsound and is withdrawn: it walked only the 50 `.sze`, swallowed load errors, printed only successful matches and filtered the potatoes to neutral-owned ones, while the harvest search `src/sim/src/harvest.cpp` takes candidate tiles of its square whatever their owner).**
- *Raw, all files:* 50 `.sze` + 465 `.scp` = 515 files read, **0 load failures**, 1 133 islands, **26 islands with potato objects (709), 2 388 objects** (records GELAENDE, PROD2, FLUESSE, duplicates removed per island).
- *Sim worlds, all 50 `.sze`* (owners known): 50 loaded, 0 failures, 897 islands, the same **26 islands with candidate tiles (the farm's input-0 goods 68 by `harvest_candidate_type`), 2 388 candidate tiles: 2 384 inside a city's territory, 4 on neutral land** ((384,482) in `0201SingleAddOn/single41.sze` island 0; (259,344), (259,345), (259,346) in `Tutorial/02Trade01.sze` island 7). The earlier statement "none is on free land" was wrong by these four.
- *Sites:* farm 809 is 3 x 3, action radius 3 (the search square is the 9 x 9 tiles around the centre, `search_area`). On the 26 islands there are **460 357 positions with a free footprint (all nine tiles object-free and neutral). For every one the real search square was counted with the farm's own candidate predicate, owner ignored: not one position has a single candidate tile in its square (0 of 460 357)**, so `placement_rule` was never reached for a candidate (rejects: none). The four neutral potatoes are encircled by territory and objects.
- *What this shows and does not show:* in the shipped files no position on free land would let a new small farm's worker see a potato object. It says nothing about a position inside a city's territory that a ship-founded city does not own, and it does not model the territory a new Kontor claims. A ship-founded settlement on shipped objects therefore cannot start the alcohol line (consistent with OQ-491: nothing creates potato objects). Not run: the sailing and founding sequence itself (not attempted, as asked).
**(b) Tables.** Farm 809 INFRAMIN is `INFRA_EVER`, cost gold 250, tools 2 t, wood 4 t; the Inn 1335 needs `INFRA_STUFE_1B` (docs/re/population.md section 18); the default ship loadout holds 50 t of wood and 80 t of tools (12 800 and 7 680 + 12 800 raw): the materials are not the blocker.
**(c) City creation and BuildCmd:** not attempted: with (a) there is no site with a candidate, and a farm on grass alone would only show the food line (S3 / S4) again. **Measured blocker:** no free position with a potato object in the search square. The food line (grass objects are everywhere) would be the feasible smoke, not run here.

## 13. S6: gates, identity and what the Windows side must do (2026-11-05)

**Frozen source.** Code commit `3c758a7e79dae7e8823fc20efa1460f86fd48584` (clean tree, no source edits during a gate; the commits after it are documentation only and named below). Host: 18 cores, 48 GiB, 1.5 TiB free, load about 3, none of my other jobs running.
- **Debug** (`run_selected.py`, main workflow `27022d75b5b8...`, sha256 of the script `3c84f394...`; log `cache/map2-supply1/gates/S6_debug_sim.log` sha256 `c3de67fb1c2667d8466653d4cc62747578e6ab309957b8c90dd877588651c6fa`): `anno_sim|_reference_|vectors_agree|lint_sim|anno_formats_tests`, 2 test processes, `--asset-policy required`, no `--allow-skip`, fifteen required contracts by exact name
(`small_farm_reference_agrees`, `small_farm_reference_contract`, `house_progression_vectors_agree`, `house_transition_reference_agrees/contract`, `walk_goal_reference_agrees/contract`, `inn_reference_agrees/contract`, `resident_reference_agrees/contract`, `house_kernel_reference_agrees/contract`, `forester_reference_agrees/contract`).
**selected 1435, executed 1433, failed 0, empty 0, skipped 2 (both opt-in: the nav-scale and transport-statistics measurements), incomplete 0, sanitizer lines 0, VERDICT PASS.**
- **First attempt preserved** (`S6_attempt1_preflight_fail.log/.out`): the workflow's mandatory test-name preflight refused the animal original-data case because its name contained a semicolon (CTest/doctest cannot select it); the case was renamed (commit `3c758a7e`) and the gate re-run. No test had started.
- **ASan/UBSan** (`S6_asan.log` sha256 `36a13c0f59a40337540d8ffdf22f40a5ebb9715529c0b4c37ee821b9cea2352c`, selection in `S6_asan_selection.txt`): **selected 282, executed 281, failed 0, empty 0, skipped 1 (opt-in), sanitizer lines 0, PASS.** The selection comes from the changed-file map of `a50aec3a..3c758a7e` (production files only; no change to
`types`, `building` or `state_fields`): `driver.cpp` (the small-farm ledger, the worker and animal spawn, the credit slots) <- small farm, supply chain, resilience, G1, G3, G4, G6, scenario, house, resident, inn cases; `harvest.cpp` <- harvest, small farm, forester, resilience; `figure.cpp` (validator) <- figure, harvest, load and state cases;
`invariants.cpp` <- every `check_invariants` caller (all of the above); `scheduler.cpp` (the diagnostics merge) <- supply chain, resident, canary S01_07_01, progression K3, transport; `world.hpp` (the version word) <- state_hash, world core, load refuses, "a valid stream"; the headers (`driver`, `figure`, `harvest`, `transport`) are reached through the same cases. Every changed production file is reached by the selection; `invariants.cpp` is reached **only through the `check_invariants` callers** (it has no test of its own among the 282), and the other files are reached directly by their named cases. **The full-suite sanitizer run is NOT_RUN** (only these 282 cases).
- **Hashes.** Debug `anno_sim_tests` `d3d977cb...3aea`, ASan `anno_sim_tests` `75b3f829...a8b0`, `anno-sim-replay` `72ec1ca5...dc4e` (the S5 fresh-process evidence used the earlier build `17d1b187...`, same sources but the S5 commit), `fixtures/synthetic/sim` tree `9c23e9f5...1981`, `src/sim` tree `a620b7fc...37e2`.
- **Also reproduced by reviewers during the packages:** S4 full suite 1268, S5 1273, S3 re-review 1337, ASan 70 / 71 / 95.

**Identity (Windows integration delta).** `kStateHashVersion` is **129**, `kPrngTimelineVersion` stays **5** (no field, no draw site). `128 / 5` was published at `a50aec3a` and is unchanged in git history. What 129 is incompatible with: every save, replay log, golden and checkpoint hash made at 128 (the loader refuses 127 and 128 with its own text; `goldens.txt`, the recorded `.log` fixtures, `ship_holds`, `cargo_rules_bytes`, the stage digest and `scenario_corpus.txt` are re-baselined; the g0/g1/s5/s6 goldens come from the independent `tools/sim_stream.py`, the other goldens and the corpus are the engine's own values). Why it moved: the small farm's worker and animals now harvest
(worlds with a small farm evolve differently; the K3 replay checkpoints and the S01_07_01 canary pins moved). **Windows keeps its pinned input until the orchestrator says otherwise;** to integrate it must take the `src/sim` production changes (driver, figure, harvest, invariants, scheduler, headers, world.hpp), `tools/sim_stream.py`, the fixtures, the new tests and the CMake registrations (Python 3 for the reference checks), re-record any 128 logs, and run its own MSVC build and gates (not run here). No new operating-system API: the changes are standard C++20.

**Documentation commits after the gate (documentation only):** the ship-smoke result, this section, the review ledger rows S0-S5, OQ-491 / OQ-464, PLAN status (the commits following `3c758a7e`).

## 14. CLOSE: Release samples of the search work against unrelated farms (2026-11-05)

Opt-in case `resilience: harvest search work against unrelated farms (opt-in measure, Release samples)` (`ANNO_SIM_SUPPLY_SCALE=1`, skips with a message otherwise; wall time is printed, never asserted). Host: Apple Silicon Mac, 18 cores, 48 GiB, load about 3 at the start (`cache/map2-supply1/gates/S7_release_host.txt`), the measurement ran alone in one thread. Build: separate `build/mac-release` (`CMAKE_BUILD_TYPE=Release`, AppleClang, Ninja, tests on, warnings as errors, GPU and viewer off; configure and build logs `S7_release_configure.log`, `S7_release_build.log`); `anno_sim_tests` sha256 `d75980ab4846c9114e8ed1815905a33613f28a347906b705ed21e6e1e3326c9d`. Scope: the synthetic rig (one 32 x 32 island, one small farm with two grain tiles at distance 3, k unrelated idle farms in a grid with no field within 3 tiles), 8 000 advances of 50 ticks (400 000 ticks). Three runs:

| farms | searches | tiles scanned | candidates | walk nodes popped | cuts | wall ms (3 runs) |
|---|---|---|---|---|---|---|
| 1 | 1 564 | 76 636 | 3 | 28 | 2 | 10.3 / 10.2 / 10.4 |
| 8 | 12 750 | 624 750 | 3 | 28 | 2 | 39.3 / 40.0 / 39.2 |
| 15 | 23 936 | 1 172 864 | 3 | 28 | 2 | 69.7 / 70.9 / 69.9 |
| 22 | 35 122 | 1 720 978 | 3 | 28 | 2 | 100.5 / 101.7 / 99.9 |

Reading: the counters of the farm that works (candidates, nodes popped, cuts) do not change; each idle farm adds about 1 600 searches of 49 tiles over the run (the 7 x 7 window, never the island), so tiles scanned and wall time grow **linearly** with the number of farms (about 4.3 ms per farm per 400 000 ticks in Release, 1 core); there is no superlinear term in this range. Not measured: unrelated cities (the rig has one city), more than 22 farms (the island is 32 x 32), farms with fields all around (work then scales with the fields), other hosts, Linux and Windows Release (NOT_RUN).
