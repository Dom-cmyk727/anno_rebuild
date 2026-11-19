# WILDLIFE-PLAN-CI-CLOSE-1 — narrow portability closeout

RELEASED 2026-11-19 after independent source/scope review. Target 1–2 hours of
active work; hosted queues are reported separately. No new gameplay scope.

## P0 — intake and preserve the exact failures

Start `task/WILDLIFE-PLAN-CI-CLOSE-1` from reviewed
`1c027079fd0d431fae1613df839f1bde9c38d2bc`. Read this exact dispatch plus
`CLAUDE.md`, `docs/testing/evidence-contract.md`, `docs/testing/README.md`, and
`docs/sim/wildlife-chunk-plan.md`, `docs/sim/wildlife-materialize-plan.md` and both
packet evidence files. No board/main merge, rebase, reset or force push.

Predecessor CHUNK head 95d2eb0f, run44522148816, is a real terminal failure:
- MSVC job135553390406 failed build on C4244/WX: int to uint16_t through
  std::optional. Public check-run annotations retain the diagnostic.
- Linux GCC Release job135553390583 failed apt dependency installation with
  exit100 BEFORE configure/build/tests. No source verdict came from that job;
  the specific package/network cause is not established by exit100 alone.
- Aggregate failed. Do not call this accepted or rewrite its failed receipt.

Retain the annotation/step receipts and current materialization CI44528637255.
Do not cancel that running CI or rerun its passing jobs. Diagnose source and setup
failures separately. If detailed remote logs are unavailable, state that; do not
invent a package outage or claim a local compiler reproduced MSVC.

## P1 — fix the proven narrowing mechanism only

Inspect `src/sim_load/src/wildlife_chunk.cpp`: assigning literal255 to optional
uint16_t at the goods write instantiates a narrowing template conversion under
MSVC. The public annotation omits the source backtrace: this is a demonstrated
matching instantiation and likely cause, not yet the proven sole diagnostic site.
Use `std::uint16_t{255}`, preserving value255 and the exact original
write order. Check the new chunk/materialization code and their tests for the
same template-conversion pattern where a hosted diagnostic identifies it or its
instantiation is unambiguous. Include further same-cause warnings exposed after
the first blocker in the same finite correction.

Do not suppress C4244, disable warnings-as-errors, widen original storage merely
to hide a diagnostic, change rules, move writes, alter optional knownness, or
redesign APIs. No unrelated compiler cleanup. Any different failure must be
reported with its exact source/job and bounded proposed disposition.

## P2 — cheap verification and frozen focused gate

Verify the changed expression's type and its preserved value/write position.
Reuse the existing semantic controls; no mirror test of a cast is needed.
Build affected targets with the available strict compiler lanes and formatting.
Run canonical focused chunk and materialization cases in Debug and ASan/UBSan,
plus directly reached/refactored source cases only if changed. Record required
cases by inventoried exact names before starting. Preserve the first attempt.
Do not repeat full/heavy/opening or mutation matrices for a typed constant.
If only test literals change after the production fix, justify the narrower rerun.

Keep admission on and respect other jobs. Freeze source during each local gate.
Preserve tested source, compiler commands, actual binary identities and truthful
selected/executed/failed/empty/skipped/incomplete counts. No inferred cross-platform
pass from local Apple Clang or GCC.

## P3 — publish, review and stop cleanly

Ordinary-push the correction branch. Its exact-head hosted CI is the MSVC proof;
retain all12 jobs and any remaining setup failure separately. Do not rewrite the
old chunk/materialization histories or mark their old failed runs green because
a descendant passes. The correction's final review can reference already approved
semantics and local evidence with the small delta stated explicitly.

Report READY FOR REVIEW directly to the Mac orchestrator as soon as the focused
correction is frozen: full head/tree, tested head, precise diff, evidence/review
paths, CI link/status, failure extracts and limitations. No executor reviewer
agents. The orchestrator provides focused review and terminal hosted acceptance;
you need not burn active time polling a queue. Further gameplay is released after
this correction is reviewed, with any CI-dependent acceptance kept explicit.

No World/state/default/identity/PRNG/navigation/FP/raster/graphics or original asset
change. Leave other coordinators/jobs and the owner's awake assertion intact.
