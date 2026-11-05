# MAP-NEEDS-2 — early cloth and leather, from resources to households

**READY / RELEASED, 2026-11-05. Owner: first Mac gameplay executor.**

## Assignment and accepted base

Continue in `/Users/dom/Dev/anno1503-worktrees/anno1503-worktrees/anno1503-MAP-2`,
on `task/MAP-2-original-like`, from accepted local head
`67158af4d29d87fed8686dc6542ca2a9f4b1f75c`. This includes MAP-SUPPLY-1 and its
corrections through `eda3441d`, plus the orchestrator's documentation correction.
Check the actual HEAD and tree first; do not reset unexpected work. Obtain this
packet and the current instructions from fetched `origin/main` with `git show`;
do not merge main into your branch to read them.

Deliver two tested headless chains: sheep/grazing → wool → cloth → households,
and hunting → hides → leather → households. Account separately for hunting's
food output. Reuse existing production, carriers, stocks and resident jobs where
their contracts match. The intended outcome is actual new goods reaching people,
not another set of disconnected arithmetic kernels.

Proceed N0–N7, with independent package approvals releasing dependent work.
There is no routine orchestrator checkpoint between packages. Conduct the
hunting feasibility/contract work early, so unknown dependencies are discovered
before a long end-to-end run. Finish all unblocked packages in this assignment.
If hunting needs a new unsupported wildlife lifecycle or an unapproved gameplay
adaptation, isolate that dependency and complete cloth; report the whole bundle
PARTIAL, not complete. A finite original resource pool may support a completed
production slice, but cannot establish indefinite renewal.

MAP-SUPPLY-1 acceptance is limited to its documented scope. Its corrected food
production is 1,183 raw units in the selected town, and purchases are 756 versus
237 with and without farms. Its 1 t initial food store is named test setup.
The scan of 460,357 interior footprints does not prove ship-founded alcohol
impossible: borders and founding-territory effects remain untested. Do not reopen
that research or depend on the withdrawn conclusion in this package.

## Read in this order

1. Your checkout's `AGENTS.md`, `CLAUDE.md`, `docs/PLAN.md`; the current main
   `docs/testing/evidence-contract.md` and `docs/testing/README.md`; this packet
   and its dispatch in main's `docs/orchestration/TASKS.md`.
2. Your branch's `docs/sim/small-farm-supply.md` §§1–15 and main's
   `docs/orchestration/reviews/2026-11-05-map-supply1-close.md`,
   `docs/sim/forestry.md`, and the related open questions, especially OQ-491
   and OQ-464. Read the accepted lifecycle and short-visit timing corrections.
3. `docs/re/production.md`, `docs/re/forestry.md`, `docs/re/transport.md`,
   `docs/sim/production.md`, `docs/sim/population-inputs.md`,
   `docs/sim/population-live.md`, `docs/sim/state.md`. Use
   `docs/orchestration/gameplay-next-chain.md` §2 as context, not a second scope.
4. Follow actual callers in `src/sim/src/driver.cpp`, `harvest.cpp`,
   `production.cpp`, `transport.cpp`, `config.cpp`; their headers under
   `src/sim/include/anno/sim/`, including `figure.hpp` and `state_fields.hpp`;
   original registry/scenario loading under `src/sim_load/`.
5. Relevant tests under `src/sim/tests/`: `production_test.cpp`,
   `driver_classes_test.cpp`, `transport_test.cpp`, `harvest_state_test.cpp`,
   `harvest_lifecycle_test.cpp`, `harvest_delivery_test.cpp`, plus the actual
   small-farm, resident and population integration tests found with `rg`.
   Read their references, fixture registration, and `tools/sim_stream.py`.
6. Read-only original `Anlagen.dat`, `FigurenHeader.dat`, `Data/BGruppen.dat`, selected
   scenario/island records, and the relevant original binary functions. Record
   the real paths and hashes. Prior notes do not replace checking decisive
   masks, dispatch kinds, widths or timing against primary evidence.

Create `docs/sim/early-needs.md` as the concise contract/acceptance index. Put new
binary findings in the appropriate `docs/re/` file. Link raw evidence rather
than duplicating it into several prose reports. Original files remain private.

## N0 — inventory, feasibility and small acceptance table

- Inspect host CPU, available RAM, disk and competing jobs. Reuse calibrated
  build/test limits where still appropriate; keep admission control on. Do not
  stop anyone else's jobs or the owner's keep-awake assertion.
- Inventory the actual producer types, figure routes, resources, input/output
  goods, ratios, worker slots, transport and consumer roles from parsed data.
  Starting examples are sheep farm 807, weaver 1001, hunter 805 and tannery 1023;
  verify these and the variants. They are evidence examples, not engine allowlists.
  Do not substitute later cotton or confuse hunter and fur trapper behavior.
- Mark each stage existing/incorrect/missing/unread. In particular, do not assume
  SmallFarm's animal path already enables the Farm-class sheep path. Check
  whether hunting targets are loaded tile objects, figures or something else,
  and which creation/renewal path the proposed proof actually needs.
- Choose bounded original-data scenes with reachable resources and consumers.
  Audit initial stocks, in-flight cargo, other towns and alternative suppliers
  before promising a canary. Identify construction/availability dependencies;
  imported buildings prove production, not player construction eligibility.
- Write one acceptance table mapping each claim to a positive case, decisive
  counterexample, exact source evidence and reviewer disposition. Record the
  intended mutation and assertion before implementation. Do cheap feasibility
  checks before commissioning a long simulation.

Exit: independently reviewed inventory and feasible cloth proof plan; explicit
hunting dependency list. No implementation based on an unresolved predicate.

## N1 — original contracts and independent examples

Trace sheep dispatch, targeting, travel, work, raw credit, production and cleanup.
For hunting, trace target selection, claim/kill/consumption, return, both outputs
and failed-job cleanup. Establish quantities, units, masks, counters, ownership,
PRNG draws, output saturation and time handling. Distinguish resource ownership
from worker claims; do not invent a territorial veto absent original evidence.
Identify wildlife/resource renewal separately from consumption.

Use existing research/reference infrastructure. Commit hand-derived examples
and an independent reference before the corresponding new engine behavior.
Include smallest-positive and zero visits, split delays, boundary capacities,
unavailable targets, refused dispatch, removed home and repeated end. An example
must distinguish the plausible wrong bit mask, rounding or goods slot. A script
that echoes expected values, accepts zero vectors or never terminates must fail
its bounded CLI contract.

Review cloth and hunting contracts separately, against original bytes/data and
callers. An approved cloth contract releases N2 while hunting research continues.
No new policy decision is delegated to a reviewer. Unknown hunting behavior
blocks only the dependent implementation, with an exact function/data question.

## N2 — sheep, wool and cloth implementation

- Connect the data-defined sheep behavior to the appropriate shared worker path.
  Preserve SmallFarm and forestry behavior. Support parsed variants without a
  production list of original IDs or fixed original registry sizes.
- Complete wool credit, weaver input acquisition, conversion, output collection
  and delivery through normal jobs. Reuse correct existing stages; do not
  rewrite them merely to make this package larger.
- Check each transfer's debit and credit, reservations and one-time counter
  release. Cover full input/output, failed send, target exhaustion, interrupted
  walk, home demolition, public worker end, repeated end and save/reload.
- Add an alternate data-defined producer/consumer configuration to demonstrate
  the new seam is reusable. Unsupported behavior must refuse explicitly; do not
  introduce a plugin ABI or general scripting system.

Exit: reviewed engine and focused tests, including a legal control followed by
the specific broken implementation failing its intended assertion.

## N3 — cloth reaches real households

Build one bounded real-file canary through ordinary simulation progression.
Use public commands for changes during the run. Imported original producers
are allowed and must be identified; prefer public construction where the
already-supported placement and available original supplies permit it.

Prove new wool becomes new cloth and is delivered to a relevant stall/store and
consumed by multiple eligible households where the scene supports them. Derive
the number and roles from the scene rather than forcing a production invariant.
Record target player, city, buildings, goods, elapsed ticks and starting stocks.
Reconcile per-stage conversion and transfers, starting/ending stores and cargo,
household purchases and any legitimate loss. Assert unexplained terms are zero.
Scope every counter to that same town and producer set; unrelated towns must
not contribute to the claimed production.

Use a producer-disabled control that still demonstrably runs and can consume
any initial inventory. Show the treatment's new supply and its causal benefit;
do not declare success merely because either arm consumed preloaded cloth.
If a stock-limited causal fixture is needed, label its setup explicitly and
retain a distinct unmodified-original proof. Neither is a ship-start demonstration.
Add interruption/recovery through supported commands and explain whether the
observed resource renews or only lasts through repeated cycles.

Exit: cloth package review approved, with a reconciled ledger and no vacuous
control. Keep this result even if hunting remains blocked.

## N4 — hunting and leather implementation

Begin engine work only after the hunting part of N1 is approved. Implement the
minimum traced reusable hunting behavior, then connect the existing tannery and
transport/consumer paths. Do not turn this into combat, animal AI or spawning
policy development. If a missing lifecycle is essential, describe and contract
it before implementing; a new gameplay adaptation needs the owner decision.

Test food and hides separately, including exact raw credits and conversion.
Cover target gone/taken, competing workers, empty search, full home/output,
failed dispatch, home/worker removal, in-flight save/reload and duplicate cleanup.
Do not infer the hunter's claim or death behavior from forestry or fur trapping.
Keep data/configuration boundaries and deterministic ordering explicit.

Exit: independently approved implementation and targeted wrong-reason controls.

## N5 — leather supply, isolation and persistence

Apply N3's canary/ledger requirements to hides → leather → household consumption.
Account for food produced by hunters and competing suppliers. Do not seed an
unlabelled animal, hide stock or finished goods into the real proof. If the
scene lacks a required original resource, report it and use a separately named
synthetic mechanics fixture without promoting it to real-file acceptance.

Add a small two-owner/two-town test that proves transfers and measurements stay
with their intended owners. Exercise shared dispatch interactions between the
new chains and existing farms, rather than assuming isolation from unit tests.
Save/reload at distinct in-flight worker, processor and delivery states. Check
the resulting world and ledger against uninterrupted execution.

Record the successful real chains and replay them in a fresh executable through
the canonical workflow. Include a legal changed command that must diverge at an
identified checkpoint. Test a refused command separately; a mutation that never
reaches the intended operation is not a replay fault-sensitivity proof.

## N6 — identity, scale and compatibility

The accepted local baseline is stream 129 / PRNG timeline 5, House updates off
by default. Determine whether this implementation changes serialized state,
rules/registry digests, command behavior or draw ordering before recording logs.
Follow the version contract and document the migration/refusal decision. Do not
assume the other Mac's independently numbered stream is compatible, amend a
published identity, or rebaseline unexplained goldens. Hand-derive new serialized
fields and update the independent stream oracle where applicable.

Run a bounded Release comparison of active producers and increasing unrelated
town/figure content. Record work counts, simulation duration, wall time and
sample count; distinguish cost per entire run from cost per tick or frame.
Avoid new whole-world scans per worker visit when an existing local index fits.
Fix an introduced pathological scan with focused proof; do not begin a general
scheduler redesign or claim multiplayer/late-game performance from this probe.

## N7 — final gate, independent integration review and handoff

Order work: cheap contracts → focused controls/mutants → one successful bounded
canary → final frozen gate. Use main's canonical `tools/test/run_selected.py`
with explicit source/build roots if your branch lacks it. Record the runner's
revision independently from the engine revision. Serialize expensive runs on
this shared Mac. Restore mutations and rebuild clean binaries before gating.

Run one Debug sim gate covering the affected production/harvest/transport,
resident/population, config/registry, persistence and replay paths; run affected
formats/loaders if changed. Run ASan/UBSan over those changed paths and lifecycle
cases plus the new integration coverage. Require new reference/CLI contracts by
exact name. Include real-file prerequisites; count selected, executed, failed,
empty, skipped and incomplete separately. Pick and disclose sensible per-test
timeouts before running. Preserve failed attempts; no silent retries or blanket
skip policy. Do not rerun unrelated sailing/render/heavy suites absent a real
dependency change. Reuse existing results only with an explicit dependency case.

Every substantive package requires an independent read-only reviewer, with
identity, source pin, exact reviewed scope, evidence and findings retained in
one ledger. The reviewer reads callers and raw artifacts, independently derives
decisive examples and checks the intended assertion caught each targeted fault.
Fix findings and obtain focused re-review. Do not duplicate a long campaign or
add a second review ceremony for already-closed unchanged facts.

The final integration reviewer checks package interactions, final source versus
evidence pins, conservation, persistence and all completion claims. After code
changes, rerun affected checks; document which earlier results remain valid.
Docs-only corrections do not require broad test repetition.

Report one consolidated result in `REPORTS.md`: code/gate/docs pins, package
table, reviewers, original evidence, adaptations, raw artifact hashes, exact
ledgers, gate counts, failed attempts, compatibility/Windows delta and limits.
Commit small concerns locally. Stop for orchestrator review when both chains
meet their scoped exit criteria, or when all unblocked work is complete and a
precise externally dependent remainder is documented. Do not call the entire
bundle complete with a missing chain or an unreviewed implementation.

## Boundaries throughout

No merge, rebase, push, TASKS edits, other-executor imports, shared coordinator
changes, active-map selection, original asset changes, or Windows work. Leave
the scripting/unit-import lane and ECON-BOOK-1 to their owners. Do not flip
`house_updates`, implement tax/upkeep, salt/mining, automatic planting, higher
population stages or a complete ship-start settlement. Preserve the original
game's mechanics; unresolved evidence is not permission to invent a rule.

Windows/Linux builds, original-runtime observations and GPU/live-window checks
are NOT_RUN unless actually performed by an authorized lane. This assignment
completes a reviewed headless gameplay candidate, not cross-platform acceptance.

Dispatch review: `/root/review_needs_dispatch` independently reviewed this packet,
the task-board delta and accepted-base pointers on 2026-11-05 against main
`768d7354` plus this docs change. Approved with no blocking findings; registry
filenames, the supply evidence pointer and the roadmap's old status were corrected.
