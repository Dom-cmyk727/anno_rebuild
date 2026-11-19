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
below32 and511 otherwise.
**D/A — correction, 2026-11-13 (FISHER-RE-1c fix1):** the prior wording
“below-water goals have all nine free subcells” inverted that result.
`AnnoWelt.dll 0x10090689`–`0x10090697` yields walkable layer2 cell0x00000fff
for mean<32 and closed0x00070000 otherwise; goal-mask
`0x1009226b`–`0x100922df` instead yields0 below32 and511 at/above32.
Thus mean31 means open1/free0; mean32 means open0/free511.
A fish goal and the water cell used to approach it are distinct cells.
These bodies have no world draw. Equality at32 is the refused walking side (fix1 clarification).
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

### Return search cell mode — 2026-11-14, FISHER-1b

**D — return uses the fisher override, hence mode 3.** In AnnoWelt.dll
`0x10057ae0`–`0x10057bbe`, this is the shared home job and its figure argument
is the primary figure interface. An already-inside-home figure returns success
at `0x10057b68`–`0x10057b73` without a search. Otherwise `0x10057b79`–
`0x10057b7d` calls the figure's virtual `+0xc8(0)`. The fisher's primary
table `0x100cb4d8`, slot `+0xc8`, is `0x10038ad0`. That override constructs
the home-centred rectangle, obtains the searcher at `0x10038b4e`, and
initializes it with cell-building **mode 3** at `0x10038b91`–`0x10038ba1`.
It supplies home entrance starts while inside the footprint, or the current
position otherwise (`0x10038bb0`–`0x10038cc5`). The home job then adds home
entrance goals through home `+0xf8` at `0x10057b87`–`0x10057b94`, installs
the completion callback, and calls figure `+0xcc` at `0x10057ba8`–`0x10057bb1`.
It does not replace that searcher's cells with mode 0. The full two bodies
were read; neither contains a direct world `+0xb4` draw. This settles the
cell-mode dependency, not OQ-603's float continuation.

**T — planned planar boundary.** The rebuild has no vertical figure coordinate.
A port can reuse its planar integer segment representation without inventing
terrain-height following or a synthetic Y adjustment. This checkpoint implements
no fisher movement and makes no original Y/float-continuation equivalence claim
(OQ-603).

### Goal-mask consumer contradicts the port premise — 2026-11-14, FISHER-1b

**D — interface chain and resource goals.** Let `S` denote the search interface
returned to the fisher (the `0x100c14e4` interface at `Y + 4`, not `Y`). Its
`+0x50 = 0x10002b80` stores the selected mode at `S + 0x2040`
(`0x10002b80`–`0x10002b98`); the fisher passes mode 3 as above. Its `+0x78 =
0x10002e20` copies the resource-query tile pairs into `S + 0x1800`, with count
at `S + 0x17fc` (`0x10002e20`–`0x10002e60`). The fisher uses that slot at
`0x1005931b`–`0x10059325`, after the nonempty query. Search `+0x84 =
0x10003d90` calls goal-pop `0x10003220` with the same `S` receiver at
`0x10004779`–`0x1000477b`. These offsets are relative to `S` throughout.

**D — the missing consumer operation.** In the complete goal-pop read
`0x10003220`–`0x1000377d`, the resource-tile loop compares the popped tile
with each stored resource pair (`0x10003399`–`0x100033aa`). On a match,
`0x100033ac`–`0x100033bd` calls island `+0x12c`, passing the mode from
`S + 0x2040` and that tile. Crucially, `0x100033c3`–`0x100033ce` computes
**`0x1ff - returned_mask`** and ORs that into the accumulated usable goal mask.
The island's primary `0x100cac2c` slot `+0x12c = 0x100921b0` dispatches
mode 3 to the accepted mean/threshold arm `0x1009226b`–`0x100922df`.
Thus its returned 0/511 is an **exclusion mask for this consumer**, not the
usable/free mask. The callback-order loop at `0x1000370c`–`0x1000374c`
tests each accumulated bit before calling the callback.

| A, from these D ranges; threshold 32 | Walking cell | Returned island mask | Usable resource-goal bits |
|---|---|---|---|
| Mean 31 | open | 0 | 511 |
| Mean 32 | closed | 511 | 0 |

**T/U — contract conflict, not an implementation change.** FISHER-1a's
`fisher_cell.goal_mask` and `fisher_ref.py sea_cell.free` retain the returned
island mask values; they were not changed in this checkpoint. The latter's
`free` label and the earlier census interpretation as 2473 *usable goals*
miss this inversion. The 50/2473/619/1854 measurements still describe returned
mask polarity and local shoreline geometry, not catchability. The FISHER-1b
packet's requested closed shallow-goal synthetic fixture cannot be justified
by treating 511 as usable. No dry-goal admission exception is invented and
mode 0 is untouched. This is a QUESTION for correcting/releasing the port
contract, references and fixtures; full original-runtime reachability and
OQ-609 remain open. The existing runtime-source lead `0x100928a0` (mean below
16) is compatible with the consumer polarity, but is not ported or expanded here.

**D/A — accepted correction, 2026-11-14 (FISHER-1b question review).**
The orchestrator independently re-checked this consumer chain. The island answer
is now named `blocked_mask`, with `usable_mask = 0x1ff - blocked_mask` in
FISHER-1a's primitive. The reference outputs `blocked` and `usable`; all six
boundary vectors keep their raw values and hand-derive the usable bits. The
previous statement that mean 32 is a usable goal is superseded: equality is
closed to walking **and has zero usable goal subcells**. The on/off draw scope
is the owner's per-eligible-attempt decision below.

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
an up-front “no sea, no worker spawned” rule.
**D/A — clarification, 2026-11-13 (fix1):** “refuses height means≥32” above
refers to walking cells (`0x10090689`–`0x10090697`), not goal admission:
the goal mask admits those means (`0x1009226b`–`0x100922df`). A closed
walking tile can therefore be a fish goal. Whole original-runtime recovery
from malformed/stranded entrances is NOT_RUN; do not invent relocation.

**D/A — no-sea/goal wording corrected, 2026-11-14.** The earlier sentence
“a closed walking tile can therefore be a fish goal” missed goal-pop's inverse
at `0x100033c3`–`0x100033ce`. For this mode-3 resource path a usable fish goal
must itself have mean below threshold. No closed-tile exception is implied.

### Draw-contract clarification — owner decision, 2026-11-14

**Owner decision / D/T scope.** Preserve exactly one discarded world draw per
eligible farm dispatch attempt (`0x1006ed78`–`0x1006ee58`, §2) and add no
fisher-body draw. Test paired identical eligible attempts and isolated trip-body
execution. This does **not** require identical full flag-on/off draw trajectories:
a fisher still away occupies its home worker slot, while a flag-off refused send
does not. The next count gate can therefore suppress one world's dispatch and
allow the other's. Do not add dummy draws or alter the flag-off stream to hide
that state-dependent divergence. The owner explicitly accepted this scope;
the separate usable-goal contract question remains open.

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

## 5. HEIGHT corner grid and water census (FISHER-RE-1c, 2026-11-13)

**D — receiver and allocation.** This section resolves the load-side part of
OQ-200, extending [annowelt.md's earlier grid read](annowelt.md#height-grid-is-per-corner).
`I` is the island base: the primary table installed at `0x10094c30` is
`0x100cac2c`, whose `+0x180` is allocation/initialization `0x1008c090`.
The other two table references to that same initializer are `0x100ca31c`
and `0x100ca6b0`; no secondary-interface displacement is added to I.
Width/height are unsigned words at I+8/I+0xa; I+0x14 is the corner buffer,
**not** the eight-byte tile records at I+0xc or GFXID words at I+0x10.
`0x1008c18a`–`0x1008c1d3` allocates **(W+1)*(H+2) bytes** and zeroes the
whole allocation (dword loop plus byte remainder), then stores its pointer
at I+0x14. Logical corners occupy (W+1)*(H+1); the further row is allocation
padding, not an additional row of tiles or an input HEIGHT row.

**D/P — exact fresh-load mapping.** Let F be the assembled row-major W*H
HEIGHT samples, as in [insel.md, Blocks](../formats/insel.md#blocks).
For local corner coordinates 0<=cx<=W, 0<=cy<=H:

| Corner | Value after the shipped HEIGHT blocks load | Evidence |
|---|---|---|
| cx<W and cy<H | F[cy*W+cx], copied unchanged | `0x10089130` HEIGHT arm; `0x1008499e`–`0x100849a5` |
| cx=W or cy=H | 0, the allocation's initial value | allocation `0x1008c18a`–`0x1008c1d3`; file block coverage W*H |
| extra physical row cy=H+1 | initialized zero; outside the logical corner-grid result | same allocation, no shipped block covering it |

There is **no mean, interpolation, scale, offset, edge replication or neighbour
island lookup in this mapping**. The means in §3 belong to consumers of four
already stored corners. This settles the previously “Likely” copy/border portion
of annowelt.md without rewriting its accepted historical text.

**D — file-to-buffer chain, including block boundaries.** Loader
`0x10089150`–`0x1008922a` recognizes HEIGHT at string `0x100dcf1c`, reads its
six-byte block header, adds the island's world origin to the header x/y
(`0x100891a0`–`0x100891b1`), reads the block samples, and calls island
`+0xac` at `0x1008920c`. That slot is `0x10084870`, this=I; it subtracts
I's origin, clips to exclusive W+1/H+1, indexes the destination with stride
W+1, and copies bytes with source stride equal to the block width
(`0x10084965`–`0x100849cd`). The origin addition/subtraction cancels;
a block sample at file-local (cx,cy) becomes that same corner (cx,cy).
Actual shipped blocks cover W*H, so they never write the extra last column
or row. Clipping support for a larger caller-provided rectangle does not
imply that the shipped files contain that larger rectangle.

**D/U — later writes and the placement distinction.** The bulk setter
`0x10084870` is a genuine public mutation interface: calling it again replaces
the covered corner bytes, including a logical edge if the supplied block reaches
it. It then invokes derived-region notification `+0x19c=0x10084150` and
`+0x188=0x1008f2d0`, kind0xc9 (`0x100849cf`–`0x10084a2b`). The former
reads corner minima/maxima for derived data; it is not an averaging writer
back into the corner grid. Ordinary placement/clearing's stored item height
is a different byte in I+0xc's tile record. For example `0x10082e4d`–
`0x10082eb2` reads the corner pointer and calls type `+0x68`; the inspected
AnnoGame getter `0x10008f90`–`0x100091ce` computes a height/acceptance result,
and its `0x10008ea0` helper reads the supplied heights. It does not turn
that item's height write into a store to the corner grid. Read methods
`0x10084820`, `0x10084c20` and `0x10084c60` respectively return four corners,
one corner's scaled height and a four-corner mean; +0x9c is a **getter**,
not a single-corner terrain setter. Derived normals `0x1008e3d0`–
`0x1008e69e` read this grid and write their separate output vectors.
This is a bounded writer/consumer read, not proof that no external/editor
caller ever invokes the public bulk setter later; OQ-610 retains that closure.

**D — save and reload.** When save flags include mask0x2, serializer
`0x10089e01`–`0x10089f51` emits HEIGHT in blocks of at most32 by32 over
x<W/y<H. It calls `+0xb0=0x10084a30` with mask255 at `0x10089ec2` to copy
**current interior corner values**, using W+1 source stride, into each
W*H-format block. The HEIGHT chunk is constructed at `0x10089ede`–
`0x10089f18`. Readback `0x10084b34`–`0x10084b9c` copies to the supplied
output buffer, not into I+0x14; its alternate mask arm also writes that output.
Neither logical border nor allocation padding is serialized in those blocks.
Reload allocates zeros and copies the interior again. Thus an ordinary loaded
island round-trips this mapping; a hypothetical nonzero edge written through
an expanded bulk-setter block is not preserved by these HEIGHT blocks.
Other save modes can omit HEIGHT entirely; this does not assert every save
embeds terrain or resolves all later terrain-mutation callers.

### Worked window and the failed fish-water assumption

**P/A — worked island.** Original `Scenes/0200Single/Single_06.sze`, SHA-256
`29767be35e21f5c59dfc4f9178ece33f342e482fa22380ea0bf74f5dc6ef5fed`,
island12, world origin(72,264), size80x80. GELAENDE block(64,64), size16x16,
item14 (payload offset90) places tile11 at local(69,66), world(141,330).
Its HEIGHT block(64,64) supplies the four corner measurements:

| Local corner | Block-payload offset | Height |
|---|---:|---:|
| (69,66) | 43 | 32 |
| (70,66) | 44 | 24 |
| (69,67) | 59 | 48 |
| (70,67) | 60 | 24 |

The mapping copies these values, so mean=(32+24+48+24)//4=**32**, which is
**closed**, not water, under §3's strict test. The adjacent tile(70,66) uses
24,16,24,8: mean18, open. This distinguishes a real height-dependent result
from an implementation that accidentally declares the whole island closed.
The fish item itself records height32. No offset or threshold was changed to
force it to become water; original-runtime reachability is NOT_RUN.

**P/A — complete opt-in result, not a passing water proof.** Run:

```sh
python3 -I tools/fisher_ref.py --check-fish-water /path/to/game/app
```

This reads the existing `tools/insel.py` scenario/block/grid interfaces and
checks all three placed-object layers in all50 shipped .sze files. It requires
the documented50 scenarios and2473 placements (configurable expected counts
exist only for explicit corpus variants and self-authored contract controls).
**Selected2473/executed2473/water0/failed2473/empty0/skipped0/incomplete0**,
exit1: **every serialized tile11 in §1's24 positive scenarios is closed**.
Means range32–96; the24 scenario counts in §1 are consequently also the
exception counts. The command prints **every** exception, including scenario,
island, local/world tile, layer/block/item, corner measurements and mean.
The complete terminal list is retained in ignored
`cache/fisher-re-1c/fish-water-02.log`; no raw map files or full placement
inventory are added to git. This is an observed failed assumption, not a skip,
parser failure, adjusted positive fixture or waiver of a future port canary.

**D/U — a relevant runtime source, not an explanation invented for the failure.**
The island's `+0x1a0=0x100928a0`–`0x10092b32` inspects corner heights and
can create tile11 on **empty** low ground: the nonzero-id test at
`0x10092976` skips existing objects; `0x10092a61`–`0x10092adb` additionally
requires the four-corner mean below16 before selecting type11. It reads the
corner grid and writes the tile records, not the heights. Therefore the raw
file inventory is not a complete census of possible runtime fish resources,
and this helper does not repair the closed, already nonzero file placements.
OQ-609 records why §1's raw imported-tile count cannot serve as a successful
fisher canary or a complete runtime-source import proof. Do not silently
reinterpret these placements or generate replacement fish in this packet.

**A/T — reference and acceptance boundary.** `corner_grid` takes width,
height and exactly W*H unsigned HEIGHT samples, returns stride W+1 and the
logical grid. Five hand-derived vectors cover singleton, square, wide/tall
and mixed samples; two additional sea vectors distinguish border means31/32.
The public contract rejects edge-replication mutants and checks opt-in corpus
success/failure/missing-data behavior on small self-authored files. OQ-603/604
remain refused; OQ-200's load mapping is answered, broader terrain callers
remain OQ-610, and the all-closed raw fish finding remains OQ-609. Engine
code, HE reading and original-runtime fishing remain NOT_RUN/out of scope.

### Census meaning corrected — 2026-11-14, FISHER-1b

The unchanged 50 scenarios / 2473 embedded fish / 619 adjacent-water / 1854
without adjacent water counts describe **blocked** placements and local shoreline
geometry. All 2473 have mean at least 32, so none has usable mode-3 goal bits
as loaded. The Python opt-in census retains its legacy output key `goals` for
that blocked count; it is not a catchability result. The C++ corpus test now
names the blocked count and asserts zero usable. Why original scenarios contain
these blocked fish, later corner writers and runtime creation are FISHER-RE-2 /
OQ-609/610; this packet neither invents fish nor repositions existing ones.

### Corrected shoreline census — 2026-11-13, fix round1

**D/A — superseding criterion, not a changed mapping.** The goal-mask and
walking predicates in §3 have opposite senses. The §5 original census's
0/2473 water result is expected for goals; its interpretation as a source
contradiction was wrong. The accepted HEIGHT mapping, OQ-200 disposition,
OQ-610 and runtime-fish helper read above are unchanged.

**P/A — opt-in local check.** `fisher_ref.py --check-fish-water GAME_DIR`
now checks each serialized11 for (a) goal mean≥32 and (b) at least one
in-bounds tile among the eight neighbours with mean<32. The fish tile itself
is excluded; diagonals included; no wrapping or invented off-island water.
This explicitly chosen Chebyshev-distance1 neighbourhood checks immediate
shore adjacency around a goal, rather than treating the goal as a walking
cell. It is a local corpus criterion authorized by fix1, **not** a decoded
searcher goal radius, global route test, entrance check or proof that all
failed placements are unreachable. The predicates are from
`AnnoWelt.dll 0x10090689`–`0x10090697` and `0x1009226b`–`0x100922df`;
the neighbourhood is a stated measurement scope, not an original rule claim.

**P/A — complete result.** Fifty scenarios,2473 selected/executed:
**2473 goals,619 with adjacent water,619 satisfying both,1854 exceptions**.
Failed1854, empty0, skipped0, incomplete0; exit1. All exceptions are goal1
with zero adjacent water; no non-goal exception. Every exception and all50
source hashes are retained in `cache/fisher-re-1c/fix1-corpus-02.log`.
For example `00_1024_easyentry_01.sze`, island3 local(92,33),
world(228,209), corners40/48/48/48, mean46: goal1, adjacent_water0.
No exception is waived; OQ-609 is narrowed to this measured local gap.
The earlier logs remain evidence of the superseded question, not new failures.

**P/A — worked-window reinterpretation.** Single_06 island12 local(69,66)
mean32 is a goal, and its east neighbour(70,66) mean18 is water: this placement
passes both corrected predicates. No byte, border, position or threshold changed.
The reference's six `sea_cell` vectors now report free0 below32/free511
at or above32; the newly applied polarity mutant must exit1 disagreement.

### Implemented planar boundary — 2026-11-14, FISHER-1b

**T — bounded rebuild scope.** The optional header `fisher_rules` enables mode-3
search and return using the existing integer planar `HarvestState`. The original
segment Y rule at `0x10038730` is not represented: there is no vertical figure
coordinate, no synthetic terrain-following, and no claim to solve OQ-603's float
continuation. This supersedes the earlier checkpoint's “not implemented” status.
The discarded dispatch draw remains in the shared Farm tick; the enabled body
adds no draw. The owner permits later on/off trajectories to differ because
an active worker changes eligibility at subsequent dispatch gates.


## 6. Runtime fish fill and the real-map boundary (FISHER-RE-2, 2026-11-15)

**D — entry and exact predicate.** The in-game island table `0x100cac2c`
has `+0x1a0 = 0x100928a0`; `+0x19c` is `0x10084150`, a different
notification. This corrects the old slot label in [transport.md §3.4](transport.md).
The complete fill body `0x100928a0`–`0x10092b32` takes `this = island`
and one pointer to a half-open rectangle of four signed 32-bit world coordinates.
It clips against origin words `+4/+6` and extent words `+8/+0xa`, translates
into local coordinates, then visits rows and columns in increasing order
(`0x100928c3`–`0x10092976`, `0x10092ae3`–`0x10092b32`). There is no
quota, live-figure count, timer, or spawn-record allocation in this body.

**D/P/A — fish versus grass.** A nonzero **whole unsigned 16-bit object word**
at tile-record offset 0 skips the cell (`0x10092976`–`0x1009297b`); no
14-bit mask is applied to that test. The corner pointer is island `+0x14`,
stride `W + 1`; four unsigned bytes produce integer `mean = sum / 4`
(`0x10092981`–`0x100929d7`). Default means at least 32 enter the grass
branch, which additionally tests ground fertility (`0x100929d9`–`0x10092a60`).
Means below 32 enter the fish branch, where a second strict test admits only
**mean < 16**, not mean 16 (`0x10092a61`–`0x10092a9e`). Initializers
`0x1008fea0`–`0x1008feb2` and `0x1008ff00`–`0x1008ff0e` establish
these thresholds from 16.0 and the common height scale; they are not raw
zero-initialized image values. This reference uses those shipped defaults.

**D — writes, not a whale-style occurrence.** The fish branch selects catalogue
id 11 (`0x100928bc`–`0x100928c3`) and writes its type id into the existing
8-byte tile record at `0x10092aa0`–`0x10092aaf`. It clears byte 2, writes
the item height into the record's second 32-bit word, and retains
only bit 7 of byte 3 (`0x10092ab5`–`0x10092ade`). More precisely, the
height is the low 8 bits of the record's **second 32-bit word**; the remaining
24 bits are preserved. No corner height changes. The selected type getter
`AnnoGame.dll 0x10008f90`, low flag bits equal to 1, returns the same corner
mean (`0x10008fae`–`0x10008fdf`). Parsed HEAD flags of types 2 and 11 are
`0x29`, selecting that branch. This preserves the tile's existing walk code,
mask and ownership fields; it creates no figure, figure id, occurrence or new
saved counter. Fresh records' code 2 comes from `0x1008c105`–`0x1008c115`.

**D/P — identity and why the embedded inventory misled.** It is the same
Anlagen 11 / ROHSTOFF / WAREFISCH 71 already identified in §1, not a new
Figuren id or a scenery-only subtype. Existing embedded type-11 records are
not overwritten, even when their mean is at least 32. The fisher's existing
resource-goods query (`0x10059325`, island query `0x10092320`) finds the
new type-11 cells by goods 71 just as it finds an embedded resource. Their
mean below 16 also means below 32: island mode 3 returns blocked mask 0
(`0x100922cd`–`0x100922df`), and goal-pop `0x100033c3`–`0x100033ce`
therefore exposes usable mask 511. The 2,473 blocked embedded records do not
exhaust runtime fish. No lowering of those embedded records is needed to
explain a separate population of catchable resources.

**D — when it runs.** File loading `0x100a1da0` reads INSEL4 / INSEL5,
completes the island child-record loop, then invokes world slot `+0x1d4`
at `0x100a1f9d` / `0x100a20b8`. The INSEL5 chunk-buffer loader
`0x100a2ea0` similarly calls island `+0xdc` for its records before the
same world slot at `0x100a2fc8`. World wrapper `0x100a1810`–`0x100a183c`
conditionally preprocesses ground through island `+0x1a4` and then always
calls `+0x1a0`. The preprocessing body `0x10084370`–`0x1008480e` computes
a separate ground/fertility buffer, not a new corner grid. Clearing an area
also calls `+0x1a0` on its affected rectangle at `0x1008376e`–`0x10083796`,
then `+0x19c` at `0x100837a6`. This is not evidence of a periodic fish
spawner. Cutting 11 to 10 and regrowth remain the shared resource paths in
[forestry.md](forestry.md); they are not this clear-and-refill operation.
Ordinary placement's call at `0x10082ab9` is `+0x19c`, **not** this fill.

**D/U — draw closure.** There is no PRNG call in the complete fill body.
The standard catalogue lookup `AnnoGame.dll 0x100037e0`–`0x100037eb`,
type-id getter `0x1000a660`–`0x1000a666`, and selected height branch
`0x10008fae`–`0x10008fdf` are leaves without draws; the grass walk-code
getter `0x10007dc0`–`0x10007ea2` also draws nothing. Island initialization
`0x1008c0c9`–`0x1008c0de` obtains `+0x1c` from world `+0x18`, whose
getter is `0x100a37d0`–`0x100a37d6` (world `+0x110`). The caller that
supplies that world's concrete catalogue and its type-array construction
was not newly closed here: retain [OQ-202](../open-questions.md).
The zero-draw claim applies to the named standard getter bodies and fish
projection, not an arbitrary substituted catalogue or transitive load session.

**2026-11-19, D/P — first-call binding narrowed:** the earlier caller/array
limitation above is now followed for the fresh standard catalogue and type 2/11
profile in [the producer read](wildlife-postload-inputs.md). Frame
`0x10020108–0x100201d7` ->Game `0x10013776–0x10013789` constructs table
`0x100215a4`; ENTRY insertion `0x1000329d–0x100032af` populates its array.
Actual descriptor table is `0x10021a88`, whose relevant getters match those
named above. Both loaded HEADs select the mean branch and code 2. This closes
that finite first-call getter binding, not arbitrary reuse, catalogue mutation,
global initialization or full postload execution; no new runtime test is claimed.

| Position | Modulus | Selection | Condition |
|---|---|---|---|
| none | none | no draw consumed | Fish fill, complete `0x100928a0`–`0x10092b32`, with the standard catalogue/type getters named above |
| none | none | no draw consumed | `runtime_fish` reference: supplied raw draw suffix remains unused; no generator implemented |

**D/U — later corner writers, OQ-610 narrowed.** Reuse §5's established
allocation, HEIGHT setter and save/reload mapping. The new load-fill and clear
call paths above write tile resources and derived ground, not `island +0x14`.
The bulk setter `0x10084870`–`0x10084a2b` remains a mutator capable of
changing logical edges; HEIGHT load calls it at `0x1008920c`. Serialization
`0x10089e01`–`0x10089f51` saves current interior heights. This read does
**not** establish every possible external/editor caller of the virtual setter,
and does not close OQ-610 as an immutability claim. No inspected path lowers
an embedded fish tile to make it catchable. The load-time empty-cell fill is
already a separate explanation.

**P/A/T — census method and boundaries.** `tools/fisher_ref.py --runtime-fish
GAME_DIR --output FILE.json` reads the original HEIGHT and all GELAENDE,
FLUESSE and PROD2 placements through the existing independent parsers. It marks
rotated footprints occupied, clipped to the island; the original overlay and
ordinary placement both write ids (`0x1008220e`–`0x10082215`,
`0x10082422` onward; [transport.md §3.4.17](transport.md)). It projects the
strict empty-cell / mean-below-16 branch after all serialized placements.
This is a static **embedded-island** census; unresolved INSELLNK templates are
not silently counted as empty sea. Output includes source SHA-256 and every
created cell as half-open row runs `(island, y, x, end_x)` plus world origin.
Every such cell has usable mask 511. It is not an original-runtime trace,
path-reachability proof, or complete actor/script import claim.

| Scenario (under Scenes) | Runtime fish cells | Usable mode-3 goals |
|---|---:|---:|
| `0010SingleEndlos/00_1024_easyentry_01.sze` | 86,399 | 86,399 |
| `0010SingleEndlos/01_1024_easy_256.sze` | 4,898 | 4,898 |
| `0010SingleEndlos/02_1024_easy_128.sze` | 4,317 | 4,317 |
| `0010SingleEndlos/03_1024_med_256.sze` | 0 | 0 |
| `0010SingleEndlos/04_1024_med_128.sze` | 0 | 0 |
| `0010SingleEndlos/05_800_Med_128.sze` | 0 | 0 |
| `0010SingleEndlos/06_1024_heavy_256.sze` | 0 | 0 |
| `0010SingleEndlos/07_1024_heavy_128.sze` | 0 | 0 |
| `0010SingleEndlos/08_800_heavy_128.sze` | 0 | 0 |
| `0011SingleEndlosAddon/endless01.sze` | 176,616 | 176,616 |
| `0011SingleEndlosAddon/endless02.sze` | 372,675 | 372,675 |
| `0011SingleEndlosAddon/endless03.sze` | 113,595 | 113,595 |
| `0100Kampagne01/S01_01_01.sze` | 213,888 | 213,888 |
| `0100Kampagne01/S01_02_01.sze` | 179,016 | 179,016 |
| `0100Kampagne01/S01_03_01.sze` | 214,947 | 214,947 |
| `0100Kampagne01/S01_04_01.sze` | 55,767 | 55,767 |
| `0100Kampagne01/S01_05_01.sze` | 24,852 | 24,852 |
| `0100Kampagne01/S01_06_01.sze` | 130,527 | 130,527 |
| `0100Kampagne01/S01_07_01.sze` | 214,626 | 214,626 |
| `0100Kampagne01/S01_08_01.sze` | 39,940 | 39,940 |
| `0100Kampagne01/S01_09_01.sze` | 207,914 | 207,914 |
| `0100Kampagne01/S01_10_01.sze` | 232,120 | 232,120 |
| `0100Kampagne01/S01_11_01.sze` | 15,406 | 15,406 |
| `0100Kampagne01/S01_12_01.sze` | 252,755 | 252,755 |
| `0100Kampagne01/S01_13_01.sze` | 99,798 | 99,798 |
| `0200Single/Single_01.sze` | 133,809 | 133,809 |
| `0200Single/Single_05.sze` | 39,271 | 39,271 |
| `0200Single/Single_06.sze` | 54,457 | 54,457 |
| `0200Single/Single_08.sze` | 21,532 | 21,532 |
| `0200Single/Single_10.sze` | 116,574 | 116,574 |
| `0200Single/Single_12.sze` | 22,725 | 22,725 |
| `0200Single/Single_14.sze` | 102,505 | 102,505 |
| `0200Single/Single_15.sze` | 39,664 | 39,664 |
| `0200Single/Single_16.sze` | 42,990 | 42,990 |
| `0200Single/Single_24.sze` | 30,823 | 30,823 |
| `0201SingleAddOn/single30.sze` | 199,922 | 199,922 |
| `0201SingleAddOn/single31.sze` | 60,872 | 60,872 |
| `0201SingleAddOn/single32.sze` | 126,823 | 126,823 |
| `0201SingleAddOn/single33.sze` | 80,302 | 80,302 |
| `0201SingleAddOn/single34.sze` | 112,780 | 112,780 |
| `0201SingleAddOn/single36.sze` | 202,179 | 202,179 |
| `0201SingleAddOn/single38.sze` | 161,229 | 161,229 |
| `0201SingleAddOn/single39.sze` | 216,685 | 216,685 |
| `0201SingleAddOn/single40.sze` | 168,129 | 168,129 |
| `0201SingleAddOn/single41.sze` | 99,035 | 99,035 |
| `0201SingleAddOn/single42.sze` | 167,013 | 167,013 |
| `0201SingleAddOn/single43.sze` | 5,840 | 5,840 |
| `Tutorial/01Discover01.sze` | 8,486 | 8,486 |
| `Tutorial/02Trade01.sze` | 22,472 | 22,472 |
| `Tutorial/03T_Combat_01.sze` | 32,097 | 32,097 |

The 50 embedded-scenario projections total **4,908,270 created cells and
4,908,270 usable goals**. Six zero-result maps can have procedural island
links; zero here means no eligible embedded cell, not no runtime fish after
link resolution. Census log: `cache/fisher-re-2/runtime-fish-final-02.log`;
exact cells: `cache/fisher-re-2/runtime-fish-final-02.json`.

**P/A — lan5 scope made explicit.** There is no shipped `lan5.sze`. Both
repository meanings were measured from their original SCP templates with
`--layout`: `data/maps/lan5.txt` has **280,316** runtime cells / usable goals;
`data/maps/candidates/ship-start-v2/lan5-ship-a.json` has **420,194**.
The latter's 421 embedded fish are still distinct from this generated count.
The JSON gives exact cells for both layouts; their template selection and
placement are the layouts', not a procedural draw emulation.

**P/A — S01_02_01 and its local farms.** The map has **179,016** generated
cells, all usable, across its 27 embedded islands. The two type-810 farms on
island 58 are local `(196,155)` and `(201,163)`, both rotation 1, world
`(628,267)` and `(633,275)`. Their integer search rectangles are respectively
`(190,149,16,15)` and `(195,157,16,15)`. The generated-cell projection finds
**28** and **64** cells inside these rectangles. First row-major witnesses
are `(205,149)` and `(202,157)`; these are candidate facts, not a claim that
an entrance-to-goal path or harvest continuation succeeded.

**T/U — can FISHER-1c run on S01_02_01?** The fish rule can be derived from
loaded HEIGHT and occupancy without new state or a new tick system, and it
would supply usable candidates near both farms behind the existing flag.
However, the requested normal-session canary **cannot yet run**: the normal
adapter successfully imports all 27 islands, then `World::create` refuses
`script offset 0: unsupported object definitions` (`src/sim/src/script.cpp`,
the explicit `!a.objects.empty()` refusal). The sim needs the flag-gated
resource derivation **and** an authorized script-object implementation or an
explicitly scoped alternative canary contract. Disabling the script or using
ActorFreeComponent would not prove the requested original normal session.
That decision is [OQ-625](../open-questions.md#oq-625-s01_02_01-normal-session-canary-and-script-object-definitions-fisher-re-2-2026-11-15-open).

**A/T — reference and limits.** Four hand-derived `runtime_fish` vectors
separate mean 15 from 16, occupied from empty, and the whole raw object word
from a masked id; a supplied draw remains unconsumed. The contract applies
strict-depth and occupancy mutants exactly once and demands exit 1. It also
checks an exact three-cell synthetic census and refusal of missing HEIGHT.
The C++ geometry reader skips this new record by name and continues to assert
all 6 sea-cell, 5 corner-grid and 5 search-rectangle vectors. The `runtime_fish` record refuses an empty mean-at-least-32 cell because
its separate grass path needs fertility, outside this sea-branch record.
No engine code,
state, pins, asset bytes or scheduler wiring changes in this packet. HE and
original live-game execution are NOT_RUN; OQ-603 / OQ-604 stay open.

## 7. Runtime fill in the sim and follow-ups (FISHER-1d, 2026-11-15)

**D/T — candidate count is not usable-goal count.** Resource query return
`0x100592f4`–`0x100592fa` is tested for zero at `0x100592fc`, before the
candidate list is passed to search at `0x10059325`. The later goal-pop at
`0x100033c3` subtracts the blocked mask from `0x1ff`. Accordingly, an
unclaimed blocked type-11 tile still counts as a candidate; it contributes no
usable goal subcells. The sim now counts it before that mask is applied.
Separate rigs distinguish blocked-only fish (search attempted) from genuinely
no fish (no nodes popped). This does not change the limit of 20 or add a draw.

**T/A — fill API and lifecycle.** `fill_fish_resources(TileLayer&, HEIGHT,
TileRect, enabled, threshold=16, resource=11)` implements §6's clipped,
row-major sea branch. It reads the entire object id, copies the unsigned mean
into item height, resets phase/footprint offsets/rotation, and preserves the
anchor, owner, claim, other auxiliary bits and walk data. Disabled or absent
HEIGHT does nothing. `World::create` calls it after loaded objects and buildings;
the shared object-clear path calls it over the cleared footprint when enabled.
Public demolition, city removal and house removal propagate the existing
fisher flag to that path. Placement, ordinary ticks and native save reload do
not regenerate resources. This covers §6's load and area-clear callers
(`0x100a1f9d` / `0x100a20b8` / `0x100a2fc8`, `0x100a1810`–`0x100a183c`,
`0x1008376e`–`0x10083796`), without adding saved state or porting grass.

**T — proof scope.** Four hand-derived `runtime_fish` records now also drive
C++; mean 15 versus 16, raw object word 16384 and occupied 11 are controls.
Synthetic `World::create`, public demolition and save/reload exercise the
production hooks. The C++ 50-map test matches the accepted Python census:
4,908,270 new cells, all usable; S01_02_01 has 179,016. This census parses the
raw island geometry and applies the normal object-placement kernel to **all**
GELAENDE/FLUESSE/PROD2 footprints, including orphan city records. It compares
against the independent RE2 count fixture. It is a geometry projection, not a
normal actor/script session, and it does not establish the two original farms'
complete trips. Unsupported actors or script objects are not silently removed
from a gameplay canary. Native reload preserves a deliberately empty cell;
only the original load/clear lifecycle is newly covered.

**T/A — strengthened continuation and entrance proofs.** The mid-cut reload
runs until worker release and asserts the full 768 raw delivery; a separate
pair of from-create runs compares hashes through completion. The rotated launch
uses a hand-derived literal, not the function under test. The mode-0 test is
explicitly named a default/explicit Land selection-equivalence check. A rig
starts at the first entrance on mean 32 land, escapes onto water, claims and
cuts fish, then returns through its second entrance on mean 31 water. The
initial diagnostic with **both** entrances closed reached the cut but did not
deliver within the bound. Return through an all-closed entrance set is not
proved or repaired here; no movement rule is invented to make it pass.
The enabled invalid-job diagnostic includes job `0x2d`.

**T — compatibility.** The existing default-off header tag 1 is unchanged;
state 139 and every section version stay unchanged. The full pins gate must
show zero moved pins and unchanged current recordings. Threshold 16 remains a
named addressed default passed to the state-free function; no Rules field,
extension, periodic spawn system or RNG call was added. The owner's earlier
draw decision remains per eligible attempt, not equality of whole enabled and
disabled trajectories.

## 8. Script-object inventory and proposed next packet (2026-11-15)

**P/T — owner choice.** The owner chose OQ-625 option 1: inventory and propose
SCRIPT-OBJECTS-1, retaining the requested normal-session claim. This is a sizing
proposal, not an assignment or authorization to implement bindings here.
Parsed source is `Scenes/0100Kampagne01/S01_02_01.sze`, SHA-256
`23a99f002e422df6580781746f4cf3ed6766595540f67acd867d88b6ddf17630`.
`tools/bigbrother.py::parse/walk` finds exactly two definitions and exactly one
kernel use of each name; the following offsets are BIGBROTHER payload offsets,
not DLL addresses. No original script listing is reproduced.

| P: name | Definition / name offset | Kind / numeric operand | Kernel primary / name offset | Use |
|---|---|---|---|---|
| CG_ENEMY_CITY1 | 16 / 29 | OBJECT_STADT / 4 | 4209 / 4216 | KILLED |
| CG_ENEMY_CITY2 | 48 / 61 | OBJECT_STADT / 18 | 4306 / 4313 | KILLED |

**P — all uses.** These are the only four quoted-name occurrences in the
whole payload. The two conditions start at 4182 and 4279: each guards the
object's KILLED query with `CITY_KILLED == FALSE`, then sets that signal at
4251 or 4348. There is no object-name action or reassignment. This establishes
use shape, not the original meaning of the numeric city operand or KILLED.
[bigbrother.md §3.1](../formats/bigbrother.md) explicitly leaves that operand
untraced; the future packet must prove its namespace and lifetime semantics.

**P/T/U — objects are the first refusal, not the only missing language.**
The current compiler explicitly refuses nonempty object definitions, and only
AREA subject queries are supported. This map also has PLAYER queries:
INHABITANTCOUNT (2), DEAD (2), BOUGHT (1). Its calls are QUESTLOG_SHOW (12),
SETWARELOG (1), SET_FOR_BUY (1), SET_DIPLOMACY (4), ATTACK_AREA (1), BEAMDOWN
(1), GOTO_SCREEN (1), SET_PROFILE (2), SHOWTEXT (3), LOST (2), WON (1).
None is in `script.cpp::Compiler::statement`'s current action dispatch.
INIT already begins with unsupported QUESTLOG_SHOW at payload offset 1339.
Consequently, merely admitting the two objects **will not** make the unchanged
script compile. Inventorying those calls is not decoding their original effects.

**Proposed SCRIPT-OBJECTS-1: one concern, about 3–4 hours.**

1. Spend up to 60 minutes tracing original OBJECT_STADT binding and KILLED
   evaluation, starting from the **AnnoScene.dll string** OBJECT_STADT at
   `0x10040244` and KILLED at `0x10040114` (S: the existing name catalogue in
   [bigbrother.md §6](../formats/bigbrother.md#6-the-interpreters-names-s), not
   decoded binding functions). Prove id namespace, lookup order,
   absent/destroyed-city behavior and any mutable binding state. Stop with a
   QUESTION if those cannot be proved; do not equate 4/18 with island slots.
2. If that contract is settled, spend about 90 minutes on only this binding and
   query, with an immutable program representation and stable world identity.
   Specify any persistence/section requirement before implementation; no global
   state bump or script bypass. All other unsupported queries/actions still
   refuse explicitly.
3. Spend about 60 minutes on synthetic binding/kill/removal, bad-id and save/
   reload controls, affected script gates and handover. The orchestrator reviews
   the original addresses. Acceptance is the binding/query slice, **not** the
   S01_02_01 normal-session canary. A follow-on script-language plan must size
   the additional PLAYER queries and 11 action names above before that canary
   can honestly be released.

**U — boundary.** No script semantics, identity, actor import or gameplay
canary is changed in FISHER-1d. OQ-625 remains open for normal-session acceptance,
with the owner's scope choice recorded; OQ-603's float/Y continuation and other
maps' complete gameplay paths remain outside this proof. Original live-game,
HE, Windows and WSL runs are NOT_RUN on this Mac.

## 9. Selected normal-map canaries (FISHER-1e, 2026-11-15)

**P/T — scope and setup.** The owner's selected maps are the first two in
[fisher-canary-maps.md](fisher-canary-maps.md). The tests use the normal
`load_scenario_file`, `World::create` and `Recorder::start` paths, original
actors, positions, stocks, terrain and scripts. Only existing header tag 1's
`fisher_rules` is enabled. Venice receives no command. The human tutorial gets
exactly one Build: issuer 2, island 3/city 0, type 810 at (243,216), rotation 0.
No projection, actor removal, script substitution, inventory injection or
production implementation change is involved. Original mechanism evidence is
§§1–3 and 6–7; this is a rebuild execution proof, not a new original-runtime read.

| Acceptance requirement | Positive proof | Counterexample / control | Review |
|---|---|---|---|
| Original complete trips | Per-hut departure, three claimed cuts, raw receipt and exact conversion | Disabled original worlds have no worker/raw/food progression | pending orchestrator |
| Deterministic continuation | Independent starts and a native pending-cut continuation agree every turn and in final bytes | Enabled hash differs from disabled hash; no enabled/off equality is assumed | pending orchestrator |
| Default compatibility | Default-only driver reproduces disabled start/end/trace; pins and fixture bytes unchanged | Explicit zero extension equals omitted/default extension | pending orchestrator |

**T — finite workload.** Each turn advances 20 game milliseconds, or 10 world
ticks. The watchdog was fixed before execution at 60,000 ticks (120 game seconds),
below the first u16 tick wrap. Both maps complete before it. Observed trip time
runs from the first visible spawn to the raw receipt; times have the 20 ms turn
resolution. No timing constant was changed to obtain a pass.

| Map / hut | Spawn | Open entrance / departure | First catch | Raw receipt | First food | Trip ticks / game seconds |
|---|---|---|---|---|---|---|
| Venice 3978, (217,326) | 810 | 810 / 910 | 6810 | 20660 | 20810 | 19850 / 39.70 |
| Venice 3978, (212,314) | 850 | 850 / 910 | 6860 | 20660 | 20850 | 19810 / 39.62 |
| Venice 3978, (216,301) | 890 | 890 / 960 | 6910 | 20760 | 20890 | 19870 / 39.74 |
| Human 810, (243,216) | 690 | 690 / 770 | 7520 | 22240 | 22290 | 21550 / 43.10 |

**T — actual state assertions.** Each figure is kind 142/job 45, starts at the
original first entrance, is observed on an open entrance tile, and then outside
the rotated footprint. Catch evidence is a claimed mature tile 11 at the worker's
position during Cut, followed by that tile becoming 10, its claim clearing and
cargo rising by 256. Each worker makes three cuts, returns with 768 raw, ends
with an expiry stamp inside its home footprint, and raises that hut's raw stock.
Food then rises by 51 while raw falls by 128 for Venice type 3978; the human
hut consumes 69 and produces 17. All four chains finish without a world fault.
The asserted output is the hut's food stock, not delivery into a warehouse or
consumption by residents.

**T — determinism and native cuts.** Each map has two independent normal
creations. Their whole-world hashes agree after every turn through completion:
2,089 turns for Venice, 2,229 for the human map. A third continuation is loaded
from a native save during a real claimed cut, at tick 1,210 (Venice, remaining
wait 5,597) or 1,920 (human, wait 5,600). Reload preserves the enabled header,
initial hash and save bytes, then matches the uninterrupted hash after all
1,968 / 2,037 remaining turns. Final native saves agree byte for byte. These
are independent logical runs in one process, not a cross-process/platform proof.

**T — default-off compatibility.** Explicit disabled-extension worlds and worlds
created with the existing default arguments agree after every corresponding turn
and in native bytes at start/end. No active worker of kind 142/job 45 appears;
workers_out, raw and food remain zero in the watched huts. Flag-off dispatch
may still consume its existing discarded attempt draw; this does not assert
whole enabled/disabled PRNG trajectories are equal. A separate default-only
measurement driver, linked to production sources verified identical to accepted
main 816c6352, independently reproduces the initial, final and rolling per-turn
flag-off digests below. It is not a separate build of another engine revision.

| Map / end tick | Initial disabled hash | Final disabled hash | Disabled per-turn trace digest |
|---|---|---|---|
| Venice / 20890 | c44b7be385cc4747 | 38b1627adcca2430 | d5811ae9c021508c |
| Human / 22290 | ccd55f2fa14ec44a | 2cbca03efb4875f7 | af9aa0674ef513ec |

**T — evidence and boundary.**
[fisher_map_canary_test.cpp](../../src/sim/tests/fisher_map_canary_test.cpp)
contains the two original-file heavy cases; they print an explicit prerequisite
skip when `ANNO1503_GAME_DIR` is unset. Gate receipts and source identities are
in `cache/fisher-1e/`, with exact classified lines in
[REPORTS.md](../orchestration/REPORTS.md). Existing fisher regressions and reference
vectors are retained; no pin or fixture is edited. The census's F1 correction is
13 empty kernels: Tutorial/03T_Combat_01 has an empty kernel but still refuses
its unsupported object declarations.

**NOT_RUN / U.** All-closed entrances remain FISHER-RE-3. Original float/vertical
continuation (OQ-603), the transitive original draw inventory (OQ-604), other
maps' full trips, long-running town balance, original live gameplay, separate
processes and target-host game-file runs are not established here. No existing
state number, section version, extension schema or default rule changes.

## 10. All entrances on land (FISHER-RE-3, 2026-11-16)

GOG 2.0.0.5 static read; **D/P/A/T/U** retain the legend above. AnnoGame
addresses below are in **AnnoGame.dll**; all other original addresses are in
**AnnoWelt.dll**. **History Edition NOT_RUN**: no HE AnnoWelt.dll or
AnnoWeltAddOn.dll was available in the Mac's game/cache roots. The edition
comparison's [class labels](../research/he-vs-gog-comparison.md)
do not establish code equality. Pixel/frame parity is expressly unclaimed.

### 10.1 Placement does not guarantee an open entrance

**P/D — the three types must not be conflated.** Parsed Anlagen.dat HEAD,
ENTRANCE and PROD3 records, interpreted by AnnoGame's type getters
`0x100084c0`–`0x1000861c` and height analysis `0x10008f90`–`0x100091d1`:

| Type | Footprint | Flags | Worker / route | Unrotated entrance tiles |
|---|---|---|---|---|
| 810 | 3 × 4 | `0x2087` | 142 / 45 | (1,3), (1,0) |
| 3978 | 3 × 4 | `0x2087` | 142 / 45 | (1,3), (1,0) |
| 3510 | 4 × 3 | `0x2087` | 393 / 44 | (0,0), (1,1) |

**P/D — 3510 is a separate worker case.** FigurenHeader.dat gives 393
BEWOHNTYPE `MAEHER`, ROUTETYPE `STEINMETZ`; 142 is `FISCHER` in both fields.
Route 44 selects byte 5 / arm `0x1003b1f6`–`0x1003b250`, constructor
`0x1005d450`, route table `0x100c44e8`; route 45 selects byte 6 / the
already read §3 arm. The ordinary worker table `0x100c4950` uses search
`0x1003acc0` (mode 0), whereas 142's `0x100cb4d8` uses `0x10038ad0`
(mode 3). A mode-3 closed entrance therefore does **not** establish that
3510's land worker fails to return. The boat/closed-return conclusions below
are for 810/3978 and figure 142, not an invented ice-fishing implementation.

**D — coast predicate.** In AnnoGame `0x10008f90`–`0x100091d1`, flags
`0x2087` take the coast helper at `0x10009184`. Complete helper
`0x10008ea0`–`0x10008f81` uses unrotated `n = width + 1`,
`m = height + 1`; with n and m above 2 it requires:

- the maximum corner height rounded down to a multiple of 16 is 48;
- every vertex of the shore edge is exactly 48;
- at least `floor(n/2)` vertices of the opposite edge are strictly below 32.

**D — rotation and scope.** The shore edge is top / left / bottom / right
for rotation 0 / 1 / 2 / 3 respectively; the opposite edge has n vertices.
The helper accepts immediately when either n or m is at most 2. It reads
neither ENTRANCE nor any entrance's four-corner mean. The enclosing island
predicate `0x1007c790`–`0x1007cc29` additionally checks bounds and existing
objects, calls type height analysis at `0x1007cb91`, and accepts the coast
class at `0x1007cbc8`–`0x1007cbd6`. This is a placement predicate, not proof
of ownership, funds, build-menu permissions or a live HE build. See also
[ship-placement.md §3](ship-placement.md).

**A — distinguishing counterexample from that read.** A synthetic 3 × 4
footprint has a 4 × 5 corner window. Set every corner to 48 except the two
endpoints of the bottom edge, which are 0. At rotation 0 the coast passes:
shore 4 × 48, maximum 48, opposite wet count 2. Both entrance tiles in the
table still have mean 48. Replacing the bottom middle pair by 16 gives a
first-entrance mean of exactly 32, still closed; replacing the entire bottom
edge by 0 gives mean 24, the open control. These are authored arithmetic
vectors, not extracted original windows. Consequently coastal placement does
not imply a mode-3 water entrance. No placement prohibition closes this issue.

**P/A/T — real build candidate, no edited inputs.** Tutorial/02Trade01.sze,
player 2, island 3/city 0, type 810 at world (242,217), rotation 1, has
entrance tiles **(245,218), mean 32**, and **(242,218), mean 48**. Raw HEIGHT
uses §5's proved load mapping; rotated entrance coordinates use AnnoGame
`0x10008523`–`0x10008571`. The existing placement reference accepts its
terrain/object window. A private measurement linked to a freshly checked
`anno_sim_load` build at base `67a960c9` ran the normal scenario loader,
World::create, original INIT and exactly one successful public Build command,
then measured those two closed means. No projection, actor deletion, injected
stock, terrain edit or script substitution was used. Log:
`cache/fisher-re-3/build-site-current.log`. **U:** a live original build of this
site and any HE-specific placement difference are not yet measured. Ordinary
placement's inspected writers do not lower the corner grid (§5/§6);
[OQ-610](../open-questions.md)
continues to bound the wider writer/caller closure.

**P/A — correct the earlier census's scope.** The four huts in
[fisher-canary-maps.md](fisher-canary-maps.md) were in the maps which passed
that compiler/normal-create selection, not an exhaustive raw-file inventory.
`tools/fisher_ref.py --fisher-entrances GAME_DIR` now reads all 50 .sze files:
95 PROD2 records, **92 unique geometrical placements**, 3 repeated keys;
87 have one open entrance, 5 have none. The unique key is map/island/type/
origin/rotation, not a claim that every saved city/actor can be imported.
There are 73 type 810, 13 type 3978 and 6 type 3510 placements. The five
all-closed placements are **3510**, so do not contradict the route-45 canary:

| Map under Scenes | Island / world origin / rotation | Entrance means |
|---|---|---|
| 0011SingleEndlosAddon/endless02.sze | 7 / (180,73) / 2 | 44,46 |
| 0201SingleAddOn/single33.sze | 11 / (79,74) / 0 | 48,43 |
| 0201SingleAddOn/single34.sze | 8 / (63,58) / 0 | 48,43 |
| 0201SingleAddOn/single36.sze | 1 / (369,156) / 2 | 48,43 |
| 0201SingleAddOn/single41.sze | 10 / (153,76) / 2 | 48,43 |

### 10.2 What the sea worker does

**D — dispatch is allowed.** Reuse §2's complete Farm tick
`0x1006ea90`–`0x1006ee5f` and special creator `0x10067e60`–`0x10067f61`.
Eligibility does not inspect open water. The eligible attempt draws once at
`0x1006ee06`, then the special creator uses the first rotated entrance;
it does not call the ordinary reachable-entrance gate `0x10072b50`.
Creation failure remains possible; no statement here guarantees every send
succeeds. The first job starts after creation, not during the send.

**D/A — a closed start is not a closed destination.** Figure 142's
`0x10038ad0`–`0x10038cc5` supplies the home entrances as starts when inside
the footprint. Search setup `0x10003d90`, start store `0x10003f73`, seeds
the start nodes and ORs the position's edge keys from `0x10003aa0`.
A closed start can therefore expand **out** to an admitted neighbouring water
cell; it need not first enter itself. This permits departure but does not
promise a route through arbitrary surrounding land. Resource search, limit 20,
claim and cuts are unchanged from §3: `0x10059210`–`0x10059398`,
`0x100553e0`, `0x10054530`; three successful cuts normally give 768 cargo.
The inherited finer-route/float qualification remains [OQ-603](../open-questions.md).

**D — paired-subcell refinement, 2026-11-16 (FISHER-1f).** The point flood
`0x10003aa0` reads the configured coarse mode at `0x10003b07`, increments
it at `0x10003b0f`, and calls island `+0x128` at `0x10003b17`. The ordinary
land pair is modes 0/1; the fisher pair is **3/4**. Dispatcher table
`0x10092190`, entry 4, selects `0x1009215e`–`0x10092187`: stores at
`0x10092166`–`0x10092181` write nine bytes of value 8, the free-subcell
value. Thus the fisher point flood's blocked bitmap is **zero even on a
closed coarse cell**. All four edge keys can be reached from its seed.
Neither mode 3's blocked bitmap nor the object's mode-1 WALKMASK is the
correct point-flood input. This explains the real site's departure toward
adjacent water; it does not admit a closed destination. The coast, coarse
cell and goal-mask rules above are unchanged. Original listings remain in
`cache/fisher-1f/point-mode-original.txt`, outside git.

**D/A — closed home goals cannot be entered.** The home start
`0x10057ae0`–`0x10057bbe` bypasses search only if already inside the home
footprint (`0x10057b68`–`0x10057b73`). Otherwise it uses the fisher's mode-3
setup, adds the home's explicit entrance points (`0x10072be0`–`0x10072ca6`),
and calls figure `+0xcc` at `0x10057bb1`. Goal marking at `0x10004068` /
`0x1000414b` adds the goal flag and low-word geometry keys, as decoded in
[population.md §19](population.md#19-map-pop-close-2-k2-reads-how-the-original-reaches-a-goal-on-a-mapper-0-tile-2026-11-04-annowelt-f220a878).
It does **not** replace the admission code. The independent layer test masks
that code with `0x3ff` (`0x10006aab`–`0x10006ab3`, other directions
`0x10006c9c`, `0x10006df7`, `0x10006f30`–`0x10006f64`). Mode-3 land has
code 7 and layer 2's entry 7 is zero (table `0x100d4940`, entry
`0x100d4947`, **P**). Even an explicit home goal with geometry bits cannot
be entered from another tile. If every entrance is closed and the worker is
outside, the home search fails; no walk home is installed. The last reached
position stays the last catch/search position, not an entrance or teleport.

**D — failed home means no raw credit.** `0x10034e06`–`0x10034e1b`
returns 0 when the search failed. Route run `0x1005b6c0`–`0x1005b707`
then takes fallback `0x1005e280`–`0x1005e2a8`: only failed kind 7 retries
the home node; other failed kinds return null. **Dated correction to §3's
job table:** the home job's kind getter `0x10057ad0` returns **4**, not 8.
Its failed start does not call completion `0x10057bd0`–`0x10057c27`.
Consequently the Farm receiver `0x1006ee70` is not called and raw stock does
not rise. A worker already inside is the counterexample: it can complete
without a search and credit cargo (nonzero goods), with u16 addition.

**D — release and disappearance are distinct.** `0x10034380`–`0x100343ae`
deletes the failed job. The inherited tick eventually takes end
`0x1003af00`–`0x1003afda`, zeroes velocity at `0x1003af69`–`0x1003af6f`,
and immediately calls release at `0x1003af96`. Release
`0x1003b6e0`–`0x1003b717` clears the home pointer and the Farm's worker
slot (`0x1006f070`); it does not call the raw receiver or transfer cargo.
Cargo is not delivered or dropped into a new resource. The outside wait is
15.0 at rate 0.01; the inside wait is 0.0. The old unknown footprint scale
is narrowed here: initializer `0x1003aa20`–`0x1003aa32` writes 1.0 / 1.0
to `0x100e0f58`. Thus outside waiting is nominally 7,500 ticks before Remove,
subject to the existing float/visit boundary qualification. The registered
unit's further expiry delay is separate ([annowelt.md](annowelt.md#the-carrier-worker-and-animal-class));
release does not mean immediate disappearance or immediate registry-slot reuse.

**D/U — state and save boundary.** The read exposes ordinary current-job,
position, cargo, home link, end flags/rate/remaining state; no separate
“all entrances closed” latch is written. The Farm saver `0x1006f010`–
`0x1006f060` writes its 10-byte record: switch at byte 1, output at 2,
raw at 4, activity at 8; loader `0x1006ef70`–`0x1006f009` restores these.
It writes no worker pointer or failed-home result. Generic job saver
`0x1005b9a0`–`0x1005bb9b` serializes step kind/variant, each step's private
payload and a u16 current-node index; it is not a new closed-home format.
The worker's common root saver `0x10031b20`–`0x10031d86` uses a zero-size
HEAD getter (`+0x144=0x100a3930`) and empty private writer
(`+0x130=0x10064a10`). **U:** the complete original temporary-worker save
caller/round-trip closure is not proved here; do not infer that a live failed
trip survives an original save, or that the rebuild's stronger native
continuation proof establishes original/HE save parity.

| Draw position | Modulus | What it selects | Condition / read range |
|---|---|---|---|
| One, before creation | none; discarded | nothing | Eligible Farm attempt, `0x1006ee06`, §2 |
| None directly | none | none | Coast helper AnnoGame `0x10008ea0`–`0x10008f81`; creator §2; search setup §3 |
| None directly | none | none | Home `0x10057ae0`–`0x10057c27`, fallback `0x1005e280`–`0x1005e2a8`, end/release ranges above |

**D/U — draw scope.** This does not close [OQ-604](../open-questions.md)
for arbitrary indirect callees. The fisher display pass `0x100384d0`–
`0x10038689` calls CRT random `0x100b68a9` at `0x10038604` / `0x1003861d`
for sound selection; these are not world `+0xb4` draws or sim selections.
No CRT generator is reproduced in the integer reference.

### 10.3 Reference and the later Windows capture

| Requirement | Read-backed positive / boundary | Distinguishing negative control | Review |
|---|---|---|---|
| Coast does not promise a water entrance | `closed_810`, `closed_equality`; §10.1 helper | `coast_requires_open_entrance` mutant fails; `open_control` remains legal | orchestrator pending |
| Failed home does not deliver | `outside_cargo`; §10.2 admission and fallback | `inside_bypass` credits 768; failed-credit/admission/release mutants fail | orchestrator pending |
| Census keeps its actual scope | 50 raw maps; repeated geometry counted separately | Authored duplicate/equality-32 versus 31 chunk controls | orchestrator pending |
| HE pixel acceptance | Fixed site/control capture card below | No original/HE capture is counted as passed | NOT_RUN |

**A — reference contract.** `coast_site` in `tools/fisher_ref.py` takes an
explicit rotated integer corner window and entrance **tile offsets**. It
models the coast helper only, not the full Build predicate. `closed_home`
is restricted to means at least 32 and an existing unchanged home; it models
the outside-failure / inside-bypass branch and eventual raw credit/release,
not float movement, pixel positions, elapsed time or original save survival.
The separate authored `fisher_entrance_vectors.txt` has 8 coast and 5 home
vectors, including a rotated window, insufficient wet count, exact shore
height, mean 32, wet-vertex 32, inside bypass and u16 wrap. The contract runs
both fixture files and kills applied mutants for a fictitious open-entrance
placement prerequisite, equality-as-water, closed-goal admission, failed
credit and absent release. Keeping it separate preserves the C++ geometry
reader's existing accepted record types; no engine/test/pin changes are needed.

**P/A — capture card for FISHER-HE-1, not an executed comparison:**

1. Fresh `Tutorial/02Trade01.sze`: GOG path `Scenes/Tutorial/02Trade01.sze`;
   HE path `Languages/Eng/Scenes/Tutorial/02Trade01.sze`. Select human player 2,
   its city 0 on island 3. Keep original actors, INIT, terrain, inventory and
   resources. Use normal game speed, pause before building; no editor, cheat,
   stock injection or deleted script. If the selected edition refuses this
   start or build, retain that refusal and stop; do not move the hut to fit.
2. Build **810 at (242,217), rotation 1**, footprint 4 × 3. Verify the entrance
   arrows occupy (245,218) and (242,218), respectively means 32 and 48 in the
   GOG source geometry. This is the all-closed case. In a separate fresh start,
   build the open control **810 at (243,216), rotation 0** (§9); its first
   entrance (244,219) has mean 30. Do not build both in one run: their footprints
   overlap. No other construction or movement command is required.
3. Fix the same viewport, camera rotation, zoom/art group and client dimensions
   for both editions. Record those values, game speed, presentation frame
   numbers and world/game time; a video-frame number alone is not a sim tick.
   Begin before Build confirmation and retain 120 game seconds after it,
   including idle/no-dispatch intervals. Do not accelerate. Record each boat's
   tile positions, headings, first appearance, turns, catches, stationary end
   and disappearance frame. Record the hut's stock display before dispatch,
   after each catch, at attempted return and at disappearance; distinguish
   displayed food from internal raw fish. Keep captures outside git.
4. For the rebuild repeat the exact fresh load and Build with only the existing
   `fisher_rules` extension enabled, recording workers_out, cargo, raw and food
   alongside the rendered frames. Match time-labelled event frames before
   pixel comparison. The GOG read predicts failure to credit a return from
   outside an all-closed hut; it does not supply HE frames or a pixel tolerance.

**T/U — executable-capture prerequisite / QUESTION.** At this base,
`src/app/src/play.cpp`'s normal scenario World::create call supplies default
extensions; it offers no fisher_rules switch. The existing headless sim can
set the flag and perform the Build (the measurement above), but that is not
a rendered HE/rebuild comparison. FISHER-HE-1 needs an explicit enabled
capture-session entry point and stock/time instrumentation before this card
can run in the rebuild viewer. It must also verify the HE build/entrance
means, rather than assume GOG equality. No viewer wiring, original live run,
HE code read, pixel capture or new rule is smuggled into this read packet.
The remaining save-caller closure and OQ-603 likewise remain unproved.

## 11. Closed fisher runtime and native proof (FISHER-1f, 2026-11-16)

**T — measured base before edits.** Normal Tutorial/02Trade01 loading,
World::create, original INIT and one player-2 Build of 810 at (242,217),
rotation 1, reproduced the two entrance means 32/48. At base `5901b846`,
worker 16 spawned at tick 690, ended at 720 without leaving or catching,
released its slot and entered expiry immediately. Worker 44 spawned at 1090;
the first registry slot disappeared at 8220. Raw and food stayed zero.
`cache/fisher-1f/baseline-site-01.log` records the stages. This did not meet
§10.2's point-flood rule or outside end wait. The new real-site test linked
to preserved base production archives fails `REQUIRE(left)`; the two new
synthetic cases also fail on the base. These are negative proofs, not passes.

**D/T — only two behavioral corrections.** The paired-mode read in §10.2
makes `WalkCells::point_mask` return the free mode-4 bitmap for FisherSea,
while coarse cells and layer admission retain their mode-3 values. Land
point floods are unchanged. Start, explicit-goal and route-validation keys
all use the paired bitmap. In the harvest body's Done state, an enabled
fisher ending outside releases its home once, starts the nominal 7,500 ticks
in its existing `wait`, and consumes elapsed/surplus time without movement.
Only after that does `end_figure` start registry expiry. A forced end cancels
the pending wait. Inside completion, raw receipt, harvest and eligible-attempt
draw order retain their existing rules. Types 810/3978 use this route-45 path;
3510/route 44 is untouched. The duration is the original timed rule, not a
capacity limit, and is separate from the configurable registry-expiry delay.

**T — state/validation.** Done with a positive wait is allowed only for an
enabled fisher with no home and no expiry stamp. The existing authoritative
step, wait, position, cargo and home/expiry fields suffice; no field, enum,
tag, payload, state number or section version changes. The native loader
accepts this valid continuation and still refuses unresolved Done with zero
wait. The disabled-state refusal wording is retained.

| Stage | Base | Corrected real-site measurement |
|---|---|---|
| Spawn | 690, first entrance | 690, same first entrance |
| First movement / footprint exit | none | 720 / 1170, normal sea walk |
| Catch | none | 7370 / 13580 / 19720, cargo 256 / 512 / 768 |
| Failed home / release | 720, empty cargo | 19720, cargo 768, raw 0, workers_out 0 |
| Second dispatch | 1090 | 19890, while the first worker still waits |
| Begin registry expiry | 720 | 27230 |
| Registry removal | 8220 | 34730 |

**T/A — time qualification.** The measurement above uses 10-tick turns;
the exact departure/catch/release observations and remaining-wait values are
in the canary log. The private production driver observes 7,480 ticks left
on release, because that visit already carried surplus time into the new
wait. Registry visits determine when an expired remainder is next observed.
The integer transition is tested at zero elapsed, one tick, a partitioned
wait ending with one tick owed, and a final tick plus surplus; a saved wait
resumes without re-arming or losing whole ticks. This does not assert x87,
vertical, frame-time or HE equality (OQ-603).

**T — proof scope.** All five `closed_home` reference records drive the C++
runtime, including inside bypass, no goods, empty cargo and u16 raw wrap.
A separate paired-mode control departs from a closed coarse cell even when
the object's raw mask is fully blocked, and cannot enter that cell as a goal.
The real canary retains original actors, terrain, stocks, resources and INIT:
no projection, actor deletion, inventory injection or relocation. Its two
independent starts compare every turn; native continuations are captured both
mid-cut and mid-released-wait. The two old open canaries remain controls.
Exact gate outcomes, including unsuccessful Debug timeouts and their targeted
Release retries, belong to the package entry in [REPORTS.md](../orchestration/REPORTS.md).

| Requirement | Positive proof | Counterexample / boundary | Review |
|---|---|---|---|
| Paired start flood, closed goals | Coarse mode 3 / point mode 4; real departure | Closed destination still refused | orchestrator pending |
| Failed home never credits or drops cargo | Three claimed cuts, raw/food 0, fixed position/cargo | Inside reference records do credit, with u16 wrap | orchestrator pending |
| Separate release, wait and expiry | Slot free and second worker sent before first expiry | Zero/one tick, native remainder, surplus and forced end | orchestrator pending |
| Disabled compatibility | Disabled/default runs plus preserved-base trace | No update mode or pin/recording edits | orchestrator pending |

**T — compatibility measurements.** At tick 60,000 the closed-site disabled
world is `da38267a8c04253f`, with per-turn trace `cf34641133b002cb`, exactly
matching the preserved base-engine driver. Explicit off and omitted/default
extensions also agree every turn and in native bytes. The two existing open
canaries retain their earlier enabled traces (`2550a07bc1a58b` for Venice,
`cfe100edcf0f7bfc` for the human hut) and their disabled traces from §9.
The scoped pin check covers touched lanes; all pin values and existing fixture
bytes remain unchanged. It is not a claim that the full game-file suite ran.

**NOT_RUN / U.** HE/live-original captures, original temporary-worker save
survival and float/Y parity remain unproved. This is the GOG-read integer sim
slice. The enabled viewer entry point, stock/time readout and picture comparison
remain the UI lane's FISHER-HE-1 work; no app wiring is included here.


## 12. Released fisher visibility (FISHER-RE-4, 2026-11-16)

**D/P — answer: figure 142 is not drawn after the normal release.** The
registry can still hold and visit it during the end wait. Its absence from the
picture follows from its selected animation and the shipped figure data, not
from deleting the figure or a universal hide-on-release flag. Addresses below
are GOG 2.0.0.5; D = disassembled control/data flow, P = parsed original file.

**D — end flags are behavioral.** AnnoWelt `0x1003af80`–`0x1003af96`
updates the u16 at figure +6 to `(old & 0xfffe) | 0x202` before releasing:
bit 0 (value 1) is cleared; bit 1 (2) and bit 9 (512) are set. Bit 0 gates
active route processing (`0x10033cba`–`0x10033cc7`) and the active-rate
getter (`0x10035bc0`–`0x10035bd0`). Bit 1 is the terminal-step marker:
when the remaining step expires, the tick removes the figure from its sector
and starts registry expiry (`0x1003481a`–`0x10034844`, already read in
[annowelt.md](annowelt.md#the-carrier-worker-and-animal-class)). Bit 9 is
also an end marker: other end/restart handlers test it to avoid installing a
fresh short delay (`0x100518b4`–`0x100518d2`, `0x10045c4e`–`0x10045c60`).
These consumers establish their effects, not original symbolic bit names.
Neither the fisher display routine nor the sector draw loop tests bit 9 to hide it.

**D — the sector pass still calls display.** AnnoWelt
`0x1009ffc2`–`0x1009ffdb` obtains the sector list through registry slot +0x30;
`0x1009ffde`–`0x100a0004` walks its records (+0xc figure, +4 next) and calls
figure slot +0x7c. The subsequent bit-1 test at `0x100a000d`–`0x100a0052`
is after display and conditionally deletes class codes 162/163, not FISCHER 4.
Figure 142 uses table `0x100cb4d8`, whose +0x7c is `0x100384d0`.

**D/P — why that call emits no fisher sprite.** End sets animation byte +0x10
to 17 and its stamp at `0x1003af9c`–`0x1003afce`; the normal fisher branch
passes this code unchanged. The notification at +0x150 is the no-op
`0x10099840`. With home cleared, display skips its owner-visibility branch
(`0x100384db`–`0x10038526`), then queries animation at `0x10038530`.
Shared `0x100330e0`–`0x1003315b` passes that byte to the type's +0x78;
AnnoGame's graphics-enabled tables `0x10021e8c`/`0x10021f58` use
`0x100113a0`. Its missing-set branch `0x100113e2`–`0x100113ff` returns -1.
The fisher exits at `0x10038537`–`0x10038539` when that result is negative.
Even at zero animation elapsed, its later piece lookup is decisive:
AnnoGame `0x1001137d`–`0x1001139a` returns false for an absent set, and
AnnoWelt `0x10038583`–`0x10038588` exits before the sprite submission
`0x100385a5`. The loader attaches each set by its prefix's low u16
(AnnoGame `0x10011818`–`0x10011836`, `0x10011af7`–`0x10011b04`).
FigurenHeader.dat figure 142 has exactly four HAEUSER sets, keys 4, 7, 8, 9;
17 is absent. No fallback boat sprite should be invented for that missing set.

**D/P — not a blanket rule for released workers.** The same end selects 17
for the ordinary carrier/hunter classes, and their common display wrapper
AnnoWelt `0x1003ab20`–`0x1003ab7a` delegates to `0x100332b0`, using the
same animation/piece lookup. Parsed figures 108 and 203 *do* contain set 17;
figures 102 and 106 do not. Thus release flags alone cannot mean invisible
for every worker. Visibility remains dependent on its selected set, frame,
normal view admission and eventual sector removal, not just a null home.

**Viewer rule:** a released figure-142 fisher in its end wait emits no sprite;
retain its sim continuation and honor the absent animation 17, without a fallback.

**U / NOT_RUN.** HE binaries, live GOG/HE captures, pixel comparison and other
workers' complete animation timelines were not run. The GOG read already
explains the owner's observed disappearance; it establishes no HE code parity
and no need to remove the saved waiting figure. No engine state or pin changed.
