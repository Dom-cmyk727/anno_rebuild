# Ship-start integration contract

**Status:** specification and **implemented (load only)** on branch
`task/MAP-2-original-like`. The loader builds a new headless `World` from an
explicitly supplied baseline version-2 manifest; section 9 records what exists,
what was verified and what is still open. It does not approve the starting
economy. Version-2 candidate bytes stay as they are. Movement stays in
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

The hierarchy is the human ship in `03_1024_med_256.sze`, read as
`SCHIFF` / `PRIVAT` / `CREW` / `ENTRY` / `PRIVAT` / `ESEL`. The ship is
figure 1502. The crew entry is scout 717 in slot 0. The `ESEL` record is
inside that scout and identifies donkey 718. The donkey is not a child of
the ship.

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
| Donkey | same player | the scout's `FigureId` | none |

Seat 0 is figure ids 1, 2, 3 with parents invalid, 1, 2. Seat 4 is ids
13, 14, 15 with parents invalid, 13, 14. A donkey whose parent is the ship
fails this baseline expectation. It is a different tree from the file.

`validate_figures` keeps the rule that every id is in `1 .. next_figure-1`
and unique. The load unit adds, without relaxing that rule: a parent is
either invalid or the id of a figure in this registry; the parent is not
the figure itself; following parents ends; a child has the same owner as
its parent; a crew slot requires a parent and is unique among that
parent's children. A missing parent, a cycle, or two children with the
same crew slot is a validation failure. The baseline expectation is
stricter than that graph check: the donkey's parent is the scout.

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
| Load each baseline candidate | Five players in slots 0..4, `money` 30000. Fifteen figures, ids 1..15, `next_figure` 16. Each ship kind 1502, parent invalid, no crew slot, four holds in file order with the two tools entries unmerged. Scout kind 717, parent the ship, crew slot 0. Donkey kind 718, parent the scout, no crew slot. A donkey parented to the ship fails the baseline expectation. `cargo_goods` and `cargo_88` are 0. Zero cities. `developer_start` is off. |
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

## 9. Implementation status (task MAP-2)

| Piece | Where | State |
|---|---|---|
| JSON reader (RFC 8259, duplicate keys at any depth, integer/real/bool kept apart, UTF-8 checked, depth 64), canonical writer, SHA-256 | `src/formats/{json,sha256}.*` | implemented, tested |
| Typed manifest reader, mirrors `tools/map2_manifest.py parse` | `src/formats/ship_manifest.*` | implemented; 84 shared cross-check cases (`fixtures/synthetic/map2_manifest/`, generated from the Python reference by `tools/map2_manifest_xcheck.py`, read by the C++ test) |
| `Figure.dir16`, `parent`, `crew_slot`, `holds`; `validate_figures`; `ScenarioSpec::figures`; stream 116 | `src/sim` | implemented |
| Loader: `load_ship_start`, `install_ship_start`, `expedition_baseline_problem`, `milli_tile_to_fixed`, `facing_to_dir16`, `manifest_goods` | `src/sim_load/{include,src}/ship_start.*` | implemented |

**Hierarchy as built.** Ship 1502, no parent, no crew slot; scout 717, parent the ship, crew
slot 0; donkey 718, parent the scout, no crew slot; one owner; `home` invalid; `cargo_goods` and
`cargo_88` 0. Seat 0 holds figure ids 1, 2, 3; seat 4 holds 13, 14, 15; `next_figure` is 16. A
donkey parented to the ship is a valid *graph* (the generic rules do not forbid it) and fails
`expedition_baseline_problem`, which is the baseline expectation.

**Content.** Goods names resolve through the registry: wood `HOLZ`, food `NAHRUNG`, tools
`WERKZEUG`, bricks `ZIEGEL`, spices `GEWUERZE` (original ids 42, 26, 41, 43, 31, checked against the
original files). Figure kinds 1502/717/718 must be figure types of the registry. A missing one is
an error naming the field. Ship names are not drawn.

**Parser compatibility.** The C++ reader accepts and rejects what the Python reference does, with
the same content hash, except where it is deliberately stricter. Those cases are marked
`ref=ok engine=reject` in `cases.txt`: integers beyond int64 (Python integers are unbounded), the
island id -1 (the reference starts its ordering check at -1), a lone surrogate escape, and nesting
deeper than 64. The bytes must be ASCII as the reference's command line requires. `content_hash`
follows the reference exactly: absent or `null` is accepted; anything else must equal the lower-case
SHA-256 hex of the canonical bytes (sorted keys, preserved arrays, compact separators,
`ensure_ascii`, `content_hash` removed, rebuilt from the parsed fields). The engine adds no
requirement that the file carry a hash.

**Schema validity is not loader support.** A schema-valid alternative loadout (for example gold
25000) parses and is rejected by `load_ship_start` with `loadout: not the supported baseline`. Five
seats and the baseline cargo are constraints of manifest version 2 and of this adapter; `World`,
`FigureRegistry` and `validate_figures` contain no seat or cargo limit.

**State stream.** `kStateHashVersion` 115 -> 116: the figure record gains `u8 dir16`, `u64 parent`,
optional `u32 crew_slot` and the hold list (`u64` count, `u32 goods`, `u32 tons` each); 14 -> 18
serialized fields. A figure without them writes 0, none, none, empty (18 bytes). Every golden moved
through the header; no rule or tick behaviour changed (the three event digests are identical to
115; `tools/sim_stream.py`, bumped to 116, agrees with the C++ on g0, g1, s5 and s6). The hold count
is bounded by the remaining input at 8 bytes per hold, so a hostile count cannot size a collection.

**What the loader does not do** (unchanged from section 6): no sailing, founding, combat,
rendering, developer input or economy; no Kontor, city, owned tile or island reservation; the
fixed-map text loader and `developer_start` are untouched; `data/maps/lan5.txt` and the candidate
files are not rewritten and no candidate is selected or installed. The loader duplicates the island
placement block of `fixed_map.cpp` (about 60 lines); folding both onto one function is a follow-up
that would touch the fixed-map path and was left out of this unit.

## 10. Review repair (task MAP-2, second round)

A review of `5804da0` found three defects. The simulation, the state stream (still 116) and every
golden are unchanged by the repair; the world a manifest loads into is bit-identical (candidate
state hashes `e471f6e1ff1164ec`, `0ffe696a2a07cc14`, `7ec33fd43149094b`).

**The base path is content-relative (loader boundary).** `sim_load::resolve_content_path` runs
before the base scenario is opened. A path is accepted only if it is non-empty and `/`-separated,
has no NUL or other control character, no backslash, no `:` (a Windows drive or stream), no
leading `/`, no empty segment (this also rules out UNC `//host` and a trailing `/`), no `.` or `..`
segment, and no segment that ends in a space or a dot. Nothing is normalised: a path that breaks a
rule is an error naming `base`, never another file name. The content directory itself is resolved
normally (it may be a link). The file is resolved through its links and must lie inside the real
content directory and be a regular file; the loader opens that resolved path. A link that stays
inside is followed; a link that leaves is refused. The schema reader is unchanged (`base` is any
non-empty string there, as in the Python reference): what may be opened is the loader's rule, so
the manifest compatibility contract stays one. Island `key`s are only looked up in the table of
scanned templates and are never opened by name. A refused load leaves an existing world untouched
(`install_ship_start`, checked by bytes and hash for every refusal).

**The coordinator that admits the tests.** The coordinator problem found in review was not
confined to the test harness: on macOS the production coordinator misjudged live owners and kept
its starter's output pipe. Both are repaired in `src/test_support/src` and described, with the
namespace, journal and compatibility consequences, in `docs/perf/README.md` ("MAP-2 repair").
Nothing in the load path depends on them; they decide whether the memory-admitted tests of this
unit (the scenario corpus, the candidate load) can be run on a Mac at all.


## 11. Repair unit 3 (task MAP-2, third round): trustworthy gate, isolated fixtures, coordinator transition

Loader semantics, state version, goldens, candidates and the active map are unchanged; only tests, the test-support coordinator and test tooling moved (`git diff a08b0d7 HEAD` touches no file under `src/sim/src`, `src/sim/include`, `src/sim_load`, `src/formats`, `data`, `fixtures`).

- **Fixtures.** The three content-path cases no longer share `anno1503-f6-content`. Each `ContentTree` owns a directory it created atomically (`create_directory` returns false for an existing name) under a random name and removes only that directory, also when its construction fails half way (`OwnedDir` is the first member). A new case checks distinct directories and cleanup. Every hostile-path row, the valid relative path, the internal link, the linked root and the unchanged-world checks are kept. Where a platform refuses to create a symlink (Windows without the privilege) the fixture says `skipping the symlink rows` and the link rows do not run; they are not counted as passed.
- **Gate.** See `docs/perf/README.md` ("MAP-2 repair 2"). A mandatory preflight (`test_names_are_selectable`) and per-test accounting of executed, empty, skipped (by class) and failed tests.
- **Coordinator.** Compatibility 4, versioned default journal name, one-time read-only adoption of the version-3 journal; the rollout limits are listed there. The ownership rules of §10 are unchanged.
