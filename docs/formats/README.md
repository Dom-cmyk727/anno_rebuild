# Format specifications

One spec per original file format, written as the format is confirmed. Every field must carry evidence (see legend). Anything not yet proven goes to `docs/open-questions.md` and is linked from the spec as `?` — never stated as fact.

Counts refer to GOG 2.0.0.5 (`game/app/`), from `docs/research/game-files-analysis.md`.

## Index

| Format | Files | Spec file | Status |
|---|---|---|---|
| Chunk container | `.sze`, `.scp` (root), `.tex`, `.inc`, `.zei`, `Anlagen.dat` | `chunk-container.md` | not yet written — container validated by `tools/chunks.py` (all files of these types parse; `.scp` see OQ-001) |
| Scenario | `Scenes/**/*.sze` (50) | `sze.md` | not yet written |
| Island template | `Scenes/Islands/**/*.scp` (465) | `scp.md` | not yet written |
| Textures | `*.tex` (1,214), `*.inc` (1,213) | `tex.md` | not yet written |
| Palettes | `COLOR` in `.tex`, `COLORS` in `Anlagen.dat` | `palette.md` | not yet written |
| Fonts | `*.zei` (11) | `zei.md` | not yet written |
| Buildings | `Anlagen.dat` (1) | `anlagen.md` | not yet written |
| Figures / sprites | `FigurenHeader.dat`, `Figuren*.dat` (16) | `figuren.md` | not yet written |
| Plain-text data | `Data/*.dat` (12, text ones), `Texte.dat` | `data-text.md` | not yet written |

Third-party formats (`.wav` RIFF, `.bik` Bink, `.jpg`) get no spec; note only how the game uses them.

## Template

Copy the block below into `docs/formats/<name>.md`.

```markdown
# <Format name>

**Status:** draft | partial | complete (all fields have P or D evidence)

## Files covered
- Glob: `<path glob relative to game/app>`
- Count in GOG 2.0.0.5: <n>
- Related formats: <links>

## Identification
- Magic / root chunk: <bytes or chunk name>, evidence: <P/D/C>
- How to tell variants apart: <...>

## Layout
All integers little-endian unless stated otherwise; offsets relative to <payload start / file start>.

| Offset | Size | Type | Name | Meaning | Evidence |
|---|---|---|---|---|---|
| 0x00 | 4 | u32 | `example` | ... | P: all 50 `.sze` |
| 0x04 | 2 | u16 | `unknown_04` | unknown | ? OQ-NNN |

## Evidence legend
- **P** — parsed corpus file(s); name the files or the corpus subset and the check performed.
- **D** — disassembly; give module + address (e.g. `AnnoWelt.dll 0x1001A2B0`) and link the `docs/re/` note.
- **C** — community source; give URL.
- **?** — hypothesis; must link an entry in `docs/open-questions.md`.

## Variants / versions
<versioned chunk names (e.g. INSEL4 vs INSEL5), ANNO vs ANNOADDON1 differences, per-version layout tables>

## Open fields
- `<field>` — OQ-NNN

## Tests
- Round-trip test: `crates/formats/<path>::<test name>` (parse → write → byte-identical)
- Corpus test: `<test name>` over `<glob>` (needs `ANNO1503_GAME_DIR`, skips otherwise)
- Reference tool output: `tools/chunks.py` / `anno-dump <subcommand>`
```

Reminder: a field without P or D evidence is a hypothesis. Log it in `docs/open-questions.md` first, then reference the OQ number in the Evidence column.
