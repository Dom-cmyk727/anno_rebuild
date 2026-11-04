# W4-OPENING-1 — owner playtest rounds 1–7: direct review

**Not accepted yet. The valid work is retained.**

The playtest corrections after the published candidate are reviewed below. Rounds 1–5 had no recorded independent
review; they now have one (§3). Round 6 and its two follow-up fixes had already been reviewed by the executor's
reviewer. Before acceptance, these remain:
- the findings in §3;
- the NOT_RUN window and sanitizer gates;
- three owner decisions (§5).

**Not part of this review:**
- the final acceptance of the published candidate `55e4ee61` itself (I1–I6), which stays with the board's orchestrator;
- the projection fix branch.

Written by the Claude orchestrator session on the owner's instruction of 2026-11-03 ("write to the board the Codex
orchestrator runs"). It does not change any other lane.

## 1. Reviewed identities

| Input | Exact pin |
|---|---|
| Published candidate (I1–I6, on GitHub) | `55e4ee61` |
| Branch head (local only, not pushed) | `task/W4-opening-1` = `c1bf35ccf983c71c1507e498763b22b0352183ef` |
| Last code commit; gate source | `28f548c84dbf960797fb778ea3b3f3decae4d954` |
| Gate workflow (tools) | `8fbcf3f2c90dd68cf773798431637f7c3dcf9827` |
| Projection fix, not reviewed here | `task/iso-handedness-W4` = `d73c58b9` |

There are 21 commits in `55e4ee61..c1bf35cc` (+3,206 / −85 lines in 47 files). The executor's worktree
`E:\anno1503-worktrees\anno1503-W4-opening-1` was clean at `c1bf35cc` when read. Entry points:
- REPORTS on the branch, section "Owner playtest rounds 2026-11-03";
- `docs/orchestration/evidence/w4-opening-1/owner-playtest-2026-11-03-handover.md`;
- `docs/app/ship-opening.md`, "Playtest rounds".

## 2. Verified directly

- **Gates.** The logs `cache/gates/native-n17{a,g,r}.txt` exist, name source `28f548c8` with `dirty=0` and workflow
  `8fbcf3f2`, and report:
  - app: selected 254, executed 252, failed 0, skipped 2 (opt-in);
  - GPU: 111 executed, 0 failed, 0 skipped;
  - render: 184 executed, 0 failed, 0 skipped;
  - sanitizer lines 0, verdict PASS.

  `unix-warnings-n18.log` exists. NOT_RUN, as reported: the window tests and the WSL sanitizer lane.
- **Round 7 A (the Kontor drawn turned the wrong way).** The executor's table matches the original's code read
  independently today:

  | Rotation | Water edge (coast rule) | Pier side of the sprite, original default view |
  |---|---|---|
  | 0 | `+y` | lower right |
  | 1 | `+x` | lower left |
  | 2 | `−y` | upper left |
  | 3 | `−x` | upper right |

  The sources: the coast helper AnnoGame `0x10008ea0` and the decoded set-0 frames of entry 1302. So the sim's rotation
  is right, and the W4 build draws every Kontor mirrored. The cause is the projection, not the founding code.
- **Shipped Kontors pass the coast rule** at their stored rotation and origin, independently of the branch test. Over
  the shipped scenarios, all 115 distinct player Kontor placements (1302/1304/1306/1415) pass; 107 have exactly one
  passing rotation and 8 have two adjacent ones.
- **Round 7 B, numbers reproduced exactly.** Over the 64 Kontor 1302 items of the shipped scenarios, the 320 water-edge
  corners give the same histogram as the hand-over (`10`: 125, `18`: 90, …):
  - 55 corners are below 16;
  - 180 are at or below 16;
  - 278 are below 32.

  Our code's water test (`sea.cpp`, `g.under < kWaterline`, 16) is stricter than our own documented evidence:
  `docs/render/iso.md` "Waterline" puts the painted shore at 16.5.

## 3. Rounds 1–5 code review (`55e4ee61..b49438b9`)

Reviewer: an independent read-only review agent of the Claude orchestrator session, briefed with the diff and the project
rules only. It was given no conclusions. It used only `git show`/`diff`/`grep`: nothing was built or run, and no
worktree was touched. Scope: the 13 commits, about 2,000 lines in 34 files, each finding re-checked at head `c1bf35cc`.
The orchestrator re-read the code of findings 1 and 2 at `c1bf35cc` and confirms them. Findings 3–10 are the reviewer's,
not independently re-checked.

**Verdict.** The architecture holds:
- no sim code changed in the range;
- the founding probe reads the world on the sim thread without mutating it;
- the window thread only reads published views.

One P1 and one P2 must be fixed before acceptance.

**[P1] Out-of-bounds read on every enhanced-water GPU frame at a derived zoom level.** Introduced by `423b993f`; still at
`c1bf35cc`.
- **Where:** `src/render/src/gpu.cpp:2514`, in `update_fluid`: `raise = scene.lift.scaled(kSeaLevel, 1, group)`.
- **Why it is out of bounds:** `Lift::scaled` reads `kTile[group].w` (`iso.cpp`), and `kTile` has 3 entries.
- **How it became reachable:** `423b993f` removed `group < kGroups` from the `fluid_on` condition (`gpu.cpp:3200`) so that
  waves run on derived levels. It clamped only `cells_group`. Two lines above, the tile size already uses the
  derived-aware `levels->tile_w(group)`.
- **Effect:** the ship-start opening forces enhanced water on the GPU, so zooming to level 3 or farther is undefined
  behaviour. The wave window centre (`py`) moves by garbage; ASan, or a checked STL build, reports it.
- **Fix:** use `levels->lift_scaled(scene.lift, kSeaLevel, 1, group)`, as the frame uniform does at `gpu.cpp:2450`.
  Add a GpuFrame test at a derived group with `wave_simulation` on; none exists today.

**[P2] The far-level prewarm can give up a buildable chunk.** This is a race, still at `c1bf35cc`; how often it happens
was not measured.
- **Where:** `src/render/src/frame.cpp:970` gives the target up when `prewarm_target_ && !pending_`. `pending_` is the
  previous frame's `submit` result.
- **Scenario:**
  1. Frame F−1: the last missing child of the target finishes between `take_done` and `submit`. `submit` skips it as
     finished, and the parent cannot be planned yet, so `pending_ = false`.
  2. Frame F: the child arrives, the target is still uncached, and it goes into `prewarm_skipped_`.
- **Consequences:** that set is never cleared, not even by `invalidate`. The chunk is then built only on demand at the
  next far zoom. That matches "needs a moment" and may feed round 7 C.
- **Why the tests cannot show it:** they call `wait_idle()` between frames, so the gap never opens.
- **Fix:** give up only on a reported build failure of the target or a planned descendant, or require the condition on
  two consecutive frames that received nothing. Add a test that opens the gap.

**[P3, reviewer] Low findings.** These are for the executor to fix or disposition:
1. **Turn clock.** `ship_motion.cpp:80-81` restarts the turn clock on every target change, also in the middle of a turn.
   At high speed, or with targets that change on consecutive publications, the shown facing lags. Reset only when
   `shown == asked`.
2. **Chunk count.** `viewer.cpp` (`levels.chunks`) computes `(x + w − 1) / 512 − x / 512 + 1` with truncating division,
   so it undercounts for negative iso x; `sync_levels` uses `div_euclid`. The literal duplicates `kChunk`. It is likely
   the source of the "91 chunks" behind the first level-4 threshold.
3. **Probe drift.** `probe_founding` copies `Founding::apply`'s check order into the app. Only a CI-skipped real-file
   test guards it, and it silently skips `NotAShip`. Suggested: one const probe in `sim` that `apply` itself calls.
4. **No CPU reference for the wave carry.** The wave carry-over (`resample_state`) runs on the GPU only. Every
   GPU-vs-CPU comparison turns it off.
5. **Wake limit.** `kMaxShipWakes = 16` has a reason (the uniform array) but no path to configure it. Beyond 16 ships,
   wakes silently drop. The shader also hard-codes copies of 0.05, 0.6 and 40.0.
6. **Doc and code disagree.** `docs/app/ship-opening.md` says the wake source is "full from 32 deep"; code and shader use
   40.
7. **Water override.** An explicit `--water original` is overridden to Enhanced on the first opening snapshot on the GPU.
8. **Test gaps:**
   - nothing would fail if `5d25f823`'s no-fade-on-carried-switch were reverted;
   - no test renders at a derived group with waves (finding P1);
   - no test covers `ShipMotion::wakes` or `set_wakes` wiring;
   - `make_found`'s branches are covered only by the window script;
   - the real-file founding-sites test clamps at the island edge, where the viewer returns nullopt, and checks only
     `full > 0`.

**Checked and found correct, per the reviewer:**
- **`probe_founding` against `Founding::apply`:** the same check order, `rotation_trials`, reasons and rotation;
  1,458 clicks are compared against `apply` on a world copy.
- **The preview command:** it equals the clicked command.
- **`kFoundingCursorRotation`:** consistent with the sim. See §4 for its fidelity.
- **Threading:** `probe_` under `probe_mu_`; no dangling snapshot references.
- **ShipMotion:** no overshoot; snap above 6 tiles; `dir16_of` matches `ship_step.cpp`; no NaN; bounded memory.
- **Wake term:** CPU `ship_term` equals the shader's.
- **Resample pass:** the ring, `Old` uniform layout, units, `prev_grid_` lifetime and `drop_window` are correct.
- **Fade and window-size hysteresis:** bounded.
- **Report counters:** diagnostics only.
- **Stale-chunk prewarm:** the rebuild test fails without the change.
- **The docs commits:** correct, except findings 2 and 6.

## 4. Findings of this review

**[P2] Review gap, now closed by §3.** The 13 commits of rounds 1–5 had no recorded independent review; the executor's
review covered `b49438b9..6ea1901e` and the two later fixes only. The evidence contract requires one. §3's findings
need the executor's fixes or dispositions, with focused re-review.

**[P3, fidelity] The founding cursor drops the player's start rotation.** `f3dd5053` fixes every `FoundCmd` at
rotation 0 (`kFoundingCursorRotation`) and disables T. The comment says the original's choice "is not traced". It is
traced:
- The Kontor cursor's footprint helper returns the cursor's rotation byte `+0x46`; AnnoFrame `0x10049340` returns it at
  `0x100493e0`.
- That byte is the first of the four trials in all mouse paths (`0x10048f06`, `0x1004900c`, `0x100490d8`; loop
  `0x10048de7`). MAP `docs/re/ship-founding.md` §8.6 says the same.
- The byte is written by the cursor's property-5 setter `0x10049166`, the rotation property the ordinary cursor's
  rotate keys use. That the keys reach it while founding is S, and needs a one-minute check in the Wine runner.

**Effect:** none on straight coasts. Where two rotations fit (8 of 115 shipped sites, convex corners) the original
lets the player pick with `.`/`,`, while ours always takes the first counting from 0. This contradicts the owner's
round-5 statement "no manual rotation", so it is an owner decision (§5). Recommended: restore the key as the start
rotation; nothing else changes.

**[Decision] H1: height 16 counts as water.** Supported: the code is behind its own evidence (§2). It changes every
coastline, so produce the executor's proposed before/after images. Use the GOG original in the Wine runner (scenario
coasts and the four `S01_02_01.sze` Kontors) as the reference, not the History Edition screenshot. HE footage is not a
pixel reference: the HE rasterises at 32-bit colour, and its "improved coastlines" option alpha-blends the water (the
owner's install has that option off). That the water extent is the same as GOG is I.

**[Open] H2: whether the original reshapes terrain under a founded Kontor.** A quick static scan of AnnoWelt for
height-grid writes was inconclusive: the heuristic missed even the known `HEIGHT` loader. Cheapest decisive check:
1. In the Wine runner, look at one shipped Kontor whose water edge is at `0x18`. That is 28 % of them; `edges.txt` in
   the hand-over lists profiles.
2. If its piles look wet, the original's water near buildings reaches above 16, and H1 alone will not match.
3. If they look dry, H1 suffices. One founding, with `HEIGHT` compared before and after, then settles H2.

**[Open] Projection fix.** `task/iso-handedness-W4` is the cause of round 7 A. It needs its own independent review and
the owner's merge decision, with its two recorded open failures: river motion check Q5 at 48.5 % against 55 % on one
island, and the lan5 dev-tool pick, 1 of 8,486 pixels.

## 5. Next steps for the Windows executor (in order)

1. Fix §3's findings, with focused re-review:
   - **P1 out-of-bounds read:** a one-line fix, plus a derived-group wave test that fails without it under the WSL
     ASan/UBSan lane;
   - **P2 prewarm give-up:** a fix, plus a test that opens the `take_done`/`submit` gap;
   - **the P3 items:** fix or disposition each one.

   Then run the NOT_RUN window tests and the WSL sanitizer lane once the owner is not using the PC.
2. **Owner decisions** (ask, do not assume):
   - (a) merge `task/iso-handedness-W4` after its review;
   - (b) H1 as the new default, after before/after images against GOG;
   - (c) restore the rotate key as the founding start rotation.
3. H2 by the Wine check above. The Wine runner is pre-authorised and must stay silent.
4. Round 7 C (zoom artifacts): wait for the owner's next console log, then do a GPU-side replay on the opening map if it
   points at uploads or dissolves.
5. Publishing `task/W4-opening-1` beyond `55e4ee61` is an ordinary fast-forward push of the task branch and needs the
   owner's OK. Nothing goes to `main`. A local orchestrator reads the branch without it.

No merge into `main` is authorised by this review.

## 6. Update, 2026-11-03 evening: published, projection fix reviewed and merged into the task branch

**Owner instructions:** "Push", then "And merge" (the projection fix into the task branch, not `main`).

**Pushed.** `task/W4-opening-1` went to `c1bf35cc` by fast-forward, then to the merge below. Both remote hashes were
checked.

**The executor's follow-ups since this review.** Committed on the branch, not reviewed here:

| Commit | Content |
|---|---|
| `cd459bba` | P1: the derived lift |
| `d5b8b30e` | P2: give up only on a failed build |
| `231640c0` | a CPU reference for the wave carry; the wake tuning as configuration |
| `201a3990` | the low items |
| `5afc14a7` | docs |
| `ed821f42` | the re-review's test points; the two Unix warnings |
| `9ed05d75` | docs: H2 settled in the Wine runner, H1 against the GOG original |

They need this review's focused re-review: §3's P1, P2 and P3 items, against the executor's own re-review record.

**Projection fix review** (`task/iso-handedness-W4` at `d73c58b9`, its 14 own commits). Reviewer: an independent
read-only review agent of the Claude orchestrator session, briefed with the diff, the project rules and the original's
handedness facts only.

**No blocker.** The handedness change is complete and consistent across every CPU, GPU, picking and app path traced:
- forward projection, sea lattice, wave and river mapping, picking (the exact inverse, floor correct for negative
  tiles), terrain split, ship facing (`frame = dir16`), dev-tool and tool helpers;
- no leftover `(x − y)` form except the intended negative control;
- the hand-derived values match the tests: pick (−210, 336) → (10, 5), `tile_top(1,0,0)` = (−42, 21), and others;
- every re-baselined digest has an independent reason to change;
- the Kontor frame `k` = rotation `r` matches the original (dock lower right / lower left / upper left / upper right).

Findings:
- **[P2] The branch's gate is FAIL.** `cache/gates/accept-04.log` ran at `d73c58b9` on a clean tree: 549 executed,
  6 failed. Four were memory-budget refusals (`low-memory stop`) and pass in the 4-test rerun `accept-05`. Two are
  real:
  - **Q5:** `river_flow_corpus_test.cpp` fails with share `0.4848 >= 0.55` on island `D_256x256_0045_5B`.
    - Reviewer: not a sign or axis defect. The other islands score 79–100 %, overall 82.8 % against 82.9 % before,
      and the stem test passes.
    - The likely cause is changed inputs: the river mask comes from sprite alpha, the sprites now stand in the correct
      handedness, and the island's window count went from 41 to 33. The island was already marginal (63.4 %).
    - Remedy: compare D's mask and flow before and after (`river_qa`). The orchestrator then decides the per-island
      bar; do not lower it silently.
  - **lan5 pick:** `dev_tool_test.cpp` has 1 wrong pixel of 8,486.
    - Likely a gap in the test's interior filter that predates the branch: pixels just beyond a triangle's B–C
      diagonal on a ridge silhouette. The filter skips only the tile edges, while the rasteriser draws a 0.04-px
      tolerance band.
    - Print the failing pixel to confirm. Fix: also skip within 0.06 of the diagonal, or rasterise with an exact
      fill rule.

  The docs (`docs/render/iso.md:23`, `docs/open-questions.md:209`, commit `3c13077e`) imply that Q5 passes with the
  kept −x light, and mention neither failure. Record both as FAIL/open.
- **[P3] Smaller items:**
  - the GCC/Clang `-Werror` check was not re-run after `b7aab0ff` fixed its Clang error, so it is NOT_RUN at the head;
  - `docs/PLAN.md:198` is a stale "needs the same change when it merges";
  - the Linux and Apple digests of the enhanced lines are not measured, which affects only runs with game files;
  - the playtest card has no checks for the fix: Kontor piles in the water, continuous fences, ship bows, the light,
    the reversed tie-break;
  - the projection formula is written out inline in about 12 places despite "every mapping is this one"
    (`iso.hpp:111`), a hazard for a future view rotation (OQ-026);
  - `river_flow_corpus_test.cpp:1116` prints a pointer instead of the island name.
- **Light, as input for the owner's decision.** In 239 of 266 sprite/shadow frame pairs of `Anlagen.dat` (group 2), the
  shadow centroid lies left of and below the sprite's, and right of it in none. The original's baked sun is on the
  screen's right, so the kept −x light (now from the top right) agrees with it.

**Merged.** `a150c11d` = `Merge task/iso-handedness-W4 into task/W4-opening-1` (parents `9ed05d75` and `d73c58b9`):
- made in the executor's worktree while it was clean, with no build or test process running;
- the textual merge had no conflicts, in 40 files;
- the W4 commits after `b49438b9` use the shared `tile_top`/`screen_tile`, with no new inline projection arithmetic;
- the merged tree has **not** been built or tested yet.

Nothing went to `main`.

**Next for the Windows executor**, before the owner's next playtest build:
1. Run the full gates on `a150c11d`:
   - native app, GPU and render lanes;
   - GCC 13 and Clang 18 `-Werror` (WSL), viewer with and without the GPU;
   - the window tests, and the WSL sanitizer lane when the PC is free.
2. Settle Q5 and the lan5 pixel as above, with the failing pixel printed and D's before/after flow. Record both
   honestly in the docs.
3. Fix or disposition the P3 items, and add the projection checks to the playtest card.
4. Then the owner's playtest. Decisions (b) H1 and (c) the start rotation of §5 remain the owner's. Decision (a) is
   done: the owner chose this merge.

## 7. Owner answers (b) and (c): review, 2026-11-04

**Identity.**
- Reviewed: `task/W4-opening-1` from `a150c11d` to `02133ade`, eight commits. The code commits are `ee6a12e0`,
  `53101c96`, `38af87b2` and `9173fc6e`. The docs commits are `6793191d`, `99437681`, `d9704b98` and `02133ade`.
- Published at `02133ade` on the owner's instruction ("Yes", 2026-11-04). Remote verified.
- Reviewer: the Claude orchestrator session. It read the diff, the tests and the gate files under `cache/gates/`. It
  built and ran nothing.
- The owner's answers, given to the executor on 2026-11-03:
  - **(b)** H1 becomes the default.
  - **(c)** the founding turn key is restored.

**Verified.**
- **Shore at 16.5 (`53101c96`, `38af87b2`).**
  - It changes the renderer only. Nothing in `src/sim` or checksummed state changes.
  - It is a setting per scene: `render::kShoreHeight` = 16.5, `Scene::shore_height`, carried into each new scene
    generation.
  - `--shore` takes values from 0 to 255. Negative values, 256 and `nan` are refused, and this is tested.
  - Every consumer that asks "is the terrain water here" now reads the scene's shore:
    - `shore_distance`;
    - `IslandWater::build`, through `smooth_shore`;
    - the empty-cell bound `empty_land_height(S)`, whose proof in `docs/render/iso.md` is restated for any shore S;
    - `tile_ground_with`, for `original_water` and `sd`;
    - `Scene::fluid_depth`;
    - `river_qa`.
  - The WGSL shaders have no waterline constant of their own. They read the field texels that the CPU computes.
  - `kWaterline` = 16 now appears only in the split of water objects under and over the sea (`iso.cpp:379`). The reason
    is in `iso.hpp`. The executor measured 0 differing pixels against the H1 experiment on all seven H1 views.
- **Founding turn (`ee6a12e0`, `9173fc6e`).**
  - The UI keeps the turn in `found_rotation_`, and the `FoundCmd` carries it. The sim's rotation trials from the
    command's rotation are unchanged.
  - `T` and `.` add 1 modulo 4; `,` adds 3.
  - The origin is centred with the footprint at the cursor's turn.
  - The test `controls: on a corner where two turns fit, the founding cursor's turn decides` builds a synthetic convex
    corner. It expects the rotations 0, 1, 0, 0 for the turns 0 to 3. I checked this by hand against the coast rule,
    where rotation r has its water edge at +y, +x, −y, −x.
  - The figure "8 of 115 shipped sites" matches the orchestrator's independent corpus count. That count is in
    `docs/research/building-placement.md` §5 (branch `docs/placement-research`, not merged): 115 Kontor placements, 107
    with one passing rotation, 8 with two.
- **Why the Kontor floats.** `docs/re/annodisplay.md` confirms the executor's cause:
  - the original keeps a u16 depth buffer;
  - `Z` sprite formats carry a signed depth offset for every pixel;
  - a pixel is drawn when its depth is at most the buffer's;
  - the ground triangles test and write depth.
- **Gates.** I read the verdict lines in the executor's worktree. All match REPORTS:

  | Lane | Source head | Result |
  |---|---|---|
  | native app (`native-n25a`) | `99437681` | 257 executed, 1 failed (lan5 pick), 2 opt-in skips |
  | GPU (`native-n25g`) | `99437681` | 115/115 PASS |
  | render (`native-n28r`) | `d9704b98` | 193 executed, 1 failed (Q5, `0.485714 >= 0.55`) |
  | window, RTX 4090 (`gpuwin-n26`) | `d9704b98` | `opening_gpu_window`, `opening_controls` PASS |
  | WSL warnings (`unix-warnings-n27`) | `d9704b98` | GCC 13 / Clang 18, four variants clean |

  The commits after `99437681` change docs only, so the app and GPU lanes cover all of the code.

**Findings.** No P1 and no P2.
- **[P3] "Q5 unchanged by the shore" was not measured.**
  - Q5 gave 0.4848 at `d73c58b9` (the projection branch alone, `accept-04`) and gives 0.485714 at `d9704b98`.
  - `river_flow_corpus_test.cpp` sets no shore, so Q5 runs at the default 16.5.
  - No render gate completed at `a150c11d` without the shore change: `native-n21r` was voided (next item).
  - Either measure Q5 at shore 16 on the head, or record the claim as unmeasured.
  - The docs give the value two ways: 48.5 % in the hand-over note §4.1 and 48.6 % in REPORTS.
- **[Process, orchestrator] My merge voided one of the executor's gate runs.**
  - I committed `a150c11d` in the executor's worktree at 19:55:36. Its render lane `native-n21r` started at 19:55:37,
    between two lanes of its gate script, while no compiler or test process was running.
  - The lane therefore ran the `9ed05d75` binaries against the merged sources, and the executor voided it
    (`cache/gates/native-n21r.txt`).
  - From now on, the orchestrator makes merges onto task branches in a separate checkout, never in an executor's live
    worktree.
- **NOT_RUN at the head:**
  - the WSL ASan/UBSan lane (it last ran at `a150c11d`);
  - a Linux corpus run with the game files at the new shore;
  - the Wine check that the original's `.` and `,` reach the Kontor cursor while founding. Wine runner runs are
    pre-authorised, so no owner approval is needed.

**Items for other lanes, recorded, not edited.** The executor flagged these; I confirmed each on the named branch.
1. `docs/re/ship-founding.md` on `task/MAP-2-original-like` still calls `0x10049340` unread or unknown, in lines 13,
   166 and 310 and in the §3.5 table. Its own correction at lines 420–421 (§8.2) closes this: the function returns the
   rotation byte `+0x46` and writes the centred rectangle. The property-5 setter `0x10049166` appears only in this review's
   §4 and in OQ-520. **For the MAP lane:** align the note at its next docs checkpoint.
2. `tools/map2_shape.py` (line 3) and `tools/map2_measure.py` (line 445, `land_definition`) on
   `task/MAP-2-original-like` define land as `HEIGHT >= 16`, "as the renderer". That is no longer true at the default
   shore. **For the MAP lane:** the fairness measure's land definition is its own decision. Keep 16 with a reason of
   its own, or follow `kShoreHeight`, and say which.
3. BEL characters (0x07) on `main`, where a `\a` escape swallowed a backslash in a Windows path (`src\app\...`,
   `C:\anno1503-original`): `docs/PLAN.md:484` and `docs/orchestration/REPORTS.md:2690`. **For the next docs edit of
   either file:** restore the backslashes.

**New task.** The floating Kontor goes to its own Windows task, `W4-DEPTH-1` in `TASKS.md`. It is queued, not released,
and the owner releases it after the playtest of this head. W4-OPENING-1 does not take it on.

**Next for the Windows executor.**
1. Settle the Q5 shore claim as above.
2. Run the Wine check of `.` and `,`, and record which way each key turns.
3. Run the WSL sanitizer lane on `02133ade` while the PC is free, then the native render gate. Never run the two at the
   same time.
4. The owner's playtest of `02133ade`.

The owner's console log for the zoom artifacts is still pending.
