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
| Base has no player of kind 8 or 11, and the layout sets no money | `base: no human player` |

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
when the record is present, otherwise the first kind-8 or kind-11 player's
HEAD money in the base scenario (file order). Both kinds are human
(`docs/formats/sze.md`, players). The endless base used here has a kind-11
human and no kind 8.

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
| Default money | The base scenario's first kind-8 or kind-11 player's HEAD money (`read_player_head`, offset 4). |
| Island width, height, climate, terrain | The named template's INSEL5: climate is INFO[0], size is the head. |
| Kontor type id 1302 | PRODTYPE 35 KONTOR (`synthetic.hpp`). |
| Kind 8, volk 1 | A human player (`scenario_formats.cpp`). |
| Owner of a tile | Only `owner_runs`. The sim derives none from the Kontor. |

## 7. Not applied yet

Raw materials and ores. The fields are in the file, the canonical form and the
digest, and they change no sim state. Deposits stay empty, as they do for
every original scenario today.

## 8. Measured originals (MAP.2)

`tools/map2_measure.py` remeasured the nine endless scenarios on 2026-10-31.
The numbers below are that run. `docs/sim/map2-measured.json` holds the same
counts and hashes and no island masks. Rectangle share is the union of island
rectangles, in tenths of a percent, the same unit as `tools/world_measure.py`.
It is not buildable land. Links have no HEIGHT grid, so actual land is known
only where the scenario embeds an INSEL5. Those worlds are marked incomplete.

Land, where a grid exists, is `HEIGHT >= 16`. That is `tools/fixed_map.py`'s
land count and `kWaterline` in `anno/render/iso.hpp`. It is not `HEIGHT > 0`,
and it is not the original land classifier (average above 19).

| File | Islands (links + INSEL5) | Rectangle share | Gap min / p10 / median / max | Centre median (milli) | Clark-Evans R |
|---|---|---|---|---|---|
| 00 easyentry | 20 (16+4) | 39.4% | 24 / 24 / 32 / 64 | 184173 | 1.557 |
| 01 easy 256 | 28 (27+1) | 46.2% | 8 / 8 / 16 / 40 | 124193 | 1.379 |
| 02 easy 128 | 29 (28+1) | 29.6% | 16 / 16 / 24 / 72 | 144222 | 1.482 |
| 03 med 256 | 27 (27+0) | 45.6% | 8 / 8 / 32 / 72 | 141534 | 1.463 |
| 04 med 128 | 28 (28+0) | 29.0% | 16 / 24 / 32 / 88 | 152839 | 1.579 |
| 05 800 med | 23 (23+0) | 37.9% | 0 / 8 / 16 / 72 | 128996 | 1.501 |
| 06 heavy 256 | 27 (27+0) | 45.6% | 8 / 16 / 16 / 40 | 128996 | 1.396 |
| 07 heavy 128 | 28 (28+0) | 29.0% | 8 / 16 / 35 / 78 | 161987 | 1.603 |
| 08 800 heavy | 25 (25+0) | 28.4% | 16 / 16 / 32 / 72 | 120266 | 1.529 |

00 is 20 islands and 39.4% rectangle share. The board's "16 islands / 19%"
does not match this tree: 16 is the link count, and 19% is not the rectangle
union. 01 has 28 islands, not 27. Gap medians on the 1024² "256" maps are 16
or 32, and the minima are 8. 05 touches (gap 0). Clark-Evans R is about
1.38–1.60 with no edge correction, so the centres are more even than a Poisson
scatter. Centre distance is `floor(euclidean × 1000)` between rectangle
centres.

03 is the scale reference: 64²×5, 80²×10, 128²×8, 256²×4, climates
polar 3, tundra 3, nord 5, praerie 4, dschungel 7, steppe 5, rectangle union
477696 tiles, 45.6%. The 1431 edge is `(isqrt(1280² × 5) + 1) // 2`. World
area `1431² = 2047761`. Area ratio against 1024² is `2047761/1048576`.

Scaled from 03 by that ratio, the size mix is 64²×10, 80²×19, 128²×16,
256²×8: 53 islands, 46.3% rectangle share. The owner's ten 256² islands are
655360 tiles, 32.0% of 1431², against four 256² islands at 25% of 1024².
Three readings, none of them dropped:

| Reading | Islands | Rectangle tiles | Share |
|---|---|---|---|
| Proportional mix, including eight 256² | 53 | 948992 | 46.3% |
| Ten 256² plus the proportional 64/80/128 counts | 55 | 1080064 | 52.7% |
| Ten 256² and filler scaled to stay near 45.6% | 40 | 931072 | 45.5% |

Keeping both the reference rectangle share and the proportional filler counts
is not possible. The world stays 1431. The ten large islands stay. Which
filler density to ship is OQ-516.

Climate bands are the 03 bands scaled by the same edge rule and snapped down
to a multiple of 8. At 1431, inclusive start and exclusive end: polar
`[16, 296)`, tundra `[120, 488)`, nord `[40, 872)`, praerie `[160, 1104)`,
steppe `[232, 1336)`, dschungel `[552, 1368)`.

### Large-island templates

A shape is the land mask (`1` where `HEIGHT >= 16`) under the dihedral group:
four rotations and a left-right mirror. The canonical orientation is the
lexicographically smallest. The height hash uses that same orientation, so two
files can share a coastline and still differ in relief. Git stores the
SHA-256 of those bytes, not the mask.

The Nord folder has 13 numbered `.scp` files, plus `N_256x256_DUMMY.scp` and
`N_256x256_0010_1B_Mongolen.scp.bug`. There is no 0001, 0004, 0005, 0008 or
0009. The `*.scp` count is 14 only because it includes the dummy. Eligible
files (no `PIRAT` in the name, no dummy, no `.bug`, INFO[8] = INFO[9] = 0,
prefix matches INFO[0]) are 12, and three of those are Mongolen. The candidate
pool, eligible and with no people marker, is 9 distinct land shapes. The
board's "14 usable" does not match this tree.

Dschungel has 18 numbered `.scp` files plus a dummy. Eligible is 18. The
candidate pool is 10 distinct shapes. People-marked files that copy a plain
island, with height hashes agreeing, are 0001=0011 Azteken, 0003=0013
Azteken, 0004=0014 Azteken, 0006=0016 Suedsee=0018 Azteken, and 0010=0012
Afrika. Those names are not extra shapes. `N_256X256_0011` (Mongolen) and
`N_256X256_0019` share a land mask and disagree on the height hash, so the
relief is not a copy. 0011 is excluded by the people marker. 0019 stays.

No five-shape subset of either pool is within ±5% of its mean land (50
permille). Nord has one subset within ±10% (100 permille): 0002, 0003, 0007,
0018, 0019, lands 29461, 29828, 30383, 26213, 26304, worst 79 permille.
Dschungel has six within ±10%. The closest is 0002, 0003, 0005, 0006, 0010,
lands 28126, 26648, 29033, 29022, 27638, worst 52 permille. 52 is the ceiling
of `|5 × 26648 − 140467| × 1000 / 140467`. It fails 50 permille. Widening the
limit to make that a pass is not a measurement.

The half-plane on edge 1431 is integer, and origins are multiples of 8. A
southern top passes when `2 × y >= 1431`. `y = 704` gives `1408`, which fails.
`y = 712` gives `1424`, which fails. The first eligible 8-grid southern top
is `y = 720` (`1440 >= 1431`). A northern 256² centre passes when
`2 × (y + 128) <= 1431`. `y = 584` gives centre 712 and `1424 <= 1431`, so it
passes. `y = 592` gives centre 720 and `1440 > 1431`, so it fails.

## 9. Retired profile `map2-proposed-v1` (historical)

Sections 9 and 10 are the retired Kontor-start profile. They are not active
acceptance. Assigned northern homes, assigned southern partners, nearest-Kontor
ownership of neutral islands, and equal land or equal distance within ±5% or
±10% are not gates for the ship-start candidates. The numbers below stay as
results of that retired profile. A failure there is not a pass of
`map2-ship-start-v1`, and a pass of one of its distance gates is not a pass
of the new profile.

## 9.1 What `map2-proposed-v1` checked

`tools/map2_profile.py validate` is a separate check. `tools/fixed_map.py
check`, `dump` and `fairness` are unchanged. An ordinary map, including
`data/maps/lan5.txt`, stays valid under the generic check and is not required
to have five northern and five southern large islands. The profile is the
named five-player layout.

A full pass prints `PASS profile=map2-proposed-v1` and the permille limits.
Exit status is 0 only then. The limits are proposals (OQ-515), not an approval.
±10% large-island land is 100 permille. ±5% fairness is 50 permille. The older
±10% on distances and ±25% on filler are printed as sensitivity and are not
gates. A failing measure is not made green by widening the limit.

Comparison is integer. A list `v` is within `permille` of its mean when every
value satisfies `|n × v − sum| × 1000 <= permille × sum`, inclusive. An empty
list fails. All zeros pass the ratio and are reported separately if a route
length is zero. A climate that some seats have and others do not is absent,
not a zero to divide by.

Structural rules:

- five large NORD 256² islands and five large DSCHUNGEL 256² islands;
- each northern centre on the north side of the middle line, each southern
  top on the south side;
- five different land shapes in each group, by the mask hash in §8, not by
  filename;
- no people marker (`MONGOLEN`, `PIRATEN`, `AZTEKEN`, `AFRIKA`, `SUEDSEE`);
- rectangles on the 8-grid, inside the world, and not overlapping (a shared
  tile fails; a touch, gap 0, does not);
- filler islands inside their climate band when the island fits, otherwise on
  the band's snapped midpoint;
- five starts, seats 0..4, each Kontor a 4×4 on free coastal land of a
  different large northern island (`HEIGHT >= 16`, no GELAENDE, FLUESSE or
  PROD2 origin on the footprint, one side on water).

The Kontor rotation in the file is 0 because the footprint is square. The
original door direction is unread (OQ-520). The generator places the footprint
on the coast most aligned with the map centre. That choice is in the layout
tool. `World::create` does not read it.

Fairness endpoints, all straight-line:

| Measure | Endpoint | Gate |
|---|---|---|
| Home land | `HEIGHT >= 16` on the five northern islands | 50 and 100 permille |
| Paired southern land | the same, on the assigned southern islands | 50 and 100 permille |
| Pair distance | Kontor centre to the nearest land-tile square of the assigned south island, `floor(euclidean × 1000)` | 50 permille |
| Rival distance | nearest other Kontor centre, same milli unit | 50 permille |
| Centre distance | Kontor centre to the map centre `(width // 2, height // 2)` | 50 permille |
| Reach by climate | filler only; owner is the nearest Kontor, tie to the lower seat; contested when the map centre is strictly closer | 50 permille when every seat has the climate; absent otherwise |

Home land is gated twice: 100 permille as large-island land, and 50 permille
as fairness. The measured templates fail the second and pass the first.
Pairing minimises the spread of the five route lengths, then the sum of those
lengths, then the permutation in lexicographic order (`min-spread-sum-lex`).
The report also prints `min-spread-lex`, which stops after the spread. Those
two disagree on a constructed case (a short cycle of length 150 against five
equal routes of length 100, sum 500). On the three candidates they agreed.
Navigable water is not computed (OQ-517).

A radial screen flags the five home-island centres when both their radii and
their consecutive angle gaps, including the wrap around the centre, are within
50 permille of their means. A northern arc has one large wrap gap, so it is
not that pattern. The screen is not a fairness score.

`tools/map2_selftest.py` is synthetic. It does not read original files except
to dump `lan5.txt` and check the digest `c6af83f95104cd4f`, 21 islands, and
1476 bytes.

## 10. Historical Kontor-start candidates

`tools/map2_layout.py` writes `data/maps/candidates/lan5-a.txt`, `lan5-b.txt`
and `lan5-c.txt`. It does not write `data/maps/lan5.txt`. The same arguments
write the same bytes. Large-island origins are authored. Filler is first-fit
on the 8-tile lattice in an order fixed by the layout seed, larger rectangles
first, preferring a gap of at least 8 and then allowing a touch. The engine
still loads the text as an ordinary fixed map. Nothing here replaces lan5.

Each southern island is placed on the opposite ray from its home, so the
inner coast faces both the map centre and that partner. The routes are long
because the partner is past the centre. Seeds are 15031, 15032 and 15033.
Template order rotates by candidate; the set does not. Nord is the only
within-100 quintuple. South is the closest quintuple.

Filler weights are the non-256 part of 03's climate×size table (23 islands):
polar 64×1 and 80×2, tundra 80×2 and 128×1, nord 64×1 and 80×2, praerie 80×1
and 128×3, dschungel 64×2, 80×2 and 128×1, steppe 64×1, 80×1 and 128×3. There
is no tundra 64 and no nord 128 in that table, so none is placed. Counts are
largest-remainder. A and B ask for 30 filler islands (the coverage-matched
reading). C asks for 45 (the denser reading).

| | lan5-a | lan5-b | lan5-c |
|---|---|---|---|
| Arc | 152° | 144° | 140° |
| Islands | 40 | 40 | 55 |
| Rectangle share | 45.5% | 45.5% | 52.2% |
| SHA-256 | `0ac467ca4f2f2beaec91362ed61482120c99fd8de952d8f0d28b540766abc846` | `e391dbda37de3993040b8bdea3303dfaa71fb6f33e20cf49d655d57cfe084772` | `bc9f70791cad62da2350e490841ee7bc807265d59d00e7e6c0dca15460086e02` |
| Bytes | 2442 | 2442 | 3160 |
| Large-land nord / south | pass 79 / pass 52 | pass 79 / pass 52 | pass 79 / pass 52 |
| Home land ±5% | fail 79 | fail 79 | fail 79 |
| Paired south land ±5% | fail 52 | fail 52 | fail 52 |
| Pair distance ±5% | pass 11 | pass 29 | pass 21 |
| Pair lengths (milli) | 888005, 905803, 899246, 903285, 886778 | 840535, 804110, 831356, 811876, 850652 | 876816, 872066, 874245, 861278, 893344 |
| Centre distance ±5% | pass 9 | pass 41 | pass 43 |
| Rival distance ±5% | fail 177 | fail 210 | fail 215 |
| Reach | every climate absent for at least one seat | same | same |
| Assignment | slots 10..14 in seat order | slots 10..14 in seat order | swaps slots 12 and 13; both assignment modes agree |

Pair lengths are milli-tiles, so lan5-a's routes are about 887–906 tiles.
The spread is within ±5%. The length itself is a crossing of the map.
`min-spread-lex` selected the same pairing as `min-spread-sum-lex` on all
three. lan5-c's measured pairing is not the authored opposite ray for seats
2 and 3: the swap of slots 12 and 13 has the smaller spread. Both modes
choose that swap.

No candidate is a full pass of `map2-proposed-v1`. Structural rules pass:
five and five, half-plane, distinct shapes, no people templates, coasts,
bands, no overlaps, no missed filler. The failed gates are the ones the
template pools and the coastlines force. Status for owner selection is
QUESTION, not a pass.

`tools/map2_preview.py` draws land masks, not rectangles, into an ignored
cache directory. Top-down has north at small y. Isometric has world +x
down-right and +y down-left, so north is at the top. The pictures inspected
on 2026-10-31 show five irregular northern islands in an arc, five irregular
southern islands across the centre, a horizontal middle line, a centre mark,
seat numbers on the inner coasts, and pairing lines through the centre.
Filler uses all six climate colours. lan5-c is visibly the denser of the
three. The lines are geometric reach, not shipping routes. Land area and
these distances do not show fertility, ores, usable building space, or
late-game economics. The files stay in `data/maps/candidates/` as historical
Kontor-start proposals. The runtime loader still reads that grammar. It does
not read the ship-start manifests in §11.

## 11. Ship starts, natural differences, optional sharing

Owner decisions of 2026-10-31, recorded in
`docs/research/endless-start-conditions.md`. That report is the evidence.
This section is the reconciliation. The geographic envelope stays: world
1431 × 1431, five seats, five distinct plain NORD 256² islands whose centres
are on the north side of the middle line, five distinct plain DSCHUNGEL 256²
islands whose tops are on the south side, and smaller islands in the climate
bands of §8. Those are project choices. They do not assign an island to a
seat. A first settlement may be on a suitable temperate or jungle island.

### 11.1 Decision table

| Item | Status |
|---|---|
| Players start aboard ships and choose the first settlement. No preassigned Kontor, home island, or paired southern island. | **Settled.** |
| Island quality, building space, resources and travel may differ. Competition for desirable islands is intentional. ±5% and ±10% equality are not acceptance goals. | **Settled.** |
| Several players may settle one large island: cooperation, rival footholds, borders, later land warfare. | **Settled.** Sharing is optional. |
| All five players must have viable chances to settle different islands. Sharing is not how the player count is made to fit. | **Settled.** |
| The first Kontor is a territorial foothold. It does not reserve the whole island. The whole-island owner run is the developer-start test shortcut (OQ-490). | **Settled.** |
| Edge 1431, five seats, five plain NORD 256², five plain DSCHUNGEL 256², half-plane, banded filler. | **Retained** as geography, not as ownership. |
| Original templates, people/pirate/dummy exclusions, `HEIGHT >= 16` as a land count. | **Retained** as measurement. `HEIGHT >= 16` does not prove a legal building or a viable economy (OQ-519). |
| `map2-proposed-v1` Kontor starts, opposite-ray pairing, nearest-Kontor reach, equality gates. | **Superseded** for acceptance. Historical results stay in §9–§10. |
| `anno1503-fixed-map 1` grammar, digest, and `start` as a Kontor footprint. | **Retained.** A `start` record is not a ship. |
| Generator 99% floor (`world-scaling.md` §12, OQ-100). | **Retained** for the later generator. It does not promise equal holdings after a race to settle, and it is not this profile's gate. |
| Filler density, 45.5% versus ~52% rectangle share (OQ-516). | **Open.** |
| Navigable shipping, fertility, ores, building legality (OQ-517, OQ-519). | **Open.** Tool routes are geometric feasibility. |
| Clustered versus dispersed spawn policy (OQ-521). | **Open.** These proposals use authored, separated sea positions. |
| Founding orientation, fog, simultaneous founding, combat. | **Unknown** until W6.1 / W6.5. Not implemented here. |
| Starting gold and cargo. | **Proposed** comparison baseline from `03_1024_med_256`, not an economy approval. |
| Which of the three ship-start candidates to ship. | **Open.** This lane recommends and does not install. |

### 11.2 Profile `map2-ship-start-v2`

The version-1 checker committed at `570bb97` is not acceptance. It treated a
reachable island rectangle plus any coastal 4×4 as one opportunity, blocked
only the origin tile of an obstacle, and reported the largest free component
anywhere on the island. Those `PASS_TOOL_CHECKS` lines are withdrawn. The
version-1 files stay in `data/maps/candidates/ship-start-v1/`. `parse` rejects
version 1. `upgrade_v1` is the explicit copy onto version 2: same placements,
a recorded policy, and a new content hash.

Offline tools only. The present engine does not load the manifest, does not
spawn these ships, and does not found cities from them. Turning off
`developer_start` omits the whole-island grant. It does not create a ship.

The evaluated policy is part of the manifest and part of the content hash:

| Field | Value | Meaning |
|---|---|---|
| `grid` | `tile` | One cell is one world tile. |
| `metric` | `manhattan-4` | Water movement is 4-connected. Each step costs 1 tile. A diagonal is not a move, so the cost is not a Euclidean length and not an upper bound from a unit-cost diagonal. |
| `clearance_scope` | `spawn` | Chebyshev gap from the spawn tile to the nearest `HEIGHT >= 16` tile. Clearance is not applied along the route. |
| `obstruction` | `anlagen-head-conservative` | ANLAGEN `HEAD` width and height, with the axes swapped for an odd placement rotation. These are world tiles, not sprite pixels. Removability is unresolved, so the whole footprint blocks. |
| `clearance` | 8 | Spawn gap, in tiles. |
| `max_route_tiles` | 400 | Useful-opening bound, in tile steps. A longer connected route is a detour. The number of tiles is the old bound; the unit is no longer a coarse step times 8. |
| `min_city` | 256 | Provisional attached city-space estimate. Not an original rule and not an equality target. |
| `share_gap` | 32 | Minimum Chebyshev gap between two 4×4 centres before a share is reported. |
| `share_min_side` | 128 | Island side below which sharing is not reported. |

An eligible seat-to-island edge is one linked witness: the spawn, a
4-connected route, the approach water cell beside that site, the 4×4, and
that site's own connected free-land component of at least `min_city` tiles.
The route ends on that water cell. Bounding rectangles are only a broad
phase for spawn clearance. An enclosed lake is not ocean. A larger component
elsewhere on the island is not credited to the site. The saved witness stores
the spawn tile, the site origin, the water cell, the tile cost, and that
component's size.

The distinct-island matching runs on those edges. It is a feasibility
witness, not ownership. Optional sharing is descriptive and cannot stand in
for a missing island. An island with no eligible site, including a decorative
filler island, is a diagnostic. It does not fail the candidate while five
distinct islands still match. `tools/map2_ship.py` emits
`distinct-island witness covers N of 5` when the matching is short, and
`island ID has no eligible founding site` as a diagnostic.

Hard structural checks, always on:

- schema version 2, no duplicate JSON member names, no unsupported fields,
  and `excluded` equal to the ship-start exclusion list;
- the retained geographic envelope, distinct plain shapes, half-plane, bands,
  8-grid, bounds, and no rectangle overlap;
- no people, pirate, or dummy template;
- five sea spawns inside the world, on sea, separated, with the spawn clearance;
- no player city, no island reservation, and no field that assigns a home or
  an expansion island to a seat.

`load_templates(game, keys)` without a HEAD table keeps origin-only occupancy
for the retired profile tools. Ship checks pass the HEAD table. A missing
ANLAGEN id is an error.

Descriptive diagnostics, not gates and not optimised toward equality:

- alternative islands inside the bound, with route cost, site, water cell,
  and attached component size;
- optional sharing, not added into the witness;
- every climate from polar through wueste, including a climate absent
  everywhere. Reach is among islands inside the bound. Fertility and deposits
  are not treated as known, and an absent climate is not a zero-distance success.

Future engine validation, not run here: the loader seam, save and checksum,
original movement and founding predicates, which objects can be removed, the
warehouse door, visibility, simultaneous founding, and host tests.

### 11.3 Opportunity, not equal holdings

The scaling formula in `world-scaling.md` §5 stays. Overall land and the
climate mix stay the supply the world offers. After players sail and settle,
holdings may differ. The profile checks that a complete matching onto five
distinct islands exists, and it lists other choices. It does not assign those
islands, and it does not require each seat to hold every climate.

### 11.4 Manifest boundary

`tools/map2_manifest.py` reads `map2-ship-manifest` version 2, profile
`map2-ship-start-v2`. Positions are integer milli-tiles (1000 per tile).
The content hash is SHA-256 of the canonical JSON with `content_hash`
removed: object keys sorted, arrays in id and seat order, ASCII, separators
`,` and `:`. Every accepted public field is in that hash, including `policy`
and `excluded`. Duplicate member names are rejected at every object.
Unsupported fields are rejected. A supplied `excluded` list that is not
exactly `base-scenario players`, `base-scenario units`, `player cities`,
`island reservations` is an error. The parser does not replace it. The
object rejects `cities`, `reservations`, `homes`, `partners`, `starts`, and
`owner_runs`. Version 1 is unsupported. It does not copy the base scenario's
players or units. The proposed loadout, identical for every candidate unless
the owner changes the baseline, is 30000 gold, ship figure 1502, scout 717,
donkey 718, and holds wood 50 t, food 10 t, tools 50 t, tools 30 t, from the
human party of `03_1024_med_256`. That baseline is evidence for comparison,
not an economy approval.

`src/sim_load` still loads only `anno1503-fixed-map 1`. These manifests are
not executable ship starts. The version-2 proposals are
`data/maps/candidates/ship-start-v2/`. The version-1 files stay beside them
as the pre-repair inputs. The historical `lan5-a.txt`, `lan5-b.txt`, and
`lan5-c.txt` files stay as Kontor-start measurements. `data/maps/lan5.txt`
is not one of these proposals.
