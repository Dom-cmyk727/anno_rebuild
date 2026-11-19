# ZOOM-SMOOTH-1e, the step-5 test, the perf entry and the live proof: review (2026-11-15)

**Reviewer:** the Claude Code orchestrator.
**Head:** `task/ZOOM-SMOOTH-1e` `4ebb9de0` (code `84e6fc53`), built on `task/ZOOM-SMOOTH-1d` `1f7a5a9f`.
**CI:** green, `gate` included.

## Verdict

**ACCEPTED.** F1 and F2 are closed, and the live proof was run as asked and reported honestly: **the target is NOT
MET** with step 5 alone (ring off). That ends ZOOM-SMOOTH-1 as a **partial**. Its 0-on-all-counters target is not
chased further inside the baked chunk renderer; it is RENDER-DIRECT-6's acceptance already
([ADR 0007](../../adr/0007-direct-rendering.md) decision 4 and step 6: no stand-ins, no dissolves, the sweep at 0).

## What was checked

- **F1, the step-5 test** (`gpu_test.cpp`):
  - two visible chunks with `uploads_per_frame = 1` leave one waiting inside one render call, deterministically
    (the visible set is a `std::set`);
  - budget 0 gives `chunks_prefetched == 0`, the old gate's behaviour; budget 1.0 gives 1 while a chunk still waits;
  - the deadline is taken when prefetch starts (`gpu.cpp` 2201-2203) and checked before each candidate, so the
    first candidate always starts: no timing flake;
  - not `test_hold_uploads`, which also closes the prefetch gate: the right call, reasoned in the report.
  - "Fails on `619f88f6`" is a compile failure, since the field is new there. Acceptable: the budget-0 half of the
    test is the old gate's behaviour on the new code.
- **F2:** the `docs/perf/README.md` entry has the replaced gate, the 0.3 ms default and why, the table against 1c2,
  and the isolation flag.
- **`wheel N X Y`** (`dev_script.cpp`): 1 or 3 arguments, 2 fails on step like `click_px`; a parse-and-exec test.
  The off-centre point (1920, 890) is the 25 %/15 % anchor fraction added to the centre, not `bench_frame`'s anchor
  taken from the top-left corner; comparable within this proof, noted only.
- **Gates:** render+app+GPU+input 648 executed, 0 failed, PASS; GPU on the 4090 118/118; WSL `-Werror` clean apart
  from the known `extensions_test.cpp`.
- **No original pixels in git:** the per-phase tables stay in `cache/`.

## The executor's QUESTIONs, answered

1. **Next move:** neither a bigger share nor the ring's own budget. The owner's queue already puts RENDER-DIRECT
   after ZOOM-SMOOTH-2 and 3, and ADR 0007 removes the stand-ins and dissolves the counters measure. More tuning of
   the baked path would be thrown away.
2. **Are dissolves inherent?** Yes, under the baked model. Any chunk that becomes ready while visible fades in over
   `chunk_fade_ms`, and a zoom-out-and-return shows chunks the budget evicted or never built. So a dissolve count
   above 0 on every oscillation is the design, not a tunable bug. That is ADR 0007's own finding
   ([`zoom-smoothness.md`](../../render/zoom-smoothness.md) §6).

## What follows

- **ZOOM-SMOOTH-1c2, 1d and 1e** wait for the owner's OK to land, stacked on main. The ring stays off by default.
- **ZOOM-SMOOTH-2** (window drag and resize) is released under the standing go, base `origin/main`.
- The live numbers above become RENDER-DIRECT's "before" on `lan5`, centre and off centre.
