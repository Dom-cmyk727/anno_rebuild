# UI-WINDOW-1, the board orchestrator's review (2026-11-14)

**Verdict: ACCEPTED for landing.** This is the board's check before landing, on top of the UI lane's own independent
review (`docs/orchestration/reviews/2026-11-14-ui-window-1-review.md` on the branch: ACCEPT WITH FINDINGS, findings
1–7 fixed in fix round R1).

Landing waits on two things:
- the owner's landing OK, which the orchestrator asks for itself (a peer session's relay is not the owner's OK);
- the game-file run of the landing candidate.

**Reviewer:** the Claude Code orchestrator (Opus 5.5).

**Reviewed head:** `task/UI-WINDOW-1` `9925860c` (code head `e107a8f6`). It contains `main` `f54e36f0`. CI run
43591693332 on `e107a8f6` is green in all 12 jobs, `gate` included; `9925860c` adds only the UI lane's review note.

## What the orchestrator checked

- **Scope.** 32 files against `main`, and none in `src/sim`, `src/render`, `crates` or `game/`.
  - `src/app`: the new `ui_transform`, plus `ui_layout`, `ui_mock`, the viewer's draw and click paths, `cli` and
    `dev_script`;
  - `data/ui`: anchors, masks and the construction grid;
  - docs: ADR 0004 R3, `ui.md` 5.1, `he-ui-scale.md`, OQ-614 to OQ-620;
  - tests: one compare script and four viewer-wiring scripts.
- **Publication** (GitHub is public).
  - No binary file (`git diff --numstat` shows none).
  - The `data/ui` additions hold only element names, positions, sizes, file and access names, anchors, method tags and
    shot names. No game text and no pixels.
  - The HE captures stay outside the repo, in `E:\anno1503-worktrees\he-ui-captures`.
- **The open-question numbers.** OQ-614 to OQ-620 are free on `main`, on `orch/board-next` and on every other
  `origin/task/*` branch.
- **The code** (`ui_transform.hpp/.cpp`, read in full for the header).
  - **Pure arithmetic, no SDL.** Each constant cites its HE address (step-down at `AnnoDisplay` 0x2ce11, fit at 0x5a961,
    click inverse at `AnnoFrame` 0x22a26, draw quad at 0x5c9dd).
  - **No hardcoded limits.** The rule's numbers (the 1024 × 768 reference, the 0.1 step, the 1200-line rule, the
    slider range) live in a `UiScaleRule` struct, with the original values as defaults.
  - **The default is the original's 1:1 UI** (the owner's decision, "1.0, HE rule optional"). It is the identity at
    every size of 1024 × 768 or more. HE's rule is opt-in behind `--ui-scale`, which is refused without `--play`.
  - **A click on UI chrome never reaches the map.** The test `viewer_wiring_ui_chrome_press` shows that a chrome drag
    doesn't pan and a map drag does.
  - **The construction menu stays opt-in** (`--construction-menu`, default off), so default sessions are unchanged.
- **The evidence** rests on the UI lane's independent reviewer:
  - its own numpy model of the transform matches on 17 sizes;
  - the GOG per-width status-bar picks were re-disassembled;
  - the bit-identity at 1024 × 768 is unchanged;
  - R1's per-axis stretch was re-disassembled by the UI lane.

  The orchestrator did not repeat the pixel measurements.

## Not verified by the orchestrator

- **The focused gate** (48/48 passed, by the UI lane's report) and the WSL warning check. The landing's game-file run
  reruns every test on the merged commit.
- **An automated GPU check at sizes other than 1024 × 768:** NOT_RUN, because it needs a desktop window. The UI lane
  checked it by hand at 1280 × 1024, 900 × 600 and 1920 × 1080.
- Linux and macOS game-file runs.

## Notes (not blocking)

- **Performance, a follow-up for the UI lane (UI-PERF-1).** With the construction menu on, the window thread spends
  about 5–6 ms per frame composing the UI at 2542 × 1351. That is the step "construction ui": median 5.7 ms on the
  CPU path, 4.6–6.5 ms on the GPU path. With `--ui-scale` a re-stretch reaches 15.7–27.8 ms, and `UiLayerScaler`
  compares about 4.7 MB per frame. The owner's display runs at 500 Hz, so every such frame drops refreshes.
  - CLAUDE.md puts per-pixel work on the GPU, and the window thread must not wait.
  - So the HUD should be composed only when it changes, uploaded once, and scaled on the GPU.
  - Target: under 1 ms per frame at 2560 × 1369.
  - It is opt-in today and was inactive at this size before this packet, so it blocks nothing now. It must be fixed
    before the UI becomes a default.
- **The owner's choices recorded on the branch:** OQ-614/615 (HE's own graphics when HE is installed) and OQ-616 (a
  window below 1024 × 768). They wait for the owner, as the branch says.

## For the landing

**The candidate:** `main` `f54e36f0`, then `task/UI-WINDOW-1` `9925860c`, then the board. Its game-file run is the
canonical call with:
- `ANNO_UI_ORACLE_SHOTS` set to a copy of `E:\anno1503-worktrees\anno1503-UI-1-F2\cache\orig\ui-1\shots`;
- `ANNO_UI_HE_CAPTURES` set to `E:\anno1503-worktrees\he-ui-captures`.

With those set, the GOG and HE comparisons actually run instead of skipping.

## Game-file run and the owner's playtest (2026-11-14)

**The first candidate:** `orch/land-ui-window-1` at `e0aeac86`: `main` `f54e36f0`, then `9925860c`, then the board up to
`cf0f704a`. It was built locally and not pushed.

**The run:** the canonical call, with the GOG oracle shots and the HE captures set, 13:26–13:37.
- `COUNTS: selected=2606 executed=2567 failed=12 empty=0 skipped=27 (required-asset=0 platform=3 opt-in=21 gpu=0
  other=3) incomplete=0 sanitizer_lines=0`, `VERDICT: FAIL failed tests`
- **All 12 failures are memory refusals** (`reason=low-memory stop`): the Windows executor's SHORE-2 tests ran at the
  same time. None of the 12 ran.
- Log sha256 `60b4eeff9deb8f8e…`.

**The owner's playtest.** The owner tested the UI lane's build first ("Test it first", relayed by the UI lane; the
orchestrator held `land.py`). The playtest "showed a lot of issues and problems, we are fixing them".
- The reruns of the 12 refused tests were stopped: they tested a candidate the fix round replaces.
- **The landing waits** for the UI lane's new head. Then the candidate is rebuilt from it, a fresh game-file run is
  made, and the owner's OK is given directly.

## Fix rounds R2 and R3, the board's check (2026-11-14)

**Verdict: ACCEPTED for landing**, subject to the game-file run below and the owner's OK given directly.

**Reviewed head:** `task/UI-WINDOW-1` `0143f045` (code head `7cddd25f`). It contains `main` `f54e36f0`. CI run
43604656130 on `7cddd25f` is green in all 12 jobs, `gate` included; `0143f045` adds only the UI lane's review note.

**What R2 and R3 change** (`9925860c..0143f045`, 21 files):
- the four HUD tabs become switches, each showing its panel's static chrome (`data/ui/hud.txt`, `ui_mock`, `ui_layout`);
- the status-bar counters show outside the construction menu, for the local player's city on the island under the
  view centre;
- R3 fixed the UI lane reviewer's seven findings (two MEDIUM: sub-tab clicks reaching the map, one compare bound per
  panel).

**What the orchestrator checked:**
- **Scope.** No file under `src/sim`, `src/render`, `crates` or `game/`; no binary file against `main`.
- **The snapshot** (`play.hpp` `CityCounters`, `play.cpp` `publish`). It is read on the sim thread with read-only
  accessors only (`World::islands()`, the island's tile rectangle, `City::owner`/`number`/`stock`,
  `World::city_population`), only with a developer city. No state, stream, hash or draw changes. The clock is read
  only with `ANNO_PUBLISH_STATS` set.
- **Publication.** The `hud.txt` additions hold only element names, positions, sizes, ACCESS names, texture ids,
  anchors, panel names, method tags and shot names. No game text, no pixels.
- **Open-question numbers.** OQ-621 and OQ-622 are free on `main`, `orch/board-next` and every other
  `origin/task/*` branch.
- **The board's three notes** (sent to the UI lane before R2):
  - the snapshot build median went from 41–47 to 47–49 µs; turn p95 shows the same 10 ms spikes before and after;
  - open sea and foreign islands are written up as OQ-622, with the owner's first-hand rule and the rebuild's
    choice (counters hidden, as before founding);
  - the "construction ui" median dropped: CPU 3.72 → 0.59 ms and GPU 4.10 → 0.93 ms at 2542 × 1351, through the
    texture cache (`f9a49436`). That meets UI-PERF-1's target of under 1 ms for the construction menu without
    `--ui-scale`. The re-stretch with `--ui-scale` was not re-measured, so UI-PERF-1 stays open for that case.

**Not verified by the orchestrator:** the UI lane's measurements (read from its report, not re-run), its focused
runs (9 of 9 and 98 of 98), the panel compare's pixel figures, Linux and macOS game-file runs.

### The game-file run of the R3 candidate

**The candidate:** `orch/land-ui-window-1` at `f4efaa81`: `main` `f54e36f0`, then `0143f045`, then the board up to
`9abd22f7`. It was built locally and not pushed.

**The run:** the canonical call with the GOG oracle shots and the HE captures set, 15:56–16:07.
- `COUNTS: selected=2609 executed=2578 failed=4 empty=0 skipped=27 (required-asset=0 platform=3 opt-in=21 gpu=0
  other=3) incomplete=0 sanitizer_lines=0`, `VERDICT: FAIL failed tests`
- Log sha256 `d282dd71bcac4df6…`.
- A first start stopped at once on a run lock left by the orchestrator's own stopped retry run of the first
  candidate. That lock was removed, and no test had run.

**The four failures** are all memory refusals (`reason=low-memory stop`, free memory down to about 2 GB). None of
them ran, and all are render corpus tests this package doesn't touch:
- `render_chunk: the Cpu builder keeps the planes, GpuList keeps the list`;
- `river flow corpus: every island with rivers`;
- `water field: empty cells hold on every island`;
- `legacy river shading reproduces the previous enhanced digests` (heavy).

**Each rerun alone** with `--jobs 1`, same build, 16:08–16:15: `COUNTS: selected=1 executed=1 failed=0`,
`VERDICT: PASS` for all four. Log sha256s: `aa804338c62d29d8…`, `ec7e8bc8be4f66c8…`, `9eac2814e657fac2…`,
`6288ef63e035947d…`.

**Result:** all 2,582 selected tests that are not skipped executed and passed on the candidate.

