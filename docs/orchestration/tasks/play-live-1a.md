# PLAY-LIVE-1a: a "living town" rules profile for the window, behind one flag

**Status:** RELEASED 2026-11-13 by the Claude Code orchestrator, under the owner's standing go for the Windows lane, to
**the Windows executor**. Step 4 of the queue on the board ([`TASKS.md`](../TASKS.md), "Current work").

**Base:** `origin/main` (`96f7980c` when this was written), branch `task/PLAY-LIVE-1a`, in a new worktree on `E:`. About
2–3 hours. When `main` moves (GATE-1c and COORD-FLAKE-1 land on the owner's OK), merge `origin/main` in. Never rebase
or force-push.

**Rules:** the owner rule of 2026-11-12 on the board:
- one concern;
- no reviewer subagents (the orchestrator reviews your handover);
- one REPORTS entry of about 60 lines, READY FOR REVIEW.

This packet uses only fields that `sim::Rules` already has, so no state number moves. If anything needs a new field,
a new stream or a changed default, stop with a QUESTION.

## Why

No window session has living houses (the [project analysis](../reviews/2026-11-12-project-analysis.md) §4.1, D7):
- `--play` creates its world with `sim::Rules{}` (`src/app/src/play.cpp`, both `World::create` calls);
- `--ship-start` sets only `city_names` and the optional `city_booking` (`src/app/src/opening.cpp`, the comment at
  `rules.city_booking`);
- outside tests, `house_updates = true` is set only by `src/tools/sim_replay/main.cpp` (`cmd_population`).

So houses are inert in every window session, and no city earns money without `--city-booking on`. The analysis
(§4.2) asks for a named rules profile, selected by one flag, so the owner's playtests can see a living town.
**Whether it becomes the default is the owner's decision** (the "feeling" rule). This packet keeps every default
as it is today.

## Do

1. **One flag:** `--rules original|living`.
   - `original` is the default and gives each start exactly the rules it gets today.
   - The flag is accepted wherever the window runs the sim: `--play` with a fixed map, `--play` with a scenario, and
     `--ship-start`, including the headless `--play … --commands … --replay-hashes …` run.
   - Where there is no sim (`--view`, `--png` without `--play`), it is refused with the usage text, as other
     misplaced flags are (`src/app/src/cli.cpp`).
   - An unknown profile name is refused.
2. **`living` is exactly two fields** on top of the start's own rules:
   - `house_updates = true`: the House tick's effects (walker spawn, growth, upgrade, downgrade, removal;
     `docs/sim/population-live.md` §8). The Inn's carriers belong to the same population executor
     (`docs/sim/economy-booking.md`: "The population executor retains ownership of Inn carriers").
   - `city_booking = true`: ECON-BOOK-1's checked city sales and upkeep (`docs/sim/economy-booking.md`). It needs
     `save_billed_goods`, which is already `true` by default; leave it so.

   Everything else keeps the start's own value; the ship start, for example, keeps its `city_names`. The ENHANCED
   road and forestry choices (`road_require_land_level`, `road_preserve_nonroad_pieces`, `forestry_claim_hygiene`,
   `forestry_skip_uncuttable`) are not part of it: each has its own owner decision. If the code shows a living
   town needs another existing field, stop with a QUESTION; do not add it.
3. **One definition.** The profile is one function in `src/app`, used by all three starts.
   - Every loader that takes rules gets the profile's rules too: `load_scenario_with_context` in the scenario
     path (today it is passed `{}`), and `load_ship_start` in `load_opening`. The loaded world and the created world
     must agree.
   - No other code reads the profile's name.
4. **`--city-booking on|off`** keeps working where it is accepted today. Given together with `--rules`, it sets
   that one field after the profile (it is the more specific flag). Document this in `kUsage`.
5. **Saves and recordings carry the profile's rules.**
   - A ship-start save already records its rules digest, and `--resume` refuses another (`opening.cpp`,
     `rules_digest`). A save made with `living` must resume with `living` and be refused without it, with that
     message.
   - A recording's replay (`--replay-hashes`, `session.txt`, `src/app/src/main.cpp`) must use the recording's
     profile. Check how a `--play` recording records its invocation:
     - if the replay can read the profile from there, do that;
     - otherwise refuse a differing flag before turn 1, with a message naming the rules, as the ship-start
       replay does.

     A silent hash mismatch at some later turn is not acceptable.
6. **Measure the cost** (PLAN §0.3, [`docs/perf/README.md`](../../perf/README.md)). Headless on this PC (the
   `--replay-hashes` path, or a test that steps the same runner), measure the sim's milliseconds per 100 ms turn,
   median and p95, with `original` and with `living`, on:
   - the fixed map;
   - `Scenes/Tutorial/02Trade01.sze`, a town with houses (`docs/sim/early-needs.md`).

   Report the numbers. If `living` exceeds the turn budget, report it and do not optimise in this packet.
7. **Docs.**
   - The flag goes into `kUsage`, [`docs/app/game-shell.md`](../../app/game-shell.md) and
     [`docs/app/ship-opening.md`](../../app/ship-opening.md).
   - An owner playtest card goes into [`docs/testing/checklist.md`](../../testing/checklist.md):
     - the two command lines, with `--rules living`, for the fixed map and for the ship start;
     - what the owner should see: houses that grow and upgrade near a market, and income in the city's account;
     - what is still not drawn: roads (PLAY-LIVE-1b), and carriers, carts and residents (PLAY-LIVE-1c);
     - the open question: should `living` be the default?

## Proof

- **Tests in `src/app/tests`:**
  1. **Without `--rules`, and with `--rules original`,** each of the three starts creates its world with the same
     rules as before this packet. Compare against rules built the old way (`sim::Rules{}`; the ship start's
     `city_names` and `city_booking`), not against a literal digest the new code printed.
  2. **With `--rules living`,** the created world's rules are the start's own rules with exactly the two fields
     set. Compare the whole struct, not just the two fields.
  3. **The command line:**
     - `--rules living` and `--rules original` are accepted;
     - an unknown name is refused;
     - `--rules` with `--view` is refused;
     - `--city-booking off` after `--rules living` turns booking off and leaves `house_updates` on.
  4. **A living town, with game files** (skips without them). Load `Scenes/Tutorial/02Trade01.sze` through the
     `--play` scenario path and step it headless for a stated number of game minutes, once with each profile:
     - with `living`, at least one house changes (residents or level), and city booking moves the account;
     - with `original`, the houses' state is unchanged.

     State the numbers you measured; the review checks the case against them.
  5. **Resume and replay** with the profile: as in Do item 5, each with its refusal message.
- **The app tests** (`ctest -R anno_app_tests`, or the canonical runner selecting that suite):
  - on MSVC Release with the game files;
  - in the WSL `-O1` sanitizer build.

  Give the classified counts and `sanitizer_lines` 0.
- **Hosted CI green** on the head, `gate` included. Read CI after every push. A macOS job that ends with no steps (no
  runner) is infrastructure: rerun it alone and say so.

**Not yours:**
- the default (the owner's);
- drawing roads (PLAY-LIVE-1b) and figures (PLAY-LIVE-1c);
- any new `Rules` field;
- the full WSL gate and the MSVC game-file run (the orchestrator, at the merge).

## Report

READY FOR REVIEW, about 60 lines:
- the flag and where it is accepted;
- the profile's fields, each with its source;
- the resume and replay behaviour;
- the measurements;
- the tests and their counts;
- the gates and CI;
- NOT_RUN;
- Decisions and reasoning;
- QUESTIONs.
