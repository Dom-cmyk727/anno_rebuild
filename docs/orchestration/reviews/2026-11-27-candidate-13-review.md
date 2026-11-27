# Candidate 13 — landing review (ORCH III, 2026-11-27)

**Candidate:** `refs/candidates/cand13` = `aeb654245645c030d38483bf288c48646635abfd`. Base `origin/main` `2bba18e3`
(an ancestor). Verdict: **LANDS**, through `land.py`, on the owner's answers below. The advisor reviewed the bundle's
results (owner rule 2026-11-27: advisor review per bundle); its four blocking checks are §3.

## 1. Contents

Candidate 12 `c5af390c` (candidate 11 `e2e4b617` = candidates 6-11; UI-GHOST-2 with ROADS-0/1/3, UI-GHOST-1,
BUILDINGS-REF-2; GATE-2a; FLAKE-RESIZE-1; WATER-SUBMERGED-1; UI-CATNAV-1), then the UI lane's owner-test merge
`b1315085` (UI-HARVEST-ICON-1 B/C + R1 `6ee17e00`, UI-RANGE-1 A + B1 `bdaeff91`, ROADS-2a + 2b `83e16424`), then
CAND12-PATCH-SUB-1 `f0a5b3c1`, RENDER-DIRECT-2a `bf185b2e`, PICTURE-IDS-1 `6cd2dfb9`. Every packet: CI green on its
reviewed head and accepted by the orchestrator. Merge conflicts were resolved keeping both sides; the open-questions
register is in numeric order again (the owner-test merge had OQ-902 before 795); `src/app/CMakeLists.txt` test list is
the union.

## 2. Owner answers (verbatim)

- Asked whether one full game-file run on this PC (GATE-2a's retry) may stand in for the Mac's run: **"Yes, PC run
  lands it"**.
- Asked whether all accepted packets land with that single run or only B0: **"All in one PC run"**.
- Asked whether the forester check counts as passed (the icon clears on the first wood, GOG `0x10072ff1`): **"Yes,
  passed"**. Viewer checks on the integration build `d5cdbc21`: WATER-SUBMERGED-1 **pass**, UI-CATNAV-1 **pass**.
  Roads on `b1315085`: "looks good" (relayed by the UI lane).

## 3. Evidence

- **Full run** (`cache/gates/cand13-full-01.log` in `E:\anno1503-worktrees\cand12`; MSVC Release, game files, this
  PC): `COUNTS (after retry): selected=2928 executed=2892 failed=1 refused=0 skipped=35 (platform=3 opt-in=27 other=5)`,
  printed verdict **FAIL failed tests**. Five render cases were refused for memory and passed when retried alone.
- **The one failure,** `anno_test_support_tests.F4 (N4)` (`budget_f4_test.cpp`, `REQUIRE(false)`, log `nohup: failed
  to run command '.../cand12/cache/coord/anno_budget_coord-208a2c085d5db934': No such file or directory`): the WSL
  coordinator binary was not staged yet in a fresh candidate tree when the case started. Rerun alone: PASS
  (`cand13-f4-rerun-01.log`); 20 loops alone: 20/20 pass. Test infrastructure, not candidate code. Owner: the Windows
  render lane, packet FLAKE-COORD-STAGE-1 (queued).
- **Skips:** 27 opt-in measurements; 3 platform; 3 `ship start:` content-path cases (Windows symlinks; they pass on
  Linux CI); `info_panel_gog_compare` and `info_panel_rotation_gog_compare` (shots not set). With the shots set
  (`ANNO_UI_ORACLE_SHOTS`, `ANNO_UI_ROTATION_SHOTS`): the five GOG compares 5/5 PASS (`cand13-gog-01.log`,
  `cand13-gog-rot-01.log`).
- **GPU:** both adapters ran (log lines naming RTX 4090: 70, Radeon: 59); 0 gpu skips.
- **The two 6.84 GiB digest corpus cases** (UI-GHOST-2 review note): `scene renders match the recorded digests` PASS
  (256 s), `legacy river shading reproduces the previous enhanced digests` PASS (607 s).
- **Each packet's own cases executed:** the 2a instance corpus and lan5 cases (4 + 2), the bucket case, the 10 road
  shape cases plus `road_shape_reference_selftest`/`_corpus_reference_agrees`, the six range cases and
  `viewer_wiring_ui_range`, the PICTURE-IDS guard, `viewer_wiring_ui_catnav`, the ui_ghost2 cases,
  `viewer_wiring_status_icon_zoom_gpu`.
- **WSL GCC 13 / Clang 18:** clean on all four configurations (453 + 102 files), both negative controls caught
  (`cand13-unix-warnings.log`).
- **Content:** no change under `game/` or `.github/`; binary files are synthetic fixtures of accepted packets;
  `fixtures/synthetic/sim/road_shape_corpus.txt` holds per-island counts only (141 lines), no original data.
- **ROADS-2's `src/sim` change** read by the orchestrator: ordered containers only, no float, no time; the reshaper runs
  only while `road_rules` is enabled (header tag 2); the drag-cap rule is labelled L (OQ-903); `lint_sim: ok`.
  `abaf6595..bdaeff91` is one docs line.

## 4. NOT_RUN

- The Mac's full run (Mac away; the owner's PC-run answer stands in).
- RENDER-DIRECT-2a's Release load-time and memory numbers (the PC was never quiet).
- A game-file sanitizer run (ASan/UBSan run only in hosted CI, without game files).
- Hosted CI on `aeb65424` itself runs as `land.py`'s `gate`.
