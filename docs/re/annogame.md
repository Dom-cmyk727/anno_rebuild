# `AnnoGame.dll` — figure and building data loaders

**Status:** first notes (2026-10-17), static analysis only (Ghidra 12.1.4 headless, `tools/ghidra/DecompileAllPlus.java`). Module: `game/app/AnnoGame.dll` (GOG 2.0.0.5), image base `0x10000000`. Confidence as in `annodisplay.md`: **confirmed** = read in the decompiled/disassembled code, **likely** = strong inference.

## Colour tables and player colours
- `0x10004e60` loads the `Anlagen.dat` `COLORS` tables and creates one colour-table object per table; the table kind passed to `AnnoDisplay.dll` is the table's `u32 flag` (first field of the `COLORS/ENTRY` payload), with 0 mapped to 3 (`Effects`). Table 109 `Flagge01_01.PCX`, the only one with flag 4, therefore becomes an `EffectsMulti` table: the building flag in player colours. **Confirmed.** (Resolves OQ-022 a.)
- `0x10011570` loads the colour tables of a `FigurenHeader.dat` entry (all 477 have flag 0). It uses kind 1 (`Gouraud`), or kind 2 (`GouraudMulti`, player-colour variants) when the entry's `BEWOHNTYPE` number is 97–112, 150 or 53 (test at `0x100115b7`). **Confirmed.**
- `BEWOHNTYPE` strings map to numbers through a sparse table of name pointers at `0x10024cb8` (`0x10013a20` returns the name of a number): 53 `ESEL`, 97 `ERKUNDER`, 98 `TRAGTIER`, 99 `SCHWERT`, 100 `BOGEN`, 101 `ARMBRUST`, 102 `LANZE`, 103 `MUSKETIER`, 104 `KAVALARIE`, 105 `GEWEHR`, 106 `MEDICUS`, 107 `BEDIENUNG`, 108 `KANONIER`, 109 `MOERSER`, 110 `BELAGERTURM`, 111 `KATAPULT`, 112 (no name), 113 `SHIP`, 150 `FLAGGE`; 57 names in total. **Confirmed** (table read from the data section). So 69 of 368 figure entries are drawn in player colours — soldiers, scouts, pack animals, donkeys, flags; not ships (113) and not the thief (`DIEB`).

## Other records
- The figure `ENTRY` reader also reads `OBJECTPOS` sub-records `SCHUSS`, `FLAGGE`, `SHOTWI` (and one more four-letter tag): positions on the object (shot origins, flag position) — the `FLAGGE` string near `0x10024520` belongs here, not to colours. **Likely.**

## BEWOHNTYPE identity completion — 2026-11-18 (KBEWOHN-1)

Evidence tags here: **P** = read from the matching original file, **D** = original
instructions read, **C** = rebuild code/measurement. Reference AnnoGame.dll SHA256
`ac721220afd27ebca5fd2ece6a8ff452289b9d7458c4e091c68adea2012db7bc`.
No original bytes or instruction listings accompany this note.

- **P**, `0x10024cb8` through exclusive `0x10024fb8`: 192 pointer slots, 57 unique
  nonempty names and 135 null pointers; no duplicate named aliases. Indices are
  sparse semantic numbers, not figure asset ids or compacted row positions.
  Examples: `SCHIFFWARE` is 7, slots 8–16 are holes, `TRAEGER` is 17;
  `HIRSCH` is 49, `RAUBTIER` is 50, `ESEL` is 53 and `SHIP` is 113.
  `WUESTE` is explicitly the name stored at index 0. The adjacent ROUTETYPE table
  begins at `0x10024fb8`; its first null pointer is not another BEWOHNTYPE name.
- **D**, `0x10013a20`–`0x10013b33`: this is a **number-to-name accessor**, not an
  inverse lookup. Its range blocks admit 1–192; a null result, number 0 or an
  out-of-range input returns the `UNUSED` fallback at `0x100261e8`. Thus the
  physical named zero and the accessor's fallback policy are separate facts;
  the accessor does not return the physical `WUESTE` entry for input 0.
- **D**, property dispatch `0x100033c0`–`0x100033da`, case `0x1f` at
  `0x10003426`–`0x1000345a`: the property getter calls that numeric accessor
  (`0x10003432`) and copies the returned name after the caller's index word.
  The figure loader's BEWOHNTYPE branch `0x1000ff6d`–`0x10010019` scans candidate
  indices 0–192, compares the returned string case-sensitively and stores the
  matched candidate index at `0x10010016`. A direct table read alone must not be
  described as this accessor's inverse implementation.
- **P**, installed FigurenHeader.dat, checked through its public record reader:
  368 entries, 368 known named types, 50 distinct names, 0 absent names, 0 unknown
  names and 0 entries naming `WUESTE`. Seven table names are unused in this copy:
  `WUESTE`, `HASE`, `KROKODIL`, `MAEHERSPEZ`, `PELIKAN`, `VERKAEUFER`, `VULKAN`.
- **C**, `figure_type_from_entry` / `bewohntype_by_name` in
  [types.cpp](../../src/sim/src/types.cpp): the previous map held 20 real names
  and one ignored empty sentinel. All 37 omitted semantic names are now mapped,
  including the legitimate named zero. The parser still stores 0 for absent,
  empty, `UNUSED` and unrecognized spellings; this preserves rebuild policy and
  does not claim original unknown-name rejection. The optional named lookup
  distinguishes those spellings from an engaged `WUESTE = 0` result. A supported
  class on a different asset id uses the same mapping; this is no figure-count cap.
- **C**, registry field audit of all 368 imported definitions: exactly 272
  bewohntype values change and 96 remain equal; all other fields, navigation,
  hull, route and ore facts remain equal. No added name enters 97–128 or equals
  53, 145, 146 or 150. Existing founding/crew, original-unit, ship-cargo/freight,
  ore placement/source and script-classification predicates therefore retain
  their decisions for these definitions. Assets' player-colour selection uses
  its separate string-name table and is unchanged.
- **C**, the complete original registry digest changes from
  `5560dbe117c8900b` to `da762f6991f0f832`; it hashes bewohntype. A native save
  and replay recorded with the previous original registry are specifically
  refused by the existing registry-identity checks under the corrected registry.
  No version, field layout or migration is added, and old original-data recordings
  are not claimed byte-identical. The affected current fixture producers are
  identified and measured in the KBEWOHN report; synthetic numeric oracles are
  not regenerated to conceal this content change.

This completes classification, not wildlife creation, hunting, combat or animal
movement. [Wildlife reads](wildlife.md) and the separate helper packet retain
their stated lifecycle/World integration limits. Original-runtime and History
Edition execution are not established by these static/data reads.
