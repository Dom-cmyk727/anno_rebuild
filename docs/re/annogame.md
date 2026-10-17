# `AnnoGame.dll` — figure and building data loaders

**Status:** first notes (2026-10-17), static analysis only (Ghidra 12.1.4 headless, `tools/ghidra/DecompileAllPlus.java`). Module: `game/app/AnnoGame.dll` (GOG 2.0.0.5), image base `0x10000000`. Confidence as in `annodisplay.md`: **confirmed** = read in the decompiled/disassembled code, **likely** = strong inference.

## Colour tables and player colours
- `0x10004e60` loads the `Anlagen.dat` `COLORS` tables and creates one colour-table object per table; the table kind passed to `AnnoDisplay.dll` is the table's `u32 flag` (first field of the `COLORS/ENTRY` payload), with 0 mapped to 3 (`Effects`). Table 109 `Flagge01_01.PCX`, the only one with flag 4, therefore becomes an `EffectsMulti` table: the building flag in player colours. **Confirmed.** (Resolves OQ-022 a.)
- `0x10011570` loads the colour tables of a `FigurenHeader.dat` entry (all 477 have flag 0). It uses kind 1 (`Gouraud`), or kind 2 (`GouraudMulti`, player-colour variants) when the entry's `BEWOHNTYPE` number is 97–112, 150 or 53 (test at `0x100115b7`). **Confirmed.**
- `BEWOHNTYPE` strings map to numbers through a sparse table of name pointers at `0x10024cb8` (`0x10013a20` returns the name of a number): 53 `ESEL`, 97 `ERKUNDER`, 98 `TRAGTIER`, 99 `SCHWERT`, 100 `BOGEN`, 101 `ARMBRUST`, 102 `LANZE`, 103 `MUSKETIER`, 104 `KAVALARIE`, 105 `GEWEHR`, 106 `MEDICUS`, 107 `BEDIENUNG`, 108 `KANONIER`, 109 `MOERSER`, 110 `BELAGERTURM`, 111 `KATAPULT`, 112 (no name), 113 `SHIP`, 150 `FLAGGE`; 57 names in total. **Confirmed** (table read from the data section). So 69 of 368 figure entries are drawn in player colours — soldiers, scouts, pack animals, donkeys, flags; not ships (113) and not the thief (`DIEB`).

## Other records
- The figure `ENTRY` reader also reads `OBJECTPOS` sub-records `SCHUSS`, `FLAGGE`, `SHOTWI` (and one more four-letter tag): positions on the object (shot origins, flag position) — the `FLAGGE` string near `0x10024520` belongs here, not to colours. **Likely.**
