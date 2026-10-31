# Ship-start integration contract

**Status:** revised specification for a load-only unit. It does not load a
manifest into the simulation, and it does not approve the starting economy.
Version-2 candidate bytes stay as they are. Movement stays in
`docs/re/ship-navigation.md`. The warehouse click stays in
`docs/re/ship-founding.md`. Neither is behaviour of the load unit.

The load unit uses the existing player table and the existing figure
registry (`World::figures()`, `docs/sim/state.md` §4.9). It does not add a
second world of ships.

## 1. Field owners

The manifest is `map2-ship-manifest` version 2, profile `map2-ship-start-v2`.
`src/sim_load` does not read it today. Owners below are the split when a
loader is added.

| Field | Owner | Authority |
|---|---|---|
| `manifest`, `version`, `profile`, `name` | Format (`tools/map2_manifest.py`, later the loader's schema) | Identity of the file. Not world state. |
| `edge` | Format, then the world bounds in `sim_load` | World size. The renderer reads the bounds; it does not choose them. |
| `base` | Content registry path | Which scenario file supplies settings. |
| `seed` | Format, recorded on the world | Map identity. Load must not draw ship positions from it. |
| `generator` | Format provenance | Not simulated. |
| `islands[].key` | Content registry | Which SCP template. |
| `islands[].id`, `x`, `y` | `sim_load`, then the island table in the simulation | Placement of templates. Not ownership. |
| `spawns[].seat` | `PlayerId` slot | Owner of that ship. Not a `FigureId`. |
| `spawns[].x`, `y` | Manifest: integer milli-tiles. Simulation: `FixedPos` | The conversion in §2. The manifest integer is not rewritten. |
| `spawns[].facing` | Manifest 0..3. Simulation: `dir16` on the ship figure, written at load | The table in `docs/re/ship-navigation.md` §4. Not a warehouse door. |
| `loadout` | `Player.money` and the ship's ordered holds | Comparison baseline only. See §4. |
| `excluded` | `sim_load` | What the base scenario must not copy. |
| `policy` | Offline checker | Not engine rules. The loader may store the block for provenance and must not sail by it. |
| `content_hash` | Serialization check of the file | Not the world checksum. |
| Exterior-4, routes, 256-tile component | Offline checker only | Not stored as passability. |
| Sprite frame for `dir16` | Renderer | Display. A wrong frame does not move the ship. |

A checksum of a running world covers `Player.money`, figure positions,
`dir16`, holds, parent links, and cities, because those fields are in the
state stream. It does not cover `figures.by_id`. That map is derived.

## 2. Where the units live

Ships, embarked scouts and donkeys are `Figure` values in
`World::figures()`. `FigureId` is the instance. `Figure.kind` is the
FigurenHeader type: 1502 ship, 717 scout, 718 donkey. A scout is not a
second ship. A donkey is not a crew slot (`docs/re/annoscene.md`: `CREW/ENTRY`
is 6 bytes and `ESEL` is a separate 8-byte record).

`Figure.home` stays invalid on all three. In the current registry `home` is
the building whose counter the figure holds. These units have no building.
Nesting uses a new parent link, not `home`.

Gold is `Player.money` on the player in slot `seat`. The constructor's
initial 10000 is replaced by the baseline's 30000. No second purse is
created.

### Identity

`IdCounters.next_figure` starts at 1 (`ids.hpp`). `validate_figures`
rejects `id.v == 0` and `id.v >= next_figure` (`figure.cpp`). The default
`FigureId` is `uint64` max, not 0. Seat 0 is a valid `PlayerId` and an
illegal `FigureId`. `ship_id = seat` is not the allocation.

The loader assigns ids from `next_figure` in this order, and advances the
counter by one after each figure:

for seat in `0..4`: the ship, then the scout, then the donkey.

Seat 0 receives figure ids 1, 2 and 3. Seat 4 receives 13, 14 and 15.
After the load, `next_figure` is 16. The same manifest produces the same
ids. Nothing is drawn from `seed`.

`figures.by_id` is rebuilt with `rebuild_figure_index`. It is not written
to the stream and it is not part of the hash.

### Positions

A manifest coordinate `m` is an integer milli-tile. `FixedPos` is Q24.8:
one tile is 256. Those units meet at

```
q = m * 32 / 125
```

because `256/1000 = 32/125`, and 32 and 125 are coprime. The loader accepts
`m` only when `m * 32` is divisible by 125. The quotient is the stored
`int32`. There is no rounding step. A value that does not divide, or whose
quotient does not fit in `int32`, is a load error and produces no world.

A tile centre does divide. Milli-tile `1000*t + 500` is `125 * (8*t + 4)`,
so

```
q = (8*t + 4) * 32 = 256*t + 128
```

which is the centre of tile `t` in Q24.8. Examples, both exact:

| Milli-tile | Quotient |
|---|---|
| 500 | `500 * 32 / 125 = 128` |
| 110500 | `110500 * 32 / 125 = 28288` (`256*110 + 128`) |
| −125 | `−125 * 32 / 125 = −32` |

`kWorldEdgeBound` is at most `2^23` tiles (`figure.hpp`). A centre inside
that edge fits the `int32`. The original ship's float height
(`+0x18`) is not a manifest field and is not stored. `FixedPos` has x and y.

The scout and the donkey receive the ship's `FixedPos`. The original
`ESEL` position is `u16` tile×10, a different scale. The load does not
keep a second coordinate that can diverge. Moving them apart belongs to a
later movement unit.

### Facing

Conversion is part of this load, using the table in
`docs/re/ship-navigation.md` §4: version-2 facing 0, 1, 2, 3 becomes
`dir16` 12, 8, 4, 0. A version-2 facing outside 0..3 is a load error.
The manifest integer is not rewritten and is not stored as if it were
already `dir16`.

`Figure` has no facing field today. The load unit adds a serialized `u8`
`dir16` (0..15) on `Figure`. The ship stores the converted value. The
scout and the donkey store the same value, because they are embarked. A
figure this loader does not create keeps 0.

### Holds, ownership, nesting

The ship's cargo is a new ordered list of holds, `{GoodsId goods, uint32
tons}`, serialized on the ship figure. The baseline is four entries, in
file order: wood 50, food 10, tools 50, tools 30. The two tools entries
stay two entries. The list is not sorted and is not merged.

`Figure.cargo_goods` and `Figure.cargo_88` stay the existing single-cargo
fields. This loader writes 0 in both for the ship, the scout and the
donkey. Manifest tons are whole tons. They are not shifted into 8.8 in
this unit. A later founding payment that needs 8.8 names that shift
itself.

`Figure` has 14 serialized fields today (`state_fields.hpp`). Adding
`dir16`, the parent id, the crew-slot index and the hold list requires a
line in `fields(Figure)`, a new `field_count<Figure>`, and a bump of
`kStateHashVersion` from 115. `by_id` stays derived. A stream whose
version is not the new value fails the existing version check. Goldens
are re-baselined in that same unit. An empty hold list and an invalid
parent are the values for a figure this loader did not create, so older
worlds change hash because the stream grew, not because their units moved.

Parent and crew slot:

| Figure | `owner` | parent | crew slot |
|---|---|---|---|
| Ship | `PlayerId(seat)` | invalid | none |
| Scout | same player | the ship's `FigureId` | 0 |
| Donkey | same player | the ship's `FigureId` | none |

`validate_figures` keeps the rule that every id is in `1 .. next_figure-1`
and unique. The load unit adds, without relaxing that rule: a parent is
either invalid or the id of a figure in this registry; the parent is not
the figure itself; following parents ends. The scout's parent and the
donkey's parent are the ship. A missing parent is a validation failure.

The original `CREW/ENTRY` also stores a u16 crew-member id. That field was
not given an instance meaning here. The scout's instance id is its
`FigureId`, and 717 is its `kind`. The donkey's 718 is its `kind`. The
original `ESEL` id u16 is not a second `FigureId` space.

Cities: none. Island reservations: none. No tile owner is written because
a ship exists. `developer_start` must be off for this load. The base
scenario supplies rules and catalogues. Its players and units are not
copied, because `excluded` lists `base-scenario players` and
`base-scenario units`.

Ship names that the original draws from a shared table are left unset. A
later spec that wants those names names the RNG call. Load itself performs
no draw.

## 3. Failure before any live world changes

Preparation builds a new world value, or it fails. It does not edit a world
that is already ticking.

| Failure | Result |
|---|---|
| Missing template, missing figure type, missing good name | Error, no world |
| Loadout is not the proposed baseline (§4) | Error, no world |
| Spawn milli-tile is not exact in Q24.8, or the quotient overflows `int32` | Error, no world |
| Facing outside 0..3 | Error, no world |
| Spawn owner is not the seat, or two ships share an owner | Error, no world |
| Seat count is not five, or a seat is missing | Error, no world |
| Unsupported manifest field or version | Error, no world |
| `excluded` is not the four-entry ship-start list | Error, no world |
| Base scenario file unreadable | Error, no world |
| A parent link fails the check in §2 | Error, no world |

A failure after a world already exists is a rejected command, not a
half-applied load. The founding command is not this unit.

## 4. What the first loader accepts

`tools/map2_ship.py` `load_and_check` calls `baseline_ok` and rejects any
other loadout. The first loader uses that same gate. It loads a version-2
manifest whose loadout equals the proposed baseline: status
`proposed-baseline`, the `03_1024_med_256` human-party source, gold 30000,
ship 1502, scout 717, donkey 718, and the four holds in §2.

A version-2 file with another gold total, another figure type, another hold
count, or another good is a valid manifest only in the sense that a later
loader might accept it. This loader rejects it. Acceptance tests use the
baseline, including the three candidate files, and one negative manifest
that differs only in the loadout. They do not claim that every
schema-valid loadout loads.

`loadout.status` remains `proposed-baseline`. Loading it does not record an
owner approval.

## 5. Tests the load unit must pass

Headless. No GPU. No movement and no founding.

| Test | Pass |
|---|---|
| Load each baseline candidate | Five players in slots 0..4, `money` 30000. Fifteen figures, ids 1..15, `next_figure` 16. Each ship kind 1502, parent invalid, four holds in file order with the two tools entries unmerged. Scout kind 717 and donkey kind 718, both parented to that seat's ship, scout crew slot 0. `cargo_goods` and `cargo_88` are 0. Zero cities. `developer_start` is off. |
| Positions | Each spawn centre `1000*t+500` is stored as `256*t+128`. A milli-tile not divisible by 125, such as 1, is rejected and no world is published. |
| Facing | Manifest 0, 1, 2, 3 is stored as `dir16` 12, 8, 4, 0 on the ship, the scout and the donkey. Facing 4 is rejected. The manifest bytes are unchanged. |
| Save and load | Positions, `dir16`, holds, parents, gold and the empty city list match. `by_id` after the load equals a rebuild. |
| Replay and a second peer | World checksum matches. The checksum includes the new fields and excludes `by_id`. |
| Non-baseline loadout | Error, and a pre-existing world is unchanged. |
| Missing template | Error, and a pre-existing world is unchanged. |
| Existing figure invariant | A fixture that inserts figure id 0 still fails `validate_figures`. |

The geometric checker suites are not these tests. Founding and sailing are
not these tests.

## 6. What this unit does not build

The ownership index and the spatial index wait until a command queries
them. `0x10049340` is unread, so no founding window is implemented.
Route search waits on the navigator in `docs/re/ship-navigation.md`.
Exterior-4 stays a checker rule.

Original type defaults (costs, radius, entrance points) come from the
content files when a later command needs them. This unit does not copy
them into a placement.

## 7. The next implementation unit

**Load the proposed baseline into a new headless world.** One unit. It
includes the Q24.8 conversion and the facing table. It does not include a
follow-up patch for either.

Files likely to change: a reader next to `src/sim_load/src/fixed_map.cpp`
(the fixed-map text parser stays), `Figure` and `fields(Figure)` with the
hash-version bump, `validate_figures` for the parent rule, and a headless
test. No movement, no founding, no renderer, no change to
`data/maps/lan5.txt` or the candidate JSON.

Acceptance is §5. Unresolved founding (`0x10049340`, OQ-490, OQ-520) and
unresolved navigation (the planner after `AnnoWelt.dll` `0x10030100`) stay
outside this unit's claims. A later founding unit and a later navigator
are separate, and they are not started by the load.
