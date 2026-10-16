# Palettes

**Status:** partial — `COLOR` palettes in `.tex` are complete (P); `COLORS` in `Anlagen.dat` is not decoded yet.

## Files covered
- `COLOR` chunks in `**/*.tex`: 1,427 palettes in 1,214 files (P, `tex_corpus`). Container layout around them: [`tex.md`](tex.md).
- `COLORS` in `Anlagen.dat` (440 `ENTRY` children per `docs/research/game-files-analysis.md`): **not covered yet**. Whether its entries use the same 1,024-byte layout is unchecked; do not assume it.

## Identification
- A `COLOR` entry in a `.tex` root: 8-byte prefix (`flag`, `id`, see `tex.md`) followed by exactly one sub-chunk `BODY` (P, all 1,427).
- `BODY` is exactly 1,024 bytes (P: the reader requires 256 × 4 bytes and every palette parses).

## Layout (`COLOR/BODY`)
256 entries of 4 bytes, entry 0 first. Offsets relative to the start of `BODY`.

| Offset | Size | Type | Name | Meaning | Evidence |
|---|---|---|---|---|---|
| 4·i + 0 | 1 | u8 | `blue` | blue of palette index `i` | P, see byte order below |
| 4·i + 1 | 1 | u8 | `green` | green of index `i` | P |
| 4·i + 2 | 1 | u8 | `red` | red of index `i` | P |
| 4·i + 3 | 1 | u8 | `reserved` | 0 in every entry of every palette | P (`tex_corpus` asserts it) |

This is the Windows `RGBQUAD` order. Colour components are full 8-bit values, not 6-bit VGA values: the sea-tile average below (122) is already above 63.

**Byte order evidence (P, M1.2 survey, not asserted in a test).** Averaging the decoded sea tiles (`Meer*.bmp` textures in `Textures/Felder.tex`) through their palette gives byte 0 = 122, byte 1 = 77, byte 2 = 33. Read as B, G, R that is a dark blue; read as R, G, B a brown. The visual check with `anno-extract tex` (open, M1.2) should confirm this on UI textures.

## Use
- A texture uses the palette whose `id` equals its `HEAD.palette_id`; it is always in the same `.tex` file (P, all 5,176 textures).
- Whether palette ids are unique within a file, or shared across files, has not been checked.
- Which index (if any) the engine treats as transparent for `TEX_RGB`, and how `HEAD.color_key` relates to the palette, is open (OQ-021).

## Open fields
- `flag` in the `COLOR` prefix (2 on 10 palettes in `ToolGfx/fenster*.tex`) — OQ-021 (e)
- `Anlagen.dat/COLORS` layout — not yet investigated (M1.2)

## Reference dump
`anno-dump tex` prints one `PALETTE` line per palette (`rgb=` = FNV-1a 64 over the 768 `R, G, B` bytes); `--full` adds the 256 colours as `rrggbb`. Exact format: [`tex.md`](tex.md#reference-dump-anno-dump-tex-anno-dump-inc).

## Implementation
- `anno_formats::tex::Palette` (`entries` raw `[B, G, R, 0]`, `rgb(index)` → `[R, G, B]`).

## Tests
- Corpus: `crates/formats/tests/tex_corpus.rs::tex_corpus` (count, `reserved` = 0, prefix values; needs `ANNO1503_GAME_DIR`).
- Round trip: `BODY` is copied verbatim by the chunk writer (`chunk_corpus`).
