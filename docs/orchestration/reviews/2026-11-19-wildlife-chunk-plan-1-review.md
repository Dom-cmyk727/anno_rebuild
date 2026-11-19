# WILDLIFE-CHUNK-PLAN-1 — independent review

2026-11-19. LOCAL REVIEW APPROVED; exact-head hosted CI pending.

- Publication `95d2eb0f4bf09c559413ef380fba3fd4281939ff`, tree
  `403b4c32a144d26442a87a45bf8fe64dc0e11e08`.
- Tested implementation `ef8fcb48349bf2bc8fe6521a23957e481cf129ec`, tree
  `70e106988054ae54ab99e5c763aabb7da04ec0bc`; publication delta is docs only.
- Base `f5f1c9fa7f0eca4d6263147797022ff1c5e6d771`.
- Hosted CI [44522148816](https://github.com/Dom-cmyk727/anno_rebuild/actions/runs/44522148816)
  in progress at review. This is not hosted acceptance.
- Task-branch contract `docs/sim/wildlife-chunk-plan.md`; evidence
  `docs/orchestration/evidence/wildlife-chunk-plan-1.md`.

## Approved behavior

The pure owned chunk planner reproduces ordered category-0 retention, recycling,
candidate scan, reservation, collision retry, insertion and cleanup before animal
materialization. Its supplied per-cell habitat and normal-success allocation
profile are explicit assumptions, not native terrain/allocator behavior.

Occupied records survive without habitat reevaluation. Rejected unoccupied records
form reverse head-reuse order; other categories keep their relative order.
Candidate scan is row-major with swap-pop removal. Any first matching category-0
record can collide, regardless of its occupancy, subject to the category flag.
The same reservation survives retries. Goods and position change before lookup;
timestamp and clearing occupancy happen only on successful insertion. Reused
unwritten fields retain values; unknown fresh fields stay unknown. Excess records
are not trimmed. Cleanup discards unused reuse only after successful completion.

Failures own exact partial diagnostics and consumed counts, never an apparent
successful chunk or original rollback. Local record tokens, including known zero,
are distinct from missing saved figure IDs. No World IDs are allocated.
The [C0 reachability disposition](2026-11-19-wildlife-chunk-plan-c0-review.md)
remains: consistent complete inputs cannot exhaust candidates too early. The
lower-level exhaustion proof is explicitly not full-adapter branch coverage.

## Independent reviews and evidence

Primary reviewer `/root/review_docs_sim_dispatch` APPROVED with no actionable
findings. It independently reread retention, candidate and cleanup instructions,
with the previously checked reservation/probe writes and C0 proof. Optional
knownness, ordered effects, flags and partial snapshots agree within the profile.

Evidence reviewer `/root/review_town_ci_close` APPROVED with no actionable findings.
It verified all 110 receipt hashes and 1019 frozen identities; independently
re-derived picks 9 (collision), 59, 11, 57 and final tokens
[50,10,30,90,20,40]; and checked literal partial snapshots and known-zero controls.
All eight actual production-TU mutants fail their intended semantic assertions
after a legal 7-case/268-assertion control. The earlier mutation-harness pattern
failure happened before mutation and remains preserved, not counted as a kill.

The root also verified all 110 closeout hashes. Private manifest
`cache/wildlife-chunk-plan-1/closeout-sha256.json` SHA256:
`b57d0cde2c68a4a8c2744677225777a2ed93c3887a57985ac5b1a9430dec36e9`.
Debug gate SHA256:
`28b5a8365c5aee10c6f55ecbf41a2c555c7d0952df10d4a2be15a24975867a11`.
ASan gate SHA256:
`710e8866d0d65f1be8a7014e84a96e71e0e84459c7eb98d5fd3a237f3f02c858`.

Each profile executes 16 unique cases: one required plus 15 main, zero failed,
empty, skipped, incomplete or sanitizer lines. Coverage is seven chunk, four
release, two prefix, one all-family 74-vector case and two leaf controls. These
are synthetic checks, not corpus coverage. Source species/creation regressions
remain unchanged and retain prior evidence; no unrelated broad gate repeated.
Affected Apple Clang/GCC strict checks, formatting and lint pass. All 292 tracked
fixtures and predecessor evidence remain intact. No reconstructed binary claim.

## Limits and continuation

Actual terrain/native allocation/destruction, original or HE runtime, species
materialization, geometry, manager, first actions and live hunting remain outside
this result. Full/heavy/opening/pins, LeakSanitizer and local Windows/WSL were not
run. No state, default, identity, global PRNG, navigation or graphics changed.

[WILDLIFE-MATERIALIZE-PLAN-1](../tasks/wildlife-materialize-plan-1.md) is released
as a locally reviewed stacked continuation while hosted CI is pending. Independently
scope-reviewed work composes eligibility and ordered species selection with supplied
facts and exact draw accounting; it stops before live creation or unresolved numeric
policy. Main and other lanes remain untouched.

## Hosted failure update — 2026-11-19

CI44522148816 is terminal FAIL, not acceptance. MSVC build job135553390406
fails C4244/WX on int-to-uint16_t template conversion through std::optional.
Linux Release job135553390583 fails dependency installation with exit100 before
configure/build/tests; its root setup cause is not established. Other leaf jobs
succeeded and the aggregate failed. Public jobs response SHA256:
`f77010cbe2934c1d947227822f2c0d6eca2e649f25c3a4d6a03591dbe427b7d6`.
Independent bounded semantic/evidence approval remains, but the portability
correction is required. [Finite correction](../tasks/wildlife-plan-ci-close-1.md)
will run on the reviewed descendant; no old failed receipt is rewritten.

## Correction accepted on descendant

The type-only portability correction is independently reviewed and accepted at
`86e24940256d2cadf17a585b31b836b02701ae9e`, with all12 exact-head CI jobs green.
See [correction review](2026-11-19-wildlife-plan-ci-close-1-review.md).
This does not rewrite this packet's original failed CI result or claim its old
head passed. No further semantic correction was needed in the reviewed scope.
