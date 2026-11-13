# Fisher farms, route 45 and fish sources (FISHER-RE-1a)

2026-11-13. Documents-only read against base `961a9d4a`; dispatch and the
accepted WHALE-RE-1c review read from `origin/orch/board-next` at `83d55389`.
Independent address verification is pending the orchestrator, **READY FOR REVIEW**.

Reference: GOG AnnoWelt.dll 2.0.0.5, SHA-256
`f220a878510f740c4cfb7224ade0a260e9a0fd7e99113e1201e7a37c0dfa6ae2`.
**D** = original instructions/table read; **P** = parsed original data;
**A** = hand-derived arithmetic; **T** = rebuild code at the base;
**U** = unresolved. Addresses are AnnoWelt unless AnnoGame is specified;
file offsets identify record payloads. There are no original listings or bytes here.

Reuse [whaler.md](whaler.md) §§2/4 and accepted §§6–9 from branch
`task/WHALE-RE-1b` at `109ec14c` (not yet on this base),
[hunter.md](hunter.md) and [wildlife.md §8](wildlife.md#8-hunter-approach-and-the-resource-walk-class-label).
They establish the ship's pursuit/take route, which is **not** figure 142's route.
Shared resource harvesting instead reuses [forestry.md §§2.5–2.13](forestry.md),
[small-farm-supply.md §2.3](../sim/small-farm-supply.md)
and [transport.md](transport.md). These accepted bodies are not newly decoded here.

| Read | Positive evidence | Distinguishing control | Disposition |
|---|---|---|---|
| Data and sources | addressed entries; 50 scenarios and 465 templates read | tile 11 versus figure 511; embedded versus procedural islands | P; orchestrator review pending |
| Class and dispatch | complete tick and fisher creation body; 75-slot comparison | full output does not necessarily stop dispatch; unused draw still consumes | D/A; review pending |
| Figure and route | manager arm, factory selector, jobs, special search/motion | same route number on a ship selects a different class | D; shared reads cited |
| Stops and port boundary | local predicates, mode-3 cells, importer and typed refusal | no mature tile versus no legal sea path; cargo versus output | D/P/T; OQ-603/604 retained |

## 1. Original data, sources and shipped maps

**P — method and identity.** Existing `tools/figuren.py` and
`tools/anlagen_roads.py::parse_anlagen/record_of` read the records.
Anlagen.dat SHA-256 is
`b2317a5c24b8c9fe796cf40f3fedaefcfa30ad3f185df9e69b2dca6a03d8dd75`;
FigurenHeader.dat is
`29fa1f0528fb337d9a61e8b73169281fd05bb27efa6df65638580a4c36c468a8`.
The field conversions reuse AnnoGame `0x100095c7`–`0x10009717` and
`0x1000fe00`, documented in [production.md §§2/5](production.md).

| P: Anlagen entry | HEAD | PRODTYPE | PROD3 | WARE | ROHSTOFF |
|---|---|---|---|---|---|
| 810 | `0x103e3b` | `0x103e83` | `0x103e9b` | `0x103f0a` | `0x103f29` |
| 3978 | `0x24cfed` | `0x24d035` | `0x24d04d` | `0x24d0bc` | `0x24d0db` |
| 10 | `0x76102` | `0x76146` | `0x76161` | `0x761d0` | `0x761ee` |
| 11 | `0x762a4` | `0x762ec` | `0x76305` | `0x76374` | `0x76391` |
| 3506 | `0x207372` | `0x2073ba` | `0x2073d2` | `0x207441` | `0x207461` |

**P/D/A — the two requested farms (addresses above).** Both are PRODTYPE
FISCHER=6, footprint 3×4, radius 7, flags `0x2087`; input FISCH=71, output
NAHRUNG=1. Both have output capacity 5 tons =1280, group-0 figure-type count1,
maximum workers1 and first figure142 (PROD3+0x28/+0x29/+0x32). Entry810 has
T0=15 and I0=4.04296875, converted need69/output17 per 500-tick step.
Entry3978 has T0=5 and I0=2.5, need128/output51. Output uses
`trunc(256/T0 + .5)`; input uses the unrounded quotient times I0, truncated.
A third FISCHER entry3510 uses figure393; it is not evidence that these two
farms use that figure or another factory.

**P/D — figure142.** FigurenHeader HEAD `0x4b5682`, BEWOHNTYPE `0x4b56a2`,
ROUTETYPE `0x4b56ba`, LOGIC `0x4b56d2`: FISCHER kind4, FISCHER route45,
capacity3 tons=768 (LOGIC+0x11), walking layer2 (+0x10), speed f32 about
.018 (HEAD+4), load/harvest wait f32 about11.24 (LOGIC+4).
There is no WARE record. Kind4 is AnnoGame's name-table string at
`0x100261a8` (table `0x10024cb8`); route45 is in its route table
`0x10024fb8` (forestry §2.4). This is a specialized Boden worker, **not a
SHIP/whaler boat in the manager**; its special sea search and movement are
specified in §3. A boat-like presentation does not select ship route code.

**P/D — every fish-source representation found.** Across all921 Anlagen
entries, exactly11 yields FISCH and3506 yields EISFISCH=72. Entry11 is a
ROHSTOFF tile, not a figure or occurrence; its predecessor10 is ROHSTWACHS,
WARE UNUSED. Harvest `0x10054530` -> island `0x10087340` changes11 to10,
then awards256. Entry10 has S5/k2/T0=200: the existing growth rule
`0x1008b5f0` / AnnoGame `0x100088b0` gives P10000, twenty parity flips at
P/2=5000 before10 becomes11. This is regeneration through the ordinary
resource grid, not a fish-population counter or a fish-spawn PRNG.
Entry3506 is an EISLOCH building (goods72), not a mature FISCH71 tile;
its output cannot satisfy the fisher's equality test (§3).

**P/D — fish figures are distinct.** All FISCH kind65 entries in the figure
header are511,1201,1203,1204,1205,1207. Their BEWOHNTYPE/LOGIC payload
addresses respectively are `0x11b0a49`/`0x11b0a76`,
`0x19b1bbc`/`0x19b1bd2`, `0x19c741d`/`0x19c7433`,
`0x19cb0f7`/`0x19cb124`, `0x19d427e`/`0x19d4294`,
`0x19e354e`/`0x19e357b`. None has WARE; route is absent or UNUSED.
Manager `0x1009a110`, kind65 arm `0x1009a6ab`–`0x1009a6d6`, creates
these with `0x1004f5d0`, not figure142's class. Their lifetime is not decoded
here. The actual catch path never looks up or kills one of these figures.
No fish-source occurrence kind is named in the parsed figure-header records;
WALSYMBOL147 belongs to the separate whaling source, not FISCH.

**P — corpus measurement, not runtime tests.** `tools/insel.py` (islands,
blocks) and `tools/sze.py` (scene/tagged/unit-prefix layout) read all50 shipped
Scenes/**/*.sze and all465 Scenes/**/*.scp. Placed ids are masked with
`0x3fff` in the six-byte GELAENDE/FLUESSE/PROD2 items; OCCUR ENTRY2 type
is `(dword0 >> 14) & 0xfff`, matching original loader `0x10095eb0` and
[scenario_formats.cpp](../../src/sim/src/scenario_formats.cpp).
Selected515/read515/failed0/empty0/skipped0. The source census is a static
inventory; it does not claim an original executable or rebuild simulation run.

**P — embedded sources.** There are2473 tile11 placements across24 scenarios;
no tile10 placements. The exact paths/counts below identify the positive maps
without inferring which procedural island a later seed selects.

| Scenario path under Scenes/ | Tile11 count |
|---|---:|
| 0010SingleEndlos/00_1024_easyentry_01.sze | 652 |
| 0011SingleEndlosAddon/endless01.sze | 420 |
| 0011SingleEndlosAddon/endless02.sze | 366 |
| 0011SingleEndlosAddon/endless03.sze | 2 |
| 0100Kampagne01/S01_01_01.sze | 5 |
| 0100Kampagne01/S01_02_01.sze | 4 |
| 0100Kampagne01/S01_03_01.sze | 5 |
| 0100Kampagne01/S01_06_01.sze | 73 |
| 0100Kampagne01/S01_07_01.sze | 5 |
| 0100Kampagne01/S01_10_01.sze | 123 |
| 0100Kampagne01/S01_12_01.sze | 230 |
| 0200Single/Single_01.sze | 5 |
| 0200Single/Single_06.sze | 1 |
| 0200Single/Single_10.sze | 1 |
| 0200Single/Single_15.sze | 1 |
| 0201SingleAddOn/single30.sze | 5 |
| 0201SingleAddOn/single31.sze | 112 |
| 0201SingleAddOn/single32.sze | 1 |
| 0201SingleAddOn/single33.sze | 101 |
| 0201SingleAddOn/single34.sze | 2 |
| 0201SingleAddOn/single36.sze | 146 |
| 0201SingleAddOn/single38.sze | 84 |
| 0201SingleAddOn/single41.sze | 127 |
| 0201SingleAddOn/single42.sze | 2 |

**P — the other source and absence controls.** The25 ice-hole3506 placements
are in endless02(3), endless03(1), S01_05_01(1), S01_13_01(3), single32(1),
single33(2), single34(2), single36(5), single38(3), single41(4), using the
same directory prefixes above. Among465 island templates,29 contain11 and12
contain3506; none contains10. All668 embedded OCCUR records contain only
ERZSYMBOL(1764 entries), WALSYMBOL(621), SCHATZ(18), VULKANSYMBOL(51);
zero FISCH entries. No SPIELER SCHIFF/SOLDAT actor is FISCH kind65.
The nine 0010SingleEndlos maps also have `_dummy.scp` links: these are
procedural placeholders, not missing concrete files. Their eventual selected
islands/fish distribution are **NOT_RUN**, not asserted fish-free.

**T — main's import boundary.** At961a9d4a,
[scenario_formats.cpp](../../src/sim/src/scenario_formats.cpp) `read_island`
keeps known GELAENDE/FLUESSE/PROD2 tile objects in file order, including11;
ordinary growth supports10. OCCUR keeps only ERZSYMBOL145, but the census
found no fish occurrence requiring an additional importer. `types.cpp::figure_type` keeps the figure id/route/capacity/layer, but its
limited BEWOHNTYPE mapping records FISCHER4 and FISCH65 as0, not their
original kinds. Neither a live fisher class nor a FISCH figure simulator is
thereby imported or implemented. EISLOCH uses the base/inert runtime class, not a
ported ice-fishing production tick. Fish tiles already import; that does not
make the refused route functional or settle procedural initialization.

## 2. FISCHER table, tick and creation

**D — table comparison.** Compare75 primary slots at `0x100c2ac4` against
Farm `0x100c8ae0`: exactly two differ. `+0x110` is deleting destructor
`0x100254e0` rather than `0x1006f140`; the fisher thunk reaches
`0x10025590` -> Farm destruction `0x1006f0c0`. `+0x118` is worker creation
`0x10067e60` rather than ordinary entrance creation `0x10072d20`.
Tick `+0x8c=0x1006ea90` and raw receipt `+0xc0=0x1006ee70` are shared.
The 0x38 Farm construction/table overwrite is already in production §3.

**D — complete tick `0x1006ea90`–`0x1006ee5f`.** The u16 elapsed gate
requires500, then advances the stamp by500 once. Off returns; activity ages
by `a*255 >> 8`; suspended flags0x10/0x20 return. With maximum1 and a
worker out, conversion is skipped. Otherwise output at capacity blocks
conversion; input ratio is `min(128, raw*128/need)`, below64 starves,
and a productive step subtracts `need*ratio >> 7`, adds `rate*ratio >> 7`
to u16 output, adds16 times that output to u16 activity. There is no final
output-cap clamp. Shared status/display handling is in production §4.2.

**D — dispatch ordering `0x1006ed78`–`0x1006ee58`.** A single-worker farm
that made something does not dispatch. Count the five non-null worker slots;
require count<maximum and signed-integer `384/rate >= raw/need`. Only then
query group0's type count, consume **one world+0xb4 draw at `0x1006ee06`**,
and discard its value. The figure is always type-list element(0,0),142;
its type `+0x54` supplies45; building `+0x118` creates it. A non-null result
has cargo goods set to input0, FISCH71 (`0x1006ee37`–`0x1006ee56`).
The draw is before creation failure and not a random species/target choice.

**D — complete special creator `0x10067e60`–`0x10067f61`.** Read the
building type's rotated ENTRANCE vectors through `+0x98(flags&3,&buffer)`;
use the **first** pair plus building x/z times1.0 (`0x100c7ff8`) for the
spawn. Y is `0x100e2adc`, initialized by `0x10067d90`, `0x10067df0`,
`0x10067e10` as twice the stored f32(.04*16), minus f32(.35).
Both requested records have a16-byte ENTRANCE record. This is not the
ordinary `0x10072b50` reachable-entrance test. The manager's creation slot0
gets position, figure type, id=-1, time and out-pointer; successful creation
attaches building↔figure and island, obtains figure `+0xa4(45)` and installs
it via `+0xac`. The body has no direct world draw and no local sea-legal
position check. A null route is not converted into a local launch retry.
Shared manager/worker construction is covered by wildlife §5/forestry §2.13;
complete transitive draw accounting remains OQ-604.

## 3. Figure142, actual route45, target and return

**D — concrete figure identity.** Manager `0x1009a110` selector tables
`0x1009a7e8`/`0x1009a774`, kind4 arm `0x1009a36b`–`0x1009a39d`, allocates
0x7c, calls worker constructor `0x1003b7c0` and installs `0x100cb4d8`.
Against the101-slot worker table `0x100c4950`, four entries differ:
`+0x7c=0x100384d0`, `+0x80=0x10038690` (presentation, not decoded here),
`+0xc8=0x10038ad0` (search setup), `+0x17c=0x10038730` (movement).
Inherited tick is `0x10034760`, cargo `0x1003b6d0`, route factory
`+0xa4=0x1003b0e0`, route conversion `+0xcc=0x10034df0`, release
`+0x110=0x1003b6e0`. There is no switch to the whaling ship's factory
`0x10043a20` -> `0x1005df70` (whaler §§4/9).

**D — resolve both selector levels.** `0x1003b0e0` subtracts13 from job45,
reads byte6 from `0x1003b480`, then entry6 of `0x1003b450` selects
`0x1003b314`–`0x1003b36e`. It allocates0x48, calls `0x1005c350`, installs
route `0x100c4340`, search job `0x100c4398`, harvest job `0x100c4858`,
home job `0x100c4640`. The adjacent arm `0x1003b373` is selector10 for
job65, **not** job45; neither that arm nor the constructor's temporary
search table `0x100c4928` defines the installed fisher search.

| D: order / route node | Embedded job / table | Start / completion | Meaning |
|---|---|---|---|
| A, +0x10 | +0x34 / `0x100c4398` | `0x10059210` / `0x1005f710` | kind7, resource search/walk |
| B, +0x1c | +0x3c / `0x100c4858` | `0x100553e0` / `0x10054530` | kind11, timed harvest |
| C, +0x28 | +0x44 / `0x100c4640` | `0x10057ae0` / `0x10057bd0` | kind8, return and credit |

**D — target `0x10059210`–`0x10059398`.** Home footprint and radius form
u16 origin `x+floor((w-1)/2)-r`, similarly y, byte dimensions
`2r+1+(w-1)%2`, similarly h. An unrotated3×4 farm gives origin(x-6,y-6),
size15×16. Query island `+0x10c(rect,0,cargo.goods,buffer)` for unclaimed
FISCH71-output tiles. Zero candidates returns0 before path setup. Goals go
to the specialized search; callback is shared `0x10058a40` with limit20,
not the land worker's0 (`0x1005932d`). Per small-farm-supply §2.3 it keeps
replacing the best unclaimed goal below20, stops at the first with score≥20,
or keeps the last reached if all are below20. Successful search stores the
tile and sets its claim through island `+0xc0(x,y,0,1)`.
This is not a nearest-animal query, pursuit, projectile, death or corpse take;
[hunter.md](hunter.md)/[wildlife.md §8](wildlife.md) supply that contrast.

**D — specialized search `0x10038ad0`–`0x10038cc5`.** It uses the same
home-centred rectangle, obtains the searcher through `+0x190=0x10035d50`
(game kind0xc00,index0), and calls searcher `+0x50(island,rect,3,0)`.
Inside the home footprint it adds the rotated entrance vectors as starts;
otherwise the current position. It resets searcher `+0x80(0)`.
The **3 is cell-building mode, not figure walking layer2**.

**D/A — sea cells and goals, not land cells.** Island `+0x128`, dispatcher
`0x10091fa0`/table `0x10092190`, mode3 arm `0x1009211a` calls
`0x10090560`–`0x100906df`. It reads the corner-height grid at island+0x14,
stride width+1, averages four unsigned bytes by integer division4, and
compares against byte(trunc(`0x100e34dc` / f32(.04))). Initialization
`0x1008fea0`/`0x1008ff00` makes that threshold32 (.04*16, doubled).
Mean<32 produces cell `0x00000fff`; mean≥32 produces `0x00070000`.
Layer2's admission table `0x100d1140 + 2*0x1c00` admits code0 and refuses7.
Goal-mask slot `+0x12c`, mode3 arm `0x1009226b`–`0x100922df`, returns0
below32 and511 otherwise: below-water goals have all nine free subcells.
These bodies have no world draw. Equality at32 is the refused side.
The ordinary mode0 object-mask cell builder cannot substitute for this mode.

**D — movement `0x10038730`–`0x10038a85`.** This worker's segment advance
sets vertical velocity to0 when consuming route segments and retains the
segment's Y instead of following ground height; headings/segment timing and
terminal descriptors still apply. There is no direct world draw in the body.
This does not prove cross-host float continuation equivalence (OQ-603), nor
turn the worker into the sea-ship route/attack machine in whaler §9.

**D/P — catch and conversion.** Shared kind11 start `0x100553e0` uses the
figure type's `+0x64(1,7,time,0)` wait, then its `+0xe0` timed action.
Completion `0x10054530` clears the claim, cuts through island `+0x110`,
and only on success adds256 to cargo. Route arrival `0x1005c260` passes
A's selected tile to B; after B, quantity<type capacity repeats A, otherwise
advances C. Figure142 thus normally needs three successful cuts for768.
A replaced tile can fail the shared predecessor test; no invented fish-kill
fallback awards cargo. Forestry §§2.8–2.10 documents those exact shared calls.

**D — return/receipt.** Shared `0x10057ae0` goes straight to completion if
inside home; otherwise it searches the home's entrances. `0x10057bd0`
credits nonzero cargo goods/quantity to home `+0xc0`, which for FISCHER is
`0x1006ee70`: u16 raw+0x1c adds the quantity, with no clip or goods recheck.
It does not directly credit NAHRUNG or the city warehouse. The Farm tick
converts raw to food (§2); market collection is the already documented
transport path. On route exhaustion the shared worker release/end path
`0x1003af00`/`0x1003b6e0` clears the home worker slot (forestry §2.13).

## 4. Stops, draw ledger and port requirements

**D — no fish.** Zero unclaimed mature goods71 tiles makes search start
return0 (`0x100592fc`–`0x1005930a`); a failed search takes shared route
fallback `0x1005e280` to C. Existing cargo can return; zero cargo makes no
new raw. There is no automatic fish figure/occurrence creation by the fisher.
Tile10 regrowth is the source's ordinary growth, not the failed-search action.

**D/A — full store.** Output≥1280 blocks conversion but can still reach
dispatch. For3978, no worker, raw0 and full output still pass `384/51 >= 0/128`:
one dispatch draw and a creation attempt follow. Raw1023 passes (7≥7),
raw1024 refuses (7<8). For810 the corresponding last admitted raw is1586
(22≥22), next1587 refuses (22<23). Cargo768 controls return, not the output
capacity1280; return can overflow raw's nominal stock policy because receipt
is an unclipped u16 add. Neither “output full immediately stops all fishing”
nor “five-ton output capacity is the boat hold” matches these predicates.

**D — no sea.** No mature fish tile and no legal path are separate failures.
The creator itself checks neither a sea predicate nor reachable entrance
(`0x10067e60`–`0x10067f61`); mode3 search later refuses height means≥32.
If fish goals exist but no route is found, no claim is set and the search
returns0 (`0x10059354`–`0x10059398`), taking the home fallback. This is not
an up-front “no sea, no worker spawned” rule. Whole original-runtime recovery
from malformed/stranded entrances is NOT_RUN; do not invent relocation.

### Draw ledger

**D/U — scope.** Direct world+0xb4 sites on the inspected fisher path are
below. “None” applies to the named body/selected shared branch, not every
transitive callback. OQ-604 preserves that distinction. Creation to removal
is not asserted to have a closed global one-draw total.

| Position / range | Modulus | Selects | Condition |
|---|---|---|---|
| Farm after made/worker/raw checks, `0x1006ee06` | none | nothing; result discarded | dispatch reached, including a later failed creation |
| Special launch `0x10067e60`–`0x10067f61` | none: no direct draw | fixed first entrance offset, not random position | creation attempt |
| Factory arm `0x1003b314`–`0x1003b36e`, route ctor `0x1005c350`–`0x1005c3fa` | none: no direct draw | fixed A/B/C nodes | route45 creation |
| Search `0x10059210`–`0x10059398`, setup `0x10038ad0`–`0x10038cc5` | none: no direct draw | goods71 goals; limit20 callback | each search/retry |
| Mode0 resource query `0x10092320`–`0x1009246d` | none (shared forestry §2.6) | row/column tile list | category0, not random mode1 |
| Mode3 cells `0x10090560`–`0x100906df`; goals `0x1009226b`–`0x100922df` | none | height threshold32 | lazy search cells/goals |
| Wait start `0x100553e0`–`0x10055414`; completion `0x10054530`–`0x100545b8` | none: no direct draw | fixed action7, one cut | reached goal and timed completion |
| Route arrival `0x1005c260`–`0x1005c33e` | none | repeat below768, otherwise home | A/B/C completion |
| Motion `0x10038730`–`0x10038a85` | none: no direct draw | supplied segment | moving worker |
| Return credit `0x10057bd0`–`0x10057c27`; raw receipt `0x1006ee70`–`0x1006ee78` | none | cargo quantity -> raw | home and nonzero goods |
| Cut/growth `0x10087340`–`0x100873ec`, `0x1008b5f0` (shared forestry/production) | none in documented cut/parity rules | 11 ->10 ->11 | successful cut, later growth visits |
| Manager/type/animation/search/event/load callbacks | **U, OQ-604** | not assigned a fictitious zero count | transitive closure not completed |

### Main versus a later faithful port

**T/D — inventory at961a9d4a; requirements, not authorization to change code.**

| Requirement | Already on main / remaining work |
|---|---|
| Farm data, conversion, output and market collection | `production.cpp::farm_step`, FISCHER -> Farm in `types.cpp`, ordinary transport exist; do not duplicate the food recipe |
| Source import and regeneration | `scenario_formats.cpp::read_island` already retains fish11; generic growth/cut model covers10/11. Verify procedural selected-island initialization in the later port; no invented fish-occurrence importer |
| Figure142 identity/dispatch | Extend/verify class facts: the current limited BEWOHNTYPE map reduces FISCHER to0. `figure.hpp::farm_figure_route` explicitly returns Unsupported for0x2d; `driver.cpp::consume` refuses spawn/counter/cargo. Lift only with the real fisher behavior |
| Resource jobs and claims | animal-route goal limit20, cut/wait/repeat/return machinery exists for0x2e; preserve tile goods71, capacity768 and original failed-search fallback |
| Specialized launch/search/motion | add/verify first rotated-entrance launch, mode3 corner-height cells and goals, layer2 admission, Y-preserving motion; ordinary land worker and ship navigation are not interchangeable |
| Draw/state fidelity | preserve conditional discarded dispatch draw; track route node/goal/claim, cargo, home slot, timed action, segments and continuation. OQ-603/604 must delimit any remaining adaptation; this packet changes no state identity |
| Fish figures and ice fish | fish figures are not caught; EISLOCH/goods72 is a different chain. Neither a predator/hunter port nor a fish-figure population is a prerequisite shown by this route |

**A — distinguishing controls for review.** Primary evidence above gives:
499 elapsed does not step;500 advances once. For3978, raw63 gives ratio63
and no food, raw64 gives ratio64 and25 units of food if no worker/out-cap
block. Quantities767/768 select repeat/home. Unclaimed11 matches goods71;
young10 and ice-hole3506 do not. Four corners31 give open mode3 cells;
four corners32 give code7 and no free goal subcells. A full output/raw0
attempt still consumes the dispatch draw, while raw1024 on3978 consumes
none. These hand controls distinguish the named wrong interpretations;
original-runtime experiments/reference vectors and independent re-derivation
are NOT_RUN/pending, not claimed executed assertions.

**U / stopping boundary.** New [OQ-603 and OQ-604](../open-questions.md)
retain exact float/continuation and transitive draw questions;603–610 were
reserved,605–610 unused. HE86, Ghidra, original-runtime fishing and every
engine/test/build gate are **NOT_RUN** by this docs-only packet. Capstone5.0.7
and pefile supplied the new static reads. No engine behavior is enabled.
