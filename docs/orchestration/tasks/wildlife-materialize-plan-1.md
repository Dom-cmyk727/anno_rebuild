# WILDLIFE-MATERIALIZE-PLAN-1 — ordered eligibility and species selection

RELEASED after independent scope review, 2026-11-19. Target 4–6 hours; checkpoint
at six hours. Build on the reviewed chunk-maintenance result and existing species
binder. This is an owned planning stage toward hunting, not live fauna yet.

## M0 — read, pin and define the bounded contract

Start `task/WILDLIFE-MATERIALIZE-PLAN-1` from locally reviewed
`95d2eb0f4bf09c559413ef380fba3fd4281939ff`. Fetch and read this packet at the exact
orchestrator dispatch pin; do not merge/rebase the board or main. Preserve evidence.
Read `CLAUDE.md`, `docs/PLAN.md` §§0.1–0.3, current owner rules,
`docs/testing/evidence-contract.md`, `docs/testing/README.md`,
`docs/re/wildlife.md` §§1–2, `docs/sim/wildlife-chunk-plan.md`,
`docs/sim/wildlife-species-load.md`, `docs/sim/wildlife-create-effects.md` and
`docs/sim/wildlife-decisions.md`. Inspect the chunk/species/create APIs and their
focused tests, plus the existing whole-object ground-facts reader where relevant.

Before code, write the small input/order/effect/error and acceptance tables.
Trace only newly needed predicates at materialization entry 0x10093767 through
0x10093886 and creation-helper entry; reuse settled evidence. Establish original
branch order and exact signed-byte wrapping eligibility. No broad new RE campaign.

Scope: process one completed owned chunk in record order, check eligibility and
produce owned ordered species-selection plans using caller-supplied immutable
final tile/cave facts and raw draws. Facts are explicitly supplied outcomes, not
proof of real terrain queries, cave geometry or native managers. No claimed full
sweep, actual animal creation, first action or runtime PRNG history.

## M1 — precise eligibility without invented fields

Preserve complete list order and distinguish skip reasons. Pin category, occupancy,
then timestamp requirements against the original branch order. Test the signed
8-bit difference of low8(time/1000) and timestamp at zero, 127, 128, 255 and wrap;
use explicit integer semantics without host signed-char assumptions.

Only inspect fields actually needed for the reached stage. A non-category record
or occupied record must not require a tile fact or invented timestamp. Unoccupied
records require known fields when consumed. Unknown is distinct from known zero.
Use the current optional-knownness record representation; do not turn unknown
fresh ID/high bit/byte11 into zero merely to fit another API.

The final tile lookup uses its actual type: NO habitat/preparation substitution
of a growing type with id+1. Pin output-0 goods 0x46 and kind 0x145 against the
original calls and preserve zero-object and nonqualifying behavior. Missing or
ambiguous supplied facts are errors, not a false predicate. Keep original narrow
projection fields separate from engine IDs and configurable content.

## M2 — owned species source once per batch

Reuse the climate-2 source profile and exact 32-slot order/duplicates. Prepare
owned catalogue facts once per batch, rather than reading/hashing both PE files
and parsing the registry for every record. Make only the factoring necessary for
this composition. The existing single-record public API must retain its semantics,
error order, unsupported-kind behavior and source-lifetime guarantees.

In particular, do not make a previously unselected unsupported or invalid New
capability fail earlier merely because a batch cache eagerly binds everything.
Keep generic facts separate from selected New capabilities; a small lazy cache or
stored per-type binding result is sufficient, with errors surfaced at their actual
selection boundary. Do not filter entries, reroll, replace unsupported kinds or
expand climate support. No generic cache framework or registry/digest repair.

Use one immutable raw-draw span and cumulative offset. Ordinary versus cave
selection, saved goods, 18-inspection limit, missing entrance and selector errors
keep the accepted rules. Supply cave facts explicitly; missing facts differ from a
known zero-entrance outcome. No fabricated position provider or FP conversion.

## M3 — ordered batch results and honest failure boundary

Return record-token-associated eligibility results and selection plans in original
list order, with exact per-record and cumulative consumed counts and unused suffix.
Cumulative counts cover calls to the accepted species selector only; they do not
certify draws consumed by omitted position, allocation or creation callbacks.
A skip consumes no selection draws. A selected unsupported creation kind retains
its actual choice and consumed count, then stops this supported batch as a typed
noncommittable boundary; it must not become an original rule to skip that animal.

Missing required facts or selector exhaustion likewise retain the completed prefix,
failing record/stage and exact draws already consumed as diagnostics, never a
successful partial chunk or original rollback. Do not infer physical spawn changes
from a selection plan. Records remain unchanged, including saved ID, occupancy and
cooldown; those writes require actual successful creation in a later package.

Output owns source facts and selected capabilities after input owners disappear.
Document that this plan assumes supplied facts remain immutable across records;
it cannot establish original mutable callback behavior, real allocation outcomes
or complete materialization parity. Stop before unresolved coordinates or geometry
are needed. No invented zero position, actual manager or live publication.

## M4 — distinguishing proofs and one bounded source case

Hand-derive complete expected sequences before implementation:

- Mixed records: wrong category, occupied, not-yet-eligible, disallowed actual tile,
  then at least two eligible records using ordinary and cave selection. Assert exact
  output order, skip reasons, chosen ring slots and cumulative draw suffix.
- Boundary time cases above, including equal timestamp eligible and negative wrapped
  delta ineligible. No whole-tick or real-time scheduling claim.
- A growing actual type versus id+1 whose facts differ, so erroneous substitution
  changes eligibility; include the legal actual-type control.
- Missing facts only when reached, known zero versus absent, source destruction,
  unchanged chunk/draws and deterministic repeated execution.
- Two supported selections followed by an unsupported selection or draw exhaustion:
  exact retained prefix and stop record, consumed count and no fabricated success.
- Source-bound climate-2 case using retained original files and independent raw-source
  expectations from the accepted species proof, with authored record/fact/draw inputs.
  It proves original source binding plus batch composition, not runtime fauna.

Use a few actual targeted mutants after legal controls: inverted/unsigned timestamp,
wrong traversal order, growing substitution, draws on a skipped record, wrong batch
cursor or filtering unsupported selection. Choose minimal distinguishing cases;
no needless Cartesian matrices. If introducing a public reference, its CLI must
reject empty/wrong/echo output. Prefer existing independent literal/source evidence.

## M5 — freeze, focused gates and report directly

Cheap controls first, then freeze. Canonical Debug and ASan/UBSan cover new batch
logic and the reached/refactored species and creation-binding APIs. Keep required
real-file case separate and executed with the game directory set. Run unchanged
chunk/release checks only when reached or modified; document reuse honestly.
No broad/full/heavy/opening/pins rerun. Memory admission stays on; select jobs for
actual contention. Format, lint, affected strict warnings and public-file checks.
Retain exact original failed attempts, source/binaries, meaningful test counts,
mutant outcomes, source identities, skips and NOT_RUN limitations.

Ordinary-push your own task branch and report READY FOR REVIEW directly to the
Mac orchestrator with head/tree, tested source, exact CI link, contract/evidence
paths, changed files, original predicate proof and remaining boundaries. No executor
reviewer agents; the orchestrator independently reviews the frozen result. Stop at
six hours with concrete remaining work if needed rather than expanding the packet.
No World/scheduler/native/save/Rules/tag/default/identity/global-PRNG/navigation/
raster/graphics change. Do not touch original files, other jobs/coordinators or the
owner's keep-awake assertion. Do not turn this into a new owner policy decision.
