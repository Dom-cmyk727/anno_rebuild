# WILDLIFE-RESOURCE-1 — ordered resource initialization on an isolated result

**RELEASED 2026-11-18** after independent local INIT review at97d893af.
Exact-head CI and landing acceptance remain pending; this packet takes no pending code. EXECUTOR II MAC; approximately 2–4 hours. Implement the missing
GELAENDE resource prefix as a concrete, tested C++ import evaluator. This is a
faithful prerequisite to wildlife and hunting, not a new gameplay policy.
Production World loading remains unchanged in this unit.

## Intake and files to read

Create `task/WILDLIFE-RESOURCE-1` from accepted
`2f9e4c0bf84cf09998a43c7ff8098fa1522f3a4e` in your existing clean owned checkout.
Preserve INIT and every prior branch/evidence directory. No pending source intake,
main merge, rebase, force push or main push. Read INIT at exact
`97d893af9d78e6662dd8859acc3a905f71c80402` by ref (or its explicitly reviewed
correction supplied at release). Do not cherry-pick it just to obtain its notes.

Read CLAUDE.md, PLAN §§0.1–0.3, TASKS owner rules/§1, evidence-contract.md and
testing/README.md; then:

- `docs/sim/wildlife-initialization.md` §§3–4,6–7 at the INIT pin.
- `docs/re/islands.md`, `docs/re/annowelt.md`, `docs/re/ship-placement.md` and
  `docs/sim/ship-placement.md` where they cover the import admission/placement path.
- `tools/placement_ref.py`: original `place()` versus rebuild `engine_place()`.
- `src/sim/src/scenario_formats.cpp`: raw order, deduplication and import facts.
- `src/sim/src/scenario.cpp`, `src/sim_load/src/scenario_load.cpp`: existing load
  boundaries; read only, no production wiring in this packet.
- Existing `terrain_view.hpp`, `site_checks.hpp`, `walk_cells.hpp` and their
  implementations/tests (locate with rg). Read `TileLayer` ownership and validation
  in `src/sim/include/anno/sim/island.hpp`.
- Existing CMake test registration and public-reference contract conventions.

Reuse preserved primary evidence in cache/wildlife-init-1. Re-read only decisive
newly relied-on instructions, not whole modules. Keep raw binaries, decoded
records, listings and host paths out of published commits.

## R0 — acceptance table and finite selector closure

Before implementation, list requirements with primary addresses, hand-derived
positive/counterexample, named test and expected disposition. Confirm the actual
catalogue receiver; four candidate tables alone do not prove its dynamic type.
Read AnnoGame `0x10001b30` reached by catalogue +0x88, only directly needed
getters/data, and original import admission +0x6c / placement +0x78 reached at
AnnoWelt `0x100893f3` / `0x10089417`.

Use kind values225–256 for the group, not content type ids; INIT §4’s “types”
wording is corrected by this packet (the witness is type70/kind226).
Establish supplied draw interpretation, climate/type dependencies, result range,
extra draws (if any), destination-byte mask and commit effects. Verify widths,
signedness and rounding with distinguishing examples. The original resource
branch is after successful placement; do not guess a modulus or insert dummy
RNG advancement. Scope surrounding calls only as required for this prefix.
Do not expand into actor loading, whole-map callbacks, fauna lifecycle or combat.

If unresolved calls prevent even the bounded evaluator, report the exact finite
edge and preserve a clean checkpoint. No invented admission callback or supplied
“success” Boolean may replace the predicate being implemented. At six hours,
return what is established instead of expanding the package indefinitely.

## R1 — independent examples and concrete transient contract

Write a compact resource-initialization contract and authored examples before
C++. Inputs are ordered original placement records, terrain, resolved type facts,
initial scratch objects/variation bytes, climate facts and a bounded supplied
u32 draw sequence. Define supported records precisely; return an explicit
unsupported disposition for unknown semantics, never silently successful zero draws.

Output owns the resulting scratch tile data and a separate transient variation
plane, with per-record dispositions and exact consumed-draw count/order. Keep the
input unchanged. Define draw exhaustion and malformed geometry/fact handling so
no caller receives an apparently complete successful result after partial work.
Do not claim original rollback if this is the evaluator's transactional API.

The original variation is the separate byte plane at island +0x18. Do not alias
it to growth phase or invalid spare aux bits. If existing registry facts omit
selector metadata, use explicit transient import facts; no silent registry hash
change. No persistence layout or numeric feature tags are allocated here.

## R2 — implement the bounded evaluator

Add a small dedicated import helper/header/source and tests in sim. Reuse proven
lower-level geometry and overlap operations where they match the original caller.
Do NOT reuse BuildCmd `placement_rule` wholesale or widen BuildCmd eligibility:
it intentionally refuses scenery flags0x29. Check the effects of
`place_object_record` before reuse rather than assuming equivalent import flags.

Keep original input order. For each supported record, compute admission against
current scratch state, apply accepted placement, then perform the qualifying
resource selector and variation write in the proven order. Later records see
prior accepted changes. Test rejection and nonresource behavior explicitly.
Current import deduplication cannot stand in for this sequence; do not feed the
helper only the filtered existing ScenarioSpec object list.

No generic interpreter, callback/plugin framework, simulation World mutation,
production loader switch, RNG-global access, native-save change or default flip.
Content selection uses parsed facts rather than a whitelist of shipped type ids.
The evaluator must work for a second data-defined type with matching behavior.

## R3 — prove original prefix and counterexamples

Using real files read-only, exercise the smallest verified Tutorial/01Discover01
prefix ending with header34's first resource: type70/kind226, local59,29, flat48.
Derive expected variant result independently from R0. Distinguish original
admission from mere rectangle containment. Extend the prefix only while all
required facts are resolved. Report supported/unsupported records and actual
draws separately: 4,589 candidates are not 4,589 proven draws.

Synthetic cases must distinguish:

- accepted resource, rejected resource and nonresource draw counts;
- overlap/duplicate records in both orders, with later admission reading updates;
- low-nibble preservation and high-nibble replacement, distinct from growth phase;
- draw exhaustion and missing/invalid facts without an apparently complete result;
- two different draw values with independently predicted variant outcomes;
- a second data-defined resource type and relevant width/rounding boundaries.

One legal control must execute before a minimal wrong-order/draw or wrong-mask
mutation fails its intended assertion. Use the independent reference for numeric
expectations; no echo oracle, self-baseline or checks merely restating outputs.
Preserve first failures. Do not repeat a broad mutation campaign.

## R4 — focused gate, review and stopping criteria

Inspect host capacity/contention and retain admission control. Run cheap reference
contracts, focused Debug and ASan/UBSan through the canonical runner, then the
bounded real-prefix test once. Require the new public-reference contract. Reuse
unaffected long sailing/render/corpus gates with a dependency explanation. Record
selected/executed/failed/empty/skipped/incomplete separately and exact source,
fixture, binary and log identities. Restore and rebuild after mutations before
claiming a clean gate. Re-run only changed or failed scope, never unrelated lanes.

Freeze and ordinary-push only your task branch. Report READY FOR REVIEW directly
to the Mac orchestrator with head/tree, CI run/status, acceptance table, addressed
primary facts, actual prefix effects/draw count, preserved failures and an exact
production-integration handoff. The orchestrator supplies independent read-only
review; start no reviewer agent yourself. Do not self-certify acceptance or wait
for CI before completing independent documentation.

Stop with a reusable C++ prefix evaluator, independent examples, real-file prefix
proof and honest limits. It does not establish a full original RNG timeline or
spawn wildlife in play. No base state/version/digest/fixture pin changes. Before
production integration the orchestrator must coordinate variation/spawn storage
and feature tags with the board owner. Do not start that next unit on your own.
