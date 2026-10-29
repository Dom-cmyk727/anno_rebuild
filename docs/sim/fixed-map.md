# The fixed map (MAP.1)

A layout file of our own. It names original island templates and copies none of
their bytes, so it can live in git. The loader turns it into the same
`ScenarioSpec` an original `.sze` produces (`docs/plan/lan-mvp.md` §3). The
rest of the sim does not know the map is ours.

The first layout, `data/maps/lan5.txt`, is a **proposal** (owner question 6).
Its numbers are design. Everything the layout does not set comes from an
original file, and the Evidence column below says which.

## 1. Grammar

Line-oriented ASCII. A byte outside 0x09, 0x0a, 0x0d and 0x20–0x7e is refused
(`line is not ASCII`). CRLF and LF are the same: a trailing CR is stripped
before the line is read. Blank lines are ignored. A line whose first
non-whitespace character is `#` is a comment and is ignored; a `#` later on a
line is not a comment. Line numbers count every physical line, from 1,
including blanks and comments.

The first content line is the version line `anno1503-fixed-map 1`. Any other
first content line, including another version, is `version line must be
'anno1503-fixed-map 1'` on that line. A file with no content line is
`missing version line` on line 1.

After the version line, one record per line. Fields are separated by ASCII
space or tab. Every field of a record is required. An unknown record kind is
`unknown record '<kind>'`. A second `name`, `edge`, `base`, `seed` or `money`
is `duplicate record '<kind>'`. A second island with the same slot is
`duplicate island slot N`. A second start with the same seat is
`duplicate start seat N`. A second `raw` or `ore` for one slot is
`duplicate raw for island slot N` (or `ore`), including a line with no
values. A `raw` or `ore` whose slot is not an island is
`raw names island slot N, which is not in the layout` (or `ore`).

| Record | Fields | Notes |
|---|---|---|
| `name` | one token | The layout's name. One token, so a space is a second field and is refused. Kept as written. |
| `edge` | `<width> <height>` | World size in tiles. Each is an integer in `1 .. 16384` (`kWorldEdgeBound`, `config.hpp`). |
| `base` | one token | Path of an original endless `.sze`, relative to the game folder, `/`-separated. Kept as written. |
| `seed` | `<u32>` | `ScenarioSpec::seed`. |
| `money` | `<i32>` | Optional record. Absent: the loader uses the evidence default below. |
| `island` | `<slot> <x> <y> <template key>` | `slot` is a u32, unique. `x`, `y` are i32 world tiles of the island's top-left, the same coordinates as `IslandSpec::x0`/`y0` and `BuildingSpec` (`scenario.hpp`). |
| `start` | `<seat> <island slot> <kontor x> <kontor y> <rotation>` | `seat` is a u32, unique. `rotation` is an integer in `0 .. 3`. Kontor `x`, `y` are world tiles of the footprint's north-west corner (`BuildingSpec`). |
| `raw` | `<slot> <u32>...` | Optional. The integers after the slot, in order. **Parsed, not applied** (the existing deposit gap). |
| `ore` | `<slot> <u32>...` | Optional. Same as `raw`. |

A `raw` or `ore` line names an island slot that the file also has. A second
`raw` or `ore` for the same slot is `duplicate raw for island slot N` (or
`ore`). A slot with no `raw` line has an empty raw list.

Integers are decimal. A leading `-` is allowed only on a signed field (`x`,
`y`, `money`). No `+`, no fraction. A token that is not such an integer is
`field K of '<record>' is not an integer` (K counts from 1, the fields after
the record kind). A value outside the field's range is `field K of '<record>'
is out of range`. The wrong number of fields is `record '<kind>' needs N
fields, has M`. `raw` and `ore` need at least the slot; further fields are the
values, up to 256 (`kMaxValues`).

The reader accepts at most 65536 `island` lines and 65536 `start` lines
(`kMaxRecords` in `fixed_map.cpp`). The cap is the reader's, so a text file
cannot ask for an unbounded vector. The engine does not use it: island count
is `SimConfig::island_slots` and the seat count is the layout. Raise
`kMaxRecords`.

The template key is stored upper-cased (ASCII `a`–`z` only) with `\` rewritten
to `/`, which is how `scan_island_templates` keys a file (`scenario_load.cpp`,
`generic_string` then `ascii_upper`). `name` and `base` are not rewritten.

`edge` wider than 16384 is refused here because `WorldSettings::validate`
refuses it (`config.cpp`). 16384 is `kWorldEdgeBound`. Raise the reader's
`kEdgeBound` together with that constant.

## 2. Canonical form and digest

The digest is FNV-1a 64 (offset basis `0xcbf29ce484222325`, prime
`0x100000001b3`, the same function as `anno-dump`) over the canonical form, not
over the file bytes. Comments, blank lines and CR versus LF do not change it.
It is what the handshake will carry (W3).

Every integer is little-endian and fixed-width. A string is a u32 byte length
then the bytes, with no trailing NUL.

```
u32 version
u32 name_len, name bytes
u32 width, u32 height
u32 base_len, base bytes
u32 seed
u8  money_set          1 when the file has money, else 0
i32 money              0 when money_set is 0
u32 island_count       islands in ascending slot order
  u32 slot, i32 x, i32 y
  u32 key_len, key bytes
  u32 raw_count, raw_count times u32
  u32 ore_count, ore_count times u32
u32 start_count        starts in ascending seat order
  u32 seat, u32 island_slot, i32 kontor_x, i32 kontor_y, u32 rotation
```

## 3. Dump text

`anno-dump fixed-map <file>...` and `tools/fixed_map.py dump <file>...` print
the same text. The tool writes `# <path>` (the path as given) and then, for a
file that parses:

```
fixed-map 1
name <name>
edge <width> <height>
base <base>
seed <seed>
money <i32>
money default
island <slot> <x> <y> <key>
raw <slot> <u32>...
ore <slot> <u32>...
start <seat> <island> <x> <y> <rotation>
digest <16 lower-case hex digits>
```

`money <i32>` when the record is present, otherwise the single line
`money default`. `raw` and `ore` are printed only when that list is non-empty,
immediately after their island. Islands are in slot order, starts in seat
order. A file that does not parse prints one line
`error: line <n>: <message>` and the process exits 1. No partial summary.

## 4. What the loader checks

`sim_load::load_fixed_map` reads `base` from the game folder and calls
`load_scenario` (not the link resolver: no draw). It keeps that scenario's
`WorldSettings` and replaces `world_width` and `world_height` with `edge`.
The base scenario's islands, players, links and seed are not kept. `spec.seed`
is the layout's seed.

| Check | Result |
|---|---|
| Edge outside `kWorldEdgeBound`, or `prop10` not in 0..2 | `WorldSettings::validate`'s message. A larger edge is already refused by the reader. |
| Unknown template key | `unknown template '<key>'` |
| Template has no INFO, or INFO[0] is not a climate | `template '<key>': no INFO or an unknown climate` |
| Island rectangle outside the world | `island slot N: outside the world` |
| Two island rectangles overlap or touch on a tile | `island slot N: overlaps island slot M` (the message of `validate_island_rects` is not reused; this check is the loader's, so a test can see it without `World::create`) |
| Origin not a multiple of 8 | warning `UnalignedIsland` only, as `island_from_insel` does |
| Start seats are not exactly `0 .. n-1` | `start seats must be 0..N-1 without gaps` |
| Start names a slot the file does not have | `start seat N: island slot M is not in the layout`. The reader allows it; only the loader refuses. |
| Fewer than 2 seats, or more seats than start regions | `seats: N is not from 2 to M start regions` |
| Two starts name one island while the developer start is on | `start island slot N is used twice` |
| Kontor footprint not inside its island | `start seat N: the kontor footprint is outside island slot M` |
| Kontor type missing from the registry | `kontor type <id> is not in the registry` |
| Base file missing or `load_scenario` refuses it | `base: ...` |
| Base has no player of kind 8 | `base: no human player` |

Islands are placed by `place_island_links` with a hand-built `LoadedScenario`
and a `LinkResolution` whose every entry has a candidate (the value is not an
index; `place_island_links` only tests `has_value`). Each link's climate is
the template summary's INFO[0]. `place_island_links` then writes that climate
onto the island (`scenario_formats.cpp`, the assignment from
`IslandLinkSpec::climate`). Leaving the link's default 0 would mark every
island polar. The call does not draw.

`raw` and `ore` are not written to `IslandSpec::deposits` and are not passed
as link marks. The gap is unchanged: a `.sze` already warns `LinkRawMaterials`
and `LinkOresUnplaced` and fills no deposit (`scenario_formats.cpp`).

## 5. Players, cities, the developer start

One player per seat, in seat order. Id is the seat index. Kind 8, volk 1
(`scenario_formats.cpp` for a human HEAD). Colour is the seat's colour index,
stored where `World::create` stores a scenario colour (`default_colour`,
`scenario.cpp`). Name is the seat's name bytes. Money is the layout's `money`
when the record is present, otherwise the first kind-8 player's HEAD money in
the base scenario (file order).

The base scenario's player 31 is **not** copied. `SimConfig::neutral_player`
(31) is an index the bankruptcy tick skips when that player exists
(`player.cpp`); nothing in `World::create` or the tick requires the slot to be
filled. Neutral tiles use `neutral_owner` (15), a city number, not a player.
The player table is exactly the seats.

`FixedMapOptions::developer_start` defaults to true (MVP-A). For each seated
player, on the start island:

- one city, number = the seat index (below `city_slots`, and not
  `neutral_owner`; a seat index that collides is refused);
- one building, the registry's type for `FixedMapOptions::kontor_type`
  (default 1302, PRODTYPE 35, `synthetic.hpp`), at the layout's tile and
  rotation, created at tick 0 with no cost (`create_building`);
- `owner_runs` of one run, `{city number, width * height}`, so every tile of
  the start island belongs to that city.

Unseated start regions are the start islands whose seat is not among the seats
passed in. They are placed as islands and get no city and no owner run, so
their tiles stay neutral. Seats are a prefix: passing 3 seats seats 0, 1 and 2.

There is no coast check. The placement stand-in does not read the coast flag
(`placement.hpp`, OQ-052). The layout puts the Kontor on the coast anyway; the
preview shows it.

The whole-island owner run is a developer-start stand-in. The original's
Kontor territory is not modelled (OQ-490). Without it a `BuildCmd` is refused
`TileNotOwned` (`placement.hpp`).

`FixedMapOptions::kontor_type` is the original id as a default, not a limit.
A mod registry passes another id.

## 6. Evidence for values the layout does not set

| Value | Source |
|---|---|
| Every `WorldSettings` field except the edge | The base scenario's SCENE HEAD, via `load_scenario` / `read_head` (`scenario_formats.cpp`). |
| Default money | The base scenario's first kind-8 player's HEAD money (`read_player_head`, offset 4). |
| Island width, height, climate, terrain | The named template's INSEL5: climate is INFO[0], size is the head. |
| Kontor type id 1302 | PRODTYPE 35 KONTOR (`synthetic.hpp`). |
| Kind 8, volk 1 | A human player (`scenario_formats.cpp`). |
| Owner of a tile | Only `owner_runs`. The sim derives none from the Kontor. |

## 7. Not applied yet

Raw materials and ores. The fields are in the file, the canonical form and the
digest, and they change no sim state. Deposits stay empty, as they do for
every original scenario today.
