# SHORE-2, the orchestrator's review (2026-11-14)

**Verdict: FIX ROUND 1** (about 2 hours). The code change is sound and small. The report skipped measurements on
premises that don't hold, the owner's own view isn't shown, and the spec wasn't updated.

**Reviewer:** the Claude Code orchestrator (Opus 5.5).

**Reviewed head:** `task/SHORE-2` `146409c5`. It contains `main` `f54e36f0`. Hosted CI run 43607764171 on
`146409c5` is green in all 12 jobs, `gate` included.

## What holds

- **Scope:** `src/render/src/sea.cpp` (10 lines), the re-pinned digests in `render_corpus_test.cpp`, `annowelt.md`,
  OQ-026 and the playtest card. No binary file; nothing outside `src/render` and `docs`.
- **The trace, time-boxed.** The writer of the chunk sea bit is `0x1009ebf0`. For an island corner it asks a per-GFXID
  property (`+0x13c`), not the corner's height. Only a corner outside every island tests a height (`0x100b5dc0`).
  OQ-026 is narrowed, not closed. That is an honest time-box result.
- **The rule as built** is the packet's fallback, labelled as one: a chunk holds sea if any of its tiles has a corner
  below `Scene::shore_height`. It no longer depends on the fixed 19, so it can't cut sea out of terrain below the
  plane. `kLandAverage` stays for its other users, which is right.
- **GPU = CPU by construction.** `IslandWater::build` has one caller, and its `sea_chunk` feeds both chunk paths.
  Accepted as an argument; F6 asks for it in the spec.
- **The gates:** MSVC render and app with every memory refusal rerun alone, GPU 117/117 on both adapters before and
  after the merge, WSL `-O1` ASan before the merge, hosted CI after it.

## Findings

- **F1: the zoom sweep was not run, on a wrong premise.**
  - The report says this lineage's `bench_frame` has no `--zoom-sweep`. It does: `main` has had it since
    ZOOM-MEASURE-1 (`src/tools/bench_frame/main.cpp`, the parser at about line 2002, with `--zoom-sweep-mode` and
    `--zoom-sweep-warm`).
  - `--help` may not list it, and the flags need the GPU build (`ANNO_BENCH_GPU`). Check which binary you ran.
  - **Do:** run one `lan5` legs profile, before (`eff68d5e` merged with `main`, or `main` itself) and after, and
    report the readiness counters. Readiness must not get worse (packet item 6).
- **F2: the coverage table was not run, on a wrong premise.**
  - The report says it needs a live original capture. It doesn't. The original's side was recorded on 2026-11-04 (the
    W4-DEPTH-1 review §3, `annodisplay.md` §9.3). Packet item 3 asks for **our** side, recomputed at the same world
    points with the new rule.
  - **Do:** the table, before and after: height bands, the original's recorded share, ours.
  - If the recorded data lacks something the table needs, say exactly what. (The Wine runner is pre-authorized, if a
    capture is really needed.)
- **F3: the river-mouth count was not run, on the same premise.**
  - It counts our own renderer's river cells or river-water pixels that the sea covers at 26.5 against 16.5. No
    original is needed.
  - **Do:** the count over the river corpus (the islands with rivers that `river flow corpus` uses), with both the old
    rule and the new.
- **F4: basins aren't answered** (packet item 2).
  - Your own trace makes this sharper. For island corners, the original's bit comes from a per-GFXID property, not
    from height. So the original may draw **no** sea over low sand or an inland basin whose ground graphics aren't
    water or coast. The new rule draws sea there.
  - **Do:**
    1. Count the tiles that get the sea bit only through the new rule, inside islands and away from the island's
       sea border.
    2. Show one such place, if there is one.
    3. Say what the original draws there: the Wine runner, or a recorded capture.

    If there's no evidence, record it as an OQ: "the new rule may over-draw sea in inland basins".
- **F5: the pictures don't show the owner's case.**
  - The packet asked for the owner's Kontor spot: candidate A, seat 0, `(255,138)`, as in DEPTH-INTEGRATE-1. The
    report shows `S01_02_01.sze (421,605)`, with 2 pixels changed. The whole wide-beach map changed 930 pixels.
  - **Do:**
    1. Before and after at `(255,138)`, the same camera as DEPTH-INTEGRATE-1's pictures.
    2. Measure the stepping itself: the number of pixels whose terrain lies below the plane but whose chunk had no sea
       bit, before and after. After should be 0 by construction. Before shows how much the fix changes.
    3. If the visible stair-steps at `(255,138)` are still there after the change, say so plainly. The steps then have
       another cause, and the playtest card must not say they are gone.
- **F6: the spec is stale.** `docs/render/iso.md` still describes the old rule:
  - the sea "Where" row: "corner average ≤ 19";
  - the Waterline row: "On wide beaches it shows the sea regions' 8×8-tile blocks … an open point".

  **Do:** update both to the rule as built. Note that `--shore 16.5` no longer gives the exact former picture, since
  its digests moved too (`expected_16_5[11]`). Also add GPU = CPU by construction. Then correct the playtest card to
  match F5's result.

## Rules for the fix round

- Same branch, about 2 hours. Gates: render and app lanes only if code changes; F1–F5 are measurements. Hosted CI
  green on the new head.
- One REPORTS section, "fix round 1", about 40 lines, READY FOR REVIEW.
