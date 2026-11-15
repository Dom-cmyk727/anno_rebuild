# Shipped-map candidates for the fisher canary

2026-11-15 — FISHER-MAP-1, **READY FOR REVIEW**. Source: accepted SO-1b
`d19c446a867f81fa37b262af298ddd6dc772bbbc`, branch `task/FISHER-MAP-1`.
This is an inventory, not the canary. The orchestrator chooses the map.

**P** = original files parsed; **T** = accepted rebuild compiler/loader or bounded
public-API measurement; **A** = deduction from those results; **U** = not proved.
Reuse [fisher.md](fisher.md) §§3, 5–8, and
[script-objects.md](script-objects.md); no original routine is newly decoded here.

## Method and acceptance boundary

**P/T.** All 50 `.sze` files below `Scenes` have a BIGBROTHER payload. Thirteen have
an empty SCRIPT_KERNEL, not a stripped or suppressed script. Fourteen compile
unchanged with the original registry, 32 player slots, five timers, its goods table
and each file's scene-relative asset namespace. Thirty-six refuse. Implementing
only QUESTLOG_SHOW, GOTO_SCREEN, SHOWTEXT, INHABITANTCOUNT and DEAD would add
**zero** maps: every currently refused map has another blocker.

**T.** Private refusal ladders replace only the refused call/query leaf with a
supported inert diagnostic leaf; they preserve all enclosing conditions and
other leaves. Unsupported object declarations are blanked, not converted into
city definitions. Offset mapping translates every diagnostic back to the original
payload even when replacement lengths differ. Every ladder reaches compilation.
These diagnostic copies are **never executed or committed**. Original read-on
semantics are retained; the sole swallowed statement is S01_09_01's MINIMAP_ALERT
at payload offset 3735. `KILLED†` below is the dependent unknown-object refusal
revealed by blanking an unsupported FIGUR/ANLAGE definition; city KILLED is already
supported. It is not a newly missing city-query handler. The inventory contains
30 action names, 22 query names including this dependency, and two definition
names. Names and original first-use offsets are metadata, not script excerpts.

**T.** Only original, currently compiling inputs enter `load_scenario_file` and
`World::create`. Default strict unit import, original actors, script, positions,
resources, registry and linked-island resolution are retained. Only the existing
`ExtensionContext::fisher_rules.enabled` is set to 1, as required to inspect the
runtime fill. There is **no ActorFreeComponent projection, fallback, deleted
actor, replaced gameplay script, injected stock or ownership change**. Eleven
worlds create; three fail in normal import before World::create. All eleven also
pass ordinary `Recorder::start`, executing the original INIT path.

**T/A.** Classes apply only after compiler and world creation succeed:
A = existing FISCHER hut with an open entrance; B = no such hut, but a demonstrated
single successful public Build command for type 810 with an open entrance;
C = neither. Blocked imports/scripts are `—`, not C. All three original PRODTYPE 6
entries are inventoried: 810/3978 use figure 142, route 45; **3510 uses figure 393,
route 44**. Thus the second A map satisfies the geometry class but its existing
hut does not exercise the new route-45 body. It must not be presented as that proof.

Water means unsigned four-corner mean < 32, usable mask 511, as corrected in
[fisher.md §3](fisher.md#3-figure142-actual-route45-target-and-return).
The original mode store/caller are `0x10002b98` / `0x10038b91`;
mode-3 mask `0x100922cd`–`0x100922df` is blocked bits and goal-pop
`0x100033c3`–`0x100033ce` complements it. Mean 32 remains closed.
The runtime fill is `0x100928a0`–`0x10092b32`: empty raw object word and mean < 16.
These are cited accepted reads, not fresh binary verification.

| Requirement | Positive measurement | Counterexample | Review |
|---|---|---|---|
| Unchanged script admission | 14 compile with full registry environment | S01_02_01 refuses QUESTLOG_SHOW@1339; five names cannot erase its other nine gaps | pending |
| Strict normal import | 11 original worlds create and INIT | 02_1024_easy_128 refuses nested actor state; no projection retry | pending |
| Existing target worker | three 3978 huts use figure 142/route 45 | 3510 uses figure 393/route 44 despite qualifying as a FISCHER | pending |
| One-command construction | human Tutorial/easyentry commands each succeed in fresh worlds | six C worlds have no CityRef and need an additional founding step | pending |
| Open entrance and fish reach | complete pure searches on original water geometry | blocked entrance means 32 or above remain closed; no all-closed adaptation | pending |

## All 50 maps

All rows have a script payload. `empty` means zero kernel items; `active` means
nonempty. `+5` asks whether only the five names above suffice. `N@offset` is the
first refusal; the names column gives the complete ladder set, each at its first
original payload offset. `created` includes successful normal INIT. A dash means
the dependent world/fisher measurement was not run. Paths are relative to Scenes.

| Map | Kernel / compile | Full refusal-name set | +5 | Normal world | Class / huts |
|---|---|---|---|---|---|
| 0010SingleEndlos/00_1024_easyentry_01.sze | empty; compiles | none | yes | created | B / 0 |
| 0010SingleEndlos/01_1024_easy_256.sze | empty; compiles | none | yes | created | A / 3 |
| 0010SingleEndlos/02_1024_easy_128.sze | empty; compiles | none | yes | import refuses: players[7].SCHIFF[0].PRIVAT.CREW.ENTRY[0].PRIVAT.ESEL.TASK: unsupported unit record/state | — |
| 0010SingleEndlos/03_1024_med_256.sze | empty; compiles | none | yes | created | C / 0 |
| 0010SingleEndlos/04_1024_med_128.sze | empty; compiles | none | yes | created | C / 0 |
| 0010SingleEndlos/05_800_Med_128.sze | empty; compiles | none | yes | created | C / 0 |
| 0010SingleEndlos/06_1024_heavy_256.sze | empty; compiles | none | yes | created | C / 0 |
| 0010SingleEndlos/07_1024_heavy_128.sze | empty; compiles | none | yes | created | C / 0 |
| 0010SingleEndlos/08_800_heavy_128.sze | empty; compiles | none | yes | import refuses: players[7].SCHIFF[0].PRIVAT.CREW.ENTRY[0].PRIVAT.ESEL.TASK: unsupported unit record/state | — |
| 0011SingleEndlosAddon/endless01.sze | active; compiles | none | yes | created | B / 0 |
| 0011SingleEndlosAddon/endless02.sze | active; compiles | none | yes | import refuses: players[11].SOLDAT[0]: unsupported actor kind/class/import facts 373 at offset 4700322 | — |
| 0011SingleEndlosAddon/endless03.sze | empty; compiles | none | yes | created | A / 1 |
| 0100Kampagne01/S01_01_01.sze | active; OBJECT_FIGUR@16 | BEAMUP@4251, BOUGHT@3024, CONTAINS@3740, GOTO_SCREEN@580, INHABITANTCOUNT@831, KILLED†@2380, LOST@2449, MINIMAP_ALERT@658, OBJECT_FIGUR@16, PLAYVIDEO@1436, QUESTLOG_SHOW@1121, SETWARELOG@2279, SET_UNIT_VISIBLE@506, SHOWTEXT@2415, TOUCHED_BY_OBJECT@1641, WAR_STATUS@2790, WON@4276 | no | — | — |
| 0100Kampagne01/S01_02_01.sze | active; QUESTLOG_SHOW@1339 | ATTACK_AREA@2055, BEAMDOWN@2097, BOUGHT@5046, DEAD@3750, GOTO_SCREEN@2169, INHABITANTCOUNT@3501, LOST@3832, QUESTLOG_SHOW@1339, SETWARELOG@1634, SET_DIPLOMACY@1827, SET_FOR_BUY@1715, SET_PROFILE@3592, SHOWTEXT@3783, WON@6907 | no | — | — |
| 0100Kampagne01/S01_03_01.sze | active; GOTO_SCREEN@799 | CONTAINS@3261, DEAD@2706, GOTO_SCREEN@799, INHABITANTCOUNT@1490, LOST@2353, MAKES_PROFIT@1459, PLAYVIDEO@1549, QUESTLOG_SHOW@1327, SHOWTEXT@1290, WAR_STATUS@2092, WON@3357 | no | — | — |
| 0100Kampagne01/S01_04_01.sze | active; OBJECT_ANLAGE@16 | DEAD@3000, FREEZE_CITY@1717, KILLED†@4918, LOST@3121, OBJECT_ANLAGE@16, PAID_TRIBUTE_TO@3835, QUESTLOG_SHOW@1235, SETTRIBUTE@1558, SHOWTEXT@3071, WAR_STATUS@3423, WON@5634 | no | — | — |
| 0100Kampagne01/S01_05_01.sze | active; OBJECT_FIGUR@47 | BOUGHT@6569, CHEAT@5012, INHABITANTCOUNT@4405, KILLED†@6147, LOST@3367, OBJECT_FIGUR@47, PLAYVIDEO@3936, QUESTLOG_SHOW@1668, SETWARELOG@1882, SHIPCNT@6115, SHOWTEXT@3331, TOUCHED_BY_SOLDIERS_FROM@3720, WARECOUNT@4653, WAR_STATUS@3196, WON@7499 | no | — | — |
| 0100Kampagne01/S01_06_01.sze | active; SOLDIERCOUNT@769 | LOST@870, SHIPCNT@929, SOLDIERCOUNT@769, TOUCHED_BY_SOLDIERS_FROM@1027, WON@1082 | no | — | — |
| 0100Kampagne01/S01_07_01.sze | active; SET_DIPLOMACY@475 | ATTACK_AREA@754, GOTO_SCREEN@868, INHABITANTCOUNT@1348, PLAYVIDEO@840, SET_DIPLOMACY@475, SET_PROFILE@1111, SOLDIERCNT_FROM@1026, WON@1696 | no | — | — |
| 0100Kampagne01/S01_08_01.sze | active; OBJECT_ANLAGE@16 | ASSIGN_OBJECT@2804, GOTO_SCREEN@3123, KILLED†@1592, LOST@1654, OBJECT_ANLAGE@16, OBJECT_FIGUR@51, OWNER@2036, SET_DIPLOMACY@1228, SET_UNIT_VISIBLE@946, SHOWTEXT@1623, SOLDIERCOUNT@2452, WON@2080 | no | — | — |
| 0100Kampagne01/S01_09_01.sze | active; QUESTLOG_SHOW@1437 | BOUGHT@4299, FREEZE_CITY@1566, INHABITANTCOUNT@3041, LOST@4170, MINIMAP_ALERT@4910, PLAYVIDEO@3197, QUESTLOG_SHOW@1437, SETWARELOG@3576, SET_DIPLOMACY@1610, SET_FOR_BUY@3637, SHOWTEXT@4134, WAR_STATUS@4047, WON@5536 | no | — | — |
| 0100Kampagne01/S01_10_01.sze | active; SET_DIPLOMACY@224 | FREEZE_CITY@340, SET_DIPLOMACY@224, WON@488 | no | — | — |
| 0100Kampagne01/S01_11_01.sze | active; OBJECT_FIGUR@16 | HEALTH@3470, KILLED†@3028, LOST@3183, MOVE_UNIT@1270, OBJECT_FIGUR@16, QUESTLOG_SHOW@1162, SET_DIPLOMACY@1220, SHOWTEXT@3139, TOUCHED_BY_OBJECT@2156, WON@3717 | no | — | — |
| 0100Kampagne01/S01_12_01.sze | active; OBJECT_ANLAGE@16 | KILLED†@2760, OBJECT_ANLAGE@16, QUESTLOG_SHOW@2004, SET_DIPLOMACY@2062, WON@3490 | no | — | — |
| 0100Kampagne01/S01_13_01.sze | active; TOUCHED_BY_SOLDIERS_FROM@276 | TOUCHED_BY_SOLDIERS_FROM@276, WON@395 | no | — | — |
| 0200Single/Single_01.sze | active; QUESTLOG_SHOW@882 | DEAD@1713, INHABITANTCOUNT@1781, QUESTLOG_SHOW@882, SHOWTEXT@1975, WON@2015 | no | — | — |
| 0200Single/Single_05.sze | active; QUESTLOG_SHOW@871 | INHABITANTCOUNT@1647, QUESTLOG_SHOW@871, SHOWTEXT@1856, WON@1898 | no | — | — |
| 0200Single/Single_06.sze | active; QUESTLOG_SHOW@828 | INHABITANTCOUNT@1349, QUESTLOG_SHOW@828, SHOWTEXT@1563, WON@1605 | no | — | — |
| 0200Single/Single_08.sze | active; QUESTLOG_SHOW@871 | INHABITANTCOUNT@1731, QUESTLOG_SHOW@871, SHOWTEXT@2450, WON@2493 | no | — | — |
| 0200Single/Single_10.sze | active; QUESTLOG_SHOW@960 | INHABITANTCOUNT@2111, LOST@1776, QUESTLOG_DRAW@2416, QUESTLOG_SHOW@960, SHOWTEXT@1731, WON@3954 | no | — | — |
| 0200Single/Single_12.sze | active; QUESTLOG_SHOW@821 | INHABITANTCOUNT@1593, QUESTLOG_SHOW@821, SHOWTEXT@1805, WON@1848 | no | — | — |
| 0200Single/Single_14.sze | active; QUESTLOG_SHOW@1033 | LOST@1800, QUESTLOG_SHOW@1033, SHOWTEXT@1754, WARECOUNT@2452, WON@2963 | no | — | — |
| 0200Single/Single_15.sze | active; QUESTLOG_SHOW@981 | INHABITANT_AMOUNT@1753, QUESTLOG_SHOW@981, SHOWTEXT@1958, WON@2001 | no | — | — |
| 0200Single/Single_16.sze | active; QUESTLOG_SHOW@952 | INHABITANT_AMOUNT@1722, QUESTLOG_SHOW@952, SHOWTEXT@1932, WON@1976 | no | — | — |
| 0200Single/Single_24.sze | active; OBJECT_ANLAGE@16 | ADD_SCORE@3120, ATTACK_AREA@1336, ATTACK_OBJECT@2038, DEAD@3010, KILLED†@3504, LOST@3615, OBJECT_ANLAGE@16, PLAYVIDEO@1690, SET_DIPLOMACY@614, SHOWTEXT@3398, WON@3342 | no | — | — |
| 0201SingleAddOn/single30.sze | active; OBJECT_FIGUR@16 | ADD_HEALTH@13350, ASSIGN_OBJECT@6459, CONTAINS@6691, DISASTER_AT@14850, GOTO_SCREEN@1867, KILLED†@27913, KILLPLAYER@11500, LOST@27985, MINIMAP_ALERT@3603, OBJECT_FIGUR@16, PLAYVIDEO@2368, QUESTLOG_DRAW@6111, QUESTLOG_SHOW@2449, SET_PIONIER_ITEM@6376, SET_UNIT_VISIBLE@1932, TOUCHED_BY_OBJECT@7071, TOUCHED_BY_SOLDIERS_FROM@4543, WAKEUP@24329, WON@27851 | no | — | — |
| 0201SingleAddOn/single31.sze | active; OBJECT_FIGUR@16 | ASSIGN_ALL_SOLDIERS@7890, ASSIGN_OBJECT@4609, FREEZE_CITY@1248, GOTO_SCREEN@1155, KILLED†@4206, LOST@4309, MINIMAP_ALERT@9632, OBJECT_ANLAGE@75, OBJECT_FIGUR@16, OWNER@3077, PLAYVIDEO@1306, QUESTLOG_DRAW@8239, QUESTLOG_SHOW@1739, SET_DIPLOMACY@1203, SET_UNIT_VISIBLE@1451, SOLDIERLOST@8571, TOUCHED_BY_OBJECT@9798, TOUCHED_BY_SOLDIERS_FROM@4411, WON@9926 | no | — | — |
| 0201SingleAddOn/single32.sze | active; OBJECT_FIGUR@16 | ASSIGN_OBJECT@2512, GOTO_SCREEN@3635, INHABITANTCOUNT@4971, LOST@1244, OBJECT_FIGUR@16, PLAYVIDEO@1519, QUESTLOG_DRAW@2556, QUESTLOG_SHOW@734, SET_UNIT_VISIBLE@621, SHIPCNT_FROM@2203, SHOWTEXT@1177, WON@4872 | no | — | — |
| 0201SingleAddOn/single33.sze | active; OBJECT_ANLAGE@44 | FREEZE_CITY@1443, GOTO_SCREEN@953, MINIMAP_ALERT@1512, OBJECT_ANLAGE@44, PLAYVIDEO@1005, QUESTLOG_DRAW@4344, QUESTLOG_SHOW@1036, SET_DIPLOMACY@1398, TOUCHED_BY_CONSTRUCTION@3198, TOUCHED_BY_SOLDIERS_FROM@2341, WON@9459 | no | — | — |
| 0201SingleAddOn/single34.sze | active; OBJECT_FIGUR@41 | ASSIGN_ALL_SOLDIERS@5416, ASSIGN_OBJECT@4910, ATTACK_AREA@1296, FREEZE_CITY@3140, INHABITANTCOUNT@4175, MINIMAP_ALERT@2977, MOVE_UNIT@4874, OBJECT_ANLAGE@332, OBJECT_FIGUR@41, PLAYVIDEO@1931, QUESTLOG_DRAW@4577, QUESTLOG_SHOW@2048, SET_DIPLOMACY@3679, SET_PROFILE@1530, SET_UNIT_VISIBLE@2476, SOLDIERLOST@5303, TOUCHED_BY_CONSTRUCTION@3479, TOUCHED_BY_SOLDIERS_FROM@6779, WON@6934 | no | — | — |
| 0201SingleAddOn/single36.sze | active; OBJECT_FIGUR@16 | INHABITANTCOUNT@8689, LOST@9145, OBJECT_FIGUR@16, PLAYVIDEO@6594, QUESTLOG_DRAW@1561, QUESTLOG_SHOW@841, SHOWTEXT@8800, WAR_STATUS@8933, WON@8838 | no | — | — |
| 0201SingleAddOn/single38.sze | active; OBJECT_ANLAGE@16 | ASSIGN_ALL_SOLDIERS@8367, ASSIGN_OBJECT@4605, GOTO_SCREEN@2064, INHABITANT_AMOUNT@10991, KILLED†@6924, LOST@9017, MINIMAP_ALERT@7408, OBJECT_ANLAGE@16, OBJECT_FIGUR@247, PLAYVIDEO@2116, QUESTLOG_DRAW@6608, QUESTLOG_SHOW@2275, SET_DIPLOMACY@2167, SET_UNIT_VISIBLE@3016, SHIPCNT@10930, SHIPLOST@6692, SOLDIERCNT_FROM@8062, SOLDIERCOUNT@11061, TOUCHED_BY_OBJECT@5368, TOUCHED_BY_SOLDIERS_FROM@7883, WAKEUP@7944, WON@11349 | no | — | — |
| 0201SingleAddOn/single39.sze | active; OBJECT_FIGUR@58 | ATTACK_AREA@1926, FREEZE_CITY@912, GOTO_SCREEN@5143, KILLED†@3046, LOST@3076, OBJECT_FIGUR@58, QUESTLOG_DRAW@1684, QUESTLOG_SHOW@1314, SET_FOR_BUY@727, SHIPCNT@5085, WARECOUNT@3280, WON@4995 | no | — | — |
| 0201SingleAddOn/single40.sze | active; QUESTLOG_SHOW@550 | ASSIGN_OBJECT@3352, DEAD@7217, DISASTER_AT@1114, GOTO_SCREEN@4678, INHABITANTCOUNT@13854, INHABITANT_AMOUNT@7433, LOST@3155, QUESTLOG_DRAW@2914, QUESTLOG_SHOW@550, SHOWTEXT@3090, TOUCHED_BY_CONSTRUCTION@7718, TOUCHED_BY_SOLDIERS_FROM@6156, WON@14135 | no | — | — |
| 0201SingleAddOn/single41.sze | active; SHOWTEXT@1139 | BOUGHT@9024, DISASTER_AT@5542, INHABITANTCOUNT@2797, LOST@2700, QUESTLOG_SHOW@4742, SETWARELOG@8831, SET_DIPLOMACY@1178, SET_PROFILE@7986, SHOWMESSAGE@5586, SHOWTEXT@1139, TOUCHED_BY_SOLDIERS_FROM@10413, WAKEUP@7623, WAR_STATUS@2125, WON@2929 | no | — | — |
| 0201SingleAddOn/single42.sze | active; OBJECT_FIGUR@240 | DEAD@1642, INHABITANTCOUNT@2960, KILLED†@1238, LOST@1404, OBJECT_FIGUR@240, QUESTLOG_DRAW@885, QUESTLOG_SHOW@829, SHOWTEXT@1360, TOUCHED_BY_ANYCITY@1075, WON@3427 | no | — | — |
| 0201SingleAddOn/single43.sze | active; SHOWTEXT@323 | GOTO_SCREEN@402, SET_DIPLOMACY@356, SHOWTEXT@323, TOUCHED_BY_SOLDIERS_FROM@483, WON@651 | no | — | — |
| Tutorial/01Discover01.sze | empty; compiles | none | yes | created | C / 0 |
| Tutorial/02Trade01.sze | empty; compiles | none | yes | created | B / 0 |
| Tutorial/03T_Combat_01.sze | empty; OBJECT_FIGUR@16 | OBJECT_FIGUR@16 | no | — | — |

**P/T counts:** 50 payloads; 13 empty kernels; 14 original compilations; 36
original compiler refusals; 11 normal created/started worlds; 3 import refusals;
0 World::create refusals after successful import. Among those 11: **A 2, B 3,
C 6**. Four existing FISCHER huts in two maps; three of them are route 45.

## Existing huts and water reach

**P/T.** Entrances retain original file order and rotation. Values below are
corner means at the entrance tiles. Each hut has exactly one open entrance;
none requires an all-closed-entrance return adaptation. Fish counts are unclaimed
mature tile-11 resources with usable mode-3 bits inside the actual rotated search
rectangle after normal flag-enabled creation. The pure sim WalkCells/search API,
layer 2 and FisherSea mode, finds a path from the original entrance set to a
fish goal for all four, with complete=true and no search error. This proves
existence of a first-leg path, not a delivery or the worker-393 route-44 behaviour.

| Map | Type / owner / kind | Island; hut | Entrance tiles and means, in order | Usable fish in rectangle | Mode-3 path |
|---|---|---|---|---|---|
| 01_1024_easy_256.sze | 3978 / 5 / 16 | 28; 217,326 | 218,329: 16; 218,326: 46 | 53 | yes |
| 01_1024_easy_256.sze | 3978 / 5 / 16 | 28; 212,314 | 212,315: 14; 215,315: 48 | 52 | yes |
| 01_1024_easy_256.sze | 3978 / 5 / 16 | 28; 216,301 | 217,301: 12; 217,304: 42 | 109 | yes |
| endless03.sze | 3510 / 23 / 14 | 4; 36,70 | 36,70: 43; 37,71: 28 | 69 | yes |

All four original Farm payloads are switched on, not suspended or blocked, with
workers_out/raw/output all zero at creation. Owner kind 16 is Venice; kind 15 is
pirates. These are the file's owners, not converted human players.

## Single-command construction witnesses

**T.** Each following witness is a **fresh normal world**, original INIT, then
exactly one Build command: issuer, CityRef and one PlacementWire for type 810.
Both command and record return `None` (success). No preceding rejected command,
founding, unloading, extra money, resource edits or permission override is needed.
Each built hut has one of two entrances open and a complete layer-2 mode-3 path
to usable fish in its real search rectangle. The exploratory coast scan is kept
separate from these fresh one-command confirmations.

| Map | Issuer / kind | Island / city | Placement x,y,rotation | Open entrances | Usable fish | Path |
|---|---|---|---|---|---|---|
| 00_1024_easyentry_01.sze | 0 / 11 (human) | 21 / 0 | 633,471,0 | 1/2 | 82 | yes |
| 02Trade01.sze | 2 / 8 (human) | 3 / 0 | 243,216,0 | 1/2 | 26 | yes |
| endless01.sze | 1 / 14 (natives) | 9 / 0 | 660,289,3 | 1/2 | 68 | yes |
| endless03.sze | 24 / 15 (pirates) | 11 / 0 | 670,647,2 | 1/2 | 107 | yes |

The final row is an additional route-45 construction witness on a class A map,
not another class B: the map already has a type-3510 FISCHER hut.

## Fish totals and the six C maps

**P/T.** Counts are all mature tile-11 resources after normal creation and fill,
then the subset with usable mode-3 goals. They include retained embedded resources
and resolved linked islands. They are not the earlier embedded-only fill census.
No hut means no hut-specific reach rectangle; a large sea does not prove that one
command can create a functioning fishery. The six C worlds have **zero cities**:
a Build command has no valid CityRef. A ship-founded route would need at least
Found and Build, outside the one-command B criterion.

| Created map | All mature fish / usable | Cities | Class |
|---|---|---|---|
| 00_1024_easyentry_01.sze | 188083 / 187414 | 3 | B |
| 01_1024_easy_256.sze | 273360 / 273358 | 1 | A |
| 03_1024_med_256.sze | 258105 / 257728 | 0 | C |
| 04_1024_med_128.sze | 155516 / 155516 | 0 | C |
| 05_800_Med_128.sze | 120531 / 120531 | 0 | C |
| 06_1024_heavy_256.sze | 259309 / 259168 | 0 | C |
| 07_1024_heavy_128.sze | 164897 / 164897 | 0 | C |
| endless01.sze | 177040 / 176620 | 5 | B |
| endless03.sze | 113603 / 113601 | 8 | A |
| 01Discover01.sze | 8486 / 8486 | 0 | C |
| 02Trade01.sze | 22472 / 22472 | 3 | B |

## Ranked shortlist for review

1. **01_1024_easy_256.sze — A, existing route 45.** Three type-3978 huts,
   owner 5/Venice, empty kernel, normal import/create/INIT, open entrances and
   usable fish paths. This needs no construction command and directly uses
   figure 142. The canary must honestly retain the Venice owner and type-3978
   conversion rates; it would not prove human construction or type-810 rates.
2. **02Trade01.sze — B, human type 810.** Empty kernel, normal creation and a
   verified single Build by human player 2. Twenty-six usable fish and an open
   entrance with a path. Prefer this if the intended canary should prove the
   standard human fishery rather than Venice's variant.
3. **00_1024_easyentry_01.sze — B, human type 810.** Equivalent one-command
   human witness (player 0, kind 11), 82 usable fish. Larger loaded world; no
   canary runtime or performance comparison is claimed.
4. **endless01.sze — B.** Original nonempty supported script/INIT; construction
   succeeds for native player 1, 68 usable fish. More script and owner context
   than the empty-kernel human alternatives.
5. **endless03.sze — A geometry, route-44 caveat.** Its sole existing fisher
   is type 3510/figure 393/route 44, not the new route-45 body. A normal one-command
   type-810 alternative is demonstrated for pirate player 24, with 107 usable
   fish, but the earlier choices exercise figure 142 more directly.

## Evidence, limits and stopping boundary

**T.** Bounded probes link the existing Release libraries built for accepted
SO-1b; all tracked src/tools/fixtures inputs equal d19c446a. Local probe sources,
metadata and source/binary identities are retained under `cache/fisher-map-1/`.
The final original-world inventory is `final-world-facts.tsv`; complete original
payload refusal offsets are in `ladders.json`. `normal-facts.tsv` retains the
initial two-type exploratory pass; it missed type 3510 and is superseded by the
explicit three-type inventory, not used for the final A/B counts.

**NOT_RUN:** complete fisher dispatch/catch/return/food-production canary;
long-running script continuations; flag-off digest and determinism gates; native
save/reload canary; Windows/original live gameplay; runtime actor import for the
36 compiler-refused maps. No local test suite, pins check, build or source change
is needed for this documents-only packet. No pin or fixture is changed.

**U / QUESTION:** Which ranked map and owner should FISHER-1e use: the existing
Venice type-3978 huts, or the one-command human type-810 alternative? The owner
requested another map, not a changed script. No map is selected here. SL steps,
BEAMDOWN policy, OQ-603/604 and the all-closed return issue remain untouched.
