# MAP-NEEDS-2 orchestrator review — 2026-11-06

Candidate `5df34b38`, tested source `e337c75d`, base `5f6092b4`.
Verdict: **CHANGES REQUIRED for bounded acceptance; hunting remains PARTIAL.**
No candidate merge or executor-worktree edit was performed. Current main before
this review was `c5c7007f`, state 133 / ECONBK1 / PRNG 5; the report's main
`3b74201f` and candidate state 132 are historical.

## Findings

1. **P2: in-flight cuts are not scoped to the target chain.**
   `src/sim/tests/leather_persistence_test.cpp:40` scans all figures. A rival's
   hide/leather cargo can select a cut; resident cargo requires only a goods id,
   not a positive amount. Require the intended owner, city/home and job/producer
   relationship plus positive cargo. Prove unrelated loaded figures and a
   zero-amount resident cannot select a cut; a target figure must select it.
2. **P2: recovery reconciliation mixes scopes.**
   `src/sim/tests/cloth_canary_test.cpp:269` sums world-wide market-cart cloth and
   workshop-carrier wool against target-only producer, store and household totals.
   Restrict terms to the target chain, including old/replacement producer handling.
   Audit adjacent island-only helpers: use exact CityRef, or assert the one-city
   assumption and label the scope. Positive rival cargo must remain excluded.
3. **P2: original-scenario claim is too broad.**
   The cloth case's `unmodified original` title uses `tutorial_world()`, whose
   loader explicitly requests `original_component::actor_free_units()`. Name it
   an original-data component projection, disclose omitted actors, and retain
   the valid production proof. Do not replace this fixture with a full scenario
   campaign merely to repair the wording.
4. **P3: scale conclusion exceeds measurements.**
   `docs/sim/early-needs.md` section 15 cannot infer absence of per-visit scans
   from 76/87/88 buildings without operation counters. Keep the measured timing
   sample and limits; remove that causal inference.

Independent read-only reviewers `/root/review_consolidation_plan` and
`/root/review_econ_evidence` inspected the candidate diff, helpers and retained
logs. No production defect was found in the guarded workshop diagnostic addition.
The independent reviews do not waive the four corrections above.

## Evidence retained and finite closeout

The original canonical logs genuinely record Debug 1600 selected / 1595 executed
passes / 5 opt-in skips and ASan 112 / 110 / 2 at clean `e337c75d`. Both omit a
build target; a bench object compiled manually with `-Wno-error` leaves clean-build
acceptance open. This does not turn the simulation's executed passes into failures.
Manual fresh-process replay retains genuine 9/9, altered command divergence  at 101,
altered checkpoint divergence  at 505 and refused-command reproduction 1/1.

Revalidate only the corrected cloth/leather/persistence scope and dependencies,
in Debug and ASan, on current main 133. Keep old 130/132 recordings; re-pin current
identity hashes only after numerical production/payment/lifecycle ledgers agree.
Run the four replay dispositions at the new pin. No full 1600-case rerun is required
solely for the benchmark fix or these local test corrections.

Hunting was explicitly allowed to finish PARTIAL under the original assignment.
It needs fauna sweep `0x10093480`, creation `0x10092ca0`, route-62 animal behavior
and HIRSCH loading `0x100946c0`, with world-PRNG consequences. No invented spawning,
seeded-hide hunting claim, timeline amendment or wildlife implementation is released
by accepting cloth. Salt extraction is a separate follow-up.

## Orchestrator-owned CPU-only build correction

`b408f311` marks only five GPU-only option values `[[maybe_unused]]`; parsing,
validation and unsupported-GPU refusal remain intact. No target warning flag was
weakened. `/root/audit_completed_refs` independently approved the code correction.

Reproduction: the unmodified main translation unit fails with five
`-Werror,-Wunused-but-set-variable` errors when ANNO_BENCH_GPU is absent. Corrected
GPU and no-GPU syntax checks pass. A fresh no-GPU/no-viewer CMake build compiled
and linked all 139 bench dependencies normally. Canonical gate at clean `b408f311`
rebuilt `bench_picture` normally and passed `picture_matrix_contract` and
`window_table_contract`: selected 2 / executed 2, no failures, skips, empty or incomplete.
No GPU runtime or Windows/Linux target compilation was run for this correction.

Raw Mac-local evidence: `cache/needs2-orchestrator/` (ignored), including before/
after syntax logs, command JSON, configure/build logs and canonical gate/inventory.
Gate SHA-256: `d9f37430dab4100a2e5aa18bca7d93401b147c6241f66888065f1a9146d26035`.
Clean-build log SHA-256: `39627c8533dbfc0f4090e8e26331d9b3fe5447c9f49ad9a1d756f8c4bcdff3d2`.
No simulation code, assets, active map, rule defaults or state identity changed.
