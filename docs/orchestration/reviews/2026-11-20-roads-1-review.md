# Review: ROADS-1 (SIM-ROADS-1 — original road drag rules in the sim)

Reviewer: Windows orchestrator, 2026-11-20.
Branch: `task/ROADS-1`, base `df15310b` (`df15310b0a8c3792b296cf6c626a6b54e9e2a9ed`), reviewed code+docs commit `bb3dcaf6`, report commit `0db39280`.
Packet: `docs/orchestration/tasks/roads-1.md` on `task/ROADS-1`.
Standing reference: `docs/BUILDINGSOBJECTS.md` §10.1.

**Verdict: CHANGES REQUESTED (Fix Round 1).**

---

## Board Reservation Authorized by This Review

- **ADR 0006 header extension tag 2 (`road_rules`)** is reserved for `ROADS-1` on `orch/board-next` (verified unused across `origin/main` `a6468efd`, `origin/orch/board-next` `8fc3e47a`, and `origin/orch/mac-lane` `c8c6d773`; next free header extension tag is **3**).
- The permanent allowlist entry in `tools/lint_sim_allow.txt` for `src/sim/include/anno/sim/extension_context.hpp:static constexpr std::uint16_t tag = 2;` is authorized (same discriminant pattern as `fisher_rules` tag `1`).

---

## Verified Strengths (inspected directly in `df15310b..0db39280`)

- **`road_path(start, end, cap = 40)`** (`src/sim/src/road_drag.cpp:11-52`): deterministic integer-only (`int64_t` deltas, no globals, no floats), L-path with longer axis first and `y` first on tie (`adx > ady`, `AnnoFrame 0x1004b392`-`0x1004b51f`), empty when `cap == 0`, truncates cleanly at `cap`.
- **`footprint_all_owned_rule`** (`src/sim/src/placement.cpp:489-509`): matches `footprint_owner_rule`'s `inside_layer` / `clip_to_layer` tile-indexing contract, rejects out-of-island footprints (`Reject::OutOfIsland`), and rejects both neutral (`cfg.neutral_owner`) and non-target-city tiles (`Reject::TileNotOwned`).
- **Data-driven road family and downgrade classification** (`src/sim/include/anno/sim/placement.hpp:125-161`, `src/sim/src/placement.cpp:267-292`): `RoadParams` (`road_group = 0xc1`, `bridge_prodtype = 198`, `road_family_flag = 0x100`, `cost_gold` + `cost_goods` equality), with no hardcoded type IDs or gold literals; verified across `Anlagen.dat` entries `2500..2537` (`road_drag_test.cpp:387-423`, 107 assertions passing in `roads1-final.log`).
- **ADR 0006 framing preserved**: `kStateHashVersion` stays `139`; default `RoadRules{}` (`enabled = 0`) emits 0 extension bytes, 0 fixture changes (`git diff df15310b..0db39280 -- fixtures/` empty), and `0 pins moved` in `pins.py check`.
- **`check_road_figure_veto`** (`src/sim/src/road_drag.cpp:54-128`): mirrors `founding.cpp:271-333`, including `trunc_tiles` (`pos / 256`, matching `figure_veto.cpp:44`), owner/relation/crew occupancy assembly, and `veto::nearby_figures`.
- **Formatting and local Unix warning build**: `clang-format 23.1.3` reported clean and local WSL `tools/check_unix_warnings.sh` (GCC 13.3 + Clang 18.1.3 `-O3 -Werror`) reported clean.

---

## Blocking Findings (F1–F8)

### F1 [BLOCKING — Determinism Lint Failure]
- **Evidence:** Running `python -I tools/lint_sim.py --root E:/anno1503-worktrees/anno1503-ROADS-1` exits **1**:
  `src/sim/include/anno/sim/extension_context.hpp:80: [constexpr_count] static constexpr std::uint16_t tag = 2;`
- **Impact:** Confirmed as the sole failure in GitHub Actions run `44716252854` job `cpp (macos-clang)` (`FAILED: anno_sim_tests_lint_sim_passes`, 2,366/2,367 executed passed). Missed locally because `cache/gates/roads1-final.log` passed `--select "^anno_sim_tests\.(road_path:|road drag:|anno_sim_tests_lint)"` where the literal `\.` fails to match the underscore in `anno_sim_tests_lint_sim_passes` and `anno_sim_tests_lint_sim_self_test`.
- **Fix:** Add the authorized `tools/lint_sim_allow.txt` entry for `tag = 2` (matching `tag = 1` on line 173) and require both `anno_sim_tests_lint_sim_passes` and `anno_sim_tests_lint_sim_self_test` via `--require-test`.

### F2 [BLOCKING — Code Bug: Duplicate Figure Veto Call]
- **Evidence:** Inside `check_record()` in `src/sim/src/apply.cpp`, the exact same `check_road_figure_veto` block appears twice:
  - lines `447-455` (before the `construction_eligibility` loop), and
  - lines `469-477` (immediately after the `construction_eligibility` loop).
- **Fix:** Remove the duplicate block; keep the single call after `construction_eligibility` (or document and match `BUILDINGSOBJECTS.md §10.1` 4(e)-(f): `City::Build`/eligibility before figure veto `0x10045080`) and add a precedence unit test where both `ConstructionDenied` and `FigureInTheWay` apply.

### F3 [BLOCKING — Code/Contract Bug: Prefix Stop Leaves `Reject::None` on Tail Records & Stops Non-Road `BuildCmd`s]
- **Evidence (`src/sim/src/apply.cpp:680,735-738`, `src/sim/include/anno/sim/reject.hpp:121-132`):**
  1. **False `Reject::None` on skipped tail records:** `build()` resizes `result.records` to `cmd.placements.size()`, whose elements default-construct with `RecordResult::status == Reject::None`. When `break;` executes at the first refusal index `k`, records `k+1..n-1` are never visited and remain `Reject::None` even though they were neither skipped by `type == 0` nor placed.
  2. **Over-broad scoping to non-road `BuildCmd`s:** `if (env.w.road_drag_enabled() && env.w.road_rules().stop_at_first_refusal != 0) break;` fires on **any** refused `BuildCmd` record when `road_drag_enabled()` is true — breaking `state.md §8.2` item 5 (`op 6` `0x10058a6d` continues past a failed building placement) for ordinary multi-building `BuildCmd`s whenever `RoadRules::enabled == 1`.
- **Fix:**
  - Append `Reject::PrefixStopped` to the append-only `Reject` enum in `src/sim/include/anno/sim/reject.hpp` (plus `reject_name()` and `docs/sim/state.md`), set every trailing unvisited record after an early prefix stop (refusal or cap cutoff) to `Reject::PrefixStopped`, and assert the exact status of every tail record in `road_drag_test.cpp`.
  - Scope kind-9 prefix stop (`stop_at_first_refusal`) to road placements (`is_land_road(*type)` on the failing record / road drag command, not ordinary buildings), add a negative test proving a multi-building `BuildCmd` still continues past a failing first building with `RoadRules::enabled == 1`, and clarify/test interaction with `Rules::build_atomic`.

### F4 [BLOCKING — Dead Persisted State: `RoadRules::drag_cap` Unenforced & Invalid Default-Off Payloads Allowed]
- **Evidence (`src/sim/include/anno/sim/extension_context.hpp:68-103`, `src/sim/src/apply.cpp`):**
  - `RoadRules::drag_cap` (default `40`) is serialized in ADR 0006 header tag 2 and validated `drag_cap != 0`, yet `apply.cpp` never reads `env.w.road_rules().drag_cap`! A `BuildCmd` with 50 road placements places all 50 even when `drag_cap == 40` (or `drag_cap == 3`).
  - `RoadRulesExtension::validate()` currently permits `enabled == 0` alongside modified subfields (e.g., `enabled = 0, drag_cap = 10`), which serializes non-default tag-2 bytes that change state hashes while remaining functionally inert.
- **Fix:**
  - Enforce `env.w.road_rules().drag_cap` in `apply.cpp::build()` for road placements (process at most `drag_cap` non-skip road placements; mark excess road placements `Reject::PrefixStopped` or defined refusal), and prove both default `40` and custom `drag_cap` (plus a mutation control reverting cap enforcement) in `road_drag_test.cpp`.
  - In `RoadRulesExtension::validate()`, refuse `payload.enabled == 0 && payload != default_value()` (just as `fisher_rules` allows no inert non-default payload).

### F5 [BLOCKING — Scope Guards (`tile_object` vs `is_land_road`), Option Toggles & Tag-2 Persistence Coverage]
- **Evidence:**
  1. `src/sim/src/apply.cpp` lines `352`, `376`, `448`/`470`, `504` gate `footprint_all_owned_rule`, `no_downgrade`, `check_road_figure_veto`, and `same_family_free` on `out.tile_object` rather than road identity (`out.tile_object && is_land_road(*type)`). Once `FERTILITY-1` crop/tree tile objects (`flags 0x29`) become placeable, gating on bare `tile_object` will wrongly apply road drag rules to fields/trees.
  2. `src/sim/src/apply.cpp:505` initializes `bool all_same_family = true;` before `for_each_footprint_tile(...)`; guard against empty iteration (`bool any_tile = false; ... same_family_free = any_tile && all_same_family;`).
  3. **Untested `RoadRules` fallback options (`roads-1.md` rule 5):** No test exercises `every_tile_territory = 0`, `no_downgrade = 0`, `free_same_family = 0`, `stop_at_first_refusal = 0`, or `figure_veto = 0` when `enabled = 1`.
  4. **Incomplete ADR 0006 tag-2 persistence proofs (`extensions_test.cpp`):** `extensions_test.cpp` checks `kKnownExtensions` static schema assertions for tag 2, while `road_drag_test.cpp` only tests a simple save/load round-trip and `validate()`. Missing: proof matching `fisher extension` that `enabled = 1` moves only the `Header` hash-tree section and leaves sections `1..9` byte-identical, tag `1` + tag `2` co-exist in ascending order, malformed saved tag-2 bytes (corrupt flag/zero `drag_cap`) are refused by `World::load`, and invalid `RoadRules` are refused by `World::create`.
- **Non-blocking note (address or document cleanly):** `apply.cpp` passes `footprint(*type, rec.rotation)` to `check_road_figure_veto` while `founding.cpp:272` uses `veto::scan_extents(type->width, type->height, rec.rotation)` (`AnnoFrame 0x100450bc-0x100450f9`; identical for `1x1` roads, differs only for non-square pieces at rotation 1/3). Align or document with a test.

### F6 [BLOCKING — Missing `origin/main` CI Pin (`a6468efd`) & Red Hosted CI]
- **Evidence:** `git merge-base --is-ancestor a6468efd HEAD` is false on `task/ROADS-1`. Hosted CI run `44716252854` failed `cpp (linux-gcc-release)` on Ubuntu 26.04 / GCC 15.2 (`freight_state.hpp:33:8: -Werror=null-dereference`) and failed `cpp (macos-clang)` (`anno_sim_tests_lint_sim_passes`).
- **Fix:** Merge `origin/main` (`a6468efd`) into `task/ROADS-1` with an ordinary `--no-ff` merge (no rebase, no force-push), push, mirror with `python -I tools/orch/sync_gitlab.py --repo .`, and obtain an exact-head green CI run (which also covers `linux-clang-asan`).

### F7 [BLOCKING — Unresolved `low-memory stop` Gate Failures & Missing Required Tests]
- **Evidence:**
  - `cache/gates/pins-roads1.log` (`selected=1727 executed=1702 failed=4 empty=0 skipped=21 ... incomplete=0`) and `cache/gates/sim-refused-rerun.log` (`selected=4 executed=1 failed=3`, `VERDICT: FAIL`, `GATE_END status 1`) both ended FAIL with 3 heavy tests refused by `anno_budget_coord` (`low-memory stop`): `population A`, `scenario corpus`, and `scenario corpus: every island gets its crop list`. Per `docs/testing/README.md`, a refused test did not run and must pass on standalone `--jobs 1` rerun.
  - Post-edit `cache/gates/roads1-final.log` only ran 9 tests (and missed `anno_sim_tests_lint_sim_passes` via regex `\.`). Missing after final edits: full `pins.py check` to `0 failed` / `0 refused`, `placement_reference_agrees`, `placement_reference_selftest`, `placement_tile_corpus_reference_agrees`, `anno_sim_tests_lint_sim_passes`, `anno_sim_tests_lint_sim_self_test`, plus affected `anno_app_tests` and `anno_render_gpu_tests` on MSVC Release per `TASKS.md` §1 item 4 whenever `src/sim` changes. (UI pixel-oracle tests in the shared packet footer are explicitly N/A when `src/app`/`src/render` are untouched.)

### F8 [BLOCKING — Missing Mandatory Evidence-Contract Items in `REPORTS.md`]
- **Evidence (`docs/testing/evidence-contract.md`, `docs/orchestration/tasks/roads-1.md`):**
  - `REPORTS.md` omits the mandatory Acceptance Table (`Requirement | Positive proof | Counterexample / negative control | Evidence | Reviewer disposition`).
  - Mutation controls were demonstrated for only 2 rules (`road_path` tie-breaker and `is_road_downgrade`), leaving territory ownership, figure veto, free same-family re-lay, `drag_cap`, and kind-9 prefix commit without negative controls.
  - Missing explicit statement on `tools/placement_ref.py` impact (required by `roads-1.md`: unchanged vs updated, backed by `placement_reference_agrees` / `placement_tile_corpus_reference_agrees`).

---

## Fix Round 1 Re-Review (2026-11-20)

Reviewer: Windows orchestrator, 2026-11-20.
Branch: `task/ROADS-1`, merge `82111b9e` (`origin/main` `a6468efd` `--no-ff`), code+docs commit `ed040a55`, report commit `45dc2c3ea188d786b38e8cbd6e15f431f0047ca0` (synced identically on `origin`, `gitlab`, and `share`).
Exact-head GitHub Actions CI: run `44730763633` (`https://github.com/Dom-cmyk727/anno_rebuild/actions/runs/44730763633`) on `45dc2c3ea188d786b38e8cbd6e15f431f0047ca0` — **12/12 GREEN** (`changes`, `build-macos`, `cpp (linux-gcc)`, `cpp (linux-clang-asan)`, `test (ubuntu-24.04)`, `cpp-format`, `cpp (windows-msvc)`, `cpp (macos-clang)`, `cpp (linux-gcc-release)`, `test (windows-latest)`, `lint`, `gate`).

**Verdict: ACCEPTED WITH NOTES.**

### Verification of Blocking Findings F1–F8 (all closed)

- **F1 (`lint_sim`):** Closed. `tools/lint_sim_allow.txt:175` adds the authorized `static constexpr std::uint16_t tag = 2;` entry. Independently verified `python -I tools/lint_sim.py --root E:/anno1503-worktrees/anno1503-ROADS-1` exits `0` (`lint_sim: ok`); `anno_sim_tests_lint_sim_passes` and `anno_sim_tests_lint_sim_self_test` both PASS in `roads1-r2-contracts.log` and CI `44730763633`.
- **F2 (duplicate figure veto):** Closed. Pre-eligibility `check_road_figure_veto` removed in `src/sim/src/apply.cpp`; single post-eligibility call retained at `apply.cpp:466-474`; precedence test `road drag: ConstructionDenied precedes FigureInTheWay when both apply` added (`road_drag_test.cpp:299-318`).
- **F3 (`stop_at_first_refusal` tail status & scoping):** Closed. `Reject::PrefixStopped` appended to `src/sim/include/anno/sim/reject.hpp:119` (with `reject_name()` `L251-252` and `docs/sim/state.md:1844`). `apply.cpp:755-762` scopes early prefix stop to `stop_on_refusal && (chk.is_road || road_count > 0)` (with `out.is_road = is_land_road(*type)` set early at `apply.cpp:337`), marks trailing non-skip placements `Reject::PrefixStopped`, preserves ordinary multi-building `BuildCmd` continuation (`road_drag_test.cpp:442-463`), and covers `Rules::build_atomic` interaction (`L464-476`).
- **F4 (`drag_cap` & validation):** Closed. `apply.cpp:728-741` enforces `drag_cap` in `build()` for road placements (excess non-skip placements receive `Reject::PrefixStopped`, tested for default `40` and custom `3` in `road_drag_test.cpp:406-440`); `extension_context.hpp:102-104` refuses `payload.enabled == 0 && payload != default_value()` (tested in `extensions_test.cpp:426-433` and `road_drag_test.cpp:587-589`).
- **F5 (guards, toggles & tag-2 persistence):** Closed. `apply.cpp` lines `354`, `378`, `466`, `501` gate road rules on `out.tile_object && out.is_road`; `same_family_free` guards empty footprints via `any_tile` (`L503-519`); all 5 fallback toggles (`every_tile_territory=0`, `no_downgrade=0`, `free_same_family=0`, `stop_at_first_refusal=0`, `figure_veto=0`) are tested (`road_drag_test.cpp:478-563`); rotated 2x1 figure veto is tested (`L291-297`); `extensions_test.cpp:361-434` proves exact `+16 B` header-only delta, byte-identical sections `1..9`, `HashTree` moving only child 0, ascending tag `1` + tag `2` coexistence, corrupt tag-2 native bytes refused by `World::load`, and invalid `RoadRules` refused by `World::create`.
- **F6 (CI pin & green CI):** Closed. `git merge-base --is-ancestor a6468efd HEAD` exits `0` (`82111b9e`); exact-head CI `44730763633` is 12/12 GREEN. Independently verified `clang-format 23.1.3 --dry-run --Werror` exits `0` on all 11 touched C++ files and `git diff --stat df15310b..45dc2c3e -- fixtures/` is empty (`kStateHashVersion = 139`).
- **F7 (local gates, 0 refusals):** Closed. All 4 post-edit logs share identical `source_before == source_tested` (`head: 82111b9e`, `diff_sha256: 083f26d564c67bfdf6dbb50e42bb048b103b2f30fff32ea31732c500b1106e2c`):
  - `pins-roads1-r2.log`: `selected=1732 executed=1711 failed=0 empty=0 skipped=21 (required-asset=0 platform=0 opt-in=18 gpu=0 other=3) incomplete=0 sanitizer_lines=0`, `ctest_rc: 0`, `0` memory refusals, `0 pins moved; 0 fixture byte changes`.
  - `roads1-r2-contracts.log`: 4/4 required contracts (`anno_sim_tests_lint_sim_passes`, `anno_sim_tests_lint_sim_self_test`, `placement_reference_agrees`, `placement_tile_corpus_reference_agrees`) PASS 1/1 + selection `1/1` (`placement_reference_selftest`) PASS, `VERDICT: PASS`.
  - `roads1-r2-app.log`: `selected=369 executed=366 failed=0 empty=0 skipped=3 (opt-in=3)`, `VERDICT: PASS`.
  - `roads1-r2-gpu.log`: `selected=135 executed=134 failed=0 empty=0 skipped=1 (opt-in=1 gpu=0)`, `VERDICT: PASS`.
  - WSL `tools/check_unix_warnings.sh`: clean on GCC 13.3 + Clang 18.1.3 `-O3 -Werror` (437 + 92 files).
- **F8 (evidence contract):** Closed. `REPORTS.md` includes the complete 8-row Acceptance Table, explicit `tools/placement_ref.py: 0 changes` statement, and 7 documented mutation controls (`road_path` tie-breaker, `check_road_figure_veto`, `is_road_downgrade`, `same_road_family`, `footprint_all_owned_rule`, `drag_cap`, `stop_on_refusal`).

### Non-Blocking Notes

1. **Minor comment wording (`src/sim/src/apply.cpp:465`):** The comment states `footprint() swaps width/height on odd rotation, matching veto::scan_extents.`, whereas `veto::scan_extents` (`figure_veto.hpp:178`) returns `{type_h, type_h}` on odd rotation (original `mov ebp, eax` quirk) while `footprint()` returns `{type_h, type_w}`. Identical on all shipped `1x1` roads and explicitly unit-tested on synthetic rotated `2x1` roads (`road_drag_test.cpp:291-297`); harmless wording nit for a future touch-up.
2. **Edge case (`Rules::build_atomic == true` + `road_rules.enabled == 1` + road `BuildCmd` exceeding `drag_cap`):** `drag_cap` is enforced in the live loop (`apply.cpp:728-741`) rather than the `build_atomic` dry-run loop (`L685-710`), so an affordable >`drag_cap` road command under `build_atomic == true` would place `drag_cap` tiles and mark the excess `PrefixStopped` instead of atomically aborting all tiles. Benign because `build_atomic` is an off-by-default enhanced option and `make_road_drag_cmd` already caps at `drag_cap`; note for a future touch-up if needed.
3. **Gate log details & stacking order:** `pins-roads1-r2.log` exited status `6` solely due to the 3 pre-existing Windows symlink `ship start:` `other` skips (0 failed, 0 refused, `ctest_rc: 0`; all 3 pass on Linux CI `44730763633`), and `roads1-r2-app.log`/`roads1-r2-gpu.log` ran with `--exclude-label heavy` (0 failed, 0 GPU skips on RTX 4090 / Radeon). Lineage: `task/ROADS-1` (`45dc2c3e`) descends from `BUILDINGS-REF-2` (`60a26f0d`, stacked on Candidate 10 `95897a55`) + `origin/main` (`a6468efd`), so it queues for the next candidate wave **after** Candidate 11 (`e2e4b617`) lands on `main`.
