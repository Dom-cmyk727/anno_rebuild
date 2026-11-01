# MAP.2 founding: independent review ledger (second packet)

Each entry: package, frozen revision, reviewer role, findings, disposition. The full reviewer reports are quoted here in summary; raw material is under `cache/map2-review5/` and `cache/map2-gates4/` (git-ignored, on the executor's machine). Earlier packet reviews: `docs/orchestration/REPORTS.md` MAP.2 entries.

## Package A: crew occupancy, hull facts, draw accounting (frozen `ad575e7`)
Reproductions: `/private/tmp/anno-founding-review-jzqem6mk/probe.cpp` (results copied to `cache/map2-gates4/pkgA-review-probe-results-before-fix.txt`); first failures of the new regressions: `cache/map2-gates4/pkgA-before-fix.txt` (A1 case 3 and A3 failed for the intended reasons before the fixes).
Reviewer: APPROVED FOR THIS PACKAGE, no P1/P2. Checked: real loader path (crew_slot from scenario to Figure, FigurenHeader re-read, ship-class table), negative tests reach their conditions (hand geometry), positive controls, no silent default, four own mutants each caught by exactly one test (crew by parent link, dropped Scheduler::merge, absent hull read as 0, commit drawing on the generator directly).
Findings: P3 the digest layout change has no independent oracle (Python reference lacks figure types) -> documented in contract 17.3; P3 stream version stays 120 although digest bytes moved (pre-release) -> documented; residual: gate list wording -> 17.2 updated.

## Package B: replay evidence (frozen `c086149`)
Reviewer: CHANGES REQUIRED (small). Fixture decoded independently: 5 FoundCmd records (wire `70 F0`) and the Build; wrong log differs by exactly one byte (click_x); CLI exit codes 0/1; regeneration byte-identical; own variants (drop founding, drop Build, swap) detected.
Findings and dispositions: P2 the CLI runs were not in the gate -> `anno_sim_replay_founding_run` and `anno_sim_replay_founding_wrong_run` ctest cases added (commit `eacda94`); P2 the executor test could not notice an ignored executor (mutation passed) -> `ThreadExecutor::calls()/jobs()` asserted; P3 misleading comment -> reworded; P3 command order between two players not pinned by the fixture -> accepted (a refused command has no effect; order is covered by the conflict cases).
