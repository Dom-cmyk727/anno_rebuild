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
