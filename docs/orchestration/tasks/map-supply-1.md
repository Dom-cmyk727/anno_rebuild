# MAP-SUPPLY-1 — recurring food and alcohol supply

**READY / RELEASED, 2026-11-05.** Mac executor, existing
`task/MAP-2-original-like` worktree; base `a50aec3a6b0f6bbeb90fee5525c757f2b58b4da0`.
Work through S0–S6 with independent package reviews and one consolidated final
handoff. Internal review approval releases the next dependent package; do not
stop for orchestrator approval after each small task.

## Outcome and ownership

Turn the successful stocked-town progression into a working supply chain:
resource harvesting/grazing → small-farm production → warehouse transport →
food stalls and Inn → several resident households. Prove shortage and recovery
and that delivered goods were newly produced, not merely imported stocks.

The first target is the shipped small farm, type 809 / UNIVERSAL: existing
production tests identify alcohol from potatoes and food from grass. **Verify
that mapping from original files and routines in S1 before relying on it.**
This is one data-selected behavior family, not permission for an 809-only engine
branch. Its production kernel and dispatch already exist; actual worker and
animal completion must be audited rather than inferred from kernel tests.

Keep income/tax/booking/upkeep out of this bundle: ECON-BOOK-1 is being researched
in parallel by the orchestrator. Do not change its checkout or files. Do not
touch Windows, rendering, coordinator/test_support or active map/candidate data.
Do not merge main, rewrite history, flip `house_updates`, or substitute a newer
Mac pin into Windows. The existing Windows dispatch alone controls its inputs.
Original assets remain read-only. No broad process kills or shared coordinator
changes. A missing optional original-runtime capture is NOT_RUN, not a reason
to invent a rule or halt unrelated static work.

## Read first

1. CLAUDE.md / applicable AGENTS.md, PLAN §0, the current TASKS.md dispatch and
   `docs/testing/evidence-contract.md` plus `docs/testing/README.md` from main.
2. `docs/orchestration/reviews/2026-11-05-map-pop-close2.md` and the latest MAP
   report; `docs/sim/population-live.md` §§23–26 and its review ledger.
3. `docs/re/production.md` small-farm/farm sections;
   `docs/sim/production.md`; `docs/re/forestry.md`, `docs/sim/forestry.md` including
   shared-job eligibility, first-delay, claims, lost/historical cargo and growth.
4. `docs/re/transport.md`, `docs/sim/transport.md` if present, and the relevant
   routing/figure/production sections of `docs/sim/state.md` and config/rules docs.
5. Actual callers: `src/sim/src/production.cpp`, `driver.cpp`, `scheduler.cpp`,
   `harvest.cpp`, `transport.cpp`, `resident_job.cpp`, `create.cpp`, `apply.cpp`;
   `production.hpp`, figure/building state and field lists; original-registry
   loaders. Use rg to locate moved files and precise SmallFarm/0x2a/0x2e branches.
6. `production_test.cpp`, harvest/forestry/transport tests,
   `population_supply_test.cpp`, `population_progression_test.cpp`, and
   `tutorial_town.hpp`. Reuse their valid mechanisms without inheriting unproved
   expected values or copying an entire expensive opening for every test.

Read decisive original binaries from the main checkout's `game/app` when absent
in this worktree. Record module hashes and exact bounded function ranges; never
confuse instructions beyond a return with the function under review.

## S0 — close the two bounded review findings

1. Guard both Upgrade transition pointers before dereference in the progression
   canary. Re-run the two legal cases and a missing-upgrade mutation/control.
   The negative case must fail the intended assertion, not crash or time out.
2. Correct the K1 comparison's executed/skip counts without rerunning it.
3. Record ASan as 274 executed + one opt-in skip. Update the stream note to say
   that 128 / timeline 5 became published at a50aec3a; retain the earlier gate-time
   history. No in-place incompatible amendment under that old exception.
4. Focused independent read-only re-review closes S0. No new full CLOSE-2 gate.

## S1 — trace and freeze the small-farm contract

1. Map the actual registry records: inputs/outputs, quantities, timing, capacity,
   worker/animal type, routes, counters, resource tiles and their growth cycle.
   Separate warehouse units, raw production units and figure cargo units.
2. Follow worker job 0x2a and animal job 0x2e from spawn through target selection,
   travel, harvest/graze, return, credit, release and destruction. Read switch,
   suspension, capacity and claim behavior, and original PRNG draw sites/order.
   Determine how the required crop/grass resources first appear and regenerate.
   Do not assume a generic Farm-home harvest job accepts a SmallFarm home.
3. Audit existing code against the trace. Mark each link implemented, incomplete
   or unknown. Reuse correct kernels, counters and transport, avoiding a second
   farm implementation. Define precise missing state and ownership invariants.
4. Commit independent hand-derived fixtures/reference before engine changes.
   Include zero/smallest-positive elapsed time, accumulated delay, full output,
   scarce resource, worker limit and a failed dispatch. For each decisive mask,
   width or timed predicate, give its primary address and a distinguishing case.
5. Write `docs/sim/small-farm-supply.md` and an acceptance table; an independent
   read-only reviewer must check original bytes/data, not merely the prose.
   Existing disclosed deterministic walk adaptations may be retained and named;
   no need to solve the whole fine-route engine. Do not label them original.

If a truly unresolved rule prevents one line, isolate it precisely and continue
the independently grounded line and tests. If the target actually requires a
larger untraced animal/planting system, return its exact bounded dependency rather
than substitute spawned cargo or claim the two-line loop complete. Do not spend
the whole bundle proving additional houses still consume starting stocks.

## S2 — complete the crop worker lifecycle

1. Extend the existing data-driven harvesting mechanism only where the approved
   contract requires SmallFarm support. Route goods to the correct raw-input
   slot; keep home ownership and counter release explicit and exactly once.
2. Implement verified initial resource creation/planting only if necessary for
   the selected public gameplay path. Derive type, extent, eligibility and costs
   from evidence; no invisible test-only crop seeding in a claimed real run.
3. Cover failed search, home destruction, end during work/return, resource
   replacement, full stock and simultaneous claim attempts. Validate and reload
   after each terminal path. Reuse the forester's corrected end path.
4. Prove potato/input-specific harvesting and delivery, plus a different goods/id
   and owner fixture using the same behavior. Pin exact debit/credit and draws.
5. Independent package review, focused fixes and re-review before S3 integration.

## S3 — complete grazing and two-line production

1. Implement the traced animal lifecycle and resource interaction, retaining the
   existing data-defined kind distribution and exact eligible-attempt draw order.
   Do not approximate a completed grazing trip by a timer that grants raw stock.
2. Connect each line to the correct input and output. Verify gate, suspension,
   output-full behavior, dispatch limits, refused sends and per-kind counters.
3. Test ending/removing the home with figures out, reloading mid-trip and partial
   delays. No leaked counter, stale claim, double credit or orphan-invalid save.
4. Preserve original defaults; new mod-relevant roles/ranges belong in validated
   facts or hashed config. No goods-id/type-id switches for this one scenario.
   A semantic two-line SmallFarm constraint may be explicit; do not silently
   truncate unsupported data. Bump stream/timeline only when the change requires
   it, and reproduce new fields with independent expected bytes.
5. Independent package review; correct the findings before broad canaries.

## S4 — prove warehouse delivery and supply to households

1. Use the existing cart/transport mechanism to collect both outputs. Check
   output slot choice, reservations versus actual stock, partial pickup, warehouse
   capacity and remaining cargo. Add only missing, traced behavior.
2. Select an original scenario with legal resources and space using a small
   documented inventory. Prefer a fresh public-command farm build where feasible;
   disclose an imported farm if that is the verified slice. Do not alter shipped
   assets, free resource grants, hidden relocation or scenario stage flags.
3. Supply at least three households via the real food stall and Inn carrier paths.
   Track the farm inputs/outputs, transport loads/deliveries, warehouse changes,
   Inn inventory, purchases, delivered/in-flight/lost goods and resident stocks.
   Count ended figures' cargo as history, not active stock; reservations are not
   extra goods. Assert every unexplained ledger term is zero.
4. Prove newly produced goods were needed: account for all initial producer,
   carrier and warehouse inventory and show production/receipt beyond that
   baseline, with a producer-disabled/removed comparison. A warehouse balance
   merely staying positive is insufficient. Use bounded observations rather than
   waiting hours for 50 t to exhaust when a local causal control answers the rule.
5. Show repeated resource/production cycles; if regeneration is unverified, claim
   finite production only and leave sustained supply open. Do not call a few
   deliveries a balanced infinite economy or guarantee an upgrade from two goods.

## S5 — shortage, recovery, isolation and persistence

1. Interrupt a real link with a supported public command (demolition/rebuild or a
   verified switch). Observe inventory draining and the intended consumer supply
   stopping; restore the link and observe actual production/delivery recovery.
   If a proposed switch command is absent, use an existing command rather than
   add a new UI/command family just for the test.
   If starting stocks mask the interruption, choose a low-stock original town
   or drain goods through supported public commands. Increased warehouse receipts
   alone do not prove that households depended on the renewed supply.
2. Test two owners using the same data-driven behavior in a focused world: each
   worker/cart/Inn credits its own city. Place a rival source closer to expose a
   missing ownership check. Retain exact single-owner ledger controls.
3. Reload at crop work, grazing, production/input balance, cart cargo and resident
   cargo boundaries. Compare each continuation with uninterrupted execution.
   A fresh replay process must match the genuine checkpoints; one changed legal
   command must diverge at a relevant checkpoint, with causal controls separate.
4. Measure visit/search work against increasing unrelated farms/cities using
   deterministic operation counters and modest Release samples. Fix newly
   introduced global scans if found. Record scope; no multiplayer-scale claim.
5. If existing APIs allow, add a short ship-founded settlement smoke that builds
   and receives the first new supply with its unchanged loadout. This is an
   extension, not permission to rerun a long sailing sequence for every package;
   exact resource/placement blockers are a useful bounded result.

## S6 — one closeout, independent review and final gates

Use one read-only reviewer per substantive package; it may not author the code
it reviews. Review callers and primary evidence, not only reference agreement.
The author fixes findings; focused re-review of the changed area is enough.
A fresh final reviewer checks interactions, ledger, identity and reuse arguments.

Before expensive work, inspect current CPU/memory/disk and competing jobs. Use
established host calibration, admission control and owned processes only.
Run cheap contracts/negative controls, focused cases, then a single real-chain
canary before the broad matrix. Register reference/runner public-command
contracts and require them by exact name in the canonical gate. No empty tests,
missing required game files or caught-but-unreported child errors.

Freeze the final source. Run affected Debug and ASan/UBSan cases selected by a
changed-file/caller coverage map, including scheduler, driver, types, building and
state-field tests where touched—not only names containing "farm". Run Debug
sim-all once and affected format/replay/reference checks. A full sim sanitizer
campaign is not required merely to repeat CLOSE-2; disclose exact coverage and
retain valid unaffected evidence. Preserve first failures and aborted attempts.
No source edits during gates. Record source/workflow/binary/fixture hashes and
selected/executed/failed/empty/skipped/incomplete counts separately.

Return one report with package verdicts, original versus adapted rules, raw
evidence index, code/docs/gate pins, exact successful loop and its limits, new
identity and Windows integration delta. Keep `house_updates` false by default.
Windows/Linux/original-runtime checks not run on this Mac stay NOT_RUN. Stop at
the reviewed local candidate; no merge, rebase, push or further gameplay scope
without the next orchestrator instruction. This packet authorizes S0–S6 work,
not a default flip, all farming families, citizens progression, economy booking
or a rendering rewrite.
