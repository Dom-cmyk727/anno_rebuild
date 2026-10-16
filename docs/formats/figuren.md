# Figures and sprites (`FigurenHeader.dat`, `Figuren*.dat`)

**Status:** partial — the index parses completely and re-encodes byte-identically, every frame of every volume decodes to its declared size, and the decoded images look right (P). The meaning of several records and fields is open (OQ-023); frame pixel re-encoding is not implemented.

## Files covered
- `FigurenHeader.dat` (27,904,566 bytes): index of all figure types, colour tables and frame headers.
- `Figuren<g><nn>.dat` (16 files, 250,853,441 bytes in total): pixel data, no header. `<g>` = zoom group 0–2, `<nn>` = part number from `00`. Parts: group 0 = `Figuren000.dat`–`Figuren010.dat` (11), group 1 = `Figuren100.dat`–`Figuren103.dat` (4), group 2 = `Figuren200.dat` (1) (P).
- Built on: [`chunk-container.md`](chunk-container.md) (12-byte name + `u32` size records). Colour tables: [`palette.md`](palette.md). Pixel RLE: [`tex.md`](tex.md).

All integers little-endian. The community note that "the first four bytes of a sprite header are big-endian" (CL §3) does not hold for this build: every size, offset and count reads consistently as little-endian (P: the whole index parses as exact record sequences, and frame ranges tile the volumes, see below).

## `FigurenHeader.dat` layout

```
ANNO header (16)          size = file size − 16 + total size of all Figuren*.dat
  FIGUREN chunk           368 × ENTRY chunks, exact sequence
  volume header × 3       24 bytes each, one per zoom group
```

| What | Evidence |
|---|---|
| Root `ANNO` size field = 278,757,991 = (27,904,566 − 16) + 250,853,441, i.e. the root logically spans the header file and all volume files | P (`figuren_corpus` asserts it) |
| `FIGUREN` payload is exactly 368 `ENTRY` chunks; the generic container parser stops at the root because its size exceeds the file | P |

### Volume headers (file tail, 3 × 24 bytes)

| Offset | Size | Type | Name | Meaning | Evidence |
|---|---|---|---|---|---|
| 0x00 | 12 | name | | `TEXTURES` | P |
| 0x0C | 4 | u32 | `size` | 8 + total size of the group's volume files (179,919,250 / 55,077,515 / 15,856,700): a chunk header whose payload continues in the volume files | P (asserted) |
| 0x10 | 4 | u32 | `group` | 0, 1, 2 in that order | P |
| 0x14 | 4 | u32 | `frames` | 243,707 for each group = number of frame headers of that group in the index | P (asserted) |

### `ENTRY` (figure type)

| Offset | Size | Type | Name | Meaning | Evidence |
|---|---|---|---|---|---|
| 0x00 | 4 | u32 | `flag` | 0 in all 368 | P |
| 0x04 | 4 | u32 | `id` | 1 … 1666, unique, ascending | P |
| 0x08 | … | records | | exact sequence of named records | P |

Records (name: count over 368 entries; payload sizes):

| Record | Count | Payload | Evidence |
|---|---|---|---|
| `VERSION2` | 368 | 24 bytes; meaning ? OQ-023 | P |
| `HEAD` | 368 | 16 (136×) or 20 (232×) bytes; first `u32` looks like a float (`0.2f` on entry 1); ? OQ-023 | P |
| `BEWOHNTYPE` | 368 | NUL-terminated string, e.g. `EXPLOSION`, `MAEHER` | P |
| `ROUTETYPE` | 361 | NUL-terminated string, e.g. `UNUSED`, `MAEHER` | P |
| `FORSCHTYPE`, `WARE`, `BAUWARE` | 19, 89, 27 | strings (short, NUL-terminated) | P (sizes); content ? OQ-023 |
| `LOGIC` | 368 | 28–44 bytes; contains `1.0f` values; ? OQ-023 | P |
| `SCHUSSPOS`, `OBJECTPOS` | 7, 24 | 12–156 bytes; ? OQ-023 | P |
| `PATH` | 368 | NUL-terminated string, source directory, e.g. `Effekt_Feuerwerk02\` | P |
| `COLORS` | 368 | colour tables, see below | P |
| `HAEUSER` | 368 | sprite sets, see below (empty in 1 entry) | P |

Record order is fixed: `VERSION2, HEAD, BEWOHNTYPE, [ROUTETYPE], [FORSCHTYPE], [WARE], [BAUWARE], LOGIC, [SCHUSSPOS], [OBJECTPOS], PATH, COLORS, HAEUSER`; 15 distinct combinations occur (P).

### `COLORS`
An exact sequence of `ENTRY` chunks, each with the `Anlagen.dat` colour-table layout ([`palette.md`](palette.md): `u32 flag`, `u32 id`, `BODY` 256 × `[B, G, R, 0]`, `NAME` `Farbtabellen\….pcx`). Tables per entry: 0 (1 entry), 1 (282), 2 (60), 3 (25) (P). Inside an entry the ids count from 0 (P on samples, e.g. entry 101: 0, 1, 2).

### `HAEUSER` → sprite sets
An exact sequence of `ENTRY` chunks (1,288 in total). Each is a **sprite set**:

| Offset | Size | Type | Meaning | Evidence |
|---|---|---|---|---|
| 0x00 | 16 | bytes | prefix; often `u16, u16 4, …`; the last 8 bytes sometimes hold text fragments (e.g. `cte_Nebe`), i.e. probably uninitialised memory; 259 distinct values | P; ? OQ-023 |
| 0x10 | … | records | `BODY2` (44), `ANIM` (24), `SOUND2` (128), `GFXDATA` (84, contains the source `.tga` path), `TEXTURES` × 3, optional `SHADOW` | P |

Record orders: `BODY2, ANIM, SOUND2, GFXDATA, TEXTURES ×3, SHADOW` (1,047), the same without `SHADOW` (238), without `TEXTURES` (2), without `ANIM` (1) (P).

`SHADOW` = 8-byte prefix `03 00 00 00 00 00 00 00` (all 1,047) + `BODY2`, `ANIM`, optional `SOUND2` (477), `GFXDATA`, `TEXTURES` × 3 (P). Its frame counts equal those of the enclosing set, group by group (P, asserted).

### `TEXTURES` (frame list)

| Offset | Size | Type | Name | Meaning | Evidence |
|---|---|---|---|---|---|
| 0x00 | 4 | u32 | `group` | zoom group; the three records of a set are groups 0, 1, 2 in order, with equal counts | P (asserted) |
| 0x04 | 4 | u32 | `n` | number of frames; payload size = 8 + 36 n | P (asserted) |
| 0x08 | 36 n | | frames | | P |

Frame header (36 bytes):

| Offset | Size | Type | Name | Meaning | Evidence |
|---|---|---|---|---|---|
| 0x00 | 2 | u16 | `width` | | P: every frame decodes to `width × height` |
| 0x02 | 2 | u16 | `height` | | P |
| 0x04 | 2 | u16 | `palette` | index into the entry's `COLORS` tables; 0–2, always in range | P (asserted); visual: sheep (entry 103) renders correctly only with table 1 `Schaf.PCX` |
| 0x06 | 2 | u16 | `unknown_06` | 8 in all frames; plausibly bits per pixel | P; ? OQ-023 |
| 0x08 | 12 | char | `format` | NUL-padded: `BSH_RGBV`, `BSH_RGBZ`, `BSH_RGBAV`, `BSH_RGBAZ`, `BSH_RGBH`, `BSH_ALPHA` | P |
| 0x14 | 2 | u16 | `anchor_x` | e.g. `(width/2, height−4)` for the mower, waterline for the galleon; outside the frame for 783 frames | P (values); meaning ? OQ-023 |
| 0x16 | 2 | u16 | `anchor_y` | | |
| 0x18 | 4 | u32 | `offset` | byte offset in the group's concatenated volume files | P |
| 0x1C | 4 | u32 | `length` | byte length; 0 for 5,697 frames = empty frame | P |
| 0x20 | 4 | u32 | `unknown_20` | 0 in all frames | P |

Totals: 731,121 frames (243,707 per group); formats `BSH_RGBV` 215,178, `BSH_RGBZ` 164,520, `BSH_RGBAV` 3,837, `BSH_RGBAZ` 504, `BSH_RGBH` 24, `BSH_ALPHA` 347,058; `BSH_ALPHA` occurs exactly in `SHADOW` sets (P, asserted).

Zoom groups: group 1 frames are about half, group 2 about a quarter of the group 0 size (e.g. 358×179, 180×90, 90×44) (P).

## `Figuren*.dat` (volumes)
A group's volume is the concatenation of its part files in part order. The distinct non-empty `(offset, length)` ranges of a group tile its volume exactly: they start at 0, have no gaps or overlaps, and end at the volume size (P, asserted for all three groups). Frames share ranges (655,842 distinct ranges for 725,424 non-empty frames). Ranges may cross part boundaries; readers must not assume a frame lies within one file (`figuren::locate`).

## Frame pixel encoding
Row RLE with the `.tex` grammar ([`tex.md`](tex.md)): per row `(skip: u8, count: u8, data)` runs, `0xFE` ends a row, `0xFF` ends the last row. Differences per format:

| Format | Bytes per pixel | Count byte | Pixel | Evidence |
|---|---|---|---|---|
| `BSH_RGBV` | 1 | count | palette index | P + visual (mower, entry 101) |
| `BSH_RGBH` | 1 | count | palette index; `H` ? OQ-023 | P |
| `BSH_RGBZ` | 2 | count (up to 255) | `[z, index]` | P + visual: sheep/galleon render correctly only with the second byte as index |
| `BSH_RGBAV` | 1 | `level << 6 \| count` (count ≤ 63) | palette index | P; `level` ? OQ-023 |
| `BSH_RGBAZ` | 2 | `level << 6 \| count` | `[z, index]` | P + visual (gondola, entry 1510) |
| `BSH_ALPHA` | 0 | count | coverage only, no pixel bytes | P + visual: shadow masks match the sprite silhouettes |

Every one of the 655,842 distinct ranges decodes with these rules: runs stay inside `width`, the row count equals `height`, and `0xFF` is the last byte of the range (P, `figuren_corpus`). A frame with `length = 0` is fully transparent.

Meaning of `z` and `level` is not established. Observations: in the gondola (`BSH_RGBAZ`) level 3 covers 2,195 of 2,322 pixels and the frame looks right when all levels are drawn opaque; the sheep's `z` bytes are nearly all 255. See OQ-023.

## Reference dump
`anno-dump figuren [--full] <FigurenHeader.dat>` and `tools/figuren.py dump [--full]` print, after `# <path>`:

```
FIGUREN entries=<n> volumes=<n>
VOLUME group=<g> size=<u32> frames=<u32>                      (per volume header)
ENTRY id=<id> flag=<u32> records=<NAME,NAME,…>                 (per entry)
  RECORD <NAME> size=<n> hash=<fnv64>                          (every record except COLORS, HAEUSER)
  COLORTABLE i=<k> id=<id> flag=<u32> rgb=<fnv64> name=<str>   (per colour table)
  SET i=<k> prefix=<hex> records=<NAMES> other=<fnv64>         (per sprite set)
    TEXTURES group=<g> frames=<n> hash=<fnv64>
      FRAME k=<k> width= height= palette= u06= format=<str> anchor=<x>,<y> offset= length= u20=   (--full)
    SHADOW prefix=<hex> records=<NAMES> other=<fnv64>          (then its TEXTURES, indented 2 more)
```

`hash` = FNV-1a 64 over the record or `TEXTURES` payload; `other` = FNV-1a 64 over the concatenated payloads of all records of the set except `TEXTURES` and `SHADOW`; `rgb` as in `palette.md`; `<str>` quoted as in `tex.md`. `NOPARSE` if the file does not have this layout (exit code 1).

`anno-dump frames <game_dir> <entry id>…` and `tools/figuren.py frames` print per entry `ENTRY id=<id> sets=<n>` (or `ENTRY id=<id> MISSING`) and per frame of every set and its shadow:
`FRAME set=<i> shadow=<0|1> group=<g> k=<k> width= height= format=<str> opaque=<n> pixels=<fnv64|BAD>`, where `pixels` hashes one byte `0` per transparent pixel and `1, index, z, level` per covered pixel (fields a format does not store are 0).

## Implementation
- `anno_formats::figuren` (`FigurenHeader::parse/to_bytes`, `Entry`, `GfxSet`, `FrameSet`, `Frame`, `decode_frame`, `volume_file_name`, `locate`). Colour tables via `anno_formats::colors::parse_entry`.
- Python reference: `tools/figuren.py` (independent).
- RGBA: `anno_assets::figure::to_rgba` (index → table colour, alpha 255; shadow coverage → `[0, 0, 0, 255]`; `z` and `level` not applied).
- PNG export: `anno-extract figuren <game_dir> <outdir> <entry id>…` → `<outdir>/<id>/s<set>_g<group>_<k>[_shadow].png` + `index.txt` (`<file> width= height= format= anchor=<x>,<y> palette= table=<name|->`). A text index like `anno-extract tex`, rather than the JSON the plan mentioned.

## Open fields
- `VERSION2`, `HEAD`, `LOGIC`, `SCHUSSPOS`, `OBJECTPOS`, sprite-set prefix, `BODY2`, `ANIM`, `SOUND2`, `GFXDATA` contents; `z`, `level`, `H`, anchor semantics — OQ-023.

## Tests
- Corpus: `crates/formats/tests/figuren_corpus.rs` — all figures marked P above (needs `ANNO1503_GAME_DIR`): byte-identical round trip of `FigurenHeader.dat`, structure counts, volume tiling, root size, decode of every distinct frame range.
- Unit: `figuren::tests::{frame_round_trip, decode_formats, locate_spans_parts, header_round_trip}`, `anno_assets::figure::tests::sprite_and_shadow_to_rgba`.
- Reference output: `agree_python.rs::{anno_dump_matches_figuren_py_synthetic (CI), anno_dump_matches_figuren_py_corpus}` — `--full` dump of the real file (743,672 lines) and pixel hashes for entries 101, 103, 1510, 1539, 1546, 1574 (all six formats).
- PNG export: `extract.rs::extract_figuren_corpus_samples` — PNGs equal `figure::to_rgba` for entries 101, 1501, 1510.
- Visual check 2026-10-16: mower (101, `V`), carrier (203, `V`, `TRAEGER`; id as cited from AnnoZone in PLAN M1.3), sheep (103, `Z`), galleon (1501, `Z`, groups 0 and 2), gondola (1510, `AZ`), shadow masks of 101 — all render as expected.
