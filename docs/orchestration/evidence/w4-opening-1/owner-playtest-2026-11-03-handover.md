# W4-OPENING-1: owner playtest rounds of 2026-11-03, state and hand-over (round 7)

Written by the Windows executor at the owner's request after round 7 ("don't start executing, document those results"):
nothing in this note is implemented. It is for the executor who assesses the next step. The playtest history of rounds 1-6 is
in `docs/app/ship-opening.md` ("Playtest rounds"); this note adds round 7 and the state of the branch.

## 1. Where the branch stands

- Branch `task/W4-opening-1`, worktree `E:\anno1503-worktrees\anno1503-W4-opening-1`, head `cfede292` (docs), last code commit
  `28f548c8`. All commits since the published candidate `55e4ee61` are local: nothing pushed, nothing merged into `main`.
- Round 6 commits (owner: "yes, fix them"): `014c3455` scenery under a building is not drawn; `a9127847` a chunk outside the
  scene is open sea without building it; `fad99e79` far zoom levels down to level 4 kept built (`kOpeningResidentLevelChunks`
  = 128); `86d9b166` a zoom in carries the waves smooth and without the far surf; `f21b2cb0` report line counts chunks
  dissolving in; `6ea1901e` docs; after the gate and the review: `0e72fdcf` (GpuList derived chunk outside the scene uploads
  as open sea; `FrameRenderer::build_outside_chunks` for the island-less freshness tests), `28f548c8` (scenery drop marks the
  island's tiles once: 63 s -> 10 ms worst island), `cfede292` docs.
- Gates at `28f548c8`, clean tree: native app lane 252 executed / 0 failed (2 opt-in skips), GPU lane 111/0, render lane
  184/0 (`cache/gates/native-n17{a,g,r}.txt`); WSL GCC 13 and Clang 18 `-O3 -Werror`, viewer with and without GPU: clean
  (`cache/gates/unix-warnings-n18.log`). NOT_RUN: the window tests (the owner was using the PC), the WSL sanitizer lane.
- Reviews: one independent review of `b49438b9..6ea1901e` (1 medium: the scenery drop's cost, fixed in `28f548c8`; low notes
  documented), a re-review of `0e72fdcf` and `28f548c8` (no correctness bug; low notes in `docs/app/ship-opening.md`).
- The projection fix is **not** on this branch: `task/iso-handedness-W4` at `d73c58b9` (session "Fix the mirrored iso
  projection"), handed over for review and merge. It merged W4's `b49438b9`; six files overlap with W4's later commits
  (`docs/app/ship-opening.md`, `docs/open-questions.md`, `docs/render/iso.md`, `src/app/tests/opening_test.cpp`,
  `src/render/src/gpu.cpp`, `src/render/tests/gpu_test.cpp`); none of W4's new lines contains tile/screen projection
  arithmetic (checked by both sessions). Two product failures stay OPEN on that branch: the river motion check Q5 on one
  island (48.5 % against 55 %) and the lan5 dev-tool pick (1 of 8,486 pixels: the 1-unit march steps over a ridge crest).
  Merging needs the owner's OK; the owner's round 7 build did not contain it.

## 2. Round 7: what the owner saw

The owner's words: the Kontor's "building rotation is not correct", "scrolling artifacts are better but not gone yet", and,
with a History Edition screenshot for comparison, "Kontor rotation, dock position (slightly in the water, maybe we have to
adjust the shoreline)". Screenshots (not in git): `cache/o1/r7/owner_round7_ours.png`, `owner_round7_history_edition.png`.

### A. The Kontor's turn: the picture, not the sim

- The owner's Kontor shows the image the original draws for **rotation 0** (deck and piles to the lower right, the earth wall
  with plants to the upper left; crop `cache/o1/r7/ours_kontor.png`), with the water to its left. The History Edition Kontor
  (`orig_kontor.png`) is the same image with the water to the lower right, under the piles.
- Each rotation's image, rendered on the original's own coasts (`S01_02_01.sze`, islands 50, 9, 53, 56 hold Kontors of the
  stored rotations 0, 1, 2, 3; `cache/o1/r7/render4.sh`), with the W4 build and with the projection-fix build (the fix
  worktree's own build of 12:23, which contains `6bb774ca`/`28a5bb44`): `k_sheet.png`, `k_blocks.png` (top W4, bottom fix).

  | rotation | water side by the coast rule | pier side of the image | projection fix (+x lower left, +y lower right) | W4 (+x lower right, +y lower left) |
  |---|---|---|---|---|
  | 0 | +y | lower right | faces the water | faces the land |
  | 1 | +x | lower left | faces the water | faces the land |
  | 2 | -y | upper left | faces the water | faces the land |
  | 3 | -x | upper right | faces the water | faces the land |

  So the sim's rotation is right (rotation 0 for the owner's coast; every original Kontor passes the coast rule at its stored
  rotation, test `the original's own warehouses pass the coast rule as stored`), and every Kontor is drawn mirrored in the W4
  build. The projection fix turns all four the right way (`k_r0_pair.png`: left W4, right fix). Next step: the merge decision.

### B. "Slightly in the water": our waterline is one height step too low for the Kontor's water edge

- **The original (pixel by pixel, `orig_dock_x4.png`, 4x):** along the Kontor's lower-left edge the first three pile positions
  stand on gravel and the waterline crosses between the third and the fourth; every pile of the lower-right edge (the water
  edge of a rotation-0 Kontor in the original's view) stands in the water, its foot submerged; the ship lies alongside.
- **Ours, both builds:** the piles of the water edge stand on dry sand and the water starts about one tile in front of them
  (`k_r0_pair.png`, `k_sheet.png`, `ours_kontor.png`).
- **The terrain under the original's Kontors** (`heights.txt`, `edges.txt`; corners = the island's `HEIGHT` bytes): the coast
  rule (AnnoWelt.dll `0x1007c790`, our `site::coast_height_rule`) wants the land edge exactly `0x30` and at least half of the
  water edge below `0x20` (sea level 32). Over the 64 Kontors of the shipped scenarios the 320 water-edge corners are
  `00`:15, `08`:20, `0a`:1, `0c`:19, **`10`:125 (39 %)**, `14`:8, **`18`:90 (28 %)**, `20`:27, `26`:2, `28`:5, `30`:8.
- **Our waterline test:** terrain is water where it lies *strictly below* `kWaterline` = 16 (`src/render/src/sea.cpp:647`,
  `g.under < kWaterline`; the enhanced water's depth is `kWaterline - smooth_height`, `Scene::fluid_depth`), so a corner at
  exactly 16 is dry. The evidence that set 16 (`docs/render/iso.md` "Water", OQ-026 update 2026-10-17) says the painted ground
  textures are water "at every height <= 16" and sand from 17 on: the shore lies at 16.5, so 16 is water in the original's
  art. Under our rule only 55 of the 320 water-edge corners (17 %) are wet, under "<= 16" 180 (56 %). On the shipped island
  templates 7.1 % of all corners below sea level are exactly 16 (flat shelves along the coasts), drawn as sand by us.
- **H1 (strong, small):** treat height 16 as water (threshold 16.5 or `<=`), in the CPU field, the GPU field and the wave
  simulation's depth alike. Expected: the water reaches the water edge of a Kontor whose edge is at 16, and the 16-high shelves
  along every coast turn to water. It is a visible change of every coastline: show the owner before/after images (the four
  Kontors above and an ordinary coast) before it becomes the default (owner rule: a change of the look is the owner's pick).
- **H2 (open):** does the original reshape the ground under a coast building when it is placed? The originals' profiles are
  not one ramp (`edges.txt`: e.g. `30 30 30 30 30` x7, `30 30 30 20 18` x6, `30 30 28 20 10` x5), so the files do not show a
  standard shape; the RE notes document only the object-id writes of the commit (`0x100823ed`) and the height analysis
  (`0x10008f90`), no `HEIGHT` write. Our sim does not reshape; a founded Kontor whose water edge is at `0x18` (as 28 % of the
  originals') stays dry under H1 too. To settle: trace `City::Place 0x100279a0` -> island `+0x78` -> `0x10082040` for writes
  to the height grid, or capture an original founding on a known coast in the Wine runner and compare the `HEIGHT` bytes
  before and after.
- Not the cause: the Kontor's height. A founded Kontor carries the coast rule's result byte `0x30` (`founding.cpp:373`) and
  the picture draws it at that height (`play.cpp:357`), as the original draws an object at its own height byte
  (`0x1009f870`).

### C. Zoom artifacts: better, not gone, no log this round

- No console log came with round 7; ask the owner for one (zoom out to the farthest level and back a few times). The report
  line now counts frames with a visible chunk not built yet *and* frames that dissolved chunks in over a stand-in.
- What round 6 measured: the CPU chunk cache on the opening map, replayed headlessly (`cache/o1/zoom_diag.patch`, test
  `diagnostic: the opening map's chunk cache ...`, `ANNO_ZOOM_DIAG=1`): after the fixes no frame drew a visible chunk not built
  yet, out or back in. What it did not measure: the GPU side on the opening map. Remaining candidates there, each by design so
  far: uploads are spread (`upload_budget_ms` 1 ms per frame in the window); the compositor keeps a bounded set of chunk planes
  (`prefetch_chunks` 96, the idle plane budget 256 MiB), so a zoom back in can need uploads again; every chunk that arrives on
  the GPU dissolves in over its stand-in (the nearest level, magnified) for 150 ms (`chunk_fade_ms`); while a new wave window
  is prepared (10-40 ms on the opening map) the old, coarser one draws the finer view. The GPU picture diagnostic
  (`cache/o1/zoom_diag2_with_b1.patch`, `ANNO_ZOOM_DIAG_DIR`) replays a zoom on one original island, not on the opening map.

### D. Seen in passing

- The founded Kontor's deck is free of trees in the round 7 screenshot (`014c3455`).
- The white surf over the offshore shoals is still there (our wave simulation; the original shows only lighter water). A look
  for the owner to decide; not raised this round.
- Low notes from the reviews, not acted on (`docs/app/ship-opening.md`, fifth round): `sync_scene` is set and reset by hand
  around the GPU chunk sync; the GPU path ignores `build_outside_chunks`; a malformed scenery item off its island is kept; the
  wave carry's B-spline lifts shallow cells near the shore a little and samples the staggered velocities at cell centres.

## 3. Suggested order

1. The owner's merge decision on `task/iso-handedness-W4` (fixes A; two open failures there).
2. H1 as an experiment with before/after images for the owner; then the default.
3. H2: the RE trace or the Wine capture.
4. C with the owner's next log; a GPU-side replay on the opening map if the log points at uploads or dissolves.

## 4. After the orchestrator's review (2026-11-03, evening)

The review `docs/orchestration/reviews/2026-11-03-w4-opening1-playtest.md` (on `main`, `4c21b5e9`) and the owner's go-ahead
("fix P1 and P2 with tests, handle the low items, run the NOT_RUN gates when the PC is free, ask the three decisions, do the
Wine check for H2, wait for the zoom log").

- **Fixes**, each with a test that fails with the fix reverted (mutation checks): P1 `cd459bba`, P2 `d5b8b30e`, the low items
  `231640c0` and `201a3990`, docs `5afc14a7`; the re-review's test points and two Unix warnings `ed821f42`. Details:
  `docs/app/ship-opening.md`, "Review follow-ups".
- **Reviews:** an independent read-only re-review of `c1bf35cc..5afc14a7`: no P1/P2; its P3 test points acted on in
  `ed821f42`, the rest recorded there.
- **Gates at `5afc14a7`:** native app 256 executed / 0 failed (2 opt-in skips), GPU 115/0, render 185/0
  (`cache/gates/native-n19{a,g,r}.txt`); WSL warnings failed on two test lines (`unix-warnings-n20.log`), fixed in
  `ed821f42`. The gate of the final head follows in REPORTS.
- **H2 settled (D, dynamic): the original does not reshape the ground when it places a building.** Wine runner, a new Citizen
  game (`cache/orig/h2-w4-opening1/d91-20261103-193852/` in the main checkout, not in git):
  - the runtime height grid (island `+0x14`, `(H + 2) x (W + 1)` bytes, `docs/re/ship-founding.md` 12.3) of all 20 islands
    equals the quicksave's `HEIGHT` records byte for byte, its far column and two far rows 0 (the section's reading, now
    seen at run time; a first run gave the same on another generated map);
  - the playbook opening placed 17 objects through `City::Place` on the player's island (4 foresters, the market hall,
    2 hunting lodges, the tannery, 2 weaving huts, 4 sheep farms, road tiles), each verified by the stores: the runtime grids
    of all 20 islands are unchanged byte for byte, and so are the two quicksaves' `HEIGHT` records;
  - not run: a founding by a ship (no macro for it yet). The founding reaches the same `City::Place` and tile writes
    (12.1) as these placements, and the static search of 12.3 found no height write on its path; that a Kontor founding
    alone reshapes is therefore not supported by anything found.
  So the water edge of a founded Kontor keeps the coast's heights: at `0x18` it stays above a waterline at 16.5.
- **H1 evidence (GOG reference, as the review asked):** the game's own start Kontor of that run (rotation 0, water edge
  `10 10 10 08 08`, land edge `30` x 5) is drawn by the original with every pile of its water edge standing in the water and
  the water up to the deck (`cache/o1/r7/gog_start_kontor_x4.png`). Corners at exactly 16 are water in the original's
  picture; ours draws them as sand.
- **H1 before/after** (`cache/o1/h1/`, not committed): the projection fix `d73c58b9` built as is and with the shore at 16.5
  (`h1exp.py` in the session scratchpad: `kShore` in the field, the wave depth, the object test and the shore distance;
  throw-away worktree `E:\anno1503-worktrees\anno1503-W4-h1exp`). Our viewer opens the original's quicksave, so the same
  island renders on both sides:
  - `sheet_start_g1.png`: the original's start view against ours now and with H1, at the original's zoom;
  - `sheet_kontors.png`: the four `S01_02_01.sze` Kontors (4,286 / 495 / 1,746 / 856 pixels change; rotation 0 gains water
    under its piles);
  - `sheet_coast.png`: the start island's coast at group 1 (481 pixels change: the 16-high shelves).
  Finding: H1 moves the waterline the way the original draws it but closes only part of the gap at the start Kontor: in the
  original the water touches the piles along the whole water edge, in ours (with or without H1) sand and gravel remain under
  part of it. The rest is not the waterline's height; candidates (not measured): the screen height at which our water is
  drawn against the terrain near the shore (the original's painted water lies at the terrain's height, its sea sprites at
  32; ours draws the water area at 32), and the ground texture under the piles.
- **Owner decisions** asked now: (a) the projection merge, (b) H1 as the default, (c) the rotate key as the founding start
  rotation. Window tests and the WSL sanitizer lane run when the owner is away from the PC.

### 4.1 The owner's answers and what followed (2026-11-03, late evening)

- **(a) yes:** the orchestrator merged `task/iso-handedness-W4` into the task branch (`a150c11d`). Its two open failures
  stay open (river Q5 on `D_256x256_0045_5B`, 48.5 % against 55 %; the lan5 dev-tool pick, 1 of 8,486 pixels).
- **(b) yes, H1 is the default** (`53101c96`; `--shore`: `38af87b2`): terrain below `kShoreHeight` = 16.5 is water (the original water's test,
  the enhanced water's zero, the shore distance, the empty-cell bound, the wave depth), per scene
  (`render::Scene::shore_height`, the viewer's `--shore 16` for the earlier look). The water objects' split under and over
  the sea stays at 16 (`kWaterline`). The recorded corpus tables (the scene digests, the legacy river digests, the frames)
  stay at the shore of 16, their recording; the frames test adds a third pass at the default shore (10 of 18 frames move:
  Single_01's group 2 enhanced view and every Single_24 view), recorded on this PC. Nine mutants of the shore setting
  (five in `sea.cpp`, one in `iso.cpp`, three in `scene.cpp`) are killed.
- **(c) restore it:** `ee6a12e0`. `T` or `.` turns the founding cursor on, `,` back; the command carries the turn and the
  trials start there. That the original's `.`/`,` reach its Kontor cursor while founding is supported (the review's
  trace), not yet checked in the Wine runner. Five mutants of the key handler and the preview are killed.
- **The owner on the H1 pictures:** the original's Kontor is embedded in the ground; ours floats, its road floats, grass is
  drawn over the Kontor, its piles are not in the water. Cause (D, `docs/re/annodisplay.md`): the original depth-tests
  every sprite pixel against the terrain triangles and the water (u16 depth buffer; the Kontor's 12 frames are
  `BSH_RGBAZ` with per-pixel offsets −70 … +54, roads `BSH_RGBAH` decals tested but never written). This renderer draws
  sprites whole on top of the terrain and depth-tests only the sea and the rivers. H1 cannot close that; it is a renderer
  task of its own (OQ-026 c), not started.
- **Package review** (independent, read-only, of `a150c11d..6793191d`): no P1; two P2, seven P3. Dispositions:
  - P2 the water-object split stays at 16 while the experiment moved it to 16.5: measured, the shipped default renders the
    six H1 views (`cache/o1/h1/render.sh`) pixel for pixel as the experiment's H1 build, and `--shore 16` as its base
    (`cache/o1/h1now/`, 0 differing pixels each). Kept at 16 (it also decides which river pieces form the river surface),
    the reason in `iso.hpp` and `docs/render/iso.md`; which side an object at exactly 16 belongs to stays open (OQ-026 b).
  - P2 the empty-cell proof in `docs/render/iso.md` was written for a shore of 16: restated for the island's shore S
    (31.6 at the default; the corners are whole numbers, so the marked tiles are the same), and the perf note.
  - P3 fixed: the kept turn reworded as our choice (OQ-520 update); the status line shows `rot R (turn T)`, tested;
    `--shore 256` and `nan` refused, tested; the frames table's provenance (this renderer's, MSVC); the markdown here.
  - P3 not changed: `tools/map2_shape.py` / `map2_measure.py` still describe land as HEIGHT >= 16 "as the renderer":
    the Mac MAP lane's tools, not edited here (the fairness measure's definition is that lane's, for the orchestrator);
    `Scene::shore_height` is a plain field "set before the first render" (an island's water keeps the shore it was
    built with); the wave-depth test covers the open sea only; the corpus at the default shore is the frames test's
    third pass (the scene and legacy-river tables run at 16), recorded on MSVC, no Linux run with the game files.
- **Gates of the final head:** REPORTS.
