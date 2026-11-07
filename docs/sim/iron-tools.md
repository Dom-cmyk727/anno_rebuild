# Iron ore to iron to tools (MAP-EXTRACT-2 part T): T0 inventory, T1 contract

Status: **T0 + T1 (2026-11-07), docs only, before any C++.** Branch `task/MAP-EXTRACT-2`, base identity 135; the behaviour change of T2 allocates 137 in its own first commit. Evidence tags: P parsed, D disassembly, T observed in the sim, U unknown. Probe sources and raw outputs: `cache/close-extract/T0/` (git-ignored).

## T0. Inventory (sections 0-6 below came from the probe run of 2026-11-07)

Worktree `anno1503-MAP-2`, branch `task/MAP-EXTRACT-2` @ 7ae63a15. Read-only; no tracked file changed, game dir untouched.
Tags: **P** parsed from the original data (probe), **D** disassembly note already in `docs/` (module + address), **T** engine behaviour observed by running the repo code, **U** unknown / not determined.
Method: throwaway C++ probes in `scratchpad/probe/` linked against `build/mac-headless` libs (`load_original_registry`, `load_scenario_with_links`, `World::create`, `placement_rule`, `construction_eligibility`), cross-checked by independent Python (`tools/chunks.py`, `tools/insel.py`, the `tools/baustufen_ref.py` table) for the raw records, INFRAMIN names, HIGHMAP bytes and OCCUR ore objects. Supporting outputs in `scratchpad/probe/`: `types.txt`, `survey.txt` (every ore + city of the 50 .sze), `sites_all.txt` (legal sites per city), `ore_legal.txt` (per iron ore), `T0_ore_by_file.txt`.

### 0. Findings that change the plan (read first)

1. **The reserve is 1,600 t, not 400 t (P/T/D).** `kDepositReserve = 0x64000 = 409,600 raw = 1,600 t` (`src/sim/include/anno/sim/production.hpp:358`; `docs/sim/production.md`: "a shallow mine stops at 1,600 t"). Section 24.3 of `salt-extraction.md` writes "0x64000 (400 t)": an arithmetic slip (0x19000 = 400 t is the other constant, the type-1661 attach). Consequence: a **shallow** iron mine (1205/1210) on a 2,000 t deposit can extract only **2,000 - 1,600 = 400 t** (and only where depletion exists, item 3); a **deep** mine (1216/1217, or any non-iron mine) takes the full rate and uses nothing up.
2. **Owner 8 of endless02 island 1 cannot build any iron mine through the public path today, and the engine would refuse it even if it could (T).** 1205/1210 (stage 2C) become `Allowed` only after the stage pass runs (needs `Rules::house_updates`; first reached between step 900 and 1000 of 100 ms = 90 to 100 s of game time); but `placement.cpp::classify` makes the mountain class placeable only for a **deep** mine whose ware is in `PlacementParams::mountain_wares` (default `{"SALZSTEIN"}`), and `mine_is_deep` is false for 1205/1210 (iron, research_id 0). 1216/1217 are deep but need research 57 (TIEFE_ERZMINE), which owner 8 does not hold (`ResearchRequired`); the sim has no research command (command.hpp: Build, Demolish, SetProductionLine, trade/speed, Found, ship commands) and the endless02 script grants nothing (P: the script text has no construction/permission call), so research 57 can only come from a scenario's player record.
3. **Depletion exists only in the add-on edition (D, T).** `depletes` = world setting 14 bit 1 = root chunk `ANNOADDON1`. Of the 50 files 27 are `ANNOADDON1` (endless01-03, all 0010SingleEndlos, single30-43, ...) and 23 are `ANNO` (Tutorial/*, S01_*, Single_*): in the latter no mine depletes.
4. **Only 13 of 50 scenarios keep their whole script in the sim (T).** `isolated=0`: 00..08 SingleEndlos (no player city at start, ships only), endless02, endless03, Tutorial 01Discover01 and 02Trade01. In every other file (including endless01, all S01_*, Single_*, single30-43) the script does not compile, the projection is `isolated`, `construction_eligibility` = `ContextUnavailable` for every type and a BuildCmd is refused (the commit d2be23aa rule). So the only player-owned towns where the iron chain can be built through public commands are **endless02 island 1 owner 8** and **Tutorial/02Trade01 island 3 owner 2** (plus island 7 city 1 owner 0, a computer seat).
5. **Stages 3D (1035) and 3F (1034) are not reachable in the pilot (T).** 3D needs 600 and 3F 1,100 residents at group >= 2 (P, BGruppen [BAUSTUFEN]: `fixtures/synthetic/sim/baustufen_table.txt`, stage 14 = group 2 count 600, stage 16 = group 2 count 1100). Owner 8 with live houses reached stage mask 0x7ff (stages 0-10) at step 3000 and stayed there to step 12000 (20 game minutes). So the charcoal route (1035 -> 1034) is not constructible there; only 1017 (2C) and 1019 (2C) are.
6. **WERKZEUG reaches the store by a market cart, not by a workshop carrier (D).** See section 2.

## 1. Chain from parsed data (P)

Units: 8.8 fixed (raw/256 = t). "step" = 500 ticks = 1 s of game time (tick = 2 ms). Rate t/min = raw x 60 / 256. Goods ids (P): EISENERZ 1, GOLD 2, SALZSTEIN 3, EDELSTEINE 4, STEINE 5, EISEN 15, HOLZKOHLE 23, SCHMUCK 37, WERKZEUG 41, HOLZ 42, ZIEGEL 43, SCHWERT 46, BOGEN 47, KANONE 110 and MOERSER 112 (above the 56 warehouse goods).
Flag word = HEAD u16 [0x1c]. INFRAMIN = raw name (P, Python) and the registry stage index (P). Stage vocabulary (D, `docs/re/build-availability.md`): EVER 0, 1C 3, 2C 6, 3D 14, 3F 16, 4C 19, 4F 22, FORSCH 31; the query tests bit `stage` of the player's flag dword (+0x90), then (stage 31 only) research alternatives; it is not an ordinal compare.

### 1.1 Iron

| type | role | sim class / PRODTYPE | flag word | footprint | gold | goods cost | INFRAMIN (stage) | research | in -> out (raw/step) | out cap (raw) | running cost [a,b] |
|---|---|---|---|---|---|---|---|---|---|---|---|
| **1205** | shallow iron mine | Mine / MINE(3) | 0x3096 | 3x4 | 1,200 | WERKZEUG 5 t, HOLZ 15 t | 2C (6) | none (research_id 0) | - -> EISENERZ 21 (4.9 t/min) | 1,792 (7 t) | [15,40] |
| **1210** | shallow iron mine (variant; HEAD byte 0x0b = 1 vs 0, meaning U) | Mine | 0x3096 | 3x4 | 1,200 | same | 2C (6) | none | same, 21 | 1,792 | [15,40] |
| **1216** | deep iron mine | Mine | 0x3096 | 3x4 | 1,700 | WERKZEUG 8, HOLZ 20, ZIEGEL 10 t | FORSCH (31) | FORSCHTYPE `TIEFE_ERZMINE` = research 57 (alternatives [57]) | - -> EISENERZ 38 (8.9 t/min) | 1,792 | [30,65] |
| **1217** | deep iron mine (variant; HEAD byte 0x0a = 2 vs 0) | Mine | 0x3096 | 3x4 | 1,700 | same | FORSCH (31) | 57 | same, 38 | 1,792 | [30,65] |
| **1017** | smelter, ore + wood | Workshop / HANDWERK(1) | 0x0084 | 3x3 | 800 | WERKZEUG 10, HOLZ 4, ZIEGEL 10 t | 2C (6) | none | EISENERZ 21 (4.9 t/min) + HOLZ 13 (3.1 t/min) -> EISEN 17 (4.0 t/min) | 1,792 | [20,40] |
| **1019** | toolsmith | Workshop | 0x0084 | 3x3 | 500 | WERKZEUG 3, HOLZ 4, ZIEGEL 8 t | 2C (6) | none | EISEN 8 (1.9 t/min) + HOLZ 2 (0.5 t/min) -> WERKZEUG 9 (2.1 t/min) | 1,280 (5 t) | [15,25] |
| **1035** | charcoal kiln | Workshop | 0x0084 | 3x3 | 100 | WERKZEUG 2, HOLZ 2 t | 3D (14) | none | HOLZ 20 (4.7 t/min) -> HOLZKOHLE 26 (6.1 t/min) | 2,048 (8 t) | [5,12] |
| **1034** | smelter, ore + charcoal | Workshop | 0x0084 | 3x3 | 1,000 | WERKZEUG 10, HOLZ 10, ZIEGEL 20 t | 3F (16) | none | EISENERZ 36 (8.4 t/min) + HOLZKOHLE 20 (4.7 t/min) -> EISEN 34 (8.0 t/min); max_figures 2 | 1,792 | [32,75] |
| **3206** | native stonemason (identified) | Farm / STEINMETZ(8) | 0x0084 | 3x3 | 0 | HOLZ 3, ZIEGEL 2 t | EVER (0) | none; `folks` [4] (natives) | ROHSTOFF **STEINE** (5) -> WARE **EISEN** (15): 128 in, 51 out per step | 1,280 | [0,0] |
| 1037 | swordsmith (consumer) | Workshop | 0x0084 | 3x3 | 200 | WERKZEUG 4, HOLZ 4, ZIEGEL 2 t | FORSCH (31) | research 1 | EISEN 5 -> SCHWERT 9 | 1,280 | [15,40] |
| 3214 | native weaponsmith (consumer) | Workshop (PRODTYPE HANDWERK, two lines) | 0x0084 | 4x4 | 0 | HOLZ 5, ZIEGEL 4 t | EVER (0) | `folks` [4] | EISEN 51 + EISEN 51 -> SCHWERT 51 + BOGEN 51 | 1,280 + 1,280 | [0,0] |
| 1038 | cannon foundry (consumer) | **Cannonworks** / KANONENBAU(11) | 0x0084 | 4x4 | 800 | WERKZEUG 10, HOLZ 10, ZIEGEL 10 t | FORSCH (31) | research 21 | HOLZKOHLE 72 + EISEN 36 -> KANONE 13 + MOERSER 13 | 1,280 + 1,280 | [0,0] |

Notes (P): all mines share one HIGHMAP table with the salt mine 1203 (48 bytes: `ff`x20, `2020 ffff ffff 1010 ffff ffff 1010 1010 ff 10 0c0c 1010 ffff 0404 ffff`; 1203/1205/1216 compared byte for byte in Python; 1205==1210 and 1216==1217 in the registry), so the terrain rule and footprint are the proved salt mine's. Mines have `max_figures` 1 and figure 101 (1205/1210/1206) or none (1216/1217/1212/1214/1203); the sim never sends a mine figure (T: `mine_step` sets no send_figure). The "time" T = time0_128/128 s per tonne: 1205 12 s, 1216 6.7 s, 1017 15 s, 1019 28 s, 1035 10 s, 1034 7.5 s. Input-store capacity of a workshop: U.

### 1.2 Gold / gems (G)

| type | role | class | flag | fp | gold | goods cost | INFRAMIN | in -> out (raw/step) | cap | running |
|---|---|---|---|---|---|---|---|---|---|---|
| 1206 | gold mine (figure 101, radius 4) | Mine | 0x3096 | 3x4 | 1,500 | WERKZEUG 15, HOLZ 12, ZIEGEL 4 t | 4C (19) | - -> GOLD 9 (2.1 t/min) | 1,792 | [20,50] |
| 1212 | gold mine (variant, no figure, radius 0) | Mine | 0x3096 | 3x4 | 1,500 | same | 4C | - -> GOLD 9 | 1,792 | [20,50] |
| 1214 | gem mine (radius 8, no figure) | Mine | 0x3096 | 3x4 | 1,500 | WERKZEUG 8, HOLZ 4, ZIEGEL 10 t | 4C | - -> EDELSTEINE 9 (line 0; the second table entry 32 is unused by `mine_step`) | 1,792 | [30,80] |
| 1039 | goldsmith | Workshop | 0x0084 | 3x3 | 300 | WERKZEUG 6, HOLZ 7, ZIEGEL 11 t | 4C | GOLD 8 + EDELSTEINE 8 -> SCHMUCK 9 | 1,280 | [20,40] |
| 3410 | native goldsmith | Workshop | 0x0084 | 3x3 | 0 | HOLZ 5, ZIEGEL 4 t | EVER | `folks` [6]; GOLD 128 -> SCHMUCK 51 | 1,280 | [0,0] |
| 3010 | native jeweller | Workshop | 0x0084 | 3x3 | 0 | HOLZ 6, ZIEGEL 4 t | EVER | `folks` [2]; EDELSTEINE 128 -> SCHMUCK 51 | 1,280 | [0,0] |
| 1313 | jewellery stall | **MarketStall** / MARKTSTAND(161) | 0x0084 | 1x1 | 80 | WERKZEUG 1, ZIEGEL 1 t | 4F (22) | offers SCHMUCK (WARE record only) | 1,280 | [0,5] |

Gold, gem and salt mines are **deep** (non-iron: `mine_is_deep` true), so their flagged 2,000 t deposits never deplete. Their placement is Unsupported by default (`mountain_wares` = salt only; T: `salt_placement_test.cpp` "only the listed wares are placed"). SCHMUCK is a household need only from stage 4C (goods table `BauFirst` 19, P). Not surveyed: legal sites for the G types (ore counts only, section 3).

## 2. What the sim ticks today, per type (T/D; `original_class_table`, `types.cpp:998`)

| type(s) | RuntimeClass -> TickKind | what runs | what is missing |
|---|---|---|---|
| 1205 1210 1216 1217 1206 1212 1214 | Mine -> Mine (`mine_step`, gate 500) | extraction, status, deposit re-lookup (`find_deposit`), shallow reserve floor, `Mine::deep` set at create | **placement** (Unsupported except deep SALZSTEIN); shallow iron never placeable (`mine_is_deep` false); research 57 has no source; imported mines are inert (deposit stays nullopt by decision, salt-extraction.md 15.3) |
| 1017 1019 1035 1034 1037 3410 3010 1039 3214 | Workshop -> Workshop (`workshop_step` + carrier job 0x33, figure 203) | ratio-limited production, 1-2 inputs, multi-output vectors | nothing for 1017/1019 (salt X3 proved the class with 1022); 3214's two identical EISEN inputs and two output lines: U; 1034 has `max_figures` 2 (two carriers): untested |
| 3206 | Farm -> Farm (`farm_step`, worker job route 44, figure 400) | farm step on the raw store | the STEINE source interaction ("stone-worker/source interaction must be established", docs/sim/ship-start-bootstrap.md, 804) is U; native-only (`folks` [4], gold 0) |
| 1038 | Cannonworks -> **None** (inert, `0x1006cc40` unread) | nothing | the whole tick; KANONE/MOERSER (110/112) exceed the 56 warehouse goods |
| 1313 and the stalls 1310/1314 | MarketStall -> None (gate 5000) | household purchases read the stall's offer | stall tick body (`0x100702d0`) unread |
| 1301-1305 markets, Kontor | Market -> Market (`0x1006b6d0`, cart figure 220, job 0x34) | sends the cart | - |

**How WERKZEUG reaches the city store (D `docs/re/transport.md` 1.1, 1.3-1.5; class table T).** There is no warehouse building and no output carrier of the toolsmith. The toolsmith (1019) keeps WERKZEUG in its own output store (cap 5 t). A **market cart** (job 0x34, figure 220) from a Market-class building (MARKT 1301-1305 or Kontor) whose search square (centre of the market footprint +- its type radius, 24 for 1301) contains the toolsmith lists it as a candidate when its output is >= 1 t, takes up to the cart capacity (figure 220: 1,280 raw = 5 t) and unloads into the **city store** (`city add`, clipped at the city capacity; endless02 owner 8: 240 t). For goods 1-25 (EISENERZ 1, EISEN 15, HOLZKOHLE 23) the cart only picks a producer above 75 % fill (D 0x1005a9b3); WERKZEUG (41) has no such threshold. The toolsmith's own **carrier** (job 0x33, figure 203, capacity 768 raw = 3 t) only fetches **inputs** (EISEN, HOLZ) from producers or markets in its radius (24) into the workshop's input store. The chain mine -> smelter -> toolsmith: the smelter's carrier pulls ore from the mine's output store (proved for salt, X3: every lift attributed to the saltworks' carrier), the toolsmith's carrier pulls EISEN from the smelter, a market cart moves WERKZEUG to the store. A toolsmith site must therefore lie inside a market's square and inside the carrier radius (24) of the smelter and of a wood source.

## 3. Deposits and the reserve flag (salt-extraction.md sections 12 and 24.3)

- OCCUR ore objects (P, 24-byte ENTRY2; type = bits 14..25 of dword 0): **all 2,261 iron objects in the 50 scenarios (after link resolution) are figure type 1560 (WARE EISENERZ, BEWOHNTYPE 145)**; no object of type 1661 occurs anywhere (P: the types in OCCUR are 1550, 1551, 1560-1565; 1550/1551/1565 are not ore). Other ore objects: 1561 GOLD 68, 1562 SALZSTEIN 150, 1563 EDELSTEINE 48, 1564 MAMORSTEIN 44.
- OCCUR u16 amount: **400 or 2000** (P). Iron: 1,507 objects at 400 and 754 at 2000 in the resolved scenarios. The inline INSEL5 islands alone (independent Python parse) hold 716 x 400 + 754 x 2000; the 465 `Scenes/Islands/*.scp` templates hold 1,578 x 400 + 4 x 2000 iron objects in 359 templates (the random scenarios place these through INSELLNK). Pattern: the add-on files and S01_02/S01_07 carry 2000; base-game files carry 400 (00_1024_easyentry_01 mixes 57 x 400 and 31 x 2000).
- `deposit_load` (src/sim/src/mine_source.cpp, run: T; D 0x100634e0): both 400 and 2000 give **remaining = 512,000 raw = 2,000 t, reserve flag 1** (any amount below 1,600 t is reset to 2,000 t; exactly 1,600 stays 1,600; 3,000 stays 3,000). What a mine sees (`deposit_remaining`): **deep 2,000 t, never consumed; shallow 2,000 - 1,600 = 400 t**.
- The flag: set by the constructor (`+0x30` = 1, D 0x100637a0), neither read nor written by the ore record reader (D 24.3); nothing found clears it for type 1560 (the attach at 0x10063750 clears it only for 1661 on an island of kind 0xd00); the property setter's callers are unread (U). Treat every shipped iron deposit as flagged.
- Behaviour with the flag (T `deposit_extract`; D 0x10063670): deep -> full rate, no change; shallow and `depletes` (ANNOADDON1) -> takes until 1,600 t remain, then `found` is false and the step reports the missing-input idle plus notice 0x15b/0x19; shallow and not `depletes` (ANNO files) -> full rate, no change. A shallow 1205 at the full 21 raw/step uses its 400 t (102,400 raw) in 4,876 steps, about 81 game minutes; a 1216 never.
- Mines reaching one ore share one `Deposit` (T, `bind_mine_deposit`); the lookup takes the largest `deposit_remaining(deep)` among ores in the radius-7 circle (half-widths 7,7,7,6,6,5,4,2), first on a tie (D 0x100787c0; tie order a stand-in).

## 4. Iron ore on every island (P; `probe/T0_ore_by_file.txt`, per-ore lines `probe/ore_legal.txt`)

2,261 iron objects in 50 files, every one with >= 1 terrain-legal mine-footprint site in its radius-7 reach (HIGHMAP rule + coverage + the inherited lo >= 32 step, `placement_rule` with the ware listed, ownership and buildings ignored: the ores are authored on mountains). Per file (islands with iron / objects / at 400 / at 2000): 00..08 SingleEndlos 20-28 islands, 59-121 objects (easyentry_01 has 31 at 2000); endless01 15/66/0/66; **endless02 18/90/0/90**; endless03 2/2/0/2; S01_01 22/77/77/0; S01_02 22/62/0/62; S01_03 22/74/74/0; S01_04 8/14/14/0; S01_05 5/21; S01_06 14/66; S01_07 22/69/0/69; S01_08 1/2/0/2; S01_09 20/67; S01_10 28/90; S01_11 4/11; S01_12 24/74; S01_13 17/31; Single_01..24 2-11 islands, 4-58 objects, all 400; single30-43 1-28 islands, 2-105 objects, all 2000; Tutorial 01Discover01 1/1/1/0; **02Trade01 3/11/11/0**; 03T_Combat 4/5/5/0. Coordinates are absolute world tiles (island origin + trunc(local f32), as `Island::ore_objects`).

Cities with a legal mine site in their own territory (free tiles, owner rule, terrain; `probe/sites_all.txt`, 167 city rows on islands with iron): among the 13 non-isolated scenarios, a human or computer owner with sites exists only in endless02 (island 1 owner 8: 140 (x,y,rot)) and Tutorial/02Trade01 (island 3 owner 2: 138; island 7 city 1 owner 0: 442). All other player-owned rows (S01_*, Single_*, single3x-4x, about 60 rows) are `isolated` (`ContextUnavailable`), so their site counts only matter if script support is added.

## 5. Candidate pilots

### 5.1 endless02 island 1, owner 8 (the salt pilot): has iron, with caveats
- Owner 8: PLAYERMAPPER name "CG1" (a computer seat, kind 12, volk 1), player flags 0xf at start, gold 100,000, city 0 (82 pioneer houses, capacity 240 t). Stock (P): EISENERZ 50 t, EISEN 0, HOLZKOHLE 190, WERKZEUG 190, HOLZ 190, ZIEGEL 190, GOLD 50, SALZ 20, SALZSTEIN 50. Research 57 not held; research 1 and 21 held.
- **Imported buildings (not a zero start):** deep iron mine 1216 at (370,350) rot 3, smelter 1017 at (367,346) rot 0, toolsmith 1019 at (366,350) rot 2, plus the salt pair 1203 (354,367) and 1022 (351,363); 15 markets (1305) incl. (346,346), (321,345), (346,371) and a Kontor; 4 foresters (801 PLANTAGE->HOLZ) at (283..314, 276..285), about 60 tiles from the smelter; stalls 1310/1314. The imported mine has no deposit (inert, T); the imported smelter and toolsmith are live and consume the preloaded 50 t ore (T: store ore 50 -> 35 at step 3000 -> 0 at step 12000), so the baseline term of any iron ledger is non-zero and must be separated as in the salt families Z/P.
- Island 1 (400x400, origin 192,176) has **10 iron ore objects, all 2000 t**: (527,273) (530,278) (530,288) (531,290) (375,353) (379,361) (255,465) (253,472) (260,475) (264,477). Only **two are reachable from owner 8's own free territory**: (375,353) (discovered mask: 1 player) with 124 legal (x,y,rot) mine sites in reach, and (379,361) with 39; the other eight have 0 (the surrounding land is not the city's). Distinct legal (x,y,rot) for the 3x4 mine: **140** (31/39/28/42 by rotation 0..3); the nearest are 7 tiles from the imported smelter ((366,353) rot 0-3, (365,353) rot 1 and 3); every one of the 140 has >= 20 free legal 3x3 spots (the 1017 ordinary rule) within 10 tiles, 193-229 around the nearest. These sites differ from the salt pilot's (353,359).
- Stages (T, live houses): player flags 0xf -> bit 6 (2C) between step 900 and 1000 (mask 0x7f), 0x1ff at 1500, 0x7ff at 2000 (stages 0-10), unchanged to 12,000. 1205, 1210, 1017, 1019 `Allowed` from step 1000 on; 1216/1217 `ResearchRequired`; 1035, 1034 `StageLocked` throughout. Without `house_updates` the flags stay 0xf and 1017/1019/1205/1210 are `StageLocked` (the salt pilot ran in that state; it only needed stage 1C).
- Bills (P), all payable from the stock above: 1205 1,200 gold + 5 t tools + 15 t wood; 1017 800 gold + 10 t tools + 4 t wood + 10 t bricks; 1019 500 gold + 3 t + 4 t + 8 t bricks. Wood: HOLZ 190 t falls to about 20 t by step 3000 (the household mechanism is not established, see the salt doc); the chain's own draw is 1017 3.1 t/min + 1019 0.5 t/min.
- **Blocks (engine, not data):** (a) shallow iron placement (1205/1210) is Unsupported by design; (b) default `mountain_wares` is salt only; (c) 1216/1217 need research 57 which no source gives owner 8. Decision needed: the narrow route places 1205/1210 (the classification must accept a non-deep iron mine; `Mine::deep`, the 1,600 t reserve and notice 0x15b/0x19 already exist in `mine_step`), which yields 400 t per deposit under ANNOADDON1 (endless02 is one); the broader route is a scenario that grants research 57.
- Roads and market reach of the 124/39-site clusters to the imported smelter and markets: U (salt X3 measured it per site and found reach is a property of the site, not of legality).

### 5.2 Best alternative with numbers: Tutorial/02Trade01 (ANNO root, base game), island 3 owner 2
- Whole script retained (`isolated 0`); human seat (kind 8, volk 1); gold 100,000; flags 0x7; stock WERKZEUG 50, HOLZ 50, ZIEGEL 30 t, SALZ 50; 24 buildings; **no** iron or salt chain imported (a real zero start for iron); 6 iron ores at 400 (-> 2,000 t on load): (227,206) (255,207) (220,214) (218,211) (250,210) (212,217); 138 legal mine sites in its own territory. With live houses the flags were 0xf7 by step 2000 (bit 6 set; not bisected): 1205, 1210, 1017, 1019 `Allowed`; 1216/1217 `ResearchRequired`; 1035/1034 `StageLocked` (same blocks (a)-(c) as above). ZIEGEL 30 t pays 1017 (10 t) + 1019 (8 t). ANNO root: no depletion, a shallow mine never exhausts. Drawbacks: only 24 buildings; HOLZ 50 t and no foresters listed (wood supply U); the tutorial script may restrict permissions later (U); markets/Kontor layout not measured.
- Island 7 city 1 owner 0 of the same file: computer seat, flags 0x3f, HOLZ 56, WERKZEUG 70, 442 legal sites, no chain; the flags stayed 0x3f in 3,000 steps (bit 6 not reached).
- The 00..08 SingleEndlos files run whole scripts too but have no player city at start (ships only): iron would follow founding (U).

## 6. Not determined (U)
- Meaning of HEAD byte 0x0b (1205 vs 1210) and 0x0a (1216 vs 1217); `PermissionFacts.folks` beyond "volk list"; workshop input-store capacity.
- The STEINE source of the farm-class 3206 and what feeds it; 3214's duplicated EISEN input; the cannon foundry tick (inert, unread) and how KANONE/MOERSER (above the warehouse range) are stored.
- Who clears the reserve flag, if anyone (OQ-035 b/c; callers of the property setter 0x100636f0 unread). If a shipped path cleared it for iron, a shallow mine would deplete to 0 instead of stopping at 1,600 t.
- Load order of OCCUR versus cities in saves (24.2); `Data/Highmap.dat` vs `Anlagen.dat` precedence (irrelevant here: the iron types have identical tables in `Anlagen.dat`).
- Road connectivity, market coverage and wood supply for the iron sites (not measured); whether Tutorial/02Trade01's script changes permissions later; legal-site inventories for the G types; any in-sim source of research 57.
- Forester output rate (wood supply) was not measured.
- Salt doc 20/15.3 says a stall cart (101) carries the SALZ to the store; the class table says MarketStall -> TickKind::None (T). I traced WERKZEUG through the Market class only; the salt sentence should be checked.

## T0 decisions (executor, 2026-11-07)

- **Pilot: endless02 island 1, owner 8** (a computer seat, `isolated 0`), with `Rules::house_updates` on in the test world so the stage pass reaches 2C (bit 6 between steps 900 and 1000). The mine is the **shallow** 1205 (stage 2C, no research). The deep 1216/1217 need research 57, which no source gives owner 8 (ResearchRequired): they stay a typed refusal. Pilot disclosure: it is not a zero start. It holds an imported inert deep mine 1216 (370,350), smelter 1017 (367,346), toolsmith 1019 (366,350), the salt pair and 15 markets; its preloaded stock is EISENERZ 50, HOLZKOHLE 190, WERKZEUG 190, HOLZ 190, ZIEGEL 190 t. The ledgers keep a **baseline** term for that stock and for the imported smelter/toolsmith, whose take of the 50 t reaches zero by step 12,000 (T); new ore is told apart by its source (the new mine's `mine_made_88`).
- **Alternative pilot (not used first):** Tutorial/02Trade01 island 3, owner 2 (ANNO root, so no depletion; no iron chain imported; 6 ores at 400 t; wood supply unmeasured).
- **Charcoal route: out of scope, typed.** 1035 (3D) and 1034 (3F) need 600 and 1,100 residents at group >= 2; the pilot's live run held stage mask 0x7ff for 20 game minutes. They stay inventory: `StageLocked`, tick class Workshop but unreachable.
- **Gold and gems (1206, 1212, 1214):** stage 4C is unreachable on the pilot; they get a **typed refusal** (`Unsupported`) in T2, covered by tests; part G holds the inventory. All three are deep, so their deposits never deplete.
- **1038 (cannon foundry):** class Cannonworks, TickKind None: inert, out of scope. **3206:** native-only stonemason with ROHSTOFF STEINE and WARE EISEN; its STEINE source is U (Q may answer it); out of scope.
- **Correction recorded:** the shallow floor is 1,600 t (409,600 raw), not 400 t. A shallow mine sees 2,000 - 1,600 = **400 t** of an unspent 2,000 t deposit.

## T1. Contract with hand-derived vectors

Units: 8.8 fixed (raw / 256 = t), step = 500 ticks, tick = 2 ms. All values from the tables above; no id lists in code: every rule below is selected by a type's class, PRODTYPE, output good and `mine_is_deep`.

### T1.1 Deposits (D `0x100634e0`, `0x10063650`, `0x10063670`; section 24.3 of `salt-extraction.md`)

| # | Rule | Hand vector | Distinguishes |
|---|---|---|---|
| D1 | OCCUR u16 `a` loads as `remaining = a << 8` and reserve flag 1; an amount below the reserve is reset to 2,000 t (`0x7d000`) | a = 400 -> 102,400 raw -> reset to **512,000 raw (2,000 t)**; a = 2000 -> 512,000; a = 3000 -> 768,000 (kept) | "the file amount is the usable amount" (wrong: 400 would then be 400 t) |
| D2 | Visible amount: deep = remaining; shallow = remaining - 409,600 | 512,000 raw: deep **512,000**, shallow **102,400 (400 t)** | the 400 t floor read as the reserve |
| D3 | Depletion only when the world setting `depletes` (ANNOADDON1) is on | rate 21 per step on a shallow deposit, `depletes` off: remaining stays 512,000 after any number of steps; on: it falls by 21 per step | edition setting ignored |
| D4 | Shallow exhaustion: full steps of 21 until fewer than 21 remain visible | 102,400 / 21 = 4,876 full steps (102,396 taken), then 4 raw remain: step 4,877 is short (takes 4), step 4,878 takes 0 | off-by-one at the edge |
| D5 | The flag is never cleared by the ore reader (it is read at `0x1006353c`, never written); who else clears it is U (OQ-035 b/c) | the shallow floor holds for every iron ore of OCCUR; no path in the sim clears it | "OCCUR 400 means a shallow-only 400 t reserve" |

### T1.2 The step on a spent or short deposit (A1, A5; D `0x1006df9a`-`0x1006dfb2`, `0x1006e178`)

- Re-lookup trigger: **take < want and remaining != 0**, or **take != 0 and want >= remaining** (exact equality included). Take 0 goes to idle.
- The lookup searches **every ore object** of the island in the radius (section 24.3), not only deposits some mine already bound, takes the largest visible amount (first on a tie) and **keeps the result even when it is spent** (`+0x24` is stored on every call). Our `mine_step` currently searches only the materialised `island.deposits` (A5): **T2 changes this** so a switch to an unbound second ore materialises its deposit and binds it.
- Idle state with `+0x24` empty: scheduled idle (status 1, blocked flag once, notice via city slot `+0x168`). Our `MissingInput` report is the same effect (A5 table).
- Vectors (shallow 1205, `depletes` on, two ores in reach: A visible 4 raw left, B visible 102,400): step with take 4 -> short step -> re-lookup -> binds B (materialised on first bind); next step takes 21 from B. Control: with only A in reach the re-lookup returns A (spent). **The notice fires once, on the short step 4,877** (the step still makes its 4 raw and reports as a normal step; the notice is raised inside the re-lookup branch after the lookup fails, D `0x1006dfb4`-`0x1006e040`, review P2). Step 4,878 and later are silent missing-input idles that never re-look up, so there is no repeat notice; the mine never recovers. The notice is **0x15B** when the deposit still holds deep ore. The same fixture with `depletes` off never triggers either.
- Exhaustion notice: 0x15B when the found deposit still holds deep ore, else 0x19 (`docs/sim/production.md`). The DLL additionally gates it on `[this+0x14]+0xac(0) == 1` (`0x1006dfc7`-`0x1006dfd0`); the sim models that as "the output is EISENERZ", a stand-in (what `+0xac(0)` is stays unestablished).

### T1.3 Placement (extends the 135 path; selected by data, not by id)

- A class-`0x3000` MINE type whose first output good has a loaded ore evidence (figure type with WARE = that good and BEWOHNTYPE 145) places under the same terrain/coverage rule as the salt mine (the HIGHMAP tables of 1203/1205/1216 are byte-identical, P). It is accepted with no deposit test (section 22), charged, and bound by `bind_mine_deposit` or left idle.
- Stage and research gates are the construction-eligibility rules of `d2be23aa`: 1205/1210 need stage 2C; 1216/1217 need research 57. The default `mountain_wares` list stops being a list of ids: it is the set of output goods of MINE types whose evidence holds (salt, iron); gold and gems get the typed refusal of T0.
- Deep/shallow: `+0x21` from the type's research (0x39 -> deep), else deep for every non-iron mine (A1(b), `0x1006e2b1`-`0x1006e2da`).

### T1.4 Processors and the store (D `docs/re/production.md`, `docs/re/transport.md` 1.1, 1.3-1.5; P)

| Type | Recipe per full step (raw) | Output cap |
|---|---|---|
| 1017 | EISENERZ 21 + HOLZ 13 -> EISEN 17 | 1,792 |
| 1019 | EISEN 8 + HOLZ 2 -> WERKZEUG 9 | 1,280 |

- Ratio-limited, not all-or-nothing: the step runs at the fraction the scarcer input allows. Steady-state hand vector with ample inputs: **k** full smelter steps use 21k ore and 13k wood and make 17k EISEN; **j** full toolsmith steps use 8j EISEN and 2j wood and make 9j WERKZEUG. One 21-raw mine can feed one smelter completely; one smelter (17 EISEN/step) feeds 2.125 toolsmiths, so one toolsmith is limited by its own recipe, not by iron.
- Conservation identities for the ledgers (T3), scoped to the pilot buildings: `ore_made(new mine) = ore_carried_in_flight + smelter_ore_consumed + smelter_ore_stock_change`; `EISEN made = EISEN in flight + toolsmith consumed + stock change`; `WERKZEUG made = in flight + market cart delivered + toolsmith output store change`; `store gain = delivered - spent by BuildCmd`. The preloaded and imported terms are the baseline.
- **WERKZEUG reaches the city store by a market cart** (sim code already carries any non-raw good: `transport.cpp:396`-`421` candidates by `available >= 256`, `507`-`550` scoring with the 75 % rule only for goods 1-25 (`transport.cpp:41`), `866`-`880` unload into the city store; proven for SALZ and cloth, so no seam is needed (review (3)). Preconditions the acceptance table checks: the toolsmith lies in a market's square (2r+1 around the footprint centre) and the city store has room for WERKZEUG (the candidate is skipped when `capacity <= stock`)) (job `0x34`, figure 220) from a Market-class building (MARKT 1301-1305 or Kontor) whose square contains the toolsmith, when at least 1 t is present; the toolsmith's own carrier (job `0x33`) only fetches inputs. The 75 % pickup rule applies to goods 1-25 (EISENERZ, EISEN, HOLZKOHLE), not to WERKZEUG.

### T1.5 Identity and delta (written before the first behaviour commit)

- Allocated in the first T2 behaviour commit: **137**, built on 135 (no 136). Delta: (1) accepted-command outcomes: iron mines place; (2) saved state: unchanged fields (Mine::deposit index, Island::deposits are already saved); the step's re-lookup may append a deposit to `Island::deposits` (a saved vector, no new field); (3) content digest: unchanged unless the registry gains a field (none planned); (4) PRNG draws: none. 135 recordings are rejected at 137 explicitly and kept as history, as 135 did for 134.
- A QUESTION is written first if the work needs a new saved field or a stream change beyond this delta.

### T1.6 Acceptance table (claim; evidence; positive control; wrong reading it distinguishes; expected; reviewer)

| ID | Claim | Evidence | Positive control | Wrong reading | Expected | Reviewer |
|---|---|---|---|---|---|---|
| I1 | Shallow iron mine places on a legal site and is refused on an illegal one | HIGHMAP identity (P), section 11 | legal neighbour | "iron needs a deposit test" | accepted/charged vs refused before payment | T1 reviewer |
| I2 | Stage-locked 1205 is refused, an eligible control is accepted | `d2be23aa` | same owner after the stage pass | stage ignored | StageLocked then Allowed | T1 reviewer |
| I3 | Deep vs shallow from research 0x39 | A1(b) | 1216 with research | "all iron deep" | D2 vectors | T1 reviewer |
| I4 | Depletion only in the add-on setting | `0x10063670` option bit | both settings | edition ignored | D3 vectors | T1 reviewer |
| I5 | Spent/short deposit: rebinds to an unmaterialised second ore, else idles with 0x15B/0x19 | `0x1006df9a`, finder | two-ore fixture | search only among bound deposits | T1.2 vectors | T1 reviewer |
| I6 | A rebuild does not refill a depleted deposit | deposit shared state | rebuilt mine on the same ore | rebuild resets the ore | remaining unchanged | T1 reviewer |
| I7 | New ore -> EISEN -> WERKZEUG reaches the city store (toolsmith inside a market square, store not full); a later BuildCmd spends tools | T1.4 | smelter present, market cover checked per site | preload or import counted as production | ledgers close, store rises only with the mine | T3 reviewer |
| I8 | Controls: no mine, no smelter, no wood, no market cover or a full WERKZEUG store each stop the rise | recipe | full chain | store rises from the baseline | store flat above baseline | T3 reviewer |
| I9 | Reload equals uninterrupted at extraction, smelting, delivery and with a spent deposit | continuation | no reload | state lost across save | hashes equal | T3-T5 reviewer |
| I10 | Gold/gems refuse with a typed status | T0 decision | salt/iron accepted | silent acceptance | Unsupported | T1 reviewer |

### T1 review (independent, 2026-11-07)

No P1. Data tables, D1-D5 and the pilot facts confirmed against own parse and bytes. Folded in: P2 notice timing (T1.2), P2 market cover and store room (I7/I8, T1.4); P3: the shallow mine uses its visible 400 t in **4,876 full steps plus a short 4,877th** (about 81.3 min); stage 3D also needs `wissen` 70 (irrelevant, unreachable); the notice gate stand-in is stated; native cities (kind 14) have hundreds of legal sites on other islands but are not player seats (T0 section 4 means player seats, kind 8/12); the salt doc's "stall cart" wording is loose (the cart comes from Market-class buildings; MarketStall is TickKind::None).
