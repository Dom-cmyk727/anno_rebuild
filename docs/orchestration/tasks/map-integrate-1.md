# MAP-INTEGRATE-1 — combine accepted supply with scripting and original units

**READY / RELEASED, 2026-11-05. Owner: second Mac executor.**

## Outcome and exact authorization

Produce one reviewed headless integration candidate containing accepted
MAP-SUPPLY-1/CLOSE plus MAP-SCRIPT-1/2 and MAP-UNIT-IMPORT-1. Preserve their
behavior, reconcile their identities and prove that production workers, imported
actors, script initialization, construction permissions and replay coexist.
This is integration of completed work, not new gameplay research.

Use your existing owned checkout:
`/Users/dom/Dev/anno1503-worktrees/anno1503-worktrees/anno1503-MAP-BOOTSTRAP-1`.
Create **`task/MAP-INTEGRATE-1`** from your clean accepted UNIT1 head. Preserve
the UNIT1 and SCRIPT1 branch refs. No new worktree is required.

| Input | Exact pin | Accepted scope |
|---|---|---|
| Starting scripting/unit branch | `50aa32e38706ad3c2e4302e8fcaada9f555ba034` | Bounded original import, SCRIPT2 real area/wave and public opening; stream 130 / timeline 5 |
| Supply branch snapshot to integrate | `67158af4d29d87fed8686dc6542ca2a9f4b1f75c` | MAP-SUPPLY-1/CLOSE and corrections; stream 129 / timeline 5 |
| Verified common ancestor | `a50aec3a6b0f6bbeb90fee5525c757f2b58b4da0` | Published population-close baseline; stream 128 / timeline 5 |

This packet explicitly authorizes **one local merge of the exact supply pin
into the new integration branch**, conflict resolution, integration fixes,
tests and commits there. It does not authorize merging either active task branch,
merging main, rebasing, updating the preserved input refs, or publishing code.
Use commit hashes, never a moving MAP branch name, as merge inputs.

The first Mac is actively implementing MAP-NEEDS-2. Its newer commits and dirty
files, including its independent stream-130 work, are **excluded**. Do not copy
files from its working tree, build there, stop its jobs or ask it to pause for
this integration. Read accepted files with `git show <pin>:<path>`. Windows and
the orchestrator's ECON-BOOK lane retain their own assignments and inputs.

Proceed I0–I6, with independent package approvals releasing dependent work and
one final orchestrator handoff. No routine approval pause between packages.
An unresolved new gameplay policy blocks its dependent change, not unrelated
integration work. Do not resolve a contract conflict by silently choosing a side.

## Read before work

Read current main's `AGENTS.md`, `CLAUDE.md`, `docs/PLAN.md`, this dispatch in
`docs/orchestration/TASKS.md`, `docs/testing/evidence-contract.md` and
`docs/testing/README.md` using fetched objects if absent from your branch.
Read this packet in full; it supersedes historical no-merge instructions only
for the specific local integration authorized above.

From UNIT1 pin, read:

- `docs/orchestration/reviews/2026-11-05-map-script1.md`,
  `2026-11-05-map-script2.md` and `2026-11-05-map-unit-import1.md` in that directory.
- `docs/sim/script-runtime.md`, `docs/sim/original-unit-import.md`,
  `docs/re/original-unit-import.md`, `docs/sim/state.md` and relevant
  construction-permission/availability contracts linked by SCRIPT1.
- `src/sim/src/original_units.cpp`, `scenario.cpp`, `scenario_formats.cpp`,
  script/session/replay and permission callers; `src/sim_load/` loader entry
  points; `src/sim/tests/original_component.hpp` and the UNIT1/SCRIPT tests.

From supply pin, read:

- `docs/sim/small-farm-supply.md` §§1–15, `docs/sim/forestry.md`,
  `docs/sim/population-live.md`, and the relevant open questions.
- Main's `docs/orchestration/reviews/2026-11-05-map-supply1-close.md`.
  The accepted supply facts are there and in the supply spec; do not assume
  a report entry exists in the older branch's `REPORTS.md`.
- `driver.cpp`, `harvest.cpp`, `figure.cpp`, `invariants.cpp`, `scheduler.cpp`
  under `src/sim/src/`; related headers and small-farm/lifecycle/population tests.

For both inputs, inspect `world.hpp`, `state_fields.hpp`, `types.*`, Rules and
registry digests, `tools/sim_stream.py`, `fixtures/synthetic/sim/`, CMake test
registrations and the public-command/replay wrappers. Follow actual callers;
do not rely on the summary's file inventory as an exhaustive dependency list.

## I0 — freeze inputs and map semantic conflicts

1. Verify exact input objects, common ancestor and clean owned checkout. If the
   integration branch already exists, inspect it and resume only if it belongs
   to this assignment; never force-create over it. Record refs, source hashes
   and host CPU/RAM/disk/contention before building.
2. Create the new branch from UNIT1. Inventory both ranges from the common
   ancestor, including files changed on only one side whose callers changed on
   the other. Keep original artifacts/logs byte-identical at their input pins.
3. Write `docs/sim/gameplay-integration.md`: input table, conflict/interaction
   table, identity decision, required evidence and acceptance table. Record
   exact old and new responsibilities, not just textual conflict resolutions.
4. Establish the small set of required cross-feature proofs before running
   long cases. Reuse prior accepted baseline evidence; do not rebuild both old
   branches just to show that already-reviewed work used to pass.

Known overlapping files include `figure.hpp`, `figure.cpp`, `world.hpp`,
`invariants.cpp`, `scheduler.cpp`, CMake, configuration and native-state tests,
population fixtures, `lint_sim_allow.txt` and docs. Import source-slot handling
must coexist with animal worker lifecycle and counters. A conflict-free Git
merge is not evidence that these semantics agree.

Exit: independent read-only review approves the integration/identity contract.
Use an acceptance table with a legal control and the smallest counterexample
for each decisive interaction. No extra reviewer ceremony is required.

## I1 — local merge and explicit semantic resolution

On the new branch only, merge exact `67158af4...` with `--no-ff --no-commit`.
Resolve deliberately, then finish the local merge commit when the resolved
tree is coherent. Never apply wholesale ours/theirs to a conflicted subsystem.
Record intermediate failures as intermediate; final gates use a clean commit.

Preserve all of these:

- Strict normal actor import before INIT; unsupported required actors fail.
  Actor-free component/generated-map projection is an explicit caller choice,
  never a fallback after a failed normal load.
- Original actor slots, stable IDs, parents, crew slots, ordered cargo, health,
  fine positions, allocator hint/cursor, and the existing generated loadouts.
- Supply worker search/claims, same-worker effects, animal/raw input selection,
  timing, exactly-once release, public end/demolition and invariant checks.
- Script startup as a construction barrier; restored Ready sessions do not run
  INIT twice. Missing permission context never means unrestricted construction.
- Configurable data-defined types, roles, budgets and goods mappings. Do not
  bake original IDs or original registry sizes into an integration shortcut.
- `house_updates` remains false by default. Preserve faithful/default versus
  explicitly enhanced harvesting rules and each accepted adaptation.

Preserve both sets of test registrations and narrowly justified lint allowances;
do not expand an allowlist to hide new integration errors. Merge documentation
by subject and current status while retaining attributed historical evidence.
Keep TASKS owned by the orchestrator; this packet is read from main, not copied
over the branch's historical task board during conflict resolution.

Exit: independent diff/caller review confirms neither input's behavior was lost.
Focused fixes and re-review may continue in small local commits.

## I2 — new identity and preserved compatibility evidence

The combined candidate must have a **fresh state-stream identity, at least 131**,
distinct from both accepted inputs. Before recording anything, inspect existing
native/replay framing, schema/policy, rules/registry/content digests and the PRNG
timeline. Record whether existing timeline 5 remains justified; equal numbers
alone never establish compatibility. Do not change the command wire merely
because the merge changes no command, or leave it unchanged if its contract
actually changes.

The first Mac's independent numeric versions are not integration pins. This
packet does not reserve a version in its branch or declare two independently
numbered streams compatible. Record exact schema and provenance; if a current
supported reader can silently accept semantically incompatible input, correct
that boundary rather than relying on the filenames or branch labels.

- Preserve accepted 128/129/130 evidence as historical fixtures/artifacts.
  Keep provenance for predecessor logs before generating current-identity logs.
  Inventory both historical 129 lineages: supply's prior layout and SCRIPT1's
  script-schema layout. Equal numeric versions must not collapse them into one
  compatibility fixture or one claimed predecessor.
- Test refusal of the accepted predecessor saves/replays at the intended
  compatibility check. Repair outer framing/checksum where necessary so a
  damaged envelope cannot falsely satisfy a version-refusal test.
- Keep a valid current-identity control alongside each refusal. A changed live
  registry/rules/script digest must fail for its own reason, not an incidental
  earlier parse failure. Do not relabel old logs as newly executed evidence.
- Independently transcribe the combined fields/order/digests in the stream
  oracle. Compare meaningful populated script, actor and harvest state, not
  only an empty world. Explain every golden/log change; a C++-generated expected
  hash is a rebaseline, not an independent oracle.

Exit: focused compatibility/native/replay checks and independent review approve
the combined identity. Do this before expensive current-identity canaries.

## I3 — short cross-feature proof before full canaries

Build a small, explicitly authored integration fixture with the combined
runtime: a supported original-data type set, an imported or representative
parented ship/crew, an active farm worker and a supported script with permission
changes. Label fixture resources and script setup honestly; this fixture is not
an unchanged original scenario or an earned ship-start settlement.

Prove through normal session/command paths:

1. Pending initialization blocks construction; INIT runs once and supplies the
   expected permission state. A later allow/deny affects an otherwise legal,
   affordable command. Refusal leaves stocks/world unchanged; allowance still
   enforces terrain, ownership and cost. Use different owners as a control.
2. Script visits and farm dispatch/work/return coexist without duplicate draws,
   credits, actor-ID collisions, overwritten slot metadata or leaked home
   counters. Test a repeated public worker end and the appropriate claim rule.
3. Save/reload while both an active worker and initialized nontrivial script
   state exist, with the ship's nested cargo/crew intact. Compare continuation,
   events, transfers, PRNG state and final serialized world with uninterrupted
   execution. Include a pending INIT cut separately.
4. At least one targeted wrong interpretation at each changed integration seam
   fails its intended assertion with the legal control passing. Reuse existing
   suitable mutants; do not invent a mutation quota or a new test framework.

Exit: short focused Debug and sanitizer proofs, followed by independent review.
If these fail, isolate the cause before any long gate.

## I4 — retain both real-data achievements on the combined binary

Run the unchanged `endless01` public canary on the integrated source: source
ship/crew/holds, real area condition and wave event, sailing, founding, unload
and construction. No relocation, grants, stripped script or edited original
assets in the executed world. Planning copies remain explicitly separate.
Verify startup/permission effects, exact cargo/stock/gold accounting, actual
building placement, native cuts and fresh-process replay. Keep the denied-copy,
legal changed-command and changed-script controls distinct.

Also run accepted supply proofs on the combined binary: crop/alcohol delivery,
grazing food, household benefit and interruption/recovery. Keep the food ledger
scoped to the selected town's four farms, with both initial stores 256 and a
live control that buys food but produces no target-town small-farm food. The
accepted 1,183/756/237 figures are prior evidence, not numbers to force by
changing inputs. Explain any change through the integrated semantics first.

Some supply scenarios have unsupported actors/scripts. Preserve the already
approved, explicitly named component projection when that is their scope.
Declare it before loading; keep a separate normal-loader refusal control. Never
silently strip an original program or grant missing permissions to make a
production test green. An authored permission fixture must be labelled as such.

Check this concrete interaction cheaply in I0/I3: supply's `island7_town.hpp`
uses `tutorial_town::tutorial_world`; UNIT1's `tutorial_town.hpp` now initializes
the complete supported Tutorial script before commands. Establish its actual
permission for rebuilding farm 809 before the long resilience/replay run. If
original INIT legitimately denies a construction formerly assumed legal,
retain that refusal as original behavior and prove rebuild/recovery separately
in a clearly authored permitted scenario using the same public commands and
production code. Do not drop the recovery proof or call the authored permission
an original grant. Have the package reviewer approve that scope distinction.

These are **two original-data proofs plus a joint mechanics fixture**, not a
claim that an unchanged ship start now supports a complete balanced town.
Ship-founded alcohol feasibility, OQ-491 and new cloth/hunting work remain open.
New timestamps/hashes from the integrated identity are new artifacts. Reconcile
them with events and ledgers instead of blindly retaining old numeric goldens.

## I5 — frozen acceptance gates and bounded regression audit

Use the canonical gate from current main with explicit owned source/build
paths; record the runner revision separately. Select and declare required
reference/CLI contracts by exact registered name. Keep admission on, inspect
host contention, and serialize expensive runs rather than stopping other jobs.
Own only your own processes and any keep-awake assertion you create.

After cheap contracts and I3 pass, freeze one clean source. Run:

- Debug sim coverage including script, units, production/harvest/transport,
  permissions, config/registry, native state, session and replay. Include affected
  formats/loaders and all new tests; record the actual selection, not a guessed
  test count. A broad Debug sim gate is appropriate for the merged core.
- ASan/UBSan over affected shared files and changed call paths, including I3 and
  the real original-script and supply canaries. Distinguish affected coverage
  from a full-suite sanitizer claim.
- Independent reference/stream checks, changed-file formatting and cheap lint.
  Native Mac compile warnings are not Linux/MSVC target acceptance.

Integrate long canaries into the selected runs so the same expensive source and
configuration are not executed again just to produce a second headline. Reuse
unaffected old checks with a precise dependency rationale; do not rerun the
isolated long generated-map opening or heavy render corpus solely because a
version number changed. If an affected interaction warrants it, state that
reason and declare a measured timeout first. No timeout becomes a pass.

Record selected/executed/failed/empty/skipped/incomplete and sanitizer outcomes
separately, preserve first failures and source/build/executable/input identities.
Restore mutants and rebuild clean binaries before final gates. A late code
correction reruns affected checks; a prose-only change does not reset the matrix.

## I6 — independent integration closeout and future consumers

Every substantive package requires independent read-only review: frozen diff,
callers, tests, actual evidence and the smallest distinguishing examples.
Reviewers do not authorize gameplay adaptations or copy the implementer's
expected values. Fix actionable findings and obtain focused re-review; retain
reviewer IDs and pins in one ledger, including across context compaction.

A fresh final reviewer checks both input ranges and the merge resolutions,
identity/refusal boundaries, combined mechanics, real-data proofs and final
evidence attribution. Do not add an unrelated cleanup or optimization campaign.

Write one consolidated handoff in `docs/orchestration/REPORTS.md` and
`docs/orchestration/reviews/2026-11-05-map-integrate1.md`. Include:

- Both parent pins, actual merge base/commit and conflict decisions; code,
  test/gate, reporter and documentation identities.
- State/PRNG/wire/replay compatibility and which historical inputs are refused.
- Exact per-package outcomes, ledgers, artifact hashes, preserved failures,
  reviewer dispositions and NOT_RUN target-host/runtime checks.
- A concrete future integration delta for Windows and the first Mac. Explain
  how later MAP-NEEDS-2 should be reconciled from its accepted base; do not
  incorporate or retest its unreviewed branch and do not change Windows' pin.

Stop when the combined local candidate is reviewed and the required gates pass,
or report PARTIAL with a precise unresolved contract dependency. Do not call
conflict resolution alone complete. Preserve `task/MAP-INTEGRATE-1` locally;
**no push, main merge, rebase or other-lane integration** is released here.

No new script handlers, actor classes, disembarkation, combat, UI/audio, network
protocol, coordinator work, active-map selection or economy balancing. No
default House update flip. Future mod support is preserved through data/config
boundaries and explicit unsupported cases, not a new plugin system in this task.

Dispatch review: `/root/review_integration_dispatch` independently reviewed this
packet, the task-board delta, accepted pins and relevant interaction callers
against main `04f578f6` plus this docs change. Approved with no P1/P2 findings.
The Tutorial permission prerequisite and both historical 129 lineages are named
above to make the nonblocking execution pointers explicit.
