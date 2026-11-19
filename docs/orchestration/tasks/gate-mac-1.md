# GATE-MAC-1: the GPU timestamp tests that fail on Metal

**Status:** written 2026-11-16 by the Claude Code orchestrator for **the Mac executor**. **RELEASED** 2026-11-16 on the
owner's go (the owner pastes the prompt).

**Base:** `origin/main` after the minimap candidate has landed (the commit is named in the prompt). Branch
`task/GATE-MAC-1`, a new worktree. About 3 hours. One concern: why three GPU timestamp tests fail on the Mac, and
what the honest fix is. Never rebase, force-push or push `main`. No reviewer subagents.

## Why

The Mac now runs the full game-file suite for every landing candidate (owner rule, 2026-11-16). Its verdict is FAIL
every time, because of tests in `anno_render_gpu_tests` that expect a GPU timestamp reading on a device that says it
has timestamps:

| Test | First failing assertion | Seen |
|---|---|---|
| `river flow on the GPU: the cost is measured per tier, GPU timestamps when the device has them, and the automatic tier follows the adapter` | `gpu_test.cpp:2385` `CHECK(c.gpu_timestamps == ctx->timestamps)`: `false == true` | both gate runs |
| `every pass of a frame is timed, and the timed frame is the untimed picture` | `gpu_test.cpp:2861` `REQUIRE(pass.has_value())` | both gate runs |
| `the sea pass is timed with GPU timestamps when asked and the device can` | `gpu_test.cpp:2515` `REQUIRE(ms.has_value())` | the second run only (candidate `ac02d4a2`); it passed in the first |

Host of both runs: Apple M5 Max, Metal, macOS 27.0.1. The second candidate changed no file under `src/render`, so
the third failure is not from the candidate: the family is intermittent. On Windows (RTX 4090, Vulkan or D3D12) all
three pass. A gate that always says FAIL hides a new failure, which is what nearly happened here.

**What the code does** (`src/render/src/gpu.cpp`, `read_timestamps`): a pass has a reading only if its end tick is
greater than its begin tick and the span is under 10 s. Equal ticks are "no reading" on purpose (a driver that
ignores the writes), and `measure_river_cost` then falls back to the wall clock and reports
`gpu_timestamps = false`. So one plausible cause is that Metal gives equal begin and end ticks for a short pass.
**That is a guess. Measure it.**

## Read first

- `CLAUDE.md`; `docs/testing/evidence-contract.md`;
- `src/render/src/gpu.cpp`: `ensure_timestamps`, `resolve_timestamps`, `read_timestamps`, `measure_river_cost`;
- `src/render/tests/gpu_test.cpp` around lines 2340 to 2400, 2480 to 2530 and 2800 to 2870;
- the wgpu-native version this build pins, and what it says about timestamp queries on Metal (cite what you find).

## Do

1. **How often.** Run each of the three tests alone 30 times (`--jobs 1`, nothing else running). Report passes and
   failures per test.
2. **What the device returns.** Behind an environment switch, print for every timed pass the raw begin and end
   ticks, the timestamp period, and which rule turned a reading into "no reading" (equal ticks, end before begin,
   over 10 s, the map failed). Run the three tests with it and say which rule fires, for which pass, how often.
3. **Decide from that, one of:**
   - **the driver gives equal or reversed ticks for short passes:** the engine's rule is right and the tests ask
     too much. Change the tests so a device with timestamps may have no reading for a pass, and say exactly what
     they still require (at least: a reading is never 0 or negative, the wall-clock fallback is used and reported,
     the timed picture equals the untimed one). Do not weaken them for Windows adapters where they pass: make the
     allowance depend on what was measured, with the reason in a comment;
   - **a defect in our code** (the resolve, the copy, the map, the order of passes): fix it and show the three tests
     pass 30 of 30;
   - **neither is clear:** stop and report what was measured. Do not guess.
4. **Check nothing else moved:** `anno_render_gpu_tests` whole, on the Mac.

## Report

One REPORTS entry of about 60 lines, READY FOR REVIEW: the table of step 1, the finding of step 2 with numbers, the
decision of step 3 with its evidence, what the tests require now, and the "Decisions and reasoning" list. NOT_RUN
honestly: the Windows adapters (the orchestrator runs them at review).

## Gates

`anno_render_gpu_tests` and `anno_render_tests` on the Mac, clang-format 23.1.3, the warning check script. A test
refused for memory is rerun alone.
