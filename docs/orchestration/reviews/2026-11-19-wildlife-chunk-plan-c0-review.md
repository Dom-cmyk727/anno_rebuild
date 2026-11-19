# WILDLIFE-CHUNK-PLAN-1 — C0 reachability disposition

2026-11-19. Focused contract clarification APPROVED; implementation and full
package acceptance remain pending.

- Executor contract head: `4253dfbc0b4b7fff1418baf24f520c27a711bc02`.
- Tree: `c72a95c0984cb4fc56ff320d8d3eb0af3d35e416`.
- Base: `f5f1c9fa7f0eca4d6263147797022ff1c5e6d771`.
- Executor-reported CI [44517180198](https://github.com/Dom-cmyk727/anno_rebuild/actions/runs/44517180198)
  queued at handoff; no whole-packet or hosted acceptance is asserted here.
- Contract on that task branch: `docs/sim/wildlife-chunk-plan.md`.

Reviewer `/root/review_docs_sim_dispatch` independently APPROVED the reachability
argument for the complete immutable single-chunk profile. The root checked the
accepted density table over 0..16 and the retention/candidate/lookup relationships.
The reviewer also checked original instructions for attempt-stage writes.

Let N be unique eligible candidates, R counted retained category-0 records, and
C distinct candidate positions colliding with those records. Every collider in
this consistent habitat model is retained and counted, so C <= R. The accepted
density table has D(N) <= N. For a positive deficit K = D(N) - R, we obtain
K <= N - R <= N - C. Swap-pop consumes each candidate once, so there are at least
K noncolliding positions. New insertions cannot collide with later candidates,
because their positions have already been removed. Duplicate retained records
increase R without increasing distinct C. A missing category flag suppresses
lookup matches rather than increasing C. Signed-counter overflow is excluded.

The packet's requested full-chunk early-exhaustion example therefore cannot be
constructed under its admitted assumptions. Manufacturing a separate lookup list
or overriding density would weaken the model. The disposition retains a defensive
noncommittable guard and the accepted lower-level replenish exhaustion test, with
an explicit statement that this is not full-adapter branch coverage. The executor
will test reachable draw exhaustion and partial effects normally. No new gameplay
policy, bypass, whole-game OQ-572 closure or allocator claim is authorized.

Original writes at 0x1009367b–0x100936b2 set goods and position before collision;
timestamp is written at 0x1009370b after successful insertion. A reserved record
in a collision diagnostic consequently has new goods/position but old timestamp.
Fresh packed high bits remain unknown: clearing occupancy 0x40 leaves bit 0x80
unknown. Reused untouched fields remain preserved. These distinctions belong in
the implementation's literal full-record tests, not a broad new research round.

C1–C5 may proceed under the amended
[task](../tasks/wildlife-chunk-plan-1.md). No code, state or other-lane file changed
in this orchestrator disposition, and no gate was repeated.
