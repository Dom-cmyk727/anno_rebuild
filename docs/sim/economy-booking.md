# ECON-BOOK-1: resident sales and building upkeep

Status: research started; engine implementation NOT_STARTED. This is an
orchestrator-owned parallel package, based on MAP `ec0e832e`. It does not replace
MAP-POP-CLOSE-2, approve its unfinished gates, or enable live house updates.

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
