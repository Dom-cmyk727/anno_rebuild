# Simulation state: world, buildings, scheduler, commands, hash and save

Spec for the M3 state model of `src/sim` (`anno_sim`). It extends [`core.md`](core.md) (clock, PRNG, hash v1) and [`production.md`](production.md) (the step functions, which stay as they are except for one fix, §7.4). Evidence: [`docs/re/`](../re/) plus four disassembly reports of 2026-10-18 (building creation, group lists, world tick, command set) whose corrections to `docs/re` are collected in §16.

Evidence tags used throughout:

| Tag | Meaning |
|---|---|
| **D** | disassembled; module and address given (`AnnoWelt.dll` unless named) |
| **P** | parsed from a game file |
| **I** | inferred: a reading of code or data not confirmed by a second source; the spec says what it assumes |
| **O** | open: unknown; an extension point with a faithful or inert default exists (§12) |
| **N** | own design, no original counterpart |

Status: **design; stages S1 (infrastructure), S2 (slot list, scheduler blocks, container probe) and S3 (config, validation, layout, settings, types) implemented, verified on MSVC and GCC 13 only (S2 also on the Clang 19 front end); the Clang and CI-matrix part of all three exits is outstanding** (2026-10-19: `ids.hpp`, `draw.hpp`, `archive.hpp`, `hash_tree.hpp`, `field_count.hpp`, `state_fields.hpp`, shared `RngTable`/`stream()`, `WorldClock::begin/end`, `kStateHashVersion` = 100 with the scopes `header` and `clock_rng`, `tools/lint_sim.py`, the asan sanitizer gate; notes in §15 S1). Besides that only the production step functions the S2 pieces (`SlotPolicy`, `SlotList`, the timed-event queue, `scheduler.hpp`) and the S3 pieces (`config.hpp`, `settings.hpp`, `seams.hpp`, `types.hpp`, `synthetic.hpp`) exist in `src/sim`. The type declarations below are the contract; they were checked against the real `production.hpp`, `clock.hpp`, `rng.hpp`, `hash.hpp`; the archive and scheduler building blocks of the incremental candidate design were compiled with MSVC `/W4 /WX` and run (§14.1). When this was written GCC and Clang had not seen any of it; S1's archive and canary code has since been built with GCC 13.3 (`-O3`, `-Werror`, the full CI warning set, WSL) and run under ASan/UBSan without recovery, while Clang and the CI jobs themselves have not run yet (§15 S1). Stage S2 (2026-10-19) added the early GCC-release probe of the container shapes the state uses; checked matrix: GCC 13 at `-O2`/`-O3` and GCC 12 at `-O3` found no false positive, GCC 12 at `-O2` reports one in `vector<bool>`'s range insert (`line_on = {a, b}`), avoided once in the test (§15, S2 as implemented). GCC 12 is not a CI compiler (`ubuntu-latest` is GCC 13); the shape matters again when the `production.hpp` payloads move into `Building` (S6). Stage S3 (2026-10-19) added the configuration (`SimConfig`, `Rules`, `ListLayout` with validation and digests), `WorldSettings`, the seam table (names), the type registry with the class, goods and name tables, the builder and the synthetic registry, and the header and settings scopes of the stream (`kStateHashVersion` = 101); verified on MSVC and GCC 13 (`-O2 -Werror` with the CI warning set; `-O1` under ASan + UBSan without recovery, WSL); Clang and the CI jobs have not run (§15, S3 as implemented).

Revision 2 (2026-10-18) applies the defects found by two independent reviews (42 and 16 claims; §0.4 lists what changed where). The owner's issue rule is applied throughout: every deviation is `fix` (obviously unintended: removed, documented, no switch) or `decision-needed` (could change the game's feel: the faithful behaviour stays the default behind a `Rules` option, the owner decides; the filed questions are `OQ-036`..`OQ-053` in `docs/open-questions.md`, one per row D1..D18 of §13).

## 0. Scope, layering, provenance

### 0.1 What this spec covers
- The owning state tree under `World`: islands, tiles, deposits, cities, building lists, buildings, players, figures (skeleton), timed events, script timers, monopoly table.
- `World::advance`, the faithful scheduler: order, formulas, integer widths, which stamps and cursors are state.
- Commands: the only way to change a `World` from outside besides `advance`; validate/apply contract, first command set, wire encode/decode, host stamping.
- One field list for hash, save and load; the hash tree; the save frame; invariants.
- The scenario loader (plain data in, owning state out).
- Extension points with a default for every item still open after the four RE reports.
- Deviations from the original (tag `fix` or `decision-needed`), the test plan and the staged implementation plan.

Out of scope here: the bodies of unported rules (markets, houses, public buildings, military, figure movement, AI, wildlife, city economy passes, drought tile effects), the net layer, rendering. They get seams (§4.12) at the exact point of the original call so that adding them later never changes the scheduler.

### 0.2 Layering
- `anno_sim` stays headless: it links only `anno_formats`. `scenario_formats.cpp` is the only file that includes `formats::sze`, `formats::insel` or `formats::figuren` beyond what `production.hpp` already includes; `assets::unit::start_colours` is ported into the loader by value, never linked.
- `World` owns its whole state by value (the immutable `TypeRegistry` and the PRNG table are shared by `shared_ptr<const>`). Copying a `World` is a snapshot: used by tests, the bisect helper and resync. No pointers into state, no raw owning pointers, no `unique_ptr` in copyable state (spatial indexes are plain vectors).
- Simulation code: integer arithmetic only, ordered containers only, no wall-clock, no global mutable state, no UB (UBSan/ASan in CI), PRNG draws only through `DrawSource` (§4.2), parsers and the codec return `Result`, no exceptions (CLAUDE.md ground rules; `tools/lint_sim.py` enforces the mechanical part, §14).
- Style: `namespace anno::sim`, `#pragma once`, header comment = spec + evidence, plain structs, `make_*` constructors, `Result<T, std::string>`, `static_cast` only, `-Wconversion -Wsign-conversion` and MSVC `/W4 /WX` clean, clang-format 23.1.3. New `.cpp` and test files are listed by name in `src/sim/CMakeLists.txt`; test names contain none of `; * ? [ ] \`; tests that need game files use `anno::test::game_dir()` and skip with a message.
- The existing 47 sim test cases (the count before S1; 65 after S1, 110 after S2, 148 after S3) and all step-function signatures stay, except the small-farm change of §7.4 and the migration of the single external mutation (`sim_test.cpp:193`, `w1.rng.next()`) when `World` members become private (stage S8).

### 0.3 How this spec was assembled
Three candidate designs (working papers, not repo files) were compared by independent reviewers: fidelity-first, data-oriented, incremental. This spec takes

| From | What |
|---|---|
| fidelity | the scheduler (order, formulas, stamps, cursors, hooks at the original call positions), `ListLayout` and `SimConfig` as data, `Rules` defaults that are all faithful, `WorldClock::begin/end`, the object-manager state, `Rules`/`SimConfig` in the hashed header, the monopoly/event 7.5 order |
| data-oriented | island-partitioned ownership (`TickContext` read-only views plus an ordered `Effects` buffer merged in island order), island-serial `BuildingId`, per-site PRNG draw telemetry, the assumptions table, hash sections that compare one by one, chunk-digest tile hashing, the optional deterministic parallel island tick |
| incremental | the vertical-slice stage order with stage 0 fixing the small farm first, named scheduler building blocks tested against an independent reference model, `ReplayLog`/`Recorder`/divergence finder, `lint_sim.py`, `Inert` as the default for unread classes, the latching `ReadArchive`, perturbation and allocation-pattern tests, seams hashed by name |

Resolved cross-design points: unread class bodies are **inert** (their `last` is not touched; no period is invented); the figure stand-in never returns a figure unless a rule says so; the unit registry visit count is `clamp(d·n/50, 1, n)` without `+1`; money is `i64`; `ListKind` is not a compile-time enum (lists are `ListId` indices into a `ListLayout`); the PRNG table is shared, only its cursor is per-stream state; `advance(0)` is not special-cased.

### 0.4 What revision 2 changed (review log)
| Topic | Change | Where |
|---|---|---|
| Config validation | `SimConfig::validate()`, `Rules::validate()`, `ListLayout::validate()`, called by `World::create` and `World::load` before any state is built; one table-driven test per field | §3.1, §9.5, §10, S3 |
| Invariants and load | structural invariants (payload alternative and vector sizes per type, config-sized containers, climate index, island rects, tile dimensions in `u64` against `remaining()`); `tick_building` has no throwing `std::get`; one perturbation test per check | §9.6, §7.1, S5, S6 |
| `PrngMode::PerIsland` | design-only: a reserved `Rules` value that `validate()` rejects until stage S16, so no unhashed stream state can exist; S16 owns the state, hash scope and save entry | §3, §4.2, §6, §11, D11, S16 |
| Figure phase and seams | serial `SerialWorld` context, `Effects` entries `GoodsMove`/`Reserve`/`Deliver`, merge after each figure call (C2); `Seam` carries a `ctx`; AI and event handlers receive the `TickContext` (hence the `DrawSource`); island-level queries on `Island`/`City`; `IslandsView` in `TickContext` | §2.2, §4.7, §4.8, §4.11, §4.12, §5.9 |
| Hard-coded literals | owner column `u32`; trade list and limit `u32`; `& 7` is `% size()`; `0..14` is `0..neutral_owner-1`; `& ~7` and `8 * chunk_edge` use `chunk_edge`; `drought_steps`/`drought_phases` config; `upkeep_rem` `u32`; list roles in `ListLayout` | §3, §4.5, §4.7, §4.8, §5.8, §7.4 |
| Scripted droughts | D7 scoped to the 300 s event; the constructor is also called by AnnoScene for scripted disasters: out of scope for M3, entry O-51, planned host-only door named | §5.8, D7, O-51 |
| Corrections | player-table step counts 2/4/7 for 5/10/20 ms; A11 address `0x100a3aed`; object-manager traversal tagged N (original: do-while over a tile cursor); message manager marked open (O-52); chunk over-read of the original on islands not a multiple of 8 (A14, F20); AI commands deterministic per advance and not logged | §5.4, §5.6, §5.10, §8.2, §1.2, §13 |
| Build and test plumbing | asan preset gets `-fno-sanitize-recover=all` with a failing probe; lint allow-list created with the lint; `WorldRng` keeps copy-on-move; `kStateHashVersion` is a development counter until S15; early GCC-release probe; allocation-pattern test is junk-allocation interleaving only | §14.4, §14.6, §15 |

## 1. Conventions and assumptions

### 1.1 Arithmetic
- `tick` is the 64-bit world tick (`clock.ms >> 1`); `t` is its **low 32 bits** (`std::uint32_t`), passed to every gate, stamp and step function exactly as the original passes its 32-bit tick (D: `Advance` 0x100ae040 stores `(u32)(ms >> 1)` at `+0x10c`). `grow()` divides `t` by `P/2` in 32 bits and is not wrap-invariant; 2³² ticks are 99 game days (F-time, §13).
- Stamps are `u16` and keep the original's wrap: `d = static_cast<u16>(t - stamp)`. Cursors, slots, counts and ids are widened (`u32`/`u64`): the original's `u16` cursors/slots and signed-32-bit `d·n` products are hidden limits (F3).
- Every product is computed in `u64`; every quotient truncates. `visit_count(t, stamp, n, div) = min(d·n/div + 1, n)`; `visit_count_clamped(t, stamp, n, div) = clamp(d·n/div, 1, n)` (§5.1).
- Original constants (all `SimConfig` data, §3): 500 (buildings), 50 (city ring, player table ×32, figure registry B), 250 (figure registry A), 2000/15000 (island sweeps), 5000 (object manager), 300 s, 450 000 ms, 30 000 ms. The multiplier constants are `0x10624dd3`/`sar 5` (/500, D 0x10023780), `0x51eb851f`/`sar 4` (/50, D 0x100969d0, 0x1009c4d0, 0x1009a8a0), `0x45e7b273`/`sar 12` (/15000), `0x68db8bad`/`sar 11` (/5000, D 0x10095d40); `0x10624dd3·x >> 37` is exact for `x < 2³¹`, which bounds the original lists at 32 768 slots (65 535 · 32 768 < 2³¹; 32 769 overflows).
- Money is `i64` (the original `i32`, F15). Statistics that the original keeps in `u32` and multiplies (`·104/105`) stay `u32` with unsigned wrap.
- Draw: `WorldRng::next()` (core.md). `r % k` is `u32`.

### 1.2 Assumptions (stated so a reader can falsify them)

| # | Assumption | Basis |
|---|---|---|
| A1 | In the island tick only the two sweeps depend on the chunk count: with zero chunks both sweep counts clamp to 0 and everything after them (object manager, city ring, 300 s event, drought tick, stamp) still runs. | **D** 0x1008b7b3–0x1008b7bf (count clamped to 0); the RE note "no call when chunks==0" is read narrowly |
| A2 | Inside a chunk visit the growth sweep visits tiles row-major; tiles do not interact and `grow` draws nothing. Whether callees of 0x1008b5f0 draw is unverified. | **I** / **O** |
| A3 | The sweep cursor wraps `x += 8; if x >= width { x = 0; y += 8; if y >= height { y = 0 } }` (x resets on every row wrap). The RE text places `x = 0` ambiguously. | **I** |
| A4 | Chunk count is `ceil(w/8) · ceil(h/8)` (`[island+0x90]·[island+0x8c]`, rounding unread). | **I** |
| A5 | The 300 s event tests city slots `0 .. neutral_owner-1` (original 0..14, D 0x100939e0); `start_drought` scans all `city_slots` slots (original 0..15, D 0x10093a75) and takes the first non-null building (by slot order, holes skipped) of the first city that has one in its market list (`ListLayout::market_list`). | **D** (bounds), **I** (list scan bound: `size` vs live count) |
| A6 | The figure registry reuses slots by the building-list policy (0x10097250 unread). | **I** |
| A7 | A fresh building list holds exactly one null element. | **D** ctor 0x100236a0 (closes the earlier O-12) |
| A8 | The scheduler cursor is `< n` in every consistent state (the original does not clamp); a loaded cursor `>= n` is a load error (`run_slot_list` treats it as a defined no-op, §5.1). | **I** |
| A9 | Command 0x74's `value` byte is a switch: non-zero = on. | **I** |
| A10 | The island object manager **traversal is a stand-in (N)**: it visits `Island::deposits` in index order from a cursor index with `cursor = (cursor + 1) % n`, and visits nothing when `n == 0`. The original is a do-while (the body runs once before the count test), its cursor is a **tile** (words `+0x24/+0x26`) that jumps to the next object's position in chunk-bitmap order (0x10095770, 0x10095ad0), `n` is `[mgr+0x28]`, objects are fetched through virtual slots `+0x14`/`+0x10` and ticked through slot `+0x6c`. Only the stamp arithmetic `min(d·n/5000 + 1, n)` is **D** (0x10095d40). The hashed `objmgr.cursor` is therefore **N**; the real manager replaces it with a tile cursor and a hash-version bump (O-12). | **N** / **O**; stamp formula **D** |
| A11 | World property 14 defaults to 3 (add-on bit set) in a bare `World`; the scene sets 1 (base game) or 3 (ANNOADDON1). | **D** 0x100a3aed (`mov dword [esi+0x69dc], 3` in the World constructor 0x100a3980); AnnoScene 0x10009cc0 sets property 14 from `[ebp+0x168]` at 0x10009df6 |
| A12 | The script engine (BigBrother) steps before every advance in the net client; it is not part of `anno_sim` (§12, O-34). | **D** AnnoFrame 0x1005fe10, **O** |
| A13 | A footprint tile belongs to at most one building per list; building footprints are disjoint. | **I** (the island check 0x1007c790 forbids overlap; unread) |
| A14 | The original growth visit 0x1008b5f0 always loops 8×8 tiles with index `y·width + x` and **no bounds check**. For an island whose width or height is not a multiple of 8 the edge chunks alias the next row or read past the tile array. The port clips the visit to in-bounds tiles (`TileLayer::chunk_digest` hashes in-bounds tiles only) and records the deviation as F20; it does not reproduce the over-read. The `.scp` corpus widths were not verified to be multiples of 8: the corpus test (§14.5) counts islands with `w % 8 != 0 or h % 8 != 0` and prints the number (expected 0; a non-zero number is an owner-visible finding, not a failure). | **D** 0x1008b5f0 (no check), **I** (corpus widths) |
| A15 | The AI seam produces its commands deterministically from hashed state and the `DrawSource` on every advance; they are applied at once and are **not** logged by the `Recorder`. Replay therefore needs the AI seam installed identically (its name is in the replay header's seams digest). | **N** (§8.2) |

## 2. Identity, ownership, containers

### 2.1 Identity

| Thing | Identity | Lifetime / reuse | Stored in |
|---|---|---|---|
| Island | `IslandId` = slot in `World::islands` | never freed in play | `World` |
| City | `CityRef{island, number}`; `number` = slot in `Island::cities` = the owner value written to tiles (15 = none) | freed only by demolish (D9); slot reused by a later city founding | `Island::cities` |
| Player | `PlayerId` = slot in `PlayerTable` (31 = neutral) | | `World::players` |
| Building | `BuildingId` = `(island << 32) + serial`; serial from the island's own counter, starts at 1, never reused. Order identity is `(city, list, slot)` | slot reused by the free-slot policy (§5.2); id never; serial exhaustion is a deterministic reject (`Reject::SerialExhausted`) | the slot holds the `Building` by value |
| Deposit | index in `Island::deposits`, append-only (never removed or reordered, depleted deposits stay with `remaining == 0`), so the index is a stable id | | `Island` |
| Figure | `FigureId` (u64, world counter, never reused) and registry slot (reused by the registry policy) | | `FigureRegistry` |
| Timed event | `(due, seq)` | | `EventQueue` |

The original has no unique building id (identity is list + slot, **I**: the list code addresses a building by slot; the building constructor at 0x100734c0 never writes `+0x04` (F2), so neither a stored u16 slot nor a `0xffff` none value is established by that address). `BuildingId` makes figure and event references immune to slot reuse and id assignment independent of how island ticks interleave. Figure ids and event sequence numbers are assigned only in the serial merge phase, in island order (which is the original order).

### 2.2 Ownership tree
```
World
 +- shared_ptr<const TypeRegistry>   immutable; its digest is hashed
 +- SimConfig, Rules, WorldSettings, Seams (named, hashed by name)
 +- WorldClock clock, WorldRng rng   rng = shared immutable table + cursor
 +- IdCounters                       next_figure, next_event_seq
 +- PlayerTable                      slots (default 32), stamp, cursor
 +- vector<optional<Island>> islands size = original [+0xf4] count (<= island_slots), table order = Advance order
 |    +- TileLayer                    SoA columns + chunk digest cache
 |    +- vector<Deposit>              append-only island objects of class deposit
 |    +- ObjectManagerState, SweepState, CityRing, event300 stamp, optional<Drought>
 |    +- vector<optional<City>>       size = city_slots (default 16); slot = city number
 |         +- CityStock, goods settings, build-goods records, trade lists, stamp
 |         +- BuildingIndex           stamp, vector<BuildingList> (one per ListId)
 |              +- BuildingList       hint, cursor, vector<optional<Building>> with null holes
 |    +- derived: footprint grids, by_id map, service counters (not hashed)
 +- EventQueue, FigureRegistry, ScriptTimers, MonopolyTable, event750 accumulator
 +- SimOutput                         UI-facing events; not state, not hashed
```
Rules that follow:
- A building belongs to exactly one island; its city is `city_no`. References between records are ids (`Mine::deposit` an index, `Figure::home` a `BuildingId`).
- An **island tick** receives read-only views of everything outside its island (`TickContext`: players, the island table's immutable facts through `IslandsView`, the type registry, config, settings, the `DrawSource`) and an `Effects` buffer for cross-island consequences, merged in island order after the tick (§4.11). `IslandsView` exposes only fields no island tick writes (island count and presence, rect, climate, and the derived tile-to-island lookup), so a parallel island phase (S16) cannot race on it; other islands' mutable state is not readable from an island tick. Inside its island it may write everything. Fields that only effects write (player money) are not readable from island code (`PlayersView` has no money).
- **Serial phases** (events, figure registry, players/AI) receive a `SerialWorld` (const queries over the whole world plus an `Effects` buffer merged right after the seam returns, §4.11, C2).
- Island-level query accessors (`building_at`, `rect_query`, `find_building`) live on `Island` and `City`; the `World` members of the same names delegate to them (§4.7, §4.8), so `tick_building` and later market code can call them with only an `Island&`.
- A resolve that fails (`find_building(id)` on a removed building) yields `nullptr`; callers treat that as "gone", never as an error that aborts a tick.

### 2.3 Containers and iteration order

| What | Container | Order and why |
|---|---|---|
| Islands | `vector<optional<Island>>` | table slot order = original `Advance` order (D 0x100a2e60); size = original count `[+0xf4]` |
| Cities | `vector<optional<City>>` | city-number order = ring order, drought and monopoly scans (D) |
| Players | `vector<optional<Player>>` | slot order with a round-robin cursor (D 0x1009c4d0) |
| Building lists | `vector<BuildingList>` per city, indexed by `ListId` | `ListLayout::schedule` order |
| Buildings | `vector<optional<Building>>` per list | **the original slot order including null holes**; never shrinks or compacts (§5.2) |
| `by_id` (derived) | `std::map<BuildingId, BuildingKey>` per island | lookup only; sparse, rebuilt on load |
| Footprint grids (derived) | `vector<u32>` per list, per island, lazily sized; value = building serial, 0 none | O(1) point lookup, no footprint-height limit (F3) |
| Figures | `vector<optional<Figure>>` | registry slot order with cursor (D 0x1009a8a0 B) |
| Events | `std::map<pair<u64 due,u64 seq>, TimedEvent>` | list order (D 0x100adef0: inserted after all nodes with `due <=` its own) |
| Deposits | `vector<Deposit>` | island object order (`find_deposit` ties: first) |
| Goods/type lookups | `std::map` or dense vectors | never `unordered_*` |

Forbidden in `src/sim` (lint): `unordered_*`, pointer-keyed ordering, `std::sort` on non-total keys, `vector<bool>` in new state, `float`/`double` outside `production_type`'s load-time conversion, `<chrono>`, `rand`, `random_device`, non-const `static`/globals, `long`, `size_t` in saved structs.

## 3. Configuration: `SimConfig`, `Rules`, `ListLayout`, `WorldSettings`
*Implemented in S3 (`config.hpp`, `config.cpp`, `settings.hpp`). Additions to the listing below: the constants `kMaxStampPeriod` (65535, the periods compared with a 16-bit difference) and `kOriginalCityPassCount` (8, the default of `city_pass_count` and of a default `Seams::city_pass`); `digest()` members on `SimConfig`, `Rules` and `ListLayout`; no `SimConfig::list_size_bound` (§3.1). The S3 review round added the build-time bounds `kSlotTableBound` (2¹⁶), `kEntryTableBound` (2¹²) and `kWorldEdgeBound` (2¹⁴), the saved-width limits `kMaxDroughtStep` (255) and `kMaxDroughtCounter` (65535), `kDefaultSeamName` (`seams.hpp`) and `WorldSettings::validate()` (F27, §3.1). The messages of the `validate()` functions start with the field: `SimConfig.<field>: ...`, `Rules.<field>: ...`, `ListLayout.<field>: ...` (`SimConfig::validate` returns a layout problem as `ListLayout.<field>`, except `block_size`, which it reports as `SimConfig.layout.block_size`).*

Every original count, size and period is data with the original value as default (PLAN §0.1). `SimConfig`, `Rules` and the `ListLayout` are hashed in the header scope (§9), so two peers with different options diverge visibly at tick 0. The sim contains no `constexpr` count except the documented defaults in `config.hpp` and `production.hpp`'s existing ones and the named sentinels/versions (`kNoSlot`, `kAppendIndex`, `kStateHashVersion`, `kPrngTimelineVersion`, `kOriginalRngLength`; the lint allow-list names them, §14.6). Where a width is fixed on purpose the line carries a comment with the reason and the path to making it configurable (the 16-bit **stamps** are the one model-defining case, F3).

**Validation (§3.1).** Configuration arrives from callers, mods and, through `World::load`, from a save header or a resync snapshot that may be corrupt or hostile. `SimConfig::validate()`, `Rules::validate()` and `ListLayout::validate()` therefore exist, are called by `World::create` and `World::load` **before any state is built** (and by `Seams` consistency checks, §3.1), and every code path below may assume their postconditions (non-zero divisors, in-range indexes, shift counts below 32).

```cpp
// config.hpp
enum class SlotPolicy : std::uint8_t { Original, LowestFree };           // D1
enum class SpatialOrder : std::uint8_t { YXSlot, Original0x10027000 };   // D2 (second: reserved, O-7)
enum class PrngMode : std::uint8_t { Global, PerIsland };                 // D11; PerIsland is DESIGN-ONLY: reserved, rejected by Rules::validate() until stage S16

struct SimConfig {
    // ---- table sizes (D unless noted)
    std::uint32_t island_slots = 256;       // world +0x6530 table; islands.size() <= this
    std::uint32_t city_slots = 16;          // island +0x38 table = the city ring length
    std::uint32_t neutral_owner = 15;       // owner value "nobody" (D 0x1007c790, the 4-bit nibble in the original); never a usable city number. The owner column is u32 (§4.8), so any value fits
    std::uint32_t player_slots = 32;        // player table +4..+0x84
    std::uint32_t neutral_player = 31;      // skipped by the bankruptcy tick (D 0x1001b900)
    std::uint32_t script_timers = 5;        // SKTIMER (D 0x1009cf30)
    std::uint32_t monopoly_entries = 28;    // MONOPOL table (D 0x1009d2a0): 20 raw materials + 8
    std::uint32_t monopoly_raw_slots = 20;  // first loop s = 0..19; <= 32 (s is a shift count into a u32 mask), <= monopoly_entries
    std::uint32_t service_bits = 32;        // public-need counters, index +6..+0x45 (D); <= 32 (BuildingType::service_mask is u32)
    std::uint32_t house_groups = 5;         // index +0x46.. (D)
    std::uint32_t research_entries = 65;    // player research table (D 0x10019190)
    std::uint32_t warehouse_goods = kOriginalWarehouseGoods;   // 0x38 (production.hpp)
    std::uint32_t build_goods_first = 0x29; // goods charged by Place: 0x29..0x2d (D 0x10027a77)
    std::uint32_t build_goods_count = 5;
    std::uint32_t trade_lists = 2;          // Kontor sell/buy lists (D 0x73); commands carry the list as u32
    std::uint32_t trade_limit_max = 255;    // the Kontor goods limit is a byte in the original (D city+0x334+12g+0x0b); stored as u32, values above this are rejected (BadValue)
    std::uint32_t chunk_edge = 8;           // island sweep chunk edge in tiles (D 0x1008b760); also the drought grid (0x1008aeb0)
    std::uint32_t list_initial_size = 1;    // list ctor creates one null element (D 0x100236a0, A7)
    std::uint32_t list_min_growth = 10;     // D 0x10026a00
    std::uint32_t city_pass_count = 8;      // passes after the building scheduler (D 0x1002d9a0)
    // ---- scheduler divisors and periods (D)
    std::uint32_t building_div = 500;       // count = d*n/500 + 1
    std::uint32_t city_ring_div = 50;
    std::uint32_t player_div = 50;
    std::uint32_t figure_a_div = 250, figure_b_div = 50;
    std::uint32_t objmgr_div = 5000;
    std::uint32_t grow_period = 2000, fauna_period = 15000;
    std::uint32_t figure_expiry_ticks = 7500;           // 0x1d4c
    std::uint32_t event300_gate_seconds = 300, event300_modulus = 20;
    std::uint32_t event750_ms = 450000, event750_modulus = 10;
    std::uint32_t monopoly_ms = 30000;
    // ---- player table (D 0x1001b840, 0x1001b900)
    std::uint32_t upkeep_period = 2000, upkeep_slices = 15;
    std::uint32_t stat_decay_num = 104, stat_decay_den = 105;
    std::int64_t bankrupt_money = -3000;
    std::uint32_t bankrupt_grace_ticks = 150000;
    // ---- drought (I, D 0x10087280, 0x1008aeb0)
    std::uint32_t drought_radius = 0x32;                // target ring = radius / chunk_edge = 6
    std::uint32_t drought_plateau_period = 1000, drought_plateau_count = 120, drought_strip_period = 50;
    std::uint32_t drought_steps = 8;                    // strips per phase: step advances by chunk_edge up to drought_steps * chunk_edge (I, 0x1008aeb0)
    std::uint32_t drought_phases = 4;                   // phases per ring (I, 0x1008aeb0)
    // ---- monopoly climate masks (D 0x1009d2a0): index = climate 0..6
    std::vector<std::uint32_t> monopoly_climate_mask = {0x18000, 0xf009, 0xf949, 0xf06b, 0xf6b9, 0xf04d, 0xf04d};
    // ---- session (host-side agreement, not original values)
    bool multiplayer = false;               // speed 400 % refused, turn based (D 0x100071b0)
    PlayerId host_player = PlayerId{0};     // whose SetSpeed counts
    SmallFarmAnimals small_farm_animals;    // D 0x100c8d48: kind table indexed by draw % kind_by_draw.size() (= draw & 7 for the original 8 entries)
    ListLayout layout = ListLayout::original();   // ListLayout is declared before SimConfig in config.hpp; this listing is ordered for reading

    /// §3.1. Returns an empty string when valid, else one message naming the field ("SimConfig.building_div: must be non-zero").
    [[nodiscard]] std::string validate() const;
};

/// Opt-in changes. Every default is the original's behaviour; every field exists only for a decision-needed
/// row of §13 (and two cheats/QoL switches). Hashed in the header.
struct Rules {
    SlotPolicy slot_policy = SlotPolicy::Original;               // D1
    SpatialOrder spatial_order = SpatialOrder::YXSlot;           // D2
    bool events_drain_new = false;                               // D3  re-read begin() after every dispatch
    std::uint64_t events_max_dispatch = kEventsMaxDispatchDefault;   // F24 per-call cap of pop_due (>= 1); a limit, not a behaviour switch
    bool events_flush_after_figures = false;                     // D4  second event pass after the figure phase
    bool monopoly_every_30s = false;                             // D5  acc += dt; < 30000 return; acc -= 30000
    bool player_table_full_tick = false;                         // D6
    bool drought_random_centre = false;                          // D7  the 300 s event's centre only (the draw per island per gate is kept); scripted droughts are O-51
    bool build_atomic = false;                                   // D8
    bool keep_empty_city = false;                                // D9
    bool event750_keep_overshoot = false;                        // D10 acc -= 450000 instead of acc = 0
    PrngMode prng_mode = PrngMode::Global;                       // D11  PerIsland: reserved value, validate() rejects it until S16 exists
    std::uint32_t interim_figure_return_ticks = 0;               // D12 0 = a spawned figure never returns
    bool free_build_cheat = false;                               // F13: honour a free-build flag (never from the wire)
    bool enforce_research = false;                               // refuse placing a type whose research is not done

    /// §3.1. Rejects every reserved value: prng_mode == PerIsland (no per-island stream state exists, so it could not be hashed or
    /// saved; D11, S16) and spatial_order == Original0x10027000 (O-7); also any enum byte outside its range (`valid_slot_policy` for `slot_policy`: `SlotList::insert` reads an out-of-range value as `Original`, but a loaded value never gets that far) and `events_max_dispatch == 0`.
    [[nodiscard]] std::string validate() const;
};
```

### 3.1 Validation (`config.cpp`)
All three functions are pure, total and allocation-light; each returns one message naming the first offending field. `World::create` and `World::load` call them (and `validate_seams`, below) before building anything; `World::load` calls them on the values just read from the header, so a corrupt save or snapshot cannot reach a divisor, a modulus or an index. One table-driven test per field (doctest `SUBCASE` per row) perturbs a valid configuration and expects the message to name that field.

**`SimConfig::validate()`**
- *Non-zero divisors, moduli and periods* (each is a divisor of `visit_count`, a `%` operand or `ceil(w / chunk_edge)`): `building_div`, `city_ring_div`, `player_div`, `figure_a_div`, `figure_b_div`, `objmgr_div`, `grow_period`, `fauna_period`, `event300_modulus`, `event750_modulus`, `event750_ms`, `monopoly_ms`, `upkeep_slices`, `stat_decay_den`, `chunk_edge`, `layout.block_size`, `drought_steps`, `drought_phases`, `list_min_growth` (zero would let `SlotList::insert` write past a zero-growth resize; its upper bound is below). `visit_count` and `visit_count_clamped` additionally return `n` for `div == 0` (defined, no hardware divide trap) as defence in depth against a value that skipped the validation; a valid configuration never reaches that line.
- *List sizes* (a hostile config or save): `list_min_growth` and `list_initial_size` in `1..kListSizeBound` (2²⁴; `valid_list_min_growth` and `valid_list_initial_size` in `config.hpp` are the predicates `validate()` calls). Reason: both reach `vector::resize` on an insert or a load, and `0xFFFFFFFF` would ask for about 4 G elements (`bad_alloc`, which sim code must not throw, or an OOM kill). Path to raise it: change the constant `kListSizeBound` (`config.hpp`). S3 keeps it a build-time constant and does not add the field `SimConfig::list_size_bound` the first S2 text planned: a bound that is read from the very header it protects against could be set by a hostile header, so it would protect nothing; it is a sanity bound, not a game rule. `SlotList::insert` also cuts `min_growth` to the bound and returns `kNoSlot` (leaving the list alone) when a full list already has `0xFFFFFFFF` elements, because `Slot` is a `u32` and `kNoSlot` its maximum (`SlotList::grown_size`; widen `Slot`, `cursor` and `live()` together to lift that).
- *Periods compared with a `u16` difference* must be `<= 65535`, otherwise the gate can never open: `upkeep_period`, `event300_gate_seconds`, `drought_plateau_period`, `drought_strip_period`, `figure_expiry_ticks`.
- *Table sizes*: `island_slots`, `city_slots`, `player_slots`, `list_initial_size` `>= 1` (the last because `check_invariants` requires every building list to have `n >= 1`, A7); `host_player < player_slots`; `warehouse_goods >= 1`; `neutral_owner <= city_slots` (the city scans run over the slots below it, A5); `build_goods_first + build_goods_count <= warehouse_goods` (formed in u64); `monopoly_raw_slots <= min(monopoly_entries, 32)` (shift count `s < 32`; path to more raw slots: widen the mask to u64); `service_bits <= 32` (path: widen `BuildingType::service_mask` to u64); `monopoly_climate_mask.size() >= 1`.
- *Upper bounds of the tables the loader sizes without stream backing* (F27; the stream-backed containers are bounded by the input size, `ar::seq`, but anything `World::load` derives from the config alone is not): `island_slots`, `city_slots`, `player_slots` `<= kSlotTableBound` (2¹⁶); `warehouse_goods`, `script_timers`, `monopoly_entries`, `house_groups`, `research_entries`, `trade_lists`, `build_goods_count` `<= kEntryTableBound` (2¹²); `chunk_edge` `<= kWorldEdgeBound` (2¹⁴). Reason: `0xFFFFFFFF` in a header asks for gigabytes (`bad_alloc` or an OOM kill in code that must not throw), and `ceil(w / chunk_edge) = (w + chunk_edge - 1) / chunk_edge` must not wrap in u32 (S5 forms it in u64). Like `kListSizeBound` the bounds are build-time constants, not header fields (a bound read from the header it guards protects nothing); path to raise one: change the constant. The per-city and per-island containers sized from several of them (goods × cities) additionally get the remaining-input check of §9.2 in the loader (S5).
- *Saved widths of the drought record* (§5.8, §9.3; `u8 phase`, `u8 step`, `u16 counter`): `drought_steps · chunk_edge <= 255` (u64 product; `kMaxDroughtStep`), `drought_phases <= 255`, `drought_plateau_count <= 65535` (`kMaxDroughtCounter`). Reason: `drought_tick` compares the saved fields with these values (`step == drought_steps · chunk_edge`, `phase == drought_phases`, `counter == drought_plateau_count`); a value the narrow field cannot reach never ends the drought (a u8 step wraps 248 → 0 and never equals 320), the same class of defect as the stamp periods. Path to larger values: widen the three saved fields (stage S13) and the constants together.
- *Index tables*: `small_farm_animals.kind_by_draw` non-empty (its size is the modulus of the draw), every kind `k` satisfies `base <= k` and `k - base < limit.size()`, and `make_small_farm(type, animals)` sizes `figures_out` from the table (`max(5, max(k - base) + 1)`; the original's counters `+0x2a..+0x2e` are line 0 plus four animal kinds), so `figures_out[k - base]` is in range. A small-farm production type has exactly two lines. A `SmallFarm` read from a save must have `line_on`, `out`, `raw` and `activity` of one size and a non-empty `figures_out` (else the load is rejected); `small_farm_step` additionally does nothing on a farm that breaks this, and consumes the draw without sending on a table that breaks the first part (F21).
- `layout.validate()` (below) and `city_pass_count` `<= 64` (a sanity bound for the seam vector; reason: `Seams::city_pass` is a vector sized from it, configurable by raising the bound).
- `World::create`/`load` additionally check `seams.city_pass.size() == city_pass_count`, that every non-null seam function has a name that is neither empty nor `"default"` (`kDefaultSeamName`: the name is the only identity of an implementation that the header hash sees, so a function under the default name would hash exactly like no function and two peers running different logic would not diverge at tick 0; two different functions under one name are equally invisible, so a name must be unique per implementation and per version of its behaviour), and `TypeRegistry::validate_against(config)` (§4.4: the registry fits the world's own config). The header read of the state stream runs all of them on the values just read.

**`WorldSettings::validate()`** (F27): `world_width` and `world_height` `<= kWorldEdgeBound` (the island lookup grid of the original world +0x130, 8 × 8 blocks, and every island rect are derived from them; products are formed in u64) and `prop10 <= 2` (the tax level indexes the economy's tables). The other properties are unconstrained (their meaning is O-29). The `settings` scope of the state stream is validated right after it is read.

**A failed read leaves the target `World` unchanged**: `StateIO::world` reads the configuration, rules, settings, clock and PRNG cursor into temporaries and commits them only when the whole stream was read and validated, so a caller that reuses the target after a failed `fields(ReadArchive, World&)` never runs the scheduler with a partly overwritten, possibly invalid configuration (`World::load` also builds a fresh `World`, §9.5).

**`ListLayout::validate()`**: `lists.size()` in `1..32` (a list's `lookup_mask` is one bit of a `u32`; path to more lists: widen `lookup_mask` to u64 and raise the bound); every `lookup_mask` is a distinct single bit; every `ListId` in `schedule`, `lookup_order`, `rect_order`, `block_to_list`, `default_list` and in every role is `< lists.size()`; `schedule`, `lookup_order` and `rect_order` hold no duplicate; `block_size >= 1`; `block_to_list` non-empty. The six roles (`market_list`, `house_list`, `public_list`, `military_list`, `production_list`, `stall_list`) must be valid ids (distinct ids are not required: a mod may fold roles, e.g. stalls into the market list).

**`Rules::validate()`**: see the struct; `prng_mode == PerIsland` and `spatial_order == Original0x10027000` fail with "reserved: not implemented".

`ListLayout` makes the group → list map, the scheduling order and the lookup orders data (D: Create/Attach/Remove tables 0x1002604c, 0x10026034, 0x10025348, 0x10025330; scheduler 0x10023a50; `FindByTile` 0x10023c80; rect query 0x10023f50). Original list numbering `L0..L5` is the object order at `index +0x58 + 0x20·i`.

```cpp
struct ListSpec {
    std::string name;
    std::uint32_t lookup_mask;       // bit used by FindByTile / rect queries
};
struct ListLayout {
    std::vector<ListSpec> lists;
    std::vector<ListId> schedule;       // visiting order of the building scheduler
    std::vector<ListId> lookup_order;   // FindByTile set order
    std::vector<ListId> rect_order;     // rect query order
    std::vector<ListId> block_to_list;  // index (prodtype - first_prodtype) / block_size
    std::uint32_t first_prodtype = 1;
    std::uint32_t block_size = 32;
    ListId default_list = 4;            // the original's table default for numbers outside 1..192 (original-compat decode only)
    /// Roles: which list plays which part in rules that the original hard-wires to a group. Hashed with the layout, validated.
    /// A modded layout therefore changes drought centring, defeat and the house rules visibly instead of silently.
    struct Roles {
        ListId market_list = 5;         // drought centre (§5.8), defeat_pending on removal (§7.3): the market/Kontor group
        ListId house_list = 3;          // house class (byte0e 0x82, house_counts, §4.6)
        ListId public_list = 1;         // service counters
        ListId military_list = 2;
        ListId production_list = 4;     // the default list of Plain-class numbers outside the table
        ListId stall_list = 0;          // never scheduled in the original (market stalls)
    } roles;
    /// nullopt: not a building (p == 0 or beyond the table: roads, walls, scenery are tile objects only; D: Place skips Create when the block start (p - 1) & ~0x1f is above 0xa0, i.e. for p >= 193, 0x10027b32-0x10027b3c).
    [[nodiscard]] std::optional<ListId> list_for(std::uint32_t prodtype) const;
    [[nodiscard]] std::string validate() const;     // §3.1
    static ListLayout original();
};
```
`original()`: roles `market_list = 5, house_list = 3, public_list = 1, military_list = 2, production_list = 4, stall_list = 0`; `lists` L0 `stall` (mask 0x20), L1 `public` (0x01), L2 `military` (0x10), L3 `house` (0x04), L4 `production` (0x08), L5 `market` (0x02). `schedule = {5, 1, 4, 3, 2}` (begin `+0x108, +0x88, +0xe8, +0xc8, +0xa8`; L0 is never scheduled). `lookup_order = {4, 1, 5, 3, 0, 2}` (masks 8, 1, 2, 4, 0x20, 0x10). `rect_order = {1, 3, 4, 2, 0, 5}` (masks 1, 4, 8, 0x10, 0x20, 2). `block_to_list = {4, 5, 1, 3, 2, 0}` for blocks `(p-1)>>5` = 0..5 (p 1..192): 1–32 production, 33–64 market (HAUPT, MARKT, KONTOR, MARKTKOMBI), 65–96 public, 97–128 houses (PIONIER..ARISTOKRATEN = 97..101), 129–160 military, 161–192 stalls (MARKTSTAND = 161). All **D**.

```cpp
// settings.hpp   D: world property getter 0x100a3380, setter 0x100a3230, scene HEAD 0x10009cc0
struct WorldSettings {
    std::uint32_t world_width = 0, world_height = 0;   // HEAD[0..7], world tiles
    std::uint8_t prop0 = 0;                // 0 endless, 1 mission, 2 campaign in the corpus (I)
    std::uint8_t prop1 = 1;                // never read by the sim
    std::uint8_t prop2 = 2;                // difficulty-like (I)
    std::uint16_t prop3 = 2550;            // time-limit-like, 2550 = none (I); original byte stores value/10 (F8)
    bool prop5 = true;                     // mine deposit search, ship code (original stores it inverted)
    std::uint32_t prop6 = 0;               // feature-off mask: 0x02 volcano, 0x04 drought, 0x08 city events, 0x80 event 7.5 min
    std::uint32_t prop7_offset = 0;        // game seconds = tick/500 + offset
    std::uint8_t prop9 = 0;                // AI not loaded when non-zero
    std::uint8_t prop10 = 0;               // tax level 0..2 (economy, CompLogic); validate() rejects larger values
    std::uint32_t prop11 = 0;
    bool prop12 = false, prop13 = false;
    std::uint32_t prop14 = 3;              // bit 1 (0x2) = add-on edition (A11)
    std::uint32_t speed_percent = 100;     // N: the agreed game speed (SetSpeed)
    [[nodiscard]] std::string validate() const;   // §3.1: world_width, world_height <= kWorldEdgeBound, prop10 <= 2
    [[nodiscard]] bool addon() const { return (prop14 & 2U) != 0; }
    [[nodiscard]] bool depletes() const { return addon(); }                      // OQ-035, D 0x10063686
    [[nodiscard]] bool volcano_off() const { return (prop6 & 0x02U) != 0; }
    [[nodiscard]] bool drought_off() const { return (prop6 & 0x04U) != 0; }
    [[nodiscard]] bool city_events_off() const { return (prop6 & 0x08U) != 0; }
    [[nodiscard]] bool event750_off() const { return (prop6 & 0x80U) != 0; }
};
```
Property 4 (tick) and 7 (seconds) derive from the clock; property 8 (PRNG cursor) is `WorldRng::set_cursor`. Which settings sim code reads (D/I): prop 6 as above (bit 5 `0x20` is read by CompLogic only), prop 10 by the economy pass, prop 14 by deposit depletion, event 7.5 min and the AI choice, prop 5 by the mine deposit search; props 0–3, 11–13 are read by UI/AI code only (meaning unestablished, O-29).

## 4. Type declarations

All headers live in `src/sim/include/anno/sim/`, sources in `src/sim/src/`. Listings are the contract; where a member is derived it says so.

### 4.1 `ids.hpp`
```cpp
template <class Tag, class Rep = std::uint32_t>
struct Id {
    using rep = Rep;
    Rep v = std::numeric_limits<Rep>::max();                 // default = invalid; never aliases slot 0
    constexpr Id() = default;
    constexpr explicit Id(Rep x) : v(x) {}
    [[nodiscard]] constexpr bool valid() const { return v != std::numeric_limits<Rep>::max(); }
    friend constexpr auto operator<=>(const Id&, const Id&) = default;
};
using PlayerId = Id<struct PlayerTag>;     // slot in the player table (original u8)
using IslandId = Id<struct IslandTag>;     // slot in the island table (original u8)
using CityNo   = Id<struct CityTag>;       // city number on its island = tile owner value
using TypeId   = Id<struct TypeTag>;       // Anlagen.dat ANLAGEN id (wire: 12 bits in 0x64, u16 records)
using BuildingId = Id<struct BuildingTag, std::uint64_t>;   // (island << 32) + serial
using FigureId = Id<struct FigureTag, std::uint64_t>;       // original: u16 registry slot
using GoodsId = std::uint32_t;             // index of the AnnoGame goods tables (0..0x8e)
using ListId = std::uint32_t;              // index into ListLayout::lists
using Slot = std::uint32_t;                // list slot (original u16, 0xffff none)
inline constexpr Slot kNoSlot = 0xffffffffU;

constexpr BuildingId make_building_id(IslandId island, std::uint32_t serial);
constexpr IslandId island_of(BuildingId);  constexpr std::uint32_t serial_of(BuildingId);

struct CityRef { IslandId island; CityNo city; friend constexpr auto operator<=>(const CityRef&, const CityRef&) = default; };
struct BuildingKey { CityRef city; ListId list = 0; Slot slot = kNoSlot; friend constexpr auto operator<=>(const BuildingKey&, const BuildingKey&) = default; };
struct TileXY { std::int32_t x = 0, y = 0; friend constexpr auto operator<=>(const TileXY&, const TileXY&) = default; };
struct TileRect { std::int32_t x0 = 0, y0 = 0; std::uint32_t w = 0, h = 0; friend constexpr auto operator<=>(const TileRect&, const TileRect&) = default; };
struct IdCounters { std::uint64_t next_figure = 1, next_event_seq = 1; };   // hashed
```

### 4.2 `draw.hpp`, `rng.hpp`: the only door to the PRNG
Every draw site names itself. The table of sites is the single list the parallel executor (§11), the telemetry and the "PRNG timeline" (§6) are derived from; a new site is a new enumerator, and `-Wswitch-enum` makes the predicate table incomplete at compile time.

```cpp
enum class DrawSite : std::uint8_t {
    SmallFarmAnimal,   // D 0x1006f810–0x1006f838: once per step with line 1 on, after the gate, before the limit tests
    Event300,          // D 0x100939b5: once per island per gate, before the climate and city tests, unless bit 0x04 of world property 6 is set (D 0x100939a0–0x100939c6: gate, then `test al,4`)
    Event750Gate,      // D 0x100ae140: draw #1 (r % 10)
    Event750Island,    // D 0x100ae140: draw #2, add-on only, only when #1 % 10 == 0
    // unported (declared so the timeline manifest is complete, never drawn yet):
    CityRandomEvents,  // 0x1002ba80
    WildlifeSweep,     // 0x1009366f (state dependent count)
    Ai,                // CompLogic
    Count_
};
class DrawSource {
  public:
    virtual std::uint16_t next(DrawSite) = 0;
  protected:
    ~DrawSource() = default;
};
struct GlobalDraw final : DrawSource { WorldRng& rng; std::uint16_t next(DrawSite) override { return rng.next(); } };
// S16 only (not part of M3 until then): struct StreamDraw final : DrawSource { WorldRng stream; ... };   // PrngMode::PerIsland, D11: reserved, see below
struct GuardedDraw final : DrawSource { std::atomic<bool> violated; std::uint16_t next(DrawSite) override { violated = true; return 0; } };   // S16
struct DrawRecord { DrawSite site; std::uint32_t count = 0; };      // telemetry; not hashed
```
`production.hpp` does not include the RNG: `small_farm_step` takes `DrawRef{void* ctx; std::uint16_t (*fn)(void*)}`; the driver builds one that forwards to `ctx.draw.next(DrawSite::SmallFarmAnimal)`; tests build one from a `WorldRng`. A default-constructed `DrawRef` is a driver error, but not undefined behaviour: it points to a function returning 0 (and `operator()` also guards a null `fn`).

`WorldRng` holds `std::shared_ptr<const RngTable>` (the u16 values and their digest) and the cursor; copying a `WorldRng` copies a pointer and four bytes, not 4 KiB. **Copy-on-move is kept**: `WorldRng`'s move constructor and move assignment are declared and behave as copies (the table pointer is copied, never nulled), so a moved-from `WorldRng` is still usable and the existing test `sim_test.cpp:82-88` ("a moved-from `WorldRng` still works") passes unchanged; a test pins the table pointer of a moved-from generator as non-null.

`WorldRng::stream(seed, slot)` is a **pure function**, defined here so every implementation agrees: it returns a generator over the same table with cursor `rng_mix(seed, slot) mod len`, where, in `u64` arithmetic, `x = seed + 0x9e3779b97f4a7c15 · (slot + 1)`, `z = (x ^ (x >> 30)) · 0xbf58476d1ce4e5b9`, `z = (z ^ (z >> 27)) · 0x94d049bb133111eb`, `z = z ^ (z >> 31)`, result `static_cast<u32>(z)` (splitmix64 finaliser; integer only). It is tested in S1 but **unused by M3**: `PrngMode::PerIsland` (D11) is design-only. No island carries a stream cursor, the header holds no stream seed, and `Rules::validate()` rejects the value, so there is no unhashed or unsaved stream state anywhere in the state tree. Stage S16 turns the mode on and must, in the same commit, add `WorldRng stream` (cursor only) to `Island` with its field-list entry, hash scope and save entry, add `u64 stream_seed` to the `header` scope, bump `kStateHashVersion` and re-baseline the goldens (§15 S16). The default table is `msvc_rand_sequence(2048, 1)` (core.md); a custom table travels in the scenario and in the save frame (§9.5).

### 4.3 `archive.hpp`: the four archives (one field list, §9)
```cpp
struct HashArchive;   // feeds FNV-1a 64; hierarchical via scope(); optional HashTree*; allocation-free in the hot path
struct WriteArchive;  // appends little-endian bytes
struct ReadArchive;   // fills state from bytes; latches the first error, later reads return 0; never throws or allocates beyond the input
struct TraceArchive;  // test only: records (kind, width, value) of every primitive
// common primitives: u8 u16 u32 u64 i32 i64 boolean count(u64&, min_wire_bytes = 1) str(string&) enumv(E&) bytes(span) scope(ScopeName, [index,] f)
// ar::seq ar::bools ar::opt ar::map ar::variant<Tags>  (helpers; one code path for all archives)
// ScopeName: consteval wrapper of a string literal (a name built at run time does not compile)
template <class S, class T> concept Same = std::same_as<std::remove_const_t<S>, T>;
```
Contract (the compiled part of the incremental design): `count` is a `u64` LE prefix; `ReadArchive` rejects `count > remaining() / min_wire_bytes` before anything is sized (`min_wire_bytes` is the smallest encoding of one element, default 1; a division, so it cannot overflow), and `ar::seq` / `ar::map` then clear the target and fill it one element at a time, stopping at the first error, so the memory taken tracks the bytes consumed and never `count × sizeof(element)` of a lie (a persistent container passes its element's `min_wire_bytes`; strings and bool vectors take one input byte per element); `boolean` bytes `> 1`, enums out of range, duplicate or unordered map keys and variant tags no alternative has fail; the variant tag table is a constant template argument of `ar::variant<Tags>` whose tags are pairwise distinct (`static_assert`; a duplicate would make an alternative unreachable on read), and a valueless variant, reachable only through an exception, is written as its first alternative; `enumv` writes the enum's `u8` underlying value (all state enums are `u8`); `scope()` in `HashArchive` pushes a `StateHasher` (the stack is reserved, no heap use per building) and on close feeds the child's 64-bit digest to the parent; the flat result is identical with or without a `HashTree`. Scope names are `ScopeName`, so the views kept by `HashTree`, `TraceArchive` and the read error path point at literals and cannot dangle; `HashArchive::finish()` does not consume the archive (a second call returns the same digest and tree); a `HashTree` is built only when requested and to a requested depth (§9.3). Reading containers of persistent types (`ar::opt`, `ar::seq`, `ar::map`, `ar::variant`) default-constructs the element and then reads its fields, so **every persistent type is default-constructible** (`TileLayer` and `Island` included, §4.8); there is no per-type `emplace` path. Bulk columns (`tiles_io`, §9.2) are size-checked by the caller against `remaining()` before they are resized (the generic count check does not cover them).

### 4.4 `types.hpp`: immutable type data (built once, shared)
```cpp
enum class TickKind : std::uint8_t {
    None,            // inert: the class tick is not ported (or is a stub); `last` is NOT touched
    Workshop, WeaponsWorkshop, Farm, FurTrapper, Mine, SmallFarm
};
enum class RuntimeClass : std::uint8_t {   // by PRODTYPE (D: byte table 0x1002528c, jump table 0x1002521c); append-only
    Plain, Workshop, DualWorkshop, WeaponsWorkshop, Farm, FurTrapper, Mine, SmallFarm, Quarry,
    Hunter, Whaler, Shipyard, Cannonworks, Market, MarketCombi, MarketStall,
    Inn, PublicService, Doctor, School, Castle, FireBrigade, House, Tower, CannonTower,
    Count_   // number of classes (not a class): the class table has this many entries (S3)
};
enum class PayloadTag : std::uint8_t { Inert = 0, Workshop = 1, WeaponsWorkshop = 2, Farm = 3, SmallFarm = 4, Mine = 5 };   // = the save tag of Building::payload; append-only
struct ClassInfo {
    RuntimeClass cls = RuntimeClass::Plain;
    TickKind tick = TickKind::None;
    PayloadTag payload = PayloadTag::Inert;   // the Payload alternative a building of this class must hold (check_invariants, load); FurTrapper -> Farm, Quarry/Plain/... -> Inert
    std::uint32_t gate_period = 0;   // D: the compare constant of the class tick, `(u16)(t - last) >= period` (0 = no stamp gate: Quarry, Castle; addresses in Appendix C); the update rule of
                                     // `last` is unread for the unported classes, so the scheduler never uses it (D15)
};
struct GoodsInfo { std::string name; std::uint32_t forsch = 0; };           // forsch: BGruppen [WAREN] research id, 0 = none (OQ-034)
struct CostEntry { GoodsId goods = 0; std::uint32_t amount_88 = 0; };

struct BuildingType {
    TypeId id;
    std::uint32_t prodtype = 0;                 // PRODTYPE number (D AnnoGame 0x100138e0)
    RuntimeClass cls = RuntimeClass::Plain;
    std::uint32_t width = 1, height = 1;        // HEAD[0], HEAD[1] (D 0x10009376); swapped on odd rotation
    std::uint32_t research_id = 0;              // FORSCHTYPE name index in table 0x100250f8 (D), 0 = none
    std::uint32_t cost_gold = 0;                // HEAD[8..9]
    std::vector<CostEntry> cost_goods;          // HEAD[5]=WERKZEUG 0x29, [4]=HOLZ 0x2a, [3]=ZIEGEL 0x2b, [6]=MAMOR 0x2c, each << 8 (D 0x1000a5a0)
    std::uint32_t max_hp = 0, can_burn = 0, action_radius = 0, flags_word = 0, rubble = 0;   // HEAD u16 [0x2c], u8 [0x31], u8 [0x11], u16 [0x1c], u8 [0x0a] (widths read from AnnoGame 0x10009334, docs/re/production.md §2.4)
    std::uint32_t service_mask = 0;             // public group: from WARE[0] in 0x4c..0x5a: 1<<(g-0x4c); 0x51→0x30, 0x53→0xc0, 0x54→0x1c0 (D AnnoGame 0x1000a600)
    std::vector<std::uint32_t> running_cost;    // PROD3 0x00/0x02 (size = running-cost stats, 2); the instance index P is O-9
    std::vector<GoodsId> output_goods, input_goods;   // resolved from the first TWO WARE/ROHSTOFF names (the original reads exactly two, D 0x1000a1f4 / 0x1000a2e2; further names are ignored as there); unresolved name = load error
    std::vector<std::uint8_t> head_raw;         // HEAD bytes without a known consumer; kept, never read (O-21)
    bool has_prod = false;
    ProductionType prod;                        // valid when has_prod
};

class TypeRegistry {
  public:
    [[nodiscard]] const BuildingType* find(TypeId) const;            // nullptr = unknown → commands fail closed (F4)
    [[nodiscard]] const ClassInfo& class_info(RuntimeClass) const;   // vector indexed by class, sized from the enum
    [[nodiscard]] const GoodsInfo* goods(GoodsId) const;
    [[nodiscard]] std::optional<GoodsId> goods_by_name(std::string_view) const;
    [[nodiscard]] std::uint32_t max_footprint() const;               // largest of width/height over all types (F3)
    [[nodiscard]] const BuildingType* find_growing(std::uint32_t object) const;   // GrowingItem types by tile object id
    [[nodiscard]] std::uint64_t digest() const;                       // FNV over every field above; hashed once in the header
    [[nodiscard]] static std::uint64_t digest_of(std::span<const BuildingType>, std::span<const GoodsInfo>, std::span<const ClassInfo>);   // the digest of the data as given, unchecked (tests perturb one field at a time)
    [[nodiscard]] std::string validate_against(const SimConfig&) const;   // the config-dependent checks of build() again (build cost goods in build_goods_first .. + build_goods_count, service masks within service_bits); World::create/load and the header read call it with the world's config
    class Builder;                                                    // hand-built registries for tests and the synthetic scenarios
    static formats::Result<std::shared_ptr<const TypeRegistry>, std::string>
    build(std::span<const formats::figuren::Entry>, std::vector<GoodsInfo>, const SimConfig&);   // S3: the config bounds service_mask by service_bits; unknown PRODTYPE / FORSCHTYPE / goods names, duplicate ids and entries without HEAD are load errors naming the entry (F25), and so is every set of types the simulation could not run (see the checks below)
    static std::shared_ptr<const TypeRegistry> empty();   // S3: no types, the original goods and class tables; what World() holds
    [[nodiscard]] std::span<const BuildingType> types() const;   // S3: ascending id
    [[nodiscard]] std::span<const GoodsInfo> goods_table() const;   // S3
    [[nodiscard]] std::span<const ClassInfo> class_table() const;   // S3
  private:
    std::vector<BuildingType> types_;               // ascending id; lookup by binary search
    std::vector<GoodsInfo> goods_;
    std::vector<ClassInfo> classes_;
    std::uint32_t max_footprint_ = 1;
    std::uint64_t digest_ = 0;
};
```
**Registry checks (F25; `finish()`, run by `build()` and by the `Builder`)** so that "payload matches the type" and "a Mine type has `outputs[0]`" (§9.6) hold by construction: (a) every `cost_goods` entry lies in `build_goods_first .. build_goods_first + build_goods_count` of the config (and therefore below `warehouse_goods`; `Place` indexes its build-goods array by it), the loader's goods `0x29..0x2c` being the original's values; (b) a class whose `TickKind` is not `None` needs `has_prod` with `prod.kind` of that tick (a `HANDWERK` entry without a 68-byte `PROD3` is refused instead of silently becoming a type without production data), and a `Mine` needs `prod.outputs[0]`; (c) the class table has `classes[i].cls == i` and pairs each tick with its payload (`None` ↔ `Inert`, `Workshop` ↔ `Workshop`, `WeaponsWorkshop` ↔ `WeaponsWorkshop`, `Farm` and `FurTrapper` ↔ `Farm`, `Mine` ↔ `Mine`, `SmallFarm` ↔ `SmallFarm`), so `tick_building`'s `get_if` cannot miss; (d) `width` and `height` are at least 1 (`HEAD[0]`, `HEAD[1]`); (e) the class of a type is inside the class table. `class_for_prodtype(p, layout)` takes "is a building" from `layout.list_for(p)` (a modded layout with more blocks cannot disagree with the class lookup); the one-argument form is the original layout.

`Builder` has sensible defaults (the original goods and class tables, `SimConfig{}` limits) and PRODTYPE-block helpers (`add_workshop`, `add_weapons_workshop`, `add_farm`, `add_fur_trapper`, `add_small_farm`, `add_mine`, `add_plain`, and `add` for a fully specified type), so synthetic scenarios never touch `formats::figuren::Entry` (CI has no game files); the helpers resolve their goods names when called (an unknown name is reported by `build()`), and `synthetic.hpp` (`make_synthetic_types()`) uses them for the fixture types of §14.3 under the ids 9001..9010. The class table is also served by `class_for_prodtype(p)` (nullopt for 0 and above 192) and `original_class_table()`; `prodtype_by_name`, `prodtype_name`, `research_by_name`, `original_goods()` and `service_mask_for_goods()` are the free functions behind the tables. The `prodtype → RuntimeClass/TickKind/list` table is data in `types.cpp` with Appendix C as its evidence; the goods table (ids 0..0x8e, D AnnoGame 0x10013be0) is data in `goods_table.cpp`, Appendix A; the research names (FORSCHTYPE → id, D 0x10013cf0) Appendix B. The goods names in the files resolve for all 921 entries (P/D).

### 4.5 `settings.hpp` (§3), `player.hpp`
```cpp
enum class PlayerState : std::uint8_t { Alive = 0, Bankrupt = 2, Defeated = 3 };   // D player +0x28d
struct ResearchState {                         // D player +0xb0 object, 65 entries
    std::vector<std::uint8_t> state;           // bit 0 researchable, bit 1 researched (D 0x1001da19–0x1001db4a)
    std::vector<std::uint16_t> progress;       // words at +0xb4+2i (progress << 8)
    [[nodiscard]] bool researched(std::uint32_t id) const { return id < state.size() && (state[id] & 2U) != 0; }
};
struct Player {
    PlayerId index;
    std::uint32_t kind = 8;              // D +0x78: 8, 11 human; 12 computer; 14 natives; 15 pirates; 16 Venice
    std::uint32_t volk = 1;              // D +0x7a (OQ-033 resolved): status icons only for Volk 1
    std::int64_t money = 10000;          // D +0x74 i32, init 10000 (F15)
    std::uint32_t upkeep_a = 0, upkeep_b = 0;        // D +0x88/+0x8c running per-minute sums of owned units
    std::uint32_t upkeep_rem = 0;                    // D +0xa2 (u16 in the original); u32 because upkeep_slices is u32 config: rem < slices (F3)
    std::uint16_t upkeep_stamp = 0;                  // D +0xa4 (a 16-bit stamp: kept)
    std::uint32_t stat_a = 0, stat_b = 0;            // D +0x80/+0x84 decaying statistics
    std::uint32_t flags = 1;             // D +0x90
    std::uint32_t flags_a0 = 0;          // D +0xa0 (bit 0x08: immune to random city events)
    std::uint8_t colour_slot = 0xff, default_colour = 0xff;   // D +0xa8, +0xa9
    PlayerState state = PlayerState::Alive;
    std::uint32_t bankrupt_deadline = 0; // D +0x290, 0 = not running
    bool defeat_pending = false;         // D +0x295, set by Remove of a market-group building (D 0x100255c0)
    bool free_build = false;             // O-9: owner slot +0x9c(7) waives the build cost
    std::vector<std::uint8_t> name;      // bytes; no encoding assumed
    ResearchState research;
};
struct PlayerTable {
    std::vector<std::optional<Player>> slots;    // size = player_slots
    std::uint16_t stamp = 0;                     // D +0x88
    std::uint32_t cursor = 0;                    // D +0x8c
    // +0x8a stamp and the +0x8e ring drive 0x10001020, a bare `ret 8`: no behaviour, not modelled.
};
```

### 4.6 `slot_list.hpp`, `building.hpp`
The slot list is the original group list as a reusable container (also used by the figure registry, A6). Fresh: one null element, `hint = -1`, `cursor = 0` (D ctor 0x100236a0, A7).
```cpp
struct SlotParams { std::uint32_t min_growth = 10; SlotPolicy policy = SlotPolicy::Original; };
template <class T>
struct SlotList {
    std::int64_t hint = -1;                        // D +0x00: last stored slot
    std::uint32_t cursor = 0;                      // D +0x1c scheduler cursor (u16 widened)
    std::vector<std::optional<T>> slots;           // D +0x10 vector; includes null holes; never shrinks
    explicit SlotList(std::uint32_t initial = 1) : slots(initial) {}
    [[nodiscard]] std::uint32_t live() const;      // derived (D +0x08)
    /// Free-slot policy (D 0x10026a00, 0x10026b30), §5.2. Stores `v`, sets hint, returns the slot.
    Slot insert(T v, const SlotParams&);
    /// slots[s].reset(); hint, cursor and size untouched (D 0x100255c0); s >= size is a no-op.
    void remove(Slot s);
};
```

```cpp
// building.hpp
struct Inert {                                     // D: base class 0x100734c0 with the no-op tick 0x10001030, and every class whose tick is unported
    std::uint16_t last = 0;
    bool blocked = false;
    bool suspended = false;
    StatusTracker status;
};
// FurTrapper uses Farm; the class comes from the type. Alternative order = save tag order, append-only.
using Payload = std::variant<Inert, Workshop, WeaponsWorkshop, Farm, SmallFarm, Mine>;

struct Building {
    BuildingId id;
    TypeId type;
    std::int32_t x = 0, y = 0;         // north-west tile (original u16 +6/+8)
    std::uint8_t rotation = 0;         // 0..3, flags +0x0c bits 0–1
    std::uint8_t byte0e = 0x86;        // D +0x0e (0x82 for houses); set by property 0x2b from a COMP chunk; meaning O
    std::uint8_t byte0f = 0;           // D +0x0f (property 4); the original overwrites it from terrain (0x1007c790, O-27)
    Payload payload;
    // +0x04 list slot = the vector index; +0x10 city and +0x14 type = the context; bits 6–7 of +0x0c are
    // uninitialised heap in the original and never read (F2): not modelled.
};
/// What create_building needs; the wire form (PlacementWire, §8.3) converts to it after validation.
struct PlacementRecord { TypeId type; std::int32_t x = 0, y = 0; std::uint8_t rotation = 0; std::uint8_t byte0f = 0; };
using BuildingList = SlotList<Building>;
struct BuildingIndex {                             // D object at city+0x144 (vtable 0x100c2e48)
    std::uint16_t stamp = 0;                       // D +0x04: the ONE stamp shared by the scheduled lists
    std::vector<BuildingList> lists;               // layout.lists.size()
    // derived, rebuilt on load, never hashed:
    std::vector<std::uint32_t> service_counters;   // D +6..+0x45 (size service_bits)
    std::vector<std::uint32_t> house_counts;       // D +0x46.. (size house_groups)
};
```
The production structs are the payloads, unchanged; `last`, `blocked`, `suspended` and `StatusTracker` live in the payload only, so there is no second copy. `Inert` is the payload of every class without a ported tick (markets, public buildings, houses, military, hunters, ships, MARKTSTAND, the base class): it holds a slot, so the scheduler arithmetic (which depends on list length including holes) stays faithful while those classes are unported.

### 4.7 `city.hpp`
```cpp
struct GoodsSetting {                               // D city+0x334+12g: +0x0a bit 0 buy flag, +0x0b limit byte (0x7c modes 0/1)
    bool buy = false;
    std::uint32_t limit = 0;                        // a byte in the original; u32 here, bounded by SimConfig::trade_limit_max (F3)
    std::int32_t list_index = -1;                   // trade-list entry the goods is on (0x73), -1 none
};
struct BuildGoods { std::uint32_t setting = 0; std::uint64_t spent = 0; };   // D 8-byte record at city+0x5d4+8k: setting (0x7c mode 9) and cumulative spend (+0x5d8), k = goods - 0x29
struct TradeEntry { GoodsId goods = 0; std::uint32_t amount_88 = 0; std::uint32_t price = 0; };   // I: 0x73 fields @4/@6 (O-14)
struct City {
    CityNo number;                                   // D [city+4]
    PlayerId owner;                                  // D city slot +0x1c → [city+0x20]
    CityStock stock;                                 // production.hpp; D city+0x334, capacity +0x624
    BuildingIndex index;
    std::uint16_t tick_stamp = 0;                    // D [city+8], set at the end of the city tick
    std::vector<GoodsSetting> goods;                 // size warehouse_goods
    std::vector<BuildGoods> build_goods;             // size build_goods_count
    std::vector<std::vector<TradeEntry>> trade_lists;   // size trade_lists; capacity O-14

    // ---- island-level queries (const; the World members of the same names delegate here). All return canonical order (D2).
    [[nodiscard]] const Building* find_building(const Island&, BuildingId) const;          // via the island's by_id cache (null when the id belongs to another city)
    [[nodiscard]] const Building* building_at(const Island&, std::int32_t x, std::int32_t y, std::uint32_t list_mask = ~0U) const;
    void rect_query(const Island&, const TileRect&, std::uint32_t list_mask, std::vector<BuildingId>& out) const;   // (list in rect_order, y, x, id)
};
```
`City` needs the island's derived `grids`/`by_id` to answer point and rect queries, so the query takes the `Island` (`City::building_at(I, ...)`), and `Island` offers the same names with a `CityNo` argument (§4.8). Both are const, allocation-free except for the caller-provided `out`, and are what `tick_building`, market catchment and coverage code call; `World::find_building`/`building_at`/`rect_query` only locate the island and delegate.
Not modelled: `[city+0x17]` counter (use unknown), `[city+0x61c]` mask (O-40), the resident counter object `+0x2ec` and the running-cost statistics (`city slot +0x70/+0x74`): they are derivable from the lists when a consumer exists (O-9, O-23).

### 4.8 `island.hpp`
```cpp
struct Terrain {                                    // static, shared, hashed by digest; the sim reads none of it yet
    std::vector<std::uint8_t> height;  std::vector<std::uint16_t> gfx;  std::vector<std::uint8_t> fertility;
    std::uint64_t digest = 0;
};
/// D: 8-byte tile record (island +0xc array): u16 object id, flag bytes, owner nibble ((u32@+4)>>25)&0xf, growth phase.
/// Columns, so a chunk digest is one linear pass. EVERY writer goes through a setter that marks the chunk dirty.
class TileLayer {
  public:
    TileLayer() = default;                                             // empty 0x0 layer: every persistent type is default-constructible (§4.3)
    TileLayer(std::int32_t x0, std::int32_t y0, std::uint32_t w, std::uint32_t h, std::uint32_t chunk_edge);
    /// Re-dimension and zero-fill (used by ReadArchive after the dimensions were checked against remaining(), and by the loader).
    /// Fails (returns false, layer unchanged) when w*h overflows u64 or chunk_edge == 0. It sets no size limit of its own: the load path has
    /// already checked w*h*13 <= remaining() in u64, and the loader bounds the island by the world size (§10), so memory is bounded by the input.
    bool reset(std::int32_t x0, std::int32_t y0, std::uint32_t w, std::uint32_t h, std::uint32_t chunk_edge);
    [[nodiscard]] bool contains(std::int32_t x, std::int32_t y) const;
    [[nodiscard]] std::uint32_t object(std::uint32_t idx) const;      // type id of the object on the tile (island slot +0x94)
    [[nodiscard]] std::uint8_t phase(std::uint32_t idx) const;        // GrowthTile::phase
    [[nodiscard]] std::uint32_t owner(std::uint32_t idx) const;       // city number or neutral_owner; u32 (the original nibble is 4 bits, F3), so city_slots/neutral_owner above 65535 cannot truncate
    [[nodiscard]] std::uint32_t aux(std::uint32_t idx) const;         // remaining record bits (flag 0x80 of byte +3, ...): O-11
    void set_object(std::uint32_t idx, std::uint32_t v);              // marks dirty
    void set_phase(std::uint32_t idx, std::uint8_t v);
    void set_owner(std::uint32_t idx, std::uint32_t v);
    void set_aux(std::uint32_t idx, std::uint32_t v);
    [[nodiscard]] std::uint64_t chunk_digest(std::uint32_t chunk) const;   // cached; FNV over object, phase, owner, aux of the in-bounds tiles
    [[nodiscard]] std::uint64_t chunk_digest_uncached(std::uint32_t chunk) const;   // the reference for the cache assertion
    // width(), height(), x0(), y0(), chunk_edge(), chunks_x() = ceil(w/edge), chunks_y()
    // bytes per tile in the save: 4 (object) + 1 (phase) + 4 (owner) + 4 (aux) = 13 (kTileBytes, §9.2)
  private:
    std::vector<std::uint32_t> object_;  std::vector<std::uint8_t> phase_;  std::vector<std::uint32_t> owner_;  std::vector<std::uint32_t> aux_;
    mutable std::vector<std::uint64_t> digest_;  mutable std::vector<std::uint8_t> dirty_;   // derived
};
struct SweepState { std::uint16_t stamp = 0; std::uint32_t grow_x = 0, grow_y = 0, fauna_x = 0, fauna_y = 0; };   // D +0x9a, +0x9c/+0x9e, +0xa0/+0xa2; stamp = world tick at island init (D 0x1008c2e6)
struct CityRing { std::uint16_t stamp = 0; std::uint32_t cursor = 0; };                                      // D island+0x38+0x44, +0x46
struct ObjectManagerState { std::uint16_t stamp = 0; std::uint32_t cursor = 0; };                             // stamp: D manager +0x14. cursor: N stand-in index into Island::deposits (A10); the original cursor is a tile (+0x24/+0x26)
struct Drought {                                    // D object at island+0x84, 0x18 bytes, ctor 0x10087280
    std::uint32_t x = 0, y = 0;                     // multiples of chunk_edge (original 8)
    std::uint32_t target_ring = 6;                  // radius / 8
    std::int32_t ring = 0;
    std::uint32_t city_mask = 0;
    std::uint8_t phase = 0, step = 0;
    std::uint8_t state = 1;                         // 1 expanding, 0 plateau, 0xff retreating
    std::uint16_t counter = 0;                      // plateau counter (1000-tick units)
    std::uint16_t stamp = 0;                        // u16 tick, catch-up gate (§5.8)
};
struct IslandIndexCache {                           // DERIVED: rebuilt on load, never hashed or saved
    std::map<BuildingId, BuildingKey> by_id;
    std::vector<std::vector<std::uint32_t>> grids;  // per ListId: building serial per tile, 0 none; empty until the list has a building
};
struct Island {
    Island() = default;                             // default-constructible (§4.3); an Island is only valid after World::create/load
    IslandId index;
    TileLayer tiles;
    std::shared_ptr<const Terrain> terrain;         // may be null
    std::uint32_t climate = 0;                      // D +0xc0 (property 7): 0 POLAR 1 TUNDRA 2 NORD 3 PRAERIE 4 DSCHUNGEL 5 STEPPE 6 WUESTE
    std::vector<Deposit> deposits;                  // append-only (production.hpp)
    std::uint32_t next_serial = 1;                  // building serial counter
    SweepState sweep;
    std::uint16_t event300_stamp = 0;               // D +0xa4: game-seconds stamp
    std::optional<Drought> drought;
    ObjectManagerState objmgr;
    CityRing ring;
    std::vector<std::optional<City>> cities;        // size city_slots
    std::uint64_t reserved_object_count = 0;        // flames, plague, volcano, wildlife (O-12): 0 until ported; keeps the stream stable
    IslandIndexCache cache;

    // ---- const queries (the World members of the same names locate the island and delegate here), canonical order (D2)
    [[nodiscard]] const Building* find_building(BuildingId) const;                                   // by_id
    [[nodiscard]] const Building* building_at(CityNo, std::int32_t x, std::int32_t y, std::uint32_t list_mask = ~0U) const;   // footprint grids, layout.lookup_order
    void rect_query(CityNo, const TileRect&, std::uint32_t list_mask, std::vector<BuildingId>& out) const;                // (list in rect_order, y, x, id)
    [[nodiscard]] std::uint32_t tile_index(std::int32_t x, std::int32_t y) const;                    // row-major index or kNoSlot when outside
};
```
`Island::deposits` stands in for the island object list for the one class that matters now (mines). The general island-object container is O-12; its slot in the stream is reserved.

### 4.9 `figure.hpp` (skeleton)
```cpp
struct FixedPos { std::int32_t x = 0, y = 0; };      // 1/256 tile (Q24.8); the original is x87 float with no oracle (D13)
struct Figure {
    FigureId id;
    std::uint32_t kind = 0;            // FigurenHeader id / world figure kind (small farm animals: base 106 + i)
    PlayerId owner;
    BuildingId home;                   // the building whose counter this figure holds
    std::uint32_t home_counter = 0;    // which counter of the home building (Workshop carriers_out, Farm workers_out, SmallFarm figures_out[i])
    std::uint32_t job = 0;             // original job code: 0x2e small-farm animal, 0x0d fur trapper; 0 unknown (O-8)
    FixedPos pos;
    GoodsId cargo_goods = 0; std::uint32_t cargo_88 = 0;
    std::uint32_t hp = 0;
    std::uint32_t born = 0;            // t at spawn (the interim return rule D12 reads it)
    std::optional<std::uint16_t> created_stamp;   // present = on the 7500-tick expiry list (D property 0x23); the default spawn does not set it (O-8)
    bool marked_for_delete = false;                // on the deferred-delete list (D phase D)
};
struct FigureRegistry {                           // D unit registry, world +0x114, vtable 0x100ccd74
    SlotList<Figure> list;                        // D vector of 16-byte entries; slot policy assumed = list policy (A6)
    std::uint16_t stamp = 0;                      // D +0x20
    std::vector<FigureId> expiry;                 // D list +0x48, insertion order
    std::vector<FigureId> deferred;               // D list +0x58
    std::map<FigureId, Slot> by_id;               // DERIVED
};
```
Phase A of the original registry tick (sector re-bucketing of float positions) is a derived spatial structure, not state.

### 4.10 `events.hpp`, `timers.hpp`
```cpp
enum class EventKind : std::uint8_t { TileCleanup = 0, DelayedHit = 1 };      // exactly two exist (D 0x100add90; enqueue sites 0x10063cf1, 0x100943d5, 0x1003593e)
struct TimedEvent {
    EventKind kind = EventKind::TileCleanup;
    std::uint32_t subtype = 4;         // kind 0: 4; kind 1: 2 (unit) or 4 (tile). The enqueue 0x100adef0 also accepts subtype 1 (three f32 at payload +0x18..+0x20, 0x100adf92..0x100adfa4), which this struct cannot hold; whether it is ever enqueued is open (OQ-054)
    std::uint32_t value = 0;           // kind 1: damage
    TileXY tile;                       // kind 0, kind 1 subtype 4
    FigureId target;                   // kind 1 subtype 2
    std::uint32_t attacker_kind = 0;   // payload +0x1c (2 = unit)
    FigureId attacker;                 // payload +0x20
};
struct EventQueue {
    std::map<std::pair<std::uint64_t, std::uint64_t>, TimedEvent> q;   // key (due, seq)
    /// due = tick + delay; seq = next_event_seq++ (assigned in the serial merge). D 0x100adef0.
    void enqueue(IdCounters&, std::uint64_t tick, std::uint64_t delay, TimedEvent);
    bool cancel_tile(EventKind, std::uint32_t subtype, TileXY);        // D 0x100ade40: first match in queue order; kind equal, stored subtype equal AND the argument == 4 (0x100ade79), tile equal (u16 pair 0x100ade7e..0x100ade95); any other subtype returns false
};
struct ScriptTimers {                 // D SKTIMER, world +0x69c8, vtable 0x100cb8c4, tick 0x1009cd30
    std::uint32_t last_tick = 0;
    std::vector<std::uint8_t> active;     // size script_timers
    std::vector<std::uint32_t> counter;   // ticks; seconds = counter / 500
};
struct MonopolyEntry { std::uint32_t time_s = 0; std::optional<PlayerId> player; };
struct MonopolyTable { std::vector<MonopolyEntry> entries; std::uint32_t acc = 0; };   // D world+4.., accumulator +0xe8
```
The original stores ticks in 32 bits; `due` is `u64` (F-time).

### 4.11 `tick.hpp`: read-only context and the effect buffer
```cpp
class PlayersView {                  // read-only; deliberately has NO money accessor (money changes only through Effects::money)
  public:
    [[nodiscard]] bool present(PlayerId) const;
    [[nodiscard]] std::uint32_t volk(PlayerId) const;
    [[nodiscard]] bool researched(PlayerId, std::uint32_t research_id) const;
    [[nodiscard]] bool free_build(PlayerId) const;
    [[nodiscard]] PlayerState state(PlayerId) const;
};
class IslandsView {                  // read-only, STATIC facts only: nothing here is written by any island tick, so a parallel island phase cannot race on it
  public:
    [[nodiscard]] std::uint32_t count() const;                       // islands_.size()
    [[nodiscard]] bool present(IslandId) const;
    [[nodiscard]] TileRect rect(IslandId) const;                     // x0, y0, w, h
    [[nodiscard]] std::uint32_t climate(IslandId) const;
    [[nodiscard]] std::optional<IslandId> island_at(std::int32_t x, std::int32_t y) const;   // which island contains a world tile (derived grid over the rects)
};
struct TickContext {
    std::uint64_t tick = 0;
    std::uint32_t t = 0;                       // low 32 bits: every gate and stamp uses this
    const SimConfig& cfg;  const Rules& rules;  const WorldSettings& settings;  const TypeRegistry& types;
    PlayersView players;
    IslandsView islands;
    DrawSource& draw;                          // the only door to the PRNG; the global stream in M3 (serial phases and island ticks alike)
    const Seams& seams;
};
struct SpawnRequest {                // a figure the building asks for; the building's own counter was already raised in the tick
    BuildingId home; CityRef city; std::uint32_t home_counter = 0; std::uint32_t job = 0;
    std::uint32_t figure_kind = 0; PlayerId owner; FixedPos pos;
};
struct ReleaseCounter { BuildingId home; std::uint32_t home_counter = 0; };   // a figure ended
struct MoneyDelta { PlayerId player; std::int64_t delta = 0; };
struct EnqueueEvent { std::uint64_t delay = 0; TimedEvent e; };
// Goods traffic of the serial figure phase (carts and carriers; docs/re/production.md §5). Applied at merge, in vector order:
struct GoodsMove { CityRef city; GoodsId goods = 0; std::int64_t delta_88 = 0; };              // signed change of the city stock in 8.8 tons; applied with CityStock::add/remove (an add beyond capacity is clipped, a remove below zero is clipped, as `CityStock` already does)
struct Reserve { FigureId figure; BuildingId building; std::uint32_t out_index = 0; std::uint32_t amount_88 = 0; };   // a cart takes up to amount_88 from the building's output store `out_index`; the amount actually taken is ADDED to the figure's cargo (cargo_goods must already match or be 0)
struct Deliver { FigureId figure; CityRef city; };                                              // moves the figure's whole cargo into the city stock (add; what does not fit stays in the cargo)
enum class SimEventKind : std::uint8_t { StatusMessage, Notice, BlockedChanged, FigureSpawned, Event15a, TimerExpired, CommandRejected };
struct SimEvent { SimEventKind kind = SimEventKind::StatusMessage; std::uint64_t tick = 0; PlayerId player; BuildingId building; FigureId figure; std::uint32_t code = 0; std::int32_t x = 0, y = 0; };
struct Effects {                     // filled by an island tick or a serial-phase seam, merged by World (§5.3, §5.9)
    std::vector<SpawnRequest> spawns;
    std::vector<ReleaseCounter> releases;
    std::vector<MoneyDelta> money;
    std::vector<GoodsMove> goods_moves;   // serial figure phase and later city passes
    std::vector<Reserve> reserves;
    std::vector<Deliver> deliveries;
    std::vector<EnqueueEvent> events;
    std::vector<SimEvent> ui;
    std::vector<DrawRecord> draws;   // telemetry and the MayDraw soundness check
};
/// The world as a serial-phase seam sees it (event handlers, figure behaviour, player passes, the AI): const queries over the whole
/// state plus an Effects buffer. No write other than to the figure/player being ticked, and the buffer, is possible; merge happens
/// right after the seam returns (C2). Island-level queries are the ones of §4.7/§4.8.
class SerialWorld {
  public:
    [[nodiscard]] const Island* island(IslandId) const;   [[nodiscard]] const City* city(CityRef) const;
    [[nodiscard]] const Building* find_building(BuildingId) const;
    [[nodiscard]] const Building* building_at(CityRef, std::int32_t x, std::int32_t y, std::uint32_t list_mask = ~0U) const;
    void rect_query(CityRef, const TileRect&, std::uint32_t list_mask, std::vector<BuildingId>& out) const;
    [[nodiscard]] const Figure* find_figure(FigureId) const;   [[nodiscard]] const PlayerTable& players() const;
    Effects& fx;
};
struct SimOutput { std::vector<SimEvent> events; std::vector<DrawRecord> draws; std::uint32_t rng_cursor_before = 0, rng_cursor_after = 0; };   // cleared at the start of every advance; NOT hashed
```
Constraint C1: applying an effect after the island tick equals the original's synchronous application as long as no later code of the same island tick reads the written field. Money is write-only from island code; figure creation and event enqueue are not read inside island ticks; a future reader of a written field must be added to the effect protocol (and is a review item).

Constraint C2 (serial phases): a serial-phase seam (figure behaviour, event handler, player pass, AI) writes only to the object it is called for and to `SerialWorld::fx`; the buffer is merged **immediately after that seam call returns** and before the next object is visited (figure registry order, event order, player order). Hence every call sees the effects of all earlier calls, as in the original's synchronous execution, and the merge order is part of the model. Merge order inside one `Effects`: `spawns`, `releases`, `money`, `goods_moves`, `reserves`, `deliveries`, `events`, `ui`; each vector in order. A failed `Reserve` (building gone, nothing in the store) takes 0 and is not an error. New writes needed by a ported body (removing a building, setting a tile, claiming land) are new `Effects` entries added with that body; the seam never receives a mutable `World`.

### 4.12 `seams.hpp`: extension points as named function pointers
*Implemented in S3 with the types in the signatures only forward-declared (`TickContext`, `Island`, `City`, `Effects`, `SerialWorld`, `Figure`, `Player`, `Command`, `Building`, `BuildingType`, `PlacementRecord`, and `enum class Reject : std::uint16_t;`); the stages that own them (S4..S10) define them. `Seams::city_pass` defaults to `kOriginalCityPassCount` null seams; `Seams::sized_for(cfg)`, `seam_names(seams)` (the hashed list: `city_pass` first, then the other 14 seams in declaration order) and `validate_seams(seams, cfg)` are inline in the header.*
Seams are configuration, not state: installed at `World::create`, copied with the `World`, and their **names** are hashed in the header scope (a replay recorded with one implementation cannot silently run with another). The name is the only identity the hash sees, so an installed function must have a name of its own: not empty and not `"default"` (`kDefaultSeamName`, §3.1; `validate_seams` refuses it), and two different implementations must not share a name. A `nullptr` function is the documented default (faithful where known, inert otherwise). Signatures take contexts, never a mutable `World&`: island-phase seams get `TickContext` + `Island&`/`City&` + `Effects&`, serial-phase seams get `TickContext` + `SerialWorld&` (const queries + `Effects&`, C2). Every seam receives the `TickContext` and therefore the `DrawSource` (the only door to the PRNG, so `DrawSite::Ai` and the event handlers draw through it), and a `void* ctx` that carries the seam's own configuration (recording seams in tests, an AI's parameter block). `ctx` is **not state**: it is not hashed, saved or copied deep; a production seam keeps immutable data behind it, a test seam may record into it, and a seam must not let anything behind `ctx` influence sim state (a review item; the seam's name is what is hashed). This replaces the bare function pointers of revision 1, which could not carry a recording seam without a mutable global (forbidden by the lint).
```cpp
template <class Fn> struct Seam { const char* name = kDefaultSeamName; Fn fn = nullptr; void* ctx = nullptr; };   // call: seam.fn(seam.ctx, ...); the typedefs below show the parameters after ctx
struct Seams {
    using CityPass   = void (*)(void* ctx, const TickContext&, Island&, City&, Effects&);
    std::vector<Seam<CityPass>> city_pass;                   // size city_pass_count: D 0x10027400, 0x10029610, 0x1002d350, 0x10029950 (economy), 0x1002b670, 0x1002ba80 (random events, PRNG), 0x1002a5e0, 0x1002d3b0
    using ChunkPass  = void (*)(void* ctx, const TickContext&, Island&, std::uint32_t x, std::uint32_t y, Effects&);
    Seam<ChunkPass> wildlife_sweep;                          // D 0x10093480 (O-12)
    using IslandPass = void (*)(void* ctx, const TickContext&, Island&, Effects&);
    Seam<IslandPass> object_manager;                         // D 0x10095d40 object bodies (O-12)
    Seam<IslandPass> drought_tiles;                          // D 0x1008aeb0 tile effects (O-15)
    using FigureTick = void (*)(void* ctx, const TickContext&, SerialWorld&, Figure&);   // serial figure phase (§5.9): a cart reaches buildings, the city stock and tiles through the const queries
    Seam<FigureTick> figure_tick;                            // D unit slot +0x74 (O-8); goods traffic via Effects::reserves / deliveries / goods_moves
    using PlayerPass = void (*)(void* ctx, const TickContext&, SerialWorld&, Player&);
    Seam<PlayerPass> player_groups;                          // D 0x10062a40 (O-20)
    Seam<PlayerPass> defeat;                                 // D 0x10020550 (O-22)
    using Ai = void (*)(void* ctx, const TickContext&, const SerialWorld&, std::vector<Command>& out);   // draws through ctx.draw (DrawSite::Ai)
    Seam<Ai> ai;                                             // CompLogic, every advance (O-1); commands applied at once in order, NOT logged (A15, §8.2)
    using EventHandler = void (*)(void* ctx, const TickContext&, SerialWorld&, const TimedEvent&);   // runs inside the serial event phase
    Seam<EventHandler> tile_cleanup, delayed_hit;            // D 0x100adae0, 0x100adb80 (O-18, O-19)
    using PlaceHook  = void (*)(void* ctx, const TickContext&, Island&, City&, Building&, Effects&);
    Seam<PlaceHook> on_placed, on_removed;                   // O-4, O-5: PRODTYPE hooks 0x44/0x49/0x4e/0x41, land claim, goods handlers
    using PlacementRuleFn = Reject (*)(void* ctx, const Island&, const BuildingType&, const PlacementRecord&);   // Reject lives in reject.hpp (§8.3)
    Seam<PlacementRuleFn> placement_rule;                          // D 0x1007c790 (O-27); default §8.5
    using Upgrade    = void (*)(void* ctx, const BuildingType&, PlacementRecord&);
    Seam<Upgrade> upgrade_rewrite;                           // D 0x100276a0 for PRODTYPE 0x43/0x48 (O-28)
    using CityEmpty  = bool (*)(void* ctx, const City&);
    Seam<CityEmpty> city_is_empty;                           // D city slot +0x78 = 0x1002f7f0 (O-6), default false
};
```

### 4.13 Commands, replay, scenario
Declared in §8.3 (`command.hpp`), §8.7 (`replay.hpp`) and §10 (`scenario.hpp`).

### 4.14 `world.hpp`
```cpp
// Development counter until the stream is complete (stage S15), then FROZEN as 2 (the first released stream). Values 100, 101, ... are
// development versions: the counter is bumped with every change of the field list (every stage S1..S14 changes it), goldens made
// at a development version carry the stage and the version they were made at (§14.3), and all of them are re-baselined in the one
// commit that sets the value to 2. 1 is the legacy core_hash. After S15 every field-list change is version 3, 4, ...
inline constexpr std::uint32_t kStateHashVersion = 100;
inline constexpr std::uint32_t kPrngTimelineVersion = 1;   // §6

class World {
  public:
    World();                                   // empty world, original config, empty registry (tests)
    explicit World(std::uint32_t seed);        // as before: cursor = seed mod len
    /// Both call SimConfig::validate(), Rules::validate(), ListLayout::validate() and the seam consistency checks (§3.1) first, before any state is built.
    static formats::Result<World, std::string> create(const ScenarioSpec&, std::shared_ptr<const TypeRegistry>,
                                                      SimConfig = {}, Rules = {}, Seams = {});
    static formats::Result<World, std::string> load(std::span<const std::uint8_t>, std::shared_ptr<const TypeRegistry>, Seams = {});

    // ---- the three doors
    ApplyResult apply(const Command&);                         // §8
    std::uint64_t advance(std::uint32_t game_ms);              // §5; returns the new tick; game_ms == 0 is NOT special-cased (D)
    // create() above

    // ---- const views
    const WorldClock& clock() const;  const WorldRng& rng() const;  const WorldSettings& settings() const;
    const SimConfig& config() const;  const Rules& rules() const;   const TypeRegistry& types() const;
    const std::vector<std::optional<Island>>& islands() const;      const PlayerTable& players() const;
    const FigureRegistry& figures() const;  const EventQueue& events() const;
    const Island* island_at(std::int32_t x, std::int32_t y) const;  // derived grid over island rects
    const Building* find_building(BuildingId) const;
    const Building* building_at(CityRef, std::int32_t x, std::int32_t y, std::uint32_t list_mask = ~0U) const;
    void rect_query(CityRef, const TileRect&, std::uint32_t list_mask, std::vector<BuildingId>& out) const;   // canonical order, D2
    std::vector<SimEvent> drain_events();                       // UI output; not state
    bool faulted() const;                                       // draw-guard violation (§11, S16); hashed; always false in M3

    // ---- hash, save, load
    std::uint64_t state_hash() const;                           // hierarchical, kStateHashVersion (§9)
    std::uint64_t state_hash(HashTree&, std::uint32_t depth) const;
    std::uint64_t core_hash() const;                            // the v1 five-field FNV (core.md): keeps the old golden checkable
    std::vector<std::uint8_t> save() const;
    std::string check_invariants() const;                       // empty = all hold (§9.6)

  private:
    friend struct Scheduler;  friend struct Commands;  friend struct StateIO;  friend struct WorldTestAccess;
    WorldClock clock_;  WorldRng rng_;  WorldSettings settings_;  SimConfig config_;  Rules rules_;  Seams seams_;
    std::shared_ptr<const TypeRegistry> types_;
    IdCounters ids_;
    PlayerTable players_;
    std::vector<std::optional<Island>> islands_;
    EventQueue events_;  FigureRegistry figures_;  ScriptTimers timers_;  MonopolyTable monopoly_;
    std::uint32_t event750_acc_ = 0;
    bool faulted_ = false;
    std::vector<SimEvent> out_events_;  SimOutput out_;           // not state
};
```
`World` is copyable. During stages S1–S7 the members `clock`/`rng` stay public (existing tests); S8 closes them. Since S3 the settings, config, rules, seams and registry members are private (`friend struct StateIO` for the field list, `friend struct WorldTestAccess` for tests, defined in `tests/world_access.hpp`) with const accessors `settings()`, `config()`, `rules()`, `seams()`, `types()`.

## 5. `World::advance`: the scheduler, exact

Everything here is **D** unless a line says otherwise. All subsystems take `t`, the low 32 bits of the tick; stamps are `u16`; differences are `static_cast<u16>(t - stamp)`.

### 5.1 Building blocks (`scheduler.hpp`, header-only, unit-tested before `World` uses them)
```cpp
/// Visits per call of a stamp-driven round-robin list: min(d*n/div + 1, n), d = (u16)(t - stamp).
/// D 0x10023780: imul 0x10624dd3 >> 37 (/500), +1, cmp n.  Same shape: city ring (n = 16, div 50, D 0x100969d0),
/// player table (n = 32, div 50, D 0x1009c4d0), island sweeps, object manager (div 5000, D 0x10095d40).
constexpr std::uint64_t visit_count(std::uint32_t t, std::uint16_t stamp, std::uint64_t n, std::uint32_t div) {
    const auto d = static_cast<std::uint64_t>(static_cast<std::uint16_t>(t - stamp));
    const std::uint64_t c = d * n / div + 1;
    return c < n ? c : n;
}
/// The unit registry's variant: clamp(d*n/div, 1, n) with NO "+1" (D 0x1009aa4c–0x1009aa77: imul 0x51eb851f, sar 4, then
/// compare with 1 and n; this is phase B of the figure registry's tick, /50 = figure_b_div). Phase A (D 0x1009a8d5–0x1009a907, imul
/// 0x10624dd3, sar 4 = /250 = figure_a_div, n = word [obj+0x36]) has the same clamp but is the sector re-bucketing, derived spatial data
/// (§4.9); it does not use this function. div == 0 returns n (the validation rejects it).
constexpr std::uint64_t visit_count_clamped(std::uint32_t t, std::uint16_t stamp, std::uint64_t n, std::uint32_t div);

/// One scheduled list: visit(T&) is called for each non-null slot reached; the cursor ends where the original's does.
template <class T, class Visit>
void run_slot_list(SlotList<T>& l, std::uint16_t stamp, std::uint32_t t, std::uint32_t div, Visit&& visit) {
    const std::uint64_t n = l.slots.size();                  // includes null holes; cached before the loop
    if (n == 0 || l.cursor >= n) return;                     // A8: cursor < n in a consistent state; otherwise a defined no-op that leaves the list for check_invariants
    std::uint64_t cur = l.cursor;
    for (std::uint64_t i = visit_count(t, stamp, n, div); i > 0; --i) {
        if (cur < l.slots.size() && l.slots[cur]) visit(*l.slots[cur]);   // looked up every iteration (D: begin re-read)
        if (++cur >= n) cur -= n;
    }
    l.cursor = static_cast<std::uint32_t>(cur);
}

/// The lists of one city in scheduling order (D 0x10023a50 with 0x10023810): every list named by `schedule`, in that
/// order, gets run_slot_list with the same stamp; visit(ListId, T&). The ONE shared stamp is written after the last
/// list, so every list sees the same d. A list not in `schedule` (the stalls) is never visited; an id beyond `lists` is
/// skipped (ListLayout::validate keeps schedules in range). `building_scheduler` (§5.5) is this call with
/// `C.index.lists`, `layout.schedule` and `tick_building`.
template <class T, class Visit>
void run_scheduled_lists(std::vector<SlotList<T>>& lists, std::span<const ListId> schedule, std::uint16_t& stamp,
                         std::uint32_t t, std::uint32_t div, Visit&& visit);

/// Timed-event pop (D 0x100add90): the next node is saved before dispatch, so a node a handler enqueues in front of it (key
/// order) is not seen until the next advance, one behind it is; a saved node removed by the handler ends the loop (the
/// original would follow a freed pointer). Rules::events_drain_new re-reads begin() after every dispatch. D: the event is out of
/// the queue before its handler runs (0x100addb3 copies the payload, 0x100addc4 saves the next node, 0x100addd1..0x100addf2 unlink,
/// 0x100addf5 frees, then the kind switch calls 0x100adae0 / 0x100adb80; other kinds are dropped), so a handler's cancel_tile on its
/// own tile finds nothing; the saved node is found again by its key (F19). F24: at most `max_dispatch` (Rules::events_max_dispatch)
/// events per call; at the cap the loop stops and PopResult::capped says so (the rest stays queued). Since S2 `events.hpp`
/// holds EventKind/TimedEvent/EventKey/EventQueue with the inline enqueue and cancel_tile.
struct PopResult { std::uint64_t dispatched = 0; bool capped = false; };
template <class Dispatch> PopResult pop_due(EventQueue&, std::uint64_t now, bool drain_new, Dispatch&&, std::uint64_t max_dispatch);
```
The valid range of the original arithmetic is `d·n < 2³¹`; the widened version is checked against a literal transcription of the instruction sequence (§14).

### 5.2 Slot allocation, replicated exactly (D 0x10026a00, 0x10026b30, 0x100255c0)
```cpp
template <class T> Slot SlotList<T>::insert(T v, const SlotParams& p) {
    const std::uint64_t n = slots.size();                              // incl. holes
    std::uint64_t slot = n;  bool found = false;
    if (p.policy == SlotPolicy::LowestFree) {                          // D1 opt-in
        for (std::uint64_t i = 0; i < n && !found; ++i) if (!slots[i]) { slot = i; found = true; }
    } else {
        for (std::uint64_t i = 0; i < n && !found; ++i) {              // scan starts at hint + 1, wraps; first null wins
            const std::uint64_t idx = static_cast<std::uint64_t>(hint + 1 + static_cast<std::int64_t>(i)) % n;
            if (!slots[idx]) { slot = idx; found = true; }
        }
    }
    if (!found) {                                                      // grow, zero filled; slot = old n
        std::uint64_t grown = n + n / 4;
        if (grown < p.min_growth) grown = p.min_growth;                // here n < min_growth (grown >= n)
        if (grown <= n) grown = n + 1;                                 // F22: n / 4 == 0 below 4 elements, so min_growth 1..3 would not grow
        slots.resize(grown);
    }
    slots[slot] = std::move(v);  hint = static_cast<std::int64_t>(slot);  // hint = last stored slot
    return static_cast<Slot>(slot);
}
// remove: slots[s].reset(); hint untouched; vector never shrinks or compacts.
// SimConfig::validate() requires list_min_growth >= 1; with the F22 line the insert is safe for any value, 0 included.
```
A fresh list grows 1 → 10 → 12 → 15 → 18 → 22 → 27 → 33 → 41 → 51 → 63 → 78 → 97. The first version of this section wrote the growth as `if (grown < min_growth) grown = (n < min_growth) ? min_growth : n + 1`, whose second branch cannot be reached and which left a list of 1 to 3 elements ungrown for `list_min_growth` 1..3 (slot `n` outside the vector): F22, found by the S2 test "slot list growth never leaves the new slot outside the vector". The scan start is `(hint + 1) mod n` computed in `u64` before the walk, so a corrupt `hint` (the loader rejects it, §9.6) is defined and the walk still visits every slot once. A freed slot is reused only when the wrap-around scan from `hint + 1` reaches it (not the lowest free slot), and the scheduler's `n` counts every hole: the order in which buildings tick depends on the whole create/remove history. This is state (`hint`, `cursor`, the slot vector, the shared stamp) and is hashed. The O(n) scan is the reference; an implementation may add a free bitmap if a model test against this loop passes. The explicit-slot path (`Attach`/`Put` with slot ≥ 0, D 0x100259c0, 0x10026b30, overwrites without a check) has no shipped caller (O-2) and is not provided.

### 5.3 `World::advance` (D 0x100ae040)
```
advance(game_ms):                                   // no early return for 0: the original adds 0 and still runs every subsystem
    out_.clear()
    tick = clock_.begin(game_ms)                    // ms += game_ms (u64); returns tick = ms >> 1; last_tick keeps the previous tick
    t = u32(tick)
    1. for i in 0 .. islands_.size()-1: if islands_[i]:                // D 0x100a2e60: table order, nulls skipped
           Effects fx;  island_tick(ctx(tick, t, draw_for(i)), *islands_[i], fx);  merge(fx)    // merge order: spawns, releases, money, goods_moves, reserves, deliveries, events, ui
    2. events_dispatch(tick)                        // D 0x100add90, §5.7; each handler gets a SerialWorld, its Effects are merged right after it returns (C2)
    3. figures_tick(t)                              // D 0x1009a8a0 via world +0x114, §5.9; Effects merged after EACH figure call (C2)
    4. players_tick(t)                              // D 0x1009c4d0 via +0x118; AI hook every advance, §5.10
    5. timers_tick(t)                               // D 0x1009cd30 via +0x69c8
    6. monopoly_refresh(game_ms)                    // D 0x1009d2a0 (this = world+4)
    7. event750(game_ms)                            // D 0x100ae140
    clock_.end()                                    // last_tick = tick (D 0x100ae126)
    return tick
```
`WorldClock::advance(ms)` keeps its contract (`begin` then `end`). Steps 6 and 7 are the only places that use the step in milliseconds rather than ticks. `merge(fx)` is serial and in island order, so figure ids (`ids_.next_figure++`), registry slots and event sequence numbers equal those of the original's synchronous order (C1, §4.11): spawns create figures (registry slot by the list policy, `FigureSpawned` UI event), releases decrement the named counter of the home building if it still exists, money deltas add (`i64`), event requests call `EventQueue::enqueue(ids_, tick, delay, e)`, UI events append to `out_events_`.

### 5.4 Island tick (D 0x1008b760, arg `t`, low 16 bits used)
```
island_tick(ctx, I, fx):
    chunks = I.tiles.chunks_x() * I.tiles.chunks_y()                       // A4
    d = u16(t - I.sweep.stamp);  prod = chunks * d
    n1 = min(prod / grow_period + 1, chunks)                               // = sweep_chunks(); 0 when chunks == 0 (A1)
    repeat n1: grow_chunk(ctx, I, I.sweep.grow_x, I.sweep.grow_y);   advance_cursor(I.sweep.grow_x, I.sweep.grow_y)     // D 0x1008b5f0
    n2 = min(prod / fauna_period + 1, chunks)
    repeat n2: seams.wildlife_sweep(ctx, I, fauna_x, fauna_y, fx);   advance_cursor(I.sweep.fauna_x, I.sweep.fauna_y)   // D island slot +0x194 = 0x10093480; PRNG (O-12)
    object_manager_tick(ctx, I, fx)                                        // D [island+0x28] slot +0x20 = 0x10095d40, §5.6
    // [island+0x30] message manager slot +0x10 (0x100793b0): **I/O, NOT modelled (O-52)**. Only its scheduling shell was read: a /50-paced list
    //   that calls each item's virtual slot +0x60(t) and a removal helper 0x10079270. The item tick bodies are unread, so "UI only" and "no PRNG
    //   draw" are NOT established; messages also carry event 0x15a, which script code consumes. Until O-52 is read, a PRNG draw or a state
    //   write in an item tick would be a further gap in the PRNG timeline (§6).
    city_ring_tick(ctx, I, fx)                                             // D 0x100969d0, §5.5
    event_300s(ctx, I, fx)                                                 // D island slot +0x190 = 0x10093980, §5.8
    drought_tick(ctx, I, fx)                                               // D 0x1008aeb0, §5.8
    I.sweep.stamp = u16(t)                                                 // D [+0x9a]
advance_cursor(x, y):                                                      // A3
    x += chunk_edge;  if x >= width { x = 0;  y += chunk_edge;  if y >= height { y = 0 } }
```
The base island class (vtable 0x100ca19c, ctor 0x1008c970) has no-op stubs at slots `+0x190`/`+0x194`; the in-game class is 0x100cac2c (slots 0x10093980/0x10093480): earlier notes that read 0x100ca19c saw the stubs. `grow_chunk` visits the chunk's **in-bounds** tiles row-major (A2); the original visits a fixed 8×8 block with index `y·width + x` and no bounds check, which aliases the next row or reads past the array on an island whose width or height is not a multiple of 8, so the clip is a deviation (A14, F20: fix) and the corpus test counts such islands (§14.5). For each tile whose `object` is a growing type (`types.find_growing`) it calls the existing `grow(GrowthTile{object, phase}, prod, t)` and writes the changed `object` and `phase` through the dirtying setters (successor id = `object + 1`, existing contract, O-13).

### 5.5 City ring, city tick, building scheduler
```
city_ring_tick(ctx, I, fx):                          // D 0x100969d0: 16 city pointers, u16 stamp +0x44, u8 cursor +0x46
    n = I.cities.size()                              // city_slots
    count = visit_count(t, I.ring.stamp, n, city_ring_div)                 // min(d*16/50 + 1, 16), always >= 1
    repeat count: if I.cities[I.ring.cursor]: city_tick(ctx, I, *city, fx);   I.ring.cursor = (I.ring.cursor + 1) % n
    I.ring.stamp = u16(t)

city_tick(ctx, I, C, fx):                            // D 0x1002d9a0(flag, t)
    building_scheduler(ctx, I, C, fx)                // FIRST (D 0x10023a50 on city+0x144)
    for each pass p in seams.city_pass (original order): if p.fn: p.fn(ctx, I, C, fx)    // D 0x10027400, 0x10029610, 0x1002d350, 0x10029950 economy,
                                                     //   0x1002b670, 0x1002ba80 random events (PRNG), 0x1002a5e0, 0x1002d3b0; default none (O-3)
    C.tick_stamp = u16(t)                            // D [city+8]

building_scheduler(ctx, I, C, fx):                   // D 0x10023a50 with 0x10023810, 0x100238a0, 0x10023780, 0x100239c0, 0x10023930
    for lid in layout.schedule:                      // {L5 market, L1 public, L4 production, L3 houses, L2 military}; L0 stalls never
        run_slot_list(C.index.lists[lid], C.index.stamp, t, building_div,
                      [&](Building& b) { tick_building(ctx, I, C, lid, b, fx); })
    C.index.stamp = u16(t)                           // the ONE shared stamp, written after all five lists
```
Every list of a city sees the same `d` (the stamp is not updated between lists). `n` includes holes; a list with `count == 0` cannot occur (a fresh list has one element); a zero-length loop leaves the cursor unchanged exactly as the original. The slot vectors cannot change during `tick_building` (no tick creates or removes buildings; asserted in debug builds).

### 5.6 Island object manager (stamp formula **D** 0x10095d40; traversal and cursor **N** stand-in, A10)
```
object_manager_tick(ctx, I, fx):
    n = I.deposits.size()                            // N: objects = deposits in index order (flames, plague, volcano: O-12)
    d = u16(t - I.objmgr.stamp);  count = (n == 0) ? 0 : min(d * n / objmgr_div + 1, n)   // formula D (imul 0x68db8bad, sar 11, +1, clamp); the n == 0 branch is N
    // the original first drains a deferred-removal list ([+0x18]); nothing is removed here (O-12)
    repeat count: seams.object_manager (per-object tick, default none);  I.objmgr.cursor = (I.objmgr.cursor + 1) % n      // N
    I.objmgr.stamp = u16(t)
```
What the original does differs in three ways, all open (O-12): it is a **do-while** (the body runs once before the count test, so an empty manager is not a defined no-visit case); its cursor is a **tile** (words `+0x24/+0x26`) that jumps to the position of the next object in chunk-bitmap order (0x10095770, 0x10095ad0); and `n` is `[mgr+0x28]`, objects being fetched through virtual slots `+0x14`/`+0x10` and ticked through slot `+0x6c` (fire, plague, volcano, deposits). Only the stamp arithmetic is disassembled fact. The index cursor above, and therefore the hashed `objmgr.cursor`, are our own stand-in with no original counterpart: when the real manager is ported the cursor becomes a tile cursor and `kStateHashVersion` changes. Deposits have no tick body in the evidence; stamp and cursor evolve and are hashed so the real bodies slot in later.

### 5.7 Timed events (D 0x100add90; queue at world +0x6a94)
```
events_dispatch(tick):
    const PopResult r = pop_due(events_, tick, rules.events_drain_new, [&](const TimedEvent& e) {
        if e.kind == TileCleanup: seams.tile_cleanup (default: none)    // D 0x100adae0: island by tile, finish destruction / remove rubble (O-18)
        else                    : seams.delayed_hit  (default: none)    // D 0x100adb80: unit hit (subtype 2) or tile hit (subtype 4) (O-19)
    }, rules.events_max_dispatch)
    if r.capped: record it for check_invariants (F24; a game never reaches the cap)
    if rules.events_flush_after_figures: (run pop_due again after step 3)         // D4
```
Kinds other than 0 and 1 do not exist (the original silently drops them). `cancel_tile` (D 0x100ade40) removes the first match in queue order and only for subtype 4 (OQ-056); the running event is already out of the queue, so a handler cancelling its own tile finds nothing. Enqueue = `due = tick + delay`, inserted after all nodes with `due <=` its own, i.e. key `(due, next_event_seq++)`: FIFO among equal due ticks. Enqueue sites in the original: kind 0 subtype 4 with delay 50 (hit/explosion path, D 0x10063cf1) and 25 000 (building destruction handler, D 0x100943d5); kind 1 with delay 0 from ship weapon fire (D 0x1003593e). A delayed hit enqueued during the figure phase lands in the **next** advance (D4); the original also loses pending kind-1 events over save/load (F7: ours saves both kinds).

### 5.8 The 300 s event and the drought (D 0x10093980, 0x10093a10, 0x10087280; machine **I**)
```
event_300s(ctx, I, fx):
    secs = t / 500                                                      // u32, exact
    if u16(secs - I.event300_stamp) < event300_gate_seconds: return     // stamp untouched
    if settings.drought_off(): I.event300_stamp = u16(secs); return     // prop 6 bit 0x04 (`test al,4`, D 0x100939a0–0x100939c6): no draw
    r = ctx.draw.next(Event300)                                         // EXACTLY ONE draw per gate per island, before every other test
    if r % event300_modulus != 0 or I.climate == 0 or no city in slots 0 .. min(neutral_owner, city_slots)-1:  I.event300_stamp = u16(secs); return      // A5: the gate scans 0..14 in the original (neutral_owner - 1)
    start_drought(I)
    I.event300_stamp = u16(secs)

start_drought(I):                                                       // D 0x10093a10 (wrapper); its only caller is the 300 s event
    if I.drought: return
    for k in 0 .. city_slots-1:                                         // A5: start scans all city_slots slots (0..15 in the original)
        city = I.cities[k]; if !city continue
        b = first non-null building of city.index.lists[layout.roles.market_list] in slot order
        if b: start_drought_at(I, floor_to(b.x, chunk_edge), floor_to(b.y, chunk_edge), drought_radius);  return

start_drought_at(I, x, y, radius):                                      // D 0x10087280 = the drought-area constructor, island virtual slot +0x15c
    I.drought = Drought{x, y, target_ring: radius / chunk_edge, ring: 0, city_mask: 0, phase: 0, step: 0, state: 1, counter: 0, stamp: u16(t)}
```
`floor_to(v, e) = v - v % e` (tile coordinates are non-negative; validated at load). **Scope of D7:** "always centred on the first market/Kontor-group building" holds for **the 300 s event only**, because only the wrapper `0x10093a10` picks the building and it has a single caller. The constructor `0x10087280` (island virtual slot `+0x15c`) is also called by AnnoScene.dll at `0x10013870` and `0x10013a1a` (inside the script-disaster handlers `0x100137d0..0x10013a2a`) with caller-supplied x, y and length: these are the scripted **DROUGHT** disaster (`docs/re/fire-and-ships.md` l.53 corroborates), and island files carry a `DUERRE` record. Scripted disasters are **out of scope for M3** (O-51): they belong with the script engine (O-34). `start_drought_at` is the pure function a scripted drought will call; its planned door into `World` is a host-only `Command` alternative (`ScriptDisasterCmd`, appended to the variant, in the original's scripted band `0x94..0x97`), applied between advances like every command. Until then no scripted drought can occur, and `Rules::drought_random_centre` (D7) changes only where the 300 s event puts its centre. The drought object tick (`0x1008aeb0`, **I**), no-op when `I.drought` is empty:
```
drought_tick(ctx, I, fx):
    D = *I.drought
    if D.state == 0:                                                    // plateau
        if u16(t - D.stamp) >= drought_plateau_period:                  // catch-up gate, as 0x1008aeb0: stamp += 1000, not = t
            D.stamp += drought_plateau_period;  D.counter += 1
            if D.counter == drought_plateau_count: D.counter = 0;  D.state = 0xff          // 120 000 ticks = 240 s
    else if u16(t - D.stamp) >= drought_strip_period:                   // state 1 (expanding) or 0xff (retreating): one strip per 50 ticks
        D.stamp += drought_strip_period
        strip(D)  → seams.drought_tiles over the strip (default none); then step += chunk_edge;
        if D.step == drought_steps * chunk_edge: D.step = 0;  D.phase += 1       // original 8 * 8 = 64
        if D.phase == drought_phases: D.phase = 0;  expanding: D.ring += 1; if D.ring > D.target_ring { D.state = 0; D.ring = D.target_ring }
                                       retreating: D.ring -= 1; if D.ring < 0 { I.drought.reset() }
```
Total time: (target_ring + 1) rings × (drought_steps × drought_phases) strips × drought_strip_period = 7 × 32 × 50 = 11 200 ticks to expand and again to retreat. The tile-level effects (chunk flag bit 2, city-mask message 0x17, crop objects 0xe2/0xe3 advanced by 2 or back by 1 with event 0xce) are `drought_tiles` (O-15); the per-chunk records of the original island (`island+0x94`, 16 bytes each: object list, flags) are not modelled (O-12).

### 5.9 Figure registry (D 0x1009a8a0, vtable 0x100ccd74, world +0x114)
```
figures_tick(t):
    F = figures_;  d = u16(t - F.stamp)
    // phase A (sector re-bucketing of float positions, div 250): derived spatial structure, not state (O-17)
    n = F.list.slots.size()
    if n > 0:
        count = visit_count_clamped(t, F.stamp, n, figure_b_div)       // clamp(d*n/50, 1, n) — NO +1 (D 0x1009aa4c–0x1009aa77)
        repeat count: if F.list.slots[F.list.cursor]:
                          Effects fx;  SerialWorld sw{..., fx}
                          seams.figure_tick.fn(ctx, tctx, sw, fig)  (else the default below);  merge(fx)         // C2: merged right after EACH figure call
                      cursor = (cursor + 1) % n
    F.stamp = u16(t)                                                    // D [+0x20] after phase B
    // phase C: while expiry non-empty: f = front; if f gone or u16(t - f.created_stamp) >= figure_expiry_ticks (7500): destroy(f); pop;  else stop
    // phase D: for each id in deferred: free its slot, release its counter; clear the list
```
Default behaviour (no seam): nothing moves; if `rules.interim_figure_return_ticks > 0` and `u32(t - fig.born) >= N` the figure is appended to `deferred` (D12). `destroy` = free the slot (`SlotList::remove`), erase `by_id`, release the counter at the home building. Who appends to `expiry` in the original (which unit classes, property 0x23) is unread (O-8); the default spawn does not.

### 5.10 Players, script timers, monopoly, event 7.5 min
```
players_tick(t):                                          // D 0x1009c4d0
    P = players_;  n = P.slots.size()
    count = rules.player_table_full_tick ? n : visit_count(t, P.stamp, n, player_div)      // min(d*32/50 + 1, 32)
    repeat count: if P.slots[P.cursor]: player_tick(*P.slots[P.cursor], t);  P.cursor = (P.cursor + 1) % n
    // the +0x8a-stamp ring step calls the bare `ret 8` 0x10001020: no behaviour
    if seams.ai.fn: cmds = []; seams.ai.fn(seams.ai.ctx, tctx, SerialWorld{...}, cmds); for c in cmds: apply(c)       // D: CompLogic tick EVERY advance, no rate limit (O-1)
                    // A15: the AI draws through tctx.draw (DrawSite::Ai) and is deterministic given hashed state; its commands are applied at once, in order, and are NOT logged
    P.stamp = u16(t)                                      // last (D)

player_tick(p, t):                                        // D 0x1001b900, in this order
    (1) group table: seams.player_groups (30 slots, 500-tick gate inside; O-20)          // D 0x10019390 → 0x10062a40
    (2) upkeep (D 0x1001b840): if u16(t - p.upkeep_stamp) >= upkeep_period (2000):
            S = u32(p.upkeep_a + p.upkeep_b + p.upkeep_rem);   p.money -= S / upkeep_slices (15);   p.upkeep_rem = S % 15
            p.upkeep_stamp = u16(t)                       // = t, NOT += 2000
            p.stat_a = u32(p.stat_a * stat_decay_num) / stat_decay_den;  p.stat_b = u32(p.stat_b * stat_decay_num) / stat_decay_den   // 104 / 105; u32 arithmetic with wrap
    (3) if p.index != neutral_player and p.state == Alive:        // the jne at 0x1001b927 skips (3) AND (4) for the neutral player and for a non-alive one
            if p.money <= bankrupt_money (-3000):
                if p.bankrupt_deadline == 0: p.bankrupt_deadline = u32(t + bankrupt_grace_ticks)   // 150000 = 300 s
                else if t > p.bankrupt_deadline (unsigned): p.state = Bankrupt
            else p.bankrupt_deadline = 0
            (4) if p.defeat_pending: p.defeat_pending = false;  seams.defeat(p)                // D 0x10020550; sets state = Defeated only behind kind/field/owner checks (O-22): default does nothing

timers_tick(t):                                           // D 0x1009cd30
    dt = t - T.last_tick  (u32)
    for k in 0 .. script_timers-1: if T.active[k]:
        if k > 0: T.counter[k] += dt
        else if dt >= T.counter[0]: T.counter[0] = 0;  T.active[0] = 0;  UI event TimerExpired{0}     // F5: the original deactivates only on exactly 0; an overshoot underflowed
        else T.counter[0] -= dt
    T.last_tick = t

monopoly_refresh(dt_ms):                                  // D 0x1009d2a0 (no PRNG)
    if rules.monopoly_every_30s: acc += dt_ms; if acc < monopoly_ms: return; acc -= monopoly_ms          // D5 option
    else if acc != 0 and acc < monopoly_ms: return                                                       // faithful gate: acc is only advanced inside the body,
    body();  if !rules.monopoly_every_30s: acc += dt_ms                                                  //   so the body runs on the first advance with dt > 0 and never again (D5)
  body (I): for s in 0 .. monopoly_raw_slots-1: for island I in table order:
                if bit s of monopoly_climate_mask[I.climate]:     // I.climate < mask.size() and s < 32 are validated invariants (§3.1, §9.6)
                    owner = the unique owner, among I's cities in number order, whose player has Volk == 1 (none if no such city or a second exists)   // D 0x1009d210
                    if !owner: entries[s] = {0, none}; break                                   // clear entry s, end the island scan for s
                    if entries[s].player != owner: entries[s] = {game_seconds, owner}          // game_seconds = t / 500 + prop7_offset
            second loop k = 1..8 (entries 20..27, island object queries): O-24, untouched

event750(dt_ms):                                          // D 0x100ae140, in exactly this order
    acc += dt_ms;  if acc < event750_ms (450000): return
    acc = rules.event750_keep_overshoot ? acc - event750_ms : 0                                          // D10 (original: acc = 0, overshoot discarded)
    if settings.event750_off(): return                    // prop 6 bit 0x80
    r1 = draw(Event750Gate);  if r1 % event750_modulus != 0: return      // modulus 10
    if !settings.addon(): return                          // base game: the draw was consumed, nothing else
    r2 = draw(Event750Island)                             // index into the island TABLE (size [+0xf4], null slots included)
    if islands_.empty(): return                           // F6: the original divides by zero
    I = islands_[r2 % islands_.size()];  if !I: return
    UI event Event15a{x: I.x0 + w/2, y: I.y0 + h/2}       // D: message-manager event 0x15a at the island centre; the consumer is the script engine (O-16)
```
The player table is **not** ticked in full every advance (D6). One tick is 2 ms, so the multiplayer steps of 5/10/20 ms are `dt` = 2–3 (alternating), 5 and 10 ticks, and the disassembled count `min(d·32/50 + 1, 32)` (D 0x1009c4d0) gives **2, 4 and 7 players per advance** for 5, 10 and 20 ms; all 32 are reached only from `d ≥ 49` ticks (98 ms, i.e. a single-player step near its 100 ms cap). The tests of §14.2 derive their expectations from the formula, never from a ms-to-ticks guess.

## 6. PRNG draw sites and the timeline

One world cursor, consumed in the original order: island table order → (per island: fauna sweep [unported] → ring → city → buildings in schedule/slot order [small farm draws] → city passes [unported] → 300 s event) → events → figures → players/AI [unported] → 7.5 min event. Draw sites implemented now: `SmallFarmAnimal`, `Event300`, `Event750Gate`, `Event750Island`. Each call passes its `DrawSite`; `Effects::draws` and `SimOutput::draws` count them per site and record the cursor before and after the advance (telemetry; not hashed).

**PRNG timeline.** Until the unported sites exist the cursor diverges from the original's at the first advance in which one of them would have drawn: city random events (0x1002ba80), the fauna sweep (0x1009366f: `r % remaining`, a state-dependent count), the CompLogic AI, INSELLNK template choice (loader, O-35), possibly the item tick bodies of the island message manager (0x100793b0 slot `+0x60`, unread, O-52), and possibly callees of the growth visit 0x1008b5f0 (unverified, O-13). This is a **state input**: `kPrngTimelineVersion` (§4.14) is hashed in the header, and the replay header records `{version, missing[]}` where `missing` lists those names. Porting a site is a recorded version bump plus new goldens, never a silent desync. A differential fixture against the original can only line up while every draw has its counterpart, so the cursor is the first thing to compare.

`PrngMode::PerIsland` (D11) is **design-only in M3**: a reserved `Rules` value that `Rules::validate()` rejects until stage S16, so the world stream is the only stream and there is no stream state that could escape the hash or the save. The intended S16 behaviour, for the owner's decision: each island gets a `StreamDraw` over the same table with a per-island cursor started at `WorldRng::stream(stream_seed, slot)`; world sites (7.5 min event, monopoly) keep the world stream. It would change outcomes of random events and small-farm animal rolls (same distribution, different sequence) and make every island tick pure. S16 must add the per-island cursor to `Island` (field list, hash scope, save entry) and `stream_seed` to the header scope in the same commit as the mode.

## 7. Driver: calling the production step functions

### 7.1 `tick_building` (D: building slot `+0x8c`, called for each non-null slot reached)
```cpp
void tick_building(const TickContext& ctx, Island& I, City& C, ListId lid, Building& b, Effects& fx) {
    const BuildingType& ty = *ctx.types.find(b.type);              // present by construction (create validates; check_invariants asserts)
    const ClassInfo& ci = ctx.types.class_info(ty.cls);
    const bool icons = ctx.players.volk(C.owner) == 1;             // StatusTracker::shows_icons (OQ-033); absent owner → false
    StepResult r;
    // No std::get (it throws bad_variant_access, and exceptions are forbidden in sim): the payload alternative is guaranteed by
    // check_invariants()/load (ClassInfo::payload == the alternative), so a mismatch here is unreachable; get_if + a debug assert, and a
    // release build simply returns (the faulted_ flag is NOT used for this: a mismatch can only come from a bug, and the invariant check on load/create catches it).
    switch (ci.tick) {
    case TickKind::None: return;                                   // inert: `last` untouched (D15)
    case TickKind::Workshop:        { auto* w = std::get_if<Workshop>(&b.payload); if (!w) return;  w->status.shows_icons = icons;
                                      r = workshop_step(*w, ty.prod, ctx.t); break; }
    case TickKind::WeaponsWorkshop: { auto* w = std::get_if<WeaponsWorkshop>(&b.payload); if (!w) return;  w->status.shows_icons = icons;
                                      w->unlocked.assign(ty.output_goods.size(), false);        // `unlocked` is derived and EMPTY after load: size it before indexing
                                      for each line j < ty.output_goods.size():
                                          w->unlocked[j] = goods_forsch(ty.output_goods[j]) == 0
                                                        || ctx.players.researched(C.owner, goods_forsch(ty.output_goods[j]));   // OQ-034 resolved: bit 1 of the research state byte
                                      r = weapons_workshop_step(*w, ty.prod, ctx.t); break; }
    case TickKind::Farm:            { auto* f = std::get_if<Farm>(&b.payload); if (!f) return;  f->status.shows_icons = icons;
                                      r = farm_step(*f, ty.prod, ctx.t); break; }
    case TickKind::FurTrapper:      { auto* f = std::get_if<Farm>(&b.payload); if (!f) return;  f->status.shows_icons = icons;
                                      r = fur_trapper_step(*f, ty.prod, ctx.t); break; }    // send_figure additionally needs the prey search (O-25): no trapper is sent
    case TickKind::SmallFarm:       { auto* f = std::get_if<SmallFarm>(&b.payload); if (!f) return;  f->status.shows_icons = icons;
                                      r = small_farm_step(*f, ty.prod, ctx.t, draw_ref(ctx.draw, DrawSite::SmallFarmAnimal), ctx.cfg.small_farm_animals); break; }
    case TickKind::Mine:            { auto* m = std::get_if<Mine>(&b.payload); if (!m) return;  m->status.shows_icons = icons;
                                      r = mine_step(*m, ty.prod, ctx.t, I.deposits, mine_area(b, ty), ctx.settings.depletes()); break; }
    }
    consume(ctx, I, C, lid, b, ty, r, fx);
}
```
`shows_icons` and `unlocked` are inputs derived from other state, refreshed before every step and excluded from the field list (they cannot drift). `unlocked` is **sized before it is written** (`assign`): after `World::load`, or for any payload read from a save, it is an empty vector, and `weapons_workshop_step` treating a missing entry as locked would hide an out-of-bounds write in tests built with `make_weapons_workshop`. A save/load/advance test of a weapons workshop (S11) pins this. The step functions index `out`/`in`/`activity`/`figures_out`/`line_on` by the type's output and input counts; `check_invariants` and the load path verify those vector sizes (§9.6), so the steps never see a short vector. `mine_area` builds `MineArea{x, y, w', h'}` from the position and the rotated footprint (D 0x1006ddf0).

### 7.2 `consume(StepResult)`: synchronous, in this order
| Field | Effect |
|---|---|
| `blocked_changed` | UI `BlockedChanged{building, blocked}`. The original notifies island slot `+0x168` with `{type, x, y, rot, blocked}` (a traffic/render flag); nothing in the sim reads it (O-26) |
| `message` + `status` | UI `StatusMessage{owner, building, status}`; `notice != 0` → UI `Notice{owner, building, id}` (mines: 0x19 / 0x15b) |
| `made` | none: the output stays in the building store; carriers that move it to the city stock arrive with transport |
| `send_figure` | the home building's counter is raised **in the tick** (Workshop/WeaponsWorkshop `carriers_out++`, Farm/FurTrapper `workers_out++`, SmallFarm `figures_out[0]++`) and `Effects::spawns` gets a `SpawnRequest{kind = prod.figure_ids[0], job, home_counter}` |
| `send_animal` | SmallFarm: counter `figures_out[animal_kind - animals.base]++`; spawn with `job = 0x2e`, `figure_kind = animal_kind` |

The raise-in-tick / spawn-in-merge split keeps the counter and the registry consistent (`check_invariants` asserts equality: the original keeps counters as state, so they are asserted, not derived). With the default behaviour no figure returns, so a building sends at most `max_figures` figures; tests that need the return use a scripted `figure_tick` seam or `interim_figure_return_ticks`.

### 7.3 Creation and removal (the shared tail of D 0x10024010)
`create_building` is the single creation path for commands, the loader and tests. `Create(rec, slot, now, &out)` is `BuildingIndex`'s method (`this = city+0x144`), not a stateless factory: it constructs, initialises, attaches to the city **and** inserts into the lists. Order (D):
```
create_building(w, I, C, rec{type, x, y, rotation, byte0f}, now) -> Result<BuildingKey, Reject>:
    1. type = types.find(rec.type) else UnknownType (F4);  rotation < 4 else BadRotation (F4);  list = layout.list_for(type.prodtype) else NotABuilding;
       I.next_serial != 0xffffffff else SerialExhausted                     // validation, no state touched
    2. construct by class (RuntimeClass → make_workshop / make_weapons_workshop / make_small_farm / farm / mine / Inert); every bit zero (F2)
    3. attach type: Mine: deep = mine_is_deep(prod, type.research_id)       // D slot +0x28 (type) before position
    4. position x, y; rotation; byte0e = (list == layout.roles.house_list ? 0x82 : 0x86); byte0f = rec.byte0f      // D: the house base constructor sets 0x82
    5. last = status.time = u16(now)                                         // D property 0x23 → 0x10073380 → slot +0x120 (F1)
    6. attach city (D slot +0x28 city, then hook +0x128): Mine: deposit = find_deposit(I.deposits, prod.outputs[0], deep, mine_area) — position already set
    7. id = make_building_id(I.index, I.next_serial++);  slot = list.insert(b, SlotParams{...});          // §5.2; hint = slot
    8. derived: by_id, footprint grid, service counters / house counts; tiles: footprint object := type.id (stand-in, O-5)
    9. shared tail (D 0x1002518b): residents, stat 1/2 running cost: no stored state yet (O-9, O-23)
```
```
remove_building(w, I, C, key):                                               // D 0x100255c0: exact inverse first
    undo derived counters;  grid and by_id erase;  tiles of the footprint: object := 0 (stand-in);
    list.remove(slot)                                                        // hint untouched, vector never shrinks
    if lid == layout.roles.market_list: owner.defeat_pending = true         // D: player byte [+0x295] = 1; consumer 0x10020550 (O-22)
```
The initial `last` is the placement tick in **both** single and multiplayer (F1). The original passes `now = [client+0x10]`: SP stores the world tick of the *start of the previous step* there before every advance (D AnnoFrame 0x1005b159; uninitialised heap before the first step), `tNetClientNet` never updates it (D 0x1005fdcb, 0x10060624 write 0), and the stream loader passes 0. With `last = 0` the gate `(u16)(t - last) >= 500` holds at every scheduler visit, so a new multiplayer building runs one step per visit until `last` has caught up: up to `(t mod 65536) / 500 = 131` extra steps. `StatusTracker::time` is initialised with the same value (the class status-time field written by slot `+0x120`: workshop `+0x22`, farm/fisher/trapper `+0x20`, mine `+0x1e`, class 12/17 `+0x28`). A loaded scenario starts at tick 0, where the two coincide.

### 7.4 Small farm: the existing signature is wrong (stage S0, F14)
The animal kind of line 1 is a **world PRNG draw**, not the island climate (D 0x1006f810–0x1006f838): `kind = animals.kind_by_draw[draw % kind_by_draw.size()]` (the table of 8 u16 at 0x100c8d48, so `% 8` = the original's `& 7`; the size is data and validated non-empty, §3.1, instead of a literal mask), one draw per step in which line 1 is switched on and the farm is not suspended (the original returns before the draw while flags bits 4/5 are set, 0x1006f69b–0x1006f6e2), taken after the step gate and before the limit tests. The custom-table modulus is a decision for the owner (D19, OQ-055); the draw count does not depend on the table or on the number of lines.
```cpp
struct DrawRef { void* ctx = nullptr; std::uint16_t (*fn)(void*) = &DrawRef::none; std::uint16_t operator()() const; };
StepResult small_farm_step(SmallFarm&, const ProductionType&, std::uint32_t t, DrawRef draw,
                           const SmallFarmAnimals& = SmallFarmAnimals{});
// SmallFarmAnimals::kind_by_climate is renamed kind_by_draw (same 8 values, indexed by draw % size())
```
The 13 existing call sites in `production_test.cpp` change (a one-entry `WorldRng::from_table({r})` always returns `r`). `docs/sim/production.md` and `docs/re/production.md` §4.5 already carry the corrected text (applied 2026-10-18, before S0); S0 changes the code to match. The step gate, `last += 500` and everything else stay.

## 8. Commands

Evidence: AnnoFrame.dll dispatcher `0x1005ffd0` (`tNetClientNet::ExecuteTurn`), jump table `0x10060388` (types `0x64..0x97`), server turn builder `0x100555a0`, queue entry `0x100553e0`, apply function `0x100589a0` (modes 2, 6, 7, 8, 9), `City::Build` `0x100276a0`, `City::Place` `0x100279a0`, `RemoveAt` `0x10028fd0`.

### 8.1 What the original does (D) and what we change
- The executed turn stream is `{u16 type, u16 len, payload[len]}*` with **no sender id**. The server queues any client's message of type `0x64..0xc7` (`0x100553e0`) and the turn builder writes only type and length (`0x100555a0`; the sender at item `+0xc` is copied into the recording buffer only in replay mode). The dispatcher computes the next record at `payload + len` with no bound check, handlers read fixed offsets irrespective of `len`, and expand into fixed stack buffers (0xa20, 0x201c bytes). Handlers resolve city, unit and player purely from payload ids with no range or null check (`0x100aee00`, `0x100863e0` read `[world+0x6530+4i]`, `[island+0x38+4n]`). There is no ownership check anywhere (F9, F10, F11).
- A turn has 10 steps (`0x1005fea7`); the commands of a turn all run in stream order **before** its first step. Single player has no queue: the UI calls the same apply functions directly between two advances (`tNetClient` vtable `0x1008914c`); multiplayer packs `[u8 type][payload]` (slots `0x144`/`0x148`) and replays it at turn start. So the MP command set equals the SP API, and the SP step boundary is the same moment: queueing SP commands to the next advance is not a deviation.
- We keep the model and change the trust: the **host stamps the issuer and assigns the turn**; a decoded command that is malformed, unknown, or not authorised is an identical no-op on every peer.

### 8.2 Contract
1. `ApplyResult World::apply(const Command&)` is **total**: any input yields a result; no exception, no UB, no allocation proportional to anything but the command.
2. **Pure validation, deterministic apply.** `Reject` depends only on hashed state and the command. A rejected command (or record) changes nothing: `state_hash()` before == after (property test). On every peer with the same state the result is the same.
3. `issuer` is stamped by the host (SP: the local player). City-scoped commands need `city.owner == issuer`; unit-scoped commands (later) need `unit.owner == issuer`; `SetSpeed` needs `issuer == cfg.host_player` and `is_original_speed(percent, cfg.multiplayer)`. No `state == Alive` requirement is added (the original has none).
4. Applied **between advances** at the current tick. Nothing is queued inside `World`; `Recorder` (§8.7) is what records the order. The one exception is the **AI seam**, whose commands are applied inside `advance` (§5.10): they are generated deterministically from hashed state and the `DrawSource` on every advance, applied at once in order, and **not logged** by the `Recorder`. Replay of a log therefore requires the AI seam to be installed identically; its name is part of the replay header's seams digest, which `replay()` refuses to run on a mismatch (§8.7). A divergence of the AI shows up as a hash mismatch at the next checkpoint, exactly like any other seam.
5. **Record by record** in stream order; each record validated then applied; a failed record is skipped and the next one tried (D: op 6 `je 0x10058a85` continues; op 9 `0x87` breaks, which we do not port). `Rules::build_atomic` (D8) validates every record against a scratch budget first.
6. Every id is bounds- and null-checked; unknown type ids fail closed (F4); rotation `> 3` is rejected (F4); `flags` are not on the wire (F13).

### 8.3 Types (`command.hpp`)
```cpp
/// The original's 10-byte record {u16 type, u16 x, u16 y, u8 rotation, u8 [7], u8 [8], u8 [9] unused}, widened.
struct PlacementWire { TypeId type; std::int32_t x = 0, y = 0; std::uint8_t rotation = 0; std::uint8_t byte7 = 0; std::uint8_t byte8 = 0; };

struct BuildCmd { CityRef city; std::vector<PlacementWire> placements; };                  // 0x64 (the gameplay build)
struct DemolishCmd { CityRef city; std::vector<TileXY> tiles; };                           // 0x68
struct SetProductionLineCmd { TileXY tile; GoodsId goods = 0; bool on = true; };           // 0x74
struct SetTradeFlagCmd { CityRef city; std::uint16_t mode = 0; GoodsId goods = 0; std::uint32_t value = 0; };   // 0x7c
inline constexpr std::uint32_t kAppendIndex = 0xffffffffU;
struct SetTradeSlotCmd { CityRef city; std::uint32_t list = 0; std::uint32_t index = 0; GoodsId goods = 0;
                         std::uint32_t amount_88 = 0, price = 0; };                         // 0x73; index == kAppend is 0x8a (entry add); list < SimConfig::trade_lists (a u8 in the original: F3)
struct SetSpeedCmd { std::uint32_t percent = 100; };                                       // 0x7a, host only
struct LegacyRawCmd { std::uint16_t type = 0; std::vector<std::uint8_t> payload; };        // decoded, logged, replayed, rejected

using Body = std::variant<BuildCmd, DemolishCmd, SetProductionLineCmd, SetTradeFlagCmd, SetTradeSlotCmd, SetSpeedCmd, LegacyRawCmd>;
struct Command { PlayerId issuer; Body body; };   // alternative order = the codec's tag order and part of the replay format

// reject.hpp (introduced with create_building in S6, because creation and the placement seam return it)
enum class Reject : std::uint16_t {
    None = 0, UnknownIssuer, UnknownIsland, UnknownCity, NotCityOwner, NotHost, BadSpeed,
    UnknownType, BadRotation, NotABuilding, SerialExhausted,
    Unsupported,      // decoded but not implemented: tile objects (roads, walls: PRODTYPE > 192), legacy types
    OutOfIsland, TileNotOwned, Occupied, TerrainRefused, CannotAfford, NotResearched, AtomicAbort,
    NoBuildingAtTile, LineNotFound, BadIndex,
    BadValue          // a numeric field beyond its configured bound (trade limit > SimConfig::trade_limit_max)
};
struct RecordResult { Reject status = Reject::None; BuildingId building; };
struct ApplyResult { Reject status = Reject::None; std::vector<RecordResult> records; bool ok() const { return status == Reject::None; } };
```
Not ported first and kept as `LegacyRawCmd` (fail closed): ships and routes `0x6a/0x6b/0x77–0x79/0x88`, units `0x6c–0x6e/0x75/0x8f`, player controller lists `0x80–0x86/0x90`, diplomacy and tribute `0x6f/0x70/0x7b/0x8c`, chat `0x7f/0x89/0x8b` (net layer), editor/scenario ops `0x65–0x67/0x69/0x87` (no owner or cost checks in the original; never player commands), session and sync `0x8d/0x8e/0x94–0x97` (net layer), building settings `0x71/0x72` (classes unported), dead types `0x76/0x7d/0x7e` (no handler) and `0x91–0x93` (`ret 0xc` stubs), `0x98` (sent by the MP client but outside the table: MP drops it, SP executes a building fix-up; F17).

### 8.4 Apply semantics

**Build `0x64`** (D op 6 → `City::Build` → `City::Place`). For each record in stream order:
1. `type == 0` is skipped (D). `types.find(type)` else `UnknownType`; `rotation <= 3` else `BadRotation`. `layout.list_for(prodtype)` empty (PRODTYPE 0 or > 192: roads, walls, scenery are tile objects only, D: Place skips Create when the block start (p - 1) & ~0x1f is above 0xa0, i.e. for p >= 193) → `Unsupported` until tile-object placement is ported (O-4).
2. Footprint `(w', h')` = `(width, height)` swapped on odd rotation (D 0x100279e6, 0x10071c80). Every footprint tile must be inside the island (`OutOfIsland`); every tile's owner must be `neutral_owner` or one single common owner (D `0x10058890`), which must be the target city (`TileNotOwned`); at least one tile belongs to the city (an all-neutral footprint resolves to the city numbered 15, which does not exist).
3. `placement_rule` (D `0x1007c790` through island slot `+0x6c`, O-27). Default stand-in (D17): no footprint tile carries a non-zero `object` (`Occupied`). The type's flag word (`0x2000` coast, `0x1000` mountain/quarry, `0x4000` road/bridge, correlated with the data, **I**) is not enforced, so Build can accept what the original refuses.
4. `upgrade_rewrite` (D `0x100276a0`, PRODTYPE `0x43`/`0x48` rewrite `rec.id`; O-28). Default none.
5. `rules.enforce_research` and the type's `research_id` not researched → `NotResearched`. Default off (the original has no sim-side research gate on build; where research gates availability is unknown, O-30).
6. Affordability, **read-only** (D `0x100275d0`), skipped when `owner.free_build` (D owner slot `+0x9c(7)`, O-9): `cost_gold <= owner.money` and, for each cost entry, `amount <= city.stock[goods]` (`CannotAfford`). Evaluated per record against the state left by the previous records, so a drag build spends in stream order and may end partially built (D8).
7. Place (D `0x100279a0`): charge (`owner.money -= cost_gold`; per cost entry `city.build_goods[g - first].spent += amount` and `city.stock.remove(g, amount)`), `create_building(now = tick)` (§7.3), `seams.on_placed` (D hooks `0x1002c920/0x1002ca80/0x1002d2a0/0x1002cf60`, land claim for 0x21–0x23, market/Kontor in-place replacement `0x10027b62`: O-4; an overlap that the original would replace is rejected `Occupied`). The original charges before `Create` and does not refund if `Create` fails; here validation precedes the charge and creation after validation cannot fail, so the case does not arise (F18).
`byte0f` of a command-created building is 0: the original overwrites `rec[8]` from the terrain height check (0x1007c790, type slot `+0x68`), so a client-supplied `byte8` is ignored (carried on the wire for legacy import only).

**Demolish `0x68`** (D op 6 of `0x10058e90`, then `RemoveAt` `0x10028fd0`). For each tile in stream order: skip silently unless the tile is inside the island and `owner == city.number` (D). The building is found with `FindByTile` over `layout.lookup_order` (D `0x10023c80`: masks 8, 1, 2, 4, 0x20, 0x10) → if found: `seams.on_removed` (fire/plague extinguish, market stall detach, goods handlers `0x1002c740/0x1002cb20/0x1002c090/0x1002c9c0/0x1002d2a0`, message record 0x2c: O-5), then `remove_building` (§7.3). A tile without a building but with an `object`: `object := 0` (stand-in for island slot `+0x7c`, O-5). After each tile, if `!rules.keep_empty_city && seams.city_is_empty(city)` (D city slot `+0x78` = `0x1002f7f0`, unread, default `false`: O-6) the city is removed: the whole `BuildingIndex` is dropped (D9, O-48). Border-strip re-evaluation (island slots `+0x80`/`+0x16c`) is tile bookkeeping, not modelled.

**SetProductionLine `0x74`**: tile → island (`island_at`) → tile owner → city (must be the issuer's) → building (`lookup_order`, any list) → `goods` must be one of the type's `output_goods`, index `i` → `Workshop::switched_on`, `WeaponsWorkshop::line_on[i]`, `Farm::switched_on`, `SmallFarm::line_on[i]`, `Mine::switched_on` (the original's building slot `+0xc8` is a stub for other classes) else `LineNotFound`. The `value` byte is a switch (A9).

**SetTradeFlag `0x7c`** (D `0x1002dbd0`): `g < warehouse_goods` else `BadIndex`; mode 0/1 → `value <= trade_limit_max` else `BadValue`, then `goods[g].limit = value`, `goods[g].buy = (mode == 1)`; mode 9 → for the goods in `[build_goods_first, +count)` `build_goods[g - first].setting = value` (D stores at `[city+0x48c+8g]`); any other mode is ignored. **SetTradeSlot `0x73`/`0x8a`** (I, O-14): `list < trade_lists` and `index < list.size()` (or append), `goods == 0` clears the entry; stored only. `BadIndex` otherwise. **SetSpeed `0x7a`**: stores `settings.speed_percent`; the driver derives the step (`single_player_step_ms` / `multiplayer_step_ms`) so peers agree.

### 8.5 Wire format (explicit little-endian, widened, bounds-checked; **not** byte-compatible with the original)
A command travels in two forms. From a client to the host: `ClientRecord = {u16 type, u16 len, payload[len]}` (no sender, as the original). From the host to everyone: `TurnEntry = {u32 issuer, u32 seq, ClientRecord}` where `issuer` is the connection's player and `seq` the host's order index. Native `type` values are the original's where the meaning matches.

| Type | Command | Payload |
|---|---|---|
| `0x64` | Build | `u32 island, u32 city, u32 n, n × {u32 type, i32 x, i32 y, u8 rotation, u8 byte7, u8 byte8, u8 0}` |
| `0x68` | Demolish | `u32 island, u32 city, u32 n, n × {i32 x, i32 y}` |
| `0x74` | SetProductionLine | `i32 x, i32 y, u32 goods, u8 on` |
| `0x7c` | SetTradeFlag | `u32 island, u32 city, u16 mode, u32 goods, u32 value` |
| `0x73` | SetTradeSlot | `u32 island, u32 city, u32 list, u32 index, u32 goods, u32 amount_88, u32 price` |
| `0x7a` | SetSpeed | `u32 percent` (the original sends an `f32`) |
| other | LegacyRaw | the original type in the header, raw payload |

`decode_record(span) -> Result<Body, std::string>` rejects: a length beyond the input, trailing bytes, a count larger than `remaining / min_element_size` (checked **before** any allocation), a native tag with the wrong length, a `LegacyRaw` whose type is a native tag, an unknown flag byte. Property: `decode(encode(c)) == c`; no prefix, suffix or single-byte mutation of a valid record crashes, over-reads or allocates more than a small multiple of the input. Unknown tags are decode errors dropped and logged by the host, never silently executed.

`legacy::decode_0x64(span) -> Result<BuildCmd>` imports the original group codec for replay import (optional, D14): `u32 flags, u8 island, u8 city`, then groups of an 8-byte head `{u16 type | n<<12, u16 x, u16 y, u8 rot | rec7<<2, u8 rec8}` plus `n ≤ 15` followers of 3 bytes `{rot, s8 dx, s8 dy}` **relative to the head** (not cumulative); the Demolish tile list is run-length coded `{u16 x, u16 y, u8 n, n bytes}` with signed nibbles applied **cumulatively**. Tested on synthetic bytes only.

### 8.6 Single player and multiplayer step sizes
The sim never decides how much time a step advances; the session does. SP: `single_player_step_ms(frame_ms, percent)`, one `advance` per frame (cap 100 ms), every size recorded. MP: a turn every 100 ms real time, 10 steps per turn of `multiplayer_step_ms(percent)` (5/10/20 ms). Commands of a turn are applied in stream order first. The script engine (BigBrother, `bigbrother.slot+0x20(tick)`, D AnnoFrame 0x1005fe10) steps before every advance in the original and is outside `anno_sim` (O-34); the session calls it through its own seam.

### 8.7 Turns, recorder, replay, divergence finder (`replay.hpp`)
```cpp
struct Turn { std::uint64_t index = 0; std::uint32_t step_ms = 0; std::vector<Command> commands; };
/// MP: apply the commands in stream order, then `steps` advances of step_ms (steps per turn = Timing::mp_steps_per_turn).
void run_turn(World&, Recorder*, const Turn&, std::uint32_t steps);
/// SP: apply the commands, then one advance.
void run_frame(World&, Recorder*, std::span<const Command>, std::uint32_t step_ms);

struct ReplayEntry { enum class Kind : std::uint8_t { Advance, Commands, Checkpoint } kind = Kind::Advance;
                     std::uint32_t ms = 0; std::vector<Command> commands; std::uint64_t tick = 0, hash = 0; };
struct ReplayLog {
    static constexpr std::uint32_t kVersion = 1;
    std::uint64_t config_digest = 0, rules_digest = 0, registry_digest = 0, seams_digest = 0;
    std::uint32_t prng_timeline_version = 0; std::vector<std::string> prng_timeline_missing;   // §6: the cursor is short of the original's
    std::vector<std::uint8_t> initial_save;           // World::save() before entry 0
    std::vector<ReplayEntry> entries;
};
class Recorder {                                      // the one place that feeds a World in a session: logs first, then acts
  public: explicit Recorder(World&);
    ApplyResult submit(const Command&);  std::uint64_t advance(std::uint32_t ms);  void checkpoint();  const ReplayLog& log() const;
};
std::vector<std::uint8_t> write_log(const ReplayLog&);  formats::Result<ReplayLog, std::string> read_log(std::span<const std::uint8_t>);
formats::Result<ReplayOutcome, std::string> replay(World&, const ReplayLog&);   // stops at the first checkpoint whose hash differs
std::vector<std::uint64_t> hash_per_entry(World initial, const ReplayLog&);     // 8 bytes per entry
std::optional<std::size_t> first_divergent_entry(std::span<const std::uint64_t>, std::span<const std::uint64_t>);
HashTree tree_at(World initial, const ReplayLog&, std::size_t entry, std::uint32_t depth);
std::string diff_trees(const HashTree&, const HashTree&);   // path of the first differing node, e.g. "world/islands/island[0]/cities/city[1]/list[4]/building[3]"
```
Workflow for "two builds or platforms disagree" (tool `anno-sim-replay`): `hashes log.bin` on each side → `first-diff a.txt b.txt` → entry N → `tree log.bin --entry N` on each side and diff → a path; `--dump-scope` prints the scope's fields through a `PrintArchive` (same field list, text output). Every single-player advance is recorded because the original's step sizes vary per frame.

## 9. Hash, save, load: one field list

### 9.1 Mechanism (no macros)
`state_fields.hpp` defines, for every persistent type `T`, exactly one function `template <class A, Same<T> S> void fields(A& a, S& s)` naming the fields in canonical order. `A` is `HashArchive` (S may be const), `WriteArchive` (S may be const) or `ReadArchive` (S non-const). The hash, the save and the load therefore cannot disagree about which fields exist or in which order; a field can never be saved and not hashed. Only these are excluded, each named in `state_fields.hpp` with a comment: derived caches (`IslandIndexCache`, the world's island-rect lookup grid behind `island_at`/`IslandsView::island_at`, `FigureRegistry::by_id`, `BuildingIndex::service_counters`/`house_counts`, tile chunk digests and dirty bits, live counts), diagnostics, `StatusTracker::shows_icons` and `WeaponsWorkshop::unlocked` (refreshed by the driver before each step), `out_events_`/`SimOutput`, and the `TypeRegistry` itself (its digest is in the header).

Encodings: little-endian fixed width; `bool` one byte 0/1; enums one byte; `optional` = presence byte then value; `std::vector` = `u64` count then elements; `std::map` = count then (key, value) in key order; strings and byte vectors = `u64` length then bytes; `std::variant` payload = explicit `u8` tag (constants in `state_fields.hpp`, not `variant::index()`) then fields; no padding, no struct images. **Adding a field = one line at the end of its function + a `kStateHashVersion` bump + re-baselined goldens in the same commit** (development counter before S15, §14.3; version 3 and up after the freeze).

The state hash (version `kStateHashVersion`: a development counter until S15, then the frozen value 2) is hierarchical: `hash(scope)` = FNV-1a 64 over the scope's own fields, where every child scope contributes its 64-bit digest (`u64` LE) at the point it was opened; the world digest is the root. It is **not** the FNV of the save body (so a section can be recomputed alone and compared); the invariant tested instead is that `WriteArchive`, `HashArchive` (raw-tile mode) and `TraceArchive` visit the same primitive sequence (§14). `core_hash()` keeps the five-field v1 value (`clock.ms`, `clock.last_tick`, PRNG table length, table digest, cursor) so the old golden stays checkable; `state_hash()` begins with the same five fields inside its `clock_rng` scope.

### 9.2 Tile layer
`tiles_io` is the one place where the archives differ in what they touch, and it has one layout: `u32 width, u32 height, u32 chunk_edge` (the island scope already wrote `x0, y0, w, h`; width and height must equal them, else load fails), then the four columns `object[]` (u32), `phase[]` (u8), `owner[]` (u32), `aux[]` (u32) (each `w·h` elements, 13 bytes per tile). `WriteArchive`/`ReadArchive` move the raw columns (bulk, no per-tile calls). **The read side rejects before allocating**: `chunk_edge >= 1`, `w·h` computed in `u64` without overflow, and `w·h·13 <= remaining()`; the generic `count` check of `ReadArchive` does not cover bulk columns, so without this a header claiming a huge island would over-allocate or run on inconsistent sizes. `HashArchive` feeds, for each chunk in row-major chunk order, `TileLayer::chunk_digest(k)` (cached; FNV-1a 64 over the chunk's `object, phase, owner, aux` per in-bounds tile, row-major). Cache validity is guaranteed by construction: the only mutators are the setters, which mark the chunk dirty, and `check_invariants()` asserts `cached == uncached` for every chunk in debug and CI builds (a missed dirty mark would otherwise hide a desync). Chunk digests of an island are computed in parallel and combined in chunk order (§11).

### 9.3 World stream order (normative; carried by `kStateHashVersion`, a development counter until S15 and frozen as 2 there)
Scope names are fixed strings; a repeated scope is also addressed by its ordinal. The order below is the target for the complete stream; each stage S1..S14 adds the scopes it introduces and bumps the development counter (§15). Implemented so far: rows 1 (`header`, complete since S3: the seam names are one flat list, `city_pass` first, count + string each), 2 (`clock_rng`) and 3 (`settings`, S3); stream version 101.

| # | Scope | Fields in order |
|---|---|---|
| 1 | `header` | `u32 kStateHashVersion`; `u32 kPrngTimelineVersion`; **SimConfig** in declaration order of §3 (scalars; vectors as count + elements; `small_farm_animals`: `kind_by_draw` u16s, `base` u16, `limit` u8s; `layout`: lists (name str, mask u32), schedule, lookup_order, rect_order, block_to_list as u32 vectors, `first_prodtype`, `block_size`, `default_list`, the six `roles` as u32); **Rules** in declaration order (enums and bools as one byte; `interim_figure_return_ticks` u32); `u64 registry_digest`; seam names (count + str each, in `Seams` declaration order, `city_pass` first) |
| 2 | `clock_rng` | `u64 clock.ms`, `u64 clock.last_tick`, `u64 rng.size`, `u64 rng.table_digest`, `u32 rng.cursor` (= `core_hash`) |
| 3 | `settings` | `u32 world_width, u32 world_height, u8 prop0, u8 prop1, u8 prop2, u16 prop3, bool prop5, u32 prop6, u32 prop7_offset, u8 prop9, u8 prop10, u32 prop11, bool prop12, bool prop13, u32 prop14, u32 speed_percent` |
| 4 | `ids` | `u64 next_figure`, `u64 next_event_seq`, `u32 event750_acc`, `u32 monopoly.acc`, `bool faulted` |
| 5 | `players` | `u64 slot_count`, per slot optional **Player**; `u16 stamp`, `u32 cursor` |
| 6 | `islands` | `u64 count` (= table size), per slot optional **Island** (scope `island`, ordinal = slot) |
| 7 | `figures` | **FigureRegistry** |
| 8 | `events` | `u64 count`, per event in `(due, seq)` order **TimedEvent** with its key |
| 9 | `timers_monopoly` | **ScriptTimers**; **MonopolyTable** |

**Player**: `u32 index, u32 kind, u32 volk, i64 money, u32 upkeep_a, u32 upkeep_b, u32 upkeep_rem, u16 upkeep_stamp, u32 stat_a, u32 stat_b, u32 flags, u32 flags_a0, u8 colour_slot, u8 default_colour, u8 state, u32 bankrupt_deadline, bool defeat_pending, bool free_build`, `name` (u64 length + bytes), research: `u8` states (count + elements), `u16` progress (count + elements).

**Island**: `u32 index, i32 x0, i32 y0, u32 w, u32 h, u32 climate, u32 next_serial, u64 terrain_digest` (0 if none), sweep `u16 stamp, u32 grow_x, u32 grow_y, u32 fauna_x, u32 fauna_y`, `u16 event300_stamp`, objmgr `u16 stamp, u32 cursor`, ring `u16 stamp, u32 cursor`, optional **Drought** `{u32 x, u32 y, u32 target_ring, i32 ring, u32 city_mask, u8 phase, u8 step, u8 state, u16 counter, u16 stamp}` (the narrow `phase`, `step` and `counter` are why `SimConfig::validate` bounds `drought_phases`, `drought_steps · chunk_edge` and `drought_plateau_count`, §3.1), deposits (count; each `str goods, i32 x, i32 y, u32 remaining, bool reserve`), `u64 reserved_object_count`, scope `tiles` (§9.2), scope `cities`: `u64 city_slot_count`, per slot optional **City** (scope `city`).

**City**: `u32 number, u32 owner, u16 tick_stamp`, **CityStock** `{u32 capacity, count + u32 stock[]}`, goods settings (count; each `bool buy, u32 limit, i32 list_index`), build goods (count; each `u32 setting, u64 spent`), trade lists (count of lists; each count + `{u32 goods, u32 amount_88, u32 price}`), scope `index`: `u16 stamp`, `u64 list_count`, per list (scope `list`, ordinal = ListId): `i64 hint, u32 cursor, u64 slot_count`, per slot optional **Building** (scope `building`, ordinal = slot).

**Building**: `u64 id, u32 type, i32 x, i32 y, u8 rotation, u8 byte0e, u8 byte0f, u8 payload_tag`, payload (tags: 0 Inert, 1 Workshop, 2 WeaponsWorkshop, 3 Farm, 4 SmallFarm, 5 Mine; append-only):
- `Inert`: `u16 last, bool blocked, bool suspended, Status`.
- `Workshop`: `u16 last, bool switched_on, bool suspended, bool blocked`, `u32` vectors `out`, `in`, `activity`, `u32 carriers_out`, `Status`.
- `WeaponsWorkshop`: `u16 last`, bools `line_on`, `bool suspended, bool blocked`, `u32` vectors `out`, `in`, `u16` vector `activity`, `u32 carriers_out`, `Status` (`unlocked` is derived).
- `Farm`: `u16 last, bool switched_on, bool suspended, bool blocked, u32 out, u32 raw, u16 activity, u32 workers_out, Status`.
- `SmallFarm`: `u16 last`, bools `line_on`, `bool suspended, bool blocked`, `u32` vectors `out`, `raw`, `u16` vector `activity`, `u32` vector `figures_out`, `Status`.
- `Mine`: `u16 last, bool switched_on, bool deep, bool blocked, u32 out, u16 activity`, optional `u64 deposit`, `Status`.
- `Status`: `u8 code, u8 icons, u16 time` (`shows_icons` is derived).
`vector<bool>` fields are visited through a local `bool` per element; no proxy reference escapes the archive.

**FigureRegistry**: `i64 hint, u32 cursor, u64 slot_count`, per slot optional **Figure** `{u64 id, u32 kind, u32 owner, u64 home, u32 home_counter, u32 job, i32 pos.x, i32 pos.y, u32 cargo_goods, u32 cargo_88, u32 hp, u32 born, optional u16 created_stamp, bool marked_for_delete}`, `u16 stamp`, `u64 n` + expiry ids, `u64 n` + deferred ids. **TimedEvent**: `u64 due, u64 seq, u8 kind, u32 subtype, u32 value, i32 x, i32 y, u64 target, u32 attacker_kind, u64 attacker`. **ScriptTimers**: `u32 last_tick`, `u8` active (count + elements), `u32` counter (count + elements). **MonopolyTable**: entries (count; each `u32 time_s`, optional `u32 player`).

### 9.4 Hash tree
`HashTree` nodes carry `{name, hash}`; `state_hash(tree, depth)` records depth 0 = the nine sections, 1 = islands/players/figures, 2 = cities, 3 = lists, 4 = buildings (opt-in; names are `string_view`s into constants, the tree is allocated only when requested). Peers exchange the root every turn and the section digests only after a mismatch; a desync report names "island 2 / city 0 / production list" instead of "somewhere". The hot path (`state_hash()` without a tree) performs no per-building allocation. Cost: tile chunk digests are cached (only dirty chunks are recomputed) and island sections are independent (§11); the rest is proportional to buildings and figures. If `bench_sim` says a byte-wise FNV is too slow for bulk columns, a wider mixing function is a hash-version change behind `StateHasher::bulk`.

### 9.5 Save frame
`"A3SV"`, `u32 save_version`, `u32 kStateHashVersion`, `u8 rng_table_kind` (0 = the original `msvc_rand_sequence(2048, 1)`, 1 = custom: `u64 n` + u16 values), then the world stream of §9.3 (the writer emits each section with a `u64` byte length in front, so a reader can skip a section it does not know), then the terrain blobs (heights, gfx, fertility per island; digest verified), then a trailing `u64` FNV-1a of everything before. `World::load` verifies version, registry digest, seam names and the table kind; reads the `header` scope first and runs `SimConfig::validate()`, `Rules::validate()`, `ListLayout::validate()` and the seam consistency checks (§3.1) on what it just read, **before** any container is sized from it (a corrupt save or a hostile resync snapshot cannot reach a divisor, a modulus, a shift or an index); then reads the rest with the bounds-checked `ReadArchive` (config-sized containers must have exactly the configured size, tile dimensions are checked in `u64` against `remaining()`, §9.2), rebuilds the derived indexes, runs `check_invariants()`, and recomputes the hash. Every failure is a `Result` error naming the scope path; none is an exception or UB. A save written by version `v` is rejected by `v + 1` with a clear message. `save(load(save(w))) == save(w)` byte for byte. All event kinds are saved (F7). Original savegames are **not** read: their layout cannot be validated without an x86 Windows oracle.

### 9.6 `check_invariants()`: derived equals recomputed, and the structural facts the scheduler relies on
**Structural (every one has its own perturbation test: corrupt exactly that fact in a copy through `WorldTestAccess`, expect `check_invariants()` to name it, and the load path to reject a save with that corruption):**
- *Config-sized containers have exactly the configured size*: `players.slots.size() == player_slots`; `islands.size() <= island_slots`; per island `cities.size() == city_slots`; per city `index.lists.size() == layout.lists.size()`, `goods.size() == warehouse_goods`, `build_goods.size() == build_goods_count`, `trade_lists.size() == trade_lists`; `timers.active.size() == timers.counter.size() == script_timers`; `monopoly.entries.size() == monopoly_entries`; `Player::research.state.size() == research_entries`; `index.service_counters.size() == service_bits`, `house_counts.size() == house_groups`.
- *Payload matches the type*: `building.payload` holds exactly the alternative `PayloadTag` that `class_info(type.cls).payload` names (so `tick_building`'s `get_if` cannot miss); the payload vector sizes equal the type's counts: `Workshop`/`WeaponsWorkshop` `out`/`in`/`activity` and `line_on` vs `prod.outputs`/`prod.inputs`, `Farm`, `SmallFarm::out/raw/activity/line_on` vs the line count, `SmallFarm::figures_out.size() == 5`; a `Mine` type has `outputs[0]`.
- *Islands*: `climate < monopoly_climate_mask.size()`; island rects are non-empty, inside the world (`world_width`, `world_height`) and pairwise disjoint; tile layer `w`, `h`, `chunk_edge` equal the island's and `w·h` fits the columns' sizes; `index == slot`; the loader additionally rejects `w·h·13 > remaining()` before allocating (§9.2).
- *Registry*: present (a `RegistryRef` is never null, also in a moved-from `World`); every `Building::type` exists and `rotation < 4`; the config validated (§3.1) and the registry fits it (`TypeRegistry::validate_against`).
- *Scheduler*: every `BuildingList` has `n >= 1`, `hint ∈ [-1, n)`, `cursor < n` (A8);
**Derived equals recomputed and relational:** `by_id` and every footprint grid equal a from-scratch rebuild; footprints are disjoint and inside the island and every footprint tile carries a non-zero `object`; `id.island == island.index` and `serial < next_serial`; city owner present in the player table; tile owners are present city numbers or `neutral_owner`; `Mine::deposit < deposits.size()`; carrier/worker/animal counters equal the figures referring to that building; service counters and house counts equal their recomputation; figure `by_id` consistent, `expiry`/`deferred` ids exist; event keys unique and `seq < next_event_seq`, figure ids `< next_figure`; cached tile chunk digests equal uncached ones. Run after every step in every property test and at the end of `create`/`load`; production builds run it only on load.

## 10. Loader: scenario → owning state

```cpp
struct ScenarioSpec {                      // plain data; nothing in it points into file buffers
    WorldSettings settings;                // HEAD bytes → properties (D 0x10009cc0); prop14 from the root name (1 ANNO, 3 ANNOADDON1)
    std::uint32_t seed = 1;                // world property 8: PRNG cursor = seed mod len
    std::optional<std::vector<std::uint16_t>> rng_table;      // custom table (default: the original)
    struct PlayerSpec { PlayerId id; std::uint32_t kind = 8, volk = 1; std::int64_t money = 10000; std::uint32_t flags_a0 = 0;
                        std::uint8_t colour = 0xff; std::vector<std::uint8_t> name; std::vector<std::uint8_t> research; };   // research empty = all zero
    struct BuildingSpec { TypeId type; std::int32_t x = 0, y = 0; std::uint8_t rotation = 0, byte0f = 0;
                          std::optional<Payload> payload; };          // nullopt = make_*() of the class; a given payload must satisfy check_invariants
    struct CitySpec { CityNo no; PlayerId owner; std::optional<std::vector<std::uint32_t>> stock; std::vector<BuildingSpec> buildings; };   // PROD2 order = slot order
    struct DepositSpec { std::string goods; std::int32_t x = 0, y = 0; std::uint32_t remaining = 2000 * kTon; bool reserve = true; };       // BERGWERK2 stand-in (D18)
    struct TileObjectSpec { std::uint32_t index = 0; std::uint32_t object = 0; std::uint8_t phase = 0; };   // tile index row-major
    struct IslandSpec { IslandId slot; std::int32_t x0 = 0, y0 = 0; std::uint32_t w = 0, h = 0, climate = 0;
                        std::shared_ptr<const Terrain> terrain;
                        std::vector<std::pair<std::uint32_t, std::uint32_t>> owner_runs;   // {owner, length}, row major from the top-left; missing = neutral
                        std::vector<TileObjectSpec> objects;                                // scenery and growing items
                        std::vector<DepositSpec> deposits; std::vector<CitySpec> cities; };
    std::vector<PlayerSpec> players;
    std::vector<IslandSpec> islands;       // INSELLNK order == table slot order
};
formats::Result<World, std::string> World::create(const ScenarioSpec&, std::shared_ptr<const TypeRegistry>, SimConfig, Rules, Seams);
```
Order of construction (deterministic, so ids and slots are reproducible):
1. **Validate**: `SimConfig::validate()`, `Rules::validate()`, `ListLayout::validate()` and the seam consistency checks (§3.1); registry non-null; `neutral_owner` is not a city number of any city; every island `climate < monopoly_climate_mask.size()`; player ids unique and `< player_slots`; island slots unique and `< island_slots`; island rects disjoint and inside the world (`w`, `h` any positive size: islands that are not a multiple of `chunk_edge` are accepted, edge chunks are clipped, A14/F20); owner runs cover at most `w·h` tiles with owners that exist or are neutral; deposits inside the rect; city numbers unique per island and `< city_slots`; every building type exists and is a building (`list_for` has a value: a stream-loaded PRODTYPE > 192 would make the original create default-class objects in the production list and change `n`; we refuse), rotation `< 4`, footprint inside the island and disjoint. On failure one message names the path (`islands[0].cities[1].buildings[7]: ...`).
2. Config, rules, settings, seams; PRNG table and `rng.set_cursor(seed)`; players by slot (research sized from `research_entries`); `monopoly.entries` sized from config, zero; script timers zero.
3. Islands by slot: tile layer from objects/owners (`phase` from `PlacedObject::phase()` for growing tiles), `sweep.stamp = 0` (world tick 0, D 0x1008c2e6), deposits appended in file order (2000 t, reserve flag), content built **in parallel per island** and placed in table order.
4. Cities by number (`CitySpec::stock` applied), buildings in file order through `create_building(now = 0)` with no cost charged: list slots follow file order, as the original stream loader does (D `0x100264a0` calls `Create(rec, -1, 0)` then the class `Load`). A given payload overrides the `make_*` default (stocks, switches). Mines find their deposit at attach (D slot `+0x128`, position already set).
5. `check_invariants()`; the initial hash goes into the replay header.

`scenario_formats.cpp` (stage S14) is the only adapter: `SCENE/HEAD` → settings (byte map: `[8]→prop0, [9]→prop2, [0xa]→prop1 (0 becomes 1), [0xb]→prop10, [0xc..0xd]→prop3, [0x10..0x11]→prop6`; width/height `[0..7]`; corpus of 50 `.sze`: prop0 = 0 endless, 1 mission, 2 campaign; prop3 = 2550 none or 20..540), `SPIELER` → players, `INSEL5` → islands (rect, climate, deposits from `BERGWERK2` positions with the stand-in amount and goods from `ERZE`), buildings from `PROD2` where decoded. `STADT2`/`PROD` live records are undecoded (O-36): a scenario city starts default-constructed (empty stock) and the adapter returns a `warnings` list instead of guessing. INSELLNK random template selection needs its own sorted candidate order (no `InselCache.txt`) and the original draw order (O-35): the loader takes already-resolved islands; the resolver is a separate, testable step that consumes `DrawSource` draws in the documented order. Deposits' goods (`[deposit+4]` object with slot `+0x80`) are assigned by the scenario generator, not by `BERGWERK2` (O-37).

## 11. Parallelism (optional, benchmark-gated)

Honest cost note: the faithful scheduler performs about `Σ_lists (n·dt/500 + 1)` cheap building steps per advance; for 5 islands with 4 000 buildings and `dt` = 5 ticks that is a few hundred. The scale costs are the hash, the save, the tile layer, content building at load, and later figures, wildlife and AI. The structure spends effort there first.

1. **Load time** (always on): type registry from 921 entries, terrain decode, tile layer build, deposit lists: per island/per entry in parallel, results placed in table order; no PRNG.
2. **Hash and save** (always available): island sections and tile chunk digests are pure functions of state, computed on an `Executor` (default serial) and combined in fixed order.
3. **Island ticks** (stage S16, only if `bench_sim` shows the island phase over budget). Only a draw couples islands; every other cross-island consequence is in `Effects`. Per advance, islands split into
   - `may_draw(island, ctx) == false`: ticked concurrently with a `GuardedDraw`;
   - `may_draw == true`: ticked serially in table order on the global stream.
   Merge is always serial in table order. Correctness: a no-draw island never reads or writes the cursor, so the cursor a drawing island sees equals the serial one. `may_draw` is **derived from the draw-site registry** (§4.2): each `DrawSite` has a conservative predicate (`Event300`: gate open and not `drought_off`; `SmallFarmAnimal`: a small farm with line 1 on and `u16(t - last) >= 500`; `Event750*` are world-level and never in an island tick; future sites add theirs, and `-Wswitch-enum` fails the build for a site without one). A `GuardedDraw` violation is a bug by definition: the island's result is invalid, the executor sets `faulted_` (hashed, so peers diverge loudly and the net layer resyncs from a snapshot) and the advance returns normally; debug and CI builds additionally take a pre-image copy of each concurrently ticked island and re-run a violating island serially. A property test runs random worlds serially with a counting draw source and asserts `draws > 0 ⇒ may_draw` for every site (required in CI for every new draw site).
4. **Per-island streams** (`PrngMode::PerIsland`, D11) would make every island tick pure so no predicate is needed; they change the random outcomes of the game and are the owner's call. **Design-only:** the mode is a reserved `Rules` value rejected by `Rules::validate()` until S16 builds the stream state (per-island cursor in the field list, hash scope and save, `stream_seed` in the header). Parallel island ticks therefore rest on item 3 (the `may_draw` partition over the global stream), not on this mode.

## 12. Extension points: every open item has one home and a stated default

"Default" is the faithful behaviour where the evidence exists and **inert** otherwise (nothing is guessed inside the core). A seam's name is hashed (§9.3), so installing a different implementation changes the header hash.

| ID | Open item (evidence gap) | Extension point | Default now |
|---|---|---|---|
| O-1 | CompLogic AI tick, called every advance from the player table tick (PRNG use, floats, determinism across peers unread; D 0x1009c4d0, 0x1009c630: `CompLogic.dll` with the add-on, else `CompLogicClassic.dll`; property 9 = 0 loads it) | `Seams::ai` (returns `Command`s, applied at once in order) | none: computer seats idle (timeline gap, §6) |
| O-2 | Index `Attach`/`Put` with an explicit slot: no shipped caller (D 0x100259c0, 0x10026b30) | not provided | n/a |
| O-3 | The eight city passes after the building scheduler: 0x10027400, 0x10029610, 0x1002d350, economy 0x10029950 (tax level, PreisLevel), 0x1002b670, random events 0x1002ba80 (PRNG; prop 6 bit 3 and player flag 0x08), 0x1002a5e0, 0x1002d3b0 | `Seams::city_pass[8]` in the original order | no-op (their draws are missing, §6) |
| O-4 | Place side effects: market/Kontor in-place replacement (0x10027b62), land claim for 0x21–0x23 (island slot `+0xe8`), PRODTYPE hooks 0x44/0x49/0x4e/0x41 (0x1002c920, 0x1002ca80, 0x1002d2a0, 0x1002cf60), tile-list fill for p 0x81/0x103, island slot `+0x78` tile update; tile-object placement (roads, walls) | `Seams::on_placed`; tile layer API | none; group > 0xa0 → `Unsupported`; an overlap the original would replace → `Occupied` |
| O-5 | RemoveAt tail: goods 0x51–0x56 handlers, message record 0x2c, island slot `+0x7c` tile clear, PRODTYPE switch 0x10029540, fire/plague extinguish, market stall detach, third argument | `Seams::on_removed` | footprint `object := 0` (stand-in); nothing else |
| O-6 | City-empty predicate (city slot `+0x78` = 0x1002f7f0) | `Seams::city_is_empty` | `false`: no city is ever removed (D9) |
| O-7 | Original spatial order: lazy unstable quicksort on y (0x10027000), swap-remove, rect queries and `FindNext` in set order | `Rules::spatial_order = Original0x10027000` | `YXSlot` (D2) |
| O-8 | Figure bodies (unit slot `+0x74`), registry slot allocation (0x10097250), 7500-tick expiry list membership, destruction routine 0x10097520, job codes | `Seams::figure_tick`; `FigureRegistry` | figures never act; slot policy = list policy (A6); `expiry` empty; return only via `interim_figure_return_ticks` (D12) |
| O-9 | Type slot `+0xcc(2, P)` running cost with the per-instance index `P = bld.slot+0xc4(0x8e)`; `bld.slot+0x90` values other than 1; owner slot `+0x9c(7)` (cost waiver) and `+0x68` | `BuildingType::running_cost` carried; `Player::free_build` | no running-cost state; `free_build = false` |
| O-10 | Trade-list capacity (0x73) | grows on demand | unbounded (data) |
| O-11 | Meaning of tile flag bits (0x80 of byte `+3`, walk/shot/own masks `+0x88/+0xe8/+0x148`) | `TileLayer::aux` | 0 |
| O-12 | Island objects: flames, plague, volcano, wildlife; per-chunk records (`island+0x94`, 16 bytes: object list, flags); fauna sweep 0x10093480 (spawn rules, table 0x100e34e8, draws); object manager traversal (0x10095770, 0x10095ad0) and each object's slot `+0x6c` tick | `Island::reserved_object_count`, `Seams::wildlife_sweep`, `Seams::object_manager` | not created; no draws; manager visits deposits in index order (A10) |
| O-13 | Growth: whether callees of 0x1008b5f0 draw from the PRNG; successor id function | `grow()` contract: `object + 1` | existing code; no draws |
| O-14 | Kontor trade-list entry fields (0x73 u16 `@4` = amount <<8 vs `@6` = price/limit; clear path) | `SetTradeSlotCmd`, `City::trade_lists` | stored only |
| O-15 | Drought tile effects: chunk flag bit 2, city-mask message 0x17, crop objects 0xe2/0xe3 with class in 0x38..0x44 advanced +2 / −1 via island slots `+0x140/+0x188`, event 0xce | `Seams::drought_tiles` | timing machine only |
| O-16 | Consumer of message event 0x15a (AnnoScene 0x100138d0, 0x10013a80) and of `MonopolyTable`/`ScriptTimers` (BigBrother script ops 0x10011ed0–0x10013dab) | UI events; state kept | output only; once the script engine reads them they become state and join the hash |
| O-17 | Registry phase A (sector re-bucketing, float positions, 0x10097f10) | derived bucket structure | not modelled |
| O-18 | Kind-0 event handler (0x100adae0; island slot `+0xcc(x, y, −1, flags, 0)`) | `Seams::tile_cleanup` | none |
| O-19 | Kind-1 event handler (0x100adb80): unit hit (skip if `flags & 0xc6`, damage via slot `+0x9c`, diplomacy via owner slot `+0xfc`, message 8) and tile hit | `Seams::delayed_hit` | none |
| O-20 | Player group objects (class 0x1900, 30 per player, tick 0x10062a40, gate 500 on `[+0x330]`) | `Seams::player_groups` | none |
| O-21 | HEAD fields without a consumer: `[0x02]`→`+0x74`, `[0x0b]`→`+0x7c/+0x7d`, `[0x0c]`→`+0x1d8`, `[0x12]`→`+0x7b`, `[0x13]`→`+0x78`, f32 `[0x14]`→`+8`, `[0x1a/0x26/0x28/0x2a]`→`+0x7e..+0x84`, `[0x20]`→`+0x1dc`, `[0x24/0x25/0x2e]`, `[0x2f/0x30]`→`+0x75`, `[0x32]`→`+0x72` (looks like a text id) | `BuildingType::head_raw` | kept, never read |
| O-22 | Defeat processing 0x10020550 (sets state 3 only behind kind, `+0x26c/+0x274/+0x27c` and owner checks; messages by property 0x1c) | `Seams::defeat` | flag cleared, no state change |
| O-23 | Resident counter object (`city+0x2ec`), running-cost stats (city slots `+0x70/+0x74`), `index+6..+0x4f` presence mask: group record `g` semantics | derived on demand from the lists | service counters / house counts maintained as derived; others not stored |
| O-24 | Monopoly second loop (entries 20–27 from island object queries keyed by `slot+0x84 == k`) and table consumers | `MonopolyTable` | entries 20–27 untouched |
| O-25 | Fur-trapper prey search (0x10071da0(8), float distances) | caller of `fur_trapper_step` | `send_figure` without a target: no trapper sent |
| O-26 | Blocked-flag tile effect (island slot `+0x168` with `{type, x, y, rot, blocked}`) | UI event `BlockedChanged` | event only |
| O-27 | Placement validity: island slot `+0x6c` = 0x1007c790 (terrain, water/coast/mountain via the type flag word, overlap rule 0x10008920, height reference 0x10008f90, writes `rec[8]`) | `Seams::placement_rule` | in island, owners, no overlap (D17) |
| O-28 | PRODTYPE 0x43/0x48 upgrade rewrite of `rec.id` (helpers 0x1002c090, 0x1002c740; flag 0x4000000; flag 0x40000000 set by 0x100279a0) | `Seams::upgrade_rewrite` | none |
| O-29 | Meaning of world properties 0, 1, 2, 3, 11–13 (UI/AI side), prop 6 bits 0, 4, 5, 6, 8–15, prop 5 toggle (AnnoFrame 0x10006fa0) | `WorldSettings` carries them | stored, not read |
| O-30 | Where a type's research id gates availability or placement (only the deep-iron-mine use is known) | `Rules::enforce_research` | off |
| O-31 | SKTIMER load (0x1009cdd0: restores `last`?) and script consumers | `ScriptTimers` | counts only; clamp at 0 (F5) |
| O-32 | Saved layout of the original `ENTRY` event records (0x100ad8d0, 0x100ada30) | none (own save) | n/a |
| O-33 | World `+0x69e0` dword (set to 0x640014 by Reset) and the 32-dword table at `+0x6948` | none | not modelled |
| O-34 | Script engine (BigBrother) step before every advance (AnnoFrame 0x1005fe10) | session-level callback | not part of `anno_sim` |
| O-35 | INSELLNK template choice and its PRNG draws; own sorted candidate order | separate resolver taking `DrawSource` | caller supplies resolved islands |
| O-36 | STADT2 / PROD live city and building records (scenario cities, stocks, running state) | `ScenarioSpec::CitySpec`, adapter `warnings` | default-constructed cities |
| O-37 | Deposit goods source (`ERZE`, 9 kinds → goods ids) and amounts (OQ-035) | `ScenarioSpec::DepositSpec` | 2000 t, reserve flag (D18) |
| O-38 | Building `+0x0d` status bits beyond production.md, `+0x0e` (0x86 base, 0x82 house; property 0x2b from a COMP chunk), `+0x0c` bits 6–7, `rec[7]`, `rec[9]` | `Building::byte0e`, `PlacementWire::byte7` | carried, not interpreted |
| O-39 | Other classes' `Load` slots restoring `+0x0a` (only the workshop `Load` 0x10068640 was read: it does not) | own save restores `last` | n/a |
| O-40 | `[city+0x61c]` mask, `[city+0x17]` byte counter (use unknown) | none | not modelled |
| O-41 | How a city is founded (op 8 editor path calls island slot `+0xf4(player, now, −1)`; the gameplay path, presumably a ship command, is unread) | future command `FoundCity` | scenario cities only |
| O-42 | Semantics of 0x6c, 0x6d, 0x72, 0x75 (kinds 3–11), 0x8c, 0x8f, 0x96, 0x97, 0x6f, 0x70 (kinds 1/2), controller lists 0x80–0x86/0x90; which classes override building slots `+0xdc/+0xe0` (0x71) and `+0xc8` (0x74: vtables 0x100c24e0, 0x100c8848, 0x100c9418, 0x100c9598) | `LegacyRawCmd` | decoded, logged, rejected |
| O-43 | Command 0x98 (SP: city slot `+0xd0` = 0x1002c0e0 building-at-tile fix-up; MP: dropped) | `LegacyRawCmd` | rejected (F17) |
| O-44 | UI veto hook `[client+0x124]` (installer, whether it can differ between machines) | none | not modelled (F16) |
| O-45 | Apply op 9 (0x87) in full and op 2 (0x65) for non-default classes | none | rejected |
| O-46 | Exact client turn-packet header (5 bytes stripped before the ring), ring semantics, whether the server forwards while paused | net layer (M4) | n/a |
| O-47 | Unit-spawn command 0x6e: float scale constant 0x100893e0, divisor 0x10089140, unit position format | `FixedPos` | Q24.8 (D13) |
| O-48 | Island clear / city destroy: does it compact lists (only the list destructor 0x10023610 found) | `remove_city` drops the whole `BuildingIndex` | whole index dropped |
| O-49 | Upkeep sums `upkeep_a/b` maintained by unit create/destroy (0x1001cab0, 0x1001c7f3, 0x1001cd8d) | figure create/destroy hooks | stay 0 until military units exist |
| O-50 | Dynamic confirmation that `tNetClientNet+0x10` is never set elsewhere (static scan of stores to `[reg+0x10]` only) and that slot `+0x120` does nothing else with `last` | a debugger run on the original | F1 uses the placement tick regardless |
| O-51 | **Scripted disasters**: the drought-area constructor 0x10087280 is island virtual slot `+0x15c`; AnnoScene.dll calls it at 0x10013870 and 0x10013a1a (handlers 0x100137d0..0x10013a2a) with caller-supplied x, y and length (the script disaster `DROUGHT`, corroborated by `docs/re/fire-and-ships.md` l.53); island files carry a `DUERRE` record (meaning unread); the other script disasters (fire, plague, volcano) are the same family. Only the wrapper 0x10093a10 (single caller: the 300 s event) is reached from the sim | `start_drought_at(I, x, y, radius)` (pure); planned door: host-only `ScriptDisasterCmd` (§5.8), together with the script engine (O-34) | out of scope for M3: no scripted drought occurs; D7 describes the 300 s event only |
| O-52 | Island **message manager** `[island+0x30]` (slot `+0x10` = 0x100793b0): a /50-paced list calling each item's virtual slot `+0x60(t)` and the removal helper 0x10079270. Item tick bodies unread: whether they draw from the PRNG, write sim state, or only pace UI is not established. Messages carry event 0x15a, consumed by script code | `Island::reserved_object_count` slot; a future `Seams::message_items`; read the item tick bodies first | not modelled; listed as a possible PRNG-timeline gap (§6) |

## 13. Deviations from the original

`fix` = obviously unintended (an error, a hidden limit or undefined behaviour): removed and documented, no switch. `decision-needed` = could change how the game feels: the **faithful behaviour is the default**, any alternative is an opt-in `Rules` option, and the owner decides. "Stand-in" rows (D15–D18) are not choices but places where the evidence is missing; they are `decision-needed` until the code is read. Each decision-needed row D1..D18 is filed for the owner in `docs/open-questions.md` as OQ-036..OQ-053 (in order: D1 = OQ-036, ... D18 = OQ-053; status "open, owner decides"); the table below stays the technical record.

| # | Tag | Original behaviour | Port | Evidence | Switch |
|---|---|---|---|---|---|
| F1 | fix | New building's `last` (and status time) = net client `now`: SP the tick at the start of the previous step (stale up to 50 ticks, uninitialised before the first step), MP 0 forever, load 0. A `last = 0` building steps at every scheduler visit until it catches up (up to 131 extra steps) | `last = status.time = u16(tick at placement)` in SP and MP | AnnoFrame 0x1005b159, 0x1005fdcb, 0x10060624; Create 0x100246ae → 0x10073380 | none |
| F2 | fix | Building ctor leaves `+0x0c` bits 6–7, `+0x04`, `+0x0f` as heap garbage | zero-initialised; bits 6–7 not modelled | 0x100734c0–0x100734e1 | none |
| F3 | fix | Hidden limits: tile lookup assumes footprint height ≤ 8 (`y−8` start), list slot and cursors `u16` (0xffff = none), scheduler `d·n` in `i32` overflows above 32 767 slots, unit id `u16`, player/island ids `u8`, owner nibble, upkeep remainder `u16`, Kontor trade-list index and goods-limit bytes | grids without a row limit; `u32` slots and cursors; `u64` arithmetic; wide ids; `u32` owner column, upkeep remainder, trade list and limit (limit bounded by `trade_limit_max`); 16-bit **stamps** kept (their wrap is part of the model) | 0x100230d0, 0x10035bf0, 0x10023780 | none |
| F4 | fix | Place/Create dereference the type lookup without a null check (crash on an unknown id); a rotation byte > 3 leaves the footprint uninitialised while the stored rotation is masked | unknown type or rotation > 3 rejected before any state change | 0x100279d4, 0x10024072, 0x100279e6–0x10027a1f | none |
| F5 | fix | SKTIMER countdown deactivates only on exactly 0; an overshoot underflows to a huge `u32` and keeps counting | clamp at 0, deactivate, UI event | 0x1009cd51–0x1009cd59 | none |
| F6 | fix | `r2 % island_count` divides by zero with no islands | guard | 0x100ae1bb | none |
| F7 | fix | Pending kind-1 timed events (delayed hits) are written to saves but dropped by the loader; lost over save/load and the MP resync snapshot | both kinds saved and loaded | 0x100ad900–0x100ad9ec, 0x100ada30 | none |
| F8 | fix | World property 3 stored as `value/10` in a byte (loses precision, wraps above 2559) | `u16` | 0x100a325e–0x100a326a | none |
| F9 | fix | The command stream has no sender; the server forwards any client's `0x64..0xc7`; any client can build in or demolish any city and spend its owner's money, edit any unit | host-stamped `issuer`; city commands need `owner == issuer`; editor ops not exposed | 0x100553e0, 0x100555a0, 0x1005ffd0 | none |
| F10 | fix | Dispatcher and handlers trust `len`, counts and fixed offsets; fixed stack buffers (0xa20, 0x201c) | bounds-checked decoder returning `Result`; invalid = identical no-op | 0x1005ffd0, 0x1005c7f0, 0x1005cd50 | none |
| F11 | fix | Island/city/unit/player indices neither range- nor null-checked (`island slot +0xf8(n)` with n > 15 reads other fields as a pointer) | checked containers; a miss is a rejected command | 0x100aee00, 0x100863e0 | none |
| F12 | fix | 0x77 route-node insert with an unmatched tag passes a stale integer as the node pointer | not ported (`LegacyRaw` rejects); when ported: append | 0x1005de18–0x1005de93 | none |
| F13 | fix | Build `flags` come from the payload: a client can send free-build `0x2000000` or no-upgrade `0x4000000` | flags are not on the wire; free build only with `rules.free_build_cheat` | 0x1005c7f0, 0x100589a0 | `free_build_cheat` (default off) |
| F14 | fix | (existing code, not the original) small-farm animal kind taken from island climate | one PRNG draw per step with line 1 on, `kind_by_draw[draw % size()]` (`& 7` for the original 8 entries) | 0x1006f810–0x1006f838 | none |
| F15 | fix | Player money `i32` (signed overflow on wrap) | `i64`; same results in range | 0x1001cfc0–0x1001cfec | none |
| F16 | fix | The UI veto hook `[client+0x124]` can abort a command on one machine only | not modelled; validation reads only hashed state | 0x100589a0 | none |
| F17 | fix | Command 0x98 is sent by the MP client but outside the dispatcher's table (MP drops it); SP executes city slot `+0xd0` | not ported; rejected until the action is identified (O-43) | 0x1005eff0, 0x1005ffd0, SP 0x1005a3f0 | none |
| F18 | fix | Place charges, then `Create` may fail without refund | validation precedes the charge; creation after validation cannot fail | 0x100279a0 | none |
| F19 | fix | The event pop follows a saved next-node pointer that a handler's cancel could free | `find(saved key)`; a vanished saved node ends the loop | 0x100add90, 0x100ade40 | none |
| F20 | fix | The growth visit 0x1008b5f0 loops a fixed 8×8 tile block per chunk with index `y·width + x` and no bounds check: on an island whose width or height is not a multiple of 8 the edge chunks alias the next row or read past the tile array (undefined behaviour in the original) | edge chunks are clipped to in-bounds tiles; `chunk_digest` hashes in-bounds tiles; the corpus test counts non-multiple-of-8 islands (A14) | 0x1008b5f0 | none |
| F21 | fix | The small farm's animal table lookup and counters are unchecked in the original (a kind outside the table or counters reads other bytes of the object); the line-1 result test and the line-0 result test use the low 16 bits only | an empty table, a kind below `base` or beyond the counters/limits consumes the draw and sends nothing (the config validation rejects such tables); counters sized from the table; the result tests keep the low 16 bits (unreachable difference) | 0x1006f84f–0x1006f863, 0x1006f739, 0x1006f857 | none |
| F22 | fix | (a defect of this spec's first §5.2, not of the original) the growth rule `n + n/4`, raised to `min_growth`, does not grow a list shorter than 4 elements when `list_min_growth` is 1..3: the new element's slot would lie outside the vector (undefined behaviour) | the growth is raised to `n + 1` when the rule gives no growth; identical for the original's 10 and for every `list_min_growth >= 4` | 0x10026a00 | none |
| F23 | fix | Hidden limits of the rebuild's own list: `list_min_growth` / `list_initial_size` unbounded (`0xFFFFFFFF` asks `resize` for about 4 G elements) and a `u32` `Slot` that a list of `0xFFFFFFFF` elements would truncate onto `kNoSlot` | both fields validated into `1..kListSizeBound` (2²⁴, §3.1); `insert` cuts `min_growth` to it and returns `kNoSlot` instead of truncating (`SlotList::grown_size`); identical for every valid configuration | 0x10026a00 | none (the bound is the build-time constant `kListSizeBound`, §3.1) |
| F24 | fix | `pop_due` has no bound: with `events_drain_new` a handler that always enqueues a delay-0 event never ends the loop (the same holds for the default loop when each handler enqueues behind the saved node); the original has no guard because only its shipped handlers run | at most `events_max_dispatch` (default 2²⁰) events per call, then stop and report `capped`; the rest stays queued for the next call (deterministic); identical below the cap | 0x100add90 | `events_max_dispatch` (a limit) |
| F25 | fix | The name scans of the type reader map a name they do not know to `0` = UNUSED, silently: an unknown PRODTYPE, FORSCHTYPE or goods name becomes a non-building / no research / goods 0; a duplicate id or an entry without a HEAD record is not noticed; a type that the simulation cannot run (zero footprint, a build cost good outside the charged goods, a ticking class without production data because the PROD3 record is missing or not 68 bytes, a Mine without an output, a class table that disagrees with itself) loads without a message | all of these are load errors naming the entry or the class-table row (`TypeRegistry::build`, `finish()`, §4.4); `UNUSED` and the empty name still mean 0. A typo in a mod no longer silently changes what a type is | PRODTYPE scan 0..0x1a0 at 0x100097b7 (name lookup 0x100138e0), research scan ends at 0x8f (0x10013cf0), goods scan 0..0x8e (0x10013be0), loop exit without a store | none |
| F26 | fix | The type reader zero-fills a 56-byte stack buffer (`rep stosd`, 14 dwords, 0x10009351) and reads the whole HEAD record into it (0x10009365): a record longer than 56 bytes overflows it (the records of Anlagen.dat have 36, 44, 48, 52 or 56 bytes, so the original never hits it). A PROD3 record of another size than 68 bytes is not an Anlagen.dat layout (the PROD3 branch 0x100094d2-0x1000972f reads the 68-byte layout; what it does with another size is not established, OQ-054) | HEAD longer than 56 bytes is cut to 56 (shorter is zero-filled, as the original); a PROD3 of another size means "no production data", which F25 refuses when the class ticks | 0x10009334-0x10009365, 0x100094d2 | none |
| F27 | fix | Hidden limits of the rebuild's own configuration (cf. F23): the table sizes `island_slots`, `city_slots`, `player_slots`, `warehouse_goods`, `script_timers`, `monopoly_entries`, `house_groups`, `research_entries`, `trade_lists`, `build_goods_count`, `chunk_edge` and the world size were unbounded `u32`s from which the loader derives containers without stream backing (`0xFFFFFFFF` asks for gigabytes, `bad_alloc` in code that must not throw); `ceil(w / chunk_edge)` wrapped in u32; the drought state's narrow saved fields could not reach the configured values (a u8 step never equals 320); `neutral_owner` could exceed the city table; `WorldSettings` was read unvalidated; an installed seam named `"default"` hashed like no seam | build-time bounds `kSlotTableBound`, `kEntryTableBound`, `kWorldEdgeBound` and the saved-width limits `kMaxDroughtStep`, `kMaxDroughtCounter` checked by `SimConfig::validate()`; `neutral_owner <= city_slots`; `WorldSettings::validate()`; `validate_seams` refuses the default name; a failed read leaves the target unchanged (§3.1). Identical for every valid configuration of the original | (rebuild-only) | none (build-time constants, like `kListSizeBound`, F23) |
| F-time | fix | Ticks are `u32`: events and `grow()` misbehave after 2³² ticks (99 game days) | 64-bit tick and `due`; gates keep the 32-bit `t` (so `grow()` stays not wrap-invariant, documented); `advance` does not refuse to cross 2³² | 0x100ae040 | none |
| D1 | decision-needed | Free slot = first null after the last stored slot (wrap); lists never compact; the scheduler's `n` counts holes: visit order and economy micro-timing depend on the create/remove history | replicated exactly; `LowestFree` opt-in (compaction is not offered) | 0x10026a00, 0x100255c0 | `slot_policy` |
| D2 | decision-needed | Rect queries and `FindNext` return the order of six y-sorted sets (unstable quicksort, swap-remove): implementation-defined and history-dependent | canonical `(list in rect_order, y, x, id)`; the original mode would need the quicksort and lazy-sort timing transcribed (O-7) | 0x10027000, 0x10023380, 0x10023f50 | `spatial_order` |
| D3 | decision-needed | Events a handler enqueues in front of the saved next node wait until the next advance | replicated; opt-in "process everything due, including new ones" | 0x100add90 | `events_drain_new` |
| D4 | decision-needed | Delayed hits enqueued with delay 0 during the figure phase land in the next advance (one step later: 5–20 ms MP, up to 100 ms SP) | replicated; opt-in second event pass after the figure phase | 0x1003593e | `events_flush_after_figures` |
| D5 | decision-needed | The 30 s monopoly refresh runs only on the first advance (the accumulator advances only inside the body, which the gate skips) | replicated; opt-in "every 30 s" | 0x1009d2a0–0x1009d2ce | `monopoly_every_30s` |
| D6 | decision-needed | The player table ticks `min(d·32/50 + 1, 32)` players per advance (`d` in ticks of 2 ms): 2, 4 and 7 players per advance for the multiplayer steps of 5, 10 and 20 ms, all 32 only from `d ≥ 49`; step-size dependent upkeep and bankruptcy timing; the AI ticks every advance | replicated | 0x1009c4d0 | `player_table_full_tick` |
| D7 | decision-needed | The 300 s event: the gate draws one PRNG value per island per gate even without climate or cities, and **the event's** drought is always centred on the first market/Kontor building of the lowest-numbered city, never random. (Scripted droughts from AnnoScene call the constructor with their own x, y and length: not covered, O-51) | replicated | 0x100939b5, 0x10093a10 (single caller: the 300 s event) | `drought_random_centre` (the 300 s event's position only) |
| D8 | decision-needed | A drag build continues after a failed record and spends money and goods record by record: it can end partially built | replicated; opt-in all-or-nothing | 0x10058a6d | `build_atomic` |
| D9 | decision-needed | Demolish removes the whole city when city slot `+0x78` reports empty, with no confirmation | hook; the predicate is unread so by default no city is ever removed; opt-out reserved | 0x10058f80; 0x1002f7f0 | `keep_empty_city` |
| D10 | decision-needed | The 7.5 min accumulator is reset to 0 (overshoot discarded), so the draw schedule depends on step size | replicated; opt-in `acc −= 450000` | 0x100ae16e | `event750_keep_overshoot` |
| D11 | decision-needed (design-only) | One world PRNG consumed in sequential island order | replicated. `PerIsland` streams over the shared table are **design-only**: a reserved `Rules` value that `Rules::validate()` rejects until stage S16 builds the stream state (per-island cursor, hash scope, save entry, header seed), so no unhashed stream state can exist; if the owner chooses it later it enables pure parallel island ticks | 0x100a1740 | `prng_mode` (reserved) |
| D12 | decision-needed (stand-in) | Carriers, workers and animals walk and return; trip length unknown | a spawned figure never returns (counters stay raised); opt-in return after N ticks | unit slot `+0x74` | `interim_figure_return_ticks` |
| D13 | decision-needed | Figure positions are x87 floats (no oracle); speeds float | Q24.8 fixed point, integer speeds; path and arrival ties may differ from the original; the format is the owner's call when figures are ported | 0x10059220 | none (fixed point only) |
| D14 | decision-needed | The 0x64 group codec merges records of equal type and `rec8` (lossy `rec7`, ≤ 15 followers, \|d\| ≤ 127) and floats for speed | plain widened lists on the wire; the original codec only as optional replay import (`legacy::decode_0x64`) | 0x1005c8cf, 0x1005cc07 | n/a |
| D15 | decision-needed (stand-in) | Unported class tick bodies (houses, markets, public, military, hunters, ships, doctors, schools, castle) do something after their gate; unread | `TickKind::None`: the building is inert and `last` is untouched; gate periods (500 production/market/public/school/hunter/whaler/shipyard/…, 2500 houses 0x10076230, 5000 stalls 0x100702d0 and the inn, 166 towers and cannon towers; the compare constants are read, **D**, Appendix C) are carried as data only; what the bodies do with `last` after the gate is not read (**I**) | various | `ClassInfo::tick` |
| D16 | decision-needed (stand-in) | Unported subsystems consume PRNG draws and mutate state: city passes, fauna sweep, AI, drought tile effects, event handler bodies | inert hooks at the exact original call positions; the PRNG timeline is recorded as short (§6) | see §12 | `Seams::*` |
| D17 | decision-needed (stand-in) | Island placement check (terrain, water/coast/mountain, overlap, owner) | in island, owner tiles, no overlap; can accept what the original refuses | 0x1007c790 | `Seams::placement_rule` |
| D18 | decision-needed (stand-in) | Deposit goods and amounts assigned by the scenario generator | scenario-given goods, 2000 t, reserve flag; depletion only in the add-on edition (D) | 0x10078960, OQ-035 | `DepositSpec` |
| D19 | decision-needed | The small farm picks its animal kind with `draw & 7` from an 8-entry table; the rebuild generalises the mask to `draw % size()` for data tables of any size (identical for the original 8 entries; nothing in the original can have another size) | `draw % size()`; for a custom table of a size that is not a power of two the mapping differs from a literal mask | 0x1006f810–0x1006f838 | `small_farm_kind_mask` (reserved, OQ-055) |
| D20 | decision-needed | `cancel_tile` removes only the first match in queue order and accepts only subtype 4; the pop unlinks an event before its handler runs (so a handler's own-tile cancel finds nothing). Both are disassembled (D), the choice left is whether to offer alternatives (cancel all matches, a later match, a looser subtype) | replicated exactly | 0x100ade40 (0x100ade79 `cmp ebx, 4`), 0x100add90 (0x100addb3..0x100addf5) | none reserved (OQ-056) |

Not deviations (documented so nobody "fixes" them by accident): SP commands are applied between frames and MP commands at turn start (same boundary); `advance` is **not** step-size invariant (advancing 2 × 5 ms differs from 1 × 10 ms in general; a test asserts it); the hash version bump is not gameplay; the AI seam's commands are generated deterministically in every `advance` and are not logged (A15) because the original's AI also runs inside the advance.

## 14. Test plan

Test names contain none of `; * ? [ ] \`; every doctest case is its own CTest test (`ctest --preset dev -j`). Tests that need original files read `ANNO1503_GAME_DIR` and skip with a message when it is unset. CI has no game files: all goldens come from code.

### 14.1 What was already run (candidate incremental design, scratch directory, MSVC `/W4 /WX`)
The archive classes, `visit_count`, `visit_count_clamped`, `run_building_list`, `pop_due` and the building-list policy compiled and ran: growth sequence 1, 10, 12, 15, 18, 22, 27, 33; free-slot wrap order; archive round trip and truncation robustness; hash tree equals flat hash; `visit_count` including `u16` wrap and a 100 000-slot list; clamped count; `run_building_list` with holes; `pop_due` with the saved-next quirk (a new event behind the saved node is seen; a new event when the saved node was the last is not). Not run: GCC, Clang, the constrained-friend `fields` template, `-Wconversion` in `archive.hpp`, the aggregate-arity field-count canary. Stage S1 closes that gap for the archive code. A second known risk is the `linux-gcc-release` CI job (`-Werror -Wnull-dereference`): the repo already needed a pragma for `WorldRng`'s `optional` copy, and the first copies of `std::optional<Building>`, `std::variant<Inert, Workshop, ...>` and `std::optional<Island>` are where GCC 12/13 false positives are known to appear; stage S2 probes those shapes before any stage depends on them.

### 14.2 Unit tests
- **archive**: every primitive round-trips; `ReadArchive` on every prefix of a valid stream fails cleanly; a bool byte of 2, a count larger than the input, a duplicate map key, a variant index out of range and an enum out of range fail; hash-tree root equals the flat hash; FNV vectors of `hash.hpp` unchanged; `WriteArchive`, `HashArchive` (raw-tile mode) and `TraceArchive` visit the same primitive sequence for a populated world.
- **slot list**: growth sequence; first-hole-after-hint wrap; reuse after remove only when the scan reaches it; hint untouched by remove; `LowestFree`; 100 000 random insert/remove operations against a naive transliteration of the original loop.
- **scheduler blocks**: `visit_count` against an **independent reference model** written as a literal transcription of the instruction sequence (`imul 0x10624dd3; sar 5`, signed 32-bit; likewise `0x51eb851f/sar 4`, `0x45e7b273/sar 12`, `0x68db8bad/sar 11`) on its valid range (`d·n < 2³¹`), and against the widened version where the original overflows (n = 40 000); random lists with holes, random `t` sequences with `u16` wrap, cursor wrap; the five-list order; one shared stamp; the stall list never visited; `pop_due` saved-next cases (insert behind, before, after the last, vanished saved node, `drain_new`).
- **create/remove**: `last == status.time == u16(now)`; class payload per PRODTYPE; mine finds its deposit at attach; rotation swaps the footprint; a footprint taller than 8 rows is found; unknown type and rotation 4 rejected without state change; remove leaves hint/size; derived counters inverse; market removal sets `defeat_pending`; serial exhaustion rejected.
- **island/city/player/world-level**: city-ring count for `dt` 5 and 50 and cursor wrap; sweeps use `sweep_chunks`; zero chunks still runs ring, 300 s event and stamp (A1); 300 s event: exactly one draw per gate per island, none when prop 6 bit 0x04, stamp untouched before the gate, climate/city tests after the draw, the gate scans slots `0 .. neutral_owner-1` and `start_drought` `0 .. city_slots-1` (separate tests, once with the defaults 0..14/0..15 and once with a modded `neutral_owner`/`city_slots`); drought centre = first building of `roles.market_list`, floored to `chunk_edge` (also with `chunk_edge` 16); drought timeline (plateau 120 counts, 11 200 ticks expand, catch-up gate); player table visit counts computed from `min(d·32/50 + 1, 32)` (2, 4, 7 for the 5/10/20 ms multiplayer steps run through the clock, 32 from `d = 49`), upkeep `S/15` with remainder and `stamp = t`, `·104/105` in `u32`, bankruptcy deadline `t + 150000` with unsigned compare, neutral player and non-alive player skip both bankruptcy and defeat, `defeat_pending` cleared with the default seam doing nothing; timers: overshoot clamps and deactivates; monopoly body runs once and then never (and every 30 s under the rule); event 7.5 min draw counts (base 1; add-on 1 + 1 when `r1 % 10 == 0`), prop 6 bit 0x80 suppresses before the draw, accumulator discard vs keep, zero-island guard, null-slot return; figure registry count has **no** `+1`; expiry front scan; deferred deletion frees slot and releases the counter; `advance(0)` still runs every subsystem.
- **types**: class table spot checks (809 → SmallFarm, 824 → FurTrapper, 97–101 → House, 161 → MarketStall, 0 and 193+ → not a building); `ListLayout::list_for` over 0..416; `schedule == {5, 1, 4, 3, 2}`; registry digest stable; duplicate id fails; `Builder` defaults.
- **commands**: every alternative round-trips; decode fuzz (prefixes, single-byte flips, length lies, count lies) never crashes, over-reads or allocates beyond the input under ASan/UBSan; each `Reject` reachable; Build with insufficient funds rejects one record and a following affordable record still applies (D8); `build_atomic`; footprint owner rules; issuer ≠ owner leaves the hash unchanged; free-build flag impossible from the wire; `SetSpeed` host only; `LegacyRaw` rejected; `SetProductionLine` on the five classes; Demolish silently skips unowned tiles; trade modes 0/1/9; a limit above `trade_limit_max` and a list index `>= trade_lists` (wire `u32`) are rejected (`BadValue`, `BadIndex`).
- **validation (§3.1)**: a table-driven test per validated field (doctest `SUBCASE` per row): every divisor, modulus and period set to 0 is rejected naming the field; `monopoly_raw_slots = 33`; `kind_by_draw` empty, a kind outside `base .. base+4`; every `ListId` in `schedule`, `lookup_order`, `rect_order`, `block_to_list`, `default_list` and each role `>= lists.size()`; duplicate mask bits; `prng_mode = PerIsland` and `spatial_order = Original0x10027000` rejected as reserved; `World::create` and `World::load` return the message and build nothing; a valid save whose header config is patched to a zero divisor is rejected by `load` before any container is sized.
- **structural invariants and hostile loads (§9.6)**: one perturbation test per check (config-sized container size, payload alternative vs type, payload vector size vs type counts, `Mine` without `outputs[0]`, climate index, island rect outside the world or overlapping, tile dimensions vs columns): `check_invariants()` names it, and `load` of a save carrying it fails with a `Result` error (no exception, no UB under ASan/UBSan); a header claiming `w·h·13 > remaining()` allocates nothing.
- **save/load/advance of a weapons workshop**: after `load`, `unlocked` is empty and the first tick sizes it (`assign`) before writing; equal hash to the live run after 10 000 ticks.
- **existing**: the 47 sim test cases (before S1) untouched except the small-farm cases (S0) and the one line at `sim_test.cpp:193` (S8).

### 14.3 Golden hashes (cross-platform, no game files)
Scenarios are built in code (`synthetic.cpp`); types use the values of fixtures F2 and F6 in `docs/re/production.md` §9 (weaver rate 6 / need 11 / 5 t; farm rate 37 / need 80) under ids in a synthetic range.

**Goldens carry the stage and the stream version they were made at.** Every stage S1..S14 extends or changes the stream (header, settings, players, islands, cities, figures, events), so a golden has no meaning without the field list it was made against. A golden constant is stored as `{name, stage, stream_version, value}` (`fixtures/synthetic/sim/goldens.txt`, one line each), and a test compares only goldens whose `stream_version == kStateHashVersion`; a stale entry fails with "re-baseline" instead of a bare mismatch. While `kStateHashVersion` is a development counter, a stage that changes the stream bumps it and re-baselines **all** goldens in the same commit (`ANNO_UPDATE_GOLDEN=1`); the commit that freezes the stream at 2 (S15) does the same once more. Saves and replay logs record the version, so a file from another commit is rejected rather than silently misread.

| Golden | Scenario | Driver | Stage |
|---|---|---|---|
| G0 | empty world, seed 1234 | advance 10 ms × 1000 | S1 |
| G1 | S1: 1 player, island 64×64 at (100,100) climate NORD, owner runs (2 neutral rows, rest city 0), city 0, weaver (5 t wool) at (110,110), farm (5 t raw) at (120,110) | 20 000 steps of 10 ms (MP style) and 2 000 steps of LCG-chosen 1–100 ms (SP style); checkpoint every 5 000 ticks | S7 |
| G2 | G1 driven by a recorded command log (Build a second weaver at tick 5 000, SetProductionLine off/on, Demolish, rejected commands interleaved) | `Recorder` → `fixtures/synthetic/sim/g2.log`; replay must match every checkpoint | S10 |
| G3 | S2: two islands, two cities, one building of every implemented class, a mine over two deposits, research bytes set for the weapon lines | 100 000 ticks | S11 |
| G4 | S3: players with upkeep and bankruptcy, events, timers, 7.5 min event with the add-on flag, monopoly once | 600 000 ticks (20 game minutes) | S12 |
| G5 | S4: growth tiles, 300 s drought gate with a market building | 1 000 000 ticks | S13 |

Semantic asserts (independent of the hash, so a golden cannot hide a wrong simulation): in G1 the weaver ends at exactly the F2 state after 70 000 ticks (`out == 696`, `in == 4`: 116 full steps from 5 t of wool at need 11), has reported `MissingInput` and raised `blocked`; at tick 30 000 it has done 60 ± 3 steps (the scheduler may delay a step but never lose or double one); the farm never has more workers out than `max_figures`; the production list has size 10 and hint 1 after the second insert; `city.index.stamp` equals the tick of the last city visit; the drained UI-event list hashes to a second golden constant. Update protocol: a golden changes only with `ANNO_UPDATE_GOLDEN=1` (the test prints the new value and the first differing checkpoint) and a commit message that states which field-list or rule change caused it. The same constants must pass on MSVC (`/fp:strict`), GCC and Clang, Linux and Windows, and under ASan/UBSan.

### 14.4 Property tests (deterministic LCG seeds in the test only; iteration counts reduced under sanitizers; seeds logged on failure)
- **Random command streams** on S1/S2/S3: generate Build/Demolish/SetLine/SetSpeed/garbage, apply, advance random amounts; after every command and every advance `check_invariants()` is empty; every rejected command leaves `state_hash` unchanged.
- **Determinism**: same scenario and log twice → equal hash at every tick; replay from a `ReplayLog` equals the live run.
- **Save/load**: `load(save(w))` has equal hash and `save(load(save(w))) == save(w)` at random points of random runs; both continue 10 000 ticks and stay equal.
- **Copy independence**: copy a `World`, advance only the copy; the original's hash is unchanged; the PRNG table is shared (pointer equality), not copied.
- **Allocation-pattern independence**: the same stream twice in one process with different junk allocations interleaved (a deterministic LCG decides the size and lifetime of throw-away `new`/`delete` blocks between and during steps, so containers land at different addresses) → equal hashes (catches pointer-order bugs). Sim state uses the default `std::vector`/`std::map` allocators, so there are no arenas to poison; the junk interleaving is the whole test.
- **Perturbation**: a `PerturbArchive` (a `ReadArchive` that flips the k-th primitive while loading a saved world) for every k in a sampled set → the hash changes (every field of the list participates); plus a hand-listed mutation table (change one field through `WorldTestAccess`, expect the hash to move); plus a **field-count canary** per struct (`static_assert(field_count<Workshop>() == N)` via the aggregate-arity trick, verified on the three compilers in S1; if a compiler refuses, the fallback test counts primitives per struct against a constant).
- **Effects equal synchronous**: a reference executor applies effects synchronously inside the tick; its world hash equals the buffered one for random scenarios (C1). For the serial phases (C2) a scripted `figure_tick` seam issues `Reserve`/`Deliver`/`GoodsMove` and the test asserts the merge-after-each-call order against a synchronous reference, including a failed `Reserve` (building removed) taking 0.
- **Chunk cache**: random tile writes through every setter; cached digests equal uncached; island hash independent of the thread count (1, 2, 8).
- **List model**: random create/remove sequences against the reference model for slot choice, `hint`, `n`; `by_id` and grids equal a from-scratch rebuild after every operation.
- **Bisect round trip**: corrupt one building field in a copy mid-run (`WorldTestAccess`); `first_divergent_entry` finds the entry and `diff_trees` names `…/building[slot]`.
- **Step-size sensitivity**: the test asserts that two runs with the same step sequence agree and that 2 × 5 ms differs from 1 × 10 ms in general (nobody "fixes" this by accident).
- **MayDraw soundness** (S16): `draws > 0 ⇒ may_draw` for every site over random worlds; executor with 1, 2, 8 threads equals serial.

### 14.5 Game-file tests (skip without `ANNO1503_GAME_DIR`; one clear message if the folder lacks `Anlagen.dat`)
`types_test`: the registry builds all 921 entries; class table against the PRODTYPE tally; weaver 1001 costs 4 wood, 3 tools, 300 gold; palace 1380 costs 50 bricks, 15 wood, 40 tools, 20 marble, 15 000 gold; FORSCHTYPE ids of Appendix B; every WARE/ROHSTOFF name resolves. `scenario_test`: every `Scenes/**/*.sze` (50 files) → `ScenarioSpec` → `World::create` → 5 simulated minutes → `check_invariants`, twice with equal hashes, save/load/save identity, no unknown building type; the number of islands with `w % 8 != 0 or h % 8 != 0` is counted and printed (A14, F20; expected 0, a non-zero number is reported to the owner, not a failure); the 5 add-on and 5 base scenarios with islands run 20 000 ticks under UBSan. The state spec's own spot values (`sim_test` compares the PRNG table with the DLL's bytes) stay.

### 14.6 Lint, sanitizers, portability
`tools/lint_sim.py` (a CTest case from S1) scans `src/sim` for `unordered_`, `float`, `double`, `rand(`, `time(`, `chrono`, `random_device`, mutable `static`/globals, `long`, `size_t` in saved structs, `vector<bool>` in new state, `<random>` engines, `memcpy`/`memmove`/`reinterpret_cast`, `getenv`, `std::thread`/`jthread`/`async`, `std::hash`, `set`/`map` keyed by a pointer, `#define`, and a `constexpr` count outside `config.hpp`/`production.hpp`, and fails on any hit outside a per-line allow-list with a reason. **The allow-list is created with the lint in S1** (`tools/lint_sim_allow.txt`, one `path:pattern  # reason` line per exception), because the lint as specified fails on code already in the tree: `std::vector<bool>` in the `production.hpp` payloads (`line_on`, `unlocked`; visited through a local `bool`, §9.3), `size_t` in `Mine::deposit` (`optional<size_t>`, an index into `Island::deposits`, converted with `static_cast<u64>` at the archive boundary), `constexpr` counts in `rng.hpp` (`kOriginalRngLength`) and the new sentinels and versions in `world.hpp`/`ids.hpp`/`command.hpp` (`kStateHashVersion`, `kPrngTimelineVersion`, `kNoSlot`, `kAppendIndex`), and `float`/`double` in `production.cpp` (the load-time conversion only). Rules are narrowed rather than left to fail: `constexpr` is allowed in a named header list for named sentinels, and a new allow-list line needs a reason or review rejects it. **The sanitizer gate:** `cmake/AnnoWarnings.cmake` today adds only `-fsanitize=address,undefined,float-cast-overflow`, with no `-fno-sanitize-recover`, so UBSan findings (signed overflow in footprint arithmetic, shifts, invalid enum loads) would be printed while `ctest` still passes. S1 adds `-fno-sanitize-recover=all` to the `asan` preset (and `UBSAN_OPTIONS=halt_on_error=1:print_stacktrace=1` to its test environment) and proves it with a deliberately failing probe: a tiny executable compiled only in the `asan` preset that performs a signed overflow, run by `cmake/RunUbsanProbe.cmake` as a CTest case that passes only when the probe started, exited non-zero and printed UBSan's `signed integer overflow` report (a plain `WILL_FAIL` would also pass on a crash, a link error or a missing runtime); if the gate is ever weakened the probe exits 0 and the case fails. The flag goes onto the simulation targets (`SANITIZE_NO_RECOVER` in `anno_target_defaults`); the other suites keep recovering UBSan until a first Linux asan run has shown them clean, then the keyword is added to their targets as well. The whole sim suite runs in the `asan` preset (ASan + UBSan, no recovery); `ReadArchive` and the codec fuzz run there with mutated saves/commands. MSVC `/W4 /WX /fp:strict`, GCC and Clang `-Wconversion -Wsign-conversion -Werror -ffp-contract=off`; macOS builds as a check. The sim has no float in state; the only floats are the existing load-time conversion in `production_type`. Archives write explicit little-endian bytes, so endianness cannot leak.

### 14.7 Differential fixtures and benchmarks
For later, from the original (x86 Windows or Wine; nothing of the original is stored in git): `fixtures/diff/*.jsonl` lines `{tick, city, list, cursor_before, cursor_after, visited_slots[], stamp}` and `{tick, rng_cursor}`; a harness replays a scenario in our sim and compares; the PRNG cursor is compared first (§6). `tools/bench_sim` (`docs/perf/README.md`): ns per advance and per building tick for 5 islands × 4 000 buildings and 8 players; full hash; incremental hash after a command; save and load; registry build; parallel vs serial island section hash. Budgets are set from the first measurements.

## 15. Staged implementation plan

Each stage compiles warning-free, keeps every earlier test green, adds its own tests, updates the docs it touches (this file, `core.md`, `production.md`), is committed in commit-sized pieces with conventional prefixes, and ends with `cmake --preset dev && cmake --build --preset dev && ctest --preset dev -j` plus the `asan` preset green. New `.cpp` and test files are added by name to `src/sim/CMakeLists.txt` in the stage that introduces them. Dependencies: S0 → S1 → S2 → S3 → S4, S5; S6 needs S3 and S5; S7 needs S2, S4, S6; S8 needs S7; S9 needs S8; S10 needs S8 and S9; S11 needs S7; S12 needs S7; S13 needs S7 and S10; S14 needs S6 and S11; S15 needs all; S16 needs S12 and a benchmark.

**S0. Correct what is wrong today.** `fix(sim): small farm animal kind is a world PRNG draw` (§7.4); `docs: apply the RE corrections` (§16). Files: `production.hpp`, `production.cpp`, `tests/production_test.cpp`. Tests: "small farm draws once per step with line 1 on", "small farm no draw when line 1 off or gated or suspended", "small farm kind is kind_by_draw indexed by draw modulo table size" (with the original 8 entries and with a 3-entry table); the 13 existing small-farm call sites adapted. The docs half (`docs/sim/production.md`, `docs/re/production.md`) was applied on 2026-10-18 and is a separate docs commit. Exit: 36 existing tests (small-farm ones adapted) + the new ones.

**S1. Infrastructure.** `ids.hpp`, `draw.hpp`, `archive.hpp`, `state_fields.hpp` (scopes `header` minimal and `clock_rng`), `clock.hpp` (`begin`/`end`), `rng.hpp` + `sim.cpp` (shared `RngTable`, `stream()`), `world.hpp` (`state_hash` via `fields`, `core_hash`, `kStateHashVersion` = the first development value, 100), `tools/lint_sim.py` + `tools/lint_sim_allow.txt` (**the allow-list is created with the lint, a reason per line**, §14.6) + CTest case; `cmake/AnnoWarnings.cmake` (**`-fno-sanitize-recover=all` and `UBSAN_OPTIONS=halt_on_error=1` for the `asan` preset**) with `src/sim/tests/ubsan_probe.cpp` (asan preset only, a deliberate signed overflow, CTest `WILL_FAIL TRUE`); `rng.hpp` keeps **copy-on-move** (explicit move operations that copy the table pointer; `sim_test.cpp:82-88` unchanged and green). Tests (`archive_test.cpp`, additions to `sim_test.cpp`): "archive primitives round trip", "archive prefixes fail cleanly", "archive hostile counts and enums fail", "hash tree root equals flat hash", "fnv vectors unchanged", "core_hash equals the v1 golden", "state_hash relational properties", "rng copy shares the table", "rng moved-from generator keeps its table", "rng stream cursor mix is the defined function", "clock begin end keeps last_tick", "field count canary", "lint_sim passes", "ubsan probe is caught by the asan gate", golden G0 (stage S1, development version). Exit: existing hash tests pass (all 47 earlier sim test cases green, including the moved-from `WorldRng` case); the full CI matrix (MSVC, GCC, Clang+ASan/UBSan) is green on a trivial `fields()` instantiation before anything depends on it (the compile risk of §14.1). **Status 2026-10-19: implemented, verified on MSVC and, in WSL, on GCC 13.3 (`-O3 -Werror` with the CI warning set; and ASan + UBSan with `-fno-sanitize-recover=all`, the sim suite incl. the game-file cases), not on Clang and not on the CI jobs (`linux-gcc-release`, `linux-clang-asan`, the `asan` preset, macOS). S1 stays open on that item: S2 starts only after those jobs are green and the result is written here.**

**S1 as implemented (2026-10-19).** Files: `ids.hpp`, `draw.hpp`, `archive.hpp`, `hash_tree.hpp` (the `HashTree` type, split out so `world.hpp` does not include the archives), `field_count.hpp` (the aggregate-arity probe), `state_fields.hpp` (`StateHeader`, `WorldClock`, `WorldRng`, `World`), `rng.hpp`/`sim.cpp`, `clock.hpp`, `world.hpp`; tests `archive_test.cpp`, additions to `sim_test.cpp`, `golden.hpp` (test helper), `fixtures/synthetic/sim/goldens.txt` (G0 at stage S1, version 100); `tools/lint_sim.py` + `tools/lint_sim_allow.txt`; `cmake/AnnoWarnings.cmake`, `src/sim/tests/ubsan_probe.cpp`. Choices where the spec left room:
- `Writer`-side primitives take values, `ReadArchive`'s take references (§4.3); `count` on a write archive takes the value. `scope(name, f)` and `scope(name, index, f)` are two overloads; `f` takes no arguments.
- `ReadArchive::enumv` needs `EnumInfo<E>::valid(u8)` (`ContiguousEnum<N>` for a dense enum); `ar::variant<Tags>` takes the tag table as a constant `std::array<u8, N>` template argument (N = number of alternatives and pairwise distinct tags, both `static_assert`ed). `ar::map` reads keys that must be strictly ascending (this rejects a duplicate and the one unsorted order a re-save could not reproduce).
- `HashArchive` has a raw mode (`HashOptions::raw`): scopes are transparent, so its digest is the FNV-1a of the `WriteArchive` bytes of the same stream; this is the "same primitive sequence" invariant, and the S5 raw-tile mode will build on it. `TraceArchive` also keeps the written byte stream and records scope markers.
- `ReadArchive` error text is `<scope path> (byte N): <message>`.
- All default `WorldRng`s share one table built once (a function-local `static const shared_ptr`); `shared_table()` exposes it.
- `rng_mix` is a free function; `stream(seed, slot)` is a const member returning a generator over the same table.
- `SiteDraw` (draw.hpp) is the small adapter that lets `DrawRef::of` forward a production rule's draw to `DrawSource::next(site)`; `GuardedDraw` is S16 and not declared.
- The lint scans `src/sim/include` and `src/sim/src` (`.hpp`, `.cpp`, `.h`, `.inl`, `.ipp`; not the tests); a scan directory that is missing or holds no source file is an error, and the self-test walks throw-away trees (empty root, hollow root, clean, violating, stale entry, entry without reason) through `lint_tree()` and `main()` so the gate cannot pass by scanning nothing. Its `size_t` rule flags `size_t` as a data member of a struct or class only (local indices and parameters are fine), the other rules are listed in the script's docstring (the `#define` rule runs on the preprocessor lines the other rules blank out), and the allow-list holds the pre-existing `std::vector<bool>` payload fields, `Mine::deposit`, the load-time float conversion of `production.cpp`, the archive's read offset and the one byte view of a string in `archive.hpp`. An allow-list entry that matches nothing is itself an error. The lint and its self-test are CTest cases named `anno_sim_tests_lint_sim_passes` and `anno_sim_tests_lint_sim_self_test` (so that `ctest -R anno_sim_tests` runs them); they are registered whenever Python 3 is found, and configuration fails when it is not found and `ANNO_REQUIRE_LINT` is ON (the default when the `CI` environment variable is set), so a CI image without Python cannot drop the determinism gate with a green run.
- The sanitizer gate: `-fno-sanitize-recover=all` on compile and link for the simulation targets under `ANNO_SANITIZE` (keyword `SANITIZE_NO_RECOVER` of `anno_target_defaults`), `UBSAN_OPTIONS=halt_on_error=1:print_stacktrace=1` in the environment of every `anno_sim_tests` case (keyword `UBSAN_HALT` of `anno_add_doctest`), and `anno_sim_ubsan_probe` with the CTest case `anno_sim_tests_ubsan_probe_is_caught_by_the_asan_gate`, which runs `cmake/RunUbsanProbe.cmake` (start marker, non-zero exit and UBSan's report, all three required); both exist only with `ANNO_SANITIZE` on GCC/Clang. **Verified 2026-10-19 in WSL (GCC 13.3):** the sim suite including the game-file cases passes under `-fsanitize=address,undefined,float-cast-overflow -fno-sanitize-recover=all`; the probe stops with exit 1 and prints "runtime error: signed integer overflow", a recovering build prints it and exits 0 (so the script would fail it); the script's four outcomes (ok, recovered, crash, never started) were run against stand-in probes. **Not run:** Clang, the `asan` preset through CMake, the CI jobs, macOS. The field-count canary was compiled with MSVC, with the Clang 19 front end (`clang-tidy` of Visual Studio 2022) and with GCC 13.3, over scalar, enum, nested-aggregate members and the real payloads of `production.hpp` (`Workshop`, `WeaponsWorkshop`, `Farm`, `SmallFarm`, `StepResult`, `StatusTracker`, `SmallFarmAnimals`: vector, `vector<bool>`, enum and nested members).
- **Review round (2026-10-19), all tagged fix (own-design hardening, no change of the original's behaviour):** `ar::seq` / `ar::map` fill one element at a time and take `min_wire_bytes` (hostile-count memory bound, test with a 512-byte element); `ar::variant<Tags>` checks distinct tags at compile time and writes a valueless variant as its first alternative; scope names are `ScopeName` literals; `HashArchive::finish()` is repeatable; byte output is appended element by element (GCC 12/13 `-Wstringop-overflow` guard); `World` and `WorldRng` carry a structured-binding canary next to their members (a member added without a line in `fields()` fails to compile), and `field_count<T, Max>` refuses a type with more than `Max` members instead of saturating; `hash_state()` is the generic form of `World::state_hash` so the depth rule is tested on a value that nests scopes (the world has only level-0 scopes until S2, so the pass-through in `World::state_hash(tree, depth)` itself becomes observable then: S2 adds a world-level depth test as soon as the first nested scope exists); goldens compare the stage as well as the version, and `ANNO_UPDATE_GOLDEN` is refused when `CI` is set; the lint covers the rules listed in §14.6 and fails on a scan that scans nothing; mutation survivors of the first review got tests (stale target elements of `ar::seq`, `BuildingKey` member order with designated initialisers, every `DrawSite` value pinned, `Event300` comment).
- `src/sim/tests/sim_test.cpp` still reads `w1.rng.next()` (public members until S8).

**S2. Slot list, scheduler blocks and the early GCC-release probe (pure code, no `World`).** `slot_list.hpp`, `scheduler.hpp`, `src/sim/tests/container_probe_test.cpp`. Tests (`slot_list_test.cpp`, `scheduler_blocks_test.cpp`, `container_probe_test.cpp`): the §14.2 slot-list and scheduler-block cases, including the independent reference model; and **"container shapes copy move emplace"**: a representative `std::optional<std::variant<Inert-like, Workshop, WeaponsWorkshop, Farm, SmallFarm, Mine>>` built from the real `production.hpp` payloads (copy, move, copy-assign, `emplace`, `reset`, vector-of-optional growth), an `SlotList<T>` of it, and a stand-in with the exact member shape of `TileLayer`/`Island` (a default constructor, several vectors including `mutable` ones, a nested `optional<struct>`, `reset(w, h, edge)`), all compiled and run under **linux-gcc-release (`-Werror -Wnull-dereference -Wmaybe-uninitialized`)** and clang-asan, so a GCC 12/13 false positive is found (and worked around once, in one place) before S5/S6 depend on those shapes. The same file is re-pointed at the real `TileLayer`, `Island` and `Building` when they land (S5, S6). Exit: the blocks are proven before `World` uses them; the probe is green on all four CI jobs.

**S2 as implemented (2026-10-19).** Started before the S1 CI confirmation (the staged work runs in sequence by the owner's workflow); the CI jobs have therefore still not run on anything, and "green on all four CI jobs" is open. Files: `config.hpp` (only `SlotPolicy`; S3 adds `SimConfig`, `Rules`, `ListLayout` to this file), `slot_list.hpp`, `events.hpp` (`EventKind`, `TimedEvent`, `EventKey`, `EventQueue` with the inline `enqueue` and `cancel_tile`: `pop_due` is proven against the real queue, so the types had to exist; dispatch, seams and `timers.hpp` stay in S12), `scheduler.hpp`; tests `slot_list_test.cpp`, `scheduler_blocks_test.cpp`, `container_probe_test.cpp`. Choices where the spec left room:
- `scheduler.hpp` also has `run_scheduled_lists` (the `run_building_list` of §14.1's candidate, over a `vector<SlotList<T>>`, a `span<const ListId>` schedule and the shared stamp), so that "the five-list order", "one shared stamp" and "the stall list never visited" are tested before `ListLayout` and `City` exist; `building_scheduler` of §5.5 is a call of it.
- `visit_count_clamped(.., n = 0)` is 0 (raise to 1 first, then cap at n); `run_slot_list` with a cursor at or beyond `n` visits nothing and leaves the list untouched (A8), and looks the slot up in every iteration. `pop_due` unlinks the event before calling the handler (D 0x100addb3..0x100addf5) and finds the saved next node again by its key; the handler of an event sees the queue without it.
- `SlotList::insert`: F22 (§5.2, §13) and the modulo reduction of the scan start. `cancel_tile` follows D 0x100ade40: the first match in queue order by kind and tile, and only for subtype 4 (the first S2 version matched any subtype, which the original does not; the review round corrected it, OQ-056).
- Tests: `visit_count` against a literal transcription of the instruction sequence (signed `imul` by the magic number, high half, `sar`, sign fix) for the four divisors, 27 list sizes up to 32 768 and every `d` of the valid range (7 077 888 cases); the widened version at n = 40 000 (the original's product is negative from d = 53 688) and at n = 1 000 000 with a modded divisor (no 32-bit product either); a naive transliteration of the original's insert loop (explicit wrap by comparison, 100 000 operations, both policies, `min_growth` 10 and 3); the independent models for `run_slot_list`; every `pop_due` case of §14.2 and `drain_new`. A mutation pass over the three headers (46 mutations: scan start, growth factor, F22 guard, policy switch, hint handling, `+1`, cap, 16-bit wrap, 32-bit product, wrap and cursor of the round robin, shared stamp, list order, due comparison, saved node, `find` versus `begin`, sequence counter, due tick, `cancel_tile` fields) was run against the tests by hand-compiling them with GCC; the first round had two survivors: the 32-bit unsigned product (a test gap, closed by the n = 1 000 000 case) and one mutation that did not change what `pop_due` does (equivalent, replaced by a better one); every other mutation is killed.
- The probe (`container_probe_test.cpp`): `optional<variant<Inert-like, Workshop, WeaponsWorkshop, Farm, SmallFarm, Mine>>` from the real `production.hpp` payloads (copy, move, copy- and move-assign over every other alternative, self-assign through a reference, `emplace`, `reset`), `vector<optional<..>>` growth and shrink, `SlotList` of it (copy, move, assign, a vector of six), and stand-ins with the member shape of `TileLayer` (default constructor, four columns, `mutable` digest cache and dirty flags, `reset`) and `Island` (nested `optional<struct>`, `shared_ptr<const>`, `map`, vectors of vectors, `vector<optional<City-like>>`), snapshotted in a `vector<optional<Island-like>>`. Compiled with GCC 13.3 (WSL) at `-O3 -DNDEBUG` and at `-O2` with `-Wall -Wextra -Wpedantic -Wconversion -Wsign-conversion -Wshadow -Wnon-virtual-dtor -Wold-style-cast -Wcast-align -Wnull-dereference -Werror` (`-Wmaybe-uninitialized` is in `-Wall`) and run under `-fsanitize=address,undefined,float-cast-overflow -fno-sanitize-recover=all`: **no false positive on GCC 13 and none on GCC 12 at `-O3`** (the GCC pragma of `WorldRng` stays the only one). The review round compiled the probe with GCC 12 at `-O2` and found one: `-Wnull-dereference` in `std::_Bit_reference::operator bool`, inlined from `vector<bool>::operator=(initializer_list)` / `assign(initializer_list)` (`SmallFarm::line_on = {true, flag}`); the test now builds the vector with `assign(2, true)` and an element store, which both GCC 12 and 13 accept. The same shape will occur in production code at S6 when `SmallFarm` lives in `Building` (fill `vector<bool>` with `assign(n, v)` and element stores, not with a braced list). Checked matrix: GCC 13 `-O1 -O2 -O3` and GCC 12 `-O2 -O3` with the CI warning set, plus GCC 13 under ASan/UBSan without recovery; GCC 12 is not a CI compiler. The same files are clean on the Clang 19 front end of Visual Studio's clang-tidy with the CI warning set (`clang-diagnostic-*`, no codegen), and on MSVC `/W4 /WX`. Not run: GCC 12, Clang as a compiler, the CI jobs, macOS. GCC found one sign conversion in a test that MSVC accepted.
- **Review round (2026-10-19, S2), fixes in the same stage:** (F23) `list_min_growth` and `list_initial_size` get the upper bound `kListSizeBound` (2²⁴) with the predicates `valid_list_min_growth` / `valid_list_initial_size` (S3's `validate()` calls them), `SlotList::insert` cuts `min_growth` to it and returns `kNoSlot` instead of truncating a `u32` slot (`grown_size`); (F24) `pop_due` takes `max_dispatch` (default `kEventsMaxDispatchDefault`, 2²⁰) and returns `PopResult{dispatched, capped}`, so a self-re-enqueueing handler stops with a report instead of looping (tests: both loop modes, cap edge cases); `visit_count` / `visit_count_clamped` return `n` for `div == 0` instead of dividing; `valid_slot_policy` for S3; `cancel_tile` is faithful (D: subtype 4 only) and its test no longer asserts that a subtype-2 cancel succeeds; the unlink-before-dispatch order is tagged D and tested from inside a handler; the `visit_count_clamped` comment and §5.1 now say phase B / `/50` / `figure_b_div` (phase A, `0x1009a8d5`–`0x1009a907`, `/250`, is derived spatial data) and `kMagics` has the `/250` (`0x10624dd3`, `sar 4`) and `/2000` (`0x10624dd3`, `sar 7`) transcriptions, with the clamped test looping over all six; the §1.1 arithmetic note says 32 768 slots (65 535 · 32 768 < 2³¹); the probe's `line_on` initialisation (GCC 12 `-O2`, above). Mutation survivors of the review got tests and were re-run as mutations with GCC 13 (all killed): `lid >= lists.size()` → `>` (schedule ids `2`, `7`, `0xFFFFFFFF`), `cancel_tile` comparing only one tile coordinate (queries `{3,9}`, `{9,4}`, `{4,3}`), `SlotList::insert` writing the cursor in a hole fill or a growth (a directed test, and the random model now fixes `cursor` and `same_state` compares it), `subtype != 4` guard removed, `div == 0` guard removed. The unbounded-handler cap mutation is covered by the cap tests (without the check they never end). Open: whether the enqueue of kind 1 with subtype 1 (three `f32`) is ever called (OQ-054); `TimedEvent` has no field for it.
- Open for the stages that re-point the probe: S5 replaces `TileLayerLike`/`IslandLike` by the real classes, S6 `BuildingLike` and `InertLike` by `Building` and `Inert`.

**S3. Config, validation, layout, settings, types.** `config.hpp` + `config.cpp` (`SimConfig`, `Rules`, `ListLayout` with roles, **`SimConfig::validate()`, `Rules::validate()`, `ListLayout::validate()`**, digests), `settings.hpp`, `types.hpp` + `types.cpp` + `goods_table.cpp` (registry, `Builder`, class table), `seams.hpp` (names; grows by stage), `synthetic.hpp` + `synthetic.cpp` (builder registry); `World` holds config, rules, settings, seams, registry; header and settings scopes. Tests (`types_test.cpp`, `config_test.cpp`): "list layout original maps", "list layout original roles", "list_for over 0 to 416", "schedule order 5 1 4 3 2", "class table spot checks", "class table payload tags", "registry digest stable", "registry duplicate id fails", "builder defaults", "rules defaults are faithful", "config digest changes with every field", **"config validate rejects each bad field"** (table-driven, one `SUBCASE` per divisor, modulus, period, table size, index table and shift bound of §3.1), **"layout validate rejects each bad id and mask"**, **"rules validate rejects reserved values"** (`PerIsland`, `Original0x10027000`) (`World::create` takes a `ScenarioSpec` and arrives in S5, so the "create/load refuse an invalid config" tests belong to S5; S3 tests the three `validate()` functions directly), and the game-file cases of §14.5 (skipped without files). Files also: `src/sim/tests/config_test.cpp`. Exit: all 921 entries produce types; digest identical across platforms; no unvalidated divisor exists in code that follows. **S3 checklist from the S2 review (contract rows the tests must contain):** `validate()` rejects `div == 0` for each of the six `*_div` fields (`building_div`, `city_ring_div`, `player_div`, `figure_a_div`, `figure_b_div`, `objmgr_div`), `slot_policy` outside `valid_slot_policy`, `list_min_growth` and `list_initial_size` outside their predicates (`0`, `kListSizeBound + 1`, `0xFFFFFFFF`: one `SUBCASE` per field), and `Rules::events_max_dispatch == 0`; `Rules::events_max_dispatch` and (if added) `SimConfig::list_size_bound` are in the digest and in `SlotParams`/`pop_due`'s arguments; `check_invariants` reports a capped `pop_due` (S12) and a list with `cursor >= n`.

**S3 as implemented (2026-10-19).** Files: `config.hpp` (the S2 enum and bounds, now with `SpatialOrder`, `PrngMode`, `ListSpec`, `ListLayout`, `SimConfig`, `Rules`, `kMaxStampPeriod`, `kOriginalCityPassCount`), `config.cpp` (the three `validate()` functions, `ListLayout::list_for` / `original()`, the digests), `settings.hpp`, `seams.hpp` (the whole seam table of §4.12 with `Seams::sized_for`, `seam_names`, `validate_seams`; the types the signatures name are only forward-declared until S4..S7 define them), `types.hpp` / `types.cpp` / `goods_table.cpp` (registry, `Builder`, class table, name tables, goods table), `synthetic.hpp` / `synthetic.cpp`, `state_fields.hpp` (fields for the new types; `StateIO`, the friend door of `World`; the header scope reads and validates), `world.hpp` (members `settings_`, `config_`, `rules_`, `seams_`, `types_` with const accessors; `clock` and `rng` stay public until S8; the canary binds seven members), `sim.cpp` (the two constructors), `production.hpp` / `production.cpp` (`ware_names`, the WARE / ROHSTOFF name list the type loader shares with `production_type`: the first `kWareRecordNames` = 2 names, as the original reads exactly two); tests `config_test.cpp`, `types_test.cpp`, `tests/world_access.hpp` (`WorldTestAccess`, the test-only friend, which therefore exists from S3, not S5). **Stream version 101**: the `header` scope gained SimConfig, Rules, the registry digest and the seam names (a flat list: `city_pass` first, then the other 14 seams in declaration order, count + string each) and the scope `settings` was added; the goldens `g0_*` were re-baselined (values from an independent Python transcription, `goldens.txt`), `sim_test.cpp` follows the new layout (offsets are found from the end of the stream, the header is rebuilt from `WriteArchive` bytes). Choices where the spec left room:
- **Reading the header validates it** (`StateIO::world`): after SimConfig and Rules are read, `SimConfig::validate`, `Rules::validate` and `validate_seams` run before the stream goes on; the registry digest and the seam names read are compared with the world's own (a `World` is given its registry and seams, they are not in the stream), so a stream made with other type data or other seams is refused with an error naming the header. `World::create` / `load` (S5) are the callers of the same three checks.
- **`SimConfig::list_size_bound` is not added** (the S2 text planned it): a bound that lives in the header it protects against could be set by a hostile header. `kListSizeBound` stays a build-time constant with the path "change the constant" (`config.hpp`, §3.1, F23).
- `Seams` has a default `city_pass` of `kOriginalCityPassCount` (8) null seams so that `Seams{}` is valid for the default config; `Seams::sized_for(cfg)` builds one for another `city_pass_count`. `validate_seams` also reports a null name on an installed function by its field (`Seams.city_is_empty: ...`, `Seams.city_pass[5]: ...`).
- **Registry**: `RuntimeClass` gained the sentinel `Count_`; `BuildingType::cost_goods` holds only the non-zero costs, in ascending goods order (0x29 WERKZEUG = `HEAD[5]`, 0x2a HOLZ = `HEAD[4]`, 0x2b ZIEGEL = `HEAD[3]`, 0x2c MAMOR = `HEAD[6]`, each `<< 8`, D 0x1000a5a0); `head_raw` is the whole HEAD record as the original's 56-byte buffer sees it (shorter zero-filled, longer cut), not only the bytes without a consumer, so the digest covers every byte; `running_cost` is `{0, 0}` for a type without PROD3; an unknown PRODTYPE or FORSCHTYPE name, an unknown goods name, a duplicate id, a missing HEAD record and a service mask wider than `SimConfig::service_bits` are load errors that name the entry (the original's name scans end in 0 = UNUSED for a name they do not know; a typo in a mod would silently become a non-building, so the rebuild refuses it: **F25**, tagged fix, no switch); the S3 review round added the registry checks of §4.4 (F25), the HEAD cut and PROD3 rule (F26) and the bounds of F27. `find_growing(object)` is the type with that entry id whose production kind is a growing item (ROHSTWACHS / ROHSTDUERR). The class table is `original_class_table()`; `class_for_prodtype(p, layout)` returns nullopt for 0 and for numbers the layout gives no list (above 192 in the original) and `Plain` for the unnamed numbers inside the blocks (Appendix C). The `gate_period` column carries the compare constants of the class ticks (500 for the production classes, the market classes, Doctor, FireBrigade, Cannonworks, PublicService, School, Hunter, Whaler, Shipyard; 2500 houses; 5000 stalls and the inn; 166 towers and cannon towers; 0 for Quarry and Castle, which have no stamp gate).
- **S3 review round (2026-10-19)**: two reviewers' findings were all tagged **fix** (no decision-needed item): the table bounds, drought widths, `neutral_owner`, `WorldSettings::validate()` and the failed-read rule (F27, §3.1); the registry cross-checks and `validate_against` (F25, §4.4); `RegistryRef` (a moved-from `World` keeps its registry); the `"default"` seam name; the field-count `static_assert`s next to every `fields()` of the config types in `state_fields.hpp` (the library fails to compile when a member is added without a field line); `class_for_prodtype` takes the layout; the six gate periods read from the class ticks (Appendix C), which moved the registry digest (`0x41581ac7f44d81eb` for the empty registry, `0xf7a619a85e19e062` for the 921 entries) and the `g0_*` goldens (values from the independent transcription `tools/sim_stream.py`, which reproduces the previous values when the six periods are 0); WARE/ROHSTOFF cap at two names; `head_bytes` guards the rubble byte with non-zero neighbours; the seam validation test is table-driven over all 22 seams. New cases: "registry refuses what it cannot run", "ware names after the second are ignored as in the original", "class for prodtype follows the layout", "every installed seam needs a name of its own", "settings validate rejects each bad field", "a moved-from world keeps its registry", and sub-cases of the header-read test (config the registry does not fit, settings out of range, a failed read leaves the target unchanged). Formatted with clang-format 23.1.3. Verified for the review round with MSVC `/W4 /WX` (the whole tree), 154 cases with and without `ANNO1503_GAME_DIR`; GCC, Clang and the sanitizers were not run on this host (the code is written to the CI warning set: explicit casts, no implicit narrowing, no unused parameters).
- **Facts corrected on the way** (all **fix**, documentation only): the type flag word `HEAD[0x1c]` is a **u16** (`movw`, AnnoGame 0x100093bd), the HEAD byte offsets of fire-and-ships.md are now read from the instructions (`docs/re/production.md` §2.4), Anlagen.dat HEAD records are 36, 44, 48, 52 or 56 bytes (not always 52), and goods 67 is spelled with the Latin-1 byte `0xC4` (Appendix A said `KRAEUTER`).
- **Tests** (38 new cases, 148 in all; the review round added 6 cases and many sub-cases: 154): the 14 named in the S3 list plus layout/rules/seams/settings/world-stream cases in `config_test.cpp`, and the entry-from-bytes cases of `types_test.cpp` (HEAD decoding with distinguishing neighbour bytes, PRODTYPE / FORSCHTYPE / WARE resolution, every error), so the loader is tested without game files; the game-file cases build all 921 entries and pin costs, research ids, the can-burn tally, the class tally and the registry digest (`0xf7a619a85e19e062`; `0xfa4b072578e3f587` before the gate periods of the review round, which this digest reproduces when the six periods are 0). The stream layout of the header and settings scopes is transcribed byte by byte in `config_test.cpp` ("state stream header and settings scopes"), the digest tables perturb every member (`field_count` ties each table to the struct: 51 members of SimConfig, 15 of Rules, 16 of WorldSettings, 20 of BuildingType, 18 of ProductionType), the validation tables have one `SUBCASE` per row and the accepted edge of every bound. Verification: MSVC `/W4 /WX`; GCC 13.3 (WSL) `-O2` and `-O3 -DNDEBUG` with `-Werror` and the CI warning set (library and tests), and `-O1` under ASan + UBSan without recovery (`-fno-sanitize-recover=all`), 148 cases with and without `ANNO1503_GAME_DIR`. GCC 13 at `-O3 -Wnull-dereference` reported one false positive, in the tests (growing a `vector<pair<..., std::function>>`: the `std::function` move constructor); the mutation tables of `config_test.cpp` therefore hold plain function pointers. The sim library itself was clean at every level. Not run: GCC 12, Clang, the CI jobs, macOS.

**S4. Players.** `player.hpp`, `player.cpp` (`player_table_tick`, `player_tick`; `upkeep_rem` is `u32`), `PlayersView` (declared in `tick.hpp`, §4.11: S4 creates the file with only this class, S7 completes it); `World::advance` runs step 4. Tests (`player_test.cpp`): the player cases of §14.2, with the visit counts derived from `min(d·32/50 + 1, 32)` ("player table visit counts for 5 10 20 ms and d 49"). Exit: bankruptcy and upkeep timeline reproducible.

**S5. Islands and tiles.** `island.hpp`, `island.cpp` (`Terrain`, `TileLayer` with a **default constructor, `reset(x0, y0, w, h, edge)` and a `u32` owner column**, chunk digest cache, sweep/ring/objmgr/drought state, `Island` shell with a default constructor, deposits, island-level query declarations), `tiles_io` with the `w·h·13 <= remaining()` check, `executor.hpp` (serial default; thread pool optional) for parallel chunk digests, `scenario.hpp` subset, `synthetic.cpp` island builders, `World::create` (players, islands), `World::save`/`load` frame (§9.5) for the state that exists, and `WorldTestAccess` (the test-only friend that every perturbation and invariant test of S5..S13 uses; it exists from here, not from S8). Tests (`island_state_test.cpp`, `tile_hash_test.cpp`): "tile setters mark chunks dirty", "chunk digest cached equals uncached", "tile hash independent of write order", "island hash parallel equals serial", "island perturbation moves the hash", "save load save identical with islands", "scenario validation names the path", "owner runs expand to tiles", "tile layer default constructed and reset", "owner column holds values above 65535", "load rejects tile dimensions beyond remaining bytes" (and `w·h` overflow, `chunk_edge` 0), "island rects outside the world or overlapping are rejected", "edge chunks of a non-multiple-of-8 island are clipped" (A14/F20), "world create refuses an invalid config", "load refuses a header whose config has a zero divisor before any container is sized" (§3.1), and the S2 container probe re-pointed at the real `TileLayer`/`Island`. Exit: islands can be created, saved, loaded and hashed on all platforms; `advance` still only moves the clock and ticks players.

**S6. Buildings, cities, creation, invariants.** `slot_list` use in `building.hpp`/`building.cpp`, `city.hpp`, `create.cpp` (`create_building`/`remove_building`, grids, `by_id`, derived counters), `invariants.cpp`, `reject.hpp`, point/rect lookup API, building and city scopes in `state_fields.hpp`; `World::create` places cities and buildings in file order (tick 0, no cost). Tests (`building_state_test.cpp`, `create_remove_test.cpp`, `world_state_test.cpp`): the create/remove cases of §14.2, "rect query canonical order and list masks", "perturbation moves the hash for every field", "field count canary per struct", "save load save with buildings", "check_invariants detects corrupted counters", "allocation pattern independence" (junk-allocation interleaving), **one perturbation test per structural check of §9.6** ("invariant payload alternative vs type", "invariant payload vector sizes vs type counts", "invariant config-sized container sizes", "invariant climate index", "invariant mine without outputs", "invariant island rect"; each also fed through `load`), "island and city query accessors equal the world members", the S2 container probe re-pointed at the real `Building`/`Payload`. Files also: `ClassInfo::payload` use in `invariants.cpp`. Exit: a scenario with buildings can be created, saved, loaded, hashed; `advance` ticks nothing new yet.

**S7. Vertical slice: workshop and farm through `advance`.** `tick.hpp` (`TickContext` with `IslandsView`, `Effects` including `GoodsMove`/`Reserve`/`Deliver`, `SerialWorld`, `SimOutput`), `seams.hpp` with `ctx` on every seam, `scheduler.cpp` (`advance`, `island_tick`, city ring, city tick, building scheduler, object-manager shell, 300 s event with `start_drought`, effect merge), `driver.cpp` (`tick_building` for Workshop and Farm; other ported kinds temporarily behave as `None` via a documented mask removed in S11; `consume`), `figure.hpp` (registry create only), `DrawSource` implementations and telemetry. Tests (`scheduler_test.cpp`, `slice_test.cpp`): subsystem call order with recording seams, island-tick order, zero-chunk island still runs the ring (A1), city-ring counts, building-scheduler order and shared stamp, stalls never visited, holes visited but not ticked, n = 40 000, G1 and its semantic asserts, spawn merge in island order, "effects equal synchronous", 300 s event draw rules (the drought scans use `neutral_owner`, `city_slots`, `roles.market_list`, `chunk_edge`), "recording seams carry state through ctx", "tick_building with a mismatched payload returns without throwing" (corrupt copy through `WorldTestAccess`; release-mode behaviour), "object manager stand-in on empty deposits", "advance zero ms runs subsystems", "step size is not invariant". Exit: weaver and farm produce at the F2/F6 rates through the real scheduler; G1 equal on all platforms.

**S8. Commands I: codec, apply for SetSpeed and SetProductionLine, privatisation.** `command.hpp`, `codec.cpp`, `apply.cpp`; the last public `World` members (`clock`, `rng`) become private with const accessors (`WorldTestAccess` already exists since S5); `Seams::ai` applies commands; `sim_test.cpp:193` migrated. Tests (`command_codec_test.cpp`, `command_apply_test.cpp`): codec round trips and fuzz, "decode rejects length and count lies", "legacy raw never carries a native tag", "set production line changes output", "rejected command leaves the hash unchanged", "issuer not owner rejected", "unknown ids rejected", "set speed host only", "line not found". Exit: S1 can be driven only through `apply` and `advance`.

**S9. Replay and session.** `replay.hpp`/`replay.cpp` (`ReplayLog`, `Recorder`, `run_turn`, `run_frame`, divergence finder), `PrintArchive`, `src/tools/sim_replay/` (`run`, `hashes`, `first-diff`, `tree`, `golden`) with its CMake entry. Tests (`replay_test.cpp`): "recorder replay reproduces checkpoints", "write_log read_log round trip", "first divergent entry", "diff_trees names the corrupted building", "replay refuses a config mismatch", "run_turn applies commands before steps", "log header records the prng timeline". Exit: S1 driven only through `Recorder`; replay reproduces every checkpoint.

**S10. Build, Demolish, trade commands.** `apply.cpp` (Build, Demolish, SetTradeFlag, SetTradeSlot), `placement.cpp` (stand-in rule), create with charge, `legacy_codec.cpp`, `fixtures/synthetic/sim/g2.log`. Tests (`command_build_test.cpp`, `property_test.cpp`): "build charges gold and goods", "build drag spends in stream order", "build atomic rule", "build footprint owner rules", "build occupied", "build unknown type and rotation", "build flags are not on the wire", "free build cheat rule", "demolish frees the slot and keeps the hint", "demolish skips unowned tiles", "city removal default never", "trade flag modes 0 1 9", "trade slot index", "trade limit and list bounds", "random command streams keep invariants", "G2 replay equals golden", "legacy 0x64 decode on synthetic bytes". Exit: G2 log committed; free-slot reuse after demolish matches the hand-derived order.

**S11. Remaining production classes.** `driver.cpp` (WeaponsWorkshop with research input, FurTrapper, SmallFarm with `DrawRef`, Mine with deposits), mine area, remove the temporary mask. Tests (`driver_classes_test.cpp`): each class through the scheduler equals the direct step-function tests at the same ticks, "weapons workshop unlocked from research", "mine deposit by index and depletion only in the add-on", "small farm draws once per step through the world", "fur trapper sends nothing without prey", "weapons workshop save load advance" (`unlocked` empty after load, sized by the first tick, hash equal to the live run), G3. Exit: every step function of `production.hpp` is reachable from `advance`.

**S12. World-level time.** `events.cpp` (queue, dispatch, seams), `timers.cpp`, monopoly, event 7.5 min, `figures_tick` with the slot policy and phases B/C/D. Tests (`world_events_test.cpp`): the world-level cases of §14.2 (FIFO, saved-next in `advance`, kind-1 events saved, timer clamp, monopoly once and the rule, event 7.5 min draws, accumulator discard vs keep, zero islands, registry count without `+1`, expiry, interim return, counter release), "figure phase goods effects merge after each call (C2)", "AI seam commands applied in advance and not logged", G4. Exit: every subsystem of §5.3 exists, even where its body is a seam default.

**S13. Island sweeps, growth, drought machine.** `island_tick.cpp` (`grow_chunk`, cursor wrap, drought machine). Tests (`sweeps_test.cpp`): "growth sweep visits chunks per sweep_chunks", "grow writes through the setters", "cursor wrap reading A3", "drought centre is the first market building", "drought machine timeline" (also with `drought_steps`/`drought_phases`/`chunk_edge` modded), "drought gate catches up", "drought tiles seam called per strip", "start_drought_at is the constructor the scripted door will use" (O-51), "edge chunks are clipped" (A14/F20), G5. Exit: growth tiles advance via the scheduler.

**S14. Scenario adapter and corpus.** `scenario_formats.hpp`/`.cpp`, parallel content build. Tests (`scenario_test.cpp`, skipped without files): the §14.5 corpus cases, including the count of non-multiple-of-8 islands (A14). Exit: all 50 scenarios create and run under UBSan; unknown parts are reported in `warnings`, not guessed.

**S15. Hardening and acceptance.** `src/tools/bench_sim/`, `fixtures/diff` exporter, lint completed, **the state stream frozen: `kStateHashVersion = 2` and every golden re-baselined in that one commit (§14.3)**, 1-hour determinism acceptance run (PLAN M3), `docs/PLAN.md` and `docs/perf/README.md` status, `docs/open-questions.md` entries for O-1..O-52 (the decisions D1..D18 are already filed as OQ-036..OQ-053). Tests: "one hour run is deterministic across two runs" (long, opt-in), 24 h soak under ASan (nightly), cross-platform golden matrix, "state hash version is frozen at 2". Exit: PLAN M3 acceptance.

**S16 (optional, benchmark-gated). Executor.** `executor.hpp`/`executor.cpp` (island ticks, `may_draw` registry, `GuardedDraw`, `faulted`), and **only if the owner chooses `PrngMode::PerIsland` (D11)**: `StreamDraw`, a `WorldRng stream` cursor in `Island` (field list, hash scope `stream`, save entry), `u64 stream_seed` in the `header` scope, `Rules::validate()` accepting the value, a `kStateHashVersion` bump (version 3 or higher after the freeze) and re-baselined goldens, all in one commit. Until S16 the value is design-only and rejected. Tests (`executor_test.cpp`): "thread count equality 1 2 8", "may_draw soundness for every site", "guarded violation sets faulted", and, with the mode: "per-island stream golden", "per-island stream cursor survives save and load", "per-island stream cursor is hashed" (a perturbation of the cursor moves the hash), "validate accepts PerIsland only with a stream seed". Exit: only if `bench_sim` shows the island phase above budget.

## 16. Documentation corrections owed (from the four RE reports; applied by the stage that touches the topic)

**Applied on 2026-10-18 (revision 2), where the old text was plainly wrong:** the executed turn record is `{u16 type, u16 len, payload}` with no sender (`docs/re/tick-loop.md` turn packet); the world vtable is `0x100ccdd8` with Advance at slot `+0x84`, `0x100ccd74` being the unit registry (`docs/re/production.md` §1, open list); the figure factory receiver is **world** slot `+0xa0` (`docs/re/production.md` §5.1); the small-farm animal kind is a world PRNG draw `& 7`, not the climate (`docs/re/production.md` §4.5, `docs/sim/production.md`); OQ-035 (a) is the `ANNOADDON1` edition flag (`docs/re/production.md` §4.4, `docs/sim/production.md`, `docs/open-questions.md`); OQ-033/OQ-034 are resolved (`docs/sim/production.md`, `docs/open-questions.md`). The remaining rows below are still owed and are applied by the stage that touches the topic. `docs/open-questions.md` now also holds the decisions of §13 as OQ-036..OQ-053 and the open research questions of the RE reports as OQ-054.

| File | Correction |
|---|---|
| `docs/re/production.md` §3 | `0x10024010` is `BuildingIndex::Create(rec, slot, now, &out)` with `this = city+0x144`, not a stateless factory; replace the 6-branch description with Appendix C; signature and the post-construction order of §7.3 |
| `docs/re/production.md` §1, l.15 | list ↔ group map confirmed for all branches (Appendix C / `ListLayout`); scheduling order and formula; the sixth list `+0x68` is never visited; list object layout (hint `+0`, probe counter `+4`, live count `+8`, vector `+0x0c`, scheduler cursor `+0x1c`); slot policy of §5.2 |
| `docs/re/production.md` l.91, 121, §2.4–2.5 | common fields: `+0x04` list slot, `+0x0a` initial value from the placement tick (F1) and the same value in the class status-time field, `+0x0e`, `+0x0f` (= `rec[8]`), bits 6–7 uninitialised; `research id` is a byte at type `+0x24` filled from FORSCHTYPE (not from HEAD/PROD3); HEAD byte table (footprint, cost mapping, max HP `[0x2c]`, can-burn `[0x31]`, radius `[0x11]`, rubble `[0x0a]`); market search radius = HEAD `[0x11]` via type slot `+0x78`, cart rectangle `x0 = x + (w'−1)/2 − r`, width `2r + 1 + ((w'−1) mod 2)`. **The HEAD byte table (footprint, costs, gold, rubble, radius, flag word u16, max HP, can-burn) was applied in S3: `docs/re/production.md` §2.4, `docs/formats/anlagen.md`.** |
| `docs/re/production.md` l.11–13, 125; §4.1a; §7 | `vtable 0x100ccd74 slot +0xe8` is stale: World vtable 0x100ccdd8, Advance = slot `+0x84` = 0x100ae040; 0x100ccd74 is the unit registry; island tick order; OQ-033 (owner slot `+0xc4(3)` = Volk) and OQ-034 (`b` = goods Forsch research id, bit 1 of the research state byte) resolved; goods ids 7, 8, 24, 25, 40, 45, 54, 55 unnamed, 79 = KLINIK |
| `docs/re/population.md` l.68, 240 | the house is created by `Create`, which also increments `index[+0x46+2g]`; `+0x0a` starts at the placement tick in SP and 0 only in MP and after load; "index in the house list" is not the creation order after removals |
| `docs/re/tick-loop.md` §2, l.25–28, 87, 92, 135 | sub-system identities: `+0x114` = unit registry (tick 0x1009a8a0), `+0x118` = player table (real vtable 0x100cb858, tick 0x1009c4d0, runs the AI every advance), `+0x69c8` = SKTIMER (0x1009cd30); `0x100ae140`: accumulator reset, prop 6 bit 0x80, draw 1 `% 10`, add-on only draw 2, effect = message event 0x15a at the island centre; `0x1009d2a0` = MONOPOL tracker, no draws, runs once; timed-event queue spec (node layout, FIFO among equal due, kinds 0 and 1 only, enqueue/cancel slots `+0xc4/+0xc8`, kind 1 dropped on load); the 15000-tick sweep is slot `+0x194` = 0x10093480 in class 0x100cac2c (0x1008cac0 is the base stub) |
| `docs/re/tick-loop.md` §5, l.86–92 | the executed record is `{u16 type, u16 len, payload}`: **no sender**; the server accepts `0x64..0xc7` from any client (0x100553e0); SP calls the `tNetClient` slots directly and the MP handlers call the same apply functions; the five placement types are apply ops 6/2/8/7/9 and only `0x64` is the gameplay build; full command table; `0x76/0x7d/0x7e` have no handler, `0x91–0x93` are `ret 0xc` stubs, `0x98` is sent but outside the table; world property 4 = tick, copied into `[client+0x10]` ("now") every step in SP and never in MP |
| `docs/re/tick-loop.md` l.87 | `0x1005c7f0` is a tiny float helper; the real placement entry is city vtable slot `+0xac` = `PlaceBuilding` 0x100279a0 reached through apply 0x100589a0 |
| `docs/re/annowelt.md` | add object offsets: city `+0x1c` island, `+0x20` owner, `+4` number, `+0x144` BuildingIndex (vtable 0x100c2e48), `+0x2ec` resident counter; island `+4/+6` origin, `+8/+0xa` width/height, `+0xc` 8-byte tile array (u16 object id; owner nibble `(u32@+4)>>25 & 0xf`), `+0x1c` definitions; world `+0xec/+0xf0` = width/height (not `+0xe8`), `+0x130` island index grid (8×8 blocks, stride width/8), `+4` MONOPOL table, `+0x6a94/+0x6a98` event queue; lookups `world slot +0x20(0x1300, i)`, `island slot +0xf8(n)` with no bounds check; city slots `+0xa0` Build, `+0xac` Construct, `+0xb4` RemoveAt, `+0x8c` Kontor setting |
| `docs/re/fire-and-ships.md` l.24–25, 31, 46, 80 | queue handlers 0x100adae0/0x100adb80; island object manager is `[island+0x28]` (slot `+0x20`), `[island+0x30]` the message manager, `[island+0x34]` not ticked; HEAD offsets 0x2c / 0x31 / 0x0a are confirmed; city slot `+0xb4` is `RemoveAt(x, y, who)`; the house list contains null holes, so "stop after 64" must skip holes |
| `docs/sim/production.md` | small farm: draw, not climate (§7.4); `StatusTracker::shows_icons` = owner Volk == 1; `WeaponsWorkshop::unlocked` from the owner's research state |
| `docs/sim/core.md` | hash section: v2 hierarchical, `core_hash` keeps v1; advance order and what runs (this file); step-size section unchanged |
| `docs/re/annoscene.md` §5, `docs/formats/sze.md` (OQ-027), `docs/formats/anlagen.md` | scene HEAD byte map (§10); HEAD record byte table, FORSCHTYPE/PRODTYPE/WARE/ROHSTOFF meanings, footprint swap on odd rotation (AnnoWelt 0x100279e6, 0x10071c80) |
| `docs/open-questions.md` | OQ-033 and OQ-034 resolved (§4.5, Appendix A); OQ-035 consistent (depletion = world property 14 bit 1); decisions D1..D18 filed as OQ-036..OQ-053 and the RE research list as OQ-054 (done 2026-10-18); add O-1..O-52 of §12 in S15 |
| `docs/PLAN.md` M3 | the M3 text is stale (`crates/sim`, Rust attributes, "now + delay", "10–20 Hz"); "tile owner must equal the city number or 15" holds for `0x64` only, `0x65/0x66/0x67/0x69/0x87` are editor ops; "single player also queues commands at a step boundary" is a port design decision, not original behaviour |

## Appendix A. Goods numbers (D: AnnoGame 0x10013be0; ranges → name tables 0x100251f8, 0x10025234, 0x10025178, 0x100251e4, 0x100251d0; the name scan covers ids 0..0x8e)
1 EISENERZ 2 GOLD 3 SALZSTEIN 4 EDELSTEINE 5 STEINE 6 MAMORSTEIN · 9 WOLLE 10 ZUCKER 11 TABAK 12 FLEISCH 13 KORN 14 MEHL 15 EISEN 16 FLACHS 17 HOPFEN 18 TIERHAEUTE 19 SEILE 20 SEIDE 21 FARBSTOFFE 22 WALSPECK 23 HOLZKOHLE · 26 NAHRUNG 27 LEDER 28 SALZ 29 ALKOHOL 30 TABAKWAREN 31 GEWUERZE 32 HEILKRAEUTER 33 STOFFE 34 SEIDESTOFFE 35 LAMPENOEL 36 KLEIDUNG 37 SCHMUCK 38 PELZE 39 WEIN · 41 WERKZEUG 42 HOLZ 43 ZIEGEL 44 MAMOR · 46 SCHWERT 47 BOGEN 48 ARMBRUST 49 MUSKETE 50 RUESTUNG 51 LANZE 52 AXT 53 KANONESHIP · 56 GETREIDE 57 TABAKBAUM 58 GEWUERZBAUM 59 FLACHSBAUM 60 ZUCKERROHR 61 BAUMWOLLE 62 WEINTRAUBEN 63 KAKAOBAUM 64 HOPFENBAUM 65 SEIDENRAUPE 66 FARBBAUM 67 KRÄUTER (the Latin-1 byte `0xC4` in AnnoGame.dll and Anlagen.dat; this table and the first revision wrote it `KRAEUTER`, which no file contains; `goods_table.cpp` spells it `"KR\xC4UTER"`) 68 KARTOFFEL 69 GRAS 70 BAUM 71 FISCH 72 EISFISCH · 76 WIRTSHAUS 77 BADEHAUS 78 THEATER 79 KLINIK 80 SCHULE 81 HOCHSCHULE 82 KAPELLE 83 KIRCHE 84 KATHETRALE 85 PARK 86 BIBLIOTHEK 87 FEUERWEHR 88 GERICHT 89 GALGEN · 96 SPIESSTRAEGER … 117 WELTBEHERRSCHER (units) · 122–129 ships · 132 SCHATZ · 137 FLEISCHTIER 138 PELZTIER 139 WALFISCH · 142 ALL. Unnamed ids inside the covered ranges: 7, 8, 24, 25, 40, 45, 54, 55, 73, 74, 75, 90, 118–121, 130, 131, 133–136, 140, 141 (and 0; the name scan matches `UNUSED` to 0). Warehouse goods are ids 0..0x37 (city record 12 bytes at `+0x334+12g`; `add` refuses g > 0x37): 55 usable slots of which 47 are named; crops 0x38–0x4b and animals 0x89–0x8d are not warehouse goods. Public-need ids 76–89 double as bit positions of the coverage mask (bit = id − 0x4c; HOCHSCHULE 0x30, KIRCHE 0xc0, KATHETRALE 0x1c0 combine). Mine/quarry outputs (WARE[0]): MINE 1203 SALZSTEIN; 1205, 1210, 1216, 1217 EISENERZ; 1206, 1212 GOLD; 1214 EDELSTEINE; STEINBRUCH 1201, 1208 STEINE; 1218, 1220 MAMORSTEIN; EISLOCH 3506 EISFISCH. Goods with a research requirement (BGruppen `[WAREN]` Forsch): SCHWERT 1, BOGEN 4, ARMBRUST 9, MUSKETE 12, RUESTUNG 1, LANZE 3, AXT 6, KANONESHIP 38; all others 0.

## Appendix B. Research ids (D: AnnoGame 0x10013cf0, table 0x100250f8; the FORSCHTYPE record of Anlagen.dat is matched by name; `UNUSED` → 0)
1 WAFFEN_FERTIGUNG_01 2 KATAPULT 3 WAFFEN_FERTIGUNG_02 4 BOGNER 5 BELAGERUNGSMASCHINE 6 KAVALLERIE 7 GEHAERTETE_SPITZEN 8 GEFIEDERTER_SCHAFT 9 ARMBRUST 10 ARMBRUST_REICHWEITE 11 BRANDPFEILE 12 MUSKETE 13 RADSCHLOSS · 17–20 RUESTUNG_INFAN_01, RUESTUNG_KAVAL_01, RUESTUNG_INFAN_02, RUESTUNG_KAVAL_02 · 21 KANONE 22 KANONE_GENORMT_KALIBER 23 KANONE_GENORMT_SPRENG 24 KANONE_MOERSER 25 KANONE_MOERSER_PULVER 26 KANONE_MOERSER_BRAND · 33 MITTLERES_HANDEL 34 MITTLERES_KRIEG 35 GROSSES_HANDEL 36 GROSSES_KRIEG 37 VERSTAERKTER_RUMPF 38 KANONE_SHIP · 49 BRUNNEN 50 GR_WEBER 51 FEUERWEHR 52 TIEFER_BRUNNEN 53 STEIN_BRUECKE 54 MEDICUS 55 BIBLIOTHEK 56 SCHNELLE_HEILUNG 57 TIEFE_ERZMINE 58 GERICHT. Entries using one: 744, 745, 823, 1337 MEDICUS; 1005 GR_WEBER; 1006 BOGNER; 1010 KATAPULT; 1012 KANONE_SHIP; 1036 WAFFEN_FERTIGUNG_02; 1037 WAFFEN_FERTIGUNG_01; 1038 KANONE; 1216, 1217 TIEFE_ERZMINE; 1339 GERICHT; 1341 BRUNNEN; 1342 TIEFER_BRUNNEN; 1350 FEUERWEHR; 1378 BIBLIOTHEK; 2516, 2517 STEIN_BRUECKE. The only consumer found: the mine's `deep = (research id == 0x39)` for iron mines.

## Appendix C. PRODTYPE → class, tick, list (D: factory 0x10024010, byte table 0x1002528c (161 entries), jump table 0x1002521c; second switch table 0x10025348/0x10025330)
Names → numbers: AnnoGame 0x100138e0 maps 32-number blocks onto name arrays (0x1002444c, 0x1002454c, 0x100245cc, 0x1002464c, 0x100243cc); the Anlagen loader scans index 0..0x1a0 and stores the first match as `u16 type+0x10`, `UNUSED` = 0. Every PRODTYPE string of the 921 entries resolves (5 entries are UNUSED: 101, 102, 103, 1003, 1007). Other blocks: 193 STRASSE … 201 ZAUN; 225 ROHSTOFF 226 ROHSTWACHS 227 ROHSTDUERR; 257 MAUER 258 MAUERUP 259 TOR; 289 DENKMAL 290 TRIUMPH 291 ZIERDE; 321 FELSEN … 325 TIERHOEHLE; 353 FLUSS 354 MEER 355 WASSERFALL; 385 RUINE (not buildings).

| PRODTYPE | RuntimeClass | TickKind | ListId | Original class (ctor / size / tick) |
|---|---|---|---|---|
| 1 HANDWERK | Workshop | Workshop | 4 | 0x100688f0 / 0x30 / 0x100680b0 |
| 2 PLANTAGE, 4 WEIDETIER, 8 STEINMETZ, 10 SCHAFFARM, 6 FISCHER | Farm | Farm | 4 | 0x1006f160 / 0x38 / 0x1006ea90 (FISCHER: vtable overwritten 0x100c2ac4, same tick) |
| 3 MINE | Mine | Mine | 4 | 0x1006e460 / 0x28 / 0x1006def0 |
| 5 JAGDHAUS | Hunter | None | 4 | 0x1006a9c0 / 0x30 / 0x10069f40 (unread) |
| 7 WERFT | Shipyard | None | 4 | 0x10075470 / 0x34 / 0x10074e90 (unread) |
| 9 UNIVERSAL | SmallFarm | SmallFarm | 4 | 0x1006ff80 / 0x48 / 0x1006f660 |
| 11 KANONENBAU, 130 MILITAR | Cannonworks | None | 4, 2 | 0x1006dc50 / 0x8c / 0x1006cc40 (unread) |
| 12 HANDWERKDUAL | DualWorkshop | None | 4 | 0x100693c0 / 0x38 / 0x10068a70 (unread; no entry uses it) |
| 13 STEINBRUCH, 14 EISLOCH | Quarry | None | 4 | base ctor, vtable 0x100c2614, stub tick 0x10001030 |
| 15 WALFAENGER | Whaler | None | 4 | 0x100746a0 / 0x54 / 0x10073720 (unread) |
| 16 PELZJAEGER | FurTrapper | FurTrapper | 4 | 0x1006f160 + vtable 0x100c2bf0 / 0x38 / 0x1006e5d0 |
| 17 HANDWERKWAFFEN | WeaponsWorkshop | WeaponsWorkshop | 4 | 0x100693c0 + vtable 0x100c2d1c / 0x38 / 0x10069550 |
| 18–32 (unnamed), 37–64, 102–128, 132–160, 162–192, and 65 BRUNNEN, 75 GALGEN, 76 GERICHT, 80–96 | Plain | None | by block | default class 0x100734c0, size 0x18, vtable 0x100c92b8, tick 0x10001030 (`ret 4`) |
| 33 HAUPT, 34 MARKT, 35 KONTOR | Market | None | 5 | 0x1006c080 / 0x50 / 0x1006b6d0 (unread) |
| 36 MARKTKOMBI | MarketCombi | None | 5 | 0x1006c080 + vtable 0x100c2740 / 0x1006c2e0 (unread) |
| 66 WIRT | Inn | None | 1 | 0x10075970 / 0x24 / 0x100755f0 |
| 67 KAPELLE, 68 KIRCHE, 69 BADEHAUS, 70 THEATER, 78 BIBLIOTHEK, 79 PAVILLION | PublicService | None | 1 | 0x1006b550 / 0x18 / 0x1006b4e0 |
| 71 ARZT | Doctor | None | 1 | base ctor, vtable 0x100c2998 / 0x20 / 0x100676b0 |
| 72 SCHULE, 73 HOCHSCHULE | School | None | 1 | 0x10070160 / 0x18 / 0x1006b4e0 |
| 74 SCHLOSS | Castle | None | 1 | 0x1006f1b0 (farm ctor) / 0x3c / 0x1006f1d0 |
| 77 FEUERWEHR | FireBrigade | None | 1 | base ctor, vtable 0x100c286c / 0x1c / 0x10067b50 |
| 97 PIONIER, 98 SIEDLER, 99 BUERGER, 100 KAUFLEUTE, 101 ARISTOKRATEN | House | None | 3 | 0x10077220 (house base 0x10075c60, byte `+0x0e` = 0x82) / 0x50 / 0x10076230 |
| 129 WACHTURM | Tower | None | 2 | 0x10071380 / 0x30 / 0x10070c50 |
| 131 KANONENTURM | CannonTower | None | 2 | 0x1006ac80 / 0x24 / 0x1006ab50 |
| 161 MARKTSTAND | MarketStall | None | 0 (never scheduled) | 0x10070730 / 0x20 / 0x100702d0 (only `if (u16)(t-last) >= 5000 then last += 5000`) |
| 0 UNUSED, ≥ 193 | not a building | | none | `Place` skips `Create` when the block start `(p - 1) & ~0x1f` is above 0xa0, i.e. for p >= 193 (0x10027b32-0x10027b3c; roads, walls, scenery are tile objects only) |

Tick gate periods (**D**: the compare constant is read in every class tick; what the body does with `last` after the gate is not read except for MARKTSTAND and the ported production classes; `ClassInfo::gate_period`, hashed in the registry digest): 500 in workshop 0x100680b0, farm/fisher 0x1006ea90, trapper 0x1006e5d0, mine 0x1006def0, 0x1006f660, class 12 0x10068a70, class 17 0x10069550, market classes 0x1006b6d0/0x1006c2e0, 0x100676b0, 0x10067b50, 0x1006cc40, **PublicService and School 0x1006b4e0 (`cmp dx, 0x1f4` at 0x1006b4ec, shared tick; 13 instructions, the MARKTSTAND rule: add the period to `last` once the u16 difference reaches it), Hunter 0x10069f40 (0x10069f5d), Whaler 0x10073720 (0x10073737), Shipyard 0x10074e90 (0x10074ea1)**; 2500 houses 0x10076230; 5000 0x100702d0 and 0x10075608; 166 towers 0x10070c50 (0x10070c62) and **cannon towers 0x1006ab50 (0x1006ab6a, `0xa6`)**. Castle 0x1006f1d0 has no stamp gate (a byte counter at +0x38) and the Quarry class is the stub 0x10001030: both are 0. (Before the S3 review round the periods of PublicService, School, Hunter, Whaler, Shipyard and CannonTower were 0, "unread"; the registry digests and the goldens were re-baselined, `goldens.txt`.)
