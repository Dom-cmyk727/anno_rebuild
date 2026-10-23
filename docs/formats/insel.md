# Islands (`INSEL5`, `INSEL4`) in `.scp` and `.sze`

**Status:** partial — the island head, the record sequence and the terrain grids (`HEIGHT`, `GFXID`, `FRUCHTBAR`) are complete and re-encode byte-identically (P); `GFXID` decodes to ground cells of `Textures/Felder.tex` (P + visual); the content of most other records is open (OQ-024). Read by the sim adapter (S14 stage 1, `src/sim/src/scenario_formats.cpp`, `docs/sim/state.md` §10): the head, `INFO[0]`, the three grids and the placed items; since stage 2 also the island templates of `Scenes/Islands` (their size and `INFO` bytes 0, 2, 4, 5, 8, 9 as the link selection's candidates, and the chosen ones as islands).

## Files covered
- Island templates `Scenes/Islands/<Klima>/*.scp`: 465 files, one island each (444 `INSEL5`, 21 `INSEL4`) (P).
- Scenarios `Scenes/**/*.sze`: 50 files, 668 `INSEL5` islands under `SCENE` (P).
- Built on: [`chunk-container.md`](chunk-container.md). The `.scp` 60-byte trailer (OQ-001) and the other scenario chunks are not covered here.
- This spec replaces the `scp.md` named in the plan: the island layout is the same in both file types.

All integers little-endian. Offsets of record payloads.

## Identification and placement
- `.scp` with `INSEL5`: root `ANNO` → `WARETYPE`, `INSEL5`, optional `SPIELER` (174). The 60-byte trailer follows the root (OQ-001).
- `.scp` with `INSEL4` (the 21 `*_DUMMY.scp`): root `ANNO` → `INSEL4`, then the island records **as root siblings** (`INFO`, `HEIGHT`…, `STAEDTE`, `LOCKED`).
- `.sze`: root `ANNO` or `ANNOADDON1` → `SCENE` → … `INSEL5` × n (0–36 per scenario).
- `INSEL5` is a leaf for generic container detection because of its 12-byte head (OQ-013); the rest of its payload is an exact chunk sequence (P).

## Head

`INSEL5` (12 bytes, then records):

| Offset | Size | Type | Name | Meaning | Evidence |
|---|---|---|---|---|---|
| 0x00 | 2 | u16 | `x` | world position of the island's top-left tile; 0 in all templates | P (scenarios: values such as 112, 544; templates: 0) |
| 0x02 | 2 | u16 | `y` | | P |
| 0x04 | 2 | u16 | `width` | island width in tiles; the `HEIGHT`/`GFXID` blocks tile exactly `width × height` | P (all 1,112 `INSEL5`) |
| 0x06 | 2 | u16 | `height` | | P |
| 0x08 | 4 | u32 | `number` | island number in a scenario (unique per scenario); 0 in templates | P |

`INSEL4` payload (8 bytes): `u16 width`, `u16 height`, `u32` (0 in all 21) (P).

The size in the template file name (`Po_064x064_…`) is not authoritative: `D_080x080_0022_2B.scp` and `D_080x080_0023_1B.scp` are 64 × 64 islands (P).

## Records
Order in templates: `INFO, HEIGHT…, GFXID…, GELAENDE…, FRUCHTBAR, [FLUESSE…], BERGWERK2, OCCUR, ANBAU, [DISCOVER, KONTORID, PROD2…, DUERRE]`. Scenarios add `LOCKED`, `NAME`, `HIRSCH`, `EVENTINSEL` (P, `insel_corpus` / `anno-dump insel`).

| Record | Layout | Evidence |
|---|---|---|
| `INFO` | 12 bytes; ? OQ-024 | P (size) |
| `HEIGHT` | block (below), 1 byte per tile; values 0–208 (111 distinct); 0 on 97.9 % of border tiles | P |
| `GFXID` | block, `u16` per tile; ground cell, see "Ground" | P + visual |
| `GELAENDE`, `PROD2` | block header + `n` × 6-byte items, same 32 × 32 block grid as `HEIGHT`; items place objects, see "Placed objects". `GELAENDE` = scenery (grass, bushes, trees, rocks, cliffs, ice floes), `PROD2` = built objects (roads, houses, walls, production) | P |
| `FLUESSE` | block header with 8 × 8 blocks at multiples of 8, only non-empty blocks stored, + 6-byte placed-object items | P; what the river objects are ? OQ-024 |
| `FRUCHTBAR` | `width × height` bytes, row-major, no blocks; values 0–10 | P (size asserted) |
| `BERGWERK2`, `OCCUR`, `ANBAU`, `DISCOVER` (4), `KONTORID` (2), `DUERRE`, `LOCKED` (1), `NAME`, `HIRSCH`, `EVENTINSEL`, `STAEDTE` | ? OQ-024 | P (sizes) |

### Blocks
`HEIGHT`, `GFXID`, `GELAENDE`, `PROD2`, `FLUESSE` start with a 6-byte block header:

| Offset | Size | Type | Name | Meaning | Evidence |
|---|---|---|---|---|---|
| 0x00 | 2 | u16 | `x` | tile column of the block's top-left corner | P |
| 0x02 | 2 | u16 | `y` | tile row | P |
| 0x04 | 1 | u8 | `width` | 32, or less at the right edge (16 for 80-wide islands) | P |
| 0x05 | 1 | u8 | `height` | 32, or less at the bottom edge | P |
| 0x06 | … | | data | `HEIGHT`: `width × height` bytes; `GFXID`: `width × height` × `u16`; both row-major within the block | P |

`HEIGHT` and `GFXID` blocks start at multiples of 32, are stored row of blocks by row of blocks (left to right, top to bottom), sit at the same positions in both records, and cover every tile exactly once. Re-encoding the assembled grids with these rules reproduces every `HEIGHT` and `GFXID` payload byte for byte (P, all 1,133 islands). `GELAENDE` and `PROD2` blocks use the same grid order (P).

## Ground
A `GFXID` value selects one cell of a **6 × 6** grid (cells of 256/6 ≈ 42.67 px) over a 256 × 256 ground texture in `Textures/Felder.tex` ([`tex.md`](tex.md); the textures are listed in the build script `Data/Felder.dat`):

| Bits | Name | Meaning | Evidence |
|---|---|---|---|
| 15–8 | `texture` | `Felder.tex` `TEXTURE` id (11–157); 0 = open water, which has no ground texture | P: 13,402,214 tiles name a `Felder.tex` texture, 3,804,534 are 0, 36 name 205 (below); 141 of the 147 textures are used |
| 7–5 | `u` | cell column: texture `x` from `u/6` to `(u+1)/6` of the width; 0–5 (6 and 7 on 230 of 13.4 M tiles; clamped) | D: `AnnoWelt.dll` UV table `0x100DCF44` = [0, 1/6, …, 1], indexed `u` and `u+1` for the tile's corners (`docs/re/annowelt.md`); P: value range; seam error between neighbouring tiles is less than half that of 1/8 cells (Eskimo 34.2 vs 80.2, Piraten 37.8 vs 85.2, jungle 20.2 vs 55.0) |
| 4–2 | `v` | cell row, as `u` | D, P |
| 1–0 | `r` | quarter turns: tile position `(s, t)` ∈ [0, 1)² (`s` along tile x, `t` along tile y) shows cell position `T^r(s, t)` with `T(s, t) = (1 − t, s)`; corner (x, y) of the tile is cell corner (0, 0) for `r` = 0 | D: `AnnoWelt.dll` per-corner UV functions `0x1007b030`…`0x1007b270`; P: lowest seam error of all 16 axis/rotation variants |

Correction 2026-10-16: an earlier version of this spec gave 32 × 32 px cells (8 × 8 per texture), chosen by comparing axis and rotation variants that all used the wrong cell size; static analysis and the seam measurement above show 6 × 6.

Visual check 2026-10-16 (`anno-extract insel`, top-down, `HEIGHT` not applied): `Po_064x064_0008_Eskimo.scp` shows snow fields, rocky shores and a shallow-water ring; the 256 × 256 main island of `0200Single/Single_01.sze` shows grassland, sand, rock ridges, lagoons and a continuous coast. Water tiles (texture 0) form the open sea around the shallow-water ring. The original draws the sea as animated sprites over the sea floor, not as ground ([`docs/render/iso.md`](../render/iso.md), "Water").

One 3 × 3 patch names texture 205, which `Felder.tex` does not contain: in `Nord/N_256x256_0015_5B.scp` and the three scenarios that contain that island (`endless01.sze`, `00_1024_easyentry_01.sze`, `S01_12_01.sze`) (P, `ground_textures`).

Rejected before this was found (P, 2026-10-16): `GFXID >> 5` as a `FigurenHeader.dat` id, `GFXID` or `GFXID >> 5` as an `Anlagen.dat` `ANLAGEN` id, and `GFXID` as an index into `ANLAGEN` sprite-set frame ranges.

## Placed objects
Each 6-byte item of `GELAENDE`, `PROD2` and `FLUESSE` places one `Anlagen.dat` object ([`anlagen.md`](anlagen.md)):

| Offset | Size | Type | Name | Meaning | Evidence |
|---|---|---|---|---|---|
| 0x00 | 2 | u16 | `id` | bits 0–11: `ANLAGEN` entry id; bits 12–13: 0; bits 14–15: `rotation` (quarter turns) | P: all 4,585,381 items name an existing `ANLAGEN` id; bits 12–13 always 0; the four rotation values all occur |
| 0x02 | 2 | u16 | `state` | `height << 8 \| phase`: high byte = the object's height in `HEIGHT` units (drawn at `height · 0.04` world units), low 6 bits = animation phase, bits 6–7 = 0. `0x3000` = height 48, phase 0 | D: `AnnoWelt.dll` loader `0x1007aa40`, saver `0x1007ac40`, drawing `0x1009f870`; P: height byte = terrain height at the top-left tile for 90 % of `GELAENDE`, 97 % of `PROD2`, 25 % of `FLUESSE` items (water objects keep their own level); bits 6–7 always 0 |
| 0x04 | 1 | u8 | `x` | footprint's top-left tile, relative to the block origin; always inside the block | P |
| 0x05 | 1 | u8 | `y` | | P |

Footprint: `ANLAGEN` `HEAD` bytes 0–1 (width, height), swapped for odd rotations. Evidence: after removing exact duplicate items, `PROD2` footprints overlap on 115 tiles with the swap and on 1,350 without it (P, `objects_corpus`); a top-down overlay of the 3,947 `PROD2` objects of `single32.sze` island 17 shows a walled town with a road grid, dense centre and farms, all on land (visual, 2026-10-16). `GELAENDE` footprints overlap on many tiles (scenery such as ice floes and cliffs), which is plausibly intended.

Items: `GELAENDE` 4,475,412, `PROD2` 76,863, `FLUESSE` 33,106 (P). Exact duplicate items occur in the same block, mostly back to back: 97,041 in `GELAENDE`, 485 in `PROD2` (P); whether the engine places them twice is open (OQ-024).

## Reference dump
`anno-dump insel [--full] <file>…` and `tools/insel.py dump [--full]` print, after `# <path>`:

```
ISLANDS count=<n>
ISLAND i=<k> version=<4|5> x= y= width= height= number= head=<hex> records=<NAME,NAME,…>
  RECORD <NAME> size=<n> hash=<fnv64>             (every record that is not a block record)
  GRID HEIGHT min=<u8> max=<u8> hash=<fnv64>      (hash over the row-major grid)
  GRID GFXID distinct=<n> hash=<fnv64>            (hash over the grid as u16 LE)
  GROUND water=<n> textures=<id,id,…>             (tiles with texture 0; sorted distinct texture ids ≠ 0)
    H<y> <hex row>   /   G<y> <4 hex digits per tile>          (--full)
  GRID BAD <reason>                               (instead of the two GRID lines)
  BLOCKS <GELAENDE|PROD2|FLUESSE> count=<n> items=<n> hash=<fnv64>   (if present; hash over headers + data)
```
`NOPARSE` if the file has no island root (exit code 1). `<reason>` is the bare check name, without offsets.

## Implementation
- `anno_formats::insel` (`islands`, `Island::{from_insel5, from_insel4, blocks, heights, gfx_ids, fertility, record}`, `Block`, `encode_blocks`, `GroundCell`).
- Top-down ground image: `anno_assets::island::ground_top_down`; `anno-extract insel <file> <Felder.tex> <outdir> [<px per tile>]` → `island<i>.png` + `index.txt` (`island<i>.png x= y= width= height= number=`).
- Python reference: `tools/insel.py` (independent; uses `tools/chunks.py` for the container).

## Open fields
- Open-water rendering, texture 205, `HEIGHT` units, `INFO`, item `state`, duplicate items, all other records — OQ-024.
- `.scp` trailer — OQ-001.

## Tests
- Corpus: `crates/formats/tests/insel_corpus.rs` (needs `ANNO1503_GAME_DIR`): 465 templates (21 `INSEL4`, 444 `INSEL5`) and 668 scenario islands; grids assemble, re-encode byte-identically, `FRUCHTBAR` is `width × height`, sparse blocks are 6-byte items on their grid, template heads are 0, island numbers unique per scenario, the two misnamed templates, 7,952,640 template tiles.
- Placed objects: `objects_corpus.rs` (item counts, ids, duplicates, footprint overlaps); unit `insel::tests::placed_objects`.
- Ground: `insel_corpus.rs::ground_textures` (texture ids, water count, the texture-205 patch).
- Unit: `insel::tests::{insel5_grids, rejects_gaps_and_overlaps, ground_cells, islands_in_scp_and_insel4}`, `anno_assets::island::tests::renders_cells_water_and_missing`.
- PNG export: `extract.rs::extract_insel_corpus_samples` (PNGs equal `ground_top_down`; `MISSING` only in the texture-205 template).
- Reference output: `agree_python.rs::{anno_dump_matches_insel_py_synthetic (CI), anno_dump_matches_insel_py_corpus}` — all 515 files, `--full` on `Single_01.sze`.
- Visual check 2026-10-16: the `HEIGHT` grid of `Nord/N_128x128_0001_4B_Piraten.scp` rendered as an image shows a coherent island (two mountain ridges, no seams at block borders); the `GFXID` grid has the same outline.
