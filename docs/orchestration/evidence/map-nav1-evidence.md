# MAP-NAV-1 acceptance and evidence matrix

Scope: ship navigation evidence, contract and fixtures. Static analysis only; no engine code, no state/command version
change, no candidate/active-map/asset/coordinator/W4 change. Private raw material (disassembly dumps, emulation harnesses,
per-topic reports) is in the ignored `cache/map2-nav1/` and is **not** a portable prerequisite: every conclusion is
restated with addresses in `docs/re/ship-navigation.md`.

| Packet item | Result | Where | Status |
|---|---|---|---|
| N1.1 tool inventory, hashes, bases, versions | module hashes/bases/tool versions recorded | re 6 | done |
| N1.2 order -> plan -> segment -> tick -> arrival -> founding | player path net `0x84` (right-click) and host path `0x6c`; SetOrder builds no plan; pick-up at segment end; arrival sets no flag | re 8, 7.2 | done except the UI pick rounding |
| N1.3 existing leads reconfirmed | `0x1003de40` is the local detour, not the route builder (corrected), `0x1003e930`, `0x100428d0`, `0x100427b0` confirmed | re 6, 7, 10 | done |
| N1.4 call/field ledger | state words, segment fields, order record, persistence, PRNG | re 7-10 | done (function-level ledger in the private reports) |
| N2.1 map representation, water/land, diagonals, clearance, endpoint, collision | two-level route engine; tile-average land rule; no corner cutting; no hull/shore dilation; goal acceptance 6.0; local obstacles as density stamps | re 9, 10 | done; fine stage verified under original code for interior tiles only |
| N2.2 ties, failure, work limits, replan, cancellation | FIFO ties, label at discovery, no node cap, learned masks <= 8, codes 0/2/-2, no cache; replan triggers; stop/replace | re 7.4, 8, 9 | done |
| N2.3 comparison with exterior-4 on authored cases | table and executable catalogue (16 + 2 cases) | re 11, `fixtures/synthetic/sim/nav/passage_cases.txt` | done |
| N2.4 consequences for the three candidates | no placement or label changed; witnesses stay tool-only | re 11 | done |
| N3.1 time/cadence, segments, turn, facing, arrival, modifiers, `+0x3c` vs founding | registry visit cadence, 5-tick unit, `d >= 3` turn, 16-direction facing, speed factor integer formula; founding reads bit `0x02`, not `+0x3c` | re 7 | done |
| N3.2 persisted vs derived state, crew | saver contents; crew not moved/copied per tick; release path of a destroyed ship | re 7.8 | done |
| N3.3 PRNG | none on the sailing path (indirect-call scan); damage draws | re 7.9 | done |
| N3.4 integer representation | 2^-32 tile accumulator, tick x5 scaling, exact f32 rationals; Q24.8 velocity rejected with numbers | sim 4, re 7.10 | proposed adaptation for decision |
| N4.1-N4.2 vectors and reference with negative controls | 43+ movement vectors, 18 passage cases, 6 + 5 mutants caught; reviewer's 37 further mutants | `tools/ship_nav_ref.py`, fixtures, ctest `ship_*` | done; route-run (Trace) vectors not included (stated) |
| N4.3 original-runtime capture card | CAP-1..6, 6c | `docs/testing/ship-navigation-capture-card.md` | NOT_RUN (queued) |
| N5.1-N5.5 contract, bounded work, config, visibility, packages | `docs/sim/ship-navigation.md` | sim | draft for review |

Resolved by the second static round: coarse-map build timing (per island at placement), vertex-grid padding, search window (start-centred, 1024),
hook radius (16.0) and stamp radius (5.0) sources, state `0x800`/`0x8000` semantics, the route engine's unit query and stamp semantics
(stamps affect only the goal-candidate choice), fine-stage block edges (4400 + 600 randomised cases on the original bytes, no deviation),
the multiplayer player-id path (unauthenticated), 23 + 7 emulated route-run vectors. Open static items (not blockers for contract review):
the UI pick provider and group order body; object-top contribution to block extrema / tile type 9; setters of state bits
`0x2000`/`0x4000`; the cell-list effect of the registry position update callers. Runtime-only: x87 control word, registry cursor at load,
land-start-as-candidate (CAP-6c), and confirmation of the builder call pattern at load (CAP-3).

## Review ledger
- N1-N3 evidence note: independent re-verification of 15 load-bearing claims against the binaries: CONFIRMED (stepper, segment start,
  speed factor, land/coarse thresholds, expansion order and costs, goal score and retry cap, SetOrder validation, hook callers,
  player-command chain, FigurenHeader class bytes, registry cadence, stop trim); CHANGES REQUIRED for 9 findings (hook trigger
  description, PRNG scan scope, crew release path, order-id branches, 36-values qualifier, persistence details, plan-commit semantics,
  citation, 113 wording) and one contract rule demoted (tile-centre jump -> UNKNOWN). All applied.
- N4 references: all 43 movement and 16 passage values re-derived independently, none wrong; 37 further mutants run, 8 survivors
  (rounding of L, negative hp truncation, positive wrap, chunk limit, coordinate rounding, map-edge bound, route-run coverage,
  land-start candidate): vectors added or limitation documented; `emulated` label corrected to `coarse-emulated`; capture card
  completed; Python-missing warning and empty-fixture guards added.
- Final contract review (fresh): CHANGES REQUIRED with 13 findings (atomicity label, owner-check scope, world-level coast index, route-engine
  obstacle query values, window rule, i64 widths and missing integrator references, seam fit with `scheduler.cpp`, serial planning, deferral
  semantics, crew label, P2 acceptance overstatement, unread static dependencies, codec note). Applied; the unread static dependencies were
  then read (second static round) and integrated; the remaining open items are named above. A focused re-review of the corrections follows in
  the REPORTS entry.
