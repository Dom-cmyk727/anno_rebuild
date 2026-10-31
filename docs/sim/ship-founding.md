# Ship founding: proposed headless contract

**Status: PROPOSED. Governing document of the founding work; revised in the fifth pass (research `docs/re/ship-founding.md` §12 corrects §11.3's tile-overwrite rule and adds the selected-ship predicate). Where this text and an older section of the research note differ, this text and `re` §12 govern. The command (`FoundCmd`) is NOT implemented. Isolated helpers (height-grid view, site predicates, territory ownership) are implemented in separate commits, as their own units, and are not founding authorization. Where this text says "needs an interface" it describes the baseline `62ddaf1`.** This document specifies a command by which a player's ship founds that player's first warehouse. It changes no command, format, version or golden. The evidence is `docs/re/ship-founding.md` (§8, §10, §11: addresses, hashes, tools, verification); raw material is in `cache/map2-founding-evidence/` (git-ignored). Nothing here has been observed in the running original (`NOT_RUN_ORIGINAL`); "ORIGINAL" means read from the shipped binaries and files, and **E** means the original instructions were also executed on synthetic memory in an emulator and compared with a model.

Labels on each substantive rule: **ORIGINAL**, **REBUILD** (existing code or accepted spec), **ADAPTATION** (accepted rebuild policy), **PROPOSAL** (needs review), **UNKNOWN**. Where the original rule is read and understood the contract states it; where it is not, it says UNKNOWN and §11 lists the prerequisite. There is no stand-in value for an unread rule.

## 1. Scope and exclusions
- A first warehouse founded from a ship: payment, city, building, territory, obstruction, nearby-figure rejection and the effects on deterministic state. Headless.
- Excluded: sailing and routes (`docs/re/ship-navigation.md`), rendering and UI, AI, the scout's market (`View.dll 0x10047ae0`, building 1301), takeover of another player's warehouse, further warehouses of a player on an island it already holds, production and economy, candidate selection, the active map. The offline checker's exterior-4 sea, 400-step bound and 256-tile proxy are not engine rules (`docs/sim/ship-start-integration.md`).
- ADAPTATION (accepted): founding either commits all its effects together or rejects without changing the world. ORIGINAL: **no rollback observed in the traced path**; the original takes gold and goods first and can then fail at the city ring (§5), so the adaptation is a deliberate difference.

## 2. Command, authority, entities
- PROPOSAL: `FoundCmd { FigureId ship; PlacementWire placement; i32 click_x; i32 click_y; }`, appended as the last alternative of `Body` (the codec's tag order is the replay format). Coordinates are `i32` like the rest of the widened model; the original's `u16` click storage (cursor `+0x40/+0x42`) is not a reason for a narrower field. The click is carried, the origin derived and any supplied origin validated against it (accepted direction C1).
- ADAPTATION (`state.md` §8.2 rule 3): `issuer` is stamped by the host. Authority is derived: the referenced figure must be a ship of the issuer. No payer, owner or city field is supplied by the client. ORIGINAL: no ownership check exists anywhere in the net path (`state.md` §8.1).
- ORIGINAL: the gold payer is the player object stored in the ship (`ship+0x144`, class code `0x1000` = player: `re` §11.6).

## 3. Geometry
- REBUILD: tiles are integers, figure positions Q24.8 (`FixedPos`). ORIGINAL: ship positions are f32 world coordinates (x at `ship+0x14`, z at `ship+0x1c`), tile units.
- ORIGINAL (`re` §8.2): for a click `(cx, cy)` and a footprint `(w, h)` (the type's bytes, swapped for odd rotation) the origin is `ox = max(0, cx - (w-1)/2)`, `oy = max(0, cy - (h-1)/2)` (integer division of a non-negative value, clamp after the subtraction, per axis).
- ORIGINAL (`re` §8.6): the rotation `r` carried by the cursor is the first of up to four trials `r, r+1, r+2, r+3 (mod 4)`; the origin is **not** recomputed per trial; a trial changes the footprint's parity; the first rotation that passes the ship predicate is written into the record. All failing: rejected. The validator's pre-check (island under the origin, upper bounds with the first trial's footprint, ownership of the origin tile, figure resolved, distance) rejects before any trial.
- ORIGINAL: only the accepted rotation reaches the building.

## 4. Execution-time preconditions
Everything is re-evaluated when the command runs (ADAPTATION, `state.md` §8.2); the original evaluates the checks in the client's cursor and the net executor does not repeat them. Isolated expectations are in §13.1; a founding needs all of them.

| # | Rule | Label |
|---|---|---|
| P1 | `issuer` exists; `ship` is a ship figure (figure-type class `SHIP`, BEWOHNTYPE 113) with `owner == issuer`. Classification is by the figure type's class from the registry, not by "has holds" | ORIGINAL class; PROPOSAL classification (needs figure classes in the registry, section 11 B); ADAPTATION authority |
| P1b | the selected ship's state word (`ship.slot+0x8c(0)`; the ship's `+0x40`) has bit `0x02` **clear** | ORIGINAL (`AnnoFrame.dll 0x10048d54`–`0x10048d5c`, `re` §12.2). The bit's meaning is unknown and is not named. This is a validation of the ship itself, separate from the nearby-figure filter (whose exclusion mask `0x0a` also contains this bit): a ship excluded from a proximity query is not thereby allowed to found. Regression: all other inputs valid, only this bit changed → rejected; bit clear → passes this predicate |
| P2 | `placement.type` known and non-zero; `rotation <= 3` | REBUILD (F4) |
| P3 | the type is one the ship-founding action offers, and its PRODTYPE is in 33..64 | ORIGINAL: the shared predicate tests PRODTYPE 33..64 (`((p-1)&~0x1f)+1 == 0x21`); the original menu offers only 1302 (`View.dll 0x10063ba0`). Which types the rebuild accepts: accepted direction C3 (data-driven, default 1302) |
| P4 | for goods `0x29..0x2d`: the sum of the amounts of the ship's holds of that goods is at least the type's cost; duplicates add. Units 8.8 | ORIGINAL |
| P5 | the issuer's gold is at least the type's gold cost (only when non-zero) | ORIGINAL |
| P6 | the island under the origin exists and is not locked (`LOCKED` record, island property `0x10`, `re` §11.5) | ORIGINAL |
| P7 | the rotation-adjusted footprint lies inside the island rectangle on both axes (lower and upper bounds; the validator's pre-check uses the first rotation) | ORIGINAL |
| P8 | every footprint tile has owner nibble 15 (neutral). A tile owned by any city is the takeover path: rejected here | ORIGINAL for the tile test; PROPOSAL for the explicit rejection of takeover |
| P9 | the issuer has no city on this island | ORIGINAL (`0x10044150`; list kind 2 = cities, `re` §10.5). Another player's city does **not** reject: two players may settle one island, neither is required to share |
| P10 | distance: `trunc(sqrt(dx*dx + dy*dy)) <= 12` with `dx = trunc_to_zero(click_x - ship_x)`, `dy = trunc_to_zero(click_y - ship_z)`; for integers `dx, dy` this is `dx*dx + dy*dy <= 168` | ORIGINAL (`re` §8.3), representation: accepted direction C2 |
| P11 | occupancy and terrain (below) | ORIGINAL; coast class ORIGINAL/E |
| P12 | no figure veto (below) | ORIGINAL; two inputs UNKNOWN (A1, A2) |
| P13 | the island has a free city-ring slot | ORIGINAL (a full ring makes the original fail after paying); ADAPTATION to check before committing |
| P14 | climate | **No climate test observed in the traced path** (`re` §11.3); not a rule of this contract. Residual: virtual callees not resolved (A5); runtime confirmation D1 |

**P11 occupancy and terrain** (types, not 1302 specifically; `re` §10.2–§10.4):
1. bounds (P7);
2. for every footprint tile with a non-zero object id: the placed type must accept the existing type: accepted if the existing type's flag word has bit `0x20` (in the shipped data: `ROHSTWACHS`, `ROHSTOFF`, `ROHSTDUERR`, `RUINE`, `STEIN`); else the same-PRODTYPE replacement row (existing slot `+0xd0` bit 8; takeover-related, not a founding case); else reject;
3. types with flag `0x10` need the 8×8-chunk coverage rule (`re` §10.2.5); 1302 does not;
4. height analysis and acceptance. For the coast class (`flags & 0x7000 == 0x2000`, flag word `0x2087`: 1302, 1304, 1306 and 27 further types) ORIGINAL/E: over the `(fw+1) x (fh+1)` vertex window starting at the origin with row stride `W+1`: `max & 0xf0 == 0x30`; the rotation's shore edge (0 top row, 1 left column, 2 bottom row, 3 right column) has all `n = w+1` vertices exactly `0x30`; at least `floor(n/2)` vertices of the opposite edge are `< 0x20`; here **`n = type width + 1` and `m = type height + 1` are the type's UNROTATED dimensions** (for rotation 1 or 3 the shore edge has `fh + 1 = type width + 1` vertices), and the one-wide/one-deep shortcut uses the same `n`, `m`; a footprint 1 tile wide or deep passes without the test. The result byte `max & 0xf0` becomes the record's byte 8. Other classes (road `0x4000`, mountain `0x1000`, plain, `0x3000`) are not founding types and are not decoded;
5. coast and road classes are accepted after step 4.

**P12 nearby figures** (`re` §11.4): the registry is queried for units of BEWOHNTYPE 97..128 (scout, soldiers, war machines, ships; **not** the donkey) in the footprint rectangle grown by 10 tiles, excluding units whose state word has bits `0x0a`. For each unit: if it is usable (not a war machine without crew), has an owner, and is hostile to the placing player and its owner's property `0x1c` is not `0x11`: veto when its f32 position is within its type's range (`LOGIC[0x25]/8`, 0 → 7; ships 16, scouts 7) of any of the four integer corners of the footprint. Otherwise: veto when its position lies in `[x0,x1)×[y0,y1)`, or, for ships, when its hull box (half-extent `HEAD[0x10]`, 1.55 for ship 1502, truncated) overlaps the rectangle. The founding ship and the player's own scout take the second branch; nothing excludes them. The original caps the query at 256 units; the rebuild does not inherit the cap (C6).

## 5. Payment
- ORIGINAL (`re` §10.7): amounts are the registry type's: gold `type+0x1c`, goods `byte << 8` for `0x29..0x2d`. Nothing is hard-coded to 1302.
- ORIGINAL order: gold; goods in ascending goods id, holds in ascending index, each hold giving `min(amount, remaining)`; then the city and the building (with the "already paid" flag, so `Place` does not charge again). The original checks by summing all holds of a goods and commits without re-checking. A hold taken to zero gets its goods id cleared to 0.
- ORIGINAL: gold is subtracted without a floor.
- ORIGINAL (D): the order of gold, goods, then the ring allocation means a full city ring loses the payment. ADAPTATION: P13 is checked first.
- REBUILD: since stream 117 (task MAP-2, package B) `ShipHold { goods, amount_88 }` is an exact u64 count of 1/256 ton, so original 8.8 holds are representable; at 116 it was whole tons. The original's hold amount is a u16; the rebuild takes the 8.8 semantics and no inherited capacity limit.
- ADAPTATION: checks first, effects after, charged once.

## 6. Successful effects, in the original's order
1. **Payment** (§5).
2. **City ring slot** (ORIGINAL, `re` §11.3): number = the first free slot at or after `cursor + 1` (cursor starts at −1), wrapping over the island's 16 slots (a configurable slot count in the rebuild); the cursor becomes that number; `highest used` is raised. A city number equal to the neutral owner value is a hazard of the original with 16 slots (nibble 15); the rebuild keeps city numbers and the neutral owner distinct by construction (section 11 B, ring row).
3. **City registration** (ORIGINAL): the city is added to the player's kind-2 list and takes the player as owner; the city's property-0 mask gets the bit of each player whose relation of kind 1 to the placing player equals 3; the island's "players present" word (property `0xe`) gets the player's bit; island byte `+0xb0+number` = player property 9; the island's derived word `+0xaa` is recomputed.
4. **Name and PRNG** (ORIGINAL): if the world offers a city-name list, **one world PRNG draw** `mod` the list size selects the name (city property `0x12`); no list, no draw. This draw happens after the payment and before the building exists. In the direct-call closure it is the only PRNG call; unresolved virtual callees remain (A5).
5. **Territory** (ORIGINAL, `re` §11.2): two claims. First, for PRODTYPE 33..35, the disc claim with radius `R = type action radius` (24 for 1302), the city number, no clip, neutral tiles only; then the footprint rectangle (radius 0), neutral tiles only. Own and rival tiles are never overwritten; terrain does not matter; clipped to the island. The disc is the integer profile `T[k]` of `re` §11.2 around the footprint's centre (shape depends on the footprint only through its centre and parities); rows `cy-k` and `cy+py+k`, columns `cx - T[k]` to `cx + px + T[k]`. The reference port `claim_model.py` equals the original on 600 randomised cases.
6. **Object discovery** (ORIGINAL, structure D, semantics S): objects of the island's object container inside the disc get the player's bit in their property 0; the first discovery posts a discovery event; a wider reveal (radius `R+8`) updates island masks and objects. Modelling it is decision C8.
7. **Building** (ORIGINAL): created through `BuildingIndex::Create` (market class for PRODTYPE 33..35): position, rotation, stamp, city attach, list insertion; **`byte0f` = record byte 8 = the terrain result (`0x30` for the coast class)**; the Kontor counter: island property `0x11` (default 1) → `old+1` and the building's KONTID := `old`.
8. **Tiles** (ORIGINAL, corrected in `re` §12.1): the routine also runs a second pass over the whole footprint that writes the walk code and mask of every tile, including tiles the first pass skipped. Each footprint tile's object word becomes the placed type's id, with the footprint-offset bytes, the phase bits, the anchor bit (first tile written per world-aligned 8×8 block) and `rec[8]`. The warehouse has flag word `0x2087` (bit `0x10` clear), so **every footprint tile is overwritten unconditionally, including a `STEIN` object**. Only a **placed** type with flag `0x10` consults the existing object (preserved iff the existing type has flag `0x10` and the tile's byte 3 bit 7 is set). The 8×8 redraw counters are bumped (O-12). The sim already contains this routine (`place_object_record`, `walk_cells.cpp`); it is not wired to a founding command.
9. **Messages** (ORIGINAL): two records (types 2 and 4) are posted through the message manager (O-52). Their effect on deterministic state is unknown (A4, C9); they are not assumed to be presentation-only.
- **Expedition:** no direct write to the scout, donkey or ship other than the ship's holds and the owner's gold was found in the functions inspected; the scout (not the donkey) is part of the veto query. This is "no direct write found", not "no effect on the whole call path".
- REBUILD: `make_city`, `create_building`, tile layers and `Seams::on_placed` exist (`state.md` §7.3, §8.4). `Build` cannot be reused: it assumes an existing city of the issuer and a stock that pays.

## 7. Rejection and atomicity
- ADAPTATION: `apply` is total (`state.md` §8.2); a rejected command leaves `state_hash()` unchanged (balances, holds, figures, ids and serials, PRNG state, tile owners, objects). Preparation builds the city, building, claim and tile changes off to the side; commit applies them in the order of §6.
- PROPOSAL: `Reject` enumerators appended (append-only): `UnknownFigure`, `NotAShip`, `NotShipOwner`, `OutOfRange`, `OwnCityOnIsland`, `SiteOwned`, `NoRotationFits`, `FigureInTheWay`, `IslandLocked`, `NoCitySlot`; reused: `UnknownType`, `BadRotation`, `OutOfIsland`, `Occupied`, `TerrainRefused`, `CannotAfford`.
- A repeat after success is a new command, rejected by P8/P9. No idempotency id exists in the original.

## 8. Interaction with existing APIs
- Command/codec: one appended alternative (decision C4). Reads: figure owner, position, holds, figure type class, island rectangle, tile owners and objects, the height grid, registry type data, relation data if hostility is modelled. Writes: holds, owner gold, cities, building index, tile owners and objects, PRNG, island counters.
- Ordering of simultaneous commands: the existing rule (`state.md` §8.1, host-assigned turn, stream order). Overlapping footprints: the later command fails P8.

## 9. Determinism and serialization
- Integer arithmetic only in the rebuild: the distance test, centring, coast rule, claim table are integer; the original's f32 arithmetic is reproduced exactly only where C2 allows. No clock, no unordered containers.
- No state version change is proposed by this contract. The engine work of section 11 B (8.8 holds, new persisted fields) may require one; each is its own decision with an independent explanation and must verify that unrelated goldens are unchanged. Appending a codec alternative changes the command codec, not the state layout (C4).
- Fresh-process determinism: the same command log run in two processes yields equal `state_hash()` after every command.

## 10. Evidence summary
See `docs/re/ship-founding.md` §11.8. Newly settled in this pass: the territory algorithm (E), the city ring and registration, the single PRNG draw, byte8→byte0f, scenery replacement, the figure veto's query and boundaries, the island lock, the class-code wiring of world/player/sink.

## 11. Prerequisites, by category

### A. ORIGINAL EVIDENCE STILL MISSING (a rule or effect cannot yet be specified)
- A1. The initial relation bytes between players for a new five-player start (what "hostile" means at the start: relation kind 2 non-zero), and the meaning of player property `0x1c == 0x11`. Source: the scenario/setup path that fills `player+0x1ac…` (`AnnoWelt.dll 0x1001d7ec`, the diplomacy handlers `0x100ad210…`). Static, available.
- A2. Whether an embarked figure (the scout, or the ship's own hull) is registered in the sector cells and what its state-word bits are at founding time (`re` §11.4). Static: the ship's state word writers (`[ship+0x40]`) and the embark path; or runtime D2.
- A3. Residual: effects of **unresolved virtual callees** on the path (PRNG, climate, figure writes). The direct-call closures show one PRNG call and no climate read; the unresolved virtual call sites are 85/36/137/61/29/35/65/13 in the listed functions (`17-prng-closure.txt`).
- A4. Effect of the posted messages on deterministic state (message manager, O-52), and the discovery reveal's internals (`0x10080c10`) — only needed if the rebuild models discovery (C8).
- A5. Object property 0 and the seven class arms of the discovery stage (names, events) — same condition.

### B. ENGINE WORK: what exists, what needs an interface, what is missing
Verified against the tree at baseline `62ddaf1`. Nothing in this table is a rule of the original; it is the state of the rebuild.
| Item | Status | Where / note |
|---|---|---|
| HEIGHT samples | **present** as `Terrain::height`, `W×H` bytes row-major from the `HEIGHT` records (`scenario_formats.cpp`, `island.hpp` `Terrain`); shared immutable; the pointer may be null; a flat state-stream read leaves a hash-only placeholder (empty columns, non-zero digest) | the original's runtime grid is `(H+2)×(W+1)` with stride `W+1`; its boundary is established in `re` §12.3 (column `W` and rows `H`, `H+1` are 0) |
| Corner-height grid for placement | **needs an interface**: a checked, allocation-free derived view over `Terrain::height` (stride `W+1`, boundary 0, explicit missing-terrain and placeholder status) — task U1; no change to stored data, state version 116 or goldens | new `terrain_view.hpp` |
| Tile objects | **present**: `TileLayer::object` (u32 type id per tile), loaded from scenarios and templates; `foot_dx/dy/rotation`, `foot_anchor` (byte 3 bit 7, first tile written per world-aligned 8×8 block), `walk_code/mask`, `owner`, `aux` | `island.hpp`; digest and dirty-chunk accounting exist |
| Tile placement | **present, unwired to founding**: `place_object_record` (`walk_cells.cpp`) implements both branches of `0x10082040`, keyed on the placed type's flag `0x10`; the height byte `rec[8]` is not stored (documented deviation) | `byte0f` of the building and the tile low byte are separate questions, see section 11 B |
| Registry type data | **present**: `BuildingType::{prodtype, width, height, action_radius, flags_word}`; `footprint(type, rotation)`; the PRODTYPE name table. The cost entries exist | `types.hpp`, `building.hpp`. Nothing identifies a "ship-founding type": decision C3 |
| Existing placement rules | **present, different semantics**: `footprint_owner_rule` (`Build`: needs an existing city), `stand_in_placement_rule` (D17: any object refuses). They are not the founding rules and are not changed | `placement.hpp` |
| City slots, neutral owner | **present**: `SimConfig::city_slots` (16), `neutral_owner` (15), `Island::cities`, `CityRing {stamp, cursor}` (cursor is an unsigned index; the original starts at −1) | needs the allocation rule (section 11 B, ring row) |
| Figure position | **present**: `Figure::pos` Q24.8 (`FixedPos`), owner, `holds` (u64 raw 1/256 ton since stream 117) | precision and class below |
| Territory ownership writes | **present**: `TileLayer::set_owner` with dirty/hash accounting | the claim algorithm is missing: task U3 |
| Figure classes, hull/range data | **missing**: no BEWOHNTYPE in the registry, no `HEAD[0x10]`/`LOGIC[0x25]`, no crew occupancy, no state-word bits | needed for the veto |
| Relation (diplomacy) table | **missing** | needed only for the hostile branch of the veto (C5) |
| Cargo at 8.8 | **present** since stream 117 (`ShipHold::amount_88`, u64 raw 1/256 ton; manifest tons converted once at loading) | the payment itself is the next unit |
| City registration data, name list and draw, Kontor counter (island property `0x11`), KONTID | **missing** (KONTID "not kept", OQ-158) | |
| `LOCKED` island flag | **present** since stream 118: `Island::lock` (u8, raw, `locked()` is `!= 0`), read by `island_from_insel` for scenario and template islands, persisted and hashed; no run-time rule reads or sets it | section 15 |
| Discovery state of island objects | **missing** (island objects other than deposits are not modelled) | decision C8 |
| Founding command, codec, `Reject` values | **missing** by design | decision C4 |

### C. DESIGN DIRECTIONS
Accepted directions (orchestrator, fifth pass) with their limits, then the decisions that remain open.
- **C1 accepted: click and initial rotation are the command's geometry; the authoritative origin is derived by the original formula.** Any redundant supplied origin is validated against the derived one and a mismatch rejects. Consequence: the command carries a click; distance and centring both use it.
- **C2 accepted: keep the existing fixed-point positions and toward-zero arithmetic.** Equivalence with the original is claimed only for the verified coordinate domain: ship positions exactly representable in Q24.8 (multiples of 1/256) and in f32; positions the original holds as f32 values between two Q24.8 steps (e.g. 10.9) are not claimed to behave identically at the boundary. The veto's f32 constants (hull half-extent 1.55, ranges 16 and 7) are not representable in Q24.8 either (1.55 × 256 = 396.8); boundary results of the hull box and the range comparison are therefore **outside** the verified domain unless a fixture chooses constants that are exact in both. Fixtures use representable values only and say so.
- **C3 accepted: the offered types are data-driven, defaulting to warehouse 1302.** The shared predicate's PRODTYPE 33..64 test is a necessary condition, not the list of offered types.
- **C4 accepted: a future native command tag.** Replay compatibility must be demonstrated when it is implemented (old replays decode and run unchanged; goldens without the command unchanged); not demonstrated now.
- **C6 accepted: the original's 256-figure query truncation is not inherited.**
- **C7 accepted: a rejected founding leaves the payment and the whole world unchanged** (a full city ring is checked before any change).
- **Open:** C5 (initial diplomacy and the hostile branch of the veto, alternatives unchanged below), **C8** (discovery/reveal/messages) — discovery and messages are **not** assumed presentation-only: their effect on gameplay or deterministic state is unknown (A4/A5), and nothing in the isolated helpers depends on them.
- **C5.** The original rule needs relation data (B, A1). Alternatives: (a) relations as an explicit input of the new-game setup, both branches evaluated; (b) only the non-hostile branch, with the documented rule change that no player is hostile at the start; (c) withhold the command until relations exist. Recommendation (a), staged; (b) alters original behaviour and must not be silent.
- **C8.** Alternatives: (a) model discovery with the territory; (b) defer with an explicit list of the omitted state; (c) model the object masks only. Recommendation (b) for the first command unit if island objects are not yet simulation state, with the omitted state stated; the isolated territory helper (U3) implements ownership only.
- **C9.** Messages: unknown whether they affect deterministic state (A4); until known, not outside the hash by assumption.

### D. ORIGINAL-RUNTIME CONFIRMATION (each tied to one stated uncertainty)
- D1. Climate: one founding on a polar island and one on a steppe island next to a settled start; record accept/reject and any message (confirms "no climate test in the traced path").
- D2. The ship's hull and the embarked scout: place the ship so the hull box overlaps the footprint rectangle by one tile; record accept/reject, and the scout's position (settles A2).
- D3. Hostile ship near the site (needs a declared war): record accept/reject at a distance below and above the figure type's range (confirms path A once A1 is known).
- D4. PRNG: read the world PRNG index before and after the founding (one advance expected when a name list exists).
- D5. Territory: record the owner nibble of every tile within 30 tiles after founding (confirms the shape, own/rival exclusion).
- D6. Scenery: footprint with trees on part of it: record the tile object words before and after.
- D7. Cargo: duplicate holds, an exactly emptied hold (goods id cleared), gold exactly once.
- D8. Read `byte0f` of the new building (expected `0x30`).

## 12. Proposed implementation units (U1–U3 done as isolated helpers, section 15; U4–U6 not started)
Each unit stops at its acceptance tests; later units depend on earlier ones.
1. **U1 – height-grid bridge.** Objective: a checked, allocation-free corner-height view derived from the existing `Terrain::height` (stride `W+1`, boundary vertices 0 per `re` §12.3), with explicit missing-terrain and hash-only-placeholder status; no change to stored data. Files: `terrain_view.hpp/.cpp`, tests. Tests: non-square islands with distinct row/column values, non-zero origins, last tile and corner vertices, invalid and truncated data, absent/placeholder terrain, shipped island data through the existing loaders against an independent block-copy model. Dependency: none. Stopping point: the view only.
2. **U2 – pure predicates (height view, registry data; figure data only for the veto).** Objective: pure functions for centring, rotation trials, bounds, ownership, distance, occupancy compatibility and the coast rule. Tests: the 3,000-case coast oracle (`coast_model.py`, expected table exported as a fixture), centring cases, distance pairs of §13.1. Dependency: U1. Stopping point: functions only, no command.
3. **U3 – territory ownership.** Objective: the claim function (disc table, neutral-only painter, clipping) and the footprint claim. Tests: the 600-case oracle (`claim_model.py`), the R = 5 and R = 24 diagrams, own/rival tiles untouched. Dependency: U1. Stopping point: function only.
4. **U4 – cargo and city machinery.** (Cargo representation, payment preparation and `LOCKED` done, section 15; the city ring, registration data, Kontor counter and KONTID remain.) Objective: 8.8 holds with emptied-hold clearing; city ring, registration data, Kontor counter, KONTID, `LOCKED`. Tests: exact-cost/duplicate-hold/emptied-hold, ring order with cursor −1 and wrapping. Dependency: decision on holds' representation and state version. Stopping point: state fields and functions, no command.
5. **U5 – figure classes and the veto (C5/C6).** Objective: figure classes, ranges, hull data and the veto predicate with an explicit relation input. Tests: §13.1 hull/corner cases. Dependency: decisions C5/C6, A1/A2 resolved or recorded. Stopping point: predicate only.
6. **U6 – the command.** Objective: `FoundCmd`, codec, `Reject` values, prepared effects, the commit of §6, replay and fresh-process determinism. Tests: all of §13. Dependency: U1–U5 and decisions C1–C4, C7–C9. Stopping point: command applies in a synthetic world; no sailing or UI.
The smallest faithful bounded unit that can start now is **U1** (data), followed by U2/U3, which need no economy, military system or diplomacy.

## 13. Fixtures and tests (specifications; the coast and territory fixtures of U2/U3 now exist and run, section 15)
Fixtures are named `founding_synth_*`. Ships are placed directly: this tests founding, not sailing, and says nothing about a ship being able to reach the place. Positions are given as original f32 and as proposed Q24.8 (value × 256) where they matter; only exactly representable values are used. Heights are vertex bytes.

### 13.1 Isolated predicate expectations (each predicate alone; the others assumed satisfied)
| Case | Setup | Expected | Status |
|---|---|---|---|
| distance, integer pairs | ship at f32 `10.0` = Q `2560` on both axes; click offsets `(dx,dy)` | `(12,0)` 144 pass; `(10,8)` 164 pass; `(12,5)` 169 reject; `(13,0)` 169 reject; `(9,10)` 181 reject (168 is not a sum of two integer squares) | ORIGINAL |
| distance, toward-zero truncation | ship x = `20.5` (f32 `0x41a40000`, Q `5248`), y = 10.0; click `(8,10)` (`dx = trunc(-12.5) = -12`) and `(7,10)` (`-13`) | `(8,10)` pass (a floor would give -13 and reject); `(7,10)` reject | ORIGINAL |
| distance, fractional | ship `(10.75, 10.25)` (f32 `0x412c0000`, `0x41240000`; Q `2752`, `2624`); click `(21,18)` → `dx = trunc(10.25) = 10`, `dy = trunc(7.75) = 7`; click `(22,18)` → `11, 7` | `(21,18)` 149 pass; `(22,18)` 170 reject | ORIGINAL |
| decimal values | `10.9` is f32 `0x412e6666` = 10.89999962 and not representable in Q24.8 (2790.4); never a fixture value | n/a | rationale |
| initial footprint overflow | a 4×3 coast type; the click puts the origin so that rotation 0 (4 wide) exceeds the island's east edge by one tile; rotation 1 would fit | reject `OutOfIsland`; **no rotation trial** | ORIGINAL |
| lower bound | origin one tile west of the island | `OutOfIsland` | ORIGINAL (ship predicate) |
| exact edge | footprint ending exactly on the east/south edge; one tile further | pass; `OutOfIsland` | ORIGINAL |
| coast, rotation 1 | window `x in 10..14`, `x = 10` all `0x30`, `x = 11..14` `0x10`; origin `(10,12)` | pass, result byte `0x30` | ORIGINAL/E |
| coast, wrong edge | same grid, rotation 0 | reject | ORIGINAL/E |
| coast, window maximum | one vertex `0x40` inside the window | reject | ORIGINAL/E |
| coast, opposite edge | only one of five opposite vertices `< 0x20` (needs 2) | reject | ORIGINAL/E |
| 1-wide footprint | a type with `w = 1` | terrain test passes for any heights | ORIGINAL/E |
| scenery overlappable | a tree-type object (flag `0x20`) on a footprint tile | compatibility passes | ORIGINAL |
| obstruction | a building type without `0x20` on a footprint tile | reject `Occupied` | ORIGINAL |
| owned footprint | a tile owned by the issuer's city; by another player's city | both `SiteOwned` (the second is the takeover path) | ORIGINAL |
| own city | the issuer already has a city on the island, footprint neutral | `OwnCityOnIsland` | ORIGINAL |
| other player on the island | another player's city elsewhere, footprint neutral and clear | this predicate passes | ORIGINAL |
| locked island | `LOCKED = 1` | `IslandLocked` | ORIGINAL |
| payment checks | wood 11 (cost 12); tools 4; gold 349 | each `CannotAfford`, nothing changed | ORIGINAL |
| duplicate holds | wood 5 + wood 10 + wood 20 against cost 12 | check passes | ORIGINAL |
| figure veto, own ship hull (ship state word bits `0x0a` clear, otherwise the ship is excluded from the query, P12) | footprint `[10,14)×[12,16)`; own ship (hull 1.55) at f32 `(13.5, 14.5)` (Q `3456`, `3712`) → box x `trunc(11.95)=11 .. trunc(15.05)=15` | overlaps → `FigureInTheWay`. The hull constant 1.55 is an f32 value outside the Q24.8 domain (C2); the expected truncations (11.95, 15.05) are far from an integer boundary, so the result does not depend on that gap | ORIGINAL |
| figure veto, own ship clear (state word bits `0x0a` clear) | same footprint; ship at `(17.5, 14.5)` (Q `4480`, `3712`) → box x `15 .. 19` | no overlap (needs `xmin < 14`) | ORIGINAL |
| figure veto, point in rectangle | an own scout at f32 `(10.0, 12.0)` / at `(14.0, 12.0)` | inside (x0 inclusive) / outside (x1 exclusive) | ORIGINAL |
| figure veto, donkey | an own donkey (BEWOHNTYPE `ESEL`, group 3) inside the rectangle | not queried | ORIGINAL |
| figure veto, hostile range | needs relation data (A1/C5) | UNKNOWN until A1 | UNKNOWN |
| territory, shape | R = 5, 4×4 footprint at (10,10), neutral 24×24 island, owner 3 | the 12-row diagram of `re` §11.2 | ORIGINAL/E |
| territory, R = 24 | 4×4 at (30,30), neutral 64×64 | 1,976 tiles from the disc, rows 7..56, row 7 = x 27..36 | ORIGINAL/E |
| territory, rival/own tiles | a rival-owned block and an own-owned block inside the disc | unchanged | ORIGINAL/E |
| territory, island edge | footprint 3 tiles from the west edge, R = 24 | columns clamp at `x0`; rows above/below the island skipped | ORIGINAL/E |
| ring | fresh ring cursor −1: first city number 0; second city 1; free-slot search wraps at the slot count | | ORIGINAL |

### 13.2 Rotation search
- **Rotation 0 fails the terrain rule, rotation 1 passes, same bounds**: the `coast, rotation 1` grid with the cursor rotation 0: accepted rotation 1, origin unchanged, the building keeps rotation 1. ORIGINAL/E.
- **Parity-dependent**: a 4×3 coast type; rotation 0 fails the coast rule; rotation 1's footprint (3 wide, 4 tall) leaves the island's south edge; rotation 2 passes with the shore on the bottom row: accepted rotation 2. ORIGINAL/E.
- **Non-square, rotation 1 (guards the unrotated n/m of P11.4)**: a 4×3 coast type at rotation 1 has a 3-wide, 4-tall footprint; the vertex window is 4×5, the shore edge is the left column x=0 with 5 vertices (n = type width+1 = 5, unrotated). Pair of fixtures: (a) all five shore vertices 0x30, opposite column valid: accepted; (b) identical but the fifth shore vertex (y=4) not 0x30: rejected. An implementation that takes the edge length as the rotated width+1 = 4 accepts (b). The expected verdicts are generated by `coast_model.py` (emulated original), not by the C++. ORIGINAL/E.
- All four fail: `NoRotationFits`.

### 13.3 Successful end-to-end founding (all predicates pass)
Island 32×32 at the origin; land `0x40` for vertices `x <= 9`; shoreline vertex column `x = 10` at `0x30`; sea `0x10` for `x >= 11`; every tile neutral and empty; 1302-class type (flag word `0x2087`, 4×4, cost 12 wood, 5 tools, 350 gold, radius 24, 8.8 costs `3072`, `1280`); city-name list present; the issuer has no city on the island; no other figures; island not locked; ring empty. Ship (BEWOHNTYPE `SHIP`, owner the issuer) at f32 `(17.5, 14.5)` (Q `4480`, `3712`) with holds wood 50, food 10, tools 50, tools 30 (8.8: `12800`, `2560`, `12800`, `7680`) and gold 30000. Click `(11,13)` with rotation 1 → origin `(10,12)` (`ox = 11 - 1`, `oy = 13 - 1`); `dx = trunc(11-17.5) = -6`, `dy = trunc(13-14.5) = -1`.
| Observation | Expected | Status |
|---|---|---|
| result | Ok | ORIGINAL predicates; effects below |
| gold | 30000 → 29650 | ORIGINAL |
| holds | wood `12800 → 9728`; tools hold 1 `12800 → 11520`; tools hold 2 and food unchanged | ORIGINAL |
| city | number 0, owner the issuer, registered in the player's list; island players-present word has the player's bit | ORIGINAL |
| PRNG | advances by exactly one table step (name draw); a world with no name list: 0 steps | ORIGINAL (direct closure) |
| building | PRODTYPE-35 class, origin `(10,12)`, rotation 1, `byte0f = 0x30`, KONTID = old counter (default 1), island counter 2 | ORIGINAL |
| tiles | the 16 footprint tiles carry the building type id | ORIGINAL |
| territory | the disc + footprint of §13.1, owner nibble 0 on every previously neutral tile in them, other tiles untouched | ORIGINAL/E |
| scout, donkey, ship | ids, owners, parents and positions unchanged; the ship's holds as above | direct-write search only; see §6 |
| discovery, messages | per decision C8/C9 | UNKNOWN until C8 |
| rejection variants | every reject of §13.1 on this world leaves `state_hash()` unchanged, including PRNG | ADAPTATION |

### 13.4 Other end-to-end cases
| Case | Expected | Status |
|---|---|---|
| exact-cost hold | the first tools hold with exactly 5 t → amount 0 and goods id 0 | ORIGINAL |
| two holds consumed in order | tools holds 3 t and 30 t, need 5 → first emptied and cleared, second 28 | ORIGINAL |
| scenery replacement | a tree object (existing flag `0x20`) and a `STEIN` object (existing flags `0x30`) whose tile has byte 3 bit 7 set, both on footprint tiles, placed type = warehouse (flag `0x10` clear) | **both tiles overwritten** with the warehouse id (no existing-object test on this path) | ORIGINAL (`re` §12.1) |
| overlay branch (not a warehouse) | a placed type with flag `0x10` over an existing object of flags `0x10`: anchor bit set / clear / existing flag clear / object word 0 | kept / overwritten / overwritten / overwritten (the four rows of `re` §12.1) | ORIGINAL; implemented in `place_object_record`, outside founding |
| selected ship state bit | otherwise identical valid inputs; ship state word bit `0x02` set / clear | rejected / passes this predicate | ORIGINAL |
| same island, two players | player A founds on the west coast; B (no city on the island) founds on the east coast, unowned land | both Ok if both claims leave the other's site neutral; the second claim never overwrites A's tiles | ORIGINAL/E |
| own-city repeat | A founds again at another neutral coast site | `OwnCityOnIsland` | ORIGINAL |
| conflicting commands in one turn | A and B with overlapping footprints, A first | A Ok, B `SiteOwned`, B unchanged | ADAPTATION |
| ring full | 16 cities in the ring | `NoCitySlot`, payment kept (adaptation) | ADAPTATION |
| replay / fresh process | log replayed in a second process | equal hashes | ADAPTATION |

## 14. Original-runtime capture recipe
The smallest set is D1–D8 of §11. Each records, before and after: per-hold goods id and amount, gold, the accepted rotation, the city count and numbers, the owner nibble of the footprint and a 30-tile border, the scout/donkey/ship positions and parents, the world PRNG index, island property `0x11`, the building's `+0x0e`/`+0x0f` bytes and the message log. They confirm the stated uncertainties; they do not replace reading code.

## 15. Implementation status of the isolated helpers (task MAP-2, after the fifth pass)
No founding command, codec alternative, `Reject` value or state field exists. Three isolated, state-free helper units were added; each has its own tests and an independent review.
| Unit | Files | What it decides | Verified against |
|---|---|---|---|
| U1 corner-height view | `terrain_view.hpp/.cpp` | `VertexGrid` over `Terrain::height`: stride `W+1`, column `W` and row `H` read 0, statuses for missing, placeholder and malformed data; no allocation, no hash/serialization change | `re` §12.3; 668 shipped islands (9,396,540 vertices) compared against an independent rebuild of the original's grid |
| U2 site predicates | `site_checks.hpp/.cpp` | click centring, rotation order, bounds, distance, footprint ownership, supported overlap rows, coast-class height rule with the unrotated `n`/`m`; **not** "founding allowed" | `fixtures/synthetic/founding/coast_cases.txt` (emulated AnnoGame 0x10008f90, generator `tools/founding_fixtures.py`) |
| U3 territory ownership | `territory.hpp/.cpp` | integer disc profile and footprint rectangle, neutral tiles only, island-clipped; ownership only | `fixtures/synthetic/founding/territory_cases.txt` (emulated AnnoWelt 0x10085840, generator `tools/territory_fixtures.py`) |

Limits recorded by the reviews: the territory port equals the emulated original for radius ≤ 126 on a 4×4 footprint and differs from 127 on, where the original's byte-wide spans wrap (fixtures record both; wider radii are a deliberate widening, `max_radius` is the caller's bound). Overlap rows other than placed groups 1/0x21/0x61, and same-PRODTYPE replacement (bit 8 of type slot `+0xd0`), are `Unsupported`, not guessed. The distance rule's equivalence to the original's f32 computation is claimed only in the C2 domain; its tests are hand-derived. Everything remains static analysis plus leaf emulation: no run of the original.

### 15.1 Second round (task MAP-2: coordinates, cargo, payment, lock)
| Package | Files | Result | Evidence |
|---|---|---|---|
| A. coordinate repair | `src/local_tiles.hpp`, `site_checks.cpp`, `territory.cpp`, `founding_coords_test.cpp` | footprints stay 64-bit; tiles are addressed by local offset, so layers whose far edge is above INT32_MAX work (ownership, occupancy, rectangle and disc claims); the territory and site fixtures still agree | first-failure log `cache/map2-gates2/a1-first-failure.log` (189 failed assertions on the four crossing layers), then 0 |
| B. exact cargo | `figure.hpp`, `state_fields.hpp`, `ship_start.cpp`, stream 117 | `ShipHold::amount_88` is a u64 count of 1/256 ton; manifest tons convert once with a checked helper; a hold is 12 bytes; goldens moved only through the version (control run with version 116 and the new layout reproduced every old golden) | `ship_holds.txt` (Python), control log `b-control-version116-layout117.log` |
| C. payment preparation | `founding_payment.hpp/.cpp` | `payment::prepare(holds, money, costs)` returns a refusal or the complete proposed holds and gold; nothing is mutated; goods ascending, holds by index, `take = min`, `take >= amount` clears the goods id, **including an already-empty matching hold that is visited while need remains** (AnnoWelt `0x1003b9a0` `jge`, caller loop `0x10041760`); the slot's trailing notification call is not modelled | hand-derived cases, three real-code mutants (`c-mutants.txt`) |
| D. island lock | `island.hpp`, `scenario_formats.cpp`, stream 118 | `Island::lock` carried from the LOCKED record through scenario islands and link templates, the state stream and the hash; a record that is not one byte is refused (adaptation); a later record wins | the 107 locked islands of 8 scenario files (668 islands compared), the 21 dummy templates, `s5_islands_locked_create` from `tools/sim_stream.py` |

Still not done (separated by kind). **Evidence:** figure-veto inputs (A1 initial relations, A2 hull/crew registration), city registration, name list and draw, the Kontor counter and KONTID, the discovery/reveal/message effects (A4, A5), the virtual callees on the call path (A3), whether the hold slot's notification affects state. **Engine work:** figure classes, hull and range data, the relation table, the city-ring allocation rule, city registration data, the founding command with its codec and `Reject` values, atomic application of a payment plan, a run-time setter for the lock, and the same signed-32-bit narrowing in `walk_cells.cpp` (`index_of(static_cast<int32_t>(...))` at three sites, found by the package A review; a separate follow-up, not part of the founding helpers). **Owner decisions:** C5 (initial diplomacy), C8/C9 (discovery and messages), whether a malformed LOCKED record should refuse a load or only warn for modded data.
