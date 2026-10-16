# Textures (`.tex`) and texture indices (`.inc`)

**Status:** partial — every file parses and every texture decodes to its declared size (P); several `HEAD` fields and one prefix word are unexplained (OQ-021); byte-identical re-encoding of pixel data is not implemented yet.

## Files covered
- Globs: `**/*.tex`, `**/*.inc` (mainly `Textures/`, `ToolGfx/`)
- Count in GOG 2.0.0.5: 1,214 `.tex`, 1,213 `.inc`. Every `.inc` has a `.tex` of the same name next to it (P, `inc_corpus`).
- Built on: [`chunk-container.md`](chunk-container.md). Palettes: [`palette.md`](palette.md).

## Identification
- Root chunk `ANNO` (P, all 1,214 `.tex` and 1,213 `.inc`).
- `.tex`: root children are only `COLOR` and `TEXTURE` chunks (P). 338 files have an empty root (payload size 0) (P).
- `.inc`: root has exactly one child, `ACCESS` (P); no trailing bytes (P).

## `.tex` layout

### Entry prefix (`COLOR` and `TEXTURE`)
Both payloads start with an 8-byte prefix, then a sequence of sub-chunks in the chunk-container format. Because of the prefix, generic container detection sees these chunks as leaves (OQ-013); a `.tex` reader must skip 8 bytes and parse the remainder as chunks.

| Offset | Size | Type | Name | Meaning | Evidence |
|---|---|---|---|---|---|
| 0x00 | 4 | u32 LE | `flag` | 0 on every `TEXTURE` and on 1,417 of 1,427 `COLOR`; 2 on 10 `COLOR` in `ToolGfx/fenster.tex` and `ToolGfx/fenster8.tex` (ids 32, 73–75, 98); no other value | P (`tex_corpus`); meaning ? OQ-021 |
| 0x04 | 4 | u32 LE | `id` | entry id; referenced by `HEAD.palette_id` (palettes) and `.inc` numbers (textures) | P |
| 0x08 | … | chunks | | sub-chunks, see below | P |

### `COLOR`
Exactly one sub-chunk `BODY` of 1,024 bytes: the palette, see [`palette.md`](palette.md) (P, all 1,427).

### `TEXTURE`
Sub-chunks `HEAD` (36 bytes) and `BODY` (pixels), plus `NAME` on 147 textures (P). `NAME` occurs on exactly the 147 `TEX_RGB` textures (P: every `TEX_RGB` has one, and the counts agree).

`HEAD` (all integers u16 LE):

| Offset | Size | Type | Name | Meaning | Evidence |
|---|---|---|---|---|---|
| 0x00 | 2 | u16 | `width` | width in pixels | P: every texture decodes to exactly `width × height` |
| 0x02 | 2 | u16 | `height` | height in pixels; number of RLE rows | P |
| 0x04 | 2 | u16 | `palette_id` | `id` of a `COLOR` in the **same** file | P: resolves for all 5,176 |
| 0x06 | 2 | u16 | `unknown_06` | 8 everywhere; plausibly bits per pixel | ? OQ-021 |
| 0x08 | 2 | u16 | `unknown_08` | 0 everywhere | ? OQ-021 |
| 0x0A | 10 | ASCII | `format` | pixel format name, NUL-padded: `BSH_RGB` (5,028), `BSH_RGBH` (1, `Textures/debug.tex`), `TEX_RGB` (147) | P (`tex_corpus`) |
| 0x14 | 4 | bytes | `color_key` | a colour as `[B, G, R, 0]`; `ff 00 ff 00` (magenta) on 5,169 of 5,176, `ff 94 00 00` on 5, `00 00 00 00` on 2; plausibly a transparency key | ? OQ-021 (counts from the M1.2 survey, not yet asserted in a test) |
| 0x18 | 2 | u16 | `unknown_18` | often `width / 2` | ? OQ-021 |
| 0x1A | 2 | u16 | `unknown_1a` | often `height`; with 0x18 out of the image for 1,207 textures; plausibly an anchor | ? OQ-021 (survey figure, not asserted) |
| 0x1C | 8 | bytes | `unknown_1c` | zero | ? OQ-021 (survey figure, not asserted) |

`NAME`: source bitmap file name, Windows-1252, NUL-terminated (e.g. `Wiese011.bmp`). P for presence; encoding inferred from the umlauts seen.

### Pixel formats (`TEXTURE/BODY`)
Pixels are 8-bit indices into the palette named by `palette_id`.

**`TEX_RGB` — raw.** `width × height` bytes, row-major, top row first, no transparency (P: body length equals `width × height` for all 147; these are the ground tiles, e.g. in `Textures/Felder.tex`).

**`BSH_RGB`, `BSH_RGBH` — row RLE with transparency** (P: all 5,029 decode exactly; the meaning of the `H` is unknown, OQ-021). Grammar:

```
image := row* last_row
row   := run* 0xFE
last_row := run* 0xFF
run   := skip:u8 count:u8 index:u8{count}     ; skip ∉ {0xFE, 0xFF}
```

Decoding starts at `x = 0, y = 0` with every pixel transparent. A run advances `x` by `skip` (transparent pixels), then writes `count` indices at `x … x+count-1` and advances `x` by `count`. `0xFE` moves to the next row (`y += 1, x = 0`). `0xFF` ends the last row and the image. Constraints checked by the decoder and holding on every texture (P):
- no run extends past `width`;
- the number of terminated rows (`0xFE` and the final `0xFF`) equals `height`;
- `0xFF` is the last byte of `BODY`.

Pixels not written by any run are transparent. The decoder accepts runs with `count = 0` (a pure skip); whether the corpus contains them, and how the original encoder splits long runs or wide gaps, has not been counted yet. Both matter for a byte-identical encoder.

## `.inc` layout
`ACCESS` payload: a concatenation of records, each `"<source>,<number>"` followed by one NUL byte. `<source>` is a file name (Windows-1252, e.g. `image001.pcx`); `<number>` is decimal ASCII. The comma that splits them is the last one in the record.

- 1,213 files, 340 with an empty `ACCESS`, 1,559 records in total (P).
- Every `<number>` is the `id` of a `TEXTURE` in the same-name `.tex` (P: holds for all 1,213 files). So `.inc` maps source image names to texture ids.

## Open fields
- `COLOR`/`TEXTURE` prefix `flag` — OQ-021 (e)
- `HEAD` 0x06, 0x08, 0x14, 0x18, 0x1A, 0x1C — OQ-021 (a–c)
- `BSH_RGBH` vs `BSH_RGB` — OQ-021 (d)
- Container detection for prefixed chunks — OQ-013

## Reference dump (`anno-dump tex`, `anno-dump inc`)
Language-neutral text output for cross-checking another implementation (e.g. a C++20 port). `tools/tex.py dump` prints the same text; `crates/tools/tests/agree_python.rs` compares them. Lines are `\n`-terminated, fields separated by one space, in exactly this order.

```
# <path as given on the command line>
TEX palettes=<n> textures=<n>
PALETTE id=<u32> flag=<u32> rgb=<fnv64>
TEXTURE id=<u32> flag=<u32> width=<u16> height=<u16> palette=<u16> u06=<u16> u08=<u16> format=<str> key=<hex8> u18=<u16> u1a=<u16> u1c=<hex16> body=<len> name=<str|-> opaque=<n> pixels=<fnv64|BAD>
```

- Numbers are decimal. `<hexN>` is the raw bytes in file order as N lowercase hex digits.
- `format` and `name` are the bytes up to the first NUL, quoted with `"`; bytes 0x20–0x7E except `"` and `\` are printed as is, every other byte as `\xHH` (lowercase). `name=-` means no `NAME` chunk.
- Entries are printed in file order (palettes and textures interleaved as they occur).
- `<fnv64>` is FNV-1a 64-bit (offset basis `0xcbf29ce484222325`, prime `0x100000001b3`) as 16 lowercase hex digits.
  - `rgb` hashes the 256 entries as `R, G, B` bytes (768 bytes, entry 0 first).
  - `pixels` hashes the decoded image row-major as 2 bytes per pixel: `0x01, index` for an opaque pixel, `0x00, 0x00` for a transparent one. `opaque` counts opaque pixels. If decoding fails: `opaque=0 pixels=BAD`.
- A file that does not parse as described prints `NOPARSE` after the `#` line (details go to stderr).

With `--full`, each `PALETTE` line is followed by 16 lines `  P<row> <16 × rrggbb>` (row 0–15, entries `16·row … 16·row+15`, hex, separated by spaces), and each `TEXTURE` line by `height` lines `  R<y> <width × cell>` where a cell is two hex digits of the index or `..` for transparent, cells separated by nothing. If decoding fails there are no `R` lines.

`anno-dump inc`:
```
# <path>
INC records=<n>
ACCESS source=<str> number=<u32>
```
(`<str>` quoted as above; `NOPARSE` if the layout differs.)

## Implementation
- `anno_formats::tex` (`TexFile::parse`, `Head`, `Texture::decode`, `decode_rle`), `anno_formats::inc` (`parse_access`).
- Python reference: `tools/tex.py`.

## Tests
- Corpus: `crates/formats/tests/tex_corpus.rs::{tex_corpus, inc_corpus}` — all figures marked P above unless noted (needs `ANNO1503_GAME_DIR`).
- Round trip: whole files via `chunk_corpus` (container layer); `HEAD` field-by-field in `tex_corpus` and `tex::tests::head_round_trip`. Pixel re-encoding: not yet.
- Unit: `tex::tests::{rle_two_rows, rle_rejects_overflow_and_wrong_height}`, `inc::tests::records`.
- Reference output: `anno-dump tex/inc` = `tools/tex.py dump/inc` on synthetic files (CI) and corpus samples (`agree_python.rs`, needs `ANNO1503_GAME_DIR`).
