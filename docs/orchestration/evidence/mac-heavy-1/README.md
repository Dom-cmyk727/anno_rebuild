# MAC-HEAVY-1 — READY FOR REVIEW

Tested HEAD `4dfdd9b7ce51b9ad04970da0b247294345f05ac7`; tree `e218b8f5dae9e7e133e8f803618ba6e14d355286`. Branch `task/MAC-HEAVY-1` holds report/evidence only.

Independent review is pending with the orchestrator. No engine, tool, test, fixture, default, golden or pin change is included.

| Stage | Wall s | Result / scope |
|---|---:|---|
| 0 | 203.082 | Host/freeze recorded; clean source |
| 1 | 3118.817 | Full ASan/UBSan FAIL: one minimap assertion; targeted isolation PASS; 0 sanitizer reports |
| 2 | 4931.089 | Whole-process FAIL: four operations, five native case failures; one distinct known assertion |
| 3 | 6662.897 | Three full serial Release PASS; identical status/prerequisite outcomes; no missing cases |
| 4 | 861.821 | 13 native selections: 11 passes, 2 transport failures; no sanitizer reports |
| 5 | 335.062 | Both full 100-command selections FAIL on 78 loader refusals each; no invariant/crash failures; 100 exact matched outputs |
| 6 | 234.243 | Atlas exit 0; 519 loaded / 0 refused; validated TSV |

All stages stayed within their boxes. There were no memory refusals, bad_alloc retries, stage stops or timeouts. Stage 1 LSan capability probe is an expected unsupported-host result, not an engine sanitizer finding.

## Evidence

- [Verbatim canonical receipts and primary log hashes](gate-receipts.md)
- [Findings and reproduction commands](findings.md)
- [Whole-binary order matrix](order-matrix.md)
- [Opt-in timings, peak RSS and actual assertion scope](opt-ins.md)
- [All corpus outcomes and refusals](corpus.md)
- [Methods, adaptations and logging limitations](methods.md)
- [Acceptance scope; independent disposition pending](acceptance.md)
- [All retained log paths, content-addressed copies and SHA256s](logs.json)

## Atlas

TSV: 24912 scene rows, 3 figure-catalog rows and 1 colour-eligibility row; 24925 physical lines. Full-file SHA256 `d752e056722f9654b1d3f3d11d397af2b9537c01822e0ad57c25df7d9bec3a5c`.

Combined byte-column SHA256 `f9f1cc5d0ec61c1ff58890de66c5912c3a43beb9f8f5175b58ab69c386893af6`.

Scene columns: static_bytes, water_bytes, water_shadow_bytes, enhanced_extra_bytes, figures_used_bytes, pack_bytes, rect_bytes. Then figure columns bytes, player_colourable_bytes. UTF-8 decimal strings, tabs between cells, LF per data row; headers/comments/paths/non-byte columns excluded. See stage6-atlas-summary.json for section hashes and exact schema. Windows comparison NOT_RUN.

## Environment and provenance

```json
[
  {
    "ANNO_TEST_MB_PER_THREAD": "2048",
    "ANNO_TEST_THREADS": "4",
    "ASAN_OPTIONS": "detect_leaks=1:halt_on_error=1",
    "UBSAN_OPTIONS": "print_stacktrace=1:halt_on_error=1",
    "ANNO1503_GAME_DIR": "/Users/dom/Dev/anno1503-worktrees/anno1503/game/app"
  },
  {
    "ANNO_TEST_MB_PER_THREAD": "2048",
    "ANNO_TEST_THREADS": "4",
    "ASAN_OPTIONS": "detect_leaks=0:halt_on_error=1",
    "UBSAN_OPTIONS": "print_stacktrace=1:halt_on_error=1",
    "ANNO1503_GAME_DIR": "/Users/dom/Dev/anno1503-worktrees/anno1503/game/app"
  },
  {
    "ANNO1503_GAME_DIR": "/Users/dom/Dev/anno1503-worktrees/anno1503/game/app",
    "ANNO_TEST_THREADS": "4"
  },
  {
    "ANNO1503_GAME_DIR": "/Users/dom/Dev/anno1503-worktrees/anno1503/game/app",
    "ANNO_IRON_CORPUS": "1",
    "ANNO_TEST_THREADS": "4"
  },
  {
    "ANNO1503_GAME_DIR": "/Users/dom/Dev/anno1503-worktrees/anno1503/game/app",
    "ANNO_IRON_SURVEY": "1",
    "ANNO_TEST_THREADS": "4"
  },
  {
    "ANNO1503_GAME_DIR": "/Users/dom/Dev/anno1503-worktrees/anno1503/game/app",
    "ANNO_PROGRESSION_GAPS": "1",
    "ANNO_TEST_THREADS": "4"
  },
  {
    "ANNO1503_GAME_DIR": "/Users/dom/Dev/anno1503-worktrees/anno1503/game/app",
    "ANNO_PROGRESSION_PROBE": "1",
    "ANNO_TEST_THREADS": "4"
  },
  {
    "ANNO1503_GAME_DIR": "/Users/dom/Dev/anno1503-worktrees/anno1503/game/app",
    "ANNO_SIM_SUPPLY_SCALE": "1",
    "ANNO_TEST_THREADS": "4"
  },
  {
    "ANNO1503_GAME_DIR": "/Users/dom/Dev/anno1503-worktrees/anno1503/game/app",
    "ANNO_SIM_TRANSPORT_STATS": "1",
    "ANNO_TEST_THREADS": "4"
  },
  {
    "ANNO1503_GAME_DIR": "/Users/dom/Dev/anno1503-worktrees/anno1503/game/app",
    "ANNO_SIM_NAV_SCALE": "/Users/dom/.codex/worktrees/mac-heavy-1/anno1503/cache/mac-heavy-1/nav-scale",
    "ANNO_TEST_THREADS": "4"
  },
  {
    "ANNO1503_GAME_DIR": "/Users/dom/Dev/anno1503-worktrees/anno1503/game/app",
    "ANNO_PLAY_LIVE_TIMING": "1",
    "ANNO_TEST_THREADS": "4"
  },
  {
    "ANNO1503_GAME_DIR": "/Users/dom/Dev/anno1503-worktrees/anno1503/game/app",
    "ANNO_SIM_SUPPLY_SCALE": "1",
    "ANNO_TEST_MB_PER_THREAD": "2048",
    "ANNO_TEST_THREADS": "4",
    "ASAN_OPTIONS": "detect_leaks=0:halt_on_error=1",
    "UBSAN_OPTIONS": "print_stacktrace=1:halt_on_error=1"
  },
  {
    "ANNO1503_GAME_DIR": "/Users/dom/Dev/anno1503-worktrees/anno1503/game/app",
    "ANNO_SIM_TRANSPORT_STATS": "1",
    "ANNO_TEST_MB_PER_THREAD": "2048",
    "ANNO_TEST_THREADS": "4",
    "ASAN_OPTIONS": "detect_leaks=0:halt_on_error=1",
    "UBSAN_OPTIONS": "print_stacktrace=1:halt_on_error=1"
  }
]
```

Host: Apple M5 Max, 18 logical CPUs, 48 GiB, macOS 27.0.1 (26A434), Apple Clang 21.0.0, CMake 3.31.10, Ninja 1.13.2.git.kitware.jobserver-pipe-1. GPU: Apple M5 Max (Metal). Initial load 1.609/3.382/4.094; free disk about 1.5 TiB. Coordinator compat 4; no server observed at start, later compatible server observations retained. Unrelated process command lines are excluded.

Report source identities, commands, actual explicit environment, receipts and wall times are in stage0.json through stage6.json. Frozen orchestration/binding snapshots are under orchestration/. Game hashes were observed at the Stage 5 boundary; they are not a retroactive Stage 0 snapshot.
