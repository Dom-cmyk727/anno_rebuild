# Islands (`INSEL5`, `INSEL4`) in `.scp` and `.sze`

**Status:** partial — the island head, the record sequence and the terrain grids (`HEIGHT`, `GFXID`, `FRUCHTBAR`) are complete and re-encode byte-identically (P); `GFXID` decodes to ground cells of `Textures/Felder.tex` (P + visual); the content of most other records is open (OQ-024).

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
| `GELAENDE`, `PROD2` | block header + `n` × 6-byte items (sparse), same 32 × 32 block grid as `HEIGHT` | P (all lengths 6 + 6n); item layout ? OQ-024 |
| `FLUESSE` | block header with 8 × 8 blocks at multiples of 8, only non-empty blocks stored, + 6-byte items | P; ? OQ-024 |
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
A `GFXID` value selects a 32 × 32 pixel cell of a 256 × 256 ground texture in `Textures/Felder.tex` ([`tex.md`](tex.md); the textures are listed in the build script `Data/Felder.dat`):

| Bits | Name | Meaning | Evidence |
|---|---|---|---|
| 15–8 | `texture` | `Felder.tex` `TEXTURE` id (11–157); 0 = open water, which has no ground texture | P: 13,402,214 tiles name a `Felder.tex` texture, 3,804,534 are 0, 36 name 205 (below); 141 of the 147 textures are used |
| 7–5 | `u` | cell column, texture pixels `32 u … 32 u + 31` | P: horizontally adjacent tiles of one texture differ by ±32 or ±4 in the low byte; visual |
| 4–2 | `v` | cell row | P, visual |
| 1–0 | `r` | quarter turns: the pixel shown at cell position `(x, y)` is the texture pixel at `(32 u, 32 v) + T^r(x, y)` with `T(x, y) = (31 − y, x)` | visual: with `T` coasts and snow fields blend seamlessly; without rotation, or with the inverse turn, block seams appear (`Po_064x064_0008_Eskimo.scp`) |

Visual check 2026-10-16 (`anno-extract insel`, top-down, `HEIGHT` not applied): `Po_064x064_0008_Eskimo.scp` shows snow fields, rocky shores and a shallow-water ring; the 256 × 256 main island of `0200Single/Single_01.sze` shows grassland, sand, rock ridges, lagoons and a continuous coast. Water tiles (texture 0) form the open sea around the shallow-water ring; the cell bits of water tiles still step like ground (value 0 = cell (0, 0) on an 8-tile grid), so the sea is probably drawn from a water texture or effect outside `Felder.tex` (OQ-024).

One 3 × 3 patch names texture 205, which `Felder.tex` does not contain: in `Nord/N_256x256_0015_5B.scp` and the three scenarios that contain that island (`endless01.sze`, `00_1024_easyentry_01.sze`, `S01_12_01.sze`) (P, `ground_textures`).

Rejected before this was found (P, 2026-10-16): `GFXID >> 5` as a `FigurenHeader.dat` id, `GFXID` or `GFXID >> 5` as an `Anlagen.dat` `ANLAGEN` id, and `GFXID` as an index into `ANLAGEN` sprite-set frame ranges.

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
- Open-water rendering, texture 205, `HEIGHT` units, `INFO`, item layouts of `GELAENDE`/`PROD2`/`FLUESSE`, all other records — OQ-024.
- `.scp` trailer — OQ-001.

## Tests
- Corpus: `crates/formats/tests/insel_corpus.rs` (needs `ANNO1503_GAME_DIR`): 465 templates (21 `INSEL4`, 444 `INSEL5`) and 668 scenario islands; grids assemble, re-encode byte-identically, `FRUCHTBAR` is `width × height`, sparse blocks are 6-byte items on their grid, template heads are 0, island numbers unique per scenario, the two misnamed templates, 7,952,640 template tiles.
- Ground: `insel_corpus.rs::ground_textures` (texture ids, water count, the texture-205 patch).
- Unit: `insel::tests::{insel5_grids, rejects_gaps_and_overlaps, ground_cells, islands_in_scp_and_insel4}`, `anno_assets::island::tests::renders_cells_water_and_missing`.
- PNG export: `extract.rs::extract_insel_corpus_samples` (PNGs equal `ground_top_down`; `MISSING` only in the texture-205 template).
- Reference output: `agree_python.rs::{anno_dump_matches_insel_py_synthetic (CI), anno_dump_matches_insel_py_corpus}` — all 515 files, `--full` on `Single_01.sze`.
- Visual check 2026-10-16: the `HEIGHT` grid of `Nord/N_128x128_0001_4B_Piraten.scp` rendered as an image shows a coherent island (two mountain ridges, no seams at block borders); the `GFXID` grid has the same outline.
