# Palettes

**Status:** partial — layouts of `COLOR` (`.tex`) and `COLORS` (`Anlagen.dat`) are complete (P); colour byte order rests on one survey figure until the visual check; a few constant fields are unexplained (OQ-021, OQ-022).

Both kinds of palette are 256 entries of `[B, G, R, 0]`, 1,024 bytes. They differ in the record around them.

## Files covered
- `COLOR` chunks in `**/*.tex`: 1,427 palettes in 1,214 files (P, `tex_corpus`). Container layout around them: [`tex.md`](tex.md).
- `ANNO/COLORS/ENTRY` in `Anlagen.dat`: 440 colour tables ("Farbtabellen") (P, `colors_corpus`).

## Palette layout (both kinds)
256 entries of 4 bytes, entry 0 first. Offsets relative to the start of the palette.

| Offset | Size | Type | Name | Meaning | Evidence |
|---|---|---|---|---|---|
| 4·i + 0 | 1 | u8 | `blue` | blue of palette index `i` | P, see byte order below |
| 4·i + 1 | 1 | u8 | `green` | green of index `i` | P |
| 4·i + 2 | 1 | u8 | `red` | red of index `i` | P |
| 4·i + 3 | 1 | u8 | `reserved` | 0 in every entry of every palette | P (`tex_corpus`, `colors_corpus` assert it) |

This is the Windows `RGBQUAD` order. Colour components are full 8-bit values, not 6-bit VGA values: the sea-tile average below (122) is already above 63, and `COLORS` entries contain `ff`.

**Byte order evidence (P, M1.2 survey, not asserted in a test).** Averaging the decoded sea tiles (`Meer*.bmp` textures in `Textures/Felder.tex`) through their palette gives byte 0 = 122, byte 1 = 77, byte 2 = 33. Read as B, G, R that is a dark blue; read as R, G, B a brown. The visual check with `anno-extract tex` (open, M1.2) should confirm this on UI textures. For `COLORS` the order is assumed to be the same; nothing yet tests it independently.

## `.tex` `COLOR`
- 8-byte prefix (`flag`, `id`, see `tex.md`), then exactly one sub-chunk `BODY` (a generic chunk: 12-byte name field + `u32` size) of exactly 1,024 bytes holding the palette (P, all 1,427).
- A texture uses the palette whose `id` equals its `HEAD.palette_id`; it is always in the same `.tex` file (P, all 5,176 textures).
- Whether palette ids are unique within a file, or shared across files, has not been checked.

## `Anlagen.dat` `COLORS`
Identification: `ANNO/COLORS` is the first child of the root (offset 16), followed by `ANLAGEN` and three `TEXTURES` (P, `anno-dump chunks`). Its children are exactly 440 `ENTRY` chunks (P).

An `ENTRY` payload holds an 8-byte prefix and two **tagged records**: 4-byte ASCII tag, `u16` 0, `u16` length, then `length` bytes. These are not generic chunks (no 12-byte name field, no `u32` size), so the generic reader sees `ENTRY` as a leaf (`anno-dump probe`: `CHILDREN leaf ×440`, `PREFIX none ×440`). All offsets relative to the `ENTRY` payload, integers little-endian:

| Offset | Size | Type | Name | Meaning | Evidence |
|---|---|---|---|---|---|
| 0x000 | 4 | u32 | `flag` | 0 | P (`colors_corpus` asserts all 440); meaning ? OQ-022 |
| 0x004 | 4 | u32 | `id` | 1…440: unique, ascending, equal to position + 1 | P |
| 0x008 | 4 | ASCII | tag | `BODY` | P, all 440 |
| 0x00C | 2 | u16 | — | 0 | P, all 440; meaning ? OQ-022 |
| 0x00E | 2 | u16 | `body_len` | 1,024 | P, all 440 |
| 0x010 | 1,024 | | `palette` | 256 × `[B, G, R, 0]`, see above | P: reserved byte 0 in all 440 × 256 entries |
| 0x410 | 4 | ASCII | tag | `NAME` | P, all 440 |
| 0x414 | 2 | u16 | — | 0 | P, all 440; ? OQ-022 |
| 0x416 | 2 | u16 | `name_len` | length of `name` including its NUL; the entry ends right after `name` (entry size = 0x418 + `name_len`) | P, all 440 |
| 0x418 | `name_len` | bytes | `name` | source colour-table file, Windows-1252, exactly one NUL at the end: always `Farbtabellen\<file>.pcx` (extension `.PCX` or `.pcx`), e.g. `Farbtabellen\Gräser01.PCX` | P, all 440 |

Observed values (P, M1.2 probe on the Mac with the game files):
- Entry sizes 1,071–1,112 bytes (31 distinct), i.e. `name_len` 23–64.
- Palette entry 0 is a grey (`R = G = B`) in at least 439 of 440 tables: `000000` ×363, `040404` ×47, `ffffff` ×12, `020202` ×8, `080808` ×6, `141414`, `010101`, `070707` ×1 each (one table not in this top-8 list).
- Palette entry 1 is magenta `ff 00 ff` in 287 of 440 tables (`colors_corpus` asserts 287), the same colour as most `.tex` `HEAD.color_key` values. Whether index 1 or the colour itself is the transparency key is open (OQ-021, OQ-022).

Which `TEXTURES` or `ANLAGEN` record refers to a colour table by `id` is not known yet (M1.4).

## Open fields
- `flag` in the `.tex` `COLOR` prefix (2 on 10 palettes in `ToolGfx/fenster*.tex`) — OQ-021 (e)
- `COLORS` `flag`, the `u16` 0 before each record length, meaning of entries 0/1 — OQ-022

## Reference dump
- `anno-dump tex` prints one `PALETTE` line per `.tex` palette, see [`tex.md`](tex.md#reference-dump-anno-dump-tex-anno-dump-inc).
- `anno-dump colors [--full] <Anlagen.dat>` (= `tools/tex.py colors`):
  ```
  # <path>
  COLORS entries=<n>
  COLORTABLE id=<u32> flag=<u32> rgb=<fnv64> name=<str>
  ```
  `rgb` and `--full` rows (`  P<row> <16 × rrggbb>`) exactly as for `PALETTE` in `tex.md`; `name` quoted as in `tex.md` (up to the NUL). `NOPARSE` if any entry violates the table above.

## Implementation
- `anno_formats::tex::Palette` (`entries` raw `[B, G, R, 0]`, `rgb(index)` → `[R, G, B]`).
- `anno_formats::colors` (`parse_anlagen`, `parse_entry`, `ColorTable::to_bytes`). The parser rejects any deviation from the table (tags, zero `u16`s, lengths, NUL), so `to_bytes` reproduces the payload exactly.
- Python reference: `tools/tex.py` (`color_tables`).

## Tests
- Corpus: `crates/formats/tests/tex_corpus.rs::tex_corpus`, `crates/formats/tests/colors_corpus.rs::colors_corpus` (count, ids, flag, reserved bytes, name pattern, magenta count, byte-identical `to_bytes` for all 440; needs `ANNO1503_GAME_DIR`).
- Unit: `colors::tests::{parses_and_round_trips, rejects_malformed}`.
- Round trip: `.tex` `BODY` is copied verbatim by the chunk writer (`chunk_corpus`); `COLORS/ENTRY` re-encoded field by field (`colors_corpus`).
- Reference output: `agree_python.rs::{anno_dump_matches_colors_synthetic, anno_dump_matches_colors_corpus}`.
