# TOWN-START-1: from the ship to a living pioneer town, measured

**Status:** written 2026-11-16 by the Claude Code orchestrator for **the Mac executor**, from the remaining-work
assignment ([remaining-work-2026-11-16.md](../remaining-work-2026-11-16.md), Mac task 2). Released when the owner
pastes the prompt.

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
- `docs/sim/progression-gaps.md` (whole), `docs/sim/ship-founding.md`, `docs/sim/ship-cargo.md`,
  `docs/sim/fixed-map.md` section 11 (the three ship-start candidates);
- the opt-in test `progression gaps N: ship-start settlement for 300 game seconds` in
  `src/sim/tests/founding_assets_test.cpp` and how it drives the founding through public commands;
- `data/maps/candidates/ship-start-v2/lan5-ship-a.json` (candidate A, already a test fixture).

## Do

1. **Declare before running:** the map (candidate A, seat 0 unless you give a reason), the rules (`house_updates`
   and `city_booking` on; the fisher flag as the map allows: candidate A has no fish banks, so food comes from
   farms), the time budget (**30 game minutes after founding**, 900,000 ticks), and the build order. Write the
   build order down with its evidence: the pioneers' needs and forced subsets from `Data/BGruppen.dat`, the
   buildings' costs and stages from `Anlagen.dat`, the ship's start cargo from the manifest. A choice that is a
   player's preference (where to build, how many houses) is named as such, not presented as the original's rule.
2. **One opt-in test** (`ANNO_TOWN_START=1`, default skipped, no game files in the repository) that plays that
   opening through `World::apply` only: sail, found, unload, build houses, the food chain, the forester, roads, and
   further unloads or builds when the declared order says so. No stock, flag, position or rule is patched.
3. **Every 30 game seconds** record: houses per group, residents, money, the city's store of tools, wood, bricks
   and food, every rejected command with its reason, and for every house the first failing predicate of
   `UpgradeCheck` (the diagnostic `progression-gaps.md` uses).
4. **Run it twice** with the same seed and show the two traces equal (turn hashes at the 30-second marks).

## Done when

- Either the first house reaches the settlers, with its tick; or the report names **the first step where the town
  stops**, as numbers: which predicate fails for how many houses, which good is missing, and whether the missing
  piece is a player action the opening did not take (then add it and rerun once) or something the engine cannot do
  yet (name the missing building class or command, with the evidence that the original has it).
- The 30-second table is in the report (or in a generated file the report names), and `pins.py check` reports
  0 moved.
- The opt-in test is skipped by default and runs green with the switch on; one deliberate change to the build
  order (drop the food chain) shows the measurement reacts (the food predicate rises).

## Report

One REPORTS entry of about 60 lines, READY FOR REVIEW: the declared budget and build order with evidence, the
table (condensed), the stopping step, the determinism check, and the "Decisions and reasoning" list. NOT_RUN
honestly: Windows (the orchestrator runs the switch on this PC at review) and the running original (a comparison
with the History Edition is a later step, if the owner wants one).

## Gates

The test's own lane with and without the switch; `anno_sim_tests` as far as the change touches it; clang-format
23.1.3; the warning script (now green after UNIX-WARN-1, once landed). A test refused for memory is rerun alone.
