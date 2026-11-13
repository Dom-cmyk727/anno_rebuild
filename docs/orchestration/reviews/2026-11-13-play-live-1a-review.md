# PLAY-LIVE-1a review (2026-11-13)

**Verdict: FIX ROUND 1, small.**
- **What is right.** The flag, the profile, the three starts, the ship-start resume refusal, the living-town proof and
  the measurements all do what the packet asks.
- **F1:** a `--play` recording records only the profile's *name*. `--city-booking`, which this packet newly applies
  to `--play`, also changes the rules. So a replay with different rules is not refused, and the packet ruled out a
  silent mismatch.
- **F2:** the timing case runs in every gate, though it asserts nothing.

**Reviewer:** the Claude Code orchestrator (Opus 5.5); the package's independent review under the owner rule of
2026-11-12.

**Reviewed heads:** `task/PLAY-LIVE-1a`:
- `2831f78f`: the code;
- `17559083`: its REPORTS entry.

Base `main` `96f7980c`.

## What the orchestrator checked

- **The flag** (`cli.cpp`): `--rules original|living`.
  - An unknown name is refused, and `--rules` without `--play` is refused, which covers `--view` and `--png` alone.
  - `kUsage` documents it, and says that `--city-booking` composes after it.
- **One profile function,** `apply_rules_profile`: `Living` sets exactly `house_updates` and `city_booking`, and
  nothing else.
  - **Fixed map and scenario:** `main.cpp` applies it, then `--city-booking`, into `FixedPlayOptions::rules`.
    `play.cpp` passes those rules to `load_scenario_with_context` and to both `World::create` calls.
  - **Ship start:** `load_opening` applies the profile, then `--city-booking`, before `load_ship_start`, so the
    existing `rules_digest` check covers resume.
- **The tests** (`src/app/tests`):
  - without `--rules` and with `--rules original`, both `--play` paths give exactly `sim::Rules{}`'s digest;
  - `living` gives the default with exactly the two fields set, compared by digest;
  - the command-line cases;
  - **the living town**, on `Tutorial/02Trade01.sze` over 3,000 turns:
    - with `living`, 3 of 32 houses change and the accounts go from 897 to 1,137;
    - with `original`, 0 houses change and the accounts stay at 315.
  - **the ship-start resume:** `living` resumes, and `original` is refused with "other rules".
- **The measurements,** in ms per 100 ms turn:

  | Start | p50 original / living | p95 original / living |
  |---|---|---|
  | fixed map | 0.195 / 0.196 | 0.222 / 0.222 |
  | `02Trade01.sze` | 0.112 / 0.126 | 0.382 / 0.606 |

  Far under the turn budget.
- **The playtest card** in `docs/testing/checklist.md`: the two command lines, what to look for, what is not drawn
  yet, and the default question for the owner.
- **The executor's gates:**
  - MSVC Release `anno_app_tests` with the game files: 272 selected, 270 executed, 0 failed;
  - the WSL `-O1` app lane: 316 selected, 314 executed, 0 failed, `sanitizer_lines` 0;
  - `check_unix_warnings.sh` clean;
  - the other lanes reused, under the rule of 2026-11-13.
- **CI:** green on `2831f78f` (run 43383448663) and `17559083` (run 43387281396).

## Findings

- **F1: a `--play` replay with other rules is not refused.**
  - **How it happens.** `format_invocation` writes `rules <name>` into `session.txt`, and `replay_hashes`
    (`main.cpp`) compares only that name. But for `--play`, `main.cpp` now also applies `--city-booking` on top of
    the profile, which is new with this packet. A recording made with `--rules living --city-booking off` and
    replayed with `--rules living` has the same name and different rules: it is not refused, and it diverges at a
    later turn. The same holds for `--city-booking on` with no `--rules`. The packet said "A silent hash mismatch at
    some later turn is not acceptable."
  - **The fix:** record the created world's rules digest, `rules_digest <hex16>`, as the ship-start path already
    does (`opening.cpp`, `identity.rules_digest`). Then refuse a replay whose world has a different digest, before
    turn 1. The name line may stay, as information.
  - **Make the check testable.** A function in `dev_session.cpp`, called by `main.cpp`, takes the recorded
    `session.txt` text and the replay world's digest, and returns the refusal message or nothing. Unit-test it for:
    - the same digest (accepted);
    - a different digest (refused, the message naming both);
    - a recording without the line (accepted, as an older recording).
  - **Plus one case:** `--rules living --city-booking off` gives a different digest from `--rules living`.
- **F2: the timing case runs in every gate.** `play: ms per 100 ms turn, original vs living, measured` asserts nothing
  and took 61 s in the WSL app lane. Make it opt-in, as the project's other measurements are: skip with a `set
  ANNO_<NAME>=1` message, which the runner classifies as opt-in. Keep the numbers in the report.

**Gate for the fix** (`TASKS.md` §1 item 4: the fix touches `src/app`):
- the app suites on MSVC Release with the game files;
- the WSL `-O1` app lane;
- hosted CI.

No other lane.

## Disposition

Fix round 1 to the Windows executor, by message, same branch. After it, [GATE-2a](../tasks/gate-2a.md) is the
Windows lane's next packet. PLAY-LIVE-1a lands, with WHALE-RE-1b (owner, 2026-11-13), after the orchestrator's
game-file run of the merged candidate, on the owner's OK.
