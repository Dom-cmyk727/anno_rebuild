# MAP-EXTRACT-2 part G: gold, gems and SCHMUCK (inventory and acceptance table, nothing implemented)

> Status: researched by a read-only agent and not yet independently reviewed (the part E reviewer re-checks it). Inventory and acceptance table only; nothing is implemented.

Worktree `anno1503-MAP-2`, branch `task/MAP-EXTRACT-2` @ 5bb290e5, 2026-11-07. Read-only run; no tracked file changed, game dir untouched.
Tags: **P** parsed from the original data, **D** disassembly note already in `docs/` (module + address), **T** observed by running repo code / reading sim code (file:line), **A** arithmetic from P values, **U** unknown (listed in section 9).
Method: raw records via `tools/figuren.py` (Anlagen WARE/ROHSTOFF/HEAD), `tools/anlagen_roads.py` (id, name, PRODTYPE, flag word), `tools/group_derived_ref.py` + raw `Data/BGruppen.dat` (needs, BAUSTUFEN), registry dump `cache/close-extract/T0/probe/types.txt` (p1) and ore survey `cache/close-extract/T0/probe/survey.txt` (p3, link-resolved OCCUR, all 50 .sze). The T0 probe's `cls=` column is off by one from `Cannonworks` on (it prints `Inn` for 1313); the class below is from `src/sim/src/types.cpp:985`. This file reuses `docs/sim/iron-tools.md` §1.2 and checks every number again.

## 1. Headline

1. Nothing in gold/gems/SCHMUCK is placeable today: `classify` (placement.cpp:41-56) needs the mine's first output ware in `PlacementParams::mountain_wares`, default `{"SALZSTEIN","EISENERZ"}` (placement.hpp:93). GOLD and EDELSTEINE give `Reject::Unsupported`; tests pin this (iron_mine_test.cpp:398-407, salt_placement_test.cpp:1225-1241).
2. Unlocking is a data entry for placement (the ore symbols are in the registry: figure types 1561 GOLD, 1563 EDELSTEINE, BEWOHNTYPE 145), but the chain is **not reachable in any public-command scenario**: stage 4C (bit 19) needs 750 residents at group >= 3 (KAUFLEUTE) plus `Wissen` 90, and the only start states with bit 19 are four S01_07_01 cities, in a file whose script does not compile (`isolated=1`, `ContextUnavailable`).
3. Only group **ARISTOKRATEN** (BGruppen slot 4, house types 580-585) has SCHMUCK as a need (not forced).
4. The sim has no tick for the stall 1313 (class MarketStall, `TickKind::None`); household purchases read the stall's offer flags and the city store.

## 2. Types (P unless marked)

Cost bytes `HEAD[3..6]` = ZIEGEL, HOLZ, WERKZEUG, MAMOR in tonnes (`docs/re/production.md` §2.4); gold = u16 at `HEAD[8]`. Raw units 8.8 (raw/256 = t); step = 500 ticks = 1 s of game time; rate t/min = raw x 60 / 256. Stage index = registry stage (file order of [BAUSTUFEN]: 19 = 4C, 22 = 4F; bit test on the player flag dword +0x90, not ordinal, `docs/re/build-availability.md`).

| id | name (ANLAGEN text) | PRODTYPE / sim class | flag word | fp | gold | goods cost (t) | stage | research | in -> out (raw/step) | rate | out cap (raw) | running [a,b] | hp | radius | entr. | max_figures / figure |
|---|---|---|---|---|---|---|---|---|---|---|---|---|---|---|---|---|
| 1206 | Gold mine | MINE(3) / Mine | 0x3096 | 3x4 | 1,500 | WERKZEUG 15, HOLZ 12, ZIEGEL 4 | 4C (19) | none | - -> GOLD 9 | 2.1 t/min | 1,792 (7 t) | [20,50] | 15000 | 4 | 1 | 1 / fig 101 (cap 768, route 42) |
| 1212 | Gold mine (variant) | MINE / Mine | 0x3096 | 3x4 | 1,500 | same as 1206 | 4C | none | - -> GOLD 9 | 2.1 | 1,792 | [20,50] | 15000 | 0 | 1 | 1 / none (HEAD[0x0b]=1, HEAD[0x11]=0 vs 1206's 0/4) |
| 1214 | Gem mine | MINE / Mine | 0x3096 | 3x4 | 1,500 | WERKZEUG 8, HOLZ 4, ZIEGEL 10 | 4C | none | - -> EDELSTEINE 9 (line 0; table entry 32 for line 1 unused by `mine_step`) | 2.1 | 1,792 | [30,80] | 15000 | 8 | 1 | 1 / none |
| 1039 | Goldsmith | HANDWERK(1) / Workshop | 0x0084 | 3x3 | 300 | WERKZEUG 6, HOLZ 7, ZIEGEL 11 | 4C | none | GOLD 8 + EDELSTEINE 8 -> SCHMUCK 9 | in 1.9 t/min each, out 2.1 | 1,280 (5 t) | [20,40] | 12000 | 24 | 2 | 1 / fig 203 (cap 768, route 51) |
| 3410 | (no ANLAGEN text) native goldsmith | HANDWERK / Workshop | 0x0084 | 3x3 | 0 | HOLZ 5, ZIEGEL 4 | EVER (0) | none; `folks` [6] | GOLD 128 -> SCHMUCK 51 | in 30 t/min, out 12 | 1,280 | [0,0] | 4000 | 8 | 2 | 1 / fig 318 |
| 3010 | (no text) native jeweller | HANDWERK / Workshop | 0x0084 | 3x3 | 0 | HOLZ 6, ZIEGEL 4 | EVER (0) | none; `folks` [2] | EDELSTEINE 128 -> SCHMUCK 51 | same | 1,280 | [0,0] | 6000 | 16 | 4 | 1 / fig 318 |
| 1313 | Jewelry stand | MARKTSTAND(161) / MarketStall | 0x0084 | 1x1 | 80 | WERKZEUG 1, ZIEGEL 1 | 4F (22) | none; `folks` [0,1] | WARE record SCHMUCK (offer only; capacity88 [1280 1280]) | - | 1,280 | [0,5] | 2500 | 4 | 2 | 1 / none |

Time per tonne = time0_128/128 s: 1206/1212/1214 and 1039 30 s per t; 3410/3010 5 s. 1206/1212/1214 share the salt mine's HIGHMAP table (48 bytes, `highmap=48`; 1203/1205/1216 compared byte for byte in the T0 probe; 1206/1212/1214 print the same `highmap=48` and `entrances=1`; byte equality for these three is T0-tagged, not re-diffed here: see U-9).
Every Anlagen record that names GOLD, EDELSTEINE or SCHMUCK in WARE/ROHSTOFF (raw scan this run): 1039, 1206, 1212, 1214, 1313, 3010, 3410. Nothing else consumes SCHMUCK as an input (P).
Chain arithmetic (A): one gold mine (9/step) and one gem mine (9/step) feed one goldsmith (needs 8 + 8/step), 1:1:1, surplus 1 raw/step each; the goldsmith makes 9/step. A goldsmith without the second ore makes nothing (the 2-input rule is `workshop_step`, "1-2 inputs, ratio-limited", iron-tools.md section 2; the exact both-or-either behaviour for 1039 is not asserted here, see G-10).

## 3. Ware ids and encoding (P)

Goods ids (name table, `src/sim/src/goods_table.cpp:30`; `docs/re/production.md` §2): GOLD 2, EDELSTEINE 4, SCHMUCK 37 (WERKZEUG 41, HOLZ 42, ZIEGEL 43). All below the 56 warehouse goods, so the city store holds them (endless02 owner 8 starts with GOLD 50, `survey.txt:990`; SCHMUCK 0, EDELSTEINE 0).
`BGruppen.dat` [WAREN] `Id: SCHMUCK` (file line 157-161): `BauFirst: INFRA_STUFE_4C`, `Preis: 190`, `Bonus: 5, NORD|TUNDRA`, `PreisLevel: 0,10,15`. (WEIN: 4C, 70.)
Stage 4C in [BAUSTUFEN] (file order index 19): `Stufe: INFRA_STUFE_4C, 3, 750` = 750 residents at group >= 3 (KAUFLEUTE); `Wissen: INFRA_STUFE_4C, 90`. 4F (index 22): group 3, 1,900. The `Wissen` reader is not traced (`docs/re/population.md:64,870`): U-2. Stage pass: `docs/sim/population-live.md:83`.

## 4. Ore objects (P, `survey.txt`, OCCUR after link resolution, 50 files)

Figure type 1561 = WARE GOLD, 1563 = WARE EDELSTEINE, BEWOHNTYPE 145 (same class as iron 1560, salt 1562, marble 1564). OCCUR u16 amount is 400 or 2000, same encoding as iron; `deposit_load` maps both to 512,000 raw (2,000 t) with the reserve flag (iron-tools.md section 3, D 0x100634e0).

Totals: **GOLD 68 objects (24 at 400, 44 at 2000); EDELSTEINE 48 objects (19 at 400, 29 at 2000)**. They match iron-tools.md section 3's 68 and 48 and the survey sums.

| file | GOLD obj / islands (400 / 2000) | EDELSTEINE obj / islands (400 / 2000) |
|---|---|---|
| 00_1024_easyentry_01 | 1 / 1 (0 / 1) | 2 / 2 (1 / 1) |
| 01_1024_easy_256 | 3 / 1 (3 / 0) | 1 / 1 (1 / 0) |
| 02_1024_easy_128 | 0 | 1 / 1 (1 / 0) |
| 03_1024_med_256 | 2 / 1 (2 / 0) | 3 / 3 (3 / 0) |
| 06_1024_heavy_256 | 0 | 2 / 1 (2 / 0) |
| 07_1024_heavy_128 | 0 | 2 / 1 (2 / 0) |
| 08_800_heavy_128 | 0 | 1 / 1 (1 / 0) |
| 04, 05 SingleEndlos | 0 | 0 |
| endless01 | 10 / 7 (0 / 10) | 6 / 5 (0 / 6) |
| endless02 | 12 / 8 (0 / 12) | 7 / 6 (0 / 7) |
| endless03 | 1 / 1 (0 / 1) | 1 / 1 (0 / 1) |
| S01_01 | 1 / 1 (1 / 0) | 0 |
| S01_02 | 0 | 1 / 1 (0 / 1) |
| S01_03 | 1 / 1 (1 / 0) | 0 |
| S01_07 | 2 / 2 (0 / 2) | 1 / 1 (0 / 1) |
| S01_08 | 4 / 1 (0 / 4) | 0 |
| S01_10 | 0 | 1 / 1 (1 / 0) |
| S01_12 | 4 / 2 (4 / 0) | 3 / 3 (3 / 0) |
| Single_01 | 3 / 2 (3 / 0) | 1 / 1 (1 / 0) |
| Single_05 | 1 / 1 (1 / 0) | 0 |
| Single_08 | 2 / 1 (2 / 0) | 0 |
| Single_10 | 3 / 2 (3 / 0) | 3 / 2 (3 / 0) |
| Single_12 | 3 / 1 (3 / 0) | 0 |
| Single_15 | 1 / 1 (1 / 0) | 0 |
| single31 | 1 / 1 (0 / 1) | 1 / 1 (0 / 1) |
| single32 | 2 / 1 (0 / 2) | 2 / 1 (0 / 2) |
| single36 | 2 / 1 (0 / 2) | 2 / 1 (0 / 2) |
| single38 / single39 | 0 / 0 | 1 / 1 (0 / 1), 2 / 1 (0 / 2) |
| single40 / single41 | 3 / 1 (0 / 3), 4 / 1 (0 / 4) | 0 / 0 |
| single42 | 2 / 2 (0 / 2) | 4 / 3 (0 / 4) |
| all other files (S01_04/05/06/09/11/13, Single_06/14/16/24, single30/33/34/43, Tutorial/*) | 0 | 0 |

Add-on files (ANNOADDON1) hold only 2000; base files mostly 400; same pattern as iron. Full per-file script: the aggregation of `survey.txt` ORE lines (raw lines in `cache/close-extract/T0/probe/survey.txt`; the per-file table is also in `/tmp/G_ore.txt`).

Players that could use them: of the 13 `isolated=0` scenarios, gold/gems occur on islands with a city only in `00_1024_easyentry_01` island 2 (owner 2, kind 12 computer seat, flags 1, 1 building, `survey.txt:8`). endless02 island 1 (the salt/iron pilot) has **no** gold or gem ore (its gold/gem islands are 2, 3, 9, 23, 30, 33, 41, 45, 48; none has a city). The only cities whose start flags contain stage bit 19 (4C) and 22 (4F) are S01_07_01 islands 38, 41, 42, 51 (owner 0, kind 8 human, volk 1, flags 0x7fffff). Island 38 holds GOLD (762,947) and EDELSTEINE (768,954), both 2000, `disc=1`, and the city starts with WERKZEUG 34 t, HOLZ 51 t (`survey.txt:1528,1529,1533`); island 51 has imported 1313 (1), 1205 (2), 1017/1019/1035/1034 and the salt pair (`survey.txt:1578`). But `sites_all.txt:57`: S01_07_01 is `isolated=1` (script does not compile in the sim): every BuildCmd is `ContextUnavailable`. No gold/gem pilot exists today. Whether the two ores on S01_07 island 38 (distance 6 in x, 7 in y) lie in one radius-7 reach of a legal site: U-5.

## 5. Reserve and depletion: what is shared with iron and salt

Shared (T/D, `docs/sim/salt-extraction.md` §24.3, `docs/re/production.md` 253-273, `docs/sim/iron-tools.md` §3):
- Deposit load: both OCCUR amounts give remaining 512,000 raw (2,000 t) with the reserve flag (`deposit_load`; D 0x100634e0). The ore reader never clears the flag (D 0x1006353c, read only); the constructor sets it (D 0x100637a0). Same ENTRY2 layout, same type bits 14..25.
- Deep/shallow: `Mine::deep` is set at attach: output 0 is EISENERZ -> `deep = (research id == 0x39)`; **every other mine is deep** (D `0x1006e270`; `src/sim/src/production.cpp:736`). So 1206/1212/1214 are deep: visible = full 512,000, `deposit_extract` returns the full rate and consumes nothing (production.cpp:674-690; D 0x10063650/0x10063670). **They never deplete, with or without `depletes` (world setting 14 bit 1).** The 1,600 t floor (`kDepositReserve` 0x64000) and the add-on-only depletion do not apply to them. Same as salt; differs from the shallow iron mines 1205/1210.
- Lookup: radius-7 circle (half-widths 7,7,7,6,6,5,4,2) around the building rect, largest `deposit_remaining(deep)` wins, first on a tie (D 0x100787c0; tie order a stand-in). Only ore objects of the mine's own ware match (`find_ore_in_reach` by output ware, mine_source.cpp:16-70). Same for all mines.
- Re-lookup on short step: only fires when `got < rate` and something is left, or when `rate >= left`; a deep deposit never triggers it, so a deep mine never re-looks up.
- Exhaustion notice 0x15b/0x19: sim raises it only when the output is "EISENERZ" (production.cpp:784); never reachable for deep mines anyway. The DLL gate `[this+0x14]+0xac(0)==1` is unread (U-6).

Differs / unknown for G: whether the reserve flag is cleared anywhere for ore types 1561/1563 (the same OQ-035 b/c question as iron; callers of the setter 0x100636f0 unread): if some path cleared it, a deep mine would still not deplete? No: with the flag cleared the original takes `min(rate, remaining)` (production.md 90), so a cleared gold deposit would deplete to zero. Treat as flagged, as iron (T0 decision). U-1.

## 6. Population: who needs SCHMUCK and where it is encoded

**Only ARISTOKRATEN.** Evidence:
- `Data/BGruppen.dat` [BGRUPPE]: the single `Ware: SCHMUCK` in the five group sections is file line 536-541, inside `BGruppeNr: ARISTOKRATEN` (P, raw text; `Menge: 0.3`, `Priority: NORMAL`, `Force: FALSE`). A line scan this run printed exactly one hit: `ARISTOKRATEN 540 ['SCHMUCK','0.3','NORMAL','FALSE']`.
- `tools/group_derived_ref.py` (independent oracle for the derived tables): only the ARISTOKRATEN row has `37` in its lookup: `group ARISTOKRATEN n 8 goods_end 4 infra_end 8 forced 77,78,83,85 lookup 26:0,36:1,37:2,39:3,77:5,78:6,83:4,84:4,85:7`. Need order: NAHRUNG(26) 0, KLEIDUNG(36) 1, **SCHMUCK(37) 2**, WEIN(39) 3, then infrastructure KIRCHE, BADEHAUS, THEATER, PARK. PIONIER, SIEDLER, BUERGER, KAUFLEUTE rows contain no 37.
- Not forced (`Force: FALSE`): it does not enter the mood and the upgrade check; it matters for demand and income (`docs/re/population.md:153,161`: not-forced needs affect only the upgrade check and demand; money comes from consumer goods at the good's price + tax surcharge). Tutorial V14 06:26 agrees: "jewellery and wine are optional extra income" (`docs/research/tutorial-videos.md:69`).
- House types: Anlagen entries with PRODTYPE ARISTOKRATEN are **580-585** ("Aristocrats", flag word 0x0084; `tools/anlagen_roads.py` output this run); the group comes from the type (PRODTYPE 101 -> group slot 4, `population.md:34`, `docs/formats/sze.md:178`).
- Availability: SCHMUCK `BauFirst` 4C (stage 19) and the stall 1313 stage 4F (22); ARISTOKRATEN `BauFirst` is 5A, so aristocrat houses exist only after the 4C/4F stages, i.e. with 750+ / 1,900 KAUFLEUTE-or-higher residents. The group needs `NoUpgrade`/`NoDowngrade` TRUE, so a house is created directly (`SetGroup` first assignment; `population-live.md`, `population.md:167`), not reached by an upgrade in the sim's current houses (U-7).
- BEWOHNTYPE: BEWOHNTYPE 145 (figure-type field) is the ore symbol class (GOLD/EDELSTEINE/etc.), not a population group; groups are not encoded by BEWOHNTYPE. There is no per-house-type SCHMUCK field in Anlagen (raw scan: no NEEDS/BEDARF record exists in Anlagen; the need lists are in BGruppen only).
- Natives: 3410 (`folks` [6]) and 3010 (`folks` [2]) produce SCHMUCK; `folks` is the volk list in the permission record (meaning beyond "volk list": U-3). No native house group consumes it in BGruppen.

## 7. What the sim does today, per type (T, file:line)

| types | placement class | construction | tick | note |
|---|---|---|---|---|
| 1206 1212 1214 | `Class::Other`/Unsupported because the ware is not in `mountain_wares` (placement.cpp:41-56; default placement.hpp:93); the `0x10` coverage bit makes the fallthrough at placement.cpp:58-62 Other too | `construction_eligibility` is never reached: apply.cpp:421 comes after the placement refusal (the test comment, iron_mine_test.cpp:398-399: "the placement rule precedes the permission step"), so the observed status is `Unsupported`, state byte-identical | `RuntimeClass::Mine` -> `TickKind::Mine` gate 500 (types.cpp:1006; driver.cpp:358 `mine_step`) | imported mines are inert (deposit stays nullopt, salt 15.3); `Mine::deep` true via `mine_is_deep` (production.cpp:736) |
| 1039 3410 3010 | not mountain; ordinary class (flag 0x0084, `ordinary_class` 0x0004 | ignored 0x0080) -> placeable on legal land; stage 4C `StageLocked` without bit 19; the natives' `folks` != player volk (U-3) | `Workshop` -> `workshop_step` + carrier job 0x33 (types.cpp:1001; driver.cpp:278) | 1039's two inputs: ratio-limited multi-input path used by the smelter; GOLD+EDELSTEINE untested in the sim |
| 1313 | ordinary 1x1; stage 4F | `MarketStall` -> `TickKind::None`, gate 5000 (types.cpp:985, 1015; D 0x100702d0 unread) | none | household purchase reads `supplier_available`: city store `[g]`, enabled flags from the WARE record (resident.cpp:178-206) |
| 580-585 houses | House class | n/a | `TickKind::House` 2500 | SCHMUCK slot 2 comes from the loaded BGruppen (house_groups.cpp), consumption path = `supplier_*` above |

The synthetic registry builds GOLD deposits (synthetic.cpp:239) and GOLD tests (create_remove_test.cpp:286-295, production_test.cpp:567-640) exist, but none exercises the original GOLD/EDELSTEINE/SCHMUCK types.

## 8. Acceptance table for a later implementation task (G-1..G-17)

Each row: observable, control (what must still hold / the adjacent case that behaves differently), mutation (run it; the row must fail). Identity bump: the first commit that changes behaviour allocates a new identity; rows G-1/G-2 are the only ones with a documented current behaviour.

| row | observable | control | mutation (must fail) |
|---|---|---|---|
| G-1 today's refusal (before) | BuildCmd 1206, 1212, 1214 on the site where 1205 is accepted -> `Reject::Unsupported`, `w.save()` unchanged (iron_mine_test.cpp:398-407); `rule(...) == Unsupported` (salt_placement_test.cpp:1225-1241) | the same site accepts 1203/1205; listing another ware does not unlock | add "GOLD" to the default list but keep the test: the test must fail |
| G-2 placement by data | with `mountain_wares` containing the mine's output name, `placement_rule` is `None` for 1206/1212/1214 on a legal mountain site; empty list refuses everything | 1203/1205 still accepted; removing the ware's figure type (BEWOHNTYPE 145) from the registry -> Unsupported (has_ore_evidence) | drop `has_ore_evidence`: a registry without GOLD ore symbols would accept; the control must catch it |
| G-3 stage gate | flags without bit 19: `StageLocked` for 1206/1212/1214/1039; with bit 19: `Allowed`; 1313 needs bit 22, not 19 | bit 19 set alone does not open 1313; bit 22 alone does not open 1039 | replace the bit test by `flags >= 1<<19` (ordinal): 1313 test with 0x7ffff... differs |
| G-4 bills | 1206: 1,500 gold + WERKZEUG 15 t + HOLZ 12 t + ZIEGEL 4 t; 1212 same; 1214: 1,500 + 8 + 4 + 10; 1039: 300 + 6 + 7 + 11; 1313: 80 + WERKZEUG 1 + ZIEGEL 1; exact deltas in city store and money | HEAD byte order (ZIEGEL[3] HOLZ[4] WERKZEUG[5]): swap two columns changes 1206 and 1214 differently (4/12/15 vs 10/4/8) | swap HEAD[3] and HEAD[5] reads: the 1214 and 1206 bills both fail |
| G-5 deposit bind and amounts | a gold mine near a GOLD ore (OCCUR 400 or 2000) binds a deposit of remaining 512,000, reserve flag 1, deep visible 512,000; a gem mine binds only EDELSTEINE ore, never GOLD | a GOLD ore and an EDELSTEINE ore both in reach: each mine takes only its own ware; two gold mines on one ore share one `Deposit` | remove the ware filter in `find_ore_in_reach`: the gem mine would bind the gold ore |
| G-6 mine rate | a running 1206/1212 raises its output store by 9 raw per 500 ticks (not 21); 1214 by 9; stops at 1,792 raw with `StorageFull` | an iron mine on the same world makes 21 | use the iron rate: the first 10 steps give 210 raw vs 90 |
| G-7 no depletion | `depletes` on (ANNOADDON1 world) and off: after 10,000 steps (> 4,876 iron-shallow exhaustion point) the deposit is still 512,000 minus nothing; `found` stays true; no notice; the mine never goes idle | a shallow 1205 on the same world stops at 102,400 visible (4,876 steps) | make `mine_is_deep` iron-only-by-name wrong (treat gold as shallow): remaining falls to 409,600 and the mine idles at step ~4,877 |
| G-8 no exhaustion notice for non-iron | no 0x15b/0x19 for GOLD/EDELSTEINE mines at any step (deep: unreachable) | iron shallow raises it once on the short step | widen the `goods == "EISENERZ"` stand-in to all wares: a forced short step on a synthetic shallow gold deposit would raise it; flag as U-6 until the DLL gate (`+0xac(0)`) is read |
| G-9 goldsmith recipe | 1039 with both GOLD 8 and EDELSTEINE 8 per step makes 9 SCHMUCK per step; consumed 8 + 8; output cap 1,280 raw -> StorageFull | with GOLD only (or EDELSTEINE only) the observed behaviour is pinned as data: expected 0 made (to be confirmed against the workshop code before asserting; U-4) | make the recipe either-or (consume one): output with one ore would be non-zero |
| G-10 natives 3410 / 3010 | refused for a player of another volk (typed refusal) or placeable only by the `folks` rule; rate GOLD 128 -> SCHMUCK 51 per step if an imported native building runs | player 1039 unaffected | ignore `folks`: a volk 1 player could build 3410 |
| G-11 transport of ores | the goldsmith's carrier (job 0x33, fig 203) lifts GOLD and EDELSTEINE from the two mines' output stores within its radius 24 and delivers to the input store; the mine's own lift attributable to the goldsmith (salt X3 ledger pattern) | a mine placed at distance > 24 is not served | set the carrier radius to 8: the far mine is not served |
| G-12 SCHMUCK to the city store | a market cart (Market class, job 0x34, fig 220) takes SCHMUCK from the goldsmith's output (>= 1 t) and unloads into the city store; no 75 % threshold (goods > 25; threshold is for 1-25 only, D 0x1005a9b3, iron-tools.md §2) | EISEN (15) is held back below 75 % while SCHMUCK is not | apply the 75 % rule to all goods: SCHMUCK would stay under 75 % |
| G-13 jewellery stall 1313 | `MarketStall` supplies SCHMUCK from the city store to a house whose group needs it (`supplier_available` = `city_store[37]`; enabled flag from the WARE record: `flag[0]` for output 0, `flag[1]` otherwise, resident.cpp:203-206) | a stall 1310/1314 does not offer SCHMUCK; goods ids from `service_mask_for_goods` do not apply (infra range only) | enable the wrong flag: a stall would serve a good not in its WARE record |
| G-14 ARISTOKRATEN demand | a house of 580-585 draws SCHMUCK in need slot 2 (Menge 0.3 x residents), price 190 + tax level; its absence does NOT lower mood and does NOT block the upgrade check (`Force` FALSE); presence raises income | KAUFLEUTE/BUERGER/SIEDLER/PIONIER draw none (their lookup has no 37) | set `Force` TRUE in the fixture: the forced list (`forced` mask) changes -> mood moves; the oracle `tools/group_derived_ref.py` check catches it |
| G-15 save / reload / replay | a world with 1206+1214+1039 running at step N: save, load, continue -> digest identical to an uninterrupted run; replay of the recorded commands reproduces it; fresh-process replay file as in `cache/close-extract/fresh_process_replay_*` | the same for the iron chain | drop the deposit binding from the save: the reloaded gold mine would stay inert |
| G-16 ledgers | per-ware ledgers separate imported baseline stock/buildings from the chain's own production (S01_07 island 51 imports 1313 and the iron/salt chain; endless02 imports iron and salt) | exact-cause attribution as in salt X3 | omit the baseline: the ledger fails to close |
| G-17 pilot / context | a scenario whose script compiles and whose owner holds bit 19 (and a gold + gem ore in reach) exists, or the task adds a scenario/grant path; until then every BuildCmd in S01_07 is `ContextUnavailable` | endless02 owner 8 (stage mask 0x7ff, no gold/gem ore on island 1) still refuses 1206 with `StageLocked`/`Unsupported` | grant the flag by hand in a test world only and label it synthetic |

Existing coverage to reuse: iron_mine_test.cpp, iron_chain_test.cpp, `iron_pilot.hpp`, salt_placement_test.cpp, `tools/agree.py` for the ported dumps.

## 9. Open questions (U)

- U-1: whether any shipped path clears the reserve flag for ore types 1561/1563 (OQ-035 b/c; setter callers of 0x100636f0 unread). Treated as flagged, as iron.
- U-2: the `Wissen: INFRA_STUFE_4C, 90` reader (`population.md:64,870`: use not traced). Does it gate the stage, the research, or neither? The stage pass at `population-live.md:83` reads only group and count.
- U-3: meaning of `PermissionFacts.folks` beyond "volk list" (3410 [6], 3010 [2], 1206/1212/1214/1039/1313 [0,1]); whether a volk-1 player may build the native smiths.
- U-4: workshop behaviour for a two-input recipe when only one input is present (1039) and the input-store capacity; the sim's `workshop_step` has multi-input tests for other types but not GOLD+EDELSTEINE.
- U-5: whether S01_07 island 38's GOLD (762,947) and EDELSTEINE (768,954) both fall inside the radius-7 reach of one legal 3x4 site; legal-site inventories for G types were not run (iron-tools.md §6 lists the same gap). No road/market reach, no wood supply measured.
- U-6: the exhaustion notice gate `[this+0x14]+0xac(0)==1` (D 0x1006dfc7-0x1006dfd0); the sim's "output is EISENERZ" is a stand-in. Not reachable for deep mines.
- U-7: how an ARISTOKRATEN house is created in the sim (NoUpgrade; scenario houses or a direct placement). The population doc covers PIONIER -> SIEDLER transitions; the aristocrat consumption path (stall -> house, SCHMUCK slot 2) is data-complete but unexercised.
- U-8: 1212 (radius 0, no figure, HEAD[0x0b]=1, HEAD[0x11]=0) vs 1206 (radius 4, fig 101, HEAD[0x0b]=0, HEAD[0x11]=4): meaning of the byte differences, and whether `radius` here changes anything for the mine (the deposit lookup uses the fixed radius-7 circle in the sim). Same family question as 1205/1210.
- U-9: the HIGHMAP bytes of 1206/1212/1214 were compared with the salt table only through the registry (`highmap=48`); a byte-for-byte diff for these three was not re-run here (done for 1203/1205/1216 in T0).
- U-10: the stall tick body (D 0x100702d0 unread); whether the jewellery stall's cart behaviour (salt doc 20/15.3) applies; SCHMUCK Bonus `NORD|TUNDRA` climate effect on price/demand (reader not found, population-inputs.md section 1 "Out").
- U-11: whether 1039's SCHMUCK needs roads and a market within 24 (not measured).
- Native buildings 3410/3010 are named in Anlagen without an ANLAGEN text entry; their placement for players is outside this inventory.
