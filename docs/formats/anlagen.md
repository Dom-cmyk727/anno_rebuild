# Buildings and objects (`Anlagen.dat`)

**Status:** partial — the file parses completely: colour tables, 921 `ANLAGEN` entries (byte-identical re-encoding), sprite sets and three pixel volumes; every frame decodes and renders (P + visual). What the gameplay records mean is decoded for the simulation's type registry (`src/sim/src/types.cpp`: `HEAD` bytes, `PRODTYPE`, `FORSCHTYPE`, `WARE`, `ROHSTOFF`, `PROD3`; see the table below and `docs/re/production.md` §2); the masks and the remaining `HEAD` bytes are not decoded yet.

## Files covered
- `Anlagen.dat` (205,470,957 bytes), one file.
- Built on: [`chunk-container.md`](chunk-container.md), colour tables [`palette.md`](palette.md), entry/sprite/frame layout [`figuren.md`](figuren.md).

All integers little-endian.

## Layout

```
ANNO
  COLORS      440 ENTRY: global colour tables (palette.md)
  ANLAGEN     921 ENTRY: building / object types
  TEXTURES    zoom group 0 pixel volume
  TEXTURES    zoom group 1
  TEXTURES    zoom group 2
```
(P, `anno-dump chunks`, `anlagen_corpus`.)

### `ANLAGEN/ENTRY`
The `FigurenHeader.dat` entry layout ([`figuren.md`](figuren.md)): 8-byte prefix (`u32 flag` = 0, `u32 id`), then records. Ids 1–3998, ascending (P).

| Record | Count | Notes | Evidence |
|---|---|---|---|
| `VERSION2`, `HEAD`, `PRODTYPE`, `PROD3`, `WARE`, `ROHSTOFF`, `WALKMASK`, `SHOTMASK`, `OWNMASK`, `HAEUSER` | 921 each | `HEAD` bytes 0–1 = footprint width, height in tiles (grass/trees 1 × 1, palace 1380 4 × 7, castle 2223 8 × 8; confirmed by non-overlapping `PROD2` placements, [`insel.md`](insel.md)); `HEAD` is 36, 44, 48, 52 or 56 bytes (the reader zero-fills a 56-byte buffer); the bytes the simulation decodes: `[3..6]` build cost in bricks / wood / tools / marble, u16 `[8..9]` gold cost, `[0x0a]` rubble pattern, `[0x10]` stage count, `[0x11]` action radius, `[0x18]` shift, `[0x19]` fertility, u16 `[0x1c]` type flag word, u16 `[0x2c]` maximum hit points, `[0x31]` can burn (D, [`docs/re/production.md`](../re/production.md) §2.4); `PRODTYPE`, `FORSCHTYPE`, `WARE` and `ROHSTOFF` are names looked up in the name tables of `AnnoGame.dll` (PRODTYPE number 0–416, research id, goods number 0–0x8e; `UNUSED` = 0; `docs/sim/state.md` Appendices A–C); the rest of `HEAD` and the other records ? (M1.4) | P (counts), P + visual (footprint) |
| `COLORS` | 891 | **list of `u32` colour-table ids** (1–3 ids, 4/8/12 bytes), not inline tables; ids are 1–440 = `ANNO/COLORS` table ids, or 0 = unused slot | P: 640 × 1, 195 × 2, 56 × 3 ids; no frame selects a 0 |
| `INFRAMIN` 822, `OBJECTPOS` 426, `BAUSAMPLE` 409, `ENTRANCE` 326, `FIREPOS` 26, `FORSCHTYPE` 20, `HIGHMAP` 16 | | ? (M1.4) | P (counts) |

`HAEUSER`: 1,031 sprite sets in the figure layout (16-byte prefix, `BODY2`, `ANIM`, `GFXDATA`, `TEXTURES` × 3, optional `TITLE`), 110 of them with a `SHADOW` (P). The 30 entries without `COLORS` have no frames (P).

Frame colour table: `ANNO/COLORS` table with id `COLORS[frame.palette]` of the frame's entry; `frame.palette` is always a valid index into the list and never selects a 0 (P, asserted). Visual: the palace (1380) uses `SchlossHauptteil1.pcx`, the castle (2223) `Militär\GrosseBurg01.pcx`, and both render in correct colours.

### `TEXTURES` (pixel volumes)

| Offset | Size | Type | Name | Meaning | Evidence |
|---|---|---|---|---|---|
| 0x00 | 4 | u32 | `group` | 0, 1, 2 | P |
| 0x04 | 4 | u32 | `frames` | 9,633 = frame headers of that group in `ANLAGEN` (sprites and shadows) | P (asserted) |
| 0x08 | … | | data | frame pixel data; frame `offset` counts from here | P: distinct frame ranges tile the data exactly (6,484 / 6,452 / 6,473 ranges) |

## Frames
Same frame headers and RLE as figures ([`figuren.md`](figuren.md)). Format counts over all 28,899 frames: `BSH_RGBAV` 7,470, `BSH_RGBZ` 7,212, `BSH_RGBAZ` 6,669, `BSH_RGBWZ` 3,528, `BSH_RGBV` 1,899, `BSH_ALPHA` 981, `BSH_RGBZM` 912, `BSH_RGBAH` 120, `BSH_RGBH` 108 (P). Three formats occur only here:

| Format | Bytes per pixel | Count byte | Evidence |
|---|---|---|---|
| `BSH_RGBWZ` | 2, `[z, index]`, **every second column**: stored pixel `x` of row `y` lands on column `2x + (y & 1)` (50 % checkerboard) | count | D: `AnnoDisplay.dll` `0x100161c0` (`tBrushRasterDepth*`); P: stored rows end at ≈ width / 2 in all 2,984 ranges; visual: river piece (2702, `Fluss01.PCX`) |
| `BSH_RGBZM` | 2, `[z, index]` | count | P; visual: cliff (1591) |
| `BSH_RGBAH` | 1 | `level << 6 \| count` | P: all 120 decode only this way |

Pair order `[z, index]`: `z` is a signed per-pixel depth offset (D: `AnnoDisplay.dll` `0x100178d0`). `M` = "melt": pixels at nearly equal depth to what is already drawn blend 50 % (D: `0x10017db0`). That `W` means water is a guess.

**Rows past the width.** 51 distinct ranges (`BSH_RGBAZ` 26, `BSH_RGBZ` 19, `BSH_RGBZM` 6) contain rows whose runs end 1–16 pixels beyond `width`, and 398 `BSH_RGBWZ` ranges end 1–2 columns beyond it after the column expansion (110 by 1, 288 by 2); row count and end marker are exact (P). The original never consults the frame width and draws those pixels to the right of the frame box, clipped only by the viewport (D: `AnnoDisplay.dll` blitters, OQ-025 resolved). `figuren::decode_frame_extended` keeps them by widening the decoded image (rows and anchor unchanged), and the renderer draws them (`docs/render/iso.md`, "Fidelity"); `decode_frame_clipped` drops them. Figures have no such frames.

## Reference dump
`anno-dump anlagen [--full] <Anlagen.dat>` and `tools/figuren.py anlagen [--full]`:

```
ANLAGEN entries=<n> volumes=<n>
VOLUME group=<g> frames=<u32> bytes=<data length after the 8-byte prefix>
ENTRY …                               (exactly as `anno-dump figuren`, figuren.md)
  COLORIDS ids=<id,id,…>              (after the RECORD lines, if COLORS is an id list)
```
`NOPARSE` if `ANNO/ANLAGEN` is missing or malformed.

## Implementation
- `anno_formats::anlagen::AnlagenFile` (`colors`, `entries`, `entry_chunks`, `volumes`, `footprint`); entries are `figuren::Entry` with `palette_ids`.
- `anno_formats::figuren::{FrameFormat::{RgbWZ, RgbZM, RgbAH}, decode_frame_clipped}`.
- RGBA: `anno_assets::building::to_rgba`; PNG export: `anno-extract anlagen <Anlagen.dat> <outdir> <entry id>…` (layout as `anno-extract figuren`).
- Python reference: `tools/figuren.py anlagen`.

## Tests
- Corpus: `crates/formats/tests/anlagen_corpus.rs` — entry count, ids, byte-identical entry re-encoding, `COLORS` id lists and frame references, 1,031 sets / 110 shadows, 3 × 9,633 frames, format counts, volume tiling, decode of every range (51 with clipping).
- Unit: `figuren::tests::clipped_decode_drops_overrun`.
- Reference output: `agree_python.rs::{anno_dump_matches_anlagen_py_synthetic (CI), anno_dump_matches_anlagen_py_corpus}`.
- PNG export: `extract.rs::extract_anlagen_corpus_samples` (entries 1380, 2223, 1591, 2702).
- Visual check 2026-10-16: palace (1380, `Z`), large castle (2223, `AZ`), cliff (1591, `ZM`), river piece (2702, `WZ`) render correctly.
