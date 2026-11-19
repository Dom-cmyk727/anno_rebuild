# Wildlife creation through return to the sweep

WILDLIFE-CREATE-CLOSURE-1, 2026-11-19. Contract/research only, no engine wiring.
D = original-code/table read; P = parsed source; H = hand derivation;
C = executed private checker; U = unproved. Reference is GOG AnnoWelt 2.0.0.5.
Reuse [wildlife §§1–7](../re/wildlife.md), [decisions](wildlife-decisions.md),
[load boundary](wildlife-load-boundary.md) and [first call](wildlife-first-call.md).

## Intake delta, before new callback reads

| Already established | Missing concrete edge/input | Smallest discriminator | Consumer |
|---|---|---|---|
| HIRSCH decoder, prepend order, stamp /500 | none for this boundary | 499 vs 500 | existing prefix, unchanged |
| Species/ordinary-vs-cave direct draws | selected descriptor supplier/actual class, supplied position | kind 49/51 vs other kind; cave no entrance remains unsupported | creation-effect input |
| New constructor adds 0x200, special type adjustment adds 0x600 | actual descriptor getters and attachment order | type 128 ->0x800 vs ordinary ->0x200 | quantity/type/HP snapshot |
| Manager body addressed in wildlife §5 | free-id/growth/factory/registration/spatial concrete callees | failure before/after allocation; external outcome not invented | owned isolated result vs persistent manager |
| Property 0x2f, cargo goods and island attach bodies | concrete receivers/this adjustments and transitive callback closure | home anchor vs initial supplied float position | helper return |
| Sweep success stores occupied/id/time/goods | post-helper concrete type getters and failure continuation | null helper must not acquire successful occupancy | spawn bookkeeping |
| Later first walk draws known | is any first-action/type animation callback reached during creation? | creation return vs action start | OQ-578 narrowing only |

New static ledger/checker and original runtime observation are NOT_RUN at intake.
No constructor capacity limit, zero callback draws or rollback is assumed here.

## Hand-derived discriminators, before the private checker

These are authored finite-effect expectations, not executed-original observations.
Positions enter as three supplied binary32 words. Home anchors and selected
spatial cells are explicit resolved conversion outcomes; modeling them does not
prove original FP equivalence or callback results. The authored capacity control uses 1024, distinct from the 512 initial addition.
Heap retry/handler behavior
and unsupported receiver classes are excluded, not modeled as zero effects.

| Case | Supplied facts | Hand expectation / nearest wrong reading |
|---|---|---|
| ordinary success | kind 49, type 127, HP 37, assigned id 9, stamp 500, home 1/3 | quantity 512, goods 69, stamps 500, occupied, id 9; not capacity 512 |
| special success | same but type 128, initial constructor HP 1 then descriptor HP 37 | quantity 2048 after addition; HP 37, not ctor HP or quantity 1536 |
| other admitted kind | kind 51, ordinary id 127 | same New effects; route name alone cannot admit kind 50 |
| full-word time/id | assigned id 65535, stamp 65538 | original figure/saved id 65535 (zero-extended); three time words 2; engine ids must not inherit widths |
| home versus initial position | cave with entrance supplied, initial vector differs from tile anchor | preserve all three supplied position words twice; save tile home, not entrance coordinate |
| null creation | prior id 17, timestamp 9, packed 3, goods 255; helper cursor 2 | spawn fields unchanged, helper cursor still 2; no assumed manager rollback |
| wrapping success | sweep low byte 250, initial packed 0x83 | timestamp 54; packed 0xc3; id/goods replaced; preserve bit 7 |
| deferred action | successful ordinary helper | requested 27/current 0/config 4, no action start, no additional game draw in proved normal creation profile |
| unsupported sibling | kind 50; cave without entrance; unknown concrete callback | refuse the checker profile, not a fabricated New/position/zero-draw outcome |

Counterfactuals to challenge: replace constructor addition by capacity, drop the
extra 0x600, use entrance as home, mark occupied after null, or start first action
before return. Wrong expected rows and an empty row set must fail the checker.

## 1. Concrete receiver and callback ledger

All addresses below are D, GOG only. Module hashes and private ranges are in the
[evidence](../orchestration/evidence/wildlife-create-closure-1.md). W = world base,
I = island base, G = catalogue base, Df = figure descriptor base, M = manager base,
F = New animal base, C = a 24-byte spatial cell. Those are distinct objects;
equal slot numbers across their interfaces do not identify the same operation.

| Caller / receiver origin | Concrete target and arguments | Effects / further calls / draw qualification |
|---|---|---|
| Helper 0x10092ef6/0x10092f2d, I+0x1c = G, G +0x6c | Game 0x100037f0, supplied selected type id | returns G+0x4098+4*id pointer; direct field read, no draw |
| Catalogue creation/insertion, G base | Game 0x1000221f–0x10002265 uses +4(0xd00), +0x24 and descriptor +0x3c | actual factory 0x10006400–0x10006424 ->0x10011dd0 ->base 0x10011190; final table 0x10021e8c. 0x10003287–0x1000329a inserts that descriptor in the same array; not a route-name inference |
| Helper 0x10092f37, W base +0xa0 | Welt 0x100a34d0 | returns W+0x114 manager; no draw |
| Helper 0x10092f4d, W base +0x74(4) | Welt 0x100a3380 dispatch ->0x100a33c5 | reads W+0x10c; supplied creation time, no draw |
| Manager 0x1009727a/0x100972a8, this=M+4 | Welt 0x10099130 /0x10099290 | circular empty-slot scan, reserved-id retry, cursor/scan writes and possible vector growth; normal game callbacks resolved below; allocator failure boundary separate |
| Manager 0x10097303, M base +0x60(Df) | Welt 0x1009a110, kind from Df+0x50 ->Game 0x1000fad0 | original kind 49/51 both select 0x1009a4a1 via byte/target tables; kind 50 selects another constructor and is excluded |
| Factory 0x1009a4a6/0x1009a4c7 | Welt allocator 0x100b5f80; New ctor 0x10052c10 | successful 0xbc allocation, base ctor 0x10035ce0 and cargo object F+0x6c/table 0x100c3978; no action start; allocator retry not certified |
| Manager 0x10097323, F base +0x28(Df) | Welt 0x10052a30; Df table 0x10021e8c | Game +0x14 ->0x10010a40 class 0xd00; +0x90 ->0x1000fce0 HP; +0x40 ->0x10010a50 id. Cargo +0x0c ->0x1002ff50 may add 0x600. All finite field reads/addition, no draw |
| Manager 0x1009732e, F base +0x28(W) | New 0x10052a30 ->Boden 0x10035c10 | W class +0x14 ->0x100a37c0 gives 0xf00, stores F+0x18. Both handlers query class; no type/HP repeat or draw |
| Manager 0x1009736d, this=M+4 | Welt 0x10099020 ->0x10099870 as needed | grows/zeroes 16-byte registry rows, copies existing rows via 0x10099c50; spatial backlink callback below. No direct world draw; normal-success closure excludes allocator retry/hooks |
| Manager 0x100973a4/0x100973c1, F base | +0x3c ->0x10035bf0(id), +0x4c ->0x10052aa0(0x23, property buffer) ->0x10035ca0 | stores low u16 id F+4; low u16 time to F+8/+0xa/+0xc; no draw |
| Manager 0x100973e1, F base +0x50(position) | Welt 0x100348b0 | copies supplied three binary32 words to F+0x24 and F+0x58; does not interpolate or start movement |
| Manager 0x100973f6/0x10097412 | Welt 0x100b5f90 after divide by constant 1.0 at 0x100cafac | x87 conversion truncates toward zero; signed integer division by 16 locates cell. No new cross-host FP policy; supplied valid/in-bounds outcomes required |
| Manager 0x1009749a, this=C+8, slot +0x0c | Welt 0x10096f40, pointer to registry-row pointer | append row to cell list. Actual cell construction 0x10099473–0x10099486 ->0x10099850 ->0x10097060 installs table 0x100cb09c at C+8; no draw/callee |
| Vector copy 0x10099c96, receiver old row+8 = C+8, slot +0x10 | Welt 0x10096fb0(old row,new row) | repairs previous/next links and endpoint row pointers; no draw/callee. Fresh zero rows skip it; existing linked rows do not |
| Helper 0x10092fe2, F base +0x4c(0x2f, rectangle) | Welt 0x10052aa0 arm 0x10052abc–0x10052ad6 | low u16 rectangle anchor x/z to home F+0x68/+0x6a and current tile F+0x64/+0x66; no draw |
| Helper 0x10092fed, F base +0x84(0), then returned cargo base slot 0 | Welt 0x1003b6d0 returns F+0x6c; 0x1002ff40(0x45) | writes cargo goods byte at F+0x70; does not replace quantity at F+0x72 or read descriptor capacity |
| Helper 0x10093002, F base +0x28(I) | New ->Boden 0x10035c10; I+0x14 ->0x1008c670 class 0x1300 | stores I at F+0x1c; no island enumeration/action start/draw |
| Sweep 0x10093837/+0x78 and 0x10093845/type +0x48(0x20) | Welt 0x100a3cb0 ->Df; Game 0x10011cc0 ->0x10010ae0, dispatch arm 0x10010ba8 | copies unsigned Df+0x100 goods to output+4, returns 1; sweep does not use output. Finite pure path, not an omitted arbitrary query |
| Sweep 0x1009384c, 0x10093864/0x1009386b | figure +0x40 ->0x10035be0; +0x78 ->Df, +0x80 ->Game 0x1000fc90 | zero-extended figure u16 id and descriptor byte goods; success bookkeeping below; no draw |

D/reused primary source chain: G's concrete table 0x100215a4 comes from the
Frame/Game/Scene/world construction already closed in
[resource initialization §1](wildlife-resource-initialization.md#1-finite-original-receiver-and-selector).
Welt 0x1009d848 stores G at W+0x110, getter 0x100a37d0 returns it, island
0x1008c0c9–0x1008c0db stores it at I+0x1c. These are base-interface pointers.
Game 0x1000384f–0x1000385c zeros the figure pointer array at construction;
original array extent is a format/runtime fact, not an engine content cap.
The selected actual factory yields table 0x10021e8c, not a guessed choice among
all three figure tables. The base table is transient during construction.

D: M's concrete table 0x100ccd74 is installed at 0x100aa9da after constructor
0x10098c80; initialization 0x10097150 stores W at M+0x30 and world initialization
publishes M at W+0x114 (0x100aaa15). The list interface used at 0x1009749a is
C+8, not the cell or figure base. Registry scan/growth uses M+4, not M base.
These adjustments are material to interpreting every state write above.

## 2. Defined successful effects in actual order

D: manager 0x10097269 initializes the caller's result pointer null, then searches
an id when passed -1. 0x10099130 starts after its last cursor, wraps by current
row count and tests row+0xc. 0x10099290 continues after a reserved candidate;
M+0x80 bits are checked before the factory. Growth is approximately n+n/4,
minimum 100, with a progress fallback; it is an original allocation policy, not
a species/world cap. Constructor cursor -1 and empty row 0 permit first id 0
(0x10098d06, 0x1009916e–0x1009917e); do not invent an id-1 start convention.

D: kind 49/51 dispatch (0x1009a13e–0x1009a155) chooses New, constructor
0x10052c10. Base flags become 8 and action byte 0x13. New initializes requested
27/current 0/behavior 4/config 4, radius 7, target -1, speed factors 1 and budget
25000. Constructor HP 1 is temporary. Cargo starts goods 0/quantity 0 and its
+0x0c adds 0x200 at 0x10052cc2. Type attachment then replaces HP with unsigned
Df+0xea, and Df id 128,129,529,1206 adds 0x600 at 0x10052a93: final 0x800,
otherwise 0x200. These comparisons are code behavior, not an exhaustive species
roster or capacity. Type-capacity +0x84 is not called here. Descriptor HP 0 does
not invoke death or cleanup on this path; later tick/death remains outside scope.

D: manager attaches Df then W before row publication. It ensures registry size,
zeroes three row dwords and stores F at row+0xc, updates cursor/live count, assigns
F's u16 id, increments M+0x24, and sets time property (0x10097372–0x100973c1).
The supplied full time is projected to three u16 words. Manager publishes F to
its caller at 0x100973d4 before optional position/spatial registration finishes.
Position copy preserves bit patterns, not a new fixed-point conversion. Registry
row spatial pointer is filled only at 0x1009749d. No creation callback starts a
route, tick, requested-state transition or first action.

D: helper success 0x10092f71–0x10093005 constructs a one-tile home rectangle by
truncating x/z after divide by 1.0 (0x100ca6f8), then sends property 0x2f.
For ordinary terrain this uses the supplied creation vector; for cave kind 0x145
it first replaces x/z in its local scratch vector with integer target-tile floats
(0x10092f84–0x10092f90), AFTER the manager has copied the entrance position.
Thus cave initial position and saved home can differ. Height remains the supplied
manager position; saving home does not re-copy the manager position. No fractional
upstream positioning, cave entrance producer or cross-host x87 equivalence is
newly proved. Cave without an entrance stays refused under OQ-574.
Then cargo goods becomes 0x45 (69), followed by island attachment. This carried
goods byte is distinct from the animal descriptor's yield goods (e.g. 137).

D: after nonnull helper return, 0x10093837–0x10093871 runs the pure type-property
0x20 query, gets figure id, stores its low word to spawn+0xc, stores
low8(sweep_time/1000+60) to spawn+9, ORs 0x40 into spawn+8 and stores unsigned
descriptor goods at spawn+0xe. This /1000 is the sweep clock, not loader /500.
At 0x10093875 traversal resumes; list order/chunk cursor remain the established
sweep/prefix contract. No whole load/scheduler or world draw-history is inferred.

D: Game 0x1000ff6d–0x10010016 matches BEWOHNTYPE names and stores the
resolved kind at descriptor+8. LOGIC presence zero-fills 44 bytes at
0x100101b9–0x100101cf; its HP word is stored at +0xea at 0x10010387.
Constructor default HP 5 (0x10011249) is distinct from a present LOGIC zero.

P: the bounded source parse of FigurenHeader.dat (368 entries) plus the addressed
BEWOHNTYPE name mapping finds 29 kind-49 descriptors, zero kind-51 descriptors;
three selected entries meet the special-id branch. This is this file's inventory,
not a supported roster cap. Route HIRSCH does not establish kind. C: current
FigureType::bewohntype mapping in types.cpp covers another bounded use and does
not decode these animal names; do not assume its value 0 establishes kind 0.
Existing UnitImportFacts is not a substitute for the concrete original descriptor
(default/record-presence semantics must be preserved by a future source binder).

## 3. Draw and failure boundaries

| Stage | Additional world draws proved | Failure/effect boundary |
|---|---|---|
| Helper before M slot 0 | ordinary position then species =2; cave species =1 | already established helper; no-entrance cave unsupported |
| Normal successful kind49/51 factory/type/world/id/time/position/list paths above | 0 transitive game draws under the exact normal allocation/receiver profile | actual callbacks resolved; no animation +0x64 or first action reached |
| Helper home/cargo/island and sweep tail | 0 on the concrete getters above | conversion and input provenance qualifications retained |
| Manager returns negative/null | no new helper draw; prior 2/1 not undone | caller result pointer stays null; sweep jumps to 0x10093875, no id/time/occupied/goods success stores |
| Heap failure/retry/unknown handler | U, not zero | precise allocator callback below; no rollback or full failure-path draw claim |
| Later first action | outside this boundary | ordinary first walk's six direct draws remain later; duration/search closure OQ-578 stays open |

D: no free id or null class returns 0x80004005 at 0x100972de/0x1009730c.
Id scanning can already change cursor/scan state or grow storage before null
factory, so a null animal does not prove manager rollback. Callback returns from
type/world attachment are not checked by manager; original malformed receivers
are not converted into an invented atomic failure. Spatial conversion lacks a
new checked out-of-bounds result here; require valid in-bounds resolved positions.
On null helper return, existing spawn fields survive (0x1009382f–0x10093831),
including an old or uninitialized saved-id word: do not fabricate a cleared id.

D/U: allocator 0x100b5f80 ->0x100b72c3 normally returns successful allocation.
After failed allocation, 0x100b72de–0x100b72e7 can call 0x100bafb4, which loads
an arbitrary handler pointer at 0x100e3cc4 and calls it at 0x100bafc1. Its actual
runtime value/registration is not closed. The PE imports identify HeapAlloc at 0x100c1048, HeapFree at 0x100c1068,
EnterCriticalSection/LeaveCriticalSection at 0x100c1074/0x100c1078 (KERNEL32).
CRT heap/free/lock internals and external OS allocation outcomes are not a simulated original observation. The bounded
zero-game-draw claim excludes retry/handler/hooked allocator execution. A complete
out-of-memory closure would next resolve that exact handler registration/value;
it is not a prerequisite for a pure successful creation-effect kernel with
explicit supplied allocation outcome. No new allocation-failure adaptation is
authorized by this research.

D: type/animation +0x64 is NOT reached between this manager entry and sweep
return. Its differing signature problem belongs to later action-duration calls,
not an invented construction callback. OQ-578 is narrowed only for this selected
normal-success profile; search/storage/death/animation/predator/Old paths remain U.

## 4. Private checker, prospective implementation and stopping boundary

C/H: eleven hand-derived private rows cover eight successful/null effect cases
and three explicit refusals. A legal checker run executes all eleven. Six actual
checker-source counterfactuals (constructor-as-capacity, lost extra addition,
wrong carried goods, entrance-as-home, null occupation, early action) each fail
an intended row after the legal run. Wrong expected output and zero rows return
nonzero. This checks the transcription, not the original binary or unresolved
callee behavior. Supplied manager outcome/home projection is never counted as
execution evidence for allocator/FP code.

The initial modeled rows supplied home 10/20 for ordinary binary32 x=1.25/z=3.75;
that was not a physically consistent ordinary-helper input. The checker accepts
resolved outcomes and therefore did not detect it. Initial inputs and successful
model log are preserved, not relabeled as primary proof. Corrected authored
ordinary anchor is 1/3; the cave control explicitly supplies tile anchor 5/6.
The large hypothetical id was also replaced by valid original id 65535, leaving
widened-engine id policy to later coordination. No engine expectation or pin moved.

Proposed next isolated kernel: owned, validated descriptor-kind/id/HP/yield facts,
explicit supplied manager id/time/position words and resolved supported home
projection, plus prior spawn fields and sweep time. Return owned defined creation
fields, ordered attachment/effect trace and successful sweep-field projection;
errors distinguish unsupported concrete profile, invalid position provenance and
supplied failed allocation. Define no garbage/padding bytes. Keep helper species
and supplied draw cursor separate, reuse existing decision functions unchanged.
A source binder must establish original BEWOHNTYPE and LOGIC/WARE presence and
selected normal descriptor profile; current restricted modern bewohntype is
insufficient. This can be built/tested UNWIRED with no semantic identity allocation.

A later persistent manager/World implementation needs explicit figure registration,
spatial indexing, save fields, allocation-failure policy, actual global scheduling
and PRNG order. Those are Windows/orchestrator tag/state/PRNG decisions, not this
packet's authority. No numeric tag, state number, default, fixture or production
code changed. Later animation duration, first tick, motion, hunting and cleanup
are not implemented. Original/HE runtime and cross-host FP remain NOT_RUN.
