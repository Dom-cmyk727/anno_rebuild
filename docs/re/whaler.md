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
| Whale1202 life and route0 | WAL class/route remain to be read in1b beyond the creation/release boundary |
| Whaling ship1511 and route45 | generic sailing/cargo exists, but specialized pursuit, return and ownership/boat construction are not thereby ported |
| Save/replay/rules/content identity | complete class-specific continuation and a later explicit identity contract after ADR0006; no new state number here |
| Exact indirect draw/float proof | OQ-594/OQ-578 remain; zero direct draws is not a licence to invent a total |

Open points are [OQ-591–594](../open-questions.md), with595–598 unused.
HE86, Ghidra, original-runtime experiments, live oil-chain proof, engine gates,
reference/vector execution and whale life after creation are NOT_RUN here.


## WHALE-RE-1c scalar reference boundaries (2026-11-13)

- **D — schema clarification, not a new rule.** The scalar reference uses this
  document's accepted1b text at `109ec14c` (§§6–9), pending its docs landing.
  The `whaler_tick` record includes two explicit resolved callback facts beyond
  the packet's input summary: `target` says whether the §4 search found prey;
  `launch_slot` is−1 on failure/no launch, otherwise the slot selected by the
  §4 launch helper. `idle` is the fact at the later dispatch check, after queue
  processing. These are supplied outcomes, never inferred sea/search/projectile
  decisions (`0x10073756`–`0x100738a7`, §4). A selected existing boat makes a
  supplied successful launch impossible; the reference rejects that fact.
- **D — local timing/output scope.** `elapsed` is an already computed wrapping
  u16 difference, not a frame interval. Output `advanced` is the stamp increment
  (2500 for replenish;500 for tick), once per eligible visit. Tick `r=0` when
  its gate/switch/output-capacity branch prevents entering the conversion;
  otherwise it is the computed ratio, even if below64. `launch_attempted`
  reports the helper call even when the helper fails. `progress` reports the
  head's computed progress at that call, not a surviving queue after removal
  (`0x10066e50`–`0x10066ec8`, `0x10073720`–`0x100739d9`, §§3–4).
- **D/U — finite integer slice.** HP vectors restrict both HP inputs to signed16;
  wider write/truncation aliases are outside this schema. Walk positions are
  integers before the §7 clamp, with no float interpolation or path execution.
  `whale_route` models selection of walk plus its optional failed-start fallback,
  not a complete multi-boundary tick. OQ-594 and OQ-595 remain open. The reference
  refuses sea legality/depth and indirect search/projectile draw requests rather
  than returning an invented total (§§6–9, `0x10058680`, `0x10056870`).
