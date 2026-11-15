# ZOOM-SMOOTH-1b: review (2026-11-15)

**Reviewer:** the Claude Code orchestrator.
**Head:** `task/ZOOM-SMOOTH-1b` `d71bbfbb`, built on `task/ZOOM-SMOOTH-1` `e8dcb7cf`.
**CI:** run 43661718563, all 12 jobs green, `gate` included. The executor reran one `linux-gcc` job once after it hung
for about 56 min; the same commit passed at `-O3`.

## Verdict

**ACCEPTED (steps 2, 3 and 6).** Three findings go to ZOOM-SMOOTH-1c. None of them is a correctness bug in the
cache. **Do not land 1b alone:**
- it turns the background prewarm on in every viewer session;
- the pan sweep's busy tail got worse (frames over 2 ms: 30 to 96; iGPU: 28 to 41).

It lands stacked with 1c once 1c has removed that regression, or explained it with a measurement.

## What was checked

- **The byte bookkeeping.** `cache_bytes_` changes at every place `cache_` changes:
  - `insert_chunk`;
  - the received-chunk path in `request_chunks`;
  - `fill_cache`;
  - `trim`'s `evict_one`.
  No other `cache_` mutation exists (grep). The test against the O(n) `cached_bytes()` covers insert and replace.
- **The budget is off by default** (`total_bytes = 0`): the count-based path is unchanged, so no existing digest
  moved. The viewer and `bench_frame --zoom-sweep` turn it on.
- **`trim`.** keep/retain and the resident levels are still skipped. The byte path evicts from the least relevant
  end, between the 0.9 and 0.75 watermarks, under the per-call cap. Freed chunks still go to the reaper thread.
- **Prewarm over all workers.** `prewarm_targets_` removes built and blocked targets with the old rules, including
  the P2 "nothing pending is no failure" case. It refills only when the view needs nothing. The derived_freshness
  prewarm suite passed unchanged (12/12).
- **The measurement.**
  - Readiness improved on every mode and on both adapters (non-ready frames down 46 to 82%).
  - The pan and iGPU busy tails got worse.
  Both are reported plainly, as the packet asks.
- **Gates (REPORTS):**
  - MSVC game files: 3 memory refusals, each `VERDICT: PASS` alone;
  - GPU on the 4090: 117/117;
  - WSL `-Werror`: clean apart from the known, unrelated `extensions_test.cpp`.
  - NOT_RUN: the iGPU CTest gate and WSL ASan, both reasoned.

## Findings (for ZOOM-SMOOTH-1c)

1. **F1, the resident share is checked per level, not summed.**
   - `is_resident_level` compares each level's own `chunks x kChunkByteEstimate` with the whole resident share.
   - The levels shrink by 4, so the resident levels together can take up to about 4/3 of the share.
   - Resident chunks are never trimmed, so the working set is left with less than its share. With large maps it could
     sit near the low watermark and trim every frame.
   - Fix: give out the share cumulatively, starting at the farthest level.
2. **F2, the iGPU sweep did not run the viewer's budget.**
   - `bench_frame` passes system RAM 0, so a non-discrete adapter gets the 256 MiB floor.
   - The viewer gets an eighth of RAM, capped at 2 GiB.
   - This is the B1 kind of mismatch again: the iGPU numbers measure a different configuration.
   - Fix: read the RAM without SDL (`GlobalMemoryStatusEx` / `sysconf`) or add `--system-ram-mb`, then rerun the iGPU
     sweep.
3. **F3, there is no per-frame eviction cap in the viewer.**
   - `default_chunk_budget` leaves `max_evict_bytes_per_frame = 0` (unbounded), so step 6's rate limit exists but
     nothing turns it on.
   - Fix: set a measured default, or state why the watermark band alone is enough (freeing already runs on the reaper
     thread).

## The QUESTION (0.5 resident share)

Keep 0.5 for now. F1 changes what the share means, so tune it only once the share is cumulative. 1c reports the
split with the ring's numbers. This is an implementation default, not a game-feel rule, so it doesn't need the owner.

## The pan regression

The executor's explanation is plausible: twelve workers are now filled by prewarm while the camera moves. It is not
measured, though: the chunks step is fast, but contention was not shown.

1c either measures it, for example with prewarm off on the pan leg (`--chunk-budget-mb 0` does not isolate this), or
removes it. Options include holding the prewarm while the camera moves, or leaving one or two workers free.
