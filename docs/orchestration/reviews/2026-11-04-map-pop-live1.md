# MAP-POP-LIVE-1 — changes required; retain the completed components

Reviewed locally from accepted `73e7ab1a129c4712747333d9f34a225be03f063f`
through executor report `135edf6f`. Runtime candidate `aca26cea`; earlier Debug
source `2deccd98`. Reviewer: `/root`, direct read-only source review using the
review-agent skill. No executor source was edited or rebuilt. Small probes were
compiled outside the checkout; no broad gate was run by the orchestrator.

**Not accepted as complete live gameplay.** Resident transactions, fetch jobs,
transition components and the opted-in scenario runs are substantial progress.
They remain useful evidence. The released L4/L5 conditions are not met by a
disabled default, unclosed reached effects and no real upward transition.
The last accepted and published MAP pin remains `73e7ab1a`. Windows keeps the
exact inputs in its own dispatch, including `73e7ab1a` for W4-OPENING-2.

## Actionable findings

### F1 — [P1] Keep unsaved billing scratch from deciding authoritative outcomes

`src/sim/src/resident.cpp:218–220` refuses a purchase when the billed counter
would exceed u32. With `save_billed_goods=false`, that counter is deliberately
excluded from hashing/saving and resets on reload. It therefore influences
world behavior without belonging to world identity. A valid world and its
reload can start at the same hash and diverge on their next purchase.

Root reproduced this through the live scheduler with the existing
`house_live_test.cpp` synthetic registry: House `kP` at (110,110), residents 320,
E 640, stocks {0,0}; stall at (114,110); city food 5000; billed food `UINT32_MAX`;
`house_updates=true`, `save_billed_goods=false`. Invariants pass, save/load
succeeds and hashes initially match. Advancing both gives:

```text
live fault=1 reloaded fault=0 hash equal=0
reason=resident fault billed_overflow building 2 figure 1 goods 0
```

Fix the contract as well as the code. Retain the default saved/hashed counters.
Either make unsaved scratch incapable of changing authoritative results, or
reject that live rules combination explicitly until a supported implementation
exists. Do not silently clamp a saved gameplay counter or claim exact resync for
an intentionally lossy mode. The adjacent below-bound purchase must still work.

### F2 — [P2] Record the population after the tick, not its starting value

`src/sim/src/house_tick.cpp:217` assigns `tr.residents_after = residents_before`.
The kernel has already changed residents before SetGroup preserves them. The
diagnostic record consequently lies on a growth-plus-transition tick.
`population_live_canary_test.cpp:671` pins this stale record instead of reading
the resulting house.

Root attached `TransportStats` to the existing live downgrade fixture, with
residents 300, E 512 and stocks {1000,800}, then advanced to its first taken gate:

```text
record before=300 record after=300 actual after=284
CHECK(record.residents_after == actual_house.residents_64) fails
```

Read the committed house after a surviving transition. Define removal records
separately. Compare the diagnostic to authoritative state and a hand-derived
delta; do not merely replace one self-baselined literal. The growth canary also
currently calls every unexplained delta a cap/floor clip; assert the applicable
cap/floor before attributing a delta to clipping.

### F3 — [P2] Bring the stage reference up to the independently checked bit rule

`tools/house_live_ref.py:82–83` only ORs the city mask into player flags. It omits
the bit-26 implication from newly set bits 25, 27 or 28 that the C++ implements.
The existing high-bit vector starts with those bits already set and misses this.
Root independently checked the relevant original SetProperty instructions.
This single reference input exits 1 with one disagreement:

```text
stage high25 table=25:0:5 mask=0x0 stamp=0 t=2500 above=0:5 player=0x1 => stamp=2500 mask=0x2000000 player=0x6000001
# reference currently computes player=0x2000001
```

Add distinguishing rows for 25, 27 and 28, plus a non-implication control, to
both the reference and its engine comparison. Keep the low-23-bit comparison.

### F4 — [P2] Finish through the prescribed gate rather than the older local wrapper

The saved Debug and ASan acceptance attempts invoke the MAP branch's older
`run_selected.sh`, not main's canonical Python workflow explicitly assigned in
L5. They lack immutable inventories, executable/fixture/workflow identities,
the final source check and `GATE_END`. They are retained execution evidence,
not a canonical final acceptance. The ASan selector also does not require the
resident/transition/live reference contracts or agreement checks.

The saved counts reconcile under the current verdict reader: Debug 1620
selected = 1610 executed passes + 1 lint failure + 9 skips; ASan 260 selected
and executed passes. `2deccd98..aca26cea` is only the lint allowlist; subsequent
`135edf6f` changes REPORTS and the review ledger. A prose/allowlist-only delta
does not justify repeating an entire matrix. Keep the original failed Debug
result and the focused lint correction separate. After the substantive fixes
below, use one canonical affected Debug/ASan closeout and the required references.

## Disclosed component gaps that still require implementation

These were partly documented by the executor; they are not presented as newly
discovered hidden regressions. Disclosure does not satisfy the released bundle.

**G1 — Fresh House initialization is not wired.** The new creation path only
stamps `last` (`create.cpp:230–232`). `make_payload` still supplies five residents,
E 640 and empty need stock for every House group. No production caller invokes
SetGroup with a null old group. A probe with group 1, Wohnmax 15 and a forced
food need Menge 100 gives fresh payload {residents=320,E=640,stock=0}, whereas
the implemented first-assignment calculation gives {960,1920,16000}, one draw.
Group 0 also misses its specified initial food stock. Complete play-created
House initialization, preserving explicit imported payloads, exact own saves
and the rule-off baseline. Pin public BuildCmd's state, payment and draw order.

**G2 — Cyclic substitute goods are accepted and repeatedly credited.** P-3
promises registry rejection, while §19 admits it is missing. `house_deliver`
walks the cycle until the unrelated registry size ends the loop. A valid built
registry with goods 26→27→26, needs for both, and delivery amount 1 produces
144 additions and stocks {1152,1152}, without a fault. Validate cycles and
self-loops; a runtime defensive path must refuse before changing stock, rather
than crediting a repeated cycle. Do not confuse two legitimate goods sharing a
need slot with a cycle of goods ids. Adding unrelated goods must not change a
delivery's result. Update the vector that currently expects cycle amplification.

**G3 — First-assignment faults can consume randomness.** With Wohnmax 65535,
forced Menge 65535 and draw 0, `house_set_group` consumes one draw, then returns
Overflow. The header's no-draw-on-fault promise is contradicted; §19 discloses
the exception. Close it before wiring G1. Validate the supported domain before
drawing, or use an appropriately transactional proposal. Preserve legitimate
draw order and exact bounds; do not impose shipped counts as engine limits.

## Primary evidence, review limits and retained results

The original binaries were accessible on this Mac, at the main checkout rather
than MAP's absent local `game/app` directory:

```text
/Users/dom/Dev/anno1503-worktrees/anno1503/game/app/AnnoWelt.dll
SHA-256 f220a878510f740c4cfb7224ade0a260e9a0fd7e99113e1201e7a37c0dfa6ae2
/Users/dom/Dev/anno1503-worktrees/anno1503/game/app/AnnoGame.dll
SHA-256 ac721220afd27ebca5fd2ece6a8ff452289b9d7458c4e091c68adea2012db7bc
```

Root used the existing private disassembler environment and checked selected
decisive instructions, not every population claim: factory House SetGroup call
at AnnoWelt `0x10024c3c`; upgrade next-id arithmetic at `0x1002e000`; current-kind
test at `0x1002e080`; demand `0x100765c0`; recursive substitute delivery
`0x10076610`; billing `0x1002dac0`; player flag implications in
`0x10020df0..0x10020ecb`. These support the findings and distinguish the current
type test from the unproven next-type policy. No original-runtime run occurred.

The small probes are diagnostic counterexamples, not acceptance gates. They
used current source for the transaction helpers and the existing Debug support
libraries; the record/reload probes used the existing live fixture and static
libraries. The first attempt to call doctest fixture helpers outside a testcase
exited 139; it is not a product finding. Re-running them inside a registered
testcase produced the specific failing assertions above. MAP remained clean.
The four diagnostic outputs and their commands/statuses are retained in main's
ignored `cache/map-pop-live1-orchestrator-review/INDEX.json`. The component probe
prints its observations and exits 0; record, billing and reference probes each
exit 1 at the intended disagreement. These are not canonical gate results.

The ledger's missing L0–L3 reviewer ids remain missing. Do not invent receipts
or repeat all prior reviews to repair the archive. Evidence bookkeeping and
task-board planning were delegated; root performed the source review directly.
Retain the 103 purchases, 100 deliveries, 11 removals and second-file downgrades
as the executor's bounded opted-in evidence. “241 growth steps” means resident
changes, including decline (the first is 64→48), not 241 increases. An upgrade,
material payment/refusal and changed player flags remain unproven on real data.

The older logs can be re-read, but missing provenance cannot be reconstructed.
Replayed artifact hashes and the mutation dispositions checked by the bookkeeping
audit are retained; no fresh process replay was rerun by root. Source and gate
coverage do not establish original-game fidelity or multiplayer performance.

## Decisions and finite next boundary

Release **MAP-POP-CLOSE-1** in TASKS: correct F1–F4/G1–G3, close the narrow
creation/service/route dependencies, and demonstrate useful positive progression.
Proceed through its internal package reviews without another orchestrator stop.

- Keep `house_updates=false` globally for now. An explicit enabled headless
  scenario is authorized for development and the final progression proof. Do not
  describe this as enabled in the live window or fully accepted town gameplay.
- Keep P-14 as a protective development guard, not proof of original behavior.
  Trace valid successors and distinguish terminal special types from malformed
  mod data. Do not silently convert invalid group links into “not eligible”.
- P-15's zero-loss result is useful but does not prove reversed edges are legal.
  Validate direction, endpoint entry and unreachable cases before adopting the
  scoped resident adapter. Do not change cart/harvester routing incidentally.
- Preserve safe checked arithmetic, data-defined roles, derived counters and
  exact deterministic own-save behavior. No original crash or overflow is a
  requirement to reproduce. Keep adaptations explicit and configurable where
  they change gameplay; do not add a general mod framework to this closeout.
- Readable creation/wait/timeline callees are work to finish, not an owner
  preference question. Runtime-only unknowns remain NOT_RUN with exact limits.

Final scope: a production-path newly built house supplied by real walkers,
positive growth, one legal upgrade and exact material debit, a refused upgrade
control, stage/player-mask change, reload/replay equivalence, bounded work and
one canonical affected gate pair. No new broad renderer, coordinator, sailing
or 35-scenario search campaign. Return once with the whole package or an exact
unresolved dependency; never rename missing progression as completion.

Documentation review: `/root/evidence_bookkeeping` and `/root/dispatch_planning`
independently reviewed this root-authored review/assignment against main
`ef91f70e`, with no P1/P2 findings. Their shared P3 (a nonexistent reading-list
filename) is corrected to `population-contract.md` and `population-inputs.md`.
They audited documentation/evidence attribution, not MAP source correctness.
All eleven existing W4 task sections and the Windows dispatch row are preserved
byte-for-byte from that main pin.
