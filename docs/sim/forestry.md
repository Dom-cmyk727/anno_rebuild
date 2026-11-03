# Simulation: the forester chain (contract)

**Status:** DRAFT for `docs/sim/forestry.md` (MAP-ECON-1 E1, research and contract only; no `src/` change is made or authorised by this document). Evidence: `docs/re/forestry.md` (D/E/P labels, addresses). Tags used here: **D/E/P** as there, **A** = adaptation (ours), **U** = unknown, **NOT_RUN** = needs the running original. Stage names E2–E6 below are a *proposal* for the parent; nothing here starts them.

## 0. Goal, scope, and what must not be faked

The first authentic wood chain of the opening: **an existing mature tree tile → the forester's worker (figure 102, job `0x2a`) → `Farm.raw` → the existing farm production step → `HOLZ` in `Farm.out` → the existing road-connected market cart → warehouse.** Charcoal burners (research-gated), pre-seeded `raw`/`out`, fixture trees, loadout edits, relocation and any "interim return" shortcut are **not** substitutes (task packet).

Original progression holds: building 801 has `INFRAMIN "INFRA_EVER"` (811 carries `INFRA_STUFE_3B`) and no `FORSCHTYPE` record (P); reading: no population stage or research gate, so it is placeable at the start (the placement-side gates are the existing build rules; not re-verified here). Cost 150 gold + 2 tools, 0 wood (P), so building it does not consume ship wood. The ship's wood stays in the warehouse for roads and other buildings; the canary therefore must measure *wood the forester made* (§7), not the warehouse total.

Existing machinery that is reused unchanged: `farm_step` (rate 14, need 32, capacity 1280, `max_figures` 2), `Farm` payload, `consume(StepResult)` (`workers_out` counter and `SpawnRequest`), the figure registry and its phase B seam protocol, `transport` walk/search (`walk_search.cpp`, layer blocks), `grow_chunk`/`grow()` for the tile growth, the market cart and `CityStock`. What is missing is **only** the job-`0x2a` body, its tile effects and the Farm-raw delivery.

## 1. The rule set (what the original does)

All of §2 of `docs/re/forestry.md`; the numbers a test needs:

| Item | Value | Tag |
|---|---|---|
| job id of a `MAEHER` route figure | `0x2a` (the figure type's `ROUTETYPE` index; slot `+0x54`) | D |
| farm step | 500 ticks; consume 32, make 14 at ratio 128; ratio ≥ 64 or status 1 | D |
| workers | `max_figures(0)` = 2; send iff `workers_out < 2` and `384/14 = 27 ≥ raw/32` (raw after the step) | D |
| search square | `(x + (w−1)/2 − r, y + (h−1)/2 − r)`, `2r + 1 + ((w−1) mod 2)` wide, `r` = 5 → 11 × 11 for a 3 × 3 forester; u16 words | D, E |
| candidate | tile object whose type `WARE[0]` = cargo goods (`BAUM`, 70) and claim bit clear; row-major | D, E |
| winner | first unclaimed goal popped by the walk search (layer 2, disc radius 5) | D; route order U-1 |
| claim | tile record second dword bit 29, set when the walk exists, cleared at the cut and on abort | D, E |
| cut wait | 13.0 → **6 500 ticks** after arrival | D |
| cut | `id → id − 1` if type(`id − 1`).PRODTYPE = `ROHSTWACHS`; `phase &= 0x80`; else nothing | D, E |
| cargo | +256 on a successful cut; capacity 256 → no repeat | D |
| delivery | `Farm.raw += cargo` (u16 add, no clip) at the end of step C | D |
| failed search | job goes to step C, delivers 0, figure ends | D |
| home gone | abort at the next arrival: claim cleared, no cut | D |

## 2. State: what is hashed and saved, what is derived

| Datum | Where | Hashed/saved | Why |
|---|---|---|---|
| Farm `raw`, `out`, `workers_out`, `last`, … | existing `Farm` | yes (unchanged) | the production state |
| the worker figure (`job` = `0x2a`, `cargo_goods` = 70, `cargo_88`, position, route) | existing `Figure` | yes | a figure is state |
| the harvest job step | new `Figure::harvest` (below) | yes (field list, stream bump) | the job is mid-flight state |
| the claim bit | tile record `aux` bit 29 (`kTileClaim = 1u << 29`) | yes (`aux` is in the chunk digest) | the original keeps it in the tile record; it must survive save/load and be part of the hash |
| the cut tile (`object`, `phase`) | existing tile columns | yes | |
| candidate list, search square, walk cells, route cache | computed per search | **no** (derived) | recomputed from tiles + types |
| the statistics counters `trees_cut`, `raw_delivered_88`, `wood_made_88`, `wood_sent_88` | `ForestryStats` beside `transport_stats` | **no** (diagnostic, not in the state hash) | the canary reads them; they must not change the hash |

```cpp
enum class HarvestStep : std::uint8_t { Start, ToTree, Cut, ToHome, Done };   // jobs steps A, B, C (D factory 0x1003b0e0, job 0x2a)
struct HarvestState {                       // Figure::harvest (only for job 0x2a)
    HarvestStep step = HarvestStep::Start;
    std::uint32_t wait = 0;                 // ticks left of the 13.0 cut wait (Cut)
    TileXY target{};                        // step A's tile; valid in ToTree and Cut; claimed
    std::vector<FixedPos> route;            // as TransportState (walk to the tree, then home)
    std::uint32_t next = 0, along = 0;
};
```
`validate_figures` / `check_invariants` additions: (1) the set of tiles with the claim bit equals the set of `target` of figures in `ToTree`/`Cut` (a stray bit is the original's kill leak, which the rebuild does not reproduce: **A**, every figure end path clears or aborts); (2) `Cut` ⇒ `wait > 0` or being resolved; (3) `harvest` present ⇔ `job == 0x2a`; (4) `target` inside the island.

`aux` currently refuses bits 8..31 (`validate_island`): only bit 29 is admitted (**A**: the rest stay refused). The original's owner nibble is bits 25–28 of the same dword (`docs/sim/state.md` §9), bit 29 is distinct (D).

The raw store is a u16 in the original; `Farm::raw` is u32. Under the dispatch rule `raw ≤ 895 + 2 · 256` the value never nears 65 536, so the difference is unobservable (**A**, noted; a guard `raw ≤ 0xFFFF` in `check_invariants`).

## 3. The commandless lifecycle (scheduler placement)

Nothing here is a player command; everything is driven by the existing per-tick order (`docs/sim/state.md` §5.3–§5.9):

1. **Building tick (city ring, TickKind Farm).** `farm_step` as today. When `send_figure` is true, `consume` increments `workers_out` and emits a `SpawnRequest`; the change: `send_figure_job(Farm)` returns the type's route job (`FigureType::route`, new field = `ROUTETYPE` index via the name table `AnnoGame 0x10024fb8`, 0 when the record is absent) instead of 0, and the spawned figure starts with `cargo_goods` = the building type's `inputs[0]` (D `0x1006ee06`).
2. **Figure registry phase B (the seam protocol, `docs/sim/state.md` §5.9).** A new default body `harvest_tick(fig)` beside `transport_tick`, selected by `fig.job == 0x2a` (and `0x29`, the same constructor). It reads the world and its own figure and writes only its own figure and `Effects`; it draws nothing.

```
harvest_tick(fig):                                    // D 0x1005b6c0/0x1005c260 steps A, B, C
    ft = figure type; ticks = u16(u16(t) - fig.stamp); fig.stamp = u16(t)
    if fig.expiry_stamp: return true
    loop (each pass waits or moves to a later step):
      Start:    if no home: step = Done                                // a figure without a home aborts (nothing claimed yet)
                square = search_square(home, type.radius)               // §1
                goals  = candidates(island, square, fig.cargo_goods)    // WARE[0]==goods, claim clear, row-major
                if goals empty: start_home(); continue                  // step A fails -> step C (D 0x1005e280)
                path = walk_search(layer = ft.layer, from = fig.pos, goals, accept = first unclaimed popped)
                if none: start_home(); continue
                target = path.goal; Effects.tile_edits += {claim(target)}; route = path; step = ToTree
      ToTree:   walk(ticks); if not arrived return
                if no home: Effects.tile_edits += {unclaim(target)}; step = Done      // D 0x1005c260 step 1 -> abort
                else wait = ft.load_ticks (6500); step = Cut
      Cut:      wait out the ticks
                if no home: Effects.tile_edits += {unclaim(target)}; step = Done      // abort instead of the arrival
                else: Effects.tile_edits += {unclaim(target), cut(target)}            // D 0x10054530: clear, then 0x10087340
                      if the cut took effect: fig.cargo_88 += 256
                      if ft.capacity_88 > fig.cargo_88: step = Start; continue        // D 0x1005c260 type 0xb (never for 102)
                      start_home()
      ToHome:   walk(ticks); if not arrived return
                if home: Effects.store_deltas += {home, Raw, +fig.cargo_88}            // D 0x1006ee70, u16 add, no clip
                step = Done
      Done:     end_figure(fig)                                          // existing: release the home counter, expiry list
```
`cut(target)` is applied by the merge of `Effects` in order, *immediately after this figure's call* (the seam protocol's C2 rule), so the next figure in the same pass sees the new claim bit and tile object, as in the original where every write is immediate.

3. **Growth.** The island's grow sweep (existing `grow_chunk`, every chunk at most every 2000 ticks) takes the cut tile (object `N − 1`, phase 0) through `S · 2^k` flips back to `N`. No new code; the `ROHSTWACHS` type must be found by `find_growing` (it is, `types.cpp`).
4. **Market cart.** Unchanged: once `Farm.out ≥ 256` (1 t) the home city's market cart considers the forester (goods 42 = `HOLZ`, not in 1–25 → `score = (avail << 7) / 1280`); its load adds to `CityStock` at the warehouse. Nothing in the chain cares where `out` came from, which is why §7 needs a ledger.

**Deviations from the faithful default need an owner decision (CLAUDE.md: improvements are opt-in `rules`):** the leak-free claim on figure death and the placement-aborts-job rule below remove an original effect (a leaked claim permanently hides a tree). They are provided as `rules.forester_claim_hygiene` (default faithful = leak) or recorded as justified deviations by the owner; the research does not choose.

**Rules options (A, opt-in, off by default):** (a) `forester_skip_uncuttable` — exclude from the candidate list tiles whose `id − 1` is not `ROHSTWACHS` (hazard ID 261, `docs/re/forestry.md` §4.5); the faithful default keeps the original's endless retry. (b) none for the claim, the wait or the capacity.

**Determinism.** Integer only (ticks, 8.8 amounts, Q24.8 positions); candidates row-major; the walk search is the existing deterministic search (FIFO ties); effects merged in figure-registry order; **one world-draw-index advance per DISPATCH that passes the rate test** (even if the send is then refused; reviewer finding, D `0x1006edf3`–`0x1006ee06`: city `+0x18`, island `+0x18`, world `+0xb4` = `0x100a1740`, which does `[world+0xfc] = (idx+1) mod [world+0xfe]` and returns a table entry that the farm discards; same chain as the small farm's draw `0x1006f810`). The rebuild must consume exactly one draw per such dispatch (a treeless forester burns one per step); the existing farm-family path does not, and adding it changes the PRNG stream of every farm-family fixture and golden: gap to fix at E2 with a stream bump. **No draw** inside the figure body (D: `0x10058ae0`, `0x10092320` mode 0, `0x10087340`; mode 1 of `0x10092320` samples but is unused); no wall clock; ordered containers only. Save/load mid-trip must reproduce the same hash (test T-F9).

**Placement and clearing vs the claim (transport.md §3.4.17 table):** placement clears second-dword bits 29–31 (`and 0x3fffffff`, `0x100824a7`), the clear arm preserves them. So placing over a claimed tile drops the claim while the worker still targets it, and clearing a tile keeps a stray bit. Rebuild rule (A): invariant (1) is one-way (every ToTree/Cut target is claimed, or the figure aborts at its next arrival; a claimed tile with no figure is repaired at validation); the placement path must abort the targeting figure's job. U-11 (route invalidated mid-walk) is open.

**Limits (CLAUDE.md "no hardcoded limits").** The original's 5 worker slots and 1024-pair candidate buffer, the 11 × 11 square and 2 workers are data (`HEAD[0x11]`, `max_figures`) or unbounded here; the u16 wrap of the search square is kept as the faithful default (test vector `area 3 3 3 3 5`) with the signed variant as an option only if a map ever places islands at world x < r (A).

## 4. Plug-in points (exact)

| Existing | Change |
|---|---|
| `types.cpp` `figure_type_from_entry` | add `route` (ROUTETYPE index through the table at `AnnoGame 0x10024fb8`, 80 entries read from the DLL at load like other tables, or an equivalent config table; never a compiled-in list: moddable) |
| `driver.cpp` `send_figure_job`, `spawn_request` | job from the first figure type's `route` for the farm family; cargo goods `inputs[0]` in the `SpawnRequest` |
| `figure.hpp` `Figure` | `std::optional<HarvestState> harvest` (field count + stream bump) |
| `tick.hpp` `Effects` | `tile_edits` (ordered list: claim, unclaim, cut) and a `Raw` store kind in `store_deltas` |
| `island.hpp` | `kTileClaim` in `aux` + `claimed(idx)`; validation admits only that bit |
| `transport.cpp` / new `harvest.cpp` | `harvest_tick`; reuse `search()`/`walk()`; tile goals (U-1) |
| `scheduler.cpp` | default figure body: `job == 0x2a` → `harvest_tick`, else existing |
| tests | below |

## 5. Hand-derived examples (derived on paper before `forester_ref.py` ran; reproduced by it)

Notation: 8.8 amounts (256 = 1 t); `rate0 = 14`, `need0 = 32`, `cap0 = 1280`, `max_figures = 2`.

**E-1 conversion.** `q0 = 256/18 = 14.222…`; `rate0 = trunc(14.222 + 0.5) = 14`; `need0 = trunc(14.222 · 2.25) = trunc(32) = 32`. Control: 811 `T0 = 7`, `I0 = 2.1875` → 37 / 80; 824 → 26 / 25 (documented values).

**E-2 dispatch.** raw 0, out 0, workers 0: ratio `(0<<7)/32 = 0 < 64` → status 1, nothing made; `max_figures 2 > 1` so dispatch: `0 < 2`, `384/14 = 27 ≥ 0/32 = 0` → send. Next step with 1 out: send again. With 2 out: no send. So two workers leave on two consecutive steps (500 ticks apart) if no tree has been delivered.

**E-3 the 895 / 896 boundary.** raw 927: ratio `(927·128)/32 = 3708 → 128`; consume `(32·128)>>7 = 32` → raw 895; made `(14·128)>>7 = 14`; dispatch `895/32 = 27 ≤ 27` → send. raw 928 → 896 → `896/32 = 28 > 27` → **no send**.

**E-4 partial ratio.** raw 16: ratio 64 (not < 64): consume `(32·64)>>7 = 16` → 0, made `(14·64)>>7 = 7`. raw 15: ratio 60 < 64: status 1, nothing. raw 20: ratio 80, consume 20, made `1120>>7 = 8`. raw 31: ratio 124, consume 31, made `1736>>7 = 13`.

**E-5 full output.** out 1280 (≥ cap): status 2, nothing made, raw kept, still dispatches. out 1279: produces, out becomes 1293 (no clamp): the cap test is `cap ≤ out` before adding.

**E-6 conversion per trip.** One trip = 256 raw = 8 full steps (8 · 32 = 256) → 8 · 14 = **112** = 0.4375 t `HOLZ`. raw 250: 7 full steps (224 raw, 98 wood), raw 26 → ratio `(26·128)/32 = 104` → consume 26, made `(14·104)>>7 = 11` → 109 wood, raw 0. 2.2857 trips make 1 t of wood; the 5 t output cap equals 11.4 trips.

**E-7 search square.** Forester 3 × 3 at (100, 200), r 5: `x0 = 100 + 1 − 5 = 96`, `y0 = 196`, 11 × 11 → tiles 96..106 × 196..206. A 2 × 2 building at (10, 10): `x0 = 10 + 0 − 5 = 5`, width `11 + 1 = 12`. At (3, 3) 3 × 3: `x0 = −1` → u16 65535 → no candidates.

**E-8 candidates.** Island (0, 0, 8, 8), square (1, 1, 4, 4): tiles (1,1) 105 free → yes; (2,1) 104 young → no (`WARE` unused); (3,1) 105 claimed → no; (4,1) 105 → yes; (1,2) 259 → yes (listed even though it will not regrow); (2,3) 261 → yes (listed although the cut will fail); (3,3) 713 grain → no; (4,4) 107 → yes. Order: (1,1) (4,1) (1,2) (2,3) (4,4).

**E-9 the cut.** 105 phase `0x45` → 104 phase `0x00`; phase `0xC5` → `0x80`; 259 → 258; 261 unchanged, answer 0; id 0 unchanged.

**E-10 contention.** Two trees T1 (cost 3), T2 (cost 4). Worker 1 chooses T1 and sets its claim bit; worker 2's list excludes T1 and chooses T2. If T2 is also claimed the list is empty → step A fails → worker 2 delivers 0 and ends.

**E-11 regrowth.** Tile cut at t = 100 000, phase 0, `P/2 = 12 000`, 20 flips, visits every 2 000 ticks: block `100000/12000 = 8` (even, parity 0 = stored 0, no flip); first flip at the first visit in block 9 = **108 000**; flip n at 108 000 + 12 000 (n − 1); n = 20 at **336 000** → object 105 again, 236 000 ticks (472 s) after the cut. Visits at 109 000 + 2 000 m: flip 1 at 109 000, flip 2 at 121 000 (first visit ≥ 120 000), …, flip 20 at **337 000**. Nominal `T0` is 480 s; the cut resets the counter, so the wait is about 456–484 s (block alignment plus up to 2 000 ticks of visit delay).

**E-12 one trip's clock (symbolic).** Spawn at tick `t_s` (farm step); the figure's first phase-B visit starts step A (search, claim); walk `d₁` ticks (route length / `speed_q24`, ≈ 500 ticks per tile for figure 102); wait 6 500; at `t_s + a + d₁ + 6500` the tile becomes 104, the claim clears, cargo 256; walk home `d₂`; `raw += 256` at `t_s + a + d₁ + 6500 + d₂`. For a tree 4 tiles away: `d₁ ≈ d₂ ≈ 2000` → about 10 500 ticks (21 s) per trip; 2 workers → 2 trips per 21 s = 224 wood per 21 s ≈ 2.5 t/min, below the farm's 3.28 t/min ceiling (14 per 500 ticks): the forester is **worker-limited**. First delivery of wood to a warehouse ≈ trip (21 s) + 19 production steps for the first tonne (the first worker's 256 raw gives only 112 wood, so the first tonne needs about 2.3 trips ≈ 25–40 s) + the cart's walk and its 500-tick wait: about 1 to 1.5 minutes of game time (adaptation estimate, **not** a measurement; U-1 and U-2 move it by seconds).

## 6. Test plan and acceptance table (evidence contract)

| Requirement | Positive proof | Counterexample / negative control | Evidence |
|---|---|---|---|
| T-F1 type facts of 801/102 | corpus test: registry gives rate 14, need 32, cap 1280, max 2, ids [102], r 5, capacity 256, layer 2, wait 6 500, route 0x2a; INFRAMIN not research-gated | mutate `PROD3[0x29]` 2→1 in a copy: max_figures 1 and the single-worker skip appears | vectors `conv`, corpus test (requires `ANNO1503_GAME_DIR`; classify the skip) |
| T-F2 dispatch rule | `forester_ref.py step` vectors against `farm_step` | raw 928 must not send | vectors E-2..E-5 |
| T-F3 eligible set | island with trees: young, claimed, other goods and corner trees outside the disc are listed (no distance test in the candidate list; only the walk cannot reach them) | a claimed tile appears in the list → fails | vector `cand`; unit test with a hand-built island |
| T-F4 cut | tile 105→104 phase 0, claim cleared, +256 cargo | 261 changes nothing and adds no cargo; home removed before the arrival cuts nothing | vector `harvest`; scheduler test |
| T-F5 delivery | `Farm.raw` +256 exactly once per completed trip | no-tree forester: raw +0, workers_out returns to 0 each trip | scheduler test |
| T-F6 conversion | one delivery then idle steps: out +112, raw 0 | raw 250 gives 109, not 112 | vector `drain` |
| T-F7 contention | two workers choose two different trees; with one tree the second worker ends empty | both choose the same tile → fails | scheduler test |
| T-F8 regrowth | cut tile ripens at the E-11 tick when stepped with explicit visit times | an off-by-one flip moves it by 12 000 | vector `grow` + island tick test |
| T-F9 save/load/hash | save mid-`ToTree` and mid-`Cut`, load, run on: hash equals the uninterrupted run; two identical runs equal | perturbing the claim bit or `wait` changes the hash | `World::save/load` test (existing pattern) |
| T-F10 home removal | worker out, building removed: claim cleared at the next arrival, tile untouched, figure ends without releasing | the tile is cut after removal → fails | scheduler test |
| T-F11 chain to the warehouse | forester + road + warehouse market: after the run `CityStock[HOLZ]` rises from forester wood | see §7 | canary |
| T-F12 determinism | ASan/UBSan, two runs, exactly one draw per send and none in the figure body (draw counter delta == sends) | | existing sanitizer jobs |

## 7. The canary: new wood versus ship wood

The warehouse starts with wood from the ship (HUD 50 t, verified by running in T15 stage 1, `docs/re/production.md` "Verified by running") and roads/buildings spend it, so the total proves nothing. Required evidence, all from one run:

1. **Differential.** Run the same seed/scenario twice, (A) with mature trees inside the forester's square, (B) with every `BAUM` tile in the square young or removed (fixture variation made in the *scenario inputs*, not by seeding `raw`/`out`). Assert `CityStock[HOLZ]` and every other input equal at the start, and after the run `Δ_A − Δ_B > 0` with identical spending in both (same commands).
2. **Ledger (non-hashed `ForestryStats`).** `trees_cut > 0`; `raw_delivered_88 = 256 · trips`; `wood_made_88` equals the farm's recorded `made` sum and `⌊raw consumed / 32⌋ · 14` within the partial-ratio rule; `wood_sent_88` (cart loads whose source building is the forester) ≤ `wood_made_88`; `Δ_A − Δ_B == wood_sent_88` (delivered wood is exactly what carts took from this farm).
3. **Tile ledger.** The number of tiles that changed `105 → 104` equals `trees_cut`, and each such tile has no claim bit afterwards.
4. **Negative controls that must fail the canary:** (a) a run with `Farm.out` pre-seeded and trees absent (delivers wood, `trees_cut == 0` → rejected); (b) a run where the forester is switched off (no wood, A − B = 0); (c) a run with trees but no road/warehouse (`wood_made > 0`, `wood_sent == 0`, warehouse unchanged).
(d) `Farm.raw` pre-seeded and trees absent; (e) the route home blocked: `raw` must stay 0 and the cargo stay on the figure; and an assertion that every `raw` delivery tick is at least cut tick + the walk home (no delivery at cut time). Add: initial `raw` and `out` asserted 0; raw conservation `raw_consumed + raw_left == raw_delivered`; `wood_made` equals the step rule applied to the consumed raw (raw 26 gives 11, not `⌊26/32⌋·14 = 0`). `wood_sent` is produced by the code under test, so independence rests on raw and tile conservation plus the differential, which needs two runs. Nothing in the canary may edit the ship's loadout.

## 8. Scope dependencies, honestly

- **Planting/regeneration is part of the harvest lifecycle but needs no new planting rule:** the cut writes the young stage (`id − 1`) and the *existing* growth sweep regrows it. For the *first-wood* canary regrowth is irrelevant (the first tree is only cut); for any run longer than ~8 minutes, or a sustainable economy, `grow_chunk` and its sweep schedule must be live and the T-F8 timing must hold. Whether anything else plants trees is U-6.
- **Walk search with tile goals on layer 2** is a dependency of the implementation, no longer an unknown of the rule (`docs/re/forestry.md` §2.11): the sim's `search()` takes point goals from building entrances; the forester needs **tile goals**, whose goal sub-cells are the tile's free sub-cells (`0x1ff − tile mask`, goal bit 10 on the cell, callback per goal sub-cell in arrival order). Trees are walk code 2 (259/261 code 4) with masks 0x000/0x010/0x012/0x0ba, enterable on layer 2 like open ground (`L[2] = L[4] = 0xff`, costs 30/41). The first-wood timing still inherits the undecoded finer-route validation (U-1b).
- **Kill/removal paths** that leak a claim in the original (U-5) are adapted away (§2).
- **Rendering/UI** are out of scope; the worker's figure is drawn by the existing figure draw.

## 9. Proposed staging (for the parent; not authorised here)
E2 `FigureType::route` + spawn job/goods + `Raw` delivery with a scripted tree list (unit level). E3 tile goals in the search + claim bit + cut effect (island level). E4 `harvest_tick` end to end with fixtures built from scenario inputs. E5 canary and negative controls. E6 regrowth timing and save/load/hash in a longer run. Each stage ends with the evidence-contract gate and an independent read-only review.
