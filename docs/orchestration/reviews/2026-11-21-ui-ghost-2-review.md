# Orchestrator Independent Review — `UI-GHOST-2` (`ACCEPTED WITH NOTES`)

- **Date:** 2026-11-21
- **Reviewer:** Windows Orchestrator (`coordinator`, independent read-only audit)
- **Worker:** Swarm worker `win-exec-ui` (`OPEN1503 Windows Orchestration`)
- **Branch / Worktree:** `task/UI-GHOST-2` in `E:/anno1503-worktrees/anno1503-UI-GHOST-2`
- **Reviewed `HEAD`:** `825ad577aed24798b384ebb958fedbf2e05dedd7`
- **Merge Commit:** `f08e4ab01951716d2c92a809d4f4d982ca7c4524` (onto accepted `origin/task/ROADS-3` `7288379f94d0441b028a7ee09ae38aa05534fbab`)
- **Ancestor Stack Verified (`rc=0`):** `7288379f` (`ROADS-3`), `944d317f` (`ROADS-0`), `45dc2c3e` (`ROADS-1`), `7a51050d` (`UI-GHOST-1`), `a6468efd` (`origin/main`)
- **Remote Sync (`origin` / `gitlab` / `share`):** all `825ad577aed24798b384ebb958fedbf2e05dedd7`
- **Exact-Head GitHub Actions CI:** Run `44951265341` — `completed success` (`12/12` jobs green: `changes`, `test (windows-latest)`, `cpp-format`, `lint`, `test (ubuntu-24.04)`, `build-macos`, `cpp (linux-gcc-release)`, `cpp (windows-msvc)`, `cpp (linux-gcc)`, `cpp (linux-clang-asan)`, `cpp (macos-clang)`, `gate`)
- **Verdict:** **`ACCEPTED WITH NOTES`**

---

## Verified Strengths

1. **Scope & Formatting Discipline:**
   - Zero diff on `src/sim/`, `fixtures/`, and `docs/open-questions.md` (`kStateHashVersion = 139`; reserved OQs `790–794` unused).
   - `clang-format 23.1.3` passed (`rc=0`) across all 19 touched C++ files.
   - Touched scope matches packet boundaries: `src/render/` (`frame.{hpp,cpp}`, `gpu.{hpp,cpp}`, `scene.{hpp,cpp}`, `shaders/gpu.wgsl`, `tests/gpu_test.cpp`, `tests/render_test.cpp`), `src/app/` (`cli.{hpp,cpp}`, `main.cpp`, `loader.{hpp,cpp}`, `placement_ghost.{hpp,cpp}`, `placement_ghost_test.cpp`, `viewer.{hpp,cpp}`), `tools/test/ghost2_pixel_check.py`, `tools/test/viewer_wiring/ui_ghost2.script`, `tools/test/viewer_wiring/ui_ghost2_occupied.script`, `docs/BUILDINGSOBJECTS.md`, and `docs/orchestration/REPORTS.md`.

2. **Render & Ghost Semantics (`ObjectsPatch` & Refusal / Outline Behaviour):**
   - Ephemeral render-layer `ObjectsPatch` cleanly suppresses flag-`0x20` objects under valid, phase-0 unaffordable, and drawn road ghosts while preserving flag-`0x10` rocks via `overwrites()`, with zero `Scene` mutation, CPU and GPU max difference `<= 1`/channel, and immediate restore on cancel/clear.
   - Yellow `0xffff00` refusal rectangle matches `AnnoFrame 0x10048450` / `0x10047848`.
   - `--occupied-outline` is properly labeled as an ADAPTATION and defaults to OFF.

3. **Verified Gate Evidence (`head=f08e4ab0`, provenance `diff_sha256=b0f20e1e...` over all 25 non-`REPORTS.md` files in `825ad577`):**
   - `gate-final-app-ui.log` (`sha256: 65f8a242a37b...`): `selected=430 executed=427 failed=0 skipped=3 (opt-in=3)`, `VERDICT: PASS` + 18/18 required contracts (all 6 UI oracles, `roads_picture_objects`, `ui_ghost*`, `ui_roads*`, `ui_ghost2*`) PASS.
   - `gate-final-render.log` (`sha256: 755a4857a842...`): `selected=220 executed=220 failed=0 skipped=0`, `VERDICT: PASS`.
   - `gate-final-gpu.log` (`sha256: 9d03637716da...`): RTX 4090 `selected=20 executed=20 failed=0 skipped=0`, `VERDICT: PASS`.
   - `gate-final-gpu-wiring.log` (`sha256: a343a471656c...`): `ui_ghost2 --gpu` (`6.4 s`) & `ui_ghost2_occupied --gpu` (`3.9 s`) PASS (`BitBlt` screenshots `NOT_RUN (the desktop copy failed)`).
   - `gate-final-noassets.log` (`sha256: cff6ce072656...`): `selected=5 executed=1 failed=0 skipped=4 (required-asset=4 other=0)`, `VERDICT: PASS`.
   - `gate-unix-warnings-clean.log` (`sha256: b31889efe327...`): GCC 13.3 + Clang 18.1.3 (`442 + 97` files) clean.

---

## Non-Blocking Notes (Follow-up / Integration Check Items Only)

1. **`Viewer::ghost_update_patch` scene-identity check (`viewer.cpp:3118`):** Currently compares raw `const render::Scene* patch_scene_ == &scene`. Preferring `scene.generation()` in a follow-up avoids theoretical allocator pointer-reuse ABA if a `Scene` is destroyed and reallocated at the same address.
2. **Stacking with `WATER-SUBMERGED-1` (`05db2101`):** When stacking `UI-GHOST-2` and `WATER-SUBMERGED-1` during integration, check whether `patch_submerged_pipe` in `gpu.cpp` should also run under `WaterStyle::Enhanced` when `submerged_over_objects` is active.
3. **Host-memory exclusions & log location:**
   - Two `6.84 GiB` digest corpus `anno_render_tests` cases (`scene renders match the recorded digests`, `legacy river shading reproduces the previous enhanced digests`) were excluded locally (`5.65 GiB` free host RAM) and skipped in CI (no game files); schedule both in the clean post-Candidate-11 PC gate.
   - Live GPU `BitBlt` PNG capture reported `NOT_RUN (the desktop copy failed)` on the locked desktop (offscreen pixel readback and CPU/GPU invariants already proved `max <= 1`/ch).
   - Verified final gate logs reside in worker `win-exec-ui`'s scratch directory (`C:\Users\altin\.gemini\antigravity\brain\4bbe5656-4025-4dc2-be36-807fa18712cb\scratch\gates\`, hashes recorded above).
