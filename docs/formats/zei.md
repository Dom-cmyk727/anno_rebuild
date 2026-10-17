# Bitmap fonts (`.zei`)

**Status:** partial — layout complete, all 11 files round-trip byte-identically and every glyph decodes (P); the meaning of the two header sizes and of the three pixel levels is open (OQ-031).

## Files covered
- `Textures/ScapeSmall.zei`, `Textures/ScapeMedium.zei`, `ToolGfx/{Main,Help}*.zei`, `ToolGfx/Startup.zei`: 11 files (P).
- Built with a `konvertchars` tool from Verdana and other fonts (`Data/ControlChars.bat`, `Data/ScapeChars.bat`: `konvertchars verdana -h11 -w6 -s1 -f..\toolgfx\MainSmall.zei`; C, not verified against the binaries).
- Built on: [`chunk-container.md`](chunk-container.md).

## Layout
Root chunk `CHARSET` with exactly two children, `HEAD` and `BODY` (P, all 11). In the two `Textures/Scape*.zei` the name fields carry bytes after the NUL (`CHARSET\0` + `e0 e1 e2 e3`; `HEAD\0`/`BODY\0` + 3 × 0 + `u32 16`); the nine `ToolGfx` fonts are zero-padded (P; kept verbatim for the round trip).

`HEAD` (2,064 bytes):

| Offset | Size | Type | Name | Meaning | Evidence |
|---|---|---|---|---|---|
| 0x00 | 2 | u16 | `size_a` | 10–31 | P; ? OQ-031 |
| 0x02 | 2 | u16 | `size_b` | 13–31 | P; ? OQ-031 |
| 0x04 | 12 | | reserved | 0 | P, all 11 |
| 0x10 | 256 × 8 | | glyph table | per character code 0–255: `u32 width` (advance in pixels), `u32 length` (bytes of its RLE in `BODY`) | P: lengths sum to the `BODY` size in all 11 |

Codes 0–31 have width 0 and length 0; codes 32–255 have a glyph (224 per font) (P). Codes are Windows-1252, as `Texte.dat` (e.g. 0x96 `–`, 0xC4 `Ä` render correctly).

`BODY`: the glyphs of codes with length > 0, in code order, back to back (P).

## Glyph
A row RLE as in `.tex` `BSH_RGB` ([`tex.md`](tex.md)): runs of `skip, count, count × level`; `0xFE` ends a row, `0xFF` the glyph (P). Levels are 1, 2 or 3 (P); in the large fonts they form a fill with a dark and a light edge (visual), what each level means when drawn is open (OQ-031).

- All glyphs of one font have the same number of rows, which is not stored: HelpNormal, HelpNormal8, MainMedium, MainSmall 25; MainLarge 37; MainLarge8 33; MainMedium8 17; MainSmall8 13; ScapeMedium 21; ScapeSmall 17; Startup 29 (P). For four fonts it equals `size_b`, for the others it is 4–6 more.
- No pixel lies past the glyph width. In `MainSmall8.zei` and `MainMedium8.zei` every glyph has a row whose skip ends one column past the width, without pixels; the decoder widens images only for pixels (P).
- The `.tex` RLE encoder (`tex::encode_rle`) reproduces 2,016 of the 2,464 glyphs byte-identically; the other 448 are exactly the glyphs of those two fonts, whose trailing skips it does not write (P) — the same RLE.

## Reference dump
`anno-dump zei [--full] <file.zei>...` = `python3 tools/zei.py dump [--full] …` (`crates/tools/tests/agree_python.rs`, corpus and synthetic):

```text
# <path>
CHARSET size_a=<a> size_b=<b> reserved=<24 hex digits> glyphs=<codes with data>
GLYPH code=<code> width=<width> length=<bytes> image=<w>x<h> hash=<FNV-1a 64 of the RLE bytes>   or `… BAD`
  <row>          with --full: one line per row, `.` empty, the level digit, `#` for levels ≥ 10
```

`NOPARSE` if the file is not `CHARSET { HEAD, BODY }` with a 2,064-byte `HEAD` and matching `BODY`.

## Open fields
- `size_a`, `size_b`, the three levels, the name-field bytes of the `Scape` fonts — OQ-031.

## Tests
- Unit: `crates/formats/src/zei.rs` (parse, decode, round trip, widening only for pixels).
- Corpus: `crates/formats/tests/zei_corpus.rs` (all 11: byte-identical round trip, header values, 224 glyphs, rows per font, levels 1–3, no pixel past the width, re-encoding count).
- Reference tool output: `anno_dump_matches_zei_py_corpus`, `anno_dump_matches_zei_py_synthetic`.
