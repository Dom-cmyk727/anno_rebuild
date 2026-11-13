# Whaler, whale origins and lamp-oil chain (WHALE-RE-1a)

2026-11-13. Read scope: original data; GOG class identities; whale creation;
whaler tick and loader. Whale life after creation, engine changes and reference
vectors are excluded. Stop at a local handoff candidate and green docs CI; review remains pending.

| Read | Positive evidence established | Distinguishing control | Independent review |
|---|---|---|---|
| Data/chain | parsed original records and main class mapping | workshop versus inert whaler; stall tick versus purchase path | pending orchestrator |
| Class identities | factory, constructor and table/call profile | occurrence versus figure; route versus embedded job | pending orchestrator |
| Whale origins | occurrence attach/load/tick/creation paths | fixed source position versus random ocean placement | pending orchestrator |
| Whaler tick/load | complete addressed bodies and selected callees | output-full versus raw-full; version versus inner length | pending orchestrator |

Reference: GOG AnnoWelt.dll 2.0.0.5, SHA-256
`f220a878510f740c4cfb7224ade0a260e9a0fd7e99113e1201e7a37c0dfa6ae2`.
**D** = original instructions/table read; **P** = parsed original file;
**T** = inspected rebuild code at base `96f7980c`; **A** = hand arithmetic;
**U** = unresolved. Code addresses are AnnoWelt unless AnnoGame is named;
file offsets below address record payloads. No original listing or bytes are
included. Independent address/data review is pending the orchestrator.

Reuse [wildlife.md](wildlife.md) §§5–6 for the manager, shared construction and
cleanup, and [hunter.md](hunter.md) plus wildlife §8 for the four pursuit/take
jobs. Their reads are not repeated here. This extends the whaler note in
[economy-booking.md](economy-booking.md#b1-financial-only-inert-selector-addendum),
without editing those three documents.

Ghidra is **NOT_RUN**: `java -version` reports no runtime and no Ghidra install
was found in the established location. As in wildlife, `tools/disasm.py`
(Capstone5.0.7/pefile) and direct table reads supplied the instruction evidence.
HE86 is **NOT_RUN**, its DLL being on Windows only. Class names below are the
[edition comparison's labels](../research/he-vs-gog-comparison.md#4-class-names-from-the-history-edition-applied-to-the-gog-module-s),
not newly obtained GOG RTTI or a fresh HE equivalence proof.

## 1. Original data and the downstream chain

**P — reproducible data addresses.** Read with `tools/figuren.py` and
`tools/anlagen_roads.py::parse_anlagen/record_of`, then field reads from the
same records. Anlagen.dat SHA-256 is
`b2317a5c24b8c9fe796cf40f3fedaefcfa30ad3f185df9e69b2dca6a03d8dd75`;
FigurenHeader.dat is
`29fa1f0528fb337d9a61e8b73169281fd05bb27efa6df65638580a4c36c468a8`.
The following facts are record data, not observed original-runtime behavior.

| Entry | HEAD payload file offset | PROD3 payload | WARE / ROHSTOFF payload |
|---|---|---|---|
| Anlagen821 | `0x107641` | `0x1076a4` | `0x107717` / `0x107737` |
| Anlagen1028 | `0x12392d` | `0x12398e` | `0x123a01` / `0x123a22` |
| Anlagen1317 | `0x14153e` | `0x1415a1` | `0x141614` / `0x141635` |

**P/D — interpretation.** HEAD+0/+1 give footprint, +3/+4/+5/+6 costs in
ZIEGEL/HOLZ/WERKZEUG/MAMOR tonnes, +8 the u16 gold cost, +0x11 action radius,
+0x1c the u16 type flags and +0x2c HP. The existing AnnoGame load conversion
`0x100095c7`–`0x10009717` turns PROD3 time/input floats into per-step 8.8
amounts; capacity is PROD3+4 times256; figure id0 is u16 PROD3+0x32.
These conversions are already documented in [production.md §2](production.md).

| P: entry / PRODTYPE | Footprint; gold; goods costs | Input -> output | Converted per step; output capacity | Figure0; stage |
|---|---|---|---|---|
| 821 / WALFAENGER15 | 4×4;500; bricks5,wood5,tools5 | WALFISCH139 -> WALSPECK22 | need16 ->17;2560 | 1511; INFRA_STUFE_3D |
| 1028 / HANDWERK1 | 3×3;500; bricks5,wood5,tools5 | WALSPECK22 -> LAMPENOEL35 | need12 ->13;1280 | 203; INFRA_STUFE_3D |
| 1317 / MARKTSTAND161 | 1×1;50; wood1,tools1 | offers LAMPENOEL35, no input | offer only, not a production recipe | no figure; INFRA_STUFE_3D |

**P — whaler821 details, addresses above.** Flags `0x2087`, HP5000,
radius60; PROD3 time0=15, input factor0=.9375, first figure-type count1,
maximum figure count1. All three buildings have no FORSCHTYPE record.
For1028 time0=20 and input factor0=.9375; its radius is20 and HP6000.
Stall1317 radius4/HP2500; its PROD3 numbers do not authorize producing oil.
Goods ids22/35/139 reuse the AnnoGame name table documented in state.md
Appendix A;139 is an animal pseudo-good, outside the city warehouse range.

| P: figure | Data addresses in FigurenHeader.dat | Decoded fact |
|---|---|---|
| 1202 | BEWOHNTYPE `0x19bc781`, ROUTETYPE `0x19bc795`, WARE `0x19bc7ac`, LOGIC `0x19bc7c5` | WAL; UNUSED route; WALFISCH; layer0; capacity15×256=3840; HP60 |
| 1511 | BEWOHNTYPE `0x1a0041e`, ROUTETYPE `0x1a00439`, LOGIC `0x1a0046b` | WALFAENGER; UNUSED route; layer2; capacity20×256=5120; HP5000 |
| 1559 | BEWOHNTYPE `0x1a2af66`, ROUTETYPE `0x1a2af80` | WALSYMBOL; UNUSED route; a source symbol, not the whale1202 |

**P/D — figure fields.** Capacity comes from LOGIC+0x11, HP from u16+0x1e
(AnnoGame `0x1000fb50`, `0x10010368`–`0x10010387`); layer is LOGIC+0x10.
AnnoGame kind-name table `0x10024cb8` is indexed from0: WAL=54,
WALSYMBOL=147, WALFAENGER=178. UNUSED ROUTETYPE is not evidence that these
classes lack routes: their own factories below select them explicitly.

**T — what main already ports.** `src/sim/src/types.cpp:1000` class table
maps Workshop to Workshop tick/payload and Whaler to None/Inert;1028 therefore
needs no new oil-production class. The original workshop is ctor `0x100688f0`,
tick `0x100680b0`; whaler821 is still inert. MarketStall is None/Inert and list0
is never scheduled ([state.md class table](../sim/state.md)).
However, generic stall availability/billing and household buying already exist
in `src/sim/src/resident.cpp` (`supplier_available`, `supplier_enabled`,
`resident_buy`) and `resident_job.cpp` (`supplier_model_of`, purchase step).
They use the stall's offered goods and the city store; this reuses original
stall billing `0x100702f0`, not a new oil-specific tick. Thus “no stall tick”
does not mean “no household oil purchase path”. A whole live oil chain was NOT_RUN.

**P/U — alternative content, not a whaling substitute.** The same raw scan
also finds WALSPECK outputs in native Anlagen3501 (PIONIER) and3515 (PLANTAGE),
both with EISFISCH input. Their availability and complete production paths are
not read here. Do not infer that whaling is the only authored source, or that
this proves those native sources work on main.

## 2. GOG class identities and confidence

**D/U — method.** `analysis/pe_symbols.json` records zero AnnoWelt RTTI names.
The matches below follow factory kind -> constructor -> installed primary
vtable -> function/embedded-job profile. All receivers listed are unadjusted
object bases (table at object+0). GOG role identity is strong; literal HE-name
equivalence remains structural and unverified by a fresh HE read (OQ-591).

| Supplied HE label | GOG primary table / slots | Positive GOG evidence (D) | Confidence |
|---|---|---|---|
| tProdWalfang | `0x100c9418` /75 | ctor `0x100746a0` installs it at `0x100746c8`, inherits Plantage; tick/loader `0x10073720`/`0x100741a0`;64/75 slots equal Farm `0x100c8ae0` | strong functional match; HE comparison NOT_RUN |
| tBewohnerWal | `0x100c5f98` /101 | kind54 in manager factory `0x1009a110`, arm `0x1009a5bb`, alloc0x90, ctor `0x1004f410`;92/101 slots equal Old animal `0x100c653c` | strong whale-class match; not a New animal |
| tRouteWal | `0x100c7218` /22 | whale +0xa4=`0x1004ea70` accepts route0 only, ctor `0x1005d9d0`, installs table at `0x1005da56`;20/22 slots equal Old route `0x100c7348` | strong route match; later whale life deferred |
| tRouteWalfaenger | `0x100c72a8` /22 | ship +0xa4=`0x10043a20` accepts45 only, ctor `0x1005df70`;20/22 slots equal hunter route `0x100c6f90`, differences +0x34/+0x4c | strong route match |
| tOccurrenceWal | `0x100c7e70` /37 | occurrence factory `0x10094e00` kind147 arm `0x10094f51`, alloc0x38, ctor `0x10066f20`; WHALE loader/tick and hardcoded1202 creation | strong occurrence match; not the figure |

**D — whaling ship, distinct from whale.** Kind178 selects `0x1009a5eb`,
allocates0x10c and calls `0x10044150`, installing `0x100c4fc0` (97 slots)
after the ship constructor `0x100405d0`. This is a ship class, not a resident
walker. Its route setter `0x100400b0` stores the route pointer; idle query
`0x100401c0` returns that pointer. No route is running when it returns0.

## 3. Whale origin, reconstruction, replenishment and count

**D — placed occurrence.** The source is an island-owned runtime occurrence
for figure-type WALSYMBOL147, not the fauna sweep `0x10093480`.
Occurrence-manager create `0x10095100` uses the factory above, attaches the type
and island, copies the supplied float position through `0x100320e0`, and
registers a category6 chunk object (`0x10095221`–`0x10095255`). Its loader
`0x10095eb0` extracts a figure type from bits14..25 of a saved occurrence
header and creates/loads it (`0x10095f8b`–`0x1009611c`). This establishes the
runtime record path; initial map-authoring/sea-placement policy is not inferred.

**P/D — bounded shipped example, not a world corpus census.** In
`Scenes/0200Single/Single_08.sze` (SHA-256
`6e04fd8fd841de2915ceb6ef8c9b1619f42af0b5cd229a931585997340633028`),
INSEL5 index3 is64×64 at origin(24,16). Its WALSYMBOL1559 ENTRY2 payloads at
file offsets `0x2c727`, `0x2c799`, `0x2c7cb` have local x/z(11.5,3.5),
(58.5,55.5), (3.5,56.5). Adding the origin as the original loader does
(`0x10095fe8`–`0x10096012`) yields world centres(35.5,19.5), (82.5,71.5),
(27.5,72.5). The corresponding HEIGHT bytes are0. Each contains WHALE18 with
three counters0, offsets0 and ids0xffff: saved empty sources, not three
already loaded whale figures. Data was checked with `tools/insel.py` and
`tools/sze.py::tagged`; this is one island sample, not a claim about every map.
**T:** `src/sim/src/scenario_formats.cpp:1268` keeps only BEWOHNTYPE145 ore
occurrences and skips WALSYMBOL147. The source state is therefore missing on
main even where the original scenario carries it.

**D — instance fields (`0x10066f20`–`0x10066f49`, base `0x100648a0`).**
O+4 is the attached type interface, O+8 island, O+0xc another
attached interface; position is float x/y/z at+0x10/+0x14/+0x18; stamp is u16
+0x1e. Three figure pointers at+0x28/+0x2c/+0x30 and three retry bytes at
+0x34/+0x35/+0x36 start zero. The type/world/island attach selectors are
`0x10064740`/`0x10066ef0`; do not reinterpret these pointers as figure ids.

**D — WHALE load/save (`0x10066c00`–`0x10066e45`).** Find subrecord WHALE
(string `0x100dce50`); its count is payload-size/6. Each six-byte entry is:
byte0 retry counter (used when no figure), byte1 unconsumed here, signed byte2/3
x/z offsets from the occurrence's centre, u16+4 saved figure id. Id0xffff means
no saved whale: set the pointer null and restore byte0. Otherwise create1202 at
centre+offset, passing the saved id and time0; success stores the pointer and
zeros its counter. The saver writes exactly three entries, zeroes the first
word for a live pointer, and saves truncated centre-relative x/z plus its id;
an empty slot writes its counter in that first word and id0xffff.
The loader skips reconstruction if island property16 is nonzero
(`0x10066c82`–`0x10066c90`); getter `0x10087c30`, table `0x10087c98`, returns
island byte+0xc3 for this key. Its named policy and outer scheduling effect are
U, OQ-592. The loader itself has no <=3 count guard: valid saver output is18
bytes; larger/malformed records are not a modern parsing contract.

**D — timing and count (`0x10066e50`–`0x10066ec8`).** At wrapping u16
elapsed>=2500, add2500 to the stamp once. In slot order0,1,2, skip nonnull
pointers. For each empty slot, increment its u8 retry counter; at counter>=10,
try creation at **the occurrence's own centre**, id-1 and the current time.
Success installs the pointer and resets counter0. There is no extra random
population count, random delay or random coordinate in this body. From a fresh
instance, ten eligible visits can create all three in the same invocation.
Failure keeps the incremented byte:255 wraps0 on the next visit, temporarily
falling below the threshold; it is not a saturating counter.

**D — position/create (`0x10067000`–`0x100671cc`).** Before allocating, require
1<=x<world.width-1 and1<=z<world.height-1. The local helper does not test sea
depth or choose a different coordinate. The creation vector uses the supplied
x/z and height `global[0x100e2a50]+2` (`0x100670a3`–`0x100670c5`); the global
is initialized via `0x10066b10`/`0x10066b70`, not a new random height.
Always resolve figure type1202 (`0x100670cb`), create through manager slot0,
then attempt occurrence/island attachment, set home property0x2f from the
occurrence's centre, request the type's route (0), and install it. Failure of
manager creation returns null; missing route cleans up/removes the new figure
and returns null. The helper installs the route without starting its jobs
(`0x100334f0`). Shared manager construction is the already read wildlife §5.
No arbitrary-ocean placement rule follows from these local bounds (OQ-592). The occurrence
attachment return is not checked: inherited figure+0x28=`0x10035c10` accepts
type/world/island selectors, not occurrence selector0x1800. Replenishment
release instead uses the saved-home lookup below; do not invent a required
direct whale-to-occurrence pointer from that attempted call.

**D — cap means tracked slots.** There are three slots per occurrence, not
a world-wide live-whale census. Whale cleanup `0x10050d70` reaches island
+0x148=`0x1008bc80`. Its whale-specific kind54 arm (`0x1008bcb0`–`0x1008bd02`)
looks up the occurrence by the whale's saved home, sends property0x1f with the
figure pointer, and `0x10066ed0`/`0x100671d0` clear only a matching pointer and
its retry byte. It does not wait for registry removal. The occurrence destructor
`0x10066f70`–`0x10066ff9` cleans up/removes each remaining tracked whale.
This justifies replenishment after cleanup; it does not establish a global cap
or count ending corpses as still occupying these slots. Whale behavior after
creation stays with1b; only this count-release boundary was read here.

## 4. Whaler tick, routes, stores and loader

**D — B is the building base.** Constructor `0x100746a0` initializes output
u16 B+0x18, reserved output+0x1a, raw+0x1c and activity+0x1e to0; switch+0x22=1;
queue count+0x44 and sequence byte+0x45=0; launch offsets float+0x48=2,
+0x4c=6.5; selected boat-array index dword+0x50=-1. Five pointer slots at
+0x24..+0x34 are inherited; ordinary autonomous dispatch uses the selected
one, not five independent hunting workers (`0x10074540`/`0x100745a0`).

**D — tick order (`0x10073720`–`0x10073b34`).** Wrapping u16 elapsed<500
returns; otherwise last+=500 **once**, then switch0 returns. Next process a
queued boat, decay activity by trunc(activity*127/128), try hunting dispatch,
and finally convert raw stock to output. This body has no Farm-style explicit
flag0x10/0x20 suspension branch; do not import that predicate from another class.

**D — constructing the boat (`0x10073756`–`0x100737e5`).** Queue-head active
byte atB+0x3c must be nonzero. If its u16 progress at+0x3e is below0x6400,
add0x355; on crossing, resolve **1511**, call launch helper `0x10073d50`, then
remove the head even if creation fails. Enqueue `0x10073b40` refuses count>=1,
creates a twelve-byte head at+0x38, initializes active/progress/sequence and
charges costs through the ship type/owning interfaces. This is a requested boat,
not an automatic consequence of placing building821; the public queue-command
contract and legal failure/recovery callers remain OQ-593.

**D — launch search (`0x10073d50`–`0x1007416f`).** Refuse if selected index
is not-1. Rotate the building-relative offsets (2,6.5) by building rotation,
query at most64 mask0x80 nearby figures within40, then deterministically search
candidate offsets outward in even increments below30. Candidate filtering rejects positions within
squared-distance25 of a returned figure or failing `0x10074880`'s 3×3 tile-code
check (code9 is rejected). A found candidate replaces the initialized base
launch vector; exhausting this search does **not** return failure: the later
manager call still uses the original base vector (`0x10074080`–`0x100740db`).
No random position selection occurs. Successful manager creation creates1511, attaches building/island/player, saves its array index, and assigns its
cargo goods to input0 (WALFISCH). This identifies the deterministic procedure;
float/path/query transitive closure remains OQ-594.

**D — hunt dispatch (`0x10073803`–`0x100738a7`).** Require selected index!=-1,
selected ship idle (its +0xa8 returns0) and raw<type capacity0. Search with
`0x10071da0(8)` (existing hunter read): strict nearest type-goods match for
input0, at most512 returned figures, ties keep earlier enumeration; no new
ownership/claim predicate is introduced. For a target, request ship route45,
set the first job's target id with operation2, and install the route. No target
means no new hunt; existing raw can still be converted later in this tick.

**D — route/cargo, reuse rather than repeat hunter jobs.** Constructor
`0x1005df70` builds four shared jobs at route R+0x10/+0x20/+0x28/+0x30:
kind13 approach (`0x100c7060`),21 firing (`0x100c7038`),5 corpse walk
(`0x100c7010`),11 take (`0x100c6fe8`). The fifth, R+0x38, is kind8 return
(`0x100c7300`), start `0x10056870`, using ship movement/path callbacks.
The shared take removes an ending, non-removal-guard victim and adds the whale
capacity (1202:3840) to ship cargo, as in wildlife §8. Ship hold table
`0x100c4f28` +0xc=`0x1003b950` adds to the u16 quantity without saturation
(and invokes its owning-ship speed hook); the concrete hold receiver therefore
does not silently clip that award. This reuses [ship-cargo.md](ship-cargo.md). Completion
`0x1005dc30` copies ids through pursuit nodes; kind11 selects the return node
R+0x74 and configures a building-side destination by rotation. Kind8 completion
`0x1005deb1`–`0x1005df0e` passes cargo goods/amount to home+0xc0, clears cargo,
restores the goods selector and ends continuation. The home receiver is
`0x1006ee70`: add low16(amount) to **raw B+0x1c**, ignoring the goods argument.
It does not directly add WALSPECK output or city stock. Failed-start fallback
`0x1005dbe0` sends kind21 to approach nodeR+0x44, kinds13/5 to return nodeR+0x74,
others to null. Failed corpse/take and absent/changed prey retain the shared
hunter limitations, including target-id reuse OQ-584; callbacks remain OQ-594.

**D — conversion (`0x100738c8`–`0x100739d9`).** If output>=capacity0, take
the blocked-status arm, without conversion. Otherwise let
`r=min(128,trunc(raw*128/need0))`; r<64 takes missing-input status. For r>=64,
debit `d=trunc(r*need0/128)`; d==0 returns. Add `o=trunc(rate0*r/128)` to output
and16*o to activity, both u16, with no post-add capacity clamp. Status/display
updates follow. Whaler821's need16/rate17 and capacity2560 are the P data above.
The **hunt check precedes output-full** and checks raw separately: a full
WALSPECK output store is not by itself proof that no further hunting occurs.

### 4.1 Version1 load: field by field

**D — framing (`0x100741a0`–`0x100743c7`; saver `0x100743d0`–`0x10074505`).**
Read one version byte; non1 returns1 without parsing this class's remaining
fields. For version1, request a zero-filled ten-byte production prefix, then a
zero-filled ten-byte boat header. Actual stream-read counts accumulate into the
out-consumed value. The prefix's own first byte is not a length controlling this
loader. These are original operations, not a validated modern parser contract.

| Version1 inner offset (outer offset = inner+1) | D: consumed meaning | Address |
|---|---|---|
| 0 | saver writes0; loader ignores | `0x1007440e`–`0x10074447` |
| 1 | bit0 enables output line0 through type+0xac and building+0xc8; other bits unused here | `0x10074232`–`0x10074254` |
| 2..3 | u16 output ->B+0x18 | `0x10074218`, `0x1007422e` |
| 4..5 | u16 raw ->B+0x1c | `0x1007421d`, `0x10074238` |
| 6..7 | saver zeroes; loader does not assign B+0x1a from it | `0x10074200`, `0x10074410`–`0x10074447` |
| 8..9 | u16 activity ->B+0x1e | `0x10074224`, `0x1007423f` |

| Boat header offset (outer offset = offset+11) | D: consumed meaning | Address |
|---|---|---|
| 0..1 | saved figure id, passed to manager creation | `0x10074330`–`0x1007434e`; saver `0x10074477` |
| 2..3 | figure type id; zero means no boat to reconstruct | `0x10074285`, `0x100742e7`–`0x100742f6` |
| 4..5 | saver zeroes; not a selected-slot value | `0x10074455`–`0x100744cc` |
| 6..7 | u16 x ->creation vector x | `0x10074294`–`0x100742b6` |
| 8..9 | u16 z ->creation vector z | `0x10074298`–`0x100742d0` |

**D — nested figure (`0x10074302`–`0x100743af`).** For nonzero type, create at
(x, global sea-height `0x100e2cbc`, z) with saved figure id and world property4
(time). On success attach building and island, insert into the building pointer
array, save selected index, then call figure+0x6c with the remaining declared
length/context. Accumulate its consumed count; a zero return propagates0.
Other branches return1. This body does not prove transactional rollback after a
nested load fails. Queue progress/count/sequence and launch offsets are not
serialized here: ctor defaults remain, while nested figure persistence holds
boat state. The already accepted economy-booking selector covers only the
complete version1 first11-byte financial prefix, not boat restoration.

## 5. Draw ledger, hand controls and port boundary

**D/U — direct draw ledger.** These are position-in-processing rows, with the
exact condition and absence of a direct world+0xb4 draw in the named bodies.
No fabricated modulus or transitive zero is supplied. OQ-594 keeps the remaining
callback proof separate; this is not a whole-lifecycle reference certificate.

| Position | Address / body | Modulus | Selects | Condition |
|---|---|---|---|---|
| Create the WALSYMBOL occurrence | `0x10095100`, `0x10066f20` | none | constructor's zeroed three slots | kind147 placement/load |
| Reconstruct saved whale entries, file order | `0x10066c00`, `0x10067000` | none | saved offsets/id; fixed type1202 | WHALE entry id!=0xffff and load gate permits |
| Replenishment, slots0/1/2 | `0x10066e50`, `0x10067000` | none | centre position, fixed type1202 | empty pointer, incremented byte>=10 |
| Prepare/install whale route | `0x1004ea70`, `0x1005d9d0`, `0x100334f0` | none | type route0 | successful creation; jobs not started here |
| Queue progress and boat launch | `0x10073756`, `0x10073d50`, `0x10074880` | none | deterministic launch candidates, type1511 | active queued request crosses0x6400 |
| Whaler idle dispatch | `0x10073803`, `0x10071da0`, `0x10043a20`, `0x1005df70` | none | nearest input-goods target, route45 | selected idle ship and raw<capacity |
| Shared take / return credit / conversion | `0x100547e0`, `0x1005deb1`, `0x1006ee70`, `0x100738c8` | none directly | target capacity ->raw; ratio ->output | phase-specific predicates above |
| Shared movement/search/type/projectile/event callbacks | OQ-578 / OQ-594 | U, not0 | not certified | actual receivers must be closed before complete draw vectors |

**D — negative class control.** Nearby `0x10066900` contains world draws at
`0x1006693e` (%0x970fe0 for a timer) and `0x10066956` (%3 for a branch).
It belongs to table `0x100c7dc8`, installed by the **different** constructor
`0x10066a40`, not WALSYMBOL147's table/tick. Its CRT call `0x100b68a9` in that
constructor is not a world draw either. Do not copy those nearby draws into
tOccurrenceWal or treat code adjacency as a class match.

**A/D — distinguishing hand controls (not executable vectors or original runs).**
From `0x10066e50`, empty counter9 becomes10 and attempts creation; nonnull
pointer skips it; counter255 becomes0 and does not attempt. Three empty slots
at9 can all create in one visit, with no direct draw. From `0x10073737`,
elapsed499 skips,500 runs,1500 still advances last by500 only. Queue progress
30×853=25590 remains below25600; the31st eligible active call crosses it.
From `0x10073901`, need16/raw7 gives r56 and no output; raw8 gives r64,
debit8/output8; raw16 gives r128,debit16/output17. Output2550 can become2567;
output2560 blocks conversion, but the earlier raw-stock hunt predicate still
stands. These distinguish saturation, per-call catch-up and unconditional
“full output stops hunting” misreadings. An ending whale can release a slot
before corpse removal, unlike a true live/corpse census (`0x1008bcb0`).

**T/U — later port inventory, at base96f7980c.**

| Needed | Already on main / boundary |
|---|---|
| Original821/1028/1317 and1202/1511/1559 type records, goods22/35/139 | registry readers exist (`types.cpp`, `production.cpp`); class behavior is separate |
| Oil conversion and carrier/city-stock flow | generic Workshop already ported; no new HANDWERK class required |
| Oil offers and household debit | existing MarketStall supplier/purchase path; no scheduled stall production tick required |
| Whaler building state/tick, requested-boat queue and selected boat | Whaler None/Inert; new behavior/state needed; financial enabled fact alone already read |
| Whale occurrence slots, retry bytes/stamp, saved-home release, source record load | WALSYMBOL OCCUR entries are skipped by scenario_formats.cpp; occurrence behavior/state and import are needed |
| Whale1202 life and route0 | read in §§6–7: Old tick, nine overrides, walk/stand route and five ordered walk draws; implementation/state still absent |
| Whaling ship1511 and route45 | generic sailing/cargo exists; §§8–9 add command0x71, paid queue, detach behavior, movement callbacks and ship attack draw; specialized behavior/state still needed |
| Save/replay/rules/content identity | complete class-specific continuation and a later explicit identity contract after ADR0006; no new state number here |
| Exact indirect draw/float proof | OQ-594 narrowed by §9; OQ-595 retains sea legality; OQ-578 shared closure remains; no complete total or float vectors claimed |

Open points are [OQ-591–594](../open-questions.md), with595–598 unused.
HE86, Ghidra, original-runtime experiments, live oil-chain proof, engine gates,
reference/vector execution and whale life after creation are NOT_RUN here.

## 6. Whale life: nine overrides (WHALE-RE-1b, 2026-11-13)

**D — scope and comparison.** This extends the accepted sections 1–5 and their
[1a review](../orchestration/reviews/2026-11-13-whale-re-1a-review.md).
Comparing all 101 pointers at `0x100c5f98` with Old animal `0x100c653c`
gives exactly the nine differences below. Addresses in this section are
AnnoWelt unless another module is named. Shared code is reused from
[wildlife §4](wildlife.md#4-animal-tables-tick-movement-and-death-1b-2026-11-12)
and [§7](wildlife.md#7-routes-stand-selection-and-exact-walk-draws); it was not re-read as a
second whale implementation. The evidence tags and GOG reference hash above
still apply. Independent address verification awaits the orchestrator.

| D: slot | Whale implementation / Old implementation | Whale-specific behavior; direct world draws |
|---|---|---|
| `+0x50` | `0x1004f3a0` / `0x100348b0` | Copies supplied position to current position, motion origin and extra vector at figure+0x84; truncates x/z into home words+0x64/+0x66. None in `0x1004f3a0`–`0x1004f405`. |
| `+0x7c` | `0x1004e830` / `0x100332b0` | Display pass uses animation lookup and interpolated position `0x1004f140`. Sound selection calls CRT `0x100b68a9` at `0x1004e935`/`0x1004e94e`; these are not world draws. None directly in `0x1004e830`–`0x1004e9ba`. |
| `+0x80` | `0x1004e9c0` / `0x10033440` | Alternate display/picking pass uses the same interpolated position. None directly in `0x1004e9c0`–`0x1004ea6c`. |
| `+0x94` | `0x1004f280` / `0x100349c0` | HP setter enters ending state on a positive-to-nonpositive transition; details below. None directly in `0x1004f280`–`0x1004f36e`. |
| `+0xa4` | `0x1004ea70` / `0x10050c10` | Route factory accepts route 0 only, allocating `0x64` bytes and calling `0x1005d9d0`. None in `0x1004ea70`–`0x1004ead1`. |
| `+0xc8` | `0x1004eda0` / `0x100507e0` | Configures searcher mode 2 with layer/code 9, rectangle from `0x1004ed00`, and the current position as start. None directly in `0x1004eda0`–`0x1004edeb`. |
| `+0xcc` | `0x1004eae0` / `0x10050910` | Converts the supplied searcher's result into whale motion descriptors. None directly in `0x1004eae0`–`0x1004ecfc`; searcher closure is not thereby zero. |
| `+0x17c` | `0x1004edf0` / `0x100343c0` | Consumes descriptors, changes facing/action, splits long movement into units, or enters ending state for terminal descriptor kinds. None directly in `0x1004edf0`–`0x1004f0f0`. |
| `+0x190` | `0x1004f430` / `0x10035d50` | Returns world interface `0xc00`, index 2. None in `0x1004f430`–`0x1004f43f`. |

| Requirement | Positive evidence | Distinguishing hand control | Review disposition |
|---|---|---|---|
| Whale class/life | all101 pointers compared; nine overrides above | current-centered radius30 versus source-centered leash; Old tick versus New budget | pending orchestrator |
| Route/jobs/draws | two route overrides and whale walk start below | five draws despite path failure; no Old completion parity | pending orchestrator |
| Request | four-module caller inventory, command0x71, costs in §8 | count-full versus unaffordable; inactive request still paid | pending orchestrator |
| Route45 callbacks | concrete ship receivers/ranges in §9 | modulus1 attack still draws; return threshold differs from whale | pending orchestrator; transitive limits explicit |

**D/P — tick and speed.** Tick slot `+0x74` remains `0x10034760`, not New's
`0x10051750`: its motion integration and route/job action boundaries are the
shared Old tick in wildlife §4. The path conversion reads type `+0x60(1.0)` at
`0x1004ec22`. AnnoGame `0x1000fb10` multiplies by type float+0x10;
HEAD loading `0x1000ff09`–`0x1000ff50` assigns that float from HEAD+4.
Whale1202's HEAD+4 is approximately .01 (FigurenHeader.dat, same entry as §1),
so this path uses that speed, without New's per-figure speed multiplier.
Descriptor helper `0x1002fde0`–`0x1002ff1b` stores the supplied speed as rate
and cardinal velocity, with diagonal normalization constants for diagonal
steps. These are original internal movement units, not a measured speed in
seconds. There is no world draw in that descriptor helper.

**D/U — where it goes.** Constructor `0x1004f410`–`0x1004f42b` sets radius
byte+0x11 to30. Rectangle helper `0x1004ed00`–`0x1004ed9c` centers a nominal
61-cell rectangle on the whale's **current** x/z, clips its low edges to zero
and high edges against world width/height. It does not center this rectangle
on the occurrence or pass an island boundary. `0x1004eda0` supplies that
rectangle and code9 to the shared sea-search interface. World factory
`0x100aee00`–`0x100aee34` returns the index-2 object at world+0x6ab0;
construction `0x100aab8e`–`0x100aabdb` installs `0x10012030`, primary table
`0x100c197c`, wrapping the search interface at object+4 (`0x100c1a3c`).
This identifies the actual receiver rather than assuming a land searcher.

**D/U — sea legality boundary.** Path setup/extraction wrappers are
`0x100107b0`–`0x100107e4`, `0x10010830`–`0x1001088d`,
`0x10011500`–`0x1001163a`, `0x10011640`–`0x100118c8`.
The inspected fine navigation-mask path `0x1000fa50`–`0x1000fd5d` adds1
to setup code9 before calling world+0xec (`0x1000fae0`–`0x1000faed`).
Thus it dispatches **selector10**, not selector9: `0x100abddb` reaches
`0x100abc60`–`0x100abd00`. Without an island, that fills cells with8;
with one, it calls island `0x10090350`–`0x10090550`.
The first pass compares the low byte of truncated global float
`[0x100e34d0]/[0x100ca6fc]` against the integer mean of four terrain-height
bytes, writing9 when the threshold is below the mean, otherwise8
(`0x100903af`–`0x100903f8`). A second pass can write9 using an existing
object type's +0x90 result plus the tile's byte+4 (`0x1009049d`–`0x100904fa`).
The fine flood traverses cells equal to8 (`0x1000fbca`–`0x1000fc69`).
These read bodies have no direct world draw; the selected object-type callback
and the complete search expansion/ties are not thereby certified zero.

**U — OQ-595.** The physical units/writers of that global threshold and the
complete selected object-height/search graph are not settled here. Do not turn
this arithmetic into an invented numeric minimum sea depth or claim every
coarse/fine path branch was closed. The inspected whale setup does not pass an
island rectangle or source-centered leash. Its source still supplies the walk
job's fallback goal (§7), distinct from the current-centered search rectangle.

**D — ending, not an imported age rule.** In `0x1004f280`–`0x1004f36e`, an
already nonpositive HP receiving another nonpositive write returns without a
new ending transition. Otherwise store the low HP word; a nonpositive result
with ending bit2 clear cancels the action, clears translation, copies the
motion origin, sets remaining motion15 and rate .01, sets ending bit2,
selects action17, and calls cleanup `+0x110` (`0x10050d70`, already §3).
The shared tick subsequently performs terminal manager removal (wildlife §4).
Terminal motion descriptor kinds other than normal0 and facing3 also set
ending bit2 and call cleanup (`0x1004ef34`–`0x1004efb9`). Neither these
nine overrides nor the whale route contains New's quantity-debit/25000-budget
age mechanism. That mechanism must not be copied from wildlife §4 into WAL.
Hunting can drive HP through the setter; the already accepted saved-home
cleanup releases the occurrence slot before final registry removal (§3).
This is not a claim that every external script/removal caller has been read.

## 7. Whale route, jobs and ordered draws

**D — two overrides.** Table `0x100c7218` and Old route `0x100c7348` each
have22 slots. Only `+0x34` (`0x1005d950` versus `0x1005e2b0`) and
`+0x4c` (`0x1005d920` versus `0x1005e280`) differ. The constructor
`0x1005d9d0`–`0x1005da97` links active nodes R+0x10 and R+0x1c to jobs
R+0x50 (table `0x100c7270`, kind12) and R+0x60 (`0x100c64c4`, kind3).
The resource-job tables also constructed at R+0x40/+0x48 are not active nodes
in this route. Do not transplant Old's four-node habitat/feeding loop.

**D — transitions (`0x1005d920`–`0x1005d9ca`).** On initial selection,
copy figure property0x2f home x/z into the first job using operation2.
On subsequent selection, complete the current job then select R+0x10 again,
without testing carried quantity or drawing. A failed kind12 start selects
R+0x1c; other failed starts return null. Thus a successful walk leads to
another walk; a failed walk can stand, then return to walking. Old's parity
draw on empty kind3 completion (`wildlife §7`, `0x1005e35d`) is **replaced**,
not an extra whale draw. The shared kind3 stand job is reused from wildlife
§7; its type timing and quantity completion are not new whale random choices.

**D — walk start (`0x10058680`–`0x1005889c`).** Only start slot+0xc differs
between whale job `0x100c7270` and land walk `0x100c64ec`; completion/cancel
`0x100588a0`–`0x100588b4` reset attempt byte+4, and operation2 setters/getters
`0x100588c0`–`0x100588f4` transfer the packed home x/z at job+6/+8.
Query up to256 mask0x80 figures within radius20 of current whale position,
and retain the first returned pointer if the count is nonzero. This is not
an explicit nearest-choice loop and not a whale-target query.

**D — goal choice.** Draw two magnitudes and signs in the order below.
With a returned figure and signed dword job+0xc<4, add the offsets to that
figure's current x/z, truncate, then increment +0xc. Otherwise decrement
+0xc with floor zero and add the offsets to the saved home words.
Then consume a fifth draw, discard its value, and call world+0x8c on the
unclamped goal; its return is unused too (`0x100587c4`–`0x100587d4`).
Clamp the goal to [0,width] and [0,height], including the high bound as
written. Increment attempt byte+4, supply a radius1 goal to the searcher,
install callback `0x10054270` with threshold0x40, then call whale+0xcc.
All five draws precede any search failure. Land's three radius/angle pairs
are not used by this overridden start.

| D: position in this start | Draw address | Modulus/mask | Selects | Condition |
|---|---|---|---|---|
| 1, after nearby-figure query | `0x100586f7` | `%5`, then+7 | x magnitude7..11 | every invocation |
| 2 | `0x10058713` | `%5`, then+7 | z magnitude7..11 | every invocation |
| 3 | `0x1005872f` | `&1` | odd negates x | every invocation |
| 4 | `0x10058743` | `&1` | odd negates z | every invocation |
| 5, after goal branch and before lookup/clamp | `0x100587c8` | none; discarded | no selected value | every invocation, including failed paths |
| Route completion/fallback | `0x1005d920`–`0x1005d9ca` | none | fixed walk/stand transition above | no direct draw |
| Shared search expansion and navigation classification | OQ-594/OQ-595 | U | no total fabricated | transitive closure remains open |

**A/D — distinguishing hand controls.** Draws [0,4,1,0,anything] produce offsets
(-7,+11) before goal/clamp (`0x100586f7`–`0x10058753`); changing the fifth
value does not change that local decision but still advances the stream.
Counter3 with a nearby pointer uses it and becomes4; counter4 uses home and
becomes3 (`0x10058757`–`0x100587c2`). These are arithmetic controls, not local
tests or running-original observations.

## 8. Boat request callers, charge and recovery (OQ-593)

**D — cross-module caller inventory.** Raw pointer references to
`0x10073b40` in AnnoWelt resolve to the whaler table entry `0x100c94f8`
(primary `0x100c9418`+0xe0). Executable-section scans of AnnoWelt, AnnoGame,
AnnoFrame and View found no direct call to that address. Inspecting the
literal virtual-slot candidates gives the following receiver distinctions;
this is static caller evidence, not a running-game coverage claim.

| D: module | Enqueue-capable caller / exclusion evidence |
|---|---|
| AnnoWelt | No enqueue call identified. Its56 literal calls at +0xe0 are figure actions, type production queries or world/serialization operations, not the whaler production receiver. For example whaler tick's `0x1007394a` calls its **type**; it does not request a boat. |
| AnnoGame | No enqueue call identified. `0x10002b2f`/`0x10002feb` are type production arithmetic; `0x10005e23` is a registry operation. |
| AnnoFrame | Production helper `0x10059bb0`–`0x10059bde`: call production+0xdc, then production+0xe0 at `0x10059bd5` only on a true check. This resolves to `0x10073c10` then `0x10073b40` on the whaler. |
| AnnoFrame, negative control | `0x10059db0`–`0x10059dc6` is a **figure** operation reached through manager lookups in `0x1005e4d0`–`0x1005e588`, not this production request. The other +0xe0 calls are host/renderer/figure receivers. |
| View | No literal +0xe0 call or direct enqueue call identified. No separate View enqueue path was found. |

**D — command path, not an invented button label.** AnnoFrame serializer
`0x1005d890`–`0x1005d8f6` emits command0x71 with building x/z, its first
byte argument, a zero word and a final word argument, length10.
Dispatch table `0x10060388` entry(0x71−0x64) selects `0x100600be`, which
calls handler `0x1005d810`. The handler (`0x1005d810`–`0x1005d88c`)
resolves island/city/production at x/z, and does nothing if any required
lookup is null. It passes payload byte+4 and word+7 to `0x10059bb0`.
These are offsets in the handler payload, after the command envelope.
The whaler's check ignores both arguments; enqueue saves the byte as request
active state and ignores the word. A zero active byte therefore creates and
charges an inactive request, rather than refusing it. Command0x73 is a
different dispatcher arm (`0x100600e8`), not the boat-request command.
No localized UI-button wording or UI automation is asserted.

**D/P — price and units.** Affordability `0x10073c10`–`0x10073cac` resolves
figure type1511, compares player money with type+0x94(1,0), then compares city
stocks with type+0x94(0,i) for i=0,1 and goods type+0x98(i). Enqueue
`0x10073bb8`–`0x10073bfc` debits exactly those values immediately.
AnnoGame getter `0x1000fca0`–`0x1000fcdb` returns the u16 money field+0xf4
for selector1, or a cost byte+0xf9+i shifted8 for selector0;
`0x1000fd20`–`0x1000fd2d` returns the goods id from+0xfc+i.
LOGIC loading `0x10010251`–`0x1001028d` maps LOGIC+0x16 to money and
+0x20/+0x21 to the first two goods quantities. Original1511 LOGIC
(`0x1a0046b`, §1) gives **600 gold, 12 wood and 7 rope**, represented as
3072 and1792 stock units. Its BAUWARE is HOLZ,SEILE, whose loader resolves
names to goods ids (`AnnoGame 0x10010460`–`0x1001053c`). This is the boat
price, separate from the building's500 gold and construction materials.

**D — refusals and lost boats.** Insufficient gold or either good makes
check return0; the Frame helper does not enqueue or debit. Queue count>=1
makes enqueue return0 before any new state or debit, even when affordability
passed (`0x10073b44`–`0x10073b50`). The check itself has no queue-count or
selected-boat test. Enqueue likewise does not test selected index: a paid
request can reach launch while a selected boat exists, and launch refuses
as already read in §4. The tick consumes that head; there is no refund or
retry in that path. Boat cleanup `0x10043c80`–`0x10043cbd` calls home+0x2c;
whaler detach `0x100745a0`–`0x100745f6` clears its matched figure pointer
and sets selected index to−1, without enqueueing. The whaler tick/launch,
detach and the discovered command caller contain no automatic replacement
request: a new command is needed on these paths. No claim about an unread
AI policy emitting that command is substituted for this evidence. Cancellation
and possible refund paths outside this flow remain under OQ-593.

## 9. Route45 movement callbacks and extended draw ledger (OQ-594)

**D — actual ship receivers.** Resolve the callbacks on primary whaling-ship
table `0x100c4fc0`, not land hunter `0x100c4950`. Shared pursuit/fire/take
job bodies remain [wildlife §8](wildlife.md#8-hunter-approach-and-the-resource-walk-class-label);
route45's return job is `0x100c7300`, start `0x10056870` (§4).
The following table says **direct** absence for the stated read ranges.
Unclosed callees do not contribute a guessed zero to a total.

| D: route callback | Selected body/range read | Draw result and effect |
|---|---|---|
| Ship `+0xc8`, search setup | `0x10043b60`–`0x10043c5f` | None directly. Uses home building type radius, rectangle centered on **ship's current** position, clipped to world bounds; same world search interface0xc00/index2, code9, current-position start. |
| Ship `+0xcc`, path conversion | `0x1003fad0`–`0x1003fcfd` | None directly. Calls searcher+0x84 and+0xa4, reads type speed, builds descriptors through `0x1002fde0`, starts via ship+0xd8. |
| Ship `+0xd0`, turn | `0x1003fe50`–`0x1003ff05`, helper `0x1003fdb0`–`0x1003fe3e` | None directly. Appends facing descriptors via+0xd4 until the requested heading. |
| Ship `+0xd4`/`+0xd8`, descriptor insertion/activation | `0x1003ff10`–`0x1003fffd`, `0x1003f910`–`0x1003fa44` | Neither body directly draws. Activation copies the descriptor, sets heading and moving state, and maps the action group. Complete ship tick/callee closure remains OQ-594. |
| Ship `+0xdc`, stop | `0x1003fd00`–`0x1003fda9` | None directly. Clears moving state and snaps the motion continuation. |
| Ship `+0xe0`, timed action | `0x10043fd0`–`0x10044040` | None directly. Stores time/action/facing; no random animation variant selected here. |
| Ship `+0xe8`, target check | `0x10043cc0`–`0x10043fbe` | None directly. Mode2 resolves id, rejects absent/ending targets, compares range, computes target-side geometry, checks world+0x6c, and may turn. Other mode handles building targets. |
| Ship `+0xec`, attack | `0x1003d450`–`0x1003d9cb` | **Yes**, world draw at `0x1003d615`; see row below. Creates/configures a projectile through the manager, rather than crediting cargo immediately. Remaining projectile callbacks stay explicit U. |
| Return start, kind8 | `0x10056870`–`0x1005691d` | None directly. Search setup+0xc8; radius6 goal; callback `0x10054270` with threshold0x900; path conversion+0xcc; on success save selected goal x/z. |
| Return/whale goal callback | `0x10054270`–`0x1005430e` | None directly. Scores squared displacement using searcher coordinate getters, marks/stops search; +0xb0 is **searcher stop**, not a world draw. |
| Approach callback | `0x100575d0`–`0x10057676`, reused wildlife §8 | No direct draw in the accepted read; world geometry closure still U. |
| World line/terrain bridge | `0x100a1680`–`0x100a173b`, `0x100a2660`–`0x100a26a6` | None directly. Resolve islands at endpoints; both absent returns true, otherwise delegate island+0xd4. That island geometry closure remains U. |
| Sea search wrappers / goals / coordinate getters | `0x100107b0`–`0x1001088d`, `0x1000f260`–`0x1000f40d`, `0x1000f4f0`–`0x1000f4f7` | No direct draw in these read bodies. Complete expansion, navigation construction and all selected callees remain OQ-594/OQ-595. |
| Return credit / cargo / raw conversion | §4, `0x1005deb1`, `0x1006ee70`, hold `0x1003b950` | Accepted direct absence remains; cargo's owner-speed hook is not silently treated as a complete tick audit. |

**D/P — attack draw, including modulus1.** At `0x1003d5f7`–`0x1003d634`,
ask type+0xa0(0) for firing-position count, retain its low byte, call world
+0xb4 at `0x1003d615`, divide by that count and pass the remainder to type
+0xa4(0,index,&vector). AnnoGame `0x1000fc50`–`0x1000fc82` returns count
byte+0xee; OBJECTPOS/SCHUSS loader `0x10010560`–`0x10010642` loads one
position per16-byte child after its name. Original1511's OBJECTPOS SCHUSS
has one such position, so its modulus is1 and the world draw still occurs.
The type lookup `AnnoGame 0x1000fb70`–`0x1000fc43` copies the selected
position and returns its projectile type. Creation may fail **after** the draw.
Do not import the land hunter's “no direct draw in its attack body” as a ship
rule. Projectile initialization/event callbacks beyond the read boundary are
not certified zero by this count.

| D: position in boat processing | Draw address/body | Modulus | Selects | Condition |
|---|---|---|---|---|
| Attack, after action time/facing and before projectile creation | `0x1003d615` | `%uint8(type+0xa0(0))`; 1 for original1511 | firing-position entry passed to type+0xa4 | ship attack callback reached, even if later creation fails |
| Return start and goal scoring | `0x10056870`–`0x1005691d`, `0x10054270`–`0x1005430e` | none directly | deterministic goal setup/scoring above | return step starts / search visits a node |
| Movement callbacks | ranges in preceding table | none directly, except attack row | descriptors / turns / checks | callback-specific |
| Full transitive lifecycle total | OQ-594/OQ-595 | U | not a completed reference vector | search/terrain/projectile/type/persistence closure outstanding |

**A/D — distinguishing controls.** An attack with exactly one firing position
consumes a draw and selects index0; a later manager-create failure does not
unconsume it (`0x1003d615`–`0x1003d71f`). Return start uses threshold0x900,
where whale walk start uses0x40, despite sharing callback `0x10054270`.
These are hand-derived controls, not local tests.

**U — stopping boundary.** OQ-594 is narrowed, not closed: direct ship motion,
return setup/scoring, the separate ship attack draw, and concrete sea-search
receivers are now addressed. A complete expansion/tie/terrain and ship
activation/projectile/nested-load audit, and original float evaluation remain
unsettled. OQ-595 isolates the sea-legality threshold semantics required to
state a numeric depth rule. HE, Ghidra, original-runtime experiments, local
engine/reference tests and engine changes are NOT_RUN; the docs-only hosted
CI gate supplies no original-game runtime parity evidence.
