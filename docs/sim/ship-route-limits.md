# Ship route engine: work and memory limits, measured (MAP-NAV-2 package C)

Status: measured defaults of `RouteLimits` (`src/sim/include/anno/sim/route.hpp`). The original engine has no work cap: it floods
the whole window (`docs/re/ship-navigation.md` 9.3, 9.4). The port bounds it with node-pop budgets and an accounted-bytes limit,
so an adversarial map cannot stall a tick; exceeding a limit returns `BudgetExceeded` (never a partial route).

## What is counted

- `coarse_pops`: every pop of the coarse FIFO Dijkstra, over all retries (the learned-mask retries re-run the coarse search).
  The port stops the flood once every goal-flag cell has been popped (output-identical, the original keeps flooding); an
  unreachable goal still floods the whole window.
- `fine_pops`: every pop of the fine Dijkstra, over all retries. The flood stops when the chosen goal tile is labelled
  (output-identical: labels never change after discovery).
- `unit_cap` is validated to at most 1024: the density work of one search is `unit_cap x 64 candidates x goal cells`, at most about
  6 million operations per query at the bound (9 searches, 9 goal cells) and 0.4 million at the default 64. This replaces counting
  density work in the pop budgets.
- `memory_bytes`: **logical** sizes only (element count times element size, never `capacity()`, which differs between standard
  libraries and would make the verdict platform dependent): the window arrays (label, flag, fine-block ordinal: 6 bytes per
  coarse cell), the two heaps, the corridor's fine blocks (64 bytes each) and ordinals, the coarse path, the learned list, the trace and the
  result runs. Every allocation that can grow with the map or the path (the coarse path, each corridor block, each heap node, each
  trace step and run) is accounted BEFORE it is made (`grow`), so a large `max_window_blocks` cannot overshoot: a failing query's
  high-water mark is at most one refused allocation (68 bytes) above the limit (`route limits: memory is checked before every
  allocation`, swept over 38 limits with a valid control). High-water mark in `RouteWork::bytes_high`.
- `max_window_blocks` (32 = the original `SetWindow` limit) is checked on the start-centred window before anything is allocated;
  a larger window is `BudgetExceeded{Window}` (the original answers -2 for the same case).

## Measurements

Release of the numbers: `anno_sim_tests -tc='route*'` (the real-island case needs `ANNO1503_GAME_DIR`; its TSV is written when
`ANNO_ROUTE_REAL_OUT` is set). Debug build, Apple Clang 21, arm64; the counters are platform independent.

| query | coarse pops | fine pops | learned | bytes |
|---|---|---|---|---|
| fixture V1, straight east 36 on 64 x 64 | 47 | 310 | 0 | 1,284 |
| fixture V7, detour around a wall end | 41 | 1,150 | 0 | 2,420 |
| fixture V21, bay with two learned masks | 172 | 1,433 | 2 | 1,804 |
| fixture V22, diagonal-only wall (unreachable) | 36 | 0 | 0 | 480 |
| largest of the 24 route-run vectors | 172 | 1,433 | - | 2,420 |
| real candidates a, b, c: 30 seat-to-seat routes (1431 x 1431 world, window 179 x 179 cells) | max 22,691 | max 26,523 | 0 | max 215,724 |
| real candidates: the 15 witness queries | max 22,324 | max 3,169 | 0 | max 196,434 |
| synthetic 2048 x 2048 sea, diagonal 1016 tiles from the middle (window 256 x 256 cells) | 39,117 | 32,302 | 0 | 442,568 |
| same sea, 1000 tiles west | 65,421 | 8,059 | 0 | 405,396 |
| same sea, unreachable goal (the full flood of every cell) | 65,487 | 0 | 0 | 405,252 |

The synthetic rows are the largest window the original can build (2048 x 2048 tiles, 256 x 256 cells = 65,536 cells; an
unclipped 1024-radius box is 33 blocks and is rejected). All 30 real seat-to-seat queries and all 15 witnesses needed zero
learned masks; the learned-mask retries are exercised by the emulated vectors (up to the cap of 8).

## Defaults and margin

| limit | default | largest measured legal query | margin |
|---|---|---|---|
| `coarse_budget` | 600,000 | 65,487 (full window flood) | 9.2x; also above 9 x 65,536 = 589,824, the most pops the original-sized window can cost in 9 searches (retry cap 8 + 1), so a query the original completes on its window is never refused |
| `fine_budget` | 4,000,000 | 32,302 | 124x; corridor flood of a 1016-tile diagonal. A 4096-tile world path (about 1,500 corridor cells, 9 searches) costs about 0.9 million |
| `memory_bytes` | 64 MiB | 442,568 | 151x |
| `unit_cap` (validated maximum 1024) | 64 | - | the original value; bounded for the density work |
| `window_radius_tiles` | 1024 | - | the original value |
| `max_window_blocks` | 32 | - | the original value |
| `goal_radius_tiles` | 6 | - | the original value |
| `run_cap` | 10 | - | the original value |
| `retry_cap` | 8 | - | the original value |
| `unit_cap` | 64 | - | the original value |

`src/sim/tests/route_unit_test.cpp` ("route limits: the defaults carry a margin of at least 8x ...", "work of the representative
small queries") and `route_world_test.cpp` (real candidates) enforce `default >= 8 x measured` on every row above, so a regression
that makes queries more expensive, or a future lowering of a default, fails a test. Larger maps (a configured
`max_window_blocks` above 32) need larger budgets: the budgets scale with the window cell count, which is why they are data and not
constants. Per-query work on the real candidates is a few tens of thousands of pops (milliseconds in a Release build); the
worst case is bounded by the budgets, not by the map.

## Determinism of the verdict

The verdict `BudgetExceeded{CoarsePops|FinePops|Memory|Window}` depends only on pops counted and logical bytes, both functions of
the arguments; it is identical on every platform, thread count and run (`route determinism ...` runs 48 queries on 1, 2, 4 and 7
threads and compares the serialised results byte for byte; `route limits ...` checks each limit at the exact boundary with a valid
control one unit above).
