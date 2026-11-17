# TOWN-START-1: from the ship to a living pioneer town, measured

**Status:** RELEASED 2026-11-17 by the Mac orchestrator to **EXECUTOR II MAC**, wave 1.1,
after the candidate gate closed with a preserved FAIL and MAC-HEAVY-1b was accepted.
Start from main, not the failed UI candidate; no overlap with a GATE-RUN.
This refresh preserves the original measurement scope from the 2026-11-16 packet.

**Base:** `origin/main` at the time of starting (name the commit in the report). Branch `task/TOWN-START-1`, a new
worktree. About 3 to 4 hours. One concern: measure how far a town founded from the ship gets through the public
command path alone. Measurement only: no rule, default, state number, save layout or pin changes. Never rebase,
force-push or push `main`. No reviewer subagents.

## Why

A town founded from a ship has never been shown to grow. The one measurement (`docs/sim/progression-gaps.md`
section 2.2, MAP-INTEGRATE-2 N) founded a city, built a forester and a road, and ended after 300 game seconds with
no house and no tools in the city store. It stopped before the first house, so it says nothing about where a real
opening stops. Every later town task (the hunter, the whaler, the default for `living` rules, the map's start
cargo) is ordered by that answer.

## Read first

- `CLAUDE.md`; `docs/testing/evidence-contract.md`;
- TASKS.md owner rules and section 1; `docs/testing/README.md`; `docs/orchestration/mac-lane.md`;
- `docs/sim/progression-gaps.md` (whole), `docs/sim/ship-founding.md`, `docs/sim/ship-cargo.md`,
  `docs/sim/fixed-map.md` section 11 (the three ship-start candidates);
- the opt-in test `progression gaps N: ship-start settlement for 300 game seconds` in
  `src/sim/tests/founding_assets_test.cpp` and how it drives the founding through public commands;
- `data/maps/candidates/ship-start-v2/lan5-ship-a.json` (candidate A, already a test fixture).
- Current `src/sim/include/anno/sim/config.hpp`, the application's `living` preset mapping,
  `docs/sim/population-live.md`, and the existing house diagnostic in
  `src/sim/tests/population_live_canary_test.cpp`. The progression-gaps document is a historical
  state-138 measurement: check current code before repeating its old unsupported-fisher claim.

## Do

1. **Declare before running:** the map (candidate A, seat 0 unless you give a reason), the rules (`house_updates`
   and `city_booking` on, matching the existing `living` mapping, and the fisher option explicitly on at setup),
   the time budget (**30 game minutes after founding**, 900,000 ticks), and the build order. Inspect whether
   the loaded candidate has usable fish rather than inferring it from the flag; add no fish or bank to make
   it work. Plan food from the supported farm chain if there is no usable fish source. Write the
   build order down with its evidence: the pioneers' needs and forced subsets from `Data/BGruppen.dat`, the
   buildings' costs and stages from `Anlagen.dat`, the ship's start cargo from the manifest. A choice that is a
   player's preference (where to build, how many houses) is named as such, not presented as the original's rule.
2. **One opt-in test** (`ANNO_TOWN_START=1`, default skipped, no game files in the repository) that plays that
   opening through `World::apply` only: sail, found, unload, build houses, the food chain, the forester, roads, and
   further unloads or builds when the declared order says so. No stock, flag, position or rule is patched.
3. **Every 30 game seconds** record: houses per group, residents, money, the city's store of tools, wood, bricks
   and food, every rejected command with its reason, and for every house the first failing predicate of
   `UpgradeCheck` (the diagnostic `progression-gaps.md` uses). Restrict counters to the founded city and
   named participants; don't accidentally sum other towns. Include the founding checkpoint and final one,
   distinguish below-cap/ineligible houses from a failed actual upgrade, and record source/build/seed,
   candidate hash, rule values and public-command outcomes alongside the table.
4. **Run it twice** with the same seed and show the two traces equal (turn hashes at the 30-second marks).
   Assert that houses actually exist before claiming a house measurement. A blocked pre-house opening is
   instead reported with the exact refused command, stock and cost. Check all output writes and exits.
   Keep these two repetitions as the explicit determinism experiment, not repeated whole-suite gates.

## Done when

- Either the first house reaches the settlers, with its tick; or the report names **the first step where the town
  stops**, as numbers: which predicate fails for how many houses, which good is missing, and whether the missing
  piece is a player action the opening did not take (then add it and rerun once) or something the engine cannot do
  yet (name the missing building class or command, with the evidence that the original has it).
- The 30-second table is in the report (or in a generated file the report names), and `pins.py check` reports
  0 moved.
- The opt-in test is skipped by default and runs green with the switch on; one deliberate change to the build
  order (drop the food chain) shows the measurement reacts (the food predicate rises).
  If starting food masks that control during the declared budget, report it as inconclusive and use a
  bounded distinguishing check of the diagnostic; do not claim a causal food result from equal arms or
  extend the game budget, drain stock by hand, or change the rule to force one.

## Scope and checkpoint

Use public commands and normal Advance scheduling; setup may select the documented rules before
World creation, but never patch an already-created world's stock, positions, permissions, claims or
house progress. No free-build cheat, artificial cargo, selected production map or living default flip.
The result can be a measured blocker; it is not permission to implement wildlife, a new producer,
terrain changes or another engine repair. Keep tests and diagnostics small and packet-local; use
existing loader/replay/test seams. If a production fix or owner policy is needed, stop that step with
QUESTION and exact evidence. At about six hours stop at a clean checkpoint rather than expanding scope.

## Report

One REPORTS entry of about 60 lines, READY FOR REVIEW: the declared budget and build order with evidence, the
table (condensed), the stopping step, the determinism check, and the "Decisions and reasoning" list. NOT_RUN
honestly: Windows (the orchestrator runs the switch on this PC at review) and the running original (a comparison
with the History Edition is a later step, if the owner wants one).
Keep the compact summary, commands and SHA-256 index in git; raw traces/recordings stay in ignored cache.
No absolute host paths, original tables, script text or pixels in committed files. Ordinary-push the task
branch and report the exact head and hosted CI run. The Mac orchestrator performs the independent review;
the Windows orchestrator owns target-host intake and landing. Stop READY FOR REVIEW, never self-approve.

## Gates

The test's own lane with and without the switch; `anno_sim_tests` as far as the change touches it; clang-format
23.1.3; the warning script (now green after UNIX-WARN-1, once landed). A test refused for memory is rerun alone.
