# ECON-BOOK-1: resident sales and building upkeep

**Dispatch update, 2026-11-06:** research and engine completion are assigned to the
second Mac by `docs/orchestration/TASKS.md`, section
“Second Mac — ECON-BOOK-1: earned income and building upkeep”, packages B0–B7.
That packet supersedes the historical research-only ownership, old base and
pre-132 identity guidance below. Engine work follows independent contract review;
this note does not declare the remaining lifecycle/policy research complete.

Current execution: B0–B5 independently approved; B6 scaling/consumer work on
`task/ECON-BOOK-1-main`, initially pinned to
`3b74201f8a5eafedcb8cd928bc52e191b3b5908c`; production booking/lifecycle wiring approved at ff095351.
The second Mac owns B0–B7 under the dispatch above. Historical research/ownership
and prior identity language below retain their actual dates and source pins.
Existing rules remain unchanged; booking will be separately default-off.

The player-facing outcome is a town whose purchases earn money and whose
buildings cost money to maintain. For example, two groups of resident purchases
can yield 88 gold at a booking visit, while combined building costs of 28 yield
an upkeep charge of 1. Those figures are arithmetic examples, not a prediction
of the current opening scenario.

## Boundaries and reading order

Read CLAUDE.md, PLAN §0, the shared `docs/testing/evidence-contract.md`, then
`docs/re/population.md` §8 and §11, `docs/sim/population-live.md` including its
current policy decisions, `docs/sim/state.md`, and `docs/sim/config.md` if present.
Inspect City, Rules, goods parsing, MoneyDelta application, island scheduling,
building creation/suspension/removal, and their save/replay callers before
proposing an engine change. Locate those symbols with rg; do not assume that
their paths or signatures stayed fixed during the population executor's work.

The population executor retains ownership of Inn carriers, market supply,
progression canaries, population closeout and their gates. Windows keeps its
fixed input. This package currently writes only new research/reference files in
`orchestrator-economy-booking`, an isolated local clone. The managed worktree API
could not create a worktree from the enclosing non-repository workspace, so a
local shared clone was used; no existing worktree, index or job was changed.
Its origin is the local source repository, not GitHub: do not push there.

## A. Booking arithmetic research — in progress

Original module: AnnoWelt.dll, SHA-256
`f220a878510f740c4cfb7224ade0a260e9a0fd7e99113e1201e7a37c0dfa6ae2`.
The orchestrator directly read `0x10029950..0x10029aa4` using the existing private
Capstone environment. Raw output is in ignored
`cache/economy-booking/booking-disassembly.txt`; no original bytes are committed.
Player money add/subtract routines `0x1001cfd0` and `0x1001cfe0` were also read;
their dispatch-slot mapping still needs independent verification.

| Predicate | Primary instructions | Distinguishing hand-derived check |
|---|---|---|
| Visit due after 2000 elapsed word ticks | 0x10029951–61 | 1999 does nothing; 2000 books; wrap from 65000 to 1464 is due |
| Price and surcharge are unsigned words | 0x100299bc–cd | price 65535, billed 1 yields 256, not signed-price arithmetic |
| Sum products, then round once | 0x100299db–fc | two products of 128 yield 1, not 2 |
| Signed truncation after wrapped sum +128 | 0x100299e6–fc | 65535 squared gives income -511, not arithmetic-shift -512 |
| Clear each billed word | 0x100299d5 | booked rows become zero; early return preserves them |
| Decay sales/trade statistics by 104/105 | 0x10029a05–85 | sales 105 and income 88 yield 192; trades 210 yield 208 |
| Combine upkeep before unsigned divide by 15 | 0x10029a57–93 | civil 14 + military 14 yields 1, not 0 |
| Set timestamp to now, no catch-up loop | 0x10029a96–9d | visit at 6001 stamps 6001 and a repeat is not due |

`tools/booking_ref.py` is a pure research oracle for the original word/dword
arithmetic. It accepts already-resolved goods rows; it does not verify which
goods or price columns should be supplied. The original loops 15 entries from
goods id 26, with record stride 26 and billed stride 12. Those are original
format facts, not modern engine limits. The climate/tax meanings of the getter
results are inherited hypotheses pending caller/getter verification.

Original low-dword overflow is deliberately represented in this oracle. It is
NOT approved engine policy. No C++ integration, original runtime capture,
cross-platform check or canonical acceptance gate is claimed by Python unit
tests. The original clears counters during the loop; modern transactional
validation and wider arithmetic must be specified as adaptations before use.

## B. Finish the contract and data mapping

1. Resolve the world property 10 and island property 7 getters, their valid
   ranges, the goods price/surcharge fields and player money vtable entries.
2. Trace upkeep source accounting: placement, suspension/resume, upgrade,
   demolition and load. Determine when totals are updated and whether city
   booking runs before or after building/house visits.
3. Inventory existing parsed fields and saved billed counters. Reuse them;
   do not create a second source of truth or silently reset imported debt.
   At ec0e832e, City::billed is already a saved u32 vector, while Preis/Bonus
   are not parsed by goods_metadata. `player.cpp::book_upkeep` already charges
   player upkeep_a/upkeep_b/rem. Trace how those relate to city building costs
   and prove that integrating city booking cannot charge the same cost twice.
4. Specify configurable consumer roles and price columns, supported mod ranges,
   arithmetic overflow outcomes, deterministic ordering and default-off rollout.
   Keep original rules, approved adaptations and unknowns separate.
5. Independent read-only review must derive decisive examples from original
   bytes and callers. Stop only the affected rule if evidence remains unresolved;
   continue independent work. No guessed tax command or global default flip.

## C. Implement validated facts and persisted state

After B approval and a reviewed population dependency pin: import the verified
facts, validate dimensions and ranges, and extend hashed Rules/registry identity
only where necessary. Make runtime collections data-sized. Persist booking
timestamp, pending bill amounts and any authoritative accounting state with
independent expected-byte tests. Resolve the actual stream version at integration
time; do not pre-allocate or amend another executor's version. Reject incompatible
identities clearly and check save/reload before wiring a tick.

## D. Integrate deterministic booking and upkeep

1. Calculate one city's booking into a typed result before mutation; make
   invalid configuration/overflow explicit under the reviewed policy.
2. Connect resident purchases to pending billed amounts exactly once, and apply
   income, upkeep and resets in the reviewed scheduler order.
3. Wire every upkeep lifecycle transition without scanning all buildings each
   tick. If a scan is required for load, keep it outside the hot path.
4. Test two owners/cities, zero and boundary visits, suspension, upgrade/removal,
   negative balances, duplicate visits and save/reload between purchase and
   booking. Prove that default-off behavior remains unchanged.

## E. Prove an actual economic loop

Use an unchanged original scenario with resident purchases and known upkeep.
Show production/delivery, purchase, pending bill, money booking and upkeep as
separate ledger entries. Assert unexplained terms are zero. Show a second owner
does not receive the first city's income. Include a controlled shortage and
recovery without test-only grants in the accepted real-data canary. A focused
synthetic case may supply exact boundary conditions separately.

Reload before booking must equal uninterrupted execution; a fresh-process replay
must match checkpoints, and a changed legal purchase/booking input must diverge
at the expected boundary. Do not require replay timing or counts to match an
older binary with different rules. Measure visit work against unrelated-city
size; this is bounded-cost evidence, not a multiplayer performance claim.

## F. Review, gate once and hand off

Each A–E package gets one independent read-only review and focused correction
review where needed. Use legal controls and a targeted wrong interpretation for
decisive arithmetic/accounting checks before broad tests. Freeze the final source,
use the canonical gate with explicit source/build paths, and record selected,
executed, failed, skipped, empty and incomplete separately. Run relevant Debug,
ASan/UBSan and reference/stream cases, then one real loop/replay. Reuse unaffected
long gates by dependency; do not repeat sailing for a pure arithmetic correction.

Stop this bundle when the reviewed opt-in headless income/upkeep loop, identity,
ledger and replay are proved, and the Windows API delta and NOT_RUN lanes are
documented. No renderer, trading routes, diplomacy, tax UI, default flip or
unrelated coordinator work is included. Report complete packages and remaining
dependencies honestly rather than requesting orchestrator review after each
small implementation step.

## Research checkpoint, 2026-11-04

Independent reviewer `/root/review_booking_research` directly decoded the
original bytes, checked the two unsigned division sequences on 10,009 inputs,
reran the first 11 unit tests and inspected all six negative-control logs.
Verdict: approved as an initial arithmetic research component, no P1/P2.
The reviewer requested explicit statistics-product and upkeep-sum overflow
examples before A could be completed; those were added as two focused tests.
Their focused re-review approved both additions: 13/13 tests pass, and the two
matching widening mutants each fail only their intended new boundary assertion.
Together with the earlier six controls, eight wrong implementations are caught.
Package A and the full contract remain in progress; this approves only the
initial arithmetic component. The source identity and tests' SHA-256 values are
in `cache/economy-booking/boundary-checks.json`.

Research command: `python3 -I tools/tests/booking_ref_test.py -v`. This is a cheap
component check, not a canonical project gate. The control/mutation logs and
source hashes are retained under ignored `cache/economy-booking/`. No C++,
registry, state format or engine tests were changed or run. No original runtime,
Windows/Linux, GPU or scale result is claimed. No source job or keep-awake
assertion owned by another executor was touched.

## B checkpoint and implementation shape, 2026-11-05

The primary-evidence follow-up is `docs/re/economy-booking.md`. It resolves the
money dispatch slots and confirms the scheduling position and distinct city /
player accounting. The price loader's actual unchecked writes make malformed
input validation a required adaptation. B remains in progress: upkeep lifecycle,
all import paths and the modern arithmetic policy are not yet closed.

Reuse before adding data:

- `WorldSettings::prop10` already persists the validated tax index; no new tax
  field or command is needed for the first loop.
- `Island::climate` already exists; verify original source mapping and use it.
- `BuildingType::running_cost` already parses and digests inactive/active costs;
  expose verified selection rules instead of parsing a second table.
- `City::billed` is already a persisted u32 vector. Original u16 wrap is research
  evidence, not permission to narrow that accepted representation.
- Player upkeep already exists and has its own remainder. City booking has no
  such remainder. Never mirror city running costs into player upkeep fields.

Complete B with these small subunits, each independently reviewable:

**B1: input decoding.** Verify scenario climate/tax source fields and complete
ordered Preis/Bonus/Malus/PreisLevel semantics. Fixtures must distinguish an
ordered reset from accumulation, two successive bonuses, repeated tax lists,
unknown climate, extra columns, and a mod registry beyond original sizes.
Reject malformed input transactionally, with the offending field named. The
runtime registry stores resolved prices indexed by configured climate/tax roles;
original fixed layout dimensions belong only to the source decoder.

**B2: lifecycle.** Trace and table each building cost contribution at create,
load, switch, upgrade, remove and owner transfer. Use the existing type costs;
resolve stat-kind meanings without assuming every class has identical rules.
Specify exactly which transitions replace a contribution and which do nothing.
Prove a duplicate switch/remove cannot debit twice. Keep figure/player accounting
separate; a city with costs 28 and a player with costs 14 plus remainder 1 yields
charges 1 and 1, with each charged once. Resolve remaining primary callees before
implementing their transitions.

**B3: modern contract.** Propose a checked widened calculation with a typed fault
before any bill reset or money effect on invalid input/overflow, following the
existing deterministic effects model. Set an explicit policy for signed prices
and for balances near i64 limits. Compare ordinary supported inputs against the
original oracle; label wider-domain differences as adaptations. Do not silently
choose original wrapping, saturation, a new tax UI, or a house_updates default
flip. Keep the income/upkeep feature separately controllable until accepted.

**B4: state/order acceptance table.** Record the booking stamp and authoritative
statistics needed for future visits, expected byte order, hash scope, effect
merge ordering, and original-import versus modern-reload rules. Changes after
MAP's now-pushed stream-128 pin must use a new compatible identity; the prior
unpublished-amendment exception does not apply. Decide the actual version at
integration, not in this research branch. A purchase before a due booking is
charged once; one after booking remains owed; reload at either boundary preserves
that distinction.

Only after B1–B4 review should C/D engine work begin. C/D can be handed over as a
single implementation bundle with internal package reviews, then E's loop/replay
and F's final gate. This checkpoint does not authorize engine changes or claim
those dependencies complete.

B checkpoint review: `/root/econ_booking_contract/review_booking_b` independently
read the named original dispatch/price/accounting/scheduler bytes, checked the
existing fields and reran 13/13 reference tests. Verdict: approved as a bounded
static-research checkpoint, no P1/P2. Its sole provenance correction (recording
AnnoGame and AnnoScene SHA-256 identities) was applied and focused re-review
approved it and the B1–B4 plan. This is not completion of B or approval to
integrate the engine. No source, tests, state identity, shared TASKS/REPORTS or
other executor worktree changed in this checkpoint.


## B0 — execution baseline and dependency ledger (2026-11-06)

Exact source `3b74201f8a5eafedcb8cd928bc52e191b3b5908c` is documentation-only
relative to accepted code `7b57ebd968445bdb8ab8f0ad3e565632a7df24ee`.
Branch task/ECON-BOOK-1-main was created at that pin in the owned MAP-BOOTSTRAP-1
checkout, which was clean at completed MAP-INTEGRATE-1 head5ef163a6. That branch,
its old build/debug, build/asan and cache/integrate1 evidence remain preserved;
ECON builds will use separate build/econ-* directories. The completed adoption
branch/worktree0648bcab also remains unchanged. No old branch merge is needed.
No runtime diff, other-lane implementation or assets have been copied.

Current132/SIMINT1/timeline5/frame4/wire1 remains untouched in B0. Dispatch
reserves133/ECONBK1 for this candidate; exact bytes and independent state/refusal
proof belong to B3. No competing current allocation appears in the pinned
canonical dispatch. Eventual combined identity remains orchestrator-owned.
No PRNG/wire change is justified merely by booking. No new package or merge for
the first Mac or Windows is released by this executor.

Baseline receipts: ignored cache/econ-book1/B0/baseline.json, preflight-01.log,
booking-reference-01.log and the pinned read-only analysis-tool install log.
Canonical source preflight PASS (25 verdict-reader Python contracts separately);
existing booking_ref research tests13/13 PASS. These are cheap Python/source
checks, not C++/game/ASAN acceptance. No full sailing or physical corpus run.
Original module SHA256s rechecked: AnnoWelt f220a878510f740c4cfb7224ade0a260e9a0fd7e99113e1201e7a37c0dfa6ae2;
AnnoGame ac721220afd27ebca5fd2ece6a8ff452289b9d7458c4e091c68adea2012db7bc;
AnnoScene5b521f9541332846d32b57c93eaaee9c9d8de2ac2404cd75da0fe93e0e36fd38.
Bounded static reads use pinned Capstone5.0.6/pefile2024.8.26 in this task's ignored
cache; original binaries are never changed or committed.

Host observed18 logical CPUs,48GiB RAM,~1.5TiB free disk, AppleClang21/CMake3.31.10.
The first Mac's jobs2 gate/two CPU-bound opening children were active and left
alone. No admission override is set: existing admission defaults remain enabled.
Start owned compile/gate concurrency conservatively (four build jobs, one heavy
CTest child); refresh contention before expensive work. No process/assertion
owned by another lane will be stopped. B0 starts no game build.

### Current authoritative inventory (source3b74201f)

| Area | Current field/caller and behavior | Consequence for this slice |
|---|---|---|
| Money | Player::money i64; construction/founding payment debit directly; scripts commit their reviewed transaction; player book_upkeep remains separate. Scheduler::merge generic MoneyDelta uses add_money_saturating. | No city income path exists. Do not use generic saturating deltas as the checked booking commit or rewrite unrelated payment/script/player policy. |
| Purchases | resident.cpp::bill increments u32 requested bill before clipping stock; direct overflow yields BilledOverflow without that bill mutation. supplier_bill: MarketStall requested/take, services requested/no take, Inn min(request,stock) on input good/no city stock take. resident_job emits only increases as BilledDelta; return does not add a second bill. | Preserve requested-versus-delivered and service semantics. B1a must independently pin examples and direct/queued overflow distinctions. |
| Bill merge | scheduler.cpp::merge applies BilledDelta with u64 sum then u32 saturation and diagnoses actual merged amount. Each resident figure merges immediately. | A booking helper cannot silently change this accepted saturation or claim all requested deltas were credited. |
| Pending bills | City::billed is warehouse_goods-sized u32 vector, native/hash only with Rules::save_billed_goods (default true); false reloads zero. Original imports have no pending-bill source. | Booking must reject unsupported save_billed_goods=false or explicitly support it; pending income must survive native reload. No u16 narrowing. |
| Existing clocks/stats | City tick_stamp/stage_stamp are u16; player upkeep_stamp/rem and stat_a/stat_b belong to player. City has no booking stamp, sales/trade statistics or maintained running-cost totals. Scenario HEAD/HEAD2 notices currently report unmodelled statistics. | Reuse neither stage stamp nor player remainder for booking. B1 resolves original init/save/stat units; B3 decides authoritative persistence versus reconstruction. |
| Cost facts | BuildingType::running_cost[0/1] already loads PROD3 words0/2 and is digested. Payload blocked changes in production kernels; driver.consume sees blocked_changed. Manual SetProductionLineCmd is a separate switch route. | B1b must prove the state selector and per-class statistic-kind getter, then maintain success-only contributions, never copy them to Player::upkeep_a/b. |
| Lifecycle | create.cpp insertion/creation and remove_building (including apply.cpp public demolition/whole-city cleanup); scenario/native import creation and cache rebuild; apply.cpp::set_production_line/switch_line handles public SetProductionLineCmd, writing switched_on for Workshop/Farm/Mine and line_on for WeaponsWorkshop/SmallFarm, separately from blocked transitions; house_transition.cpp shared transition swaps type for upgrade/downgrade, remove_house detaches walkers then removes. | Every actual mutation route needs a cost invariant. No existing public city/owner transfer command was found; classify absent transfer as Unsupported rather than inventing one. Load reconstruction is outside the booking hot path. |
| Schedule | island_tick city_tick: building scheduler, stage_pass if no fault, configured city seams. One Effects buffer is shared by cities on that island; Scheduler merges after the island. Across islands merges occur in table order; resident figure purchases merge later, then player upkeep. | Same-island cities see stale external money views until merge. B1d must require ordered commit-time preflight/read-own effects, suppress booking after earlier faults and leave figure-phase bills for a later city visit. |
| Configuration | WorldSettings::prop10 already validates0..2/imports scene HEAD[0xb]; Island::climate exists. Goods metadata currently reads BauFirst/IdSec, not Preis/Bonus/Malus/PreisLevel. | No duplicate tax/climate/cost/bill fields. Verify direct/link climate sources and price ordering before registry/state work. |

### Acceptance/dependency table for the complete assigned slice

Each row requires independent read-only approval at a frozen diff/pin before its
dependent implementation. Passing research alone is never a passing engine test.
The existing arithmetic research is approved only within its original-wrap model.

| Package | Positive acceptance | Small distinguishing negative/control | Evidence and disposition |
|---|---|---|---|
| B0 adoption/inventory | Exact clean pin, module hashes, actual callers, cheap preflight/ref checks, unchanged old refs/evidence, scoped participating paths | No replay of old merge, no unexplained132 edit/default flip, no claim that Python baseline executes economy |13/13 reference and source preflight pass; `/root/econ_b0_review` APPROVED75b16493 after manual-switch inventory correction |
| B1a inputs | Verified climate/tax/ordered prices, actual billing units, imported stats/stamps | Requested stall versus delivered/Inn-clipped/service-only; repeated Preis resets, partial repeated tax lists, invalid names/dimensions | APPROVED4f644fc8 by primary reviewer; additive inert selectors independently derived and recorded7d92b230 |
| B1b lifecycle | Type getter/stat-kind and every current create/load/switch/house replace/remove path table | Failed/duplicate transition unchanged; city28/player14+rem1 charged1+1, no cost mirroring | APPROVED4f644fc8; absent owner-transfer remains Unsupported |
| B1c checked policy | Signed domains, exact ordinary arithmetic, word stamps, sum-before-rounding, gross credit then debit, negative money, explicit atomic booking fault | Final net fits but gross credit overflows; no reset/stats/stamp/debit on failure | APPROVED4f644fc8 by `/root/econ_b0_review` after comparison-domain/fault corrections |
| B1d order | Actual island/city/effect/figure/player sequence; same- and cross-island owner preflight | Two cities each individually fit but second combined credit overflows; earlier house/script fault blocks booking; save_billed_goods=false cannot lose income | APPROVED4f644fc8; actual-boundary engine proofs remain B4 |
| B2 independent references | Hand-pinned checked versus original-wrap vectors, lifecycle/order/width and smallest-positive/zero/partitioned timing | Width/order/double-charge/reset-before-fault mutants fail intended assertions; CLI empty/bad expected/incomplete refused | APPROVEDb62047ee by `/root/econ_b0_review`, canonical03 selected2/executed2, required3 invocations separate |
| B3 facts/state/identity | Data-sized prices/roles; minimal authoritative stamp/stats/cost state;133/ECONBK1 populated byte oracle and native round-trip | Actual132 save/log refuses intended boundary; invalid dimensions/signed prices/unknown climate transactional; alternate generic types/roles | APPROVEDb7908bef by `/root/econ_b1_primary_review`; canonical correction02 selected69/passed69/others0, required7 invocations separate; first211 gate retained206/5 |
| B4 production/lifecycle | Opt-in scheduler booking exactly once, full supported lifecycle, owner split and negative-balance continuation | Real last-city merge overflow leaves its money/bills/stats/stamp unchanged, queued prior effects accounted; default-off132 semantic controls | APPROVEDff095351 by `/root/econ_b0_review`; corrected Debug/ASAN each34/34, all other categories0; atomic P2 fixed and regression negative verified |
| B5 actual loop | Cheap original Tutorial path, new goods reach several households, bills/income/upkeep exact, public shortage/recovery, owner/city isolation | No starting-stock-as-production or grant in real arm; explicit component versus strict loader refusal; legal altered command diverges causally | APPROVED9801afcf by `/root/econ_b1_primary_review`; final Debug producer1/1 + exact-artifact prior full6/6, final ASAN6/6, all other categories0 |
| B6 scaling/consumer | Three nonempty legal sizes, city/goods operation counts and modest Release samples, unrelated objects do not add global scans | Fixture legality/positive bookings first; no latency threshold or multiplayer performance claim | Host/raw manifests and Windows data/events/refusal/config contract required |
| B7 frozen closeout | Corrected focused Debug/ASAN, relevant full coverage, required refs/CLI, real loop integrated once per configuration, format/lint | Keep first failures/known skips distinct; fresh final reviewer checks interactions and default-off/primary/replay claims | One final REPORTS+handoff, clean local candidate; no main merge/push |

First participating path is the existing configured resident service model
(MarketStall/PublicService/School/Inn), House lifecycle, SmallFarm/Farm production
and transport into a city. Prices/roles and cost categories must be generic data,
not a fixture id allowlist. Other currently supported building classes are part
of the lifecycle audit; unsupported original classes/ownership transitions stay
explicit refusals, never silently omitted. No full-game balance, ship-founded
alcohol feasibility, planting, new tax UI, figure upkeep rewrite or default change
is implied. B1 contract review must resolve every fact used by B3/B4 first.

B0 approval: `/root/econ_b0_review` independently reviewed clean
`75b16493070f0f333fdf98e31a5326795a013a09`, primary module identities, baseline
logs and current billing/lifecycle/order callers. The missing manual production
switch route was added and re-reviewed; no actionable B0 finding remains. This
releases B1 contract work, not C++ implementation before B1/B2 approval.

### B1a input contract in progress

Direct input/billing follow-up is in docs/re/economy-booking.md, ExecutorB1a.
Original ordered price directives and partial source tax-prefix writes are
retained. Proposed modern price domain: accumulate signed directives with checked
i64 operations, allow ordered Malus and temporary negative climate intermediates,
then require resolved climate prices and tax surcharges to fit nonnegative u32.
Effective price is their checked widened sum. Original source defaults have seven
climates/three tax levels; modern registry/config dimensions are data-sized and
validated separately, not bounded by the original format's record size. Runtime
resolved vectors must have exact configured dimensions; source partial writes
are not malformed runtime rows. Malus negation at I64_MIN, unknown climate,
excess source tax lists, invalid final prices or incomplete runtime rows
must name the field and leave the destination unchanged. No signed-price wrapping
or clipping is approved. The precise original-agreement domain is in B1c; the wider modern domain is an
opt-in safeguard, not a claim of wrapped signed-original arithmetic.

Decisive examples to hand-pin in B2: Preis45; Bonus5 STEPPE; Bonus2 STEPPE =>52
there/45elsewhere; later Preis40 resets all climates40. Tax[1,2,3] then[7] =>[7,2,3].
Preis40; Malus45 STEPPE; Bonus10 STEPPE =>5there (valid final); omit the last
bonus => negative final price refused. Stall request300/store50 at price50 bills
300 and earns rounded59, whereas Inn clips50 and earns10. U32_MAX-5+10 direct
bill faults, but an accepted queued delta saturates and credits5. These are
contract examples, not a claim that new engine cases already execute. Remaining
B1a direct island INFO/save/init reads and B1b–d lifecycle/arithmetic/order work
must close before B1 authorizes B2/B3 implementation.

### B1b supported lifecycle and accounting history — proposed contract

Primary callee audit is the ExecutorB1b section of the RE note. Each original
inspected building stat-kind getter returns1, including military classes; the
second city cost bucket exists but must not be selected by guessing from a list.
Modern type facts may explicitly select supported statistic kind1/2, defaulting
to the verified1 for original types; unknown kinds refuse validation. This is a
field-role distinction, not a building/type-id allowlist.

| Supported engine entry | Original contribution rule / proposed checked equivalent | Refused or duplicate |
|---|---|---|
| create_building, scenario/import creation, public Build/Found warehouse creation | Add type.running_cost[active-selector] once to its city kind after complete creation validation. Native continuation restores authoritative totals instead. | Validate prospective sum before payment/placement/IDs; failure adds nothing. |
| SetProductionLineCmd / set_production_line / switch_line | Single-line selector switched_on; base dual/SmallFarm OR of line_on; WeaponsWorkshop OR of enabled AND research-eligible output lines (research id0 or owner state bit2). Subtract old selected cost then add new selected cost only if selector changes. | Invalid owner/goods/index or same state leaves costs unchanged; switching one of two eligible enabled lines off keeps active cost. A research-ineligible remaining weapons line does not keep the selector active. |
| production blocked/status transition | No cost contribution change merely for blocked/starved/full/suspended display state. | Do not confuse flags with manual switch. |
| house_transition shared Upgrade/Downgrade type swap | Preserve existing city cost contribution, matching inspected original path. A level change is not the adjacent general type-replace helper. | Failed materials/domain/duplicate outcome leaves totals unchanged; no invented cost correction. |
| remove_building via public Demolish/whole-city cleanup or remove_house | Subtract current type cost at its current active-selector, then remove once. Checked preflight precedes walker detach, tile changes and every other mutation. | Missing slot does not subtract; insufficient accounted total explicitly refuses before removal, rather than reproducing original underflow. |
| Native load | Restore authoritative booking stamp/statistics/cost totals exactly; reconstruct derived indexes only. | Recomputing costs from current types could erase the preserved house history and is forbidden for native continuation. |
| Original-format import | Rebuild costs using imported current types/manual states; original headers did not save those totals. Booking stamp/pending bills start0; saved sales/trade statistics follow B1c's explicit widened import rule. | No invented debt reset on modern reload. |
| Owner/city reassignment | No supported engine command exists. Initial scenario ownership is validated before creating a world. | Remains Unsupported; no new transfer command or hidden repair. |

City cost totals are authoritative history, not a cache derivable solely from
current building types. Original household costs are mostly0, but one original
PIONIER has10/20; mods can distinguish history. A synthetic house changing active
cost10->20 retains10 on replacement, and later removal at cost20 must refuse
before mutation if the city's recorded bucket is only10. This preserves the
original transition while replacing unsafe underflow with the packet's explicit
checked safeguard. The native cut must retain10 rather than silently rebuild20.
No per-visit building scan is permitted. City costs never touch Player::upkeep_a/b.
Script research grants can change the weapons selector without changing recorded
city cost totals: the original research setter only updates research bytes.
Retain this history as well. Creation with both lines locked may record inactive
cost; after a grant, removal uses the now-active selector and can checked-refuse
an insufficient total. A same-state command must not silently repair it.
Original weapon setters reject enabling a research-ineligible line, while the
accepted current SetProductionLineCmd already permits that switch (production
remains research-gated). Keep the existing command acceptance, including with
booking off; the new cost calculation still uses the verified eligibility filter.
No new permission restriction or research-notification scan is introduced.
Original unmodelled stall manual flags remain the existing default-on component
boundary; this task does not introduce a new stall switch command.

### B1c modern arithmetic, dimensions and fault domain — proposed contract

Original word/dword wrapping stays in booking_ref's research mode. The opt-in
engine uses these exact checked domains, with no negative-balance clamp:

- Source price directives parse checked signed i64. Malus negation/addition must
  be representable; intermediate climate values may be negative, final resolved
  prices and surcharges must be0..U32_MAX. Runtime goods/climate/tax vectors have
  exact configured dimensions. Tax count is an explicit SimConfig field, default3;
  world validation uses it rather than the old hard-coded<=2. prop10 remains the
  one tax selector and may be widened under133 to index the configured domain.
  Climate-name roles are configured in index order and must match the existing
  monopoly_climate_mask dimensions; Island::climate indexes those same rows. Source defaults0, partial tax writes and the original empty-list no-op
  are preserved; excess configured source columns and unknown names refuse.
  Modern data builders may use larger configured tables; no original id list is
  an engine limit. Effective price is u64(climate_price)+u64(surcharge).
- Bills retain accepted u32 values and existing direct-fault/queued-saturation
  policies. Only configured billed goods are priced/reset; malformed counter or
  price dimensions refuse. Products and total numerator use checked u64, including
  the final+128. Income is floor((sum+128)/256), computed after summing all rows.
  A nonrepresentable product/sum/rounding intermediate is a typed deterministic
  booking fault, never a wrap or saturation.
- Each city owns u64 cost-kind1/2 totals and u64 raw sales/trade-sales/trade-purchase
  statistics, plus an independent u16 booking stamp. Sum costs checked in u64,
  charge=floor(sum/15) with no remainder. Existing player upkeep/rem semantics
  are unchanged. Original imported displayed statistics are widened to exact
  u64(saved_u32)*105/15 (equivalently7*saved), explicitly avoiding original
  multiplication overflow. Legacy HEAD imports sales only; HEAD2 all three.
  Modern native saves preserve exact raw statistics, bills, stamps and totals,
  with no original save quantization/reconstruction loss.
- Statistics decay is exact floor(stat*104/105) without overflow, using checked
  widening or quotient/remainder decomposition; sales then adds income checked.
  Nonoverflow original examples match; original overflowing statistical products
  remain separately labelled research results. No unexplained statistical clamp.
- Player money remains i64, including negative values. Preflight the *gross*
  income credit against the actual ordered balance, then upkeep debit against
  that intermediate. Both must fit. Netting income-upkeep first is forbidden:
  money=I64_MAX, income1, upkeep1 must fault even though the final net fits.
  money=I64_MIN, income0, upkeep1 likewise faults. Compute all new stats/reset
  effects before committing any booking mutation.
- Due test is u16(low32(tick)-stamp)>=configured booking period, default2000;
  period must be1..65535. A taken visit stamps current low16, not stamp+period;
  an overdue visit performs one booking, not catch-up. 1999 is not due,2000 is;
  wrap65000->1464 is exactly due;65536 elapsed aliases0 by original word semantics.
  No failed/undue/repeated booking advances stamp or clears bills.
- Worlds with booking enabled and save_billed_goods=false refuse creation/load.
  Booking does not require house_updates=true: with houses off it may still book
  existing valid pending bills and upkeep. Real-loop arms explicitly enable both.
  Neither default changes. Booking state is authoritative when enabled; mode-off
  lifecycle must preserve established default-off behavior and add no new money
  mutation or fault. B3 freezes exact byte placement; required authoritative fields are the u16
  booking stamp, three u64 raw sales/trade statistics and two u64 cost buckets.
  Modern native continuation preserves these fields without reconstruction.
  Original import widens statistics; original pending bills/stamp initialize0.
  With booking off, newly maintained cost bookkeeping is inactive/zero and
  cannot introduce new command faults; imported statistics may still be retained.
  Serialization of these fields is explicit and must not silently lose data.

A booking fault names a typed cause/city, stops the world through the existing
fault/output mechanism, and leaves *that booking's* money, billed counters,
statistics and stamp unchanged. Earlier committed activity remains committed.
The current world fault latch is authoritative and sticky: further Advance does
nothing, and a native save of that faulted world reloads stopped (not cleared).
Retry tests start from the still-unfaulted pre-fault checkpoint: unchanged invalid
inputs reproduce the fault without duplicate income. A legal correction before
advancing that unfaulted checkpoint can book the retained bill once. No implicit
fault reset or new clear-fault command is part of this slice.
Invalid lifecycle arithmetic likewise preflights before its command/transition
mutates anything; failure is a typed refusal/fault appropriate to its existing
caller, never a partial walker detach or charged failed build.

### B1d production order and transaction boundary — proposed contract

Actual order is scripts-before-advance; islands in table order; each city's
building work then stage pass then booking; configured city seams; island effects
merge; next island; events/figures with per-figure merges (resident purchases);
then existing player upkeep/AI/timers. A figure purchase made after the island
phase is pending for the next due city booking. An original overdue booking
cannot be forced to wait for later figure purchases in the same Advance.

Cities on one island share an Effects buffer and const external player views.
Booking therefore queues a typed request rather than resetting its City early
or trusting an isolated read-only balance check. The request records its city,
owner, tick and the current prefix lengths of queued money, bill and fault effects,
plus the cost/stat snapshot needed to preserve the call-time contribution.
At the serial merge, process those prior prefixes with their established
semantics, then preflight the complete booking against the *actual* owner
balance. Commit credit, statistics, debit, selected-bill reset and stamp together
only on success. Remaining bill effects follow and stay pending. Requests are
handled in actual city visitation order; a second city sees the first city's
committed money, including when both individually fit an old snapshot. Across
islands, the existing immediate island merge already supplies this ordering.
No new PRNG site or wall clock is involved.

Earlier script failure prevents the advance. Earlier House/stage faults prevent
queuing a later booking; merge-time earlier faults prevent its commit. Fault
prefix barriers classify queued faults too: consume only faults preceding each
booking before its preflight. Valid bookingA followed by a later city/seam fault
commitsA, then records that later fault; inverse order queues no later booking
(or suppresses it at its recorded fault barrier). Immediate earlier merge-phase
faults already latched on World also prevent commit. A blanket check of the
whole final fx.faults vector would incorrectly suppressA and is forbidden. A fault
from one booking prevents later booking commits in that advance. Non-booking
activity already executed/queued before discovery of a serial commit fault keeps
its existing effect-merge semantics; it is not rolled back and its earned bills
must not be dropped. No global all-Advance transaction or player-policy rewrite
is implied. Legal configured city seams that queue bills before/after the booking
barrier are distinguished; directly rewriting booking-owned authoritative fields
inside arbitrary hooks is not an unversioned supported mutation API.

Production-boundary controls must use: two same-island cities sharing Effects,
two different islands with one owner, a different-owner city, prior queued money,
bills on both sides of the barrier, a late gross overflow, earlier House/script
fault, duplicate request/repeated visit, save/reload and retry. All include a
legal nonempty booking control. Existing World::advance accepts u32 milliseconds,
including0/1: the clock keeps odd-millisecond surplus in its accumulator. Tests
must reach the next due city visit through that real scheduler, prove two1ms
calls retain the fractional tick, and preserve partial elapsed delay across
native cuts. Interval partitions can change which original visits occur and
stamps become now; no full-world or overdue-partition invariance is promised.


#### B1c exact comparison and ordering corrections

Original-agreement domain is *not* merely unsigned-word inputs. Require original
price/directive/row products and summed numerator to avoid word/dword wrap, and
**sum+128 <= I32_MAX**, because original income reinterprets that dword as signed.
Also require original i32 money credit/debit intermediates, u32 cost sum,
statistics decay product/addition and import product105 to fit their original
widths. Within that domain the algorithms agree. Beyond it, checked modern
positive/wider results are explicit adaptations. Example65535²+128 fits u32 but
original income is-511, modern16776704; both expected answers must be pinned.
For import, saved37 gives259 ordinarily; saved40904451 gives original3 versus
modern286331157 (original last nonoverflow multiplicand40904450).

Ordering controls: MAX-1 and two cities each income1/upkeep0 => first commitsMAX,
second faults without its own bill/stat/stamp reset. AtMAX, a queued money-1
*before* income1 lets booking succeed; the same-1 *after* booking cannot rescue
its gross overflow. After that booking faults, the unrelated queued-1 still
follows the established effect merge, so final world money may differ even
though the failed booking contributes no mutation. Diagnose those ledger terms
separately. The request's fault-prefix barrier is necessary alongside its money/
bill barriers. None of these controls requires a new command or live tax UI.

#### B1 financial-only selectors for currently inert payloads

Early real-loop prerequisite inventory found original type805/JAGDHAUS on
Tutorial island7. Its production body remains unported; refusing to inspect its
financial switch would leave upkeep guessed. Minimal accounting activation
metadata on an Inert payload is allowed here; it does not implement hunting,
Whaler production, supplier flags/purchasing or a new line command. Existing
UndecodedBuildingRecord/body limitations remain visible. The type's existing
output roles determine the Stall aggregate; no literal building ids are needed.

Independent primary audit `/root/econ_b1_primary_review` established these
constructor/load financial facts (original addresses are in its bounded read
record and RE addendum):

| Class with unrepresented manual state | No class record / constructor | Supported financial record |
|---|---|---|
| Hunter, base DualWorkshop | Both flags1 => aggregate1 | Nonempty length<=14, zero-filled14-byte scratch: (byte1&3)!=0; >14 is an unsafe original write and remains Unsupported for booking |
| Castle | Inherited Farm switch1 | Exactly10 bytes: byte1&1; other lengths are skipped by original, so default1 |
| Whaler | Switch1 | Version!=1 retains default; version1 requires at least11 bytes for verified prefix, outer byte2&1; truncated version1 is Unsupported for booking |
| MarketStall | Both flags1, aggregate OR(flag_i AND output_i!=0) | Exactly1 byte: flags from byte0 bits0/1 with the same nonzero-output test; other lengths leave constructor flags |

Store only the required financial activation fact/knownness on otherwise inert
payloads and persist it under133. Unknown unsafe/truncated financial input must
not become a free/active fallback: booking-enabled validation names and refuses
it. Booking-off retains the existing warned/unmodelled-body behavior and does not
start production or add financial mutations. Current supported line commands
still return Unsupported for these inert payloads; a future body owner must
reconcile the financial field with its actual state before a combined identity.

Core B1a/b primary review APPROVED4f644fc8 after correcting Weapons eligibility,
empty source tax no-op and import widths. B1c/d review by `/root/econ_b0_review`
APPROVED the same pin after correcting saved fault semantics, fault-prefix order
and signed-original agreement bounds. The same primary reviewer independently
approved the additive financial selector facts/length boundaries above. No
unresolved rule for the first slice is hidden in a default. B2 may implement the
reviewed independent models; engine implementation still waits on B2 and B3's
own schema/facts proofs. No routine orchestrator pause is required.

## B2 — independent checked references and hand vectors

`tools/booking_checked_ref.py` is separate from the unchanged original-wrap
`tools/booking_ref.py`. Its inputs are resolved rows, never C++ output. The
committed `fixtures/synthetic/sim/econbook1/checked_vectors.json` contains91
hand-pinned complete results. The CLI computes all fields, compares the entire
answer, reports each executed id and rejects empty/type-only/malformed/duplicate
inputs, partial expected answers and unknown kinds. CTest names
`booking_checked_reference_agrees` and `booking_checked_reference_contract`
must be required in every affected gate. Python methods are not CTest counts.

The existing acceptance table's B2 assertions are realized as follows:

- Word gate:0/1/1999 preserve bills;2000 books;65000→1463 is1999,
  →1464 is2000;65536→word0 intentionally aliases;6001 books once and stamps now.
- Rounding:127/256→0,128/256→1,129/256→1; two128 numerators sum to256
  and yield1, not2. Ordinary15000+7500 yields88. The original65535² case
  yields-511; checked yields16776704. U32_MAX² yields72057594004373504.
- Width/fault: effective price2^32+1 times bill2^32-1 equalsU64_MAX,
  so the product fits but rounding+128 refuses. Separate product, sum, cost sum,
  credit and debit failures preserve every booking field. MAX+income1-upkeep1
  must fail gross credit despite net fitting; MIN+income1-upkeep1 succeeds.
- Statistics:105 decays104 before income88→192. AtU64_MAX, division proof
  n=105×175683276892471920+15 gives floor(n×104/105)=18271060796817079694.
  The first exploratory expected answer had a one-unit transcription error;
  retained `cache/econ-book1/B2/reference-exploration-01.log` records it, and
  quotient/remainder derivation corrected only that expected literal before
  acceptance. `reference-exploration-02.log` executes all66.
- Requested Stall300 returns/takes50 and bills300; service300 returns300 and
  bills300 without warehouse take; Inn returns/takes/bills50; later return adds0.
  Returned purchase/service units and physical stock taken are distinct fields.
  Direct MAX-5+10 faults unchanged,
  effect merge saturates with actual increment5. City28 charges1; fifteen
  visits at city14 charge0, while player14+initial remainder1 totals14/rem1.
- Four tax columns and custom climate are data-sized. Repeated partial tax
  overwrites prefix, empty source tax does nothing; later Preis resets climate
  prices, retains tax. Intermediate negative Malus may recover; invalid final
  negative/wide price, excessive columns and unknown climate refuse.
- Lifecycle history distinguishes dual OR, research-filtered Weapons, research
  without retroactive accounting and house type changes without recomputation.
  Removal underflow/failed duplicate/unsupported inert switch preserve state.
- Chronological event tape proves two-city combined overflow, money before/after
  booking, fault before/after booking, late pending bills and owner isolation.
  This intentionally does not copy the proposed engine prefix-buffer algorithm.
- Clock model retains1ms surplus across0/1/partitioned advances and acceptsU32
  milliseconds. Real scheduler visits, native fractional cuts, real same/cross
  island boundaries and saved fault/retry remain explicitly PENDING B4, rather
  than counted as passing by this arithmetic reference.

The public contract executes legal controls before twelve intentional wrong
models: u32 product, row rounding, netting gross credit, reset before fault,
double booking, house rescan, raw Weapons OR, blanket final-fault suppression,
unsigned source Preis, unchecked signed accumulation, equal-cost selector bypass
and unchecked I64_MIN Malus negation.
Each must return a disagreement at its named decisive vector, not timeout,
compilation or unavailable assets. Bad expected-income89 must be recomputed88
and rejected; an echo-expected mutant demonstrates that this negative control
distinguishes the fake implementation. Partial expected fields also disagree.

Initial local CLI contracts:9 Python methods passed at18da741b. Canonical01
failed the unchanged ancillary headless bench_picture build before selection
(five AppleClang unused-but-set GPU-option variables);02 then passed both selected
references, with0 failed/empty/skipped/incomplete/sanitizer lines. Its three
required invocations (agrees, contract, picture_matrix_contract) each passed1/1,
separate from selection2/2. These results retain their original source18da741b;
they do not approve the following reference corrections.

Independent reviewer `/root/econ_b0_review` found signed-source directives were
incorrectly restricted to unsignedu32; equal old/new cost values incorrectly
bypassed a changed selector's checked subtract/add; Python equality conflated
JSON88.0 with integer88; service returned quantity was conflated with stock take;
and negative event indices could use Python's last-element indexing. Corrections
add25 vectors, strict result JSON types, bounded indices/boolean fields and
the original-stat import pair37→259 and40904451→286331157 (original3). The existing
B1 refusal of I64_MIN Malus negation is retained, including when mathematical
subtraction might cancel; no arithmetic-policy expansion is hidden here.
The equal-cost counterexample creates workshop20, house10, replaces house cost30
without historical recomputation, removes house30 leaving0, then switches the
workshop20→20: checked subtraction must refuse and preserve its old switch.
Corrected frozen pinb62047ee:91 vectors,10 Python contract methods,12 model
mutants. `/root/econ_b0_review` independently challenged signed Preis-1+Bonus2,
the equal-cost history underflow and float88.0 expected; all now distinguish the
right result. Focused review found no remaining P1/P2 and approved B2's bounded
reference scope; real production proofs remain B3–B5.
`cache/econ-book1/B2/canonical-debug-03.log` selected2/executed2 with all other
outcome counts0; three required invocations each1/1 pass separately. Log SHA
`a16d7695d3ab4f1de457632cc8eeb0f07dd4574ff251eaabc91cf916c3c37f0e`, inventory SHA
`1c5fb4df31331e5d583fd7cfe0ad7c07dd0370997048e9117a916579741752b1`.
The own `receipt.json` records reference/fixture/config/hook/ancillary executable
hashes as well as first failure and corrected gate identities. No rerun is needed
for this documentation-only approval record. B2 releases B3 automatically.

New own `build/econ-debug` avoids modifying the
preserved MAP-INTEGRATE-1 builds. Its read-only doctest dependency was copied
into own ignored cache before configuration. Headless bench_picture is built
only to satisfy the canonical compulsory public picture contract; this is not
a GPU/render acceptance campaign. The own ignored CMAKE_PROJECT_INCLUDE file
`cache/econ-book1/B2/ancillary-headless.cmake` adds only to bench_picture
`-Wno-error=unused-but-set-variable`; warnings remain visible and all simulation,
formats/load/replay targets retain-Werror. Reviewer independently confirmed the
single affected compile command. Current other-executor gates are left alone;
18 logical CPUs/48GiB/~1.5TiB available permits4 build jobs and1 CTest child.

## B3 — facts/state candidate contract and proof plan

Candidate identity is133 / ECONBK1 (`45 43 4f 4e 42 4b 31 00`, u64LE
`0x00314b424e4f4345`), with unchanged save1/replay4/wire1/PRNG5. Canonical
main's132 files and the completed MAP branch remain intact. Actual published132
G2 was reproduced6/6 in a fresh process atb62047ee by the canonical gate
`cache/econ-book1/B3/actual132-control-01.log` (selected1/executed1/others0,
three required invocations separately passed). Its initial native save was
extracted byte-for-byte with checksum validation; provenance/offset/hashes are
committed under `econbook1/legacy132`. The actual132 executable is retained in
own cache for later default-off semantic projection, not rebuilt as133.

Schema and facts:

- `SimConfig.economy` holds climate names in existing Island.climate index order,
  tax_count and word booking period; defaults original7/3/2000. Sizes use the
  existing entry-table allocation bound, never original-count limits. Booking
  requires the climate-name count to match monopoly_climate_mask. The one existing
  WorldSettings.prop10 widens from byte to u32 and validates against tax_count.
- Optional immutable GoodsPrices follows id_sec in registry digest: presence,
  count-prefixed climate u32s and tax u32s. Absence explicitly means original
  constructor zero in all configured roles. Present rows must have exact sizes.
  Registry sealing retains a derived name-order/tax-count binding, like existing
  population roles; a world cannot reinterpret tables under reordered names.
  Roles are hashed in SimConfig, not a competing world copy. Period is not part
  of the immutable price binding and can be configured independently.
- The goods reader stages *all* metadata, including existing BauFirst/IdSec,
  before commit. Ordered signed-i64 Preis/Bonus/Malus/PreisLevel supports negative
  intermediate values and rejects invalid final u32 values, unknown names,
  excessive source columns, arithmetic overflow and I64_MIN Malus negation.
  Empty source tax lists are no-ops; partial lists retain prior suffixes; Preis
  resets climate values only. A local checked full-i64 decimal reader is used:
  the shared text::to_int intentionally accepts only18 digits, so widening it
  globally would change unrelated parsers. No generic formats change is needed.
- BuildingType.statistic_kind u32 follows running_cost in registry digest.
  Original audited defaults1; explicit facts may select2. Running costs remain
  the existing inactive/active vector (empty zero default or exactly2 columns).
  Neither field is copied to Player upkeep.
- CityAccounts is exactlyu16 stamp then fiveu64s: sales, trade_sales,
  trade_purchases, civil_cost, military_cost. It is saved/hashed after billed and
  before index regardless of booking flag. Native restores historical totals,
  never rescans them. Original map import retains HEAD sales / HEAD2 all three
  raw statistics using widened saved*105/15, constructor stamp/bills0; B4 supplies
  opt-in current-type/manual-state initial cost reconstruction.
- Inert stores optional financial_active. Defaults/import masks are the reviewed
  B1 table; unsafe Hunter/Dual>14 or truncated version1 Whaler returns unknown.
  Booking-enabled world validation refuses the named field; booking-off retains
  warned body limitations. UndecodedBuildingRecord still describes the unported
  body, even when its minimal financial selector was decoded. No new production
  body or line command is added. Rules.city_booking remains separately false;
  enabled plus save_billed_goods=false refuses both creation and native load.

Independent populated `integration_stream_ref.py` now encodes7 complete structural
records, adding a42-byte accounts record,8-climate/4-tax configuration and a
three-goods price-registry digest. The accounts literal includes sales
0x0102030405060708 and civil cost4294967313, catching narrowing/field swaps, not
just a header change. Its existing public CLI contract now has6 methods.
The scheduler oracle uses an explicitly opaque synthetic-registry binding (as
before); its new binding is77d2dd9ed3f484b1 from generated registry evidence,
not an independent transcription of every synthetic type. Independent empty
registry804f19da7cae34ee and empty world69871c0febf78a1c are computed by Python;
new price-registry/role/account bytes are independently transcribed separately.
The structural CLI contract has6 methods after adding re-signed wrong accounts,
tax count and climate-price controls; matching a digest alone cannot satisfy it.

The independent raw-SZE inventory of50 source files is committed as
`econbook1/city_statistics_import.txt` (only counts/derived sums, no asset bytes).
It records23 scenarios whose former nonzero HEAD statistics warning disappears,
and separately retains STAT2 tick-counter warnings. C++ creation-only corpus
checks imported raw sums/warning counts against this fixture. Only that verified
warning column changes in reused physical/movement rows; actual133 creation
identities are generated anew. Creation-only evidence does not re-execute those
historical movement columns or become a full corpus movement claim.

Focused tests cover transactional ordered parsing, signed endpoints, >original
goods/role counts, a second producer/service via data, u32 tax299, exact native
history/u32 bills, malformed dimensions, role reorder, known/unknown financial
flags, opt-in persistence refusal and actual132 version refusal with a legal133
recording. No money movement is connected yet. Engine integration B4 waits for
independent B3 approval.

Development evidence remains exploratory until the clean frozen gate: first
compiler failures were missing settings_section config argument/callers and
test-helper typos, all retained in build logs. `facts-exploration-01.log` crashed
because the new test dereferenced island slot0 although its fixture explicitly
uses7; corrected test now asserts the actual slot/pointer, and02 passes7/7.
`facts-exploration-03.log` passes12/13; the remaining8 assertions are stale
38-byte settings offsets after prop10 widens to41 bytes, now corrected. A
separate registry-binding exploratory failure records old expected bindings,
corrected against new byte schema/Python empty-registry calculation. These
are preserved first failures, not acceptance results or waived runtime defects.

Fixture generation completed14+6 direct CTest invocations, all passing in24.14s
and36.85s respectively, under explicit ANNO_UPDATE_GOLDEN=1 (not acceptance).
The six-case run's original Tutorial replay controls measured progression and
interruption/recovery with their full existing semantic assertions; the latter
took25.48s Debug. Own gates remain serial with admission enabled. There are41
new133 checkpoint values and11 newly recorded133/ECONBK1 logs. Independent
scheduler values reconcile before the C++ values are installed. Every recording
outer identity is checked; copied132 placeholders are not retained as133 data.
The final players scope of newly recorded default-off133 G2 is byte-for-byte
the actual132 CLI's players scope, including monetary state. B4 must add a
nonempty priced-bill/cost default-off control once money wiring exists.
`independent-stream-check-01.log` agrees with all current oracle-controlled
fixtures, including freshly generated creation-only corpus identity. The broad
movement corpus is reused by dependency, not counted as executed here. Clean
frozen canonical B3 acceptance and independent review are still pending.

First frozen B3 gate atb244a45c: selected211, executed passes206, failed5;
empty/skipped/incomplete/sanitizer0, terminal status1. Raw
`canonical-debug-01.log` SHA35404c77b7e5477c56509caa73e144730d40c8b3d685f80e0f2c0eae90aca22a
and its LastTest copy are retained. The failures were exact legacy transcriptions
of Inert's tail, the Rules tail, a current-version assertion, the City prefix and
the full original-registry digest binding. They are corrected by adding the new
bytes/field counts/current binding, retaining full byte/hash comparisons. Rules
now has an independent explicitly enabled booking row as well as default-off.

Independent B3 review `/root/econ_b1_primary_review` found two actual caller bugs:
fixed_map and ship_start still validated settings against default3 columns.
Both now pass their supplied config. A private authored base HEAD with tax3 is
accepted by both loaders under four configured columns and refused under three;
the existing default loaders are separate regressions. The duplicate climate
addition now asserts45+5+5=55 before a later Preis reset can mask it. Author audit
also made the registry reject invalid economy roles (including zero tax_count)
at construction, even for an unpriced table, not only at World validation.

Reviewer independently packed the42-byte accounts/129-byte economy records,
rejected swapped cost bytes with a recomputed digest after a legal control, and
re-derived all50 raw SZE rows. No other P1/P2 was found in inspected B3 facts,
state/native/default-off code. A correction gate and focused re-review are
complete: `/root/econ_b1_primary_review` APPROVED cleanb7908bef1edce1e0ad4b8d61bc1d6a04dff2fa21.
Correction gate02 selected69/passed69/all other outcomes0, status0; six explicit
required contracts plus compulsory picture each passed1/1 separately. Log SHA
c17ba4329a300de90e871742a016539c987b2cfef9c083d44b1325d49f7e5762.
The206 unaffected passes keep their originalb244a45c provenance. B4 is released;
this approval does not claim production booking or the real loop is complete.


## B4 production package — frozen candidate (2026-11-06)

`city_booking.cpp` is a pure checked calculation; the production scheduler emits
call-time barriers into each island's existing Effects and consumes the exact
money/bill prefixes at merge. It checks the actual shared treasury before each
credit then debit. A later fault does not erase an earlier successful booking;
an earlier queued/immediate fault prevents a later booking. Figure-phase purchases
follow islands and remain pending. The successful commit changes only the booked
money, selected bills, stamp and three statistics, retaining later lifecycle costs.
Typed `CityBookingLedger` observations use existing SimEvent/host output; they are
not saved state, commands, an extra money mutation, or a new random draw.

Import insertion, paid BuildCmd and founding all use the same checked contribution
preflight, before draws/charges/insertion. Native load retains saved history.
SetProductionLine preflights old-selector subtraction/new-selector addition even
when amounts happen to match. Same-state changes do nothing to history. Removal
checks before tiles/cache/counters; live house removal checks before walkers.
House replacement deliberately retains historical totals. Research eligibility
changes do not repair them. Current-selector underflow is a named refusal, not a
rescan or clamp. No new ownership/reassignment command is implemented.

City cleanup preflights its remaining building removals as one primitive. A
successful named tile removal that triggered cleanup remains completed if cleanup
then faults: all other buildings and city tiles remain, and the sticky world fault
stops subsequent work. This is separate from command-record acceptance; observers
must inspect the world fault. Player upkeep_a/upkeep_b/remainder are unchanged.

The existing acceptance table's B4 assertions are now paired with these controls:

| Assertion | Legal nonempty control | Distinguishing error/control | Evidence / disposition |
|---|---|---|---|
| Checked pure arithmetic | 88 income,1 upkeep,87 net | full hand-vector engine projection incl u64 debit spanning i64, configured statistic faults | 94 independent JSON cases; projection contract plus C++ comparisons; review pending |
| Actual ordered shared treasury | two bills1 at MAX-1, paid Build retry | second city faults at MAX on same/different islands; gross MAX+1-upkeep1 refuses | `city booking: shared owner...`, `production gross...`; review pending |
| Exact effect boundary | before-city bills book; after-city bills remain | queued pre/post faults; late money20 affects second city only | `prefixes...`, `production gross...`; review pending |
| Time/no duplicate booking | 4000x1ms; odd-ms native cut; split3999+1 | zero/repeated time, one late visit rather than catchup | `smallest elapsed...`; review pending |
| Complete supported lifecycle | all five production payload alternatives, Inert import, house/market constant facts | failed/duplicate create/switch/remove, locked weapons, historical underflow, equal-valued selectors, cleanup | new booking cases + extended house transition test; review pending |
| Default-off and separate owner | priced/costed fixture unchanged money/bills/stamp, second owner own ledger | on/off, both house rules, nonconsumer bill retained, pending bill after removal | `actual scheduler...`, `smallest elapsed...`; review pending |
| Fault/continuation | native before cut, legal paid command then retry | native saved fault remains stopped; no reset API | `shared owner...`, `actual scheduler...`; review pending |

Exploration `cache/econ-book1/B4/engine-exploration-06.log` passes12 cases/879
assertions. Earlier retained explorations expose test setup errors (island deposit
coordinates, ring visitation, missing group table, a dangling fixture pointer),
not accepted results. Compiler errors and the first failed mutation-driver attempt
are preserved; that driver wrongly required a fixed doctest spacing. Its already
valid width/order kills remain in the first raw logs; double/reset reran with a
whitespace-tolerant test-count check. All four mutants built, passed the legal
arithmetic control, and failed the intended accounting assertion with exit1;
no timeout/crash/asset or compilation failure counts as a kill. Sources restored
and rebuilt before the final12-case exploration. Raw per-mutant logs and receipt
are under `B4/mutants/`; reference contract now11 methods, including exact engine
projection completeness against the hand-pinned JSON, and retains12 model mutants.

Canonical gates will use own econ-debug/econ-asan builds, required original assets,
serial CTest, four build jobs, enabled admission, and900s per focused case. The
focused examples take under a second outside admission;900s allows shared-host
admission and sanitizer startup without borrowing an old long canary's timeout.
No other executor job was active at the read-only host observation; memory/free
disk remain ample. The bench_picture-only ancillary warning exception from B2 is
unchanged; sim/formats/replay retain warnings as errors. This package adds no new
state bytes after B3, so current default-off133 fixtures must remain exact. Full
relevant integration and the original economic loop remain B5/B7, not claimed here.

### B4 first frozen gates and independent correction

Frozen `d55df98d` Debug and ASan/UBSan each selected30/executed30, all other
outcome categories and sanitizer lines0, GATE_END status0. Raw
`B4/canonical-debug-01.log` SHA256
`ac1d4591c8a3e3597baa4c1032e6b5a55787f6fb0f681bf6edea67f73081b669`,
`B4/canonical-asan-01.log` SHA256
`162f0d888a33c08347aca4cc976edc4e9a4c6d0e196bdfcb2884bf537241966e`.
Six required invocations (including mandatory picture) each1/1 are separate.
Source/build/binary hashes are in `B4/receipt-d55.json`; no source changed during
either gate. These results do not override the review finding below.

`/root/econ_b0_review` found a P2 interaction: build_atomic's dry run lacked
aggregate city-cost scratch. Two active28 placements against historyMAX-40 could
place/debit the first before the second failed. The correction adds checked
financial history to the existing per-command Budget and carries it forward only
for successful building records; roads remain contribution-free. The same
make_payload selector and owner facts are used before live creation. Existing
Place-seam caveats remain unchanged; no new generic transaction mechanism.
The legal two-record arm builds/debits twice. The late-overflow arm aborts both
records and preserves complete native state, money/materials, IDs, PRNG and tiles.
The default non-atomic arm still accepts the first and refuses the second.
Running that regression against d55 apply.cpp (all other corrected tests retained)
passes the nonempty legal control and fails the unchanged-native assertion;
`B4/mutants/atomic-*` and `atomic-negative-driver.log` preserve it. Corrected source
was restored and rebuilt.

Two requested coverage details were added without changing runtime rules:
World::advance(UINT32_MAX) followed by saved odd-ms surplus/+1 and a further1999
ticks; exact65536-tick word alias retains bills (intentional original gate, no
catch-up). A genuine House count_mismatch fault precedes due booking at tick3000;
the same legal stocked house upgrades and books when its derived count is valid.
The first attempt incorrectly expected a resident billing fault in the House
phase; its retained `engine-correction-01.log` failed that assumption because
purchases happen later in the figure phase. The actual House-fault control fixes
the test setup; no phase was moved to make it pass. Corrected exploration02:
15/15 cases,956 assertions. Independent re-review and focused correction gates
remain pending at this paragraph's commit.


### B4 approval and release

`/root/econ_b0_review` independently APPROVED clean
`ff0953518e7d37872abde1e1f78cc92a8901e3ba`, no remaining P1/P2. Corrected
Debug02 and ASan/UBSan02 each selected34/executed34, failed/empty/skipped/
incomplete/sanitizer0, six required invocations each1/1 separately, GATE_END0.
Debug02 SHA256 `22d05637656a32614268b71bf14c68ff6611a666ed7280b86e746a4070ca5c6e`;
ASAN02 SHA256 `0e619055e79c79d312f04e9076db834ff369e5166cdc872cca9db6ed56c20d48`.
`B4/receipt.json` freezes binary/config/selector/fixture identities; reviewer notes
retained `/private/tmp/econ-book1-b4-review.md`. The d55 gates remain evidence for
the original source only. B5 is automatically released; its original economic
loop and B7 final interactions are still required.

## B5 complete original Tutorial economic loop — candidate

The prerequisites were proved before the long run. Normal
`Tutorial/02Trade01.sze` import now succeeds with all3 original actors and the
complete supported original script; the canary uses that normal path, with no
actor projection, source edit or scenario permission bypass. The first cheap
probe wrongly assumed the older component fixture was still necessary and failed
that assumption. A retained second setup error left the explicit projection
argument in the helper; the corrected prerequisites04 pass33 assertions. Compiler
and zero-case exploratory attempts are retained, never acceptance. The separate
existing strict-refusal/component test for Single_24 remains a regression only.

Target: original island7/city1/owner0,13 homes; climate5/tax0; food17408 is inherited,
alcohol in the potato farm's raw/output, city warehouse and Inn all0. Treasury50000,
tools17920, wood14336 can pay a genuine rebuild (250 gold,512 tools,1024 wood).
Initial civil history354/military0, original farm809 costs8/20, Inn1335 costs0/20,
stall1310 costs0/5. No grant, bill write, teleport, goods/price/cost override or
free-build flag participates in this real arm. Synthetic B4 boundary setups remain
separately labelled. The original actor importer and native records are retained.

Exploratory complete loop01 passes one case/62594 assertions. The farm produces
alcohol from its own original potatoes, which reaches at least3 originally empty
households. At150000 ticks the farm is demolished by the owner. The supply drains:
Inn sales stay312 through270000. At270000 a normal paid BuildCmd rebuilds809 on the
original footprint, checking available budget and exact gold/tools/wood debits.
A native-forked control never rebuilds. To510000, the recovered run sells1014
versus0 additional sales in the no-rebuild control;6 households receive during
recovery versus0. Newly made alcohol is conserved as farm output+warehouse+Inn+
sold+living cargo; starting alcohol on that producer path is0. The large food
store is explicitly inherited and not counted as newly required production.

Every booking computes its expected ordinary-domain numerator from the pre-visit
bills and original climate/tax price rows, divides after +128, checks historical
cost sum/15, gross credit/debit and three statistic values independently of the
engine helper. Every owner's balance reconciles on every advance, not merely at
end. Booked bills+pending bills equal actual purchase billing diagnostics by goods;
returns/native retry do not create extra income. Command-time unrelated city
bill/cost state and rival treasuries remain unchanged. Other cities' legitimate
booking activity continues and has its own exact ledger. B4 supplies the focused
synthetic two-owner production boundary proof as a complement.

| Owner | Start | Earned income | City upkeep | Construction | Final | Residual |
|---|---:|---:|---:|---:|---:|---:|
| 0 |50000|1020|5805|250|44965|0|
| 1 |10000|791|0|0|10791|0|
| 2 |100000|1068|3825|0|97243|0|
| 31 |10000|0|0|0|10000|0|

765 bookings,183 positive. Owner0 loses5035 overall: this is an operating economic
loop, not profitability or a balanced opening. Target upkeep changes23→22 after
removal and22→23 after rebuild. Decisive recorded target row tick18000:
price50×bill286=14300; floor((14300+128)/256)=56; upkeep354/15=23;
49816+56-23=49849. Owner1's first row tick10000 is50×47=2350;
floor((2350+128)/256)=9, upkeep0,10000→10009. These rows and all others are retained
in `B5/exploration01/ledger.tsv`; no copied expected answer drives the engine.

Five native cuts compare complete continuation state, events and per-site draws:
before/after booking, during an actual purchased resident trip, after removal and
after paid rebuild. Host event history is drained before comparing continuation
(the host queue is not native state). Seven new full-town checkpoint hashes are
explicit engine recording fixtures in `econbook1/loop_checkpoints.txt`, not an
independent fidelity oracle. Original actors and positions/resources are imported
normally; no movement/setup override is used to create a supply route.

Fresh-process exploration: genuine record10209 entries,7/7 checkpoints through
510000, exit0 (15.86s Debug); changed legal demolition names the owner's other
farm259,352, same valid framing/identity,3/4 checkpoints then divergence at entry3804
(tick190000), exit1 (10.08s). The corrected test proves that altered public command
is accepted at the actual150000 cut. The earlier three checkpoints match. Raw
`B5/fresh-{genuine,changed}-exploration-01.log` and original1.2MiB records are retained;
original-derived saves/logs stay ignored, not committed. Canonical replay tests
use the existing status+output helper and its mandatory contract, ordered after
one producer run through CTest fixtures. No new generic evidence framework.

Focused B5 gates use900s/case, enabled admission,1 CTest/four build jobs and required
assets. Debug measured fresh-process cost15.86/10.08s and B4 sanitizer ratio about3
justify this bounded timeout; the unsupported old1800s script case is unrelated.
ASAN environment is retained explicitly in the CTest property include. First
property-inventory attempt used unsupported CTest set_property(APPEND); it is
retained as a failed inventory, corrected to set_tests_properties with explicit
environment. The successful inventory pins producer/consumer fixture ordering.
Independent B5 review, canonical gates and final B7 remain pending.

### B5 first gate and explicit new-goods attribution

Frozen12f9afaa canonical Debug01 selected6/executed6, all other categories0,
GATE_END0; producer23.27s/62603 assertions, total selected65.61s. Its genuine
record SHA256 `a790ceb5e03ca51602537bcb8921a7a98bb288da95e5deed08d78eee97c3917f`
exactly equals exploration01, so retained raw CLI outputs concern the same record.
`B5/receipt-12f9.json` preserves source/binary/log/record identities.
Before ASAN the author strengthened an assertion gap: positive overall income
could otherwise come solely from inherited food even alongside new alcohol sales.
The test now asserts positive marginal booked income attributable to good29:
for each target booking, gross(all rows)-gross(without29), retaining sum-before-
rounding in each expression. This is a diagnostic calculation; it never edits
prices, bills or a real world. Independent TSV derivation gives217 gold and1014
booked alcohol units through510000. All3 original actor identities/kinds/owners
are also checked present at completion. These test-only additions do not change
the saved trajectory or checkpoint pins. Independent review and corrected gates
remain pending.

Reviewer `/root/econ_b1_primary_review` identified a related native-cut predicate:
ResidentCargo permits a named goods slot with amount0. The corrected trip
selection requires a nonempty cargo collection and at least one named slot with
positive amount, rather than merely a goods ID. This makes the cut a purchased,
loaded return trip. No production state or replay trajectory changes; native probe
outputs may legitimately select a later trip. This finding is fixed before ASAN.


B5 corrected Debug02 atf508fc2d also selected6/executed6, all other categories0,
GATE_END0. `B5/receipt-f508.json` preserves its identities. Reviewer independently
re-derived all765 ledger rows and each owner's totals, plus the354 civil-cost
sum and original rebuild budget from raw source. A final reporting correction
separates target alcohol units1014 from global booked29 units2024 (NPC town1010).
The217 marginal-income observation is already target-only. Final source now
tracks/asserts a positive target unit count explicitly and labels both scopes.
This only changes observations/assertions, not engine state. Debug03 reruns the
changed producer; earlier fresh-process passes are reusable only with equal
record hashes. ASAN runs the whole6-test slice at this final source, with its
positive-cargo cut and native actor-preservation assertions.


### B5 approval and release

`/root/econ_b1_primary_review` APPROVED clean
`9801afcf91387865c278324c40d0c50bace53120`, no remaining P1/P2. Final Debug03
changed producer1/1 (62616 assertions), all other categories0, SHA256
`1f3e9d53e5d87423530fccc08933d589fc95e2b7d0d2eef76133c261ea95ec30`;
prior full Debug02 atf508fc2d6/6, SHA256
`9f735fef087e1201a93abafacfbc1a175937cface1af104372ca2b5b952432bf`.
Final ASan/UBSan01 at9801afcf6/6, failed/empty/skipped/incomplete/sanitizer0,
GATE_END0, SHA256 `b849e31231310594d91807f27d4a14674dab1e3e0aafdfb8eb94623da0c997c1`.
Six required contract invocations separately1/1 per gate. Sanitized producer74.85s;
selected slice212.91s; picture prerequisite120.39s. The900s timeout was adequate,
with no restart, admission bypass or sanitizer waiver. Genuine records across
exploration, Debug and ASAN have the same exacta790ceb5... SHA above; final ASAN
actually re-executes both CLI arms. `B5/receipt.json` preserves all artifacts,
source/config/binary hashes before later builds. Reviewer report and raw-derived
inventory retained `/private/tmp/econ-book1-b5-review.md` and
`/private/tmp/econ-book1-b5-primary.json`. B6 is released; B7 remains required.

## B6 bounded scaling and Windows consumer contract — candidate

The synthetic measurement explicitly presets pending bills, uses original-default
roles with data-sized15/31/63 consumer rows and1/4/8 populated cities, and imports
0/64/256 unrelated inert buildings plus0/32/128 idle figures. A separate series
holds1 city/15 rows fixed while increasing unrelated objects. Shared smallest
control yields5 unique legal fixtures,3 samples each. Every fixture first passes
World::create/invariants, nonempty bills, positive actual scheduler income and
nonzero upkeep. The only producer per city is suspended to keep this a bounded
booking workload; no claim of real production is attached to these synthetic
bills. The real arm is B5, with no grants or bill writes.

Deterministic observations count15/124/504 booked rows for the growing C/G series
and15/15/15 for fixed C/G with unrelated objects; one producer selector inspects1
line regardless of world size. These count configured booking rows and selector
lines, not every CPU instruction. Normal World::advance still visits its scheduled
buildings/figures; total world wall time therefore includes that baseline work.
The code's added booking work resolves configured goods then calculates them in
two linear passes per due city. Cost updates read the changed payload/owner and
two history buckets, without a world-building repair scan. Only whole-city
cleanup scans that city's remaining buildings. Direct source/caller review is
part of the no-global-scan proof; a row counter alone cannot detect an arbitrary
extra hidden scan. B3 already proves wider climate/tax/id dimensions and an
independent data-defined producer/service without a C++ id-list change.

Debug exploration01 passes1 case/251 assertions. Raw Debug timings are exploratory,
not Release claims. A fixed-original15-row scheduler mutant retains the legal
15-row production control but must fail the31/63 row-count/income assertions;
`B6/fixed15-*` and `mutant-driver.log` preserve statuses. Source is byte-restored
and rebuilt before acceptance. First compiler attempt's explicit-ID initializer
errors are retained; no compile failure is counted as a mutant kill.

Release uses a separate `build/econ-release`, -O3 -DNDEBUG, AppleClang21/CMake3.31.10/
Ninja, no GPU/viewer, same bench_picture-only warning exception. Measurements
separate5000 pure calculations from100 complete public advances,3 raw samples per
fixture, no machine-specific latency assertions. Source/config/binary/sample
manifest and process peak RSS are required before closing B6. Host observation:
Mac17,6 arm64,18 logical CPUs,48GiB,~1.5TiB free; no competing executor build/test
was observed (the recorded own mutant build was still completing). Admission
remains enabled; four build jobs/one CTest, serialized own expensive gates. Current
host conditions are re-recorded at Release measurement. Windows/Linux/large
multiplayer/late-game timings are NOT_RUN.

Windows-facing consumer delta (candidate133, not adoption authorization):

- Native state133/schema `ECONBK1\0` (exact bytes45 43 4f 4e 42 4b 31 00,
  little-endian value00314b424e4f4345), PRNG5, replay4, command wire1, save container1.
  Actual132 save/log archives refuse at the version boundary. Regenerate trajectories
  when adopting this candidate; never relabel published132 in place. Orchestrator
  owns eventual combined identity and target-host adoption.
- `Rules::city_booking` is separately hashed/defaultfalse. House updates remain
  separately defaultfalse; supported real town explicitly enables both. Booking
  requires saved billed goods. Session configuration/rules are serializable;
  no new live toggle, tax UI, debug money mutator or unversioned mod hook is added.
- Goods metadata resolves data-sized climate prices and tax surcharge columns
  against sealed role names/count. WorldSettings.prop10 is the existing selector,
  widenedu32; Island.climate remains the island selector. Missing price metadata
  is explicit constructor-zero pricing. Invalid role/dimension/final-price/unknown
  financial records name the invalid field and refuse transactionally.
- CityAccounts persists word stamp plus fiveu64 histories (sales, trade sales,
  trade purchases, civil/military costs). Native load keeps history exactly;
  normal original insertion reconstructs costs once. BuildingType.statistic_kind
  supports city buckets1/2; the26 audited original class getters all return1,
  including military-looking buildings, without mirroring into Player upkeep. u32 bills retain the verified
  direct-fault/queued-saturation policy. Original HEAD statistics use checked wide
  import semantics. House/research history is deliberate, not repaired on every visit.
- `SimEventKind::CityBooking` appends an optional typed ledger: CityRef, owner,
  income/upkeep/rows, before/after signed money, AccountingFault. This output is
  observational, not authoritative state or a second money effect. Existing
  account/money/bill queries supply future UI observability. Consumers should
  handle the appended event kind and `Reject::CityAccounting`; command refusals
  are intentionally coarser than pure AccountingFault reasons. A failed whole-city
  cleanup can follow a completed named demolition, so inspect World::fault too.
- A booking failure is sticky/fail-closed: money/bills/stats/stamp of that attempt
  stay unchanged, earlier independent work stays committed, native faulted reload
  remains stopped. Retry from an unfaulted cut with a legal correcting command;
  no clear-fault API. Typed reasons distinguish product/sum/rounding, gross credit,
  debit, cost sum, decay/sales, dimensions/owner/city/type/financial history failures.
  Player upkeep/remainder/saturation and negative-balance support remain separate.
- Standard C++20 portable integer checks; no __int128, new OS API, draw site or wire
  command. MSVC build/UI integration and a live Windows economy remain NOT_RUN.
  Renderer/viewer, cloth/hunting/mining bodies, ownership commands, mod SDK and
  economic balance/profitability are outside this packet.
