# Review: ROADS-0 (PICTURE-OBJECTS-1 — runtime tile objects & cleared scenery in picture)

Reviewer: Windows orchestrator, 2026-11-21.
Branch: `task/ROADS-0`, base `60a26f0d` (`BUILDINGS-REF-2` on Candidate 10 `95897a55`), packet `c3489403`, reviewed code+docs commit `b0b022c6` (`b0b022c691712a9898d0aa1956027eca193bdf8d`), report commit `17d8d85e` (`17d8d85e15ab972550bc08fe27fa256df5ca7e1c`, synced on `origin`, `gitlab`, and `share`).
Packet: `docs/orchestration/tasks/roads-0.md` on `task/ROADS-0`.
Standing reference: `docs/BUILDINGSOBJECTS.md` part B §10.1–10.2.

**Verdict: CHANGES REQUESTED (Fix Round 1).**

---

## Verified Strengths (inspected directly in `c3489403..17d8d85e`)

- **Strict scope discipline:** `git diff --stat c3489403..17d8d85e -- src/sim/ src/render/ fixtures/` is empty (`0` sim changes, `kStateHashVersion = 139`, `gpu.wgsl`/`gpu.cpp` sea passes completely untouched).
- **Generic, unbounded tile-object delta pipeline:**
  - `SceneTileObject` (`src/app/include/anno/app/loader.hpp:94-102`) carries `uint32_t island`, `uint32_t type` (unbounded; `0` = cleared tick-0 scenery marker), `x, y, rotation, height, phase` (6-bit `phase & 0x3F`, matching `formats::insel::PlacedObject::phase()` in `insel.hpp:87`).
  - `TileObjectDelta`, `diff_sorted_tile_objects`, `BuildingMailbox` resync collapse & `apply_tile_object_batches` (`building_delta.{hpp,cpp}`) mirror the existing `BuildingDelta` total-order multiset design, with unit tests (`building_delta_test.cpp`, `building_mailbox_test.cpp`).
  - `PlayRunner` (`play.cpp:449-480, 962-1052`) snapshots tick-0 `initial_scenery_` anchors and emits tracked `kRoadFlag` objects plus `type == 0` cleared markers when tick-0 scenery becomes empty or overwritten by a building — avoiding spurious batches from passive `ROHSTWACHS` regrowth while preserving flag-`0x10` rocks under mines/buildings.
  - `ScenePicture` (`scene_picture.cpp`) and `apply_island_buildings` (`loader.cpp:329-600`) index per-island tile objects, emit `PROD2` items with `(id, rotation, height, phase)`, suppress cleared/covered scenery, and keep `buildings_version_` scoped to `KnownBuildings` building-footprint occupancy.
  - `viewer.cpp:3231-3235` publishes `picture.tile_objects` and `picture.cleared_tiles` facts; `roads_picture_objects.script` and updated `boundary_view.script` cover live visibility (`Visible` vs `NoVisualChange`).
- **Strong pixel + wiring proofs & failure proof:** `tile_object_picture_test.cpp` (608 lines) verifies placed road `Visible`, pixel diff vs bare ground, rot 0 vs rot 1, fresh `load_placed_scene` reference equality, demolish back to bare ground, tree suppression under building/road/direct demolish, and collapsed mailbox resync. Deliberate failure proof (`tile_deltas.clear()`) broke both `tile_object_picture` (2/2) and `viewer_wiring_roads_picture_objects`.
- **Formatting & local MSVC Release gates:**
  - Independently verified `clang-format 23.1.3 --dry-run --Werror` exits `0` on all 13 modified/added C++ files.
  - Post-final-edit logs all share `source_before == source_tested` (`diff_sha256: ec762457a54f02f9b22d64494e1412ee8a1bbb5901284971795b89969639d2ad`) and `GATE_END {"status": 0, "ctest_rc": 0}`:
    - `cache/roads-0/gate-msvc-app-2.log`: 7/7 required tests (all 6 UI oracles + `viewer_wiring_roads_picture_objects`) PASS 1/1; main selection `selected=462 executed=459 failed=0 skipped=3 (opt-in=3)` `VERDICT: PASS`.
    - `cache/roads-0/gate-msvc-gpu-2.log` (RTX 4090): `selected=15 executed=15 failed=0 skipped=0` `VERDICT: PASS`.
    - `cache/roads-0/gate-msvc-render.log`: `selected=211 executed=211 failed=0 skipped=0` `VERDICT: PASS`.
    - WSL `tools/check_unix_warnings.sh`: reported clean (GCC 13 & Clang 18, 436 + 93 files).

---

## Blocking Findings (F1–F2)

### F1 [BLOCKING — Missing `origin/main` CI Pin (`a6468efd`) & Red Hosted CI]
- **Evidence:**
  - `git merge-base --is-ancestor a6468efd HEAD` exits **1** on `task/ROADS-0`.
  - Exact-head GitHub Actions CI run `44730641751` on `17d8d85e` **FAILED** on all three Linux C++ jobs (`cpp (linux-gcc)` `anno_sim_replay_g3_run`, `cpp (linux-clang-asan)` `<ciso646>` error, `cpp (linux-gcc-release)` `freight_state.hpp:33:8 -Werror=null-dereference`) because the workflow still ran on `ubuntu-26.04` / GCC 15.2. (`cpp (windows-msvc)`, `cpp (macos-clang)`, `cpp-format`, `test`, `build-macos`, and `lint` all passed.)
  - Dry-run `git merge-tree --write-tree 17d8d85e a6468efd` succeeds cleanly (`6426e07c`, 0 conflicts; only `.github/workflows/ci.yml` + `docs/orchestration/*` change).
- **Fix:** Merge `origin/main` (`a6468efd`) into `task/ROADS-0` with an ordinary `--no-ff` merge (no rebase, no force-push), push, mirror with `python -I tools/orch/sync_gitlab.py --repo .`, and obtain an exact-head **12/12 GREEN** GitHub Actions CI run (which also covers `cpp (linux-clang-asan)`).

### F2 [BLOCKING — Report Accuracy & Mandatory Acceptance Table in `REPORTS.md`]
- **Evidence (`docs/testing/evidence-contract.md`, `docs/orchestration/REPORTS.md`):**
  1. `REPORTS.md` line 16848 states: `"Settled OQ-775..779 for runtime road drawing and cleared scenery suppression under placed/demolished footprints."`, but `docs/open-questions.md` was not edited and `OQ-775..779` do not exist (`775–779` was the packet's reserved range for new OQs if needed, unused here).
  2. `REPORTS.md` omits the mandatory Acceptance Table (`Requirement | Positive proof | Counterexample / negative control | Evidence | Reviewer disposition`) required by `docs/testing/evidence-contract.md`.
- **Fix:** Append a concise Fix Round 1 addendum to `docs/orchestration/REPORTS.md` (and fix/clarify the inaccurate `Settled OQ-775..779` claim to state that reserved OQ range `775–779` was unused with no new OQs opened), include the mandatory Acceptance Table covering all `ROADS-0` requirements, and record the exact-head green CI run ID.

---

## Non-Blocking Notes (do not require code changes unless touched)

1. `src/app/src/scene_picture.cpp:410` uses the literal `0x4000U` rather than `sim::kRoadFlag` (`types.hpp:264`, same value `0x4000`). Fine as-is to avoid unnecessary C++ rebuilds in Fix Round 1 (if you do touch any C++ file, re-run the affected MSVC Release gate, `clang-format 23.1.3`, and `tools/check_unix_warnings.sh`).
2. `PlayRunner::collect_tile_objects()` performs a linear scan over island tiles per turn; `-L heavy` (`play: the building diff on the heavy endless scenario`) stays recorded in `NOT_RUN` (monitor if large-map turn timing increases in later perf passes).
