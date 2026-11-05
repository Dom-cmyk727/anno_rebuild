# research-log.md (MAP-ECON-1 E1, 2026-11-03)

Read-only on `/Users/dom/Dev/anno1503-worktrees/anno1503-worktrees/anno1503-MAP-2` and on `game/app`. All outputs in this directory. No process was started that outlives its command; no pkill/killall; no coordinator contact; no CMake/test run.

## Commands (bounded)
- Python: `cache/map2-founding-evidence/venv/bin/python` (3.14.6; pefile 2024.8.26, capstone 5.0.7, unicorn 2.1.4).
- Disassembly: `tools/disasm.py <dll> <va> <bytes>` through a wrapper in the session scratchpad; dword/vtable reads through a 10-line pefile reader (`rd.py`); `tools/xref.py --reloc AnnoGame.dll 100033c0` (found the game-object vtable at `0x10021418`/`0x100214f8`) and `tools/xref.py AnnoWelt.dll 17fc 1804` (users of the tile-goal list).
- Data: `tools/figuren.py` (`records`, `entry`, `parse`) to dump `Anlagen.dat` entries 801, 811, 104, 105, 824 and `FigurenHeader.dat` entries 101, 102, 203, 220; scripts `dump801.py`, `dumpfig.py`, `prod3.py`, `scanware2.py` (all `WARE`/`ROHSTOFF` = `BAUM` entries and their `id − 1`), `routes.py` (farm-family figure lists and route types), `trees2.py` (465 island files, `GELAENDE` items with `WARE` = `BAUM`).
- Emulation: `emu_forester.py` (unicorn over the real bytes of `AnnoWelt 0x10087340`, `0x10087400`, `0x10087880`, `0x10092320`). Result: 13 PASS, "ALL PASS".
- Reference: `forester_ref.py forester_vectors.txt` → 38 vectors, 0 failures.

## Addresses read (AnnoWelt unless named)
`0x1006ea90` (farm tick, whole), `0x1003b0e0` + tables `0x1003b480`/`0x1003b450` (job factory; `0x29`/`0x2a` → `0x1003b255`), `0x1005c350` (job base ctor), vtable `0x100c4468` (job), `0x100c44c0` (step A), `0x100c4858` (step B), `0x100c4640` (step C), `0x100c4ae4` (cargo), `0x1005c260` (job arrival), `0x1005b6c0` (run), `0x1005b740` (abort), `0x1005e280` (next step after failure), `0x1005a030`/`0x10054750` (step types 7, 0xb), `0x1005f710`, `0x1005be80`, `0x1005be60`, `0x10058ae0` (step A start, whole), `0x10058a40` (its callback), `0x100553e0` (step B start), `0x10054530` (step B arrival), `0x100545c0` (abort), `0x10035260` (action start), `0x10092320` (island +0x10c, whole), `0x10087400`/`0x10087880` (tile flag read/write, mode 0 and the switch tables), `0x10087340` (island +0x110, the cut), `0x10072d20`/`0x10072b50` (send, entrance), `0x1006ee70` (raw receiver), `0x1003b6e0`/`0x100b4a30` (end of figure, job destructor), `0x10002e20`/`0x10002df0` (searcher goal adders), `0x10004100`–`0x10004330`, `0x10003320`–`0x10003450` (tile-goal pre-loop, partially), `0x1003b7c0` (figure ctor: cargo at `+0x6c`), `0x10035c00`. AnnoGame: `0x10021d10` (figure-type vtable), `0x1000fae0` (slot `+0x54`), `0x1000fe00` (loader, ROUTETYPE and LOGIC branches), `0x100033c0` + `0x10003650`/`0x10003678` (name-kind switch), `0x10013b40` (route name table, `0x10024fb8`), `0x1000a660`/`0x10007d20` (type slots `+0x40`, `+0x5c`), `0x10009240` (type loader, copy of the ENTRY header at `0x10009bdd`…).
AnnoFrame.dll: not read (the path does not leave AnnoWelt/AnnoGame).

## Hand derivation before the program
The vector values in `forester_vectors.txt` were derived on paper first (notation in `docs-sim-forestry.md` §5). While writing the vector file two hand errors were found and corrected **before** the first program run: the third `grow` vector (I first wrote 348 000; recomputation gave 109 000 + 19 · 12 000 = 337 000) and the order of the two `drain` outputs. The first program run then passed all 38 vectors. The emulation expectations were hand-written the same way.

## Dead ends and cautions
- First reading of the job arrival: assumed type 7/0xb were "harvest/repeat" mnemonics from `production.md` §5.1; the disassembly shows type 7 = step A (find and walk), type 0xb = step B (the cut), the repeat rule is "cargo below capacity restarts A".
- `0x10092320`'s argument order was initially misread as 3 arguments; the earlier `lea`+`push` is the 4th argument (output buffer) and the cargo-goods getter is evaluated between the pushes. Resolved by the callee's `ret 0x10` and its use of `[esp+0x34]` (goods) and `[esp+0x38]` (buffer); confirmed by the E harness.
- The emulation harness stubs registry/type methods; it cannot check that `[type+4]` is the ENTRY id (rests on the loader copy) nor the real island memory layout beyond the fields the four functions read.
- The farm tick itself was not emulated (it calls many object methods and the status helper); its arithmetic is D-read and transcribed in `forester_ref.py step`; the existing sim's `farm_step` has its own fixture F6.
- Walk-search details for tile goals were not resolved (U-1).
- `0x10003220` (the goal branch) was not read: bounded-time rule, only the identified forester path was followed.

## Not done (out of scope or unavailable)
Original-runtime observations (NOT_RUN-1/2), any change under the repository, any build or test run, E2–E6.

## Round 2 (U-1, U-10)
Read: AnnoGame `0x1000b6e0` (type driver), `0x10003260` (game slot +0x24), `0x1000a670`/`0x1000a660`; AnnoWelt `0x10003220` goal mask start, `0x10003346`-`0x10003450`, `0x10002b80` (mode store), `0x100921b0` (island +0x12c). Layer blocks and mapper via `tools/annowelt_tables.py`; `walk_tree_check.py` (new) prints codes, masks, layer-2 admission/costs for the 46 mature ids and predecessors. Vectors `walkcode` and `goalfree` added (44 vectors, 0 failures). Walk-code rule is covered only for first < 8 (all trees use 2 or 4).

## Round 2 review
Second reviewer (fresh): U-1/U-10 conclusions confirmed; citation of the ENTRY-id loader corrected to AnnoGame 0x10001d10 (0x1000b6e0 is the HAEUSER loader; the earlier line about 0x10009bdd/0x10009240 is withdrawn). Wording fixes: large-tree free sub-cells, draw per dispatch, U-4 closed, claim-hygiene as owner decision, canary controls (e). Verdict recorded in the final report.
