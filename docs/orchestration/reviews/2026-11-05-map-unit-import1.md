# MAP-UNIT-IMPORT-1 and the original SCRIPT2 F2 canary

Status: REVIEW PENDING — final integration disposition of this tracked bundle.
The implementation, package reviews, affected gates and corrected publication are complete.

The original missing-ship dependency is resolved for unchanged endless01. Normal
scenario loading imports its full five-actor closure before normal session INIT.
The original ship naturally reaches a true area condition, emits the expected
wave, returns, founds, unloads and builds HANDWERK1001 through public commands.
Independent A/B/C, bounded D/F2 and E reviews are approved. Final G integration review of this bundle remains pending; all required gate terminals are recorded below.

## Scope and preserved work

Owned checkout: /Users/dom/Dev/anno1503-worktrees/anno1503-worktrees/anno1503-MAP-BOOTSTRAP-1.
Branch task/MAP-UNIT-IMPORT-1 starts at preserved SCRIPT2 head
066ef4bb363f3fbcdd7615dee290ac8db5cc5a38; task/MAP-SCRIPT-1 remains there.
No main merge, push, TASKS overwrite, original asset change or other-executor edit.
Reviewed money/wave/startup/persistence handlers remain unchanged by UNIT1.

## Imported facts and validation

Primary notes: docs/re/original-unit-import.md; contract/chronology and acceptance
ledger: docs/sim/original-unit-import.md. All 50 original files contain 3,473 actors,
2,644 top-level and 829 nested; independent raw parsing verifies hashes/counts and
zero duplicate raw ID per scene. This inventory is not full-scenario support.

The complete endless01 actor tree is:

| Original ID / registry slot | Engine FigureId | Type | Owner | Parent FigureId | Crew slot |
|---:|---:|---:|---:|---:|---:|
| 0 | 1 | 1503 (ship) | 0 | none | none |
| 1 | 2 | 717 (scout) | 0 | 1 | 0 |
| 2 | 3 | 718 (donkey) | 0 | 2 | none |
| 3 | 4 | 717 (scout) | 0 | 1 | 1 |
| 4 | 5 | 718 (donkey) | 0 | 4 | none |

Enclosing Human player 0 owns all five. MAIN creates 100 registry slots; the final
hint is 4 and cursor is 0. The next serial engine FigureId is 6. Nonmonotone traversal/holes and requested-slot growth are
preserved; configurable budgets refuse required growth before allocation.

The ship starts at (351.5, 383.5), facing 4, with 12,000 navigation HP and state
0x8000. Held scouts/donkeys have state 0x88 and HP 350/400 respectively. Held-ESEL cached position is an explicit modern parent-cache
normalization because original own xyz is uninitialized; parent exclusion remains
authoritative. Ship fine Q32 and derived Q8 are checked using integer conversions,
with ties-even, signedzero, nonfinite and bounds controls. Cargo order/repeats are
preserved: goods41,41,26,42,42,41,43,26 each 12,800 raw units, totaling 400 tons. No loadout grant.

Two primary corrections were reopened, fixed and re-reviewed: class97 scouts do
not satisfy the ship8000 crew fallback; abstract other-crew capacity is LOGIC26,
not cannon-capacity LOGIC15. Original1503 counts8/3/6 and717 othercrew0 are verified
through the real TypeRegistry converter. Hand controls and retained wrong
interpretations distinguish each claim. All mutant source changes are restored.

Normal import supports the reviewed role graph SCHIFF/class113 with navigation,
SOLDAT or heldCREW/class97 without navigation, heldESEL/class53. Unsupported actors,
tasks/routes or required state refuse with a path. Known unprojected HEAD/cache/
name/trade details emit explicit warnings; avoidance stance remains deferred.
This is not combat, disembark, land movement or complete post-load/runtime parity.

## Public original-world proof

FileSHA b289be7fe906264b1d0fe0b2be75c0e68813ed6691128c84cfebb6e4fd4cc8c0.
Whole2488byte programSHA cac0aeb9c14dfd5c0f9b18c664e5282dff57096e2938f93c67391e3c3f8a47d0.
Normal INIT changes0 → 462 permissions, stage 1 → 0x6ffffeff; gold 10,000,000, 38 researched
entries and RNG cursor 1 remain; INIT work 463. Mapping slots 0–6 and 31 remain exact.

Starting prerequisites were proved before voyages: actual route to224,672 inside
NEGER [218,285)×[671,736), and affordable neutral-coast founding click344,405 with
watergoal350,405. Planning relocations exist only on disposable copies; executed
world starts unchanged and moves solely through public MoveCmd.

At speed 100, advances of 1,000 ms (500 ticks) take 98 calls to NEGER and 93 back. One branch0
one-shot emits code317 HAWAIMUCKE.WAV atold tick 48,000, source offset 1,068, audience AllLocal,
invalidplayer. The oracle correlates the actual pre-visit ship point, signal,
orderID/arrival endpoint and source. Original signal state becomes
[0,0,1,1,1,1,1,1]. No original soldier-money branch executes.

FoundCmd type 1302, origin (343,404), click (344,405), creates the warehouse in
island 2, city 0. Exact payment:
350 gold, 1,280 raw units of tools from hold 0 and 3,072 raw units of wood from
hold 3; all other holds remain unchanged.
Public UnloadCmd transfers only warehouse free room; legitimate StockFull refusals
leave saved state unchanged and remaining cargo stays aboard. The new city starts with zero stock.
Independent hand accounting at the existing 50-ton capacity per good gives unloads
45, 5, 50, 38, 12, 0, 50, 0 tons by hold. Final ship quantities are0, 45, 0, 0, 38, 50, 0, 50 tons
(183 tons), with holds 5 and 7 refused as StockFull. After HANDWERK, citystock is 47 tons of tools,
46 tons of wood, 50 tons of goods 26 and 50 tons of goods 43; all other goods are zero. The public-command test checks
exact transfer/remaining-stock arithmetic and conservation without giving stock.
Ordinary BuildCmd type 1001 creates building ID 8,589,934,594 at (318,401), in that
city, charging 300 gold and the exact stock costs. Gold follows
10,000,000 → 9,999,650 → 9,999,350. The separate denied-copy
fixture changes only type 1001’s permission to zero and refuses the same otherwise legal build;
the original canary retains its actual INIT rows.

Native cuts cross Pending/INIT, immediately before/after the true condition,
founding, unloading, construction and next visit. Segment replay checks checkpoints,
exact final save and complete wave sequence. Fresh producer and replay worker
consume encoded normal/alternate-build/changed-program logs, check exits/writes and
all artifact identities. Legal alternate construction diverges atcheckpoint 408;
a changed program in valid native framing gives the intended script-content-digest
refusal. Original final state hash: 3022869862516125275. Actual audio is not implemented.
Synthetic whole-source branches 0, 5 and 6 still separately prove soldier queries and
both MONEY_ADD credits 10,000 → 109,999 → 209,998; they are not original-world coverage.

## Integration and compatibility

load_scenario and sim_load file/context/link loaders accept optional UnitImportOptions,
default ImportRequired. ActorFreeComponent must be requested before loading by an
explicit component/generated-base caller; a failed normal import never falls back.
Generated fixed-map/ship-start manifests keep their established base-actor exclusion
and unchanged loadouts. Historical geometry/population/transport fixtures now
state that scope explicitly; all prior goldens/SCRIPT1 archives are preserved.

FigureSpec adds source registry_slot/generic hp/fine nav_position_q32 and the spec
can carry MAIN initial slots; insertion projects into existing FigureRegistry fields.
Import-only immutable type facts are separate from FigureType/live registry digest,
read only before World creation. Native130, saveframe1, replay3, commandwire1,
SCRIPT2policy2 and PRNGtimeline5 remain unchanged. Saved authoritative actor state
includes every projected effect; no runtime consumer depends on the import catalog.
Normal sessions must use Recorder::start or the established unrecorded startup
boundary before commands; restored Ready sessions do not rerun INIT.

Strict corpus reconciliation accepts 19 files containing 119 actors and explicitly
refuses 31 inputs. All 50 programs parse, 14 compile and 11 start normally; two compile-supported scenarios refuse
nested donkey TASK state, endless02 refuses actor373. Empty kernels stay labelled
empty. Current schema2 publication joins same-source/executable fresh manifests,
full scenario hash plus program hash and exact artifact/child outcomes. Original
true-branch coverage[0] is separate from synthetic[0,5,6]; fullscenario/campaign
completion is not claimed. Pre-UNIT SCRIPT2 publication is preserved historically.

The unchanged F5 indexed-query Release measurement is reused at its actual pin
69380a73 / cache/script2/F-scale-release-01.log (1/1), as documented in the earlier
SCRIPT2 handoff. It varies 8/64/128 players, 4/32/128 areas, 16/128/512 statements
and 100/10,000/50,000 unrelated figures, reporting build/query work and allocation
requests separately. UNIT1 changes cold actor import; indexed construction/query
code is unchanged, and current Debug/ASAN scale controls pass. This reuse is
architectural one-host evidence, not a new performance or late-game measurement.

## Evidence and review

A/B/C reviewed atd5e98d2f (15/15 cases, 841 assertions; later corpus proof: 340 assertions).
D/F2 reviewed at4e0ff63e by independent contract and state reviewers: canonical
D-fresh-04: 1/1 wrapper, 3,988 producer assertions and 45 worker assertions,
zero other outcomes, 156.39 seconds.
E code/control approved1988471a after full-scenario hash join fix,11 cheap contracts,
and actual ship x 351.5 → 352.5 same-program negative. Reader correction/republication
approved f437ac73 after12 contracts and the4/4 saved-data probe. Fresh integration reviewer is
/root/unit_import_final_integration; final disposition pending.

Current affected gates at source 1988471a: Debug **256 selected / 256 passes**,
zero other outcomes, 165.16 seconds; ASan/UBSan **256 selected / 256 passes**, zero
other outcomes and zero sanitizer reports, 924.48 seconds. Original fresh-process
ASAN takes 581.97 seconds; its four encoded artifacts exactly match Debug.

The corrected schema2 publication is independently approved at reader revision
f437ac732e8855ccc7ebd6e0fc65974ba8218770. Its tested engine provenance remains
1988471a396185fc8d7652ffb449b902079091fc; publisher script SHA256 is
8d6cff7d48237678942d32307bb1ece37d5ae931822b3944a3239e2447cf75a9.
Two refused-empty rows now correctly read NOT_EXECUTED. The old report and its
two intended regression failures remain retained; the same probe now passes4/4.
Canonical reader gate:1/1 main entry,12 contracts,zero other outcomes. No engine
input or behavior changed, so valid runtime evidence was reprocessed. Its original
manifest is cache/unit-import1/replays/fresh-zo6ompai/manifest.json; ASAN uses
fresh-w_y7fueb. The full identity maps are G-debug-manifests.json and
G-asan-manifests.json. Debug executable SHA256:
894a2abc25ecc28e7ef6b365ad5ce7ceebd2abb5fb565a6759eb50c47e6af76c; ASAN:
3db789398e119eac5b0f42a44da66a873cc8c03f7eb40c2859387624382affd6.

The isolated prior opening-loop case now passes **1 selected / 1 pass**,
76,166/76,166 assertions, zero other outcomes/sanitizer reports, 2,247.82 seconds
(gate 2,247.85), under the declared 3,600-second timeout, jobs1/admissionON.
Raw gate: cache/unit-import1/G-asan-opening-01.log at clean engine source198.
The bound was declared before running: about5× its723.26s Debug baseline and2×
the inadequate prior1,800s limit. The case was allowed to finish; no jobs stopped.

| Gate | Selected | Executed passes | Failed / empty / skipped / incomplete / sanitizer lines |
|---|---:|---:|---|
| Current affected Debug |256|256|0 /0 /0 /0 /0|
| Current affected ASAN/UBSAN |256|256|0 /0 /0 /0 /0|
| Isolated ASAN opening |1|1|0 /0 /0 /0 /0|
| Corrected reader contract |1|1|0 /0 /0 /0 /0|

Required contracts are separate. These rows are separate gates, not an aggregate
unique-test count. Historical592/current256 name overlap is121; current-only135
are selection differences, mostly existing cases. Old actorless capability and
dependency results are superseded, not reused as unchanged proof. Only unchanged
implementation/input scopes reuse older results, supported by current50creation
identities/goldens and absence of runtime/old insertion/native changes.

Final fresh integration disposition of this tracked bundle: REVIEW PENDING. Current G/reader workflow is04f578f6889fc910304db6cfce3a5165a5c41596.
Use the canonical runner with explicit owned paths, original assets,
ANNO_UPDATE_GOLDEN=0,admissionON. Required contracts are separate from main counts.
Legacy595/595Debug before storagefix,85/85+7/7afterward, and ASANFAIL591passes plus
one1800s timeout remain attributed to their original sources. They are not relabelled.

Retained first failures include wrong crew-flag oracle, zero-width signedzero input,
wrong raw metadata field, bad initial area target, test width/export compilation,
naive all-holds-unload assumption, incorrect cursor1 after full7visit wrap, and
program-only publication join. Corrections preserve their raw logs and reviewed
negative controls. No tolerance or original resource was changed to turn a test green.

Windows/MSVC, Linux compiler/sanitizer checks, original-runtime capture, GPU,
audible playback and multiplayer/late-game scale are NOT_RUN here. macOS is a
headless development host. Stop after the reviewed local handoff; integration and
any further actor/gameplay contract belong to the orchestrator.
