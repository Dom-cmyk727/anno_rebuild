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
| S0 isolation/identities | git/host snapshot, task packet | exact clean4102196a | unexpected edits preserved | this ledger/cache/script1/S0 | pending |
| S1 actual scheduling | Scene16b70; Frame5b161/fef6/ff7e | count4,tick32 visits1 | delta/Frameinterval or stamp-update reading | scheduler vectors/primary review | pending |
| S1 grouping/branch | Scene22bf0/10290 | right-associated guard | C precedence; falseAND runs ELSE | interpreter reference trace | pending |
| S1 restore/init | Scene16fb6/1706a/1708c/16360 | signals overwrite SET; INIT once | restore afterINIT; repeatedINIT | init/restored vectors | pending |
| S1 permission effects | Welt1bca0; Scene15fa0/15fe0 | raw1 grant; individual2229 | raw257 unlock; ALL alltypes | permission reference/data | pending |
| S2 compiled execution | parser/grouped/read_on + S1 | bool assigns/IF/timer/query | extractcalls from unsupported kernel | compiler/evaluator tests | pending |
| S2 bounds/cursor | originalroundrobin + explicit safety | saved incomplete round resumes | restartIF0; wall-time budget; partialINIT | reference/runtime tests | pending |
| S3 permissions/import | player1d240/ANLTRG branch | absence vszero, rawbytes | replayimportasactions; wrongowner | import/action tests | pending |
| S3 data-definedALL | HEADfolk/exclusion fields | original461types + customcontent | flags_word filter; hardcoded idlist | original/custom profile tests | pending |
| S4 native identity | explicit stream/schema/program policy | exact newbytes; reload | old128/checksum-first false refusal | independent stream vectors | pending |
| S4 production boundary | proven script-beforeAdvance | recorder/unrecorded/replay same | canary-only runner; interval batching | publicsession tests | pending |
| S5 authoritative build | shared check_record | scriptallow -> exactdebit | stalequery; deny ignored; dryrun bypass | atomic/live negative controls | pending |
| S5 path separation | create/found/house callers | later building import/founding | restrict every create_building | caller regressions | pending |
| S6 real vs synthetic | all50 AST inventory | executed supported complete program | fragment claimed as fullscenario | capability inventory/canaries | pending |
| S6 scale | deterministic counters/Release | moreplayers/types/statements | query city/figure rescan | modest benchmark | pending |
| S7 evidence | canonical runner + independent reviews | nonempty frozen-source gates | skipped/incomplete relabelled pass | retained raw logs/manifests | pending |

## S1 proposed execution contract

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
5. A Pending supported program runs its entire INIT transaction on the first
   normal World::advance visit, before clock/world advancement. While Pending,
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

## S1 proposed permission contract

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

## S1 proposed authoritative boundary and identity

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

S0/S1 independent reviews pending. S2 production implementation is not released.

### S1 clarification before review

Signal names preserve exact bytes and compare case-sensitively. Keywords use
ASCII case-insensitive matching. SET declarations become visible in source order;
an earlier condition/assignment cannot refer to a later SET. Implicit
SCHUETTIINITSIG exists first. Restored SIGNALS preserve those exact names.
Mapper declarations likewise become visible in source order, with first matching
ASCII-case-insensitive prefix-before-@ winning. Forward mapper references refuse.
The initial binding validates a signed slot and resolves the configured neutral
fallback only when that slot has no player; actual bindings remain persisted.

The original INIT guard first checks its cursor, then SCHUETTI. With any IF,
rearming SCHUETTI after the first visit does not rerun INIT; with zero IFs the
original null cursor can permit repeated INIT. This slice deliberately uses an
explicit started state and executes INIT at most once, including zero-IF programs.
That continuity/safety adaptation is part of the policy identity. WON/LOST are
unsupported: their global stop flag is therefore unreachable in accepted programs.
The world script fault barrier is separate from original endgame stop handling.
