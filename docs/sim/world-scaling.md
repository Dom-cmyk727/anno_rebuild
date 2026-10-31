# World scaling — the enhanced endless generator

Status: spec, 2026-10-26 (T37). No generator code yet. The owner has not approved the feel numbers in [§15](#15-owner-questions).

An implementer builds one function: from a `GameSetup` (T36) and a generator version, produce an `IslandLinkSpec` list plus a start list. `resolve_island_links` (`src/sim/include/anno/sim/island_links.hpp`) then runs the original island selection unchanged (`docs/re/annoscene.md` §5). The rest of the sim loads that world the same way it loads a scenario.

Evidence is the read-only measurement of `game/app` by `tools/world_measure.py`, the selection rules in `docs/re/annoscene.md` §5, and the link and template layouts in `docs/formats/sze.md` and `docs/formats/insel.md`. T36's preset table was not on a branch when this spec was written; the twelve files named in T36 step 2 were measured directly. No sentence from an `.htm` file is copied here. Description levels below are the bullet order of those files, recorded as the enum words already used by the plan.

## 1. How to reproduce the measurements

From a checkout that can import `tools/sze.py` and `tools/insel.py`:

```text
python tools/world_measure.py inventory <game>/Scenes/Islands
python tools/world_measure.py world <game>/Scenes/0010SingleEndlos/<file>.sze
python tools/world_measure.py world <game>/Scenes/0011SingleEndlosAddon/<file>.sze
```

The tool prints counts and coordinates. It writes nothing. `inventory` counts every `*.scp` under that directory. `world` reads one scenario. Both leave the game directory untouched.

## 2. Template inventory

`Scenes/Islands` holds 465 `*.scp` files in six climate folders: Dschungel 132, Nord 125, Polar 29, Praerie 66, Steppe 88, Tundra 25. The selection prefix table (`annoscene.md` §5, `0x1003b328`) is `po_ t_ n_ pr_ d_ s_ w_`. No file uses `w_`.

A file is eligible when all of the following hold, which is the scan at AnnoScene `0x10009550`:

- the file name, upper-cased, does not contain `PIRAT`;
- the file name does not end in `DUMMY.scp` (the comparison is case-insensitive here; the original compares the tail as stored);
- `INFO[0]` equals the climate id of the prefix (`po_` 0, `t_` 1, `n_` 2, `pr_` 3, `d_` 4, `s_` 5);
- `INFO[8]` is 0 and `INFO[9]` is 0;
- the island head's width and height equal the size the link asks for.

Measured exclusions: 15 files contain `PIRAT` in the name, all of them nord, and those same 15 are exactly the files with `INFO[8] ≠ 0`. 21 files end in `DUMMY.scp`, one per climate-and-size group that exists. 0 files have a prefix that disagrees with `INFO[0]`. 2 files are named 80×80 while the head says 64×64 (`D_080x080_0022_2B.scp`, `D_080x080_0023_1B.scp`, `insel.md`); the head wins, so a link that asks for 80×80 does not match them. Eligible total: 429. That is the built-in list of 428 plus the one extra jungle file the disk scan keeps (`annoscene.md` §5). No eligible template is 400×400.

Eligible counts by climate and size, then the median `HEIGHT > 0` cell share and the median `FRUCHTBAR > 0` byte share of that group. Both grids are counted over the template's own width times height. `FRUCHTBAR` is the raw byte grid (`insel.md`). `HEIGHT > 0` is a count of cells, not a proof that the cell is buildable: the value meanings are OQ-024. The generator does not read these grids. They are the pool the fairness floor is calibrated against, because selection draws a template at random from the eligible group.

| climate | size | eligible | height median | fertile median |
|---|---:|---:|---:|---:|
| polar 0 | 64 | 12 | 57.0% | 16.2% |
| polar 0 | 80 | 10 | 35.9% | 7.8% |
| polar 0 | 128 | 4 | 55.8% | 2.1% |
| tundra 1 | 80 | 11 | 35.8% | 30.7% |
| tundra 1 | 128 | 6 | 58.5% | 48.9% |
| tundra 1 | 256 | 5 | 44.3% | 89.2% |
| nord 2 | 64 | 9 | 47.1% | 77.5% |
| nord 2 | 80 | 16 | 38.0% | 31.0% |
| nord 2 | 128 | 69 | 61.9% | 62.9% |
| nord 2 | 256 | 12 | 48.8% | 45.6% |
| praerie 3 | 64 | 16 | 43.9% | 52.9% |
| praerie 3 | 80 | 19 | 41.4% | 57.0% |
| praerie 3 | 128 | 28 | 49.9% | 64.4% |
| dschungel 4 | 64 | 28 | 31.1% | 50.9% |
| dschungel 4 | 80 | 27 | 38.7% | 54.7% |
| dschungel 4 | 128 | 55 | 50.7% | 66.0% |
| dschungel 4 | 256 | 18 | 45.0% | 79.2% |
| steppe 5 | 64 | 16 | 36.7% | 52.3% |
| steppe 5 | 80 | 24 | 42.8% | 56.2% |
| steppe 5 | 128 | 32 | 58.4% | 50.3% |
| steppe 5 | 256 | 12 | 47.1% | 71.8% |

Nord 128×128 has 69 eligible files, which matches the worked example in `annoscene.md` §5. Tundra has no 64×64 template, polar has no 256×256 template, and praerie has no 256×256 template. A size class the generator emits for a climate must have a non-zero eligible count in this table. Climate 6 is never emitted.

Special templates (`INFO[2] ≠ 0`, `annoscene.md` §5 step 9) exist in every group except tundra 80×80. The generator does not pick them. Selection does, from the special count in `LinkInputs`.

## 3. The twelve shipped worlds

Every file has `HEAD.prop0 = 0` (endless). Every island rectangle has x, y, width and height divisible by 8 (`not_multiple_of_8=0`), lies inside the world (`outside=0`), and shares no tile with another (`overlap_pairs=0`). Touching at an edge was not counted separately.

| file | edge | links | fixed INSEL5 | land share | competing | pirate COMP `TYP` |
|---|---:|---:|---:|---:|---:|---|
| `00_1024_easyentry_01` | 1024 | 16 | 4 | 39.4% | 3 | no pirate player |
| `01_1024_easy_256` | 1024 | 27 | 1 (nord 80) | 46.2% | 4 | 2 |
| `02_1024_easy_128` | 1024 | 28 | 1 (nord 80) | 29.6% | 4 | 2 |
| `03_1024_med_256` | 1024 | 27 | 0 | 45.6% | 5 | 3 |
| `04_1024_med_128` | 1024 | 28 | 0 | 29.0% | 5 | 4 |
| `05_800_Med_128` | 800 | 23 | 0 | 37.9% | 4 | 4 |
| `06_1024_heavy_256` | 1024 | 27 | 0 | 45.6% | 5 | 5 |
| `07_1024_heavy_128` | 1024 | 28 | 0 | 29.0% | 5 | 6 |
| `08_800_heavy_128` | 800 | 25 | 0 | 28.4% | 4 | 5 |
| `endless01` | 800 | 0 | 23 | 58.6% | 1 | no pirate player |
| `endless02` | 1024 | 0 | 30 | 71.6% | 2 | 7 (authored party) |
| `endless03` | 800 | 0 | 12 | 39.1% | 5 | 2 and 6 (two parties) |

Land share is the union of link rectangles and `INSEL5` rectangles over the world area. The three addon files are authored islands, not dummy links: `ROHSTOFF` and `SCHATZ` are zero, and island selection returns immediately when the link list is empty (`0x10009562`). `endless02` contains one 400×400 island, which no eligible template can fill, and its `ERZE` kind bytes are 205 with an empty link list (OQ-231). The generator does not imitate the addon files. They are recorded so a later reader does not treat 205, or 400, as a budget.

Description levels, matched by the scenario's `AUFTRAGTEXT` file name, as enums in the bullet order of those files (island count, island size, ore, fertility, pirates, money). `91005` is the file for `04`, despite a heading word that disagrees with the file name; the bullets are used.

| file | islands | size | pirates | human money |
|---|---|---|---|---:|
| `00` | many | large | none | 500000 |
| `01` | many | large | not very aggressive | 50000 |
| `02` | many | normal | not very aggressive | 45000 |
| `03` | many | large | aggressive | 30000 |
| `04` | many | normal | aggressive | 30000 |
| `05` | few | normal | aggressive | 30000 |
| `06` | average | large | aggressive | 25000 |
| `07` | average | normal | aggressive | 20000 |
| `08` | few | normal | aggressive | 20000 |

The addon `htm` files have no such bullet list. They are not used as calibration.

What those words correspond to in the files:

- **Large versus normal, on a 1024 edge, is the size mix.** The three large 1024 presets (`01`, `03`, `06`) place 5×64, 10×80, 8×128, 4×256 links. Dropping `01`'s extra nord 80, the land share is 45.6%. The three normal 1024 presets (`02`, `04`, `07`) place 2×64, 13×80, 13×128 and no 256. Dropping `02`'s extra nord 80, the land share is 29.0%.
- **Many versus average is not a link count and not a land share.** `03` (many, large) and `06` (average, large) are the same 27 links and the same 45.6%. `04` (many, normal) and `07` (average, normal) are the same 28 links and the same 29.0%. The two words differ in `ROHSTOFF`, `ERZE`, `SCHATZ`, the pirate `TYP`, and the players' money.
- **Few is only the two 800-edge presets, and they are not one mix.** `05` is 2×64, 11×80, 10×128 (37.9%). `08` is 8×64, 13×80, 4×128 (28.4%).
- **Small is not in any preset.**
- **Easyentry is its own geometry:** 16 links plus four authored islands (three 256 and one 128), land share 39.4%, and each competing player already has a city. Its description says many and large. The generator does not use it as the large/many reference.

Reference used below: `03` for the large mix, the large land share, the climate bands, and the raw-material and ore budgets; `04` for the normal mix and the normal land share. Both have no authored island, so the numbers are the link list alone. `01`/`06` confirm the large mix and `02`/`07` confirm the normal mix.

### Starts

Every competing player in these twelve files has exactly one ship. The ship's `f32` position (`sze.md`, bytes 8 and 12 of the ship prefix) is a world-tile coordinate, and in the 0010 files it sits on a tile centre (`n + 0.5`). That position is the start. `annoscene.md` §8 reads it as the figure's world position. Easyentry also gives each competing player one city, and the ship lies on that city's island (distance 0). The other eight 0010 presets give competing players no city; the ship is in the water.

The ships are not on one point.

| file | closest pair of starts | farthest pair | bounding box |
|---|---:|---:|---|
| `00` easyentry | 302.93 | 621.36 | 494 × 457 |
| `01` | 39.01 | 286.92 | 281 × 100 |
| `02` | 240.03 | 542.18 | 314 × 526 |
| `03` | 125.92 | 446.66 | 348 × 296 |
| `04` | 236.17 | 488.26 | 287 × 479 |
| `05` | 67.08 | 192.26 | 147 × 151 |
| `06` | 39.01 | 286.92 | 281 × 116 |
| `07` | 57.63 | 618.77 | 270 × 572 |
| `08` | 162.36 | 471.94 | 190 × 432 |
| `endless02` | 428.98 | 428.98 | 325 × 280 |
| `endless03` | 38.90 | 508.67 | 230 × 461 |

Distance is IEEE `hypot` from point to point, in tiles. `endless01` has one competing player, so no pair. This is the measurement behind the owner question in §15: the plan's "together, at one point, as in the original" does not match these coordinates.

Nearest island and nearest rival for each start are in the tool output (`reach` lines). On the eight non-easyentry 0010 presets the nearest island is between 4.50 and 52.62 tiles from the ship. A nearest-start split of the islands (the tool's `share` lines: each island goes to the closest start, tie to the lower player index) does not meet 99%. Examples: on `01` one start receives 1.7% of the land and another 39.3%, against an equal share of 25%; on `03` the shares are 5.6% and 38.4% against 20%. The floor in §12 does not use that split for together mode.

### Climate bands

Bands overlap. They are not stacked stripes. On `03` (1024 edge, the clean large reference) the occupied y range of each climate, in tiles, is:

| climate | y from | y through |
|---|---:|---:|
| polar | 16 | 216 |
| tundra | 88 | 352 |
| nord | 32 | 624 |
| praerie | 120 | 792 |
| steppe | 168 | 960 |
| dschungel | 400 | 984 |

`06` is close (polar 24–200, tundra 72–352, nord 32–632, praerie 160–800, steppe 168–944, dschungel 400–984). The generator uses the `03` ranges. Counts on `03`: polar 3, tundra 3, nord 5, praerie 4, steppe 5, dschungel 7.

### Raw materials, ores, pirates

`ROHSTOFF` budgets on the reference files, kinds in file order: `03` is 3, 3, 3, 5, 6, 0, 4, 4, 4, 4 (sum 36) for kinds 1, 2, 4, 5, 6, 7, 8, 9, 10, 11. `04` is 4, 5, 4, 6, 5, 0, 3, 4, 4, 4 (sum 39). Kind 7 is 0 on every 0010 file. `ERZE` on `03`: kinds 1, 2, 3, 5, 6 are 4, 6, 4, 5, 20 and kind 0 is 0. On `04`: 3, 5, 3, 4, 15 and kind 0 is 0. The 800-edge files do set kind 0 (5, 11 and 9). Special counts are 3 on `03` and `04`. Treasure counts are 12 on both. The resolver already caps treasure placement at the link count (`island_links.hpp`); the generator keeps the count at or below the link count so the original's non-terminating loop (`annoscene.md` §5 step 8) cannot arise.

Each 0010 preset except easyentry has exactly one kind-15 player, with no ship and no city. `COMP` `KAMPFTYP` is 0 on all eight. `COMP` `TYP` is 2 on both "not very aggressive" files and 3, 4, 5 or 6 on the "aggressive" files. `annoscene.md` §11.6 reads `TYP` as a time-interval parameter of the computer class, not as a map field. One Venetian party (kind 16) is present in every one of the twelve files. Native players (kind 14) are absent from all nine 0010 files and five in each addon file. The 0010 natives, where a scenario has them, come from template cities at selection time (`annoscene.md` §5 step 10), and 174 `*.scp` files carry a root `SPIELER` (`insel.md`).

## 4. Inputs

The generator reads:

| input | source | rule |
|---|---|---|
| `n` | `GameSetup::seats.size()` | Competing seats only. T36 rejects an empty seat list. Venetians, pirates, natives and the neutral index 31 are not seats. |
| `base_edge_4p` | `WorldOptions`, default 1280 | `B` in the edge formula. |
| `island_size` | `Small`, `Medium`, `Large` | Size-class weights, §6. |
| `island_quantity` | `Less`, `Default`, `Many` | Scales the link count, §6. |
| `start_mode` | `Together`, `Spread` | §9. |
| `pirate_level` | `None`, `NotVeryAggressive`, `Aggressive` | Whether a pirate party exists, §10. The `TYP` value is not a map field (OQ-230). |
| `seed` | `GameSetup::seed` | The only random input. |
| `generator_version` | `u32`, constant of this spec | Starts at 1. A later change of the algorithm bumps it. It is not a T36 field; the shell stores it next to the setup. |

`start_resources`, `seed` aside, and `rules` do not change the map. Money and the ship cargo are the shell's, from `GameSetup`, and this spec does not decode the ship `WAREN` list.

## 5. Edge

```text
edge(n, B) = (isqrt(B · B · n) + 1) / 2
```

`isqrt` is the integer square root, rounding down. The division is integer. For every `n` from 1 to 8 at `B = 1280` this equals `round(B · sqrt(n / 4))` with halves rounded away from zero, and it avoids a floating square root. `n = 4` returns `B`. `n = 5` at `B = 1280` returns 1431.

The world is square: `width = height = edge`. Both are multiples of 8 because `B` is a multiple of 8 and the formula's results at the supported `B` are multiples of 8; the implementation checks that and refuses the setup if they are not. If `edge > kWorldEdgeBound` (`1 << 14`, `src/sim/include/anno/sim/config.hpp`), setup fails with the reason `edge`. It does not clamp. The product `B · B · n` is computed in a 128-bit integer.

`base_edge_4p` default 1280 is the owner decision D1 (`docs/plan/lan-endless.md`). The original edges 800 and 1024 stay the calibration maps, not the default.

## 6. Link count, size mix, land share

`IslandSize` and `IslandQuantity` are independent. The shipped presets confound them; the generator does not.

Size-class weights, from §3, as integers that sum to the reference link count:

| size | classes (tiles) | weights |
|---|---|---|
| Large | 64, 80, 128, 256 | 5, 10, 8, 4 (sum 27) |
| Medium | 64, 80, 128, 256 | 2, 13, 13, 0 (sum 28) |
| Small | 64, 80, 128, 256 | owner-pending, §15 |

Reference link count `N4` is 27 for large and 28 for medium. The default count at the generated edge keeps the reference land share, because the edge is larger than 1024:

```text
links = div_round(N4 · edge · edge · q_num, 1024 · 1024 · q_den)
```

`div_round(a, b) = (a + b/2) / b` for positive integers. Quantity factors, owner-pending (§15), proposed defaults `q_num/q_den`: less 3/4, default 1/1, many 5/4.

At `B = 1280`, `n = 4`, large, default: `edge = 1280`, `links = div_round(27 · 1280 · 1280, 1024 · 1024) = 42`. The rectangles then cover about 45.6% again. Medium at the same edge gives `div_round(28 · 1280 · 1280, 1024 · 1024) = 44` and about 29.0%.

If `links < 1` or `links` exceeds the attempt budget in §8, setup fails with the reason `islands`. A result above a few hundred is still legal; the bound is the placement search, not a constant in the sim.

### Largest remainder

To split an integer `count` across classes with positive weights `w[i]` summing to `W`:

```text
base[i] = (count · w[i]) / W
left    = count - sum(base)
```

Give one extra to each of the `left` classes with the largest remainder `(count · w[i]) mod W`, ties to the lower index.

Climate counts use this on the `03` weights `(3, 3, 5, 4, 5, 7)` for polar, tundra, nord, praerie, steppe, dschungel. Inside each climate, size counts use the same split on that climate's count and the size weights. A climate that has no eligible template for a class (tundra 64, polar 256, praerie 256) has that class's weight treated as 0 and the weight added to the next smaller class that is eligible, or the next larger if none smaller is eligible. If a climate ends with no eligible class at all, its count moves to nord, which has every class. The generator never emits a `(climate, size)` pair with zero eligible templates (§2).

## 7. Climate bands

The `03` ranges in §3 are fractions of a 1024-edge world. On the generated edge:

```text
y0 = (y0_1024 · edge / 1024) & ~7
y1 = (y1_1024 · edge / 1024) & ~7
```

`y1` is exclusive after this snap: if the snap makes `y1 <= y0`, set `y1 = y0 + 8`. An island of height `h` is placed so its `y` is a multiple of 8 and its vertical span lies inside `[y0, y1)` when `h` allows it. When `h` is larger than the band, `y` is the multiple of 8 closest to the band's midpoint, clamped so the island stays inside the world. Bands overlap, so two climates may legally occupy the same y. The band is a preference for that climate's own links, not a partition.

`x` is uniform across the world, multiple of 8, with `x + w <= edge`.

## 8. Placement

Links are placed in order: climate 0 through 5, and within a climate the larger size first, then the creation index. For each link the candidate positions are the pairs `(x, y)` of multiples of 8 that satisfy the band rule and the world bounds, ordered by `y` then `x`. Let `C` be that count. Draw one integer `k` in `0 .. C-1` from the sim generator (§13) and start at candidate `k`, then try the following candidates in order, wrapping once. A candidate is rejected when its rectangle overlaps an already placed rectangle in tile area. Touching at an edge is allowed. The first accepted candidate is the position.

There is no sea-gap requirement unless the owner sets one (§15). The twelve files have no shared tiles; a minimum gap between rectangles was not measured, and their coordinates are not used as one.

The search examines at most `C` candidates, so it ends. If none fits, setup fails with the reason `overlap` and the climate and size that did not fit. It does not drop the island and continue.

## 9. Starts

One start per seat, in seat order. A start is an integer tile `(x, y)` in world coordinates, the point the shell places that seat's ship on. The generator does not place a city. Easyentry's cities are part of that one scenario, not of the other eight presets.

**Together.** Every seat receives the same tile: the centre `(edge/2, edge/2)`, moved north in steps of one tile until the tile is not inside an island rectangle, and if the centre column is blocked, west in the same way. If no water tile exists, the start stays on the centre island. This is one point, which is what `docs/plan/lan-endless.md` W2 (R15) asks for. It is not what the shipped files do (§3). The conflict is §15 and is not silently resolved past "the generator follows the written decision".

**Spread.** Place seat 0 at the centre by the same rule. Each later seat draws a water tile that is at least `min_dist` from every start already placed and at least 8 tiles from the world border. `min_dist = edge / 4` (integer), owner-pending (§15): 320 tiles at edge 1280. Candidates are the water tiles on an 8-tile lattice. The same draw-and-probe as §8 is used. If a seat cannot be placed, setup fails with the reason `spread`.

Both modes then run the fairness checks of §12. A miss fails setup with the reason `fairness` and the measure name. Failing is the point of the floor: the shell shows the reason and the player changes the seed or the options. The generator does not quietly reshuffle.

## 10. Parties and link inputs

The generator's output does not list natives. Template selection creates them from the chosen `*.scp` (`annoscene.md` §5 step 10). The nine 0010 presets have zero kind-14 players; the addon files have five each, authored, and are not a rate to copy. "Natives scale with the islands" (`PLAN.md` §0.6) is already what selection does, because more links draw more templates and a fixed share of templates carry a native city (174 of the `*.scp` files, `insel.md`). No extra native party is inserted.

Exactly one Venetian party is requested in the output, with no ship and no city. The shell creates that party. The easy 1024 presets also contain a Venetian city; the med and heavy presets do not. The generator follows the med/heavy files.

Pirate party: none when `PirateLevel` is `None`. Otherwise exactly one kind-15 party, no ship and no city, which is the 0010 pattern. `PirateLevel` does not pick a `COMP TYP`. The eight presets that have a pirate use five different `TYP` values for two description levels (OQ-230). Aggressiveness is an AI parameter, owned by W7, not by the map.

`LinkInputs` for the whole list, scaled from `03` by the largest-remainder split of §6 with weights equal to those budgets and `count = links`:

| field | `03` on 27 links |
|---|---|
| raw kinds 1, 2, 4, 5, 6, 7, 8, 9, 10, 11 | 3, 3, 3, 5, 6, 0, 4, 4, 4, 4 |
| ore kinds 0, 1, 2, 3, 5, 6 | 0, 4, 6, 4, 5, 20 |
| special count | 3 |
| treasures | 12, then clamped to `links` |

Kind 7 stays 0 because its weight is 0. Kind 0 stays 0 for the same reason; the 800-edge files that set it are not the reference. The per-island marks are not an output. One `DrawSource` is filled with these totals, in kind order, and `resolve_island_links` consumes it. The generator does not draw per island itself.

## 11. Output

```text
struct GeneratedWorld {
    u32 generator_version;          // 1
    u32 edge;
    vector<IslandLinkSpec> links;   // path empty, climate and size set
    LinkInputs inputs;              // the totals in §10
    vector<Start> starts;          // one per seat, seat order
    bool venetian;                  // true
    bool pirate;                    // PirateLevel != None
};
```

`IslandLinkSpec` and `LinkInputs` are the types `resolve_island_links` already takes (`island_links.hpp`): `number`, `x`, `y`, `w`, `h`, `climate`, empty `path`, plus the raw, ore, special and treasure totals. `number` is the link's index in this list, starting at 0. An empty path is what makes selection run; a non-empty path would place a named island and skip the draw (`annoscene.md` §5 step 2).

`Start` is `{u32 seat; i32 x; i32 y;}`. It is not a field of `IslandLinkSpec`. The shell places one ship per start at `(x + 0.5, y + 0.5)` in the fixed-point position the sim uses for a tile centre, with the figure id taken from the start-resources entry. This spec does not choose that figure id (1502 on the eight ordinary 0010 presets, 1503 for easyentry's human).

The world hash is the sim's hash of the loaded world after `resolve_island_links` and before tick 0. The handshake also carries `generator_version`, `seed`, and the canonical encoding of the `WorldOptions` and the seat count, so two peers reject a session whose generator or options differ before they compare the hash.

## 12. Fairness floor

The floor is the owner decision of 2026-10-22 (`PLAN.md` M4.1, OQ-100) for the later generator: every measure is at least 99% of that player's reference, compared in integers. It is not the acceptance rule for the fixed-map ship starts. Those candidates allow natural differences and do not assign islands (`docs/sim/fixed-map.md` §11, owner 2026-10-31). The formula and the overall supply stay. Equal holdings after players choose where to settle are not promised.

```text
measure · 100 >= reference · 99
```

The reference is the 4-player world generated with the same options and the same seed, and the per-player reference is that world's total divided by 4, integer division. For `n = 4` the world is its own reference and the check is that the split is even. Players are seats only.

Measures, each per seat:

| measure | together | spread |
|---|---|---|
| Land area by climate and size class | the whole map, because every seat shares the start and can sail to every island | the islands in that seat's nearest-start cell (tie: lower seat index) |
| Raw-material kinds and ore kinds admitted by those islands' climates | the climates present anywhere on the map | the climates present in the cell |

"Admitted" uses the masks in `annoscene.md` §5 (`0x10009010`, `0x100090b0`), not a placed deposit. The file does not store the per-island marks (they are drawn at game start), so a deposit count is not a measure that can be read back from a shipped scenario, and the generator does not promise one. The climate masks are what the draw is allowed to place.

Land area is the link rectangle. It is what the generator controls. Template `HEIGHT` and `FRUCHTBAR` vary inside a climate and size (§2) and are not part of the floor.

Nearest-island distance is not a 99% floor. A closer island is the better outcome, and the shipped ships sit 4 to 53 tiles off an island. The check is an upper bound: together, the shared start is at most 64 tiles from some island, or the start lies on an island; spread, each start is at most `edge / 8` tiles from some island. A miss fails with `fairness/distance`. The bound is owner-pending in the same way as the spread distance (§15).

Because together uses the whole map, a constant land share (the count scales with area, §6) meets the land floor for every `n`. Spread is where a seat can miss a climate, and the failure is a setup failure rather than a clamped map.

## 13. Determinism

- Arithmetic is integer only, including the edge, the counts, the bands and the distances used for spread and for the floor. The measurement tool's IEEE `hypot` is a report of the files; the generator uses squared integer distance.
- The only random source is a `DrawSource` (`anno/sim/draw.hpp`) seeded with `GameSetup::seed`, owned by this generation, drawn in the order §8 and §9 state. No clock, no unordered container, no pointer-order walk.
- `generator_version` is mixed into the world hash together with the seed and the options. Version 1 is this document.
- The same version, seed, options and seat count produce the same links, the same starts and the same hash on every platform.

## 14. Test plan

Property tests, no original files required. They construct a `GameSetup` and call the generator.

- For every `n` from 1 to 6, every size, every quantity, both start modes, and every pirate level: 64 seeds. Each seed either returns a world or a named failure (`edge`, `islands`, `overlap`, `spread`, `fairness/<measure>`).
- A returned world has a square edge equal to §5, every link on a multiple of 8, no tile overlap, every `(climate, size)` eligible in §2, treasure count at most the link count, and the fairness comparisons of §12 holding.
- The world hash matches a second run in-process, and a fixture of one seed per option combination is compared on Windows and Linux.
- `resolve_island_links` on the output returns one island per link and consumes the `LinkInputs` totals.
- Versions: a test pins generator version 1 to a known hash for one fixed setup. Changing the version constant changes the hash.

The fairness tool `world_fairness` (W2 step 3, not this task) reads a generated world and prints the §12 measures. `tools/world_measure.py` stays the reader of the original files.

## 15. Owner questions

Each of these changes how a game feels. The generator uses the proposed value as the configuration default until the owner decides. Nothing here is decided.

1. **Together, against the files.** The plan says one point. The twelve files put competing ships 39 to 621 tiles apart (§3). Proposal: keep one shared tile, as the plan says, and treat the file scatter as the original's authored scenarios rather than as the generator's together mode.
2. **Spread minimum distance.** Proposal: `edge / 4` (320 tiles at the default 4-player edge).
3. **Small island mix.** No preset uses it. Proposal: weights 16, 12, 0, 0 for 64, 80, 128, 256, so a small world is 64 and 80 only, which are the two classes every climate has templates for except tundra's missing 64.
4. **Quantity factors.** Proposal: less 3/4, many 5/4, of the default link count. Default stays the reference land share, 45.6% large and 29.0% medium.
5. **Sea gap.** Proposal: none. Islands may touch at an edge, as §8 already allows.
6. **Distance bound in the floor.** Proposal: 64 tiles for together, `edge / 8` for spread.
7. **Pirate `TYP`.** Not chosen here. See OQ-230. The generator only decides that the party exists.

## 16. Out of scope

Money, ship cargo, victory limits, computer-player `TYP`, and the behaviour of a pirate party are not outputs of this generator. The addon worlds are not targets. Checkpoint A's developer warehouse is unchanged and is not a start the generator emits.
