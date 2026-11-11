# The co-developer's shared test list: our sim and the original (W4-NEXT-2 X)

Owner instruction via the co-developer exchange (`E:\open1503-share\from-codev\OPEN1503-STATUS.md`,
"Shared test list"). For each of the co-developer's five rows (residents, and where listed the
town store's and the workshops' input stores' change), three columns: his value (Open1503, his own
code), our sim's value, and the original's value. **Measurement only: no sim change, no "fix" of a
difference.**

## 1. Scenario identity

Our GOG copy (`game/app`, `goggame-1438074791.info`: English, standalone, 2.0.0.5) has all five
scenarios. Sizes and SHA-256 (W4-NEXT-2 part P, this branch's own REPORTS entry):

| Scenario | Path | Size (bytes) | SHA-256 |
|---|---|---|---|
| `Single_06` | `Scenes/0200Single/Single_06.sze` | 488356 | `29767be35e21f5c59dfc4f9178ece33f342e482fa22380ea0bf74f5dc6ef5fed` |
| `Single_16` | `Scenes/0200Single/Single_16.sze` | 312356 | `78229c7c6b47d9c393a0bcab3aae52057b8088040c8374a80040bba8af7753ea` |
| `01_1024_easy_256` | `Scenes/0010SingleEndlos/01_1024_easy_256.sze` | 53545 | `3e0d2c72e1ae87b76cc817f0aead7198b4f0509cb11a5246463e338ccf060e05` |
| `S01_08_01` | `Scenes/0100Kampagne01/S01_08_01.sze` | 407605 | `7f03b03cf9ba07ea184ef3093e46760d7385776a3702f007df86d3956bc0a133` |
| `Single_24` | `Scenes/0200Single/Single_24.sze` | 521724 | `861ad2066a1c26c53367f8538e328e6b00e467e4a00238265d9f20747661ffc4` |

The co-developer's own note gives hashes only for `Data/Debug.dat` and the two `.zei` fonts, not
for these five scenario files, so a byte-identity cross-check against his copy is not possible yet
from what he has sent; his copy differs from ours in at least those two kinds of file, so a
scenario difference cannot be ruled out. Worth asking for next.

## 2. Our sim (headless, `tools/sim_replay` population subcommand)

`anno-sim-replay population <file.sze>` (added this package, `src/tools/sim_replay/main.cpp`):
loads and creates the scenario exactly as `cmd_scenario` does, samples residents
(`World::city_population`, summed over every city of every player - "all towns counted") and a
town-store-total proxy (every tracked good in every city's `CityStock`, summed and divided by
`kTon`) at t=0s, then advances to t=10s/30s/60s (500 ticks/game-second, a tick is 2 ms) and samples
again. A load/create refusal is reported with its exact reason, not worked around.

**F5 correction (2026-11-11 review, findings 3/4): re-measured at the play mode's own step, with
`Rules::house_updates` tried both off and on.** The first pass above stepped 100 ms per
`world.advance()` call; play mode at 100% speed steps 10 ms (`sim::multiplayer_step_ms(100)`,
`sim_test.cpp:159`), and the island sweeps take that step as input, so "normal speed" was not what
was measured. It also ran with the default `Rules::house_updates == false`, under which the House
tick is an exact no-op by design (`config.hpp`'s own comment on the field) - so "no growth" there
was expected by the rule in effect, not a finding about the engine. Both are now explicit tool
options (`--step-ms`, default 10; `--house-updates`, off by default) and `cmd_population` now runs
`check_invariants` at each checkpoint (printed `clean` below at every one), closing the gap the
review found between this command and `cmd_scenario`'s own invariant check.

| Scenario | Our sim |
|---|---|
| `Single_06` | **NOT_RUN: scenario create refused** - `script offset 828: unsupported action: QUESTLOG_SHOW` |
| `Single_16` | **NOT_RUN: scenario load refused** - `players[3].SCHIFF[0].PRIVAT.CREW.ENTRY[0].PRIVAT.ESEL.TASK: unsupported unit record/state` |
| `01_1024_easy_256`, `--house-updates` off (default; the House tick is a no-op by rule) | **residents**: 230 at every checkpoint (t=0/10/30/60s) - flat, as this rule setting predicts. **town_store_tons**: 150 at every checkpoint - also flat (unlike `endless01.sze`'s own cross-check below, this scenario's non-house production does not move its store total either). `check_invariants`: clean at every checkpoint. |
| `01_1024_easy_256`, `--house-updates` on | **residents**: 230 (t=0s), 230 (t=10s), 230 (t=30s), **332** (t=60s) - growth does happen, between the 30s and 60s checkpoints, once the rule is on. **town_store_tons**: 150, 150, **149**, **147** - a small, real decline. `check_invariants`: clean at every checkpoint. |
| `S01_08_01` | **NOT_RUN: scenario load refused** - `players[3].SOLDAT[0]: unsupported actor kind/class/import facts 708 at offset 365132` |
| `Single_24` | **NOT_RUN: scenario load refused** - `players[9].SOLDAT[0]: unsupported actor kind/class/import facts 704 at offset 471591` |

Four of the five scenarios cannot run in this engine yet at all (each for a different,
already-known-class reason: an unsupported script action, an unsupported unit/crew record, two
unsupported soldier/actor import cases) - this is itself the evidence this row asks for, not a
gap in this pass. Only `01_1024_easy_256` is measurable. With `house_updates` on, our sim *does*
grow residents in this window (230 -> 332, between 30s and 60s) - closer in kind to the
co-developer's own numbers (his code, not the reference: 230 -> 276 -> 322) than the first,
uncorrected pass suggested, though the shape differs (his grows from the first 10-second mark;
ours holds flat through 30s then jumps). With `house_updates` off (this engine's present default),
residents stay flat the whole window. Both are recorded, neither diagnosed further - see §4.

## 3. The original (Wine runner)

**NOT_RUN for this package's own pass.** The harness's documented navigation (`docs/testing/
original-wine.md` §2.1, `tools/orig/macros.py new_game`) only reaches the random-seed "New game ->
Citizen" start, not a specific shipped scenario file - loading e.g. `Single_06.sze` by name needs a
different main-menu path (likely "Historical missions"/a scenario picker, not explored or
documented yet) that was not navigated in this pass. `tools/orig/gamestate.py` already has a
direct memory reader for a city's `inhabitants` field (`+0x2f0`, `docs/re/population.md`) through
`cities()`, so once the right scenario-file menu path is found, reading residents at real-time
checkpoints (game time tracks real time within tens of ppm at 100% speed, `docs/testing/
original-wine.md` §3) needs no OCR and no further RE - only the navigation. Left for a follow-up
pass of this same packet or a dedicated task; not guessed here.

## 4. Differences (OQs, not fixes)

- **OQ-571 (updated by F6, 2026-11-11): `01_1024_easy_256`'s growth depends entirely on
  `Rules::house_updates`, and even with it on the shape of growth still differs from the
  co-developer's own numbers.** Status: open. With `house_updates` off (this engine's default),
  residents and the town-store total are both flat over 60 game seconds - expected under that
  rule, not a finding. With it **on**, residents do grow (230 -> 332) but only between the 30s and
  60s checkpoints, not from the first 10-second mark as the co-developer's own code shows (230 ->
  276 -> 322) - a real difference in *when* growth starts, not just *whether* it does. This could
  be a real timing/threshold difference in our sim's house-growth logic (worth investigating on
  its own), a difference in what "60 game seconds, no player action" means between the two engines
  (e.g. whether the scenario's own script runs a seeded/scripted event in that window), or neither
  engine matching the original yet (the original's own number for this row is still NOT_RUN - §3).
  Recorded as a difference, not diagnosed or fixed here (the task's own instruction: measurement
  only).
- Four of five scenarios cannot be measured in our sim at all (§2) - each already has its own
  named unsupported-feature class; no new OQ needed per scenario, they are symptoms of already-known
  gaps (unsupported script actions, unit records, actor imports), not new findings.

## 5. What's kept

- `src/tools/sim_replay/main.cpp`'s `population` subcommand and its tests
  (`src/tools/sim_replay/CMakeLists.txt`, `cmake/RunPopulationTool.cmake`) - committed, reusable.
- No scripts/logs from a Wine pass yet (§3 is NOT_RUN); when that pass runs, its session output
  belongs under `cache/orig/<session>/` as usual, not in git.
