# Population corpus

50 scenarios, both modes, checkpoints 0/60/300/600 seconds, native default step 10 ms. No projection or script suppression.

Per mode and per build: 11 exit-0 runs with four clean invariant checkpoints; 39 exit-1 load refusals; 0 invariant failures, crashes or other exits. Canonical selections remain FAIL (22 passes / 78 failures across both modes).

All 100 matched scene-mode stdout/stderr/exit tuples were exactly identical between Release and ASan. This compares existing CLI observations, not full state hashes.

| Scene | Plain | House updates | Refusal (if any) |
|---|---|---|---|
| 0010SingleEndlos/00_1024_easyentry_01.sze | exit_0 | exit_0 |  |
| 0010SingleEndlos/01_1024_easy_256.sze | exit_0 | exit_0 |  |
| 0010SingleEndlos/02_1024_easy_128.sze | load_refused | load_refused | refused: players[7].SCHIFF[0].PRIVAT.CREW.ENTRY[0].PRIVAT.ESEL.TASK: unsupported unit record/state |
| 0010SingleEndlos/03_1024_med_256.sze | exit_0 | exit_0 |  |
| 0010SingleEndlos/04_1024_med_128.sze | exit_0 | exit_0 |  |
| 0010SingleEndlos/05_800_Med_128.sze | exit_0 | exit_0 |  |
| 0010SingleEndlos/06_1024_heavy_256.sze | exit_0 | exit_0 |  |
| 0010SingleEndlos/07_1024_heavy_128.sze | exit_0 | exit_0 |  |
| 0010SingleEndlos/08_800_heavy_128.sze | load_refused | load_refused | refused: players[7].SCHIFF[0].PRIVAT.CREW.ENTRY[0].PRIVAT.ESEL.TASK: unsupported unit record/state |
| 0011SingleEndlosAddon/endless01.sze | exit_0 | exit_0 |  |
| 0011SingleEndlosAddon/endless02.sze | load_refused | load_refused | refused: players[11].SOLDAT[0]: unsupported actor kind/class/import facts 373 at offset 4700322 |
| 0011SingleEndlosAddon/endless03.sze | exit_0 | exit_0 |  |
| 0100Kampagne01/S01_01_01.sze | load_refused | load_refused | refused: players[7].SOLDAT[0]: unsupported actor kind/class/import facts 251 at offset 2446235 |
| 0100Kampagne01/S01_02_01.sze | load_refused | load_refused | create refused: script offset 1339: unsupported action: QUESTLOG_SHOW |
| 0100Kampagne01/S01_03_01.sze | load_refused | load_refused | refused: players[6].SCHIFF[0].PRIVAT.ROUTE: unsupported required unit private record/state |
| 0100Kampagne01/S01_04_01.sze | load_refused | load_refused | refused: players[8].SCHIFF[0].PRIVAT.CREW.ENTRY[0].PRIVAT.ESEL.TASK: unsupported unit record/state |
| 0100Kampagne01/S01_05_01.sze | load_refused | load_refused | refused: players[4].SOLDAT[0].PRIVAT.ESEL: unheld ESEL placement is unsupported |
| 0100Kampagne01/S01_06_01.sze | load_refused | load_refused | refused: players[3].SCHIFF[0].PRIVAT.CREW.ENTRY[0]: unsupported actor kind/class/import facts 701 at offset 1291401 |
| 0100Kampagne01/S01_07_01.sze | load_refused | load_refused | refused: players[8].SOLDAT[0]: unsupported actor kind/class/import facts 701 at offset 2476951 |
| 0100Kampagne01/S01_08_01.sze | load_refused | load_refused | refused: players[3].SOLDAT[0]: unsupported actor kind/class/import facts 708 at offset 365132 |
| 0100Kampagne01/S01_09_01.sze | load_refused | load_refused | refused: players[7].SOLDAT[0]: unsupported actor kind/class/import facts 710 at offset 2169175 |
| 0100Kampagne01/S01_10_01.sze | load_refused | load_refused | refused: players[7].SOLDAT[0]: unsupported actor kind/class/import facts 711 at offset 2369913 |
| 0100Kampagne01/S01_11_01.sze | load_refused | load_refused | refused: players[4].SCHIFF[2].TASK: unsupported unit record/state |
| 0100Kampagne01/S01_12_01.sze | load_refused | load_refused | refused: players[8].SCHIFF[0].PRIVAT.CREW.ENTRY[1]: unsupported actor kind/class/import facts 700 at offset 2806031 |
| 0100Kampagne01/S01_13_01.sze | load_refused | load_refused | refused: players[8].SCHIFF[0].PRIVAT.CREW.ENTRY[2]: unsupported actor kind/class/import facts 709 at offset 771830 |
| 0200Single/Single_01.sze | load_refused | load_refused | refused: players[7].SCHIFF[0].PRIVAT.CREW.ENTRY[0].PRIVAT.ESEL.TASK: unsupported unit record/state |
| 0200Single/Single_05.sze | load_refused | load_refused | create refused: script offset 871: unsupported action: QUESTLOG_SHOW |
| 0200Single/Single_06.sze | load_refused | load_refused | create refused: script offset 828: unsupported action: QUESTLOG_SHOW |
| 0200Single/Single_08.sze | load_refused | load_refused | create refused: script offset 871: unsupported action: QUESTLOG_SHOW |
| 0200Single/Single_10.sze | load_refused | load_refused | create refused: script offset 960: unsupported action: QUESTLOG_SHOW |
| 0200Single/Single_12.sze | load_refused | load_refused | create refused: script offset 821: unsupported action: QUESTLOG_SHOW |
| 0200Single/Single_14.sze | load_refused | load_refused | create refused: script offset 1033: unsupported action: QUESTLOG_SHOW |
| 0200Single/Single_15.sze | load_refused | load_refused | refused: players[2].SCHIFF[0].PRIVAT.CREW.ENTRY[0].PRIVAT.ESEL.TASK: unsupported unit record/state |
| 0200Single/Single_16.sze | load_refused | load_refused | refused: players[3].SCHIFF[0].PRIVAT.CREW.ENTRY[0].PRIVAT.ESEL.TASK: unsupported unit record/state |
| 0200Single/Single_24.sze | load_refused | load_refused | refused: players[9].SOLDAT[0]: unsupported actor kind/class/import facts 704 at offset 471591 |
| 0201SingleAddOn/single30.sze | load_refused | load_refused | refused: players[24].SOLDAT[0]: unsupported actor kind/class/import facts 703 at offset 2154064 |
| 0201SingleAddOn/single31.sze | load_refused | load_refused | refused: players[7].SOLDAT[0]: unsupported actor kind/class/import facts 710 at offset 857353 |
| 0201SingleAddOn/single32.sze | load_refused | load_refused | refused: players[6].SCHIFF[0].PRIVAT.CREW.ENTRY[0]: unsupported actor kind/class/import facts 701 at offset 1146594 |
| 0201SingleAddOn/single33.sze | load_refused | load_refused | refused: players[12].SOLDAT[0]: unsupported actor kind/class/import facts 700 at offset 1219343 |
| 0201SingleAddOn/single34.sze | load_refused | load_refused | refused: players[10].SOLDAT[0]: unsupported actor kind/class/import facts 700 at offset 1132808 |
| 0201SingleAddOn/single36.sze | load_refused | load_refused | refused: players[13].SOLDAT[0].PRIVAT.ESEL: unheld ESEL placement is unsupported |
| 0201SingleAddOn/single38.sze | load_refused | load_refused | refused: players[11].SCHIFF[0].PRIVAT.CREW.ENTRY[0]: unsupported actor kind/class/import facts 709 at offset 1631548 |
| 0201SingleAddOn/single39.sze | load_refused | load_refused | refused: players[15].SOLDAT[0]: unsupported actor kind/class/import facts 701 at offset 1946530 |
| 0201SingleAddOn/single40.sze | load_refused | load_refused | create refused: script offset 550: unsupported action: QUESTLOG_SHOW |
| 0201SingleAddOn/single41.sze | load_refused | load_refused | refused: players[23].SOLDAT[0].PRIVAT.ESEL: unheld ESEL placement is unsupported |
| 0201SingleAddOn/single42.sze | load_refused | load_refused | refused: players[12].SOLDAT[0].PRIVAT.ESEL: unheld ESEL placement is unsupported |
| 0201SingleAddOn/single43.sze | load_refused | load_refused | refused: players[6].SOLDAT[0]: unsupported actor kind/class/import facts 700 at offset 97353 |
| Tutorial/01Discover01.sze | exit_0 | exit_0 |  |
| Tutorial/02Trade01.sze | exit_0 | exit_0 |  |
| Tutorial/03T_Combat_01.sze | load_refused | load_refused | refused: players[2].SOLDAT[0]: unsupported actor kind/class/import facts 710 at offset 264246 |

Native streams are published in population-release-plain.txt and population-release-houses.txt, with relative scene block headers. Original native files and their hashes remain in cache. Read-only game input identities are in stage5-game-input-manifest.json. Wrapper prelude-classification correction is documented in methods.md and stage5-classification-note.json.
