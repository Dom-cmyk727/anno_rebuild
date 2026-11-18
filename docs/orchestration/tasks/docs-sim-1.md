# DOCS-SIM-1 — current simulation capability inventory with drift checks

Wave 1.8. PREPARED, NOT RELEASED. The Mac orchestrator releases this only after
SIM-LIMITS-1 is independently reviewed and its exact-head hosted CI succeeds.
EXECUTOR II MAC, one bounded 3–4 hour packet. No new gameplay implementation.

## Start and preserve

The release message will name the accepted SIM-LIMITS-1 full source head. Create
`task/DOCS-SIM-1` there at an owned clean checkpoint; do not substitute moving main,
merge the board into source, or rebase any preserved branch. This is a candidate
base, not a main landing. Read this exact packet from the named `orch/mac-lane`
commit. Ordinary-push only your task branch, never main or force. The orchestrator
provides independent read-only review; no executor reviewer subagents.

Read CLAUDE.md, AGENTS.md, docs/PLAN.md goals, TASKS.md owner rules, the Mac lane
handover, docs/testing/evidence-contract.md and docs/testing/README.md. Then read:

- docs/sim/core.md, state.md, rules-presets.md, the relevant command/production
  contracts, and the current accepted SIM-LIMITS-1 report;
- src/sim/include/anno/sim/{config,command,types,building,driver,scheduler,world,
  seams,state_fields}.hpp and the source files defining Rules validation/digest, command
  decode/apply, class-table construction, building dispatch and World advance;
- the production preset/loader consumers that actually select rules, plus existing
  config, command-codec, driver-class and scheduler tests;
- tools/lint_sim.py, existing small source-inventory contract tools, and the CMake
  registration that CI actually executes. Reuse established mechanisms where useful.

Keep private cache/docs-sim-1/CURRENT.md with source/board refs, progress, next check,
owned jobs and evidence. Reread it and this packet after compaction. Preserve earlier
branches/evidence; no old gates are restarted merely because their reports appear.

## D0 — define the finite inventory and acceptance table

Write one compact acceptance table before implementation. Inventory exactly four
categories, from executable declarations/callers rather than stale prose:

1. Rules: every direct Rules member; expand the nested rule structs it owns into
   qualified leaf paths, recording shared nested types once. Distinguish behavior
   switches, enum choices, limits and data. Record declared default, supported or
   reserved values, validation constraints, production reader and persistence role.
2. Commands: every Body alternative and wire mapping, including LegacyRawCmd and
   any deliberately unsupported paths. Record actual apply handler, caller-visible
   effect/refusal boundary and production UI availability separately.
3. Building runtime classes: every RuntimeClass row, payload selection, TickKind,
   actual dispatch, optional gates and inert/unported behavior. Follow overrides
   outside the class table: a None tick does not prove the whole class does nothing.
4. Hooks/phases: every declared Seams member, plus the named top-level scheduler
   phases. Include command/placement hooks as well as advance/figure/building paths;
   on_placed, on_removed, placement_rule, upgrade_rewrite and city_is_empty must not
   vanish because they are outside advance. State which built-in behavior is live,
   conditional, intentionally empty or unported, with exact caller references.

Record inclusion boundaries and exclusions explicitly. Do not promise a general
C++ analyzer or a complete inventory of every TODO in the repository. No original
asset tables, figures, binary listings or host paths belong in the committed inventory.

## D1 — establish a readable current capability document

Create docs/sim/capabilities.md (or reuse an existing equivalent if one is found),
with these four tables and a short plain-language explanation: which pieces of an
opening/town can run now, which require explicit rules, and which are absent. Link
source symbols, focused existing proof cases and the relevant deeper contracts.

Separate four facts: code exists; the production call path reaches it; a rule/preset
allows it; a particular acceptance scope has demonstrated it. A green unit test is
not live-window, original-runtime or balanced-economy proof. Rules{} defaults and
explicit living/test/loader presets must not be merged into a single default column.
A test seam is not production support. Unknowns stay unknown; no new original-game
research or policy decision is authorized by this documentation packet.

Use meaningful statuses such as active, rule-gated, inert, reserved/refused, test-only
and unverified. Cite the real caller when correcting stale comments such as an old
House no-op claim. Make only narrow comment/doc corrections reached by this inventory;
link historical reports without rewriting their dated acceptance claims. Existing
rules-presets.md owner decisions remain decisions, not inferred implementation facts.

## D2 — add a small source-to-document drift check

Add the smallest maintainable check that reconciles the inventory with independent
source declarations: rules/member paths and normalized initializer expressions, command
variants/wire mappings, class/payload/tick mappings and every Seams member plus the named
phase surface. If the table prints a resolved constant value, explicitly track its referenced
constant definition too; do not implement arbitrary C++ expression evaluation. Structured
rows or a small companion manifest are fine; do not create another unchecked duplicate.

A bounded source extractor may support the repository's actual syntax, but it must
fail clearly on an unreadable required file, empty result, duplicate identity, missing
mapping or unsupported syntax in its declared scope. Ignore comments safely; commented
out declarations must not become capabilities. Never silently drop a new declaration
because it does not fit a regular expression. Keep output stable across paths/platforms.
Do not hash whole source files and call that semantic coverage. A generated table that
is merely compared with itself does not establish its source correspondence.

Mechanical checks do not prove the prose's fidelity or that a dispatch body works.
Document exactly which columns are extracted/checked and which are independently
reviewed semantic descriptions. Production reachability, rule-gating and descriptive semantics require independent
review against exact source anchors; declaration checks do not prove them. A narrow
fingerprint may require a human to revisit such a row, but is only a review trigger,
not semantic verification. Do not use unrestricted source-hash refreshes.

Check mode is read-only and returns nonzero on drift with actionable diagnostics.
An optional explicit write mode must fail if the destination cannot be written, and
must never run implicitly in tests/CI. No new production reflection framework, runtime
metadata API, state field or game behavior is needed.

## D3 — prove the checker catches wrong documentation

Use isolated synthetic source/doc copies or temporary patches with exact restoration;
never mutate another checkout or tracked source during a running gate. Legal control
first, then one fault per check, then restored control. At minimum distinguish:

- missing and extra inventory row, and a duplicate row;
- a new Rules field or changed declared default not reflected in the table;
- an added command alternative or changed wire mapping without an inventory update;
- a changed class-to-tick/payload mapping and an omitted hook from the defined surface;
- a missing/empty required input and an unsupported declaration in a scanned block.

The intended semantic comparison/diagnostic must fail, not an earlier syntax/import
error. Exercise the actual public checker command and exit status, not only internal
helpers; include a correct relocated temporary checkout to catch CWD/path assumptions.
Demonstrate an omission in the real inventory too. Report what the checks do not catch.
Do not force a broad mutation campaign for prose or add tautological tests for every row.

## D4 — registration, focused gate, review handoff

Register the check and its contract in the existing CTest/CI path using the smallest
CMake change. Prove discovery includes both names and canonical selection executes
them, with zero failed/empty/incomplete/unknown skips. Run the tool directly with
Python -I as supported by the existing project conventions. Run the existing relevant
lint/reference checks read-only; zero golden, fixture, recording or pin changes.

Only tools/docs/comments/test registration should change. No C++ engine/runtime,
state/section/PRNG/wire/default changes. With that scope, no broad C++/ASan campaign,
full-town opening, GPU run or original-file corpus is required locally. Exact-head
hosted CI remains required; unavailable local target coverage is NOT_RUN, not implied.
If the discovery exposes an engine defect, document and report it without expanding
this task into implementation or blocking unrelated completed inventory work.

Before handoff, independently reconcile all four category counts against their source
sets and inspect the diff for accidentally copied original data or host paths. Freeze,
ordinary-push the task branch, and send READY FOR REVIEW to the Mac orchestrator in
Orchestrator II Mac (owner-authorized reporting). Include full head/tree, CI link,
acceptance table, exact executed/failed/empty/skipped/incomplete counts, checker scope,
negative-control evidence hashes, retained failures and limitations. Raw evidence stays
in cache/docs-sim-1; commit only compact summaries and a bottom REPORTS.md entry with
decisions/reasons. Do not edit TASKS.md, STATUS.md or mac-lane.md.

Stop at review. Complete means a useful current inventory, a non-vacuous registered
source/document drift check and exact-head green CI, plus orchestrator independent
review. It does not mean missing gameplay has been implemented or defaults enabled.
The next gameplay implementation is assigned separately after this checkpoint.
