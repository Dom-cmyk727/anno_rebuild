# Buildings and objects (`Anlagen.dat`)

**Status:** partial — the file parses completely: colour tables, 921 `ANLAGEN` entries (byte-identical re-encoding), sprite sets and three pixel volumes; every frame decodes and renders (P + visual). What the gameplay records mean (`HEAD`, `PRODTYPE`, `PROD3`, masks, …) is not decoded yet.

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
| `VERSION2`, `HEAD`, `PRODTYPE`, `PROD3`, `WARE`, `ROHSTOFF`, `WALKMASK`, `SHOTMASK`, `OWNMASK`, `HAEUSER` | 921 each | `HEAD` 52 bytes on the first entries; contents ? (M1.4) | P (counts) |
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
| `BSH_RGBWZ` | 2, `[z, index]` | count | P: all 2,984 distinct ranges decode only this way; visual: river piece (2702, `Fluss01.PCX`) |
| `BSH_RGBZM` | 2, `[z, index]` | count | P; visual: cliff (1591) |
| `BSH_RGBAH` | 1 | `level << 6 \| count` | P: all 120 decode only this way |

Pair order `[z, index]` for `WZ`/`ZM` is confirmed visually only; the letters `W` (water?) and `M` are unexplained (OQ-023).

**Rows past the width.** 51 distinct ranges (`BSH_RGBAZ` 26, `BSH_RGBZ` 19, `BSH_RGBZM` 6) contain rows whose runs end 1–16 pixels beyond `width`; their row count equals `height` and `0xFF` is their last byte, so the grammar holds (P). `figuren::decode_frame_clipped` drops the overrun pixels; what the original does with them is open (OQ-025). Figures have no such frames.

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
- `anno_formats::anlagen::AnlagenFile` (`colors`, `entries`, `entry_chunks`, `volumes`); entries are `figuren::Entry` with `palette_ids`.
- `anno_formats::figuren::{FrameFormat::{RgbWZ, RgbZM, RgbAH}, decode_frame_clipped}`.
- RGBA: `anno_assets::building::to_rgba`; PNG export: `anno-extract anlagen <Anlagen.dat> <outdir> <entry id>…` (layout as `anno-extract figuren`).
- Python reference: `tools/figuren.py anlagen`.

## Tests
- Corpus: `crates/formats/tests/anlagen_corpus.rs` — entry count, ids, byte-identical entry re-encoding, `COLORS` id lists and frame references, 1,031 sets / 110 shadows, 3 × 9,633 frames, format counts, volume tiling, decode of every range (51 with clipping).
- Unit: `figuren::tests::clipped_decode_drops_overrun`.
- Reference output: `agree_python.rs::{anno_dump_matches_anlagen_py_synthetic (CI), anno_dump_matches_anlagen_py_corpus}`.
- PNG export: `extract.rs::extract_anlagen_corpus_samples` (entries 1380, 2223, 1591, 2702).
- Visual check 2026-10-16: palace (1380, `Z`), large castle (2223, `AZ`), cliff (1591, `ZM`), river piece (2702, `WZ`) render correctly.
