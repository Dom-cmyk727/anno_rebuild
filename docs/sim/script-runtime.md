# MAP-SCRIPT-1 — contract and package acceptance ledger

## S0 baseline and ownership

Base `4102196acee07facafabebab8bf5edd61a56893b` was clean on
`task/MAP-BOOTSTRAP-1`. Work continues in the same owned checkout on new local
branch `task/MAP-SCRIPT-1`, preserving that reviewed prerequisite branch.
Checkout: `/Users/dom/Dev/anno1503-worktrees/anno1503-worktrees/anno1503-MAP-BOOTSTRAP-1`.
Fetched origin/main and canonical workflow pin: `20bd97fd68f2b5fbba582e49d71cadb855346e67`.
Published starting identities: state128, save frame1, command wire1, PRNG timeline5.
No main merge, other executor changes, push, rebase, map/resource/diplomacy change,
or default House flip is authorized. The new packet supersedes the earlier
B3 block only after independent S1 approval.

Host snapshot 2026-11-05: arm64 Mac,18 logical CPUs,51,539,607,552 bytes RAM,
1.5 TiB free disk;699460 free16-KiB pages,371331 wired, no throttled pages.
Two first-executor test processes were actively using roughly six cores. Leave
them and keep-awake processes45146/48465 alone. Research and cheap contracts
first; no overlapping expensive gates. Existing calibration and admission remain
in effect; j4 maximum for this lane's initial builds, expensive gates sequential.
Use canonical main run_selected.py with explicit owned source/build directories.
Host permission is required for its local coordinator socket; never disable
admission or terminate another lane's job.

Primary module SHA256s and exact addresses are in re/script-runtime.md.
The accepted availability query, parser APIs/tests/reference, existing timers,
World creation/advance, session drivers, player/research state, codec and state
field lists are the integration baseline. Program compilation belongs in sim;
formats retains lossless syntax/grouping/read-on. Immutable compiled data is
World-owned, derived once from persisted source/policy; mutable execution state
and player permission rows are explicit state, not renderer/UI state.

## Acceptance ledger (written before production implementation)

| Requirement | Primary evidence | Positive control | Distinguishing negative | Test/artifact | Review |
|---|---|---|---|---|---|
| S0 isolation/identities | git/host snapshot, task packet | exact clean4102196a | unexpected edits preserved | this ledger/cache/script1/S0 | baseline verified / S1 reviewers |
| S1 actual scheduling | Scene16b70; Frame5b161/fef6/ff7e | count4,tick32 visits1 | delta/Frameinterval or stamp-update reading | scheduler vectors/primary review | APPROVED5081f780 /1bb960b2 |
| S1 grouping/branch | Scene22bf0/10290 | right-associated guard | C precedence; falseAND runs ELSE | interpreter reference trace | APPROVED5081f780 /1bb960b2 |
| S1 restore/init | Scene16fb6/1706a/1708c/16360 | signals overwrite SET; INIT once | restore afterINIT; repeatedINIT | init/restored vectors | APPROVED5081f780 /1bb960b2 |
| S1 permission effects | Welt1bca0; Scene15fa0/15fe0 | raw1 grant; individual2229 | raw257 unlock; ALL alltypes | permission reference/data | APPROVED5081f780 /1bb960b2 |
| S2 compiled execution | parser/grouped/read_on + S1 | bool assigns/IF/timer/query | extractcalls from unsupported kernel | compiler/evaluator tests | APPROVED8d9d7197; quoted-id correction86abd3e1 |
| S2 bounds/cursor | originalroundrobin + explicit safety | saved incomplete round resumes | restartIF0; wall-time budget; partialINIT | reference/runtime tests | APPROVED8d9d7197; quoted-id correction86abd3e1 |
| S3 permissions/import | player1d240/ANLTRG branch | absence vszero, rawbytes | replayimportasactions; wrongowner | import/action tests | APPROVED0b8f5bfe; integrated86abd3e1 |
| S3 data-definedALL | HEADfolk/exclusion fields | original461types + customcontent | flags_word filter; hardcoded idlist | original/custom profile tests | APPROVED0b8f5bfe; integrated86abd3e1 |
| S4 native identity | explicit stream/schema/program policy | exact newbytes; reload | old128/checksum-first false refusal | independent stream vectors | APPROVED86abd3e1 / review_integration |
| S4 production boundary | proven script-beforeAdvance | recorder/unrecorded/replay same | canary-only runner; interval batching | publicsession tests | APPROVED86abd3e1 / review_integration |
| S5 authoritative build | shared check_record | scriptallow -> exactdebit | stalequery; deny ignored; dryrun bypass | atomic/live negative controls | APPROVED86abd3e1 / permission_semantics_review |
| S5 path separation | create/found/house callers | later building import/founding | restrict every create_building | caller regressions | APPROVED86abd3e1; generated adapter APPROVED790921b7 |
| S6 real vs synthetic | all50 AST inventory | executed supported complete program | fragment claimed as fullscenario | capability inventory/canaries | bounded proof APPROVEDdb13e91e / review_canaries; full original BLOCKED |
| S6 scale | deterministic counters/Release | moreplayers/types/statements | query city/figure rescan | modest benchmark | APPROVEDdb13e91e; Release9da32085 |
| S7 evidence | canonical runner + independent reviews | nonempty frozen-source gates | skipped/incomplete relabelled pass | retained raw logs/manifests | APPROVED0001a020 / final_integration_review; docs reviewed12569eaf; full original claim BLOCKED |

| SCRIPT2 A closure | entire original AST + hashed bytes | seven IFs/all instructions inventoried | permission fragment substituted | cache/script2/A/endless01-inventory.json | pending A review |
| SCRIPT2 B area | Scene232a0/11010; Welt985c0/a5f70/97150 | true/false, every edge, exact owner/class/crew | swapped tuples/closed edge/ID0 wildcard/hull/nav shortcut | script2 handler vectors + focused query gate | pending |
| SCRIPT2 B bounded view | original spatial query + owner-approved cap removal |513th match, index rebuild/large unrelated set |512 truncation or hidden scan per IF | deterministic build/query work counters | owner cap choice approved; code pending |
| SCRIPT2 C money | Scene15940; Welt1cfd0; approved F15/F28/F35 | exact signed deltas/mixed staging/ledger | wrong recipient/duplicate credit/partial commit | handler vectors + money gate/mutant | primary reviewed; frozen A pending |
| SCRIPT2 D wave | Scene156f0; Frame61010/62090/62380 | exact broadcast namespaced output/order | silent/wrong event, output erased, rollback leak | typed-event consumer and artifact contracts | primary reviewed; frozen A pending |
| SCRIPT2 E startup/identity | normal Recorder/driver callers + explicit native schema | first command after INIT; Startup replay; native Ready | constructor eager/second world zero visit/old schema accepted | native literal/reference/corruption and replay gates | pending |
| SCRIPT2 F original canary | unchanged file/program hashes and actual resources | natural public-command area condition + real build | teleport/grant/extracted source/compiled-only claim | exact branch/money/event ledger, fresh process | pending |
| SCRIPT2 F scale/capabilities | complete50 AST inventory + Release counters | build AND query work, branch labels | empty kernel called tutorial/hidden unlimited query | capability snapshot/measurement provenance | pending |
| SCRIPT2 G acceptance | canonical immutable gates and fresh review | exact terminal counts/clean commits | failed/skipped outcomes promoted | final handoff and retained failures | pending |

## S1 approved execution contract

Evidence-backed original behavior is distinguished below from explicit safety or
continuity adaptations. Construction wiring remains blocked until this contract
is independently approved.

1. Compile the existing complete AST, preserving right grouping and read-on
   losses. Unknown actions/queries anywhere in a program are an Unsupported
   compile result before replacing a playable world; never execute a permission
   extraction and call it complete original scenario support. Unknown sections
   and unsupported object/area features are explicit dependencies. Empty original
   kernels establish empty-kernel execution only, not tutorial coded behavior.
2. The first feature set is typed Boolean SET/assign/signals, variable conditions,
   numeric TIMER_i and PLAYTIME comparisons, timer actions and individual/ALL
   construction actions. Numbers fit signed32 unless a documented original
   unsigned timer calculation applies. No new arithmetic language is introduced.
   Bind names/mapping indices/type ids once; retain instruction/source offsets.
3. SET defaults and implicit SCHUETTIINITSIG=true exist before saved SIGNALS are
   restored. Restoration is by declared symbol, not creation of unknown names.
   Original text import starts its IF cursor at the first IF. Native reload
   preserves the exact cursor/start state (user-required continuity adaptation).
4. Context meanings: Missing (not supplied), ExplicitNone (known script-free),
   Pending (supported compiled program, not initialized), Ready, Unsupported
   (load/compile diagnostic, no playable replacement), Fault. Missing, Pending,
   Unsupported and Fault never authorize a player build. ScenarioSpec defaults
   Missing; synthetic/ship loaders must explicitly declare their known-free
   contract, while original scenario import supplies its whole script.
5. A Pending supported program runs its entire INIT transaction on the explicit
   fresh-session World::initialize_scripts startup visit, or the first normal
   World::advance visit, before clock/world advancement. While Pending,
   public construction refuses with a context reason. No partial initialization
   is exposed: variable/timer/permission effects commit together or fault without
   their partial effects. SCHUETTI controls original INIT suppression; after the
   first visit the start/cursor state prevents repetition. This readiness barrier
   and rollback are explicit safety adaptations; original handlers can partially
   mutate before failure. No eager constructor INIT is introduced.
6. Each World::advance visits scripts with the OLD absolute world tick low32,
   then advances simulation. Existing run_turn/run_frame commands stay before
   these advances. Effects in a script visit are immediately visible to later
   statements/queries in that visit; later player commands observe committed
   permissions. World::advance(0) is explicitly supported as a visit without
   advancing time; this is a rebuild API adaptation, not a claim every original
   Frame path called scripts at zero elapsed time.
7. Original q: signed32 quotient truncating toward zero of
   `wrap32(wrap32((arg-origin)*IFcount)+125) /250`. A negative quotient or one
   above IFcount yields IFcount. Origin is0 in the supported original object;
   it is not updated per visit. Visit q IFs from the persistent round-robin
   cursor, wrapping. No fixed-step/delta scheduling or interval-partition
   equivalence is claimed. Native continuation persists cursor and call argument.
8. Conditions evaluate left once, then right unless false AND. OR evaluates
   both. Only a root false-left AND suppresses both THEN and ELSE; other false
   results select ELSE. Inner nodes have no action blocks. Pure typed queries
   cannot consume events in this slice. Assignments/actions preserve statement
   order and same-visit write/read visibility.
9. Deterministic work limits count evaluated nodes/statements/IF dispatches, not
   milliseconds. Configurable source/program/depth/work limits are validated
   against documented allocation/recursion safety bounds. INIT must fit its
   complete safety budget before gameplay; no chunked INIT. Exhaustion faults
   transactionally and stops advancement. Partial rounds due to q<count retain
   the cursor; there is no hidden mid-instruction continuation or silently
   deferred budget debt. Fault state/source offset persist and construction
   remains refused. Zero/minimal/large visits and exhaustion get explicit vectors.
10. Use existing ScriptTimers: timer reads unsignedcounter/500, SET stores
    wrapping seconds*500 without changing active, START/STOP change its flag;
    the existing World timer tick retains its ordering after script execution.
    PLAYTIME is signed32(callarg)/500, not a wall clock or a second timer system.

## S1 approved permission contract

- Per-player sorted sparse rows preserve override absence versus a stored byte0.
  Engine ids and container sizes are widened; original ANLTRG rows are u16 id,
  u8 value,pad (ignored on input, canonical original save pad0). Strict payload
  multiple-of4 and known-type validation are safety adaptations. Duplicate rows
  apply in record order, last wins; import writes state directly with no grants.
- Individual script actions compile only TRUE/FALSE to0/1. A reusable setter
  accepts rawu32, stores low8 bits, and grants only when the full input equals1.
  Thus raw257 stores1 without granting. Imported raw1 likewise grants nothing.
- For ordinary stages, raw1 directly ORs that stage bit into Player::flags;
  never use population player_flags_set's extra bit26 propagation. For stage31,
  mark type FORSCHTYPE researched, including id0; additionally mark output0's
  nonzero goods Forsch only if output0 is in the declared original research
  category46..55. Never grant output1 merely because the query accepts it.
  Research query alternatives and setter-grant facts are separate immutable data.
- ALL applies to one mapped player, ascending content type id, not every player
  or every type. Original filter is folk-mask bit and exclusion HEAD[0x2e]&3.
  HEAD[0x0c..0x0f] supplies folk mask; type+d0 is NOT flags_word. Runtime stores
  scalable folk membership/exclusion facts, derives the eligible type lists once
  at registry sealing, and hashes every behavior-relevant fact. Mods declare
  other type/folk ids without engine id branches. Individual actions ignore
  the bulk filter. The original4000-slot scan is not an engine type-count cap.
- Mapped names are case-insensitive and use the prefix before '@'. The original
  missing mapped slot falls back to player31; reuse cfg.neutral_player, not a
  hardcoded host fallback. If neither requested nor configured fallback exists,
  binding refuses. Unknown mapping names/types refuse with source diagnostics.
  Actual resolved bindings persist, so native reload cannot silently rebind.
- No transition deletes a stage/research grant when a later permission denies.
  Repeated actions follow order; an explicit zero still wins over a reached bit.
  Import/player loading finishes before first script INIT.

## S1 approved authoritative boundary and identity

Normal BuildCmd resolves issuer/city/type/rotation and existing structural/site
checks as today, including requested/effective rewrite revalidation. After the
final effective type/site is established and before optional research, serials,
affordability, draws or writes, check authoritative context and eligibility for
BOTH requested and effective types (requested first if different). This stronger
rewrite protection is a rebuild adaptation; no preview/caller eligibility flag
is trusted. A missing context is a typed refusal. Atomic dry-run and live use the
same check_record/helper; no script visit occurs between batch records.

Scenario/save building import, founding and automatic house transitions retain
their separate contracts. Never add permission enforcement to create_building.
Scripted creation is unsupported, not routed around the gate via a silent action.

New mutable fields require a NEW state identity, not an amendment to128. Plan:
state129 plus a fixed SCRIPT1 schema discriminator immediately after the state
version, before variable-layout config reads; program/policy semantic identity
is explicit and hashed. Independently numbered first-Mac streams are not claimed
compatible merely because their integer version matches. Command wire stays1;
no new PRNG draw is planned, so timeline5 remains unless evidence requires one.
Native saves include source/program identity, policy, initialization/fault state,
bool values, cursor/call argument, resolved mappings and player permission rows.
The immutable executable is rebuilt/validated once from these inputs on load.

## Capability baseline and release state

AST inventory cache/script1/S1/inventory-baseline.json:50 parsed programs,
37 nonempty kernels,6 with permission calls. No runtime exists at this checkpoint:
compiled/executed are false, not parser passes renamed as execution. The six
permission programs also use actions/queries outside the planned slice; those
specific missing handlers must be listed by the implemented compiler inventory.
If no complete shipped permission program fits, original fragments and complete
synthetic scenarios remain explicitly different evidence; do not claim full
original scenario execution.

S0/S1 review checkpoint is historical; approvals and implementation dispositions follow below.

### S1 clarification before review

Signal names preserve exact bytes and compare case-sensitively. Keywords use
ASCII case-insensitive matching. SET declarations become visible in source order;
an earlier condition/assignment cannot refer to a later SET. Implicit
SCHUETTIINITSIG exists first. Restored SIGNALS preserve those exact names.
Mapper declarations likewise become visible in source order, with first ASCII-case-insensitive matching declaration winning. Strip the suffix at
the first '@' from the action lookup argument only; preserve each declaration's
complete name. Lookup Foo@x matches declaration Foo, but declaration Foo@x does
not become Foo. Forward mapper references refuse.
The initial binding validates a signed slot and resolves the configured neutral
fallback only when that slot has no player; actual bindings remain persisted.

The original INIT guard first checks its cursor, then SCHUETTI. With any IF,
rearming SCHUETTI after the first visit does not rerun INIT; with zero IFs the
original null cursor can permit repeated INIT. This slice deliberately uses an
explicit started state and executes INIT at most once, including zero-IF programs.
That continuity/safety adaptation is part of the policy identity. WON/LOST are
unsupported: their global stop flag is therefore unreachable in accepted programs.
The world script fault barrier is separate from original endgame stop handling.

S1 execution review: /root/script_semantics_review APPROVED frozen5081f780;
no P1/P2. Permission review requested one lookup/declaration clarification,
now incorporated. Implementation tests and package reviews remain pending.

S1 permission review: /root/permission_semantics_review APPROVED1bb960b2, no
P1/P2. Together with execution approval5081f780 this releases S2–S7. S2 controls
at a98236e6: focused02 selected6/executed6/failed0/empty0/skipped0/incomplete0.
Focused01's work-count expectation failure is retained. Investigation build01
lacked PATH cmake;02 exposed Result-constructor/doctest formatting errors;03/04
build. Independent reference4/4 passes. Wrong +125 omission and root-AND ELSE
mutants both execute and fail their intended assertions; clean source restored.
S2 review corrections add keyword folding (preserving names), built-in query
precedence, explicit section guard and host/budget classification/cleanup tests.

## S2/S3 reviewed component checkpoint

S2 semantics APPROVED /root/script_semantics_review8d9d7197; five P2 compiler/
fault findings corrected. S3 components APPROVED /root/permission_semantics_review
0b8f5bfe inspected8d9d7197. Corrected control focused02 at8d9d7197 passes14/14,
no failed/empty/skipped/incomplete. Raw1/257 and ALL-filter mutants reach intended
assertions (not compilation/refusal); both exact sources restored before control.
Build attempts S3-01/02 exposed test field-count canary,04 a missing import
parameter; all are retained,05/restored build succeeds.

FORSCH has separate original refresh/default/bonus semantics. The adapter will
import only its construction-relevant researched projection: nested4-byte ENTRY
{id,flags,progress_le}; savedflags bit0 maps exactly to state bit1, duplicates
last-wins. Other research state/progress/bonus behavior remains the inherited
engine model, not a claim of full original research restoration. Missing rows
have no original default researched bit. Construction grants themselves call
plain research-byte getter19160/setter19170; OR2 is complete for that setter
path and invokes neither prerequisite nor bonus refresh. Independent permission
review verified both paths. This limited import scope must be explicit in the
handoff and original-file canary claims.

## S4/S5 integration checkpoint

S4 APPROVED /root/review_integration at86abd3e1: native execution/persistence,
scenario/native import, nonempty74-byte runtime and24-byte widened row oracles,
malformed continuations/bindings, real-host commit/rollback and both schema guards.
S5-focused02 at that clean pin passes30/30, failed/empty/skipped/incomplete0.
S4-focused01 retained18passes/1failure (live fault diagnostics intentionally make
check_invariants report the fault); focused02 at6f0eab6e passes19/19; focused03
at94a4dba3 passes20/20. Build01/02 exposed explicit field-count/version canaries;
subsequent03/04/05 succeed. Schema-bypass mutant reaches the intended repaired-
checksum REQUIRE_FALSE failure; outer source restored exactly before integration.

S5-focused01 retained27passes/1failure: synthetic road had no supported class
flags and refused Unsupported before the gate. Fixture declares flags0x4000;
no production road predicate changed. The original individual permission parser
requires quoted type text (Scene1a5d4->b370); compiler now accepts that syntax,
uses strict nonnegative known widened ids, and rejects bare-number arguments.
Quoted customid3000000000 is a legal content control. Numeric conditions remain
signed32. Wrong raw setter, missing ALL exclusion, missing authoritative gate,
and effective-only rewrite controls each fail their intended assertion; ordinary
controls execute and pass. S5 final reviewer disposition follows after restore.

## S5/S6 checkpoint and precise original-program blocker

S5 APPROVED /root/permission_semantics_review86abd3e1 inspected8aada9a5;
no P1/P2. Quoted-type correction separately checked against primary parserbytes.
All authoritative-gate mutants restored and clean rebuild completes.

S6-canary01 at8e2350db PASS35selected/35executed/0failed/0empty/0skipped/
0incomplete; required fresh-process contract3methods separately passes.
The retained fresh-mnm9838s manifest pins executable SHA256
a5a08a5c40ac003b8b19898d5ecf4c0d8e3b91391097c612b54f16a7e66af880,
producerPID57252 and workerPID57253. The complete stocked synthetic program
uses customtype90001, two players, INIT denial, later allowance/denial, exact
300gold/3ttools/4twood build debit,13encoded entries/5matching checkpoints,
native cuts0/1/5/7/10 and a changed command diverging at actionentry7 /
checkpoint8. It is not a no-grants ship-start demonstration.

Capability inventory [script-capabilities.json](script-capabilities.json):
50parsed,11compiled,0complete permission programs compiled,0full original
scenarios executed by this inventory. All six shipped permission programs need
unsupported object/area/query/action features. Compiled empty kernels are not
tutorial behavior. A bounded original endless01 INIT fragment (offset551,
length173) runs via the normal loader/World.advance in a stocked synthetic
harness:461ALL rows plus exact2229 row, then actual Inn1335 BuildCmd succeeds.
This is labelled FRAGMENT_ONLY; the complete endless01 program remains refused.

**S6_REAL_COMPLETE_PROGRAM is blocked.** No complete shipped permission program
fits this reviewed handler subset. Fragments and the complete synthetic canary
do not replace the missing full-original claim. Completing that claim requires
a new bounded handler scope and primary contracts, beyond the reviewed first
slice. Do not substitute no-op handlers or remove the surrounding program.
Independent remaining scale, compatibility/regression checks and final review
continue; this is not COMPLETE LOCAL CANDIDATE.

Exact additional parsed dependencies for the six permission programs:

- Scenes/0011SingleEndlosAddon/endless01.sze: actions MONEY_ADD, PLAYWAVE; queries AREA:TOUCHED_BY_SHIP_FROM, AREA:TOUCHED_BY_SOLDIER_FROM.

- Scenes/0100Kampagne01/S01_03_01.sze: actions GOTO_SCREEN, LOST, PLAYVIDEO, PLAYWAVE, QUESTLOG_SHOW, SHOWTEXT, WON; queries AREA:CONTAINS, PLAYER:DEAD, PLAYER:INHABITANTCOUNT, PLAYER:MAKES_PROFIT, PLAYER:WAR_STATUS.

- Scenes/0100Kampagne01/S01_05_01.sze: actions CHEAT, LOST, PLAYVIDEO, QUESTLOG_SHOW, SETWARELOG, SHOWTEXT, WON; queries AREA:TOUCHED_BY_SOLDIERS_FROM, OBJECT:INHABITANTCOUNT, OBJECT:KILLED, PLAYER:BOUGHT, PLAYER:INHABITANTCOUNT, PLAYER:SHIPCNT, PLAYER:WARECOUNT, PLAYER:WAR_STATUS.

- Scenes/0201SingleAddOn/single31.sze: actions ASSIGN_ALL_SOLDIERS, ASSIGN_OBJECT, FREEZE_CITY, GOTO_SCREEN, LOST, MINIMAP_ALERT, PLAYVIDEO, QUESTLOG_DRAW, QUESTLOG_SHOW, SET_DIPLOMACY, SET_UNIT_VISIBLE, WON; queries AREA:TOUCHED_BY_OBJECT, AREA:TOUCHED_BY_SOLDIERS_FROM, OBJECT:KILLED, OBJECT:OWNER, PLAYER:SOLDIERLOST.

- Scenes/0201SingleAddOn/single36.sze: actions LOST, PLAYVIDEO, PLAYWAVE, QUESTLOG_DRAW, QUESTLOG_SHOW, SHOWTEXT, WON; queries AREA:TOUCHED_BY_SHIP_FROM, AREA:TOUCHED_BY_SOLDIER_FROM, PLAYER:INHABITANTCOUNT, PLAYER:WAR_STATUS.

- Scenes/0201SingleAddOn/single39.sze: actions ATTACK_AREA, FREEZE_CITY, GOTO_SCREEN, LOST, MONEY_ADD, QUESTLOG_DRAW, QUESTLOG_SHOW, SET_FOR_BUY, WON; queries AREA:TOUCHED_BY_SHIP_FROM, OBJECT:KILLED, PLAYER:SHIPCNT, PLAYER:WARECOUNT.

## S7 first broader regression checkpoint

S6 bounded proof APPROVED /root/review_canaries db13e91e after two P2 wrapper
fixes. Corrected S7-focused01 PASS39/39, no failures/empty/skips/incomplete;
required six-method wrapper contract separately passes. Distinct retained fresh
producer60496 / worker60499 in fresh-lnb378t3. Full original S6 claim stays blocked.
Release9da32085 scale gate1/1:8players/100types/64statements gives85eligible,
150initwork,0idlework;128players/10000types/2000statements gives8571eligible,
10572initwork,0idlework.250000queries take3921/11301microseconds on this Mac.
Compiler-only offset/bytebuffer corrections do not alter measured hot code.

Cheap workflow contracts: first direct attempt lacked PATHcmake (0methods,
FileNotFoundError); corrected PATH run21methods passes (S7-workflow02). First
lint records5findings: two fixed identity constants, pointer membership keys,
byte reinterpretview. Stable source offsets and bounded byte buffers replace
pointer/raw-view use; narrowly documented identity exceptions keep policy/schema
constants distinct from gameplay limits. S7-lint02 passes.

Frozen db13e91e S7-debug-core01 FAIL:1282selected/1210executedpasses/71failed/
0empty/1opt-in skip/0incomplete, no sanitizer lines. Named long sailing/corpus/
population matrices were excluded while identifying compatibility effects;
this is not a complete broader gate. Original assets were present. Failures are
retained, not waived: published128fixture/native-offset expectations need explicit
SCRIPT1 treatment, generated-layout base contexts need normalization, and former
original-data component tests need explicit scope under whole-program refusal.
No tracked source changed during this gate; all workers finished naturally.

Generated-loader correction contract reviewed read-only by review_integration:
fixed-map and ship-start use bases only for settings/catalogues and replace their
players/islands (ship-start figures too). Validate supplied scripts through the
complete compiler and reject every nonempty kernel (including SET/empty INIT),
unsupported definitions/sections or malformed syntax. Unused mapper metadata
must not bind to generated players. Only these generated adapters may explicitly
produce ExplicitNone and clear validated inert/missing base source; ordinary
scenario Missing/Pending semantics are unchanged. Existing manifests, settings,
loadout, player counts and resources remain unchanged. Code proof pending.

### S7 versioned fixture refresh

At b3d78191 the generation-only run `cache/script1/fixture-generation-01/`
ran16 existing synthetic cases under explicit ANNO_UPDATE_GOLDEN=1 (all children
returned0). Eleven new recordings are stored under `fixtures/synthetic/sim/script1/`;
the published128 recordings and goldens remain byte-for-byte unchanged. The17
additional SCRIPT1 determinism pins are self-baselined, supported by the existing
hand-derived state/ledger assertions and changed-command negatives; they are not
original-runtime or independent stream oracles. The three event digests remain
identical to128, since event records do not include world identity. Normal-mode
canonical acceptance of this refresh is pending.

Tutorial/02Trade01.sze and 0011SingleEndlosAddon/endless03.sze both have complete
supported empty kernels. K2/K3 and C4 fixtures now take one normal advance(0)
before commands and require Pending→Ready at tick0. This is a real session visit
that also visits world subsystems, not a script-only initialization. The exact
mechanism vectors must still pass; no unsupported source is stripped for these
fixtures. Their initial replay save is captured after this visit.

### Original-data component regression boundary

Read-only reviewer review_integration classified each of the19 original-data
failures in S7-debug-core01 (41others concern schema/fixtures and11generated
loaders). Unsupported whole programs remain refused by normal World::create.
Test-only original_component::import_only compiles the full source, verifies that
same normal refusal, then authors a NEW fixture retaining all imported data with
Missing script context. Construction remains gated. Supported programs are kept
unchanged. This is used only for import geometry/LOCKED/DISCO/player-mask and
no-player-construction economy/population mechanism tests; names/output identify
component coverage and print the exact whole-program dependency. It is not
campaign/quest/script/permission execution evidence. Existing physical counts,
ledgers, transitions, save/reload and negative controls remain the acceptance
criteria. Review of the resulting patch and normal gates is pending.

### S7 startup lifecycle correction

S7-fixture-pop01 at5161f6bd selected31/executedpasses26/failed5/empty0/skipped0/
incomplete0, including a test SIGSEGV from dereferencing an absent K3 transition.
The extra whole-world advance(0) changed the existing K3 schedule (material
control reached55200 versus55000) and variant sequence, not the empty script's
permission semantics. The failed attempt is retained. The missing-transition
assertion is now guarded before dereference. No population vectors are rebaselined.

World::initialize_scripts is an explicit lifecycle adaptation, recommended by
script_semantics_review from Scene16b70's separate script-step and Frame5b161's
script-before-world order. It executes the SAME transactional runtime visit at
arg0 for Pending fresh clock.ms=0,last_tick=0. q=trunc(125/250)=0, so complete
INIT/suppression occurs without IF dispatch. It does not visit world subsystems
or reset outputs. Ready/ExplicitNone is idempotent; missing/fault/nonfresh Pending
is refused. INIT failure persists the same fault and stops subsequent advancement.
The lazy first-advance path and full-world advance(0) remain unchanged. Native
pending saves still work through either documented start sequence. Callers
initialize before capturing stores/stats/initial Recorder save. Different call
sequences need not have identical last_work; exact native continuation retains it.
Focused scope/continuity/barrier/fault controls and corrected population vectors
remain pending acceptance. No policy/schema field reinterpretation is required:
this adds a declared call boundary using identical arg0 execution semantics.

S7-startup01 at927fe9c3 PASS8/FAIL1 of9selected (noempty/skips/incomplete).
All six non-replay K3 cases pass with the old timing, exact stock/score/variant
vectors, control ledgers and save cuts unchanged. The only failure is a NEW
startup test's incorrect single-IF dispatch expectation atarg63: q=(63+125)/250=0.
Corrected vector usesarg125→q1; no production scheduler change. The required
house_live_reference_contract passes separately.

The SCRIPT1 corpus identity generator copies physical/movement columns from the
published128 fixture and replaces ONLY create_hash after actual creation, with
an explicit provenance header. It does not claim to rerun movement. The normal
long corpus gate compares every physical/movement column with the retained128
rows and with the new fixture, avoiding a duplicated expensive generation run.

Component review at927fe9c3 found two P2 evidence-write/provenance gaps: the long
corpus generator did not check open/write/close success, and exported population
text omitted its removed-program diagnostic. Both are corrected: success is
reported only after checked close, and each exported ledger now carries explicit
component-versus-supported mode and exact complete-program refusal. Re-review
pending; no physical or semantic expectation changes.

Identity generation at5a99eccd: two sequential children passed (K3 replay87
assertions; corpuscreation1004assertions), retained cache/script1/identity-generation-01
manifest with executableSHA/sourcepin. Six newK3pins preserve old55000tick/31418
variant semantic vectors. New50creation rows use39explicitMissing components and
11retainedsupportedprograms, all373importedmasks; physical/movement columns copied
unchanged from128 await the normal longgate. Prior full11recordings were parsed
independently by script_semantics_review: validA3RP/log2, wire1,timeline5,
A3SV/save1/native129+SCRIPT1 checksums, exactEOF.

Startup wrong-interpretation control at f52f82b8: insert one whole-world advance(0)
after the otherwise correct startup visit. Canonical S7-startup-mutant selected2,
legal construction control passed9assertions; scope-isolation test executed42
assertions and failed exactly3 non-script scope comparisons (39passes), no crash,
timeout or unrelated refusal. Clean scheduler restored byte-for-byte (git diff
empty) and normal full Debug rebuild completed; corrected final control pending.

Final cheap gate at c0a3d668 PASS29/29 (zero failed/empty/skipped/incomplete, no
sanitizer diagnostics): BIGBROTHER parser/corpus/reference and public SCRIPT1
reference/tool contracts. Final integration reviewer independently caught a P2
state-note width typo: native permission rows are u32type+u32value,8bytes; value
is constrained0..255. Documentation corrected to match the literal byte oracle
and explicit field list; original ANLTRG remains u16/u8/pad.

Final integration review independently derived a necessary corpus warning delta
from all50rawsources using tools/sze.py:28SPIELER/FORSCH records in20files and
zeroANLTRG records. Prior loader emitted UnknownRecord once per(players,name);
recognizing FORSCH removes exactly1 in each of those20files, aggregate20. The
small committed player_research_record_counts.txt fixture records raw presence,
not C++ counts; exactJSON/per-file hashes/reproducer retained in cache/script1/
S7-player-warning-delta.json and S7-derive-player-warning-delta.py. Comparison
amends ONLY this known warning and create_hash; every physical/movement column
and every other warning stays pinned to128. New fixture rows amended by that
primary derivation before the longgate, not by observed C++ mismatches. The pure
House-kernel original-data case also uses the same explicit component boundary.

Finalsemantic gate7eac32b0 PASS45/45 and freshprocess PASS2/2; both nofail/empty/
skips/incomplete/sanitizer reports. Fresh manifest fresh-sfam28__ records distinct
producer81796/worker81797, executableSHA c4d9164c68e040d7564aa7f5692f84c2e31f0b3e39efcd913abe03d4ba071805.
Startup review APPROVED7eac32b0 / script_semantics_review, noP1/P2.

Broader final01 at clean a7940cc6 retained FAIL:1299selected/1297executedpasses/
2failed/0empty/0skipped/0incomplete/0sanitizer,857.19seconds. One remaining
legacy timer test expected9root scopes instead of10; corrected expectation only.
Full50-file corpus otherwise validates determinism, native save identity, physical
counts and all warning deltas, but three producers_moved expectations differ by1:
S01_03_01:97→98/139; S01_07_01:214→215/302; Single_24:38→39/58. No blanket
movement rebaseline is authorized. Existing WeaponsWorkshop refresh_unlocked is
the only production reader of Player researched bits; the newly imported FORSCH
projection may activate an existing line. A paired with/without-research probe
and independent original-record/type review are pending before changing pins.

Research cause01 at3b82c76f PASS2/2 (61.32seconds): clear imported research in
paired worlds and observe exactlyone newly moving producer in each named file;
timer scope correction also passes. Independent final reviewer derived from raw
FORSCH/Anlagen/PROD3/BGruppen/city records and Welt69580..69640 the exact sites,
line switches, research requirements and full-ratio first steps. Original flags3
restore researched bit2. Both lines have research, but switchmask1 enables only
line0: type1006 needs research4; type1036 needs6. First outputs/inputs are:
S01_03 at144,308: [0,0]→[9,0], [10,11042]→[2,11038]; S01_07 at136,316:
[65,0]→[74,0], [583,753]→[575,749]; Single24 at223,229: [13,72]→[19,72],
[9,952]→[3,946]. Independent reproducer/JSON retained as
cache/script1/S7-{derive-research-production-controls.py,research-production-controls.json}.
The final control clears ONLY the owner's one bit4/6, pins these original stocks,
checks the isolated first legal kernel step and pairs five-minute normal advances.
Only those3 movement expectations are amended via research_production_delta.txt;
all others stay fixed. This is an existing production reader of restored research,
not new weapons gameplay or a full original-script execution claim. Full50-file
corrective gate and focused re-review remain pending.

## Final verification checkpoint

At clean0001a020a8e1025e8531fa0e22358b5c166afce8, Debug corrective PASS3/3,
zero failed/empty/skipped/incomplete/sanitizer,154.08seconds. Full50-file corpus
passes1242assertions; exact research controls and timer case112each. Independent
final reviewer approved source and corrected raw evidence. Prior broader Debug
remains FAIL1299selected/1297passes/2fails. Its unaffected1297cases are reused
because subsequent edits affect only the failing expectations/tests; correction
covers both failures plus one new causal case. The successful unique union is1300
cases, not a single all-pass Debug run.

Final ASan/UBSan at the same clean pin PASS1316/1316: failed0,empty0,skipped0,
incomplete0,sanitizer0,732.91seconds. Required five contract entries each pass
separately. Build is Debug/Werror/ASan+UBSan; four CTest jobs with host admission.
ExecutableSHA b0ff126e3b662e4b4fc57c7224b9fe55ffa88888e7db8010f4abf41c161c322e.
Fresh producer95079/worker95083 exit0, checked manifest fresh-odoyd744. The lane
covers parser/runtime/state/native/build/import/original creation/K3/C4/research
and encoded replay. Completed long original-data population/corpus and settlement
matrices are reused from Debug, not claimed as sanitizer executions.

Permanent main evidence contract re-read before handoff. Published128 fixtures,
original assets and TASKS remain unchanged. All mutants restored; no other lane's
job was stopped or rebuilt. Final source integration was independently approved
through0001a020; final sanitizer/handoff review pending at this writing. Historical
'pending' notes above record their original package checkpoints, superseded by
this final checkpoint and the ledger dispositions. The precise S6 full-original
permission-program blocker remains; do not report COMPLETE LOCAL CANDIDATE.

Forwardable handoff: orchestration/reviews/2026-11-05-map-script1.md.

Final integration disposition: **APPROVED for the bounded local implementation**,
/root/final_integration_review. Code/tests/fixtures reviewed and gated at
0001a020a8e1025e8531fa0e22358b5c166afce8; handoff reviewed at
12569eaf0e8083a2eb2700127da7bce2621fbe58; canonical workflow20bd97fd.
Reviewer independently reconciled final ASan1316/1316 and five separate required
contracts, clean tested source, executable and fresh-process artifact hashes,
distinct successful PIDs, corrected Debug reuse and accurate scope/platform
limits. No unresolved P1/P2 findings. S6 full shipped permission-program execution
remains BLOCKED, so this is not COMPLETE LOCAL CANDIDATE for the entire packet.
Only documentation records this disposition after the frozen gates.

## MAP-SCRIPT-2 package A — complete endless01 contract (proposed)

This is the continuation authorized by the exact marked packet at main27022d75,
without merging or editing local TASKS. Baseline b19b509b is clean and preserved.
Primary evidence: [SCRIPT2 research](../re/script-runtime2.md); complete AST/hash
inventory and current host/build census are in cache/script2/A. SCRIPT1's former
contract-pending research sentence is explicitly historical, not current status.

The dependency closure is empty objects, seven areas, five ship/two soldier
queries and MONEY_ADD/PLAYWAVE. Existing signal/permission/scheduling semantics
remain. Nonempty object definitions and other actions/queries still refuse whole
programs. No filename-specific compiler or stripped production source is allowed.

The owner explicitly approved complete indexed queries instead of original512
candidate truncation. All other researched area predicates remain: authored
(y0,x0,x1,y1), signed endpoints, current registered truncated point, half-open
bounds, exact mapped owner including0, class bands, masks, occupied crew, and
original clipped sector range/partial-final-sector omission and endpoint+15 wrap.
A derived sparse row/column16-tile view is built lazily at the first executed area
query of a script visit. It observes current authoritative figures/types/parent/
crew/expiry state; it is discarded after the visit and rebuilt after load. No
persistent visitation flag exists. Slot scans, relevant insertions, visited rows/
cells/candidates and crew checks consume deterministic work, including building
an index that later exhausts its budget. Idle short-circuit guards do not scan.
A large unrelated-content control must show build work separately from query work.

MONEY_ADD sign-extends validated i32 operands into the established i64 saturating
money policy (F15/F28/F35), one mapped recipient and no account/event side effects.
Staging must preserve ordered additions across aliases and mixed permission writes,
commit exactly once, and roll back after later failure. No ECON work is imported.
A typed wave instruction carries an immutable quoted asset. A generic normal file
loader supplies its scene-relative source namespace; synthetic specs can declare
one. Byte-only imports remain distinct and a wave program without known namespace
refuses instead of silently becoming a bare asset. Combined namespace/asset length
is bounded by configurable ScriptLimits asset_bytes (default4096, hard safetybound
65536 with documented build-time raise path); copied bytes are charged before
staging. This is a modern safety bound, not original sprintf overflow behavior.

PLAYWAVE emits a typed SimEvent with original code317, explicit broadcast audience,
namespaced asset, source offset and old world tick. Multiple events retain issue
order and repeat whenever the script issues them. Local queue overflow/drops do
not fail scripts or affect gameplay, work, PRNG or future decisions. Only budget/
validation/host faults roll back the transaction, including staged events. Existing
HostEventQueue capacity/drop-oldest semantics are reused; per-visit SimOutput is
bounded by charged work and asset length. Publish committed script output after
Scheduler's output reset; script-only startup appends its committed events without
resetting unrelated output or advancing world subsystems. Host output is not
saved/hashed; loaded Ready state never redelivers past output. No exactly-once
external audio promise or audio-device integration is introduced.

Normal startup ownership: Recorder::start initializes a fresh Pending world before
returning a recorder that can accept commands; unrecorded run_turn/run_frame do the
same before commands. Ready/ExplicitNone is idempotent and Missing never changes
to ExplicitNone. Failed startup exposes no command/world-advance prefix. Low-level
World::create retains Pending and pre-initialization save support. To preserve
INIT presentation in encoded replay, a recorder captures the Pending initial save
and records a typed Startup replay entry; native Ready resumes add no entry and
never rerun INIT. A bounded event-only replay observer allows headless recording
consumers to check emissions without mutable gameplay access. The normal CLI uses
the generic namespace-aware loader; no Windows UI change is needed.

Compatibility is planned before new fixtures: native130 + SCRIPT2 discriminator,
script policy2, replay frame3 with explicit Startup entry (legacyframe2 can remain
readable for framing/version-refusal inspection without reinterpreting its native
state). SCRIPT1 native129+SCRIPT1 and its recordings/tools/goldens remain frozen.
New fields include asset limit and optional namespace; derived program/index/work
telemetry are not state. An independent new stream reference and literal bytes
must precede the corresponding persistence claim. Default empty script scope is
67bytes (old62 + asset-limit4 + absent-namespace1). No cross-lane version-number
compatibility is inferred and no automatic merge is authorized.

All packages require read-only approval before their dependent implementation.
A is pending frozen contract review. This section is the current proposal, not a
claim that the new code or complete original canary has executed.

SCRIPT2 A area review: `/root/script2_review_area` independently approved
`fe964e24996c2e2b1ddd59bbd0a24b066c2f4910`, after frozen6214ac5e P2
endpoint-vector finding was fixed. Scope: primary rectangle, class, ownership,
registration, current-touch, empty-object closure and proposed work/startup/identity
boundary. No remaining P1/P2 in that scope. Read-only direct investigation:50
hand vectors and3 Python contracts exit0; canonical evidence remains pending.

SCRIPT2 A action review: `/root/script2_review_actions` approved frozen
`fe964e24996c2e2b1ddd59bbd0a24b066c2f4910` money/wave, namespace, transaction,
startup/persistence contract. P2 missing wave vectors CLOSED. Independently
derived primary bytes plus50 vector checks and3 Python contracts; deliberate
wrong namespace/recipient/unsigned-negative expectations failed as intended.
Probe: `/private/tmp/script2-actions-review/A-reference-review.json`. No remaining
P1/P2 in that scope. This approves A only; B–G remain pending.

SCRIPT2 B frozencced061f gate `cache/script2/B-gate-01.log`:8 selected/8
executed passes,0 failed/empty/skipped/incomplete/sanitizer lines. Wrong right-edge
`>=`→`>` mutation failed exactly the adjacent-right-edge assertion with other
legal controls executing (B-boundary-mutant-01:1 selected/1 failed). Restored gate
B-restored-01:8/8 passes. B-correction-01 repeated unchanged code because the edit
script refused its unmatched text; it supplies no correction evidence. Reviewer
P2: AREA mapper omitted original `@` suffix truncation. Corrected with casefolded
OTHER@ignored and unknown-prefix controls. B re-review remains pending.

B review correction: reviewer withdrew the broad suffix-acceptance demand after
tracing23af0 cargo-qualified AREA operand selection.992db04b/B-correction-02
(9/9 passes) is retained as a superseded intermediate, not accepted fidelity.
Decorated AREA FROM is now an explicit unsupported-query refusal; plain two-owner
casefold binding remains supported. Added same-Ready-runtime membership/movement/
owner/crew/removal controls and large false-AND no-index-work proof per B review.

B P2 query-host refusal classification fixed: evaluator failures now distinguish
work.exhausted (Budget) from missing/refusing typed spatial host (Host), with
transactional signal/timer rollback control. B-correction-03 passed11/11 prior
to this final review fix.
