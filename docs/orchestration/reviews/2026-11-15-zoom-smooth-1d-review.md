# ZOOM-SMOOTH-1d, the GPU upload share: review (2026-11-15)

**Reviewer:** the Claude Code orchestrator.
**Head:** `task/ZOOM-SMOOTH-1d` `1f7a5a9f` (code `63f4c69a`), built on `task/ZOOM-SMOOTH-1c2` `619f88f6`.
**CI:** green, `gate` included.

## Verdict

**ACCEPTED as a partial.** The upload share is correct and measured as a clear win on every mode. The ring is off by
default, behind a switch, for a measured reason. The packet's own target, 0 on all four counters in the live session,
is NOT_RUN. It moves to ZOOM-SMOOTH-1e with two small findings.

## What was checked

- **B3 first, as asked.** The reaper runs at lowered priority, and the warm-plane thread's comment already names the
  inversion risk (wgpu-hal's allocator lock). wgpu-core defers destruction by submission index whatever the thread,
  so the risk is lock contention, not correctness. Step 5 never releases through `reap()`. Accepted. The source was
  wgpu-core 28.0.0, not the pinned v29.0.1.1; the executor says so.
- **Step 5** (`gpu.cpp` `sync_chunks`): the gate `!waiting && uploads == 0` is gone.
  - With nothing visible waiting, prefetch runs as before, now also bounded by `upload_budget_ms`.
  - Otherwise it gets `GpuLimits::prefetch_budget_ms` (0.3 ms in the viewer's limits, 0 headless), counted from the
    end of the visible work. So it adds to the visible budget and never takes from it.
  - It is checked once per candidate, so one upload already running may overrun the share. The header says so.
  - `prefetch_chunks` and `prefetch_per_frame` still cap it.
- **The ring:** 1c2 kept its chunks in no list the GPU read, so it could never have uploaded ahead. A good find.
  `ring_keys()` now feeds a third prefetch list, after margin and adjacent.
  - With `zoom_ring_enabled` off (the default), `plan_margin` adds no rank-3 chunks, so planning equals 1c.
  - The ring's own invariant test turns it on explicitly.
- **Measurements** (three runs each side, 4090):
  - step 5 alone beats 1c2 on every mode; on zoompan, stand-ins fall from 17,002 to 14,489 and frames over 2 ms from
    226 to 173–210;
  - the ring on top helps wheel and pan a little, but costs zoompan: about 21.9k against 14.9k stand-ins, and 237
    against 197 frames over 2 ms, with no overlap between the runs.
  - Keeping it off but in place is the right call: the dispatch's rule was "remove it if it buys nothing", and it buys
    something on two modes.
- **The iGPU:** the unchanged 1c2 binary measured again today gives 14–21 frames over 2 ms; every 1d setting gives
  18–42. That is the same baseline drift as pan. No 1d regression is shown, and none is ruled out tightly. The
  live proof below covers what matters.
- **Gates:**
  - render+app: 493 executed, 0 failed;
  - GPU on the 4090: 117/117;
  - WSL `-Werror`: clean apart from the known `extensions_test.cpp`.

## Findings

1. **F1, step 5 has no test.** No test shows that a margin chunk uploads while a visible chunk waits when
   `prefetch_budget_ms > 0`, and that none does when it is 0. The only test change turns the ring on. Add one, with
   `test_hold_uploads` or an equivalent seam, that fails on `619f88f6`.
2. **F2, a missing citation.** `gpu.hpp` cites "docs/perf/README.md ZOOM-SMOOTH-1d" for the 0.3 ms, but `docs/perf`
   has no such entry. The numbers are only in REPORTS. Add the entry: the runs, the commands, why 0.3.

## The executor's question: a separate budget for the ring?

**After, and only if needed.** First prove the live session with step 5 alone and the ring off. If all four counters
reach 0 for zoom-out moves and returns, the ring is not needed and comes out in a later cleanup. If they don't, the
report says which counter and mode fail. The ring's own share is then one candidate fix, sized as its own packet.

## ZOOM-SMOOTH-1e (released 2026-11-15, about 3 h)

**Branch:** `task/ZOOM-SMOOTH-1e`, from `task/ZOOM-SMOOTH-1d` `1f7a5a9f`. **Spec:** `tasks/zoom-smooth-1.md`, its
acceptance target.

1. **F1:** the step-5 test, failing on `619f88f6` and passing on the branch.
2. **F2:** the `docs/perf/README.md` entry for 1d's measurements and the 0.3 ms.
3. **The packet's target,** with the ring off (the default): the live owner-style session on `lan5`, driven by
   `zoom.script` / `drive2.py` and analysed by `analyze2.py`.
   - Zoom-out moves and returns, centre and off centre, reported per mode on the 4090.
   - The four counters must be 0 (not-ready, stand-in, dissolve, late island).
   - Zoom-in is reported only.
   - If a counter is not 0, stop and report the mode, the counter and its numbers. Do not tune in this packet.
4. Touch nothing else. The ring's own budget is not in this packet.
