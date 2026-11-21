# Review: ROADS-3 (UI-ROADS-1 — press-hold L-drag road building in the menu)

Reviewer: Windows orchestrator, 2026-11-21.
Branch: `task/ROADS-3`, prerequisite `--no-ff` stack merges `280141a0` (`origin/task/ROADS-0` `944d317f`), `024091c2` (`origin/task/ROADS-1` `45dc2c3e`), `09a7e53c` (`origin/task/UI-GHOST-1` `7a51050d`), feature+docs commit `56c7cbfad1a8a27607efdb205303973d56dcbf30`, skip-classification fix + report commit `7288379f94d0441b028a7ee09ae38aa05534fbab` (synced identically on `origin`, `gitlab`, and `share`).
Packet: `docs/orchestration/tasks/roads-3.md` on `task/ROADS-3`.
Standing reference: `docs/BUILDINGSOBJECTS.md` part B §10.1 items 1, 2, 3, 9, 10.
Exact-head GitHub Actions CI: run `44904803422` (`https://github.com/Dom-cmyk727/anno_rebuild/actions/runs/44904803422`) on `7288379f94d0441b028a7ee09ae38aa05534fbab` — **12/12 GREEN** (`changes`, `lint`, `build-macos`, `test (ubuntu-24.04)`, `cpp-format`, `test (windows-latest)`, `cpp (linux-gcc-release)`, `cpp (macos-clang)`, `cpp (linux-gcc)`, `cpp (windows-msvc)`, `cpp (linux-clang-asan)`, `gate`).

**Verdict: ACCEPTED WITH NOTES.**

---

## Verified Strengths (inspected directly in `09a7e53c..7288379f`)

- **Clean lineage & scope hygiene:**
  - Verified ancestors: `ROADS-0` (`944d317f`), `ROADS-1` (`45dc2c3e`), `UI-GHOST-1` (`7a51050d`), and `origin/main` (`a6468efd`) all exit `0`.
  - `git diff --stat 09a7e53c..HEAD -- fixtures/ docs/open-questions.md` is empty (`kStateHashVersion = 139`, `0` fixture diffs, reserved OQ range `785–789` unused and accurately reported as unused).
  - Independently verified `python -I tools/lint_sim.py` exits `0` (`lint_sim: ok`) and `clang-format 23.1.3 --dry-run --Werror` exits `0` on all 24 touched/added C++ files.
- **Read-only sim path probe (`src/sim/`):**
  - `World::check_build_path` / `Commands::check_build_path` (`apply.cpp:1117-1187`, `road_drag.hpp:29-35`) evaluates a multi-record road path read-only with cumulative virtual `Budget` spending while affordable (`spend`), switching to footprint-only virtual placement (`record_budget_footprint`, `check_cost = false`) past the first `CannotAfford` so `valid_prefix` stops at the first site obstacle while `affordable_prefix` stops at the first unaffordable tile (matching `AnnoFrame 0x1004b1ed-0x1004b263` and kind-9 `stop_at_first_refusal`). State hash invariance (`w.state_hash() == hash_before`), partial affordability, obstacle cutoff, same-family free re-lay, downgrade refusal, and custom `drag_cap` are all tested in `road_drag_test.cpp:630-716`.
  - Compiled `RoadRules{}` default remains `enabled = 0` (`0` pins moved in `pins.log`); `--road-rules on|off` defaults `on` in interactive `--play` (`main.cpp:357-358`, `738-740`).
- **Menu L-drag, ghost/blink/refusal & UI-side variants (`src/app/`):**
  - `RoadVariantDeck` & `road_variant_id` (`road_drag_ui.{hpp,cpp}`) use `HEAD[11]` (`property 0x32` via `sim::house_variant_facts`, verified against original `Anlagen.dat` `2520` 5 variants and `2500` 4 variants plus arbitrary synthetic ID `4100` in `road_drag_ui_test.cpp`), cache per-index bytes strictly UI-side so preview == commit (`tool.road.commit_matched_preview == 1`), and reroll cleanly post-commit.
  - `plan_road_ghost` (`road_drag_ui.cpp:69-95`) and `Viewer` (`viewer.cpp:2934-3100, 3416-3438, 4121-4133, 4527-4535`) draw the affordable prefix solidly, blink the unaffordable tail using `UI-GHOST-1`'s `ghost_blink_` accumulator (`699/700 ms`), draw the `0xffff00` 1x1 refusal rectangle only when `valid_count == 0`, draw zero radius outline and zero fertility bar (`tool.fertility == none`), keep the road armed on release (`tool.armed == 2520`), and disarm/cancel cleanly on right click.
  - `commands_file.cpp` emits backward-compatible `buildv`/`buildcv` only when a single `BuildCmd` mixes variant IDs (`commands_file_test.cpp`).
- **Evidence, failure proof & raw logs independently verified (`cache/roads-3/`):**
  - Deliberate failure proof mutated `plan_road_ghost` (`draw_refusal_box = false`, `draw_count = 0`) and failed both `viewer_wiring_ui_roads` and `roads_ghost_pixel_check.py --renderer cpu` at line 46.
  - `pins.log` (`abb12ffe1c0f`): `selected=1734 executed=1713 failed=0 empty=0 skipped=21 (opt-in=18 other=3)`, `0` memory refusals, `0` pins moved, `0` fixture diffs (status `6` solely from the 3 pre-existing Windows symlink `ship start:` `other` skips passing on Linux CI `44904803422`).
  - `gate-msvc-app.log` (`3f2b7159e50d`): `selected=503 executed=500 failed=0 skipped=3 (opt-in=3)` `VERDICT: PASS`; all 12 required contracts (6 UI oracles + `viewer_wiring_roads_picture_objects` + `viewer_wiring_ui_ghost*` + `viewer_wiring_ui_roads*`) PASS 1/1 (`source_before == source_tested`).
  - `gate-msvc-gpu.log` (`d0ac428e9dc0`): `2/2` PASS + both magnifier GPU contracts; `ui-roads-gpu.log` (`675e7deac3d7`): exit `0` (`5.5 s`).
  - `unix-warnings.log` (`b97091b30935`): GCC 13.3 + Clang 18.1.3 (`442 + 97` files) clean, both negative controls caught.
  - Post-skipfix (`7288379f`, `0` diffs in `src/` or `fixtures/`): `gate-roads-pixel-noassets.log` (`ab8a090d1b71`, `required-asset=2 other=0`, `VERDICT: PASS`) and `gate-msvc-post-skipfix.log` (`bdb16fb6935b`, all 12 required contracts PASS 1/1).
  - `REPORTS.md` includes the 5-row Acceptance Table, failure proof, `NOT_RUN`, and `Decisions and reasoning`.

---

## Non-Blocking Notes

1. **`--ship-start` (`opening`) does not forward `--road-rules` yet (`src/app/src/main.cpp:338-358, 738`):** `--road-rules on` defaults on in `--play`, while `--ship-start` keeps default `road_rules.enabled = 0`. Fine for `ROADS-3`'s menu-in-`--play` scope; wire into `OpeningOptions` in a later pass if ship-start road dragging is wanted.
2. **Gate log details:** `pins.log` exited status `6` solely due to the 3 pre-existing Windows symlink `ship start:` `other` skips (`0` failed, `0` refused; all 3 pass on Linux CI `44904803422`), and GPU desktop screenshot capture in `roads_ghost_pixel_check.py --renderer gpu` was `NOT_RUN` due to the locked Windows desktop (`ui_roads --gpu` itself ran and passed in `5.5 s`, and CPU pixel proof verified `1328/961/840 px` diffs and the `0xffff00` refusal diamond).
3. **Stacking:** `task/ROADS-3` (`7288379f`) already cleanly stacks `ROADS-0` (`944d317f`) + `ROADS-1` (`45dc2c3e`) + `UI-GHOST-1` (`7a51050d`) on `BUILDINGS-REF-2` (`60a26f0d`) / Candidate 10 (`95897a55`) + `origin/main` (`a6468efd`), so it carries all four into the post-Candidate-11 candidate wave cleanly once Candidate 11 lands.
