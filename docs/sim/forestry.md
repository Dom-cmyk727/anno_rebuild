# Simulation: the forester chain (contract)

**Status:** contract for implementation (MAP-ECON-1 **E2.0** correction of the E1-FINAL draft, 2026-11-03, binding review `docs/orchestration/reviews/2026-11-03-roadclose2-forestry.md`). E2.0 corrections, each marked *(E2.0)* where it changes the text: (1) faithful placement and clearing **preserve** claim bit 29 (the `0x3fffffff` mask clears bits 30-31 only; the earlier "placement clears bits 29-31" reading and every example built on it are withdrawn, §3.5, E-16); (2) the first five-tick micro-step is a **saved remainder** consumed once across visits (§3.2, E-21); (3) home-gone and Done states are valid harvest states (§2.1); (4) the cut writes only `object` and `phase`, never the walk columns (§3.2); (5) disjoint ledger terms (§7); (6) E-15 reports unused, not delivery, time. Evidence: `docs/re/forestry.md` (D/E/P labels, addresses). Tags: **D** disassembly (module + address), **E** executed/emulated, **P** parsed from a shipped file, **A** adaptation (ours, not the original), **U** unknown, **NOT_RUN** needs the running original. The stage names E2-E6 are the board's packages (E2 state, E3 lifecycle, E4 production link, E5 canary, E6 gates); they are queued, not released, and nothing here starts them.

## 0. Goal, scope, and what must not be faked

The first authentic wood chain of the opening: **an existing mature tree tile -> the forester's worker (figure 102, job `0x2a`) -> `Farm.raw` -> the existing farm production step -> `HOLZ` in `Farm.out` -> the existing road-connected market cart -> warehouse.** Charcoal burners (research-gated), pre-seeded `raw`/`out`, fixture trees in the real opening, loadout edits, relocation and any "interim return" shortcut are **not** substitutes.

Original progression holds: building 801 has `INFRAMIN "INFRA_EVER"` (811 carries `INFRA_STUFE_3B`) and no `FORSCHTYPE` record (P); reading: no population stage or research gate, so it is placeable at the start (the placement-side gates are the existing build rules; not re-verified here). Cost 150 gold + 2 tools, 0 wood (P), so building it does not consume ship wood. The canary therefore measures *wood the forester made* (§7), not the warehouse total.

Existing machinery reused unchanged: `farm_step` (rate 14, need 32, capacity 1280, `max_figures` 2), the `Farm` payload, `consume(StepResult)` (`workers_out` counter, `SpawnRequest`), the figure registry and its phase B seam protocol (`Effects` merged right after each figure call), `transport` walk/search (`walk_search.cpp`), `grow_chunk`/`grow()`, the market cart, `CityStock`, and **`StoreDelta::Input` for `Farm.raw`** (§3.2). Missing: the job-`0x2a` body, its tile effects, the claim, the dispatch draw and two Rules choices.

## 1. The rule set (what the original does)

All of §2 of `docs/re/forestry.md`; the numbers a test needs:

| Item | Value | Tag |
|---|---|---|
| job id of a `MAEHER` route figure | `0x2a` (the figure type's `ROUTETYPE` index; slot `+0x54`) | D |
| farm step | 500 ticks; consume 32, make 14 at ratio 128; ratio >= 64 or status 1 | D |
| eligible dispatch attempt | step passed gate, switched on, not suspended; `count < max_figures` (2); `384/14 = 27 >= raw/32` (raw after the step). **Output full does not stop it.** One world draw per such attempt, before the send | D |
| send | `0x10072d20`; creates the figure, attaches it to the first free of five worker slots (the counter raise) and stores the job; **runs nothing** | D |
| first job run | after the figure has spent 5 elapsed ticks, counted across its visits (the 0.001/0.001 micro-step, D `0x10035ce0` / `0x10034760`), in the visit where the five ticks complete: search and claim happen there, never in the farm tick; surplus ticks of that visit go on to the body *(E2.0)* | D reading; integer model A |
| search square | `(x + (w-1)/2 - r, y + (h-1)/2 - r)`, `2r + 1 + ((w-1) mod 2)` wide, `r` = 5 -> 11 x 11 for a 3 x 3 forester; u16 words | D, E |
| candidate | tile object whose type `WARE[0]` = cargo goods (`BAUM`, 70) and claim bit clear; row-major | D, E |
| winner | first unclaimed goal popped by the walk search (layer 2, disc radius 5) | D; route order U-1, fine-route veto U-1b |
| claim | tile record second dword bit 29; one bit, no owner; set when the walk exists; cleared by the worker's cut arrival and by its abort, unconditionally. **Placement and clearing preserve it** (`and 0x3fffffff` at `0x100824a7` / `0x1008227b` clears bits 30-31; the clear arm keeps bits 25-31) *(E2.0)* | D, E |
| cut wait | 13.0 -> **6 500 ticks** after arrival | D |
| cut | `id -> id - 1` if type(`id - 1`).PRODTYPE = `ROHSTWACHS` (`0xe2`); `phase &= 0x80`; else nothing | D, E |
| cargo | +256 on a successful cut; capacity 256 -> no repeat for figure 102; **a larger data-defined capacity repeats from step A** | D |
| delivery | `Farm.raw += cargo` (u16 add, no clip) at the end of step C | D |
| failed search | job goes to step C, delivers 0, figure ends | D |
| home gone | abort at the next arrival: claim cleared (unconditionally), no cut | D |
| end | the end dispatch releases the home counter at once (before the last walk) | D |

## 2. State: what is hashed and saved, what is derived

| Datum | Where | Hashed/saved | Why |
|---|---|---|---|
| Farm `raw`, `out`, `workers_out`, `last`, ... | existing `Farm` | yes (unchanged) | the production state |
| the worker figure (`job` = `0x2a`, `cargo_goods` = 70, `cargo_88`, position, route) | existing `Figure` | yes | a figure is state |
| the harvest job step, incl. the first-delay remainder | new `Figure::harvest` (below) | yes (field list, stream bump at E2) | mid-flight state; **created only by the spawn of a Farm-payload home** (see selection rule below) |
| the claim bit | tile record `aux` bit 29 (`kTileClaim = 1u << 29`) | yes (`aux` is in the chunk digest) | the original keeps it in the tile record |
| the claim owner (enhanced only) | `Island::claims`: ordered map tile -> worker `FigureId`; empty unless `forestry_claim_hygiene` (the validator refuses a non-empty map under faithful rules) | yes (saved and hashed as a u64-counted sequence; empty under faithful rules) | an adaptation: the original stores no owner (D). *(E2.0: retained as specified in E1-FINAL; only the faithful placement fact changed.)* |
| the cut tile (`object`, `phase`) | existing tile columns | yes | |
| the two Rules choices | `Rules::forestry_claim_hygiene`, `forestry_skip_uncuttable` | yes (World Rules are hashed) | §3.6 |
| candidate list, search square, walk cells, route cache, **the per-call overlay** | computed per search / per call | **no** (derived, discarded at the end of the call) | recomputed from tiles + types |
| statistics `trees_cut`, `raw_delivered_88`, `wood_made_88`, `wood_sent_88` | `ForestryStats` beside `transport_stats` | **no** (diagnostic, not in the state hash) | the canary reads them; they must not change the hash |

```cpp
enum class HarvestStep : std::uint8_t { Start, ToTree, Cut, ToHome, Done };   // job steps A, B, C (D factory 0x1003b0e0, job 0x2a)
struct HarvestState {                       // Figure::harvest (only for job 0x2a / 0x29)
    HarvestStep step = HarvestStep::Start;
    std::uint32_t start_delay = 5;          // (E2.0) ticks of the first micro-step still owed; 5 at the spawn, 0 forever after
    std::uint32_t wait = 0;                 // ticks left of the 13.0 cut wait (Cut)
    TileXY target{};                        // step A's tile; meaningful in ToTree and Cut only
    std::vector<FixedPos> route;            // as TransportState (walk to the tree, then home)
    std::uint32_t next = 0, along = 0;
};
```
`start_delay` (*E2.0*, binding review): the constructor's micro-step (D `0x10035ce0`: remaining 0.001 at rate 0.001, i.e. 1.0 time unit = 5 ticks at the 0.2 tick scale of `0x10034760`) is modelled as an integer remainder that is **saved**, consumed by the figure's elapsed ticks across as many visits as it takes, and never re-armed; the visit in which it reaches 0 runs `Start` with the surplus ticks (§3.2). The original consumes the float remainder partially per visit in the same way (`0x10034760` steps 4-8: a visit whose `time · rate` stays below the remaining value only lowers it); this is the accepted integer model of that micro-step, not a claim of float identity. Validation (both rule sets): `start_delay <= kHarvestStartDelay` (5); `start_delay > 0` only with `step == Start`, empty route and `cargo_88 == 0`.

**Selection rule (job 0x2a is shared).** Job `0x2a` is also the small farm's worker job (`kJobSmallFarmWorker`, `driver.cpp`; the grain mower, RE §2.2). The body is therefore NOT chosen by the job id alone: `harvest_tick` runs exactly for figures whose `harvest` is present, and `harvest` is created only by the spawn of a `Farm`-payload (farm-family, non-small-farm) home in `consume`. Existing small-farm workers have no `harvest`, are never routed to `harvest_tick`, and load unchanged. Porting the small farm's worker onto the same body is a separate decision. **The Farm home is the spawn origin, not a standing invariant** *(E2.0)*: a worker whose home was demolished keeps a `home` id that no longer resolves (the sim does not clear `Figure::home` at a demolition; `find_building` answers null) and stays valid until its abort transition at its next arrival (§3.2, RE §2.4 step 1); a `Done`/expiring worker has `home` none (its end released the counter, `validate_figures`). So the structural facts are: `harvest` present => `job` is `0x2a`/`0x29`; and *if* `home` resolves to an existing building, its payload is `Farm` (a BuildingId's serial comes from the island's `next_serial` and is never reused, `ids.hpp`). No invariant requires a currently existing Farm home in any harvest step.

`aux` currently refuses bits 8..31 (`validate_island`): only bit 29 is admitted (**A**; the rest stay refused). The original's owner nibble is bits 25-28 of the same dword (`docs/sim/state.md` §9), bit 29 is distinct (D).

### 2.1 Permitted states, separately for the two rule sets

The validators and loaders (`validate_figures`, `validate_island`, `check_invariants`, `World::load`) **never repair authoritative state**: they accept or refuse. Repairs are deterministic runtime transitions, and only the transitions listed in §3.5 repair anything, and only under the rule that selects them.

Structural invariants that hold under **both** rule sets (refused when violated): `harvest` present => `job` is `0x2a`/`0x29`; a `home` that resolves names a `Farm`-payload building (a home that no longer resolves is the valid home-gone state, §2 selection rule); the `start_delay` facts above; `step == Cut` => `wait > 0` (a wait that reaches 0 is resolved in the same call); `target` inside the island of the home id in `ToTree`/`Cut`; `step == Done` => the figure has ended (`expiry_stamp` present, `home` none: the existing rule). *(E2.0: no invariant requires an existing Farm home.)*

**Faithful rules (`forestry_claim_hygiene = false`, the default).** The original stores one bit. Accepted (loadable, never repaired) states: *any* bit-29 pattern on tiles that the loader's tile checks accept; a claimed tile with no worker (a stray bit); a worker in `ToTree`/`Cut` whose target is unclaimed; two workers in `ToTree`/`Cut` with the same target. The validator relates claims to workers in no way - there is **no bijection and no one-way invariant** between them. Which of these states the traced transitions can *reach* is narrower *(E2.0)*: placement and clearing preserve the bit (§3.5), a set bit refuses every later selection, and only the holder's own cut arrival or abort clears it, so the traced transitions never give a held tile to a second worker and never drop a held worker's bit. A stray bit is reachable only through an untraced end path (U-5b, U-11), whose existence this contract does not assert; the unclaimed-target and shared-target states are accepted because the original has no check that refuses them (an imported or older state may hold them), not because a traced path creates them. No transition repairs anything; the sim does not claim that the original leaks on paths it has not traced (`docs/re/forestry.md` §4.3, U-5b).

**Enhanced rules (`forestry_claim_hygiene = true`).** Stable: (E-i) every `claims` entry names a worker that exists, is in `ToTree` or `Cut`, has that tile as its `target`, and the tile's bit 29 is set; (E-ii) every set bit 29 has an entry (no stray bits: the world never creates one, and a loader **refuses** a save that has one); (E-iii) a worker in `ToTree`/`Cut` without an entry is **transitional but valid** (the enhanced invalidation of a placement or a clearing dropped its claim, §3.5; it revalidates at its next visit); (E-iv) at most one entry per tile (a map). Violations of E-i, E-ii and E-iv are refused, never repaired. Transitions: those of §3.5 (enhanced column). A faithful save is not loadable into an enhanced world and vice versa (Rules are hashed and compared). *(E2.0: unchanged from E1-FINAL except that the enhanced invalidation is now the only way a held claim is dropped by a tile change, since faithful placement keeps the bit.)*

### 2.2 Raw-store width

The original's `raw` is a u16 (`+0x1c`); `Farm::raw` is u32. For the evidenced forester the largest reachable value is bounded by its own parameters: sends need `raw/need <= 384/rate`, i.e. `raw <= (384/14)*32 + 31 = 895` at a send, plus `max_figures * capacity = 2 * 256` of deliveries in flight -> **1407**, far from 65 536, so the u16 wrap is unobservable for 801 (**A**, with the arithmetic). This is a property of the **configured type**, not a global limit: the general bound is `B = (floor(384/rate) * need + need - 1) + max_figures * worker_capacity`, evaluated per `ProductionType` and figure type at load; a configuration with `B > 0xFFFF` for its store width needs a recorded decision (model the width as data, or accept the u32 difference) - the engine adds **no** `raw <= 0xFFFF` invariant and clips nothing. `StoreDelta::Input` clamps to 0 .. 0xFFFFFFFF (u32), which equals the u16 add whenever the sum stays below 65 536 (always for 801).

## 3. The commandless lifecycle (scheduler placement)

Nothing here is a player command; everything is driven by the existing per-tick order (`docs/sim/state.md` §5.3-§5.9).

### 3.1 Building tick (city ring, TickKind Farm): dispatch, the draw, the counter

`farm_step` as today (it already returns `send_figure` for the dispatch rule). The changes:
1. **One world draw per eligible dispatch attempt**: every step where `send_figure` is true draws once, **before** the send is resolved (D `0x1006edf3`-`0x1006ee06`), whether or not the send then succeeds. The count is of attempts, not of created workers: a refused send (`SpawnRequest` rejected by `add_figure`, F35 release; entrance prerequisite; flag bits) is still a draw. Switched-off, suspended and gate-closed steps return before the dispatch: no draw. A full output store (status 2) does not prevent an attempt for the two-worker forester. The result of the draw is discarded; only the cursor moves.
2. `send_figure_job(Farm)` returns the type's route job (`FigureType::route`, new field = `ROUTETYPE` index via the name table `AnnoGame 0x10024fb8`, 0 when absent) instead of 0; the spawned figure starts with `cargo_goods` = the building type's `inputs[0]` (D `0x1006ee06`).
3. `consume` raises `workers_out` and queues the spawn as today (the original's attach at `0x1007458c`, inside the same farm tick).

### 3.2 Figure registry phase B: `harvest_tick`, with a local overlay

A new default body `harvest_tick(fig)` beside `transport_tick`, selected by `fig.harvest.has_value()` (job `0x2a`, or `0x29`, the same constructor; **not** by the job id alone, §2). It reads the world and its own figure and writes only its own figure and `Effects`; it draws nothing.

**Why an overlay.** The seam protocol forbids a body from writing the island; its edits travel in `Effects` and are merged **after the whole call** (C2: merged after each figure, which orders *figures*). But the body loops (search -> claim -> walk -> wait -> cut -> repeat) and later transitions of the same call must see the earlier ones. A queued claim or cut that the next search cannot see would let one tree be selected and credited twice. Merge-after-each-figure is therefore **not** a substitute for merge-between-transitions.

**The mechanism (A).** One call is a transaction over *(its figure, a `TileOverlay`)*:
- `TileOverlay` is an ordered map (tile -> `{claim?, owner?, object?, phase?}`; `owner` is used only under `forestry_claim_hygiene`, and each `tile_edits` entry carries the acting `FigureId` so the enhanced `Island::claims` edits are represented), created at the start of the call, never saved or hashed, discarded at its end.
- Every read the body makes of a tile (`claimed(t)`, the candidate predicate, the cut predicate) consults the overlay first and the island second. Every write (claim, unclaim, cut) is applied to the overlay **and appended in order** to `Effects::tile_edits`.
- At the end of the call the world merges `tile_edits` in order (and the rest of the Effects, §3.4 for the delivery). Because phase B is serial and nothing else writes between the start of the body and its merge (no other figure, island tick or command runs in between), the view through the overlay at every point **equals** what immediate writes would have shown; the overlay is exactly the original's "every write is immediate" for this one figure.
- The merge re-applies the cut predicate to each `Cut` edit. In Debug and sanitizer builds a disagreement is a defect counter that must stay 0 (it cannot happen by the argument above); it is not repaired and not reachable in a correct build.
- **What the overlay does not cover, and why that is exact *(E2.0, corrected)*.** The walk search reads walkability from the island's **tile record columns** `walk_code` and `walk_mask` (and the item-height byte of `aux`), written when an object is *placed* (`write_walk_record`, from the placed type's immutable facts `HEAD[0x2f..0x30]` / `WALKMASK`) and reset by the clear arm. The cut (D `0x10087340`, re-read for E2.0: `mov word [edi], ax` at `0x100873a8`, `and byte [edi+3], 0x80` at `0x100873ab`; its notification island slot `+0x188` = `0x1008f2d0` case `0xcf` -> `0x1008f2f0` only increments byte `+5` of the 16-byte chunk records at island `+0x94` over the rectangle, `0x1008f41e`) writes only the object id (u16 `+0`) and the phase byte (`+3`, `& 0x80`); it does **not** write the second dword's walk code (bits 8-15) or mask (bits 16-24), and it does not look up the walk facts of `id - 1`. So after a cut the tile keeps the mature tree's code and mask (e.g. 113 keeps its large-tree mask `0x0ba` as 112, 259 keeps code 4 as 258) until something re-places or clears the tile. The growth writer (`grow_chunk`) likewise writes only `object` and `phase`. The overlay therefore covers exactly the fields the body changes (claim bit, object, phase), and the walker reading the island's walk columns sees precisely what the original's immediate writes would show: those columns are never edited during the call. The rebuild must **not** recompute walkability from the predecessor's (or successor's) type at a cut or a growth flip. The table of RE §2.11 lists the *type-derived* codes/masks (what a placement would write), not a claim about the record after a cut. The finer sub-cell route validation (U-1b) remains undecoded and inherited (§3.8).
- No rollback is needed: a body never fails midway; a refused transition is a normal state change.

**Credit and ticks.** Cargo is credited **only** by a planned cut that the overlay view accepted (`cut_ok`): `cargo_88 += 256` follows `cut_ok`, never the other way. Elapsed ticks are a variable carried through the whole call: every transition subtracts what it consumes (the first-delay remainder, walk time, wait) and passes the rest on; `Start` consumes none; a call ends only when the first delay, a walk or a wait is not finished (state saved with its remainder) or the figure is `Done`. **No elapsed tick is discarded.**

**The first delay *(E2.0, binding)*.** `harvest.start_delay` starts at 5 (spawn) and is consumed by elapsed ticks exactly once in the figure's life: a visit with `ticks < start_delay` lowers it by `ticks` and does nothing else; a visit with `ticks == start_delay` takes it to 0 and runs `Start` in the same visit with 0 surplus; a visit with more subtracts it, sets it to 0 and carries the surplus into `Start` and onwards. A `ticks == 0` visit returns before anything (D `0x10034760` step 3; the stamp is unchanged, which in u16 arithmetic equals writing `u16(t)`). Re-entering `Start` after a cut (capacity > cargo) never re-arms it. Consequences that the vectors pin (E-21): five 1-tick visits and one 5-tick visit leave the same figure and tile state; a save after 2 ticks and a reload resume with 3 owed; a 50-tick first visit walks 45 ticks. This is a per-figure transition rule; it does not claim the whole world is invariant under a different partition of the same advances (other figures, the farm step and the growth sweep have their own gates).

```
harvest_tick(fig):                                    // D 0x1005b6c0/0x1005c260 steps A, B, C
    ft = figure type;  ticks = u16(u16(t) - fig.stamp)
    if fig.expiry_stamp: fig.stamp = u16(t); return true         // ended, waiting for phase C (as transport_tick)
    if ticks == 0: return true                                    // D 0x10034760 step 3: no elapsed time, no work
    fig.stamp = u16(t)
    h = fig.harvest
    if h.start_delay > 0:                                         // (E2.0) the saved first micro-step remainder
        used = min(h.start_delay, ticks); h.start_delay -= used; ticks -= used
        if h.start_delay > 0: return true                         // still owed: nothing else this visit
    ov = TileOverlay{}
    [enhanced] if step in (ToTree, Cut) and not valid(ov, target, fig): drop(ov, target, fig); step = Start   // revalidation at each visit, before walking on
    loop (each pass waits, moves to a later step, or loops once per completed cut):
      Start:    if no home: start_home()  (no home -> Done)            // nothing claimed in this step
                goals  = candidates(island+ov, square(home, r), cargo_goods, skip_uncuttable)
                if goals empty: start_home(); continue                  // step A fails -> step C (D 0x1005e280)
                path = walk_search(layer, fig.pos, goals, accept = first goal with claimed(ov) == false)
                if none: start_home(); continue
                target = path.goal; claim(ov, target, fig); step = ToTree        // sets the bit now (enhanced: and claims[target] = fig), in the overlay
      ToTree:   walk(ticks); if not arrived: return
                if no home: abort(ov, target, fig); step = Done                   // D 0x1005c260 step 1 -> 0x100545c0
                elif [enhanced] not valid(ov, target, fig): drop(ov, target, fig); step = Start; continue
                else wait = ft.load_ticks (6500); step = Cut
      Cut:      if ticks < wait: wait -= ticks; return;  ticks -= wait; wait = 0
                if no home: abort(ov, target, fig); step = Done                   // the abort instead of the arrival
                elif [enhanced] not valid(ov, target, fig): drop(ov, target, fig); step = Start; continue
                else unclaim(ov, target, fig)                                     // D 0x10054530: clear, then 0x10087340
                     cut_ok = cut(ov, target)                                     // reads/writes the overlay; Effects::tile_edits += Cut
                     if cut_ok: fig.cargo_88 += 256
                     if no_progress_guard(...)  start_home()                      // A: only data with load_ticks == 0 and a failed cut: see below
                     elif ft.capacity_88 > fig.cargo_88: step = Start; continue   // D 0x1005c260 type 0xb; start_delay stays 0
                     else start_home()
      ToHome:   walk(ticks); if not arrived: return
                if home: Effects.store_deltas += {home, Input, index 0, +fig.cargo_88}   // D 0x1006ee70: Farm.raw += cargo
                step = Done
      Done:     [enhanced: release claims[target] if it is still fig's]  end_figure(fig)   // existing: release the home counter, expiry list
    end of call: merge Effects (tile_edits in issue order, then the rest)

abort(ov, T, W):   faithful: clear bit 29 at T (unconditional, D 0x100545c0);  enhanced: clear bit 29 and claims[T] only if claims[T] == W, else nothing
unclaim(ov, T, W): faithful: clear bit 29 at T (unconditional, D 0x10054530);  enhanced: as abort (reached only after valid() held, so W owns T)
valid(ov, T, W):   enhanced only: claims[T] == W (through the overlay) and T still passes the candidate predicate without the claim test
                   (WARE[0] of its object type == cargo goods, and cuttable when skip_uncuttable)
drop(ov, T, W):    enhanced only: forget T; clear bit 29 and claims[T] only if claims[T] == W (never another worker's claim)
```
"No home" means `find_building(fig.home)` is null (the demolished-home state, or `home` none). In `Start` with no home (the home was demolished before the first delay ran out) the worker goes straight to `Done` and writes no tile: the original's job arrival aborts there (`0x1005c260` step 1 -> step A's abort `0x100545c0`), and that abort clears bit 29 at the step's stored tile, which step A has not set yet (its initial value is not traced); the sim's "no write" is an adaptation (**A**), reachable only within the first five ticks of a worker's life. A looped `Start` after a cut always has its home (a missing home aborts at the cut arrival before the loop). Enhanced revalidation (an adaptation) runs at every visit of a worker in `ToTree`/`Cut` before it walks on or waits on, and again at the arrival and at the end of the wait, as specified in E1-FINAL; the faithful body reads nothing during a walk, as the original.
`no_progress_guard`: a failed cut with `load_ticks == 0` would loop without consuming time (the nearest uncuttable tile is picked again). With the shipped wait of 6 500 ticks every iteration consumes the wait, so the loop is bounded by the call's ticks; for modded data with `load_ticks == 0` the figure goes home after a failed cut that elapsed no time (**A**, a data guard; unreachable with the shipped data; documented here because the loop must terminate).

**The delivery** uses the existing `StoreDelta::Input` (`Store::Input`, `index 0`, `delta_88 = +cargo`): `input_store(Farm)` returns `&Farm::raw` for `index == 0` (`driver.cpp::input_store`), the merge clamps to 0 .. 0xFFFFFFFF and does nothing when the building is gone - which matches the original (the unload needs a home: D `0x1006ee70` via step C; no goods test). **No new raw effect kind is needed.** The `Effects` additions are only `tile_edits` (the island's tile columns have no existing effect).

**Tile writes, precisely (D `0x10087340`, `0x10087880`; E `emu_forester.py`).**
- *Claim*: `aux |= kTileClaim`. *Unclaim*: `aux &= ~kTileClaim`. Nothing else in the record changes; the owner nibble and the other `aux` bits are untouched. Enhanced additionally edits `Island::claims` (through the overlay's `owner` and the acting `FigureId` of each edit).
- *Cut*: tile `object` := `object - 1` (the type's own id, equal to `id - 1` because the registry is keyed by ENTRY id: `docs/re/forestry.md` §2.12), tile `phase` := `phase & 0x80` (growth counter and parity cleared, bit 7 kept). **Nothing else**: not `walk_code`, `walk_mask`, `aux`, the foot columns or the owner *(E2.0)*. The chunk digest cache is invalidated through the existing setters (`set_object` also marks the tile route-dirty, as any object write does); the original's notification (`island +0x188`, a chunk-record counter) is not state.
- *Growth* is the existing `grow_chunk`/`grow()` writing `object + 1` and the phase; the cut tile enters it at phase 0 (E-11).

### 3.3 Growth
The island's grow sweep (existing `grow_chunk`, every chunk at most every 2000 ticks) takes the cut tile (object `N - 1`, phase 0) through `S * 2^k` flips back to `N`. No new code; the `ROHSTWACHS` type must be found by `find_growing` (it is, `types.cpp`).

### 3.4 Market cart
Unchanged: once `Farm.out >= 256` (1 t) the home city's market cart considers the forester (goods 42 = `HOLZ`, not in 1-25 -> `score = (avail << 7) / 1280`); its load adds to `CityStock` at the warehouse. Nothing in the chain cares where `out` came from, which is why §7 needs a ledger.

### 3.5 The claim lifecycle: ownership, placement, replacement, termination, cleanup

The original stores **one bit and no owner** (D `0x10087880` mode 0: `shl edx, 0x1d` at `0x100878da`, `and ecx, 0xdfffffff` at `0x100878dd`, on the record's second dword `[rec+4]`), and every clear by a worker writes `0` at its stored tile without asking who set the bit (D `0x100545c0` abort, `0x10054530` cut arrival). **Placement and clearing do not touch bit 29** *(E2.0, binding; re-read against `AnnoWelt.dll` sha256 `f220a878…6ae2`)*: the ordinary placement path loads `ebp = 0x3fffffff` (`0x10082442`) and stores `[rec+4] & ebp` (`0x100824a7`/`0x100824ac`; its other write to that dword, `0x1008248a`, replaces only the low byte), the overlay path does the same (`and ecx, 0x3fffffff`, `0x1008227b`), and `0x3fffffff & (1 << 29) = 0x20000000`: bits 30 and 31 are cleared, **bit 29 is kept**. The clear arm keeps bits 25-31 (`0xfe0002ff`, `0x10082f05`). The sim's writers already match: `place_object_record` replaces only the `aux` low byte (`set_item_height`) and `clear_object_record` does not write `aux`, so **no placement-writer change is needed under the faithful rules**; the enhanced invalidation below is the only, hygiene-gated, addition. "W" is the worker; "T" its target.

| Transition | Faithful (`hygiene = false`) | Enhanced (`hygiene = true`) |
|---|---|---|
| select (Start) | bit 29 set; no owner stored | bit set **and** `claims[T] = W` |
| cut arrival | clears bit 29 at T unconditionally, then the cut (D) | if `claims[T] != W` or T no longer passes the candidate predicate (claim lost / tile changed): drop T (release only W's own entry), no cut, no credit, step Start with the remaining ticks; else clear bit + entry, then the cut |
| abort (home gone at an arrival) | clears bit 29 at T unconditionally (D `0x100545c0`) | clears bit + entry only if `claims[T] == W`; otherwise nothing |
| placement over T | **bit kept** (bits 30-31 cleared, D); W keeps its target and its claim *(E2.0 correction)* | **enhanced invalidation** (A, hygiene only): clears bit and entry; W is now "claim lost" (transitional, E-iii) and revalidates at its next visit (before walking on, before and after the wait) |
| clearing (object removed) over T | **bit kept** (D clear arm) *(E2.0 correction)* | as placement (enhanced invalidation) |
| another worker selects T while W holds it | impossible while the bit is set (candidate filter, D `0x10092320`); since placement and clearing keep the bit, no traced transition frees a held T | possible only after an enhanced invalidation dropped W's claim; the new worker then owns `claims[T]` and W's later visit fails `valid` and drops T without touching the newer claim |
| W ends by any path of this sim (`Done`, abort, expiry, removal) | the traced writes only (cut arrival or abort); nothing else | releases `claims[T]` if still `W`'s, then the existing `end_figure` |
| T replaced by a same-type object between selection and arrival | bit kept; at the cut arrival W clears it and cuts whatever the tile holds if `id - 1` is `ROHSTWACHS` (D; the id is not re-checked: U-12). No other worker can have claimed T meanwhile | the replacement's invalidation dropped `claims[T]`; W's `valid` fails and it drops T; a tile re-claimed by a **newer** worker is never cleared by W, **including same-type replacement** (identity is the worker's `FigureId`, not the object's type) |
| revalidation (enhanced) | none | at each visit, before using T: `claims[T] == W` and T still passes the candidate predicate (`WARE[0]` = goods, and `cuttable` when `skip_uncuttable`); else drop T, no write to others' claims |

*The faithful column is D; its placement/clearing rows are the E2.0 correction. The enhanced column is entirely an adaptation (A), owner-keyed by `FigureId`, retained from E1-FINAL. The earlier draft's faithful "placement drops the claim, a second worker re-takes it, the first worker's late clear erases it" sequence (E-10, E-16, RE §4.3/§4.4) rested on the misread mask and is **withdrawn** as a faithful/original fact; under the enhanced rules a similar sequence exists only because the enhanced invalidation clears the claim, and there the owner check protects the newer worker.*

**Enhanced invalidation, scope.** The binding review permits an enhanced adaptation that clears the claim bit and owner entry when the tile changes, under the false-default `forestry_claim_hygiene` only. It is a write in the placement/removal path that runs **only** when `forestry_claim_hygiene` is true (every footprint tile an object is written to or cleared from loses bit 29 and its `claims` entry); with the default false the placement and clear writers stay exactly the faithful ones (bit 29 kept). The E1-FINAL enhanced semantics are otherwise unchanged.

### 3.6 The two Rules choices (A; false by default; hashed; independent)

| Field | Scope | Faithful (`false`) | Enhanced (`true`) | Intended enhanced value |
|---|---|---|---|---|
| `forestry_claim_hygiene` | tile claims of the harvest jobs (`0x29`/`0x2a`) only; no effect on the bit's meaning for other readers, on the cut, the dispatch or the draw | §3.5 faithful column; §2.1 faithful states; `Island::claims` empty | §3.5 enhanced column (owner map, enhanced invalidation at placement/clearing, revalidation at each visit); §2.1 enhanced states | `true` in an enhanced preset (the owner decides; this document does not switch it on) |
| `forestry_skip_uncuttable` | the candidate list of step A only | candidates = `WARE[0] == goods` and claim clear (D); an uncuttable tile can be chosen again and again (each time a walk + the 6 500-tick wait + a failed cut, then step A again) | additionally `cuttable(id)` = the type `id - 1` exists and its `PRODTYPE` is `ROHSTWACHS` (`0xe2`): **the predicate of the cut itself (D `0x10087340`)**, evaluated through the registry at list time | `true` in an enhanced preset |

There is **no hardcoded tree id**: for the shipped data the registry-derived predicate excludes exactly one of the 46 `BAUM` entries (261, whose `id - 1` is `ROHSTOFF`; 259's `id - 1` is `ROHSTWACHS`, so 259 stays a candidate and becomes a permanent stump), as `tools/forester_tree_table.py` (needs `ANNO1503_GAME_DIR`; output kept in `docs/orchestration/evidence/map-econ1-e1-final-tree-table.txt`) (P) shows; other or modded data decides the same way by its own types. Both choices are new `Rules` bytes (one each, appended after the existing Rules; E2 bumps the stream version to the next free number, not asserted here).

### 3.7 Determinism and the PRNG timeline

Integer only (ticks, 8.8 amounts, Q24.8 positions); candidates row-major; the walk search is the existing deterministic search (FIFO ties); effects merged in figure-registry order; ordered containers only; no wall clock. **No draw** inside the figure body (D: `0x10058ae0`, `0x10092320` mode 0, `0x10087340`; mode 1 of `0x10092320` samples but step A does not use it).

**The dispatch draw needs its own identity.** It is a new `DrawSite` enumerator (name proposal `FarmWorkerDispatch`, phase `Advance`, ported) with its entries in `draw_site_name`, the phase table (`draw_site_phase`/`may_draw`) and `draw_site_ported`; **`kPrngTimelineVersion` bumps (3 -> the next number) because a ported site is a "recorded bump, never a silent desync"** (`world.hpp`), the replay header and `prng_timeline_missing()` follow, and `tools/sim_stream.py` is updated. Every farm-family fixture and golden that has a farm with a figure type moves by the extra draws; the explanation is per fixture and field, not a blind regeneration. A stream-layout version bump alone is not enough, and the timeline bump alone does not cover the new Rules bytes or `Figure::harvest`: both move (E2). The fur trapper's own tick is not read for the same draw (U-13); the small farm's `SmallFarmAnimal` draw is a different site.

Save/load mid-trip must reproduce the same hash (T-F9).

**Limits (CLAUDE.md "no hardcoded limits").** The original's 5 worker slots and 1024-pair candidate buffer, the 11 x 11 square and 2 workers are data (`HEAD[0x11]`, `max_figures`) or unbounded here; the u16 wrap of the search square is kept as the faithful default (vector `area 3 3 3 3 5`) with the signed variant as an option only if a map ever places islands at world x < r (A).

### 3.8 The route: the inherited tile-level walker and what it does not prove (B5)

The first slice reuses the existing deterministic tile-level walker. The omitted finer-route validation/retry (U-1b, `docs/re/forestry.md` §2.11) **can change which goal is accepted and selected**, not only how long the trip takes: the original may veto a route the tile-level search found and continue with another goal or fail step A. `complete = true` from the walker therefore proves that the tile-level search found a route; it does **not** prove the omitted stage would have passed. This is recorded as an inherited limitation; first-wood timing and the selection of trees in dense areas inherit it. No new fine pathfinding is part of this contract.

## 4. Plug-in points (exact)

| Existing | Change |
|---|---|
| `types.cpp` `figure_type_from_entry` | add `route` (ROUTETYPE index through the table at `AnnoGame 0x10024fb8`, 80 entries read from the DLL at load, or an equivalent config table; never a compiled-in list: moddable) |
| `driver.cpp` `send_figure_job`, `spawn_request`, `consume` | job from the first figure type's `route` for the farm family; cargo goods `inputs[0]`; the dispatch draw per eligible attempt (§3.1, §3.7) |
| `draw.hpp` | new `DrawSite`, name/phase/ported entries |
| `world.hpp` | `kPrngTimelineVersion`, `kStateHashVersion` (next free) |
| `config.hpp` `Rules` | two bytes (§3.6) |
| `figure.hpp` `Figure` | `std::optional<HarvestState> harvest` incl. `start_delay` (field count + stream bump) |
| `tick.hpp` `Effects` | `tile_edits` (ordered list: claim, unclaim, cut) - **no new store kind**: the delivery is `StoreDelta::Input` |
| `scheduler.cpp` merge | apply `tile_edits` in order after `store_deltas`; invalidates chunk digests |
| `island.hpp` | `kTileClaim` in `aux` + `claimed(idx)`; validation admits only that bit among bits 8..31; enhanced `claims` map (saved and hashed, empty under faithful rules) |
| placement / clear writer | faithful: **unchanged** *(E2.0)*: it already keeps `aux` bits 8..31, the original's bit-29 preservation; enhanced only (`forestry_claim_hygiene`): clear `kTileClaim` and the `claims` entry of every footprint tile written or cleared |
| `World` validation | faithful: `claims` empty; enhanced: E-i, E-ii, E-iv (§2.1); refuse, never repair |
| `transport.cpp` / new `harvest.cpp` | `harvest_tick`; reuse `search()`/`walk()`; tile goals (U-1) |
| `scheduler.cpp` | default figure body: `harvest.has_value()` -> `harvest_tick`, else existing (small-farm workers keep their handling) |
| tests | below |

## 5. Hand-derived examples (derived on paper before `forester_ref.py` ran; reproduced by it; E-13..E-20 are the E1-FINAL additions)

Notation: 8.8 amounts (256 = 1 t); `rate0 = 14`, `need0 = 32`, `cap0 = 1280`, `max_figures = 2`.

**E-1 conversion.** `q0 = 256/18 = 14.222…`; `rate0 = trunc(14.222 + 0.5) = 14`; `need0 = trunc(14.222 · 2.25) = trunc(32) = 32`. Control: 811 `T0 = 7`, `I0 = 2.1875` → 37 / 80; 824 → 26 / 25 (documented values).

**E-2 dispatch.** raw 0, out 0, workers 0: ratio `(0<<7)/32 = 0 < 64` → status 1, nothing made; `max_figures 2 > 1` so dispatch: `0 < 2`, `384/14 = 27 ≥ 0/32 = 0` → send. Next step with 1 out: send again. With 2 out: no send. So two workers leave on two consecutive steps (500 ticks apart) if no tree has been delivered.

**E-3 the 895 / 896 boundary.** raw 927: ratio `(927·128)/32 = 3708 → 128`; consume `(32·128)>>7 = 32` → raw 895; made `(14·128)>>7 = 14`; dispatch `895/32 = 27 ≤ 27` → send. raw 928 → 896 → `896/32 = 28 > 27` → **no send**.

**E-4 partial ratio.** raw 16: ratio 64 (not < 64): consume `(32·64)>>7 = 16` → 0, made `(14·64)>>7 = 7`. raw 15: ratio 60 < 64: status 1, nothing. raw 20: ratio 80, consume 20, made `1120>>7 = 8`. raw 31: ratio 124, consume 31, made `1736>>7 = 13`.

**E-5 full output.** out 1280 (≥ cap): status 2, nothing made, raw kept, **still an eligible dispatch attempt (one world draw; `max_figures` 2 never takes the single-worker skip)**. out 1279: produces, out becomes 1293 (no clamp): the cap test is `cap ≤ out` before adding.

**E-6 conversion per trip.** One trip = 256 raw = 8 full steps (8 · 32 = 256) → 8 · 14 = **112** = 0.4375 t `HOLZ`. raw 250: 7 full steps (224 raw, 98 wood), raw 26 → ratio `(26·128)/32 = 104` → consume 26, made `(14·104)>>7 = 11` → 109 wood, raw 0. 2.2857 trips make 1 t of wood; the 5 t output cap equals 11.4 trips.

**E-7 search square.** Forester 3 × 3 at (100, 200), r 5: `x0 = 100 + 1 − 5 = 96`, `y0 = 196`, 11 × 11 → tiles 96..106 × 196..206. A 2 × 2 building at (10, 10): `x0 = 10 + 0 − 5 = 5`, width `11 + 1 = 12`. At (3, 3) 3 × 3: `x0 = −1` → u16 65535 → no candidates.

**E-8 candidates.** Island (0, 0, 8, 8), square (1, 1, 4, 4): tiles (1,1) 105 free → yes; (2,1) 104 young → no (`WARE` unused); (3,1) 105 claimed → no; (4,1) 105 → yes; (1,2) 259 → yes (listed even though it will not regrow); (2,3) 261 → yes (listed although the cut will fail); (3,3) 713 grain → no; (4,4) 107 → yes. Order: (1,1) (4,1) (1,2) (2,3) (4,4).

**E-9 the cut.** 105 phase `0x45` → 104 phase `0x00`; phase `0xC5` → `0x80`; 259 → 258; 261 unchanged, answer 0; id 0 unchanged.

**E-10 contention.** Two trees T1 (cost 3), T2 (cost 4). Worker 1 is visited first, chooses T1 and sets its claim (overlay, then merged right after its call); worker 2's list excludes T1 and chooses T2. If T2 is also claimed the list is empty -> step A fails -> worker 2 delivers 0 and ends. A placement between two selections does **not** release T1: the faithful bit survives placement and clearing (E-16, §3.5). *(E2.0: the earlier sentence that placement lets a second worker re-take the tile is withdrawn.)*

**E-11 regrowth.** Tile cut at t = 100 000, phase 0, `P/2 = 12 000`, 20 flips, visits every 2 000 ticks: block `100000/12000 = 8` (even, parity 0 = stored 0, no flip); first flip at the first visit in block 9 = **108 000**; flip n at 108 000 + 12 000 (n − 1); n = 20 at **336 000** → object 105 again, 236 000 ticks (472 s) after the cut. Visits at 109 000 + 2 000 m: flip 1 at 109 000, flip 2 at 121 000 (first visit ≥ 120 000), …, flip 20 at **337 000**. Nominal `T0` is 480 s; the cut resets the counter, so the wait is about 456–484 s (block alignment plus up to 2 000 ticks of visit delay).

**E-12 one trip's clock (symbolic).** Spawn at tick `t_s` (farm step); the figure's first phase-B visit starts step A (search, claim); walk `d₁` ticks (route length / `speed_q24`, ≈ 500 ticks per tile for figure 102); wait 6 500; at `t_s + a + d₁ + 6500` the tile becomes 104, the claim clears, cargo 256; walk home `d₂`; `raw += 256` at `t_s + a + d₁ + 6500 + d₂`. For a tree 4 tiles away: `d₁ ≈ d₂ ≈ 2000` → about 10 500 ticks (21 s) per trip; 2 workers → 2 trips per 21 s = 224 wood per 21 s ≈ 2.5 t/min, below the farm's 3.28 t/min ceiling (14 per 500 ticks): the forester is **worker-limited**. First delivery of wood to a warehouse ≈ trip (21 s) + 19 production steps for the first tonne (the first worker's 256 raw gives only 112 wood, so the first tonne needs about 2.3 trips ≈ 25–40 s) + the cart's walk and its 500-tick wait: about 1 to 1.5 minutes of game time (adaptation estimate, **not** a measurement; U-1 and U-2 move it by seconds).


**E-13 drain terminals (`drain raw -> raw_left made`).** Cap 1280, rate 14, need 32, ratio 128 while `raw >= 32`. Step k starts with `out = 14 (k-1)`; it produces while `14 (k-1) < 1280`, i.e. `k - 1 <= 91` (14 * 91 = 1274), so steps 1..92 produce and `out = 14 * 92 = 1288`; step 93 sees `cap <= out` -> status 2, nothing changes. `drain 4096`: raw `4096 - 92 * 32 = 1152`, made 1288 -> **`1152 1288`**. `drain 2944` (= 92 * 32): raw 0 and out 1288 together; the full-output test precedes the ratio, status 2 -> `0 1288`. `drain 2976`: `32 1288`. `drain 2912` (= 91 * 32): made `91 * 14 = 1274 < 1280`, step 92 has raw 0, ratio 0 < 64, status 1 -> `0 1274`. The loop in `forester_ref.py` ends at any step that makes nothing (status 1 or 2); the step arithmetic is unchanged.

**E-14 one visit, two trees, capacity 768 (data-defined, > 256), several harvests (`visit` vectors).** Positions on a line, home 0, 500 ticks per position, wait 6 500, capacity 768 = 3 t; trees at 3 and 7 (both mature, unclaimed, cuttable). One call with 40 000 elapsed ticks:

| t (ticks into the call) | transition | overlay view / edits | cargo |
|---|---|---|---|
| 0 | Start: candidates {3, 7}, nearest 3 | edit `claim 3`; overlay: 3 claimed | 0 |
| 1 500 | arrive at 3; wait 6 500 | | 0 |
| 8 000 | Cut: `unclaim 3`, `cut 3` (planned cut accepted: overlay shows 3 alive) | overlay: 3 dead, unclaimed | **256** |
| 8 000 | `768 > 256` -> Start: candidates computed **through the overlay**: 3 is dead -> {7}; nearest 7 | edit `claim 7` | 256 |
| 10 000 | arrive at 7 (4 positions = 2 000 ticks); wait | | 256 |
| 16 500 | Cut 7 | edits `unclaim 7`, `cut 7`; overlay: 7 dead | **512** |
| 16 500 | `768 > 512` -> Start: candidates {} -> step A fails -> step C | | 512 |
| 20 000 | home after 7 * 500 = 3 500 ticks: `StoreDelta::Input {home, index 0, +512}`; Done, `end_figure` | | delivered 512 |

Result: **cargo 512 = exactly two successful planned consumptions**; 20 000 of 40 000 ticks unused (the figure ends); the ordered edits `claim 3, unclaim 3, cut 3, claim 7, unclaim 7, cut 7` are merged after the call: tile 3 and 7 `object - 1`, phase `& 0x80`, no claim; `Farm.raw += 512` once. With 12 000 ticks the call stops at tick 12 000 in `Cut` at tree 7 with `wait = 6 500 - (12 000 - 10 000) = 4 500`, cargo 256, tree 7 claimed (the merge applies `claim 3, unclaim 3, cut 3, claim 7`); with 3 000 ticks: `Cut` at tree 3, `wait 5 000`, nothing cut; with 18 000 ticks: `ToHome` with 2 000 of the 3 500 ticks left, cargo 512 (the carrying state used by the save/reload controls). Capacity 256 (figure 102): `Done` at 9 500, one tree.

**E-15 the defect the overlay removes (`visitstale`).** If the claim/cut edits were only queued to the end of the call, the second search reads the island: tree 3 is still alive and unclaimed at distance 0 -> selected again, the planned cut (`island` view) succeeds again: cuts at elapsed ticks 8 000, 14 500 and 21 000 of the call, cargo **768 from one tree**; the walk home ends and the delivery happens at elapsed tick **22 500** of the call, so the result's last number, **17 500**, is the call's *unused* time (40 000 − 22 500), not a delivery time *(E2.0 wording)*. The reference reproduces that wrong answer (`visitstale 768 6500 40000 | 3 7 -> 768 Done 17500 - 3,3,3`) so the check shows which implementation it rejects.

**E-16 claims under both rule sets (`claims` vectors, rewritten in E2.0).** Ops on one tile: `W+` W selects and claims (refused while the bit is set: the candidate filter), `W-` W's own cut arrival or abort (faithful: an unconditional clear; enhanced: W is the owner or the op is refused), `P` a placement over the tile, `C` a clearing of the tile (faithful: the bit is kept; enhanced: the invalidation clears bit and entry). Results `bit owner` (owner `-` in faithful mode, which stores none):
- **The discriminating case:** faithful `A+ P -> 1 -`: placement keeps the claim (the misread mask would give `0`). Faithful `A+ C -> 1 -`: clearing keeps it. Faithful `A+ A- -> 0 -`: the legal worker unclaim clears it. A temporary reference that restores the wrong placement clear fails the first and passes the third (§6 T-F14, and the contract test).
- Faithful `A+ P B+ A- -> 0 -` ends at 0 under **both** mask readings, and faithful `A+ P B+ -> 1 -` cannot show that B was refused (faithful prints no owner), so neither discriminates on its own (regressions only); the short `A+ P` vector is the discriminating one.
- Enhanced (owner `FigureId`, enhanced invalidation at placement/clearing, as specified in E1-FINAL): `A+ P -> 0 -` and `A+ C -> 0 -` (the invalidation drops bit and entry); `A+ P B+ A- -> 1 B` (B claims after the invalidation; A's late clear is refused because `claims[T] = B`); `A+ B- -> 1 A` (a non-owner's clear is refused); `A+ A- B+ -> 1 B`; `A+ P B+ B- A+ -> 1 A` (after B releases its own claim, A may claim again).
- Faithful `A+ B- -> 0 -`: the one-bit model clears unconditionally. A second worker at the same tile is not reachable through traced transitions (§2.1); the op stands for an accepted loaded state.

**E-21 the first delay (`visits` vectors, E2.0; derived by hand before the reference ran).** One worker, home and start at position 0, one tree at 3, capacity 256, wait 6 500, 500 ticks per position, `start_delay` 5; a list of visits with their elapsed ticks; `S` = save and reload between two visits. Output `start_delay cargo step owed claimed cuts` (`owed` = ticks still owed by the current walk or wait; 0 while the first delay is not over).
- `| 1 1 1 1 1` -> the fifth visit takes the remainder to 0 with no surplus: `Start` runs, claim 3, walk 1 500 owed -> `0 0 ToTree 1500 3 -`. `| 5` -> the same. (The rule the review rejected, "a visit below 5 ticks burns its ticks and the next visit owes 5 again", gives `5 0 Start 0 - -` after five 1-tick visits: never started.)
- `| 0` -> nothing: `5 0 Start 0 - -`; `| 2 0` -> `3 0 Start 0 - -`.
- `| 50` -> 5 for the delay, 45 walked: `0 0 ToTree 1455 3 -`; `| 3 50` -> 2 for the rest of the delay, 48 walked: `0 0 ToTree 1452 3 -`.
- `| 2 S 3` -> `0 0 ToTree 1500 3 -` (the remainder 3 survives the save); `| 2 S 48` -> `0 0 ToTree 1455 3 -`, equal to `| 50`. A reload that re-arms 5 would give `| 2 S 3 -> 2 0 Start 0 - -`.
- Not re-armed after a cut: capacity 768, trees 3 and 7, `| 50 9000`: delay 5; claim 3 at 5; arrive 1 505; cut at 8 005 (cargo 256); `Start` again with no delay: claim 7, walk 2 000 to 10 005; the visits end at 9 050 -> `0 256 ToTree 955 7 3`. Re-arming at `Start` would give 960.
- E-14 shifted by the delay: `| 40000` with capacity 768 -> `0 512 Done 19995 - 3,7` (19 995 unused).

**E-17 draws (`draws` vector).** Steps `(raw, out, workers)`: (0,0,0) send; (0,0,1) send; (0,0,2) two out -> no; (928,0,0) `896/32 = 28 > 27` -> no; (64,1280,0) status 2 yet `64/32 = 2 <= 27` and `0 < 2` -> send. **3 eligible attempts = 3 draws**; the full-output step is one of them. A refused send would still count.

**E-18 the enhanced candidate list (`candskip` vector).** E-8's island with the registry types of the vector file: (1,1) 105 (104 is `ROHSTWACHS`), (4,1) 105, (1,2) 259 (258 `ROHSTWACHS`: cuttable), (4,4) 107 (106 `ROHSTWACHS`) stay; (2,3) 261 (260 `ROHSTOFF`) drops out. Order (1,1) (4,1) (1,2) (4,4). No literal 261 appears in the rule.

**E-19 the tree transitions N -> N-1 of every eligible type (P, `tools/forester_tree_table.py` (needs `ANNO1503_GAME_DIR`; output kept in `docs/orchestration/evidence/map-econ1-e1-final-tree-table.txt`), derived from `Anlagen.dat` by the rule `WARE[0] = BAUM`, then `PRODTYPE(id - 1) = ROHSTWACHS`).** 46 entries: 105, 107, 109, 111, 113, 115, 117, 119; 202, 204, ..., 238 (19); 241, 243, 245, 247, 251, 255, 257, 259, 261; 405, 407, ..., 423 (10). Cut transitions (45 types): 105>104, 107>106, 109>108, 111>110, 113>112, 115>114, 117>116, 119>118, 202>201, 204>203, ..., 238>237, 241>240, 243>242, 245>244, 247>246, 251>250, 255>254, 257>256, 259>258, 405>404, ..., 423>422. Of those, 44 regrow (the young type has `S = 5` in `tools/forester_tree_table.py` (needs `ANNO1503_GAME_DIR`; output kept in `docs/orchestration/evidence/map-econ1-e1-final-tree-table.txt`); `k = 2`, `T0` = 480 s and the 20 flips of 12 000 ticks are the earlier P result of RE §1.3, not re-derived by `tree_table.py`) and **259>258 is a stump** (`S = 1`, never regrows). **261 is not a cut transition** (260 is `ROHSTOFF`): faithful candidate, uncuttable. The canary's tile ledger is built from this registry-derived table (a skipped corpus test, e.g. without `ANNO1503_GAME_DIR`, is NOT a pass of this check; the test derives it from the loaded registry with the same rule and checks that the shipped data yields 46 / 45 / 44 / 1 stump / 1 uncuttable; the numbers are a P check, not engine behaviour), not from 105 -> 104 alone.

**E-20 a treeless forester's counter and draws.** Farm step n (tick `t_n`): attempt, **draw**, send; `workers_out` 0 -> 1 (the original attaches at once, D `0x1007458c`). Same advance: the figure's visit has 0 elapsed ticks -> nothing. Next advance (example: `t_n + 50`; any advance whose elapsed ticks complete the 5-tick first delay, E-21): first visit, 50 >= 5 ticks: Start -> no goals -> step C (inside the footprint: no walk) -> deliver 0 -> Done -> `end_figure` -> release (D: at the end dispatch, before the last walk) -> `workers_out` 1 -> 0. Farm step n+1 (`t_n + 500`): `0 < 2`, rate test passes -> attempt, **draw**. Every step is an attempt: 10 steps, 10 draws, a search each. With trees and two workers out the third step fails the count test before the draw: no draw until a worker's end dispatch releases a slot.

## 6. Test plan and acceptance table (evidence contract)

| Requirement | Positive proof | Counterexample / negative control | Evidence |
|---|---|---|---|
| T-F1 type facts of 801/102 | corpus test: registry gives rate 14, need 32, cap 1280, max 2, ids [102], r 5, capacity 256, layer 2, wait 6 500, route 0x2a; INFRAMIN not research-gated | mutate `PROD3[0x29]` 2->1 in a copy: max_figures 1 and the single-worker skip appears | vectors `conv`, corpus test (requires `ANNO1503_GAME_DIR`; classify the skip) |
| T-F2 dispatch rule | `forester_ref.py step` vectors against `farm_step` | raw 928 must not send | vectors E-2..E-5 |
| T-F3 eligible set | island with trees: young, claimed, other goods and corner trees outside the disc are listed | a claimed tile appears in the list -> fails | vector `cand`; unit test with a hand-built island |
| T-F4 cut | tile 105->104 phase 0, claim cleared, +256 cargo | 261 changes nothing and adds no cargo; home removed before the arrival cuts nothing | vector `harvest`; scheduler test |
| T-F5 delivery | `Farm.raw` +256 exactly once per completed trip, through `StoreDelta::Input` | no-tree forester: raw +0, workers_out returns to 0 each trip | scheduler test |
| T-F6 conversion | one delivery then idle steps: out +112, raw 0 | raw 250 gives 109, not 112 | vector `drain` |
| T-F7 contention | two workers choose two different trees; with one tree the second worker ends empty | both choose the same tile -> fails | scheduler test |
| T-F8 regrowth | cut tile ripens at the E-11 tick when stepped with explicit visit times | an off-by-one flip moves it by 12 000 | vector `grow` + island tick test |
| T-F9 save/load/hash | save mid-`ToTree`, mid-`Cut`, **carrying (`ToHome`)**, and **inside the first delay (after 2 ticks)**, load, run on: hash equals the uninterrupted run | perturbing the claim bit, `wait` or `start_delay` changes the hash; a loader that re-arms `start_delay` diverges | `World::save/load` test |
| T-F10 home removal | worker out, building removed: the worker stays loadable (home id unresolved) until its next arrival; then claim cleared, tile untouched, figure ends without releasing again; a save in that home-gone state reloads byte-identically | the tile is cut after removal -> fails; a validator that requires an existing Farm home refuses the home-gone save -> fails | scheduler + load test |
| T-F11 chain to the warehouse | forester + road + warehouse market: `CityStock[HOLZ]` rises from forester wood | see §7 | canary |
| T-F12 determinism | ASan/UBSan, two runs, **draws == eligible attempts** and none in the figure body | a draw only per successful spawn fails when a send is refused | existing sanitizer jobs |
| T-F13 overlay | E-14: capacity 768, two trees, 40 000 ticks: cargo 512, edits in order, second search excludes the first tree; 12 000 / 3 000 / 18 000 ticks give the E-14 states | a body that reads the island only (E-15) credits 768 from one tree -> must fail | scheduler test + vectors `visit`, `visitstale` |
| T-F14 claims | E-16 under both rule sets: faithful claim -> placement keeps the bit, clearing keeps it, the worker's own cut/abort clears it; faithful placement then a second worker's selection is refused; enhanced: invalidation at placement/clearing, an old job never clears a newer worker's claim incl. same-type replacement | *(E2.0)* a placement writer or reference that clears bit 29 fails `A+ P -> 1` while `A+ A- -> 0` still passes; a non-owner clear in enhanced mode fails | unit test through the real placement path + vectors `claims` + contract mutant |
| T-F15 validators never repair | a faithful save with a stray bit / an unclaimed target / two same-target workers loads and re-saves byte-identical; an enhanced save with a stray bit, an entry without its worker, or a non-empty `claims` map under faithful rules is **refused**; an enhanced worker without an entry (E-iii) loads | a loader that clears the bit changes the hash/bytes -> fails | load tests |
| T-F16 Rules choices | the four combinations are saved, loaded, hashed; defaults false give the faithful behaviour; `skip` removes only the registry-derived uncuttable tiles | both choices flipped by a mutant change the E-18 list | unit tests + vector `candskip` |
| T-F17 first job / release timing | spawn in the farm tick does not search; the first search in the visit where 5 elapsed ticks complete; release inside the end call; E-20's counter and draw sequence | a body that searches inside the spawn changes the claim order -> fails | scheduler test |
| T-F19 first delay through the scheduler *(E2.0)* | E-21 through the actual `World::advance`: 1-tick advances (the clamped phase-B count visits a lone worker every advance), one 5-tick advance, a 0-tick advance, a 50-tick advance, save/reload after 2 ticks; the isolated worker/tile state matches E-21 | the rejected "burn and re-owe" rule leaves the worker in `Start` forever under 1-tick advances; a re-armed delay after a cut moves the second arrival by 5 | scheduler test + vectors `visits` |
| T-F18 public-command reference | `forester_ref_contract_test.py` (empty / type-only input fail, full-output drain completes and matches, wrong expectation fails, bundled vectors all executed, count >= 75; *(E2.0)* the wrong placement-clear and the re-armed/burned first-delay mutants of the reference fail their discriminating vectors while the legal controls pass) | the unfixed reference (zero vectors exit 0, drain never ends) fails the contract | CTest registration by the parent |

## 7. The canary: new wood versus ship wood (E5-aligned)

**One expensive opening.** The real canary is the board's E5: **one** unchanged-candidate public-command opening (sail, found, unload, build a legal forester and connecting roads, harvest existing eligible trees, deliver new wood), existing trees, original loadout, one seat; no relocation of the real ship, no fixture trees, no seeded `Farm.raw`/`out`/warehouse stock, no grants. Search for sites on copies. Do not multiply full sailing variants.

**Small declared fixtures carry the rest** (E3/E4/E5 focused cases): (i) the tree/no-tree differential - the same small scenario twice, (A) mature trees inside the square, (B) every `BAUM` tile in the square young or removed (a declared scenario input, not seeding): `Δ_A - Δ_B > 0` with identical commands and spending; (ii) additional workers/owners; (iii) the save/reload shapes below; (iv) the overlay and claim cases of §6. None of them is a second sailing opening.

**Starting wood derived from the current ledger, not from a HUD example.** From the repository's manifest (`src/sim/tests/founding_assets_test.cpp`, `docs/sim/ship-founding.md` §13.3, `docs/sim/ship-cargo.md` B1/B14, `src/sim/tests/unload_test.cpp`): a seat's ship holds wood 12 800 (50 t, hold capacity `cap == 12800`); founding the city (type 1302-class, cost 12 t wood = 3 072, 5 t tools = 1 280, 350 gold) takes 3 072 wood from the wood hold -> **9 728 (38 t)**; the unload command moves the hold into the empty city store -> city wood **9 728** (`unload_test.cpp: stock[kWood] == 9728`), the other holds (food 2 560, tools 11 520 and 7 680 in `unload_test.cpp`; the tools and food stores follow `ship-cargo.md` B14) are listed by the same ledger and are not forestry inputs. The canary computes every term from the recorded manifest and command results (the `combined()` conservation pattern) rather than from literals; the "50 t" of an old HUD example is the hold before founding and is **not** the warehouse start. Roads and other buildings spend wood by their registry costs (the forester costs 150 gold + 2 tools, no wood), read from the `BuildCmd` records.

**Material ledger (every term in real units; the terms of one equation are disjoint physical quantities; unexplained terms must equal zero at every checked boundary, not merely be printed) *(E2.0, corrected)*.**

Disjointness rules: (1) a **reservation is not a store**: `Farm.reserved` names part of `Farm.out` promised to a cart; the goods stay in `out` until the cart's load takes them (`load` subtracts from `Output`; the reservation is released at the same time), so `out` already counts them and `reserved` never appears as a separate term. (2) **Cart loading is not warehouse receipt**: a load moves goods from `Farm.out` into the cart's `cargo_88`; only the cart's unload at its home market moves them into `CityStock` (clipped at the city capacity; the clipped rest stays in the ended figure, lost). (3) **Active versus historical cargo**: a figure that has ended (`expiry_stamp` present) keeps its last `cargo_goods`/`cargo_88` fields until phase C destroys it (the existing transport body does not clear them after the unload, and the harvest body does not clear them after the delivery: the original does not either). Those fields are *history*, not goods: only figures without `expiry_stamp` carry active cargo. The ledger counts a delivery once, at the merge of its effect, and never again from the ended figure's fields; no existing transport semantics are changed to make this so. (4) **City stock signs**: a ship *unload* adds to the city stock and removes from the hold; a ship *load* removes from the city stock and adds to the hold; construction removes from the city stock.

| Quantity (unit) | Source | Equation at a checked boundary |
|---|---|---|
| ship wood (8.8 t of `HOLZ`) | manifest / command records | `hold_wood(t) = 12800 − founding(3072) − Σ unload_wood + Σ load_wood` |
| city wood (8.8 t of `HOLZ`) | `CityStock[HOLZ]` | `city_wood(t) = city_wood(t0) + Σ unload_wood − Σ load_wood − Σ construction_wood + Σ cart_receipts_wood`; `unexplained = city_wood(t) − (that right side) = 0` |
| trees cut, per type | tile ledger from the **registry-derived** table E-19 | `Δcount(N) = −cuts(N) + regrown(N)`, `Δcount(N−1) = +cuts(N) − regrown(N)` for every cuttable N (45 types), regrowth measured, not assumed 0; 259 never regrows |
| raw credited (8.8 t of `BAUM`) | `256 · successful planned cuts` | `credited = active_worker_cargo + delivered_raw + lost_raw`; `lost_raw` only for the evidenced home-gone arrival (cargo of a worker that ended without a delivery) |
| raw in the farm | `Farm.raw` | `raw(t) = raw(t0) + delivered_raw − consumed_raw` (fixture: `raw(t0) = 0`) |
| raw consumed -> wood made | the step rule per production step (`raw 26 -> made 11`, not `⌊26/32⌋ · 14`) | `consumed_raw = Σ (32 · ratio) >> 7`, `made_wood = Σ (14 · ratio) >> 7` over the steps that produced |
| wood made (8.8 t of `HOLZ`) | `Farm.out` (reservations inside it), carts | `made_wood = out(t) − out(t0) + Σ loaded_wood` (fixture: `out(t0) = 0`) |
| wood loaded by carts | cart loads from this farm | `Σ loaded_wood = active_cart_cargo_wood + Σ offered_at_unload_wood + Σ lost_no_unload_wood`; the last term is the cargo of a cart that ended without an unload (its home market gone: `transport.cpp` `unload` returns without a move; or no path home: `start_home` -> `Done`), a named term the fixture asserts is 0 unless a control removes the market |
| wood offered at the unload | the carts' unload effects | `Σ offered_at_unload_wood = Σ cart_receipts_wood + Σ clip_lost_wood` (the clip is a named term; 0 while the city is below capacity, which the fixture asserts) |

Positive proof of new wood: `Σ cart_receipts_wood > 0` with every equation balanced, `out(t0) = raw(t0) = 0`, and every received unit traced back to `made_wood`, which traces to `consumed_raw <= delivered_raw <= credited = 256 · cuts`.

Positive proof: a positive newly produced warehouse delivery that every equation above explains; existing wood or an unexplained warehouse increase cannot satisfy it. Negative controls that must fail the canary: (a) `Farm.out` pre-seeded and trees absent (`trees_cut == 0`); (b) forester switched off; (c) trees but no road/warehouse (`made > 0`, `sent == 0`); (d) `Farm.raw` pre-seeded and trees absent; (e) route home blocked: `raw` stays 0, cargo stays on the figure, and no `raw` delivery tick is earlier than cut tick + the walk home; (f) credit without depletion (a mutant crediting before the cut) breaks the tile equation; (g) the stale-read body (E-15) breaks `credited = 256 * cuts`. The initial `raw` and `out` are asserted 0; nothing in the canary edits the ship's loadout.

**Save/reload.** Resumed and uninterrupted runs agree in final bytes/hash and completion timing at: a worker inside its first delay (`start_delay` 3 after 2 ticks, E-21) *(E2.0)*, an outbound state (`ToTree`), the cut wait (`Cut`, wait remaining), **a carrying state (`ToHome`, cargo > 0, E-14's 18 000-tick shape) and a returning state just before the delivery**. These are covered by small fixtures (E-14 numbers); the real canary reloads at one harvest/carrying checkpoint if it falls in the run. Replay: record the real canary and replay it in a fresh process with matched checkpoints; a changed legal command (executed, not a malformed byte) must diverge.

## 8. Scope dependencies, honestly

- **Planting/regeneration is part of the harvest lifecycle but needs no new planting rule:** the cut writes the young stage (`id - 1`) and the *existing* growth sweep regrows it. For the *first-wood* canary regrowth is irrelevant (the first trees are only cut); for runs longer than ~8 minutes or a sustainable economy `grow_chunk` and its schedule must be live and T-F8 must hold. Whether anything else plants trees is U-6.
- **Walk search with tile goals on layer 2** is a dependency of the implementation (`docs/re/forestry.md` §2.11): the sim's `search()` takes point goals from building entrances; the forester needs **tile goals** whose goal sub-cells are the tile's free sub-cells. Trees are walk code 2 (259/261 code 4), enterable on layer 2 like open ground. The first-wood timing and selection inherit the undecoded finer-route validation (U-1b, §3.8).
- **Claim cleanup on paths not traced** is not asserted as an original leak (U-5b); the rebuild implements the traced transitions (faithful) or the owner-keyed hygiene (enhanced).
- **Rendering/UI** are out of scope; the worker's figure is drawn by the existing figure draw.

## 9. Staging (subordinate to the board's authorized packages; nothing here releases them)
The board's E2 (immutable facts, minimum state, persistence, the new DrawSite/timeline/stream identities and the two Rules bytes), E3 (selection, travel, harvest, return, the overlay, claims, the first delay through the scheduler; no placement hook, §3.5), E4 (production link, ledger, controls), E5 (the one opening canary, save/reload, replay) and E6 (gates, integration review) own the work; this contract is their input after acceptance. Each ends with the evidence-contract gate and an independent read-only review. The unknowns that remain after E1-FINAL are listed in `docs/re/forestry.md` §7 (U-1b fine-route validation, U-5b route-terminal ends, U-12 replaced-tile cut, U-13 fur trapper draw, U-14 tile scale, NOT_RUN-1/2 original-runtime observations).
