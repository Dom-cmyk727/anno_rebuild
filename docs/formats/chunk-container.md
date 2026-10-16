# Chunk container

**Status:** partial — the container layer is complete for the whole corpus (P); container *detection* is still heuristic (OQ-013) and the `.scp` trailing bytes are unexplained (OQ-001).

## Files covered
- Globs: `Scenes/**/*.sze`, `Scenes/Islands/**/*.scp`, `**/*.tex`, `**/*.inc`, `**/*.zei`, `Anlagen.dat`
- Count in GOG 2.0.0.5: 50 `.sze`, 465 `.scp`, 1,214 `.tex`, 1,213 `.inc`, 11 `.zei`, 1 `Anlagen.dat` = 2,954 files, 17,529 chunks (as detected by the reference heuristic)
- Not covered: `FigurenHeader.dat` (different record layout, OQ-002), `Figuren*.dat` (raw sprite data, OQ-003)
- Format-specific specs built on top: `sze.md`, `scp.md`, `tex.md`, `zei.md`, `anlagen.md` (not yet written)

## Identification
- No file magic. Every file holds exactly one top-level (root) chunk. Root names (P, `anno-dump chunks` over the corpus): `ANNO` for 23 `.sze`, all 465 `.scp`, all 1,214 `.tex`, all 1,213 `.inc` and `Anlagen.dat`; `ANNOADDON1` for 27 `.sze`; `CHARSET` for all 11 `.zei`.

## Layout
A file is a sequence of chunks. A chunk:

| Offset | Size | Type | Name | Meaning | Evidence |
|---|---|---|---|---|---|
| 0x00 | 12 | bytes | `name_field` | ASCII name, NUL-terminated, rest of the 12 bytes padding | P: all 17,529 chunks have a NUL within 12 bytes; longest name is 11 chars (`AUFTRAGTEXT`, `COMPPROFIL2`) |
| 0x0C | 4 | u32 LE | `size` | payload length in bytes, excluding this 16-byte header | P: every file is consumed exactly (or up to the root end, see `.scp`) |
| 0x10 | `size` | bytes | `payload` | leaf data, or a sequence of child chunks | P |

**Name padding.** 17,507 of 17,529 name fields are zero after the NUL. 22 carry non-zero bytes there (14 in `.tex`, 6 in `.zei`, 2 in `.inc`; on `TEXTURE`, `ANNO`, `COLOR`, `CHARSET`, `HEAD`, `BODY`); the most common tail is `01 00 00 00` (8×). They look like uninitialised buffer leftovers, but that is not proven (OQ-008). A writer must therefore keep the raw 12 bytes to reproduce files; new chunks get zero padding.

**Name alphabet.** Every name observed matches `[A-Z][A-Z0-9_]{1,11}` (P). The reader uses that pattern to tell chunk headers from data, so it is a parser rule, not a proven format rule.

**Nesting.** A payload is a container when it parses *exactly* into one or more chunks (P, all files). Children are probed down to depth 6 (`MAX_DEPTH`), mirroring `tools/chunks.py`. Which chunks are containers in the original is not encoded in the file; detection is a heuristic (OQ-013). Known gap: `INSEL5` in `.scp` starts with a 12-byte prefix before its sub-chunks and is therefore seen as a leaf (OQ-013).

## Variants / versions
- **`.scp` trailing bytes:** in exactly the 444 island files whose root `ANNO` contains `INSEL5`, the file continues for **60 bytes** after the root chunk. The 21 files with `INSEL4` end exactly at the root (P: `anno-dump chunks` over all 465 `.scp`). Content unknown (OQ-001). The reader exposes them as `ChunkFile::trailing`; the writer appends them unchanged.
- Root `ANNO` vs `ANNOADDON1`: same container layer, both round-trip (P). Content differences belong to `sze.md` (OQ-009).

## Open fields
- Name padding bytes — OQ-008
- `.scp` trailing 60 bytes — OQ-001
- Which chunks are containers (explicit rule) — OQ-013

## Implementation
- Reader: `anno_formats::chunk::parse_file` (zero-copy; keeps `name_field`, absolute `offset`, `payload`, optional `children`, `trailing`).
- Writer: `anno_formats::chunk::write_file` / `write_chunk`. Containers are re-encoded from their children with recomputed sizes; leaves and name fields are copied verbatim. `name_field("NAME")` builds a zero-padded field for new chunks.

## Tests
- Round-trip + corpus: `crates/formats/tests/corpus.rs::chunk_corpus` — all 2,954 files parse, write back byte-identically, file/trailing counts per extension and the 17,529 / 22 name-field figures above (needs `ANNO1503_GAME_DIR`, skips otherwise).
- Unit tests: `crates/formats/src/chunk.rs` (`writes_back_identically`, `rebuilds_container_from_children`, …).
- Reference tool output: `tools/chunks.py tree` = `anno-dump chunks` (`crates/tools/tests/agree_python.rs`).
