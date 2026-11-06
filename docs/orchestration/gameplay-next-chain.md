**Dispatch update, 2026-11-06 (after ECON acceptance):** ECON-BOOK-1 is accepted
on canonical main `303b3faf` / state 133. Second Mac MAP-EXPAND-1 E0–E7 is now released
through TASKS.md: two-island manual resupply using existing accepted mechanics.
First Mac retains cloth/hunting and its extraction follow-up; Windows retains
live settlement work. Town/industry/progression packages remain queued.

**Historical dispatch, 2026-11-06 (superseded above):** second Mac ECON-BOOK-1 B0–B7 was released on
consolidated main through TASKS.md. First Mac retains MAP-NEEDS-2. The remaining
queue below stays planning only; canonical baseline is state132, not the older
identities in historical planning paragraphs.

**Dispatch update, 2026-11-05:** MAP-SUPPLY-1/CLOSE is accepted within its
headless scope. MAP-NEEDS-2 alone is now released through
[tasks/map-needs-2.md](tasks/map-needs-2.md), starting at local `67158af4`.
The historical planning status below does not supersede that packet; other
queued work still needs an explicit dispatch.

# Next gameplay chain — evidence-led planning, 2026-11-05

**QUEUED / NOT RELEASED.** This identifies work after the current MAP-SUPPLY-1
and ECON-BOOK-1 packages. It changes no executor assignment, Windows input,
rules default, candidate layout or acceptance decision. Expand each selected
bundle into its reviewed implementation packet before release. No dates or
effort estimates are implied by the ordering.

## Outcome

Move from a stocked Tutorial house that upgrades to a settlement that supplies
its own early needs, earns income, pays upkeep, replenishes construction goods
and supports a second island. Only then make settlers→citizens the next full
progression claim. Existing kernels, a loaded building, and a passing canary
are useful dependencies; none alone proves that the player can build and run
the corresponding chain from the ship start.

Current prerequisites remain:

- MAP-SUPPLY-1: small-farm food/alcohol, real workers/animals and deliveries,
  several households, shortage and recovery. Accepted within the scope of the
  [SUPPLY/CLOSE review](reviews/2026-11-05-map-supply1-close.md); ship-founded
  alcohol feasibility and balanced economies remain open.
- ECON-BOOK-1: resident-sales booking and building upkeep. Arithmetic and a
  bounded input/ownership trace reviewed; lifecycle, overflow policy and engine
  integration unfinished. City costs must not be copied into player upkeep.
- Accepted wood harvesting/delivery, manual sailing/founding/cargo, placement,
  roads and opted-in population progression retain their recorded scopes.
- Windows continues its own live-opening work at its assigned pins. A new Mac
  package does not automatically update the window or approve a default flip.

## What the original data says

Read-only scan of original `Anlagen.dat` on 2026-11-05 using the existing
`tools/anlagen_roads.py::parse_anlagen` / `tools/figuren.py` record readers.
These are example shipped ids, **not engine allowlists**. They establish data
dependencies and named stage requirements, not all gameplay predicates.

| Example types | Original records | INFRAMIN | Planning implication |
|---|---|---|---|
| 809 UNIVERSAL | KARTOFFEL + GRAS → ALKOHOL + NAHRUNG | EVER | Current supply packet |
| 807 SCHAFFARM → 1001 HANDWERK | GRAS → WOLLE → STOFFE | EVER / EVER | Early cloth chain; audit whether current grazing work can be reused |
| 805 JAGDHAUS → 1023 HANDWERK | FLEISCHTIER → NAHRUNG + TIERHAEUTE; hides → LEDER | EVER / EVER | Early leather chain; Hunter is still inert at the reviewed source |
| 1203 MINE → 1022 HANDWERK | SALZSTEIN → SALZ | 1C / 1C | Salt needs extraction placement and a complete supply path |
| 1201 STEINBRUCH → 804 STEINMETZ | STEINE → ZIEGEL | 2B / 2B | Quarry/source and stone-worker lifecycle must be traced; do not invent a quarry tick |
| 1205 MINE → 1017 HANDWERK → 1019 HANDWERK | EISENERZ + HOLZ → EISEN; EISEN + HOLZ → WERKZEUG | 2C / 2C / 2C | Tools are a later construction-replenishment milestone |
| 819 PLANTAGE | BAUMWOLLE → WOLLE | 3A | Not an interchangeable early-game shortcut for sheep |
| 1035 / 1034 HANDWERK | HOLZ → HOLZKOHLE; EISENERZ + HOLZKOHLE → EISEN | 3D / 3F | Keep later charcoal route separate from the early wood route |

Data identities: Anlagen SHA-256
`b2317a5c24b8c9fe796cf40f3fedaefcfa30ad3f185df9e69b2dca6a03d8dd75`;
Data/BGruppen.dat SHA-256
`63e876fd3afedca214b3d9cc45f022f2c595f473839690e96f34e12ed7849e3b`.
No original file is copied into the repository.
The 16-row derived inventory is retained locally in
`cache/gameplay-next-chain/data-inventory.json` (ignored).

BGruppen's PIONIER rows list food/leather, plus cloth, salt, tavern and chapel
needs used by progression. Its SIEDLER rows add tobacco goods, spices, school
and church among the next-stage needs. The Force flags, unlock state and exact
upgrade formula matter; the list is not a claim that every good is equally
required at every tick. See `docs/sim/population-inputs.md` and the independently
tested population contract.

## Confirmed implementation gaps affecting order

1. **Build availability is not the same as resident-need availability.**
   `docs/re/population.md` §18(i) / OQ-464 records that BuildCmd does not check
   INFRAMIN. Direct record reads above show distinct early/later requirements.
   Trace the original availability reader and its player/city context before
   defining a new command rule. Modern authoritative enforcement may be an
   adaptation; loaded scenario buildings must not be rejected just because a
   player could not currently build another one.
2. **Mine/quarry placement is not open through ordinary BuildCmd.** Sample types
   1201/1203/1205 have HEAD flags `0x3096`. Current `ship-placement.md` R3 supports
   ordinary land/coast and refuses the coverage bit and other classes. Existing
   mine production tests do not close mountain attachment, placement, deposit
   ownership or delivery from a player-built mine.
3. **Leather is not already renewable.** `types.cpp::original_class_table` maps
   Hunter to TickKind::None / Inert at a50aec3a. The generic workshop's ability
   to consume hides does not create a functioning hunter or wildlife source.
4. **A quarry is not necessarily an active producer.** The class table and
   `state.md` Appendix C give it a stub tick. Trace how the stone worker and
   source interact before deciding whether stones enter a warehouse at all.
5. **Starting money/materials can hide incomplete gameplay.** A loaded town with
   50 t of goods can grow while production does nothing. Every later bundle
   needs a causal control and a ledger that distinguishes imports from output.

## Proposed order and manageable bundles

### 1. MAP-BOOTSTRAP-1 — availability and the real starting budget

- Read INFRAMIN/research availability consumers and build-menu/command context.
  Inventory current validation, data fields and scene-import exceptions.
- Make a small original-data table of minimum buildings/materials/stages needed
  for the first food, alcohol, cloth, leather and salt paths. Use the unchanged
  ship loadout and original costs; report a circular dependency explicitly.
- Design validated availability facts and authoritatively checked player-build
  intent without hardcoded type ids. Keep menu convenience, simulation rules
  and scene import separate; label any stronger network check as an adaptation.
- Test locked refusal with unchanged world/payment, exact unlock boundary,
  alternate-owner rules, data-defined roles and deterministic replay. Original
  scenario import must still accept legitimate later-stage buildings.
- **Exit:** the startup path and missing dependencies are known; a chain cannot
  be declared playable by bypassing its original unlock requirement.

This is the recommended next small enabling unit, not a new UI project. Its
contract can be prepared before the supply packet finishes, but engine edits
wait for an explicit dispatch and a compatible accepted base.

### 2. MAP-NEEDS-2 — renewable early cloth and leather

- Cloth package: sheep resource/animal job and wool credit, then existing weaver
  input acquisition/production and warehouse/stall delivery. Reuse small-farm
  grazing only where the traced contracts actually match.
- Leather package: hunter target selection, animal/claim lifecycle, travel,
  return, two outputs and cleanup; then tannery and stall supply. Do not infer
  wildlife renewal from a loaded animal or turn this into combat implementation.
- Shared package: several households with isolated owners, interruption and
  recovery, full-output/failed-send/home-removal cases and saved in-flight jobs.
- **Exit:** newly produced cloth and leather reach consumers beyond initial
  inventory, with no hidden grants, double credits or unaccounted cargo. If the
  original resource pool is finite, distinguish repeated production from an
  indefinitely renewable economy.

### 3. MAP-EXTRACT-1 — legal salt extraction and mineral placement

- Trace the mountain/coverage placement arm, source matching, terrain height,
  footprint replacement, ownership, payment and removal. Write independent
  fixtures before replacing the explicit Unsupported result.
- Implement the minimum reusable placement and deposit/source contract for a
  player-built salt mine; retain existing ordinary/coastal behavior.
- Connect extraction and actual transport to salt production and the food/salt
  stall; cover wrong mineral, missing source, exhausted source, full warehouse,
  rival owner, removal and reload.
- **Exit:** a legal stage-1C salt chain supplies households from original source
  material. The same contract must name which iron/quarry cases remain unproved;
  do not claim all mining complete from one working salt mine.

Cloth/leather and extraction are independent research lanes; their engine
integration must be serialized where shared state/identity files overlap.

### 4. MAP-TOWN-1 — earned settler town from a ship start

- Combine accepted supply, early needs, salt, wood, availability and ECON-BOOK
  pins. Establish one reproducible legal build order within the unchanged
  loadout, natural island resources and reviewed founding rules.
- Observe actual resident consumption, revenue and upkeep; build a small group
  of houses and earn the relevant stage without injecting flags or residents.
- Show a real pioneer→settler transition, construction/material debit, shortage
  and recovery. Reconcile money and goods independently. Replay a save spanning
  harvesting, purchase, booking and upgrade.
- **Exit:** one supported opening reaches a functioning settler town; report
  finite starting tools and unrenewed needs honestly. This is not yet proof of
  all maps, every spawn, long-term economic balance or a default House flip.
- Windows may subsequently expose the accepted pin and the resulting production
  diagnostics through its existing command/state adapters, as its own reviewed
  task. Avoid requiring a complete polished UI before this can be played.

### 5. MAP-INDUSTRY-1 — replace finite construction supplies

- At verified 2B/2C prerequisites, extend extraction to the relevant quarry and
  ore paths, then brick/iron/tool production and inter-building transport.
- Audit the actual stone-worker mechanism; do not add an imagined quarry output
  tick because the WARE record looks like a workshop's output.
- Prove newly made tools/wood/bricks fund another legal building after accounting
  for the initial cargo. Use the early wood-based smelting path unless the later
  charcoal route is independently unlocked and in scope.
- **Exit:** construction replenishes itself through the tested industry chain;
  stalled inputs/storage and destruction recover without corrupting reservations.

### 6. MAP-EXPAND-1 — a second island and manual resupply

- Compose existing public load→sail→found→unload→return commands with goods
  produced by the first settlement; no teleports or test-only resources.
- Verify a second city's ownership, stocks, costs and worker homes stay distinct,
  including a closer rival warehouse and save/reload during the voyage.
- Preserve natural resource differences. Players start with ships and may race
  for islands; shared islands remain possible, never compulsory. Initial peace,
  shared visibility and no alliances remain the owner's chosen start policy.
- **Exit:** one player supports two islands manually. Automated trade schedules,
  foreign trade, combat and new diplomacy are separate later work.

This package can move earlier if the real opening audit shows a necessary resource
is on another island and existing manual cargo primitives can resolve it. Do not
force a fixed sequence by making every start island self-sufficient.

### 7. MAP-POP-2 — settlers to citizens from actual supplies

- Read the next group's exact needs, availability and material costs; close
  tobacco/spice production, school/church service or other dependencies actually
  required by the chosen original path. Do not extrapolate the first upgrade.
- Add a multi-house positive case and one-predicate refusals at the deciding tick,
  with services, new supplies, exact debit, counters, draw order and persistence.
- **Exit:** demonstrated second-stage progression from supported production or
  manual imports, not a pre-stocked scenario relabelled as a complete economy.

## Cross-cutting acceptance and parallel work

Each bundle should contain several small implementation packages, independent
package reviews, focused corrections and one final orchestrator handoff. Start
with cheap semantic controls; broad tests run once on frozen final source.
Unknown predicates stay explicit. Reuse valid upstream evidence without turning
every integration into another sailing/corpus campaign.

Every new behavior must remain data-driven and deterministic, with appropriate
state/content/PRNG identity changes now that stream 128/timeline 5 is published.
Use alternate goods/types and another owner to test generic mechanisms. Keep
cost proportional to active/local work. Add operation counters and a modest
multi-city performance case when a new search/scheduler path appears; a full
late-game multiplayer workload needs a later combined performance gate.

The live Windows lane and eventual multi-peer determinism tests are separate
consumers of accepted gameplay pins. Parallel research can cover different
chains, but shared engine files and version changes need one integration owner.
Do not launch a mod SDK, general AI, military or automated trade-route project
merely because the underlying data contains those types. Preserve extension
points while finishing a playable economic sequence.

## Release decisions still needed

This is a planning queue, not executor authorization. At the next checkpoint:
1. Read the actual MAP-SUPPLY-1 result and ECON-BOOK contract status.
2. Freeze the next accepted integration base and choose the first bundle above.
3. Audit unlock/material/resource dependencies before declaring its opening
   achievable; resequence expansion if necessary, without altering map fairness.
4. Publish a concrete packet with file ownership, primary-evidence requirements,
   package reviewers, gates and stopping criteria. Keep current work running.

## Planning review

Independent read-only reviewer `/root/review_pop_close2` reread the selected
original records and hashes, BGruppen needs and the scoped implementation gaps.
Verdict: approved as queued planning, no blocking findings. This is neither an
engine gate nor authorization to start these bundles. Runtime feasibility,
complete bootstrap costs and the original availability consumer remain work for
the first contract, not conclusions of this inventory.
