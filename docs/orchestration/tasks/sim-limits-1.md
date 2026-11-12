# SIM-LIMITS-1: the freight save limits from one source

**Status:** written 2026-11-12, released when the owner says so. For **the Windows executor**. Step 1 of the queue
on the board ([`TASKS.md`](../TASKS.md), "Current work").

**Base:** `origin/main` after MAP-INTEGRATE-2 lands (state 138). Branch `task/SIM-LIMITS-1`. About 2 hours.

**Rules:** the owner rule of 2026-11-12 on the board. One concern, no reviewer subagents (the orchestrator reviews
your handover), the gate below, one REPORTS entry of about 60 lines, READY FOR REVIEW.

## Why

The [MAP-INTEGRATE-2 review](../reviews/2026-11-12-map-integrate-2-review.md), finding E5: the freight save checks
copy their limits by hand, and one copy has already drifted. Nothing is broken today; the next appended value would
make valid saves fail their load check.

## Do

1. **The stored reject.** `freight.cpp` (about line 222) and `state_fields.hpp` (about line 1309) accept a route's
   `last_reject` only up to `Reject::CityAccounting`. `reject.hpp` has since appended `NoDepositInReach` after it.
   Define the last valid `Reject` once, next to the enum, use it in both places, and make the build or a test fail
   when someone appends a value without updating it.
2. **The navigation failure.** `nav_failure > 3` appears in `freight.cpp` (about line 221) and `state_fields.hpp`
   (about line 1315). Give the limit one name, derived from what the values mean, and use it in both.
3. **The route's wire size.** The formula `12 + 20 × stops + 13 × actions` and the maxima `(65535 − 12) / 20` and
   `(65535 − 32) / 13` appear in `freight.cpp` (about line 79), `config.cpp` (about lines 555–556),
   `state_fields.hpp` (about lines 1278–1286) and `codec.cpp` (about line 236). Put them in one `constexpr` helper
   that all four use, with `static_assert`s on the maxima. Keep the exact values.
4. **The `HarvestState` guard.** `state_fields.hpp` (about line 1197) asserts 9 members, but `fields(HarvestState)`
   writes 7; the figure's own fields write `goal_col`/`goal_row` for the mason job only (about line 1349). Make the
   guard state exactly that, so a new member fails the build.

**Must not change:** the save layout, any golden or fixture, state number 138, or any behaviour. The proof is that
every existing fixture and stream check passes unchanged.

## Gate

- Build the `msvc` preset.
- `tools/test/run_selected.py`, the `anno_sim_tests` cases for freight, codec, config, state stream and integration
  (identity, stream, cross-feature), with `ANNO1503_GAME_DIR` set; classified counts.
- `python -I tools/sim_stream.py --check` and the integration stream references.
- clang-format 23.1.3, and `wsl -d Ubuntu -- bash tools/check_unix_warnings.sh` (sim code).
- Push; hosted CI green on the head, `gate` included. Read CI after each push.

**Not yours:** the whole suite and the game-file run (the orchestrator, at the merge).

## Out of scope

Moving original constants into data (finding E7), the freight effect queue (E2–E4), the fixtures (E1), and the veto
rules that are default-constructed in freight, cargo and founding alike.
