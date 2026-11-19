# WILDLIFE-CHUNK-PLAN-1 — one chunk before animal materialization

RELEASED after independent scope review, 2026-11-19. Target 4–6 hours; checkpoint at 6 hours.
Compose the existing decisions into the original ordered spawn-record maintenance
phase. This advances a complete sweep without bypassing position/World decisions.

## C0 — intake and exact contract

Start `task/WILDLIFE-CHUNK-PLAN-1` from locally reviewed
`f5f1c9fa7f0eca4d6263147797022ff1c5e6d771`. Read this task from the exact pinned
orch/mac-lane dispatch, no main/board merge/rebase. Preserve old refs/evidence.
Read `CLAUDE.md`, `docs/PLAN.md` §§0.1–0.3, the current owner rules,
`docs/testing/evidence-contract.md` and `docs/testing/README.md`. Then read
`docs/re/wildlife.md` §1 (0x10093480 through 0x10093767),
`docs/sim/wildlife-decisions.md`, `docs/sim/wildlife-prefix.md`,
`docs/sim/wildlife-release-effects.md` and `docs/sim/wildlife-species-load.md`;
inspect the current density/habitat/replenish helpers, typed prefix/release
records and related oracle/tests.

Write acceptance and input/effect/error tables before code. This packet is one
isolated owned chunk's retention, recycling, candidate scan, replenishment and
leftover discard, STOPPING at materialization entry 0x10093767. It is not a complete
sweep or a World state update. Supplied habitat facts/outcomes and normal-success
record allocation are explicit inputs/profile assumptions, not proven original
terrain queries or an allocator-policy simulation. Reuse settled reads; read only
specific field/mask/order gaps newly needed before implementing them.

## C1 — faithful owned record order and retention

Use small owned typed records and a chunk snapshot; reuse prior types where their
meaning fits, avoid mutating their established semantics to suit this packet.
Distinguish category, packed position/occupancy, timestamp, saved goods, and saved
figure ID presence. A fresh original record's untouched/uninitialized saved ID is
unknown/absent, never invented 0 or a newly allocated figure. Other fields likewise
need defined/provenance treatment if their original initializer leaves them unknown.
No raw pointer/padding image, live manager or persistent schema.

In original traversal order, retain occupied category-0 records without reevaluating
habitat. For unoccupied category-0 records, use explicit resolved habitat for that
cell and unlink rejected records onto the HEAD of a temporary reuse list. Preserve
non-category-0 records and their relative position among surviving records. Count
spawn records as the original does, not live figures. Do not trim excess retained
records to the desired density. Independently verify counter widths/comparison
before choosing types; any supported adapter boundary must not become a global
animal/content cap.

## C2 — exact candidate and replenish phase

Represent the original 16 candidate sample positions (x offsets 1,3,5,7 inside y
1,3,5,7) and their supplied eligibility in a way that cannot silently substitute
ID order for row-major order. Distinguish supplied facts from evaluating actual
terrain; reuse wildlife_habitat only when its full inputs are genuinely available.
Reuse the accepted density table/decision.

For positive deficit, reserve one recycled-head record or a fresh abstract record
before selecting candidates. The normal-success allocation profile must not claim
behavior of failed native allocation or arbitrary callbacks. Draw modulo current
candidate count, swap the final candidate into the removed slot, then test the
original category-0-position lookup. A collision is an EXISTING matching category-0
record, not merely one whose occupied bit 0x40 is set. Retry using the SAME reserved
record and the reduced candidates, consuming the next supplied draw. Preserve
first-match/list semantics and any category availability condition actually read.

On a successful insertion, write only the original defined fields, prepend the
record to the chunk list and set the original category flag bit. Pin position
packing/masks, saved goods 0xff, timestamp low 8 bits of (time/1000), occupancy clear, and
untouched reused bits/ID against the specific instructions. Do not assume all bits
are reset because the slot was recycled. Then reserve the next record if required.
Discard leftover recycled records only at the original successful cleanup stage;
clear chunk category flag 0 only when the counted retained/replenished total is 0,
while preserving unrelated flags. Record identities are local plan tokens, not
real FigureIds or permission to allocate a World ID.

Use existing replenish helper where its returned state fully fits. It does not
return record-list/partial failure effects, so do not infer those from chosen
counts or force a broad shared-helper rewrite to obtain them. Any small factoring
must preserve old public behavior and retain the relevant regression coverage.

## C3 — errors, ownership and stopping boundary

Draw input is immutable caller-supplied raw values with an exact consumed count;
there is no world RNG cursor or global scheduling claim. OQ-572 candidate exhaustion
remains a named unsupported boundary, never modulo-zero, arbitrary fallback or
successful completion. Distinguish draw exhaustion and invalid facts similarly.
If returning partial diagnostics, show their precise stage, reserved/recycled/list
state and consumed count, and mark them noncommittable. Do not claim original
rollback or present a partial result as a successful chunk update.

All admitted inputs must have sufficient, unambiguous habitat/lookup facts; no
missing-fact-as-false default. Output owns what it uses after callers disappear.
Failure and success leave caller inputs unchanged. Define no native destructor,
allocator failure policy, unsupported query result or silent safer gameplay rule.
Stop before materialization selection, coordinates, per-animal creation plans,
manager calls or first actions. Those can consume this phase in a later packet.

## C4 — independent full-list proof

Hand-derive complete record lists/fields and draw traces before C++. Include:

- At least two rejected records whose recycle order reverses; preserve a distinct
  non-category-0 survivor between records and an occupied record whose habitat fails.
- A collision followed by insertion with the SAME reserved record, then a fresh
  insertion. Check swap-pop candidate state, temporary reuse order, final prepend
  order, remaining unknown fresh ID, and preserved reused ID and other unwritten
  fields. Timestamp updates only on successful insertion. Reserved/colliding diagnostic states retain the prior
  timestamp until that write.
- Excess already-retained records: no trimming and no unnecessary draws.
- Category-0 collision with occupancy clear, wrong-category sibling, no candidates,
  all rejected, leftover recycle discard and unrelated flag preservation.
- Timestamp wrap, missing supplied facts, partial draw exhaustion and OQ-572
  exhaustion with exact counts/stage, never a success or fabricated rollback.
- Complete ownership, immutability, deterministic repeat and unchanged draw suffix.

Use literal independently derived outputs or a small independent reference. Comparing
only counts or two compositions of the same helper is insufficient. A few actual
production mutants must fail intended assertions after legal controls: append
instead of prepend, filter occupied only for collision, reevaluate occupied habitat,
trim excess, clear reused ID or invent fresh 0, and wrong swap-pop ordering as needed.
Do not manufacture costly full matrices; choose minimal distinguishing fixtures.
If a public reference is introduced, prove wrong/empty/echo results fail its real CLI.

## C5 — focused verification and reportback

Cheap controls first; freeze; canonical Debug and ASan/UBSan cover new chunk plan
and directly reached decisions/prefix/release regressions. Source-bound species
regressions need rerun only if touched, not because they are adjacent in the project.
Synthetic plan cases are not corpus proof. No full/heavy/long/opening/pins campaign.
Admission stays on; jobs follow current host contention. Format/lint/affected
strict warnings and public-file checks apply. Preserve first failures, exact tested
source/binaries, counts/skips/NOT_RUN and meaningful unchanged-evidence reasons.

Document the owned pre-materialization contract and evidence; ordinary-push own
branch; send READY FOR REVIEW directly to Orchestrator II Mac with full head/tree,
CI, tested identities, original field proof, full-list expectations, causal controls
and precise limits. No executor reviewer agents. At 6 hours report concrete remaining
work rather than silently expanding scope. No World/scheduler/save/Rules/tag/default/
identity/PRNG/navigation/raster/graphics change. Preserve other jobs/coordinators,
original files and the owner's keep-awake assertion.
