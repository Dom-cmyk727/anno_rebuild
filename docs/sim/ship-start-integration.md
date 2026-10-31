# Ship-start integration contract

**Status:** proposal for the first implementation units. It does not load a
manifest into the simulation, and it does not approve the starting economy.
Version-2 candidate bytes stay as they are. Facing conversion and movement
are specified in `docs/re/ship-navigation.md`. The founding click is
specified in `docs/re/ship-founding.md`.

## 1. Field owners

The manifest is `map2-ship-manifest` version 2, profile `map2-ship-start-v2`.
`src/sim_load` does not read it today. Owners below are the intended split
when a loader is added.

| Field | Owner | Authority |
|---|---|---|
| `manifest`, `version`, `profile`, `name` | Format (`tools/map2_manifest.py`, later the loader's schema) | Identity of the file. Not world state. |
| `edge` | Format, then the world bounds in `sim_load` | World size. The renderer reads the bounds; it does not choose them. |
| `base` | Content registry path | Which scenario file supplies settings. |
| `seed` | Format, recorded on the world | Map identity. Load must not draw ship positions from it. |
| `generator` | Format provenance | Not simulated. |
| `islands[].key` | Content registry | Which SCP template. |
| `islands[].id`, `x`, `y` | `sim_load`, then the island table in the simulation | Placement of templates. Not ownership. |
| `spawns[].seat` | Simulation player identity | Owner of that ship. |
| `spawns[].x`, `y` | Simulation, integer milli-tiles | Authoritative position (`docs/re/ship-navigation.md` §5). |
| `spawns[].facing` | Format 0..3, simulation `dir16` after the cardinal table | Not a warehouse door. |
| `loadout` | Simulation player and ship cargo | Comparison proposal, not approved balance. |
| `excluded` | `sim_load` | What the base scenario must not copy. |
| `policy` | Offline checker | Not engine rules. The loader may store the block for provenance and must not sail by it. |
| `content_hash` | Serialization check of the file | Not the world checksum. |
| Exterior-4, routes, 256-tile component | Offline checker only | Not stored as passability. |
| Sprite frame for `dir16` | Renderer | Display. A wrong frame does not move the ship. |

A checksum of a running world covers player gold, ship positions, facing,
holds, nested units, and cities. It does not cover the renderer's camera.

## 2. Initialization

The version-2 file describes five seats, `0..4`. That is a property of this
manifest version. The world must not use 5 as a maximum player count. A
later version can carry another count. This contract's first loader accepts
version 2 as five seats and rejects any other count in that version.

For each seat, in order `0..4`:

- Player identity is the seat index. The ship’s owner is that player. No
  other player owns it.
- Gold is `loadout.gold` (the proposal: 30000).
- One ship, figure type `loadout.ship_figure` (the proposal: 1502).
- Nested on that ship, not as further ships:
  - one crew record for figure `loadout.scout_figure` (the proposal: 717),
    using the `CREW/ENTRY` shape in `docs/re/annoscene.md` (figure id, crew
    slot index);
  - one `ESEL` record for figure `loadout.donkey_figure` (the proposal:
    718), which is the donkey carried by that unit, not a crew slot.
- Four cargo holds, in file order. The proposal is wood 50 t, food 10 t,
  tools 50 t, tools 30 t. Two holds may name the same good. They stay two
  holds.
- Position and facing come from that seat's spawn. Facing uses the cardinal
  table in `docs/re/ship-navigation.md` §4. The stored manifest integer is
  not rewritten.

Cities: none. Island reservations: none. No tile owner is written because a
ship exists. `developer_start` must be off for this load. The base scenario
(`base`) supplies rules and catalogues. Its players and units are not
copied, because `excluded` lists `base-scenario players` and
`base-scenario units`. A base player that would have occupied a seat is not
created.

Identifiers, in seat order, with no pointer addresses and no RNG:

- player id = seat
- ship id = a function of seat documented by the loader (the first unit
  below uses `ship_id = seat`)
- crew slot index = 0 for the single scout
- hold index = `0..3` in file order

The map `seed` is stored and not consumed. Ship names that the original
draws from a shared table are left unset. A later spec that wants those
names names the RNG call. Load itself performs no draw.

`loadout.status` remains `proposed-baseline`. Loading it does not record an
owner approval.

## 3. Failure before any live world changes

Preparation builds a new world value, or it fails. It does not edit a world
that is already ticking.

| Failure | Result |
|---|---|
| Missing template, missing figure type, missing good name | Error, no world |
| Hold count outside 1..4, tons outside 0..50, unknown good | Error, no world (the same bounds the manifest parser already enforces) |
| Spawn owner is not the seat, or two ships share an owner | Error, no world |
| Unsupported manifest field or version | Error, no world |
| `excluded` is not the four-entry ship-start list | Error, no world |
| Base scenario file unreadable | Error, no world |

A failure after a world already exists is a rejected command (§4), not a
half-applied load.

## 4. Future founding command

Not implemented. The boundary, when it is implemented:

1. **Validate** at execution, using the predicates that are then specified.
   Known from `docs/re/ship-founding.md` and still unresolved:

   | Predicate | State |
   |---|---|
   | Building type 1302, 4×4, coast flag on the type | File evidence |
   | Rotation 0..3 swaps axes | Static trace. The click's choice of rotation is unread (OQ-520). |
   | Uniform tile class other than `0x0f` | The compare is traced. The class name is unread. **Block:** island slot `+0xec`. |
   | Nearest category-matching object in the 7-tile window | Arithmetic is traced. The object class is unread. **Block:** island slot `+0x114`. |
   | Identity compare | The compare is traced. The operands are unread. Do not treat it as rival territory yet. |
   | Final accept `+0xa0` | Unread. |
   | Payer | **Block:** build-tool field at `AnnoFrame.dll` `0x100468ff`. |
   | Goods split across holds | Unread inside `+0x7c` / `+0xcc`. |
   | Territory radius | OQ-490. Radius 24 is the market search, not a proven city claim. |
   | Scenery under the footprint | OQ-063. Removal is unknown. |

2. **Pay** only from the accounts the command names (ship holds and player
   gold, separately). Do not invent a combined purse.

3. **Create** the city and the building in the same commit as the payment
   and the territorial write.

4. **Fail** with the world unchanged when validation fails. This is the
   rebuild invariant in `docs/re/ship-founding.md` §5. The original still
   calls the commit with flag `0xA0000000` when the affordability check
   returns 0; that callee was not opened, so the original is not claimed
   to roll back.

5. **Order** overlapping claims in one tick by seat, then by command index.
   The original's simultaneous order was not found.

The offline geometric site is not an input to this command.

## 5. Tests the implementation must pass

Minimum, for the loader first and the command later. Headless. No GPU.

| Test | Pass |
|---|---|
| Load any version-2 manifest | Five ships, nested scout and donkey, four holds, gold 30000, zero cities, zero owner runs. Same bytes in, same ids out. |
| Save and load that world | Positions, `dir16`, holds, gold and the empty city list match. |
| Replay the load | World checksum matches across two runs. |
| Two peers load the same manifest | Checksums match before any command. |
| Missing template | Error, and a pre-existing world object is byte-identical afterwards. |
| Founding, once the command exists | Insufficient goods: no city, gold unchanged. Two seats claiming the same tiles in one tick: lower seat commits, the other does not, both peers agree. |

The geometric checker suites are not these tests.

## 6. Indices and route work

Ownership index: island id, then city id, then the set of tiles that city
has claimed. It grows with cities, not with a fixed seat array.

Spatial index: ships and buildings keyed by tile. Queries for "objects near
a footprint" are bounded by the footprint plus the search window the
founding spec eventually names, not by a scan of every unit once that index
exists. Until the window's object class is known, the index is specified
and not built.

Route search, when a navigator exists, takes a step budget from data. The
checker’s 400 is not that budget. No timing claim is made here; nothing in
this packet was timed as a simulation.

Original type defaults (costs, radius, entrance points, `dir16` table) come
from the content files, not from constants pasted into the command.

## 7. Implementation backlog

Each unit is reviewable on its own. Later units wait for the named block,
not for a map to be chosen. Candidate selection, filler density, spawn
policy and economy approval stay owner decisions and are not steps in this
list.

1. **Load version-2 ships into a new headless world.** Concrete and ready
   to review. Files likely to change: a new reader next to
   `src/sim_load/src/fixed_map.cpp` (the fixed-map text parser stays), a
   small world-builder in `src/sim` that creates players, one ship, one
   crew record, one donkey record and four holds, and a test in the sim
   suite. `developer_start` stays off. No movement, no founding, no
   renderer, no change to `data/maps/lan5.txt` or the candidate JSON.
   Acceptance: §5 rows "Load", "Save and load", "Replay", "Two peers",
   "Missing template". Prerequisite: this readiness packet reviewed. No
   candidate has to be installed; the test loads a manifest by path.

2. **Facing and position fixtures.** Apply the cardinal table and the
   milli-tile rule in the loader from unit 1. Reject a version-2 facing
   outside 0..3. Do not add a `dir16` manifest field (that would be a new
   manifest version). Acceptance: the four table rows, and a position that
   round-trips as an integer.

3. **Founding command, validation only.** Implement the predicates that are
   already named, and return a structured rejection for every predicate
   still marked blocking in §4. No city is created in this unit.
   Prerequisite: unit 1. The blocking calls in §4 can land in any order
   without changing this unit's rejections.

4. **Commit.** City, building, payment, territory. Prerequisite: unit 3,
   plus a decision on the payer field and on OQ-490. Until those are read,
   this unit stays unstarted.

5. **Navigator.** Prerequisite: the planner methods named in
   `docs/re/ship-navigation.md` §2, or an explicit owner decision to sail
   with a specified approximation. Exterior-4 is not that decision.

Units 3–5 are not started by unit 1.
