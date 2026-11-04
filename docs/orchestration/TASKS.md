# Executor task board (read this first, every time)

**Who writes this file:** the orchestrator session (Codex, by owner instruction). **Who reads it:** the executor sessions. The executor never edits this file; it writes to [`REPORTS.md`](REPORTS.md). The orchestrator updates this file after every review, so re-read it at the start of every task and whenever a task ends.

## Current dispatch — 2026-11-04: Windows opening sequence; Mac population closeout and progression

**Read this dispatch before the historical rounds below.** Executors receive new
assignments through this tracked file on GitHub
(`Dom-cmyk727/anno_rebuild`, `origin/main`). This dispatch supersedes the old
round-dw CPU-only stop and GPU prohibition for W4. It does not restart an active
package or accept unreviewed changes. **W4-WIN-VERIFY is accepted** at published
`26107db089f558bc374208a4f414ca70f562e6f0`; runtime/C++ and full focused gate
source `f047165c67bc26c1498c5201c014db83553c6f6f`, final tools/tests
`3c808780c4e5e59d26375151c3ff8215c4c68602`. The
[direct verifier review](reviews/2026-11-02-w4-win-verify.md) closes sanitizer-output
propagation and complete runtime script identity. It retains the completed corpus
and unaffected GPU evidence within their recorded limits. **W4-OPENING-1 below
is released** at the exact Windows and Mac pins; no new full verifier campaign
or merge into main is assigned.
MAP-BUILD-1 and MAP-CARGO-1 are accepted for their local headless scope:
the real ship-start opening sails, founds, unloads and builds for seats 0 and 2.
Accepted Mac integration input is
`0c401c8d084cfd9feb8aec846595291f87f22cce` on `task/MAP-2-original-like`;
code/tests `7e5529f42378a13b3235d9d93023ee8a40c19c50`, clean U5 gate source
`f0c8647fb543e3e512a9fc5470a9d04c0f6ff599`. See the
[direct cargo review and limits](reviews/2026-11-02-map-cargo1.md).
The writer-test isolation finding is closed at `9b6c547f`; six U5 verdicts,
60/60 opening replay checkpoints and changed-unload divergence were independently
checked. The orchestrator published and remote-verified this exact Mac pin;
executors do not choose a newer moving tip or merge it into main.
Windows/MSVC, Linux gates and live-window validation of this Mac code remain
pending and are covered by the Windows integration packet where affected.
**W4-OPENING-1** is the Windows executor's current assignment.
**Its owner playtest rounds 1–7 are reviewed but not yet accepted.** The review covers local `55e4ee61..c1bf35cc` and
was written by the Claude orchestrator session on the owner's instruction of 2026-11-03: read the
[direct playtest review](reviews/2026-11-03-w4-opening1-playtest.md). Retained: the gates at `28f548c8` (app 252/0,
GPU 111/0, render 184/0) and the round-7 diagnosis (the sim's Kontor rotation is right, the drawing is mirrored).
Open:
- the rounds 1–5 findings, which must be fixed before acceptance: **P1**, a `kTile` out-of-bounds read in
  `update_fluid` at derived zoom levels; **P2**, a prewarm give-up race;
- the NOT_RUN window and sanitizer gates;
- three owner decisions: projection merge, waterline H1, founding start rotation. The next Windows steps are its §5; the candidate's own final acceptance is
unchanged.

**Evening update (owner: "Push", "And merge").** The projection fix `task/iso-handedness-W4` (`d73c58b9`) was reviewed:
no blocker, but its gate is FAIL on two tests (river Q5 on one island, a 1-pixel lan5 pick). It is now **merged into the
task branch**, `task/W4-opening-1` = `a150c11d`, published. Nothing went to `main`. The merged tree is not yet built or
tested. The next Windows steps are the review's §6.

**Update 2026-11-04 (owner: "Yes").** The owner answered (b) and (c):
- **(b)** the shore at 16.5 is the default, with `--shore 16` for the earlier look;
- **(c)** the founding turn key is restored: `T` or `.` turns the cursor one way, `,` the other.

The executor implemented both on `task/W4-opening-1`, now `02133ade` and published.

The [review §7](reviews/2026-11-03-w4-opening1-playtest.md) has no P1 or P2. It confirms the gates:
- app 257 executed, 1 failed (lan5);
- GPU 115/115;
- render 193 executed, 1 failed (Q5);
- window tests pass;
- WSL warnings clean.

Open:
- one P3: the Q5 shore claim was not measured;
- NOT_RUN: the sanitizer lane at the head and the Wine key check.

The floating Kontor is **W4-DEPTH-1** below. The owner released it on 2026-11-04, and it is now reviewed (update below).

**Windows sequence, owner 2026-11-04 ("Yes and merge"; chose "Main, as own package").** Three packages, run in this
order:
1. **W4-OPENING-1-CLOSE** (current) finishes the opening candidate for acceptance.
2. **W4-MAIN-1** merges the accepted Windows lane into `main`. It is released, and starts once the orchestrator accepts
   step 1. `main` moves only after the orchestrator's review.
3. **W4-OPENING-2** brings the first wood chain into the live window. It is released, starts from the new `main`, and
   takes the accepted Mac pin `73e7ab1a`.

Each package's own review gates the next, so no further owner approval is needed between them. The owner's playtest
of the opening may happen at any point: fixes from it go to `task/W4-opening-1` before W4-MAIN-1 starts, and to
`task/W4-opening-2` after.

**Update 2026-11-04, evening (W4-DEPTH-1 reviewed).** The owner released W4-DEPTH-1 as its own branch. The executor
delivered it on local, unpublished `task/W4-depth-1`: gates at `63867115`, report `0fd1d219`.
- **Review:** the [depth review](reviews/2026-11-04-w4-depth1.md) finds no P1. Mutation runs, sanitizer lanes, the
  idle-host performance figures and small P3s remain before acceptance; they are **W4-DEPTH-1-CLOSE**, released below.
- **Owner decisions:**
  1. **The water level, SETTLED 2026-11-04.** Asked in the depth session's own wrap-up which default to compare, the
     owner clicked "Try 26.5 default"; asked again once the conflict with this page surfaced, the owner confirmed
     ("we will try 26.5 and see, just note it down", then "Keep 26.5 in package"). `kShoreHeight = 26.5` is now the
     depth package's default (`3f180c62`, on top of `0fd1d219`; `bench_frame --shore h` in `b72595c2`). The
     `--shore 16.5` picture is kept as its own pass, pinned, unchanged. This overrides review §3's recommendation to
     keep 16.5 for now; W4-SHORE-2 stays queued regardless, since 26.5's stepped beach edges remain until it draws
     the sea per pixel.
  2. **The `--depth` switch.** Not separately contested; review §3's recommendation (keep it, default on) stands.
- **W4-DEPTH-1-CLOSE below runs on `task/W4-depth-1` from `b72595c2`** (so at 26.5), not from `0fd1d219`. Its "Owner
  check" step (start Kontor at `--shore 16.5` and `--shore 26.5`, beside the GOG capture) still covers both, for the
  owner's own comparison.
- **The closeout:** its latest WSL ASan app gate at `1d34dd97` fails `opening_session_replay` at stage 1. Find the
  cause before that chain runs again.
- **Merge order (review §5):** if the owner's depth check passes before the closeout's final gate chain, the depth
  merges into `task/W4-opening-1` first. Otherwise it merges into `main` after W4-MAIN-1, as its own merge, before
  W4-OPENING-2.
- **Queued, not released:** W4-SHORE-2, which draws the sea against its plane per pixel.

**MAP-CARGO-2** loading is now accepted locally at code/tests
`80f9eeacd1012cd58f3a010b8d5c6018810dedad`, gate source `abe568d6`, docs
`0aed8ec0334e9bfbdbb137d3662ac9e958c83887`; see the
[loading review](reviews/2026-11-02-map-cargo2.md). **MAP-CARGO-3 is now accepted
locally** at clean code/gate source `3276a0bbef3381a348c8bd2e912fdc83b5ee8568`,
docs `cf78d212f64b5a69633de70341e00e2b5a724e16`; see the
[direct correction review](reviews/2026-11-02-map-cargo3.md). Its ordinary
publication is remote-verified. F-L1, the duplicated outline predicate and the
combined reload cut are closed. **MAP-ROAD-1 is reviewed with one bounded policy
correction**, local candidate `242c3433d879537dbaba8162da852a48109b6635`, tested
source `6e7214686788d3d6f852e2ab4784f6c3b3ba73da`; see the
[direct road review](reviews/2026-11-03-map-road1.md). The tile-object behavior
and valid evidence are retained. **MAP-ROAD-CLOSE-1's two-policy correction is
fixed**, with clean gate source `eae9f4aeff6030c1886970fe3271160c538f15af` and
comment/doc increment `aad1735bc00612695eb49ce356a4e0a42eb17d5d`. The combined
local candidate/forester draft is `c82608bd22a8a86ff50dddc039143b46e658467c`.
**MAP-ECON-1 and CLOSE-1 are ACCEPTED for the local headless scope** at
`f40a76022c609092156c457509f0c49e7905f96e`; termination code/tests
`1e9c770e6f0a44d84aa40fb5b04bad1818875a92`, small proof tests
`44096b840bde4760e9b0520d1a9afaa1b0ec7288`, clean gate/contract source
`e6b1083c4a967bb4b1f792e557929e271d0602ad`. The
[direct closeout acceptance](reviews/2026-11-03-map-econ1-close1.md) closes the
public-end P2: the original independent counterexample now passes 42/42
assertions; Debug and ASan/UBSan each execute 202/202 without failures, skips
or sanitizer reports. The previous real opening delivered **448 raw 8.8 wood
units = 1.75 t** of new wood, with 28/28 fresh-process replay checkpoints;
that valid evidence is retained at its original pin. Stream 126 and PRNG timeline
4 remain the accepted Mac identity, unchanged by CLOSE-1. No further forestry
correction or broad test round is assigned.
**MAP-POP-READY-1 is ACCEPTED after one corrected documentation finding** at
`2f876af5a44fc3395819134e79a9595a13d9f7d9`; executor draft `dc949172`, report
`b77860e2`. The goods-id lookup has one entry per registry good, with need count
as its sentinel; zero needs does not mean a zero-length lookup. Read the
[direct review, correction and binding decisions](reviews/2026-11-03-map-pop-ready1.md).
**MAP-POP-IMPL-1 is ACCEPTED** at `73e7ab1a129c4712747333d9f34a225be03f063f`:
executor gates `a5186375`, reviewed candidate `7939b958`, bounded test/reference
correction and focused gates `4438d7f2`. Read the
[direct review and validation limits](reviews/2026-11-03-map-pop-impl1.md).
Stream is now 127, PRNG timeline 4; House dispatch remains a no-op.
**MAP-POP-LIVE-1 is REVIEWED, CHANGES REQUIRED** at local report `135edf6f`,
runtime candidate `aca26cea`. Read the
[direct population review](reviews/2026-11-04-map-pop-live1.md).
Retain the resident, transition and opted-in canary work; do not claim complete
live progression or publish this candidate as accepted. **MAP-POP-CLOSE-1 below
is READY, RELEASED**: fixes, fresh-House initialization, service/route dependency
closure and actual upward progression form one bundle. Internal independent
approvals unlock its next package automatically; return once after the final
integration review. Keep the global House default off pending acceptance.
Windows' exact inputs and assignments below are unchanged.

**Update 2026-11-04, evening (MAP-POP-CLOSE-1 reviewed).** The Mac executor returned CLOSE-1 as **PARTIAL** at local,
unpublished head `1e65ea7d` (gate source `719f394a`). Read the [CLOSE-1 review](reviews/2026-11-04-map-pop-close1.md).
- **Retained:** the corrections, first assignment on the public build path, real-data growth, reload and replay.
- **The reported blocker is narrower than stated.** The Inn's carrier is the existing workshop carrier (job `0x33`),
  and 28 original towns already stock the alcohol, leather, cloth and salt a settler upgrade needs. No production chain
  or fixture stock is needed.
- **`endless03` cannot upgrade even with a working Inn:** settlers force leather, cloth and salt, which that town does
  not stock.
- **Identity:** stream 128 and PRNG timeline 5 are confirmed as amended in place; they were never accepted or
  published.

**MAP-POP-CLOSE-2 below is READY, RELEASED.** The last accepted and published MAP pin stays `73e7ab1a`. Windows' inputs
are unchanged.

CARGO-3's wording correction is closed; do not restart cargo gates or R0–R5.
Windows' integration input stays `0c401c8d`; loading, the cargo correction and
roads do not expand its assigned playtest (W4-OPENING-2 below takes them, at `73e7ab1a`, after W4-MAIN-1). Both opening inputs are reviewed, accepted and
reachable on GitHub. W4-OPENING-1 integrates selection, sailing, founding,
unloading and building in the live window for an owner playtest. Use only the
two exact accepted pins; this does not accept a newer Mac gameplay head.

| Lane | Host and ownership | Current instruction |
|---|---|---|
| Rendering / W4.4 | Windows workstation with RTX 4090; sole rendering implementation owner | **W4-OPENING-1-CLOSE (CURRENT), THEN W4-MAIN-1, THEN W4-OPENING-2, ALL RELEASED IN THIS ORDER.** Continue `task/W4-opening-1` from `02133ade` with the closeout below. Then merge into `main` on `task/W4-main-1` (the orchestrator moves `main` after its review). Then W4-OPENING-2 from the new `main`, with the Mac pin `73e7ab1a129c4712747333d9f34a225be03f063f`. **W4-DEPTH-1-CLOSE (released) runs beside them on `task/W4-depth-1`, one lane at a time; the closeout stays the critical path. Its merge follows the depth review §5.** Do not start W4-SHORE-2 until the owner releases it. Until W4-OPENING-2, the Mac input stays exact `0c401c8d084cfd9feb8aec846595291f87f22cce`. Preserve valid verifier/corpus/adapter evidence. |
| MAP.2 / gameplay | Mac, existing MAP executor | **MAP-POP-CLOSE-1 REVIEWED, PARTIAL; MAP-POP-CLOSE-2 READY, RELEASED.** Continue local `1e65ea7d` through K0–K4 below, with independent package reviews and one final handoff. Add the Inn supply path, reusing job `0x33`, and prove the first real upgrade in an original town that stocks the settler goods; no production chain, no fixture stock, no default flip. At the handoff, one plain fast-forward push of the final head for review; no other push. Windows keeps exact `0c401c8d` until W4-OPENING-2, which takes accepted `73e7ab1a`; never substitute the population tip. At the next natural checkpoint, without interrupting this bundle, run macOS `test_support` at published `task/W4-main-1` and report its exact pin/result; this is non-blocking, authorizes no merge or coordinator redesign, and stays pending if unpublished. Complete the MAP documentation handoffs in playtest review §7 at the next docs checkpoint. |

### W4-OPENING-1-CLOSE — finish the opening candidate for acceptance · RELEASED, CURRENT

**Base:** `task/W4-opening-1` at `02133ade9e66426ab093d8377992aec159367671`, the same branch.

This closes the open items of the [playtest review](reviews/2026-11-03-w4-opening1-playtest.md). §6 still had items
open at `02133ade`, and §7 added more.

1. **River Q5.** In `river_flow_corpus_test.cpp`, island `D_256x256_0045_5B` scores 0.485714 against 0.55.
   - Measure it at shore 16 on the head, to settle "unchanged by the shore".
   - Compare D's river mask and flow with `river_qa`, before the projection fix and after it.
   - Report the numbers. The orchestrator decides the per-island bar. Do not lower the bar or skip the island
     yourself.
2. **lan5 pick.** `dev_tool_test.cpp` picks the wrong tile at 1 of 8,486 pixels.
   - Print the failing pixel: its screen position, the expected and picked tiles, its triangle, and its distance from
     the B–C diagonal.
   - Fix the test's interior filter (also skip within 0.06 px of the diagonal), or rasterise with an exact fill rule.
   - Add a test that fails without the fix.
3. **Projection P3s (§6).**
   - `docs/testing/ship-opening-playtest.md` lines 56–58 still say the mirror fault is present "until that merges".
     Remove that text, and add checks: Kontor piles toward the water, continuous fences, ship bows forward, the light
     from the top right, the reversed tie-break.
   - `river_flow_corpus_test.cpp:1214` prints a pointer instead of the island name.
   - `iso.hpp:111` says every tile-to-screen mapping is `tile_top`, but about 12 inline copies of the formula remain.
     Route them through the shared functions, or record a reason for each one kept.
   - Record the Linux and Apple enhanced digests as NOT_RUN.
   - Give Q5 one value with its head; the hand-over note says 48.5 % and REPORTS says 48.6 %.
4. **Wine check of the founding keys.** Check that `.` and `,` reach the Kontor cursor while founding, and which way
   each turns (property-5 setter `AnnoFrame.dll` `0x10049166`). Record the result in OQ-520 and the hand-over note.
   Wine runner runs are pre-authorised. Keep the run silent.
5. **Gates at the final head, in this order:**
   1. the WSL ASan/UBSan lane;
   2. native app, GPU and render;
   3. the window tests;
   4. the WSL warnings.

   Never run the sanitizer lane and the native render gate side by side.
6. **Review and publish.** Get an independent read-only review of the closeout commits, add a REPORTS entry, and
   publish with an ordinary push.

**Stop:** the candidate for the orchestrator's acceptance review. Its exact head is W4-MAIN-1's input `H`.

### W4-MAIN-1 — merge the accepted Windows lane into main · RELEASED, STARTS AFTER W4-OPENING-1-CLOSE IS ACCEPTED

**Owner decision, 2026-11-04** ("Yes and merge"; chose "Main, as own package").

**Why.** Nothing from the Windows lane has reached `main` since 2026-10-30:
- the merge base is `632229eac5c40a6196c4d734fe5198e77b96fe4f`;
- `task/W4-opening-1` is 552 commits ahead of it;
- UI.2 waits on "W4.4 merged" (round dt).

The merge also brings the Mac pin `0c401c8d` into `main`, because it is part of the Windows branch. It brings no newer
Mac commit.

**Trial merge** (orchestrator, `git merge-tree` of `main` `8b37276b` against `02133ade`):
- 675 files changed, 118,700 lines added and 1,067 removed;
- the branch changes no board or rule file (TASKS, reviews, `AGENTS.md`, `CLAUDE.md`, the evidence contract,
  `.github`);
- 11 files conflict:
  - `CMakeLists.txt` and `docs/orchestration/REPORTS.md`;
  - **the test memory coordinator:** `src/test_support/src/{coord.cpp, coord_server.cpp, coord_wire.hpp, memory.cpp}`
    and `src/test_support/tests/{budget_proc_harness.hpp, child_process.hpp}`. Both lanes fixed the same macOS problems
    in different ways:
    - `main`: `7e897fac` (run the budget coordinator on macOS) and `8550194d` (skip the WSL meter cases without a
      meter), both 2026-10-31;
    - the branch: `4cc0a3ad` (Darwin executable path, process identity, spawn) and `abc1f8c3` (detached stdio), both
      2026-10-30;
  - **add/add conflicts:** `tools/test/{ctest_verdict.py, ctest_verdict_test.py, run_selected.sh}`. Main's side is
    `0569b804` (evidence safeguards).

**Steps.**
1. **Where to merge.** Use a separate checkout: never a live executor worktree, and never the owner's `E:\anno1503`
   checkout. Branch `task/W4-main-1` from `origin/main`. Make an ordinary `--no-ff` merge of exact `H`, so the parents
   are `main` and `H`. No rebase, squash or force push.
2. **Resolve each conflict, and record each resolution in REPORTS**, with both parents:
   - **`REPORTS.md`:** keep every entry from both sides, in date order. Edit no entry.
   - **`CMakeLists.txt`:** the union of both sides' targets and options. Drop nothing.
   - **`tools/test/*`:** main's versions are the workflow authority the gates already use (every gate file names its
     `workflow head` on `main`). Take main's, unless the branch's version holds a fix `main` lacks. Then port that fix
     with a test, and list it. Also fix the skip classifier, which counts "skipping: no game files" as "other"
     (depth review F6), with a test.
   - **Coordinator files:** one implementation that keeps both sides' tested behaviour (macOS spawn and process
     identity, detached stdio, the WSL-meter skips).
     - For each hunk, record both intents, the resolution and the test that covers it.
     - If a resolution changes wire or reservation semantics, bump `kCompatVersion` (`CLAUDE.md`).
     - If a hunk needs a new semantic choice, stop and isolate it for the orchestrator's review. Do not invent
       coordinator semantics, and do not start an infrastructure campaign.
3. **Gates on the merge.**
   - Native MSVC, all lanes: formats, sim, sim_load, app, GPU and render, plus the `test_support` and `tools/test`
     tests.
   - The window tests.
   - The WSL GCC 13 / Clang 18 `-Werror` check, and the WSL ASan/UBSan lanes.
   - `tools/agree.py`.
   - The CI checks reproduced locally: clang-format 23.1.3 over the changed files, and the Linux GCC release lane.
   - **macOS:** the coordinator's macOS paths cannot run on Windows. Record them as NOT_RUN. The Mac lane runs the
     `test_support` suite at the published `task/W4-main-1` at its next checkpoint (MAP row). This does not block.
   - **Expected failures:** none, except items accepted with a disposition in W4-OPENING-1-CLOSE.
4. **Review.** Get an independent read-only review of the resolutions. Brief the reviewer with the conflict list and
   both parents, not with your conclusions. Fixes get a focused re-review.
5. **Publish and stop.** Push `task/W4-main-1` with an ordinary push, verify the remote hash, and add a REPORTS entry.
   Then stop.

   The orchestrator reviews the branch, then fast-forwards `main` to the merge commit with an ordinary push, no force.
   If `main` has moved in the meantime (board commits), merge `origin/main` into `task/W4-main-1` again; only docs
   conflicts are expected. Rerun the format check and the affected tests, and hand back.

**After `main` moves:** the orchestrator records the merge in `docs/PLAN.md` and marks UI.2's "W4.4 merged" condition
as met. It also checks GitHub CI on `main` and dispositions each failure.

**Out of scope:** Mac commits beyond `0c401c8d`, new features, and any coordinator redesign.

**Stop condition:** `main` is the reviewed merge commit, and the task branches are preserved.

### W4-OPENING-2 — the first wood chain in the live window · RELEASED, STARTS AFTER W4-MAIN-1

**Goal:** the owner's next playtest can run, in the real window:
1. found;
2. unload;
3. build a forester and a road to the warehouse;
4. watch the worker cut a tree;
5. watch the market cart bring the wood to the warehouse;
6. load the wood onto the ship.

**Inputs:**
- **`main` after W4-MAIN-1.** Record its exact hash.
- **The accepted Mac pin `73e7ab1a129c4712747333d9f34a225be03f063f`** (MAP-POP-IMPL-1, on GitHub). It contains:
  - loading (MAP-CARGO-2, `LoadCmd`) and the hold-bounds correction (CARGO-3);
  - ordinary land roads as tile objects through `BuildCmd` and `DemolishCmd` (MAP-ROAD-1, CLOSE-1/2; 21 shipped road
    types, `docs/sim/road-placement.md`);
  - the forester chain (MAP-ECON-1, CLOSE-1; `docs/sim/forestry.md`): worker figure 102 → tree cut → `Farm.raw` →
    `HOLZ` → the road-connected market cart → the warehouse;
  - the population inputs (MAP-POP-READY-1, IMPL-1); House dispatch is still a no-op;
  - saved-state stream 127 and PRNG timeline 4.

  Not the moving tip: MAP-POP-LIVE-1 is still in progress.

**Trial merges** (orchestrator, 2026-11-04):
- `73e7ab1a` into `task/W4-opening-1`: the code merges cleanly. Only `docs/open-questions.md` and `REPORTS.md`
  conflict.
- `73e7ab1a` into `main` `8b37276b`: the same, plus the coordinator files, `CMakeLists.txt` and `docs/perf/README.md`.
  If these reappear after W4-MAIN-1, apply its recorded resolution.

**Steps.**
1. **Merge.** Branch `task/W4-opening-2` from `main` and make an ordinary merge of exact `73e7ab1a`. Record both parents
   and every resolution. Re-record the Windows session and replay fixtures that the stream change invalidates, with a
   reason for each.
2. **Build the Mac code on Windows and Linux.** This is NOT_RUN in its acceptances.
   - Run the MSVC suites for sim, sim_load and formats, the WSL warnings and the ASan/UBSan lanes.
   - Run the Mac lane's headless canaries on Windows:
     - the forestry real opening: 448 raw 8.8 wood units = 1.75 t, 28/28 fresh-process replay checkpoints;
     - loading;
     - roads.
   - Results and state hashes must equal the Mac's recorded ones. Any difference is P1: stop and report the first
     checkpoint that diverges.
3. **Road tool, in the opening's building tool.**
   - Use `BuildCmd` and `DemolishCmd` with the types that `road_class` accepts.
   - A drag places one road per tile along its path. A drag limit from the original
     (`docs/research/building-placement.md` §3) may be added only as evidence-backed configuration, never as a hard
     limit.
   - The preview applies the sim's placement rule read-only. The sim's refusal is shown.
   - Roads are tile objects: draw them from the tile layer, in the original's decal formats. Update them per chunk,
     never with a whole-island rebuild.
4. **Live land figures.**
   - Extend the published snapshot for the forester's worker and the market cart: stable figure ids, type, owner,
     position, facing, and animation state. Include the tile change when a tree is cut.
   - Draw them from the original decoded figure sprites, with palette and shadows, through the existing unit renderer
     on CPU and GPU, in painter order with the buildings.
   - Interpolation is presentation-only. Movement must not rebuild terrain. Retained snapshots stay bounded.
   - A marker or a static sprite is not a live figure.
5. **Feedback.** A developer readout, not UI.2:
   - the selected building's production state: the forester's `raw`, `out` and workers out;
   - the warehouse stock, including the delivered wood;
   - each value correlated to its snapshot.
6. **Loading.** The counterpart of the unload interaction: selected own ship, own warehouse in range, hold, goods and
   quantity.
   - Use the accepted request granularity and partial-transfer rules (`docs/sim/ship-cargo.md`).
   - Show the amount actually moved, and the stock before and after.
7. **Proof.**
   - **A full window session** through the production SDL path: found → unload → forester and road → the worker cuts →
     the cart delivers → load onto the ship → quick-save → resume → continue. No grants.
   - **A replay** in a fresh headless process, with matching outcomes and hashes. A changed road or load quantity must
     diverge at a recorded checkpoint.
   - **Adapter controls:**
     - a road refused on water;
     - a road refused on an occupied tile;
     - demolishing a road;
     - loading beyond the stock or the hold;
     - a stale warehouse target.
   - **Mutation checks:** a suppressed window road command, and a partial load shown as complete. Each must make its
     check fail.
   - **Renderer:** a GPU/CPU comparison of a small scene with a walking figure.
   - **Wrap-up:**
     - a playtest card section;
     - the full gates (native, the window tests, WSL warnings and sanitizers);
     - an independent read-only review;
     - publish `task/W4-opening-2`.

     Then stop for the orchestrator's review and the owner's playtest.

**Out of scope:**
- live population (MAP-POP-LIVE-1, a later integration);
- W4-DEPTH-1's own work (its merge follows the depth review §5);
- UI.2, network and trade routes;
- the zoom artifacts (separate, once the owner's console log arrives).

### W4-DEPTH-1-CLOSE — finish the depth package for acceptance · RELEASED

**Owner and starting point:** the Windows executor, in the depth worktree
`E:\anno1503\.claude\worktrees\wizardly-chatelet-f638c2`, from `task/W4-depth-1` at `0fd1d219`. Read the
[depth review](reviews/2026-11-04-w4-depth1.md) first. The PC is shared with W4-OPENING-1-CLOSE, which stays the
critical path: run one lane at a time.

**Steps:**
1. **Mutation runs (review F1).** Run three mutants of `depth.cpp` against the depth cases. Where `gpu.wgsl` holds the
   same rule, run its twin too.
   - `<` instead of `<=` in the depth test;
   - the `z` byte ignored, so that pixel depth = base;
   - the write rule changed: once with every format writing, once with none.
   For each, record the case that fails and that the legal control passes. Restore the source byte for byte.
2. **Sanitizers (F2).** At the depth head, run:
   - the WSL ASan/UBSan app lane as the closeout runs it (`anno_app_tests` and the input and alloc tests);
   - the `scene reclaim` case alone under ASan.
   The GPU code stays NOT_RUN.
3. **Performance on an idle host (F3),** following `docs/perf/README.md`. Record the host load before and after.
   - `bench_frame --chunk-profile ... --depth on|off`;
   - `bench_frame --renderer gpu --depth on|off`, for the extra pass's frame time;
   - the atlas bytes with and without the z plane;
   - the headless +16 % split into the ground probe and the per-pixel test, or the reason it cannot be split.
4. **The anchor rule on more objects (F4).** Use the existing dumps (`cache/orig/depth1/`): check a tree and one other
   building class. If either disagrees with "footprint centre at h · 0.04", record it in OQ-026 and say which objects
   the renderer then draws wrongly.
5. **Fixtures off the height-0 stand-in (F5).** Make `dev_visibility_test.cpp`:212/:228 and
   `picture_gpu_test.cpp`:544/:561 place through the real placement rule, or assert a minimum of visible pixels. No sim
   change. Record the stand-in question for the MAP lane in the report.
6. **Small items (F6).** Fix lines 68–70 of the playtest card. Add a road view and a meadow view next to a building to
   the sheets.
7. **Gates on the final head:**
   - render, render heavy, app, app heavy, GPU and the no-game lane;
   - the WSL warnings for the changed files, and clang-format 23.1.3;
   - the sanitizer lanes of step 2.
   River Q5 stays the known failure (W4-OPENING-1-CLOSE item 1); any other failure stops the package.
8. **Review and report.** An independent read-only review of the close items. Then a REPORTS entry with the pins, the
   counts, the measured figures and Decisions and reasoning. No push or merge outside the merge order.

**Owner check** (on `task/W4-depth-1`, at any time):
- the card's "Depth checks" 1–4;
- the start Kontor at `--shore 16.5` and at `--shore 26.5`, beside the GOG capture;
- the two decisions of review §3.

**Merge:** per review §5.

### W4-SHORE-2 — the sea against its plane, per pixel · QUEUED, NOT RELEASED

**Why:** the original's sea plane is ≈ 26.5 `HEIGHT` (depth review §3, `docs/re/annodisplay.md` §9.3).
- **The original** draws the sea on every screen pixel with `(x + y)` even whose cached height lies below that plane,
  so its water edge follows the terrain.
- **Ours** decides where water may be by 8 × 8-tile sea regions with land classifier 19. At 26.5 that shows as stepped
  water edges on wide beaches.

**Start:** only after the owner's release, from the branch that holds the accepted depth.

**Steps:**
1. **Design** in `docs/render/depth.md`. Cover:
   - how the sea is drawn over all chunks and depth-tested against its plane, in the original and the enhanced water
     modes;
   - what changes for the rivers, the coast pieces and the river QA;
   - the ships' hulls below the plane, if the trace shows how (`annodisplay.md` §9.2, "Ships").
2. **Implement** on the GPU with the CPU reference, in both water modes. GPU = CPU on real data.
3. **Measure:**
   - Q5 and the five QA islands at 16.5 and at 26.5;
   - the re-recorded digests, each with its reason;
   - performance.
4. **Owner check:** the start Kontor and the four `S01_02_01` Kontors at 26.5, beside the GOG capture. The owner then
   chooses the default shore.

**Out of scope:** view rotation and the zoom artifacts.

### W4-DEPTH-1 — per-pixel sprite depth against the terrain and the water · RELEASED BY THE OWNER 2026-11-04; REVIEWED, CLOSE ITEMS ABOVE

**Origin (owner, 2026-11-03; recorded 2026-11-04).** After the H1 pictures the owner reported that:
- the original's Kontor is embedded in the ground, while ours floats;
- its road floats;
- grass is drawn over it;
- its piles are not in the water.

H1 cannot close this gap. The executor proposed a renderer task of its own, OQ-026 (c), and left the timing to the
owner. The orchestrator recommended the following, and the owner approved recording it this way (2026-11-04: "Yes"):
- make it its own task, not part of W4-OPENING-1;
- start with tracing, before any design or code;
- **start only after the owner's playtest of `02133ade` and the owner's release.**

Assigned to the Windows executor, the sole rendering implementation owner. Branch from the W4-OPENING-1 head that the
owner has playtested; `02133ade` today.

**Evidence so far** (`docs/re/annodisplay.md` §2, §3, §5, §6):
- **Depth buffer.** The original draws into a 16-bit colour surface with a parallel u16 depth buffer. A pixel is drawn
  when `pixel_depth <= buffer`, so smaller is nearer.
- **Sprite depth.** `Z` formats (`RGBZ`, `RGBAZ`, `RGBWZ`, `RGBZM`) add a signed 8-bit offset to the sprite's base depth
  for every pixel. `V` and `H` formats add a ramp per row instead. `H` decals (roads, `RGBAH`) are tested but never write
  depth.
- **Three modes:** no depth (A), test only (B), and test plus write (C, the default). See the mode table in §2.
- **Ground.** The terrain triangles are rasterised with Gouraud lighting and test and write depth
  (`tTriangleGouraudZBufferRW`).
- **Projection.** `depth = m02·x + m12·y + m22·z + oz`, with a camera depth scale of `0.000384615`.
- **The Kontor.** Its 12 frames are `BSH_RGBAZ`, with offsets from −70 to +54 (executor, hand-over note §4.1).
- **Ours today.** This renderer depth-tests only the sea and the rivers against the terrain. It draws every other
  sprite whole on top.

**Steps, in order. Each step's result is written down before the next starts.**
1. **Trace first.** Close the two open items of `docs/re/annodisplay.md`, with addresses:
   - **(a) depth units:** follow the depth argument from `0x10011e60` to item `+0x20/+0x24` to `0x1000d3d0`;
   - **(b) modes per layer:** which mode A, B or C each layer uses (static scenery, buildings, figures, ships, decals,
     UI), from the callers of `SetState(1|6, …)`;
   - **(c)** which world point the game passes as each sprite's base depth (the Open item "Which world point").

   Use the project's RE tools, and the Wine runner for dynamic checks (pre-authorised). Do not guess. A result that
   stays open goes to OQ-026 (c).
2. **Design.** Write a section in `docs/render/iso.md`, or a new `docs/render/depth.md` linked from it, covering:
   - the depth model, with evidence for each part;
   - how it maps onto the GPU compositor;
   - the CPU reference path;
   - what changes in the original and the enhanced water modes;
   - the effect on cached chunks and performance.

   Stop for the owner when the design changes the look beyond the original's behaviour (memory rule "bugs vs feeling
   changes"). Faithful depth is the original's behaviour; any softening is an opt-in option.
3. **Implement.**
   - Put the depth test on the GPU, with a CPU reference.
   - Add a test that compares GPU and CPU.
   - Re-record the corpus digests, giving a reason for each change.
   - Add mutation checks for the depth comparison, the per-pixel offset and the write rule.
   - Measure performance per `docs/perf/README.md`.
4. **Owner check.**
   - Before/after views of the four `S01_02_01.sze` Kontors, of the start Kontor against the original's
     `cache/o1/r7/gog_start_kontor_x4.png`, and of a road and a meadow next to a building.
   - Add the checks to `docs/testing/ship-opening-playtest.md`.

**Acceptance:**
- the evidence contract (`docs/testing/evidence-contract.md`), with an independent read-only package review;
- the full Windows gates, the WSL warnings and the sanitizer lanes;
- no sim change.

Out of scope:
- view rotation;
- the zoom artifacts (separate; the owner's console log is pending);
- river Q5 and the lan5 pick (W4-OPENING-1).

### W4-WIN-FINAL — targeted corrections and complete sanitizer coverage · REVIEWED; VERIFIER FOLLOW-UP BELOW

**Review checkpoint:** published `dad27da7`, tested head `37da91b`, code tree
`8367dc0`. Read [the direct review](reviews/2026-11-02-w4-win-final.md).
Retain the completed F2/F3 behavior and F4 corpus work, with the documented
stderr-verdict and script-identity limitations. Do not restart F1–F4. The packet
below is acceptance history; W4-WIN-VERIFY is now accepted and W4-OPENING-1 is
the current instruction.

Continue the existing Windows branch `task/W4-3-live-buildings` from its current
head, preserving any newer work. Reviewed candidate:
`f5a323d0ce9c6e58d2770e5ab821b98fb68aa16d`, source/tools
`07a7505201066a0522804527fd3abb4e8022fe8e`. Read the
[source review and decisions](reviews/2026-11-02-w4-win-close.md).
This packet supersedes C0–C4 as the active assignment. Do not restart host
migration, redo R1–R4, repeat valid GPU matrices, or wait for another approval
between the packages below.

**Goal:** correct the two concrete source findings and the capture-contract gap,
prove the actual stalled-reclaimer test path, and finish the missing sanitizer
coverage. Stop with a published review candidate and reconciled evidence.
UI.2, gameplay, renderer optimization and a new broad performance campaign are
outside this packet. The Mac executor continues MAP-NAV-3 independently.

#### Read order, ownership and preflight

1. Fetch `origin/main` and read its `AGENTS.md`, `CLAUDE.md`, PLAN §§0.1–0.4,
   `docs/testing/evidence-contract.md`, `docs/testing/README.md`, this packet and
   the linked review. Use `git show origin/main:<path>` or the established
   read-only workflow checkout; **do not merge main into your task branch**.
2. On your branch, read the final W4-WIN-CLOSE entry in
   `docs/orchestration/REPORTS.md`, its review records and
   `docs/orchestration/evidence/w4-win-close/INDEX.md`. Preserve all failed runs,
   manifests and the Windows-local archives. Do not publish their original-art
   screenshots, original assets, executables or a whole cache dump.
3. Read `tools/test/viewer_wiring.py` and `tools/test/viewer_wiring/*.script`,
   `tools/bench/dev_session_capture.py`, `tools/test/window_tools_contract_test.py`,
   `src/app/src/{viewer,main,dev_script}.cpp`, the relevant headers and
   `src/app/tests/{dev_script,dev_visibility,visibility}_test.cpp`.
4. Read `src/app/tests/scene_picture_recovery_test.cpp`, its settling/baseline
   callers, and the production reclaimer interface sufficiently to control its
   state. Read `src/app/tests/play_test.cpp`, `src/app/CMakeLists.txt`, the root
   `anno_add_doctest` registration, and existing sanitizer configurations.
5. Confirm branch/source/workflow/binary identities and available RAM, disk,
   CPU/GPU load and tool versions. Reuse valid host calibration; use current
   capacity for new jobs. Keep admission control and GPU serialization enabled.
   Use the existing workflow with explicit source/build roots. Do not alter
   shared coordinator semantics, another lane's files or the owner's apps.
   Keep the Windows screen awake during owned jobs and release only your own
   assertion afterward. No rebase, force push, pruning or main merge/push.

#### F1 — make viewer and capture verdicts discriminate

1. Reproduce the viewer false pass cheaply before changing code: a zero-exit
   fixture executable that creates no session/log currently passes
   `viewer_wiring.py`. Keep the output. No original game run is needed for this
   contract reproduction; provide the prerequisite environment explicitly.
2. Validate actual script execution, not the last line of the input file.
   Require the expected session evidence, complete ordered execution including
   terminal `quit`, and correspondence to the invoked script. Reject absent,
   empty, truncated, mismatched and failed records despite child exit 0.
   Preserve nonzero child and watchdog failure handling. Use the existing log
   format where sufficient; avoid an unrelated evidence-framework rewrite.
3. Add tiny public-command contracts with an otherwise successful child:
   complete matching log passes; each broken evidence condition fails at the
   intended check. A mutation restoring the former absent-log acceptance must
   fail the new contract. Register the contracts and require them in later
   gates; required viewer cases must not silently disappear from the inventory.
4. Give capture an explicit aggregate completion verdict: required capture
   refusal, missing capture or size mismatch cannot return success when the
   child exited 0. Record child status separately and preserve an already
   nonzero child status. Do not publish a success marker or treat invalid/partial
   images as valid captures. Update the CLI contract/documentation together.
5. Run native Windows public-command controls using the same tiny window with
   child exit 0: normal capture succeeds; minimised and off-desktop captures
   fail for the named capture condition. Add a separate child-exit-9 control
   proving status preservation. Test a size mismatch and an unfinished requested
   capture at the narrow seam if the desktop cannot create them deterministically;
   label mocked versus native coverage. Removing aggregate capture-failure
   propagation must fail while the zero-exit normal control stays green.
6. Revalidate saved C1/C2 session logs with the corrected execution validator.
   Do not invent missing historical records. Rerun only sessions whose evidence
   is incomplete or invalid; tool-only fixes do not require another GPU matrix.
   Keep original and revalidated verdicts linked to their respective tools.

**F1 complete:** both public tools distinguish successful execution from absent
or incomplete evidence, valid controls execute, the targeted mutations fail,
and retained viewer-session verdicts have been checked.

#### F2 — honor CPU submission/present failures in the real viewer

1. Follow `Viewer::draw_cpu` through texture update, render submission and SDL3
   present. Check their actual return values. Promote visibility only when the
   frame containing the current change was successfully delivered through that
   chain. Failure must keep the command pending and produce a useful diagnostic;
   use bounded retry/backoff rather than a busy loop or permanent pending state.
2. Exercise each failure boundary through the production CPU viewer. A narrow
   scripted/test seam may override the backend result, but the production
   result-to-visibility path must remain the consumer under test. With an applied
   command and otherwise fresh chunks, a failed update, submission or present
   must not report `Visible`; release the failure and prove that the same command
   becomes visible after a successful frame. Preserve normal positive delivery.
3. Prove that forcing success at the actual viewer call site makes the targeted
   regression fail. Testing `ChangeVisibility` alone with a supplied `false`
   argument is insufficient. Keep the CPU/GPU readiness contract consistent;
   do not alter simulation, assets, painter order or GPU algorithms to fix this.
4. Configure a **viewer-enabled ASan+UBSan** WSL CPU/SDL-dummy build and verify
   that `viewer.cpp`, the input executable and real viewer-script cases are
   built/instrumented and registered. Resolve the existing SDL configuration
   prerequisite using the smallest supported headless setup. If unavailable,
   report the exact configuration failure while finishing independent work;
   viewer-OFF tests cannot substitute for this coverage.
5. Run the focused actual-viewer controls (R1, R4 and the new CPU failure cases)
   and affected input tests under both the native Windows build and this WSL
   sanitizer configuration. Freeze source before acceptance runs and use the
   canonical runner with explicitly required contract/viewer test names.

**F2 complete:** real CPU errors cannot falsely settle visibility, successful
delivery still works, and the actual viewer/input paths have executed under
ASan/UBSan without reports. A headless CPU run is not a GPU/display claim.

#### F3 — prove the reclaimer baseline at its actual call site

1. Preserve the old failed whole-binary run and the identified baseline defect.
   Historical disposition is now approved: the missing counters prevent exact
   attribution of that occurrence; retain this uncertainty, rather than trying
   indefinitely to reproduce it under arbitrary load.
2. Add diagnostic before/current waiting, in-flight, bytes and relevant lifecycle
   counters at the stalled case's assertions. Capture consistent stats snapshots
   so a future failure explains what changed. Do not relax the assertions.
3. Arrange foreign generations with explicit gates and run the same baseline
   path the stalled-reclaimer case uses. Factoring that case's body for a
   controlled prelude is allowed. The valid path must settle before baselining;
   removing the actual call must fail the intended invariant. An independent
   helper/arithmetic test that stays green when the call is removed is not enough.
4. Replace timing assumptions in the relevant regression with observed states
   or handshakes, including the 50 ms opener assumption where it is used as
   proof. Release every owned gate before joins on all exits, retain watchdogs,
   and keep production scheduling/reclamation unchanged.
5. Run the focused case, regression, relevant recovery/lifecycle suite and a
   small ordered same-process selection that includes the foreign-work prelude.
   Use at most ten focused repeats unless a new failure supplies a distinct
   hypothesis. No CPU-saturating process farm, full-app retry loop or need to
   reproduce the undocumented historical occurrence.

**F3 complete:** the known defect mechanism is excluded by the actual-path test,
diagnostics are present, affected controls pass, and old attribution is recorded
as historical uncertainty. Any new unexplained lifecycle failure remains OPEN.

#### F4 — apply the approved corpus policy and close once

1. Put exactly these two complete cases in the existing `heavy` classification:
   `play: islands are the ones the sim placed` and
   `play: tick 0 matches the viewer except objects the sim did not place`.
   Give them an **8,000-second per-test timeout in ASan/UBSan builds**. Preserve
   all scenarios/assertions; do not replace them with a subset or raise unrelated
   timeouts. Prefer target-local registration/metadata over shared infrastructure
   changes. Record label/timeout changes in testing documentation.
2. Inspect generated CTest inventory/metadata before the expensive run: both
   stable names exist once, their heavy labels and sanitizer timeouts are effective,
   all required viewer/input cases exist, and other cases are unchanged except
   explicit new regressions. Test selection must reconcile the ordinary app lane
   plus the dedicated two-case corpus lane to the complete required inventory.
3. After F1–F3 fixes and their focused reviews, freeze source/tools. Run one
   affected native MSVC gate; the viewer-enabled WSL non-heavy app/input/viewer
   sanitizer lane; then the two full corpus cases serially with existing admission
   and the approved timeout. Keep the existing shell contracts required. Avoid
   overlapping the expensive corpus with your own builds or other heavy gates.
   Save monotonic progress and host load so timeout and work cost remain distinct.
4. Reuse already passed unaffected formats/assets/sim/render and RTX/Radeon
   gates with an explicit changed-dependency rationale. Run warning checks on
   changed C++ scope. Run one short RTX real-window/replay canary after shared
   viewer/script changes; do not repeat 106-case adapter gates or the 13-case
   performance matrix unless changed dependencies or a new failure require it.
   Revalidate earlier raw session data with the final validator instead of
   remeasuring tool-only changes. A new failure gets focused diagnosis, not an
   automatic second full acceptance run.
5. Commit a small **text-only** evidence subset under
   `docs/orchestration/evidence/w4-win-final/`: exact commands, source/configuration/
   binary/tool identities, selected inventories, verdict outputs, relevant raw
   `script.log` records, target mutation diffs/failure excerpts and corpus progress/
   result extracts. Tie extracts to retained originals by path and hash and label
   extracts as extracts. Keep blob hashes versus Windows CRLF file hashes distinct.
   Sanitize only sensitive values, explicitly recording redactions. No game data,
   screenshots, binaries or large archive in git. Full logs stay preserved locally.
6. A fresh final integration reviewer inspects F1–F4 against the final code and
   evidence, reconciles selected/executed/failed/empty/skipped/incomplete, and
   checks that negative controls fail for the intended reason. Fix findings and
   re-review only the corrections/interactions. Record the exact code, tool,
   gate and documentation heads. Later documentation corrections do not demand
   another broad gate; later code/test changes need scoped coverage and explanation.
7. Publish committed results with an ordinary push to
   `origin/task/W4-3-live-buildings`, after verifying remote ancestry, and verify
   the full remote hash. This remains authorized review publication only.
   Preserve newer/divergent remote work and report it; never force or push main.

#### Independent review and stopping criteria

For each substantive package, deploy an independent **read-only review subagent**
that did not author it. It reads the changed code, consumers, tests, controls and
evidence; it must not delegate its review or rerun broad gates. Require a valid
positive control before a negative test, and require the expected assertion/error,
not merely any nonzero exit, timeout, skip, unrelated child failure or crash.
Ask it explicitly what would still pass if the production call were removed or
the artifact were absent. Fix actionable findings and obtain a narrow re-review
before marking the package complete. Record acceptance against each numbered
requirement, not a generic approval paragraph.

Stop for orchestrator review when F1–F4 meet their stated criteria, the final
integration review approves, and the candidate plus portable text evidence is
published. If a required gate/configuration remains unavailable or a defect
remains, publish the bounded work and report **OPEN / CHANGES REQUIRED** with the
precise blocker. Do not declare acceptance, start UI/gameplay, or expand the
assignment to obtain a fully green unrelated project.

Keep robust, flexible, data-driven interfaces: original content and gameplay
remain intact; no new hardcoded player/type/island limits or registry shortcuts
for tests. Large-city/complex-economy/multiplayer scale and the named display,
terrain, road and UI follow-ups remain separate work. MAP-NAV-3 and every Mac
executor file/process remain untouched.

### W4-WIN-VERIFY — finish the two verifier contracts · ACCEPTED, DO NOT RESTART

**Acceptance, 2026-11-02:** published candidate
`26107db089f558bc374208a4f414ca70f562e6f0`, runtime/full focused gate source
`f047165c67bc26c1498c5201c014db83553c6f6f`, final tools/tests
`3c808780c4e5e59d26375151c3ff8215c4c68602`. Read the
[direct verifier review](reviews/2026-11-02-w4-win-verify.md) for exact coverage,
reused results and remaining limits. The packet below is acceptance history.
Proceed to W4-OPENING-1; do not repeat V1–V3 or the unchanged corpus/adapter matrix.

Continue the existing Windows branch `task/W4-3-live-buildings`, preserving
newer owned work. Reviewed publication:
`dad27da7ba0ee26abdd3b1de61adaddbfba05ec7`; C++ code tree
`8367dc0e02ba0d1a1e776ba4eaee7e55a5ef5db8`; tested tools/source
`37da91b901a0ead4ee4c45205a909e9e4b1eae8c`.
Read [the review and reproductions](reviews/2026-11-02-w4-win-final.md).

**Outcome:** a successful viewer test proves that the intended complete script
ran and that no child sanitizer diagnostic was hidden. This is a bounded
correction to the existing evidence path. Keep the accepted CPU delivery and
reclaimer mechanics, full corpus, timeout policy and unaffected GPU evidence.
No renderer redesign, new generic evidence framework, coordinator work, MAP
changes, gameplay integration or original-game capture campaign is assigned.

#### Read and freeze

1. Fetch `origin/main` and read the common instructions, evidence contract,
   testing README, this packet and linked review without merging main into
   the task branch. Read your W4-WIN-FINAL report and portable evidence
   `SUMMARY.md`/`INDEX.md`; preserve the original Windows logs.
2. Read `tools/test/{viewer_wiring,viewer_session,viewer_wiring_contract_test}.py`,
   `src/app/src/{dev_script,main}.cpp` (parse, execution and session output),
   `src/app/tests/dev_script_test.cpp`, the registered viewer scripts and
   `docs/testing/app-sanitizer-lanes.md`. Inspect the canonical verdict reader's
   sanitizer recognition before adding a conflicting definition.
3. Reuse valid Windows/WSL calibration after checking current host capacity.
   Keep admission and GPU serialization enabled; keep the screen awake for
   owned work. Never stop another executor's jobs or awake assertion.
   Record exact source/tool/binary identities; freeze before each focused gate.

#### V1 — make child sanitizer output part of the verdict

1. Use a tiny otherwise-successful child with the existing complete session
   fixture. A clean control must pass. A recoverable UBSan `runtime error:`
   written only to child stderr currently returns PASS; preserve that
   reproduction. No actual undefined behavior or long game run is needed for
   this public-command contract.
2. Retain the child's diagnostic streams and make sanitizer reports fail the
   public wrapper even when the child exits zero and the session is complete.
   Send the relevant diagnostic and artifact path to the gate-visible output.
   Cover the channels the child can use; do not discard stdout then claim it
   was scanned. Preserve timeout/nonzero-child behavior and the expected SDL
   failure diagnostics (those are positive evidence, not sanitizer failures).
   Fatal UBSan configuration may be added as defense in depth; it does not
   replace the zero-exit diagnostic contract.
3. Extend the cheap public CLI contracts with a clean complete control,
   stderr-only UBSan, representative sanitizer report, and diagnostic plus
   expected SDL messages. Assert the intended sanitizer rejection and absence
   of PASS. A focused mutation that drops propagation must fail these cases.
   Do not broaden the shared runner or coordinator to solve a local wrapper bug.

#### V2 — identify the complete script the viewer actually parsed

1. Reproduce the review's ambiguity: the same legitimate-format session log
   matches both `look 1 2` and `look 1 999`, because only the first argument
   is recorded/compared. A changed `wait` is also absent from the comparison.
2. Record a complete canonical script identity from the script parsed by the
   viewer itself, including every argument and wait instruction, and compare
   it with the invoked script in the wrapper. A canonical full-step record or
   an appropriate content fingerprint plus the ordered completion trace is
   sufficient. Do not record only a hash supplied by the wrapper of the file
   it hoped the child used. Keep execution completion, FAIL, exit and
   session-file checks. This is accidental-mismatch detection, not an attempt
   to authenticate a malicious executable or invent signed evidence.
3. Add hand-derived contracts: matching complete script passes; changing only
   a second coordinate, an expectation's later argument, or a wait value fails
   identity despite child exit zero and an otherwise complete trace. Check
   the runtime producer as well as the Python consumer; deleting/weakening
   the actual identity comparison must fail the specific contract.
4. Treat old logs explicitly as legacy evidence lacking the new full identity.
   Do not upgrade them to newly verified identities or discard their useful
   historical results. Keep old-script provenance with existing pins; update
   the CLI/revalidation wording to state exactly which checks ran.

#### V3 — scoped verification, one correction review and publication

1. Run the cheap tool contracts and affected DevScript/session tests first.
   Obtain one independent read-only review of V1/V2's producer, consumer,
   failure propagation and counterexamples. Correct findings and re-review
   only changed paths. Do not impose a review/mutation quota.
2. Freeze the corrected source/tools. Use the canonical runner for the three
   actual CPU viewer cases (R1 publication, R4 occupancy, CPU delivery failures)
   and affected input/DevScript tests on native MSVC and viewer-enabled WSL
   ASan+UBSan. Require the corrected public tool contract. Retain complete
   stdout/stderr and reconcile sanitizer counts from those files with the
   gate verdict. The expected SDL failure messages must still appear.
3. Run one short RTX real-window canary to exercise the updated runtime
   session identity on that consumer. Check warnings for changed C++ scope.
   Reuse the 106-case RTX/Radeon runs and F4's two completed corpus tests;
   changes to diagnostic handling/session records do not require repeating
   them. Expand only for a concrete changed dependency or newly observed
   failure. No native-4K, scale or broad performance matrix is required here.
4. Correct the WSL workflow identity record without rerunning old work: the
   inherited `GIT_DIR`/`GIT_WORK_TREE` contaminated its displayed workflow
   commit. Record the actual runner file hashes/revision from retained
   evidence when recoverable, otherwise state the uncertainty. For new runs,
   ensure workflow and source are independently identified with inherited Git
   overrides cleared where appropriate; do not alter another worktree.
5. Report the two findings, exact correction/review/gate pins, selected and
   executed counts, diagnostic evidence, retained old results and remaining
   limitations. Add a small text-only subset in the existing evidence area;
   no original art, screenshots, binaries or large cache archive in git.
   Ordinary-push the committed review candidate to
   `origin/task/W4-3-live-buildings` after checking ancestry, then verify the
   full remote hash. No force push, rebase, main merge/push or pruning.

**Stop:** both counterexamples fail for their intended reasons, real clean
viewer controls pass with complete diagnostic visibility and script identity,
the focused independent review approves, and the candidate is published for
orchestrator acceptance. The historical reclaimer occurrence stays a documented
uncertainty; missing submit-only/in-flight-only mutants are not new blockers.
That boundary is now satisfied by the acceptance records above and below.
No extra owner permission is needed between V1–V3.

### W4-OPENING-1 — live ship-start opening for an owner playtest · RELEASED; CLOSEOUT CURRENT (W4-OPENING-1-CLOSE ABOVE)

**Review checkpoint, 2026-11-04 (Claude orchestrator, owner instruction).** The owner's answers are reviewed in §7 of
[the playtest review](reviews/2026-11-03-w4-opening1-playtest.md), with no P1 or P2:
- (b) the shore at 16.5 by default;
- (c) the founding turn key restored.

The reviewed range is `a150c11d..02133ade`, published at `02133ade`. §7 is the current Windows instruction. It also
lists three items for other lanes: the MAP lane's `ship-founding.md` and `map2_*` land definitions, and the BEL
characters on `main`.

**Review checkpoint, 2026-11-03 (Claude orchestrator, owner instruction).** The owner playtest rounds after the
published candidate `55e4ee61`, up to branch head `c1bf35cc`, are reviewed in
[the playtest review](reviews/2026-11-03-w4-opening1-playtest.md). Its §5 is the current Windows instruction. The packet
below stays the requirements and acceptance history; I1–I6 are not restarted.

**Owner instruction, 2026-11-02:** once the Mac gameplay work is reviewed, wire
ship selection, sailing, founding and unloading into the live Windows window so
the owner can test the complete opening personally. Include the existing
building tool after founding; a headless command demo alone does not meet this
outcome. This is a bounded developer playtest UI, not the full UI.2/game-menu,
network multiplayer, trade-route, combat or renderer-optimization program.

**Release checkpoint, 2026-11-02:** the orchestrator accepts Windows publication
`26107db089f558bc374208a4f414ca70f562e6f0`, runtime/full focused gate source
`f047165c67bc26c1498c5201c014db83553c6f6f`, final tools/tests
`3c808780c4e5e59d26375151c3ff8215c4c68602`; see the
[verifier review](reviews/2026-11-02-w4-win-verify.md). The Mac opening input is accepted at
`0c401c8d084cfd9feb8aec846595291f87f22cce` with the scope and limits in the
[cargo review](reviews/2026-11-02-map-cargo1.md). Its ordinary publication is
remote-verified, as is the Windows publication. **Start I1–I6 below.** These are
fixed integration inputs; do not substitute either moving branch tip or add
MAP-CARGO-2/3 to this playtest. No additional owner approval is needed between
the assigned packages, subject to their review and conflict stop rules.

#### Release conditions and ownership

1. Both acceptance records now exist. Windows input is
   `26107db089f558bc374208a4f414ca70f562e6f0`; Mac MAP-BUILD-1 plus MAP-CARGO-1
   input is `0c401c8d084cfd9feb8aec846595291f87f22cce`. Record these exact
   parents, their source/tools/gate identities and the limitations in the linked
   reviews. This release authorizes their integration for the assigned owner
   playtest, not full multiplayer, physical-display, fidelity or scale acceptance.
2. Both approved pins must be reachable from GitHub with their review/evidence
   summaries. The orchestrator arranges ordinary publication of the reviewed
   Mac task branch after its review; this packet does not tell the Mac executor
   to interrupt work or push unreviewed source. Do not take a moving branch tip,
   incomplete working tree or a copied binary as the integration input.
3. Once released, use a Windows-owned integration branch `task/W4-opening-1`
   from the approved rendering pin, reusing a suitable quiescent checkout.
   An ordinary merge of the **approved MAP pin into this integration branch** is
   authorized. This is the narrow exception needed to combine the two lanes:
   no main merge/push, rewriting either reviewed branch, rebase, force push or
   pruning. Preserve source branches and record both parents and resolutions.
4. The Windows executor owns this integration/app/render work. It consumes the
   accepted sim commands and contracts; it does not redesign gameplay or edit
   the Mac executor's checkout. If merge conflicts in coordinator/test-support,
   state formats or gameplay require a new semantic choice, isolate that exact
   conflict for review and continue independent preparation. Do not resurrect
   older coordinator semantics or start another infrastructure repair campaign.

#### Read order and preflight

1. Fetch/read main `AGENTS.md`, `CLAUDE.md`, PLAN §§0.1–0.4 and §0.7,
   `docs/testing/{evidence-contract,README}.md`, this packet and the two published
   acceptance records. Read both final REPORTS entries and their portable
   evidence indexes. Record the exact source and workflow revisions separately.
2. Read accepted W4 `docs/app/{game-shell,gpu-handoff,window-trace}.md`,
   `src/app/include/anno/app/{play,dev_tool,dev_input,dev_feedback,dev_session,
   command_queue,command_outcome,scene_picture}.hpp`, and corresponding source,
   `main.cpp`, `viewer.cpp`, `loader.cpp`, `dev_script.cpp`, picking/render-unit
   APIs, `tools/test/viewer_wiring.py` and its session scripts/contracts. Follow
   current filenames if the final closeout changed them.
3. Read accepted MAP `docs/sim/{ship-start-integration,ship-founding,
   ship-navigation,ship-cargo,shared-visibility}.md`, the MAP-BUILD-1 placement
   contract, `src/sim_load` ship-start loader, current command/codec/result/replay
   APIs and the real no-grants opening canary. Use the accepted unload command's
   actual name and semantics, not a new parallel UI-only implementation.
4. Check original figure sprite decoding, direction/animation/palette/shadow
   handling and scene-unit composition already used by the scenario viewer.
   Read applicable app input, visibility, picking and sprite-parity tests.
   A static scenario's unit display is not proof of a live moving-ship path.
5. Check current CPU/RAM/disk/RTX load and reuse Windows calibration with
   admission control and serialized GPU tests. Keep the display awake during
   owned runs; preserve the owner's awake assertion and other apps. Do not
   overlap heavy acceptance with builds. Current closeout evidence stays intact.

#### I1 — combine approved code and expose a true ship-start session

1. Integrate the two exact pins on the integration branch. Review conflict
   resolutions as their own small package. Compile the combined app/sim and run
   the affected command, placement, founding, unloading and replay canaries
   before adding window behavior. Resolve concrete Windows portability defects
   with focused tests; do not silently borrow Mac or old-renderer acceptance.
2. Add an explicit launch path using the typed ship-start manifest loader.
   Preserve its content/path validation, rules, seed, original loadout, peace,
   shared information and no alliances. Do not route it through the legacy fixed
   map loader's developer resources or prebuilt-city setup. Record manifest,
   content/rules identity and chosen local participant in saves/session logs.
3. Start with ships and **zero cities**, centre the camera on the selected local
   participant's ship, and provide a readable load error for an invalid manifest
   or missing asset. Candidate A may be an explicitly named playtest input;
   none is selected/installed as the release map and `lan5.txt` is unchanged.
4. Local participant is explicit data validated against loaded seats. Commands
   get the session's authenticated local issuer. Show other participants where
   the accepted information model permits it, without granting their controls.
   This is a local playtest in a five-seat world, not proof of network multiplayer.

#### I2 — publish and draw live ships without blocking the simulation

1. Extend immutable snapshots with the stable ids, owner/type, published position,
   facing and supported animation/state needed to display/select ships, plus
   local cargo/city-stock information needed by actions. Never expose a mutable
   `World`, raw entity pointer or registry slot as durable UI identity. On reload
   or disappearance, invalidate selections and pending targets deliberately.
2. Use original decoded ship assets, orientation, colour and shadows through the
   existing unit renderer for CPU and RTX paths. Carried scout/donkey remain
   embarked; do not draw them as independent walkers or add disembarkation.
   A marker moving over a static/missing ship is not a completed live-ship view.
3. Keep moving units separate from static terrain and building generations.
   A sailing tick must not trigger a whole-island/scene rebuild. Preserve painter
   order/occlusion and the owning frame lifetime; drawing and picking use a
   coherent published frame. Interpolation, if used, is presentation-only and
   cannot change commands, replay or authoritative movement.
4. Bound publication, pending uploads and retained frame resources. Missing or
   busy content may display a truthful pending state without stalling turns or
   input. Hold a ship snapshot/frame while the sim advances, then release it:
   no dangling state, premature visibility claim or shutdown wait on an unreleased
   test barrier. Use the already established ownership/present semantics.
5. Measure a short moving-ship run and deterministic work counters: no terrain
   rebuild from movement, picking work local to visible candidates, no growth in
   retained snapshots after settling. Include unrelated-city content in one
   focused control. This is a regression check, not a new late-game benchmark.

#### I3 — selection, movement and usable controls

1. Add ship hit-testing using the displayed transform/snapshot and stable ids,
   with a visible selection indicator and owner/name/cargo feedback. Do not reuse
   building/island-only picking for open-sea destinations. Test overlapping ships,
   zoom/camera transforms, DPI/resize and stale selections; inspectable other
   owners never become controllable through a local pick.
2. Write a short action/control table before wiring handlers. Preserve existing
   building shortcuts, wheel zoom and right-drag pan. Use familiar selection and
   move gestures where compatible: distinguish a right click from a pan drag,
   consume each gesture once, and never issue a move after a pan/cancel/focus loss.
   If a gesture conflicts, use an explicit labelled move mode instead of silently
   changing an existing action. Document every new binding on screen/help.
3. Issue the accepted `MoveCmd` and `StopCmd` through the same command queue,
   codec and simulation runner as existing building actions. A valid sea click
   produces one order for the selected ship and local issuer; invalid/outside
   input produces clear feedback. No direct position edit or test relocation.
4. Track each command's own id/outcome: queued, accepted/planning, moving,
   arrived, later route failure or refused with its actual reason. Acceptance is
   not arrival, and arrival is not a successful present. Replacements/cancellation
   cannot display an earlier order's success. Input and turns continue while the
   renderer is held. No displayed route is claimed until a real plan exists.

#### I4 — found, unload and enable the existing building tool

1. Offer founding for an eligible selected ship through the accepted configurable
   founding types. Display the original warehouse sprite/footprint and rotation;
   preserve the contract's click/origin and rotation-search semantics. Preview
   validation is advisory/read-only; `FoundCmd` decides at execution. Show price,
   cargo requirements and the real refusal. Never create a city in the UI.
2. After successful founding, derive the new city's id, ownership, warehouse and
   stock from a new published snapshot. Enable/update building controls for that
   city even though the session began without one. Remove assumptions that the
   developer player's first city was fixed at runner construction. Reuse the
   accepted terrain/height rule and ordinary build/demolish commands unchanged.
3. Provide a minimal readable unload interaction: selected ship, selected own
   warehouse, hold/goods and permitted quantity, destination stock/capacity,
   action and result. Use the exact U1 request granularity and partial-transfer
   rules. No arbitrary fixed amount, founding-material whitelist, four-hold cap,
   remote-city credit or silent unload-all. Re-read/revalidate target identity
   when sending; the sim still decides permissions, range and capacity.
4. Show actual transferred amount and authoritative before/after cargo and city
   stock, correlated to the command/snapshot. Refused or capacity-limited actions
   cannot look like complete unloads. The user can unload the supplies needed
   for the first ordinary building without console injection or developer grants.
5. Keep modes understandable: select/move, founding preview and building preview
   cannot consume one click twice. Escape cancels the current action; focus loss
   cancels an incomplete gesture. A vanished/changed ship, city or warehouse
   invalidates its target safely. Support another participant in a separate
   explicitly chosen session, not a hidden ownership override.
6. Provide a minimal save/resume path for the playtest using the existing sim
   save format: capture at a defined turn boundary, perform file I/O outside the
   window thread, and report write/load failures. A validated resume restores
   the actual saved world and resets stale selections/queued input; an invalid
   load leaves the running session intact. Preserve content/rules/local-seat
   identity and start a correctly identified recording segment after resume.
   A quick-save action and explicit resume launch option suffice; no save-slot
   browser or general menu system is required. Do not assume the current live
   window already has this path merely because the sim can save and load.

#### I5 — prove the window adapters, not just helpers

1. Drive the **production SDL window/input path** through a complete real session:
   select the local ship, sail, arrive, found, unload from its remaining actual
   holds, build a legal ordinary building, save/reload and continue. Preserve the
   same no-grants ledger as the headless canary. Scripted input isolation applies
   only to automation; the final owner session must accept real mouse/keyboard.
2. Add cheap adapter controls for wrong owner, out-of-range unload, full storage,
   invalid founding terrain, stale selection, cancel, pan-versus-order and a route
   failure. Use valid controls and the actual command/outcome assertions so an
   earlier unrelated refusal or absent script execution cannot satisfy the check.
3. Challenge actual wiring with representative mutations: suppress/retarget a
   window-issued ship command and prematurely show a previous command's success.
   Require the intended command/outcome or state assertion to fail. Separately
   prove a pan does not submit movement. No arbitrary mutation quota or full
   voyage per negative test; one real-session proof plus small adapter cases.
4. Record and replay the actual window-issued commands in a fresh headless process
   with the same start manifest, local issuer, rules and seed. Match outcomes and
   hashes; a changed unload quantity/target must diverge at a recorded checkpoint.
   New commands must survive the real session serializer, not only the sim codec.
5. On RTX, verify ships visibly move/turn, the warehouse and first building appear
   after successful presentation, and selection/cargo feedback follows the right
   snapshot. Cover one zoom/pan/resize sequence and held-render recovery. Compare
   a small nonempty moving-ship scene with CPU composition including occlusion;
   renderer parity is separate from gameplay state and physical display latency.
6. Save inspected local screenshots and brief session evidence at start, selected
   ship underway, founded warehouse, unloaded stock and first built structure.
   Record actual drawable resolution. Captures containing original art stay
   local; publish only permitted text evidence/hashes, never original assets.

#### I6 — scoped target-host acceptance and owner handoff

1. Freeze corrected source. Run affected native MSVC app/input/sim/load/replay
   gates; WSL GCC/Clang warnings; viewer-enabled ASan/UBSan for changed adapters
   and gameplay integration; and relevant RTX/CPU unit-render checks plus the
   real-window canary. Reuse valid unrelated W4-WIN-FINAL/MAP evidence. Rerun the
   costly corpus only when touched dependencies or a concrete regression require
   it, under its already approved serial timeout/admission policy.
2. Every substantive package gets independent read-only review; fixes receive
   focused re-review. One final integration reviewer checks accepted-source
   integration, input authority, snapshot lifetimes, true command serialization,
   no-grants accounting, controls and the portable evidence subset. No declaration
   of ready merely because a helper suite or a native build passed.
3. Provide a reproducible Windows launch command/script using the inspected
   build, explicit candidate/local seat and original asset directory, plus a
   one-page control/playtest card (`docs/testing/ship-opening-playtest.md`):
   launch, select, move/stop, found, unload, build, save, reload, expected feedback
   and known limitations. No manual file editing or debug grants to finish the
   opening. Distinguish automated completion from the owner's still-NOT_RUN
   personal acceptance; owner availability does not block delivering the build.
4. Commit reviewable concerns and a text-only acceptance/evidence index. Publish
   the new integration branch with an ordinary push after ancestry checks and
   verify its remote hash. No main push/merge, forced update, candidate install
   or external message. Stop with the review candidate and usable owner launch
   instructions; no further UI.2, network or trade-route expansion.

**Stop condition:** an independently reviewed Windows opening-playtest candidate
whose real live-window session completes select -> sail -> found -> unload ->
build with original supplies, with replay agreement and a reproducible owner
launch path. This is not full multiplayer, full original-runtime fidelity,
native-4K or late-game performance acceptance. Record those limits explicitly.

### W4-WIN-CLOSE — publish for review and finish bounded repair acceptance · REVIEWED, SUPERSEDED BY W4-WIN-FINAL

Historical packet below. Its unpublished-source description was true at dispatch;
the published candidate and current findings are recorded in W4-WIN-FINAL above.

Read the [handoff assessment](reviews/2026-11-02-w4-win-handoff.md).
The orchestrator has reviewed the supplied report against the assignment, but
cannot inspect unpublished Windows code or local ignored logs. The reported
head is `9ba01f1`, with `src/` at `4ecea07`; resolve and record full hashes.
GitHub was still at `a29b079` when checked. **Source review and acceptance remain
pending.** The prior packet below supplies requirements, not an instruction to
reset to its older pin, repeat host migration or restart R1–R4 from scratch.

**Goal:** make the actual candidate reviewable, close missing tests of production
viewer wiring/presentation/evidence failures, and give the two unexplained test
failures an evidence-based disposition. Do not start UI.2, new gameplay, renderer
optimization, coordinator repair or another large performance campaign.

#### Read order and working boundaries

1. Fetch/read current `origin/main` `AGENTS.md`, `CLAUDE.md`, PLAN §§0.1–0.4,
   `docs/testing/{evidence-contract,README}.md`, this packet and both the linked
   handoff assessment and `reviews/2026-11-01-w4-ee.md`. Do not merge main into
   the task branch or switch workflows in the middle of a run.
2. Read the task branch's final Windows report and independent review records,
   `docs/app/{game-shell,gpu-handoff,window-trace}.md`, and relevant performance
   tables. Compare every claimed final result with its actual source/tool/binary.
3. Inspect `src/app/src/viewer.cpp` and its real title/script/frame consumers,
   `dev_feedback`, `dev_script`, `command_outcome`, `play_follow`, the code
   containing `ChangeVisibility`, and their tests. Read the changed picture-worker
   tests/reclaimer waits and all five changed `drain()` helpers with callers.
4. Inspect `tools/bench/window_table.py`, `window_table_test.sh`, the native
   Windows launch/capture wrapper and its contracts, `picture_matrix_test.sh`,
   and all files in the post-`f8bc0a6` tool/report diff. Locate new filenames with
   `git diff --name-only`/`rg`; do not infer the last tool's scope from its title.

Keep the Mac MAP lane untouched. Do not cherry-pick its active navigation work.
Record current CPU/RAM/disk/GPU/display/tool versions and host contention only
where needed to interpret new runs; reuse the valid intake. Keep admission and
GPU serialization on. Do not close the owner's game or other applications to
manufacture a clean result; choose an uncontended window or report the load.
Keep the screen awake throughout owned jobs, and preserve other awake assertions.

#### C0 — publish the frozen candidate and preserve review evidence first

1. Confirm branch, clean/dirty state and full commit hashes. Preserve newer work
   rather than resetting to `9ba01f1`. Record the ordered commits and changed
   paths since `a29b079`, including the exact final tool fix. Do not manufacture
   a new code-head label from the last report commit.
2. **Explicit publication authorization:** fetch origin and confirm the remote
   task branch is an ancestor of the committed candidate. Push that candidate
   normally to `origin/task/W4-3-live-buildings` for review and verify the remote
   full hash. No force push, main merge/push, rebase or deletion. If the remote
   advanced/diverged, preserve both sides and report; do not overwrite another
   writer. Do not wait for source approval before this review-only publication.
3. Preserve existing evidence under immutable run IDs, including the timeout,
   stalled-reclaimer failure, invalid controls and superseding runs. Add a
   compact text-only review index under
   `docs/orchestration/evidence/w4-win-close/` with source/configuration/adapter/
   binary/tool hashes, commands, test inventories, counts, paths and available
   failure extracts. No original assets, binaries, credentials or entire cache
   dump in git. Mark unavailable/deleted evidence explicitly.
4. Prepare a source-free evidence archive with the full logs, raw trace inputs,
   inventories and review records needed to reproduce the claimed tables; include
   a checksum manifest. Make it available through an existing authorized shared
   artifact channel if one exists. Otherwise retain it and report its path/size/
   hash and which evidence remains inaccessible to the orchestrator. Do not call
   a Windows-local path a shared artifact or silently claim independent verification.
5. After recording/publishing the checkpoint, continue C1–C4 without another
   owner confirmation. Source publication does not certify the repairs.

#### C1 — test R1 and R4 through their actual viewer consumers

1. R1: hold the real publication interval between recording a paid-build outcome
   and publishing its resource snapshot. Exercise the title and scripted consumer
   with that interval held. The earlier snapshot must not show the newer Applied
   receipt beside old resources; after release, the same receipt and paid resources
   must agree. Include refusal/no-op/lost controls as relevant. The window must
   remain responsive; no spin-until-published workaround.
2. The regression must reach the viewer's real read/watermark wiring, or a shared
   adapter whose use by that viewer is itself exercised. Mutate the production
   call site back to the old read/watermark behavior and show the named regression
   fails for the mismatched state. Calling a correct helper directly is insufficient.
3. R4: drive the actual viewer update path with the tool disabled, an unrelated
   island delta, a relevant local build/demolish and tool re-enable/resync. Observe
   bounded update/enumeration counters and correct occupancy feedback. Grow valid
   unrelated content separately from local occupancy; use work counts rather than
   a fragile microsecond cutoff as the regression assertion.
4. Revert the relevant production R4 call site/enable gate in an isolated owned
   mutant build and require failure. Confirm a valid local update still occurs;
   a viewer that never updates occupancy must fail too. Preserve R3's huge-invalid
   and valid-content controls; no new allocation campaign is necessary.
5. Every mutation record names source diff, rebuilt target, executable SHA-256,
   exact selected test and expected failing assertion. An unmutated binary is an
   invalid run. Restore source, rebuild the affected target and verify the valid
   control. Obtain independent read-only review of code, caller and proof together.

#### C2 — close R2's asynchronous presentation boundaries in a native window

1. Use the actual `ChangeVisibility` consumer with tracing off as well as on.
   Observe the same command/island/chunk-generation/frame/surface epoch at each
   transition. Keep an Applied edit pending while object-list work is committed
   but required CPU chunks are held, then while upload/replacement is held, and
   while a visible dissolve has not completed. Input and sim outcomes must progress.
2. Use existing limits/barriers or a small reviewed test hook to control those
   boundaries. A fixed sleep or simply skipping `frame_done` does not establish
   which boundary was held. In the real window, demonstrate the held upload/
   replacement case and dissolve case, then release them and require a successful
   matching present before `Visible`/`wait drawn` succeeds.
3. Exercise one failed/skipped present through the production consumer seam and
   one native minimized/obscured/unavailable-surface transition where supported.
   Record what the backend actually does; an obscured window may still present
   successfully. Do not equate obscuration with API failure or physical visibility.
   Pan/resize to a new surface epoch, reveal an offscreen change and confirm a
   prior frame cannot incorrectly satisfy the current visibility condition.
4. Retain a non-empty changed-region CPU/fresh/readback positive comparison and
   a negative control that promotes object-list completion prematurely. Check
   no-visual-change/offscreen/out-of-view/nothing-to-draw cases separately from
   `Visible`; no false success or indefinite wait from a mislabeled terminal state.
5. Use focused RTX window cases, not a new 13-case campaign. Run a Radeon canary
   only if needed to validate a changed shared GPU path; the full Radeon window
   matrix stays a named follow-up. Independent reviewer checks the actual viewer
   feed and why the held controls discriminate, not merely timeout exit codes.

#### C3 — explain the two failures with a finite investigation

1. Inventory the original whole-app WSL sanitizer timeout and the one stalled-
   reclaimer assertion by stable name, assertion, source/binary, command, timeout,
   admission state, host load and last observed progress. Preserve originals.
   EA FC27 running at the time does not by itself attribute either failure.
2. Inspect the two picture-worker tests and five drain-helper changes for the
   exact starvation mechanism and correctness of their synchronization. A test
   should wait for the event it asserts, release its own barriers on every exit,
   and retain a watchdog. It must not change production scheduling or drop the
   assertion merely to obtain a pass. Reproduce the necessary ordering with
   controlled gates rather than 24 unbounded busy processes.
3. For the reclaimer case, add observable lifecycle/counter state and a small
   fixed repeat set (at most ten focused attempts unless a new hypothesis
   justifies more). Obtain a deterministic regression for an identified defect.
   If it does not reproduce, say so; ten green runs do not explain the old failure.
   The missing terminal-only reproduction cannot be recovered by rewriting a log.
4. For the slow sanitizer case, inspect its changed dependencies and useful
   progress first. Make at most one instrumented isolated diagnostic run initially,
   with captured progress and owned-process stack/wait state near the deadline.
   Compare a baseline only if it can distinguish a new regression from existing
   work cost. Do not rerun the entire app gate or blindly extend its timeout.
5. If a scoped defect is found, fix it and run the discriminating case plus
   affected dependents. If an unchanged expensive test or external contention
   is established, document the evidence, a justified test-specific policy and
   separate follow-up. An unexplained failure remains OPEN; do not waive it on
   your own. End the finite investigation with cause/fix/proof or an exact
   unresolved hypothesis and the one next observation needed, not a green-only retry.

#### C4 — final tools, provenance and one bounded acceptance pass

1. Obtain read-only review of every change after the prior `f8bc0a6` audit,
   including `9ba01f1`'s report correction and the last tool fix. Reconcile
   selected/executed/failed/empty/skipped/incomplete counts from raw outcomes,
   and map each retained earlier gate to final changed dependencies. The MSVC,
   RTX and Radeon rows currently name different source revisions; keep that fact.
2. Add public-command contract cases for watchdog termination, unreadable peak
   memory and capture refusal, each with a successful control. Assert the declared
   non-success/incomplete behavior, absent success artifact, preserved child exit
   status and useful diagnostic. Use tiny fixtures/subprocesses, not a long render.
   A missing required memory measurement is not zero or a successful measured run.
   Test the native launcher behavior on Windows; run UNIX-only shell contracts
   on matching WSL source as already permitted. Do not invent native passes.
3. Re-aggregate saved raw traces using the final tool and validate the actual
   fields behind the reported 500 fps and zero drops. Distinguish frame-loop/
   present-return cadence, trace loss, API failures and physical display behavior.
   Retain the oversized 3840×2160 drawable label and its valid pixel-workload
   evidence; no native-4K or scanout claim. If raw inputs are invalid, rerun only
   affected cases after the cheap contract checks pass.
4. Freeze source and tools. Run the new consumer/visibility/tool contracts first,
   then one real-window/replay canary. Use the canonical runner for affected
   MSVC cases and focused WSL ASan/UBSan; rerun warning checks on changed C++ scope.
   Reuse expensive unaffected gates only with the recorded dependency rationale.
   A tool-only correction normally requires re-reading retained data, not rerunning
   GPU workloads. Keep all failed/invalid attempts linked to their replacements.
5. A fresh final integration reviewer checks C1–C4, final viewer wiring and
   evidence validity. It does not author fixes, delegate its review or duplicate
   broad gates. Correct actionable findings and re-review only their changes and
   affected interactions. Append the final acceptance matrix and exact code/tool/
   report heads to `REPORTS.md`, preserving unresolved items and scope limits.
6. Publish the completed committed closeout to the same review task branch using
   an ordinary non-force push and verify its remote hash. This packet authorizes
   these review publications only; **no merge or push to main**. Preserve referenced
   logs/build identities before cleaning up owned temporary resources.

**Stopping criteria:** stop for orchestrator review when source is published,
R1/R4 consumer regressions and R2 boundary controls discriminate, final tools and
affected gates are reviewed, and both failure investigations have an explicit
disposition. If either failure remains unexplained or a critical control still
fails, report **CHANGES REQUIRED / OPEN**, with the finite work completed and
precise remaining blocker; do not report acceptance or loop indefinitely.
The source/evidence must still be published/preserved for review.

Native 4K display/scanout, full Radeon window coverage, physical display latency,
late-game city/economy/multiplayer scaling, Metal timing, OQ-525, terrain/height,
roads and final UI remain separately named follow-ups. This packet does not waive
them or require them to block independently reviewed MAP gameplay. Preserve
original assets/painter order, configurable content and asynchronous bounded
rendering; no simulation mutation from the viewer.

### Prior Windows packet — requirements retained, do not restart

**Original rendering handoff pin:** `task/W4-3-live-buildings` at
`a29b07949201e3ea5bf30c800e392f337c1ec598` (round ee documentation), code
`093559ce63331770ae67b1abacacd8fc5fcc4155`. The complete source is published on
GitHub on that task branch for this migration. Main carries the instructions and
review, not a merge of rendering implementation. Read the
[round-ee review and four findings](reviews/2026-11-01-w4-ee.md).
**CHANGES REQUIRED**, not accepted W4.4. Round ed at `3483d66` remains accepted
for its prior local scope; the earlier `7d5cf21` WIP observation and `c869fb7`
remote head are superseded and must not be used as the new test baseline.

The Mac rendering executor has stopped at this clean pin. Windows now owns
further rendering implementation; do not run a second Mac implementation lane.
Verify that the fetched remote task branch contains the pin. For an older clean
owned checkout, verify its current HEAD is an ancestor of the pinned target before
fast-forwarding to that target. If local HEAD is newer than the pin or diverged,
preserve its commits and report both refs before changing it. No reset, force push,
prune or main merge. Mac builds and ignored evidence are not copied.

### Rendering read order and common delivery

1. Repository `AGENTS.md` if present, `CLAUDE.md`, `docs/PLAN.md` §§0.1–0.4,
   `docs/testing/evidence-contract.md`, and `docs/testing/README.md`.
2. This current dispatch; W4.4's **round-dr amendment** in the historical task
   entry below; `docs/app/game-shell.md`, `docs/app/gpu-handoff.md`,
   `docs/app/window-trace.md`, and the latest W4 report in `REPORTS.md`.
3. On the released rendering revision, inspect `src/app/include/anno/app/play.hpp`,
   `dev_input.hpp` and `dev_feedback.hpp` where present; their implementations;
   `src/app/src/viewer.cpp`; `src/render/include/anno/render/picking.hpp`; the
   `command_outcome`, `dev_session`, `dev_script`, `play_follow` and
   `scene_picture` interfaces/implementations; the input, feedback, picking,
   preview and picture GPU tests; and
   `tools/bench/window_{case,matrix,table}.*` plus their contracts.

Keep a clean main checkout for the latest board and canonical gate. An executor
with an older task checkout can read the fetched board using
`git show origin/main:docs/orchestration/TASKS.md`; no main merge is needed to read
instructions. Use the canonical Python gate with explicit `--source-root` and
`--build-dir`, recording both workflow and tested-source revisions. Resolve
paths on Windows; the Mac's absolute paths and ignored `cache/` files are not
portable prerequisites. W4-specific docs/code may exist only on the rendering
branch; read them at the released revision when available. Their absence from an
older main checkout does not block host intake. Do not operate in another
executor's dirty checkout.

### W4-WIN-0 — host intake and migration preparation · READY

- Record CPU/model/core counts, RAM, GPU(s)/VRAM/driver/backend, Windows version,
  free disk on source/build/evidence volumes, display resolutions/scaling/refresh,
  MSVC/CMake/Python versions and WSL distributions/toolchains. Confirm original
  game-file availability without changing or redistributing them.
- Inspect active jobs and available memory. Choose build/test concurrency from
  this host, not the Mac calibration. Keep admission control and GPU resource
  serialization enabled; do not replace a coordinator or delete its journal.
- Locate a suitable owned checkout and verify `origin` is the GitHub repository.
  Fetch/read the current board. Do not clean, reset or prune copied worktrees.
- Prepare an intake report with capabilities, missing prerequisites and exact
  remote heads. A missing 4K display or unavailable Radeon/WSL is a specific
  limitation, not permission to label an equivalent test passed.
- **Code work is released for this packet:** verify the published pin above, then
  complete W4-WIN-1's fresh build and cheap canaries before R1–R4. The review's
  fixes may be implemented on Windows; no additional owner approval is needed.
  A missing external prerequisite blocks only dependent checks. Continue useful
  independent work and report NOT_RUN with the exact cause.

### Round-ee W4.4 scope — preserve completed work while repairing the findings

These packages describe the delivered feature; do not restart them:

- **A — command outcomes:** correlate submission, turn/command position and the
  authoritative result; bound retention with explicit loss; do not infer
  acceptance from submission, missing refusal or picture publication. Cover mixed
  outcomes, identical commands, delayed consumers, overflow, unrepresentable
  commands and outcomes with no building batch. Preserve ordering and state hashes.
- **B — real input:** reuse the existing developer tool and picking helpers;
  handle window/drawable/view/island coordinates, DPI, zoom, height, focus loss,
  cancellation and safe numeric bounds. Test independent known coordinates as
  well as round trips, through the adapter used by the live window.
- **C — preview and feedback:** actual selected assets/rotation/footprint, a
  truthful distinction between geometric preview and authoritative acceptance,
  command-specific refusals and current resources. No window access to `World`.
  Complete place/rotate/demolish/road drag, with explicit bounded long-drag behavior.
  Developer resource overrides stay configurable and off for original scenarios.
- **D — flexibility/scaling:** registry-driven IDs, costs, footprints and assets;
  test a different non-square definition and non-default player/city. Reject
  unsupported definitions safely. Avoid whole-world copies/scans per pointer move
  and speculative plugin frameworks. UI.2 must be able to reuse these interfaces.
- **E — integration:** lifetime-safe remaining test observers; diagnosed contention
  check; actual input → command → replay checkpoints; input/outcomes continue while
  render work is held; inspect screenshots of preview, placement and refusal.

Each package and final integration require independent read-only review, corrected
findings and retained dispositions. The round-dr amendment keeps this a minimal
developer tool, not the final UI or a new bindings-design exercise.

### W4-WIN-R — four bounded round-ee repairs · READY after the fresh-build canaries

Read the linked review first. Implement one concern per commit, writing each
small regression before its fix. Use the actual window consumer or its shared
adapter: testing a helper with prearranged inputs does not prove the wiring.

**R1 — coherent outcome/resource feedback.**

1. Trace `PlayRunner::publish`, `OutcomeLog`, `PlaySnapshot::outcomes_next`,
   `Viewer::update_title` and scripted reads. State the legal interleaving in
   which an outcome exists before its resource snapshot.
2. Add a deterministic barrier case with a paid build: the early outcome must
   not be paired with pre-charge resources; after publication the same receipt
   and updated money/goods must agree. Include refusal, no-op and lost outcomes.
3. Implement a bounded consistent read or defer the outcome using a publication
   watermark. Do not spin/retry indefinitely or block the window until a turn ends.
   Review lock ordering and preserve command/replay/state semantics.
4. Independently review the interleaving, consumer wiring and negative control;
   correct findings before marking R1 complete.

**R2 — truthful render readiness for feedback and scripted waits.**

1. Separate accepted, desired state, object-list committed, CPU chunks fresh,
   GPU replacement usable and successfully presented. Reuse `window_trace` and
   the current chunk serial/generation/surface boundaries where practical; keep
   the behavior correct when tracing is disabled.
2. Add cases that commit the object list while chunk workers remain held, then
   hold uploads/dissolve, and then fail a present. `wait drawn` must not succeed
   and the title must not imply visibility at these earlier boundaries.
3. Correlate the command's actual affected visible content with the presented
   frame. Handle coalesced/superseded edits, no-op commands, offscreen edits,
   pan/zoom/resize and unsupported visuals explicitly; neither hang forever nor
   label offscreen work visible. Represent pending state without growing an
   unbounded per-command render history or waiting from the input path.
4. Prove the positive case with a fresh reference/readback for a non-empty changed
   region, and a negative control that prematurely accepts object-list completion.
   Update the barrier test's claim, script contract and documentation. Independent
   review must inspect production readiness use, not just a generated note string.

**R3 — bounded footprint validation.**

1. Check rotated rectangle extent with widened/checked arithmetic before enumerating
   tiles. Cover a zero/unknown definition, a much-too-wide/tall definition,
   coordinate boundaries, rotation and a legitimate larger definition.
2. Remove temporary whole-footprint allocation from the feedback query. Bound work
   by the admitted region; preserve data-driven dimensions, not a hardcoded original
   building-size cap. Review the adjacent occupancy indexing arithmetic as well.
3. Use an allocation/work guard to show huge invalid dimensions are refused before
   enumeration. Do not exhaust host memory as a test. Keep the valid control so an
   implementation that always refuses fails. Obtain independent review.

**R4 — isolate occupancy work from unrelated content and disabled input.**

1. Reproduce the global-version path: hold the selected city's content fixed, edit
   another island, and observe the current complete city-index rebuild. Also cover
   the developer tool disabled in an ordinary play window.
2. Use existing island-local deltas/versions or bounded immutable preparation to
   avoid a full world scan and full city-set reconstruction on each frame/update.
   Do not relocate that scan to another window callback. Preserve the published-data
   boundary: the window still must not read the mutable World.
3. Verify local additions, demolition, overlap/refusal feedback, resync/coalescing,
   tool re-enable and missing registry entries. An unrelated edit must not rebuild
   or enumerate unchanged city occupancy; same-island changes must still update it.
4. Measure bounded Release costs at growing valid local city sizes and unrelated
   world sizes, separately from pointer-move microbenchmarks. State what is synthetic
   and what is actually simulated. Review the algorithm and invariants, not just
   whether a fast 4090 workstation beats a timing threshold.

After each repair's independent review and focused checks pass, continue the
remaining Windows packet without another orchestrator pause. If a repair needs
an unresolved gameplay rule, stop only that dependent change and describe the
boundary; do not invent terrain, height, roads or founding behavior.

### W4-WIN-1–6 — Windows/RTX verification packet · READY

Order: host intake (0), fresh build and cheap canaries (1), repairs R1–R4, then
acceptance 2–6 on the corrected revision. Preserve valid Mac evidence at its
actual source/configuration and retain failed attempts. No initial broad matrix
is needed merely to reconfirm the reviewed defects.

1. **Fresh target build and preflight.** Configure/build the owned Windows source
   with the `msvc` presets and Release configuration. Never reuse Mac objects or
   CMake caches. Run the canonical test-name/verdict preflights, inventory the
   exact tests and required contracts, and hash the tested binaries. Run a cheap
   app/input/trace check and one RTX GPU canary before broad suites. Repair scoped
   portability defects with focused regressions and review; no coordinator redesign.
2. **MSVC app/render acceptance.** Run relevant app, allocation, input, picking,
   preview and CPU render tests with original assets set. Use the canonical Python
   runner with `--config Release`, explicit roots, retained logs and inventory.
   Inventory selectors before execution; missing or empty selections fail.
   On supported UNIX/WSL builds require `picture_matrix_contract` and
   `window_table_contract`. If those shell contracts are not registered on native
   Windows, execute them on the matching WSL source and report the boundary;
   do not fake registration or count native absence as a pass.
3. **RTX 4090 GPU acceptance.** Identify the actual adapter and backend. Execute
   `play: GPU matches the CPU path on one scene`, picture replacement/ownership,
   invalidation, stale completions, constrained upload/stand-in/dissolve cases and
   the new preview cases. Add the pending continuous-zoom preview comparison
   against the CPU composition/resampling reference, with changed pixels and an
   intentionally wrong overlay control; do not call the stepped group-1 test proof
   of this path. Then run the inventoried applicable GPU suite, including
   timing checks. An RTX-required case skipped on the RTX host is not acceptance.
   Repeat the corrected stand-ins case with controlled CPU contention; keep GPU
   work serialized, record STALLED/WATCHDOG diagnostics and investigate before a
   retry. Run applicable tests on the Radeon if available with its own identity;
   an NVIDIA-specific skip is not Radeon parity proof.
4. **Real Windows window.** First one-edit, resize and trace on/off canaries. Run
   the developer-tool session with real sprites, current-receipt outcomes and
   headless replay; preserve commands/hashes/outcomes and a failing altered-command
   control. The round-ee screenshot action is macOS-only: use a reviewed native
   capture path or explicit manual capture of the game window; NOT_RUN is not a
   screenshot. Capture title feedback separately if the client image excludes it.
   Then run the existing 13-case matrix at 1080p and actual 3840×2160 if supported. Record
   drawable pixels, refresh/cadence, trace completeness, command outcomes, per-stage
   edit latency, p50/p95/p99/max with sample counts, queues and memory. The existing
   `window_case.sh` uses UNIX `time`/`perl` and is not a native Windows launcher:
   use the native viewer trace plus a small reviewed Windows launch wrapper if
   necessary, preserving plan/results/raw-event validation. Do not run a WSLg
   window and label it native Windows. Re-aggregate raw traces; no stale summaries,
   fabricated 4K dimensions, or present-return-as-display-latency claims.
5. **WSL and fidelity follow-ups.** Run GCC 13/Clang 18 `-Werror` checks using
   `tools/check_unix_warnings.sh`, with actual tool versions and omissions stated;
   run relevant sanitizer/contract tests on a correctly configured supported build.
   Reproduce OQ-525 against the CPU/fresh/original references appropriate to each
   claim; retain existing tolerances. Record Windows timing results separately:
   they cannot close the two Metal-specific failures. Original-runtime captures
   require the documented harness and their own provenance, not GPU/CPU equality.
6. **Closeout.** Correct demonstrated in-scope defects, obtain per-package and final
   independent review, and run final affected gates after the final code changes.
   Reuse unaffected expensive gates only with an explicit dependency rationale;
   do not relabel e4c726b/269f795 Mac gates as tests of 093559c or a Windows revision. Report
   selected/executed/failed/empty/skipped/incomplete separately, preserve first
   failures, and pin source/config/adapter/binary/log identities. State remaining
   platform/visual/performance limits and stop for orchestrator review. No open-ended
   optimization, full UI, founding, map selection or network implementation is added.

**Performance and mod-support constraints:** preserve original assets, painter
order and faithful rules. Rendering must stay asynchronous and memory-bounded;
commands never wait for builds/uploads/reclamation/presentation. A 60-building
canary or the 4090's speed does not establish large-city/complex-economy multiplayer
scaling or the PLAN's four-core target. Use the existing verified larger workload
for a bounded target-host baseline where available, report its actual populations
and active work, and leave unrepresented workloads open. Configuration/content
changes that affect authoritative behavior remain deterministic and identifiable.

**Isolation and stop:** MAP remains on the Mac; do not cherry-pick its active
navigation or separate founding changes, change active map/candidates or original
game files, stop others' jobs/keep-awake assertions, or merge/rebase/push task code without a release
instruction. Write executor results to `REPORTS.md`; the orchestrator updates and
publishes this board. Preserve the owner's current keep-awake setup.

### MAP-POP-CLOSE-2 — the Inn supply path and the first real upgrade · READY, RELEASED

**Owner and starting point:** the Mac executor, in the existing `task/MAP-2-original-like` worktree, from local clean
head `1e65ea7d` (gate source `719f394a`). Preserve any later owned work and report its identity; do not reset.
- Read the [CLOSE-1 review](reviews/2026-11-04-map-pop-close1.md) first.
- The accepted predecessor stays `73e7ab1a129c4712747333d9f34a225be03f063f`.
- This continues CLOSE-1. Its C0–C3 are not repeated, and its operating boundaries (its read-order items 1–6) still
  apply.

**Deliverable:** CLOSE-1's C4 loop, completed. In an explicitly enabled headless run, on an original town that stocks
the settler goods:
- **Build and supply.** A fresh pioneer house is built through `BuildCmd`. Real resident jobs supply it from stalls,
  from a chapel, and from an Inn whose own carrier (job `0x33`) fetches alcohol.
- **Progress.** The house grows, then upgrades to settlers with an exact material debit, and a stage unlock reaches the
  player's mask.
- **Prove.** Add the refusal, lifecycle, reload and replay checks.

Return once, after K0–K4. Independent package approvals unlock the next package. `house_updates` stays false by default;
this is not live-window gameplay.

**Decisions and reasoning (orchestrator, 2026-11-04):**
1. **Stream 128 and PRNG timeline 5 stay amended in place.**
   - **Why:** they were never accepted or published. The published MAP branch is stream 127, timeline 4, at `73e7ab1a`.
   - **What to do:** add the Inn state under 128/5, again in place.
   - **Conditions:** no artifact in a superseded 128 layout is used as a test or replay input; `tools/sim_stream.py`
     describes the final layout; the report lists each amendment. Acceptance freezes the identity.
2. **No production chain and no fixture stock.**
   - **Why:** 28 original towns already hold the goods (review §5), and the Inn's carrier is the existing workshop
     carrier (review R1).
   - **Out of scope:** potato farm, distillery, brewery, tannery, weaver and salt works.
3. **`endless03` player 4 is not the progression town.** Settlers force leather, cloth and salt, and it stocks none
   (review R2). Its growth run stays as evidence.
4. **The CLOSE-1 P3s stay accepted, except the route exemption.** The code check found it wider than "leaving the
   stand" (review §6, P3 1), so K2 narrows it and adds its tests.

**Scope:**
- **In:** the Inn's state, tick, take, unload and persistence; reuse of job `0x33`; checks of the chapel and stall supply
  (and narrow fixes only where a path is missing); the town choice; the upgrade loop; docs.
- **Out:** production chains, taxes, upkeep, UI, renderer, network, the default flip, Windows code, the coordinator and
  sailing.
- **Data:** goods, groups and roles stay data-defined. No shipped id is special-cased in code or canaries.

#### Read order

1. Fetch GitHub. Read in `origin/main`:
   - this section;
   - the CLOSE-1 review;
   - CLOSE-1's section below;
   - `docs/testing/evidence-contract.md` and `docs/testing/README.md`.
   Use `git show`; do not merge main into MAP.
2. Your CLOSE-1 ledger and REPORTS entry; `docs/re/population.md` §§12–14; `docs/re/transport.md` §2 and §4.7;
   `docs/re/production.md` §4.1, the workshop carrier's source filter and callback.
3. The workshop carrier's implementation (job `0x33`, `send_figure_job`, the S11 driver) and its tests, before writing
   any Inn code.
4. The original files at `/Users/dom/Dev/anno1503-worktrees/anno1503/game/app`; check the module hashes against the
   review. Use `tools/disasm.py` with the existing private environment.

#### K0 — cheap checks and the town choice (no long runs)

1. **Re-read every address in review §4 independently.** Record widths and units. Also record:
   - what the two calls after the send set: figure `+0x84(0)`, and figure slot `+0` with `type+0xb0(0)`;
   - what `type+0xb0(0)`, `type+0xb4(0,0)` and `type+0xb8(0)` return for the shipped WIRT type;
   - in which unit the carrier unloads at the Inn's `+0xc0`, which has no `·16`.
2. **Check whether locked needs are fetched.** Read demand (`0x100765c0`) and job `0x35`'s target choice: is a need
   fetched while its `BauFirst` stage is not yet in the player's mask? WIRTSHAUS is stage `1B` (80 pioneers), KAPELLE
   `1C` (125). The answer decides which towns qualify.
3. **Choose the town.** Start from review §5, or take any other original town that meets the criteria below. Load each
   candidate once. The town needs:
   - **stock:** NAHRUNG, LEDER, STOFFE, SALZ, ALKOHOL and the settler house's materials in store;
   - **suppliers:** a market hall, stands that sell food, salt, cloth and leather, a chapel and an Inn, or free land to
     build the missing ones with `BuildCmd`;
   - **land and reach:** free land for one new pioneer house within reach of all of them, and the Inn within carrier
     reach of the market hall;
   - **stages:** the stages of item 2 available;
   - **a new stage:** a threshold that this house's growth or upgrade crosses, whose bit the player's mask does not hold
     yet.
   Record the chosen town's inputs in the report before the long run. If no town has a new stage to unlock, say so and
   name the stage the loop changes instead.
4. **Explain the first delivery.** Split the ~35,000 ticks to the first delivery on `endless03` into their terms: spawn
   gate, route, walk, stall wait, return. Give each term's address. The wait is already cited as D (review R3). Any
   term that is not read goes to an open question; never state it as original behavior.
5. **Cross-check the Inn count.** Check the report's "58 city rows with an Inn" against PRODTYPE 66 (review R5).
6. **Review.** An independent read-only reviewer checks items 1–4 against the binaries. Then continue to K1.

#### K1 — the Inn

1. **State:** `struct Inn` at `1e65ea7d` already holds the tavern stock and the taken statistic (widened from u16),
   with load, save, take and unload. Check them against review §4, including the 10-byte class record (`[2]` stock,
   `[8]` demand; shipped Inns load stock 0). Add what the tick needs:
   - the gate's `last`;
   - the active carriers: the original has two slots at `+0x1c`/`+0x20`, and the limit comes from the type
     (`+0xb8(0)`). Keep that limit data.
2. **Tick**, at its 5000-tick gate:
   1. demand decays by 31/32, truncating;
   2. stop when the Inn is switched off (flags bits 4/5);
   3. stop at the carrier limit;
   4. stop when stock ≥ `0x100` and stock ≥ demand / 2;
   5. otherwise send job `0x33` with the type's figure type and the goods found in K0 item 1.
3. **Carrier:** reuse the existing workshop carrier for search, filter, pickup, routes and unload.
   - If the Inn needs something the workshop path lacks, add it as data or as a class property, not as an Inn special
     case.
   - The pickup reduces the city store (or a producer's output); serving a resident does not.
4. **Take and unload.**
   - **Take** at a resident's visit: `n = min(request, stock)`; demand `+= n`; stock `−= n`; then bill alcohol with
     take 0. The resident's goods fill the house's WIRTSHAUS slot through the existing ALKOHOL→WIRTSHAUS lookup alias.
   - **Unload:** stock `+= amount`.
5. **Tests.** Derive each expected value by hand and check it against an independent reference: extend
   `tools/resident_ref.py`, or add a small `inn_ref.py` with its own CLI contract. Cover:
   - the gate at 4999 and 5000, and the u16 wrap;
   - the demand decay;
   - each stop condition, and the boundaries at stock `0xff`/`0x100` and at demand / 2;
   - the carrier limit;
   - a take with a request above, equal to and below the stock;
   - an unload;
   - save/reload with stock above 0, and with a carrier on its way;
   - a non-original Inn type id and goods id.
   Each wrong-behavior mutant must reach its intended assertion while the legal control passes.
6. **Regressions:** the workshop carriers and the forestry/economy canaries keep their results (448 raw units, 28/28
   checkpoints) unless a change is justified and recorded. Windows compares these values in W4-OPENING-2.
7. Package review, fixes, then continue.

#### K2 — chapel, stalls and route endpoints

1. **Test both supply paths:**
   - a resident's visit to a chapel credits the house's KAPELLE slot (PublicService, take 0);
   - stalls credit LEDER, STOFFE and SALZ from the store (take 1).
   If either path is missing, implement it narrowly with primary evidence (`population.md` §14, OQ-102 Q3).
2. **Narrow the route exemption and test it.** The exempt first step also skips the side check and the layer
   admission of `route[1]`, the ordinary tile outside the stand (`walk_search.cpp:421–425`; review §6, P3 1). Skip only
   what leaving the stand needs; keep both checks for `route[1]`.
   - Add the first-step test from CLOSE-1's accepted P3: a first step from a stall into a wall.
   - Add a tile next to a stand that refuses entry from the stand's side.
   - Add the Inn and the chapel as further endpoints.
   Do not change shared cart or harvester routing without running their regressions.
3. **Close two test gaps** (review §6, P3s 2 and 3): check every claimed floor clip against `before + VermehrTab` and
   its floor, and test the billing bound exactly at `u32` max.
4. Package review, then continue.

#### K3 — the real upgrade

1. **Build and run.** In the K0 town, build the house (and any missing supplier) with `BuildCmd`, then advance time.
   Nothing is injected after the start.
2. **Assert CLOSE-1 C4 item 2 in full:**
   - before and after: type, group, population, tile identity and city counts;
   - the exact material debit and the spent counters;
   - the draws, including a variant draw if the type has variants;
   - the city stage mask and a **changed player mask**.
3. **Check the deciding tick.** At the tick the check passes, compare the engine's food, goods and infra values and their
   table indices with the independent reference.
4. **Controls:**
   - **materials short:** ordinary `BuildCmd`s spend the materials first. Expect no upgrade, no spend, no draw and no
     type change;
   - **no Inn service:** no Inn, or the Inn switched off if the original's flag path is reachable. Expect infra below
     84 and no upgrade;
   - **no build:** the stage bit does not change;
   - keep the decline/removal control.
5. **Ledger:** trace alcohol from the store to the carrier, the Inn, the resident and the house. Trace leather, cloth and
   salt from the store to the stall and the house. Every unexplained term must be zero.
6. **Persistence:**
   - save and reload with the carrier on its way, with Inn stock above 0, with a resident carrying tavern goods, and
     just before and after the upgrade and the unlock;
   - a fresh-process replay with exact checkpoints, in which an altered relevant command diverges for the intended
     reason;
   - the replay binary built and hashed separately.
7. **If it still does not upgrade,** stop as PARTIAL. Report:
   - the engine's check values at the best tick: the three values, their indices and table entries;
   - the materials result;
   - the exact rule that is not read.
   Do not patch supplies, weaken the check or call a downgrade an upgrade.

#### K4 — gates, docs and one handoff

1. **Gates:**
   - **Runner:** use main's `tools/test/run_selected.py` as in CLOSE-1 C5 items 1–4, with new logs under
     `cache/map2-pop-close2/gates/`.
   - **Selection:** add the Inn, carrier, transport and production suites (job `0x33` is shared) and the progression
     canary.
   - **Reference entries:** keep the nine required ones and add any new Inn reference.
   - **Sanitizers:** if P-15 or this package changed shared route code, the route suites go into the sanitizer run too.
     Name the changed files.
2. **Docs:**
   - an Inn section in `population.md`, with the addresses;
   - the Inn row in the `state.md` class table;
   - OQ-102 Q2 answered for the tavern: it has its own stock, refilled by job `0x33`;
   - the community note "a tavern's alcohol is not saved" corrected for the Inn object: its Save writes stock and demand.
     What happens to the carrier's cargo on save stays open;
   - the ledger rows.
3. **Earlier docs handoffs.** If CLOSE-1 did not do them, finish the MAP docs handoffs of Windows playtest review §7:
   the founding text at `0x10049340`/`0x10049166`, the land model against shore 16.5, and the BEL escapes.
4. **macOS `test_support`:** the check stays conditional on a published `task/W4-main-1`.
5. **Final review:** a fresh read-only integration reviewer. Persist ids, revisions and dispositions.
6. **Report:**
   - pins and counts: selected, pass, fail, empty, skip, incomplete and sanitizer lines;
   - Decisions and reasoning;
   - the final 128/5 layout and its amendments;
   - the Windows API delta.
   Finish clean. Then publish the exact final head once, as a plain fast-forward push of `task/MAP-2-original-like`
   with no force, as for `1e65ea7d`, so that the orchestrator can review the source. No other push, no main merge or
   rebase, and the defaults stay off.

**Stop:** one combined handoff after K0–K4, or PARTIAL with the values of K3 item 7.

### MAP-POP-CLOSE-1 — complete resident supply and prove upward town progression · REVIEWED, PARTIAL; CLOSE-2 ABOVE IS CURRENT

The `1e65ea7d` handoff is reviewed in [2026-11-04-map-pop-close1.md](reviews/2026-11-04-map-pop-close1.md). Its
boundaries below still apply to CLOSE-2; its packages are history.

**Owner and starting point:** Mac executor, existing `task/MAP-2-original-like`
worktree, local clean report head `135edf6f`. Preserve any later owned work and
report its identity; do not reset to that pin. Read the
[direct review and counterexamples](reviews/2026-11-04-map-pop-live1.md).
Accepted predecessor remains `73e7ab1a129c4712747333d9f34a225be03f063f`.
This is the continuation of LIVE-1, not a restart of L0–L5.

**Deliverable:** in an explicitly enabled headless scenario, build a correctly
initialized House, supply it through actual resident jobs, obtain positive
resident growth, upgrade it with exact material payment, and propagate a stage
unlock to the player. Include the corresponding refusal, lifecycle and
reload/replay checks. Complete all six packages below before returning once.
Internal independent approvals authorize continuation; no extra orchestrator
approval is needed between C0–C5. The global `house_updates` default stays false
until the final acceptance decision. Do not call that default live gameplay.

#### Read order and operating boundaries

1. Fetch GitHub and read `origin/main:docs/orchestration/TASKS.md`, its linked
   review, `AGENTS.md`, `CLAUDE.md`, PLAN §§0.1–0.7,
   `docs/testing/evidence-contract.md` and `docs/testing/README.md`. Use
   `git show` or main's read-only workflow files; do not merge main into MAP.
2. Read your `docs/sim/population-live.md` in full, its review ledger, the latest
   REPORTS entry, `docs/re/population.md` (especially creation/SetGroup and the
   still-unread caller edges), `docs/sim/population-contract.md`,
   `docs/sim/population-inputs.md`, and the accepted
   population-input review. Reconcile facts, deliberate adaptations and unknowns
   in the existing ledger rather than making another reporting system.
3. Read `src/sim/src/{create,building,scenario,house_tick,house_transition,
   resident,resident_job,scheduler,figure,types,save,replay}.cpp`, their relevant
   headers, `src/sim_load/src/original_registry.cpp`, and the changed callers.
   Follow BuildCmd's real apply/payment/atomic-batch path before wiring creation.
4. Read `src/sim/tests/{house_live,house_transition,resident,resident_job,
   resident_state,population_live_canary,population_scale}_test.cpp`, the existing
   creation/command/state/replay tests, `tools/{house_live_ref,
   house_transition_ref,resident_ref,sim_stream}.py`, their CLI contracts and
   fixtures. Check the code used by an oracle, not just its expected output.
5. Original files are available at
   `/Users/dom/Dev/anno1503-worktrees/anno1503/game/app`. Check module hashes
   against the review. Reuse `tools/disasm.py` and the private environment in
   `cache/map2-founding-evidence/venv`; no whole-home search or tool installation
   is needed. A missing MAP-local `game/app` is not a missing original.
6. Check current CPU/memory/disk/load, cache roots and tool versions; reuse host
   calibration and admission control. Keep the owner's awake assertion running.
   Own only your processes and temporary artifacts; no pattern kills, shared
   coordinator changes, or edits in another executor's checkout. Inspect before
   using a build directory, and serialize expensive lanes on this shared Mac.

**Scope:** population and the narrow dependencies needed for this progression
loop. No taxes, upkeep, new production chains, UI, renderer, network protocol,
map selection, coordinator redesign or general mod framework. Do not alter game
files, candidate placement or the active map. Keep data-defined goods/groups,
roles and supported behavior mappings; never patch a canary with a shipped-id
special case. A legitimate small helper shared by these changes is allowed.

#### C0 — reproduce cheaply and settle the decisive contract edges

1. Write one compact acceptance table mapping F1–F4 and G1–G3 to a legal control,
   distinguishing failure, proposed correction and exact test. Retain all prior
   failures; label LIVE-1 L4/L5 partial and its old gates historical evidence.
2. Reproduce the review's billing reload divergence, stale transition record,
   high-bit reference mismatch, cycle and draw-on-overflow with tiny inputs.
   No 150000-tick run is needed to establish these. For G1, prove the public
   fresh-House path misses first assignment, not merely a helper in isolation.
3. Independently read the decisive primary instructions for the creation call,
   first-assignment draw order, next-type lookup/current-kind predicate,
   SetProperty bit implications and the small wait/timeline callers. Record
   addresses, widths, units and a hand-derived discriminator. Extend the trace
   only along calls needed for this slice; do not repeat all population research.
4. P-14 may protect development while traced, but must distinguish a legitimate
   terminal/special house from an invalid mod transition. A missing/non-adjacent
   group must not disappear behind a silent “not eligible” result. P-15 requires
   a legal directed route and correct entrance semantics, not just fewer lost
   goods. The default-off rule is temporary containment, not acceptance evidence.
5. An independent read-only reviewer checks these bounded derivations and the
   proposed corrections against the actual binaries/callers, using the supplied
   absolute path. Resolve findings and continue to C1. Runtime-only unknowns may
   remain explicitly NOT_RUN if they do not prevent the scoped loop.

#### C1 — deterministic state, safe data and meaningful failure

1. Fix F1. Saved/hashed billed counters remain the default. For the optional
   unsaved mode, either remove its influence on authoritative decisions or
   reject `house_updates=true` with that mode until supported. State the choice
   explicitly. A valid save and its reload must not silently differ in purchase,
   fault, resources or future hashes. Test below-bound and overflow behavior,
   including a correctly checksummed rules/save rejection if that is the design.
2. Fix G2 at registry/live-activation validation and defensively at delivery:
   reject relevant IdSec cycles/self-loops without partial credits. Keep legal
   multi-step chains and different goods sharing a need slot. Adding unrelated
   goods cannot change the result. Replace the old cycle-amplification vector;
   have the reviewer derive its corrected expected result independently.
3. Close G3 before wiring creation: a refused initialization must not consume a
   world draw, charge materials or consume an id. Preflight a supported range or
   prepare a transactional proposal; retain legal first-assignment draw order.
   Test an overflowing mod input and a neighboring accepted input, with the
   complete state/draw cursor unchanged on refusal.
4. Add only the targeted wrong-behavior controls needed for these fixes; each
   must reach the intended assertion with its legal control green. Review the
   frozen package, fix findings, re-review affected paths, then continue.

#### C2 — complete fresh-House creation and required resident service behavior

1. Wire SetGroup first assignment through the actual fresh BuildCmd/creation
   path under the live rule. Pin group-0 initial food and nonzero-group
   residents/E/forced-need stock/draws. Keep explicit imported payloads, exact own
   saves and rule-off behavior separate. Test individual and atomic construction
   where supported; a dry run must not draw or charge a second time.
2. Resolve the human-build timestamp and the scoped stall wait/attach/timeline
   edges from C0. Implement reached effects with persistent partial progress and
   explicit units. Exercise zero elapsed, the smallest positive visit, a split
   delay with surplus, and save/reload. Do not silently substitute a one-tick
   wait for readable original behavior.
3. Inventory which services a real pioneer-to-next-group transition needs.
   Reuse existing suppliers and production state. If an already-scoped Inn or
   public supplier is a required missing connection, finish that narrow supplier
   state/availability/debit path with primary evidence and its independent
   vectors before the canary. Do not manufacture service stock in the canary or
   launch an unrelated full production-chain implementation.
4. Test new homes, in-flight residents across a transition/removal, ownership,
   counter release and a data-defined house/supplier. The package reviewer traces
   one public BuildCmd to first resident delivery, including draws and payment.
   Corrections are internal; continue to C3 after approval.

#### C3 — reliable progression, home routes and observations

1. Fix F2: capture actual committed residents and independently assert the
   growth/cap/floor/transition relationship. Add the 300→284 downgrade control;
   the evidence record must agree with the resulting House. Assert every claimed
   clip against its applicable bound. Separate positive growth from decline.
2. Fix F3 in the Python reference and shared vectors. Check newly set stage bits
   25/27/28 and bit 26, a normal low stage, the low-23-bit comparison gate and a
   non-implication control. A missing bit implication must fail both the intended
   engine comparison and reference vector, not just a duplicated output literal.
3. Finish the P-14 decision within the approved scope: independently trace the
   successor rule and audit the relevant shipped families. Validate malformed
   links distinctly; prove one real legal upgrade and one deliberate terminal
   special family. Preserve type/footprint/walker/accounting invariants.
4. Validate P-15 with a small real terrain/entrance reproducer. Assert adjacent
   directed edges and legal entry into the home, plus disconnected and changed
   route controls. Prove that it cannot walk through forbidden terrain or
   teleport. A scoped resident endpoint adapter is allowed when justified;
   changes to shared cart/harvester routing require their affected regressions.
5. Review the actual dispatcher-to-effects paths and the independent state
   assertions, then continue to the gameplay proof. Do not use diagnostic
   counters as their own expected-state oracle.

#### C4 — one useful real-data progression loop, persistence and bounded scale

1. Use the original registry and a viable original scenario/terrain with its
   available stocks, or normal commands/production that supply it. Inspect the
   predicate inputs cheaply before selecting it. Do not repeatedly search all
   35 scenarios with long runs. Keep the old downgrade/removal canaries as
   complementary evidence, not substitutes for an upgrade.
2. Build a fresh house through the public command path. Advance real resident
   fetching and delivery until it grows and upgrades. Assert before/after House
   type/group/population, tile identity, city counts, exact build-material debit
   and spent counters, draws, city stage mask and a **changed player mask**.
   Set initial masks through normal scenario setup; do not inject a finished
   upgrade, resident stock, unlock or random result after the run starts.
3. Add the nearest materials-short control and show no upgrade spend/draw/type
   change while ordinary ticking may continue. Retain a decline/removal control.
   Account for supplier stock, cargo, delivery and consumption from authoritative
   state; unexplained ledger terms must be zero, not merely printed.
4. Save/reload during wait, carried goods, just before and after the successful
   transition/unlock. Reloaded and uninterrupted continuation must agree. Record
   a fresh-process replay with exact checkpoints and an independently altered
   relevant command/input that diverges for the intended reason. Build/hash the
   replay binary separately; do not assume `anno_sim_tests` built it.
5. Preserve data-defined roles and a non-original-id positive case. Reuse the
   existing unrelated-city operation-count checks and extend only for new work.
   State what remains serial and the measured workload; no late-game multiplayer
   claim follows from a few houses. No new benchmark framework or worker matrix.
6. Independent review checks raw state, first assignment, ledger and the full
   causal sequence. If a genuine dependency prevents the real upgrade, finish
   independent corrections, report the exact missing rule and stop as PARTIAL;
   do not patch supplies, weaken the predicate or call a downgrade an upgrade.

#### C5 — frozen canonical closeout, documentation and one final handoff

1. Restore mutants, rebuild, finish package corrections, and freeze source,
   fixtures, references and all writers. Verify the executable belongs to the
   restored source. Use main's current canonical workflow with explicit paths;
   the old MAP positional wrapper is not this task's acceptance runner.
2. Run one affected Debug and one affected ASan/UBSan selection covering the
   changed creation, command, resident, transition, state, scheduler and replay
   paths, including the real progression canary. Include lint. Run the following
   exact required reference entries independently of the selected C++ names:
   `resident_reference_contract`, `resident_reference_agrees`,
   `house_transition_reference_contract`, `house_transition_reference_agrees`,
   `house_live_reference_contract`, `house_live_reference_agrees`,
   `house_kernel_reference_contract`, `house_kernel_reference_agrees`, and
   `population_oracles_agree`. Reference execution must be nonempty.
3. Command template (adjust the selection to include new exact case names,
   record the chosen per-test timeout before execution, and use a new log path
   on each attempt):

   ```sh
   export PATH="/Users/dom/Dev/anno1503-worktrees/anno1503/cache/mac-tools/venv/bin:$PATH"
   export ANNO1503_GAME_DIR="/Users/dom/Dev/anno1503-worktrees/anno1503/game/app"
   python3 -I /Users/dom/Dev/anno1503-worktrees/anno1503/tools/test/run_selected.py \
     --source-root /Users/dom/Dev/anno1503-worktrees/anno1503-worktrees/anno1503-MAP-2 \
     --build-dir /Users/dom/Dev/anno1503-worktrees/anno1503-worktrees/anno1503-MAP-2/build/mac-headless \
     --target anno_sim_tests \
     --select '^anno_sim_tests\.(house|resident|population|stage|goods|world|save|scheduler|figure|city|check|state|stream|player|draw|invariant|replay)|^anno_sim_tests_lint_sim_(passes|self_test)$' \
     --require-test resident_reference_contract --require-test resident_reference_agrees \
     --require-test house_transition_reference_contract --require-test house_transition_reference_agrees \
     --require-test house_live_reference_contract --require-test house_live_reference_agrees \
     --require-test house_kernel_reference_contract --require-test house_kernel_reference_agrees \
     --require-test population_oracles_agree \
     --asset-policy required --jobs 2 --timeout 1800 \
     --log /Users/dom/Dev/anno1503-worktrees/anno1503-worktrees/anno1503-MAP-2/cache/map2-pop-close1/gates/debug-final-01.log
   ```

   Use the owned `build/mac-asan` and a separate log for sanitizers. The timeout
   above is a starting template, not permission to discard expensive cases;
   justify any change from measured cost. Inspect the immutable inventory before
   calling coverage complete. Add creation/command cases missed by the example
   regex. Required-entry counts stay separate from main selected counts.
4. Inspect `GATE_END`, source/workflow/build/binary/fixture identities and every
   selected outcome. Preserve failed attempts and skips separately. Reuse the
   older broad suites with a dependency rationale; do not rerun sailing, heavy
   rendering, GPU, coordinator or original-runtime campaigns for this packet.
   A substantive postgate fix needs affected verification; a prose-only report
   commit does not require another broad gate.
5. A fresh read-only final integration reviewer checks the combined corrections,
   primary discriminators, actual production sequence, independent assertions,
   references and frozen verdicts. Supply facts/pins rather than instructions to
   approve. Fix P1/P2 findings and obtain focused re-review. Persist reviewer id,
   reviewed revision and disposition immediately; do not reconstruct lost ids.
6. At the docs checkpoint, complete the separate MAP handoffs in Windows playtest
   review §7: reconcile stale `0x10049340` founding text with the existing trace
   and property-5 setter `0x10049166`; distinguish the tool's geometric land model
   from renderer shore 16.5 without changing candidates or sim terrain rules.
   Correct BEL path escapes in a documentation file only when editing that file.
   Preserve Windows' task sections and exact integration inputs.
7. Conditional existing duty: if `task/W4-main-1` has been published, run its
   macOS `test_support` suite at a natural checkpoint in a separate suitable
   validation checkout, with exact source/workflow identity and admission on.
   Never merge it into MAP or reuse another owner's live build. This is validation
   only, not authority to change coordinator semantics. If unpublished or a safe
   checkout is unavailable, report the specific pending dependency and continue
   gameplay; it is not a blocker for C0–C5.
8. Commit the report/ledger with exact code/gate/docs pins, real gameplay proved,
   defaults still off, remaining original-runtime limits, any identity change
   and Windows API delta. State full selected/pass/fail/empty/skip/incomplete and
   sanitizer counts. Finish clean, with no owned jobs; leave the owner's awake
   assertion alone. No executor push, main merge, rebase, prune or next feature.

**Stop:** one combined review candidate after C0–C5 and final re-review, or a
specific unresolved external/semantic dependency after all independent work is
done. No periodic owner confirmation for routine implementation choices, no
fifteen-question policy handback, and no claim of default/live-window activation.

### MAP-POP-LIVE-1 — resident supply and population gameplay bundle · REVIEWED, PARTIAL; CLOSEOUT ABOVE IS CURRENT

Historical assignment follows. The `135edf6f` handoff is reviewed in
[2026-11-04-map-pop-live1.md](reviews/2026-11-04-map-pop-live1.md); continue with
MAP-POP-CLOSE-1 above rather than restarting this packet.

**Owner:** existing Mac gameplay executor, `task/MAP-2-original-like`.
**Start:** accepted `73e7ab1a129c4712747333d9f34a225be03f063f`, including
orchestrator correction `4438d7f2` (stream 127, PRNG timeline 4).
Read the [acceptance](reviews/2026-11-03-map-pop-impl1.md).

**Goal:** residents obtain goods/services through real supply jobs, House updates
apply their full reachable effects, and a headless town demonstrates supply,
resident change and progression. Deliver L0–L5 as one larger candidate. Every
component is independently reviewed; approvals unlock the next automatically.
Do not stop for an orchestrator handoff after a contract or first code unit.
If a genuinely new owner policy is needed, stop that dependent activation only,
finish independent components and report the exact unresolved choice. An inactive
tick or untested progression cannot be reported as full bundle completion.

#### Read order, ownership and preflight

1. Fetch `origin/main`, read current AGENTS/CLAUDE, PLAN §§0.1–0.6, this packet,
   linked acceptance, evidence contract and testing README with `git show` or the
   established read-only workflow checkout. Do not merge main into the MAP branch.
   Preserve any newer owned commits; do not reset to a historical pin.
2. Read `docs/sim/population-contract.md`, population readiness/inputs,
   `docs/re/population.md` §§3–7, 13–16 and Open, `docs/sim/state.md` §§4.7c/5.9,
   `docs/re/transport.md`, the current
   movement/figure contracts and MAP-POP-IMPL-1 reports. Old P2–P5 labels are
   history; this packet is the release. Reconcile stale text in the sections you use.
3. Follow current `src/sim` producers/readers: House/group/registry metadata,
   `house_kernel`, driver/scheduler/city passes, create/remove/apply, figures/jobs,
   transport/walk search, CityStock, Rules/config and serialization/digests.
   Read original_registry loading, relevant tests, replay tools and
   `tools/sim_stream.py`. Discover exact paths with rg rather than inventing APIs.
4. Reuse host calibration; inspect current CPU, RAM, disk and contention, keep
   admission enabled, and use the existing tools. Keep the screen awake for owned
   jobs, preserving the owner's assertion. Release only your own exact PIDs.
   No broad kills, coordinator repairs, runner rewrites or whole-home scans.
5. Allowed: relevant sim/import/registry code, tests, synthetic fixtures,
   independent references, CMake registration and scoped specs/RE/reports. Do not
   change W4/app/render, active/candidate maps, assets, game/, shared test_support,
   TASKS or AGENTS. No merge/rebase/prune/force push. Windows input stays exact
   `0c401c8d084cfd9feb8aec846595291f87f22cce`; this authorizes no UI integration.

#### L0 — close the live contract, then continue automatically

1. Make one effect map from the original House tick: update order, spawn/draw,
   resident supply, upgrade queue/materials/variant choice, downgrade/removal,
   city/player counters and unlocks. Separate established facts, existing owner
   decisions, checked-arithmetic adaptations and unread edges. Reuse the reviewed
   arithmetic kernel; do not reimplement a competing formula.
2. Resolve narrowly named remaining reads: PublicService +0xa4 for infrastructure
   supply; stall waiting `0x10030090`, +0x28/+0x2c and the three-slot purchase/billing
   arguments; queued-upgrade fill/drain around `0x1002d350`; actual removal target;
   player-built Create timestamp forwarding callers in §16; city unlock timestamp
   and the extra player-mask OR where reached. Prioritize implementation blockers,
   not a full economy/fire/networking research campaign. Parallelize independent
   reads in disjoint notes while parent owns shared schemas.
3. Pin each decisive predicate to primary instructions/file data and one smallest
   hand-derived distinguishing case before implementation. Verify bits, integer
   widths, units, rounding, strict bounds, iterator order and draw timing. Establish
   zero-time and smallest-positive caller visits and preserved partial delays.
4. Carry existing decisions: exact own-save/resync; widened checked arithmetic;
   configurable content roles; `save_billed_goods` default **true** (owner decision
   2026-10-25), false mode explicitly reproducing the original's dropped scratch.
   The contradictory old default-drop paragraph does not override that decision.
   Keep accounting and scratch ownership explicit; test both rule values.
5. Define the live treatment of every reachable kernel NotEvaluated condition
   from evidence/existing decisions. Do not silently freeze, clamp or skip a house.
   If choosing that behavior needs new policy, keep activation gated and continue
   L1–L3. No zero-resident import is not proof that play/mods cannot reach one.
6. A read-only independent reviewer checks this effect contract and distinguishing
   examples against primary evidence. Fix/re-review findings, then proceed. Use
   internal component boundaries for resolved parts; do not turn every unread
   helper into a new owner handoff or stop all implementation behind it.

#### L1 — resident/supplier transactions and persistent job state

1. Load/derive actual house walker facts, supplier facts and figure behavior from
   registry data. Original counts, figure ids, group ids and goods roles are defaults,
   not allowlists. Reuse supported behavior mappings; no new scripting framework.
2. Implement checked demand/urgency/choice, supplier availability/billing/take and
   house delivery as separately testable operations. Preserve substitute goods,
   repeat goods and IdSec ordering according to traced rules; handle chains/cycles
   with explicit validation or established behavior, not unbounded recursion.
3. Implement the actual cargo-slot semantics (the original uses three slots),
   active-walker ownership and required wait/selection state. Keep data-driven
   counts where the engine can widen them; distinguish original layout facts from
   engine limits. Do not reuse the cart's single-cargo delivery as a shortcut.
4. Pin debit/credit/billed/taken/house-stock unit conversions independently. Test
   insufficient stock, duplicate offers, substitution, refusal order, overflow and
   two competing buyers. A delivery must not manufacture goods or silently lose
   an unexplained ledger term. Public services with take=0 are distinct from stalls.
5. Persist/hash every authoritative field; derived indexes are rebuilt. Use one
   combined unpublished state increment (128 if unused; otherwise next unused)
   and one appropriate PRNG timeline increment when new draw sites are activated.
   Record the choice, update the independent byte oracle and resealed-version
   positive/negative controls. Do not reset current identities to historical P4/P5.
6. Independent review covers facts, transaction ordering/units, mod-defined data,
   state bytes and the targeted controls. Focused checks green -> continue to L2.

#### L2 — actual resident fetch job 0x35

1. Implement normal spawn, candidate search, walk, waiting/purchase, return,
   delivery and end through the existing figure lifecycle. Selection uses the
   house's action area and highest reachable need score with traced tie order,
   not nearest-stall distance. Use the figure's actual walk layer: original
   residents can traverse open ground; roads are cheaper, not mandatory.
2. Respect source/target ids and owner state at each transition. Test home or
   supplier removal, blocked route, no stock, competing buyers, ending twice,
   replacement and save/reload while waiting and while carrying. Terminal work
   must reload as a valid world; no dangling jobs, counter leaks or newer-owner
   state cleared by an old job. Reuse the harvesting/transport lifecycle lessons.
3. Preserve draw timing even on a refused send where the original already drew.
   Verify a hand-derived draw/site trace, not just the final draw count. Supply
   must run through real suppliers and routes; no direct warehouse-to-house fill.
4. Check shortest visits, zero elapsed and partitioned waits. Demonstrate a real
   resident debits a supplier, carries cargo, and changes its home's need stock
   exactly. Add a blocked/no-stock control that stays short of that success.
5. Independent package review checks lifecycle, selection, units, draws and the
   test that would catch the most plausible wrong implementation. Continue to L3.

#### L3 — House upgrade, downgrade and removal effects

1. Implement SetGroup/reset/carry-over behavior, upgrade eligibility and queue,
   materials reservation/charging, variant selection and index updates. Derive
   prerequisites from the actual groups/lookup/forced data, not five shipped ids.
2. Make the same-tick ordering explicit. Two houses cannot spend the same material;
   queue entries cannot revive removed/replaced houses. Preserve original draws
   and transaction ordering unless an existing approved adaptation supersedes it.
3. Implement downgrade/removal through the production path, including active
   walkers, ownership/counters and serialization invariants. The numeric below-64
   correction remains downgrade-only. Do not bypass unsupported effects to turn
   the pure kernel's deferred labels into an apparently successful live tick.
4. Use positive/one-predicate near-miss cases for forced needs/materials, strict
   cap/floor, queue invalidation, group transition, signed correction and removal
   while carrying. Compare state/counters/draws after each operation; save/reload
   and replay through transitions must agree.
5. Independent review validates the reachable lifecycle paths, retained partial
   state and expected values from evidence. Fix and re-review, then continue.

#### L4 — activate complete live updates and population progression

1. Activation requires the L0 contract approved under existing decisions and all
   reached L1–L3 effects implemented. Wire the production scheduler/driver to the
   existing kernel and effect handlers with the original order. Never enable a
   reduced numeric tick that omits ordinary spawning or lifecycle effects.
2. Resolve/set play-created House timestamps from the closed caller evidence.
   Keep original-import last=0 and exact own-save/resync distinct; do not reset
   a loaded partial gate. Verify one skipped/taken gate, overdue repeated calls,
   minimal cadence and a nonzero saved last through the actual scheduler.
3. Implement the scoped city resident/house accounting and documented stage/unlock
   updates needed for progression, including player mask propagation. Taxes,
   upkeep booking, fire/plague, military and scenario win/loss remain outside this
   task; do not claim a complete income economy. Supplier billing counters still
   obey L1's exact semantics/persistence decision.
4. Check supported/unsupported content and arithmetic domains before publishing
   partial effects. Surface a genuinely unresolved live policy as an activation
   blocker; keep House dispatch disabled in that case and report L4 incomplete.
   Do not invent a new rules default or call that state full bundle completion.
5. Independent review traces one full shortage -> spawn -> purchase -> delivery
   -> next House gate and one transition/unlock through production code, with
   exact resource/draw/state effects. Approved integration -> proceed to L5.

#### L5 — gameplay canary, bounded scale checks and one final handoff

1. Run one short headless real-registry scenario with available original supplies:
   real resident supply, a measured resident change and a progression transition.
   Use loaded original stocks or normal production/commands for the real canary;
   synthetic fixture setup belongs in separate component proofs. Do not silently
   add grants, relocate workers or rewrite the candidate to force success.
2. Assert a per-goods ledger, actual supplier/cargo/House changes, transition
   identity and unlock outcome. Save/reload at wait/cargo/queued-transition cuts;
   fresh-process replay must reproduce checkpoints, and a changed relevant command
   or starting input must diverge. Unchanged hashes alone do not prove execution.
3. Make work proportional to active houses/jobs and spatial candidates. Test a
   mod-defined group/figure/supplier and growth of unrelated content with operation
   counts; no full world scan per house visit or quadratic resident selection.
   Record the bounded workload and timings without claiming late-game multiplayer
   performance. Authoritative order must be deterministic across supported worker
   configurations; say explicitly where the path is serial.
4. Restore mutants, rebuild and freeze all writers once. Use the canonical workflow
   with explicit source/build/game dir, required reference/CLI contracts, immutable
   logs and source/fixture/binary identities. Run affected Debug and ASan/UBSan,
   serialization/replay references and the real canary. Broaden only for concrete
   changed dependencies; no routine rerun of sailing/heavy-render/GPU matrices.
   Skips, NOT_RUN hosts, failures and reused evidence remain separate.
5. A fresh read-only integration reviewer checks the whole production path,
   meaningful assertions, primary-source derivations, state bytes, draw trace,
   frozen gate verdicts and claimed scope. Fix blockers and rerun only affected
   proofs, then obtain focused re-review at the final head. Package approvals
   alone are not the integration review. Keep actual reviewer ids, reviewed pins,
   verdicts and finding dispositions in a compact tracked review ledger; do not
   label unnamed approvals as independently reproducible evidence.
6. Commit one final report with code/gate/docs pins, delivered gameplay, API and
   Windows delta, actual selected/executed/failed/empty/skipped/incomplete counts,
   initial failures, controls, reuse and remaining policies. Stop once for the
   orchestrator, clean tree, no owned jobs. No executor push/merge or next task.
   Full completion requires L0–L5 including the real live canary. If activation is
   blocked, report the finished components and exact blocker as a partial result;
   do not discard independent implementation or disguise the no-op as live play.

### MAP-POP-IMPL-1 — population input pipeline and House calculation bundle · ACCEPTED; LIVE BUNDLE ABOVE IS CURRENT

**Acceptance, 2026-11-03:** `73e7ab1a`, executor source `a5186375`, bounded
test/reference correction `4438d7f2`. Read the [direct review](reviews/2026-11-03-map-pop-impl1.md).
The following is assignment history; do not repeat A–F or stop the new bundle
after its first contract/component.

**Owner:** existing Mac gameplay executor, `task/MAP-2-original-like`.
**Start:** accepted corrected docs head
`2f876af5a44fc3395819134e79a9595a13d9f7d9`, on accepted forestry
`f40a76022c609092156c457509f0c49e7905f96e` (stream 126, PRNG timeline 4).
Preserve newer owned work if present; identify it before starting. Read the
[acceptance and binding decisions](reviews/2026-11-03-map-pop-ready1.md).

**Goal:** implement the complete, data-driven population input/calculation slice
as one larger candidate. An original scenario must supply the real player mask
and goods/group data to a checked pure House calculation, with independently
derived numeric results and exact persistence. This is substantial engine work,
not another documentation-only readiness round. It prepares the needs, walkers
and population progression line; it does not claim that towns now grow in play.

**Autonomy:** P0 and A–F are all authorized now. After an internal independent
review approves a package, proceed automatically to the next. Routine fixes,
focused re-review, type/API design and the identity choice specified below do
not require an orchestrator handoff. Report once at F. If evidence contradicts a
binding decision or a new gameplay-policy choice is unavoidable, stop only that
dependent path, state the conflict and continue independent work. Do not stop
after the first unit merely because it is separately reviewable.

#### Read order, scope and host preflight

1. Fetch `origin/main`; read its AGENTS, CLAUDE, PLAN §§0.1–0.6, this current
   packet, linked acceptance, `docs/testing/evidence-contract.md` and
   `docs/testing/README.md` using `git show` or the read-only workflow checkout.
   Do not merge main into your branch. Read `docs/plan/lan-endless.md` W6.2 and
   `docs/plan/lan-mvp.md` MVP-B for the gameplay dependency, not permission to
   implement unrelated networking.
2. On the MAP branch read `docs/sim/population-readiness.md`,
   `docs/sim/population-inputs.md`, `docs/re/population.md` (especially §15),
   `docs/sim/state.md` §4.7c, and the MAP-POP-READY-1/CLOSE-1 reports. The new
   acceptance decisions override unresolved proposals and stale DRAFT text.
3. Read current producers and consumers: `src/sim/src/scenario_formats.cpp`,
   `scenario.cpp`, `player.cpp`, `goods_table.cpp`, `house_groups.cpp`,
   `types.cpp`, `config.cpp`, their public headers under
   `src/sim/include/anno/sim/`, and
   `src/sim_load/src/{original_registry,scenario_load}.cpp`. Follow the House
   payload, `state_fields.hpp`, `create.cpp`, scheduler and `driver.cpp` enough
   to keep the calculation/effect boundary explicit.
4. Read `src/sim/tests/{scenario_formats,scenario,player,house,types,config}_test.cpp`,
   relevant corpus/replay/serialization tests and CMake registration,
   `tools/sim_stream.py`, and existing Python reference CLI contracts. For prior
   evidence only, use `git show 075ede70166cc1272fd0fff9b4f2ac17d831a5d6:docs/sim/house-tick-readiness.md`
   and P2.1 code `62b14254f211b97b2195baaa863eb6f151f73528` / report
   `0964729c0c9a7d51977da3a1c27a577a695a703f`. Do not merge/cherry-pick those
   branches or import their old stream identities/goldens as accepted results.
5. Reuse existing host/tool calibration. Record current CPU/core count, RAM,
   available disk, contention and game-dir/tool paths; choose build/test jobs
   from measured current capacity. Keep admission control on. No hardware survey
   or benchmark campaign is needed. Keep the display awake for owned work;
   preserve the owner's assertion and release only your own recorded PID.
6. Allowed work: population-related sim/import code, tests, synthetic fixtures,
   independent references, their CMake registrations, docs and REPORTS. Do not
   edit TASKS/AGENTS, shared coordinator/runner semantics, W4/app/render files,
   candidate/active maps, assets or `game/`. No merge, rebase, force push or pruning.
   Windows remains on exact `0c401c8d084cfd9feb8aec846595291f87f22cce`.

#### P0 — bind the reviewed contract and partition the work

1. Apply all nine binding decisions from the acceptance to the branch contract.
   Resolve the existing proposal wording; distinguish original facts from approved
   safe-domain adaptations. Own saves/resync preserve exact `last`; arithmetic
   stays widened/checked; undefined operations are typed NotEvaluated results.
2. Specify the actual registry/config binding for derived groups. `TypeRegistry::build`
   already takes SimConfig: never construct under defaults and silently consume
   under different infrastructure/tavern roles. Bind and validate the immutable
   view, or key it by both identities. Keep empty non-population registries valid.
3. Use one combined identity increment: stream 127 if still unused on this
   branch, otherwise the next unused value, recorded before implementation.
   PRNG timeline remains 4; command wire unchanged. A–C share this increment;
   derived caches and a pure function do not require further bumps.
4. Write a short rule-to-proof table for the decisive rules below: primary
   evidence -> wrong reading -> distinguishing input -> intended assertion.
   Reuse the accepted tables; do not invent a second general evidence framework.
   Include the goods-domain lookup, removal bypass, Force/endpoints and smallest
   caller cadence. A rule copied into both code and oracle is not independent proof.
5. An independent read-only reviewer checks this bound design and the source-derived
   decisive examples. Fix/re-review actual findings, then proceed without an
   orchestrator request. A research/reference helper may work in disjoint owned
   files; parent owns shared headers, config, digest, version and final integration.

#### A — full player-mask import through created worlds

1. Add an optional full mask to `ScenarioSpec::PlayerSpec`; decode original
   SPIELER HEAD +0x48/+0x5c by the accepted T32.5 formula; assign it in
   `World::create`. Preserve absent -> constructor 1 and explicit 0 -> 0.
   Fixed-map/ship-start synthetic specs keep their existing default.
2. Test low bits, each high reconstructed bit, ignored packed bits and bit 23,
   all the way through the resulting world's players, not just a helper. A valid
   low-mask control must pass beside the high-bit case.
3. Independently read all 50 original scenario files to compare the complete
   imported masks/histograms. Record actual file/head counts rather than assuming
   the historical 373. Update only identity-affected corpus outputs with explained
   deltas; do not rebaseline unexplained world changes.
4. Prove that dropping high reconstruction and dropping the final assignment
   each fail the intended checks with a legal control first. Restore, rebuild
   and verify the source binary before further evidence.
5. Independent package review covers decode, optional-zero semantics, created-world
   assertions and corpus independence. Focused checks green -> continue to B.

#### B — goods metadata, configurable roles and exact identity

1. Parse `[WAREN]` BauFirst and IdSec beside the existing group reader; carry
   stage index and widened goods id in GoodsInfo. Defaults remain 0. Resolve
   stage names through the checked original 0–31 mapping; unknown names,
   GOTT/SPIELER and unresolved goods are diagnostic errors under the approved
   parser domain. Do not expand into prices, taxes or other WAREN keys.
2. Add the four reviewed infrastructure/tavern SimConfig fields with original
   defaults; validate their population use against the actual goods domain.
   Hash/serialize inputs and check config/registry mismatch explicitly. A minimal
   non-population registry must not fail because original default ids are absent.
3. Test the 23 shipped BauFirst keys, four IdSec keys, absent/explicit defaults,
   unknown/out-of-domain names and non-original goods tables. Expectations come
   from the raw files/name table, not a dump of the new C++ reader.
4. Extend registry/config identity and `tools/sim_stream.py`. Independently specify
   new canonical bytes. Pair a valid current stream with a correctly resealed
   older-version stream: prove rejection is at the version check, not checksum,
   truncated layout or missing prerequisite. Metadata/config perturbations must
   move the intended digest; unchanged semantic defaults must stay consistent.
5. Review this package independently, including golden/corpus diffs and their
   independent oracle. Do not regenerate expected values solely from production.
   Retain stream-126 recordings at their original pins; do not claim a new binary
   replays them or rewrite large opening logs without executing the new version.

#### C — immutable derived group tables with mod-sized domains

1. Build once from actual needs, goods IdSec and bound config: goods_end,
   infra_end, dynamic forced-infrastructure bits, goods-id lookup, tavern alias
   and ascending one-pass substitution. Keep derived data out of authoritative
   serialization; its inputs are hashed. No mutable global or per-house rebuild.
2. Use a lookup of **goods_count** entries with **need_count** sentinel. Cover
   the six-need PIONIER lookup of goods 76 and zero needs with nonempty goods.
   Preserve duplicate needs and the last-slot lookup; tavern alias uses the first
   eligible tavern until a real alias entry supersedes it.
3. Test all five shipped groups slot by slot against an independent transcription,
   plus C1–C12: endpoint updates independent of Force, reordered/duplicate needs,
   missing/real aliases, higher/lower substitution chains, and no fixed-point
   substitution. Reject active UNUSED/zero demand as the approved adaptation.
4. Add a synthetic registry exceeding original goods, need and forced-bit counts
   (including more than 255 needs and more than 32 infrastructure ids), with
   non-original role ids. No shipped id allowlist or original-count allocation.
   Current stage-mask width remains the documented 32-bit domain, not an excuse
   for other caps. Assert config changes cannot reuse stale derived tables.
5. Independent review checks index domains, sentinel/order rules, cache binding,
   digest coverage and original/synthetic provenance. Focused tests -> continue.

#### D — independent numeric reference and checked pure House kernel

1. Before matching C++ outputs, build a small integer Python reference and a
   durable synthetic vector file from primary instruction spans and hand-derived
   D1–D14 examples. Supply an explicit standalone formula/units contract. The CLI
   must reject zero vectors, wrong expectations and malformed input with nonzero
   status; bound child time. A reference that merely echoes expected results must
   fail a tiny public-command contract. Register both in CTest and require them.
2. Implement `house_kernel(inputs) -> typed outputs` without World access,
   mutation, I/O, PRNG or live dispatch. Inputs include actual group/config view,
   mask, House values, t/last, class gate period, mood flags and explicitly
   available previous capacity/walker facts. Return numeric predictions, domain
   results and labelled deferred effects; never claim a walker was spawned or a
   house upgraded/removed. Missing required data is not successful unchanged output.
3. Implement the audited order: unsigned u16 gate; one `last += period` per
   taken call; toward-zero smoothing; only stage-enabled stock decay; lowest on
   decayed stock; optional spawn predicate; original mood precedence; growth,
   signed strict cap/floor and the numeric counter/correction outputs. Keep slot 0
   positional; do not impose a hardcoded food id or reorder needs.
4. Test no elapsed time AND an overdue repeated call: t=5000,last=0 takes one
   step, another call at the same t takes one, the third skips. Also test one-tick
   calls, boundary/wrap, caller partition and save/reload of partial `last`.
   No whole-world schedule-invariance claim follows from this per-call behavior.
5. Pin the removal guard: the below-64 correction is on the downgrade arm only;
   the remove arm bypasses it. Include two inputs with the same below-64 value
   and different arm selection, in addition to signed-negative versus unsigned
   controls. Negative mood wins over flag 0x10; flag 0x20 forces -2 as documented.
6. Check intermediate/output ranges before operations. A reached zero divisor,
   negative smoothed E, unavailable previous capacity or out-of-table mood returns
   a specific typed domain result. E=0 mood short-circuits, but a selected need
   may reach its divide earlier: distinguish those paths. Empty metadata remains
   valid even when a requested calculation needs a missing first slot. Widened
   stocks must not multiply in 32 bits. No native fault, silent wrap or guessed clamp.
7. Use cheap distinguishing controls for the highest-risk rules: last=t, wrong
   mask, Force-based endpoints, always-average mood, wrong signed correction and
   a function that returns nominal success without calculating. Require the exact
   intended assertion, not any crash/timeout. Reuse earlier package controls;
   no exhaustive mutant campaign or expensive opening per mutation.
8. Independent reviewer re-derives the decisive expected values from primary
   evidence, inspects C++/Python differences and runs focused controls. Approval
   plus focused Debug/ASan checks unlocks E; do not stop for orchestrator review.

#### E — real-input integration and bounded live-readiness evidence

1. Research may run in parallel with A–D: follow the named Create callers for
   construction and original load, count zero-resident houses in the 50 scenarios,
   and trace below-64 reachability including removal/NoDowngrade guards. Record
   module identity, address, argument provenance and one exact worked path.
   Stop a trace at a precise unread edge after a bounded local investigation;
   do not repeat broad directory scans or wait on Windows captures. Unresolved
   live-effect evidence does not block the pure pipeline.
2. Build a short real-file canary: original registry + original scenario ->
   actual imported players/House/group/config inputs -> pure calculations ->
   checked numeric outputs matching the independent reference. Include nontrivial
   masks, stocks and at least one skipped/taken gate. Count actual calculations
   and domain outcomes; an unchanged world hash alone is not proof of execution.
3. Read the world through const interfaces. Save/reload must reproduce exact
   extracted inputs and calculations at the same supplied time; no resetting last,
   synthetic stock grants, hidden World mutation or fake invocation of live ticks.
   Use a bounded scenario load rather than rerunning hours of sailing. Keep
   House dispatch a no-op and demonstrate that boundary in one focused check.
4. Document actual complexity: registry metadata once, lookup by goods id,
   calculation proportional to needs, no whole-world scan per House. Do not claim
   late-game/multiplayer performance from a small canary. Record the next live
   effect dependencies precisely; taxes, walkers, unlocks and transitions remain
   deferred, with no assumption that ordinary towns avoid those branches.
5. Independent review covers real producer-to-calculation wiring, numeric oracle,
   save/reload identity and the bounded research claims. Fix findings locally;
   then proceed directly to the combined F closeout.

#### F — one frozen-source gate, final integration review and stop

1. Commit completed packages and freeze every helper/source writer before final
   acceptance. Use the canonical workflow with explicit workflow/source/build
   identities, current game dir, required reference/CLI contracts and immutable
   logs. Restore all mutants and rebuild clean source before the gate.
2. Run one affected Debug gate spanning player/scenario import, goods/config/
   groups, kernel, state bytes and replay/reference tests; include the original
   50-file creation corpus and E's real-input canary. Run the corresponding
   affected ASan/UBSan selection. Record complete selections and selected,
   executed, failed, empty, skipped and incomplete counts; required asset checks
   must execute. Use the supported wrapper/timeouts, not a private verdict reader.
3. Run formatting and cheap workflow preflights required by the evidence contract.
   Do not repeat full sailing/forestry/heavy render/GPU gates whose call paths
   are unchanged. Explain reuse by dependency and source identity, not by an
   unchanged file-count claim. Windows/MSVC, Linux warnings/sanitizers and
   original runtime are NOT_RUN on this host, not local blockers or passes.
4. A fresh independent integration reviewer traces raw scenario/WAREN inputs
   through the created world, bound tables, pure kernel and persistence. It also
   checks assertion provenance, positive controls, new domain/version rejection,
   saved gate verdicts and that no live effect is implicitly claimed. It must
   inspect rather than just repeat package approvals. Fix blockers and re-run
   only affected proofs; review fixes at the final source. Avoid another broad
   campaign unless a concrete new failure justifies it.
5. Append one REPORTS entry with exact code/gate/docs pins, delivered API,
   rule-to-proof index, actual counts, initial failures, independent reviewer
   identities/verdicts, reuse arguments, Windows delta and remaining live-effect
   dependencies. Distinguish implemented calculations from gameplay now active.
   Commit locally, leave a clean tree and stop once for orchestrator acceptance;
   no task-code push or merge is assigned to the executor.
6. Completion means all A–E implementation and checks are integrated, mandatory
   local gate outcomes are complete and green, and independent final review has
   no blocking finding. A stub/always-NotEvaluated kernel, metadata-only delivery,
   zero-case reference or unexecuted real-input canary does not complete this
   bundle. An exact unread live-research edge may remain disclosed because no
   live tick is released. No further task is self-started after F.

### MAP-POP-READY-1 — population prerequisites and next implementation contract · ACCEPTED; IMPLEMENTATION ABOVE IS CURRENT

**Acceptance, 2026-11-03:** corrected planning input `2f876af5`, executor draft
`dc949172`, report `b77860e2`. Read the [direct review](reviews/2026-11-03-map-pop-ready1.md).
The following is assignment history, not a reason to repeat readiness or stop
the larger implementation bundle after its first package.

**Owner:** existing Mac gameplay executor, `task/MAP-2-original-like`, preserving
newer owned work. Accepted baseline:
`f40a76022c609092156c457509f0c49e7905f96e` (stream 126, PRNG timeline 4).
Read the [forestry acceptance](reviews/2026-11-03-map-econ1-close1.md).

**Goal:** turn the existing population research into one current, decision-ready
implementation packet. W6.2 / MVP-B identify population, needs and taxes as the
next critical gameplay line. Reuse the existing audits; do not restart broad RE
or mistake a historical READY label for present authorization.

**Scope:** documentation and bounded read-only evidence checks. Allowed tracked
writes: `docs/sim/population-readiness.md` (create/update),
`docs/sim/population-inputs.md` (create/update) and an appended REPORTS entry.
Read other files and old refs without editing or integrating them. No `src/`,
state/command version, golden, original asset, candidate, active-map, app/render,
W4, coordinator, runner, TASKS or AGENTS change. No merge/rebase/task-code push.

#### R0 — current baseline and prior evidence

1. Fetch `origin/main` and read its AGENTS/CLAUDE, PLAN §§0.1–0.6, evidence
   contract, testing README, current dispatch and linked forestry acceptance
   without merging main. Reuse current host calibration; this packet requires
   no build or broad test gate. Preserve others' processes and keep-awake.
2. Read `docs/plan/lan-endless.md` W6.2 and `docs/plan/lan-mvp.md` MVP-B,
   `docs/re/population.md`, `docs/sim/state.md` §4.7c, historical TASKS
   T32.5/T32.6/P2.1–P2.3, applicable reports and the original-issues inventory.
3. Recover the existing T32.6 audit read-only from
   `075ede70166cc1272fd0fff9b4f2ac17d831a5d6` and its
   `docs/sim/house-tick-readiness.md`. That file is absent from current main/MAP;
   use `git show <ref>:<path>` rather than assuming it was integrated. Read the
   P2.1 report/diff at `0964729c0c9a7d51977da3a1c27a577a695a703f` and the P1
   baseline `b173d02217b378256a7aa04a614567c19232a4ce`. These are evidence inputs,
   not automatically accepted source for this branch.
4. Read the current scenario/player import, registry goods/groups, House payload,
   class dispatch/no-op, save/digest and relevant tests. Produce one table:
   prerequisite -> actual current producer/caller -> exact code/evidence pin ->
   present/partial/missing -> accepted/author-reported/unreviewed. Check actual
   behavior/code presence, not ancestry or a prior completion sentence alone.

#### R1 — reconcile only the remaining population dependencies

1. Reconcile the full player-mask import, corrected stage-mask evidence, house
   construction/load/live paths and the T32.6 lifecycle/side-effect inventory.
   Identify what is already present in the accepted MAP baseline and the minimum
   absent prerequisites. Do not copy older stream versions or reset current
   format identities to match old reports.
2. Distinguish inputs first read by P2 calculations from transitions/unlocks/
   walkers or deliveries first required by P3–P5. Keep direct effects separate
   from transitive effects and PRNG draws. A pure partial kernel and a live House
   tick are different deliverables; neither can silently stand in for the other.
3. Use existing original-file/address evidence first. Read only a specific
   unresolved producer/caller needed to resolve a contradiction in this inventory;
   record exact source identity and result. Do not expand into a new complete
   house/combat/economy RE campaign or require the Windows executor to run captures.
4. Reconcile OQ-450/452/473 and related audit questions against existing decisions.
   Mark established facts, approved adaptations, proposals and still-open points
   separately. For an owner decision, state the exact affected behavior, available
   evidence and bounded options. Do not choose new population policy implicitly.

#### R2 — bounded input contract and discriminating examples

1. Draft `population-inputs.md` for the audited first-reader immutable inputs:
   goods BauFirst/IdSec, group lookup/endpoints/forced infrastructure and only the
   other fields actually needed by the proposed slice. Distinguish loader values,
   post-pass substitutions and derived values; do not treat endpoint derivation
   as an unexplained Force side effect. Use the corrected prior audit.
2. Specify data ownership, integer widths/signedness/units, configurable domains,
   validation, default/absent/explicit-zero behavior, digest and future persistence
   impact. Preserve data-driven/mod support without fixed player/type/group caps
   or a new scripting framework. This packet changes no layout or version.
3. Hand-derive a few smallest distinguishing cases from primary evidence for the
   decisive masks, signed comparisons, substitutions, endpoints and boundaries.
   Explain which plausible wrong interpretation each case rejects. For any timed
   calculation proposed, identify the actual caller's supported cadence and
   partial-state requirements. Add examples to the existing acceptance table;
   no new testing framework, broad mutant campaign or copied C++ expected values.
4. Propose three to five manageable implementation packages with exact files,
   dependencies, proof cases and stopping criteria. Name the smallest implementable
   first unit and its supported/excluded paths. Identify whether an OQ blocks
   immutable metadata, a partial kernel, or only live effects. Do not self-release
   P2.2/P2.3 or integrate an unreviewed prerequisite to make the table green.

#### R3 — one independent review and stop

1. An independent read-only reviewer checks the current/prior distinction,
   first-reader inventory, source-derived examples, effect boundaries, owner
   decisions and proposed implementation scope. It must verify decisive facts
   independently rather than accept agreement among inherited notes. Fix findings
   and obtain focused re-review; no duplicate broad audit or expensive gate.
2. Check links/addresses/ref availability and `git diff --check`. Run an existing
   cheap read-only reference only if it answers a named remaining question. No
   C++ build, sailing opening, sanitizer matrix or unrelated gate is needed.
3. Commit locally and report exact inspected/current/docs pins, reviewer identity,
   scope/disposition, known missing prerequisites and the proposed first code unit.
   If earlier CLOSE-1 reviewer identifiers are readily available, append them as
   ordinary metadata; otherwise state the limit without reconstructing archives.
4. Stop with a decision-ready population packet. Actual House ticks, taxes,
   stage changes, unlocks, walkers, other production chains and prerequisite
   integration await an explicit next task. Windows stays on exact
   `0c401c8d084cfd9feb8aec846595291f87f22cce` throughout.

### MAP-ECON-1-CLOSE-1 — ACCEPTED; POPULATION READINESS ABOVE IS CURRENT

**Acceptance, 2026-11-03:** final candidate `f40a7602`, code `1e9c770e`,
proof tests `44096b84`, clean gate/contract `e6b1083c`. Read the
[direct acceptance](reviews/2026-11-03-map-econ1-close1.md). The original probe
passes 42/42; Debug/ASan each execute 202/202. The following packet is history.
Do not restart this closeout, redo the long opening or fix unrelated metadata
as another source/test round.


**Start:** existing Mac branch/worktree at reviewed candidate
`78a77625962017ff7784e2849e762193bf4c8c29`; preserve newer owned work. Gate
source `a4982f5198f433b5e9dc631dbe86d6bade50e86f` differs only by the later
report/contract commit. Read the entire
[direct review and decisions](reviews/2026-11-03-map-econ1.md).

**Goal:** close the one reproduced public end-path defect and the specific small
proof gaps, preserving the authentic forestry opening. E2.0's corrections and
valid E2–E6 evidence stand. This is a finite correction packet, not a restart.

#### C0 — read, reproduce and preserve evidence

1. Fetch `origin/main` and read its AGENTS/CLAUDE, PLAN §§0.1–0.4, evidence
   contract, testing README, this dispatch and linked review without merging
   main. On your branch read the latest REPORTS entry and forestry contract.
2. Read `figure.cpp` end/sync helpers, `tick.hpp::SerialWorld::end_figure`,
   `scheduler.cpp` dispatch/merge/expiry, `harvest.cpp`, harvest validators,
   state field lists and the related figure-removal/harvest tests. No coordinator,
   runner, app/render/W4, asset, candidate-install or Windows integration work.
3. Reproduce the valid active-worker -> public end -> invalid harvest state
   transition in a committed regression, in both faithful and enhanced modes.
   The root exploratory probe is in main's ignored
   `cache/review-map-econ1-e2-e6/harvest_review_probe.cpp`, with the corrected
   run `end-probe-correct-seams.out`. Its original compile/load-seam mistakes
   are not acceptance evidence. Save/load controls must use matching seam names.
   Before-end validity/load succeeds; after-end failure reaches the harvest
   diagnostic. Enhanced bit/map remain owned by the ended worker.

#### C1 — shared termination, exactly once

1. Normalize active harvest state through the supported public end path before
   losing its home/island association. The resulting world must satisfy invariants
   and reload immediately. Enhanced mode releases only this worker's own claim;
   a newer worker's claim survives. Faithful generic end retains its specified
   no-extra-claim-clear policy; an explicit cut/abort is a separate transition.
2. Preserve exactly-once home-counter release, expiry behavior and historical
   cargo. Repeated end is harmless. Preserve normal Done state bytes/behavior
   where possible so existing real-opening evidence remains applicable. Do not
   merely weaken validators, clear every claim, repair on load, or add a whole-
   world search, new cancel command or combat subsystem.
3. Cover ending during the initial delay, ToTree, Cut and carrying ToHome,
   repeated end, expiry and same-seam save/reload. Include an ordinary completion
   control and an old-worker late-end/new-owner control. If a shared helper
   changes, include the affected non-harvest end/counter cases too.
4. The old behavior or a reverted cleanup must fail the intended state/claim
   assertions with the valid before-end and normal-completion controls green.
   Obtain independent read-only review; fix findings and focused re-review.

#### C2 — small persistence and data-driven behavior proofs

1. Add independent hand-written expected bytes for a populated HarvestState
   and a non-empty ordered claim-owner map: nonzero distinct values, correct
   widths/order, optional presence and sequence/map counts, route points and
   indices. Use an independent little-endian transcription, not production
   `fields()` to generate the expected answer. Shared serializer round-trip
   and hash movement alone cannot establish this layout. A swapped/omitted
   field must fail the byte assertion while the valid control passes.
2. With the existing synthetic alternate-goods/other-owner fixture, exercise
   actual resource consumption, worker cargo and delivery to Farm.raw. The
   current alternate-content case checks spawning only. Prove the right goods
   and home receive the material. No new original-crop chain, planting system,
   registry-wide matrix or second expensive opening.
3. Apply the linked orchestrator decisions: the exact compiled route-name
   mapping is accepted as a supported-behavior mapping; the inherited less-than-
   one-tick walk-arrival quantization is accepted for this slice; generic shared
   dispatch remains, including 19 shipped types counting the forester. State
   their limits accurately. Other production chains are not declared verified.
   Remove the contradictory absolute lossless-time wording. Add a tiny exact/
   fractional arrival-boundary helper check; no walker rewrite or format change
   is assigned. Preserve the initial five-tick remainder and whole-tick carry.
4. Independently review the hand-derived answers and these controls within the
   same package review. Update documentation alongside the corrections.

#### C3 — finite gates, independent closeout and stop

1. Run name/verdict preflight and cheap new proof cases first. Freeze source,
   tools and helper writes, then use the canonical gate for affected end,
   figure-removal, harvest lifecycle/persistence, and small delivery cases in
   Debug and ASan/UBSan. Require the existing forestry reference contracts.
   Inspect the selection explicitly so the long opening is not included
   accidentally; record selected/executed/failed/empty/skipped/incomplete counts.
2. Retain D1/A1, the 28-checkpoint fresh replay and first failures at their pins.
   If normal Done bytes/behavior remain unchanged, give the source-dependency
   explanation and reuse the real opening/replay. If a real affected dependency
   changes, rerun that canary once and explain why. No blanket 1091-case rerun,
   full-ASan sailing campaign, corpus regeneration or infrastructure repairs.
3. A fresh independent read-only closeout reviewer inspects the frozen combined
   correction, actual callers, raw evidence and the new layout controls. It
   verifies the pre-end positive reaches the intended path, cleanup cannot erase
   another owner, and reused evidence has unchanged dependencies. Record reviewer
   identity, exact revision, scope and dispositions in the existing REPORTS entry;
   no reconstruction of missing old reviewer archives. Fix actionable findings
   and obtain focused re-review without duplicating the long gates.
4. Commit locally and stop for orchestrator acceptance. Report exact code/test/
   docs/gate pins, retained failures, remaining limits and future Windows delta.
   No merge/rebase/task-code push, TASKS edit or new feature starts in this packet.
   Windows stays on exact `0c401c8d084cfd9feb8aec846595291f87f22cce`.

**Stop:** the supported end path leaves harvest state valid and reloadable in
both modes, enhanced ownership is safely released, the small persistence/content
proofs pass, and the correction has independent approval. The existing opening
remains the gameplay achievement; this does not claim original-runtime, full
plantation-chain, cross-platform or late-game performance acceptance.

### MAP-ROAD-CLOSE-2 / MAP-ECON-1 E1-FINAL — HISTORY; CURRENT TASK ABOVE

**Disposition, 2026-11-03:** atomic code/tests `d9a05682`, clean focused gate
`d0e4ae60`, final candidate `362f99ab`. Atomic and runner fixes are accepted.
The [direct correction review](reviews/2026-11-03-roadclose2-forestry.md) found
a remaining inherited placement-mask error and a short-visit timing defect in
the proposed contract. E2.0 below is the finite follow-up; do not repeat A–D.
The following packet is assignment history, including its former stop boundary.

**Start:** existing Mac branch/worktree at local
`c82608bd22a8a86ff50dddc039143b46e658467c`; preserve newer owned work. Read the
[full direct review](reviews/2026-11-03-roadclose-forestry.md). The original road
policy findings are closed. Preserve both Rules fields, stream 125, the sealed
version controls and valid C1-1 evidence; do not restart P0–P3 or R0–R5.

**Outcome:** atomic validation observes the same latest objects as execution;
the small forester reference refuses empty runs and cannot stall at full output;
the forester contract is internally consistent and ready for implementation.
No harvest engine, new state fields, stream/PRNG bump, Windows/app/render work,
coordinator repair, main merge/rebase, task-code push or original/map edit belongs
in this correction. E2–E6 below stays conditional.

#### A — reproduce and fix the latest-object mismatch

1. Read `apply.cpp::check_record`, `Budget::objects`, default placement occupancy,
   `site::overlap_with_existing`, road eligibility and existing mixed-batch tests.
   The reviewer repro is `cache/review-road-close-forestry/atomic_overlay_probe.cpp`
   in the main checkout, with `probe.out`. It uses existing test helpers and
   links current production libraries without changing the candidate.
2. Add the deterministic regression to the real suite. Create a valid data-defined
   road with flags `0x4186 | 0x20 = 0x41a6`; assert registry/road eligibility.
   On an existing ramp, with both extra road options false, place that road and
   then a house on its tile in one BuildCmd. Sequential controls accept both;
   current atomic execution incorrectly reports `AtomicAbort / Occupied`.
3. Use one coherent latest-object view for default occupancy validation: an
   earlier virtual road supersedes the physical object. A separate virtual
   precheck followed by stale physical occupancy is insufficient. Preserve
   row-major refusal order, existing building-occupancy priority and other
   placement/affordability rules. Avoid a full-island copy per record or an
   eligibility blacklist that merely hides the valid flag combination.
4. Cover the applicable four policy combinations using existing-road/ramp
   controls, replacement and inverse record order. Verify outcomes AND final
   gold/goods/serial/building/tile state when both runs succeed, excluding only
   the intentional `build_atomic` rule difference. Rejected atomic batches
   remain byte-identical. Broaden the small input domain by meaningful type
   properties; more rounds choosing the same fixed flags cannot find this defect.

#### B — finalize the forester contract in a separate research subagent

Parent owns A and shared gate registration/reporting. A separate child may own
only `docs/{re,sim}/forestry.md`, the pure reference/vectors and research evidence.
During frozen gates it writes ignored cache drafts only; promote at an agreed
checkpoint. No engine edits by that child and no shared REPORTS/TASKS edits.

1. Read the review and current E1 evidence. The complete packet has now been
   reviewed independently by the orchestrator. Correct the named findings and
   obtain focused re-review of corrections/new claims, rather than redoing the
   full original-resource research.
2. Define same-figure read-your-own effects for search/claim/cut/repeat. Work an
   example with two trees, capacity greater than 256 and enough elapsed ticks
   to perform more than one harvest in a visit. Later transitions must see prior
   planned changes, and credited cargo must match successful consumption. State
   whether a local overlay/transaction provides that view; do not confuse merge
   after each figure with merge between transitions or discard elapsed ticks.
3. Make claim validity, placement/clearing, target replacement, job termination
   and cleanup policy coherent. An old job must not clear a newer worker's claim
   after replacement, including same-type replacement. Validators/loaders never
   silently repair authoritative state. Define permitted stable/transitional
   states separately under faithful and enhanced rules; remove conflicting
   unconditional bijection, leakage and auto-repair statements.
4. Orchestrator permits explicit false-default hashed enhanced choices for
   claim hygiene and skipping data-defined uncuttable resources. Specify their
   exact scope and enhanced values; no hardcoded tree-261 exception. Do not call
   untraced cleanup absence a proven original leak. Keep raw-store limits based
   on actual type/configuration; the 801/two-worker bound is not a new global
   u16 cap on all Farm or mod states. Engine fields wait for released E2.
5. Reconcile full-output dispatch: for the evidenced two-worker forester, full
   output need not stop a rate/count-eligible dispatch. Correct the contrary
   section 4.3 statement. Define draws in terms of eligible attempts, including
   refused sends, not successful spawns. Pin the first-job and home-counter
   release transitions using the bounded named paths (`0x10072dce–0x10072de5`,
   `0x10034760`, `0x1003af00`, common end/unlink callers). They affect future
   sends and PRNG state. Trace only the needed common forester paths, not a new
   combat/figure-lifecycle campaign.
6. Reusing the current deterministic tile-level walker is allowed for the first
   slice. State the inherited fine-route veto/retry gap accurately; it can alter
   acceptance/selection as well as timing. Do not treat `complete=true` as proof
   that the omitted stage passed. No new fine-pathfinding implementation or
   broad fine-route RE is required to finish this contract.
7. Align the canary plan with E5 below: one unchanged-candidate public-command
   opening, separate small tree/no-tree fixture controls, complete material
   ledger, and tree transitions derived for all eligible N -> N-1 types rather
   than only 105 -> 104. Derive starting wood from the current manifest/founding/
   unloading sequence, not an old HUD example. Include carrying/returning saves
   as well as outbound/cut states. Do not multiply expensive opening variants.
8. Document precise cut/growth writes and reuse existing `StoreDelta::Input` for
   Farm.raw where its semantics fit. Do not require a redundant Raw effect enum
   without need. The future dispatch draw must have its proper DrawSite and PRNG
   timeline treatment, not a claim that only version bytes change. Keep the
   proposed E2–E6 staging subordinate to this board's authorized packages.

#### C — make the pure reference a reliable public-command check

1. Reproduce `forester_ref.py` on an empty and a type-directive-only file: current
   exit 0 / zero vectors is wrong. Refuse zero executed vectors. Keep a non-empty
   passing control and a deliberately incorrect expected-answer failure.
2. Reproduce `drain 4096 -> 1152 1288`: after 92 full steps output is 1288 and
   raw is 1152; status 2 thereafter makes the current loop stall. Stop at the
   full-output terminal state (or define the bounded helper domain explicitly)
   without changing the established step arithmetic. Add the hand-derived vector.
3. Add a small public-command contract with bounded owned child processes for
   empty input, the full-output case, a correct case and an incorrect expectation.
   The correct full-output case must complete and match amounts, not pass by
   timing out. Check the bundled vector coverage explicitly. Parent may register
   these Python checks in test build metadata; that is not harvest engine work.
   Preserve all 44 existing outcomes unless a specific evidenced correction is
   required. Parameterize paths needed by the checks; no general tool framework.

#### D — focused verification, independent corrections review and stop

1. Run the new deterministic counterexamples/positive controls and pure reference
   checks before a gate. Freeze the parent/child inputs together. Use the canonical
   workflow and require the small public reference contract once registered.
2. Gate affected atomic/default placement, build/road, policy and replay cases in
   Debug and ASan/UBSan. Reuse unaffected C1-1 evidence with a source-dependency
   explanation. No format/state change or golden rebaseline is assigned. The
   real canary uses non-atomic commands; if its path is unchanged, retain its
   pinned 103/103 result instead of rerunning sailing. Expand only for an actual
   affected dependency/failure and label old versus new evidence accurately.
3. Independent read-only reviews check A's production caller/latest-object view,
   B's corrected/new contract claims, and C's public failure behavior. Fix
   findings and obtain narrow re-review. One final correction review reconciles
   the pins, evidence and prerequisite status; no repeated unrelated matrix.
4. Commit locally and report the original failures, exact code/docs/gate pins,
   executed counts and remaining limitations. Stop for orchestrator acceptance.
   E2–E6 remains queued until both accepted pins are recorded in its release
   table. No Windows, renderer, coordinator or new planting feature starts here.

### MAP-ROAD-CLOSE-1 — separate the enhanced road policies; parallel forester contract · REVIEWED, FOLLOW-UP ABOVE

**Checkpoint:** policy correction `eae9f4ae`, comment/doc increment `aad1735b`,
forester draft/report `c82608bd`. Both original road-policy findings are fixed;
the valid 46/46, 153/154 and 1061/1063 gate evidence is retained with named
opt-in skips. Full acceptance remains pending the atomic latest-object correction
and the E1 findings in the [direct review](reviews/2026-11-03-roadclose-forestry.md).
Do not repeat this historical packet or treat its research draft as approval.

**Start:** existing Mac branch/worktree `task/MAP-2-original-like`, current local
road candidate `242c3433d879537dbaba8162da852a48109b6635`, tested code
`6e7214686788d3d6f852e2ab4784f6c3b3ba73da`. Preserve all completed R0–R5 work.
Read the [direct review](reviews/2026-11-03-map-road1.md). No rollback, renewed
road implementation, full preset system or infrastructure repair is assigned.
Do not merge main into the task branch or push task code before review.

**Outcome:** the two extra road restrictions are explicit hashed/saved choices,
with the known original road predicates available. Valid previous evidence
remains scoped to its source. In parallel, obtain a reviewed, original-evidence
forester contract; this packet does not authorize the harvest engine yet.

#### P0 — read, split ownership and freeze correctly

1. Fetch/read main instructions, the evidence contract and testing README, this
   packet and review. Read the road report/contract/RE note and
   `docs/sim/rules-presets.md`. Inspect actual Rules entry points; setup's
   `RulesPreset` enum is not an implemented preset-to-Rules mapping.
2. Read `src/sim/include/anno/sim/{config,state_fields,world,placement}.hpp`,
   `src/sim/src/{placement,apply}.cpp`, relevant config/placement/build/save/
   version tests, `tools/{placement_ref,sim_stream}.py`, the road vectors and
   real opening canary. Keep the existing authoritative-height adaptation.
3. Spawn a separate research subagent for E1 below. Parent owns policy/state/
   tests and road docs; child owns only the forester evidence/contract/reference.
   It must not edit engine files, road files, shared REPORTS/TASKS or another
   executor's checkout. During a frozen gate it may read and write ignored cache
   research only; no tracked or non-ignored input changes. Promote its completed
   files only after the gate ends and record the separate source/tool identities.
4. Check current host CPU/RAM/disk/contention; reuse valid calibration with
   admission on. Keep the screen awake for owned jobs. Stop only verified exact
   owned PIDs/sessions; no pattern kills or coordinator changes. No Windows,
   app/render/W4, active map, original asset or candidate change is assigned.

#### P1 — implement the two explicit choices

1. Orchestrator decision: keep R-A2/R-A3 as explicit enhanced road choices.
   Add two Rules booleans, default false under the existing faithful convention:
   `road_require_land_level` (the additional class-`0x4000` floor) and
   `road_preserve_nonroad_pieces` (the additional ramp/pier refusal), or equally
   precise names. Both true reproduce the current enhanced road behavior.
2. Thread the world's actual Rules values through the default placement path
   and make the pure-rule policy explicit. False for the first option removes
   only the extra class-`0x4000` floor: class `0x1000` still has the original
   level threshold and bit-4-gated slope test. False for the second removes only
   R-A3; existing compatibility and unsupported-class gates still apply.
3. Keep server-derived height in both policies. Do not restore client-controlled
   heights, add new road families, change removal/refunds or revisit unrelated
   transport behavior. Update the reference and document original versus enhanced
   expectations without changing what the emulator calls the original.
4. Existing World/ship-loader entry points already accept Rules. Select both
   enhanced road options explicitly for enhanced sessions/canaries; document their
   intended preset values. Do not introduce a complete preset mapper that decides
   unrelated proposal rows or silently alters other session rules.
5. Persist/hash both options through the existing field list, validation and
   rules digest. Bump the state stream to the next free version for these actual
   new fields, with independent Python transcription/golden updates. This is not
   permission to change the command wire or Windows' exact version-124 input.

#### P2 — small causal checks, then the affected gate

1. Cover all four option combinations through public commands, pure placement
   and the independent reference. Use otherwise-legal low-level class-`0x4000`
   and road-over-ramp controls that change outcome only with the relevant option.
   Keep class-`0x1000` floor/slope and bit-4 controls under both settings.
2. Verify rules digests differ, save/reload preserves the choices, and original/
   enhanced atomic/live mixed-batch decisions agree. Use controls where old
   defaults and explicit enhanced values would produce different outcomes, so
   testing a copied default cannot pass the policy wiring by accident.
3. For the stream bump, start from a valid current save/replay control, change the
   version and correctly recompute its integrity seal. Assert version refusal,
   not checksum damage. Explain golden changes from the version word and the two
   explicit rule bytes; do not copy hashes from the implementation as an oracle.
4. Run the cheap checks first, then focused Debug and ASan/UBSan road/config/
   save/version/replay cases via the canonical gate. Require the relevant public
   contracts and reconcile exact inventories. Run one final Debug opening canary
   with explicit enhanced road rules and fresh-process honest/changed-road replay.
   If the stream bump makes old logs incompatible, retain them as old evidence
   and generate the required final-version recording once; do not claim replay
   reuse across incompatible formats.
5. Reuse the unchanged portions of the earlier gate/mutant evidence with their
   revisions. Recheck the controls affected by policy routing and state layout;
   no full R0–R5 repeat, full-ASan opening, renderer corpus, GPU/adapter matrix,
   coordinator or general runner campaign is assigned.

#### P3 — narrow review, report and stop

Obtain independent read-only review and focused re-review of fixes, covering
actual Rules wiring, all policy combinations, persistence/version negatives and
atomic/live behavior. Correct the replay-compatibility wording from the review.
Keep original cursor/refund/removal unknowns explicit and do not claim cargo
recovery beyond the travelling-cart test's actual observation. Commit locally,
report exact code/docs/gate pins and future API/stream delta, and stop for
orchestrator review. The only additional work permitted here is E1 below.

#### MAP-ECON-1 E1 — parallel original-worker research and contract ONLY

**Objective:** establish the first authentic forester chain: existing eligible
tree -> worker -> Farm.raw -> existing production step -> HOLZ output -> existing
road-connected warehouse cart. The canary must ultimately distinguish new wood
from the wood carried by the ship. Pre-stocked producer output proves delivery,
not harvesting/production. Do not substitute a later-unlocked charcoal building
or ignore original progression to obtain an easy green opening.

1. Read `docs/sim/production.md`, `docs/re/production.md` §§2/4.2/4.3/5 and its
   open questions, `docs/re/{annowelt,transport}.md`, `docs/sim/state.md`
   §§4.9/5.9 and D12/T22/T24, current road/ship-start/cargo contracts and
   `docs/formats/{anlagen,figuren}.md`. Older statements that market carts are
   unimplemented are superseded; missing harvest behavior is not.
2. Read `src/sim/src/{driver,production,transport,scheduler,figure,types}.cpp`,
   corresponding headers, `tick.hpp`, state fields/save/load, original registry,
   `{production,transport,driver_classes,figure_removal,founding_assets}_test.cpp`
   and `tools/{figure_digest_ref,sim_stream}.py`. The farm dispatch currently
   returns job 0 and the forester worker body is not implemented. Do not disguise
   a resource tile as an existing building-carrier target.
3. Parse and pin original forester 801 / figure 102 facts and units. Trace farm
   tick `AnnoWelt 0x1006ea90`, figure-type slot `+0x54` (AnnoGame loader
   `0x1000fe00`, vtable `0x10021d10` installed at `0x10011129`), worker send
   `0x10072d20`, entrance prerequisite `0x10072b50`, job factory `0x1003b0e0`,
   harvest/repeat lead `0x1005c260`, and farm raw receiver `0x1006ee70`.
   Follow only the identified forester path and its required callees. Existing
   tools/private environments are preferred; no home-directory or bytewise
   whole-binary scanning campaign.
4. Establish job identity, eligible mature-tree classification, search order/
   bounds, integer units/capacity, harvest amount and duration, tree depletion/
   replacement, return, repeat, cancellation, home removal, target changes and
   contention. Reconcile raw resource and produced-goods units from the recipe;
   do not assume equal-ton conservation across a conversion. Distinguish evidence
   from adaptations and original-runtime observations still NOT_RUN.
5. Draft `docs/re/forestry.md` and `docs/sim/forestry.md` with module hashes,
   addresses, hand-derived discriminating examples and the minimal proposed state/
   commandless scheduler lifecycle. Add `tools/forester_ref.py` and synthetic
   vectors only for established pure rules that benefit from an independent
   reference. No src/ changes or seeded raw/output, fixture trees, loadout edits,
   relocation or interim-return seam. While P2 gates run, keep drafts in ignored
   cache; promote only at the coordinated checkpoint.
6. A fresh independent read-only reviewer must challenge the evidence, units,
   state/lifetime plan and causal examples. Fix findings and obtain focused
   re-review. Stop with a genuinely ready contract or the exact unresolved
   mandatory predicate/address. If planting/regeneration is inseparable from
   the harvest lifecycle, report that scope dependency rather than silently
   omitting it. This research assignment does not release engine work; the
   conditional E2–E6 continuation below has its own approval requirements.

**Final stop:** reviewed road-policy correction with focused evidence, plus the
reviewed forester research packet or a precise research dependency. No harvest
engine until the conditional continuation below is released; no sustainable-
economy, UI/render integration or new Windows dependency.

### MAP-ECON-1 E2.0 → E2–E6 — REVIEWED; CLOSE-1 ABOVE IS CURRENT

**Disposition, 2026-11-03:** candidate `78a77625`, clean gate `a4982f51`.
E2.0 is accepted; the real forestry opening and valid gate evidence are retained.
The direct review requires the bounded public-termination correction and small
proof checks in CLOSE-1 above. The following release/stop wording is assignment
history. Do not restart E2.0 or E2–E6.

**Owner authorization, 2026-11-03:** once the road correction and forester
contract are approved, the Mac executor may implement and test the complete
headless harvesting -> wood production -> warehouse delivery loop, with an
independent review of every substantive package and a final integration review.

#### Release conditions and ownership

1. Road acceptance and the remaining contract condition are recorded here and in
   the [binding correction review](reviews/2026-11-03-roadclose2-forestry.md):

   | Required input | Orchestrator disposition / exact pin |
   |---|---|
   | Corrected road code/tests and policy/state contract | **ACCEPTED** — `d9a056823ad1aedfb7c41bf8d07b4dc6c965e39f`; clean focused gate `d0e4ae601da7281388d2e26d10fb9ae54ee6751a`; reviewed candidate `362f99ab65e5b4bac8a45f64f778bab0ec299365`; retained policy/state 125 from `eae9f4ae` |
   | E1 forestry evidence, contract and reference | **REVIEWED; E2.0 CORRECTIONS REQUIRED BEFORE ENGINE WORK** — input `362f99ab65e5b4bac8a45f64f778bab0ec299365`, as amended by the linked binding review; exact correction commit/independent approval must be recorded in the executor report |

   The orchestrator explicitly authorizes E2.0 and conditional continuation:
   once the narrow independent reviewer approves its specified corrections,
   record that exact revision and approval, then continue E2–E6 without another
   owner/orchestrator permission turn. This delegation is limited to the
   corrections stated below; a different necessary semantic choice or failure
   to reproduce the original-byte facts stops the affected transition for the
   orchestrator. Do not call the unamended E1 contract accepted. No engine
   (`src/`) work starts before E2.0 approval; package/final reviews still apply.
2. Use the existing Mac branch/worktree and the accepted input revisions,
   preserving newer owned work. Read the acceptance records and current main
   packet without merging main. No app/render/W4 changes, new Windows dependency,
   coordinator/general runner campaign, original-file edits or candidate install.
   Windows W4-OPENING-1 stays on exact
   `0c401c8d084cfd9feb8aec846595291f87f22cce` until a separate integration task.
3. Scope is the reviewed forester worker and existing eligible trees, connected
   to existing farm production and carts. The evidenced cut-to-young transition
   and its existing growth sweep are included in the lifecycle and tests. Other
   resource-worker families, separate planting systems, full progression and
   sustainable forestry balance are separate. Resolve any further inseparable
   scope dependency before release; do not silently trim the original lifecycle.

#### Read order after release

1. Read main `AGENTS.md`, `CLAUDE.md`, PLAN §§0.1–0.4, the evidence contract,
   testing README, this packet and both exact acceptance records.
2. Read the approved `docs/re/forestry.md`, `docs/sim/forestry.md`, references/
   vectors and E1 review dispositions. Follow the approved clauses and units;
   earlier research leads are not substitute implementation rules.
3. Read `docs/sim/{production,road-placement,ship-start-integration,ship-cargo}.md`,
   applicable `docs/re/{production,transport}.md` clauses, state/figure/driver
   lifecycle contracts and `docs/formats/{anlagen,figuren}.md`.
4. Read current `src/sim/src/{driver,production,transport,scheduler,figure,types}.cpp`,
   their headers, `tick.hpp`, `state_fields.hpp`, save/load/registry validation,
   `src/sim_load/src/original_registry.cpp`, the related production/transport/
   figure-removal tests, approved opening helpers, `tools/forester_ref.py` where
   established, `tools/figure_digest_ref.py` and `tools/sim_stream.py`.

#### E2.0 — finite contract/reference corrections before engine work

Read the entire linked correction review. This is docs/reference/vectors and
small contract checks only; no `src/` change, new runtime research campaign or
repeated compiled gate. Parent owns the corrections; one independent read-only
reviewer checks them, including the public reference controls.

1. **Faithful placement preserves bit 29.** Confirm the review's two short
   original-byte excerpts against the recorded module identity: placement
   `0x10082442`/`0x100824a7` masks with `0x3fffffff` (clears bits 30–31,
   preserves 29); the claim setter `0x100878da` shifts by 29. Correct
   `docs/re/forestry.md`, the relevant inherited mask text in
   `docs/re/transport.md`, `docs/sim/forestry.md`, `tools/forester_ref.py`
   and the affected vectors. Split placement from clearing. Neither clears
   faithful claims; cut/abort still does. Enhanced invalidation is an explicit
   adaptation under the false-default hygiene option. Withdraw the claimed
   faithful placement-drop/second-worker sequence; do not preserve it as an
   original fact merely because the reference once matched it.
2. Add discriminating controls: faithful `A+ P` keeps the bit; faithful clearing
   also keeps it; a legal worker unclaim clears it; enhanced invalidation and
   owner checks retain their separately specified results. The longer
   `A+ P B+ A- -> 0` ends identically under the wrong mask model, so it is
   insufficient alone. Temporarily restore the wrong placement clear: the
   intended short assertion must fail with the normal-unclaim control green.
3. **Initial delay persists.** Define the five-tick remainder as saved state,
   consumed once across short visits, with surplus elapsed ticks passed on.
   Re-entering Start after a cut does not restart the delay. Add hand-derived
   reference controls for five one-tick visits vs one five-tick visit, zero time,
   surplus time, and save/reload after two ticks. E2 implements the fields and
   independent stream bytes; E3 tests the actual scheduler. Do not retain the
   assumption that visits always arrive at roughly 50-tick intervals.
4. Fold in the linked lifecycle/ledger clarifications: Farm-home spawn origin
   versus valid home-gone/Done states; cut writes only the documented tile fields,
   not recomputed walk code/mask; disjoint physical stores (reservations are not
   extra goods), cart loading versus receipt, expired historical cargo and the
   correct sign for ship loading in the city ledger. Correct the stale example:
   arrival at 22,500, unused time 17,500. No new transport or format behavior in
   this correction.
5. Run the bounded public reference contract and all current vectors plus the
   new controls; require nonzero actual execution and intended mutant failure.
   Do not repeat 121-case compiled gates for these pure text/reference changes.
   Preserve the C2-1 evidence at its original pin. Independently review only
   the corrections and new claims, fix findings and obtain focused re-review.
   Record the exact correction commit, reviewer outcome and retained limits.
   Then **continue E2–E6 automatically**. Stop only for a new necessary semantic
   conflict or genuine dependency that the binding review does not settle.

#### E2 — immutable worker facts, minimum state and persistence

1. Implement only the immutable facts and active-job state required by the
   approved contract. Parse/derive job and resource behavior from original
   content and the approved evidence mapping. Original IDs 801/102 may identify
   real-file test fixtures; they must not select engine behavior. Do not replace
   the forester with the workshop-carrier job or the D12 timed-return stand-in.
2. Use an explicit resource target representation where needed; do not fabricate
   a BuildingId for a tree or persist a pointer into a mutable tile/registry.
   Define validation for missing/replaced targets and home/job ownership, using
   the lifecycle agreed in E1. Keep unsupported jobs explicit.
3. Include actual new registry facts/state in their appropriate digests and
   field-by-field serialization. Bump the next free stream version only for the
   actual new contract, after the accepted road version. Update independent byte
   transcriptions/goldens with a field-level explanation. A valid positive control
   and correctly resealed old-version negative must distinguish version refusal
   from checksum damage. Do not rewrite expected hashes from C++ output alone.
   The approved dispatch draw also needs its named DrawSite and appropriate
   PRNG-timeline identity/reference updates; a changed draw order is not merely
   a stream-layout change or a reason to blindly regenerate unrelated goldens.
4. Keep the design data-driven and bounded by the existing content/configuration.
   Test another data-defined type and owner without engine edits. No generic job
   scripting framework, new hardcoded type/player limits or unrelated farm port.
   Obtain independent review and focused re-review before E3.

#### E3 — selection, travel, harvest and return

1. Implement the reviewed lifecycle through normal driver/scheduler dispatch,
   existing walking machinery and serial effects. Preserve deterministic search,
   PRNG use, transition order and units. Bound work by the approved search rules;
   no whole-world scan per worker or retry loop that makes no progress.
2. Revalidate the live resource at the consuming transition. Selection alone
   cannot authorize later harvesting after depletion/replacement. Two workers
   selecting the same final resource units must not duplicate them; legitimate
   sharing and ordering follow the contract. Cover same-type replacement too
   where object identity matters.
3. Perform resource consumption/transformation, cargo credit and home return in
   the approved order through existing resource/tile writers and invalidation.
   Release reservations/home counters exactly once. Test target loss, home
   demolition before collection and while carrying, blocked return, full/missing
   receiver, cancellation, expiry and repeated cleanup as applicable.
4. Account for cargo through actual cleanup/expiry, beyond the instant a job
   ends. No unexplained duplication/loss, invented refund, resource resurrection
   or guessed recovery rule. Any evidenced original loss is explicitly recorded
   and distinguished from an accidental engine loss. If the approved contract
   leaves a necessary transition unresolved, stop that transition for clarification
   and continue independent checks; do not substitute an apparent-success path.
5. Review the package independently, including alternate orderings and failure
   cleanup; fix findings and obtain focused re-review before reporting completion.

#### E4 — connect harvesting to production and road-dependent delivery

1. Start the forester's raw/output storage at zero in the integration fixture.
   Demonstrate the causal chain: eligible resource harvested -> worker cargo ->
   Farm.raw -> recipe consumption -> new HOLZ output -> cart cargo -> warehouse.
   A sent/returned worker, reservation, capacity or pre-stocked output is not
   evidence of production. Reuse the established production/cart bodies.
2. Hand-derive one miniature resource/recipe/delivery sequence independently.
   Record each boundary in its real units. Resource input and wood output are a
   conversion, not assumed equal tonnages. Separate material retained, consumed,
   converted, delivered and any contract-authorized loss; unexplained terms must
   equal zero rather than merely being printed.
3. Include no-resource, unreachable-resource and disconnected-road controls,
   plus applicable switch/full-store cases. Follow the actual movement layers;
   do not make all workers road-dependent. Existing cart clipping/overflow does
   not authorize discarding a forester's cargo at another transition.
4. Use a few purposeful negative controls, such as credit without depletion,
   return without raw-input delivery, or production without input consumption.
   Verify the intended boundary assertion fails while its legal control executes.
   No generic mutation framework or broad repeated campaign is required.
5. Obtain independent package review of production callers, lifecycle tests and
   the material ledger, with focused correction/re-review.

#### E5 — one authentic opening canary, save/reload and fresh-process replay

1. Load an unchanged ship-start candidate and original loadout. With public
   commands and normal advances only: sail, found, unload, build a legal forester
   and connecting roads, then harvest existing eligible trees and deliver newly
   produced wood. One seat is enough for this expensive canary; focused cases
   cover additional workers and owners.
2. Search for sites on copies; do not relocate the real ship or seed fixture
   trees, Farm.raw, producer output or warehouse stock. No grants, free-build,
   interim-return seam, progression bypass or loadout change. If stock room is
   needed, use approved load/unload commands and record the transfers. If the
   unchanged start cannot support the sequence, report the exact constraint.
3. Separate initial ship wood and stock, construction costs, legal cargo transfers,
   harvested raw material, conversion, cart cargo and warehouse deliveries.
   Assert a positive newly produced delivery and every ledger equation. Existing
   starting wood or an unexplained warehouse increase cannot satisfy the test.
4. Save/reload at an outbound/harvesting state and a carrying/returning state.
   Resumed and uninterrupted outcomes, final bytes/hash and required completion
   timing agree under the contract. Cover these with small fixtures where possible
   instead of multiplying full sailing variants.
5. Record and replay the real canary in a fresh process with matched checkpoints.
   A changed legal command must cause the expected divergence; demonstrate that
   the changed command executes, so malformed bytes or an unrelated refusal do
   not supply the negative proof. Keep exact source/tool/binary/log identities.

#### E6 — finite gates, final integration review and stop

1. Order work as cheap reference/negative checks -> focused Debug -> one real
   canary/replay -> focused ASan/UBSan. Freeze all relevant source/tool inputs;
   coordinate helper writes around gates. Use the canonical workflow, mandatory
   contracts and exact inventories. Preserve first failures and report selected,
   executed passes, failures, skips, empty and incomplete outcomes separately.
2. Check affected driver/figure/resource/production/transport, persistence and
   replay dependencies. Reuse unaffected evidence explicitly. Broaden only for a
   named changed dependency or failure; no renderer/GPU matrix, full-ASan sailing
   campaign, coordinator repairs or unrelated testing-framework project.
3. A fresh independent integration reviewer inspects the frozen combined diff,
   callers and raw evidence across E2–E5. It checks actual new production, policy
   fidelity, target contention, resource/cargo accounting through cleanup, active-
   job persistence and causal replay controls. It does not author fixes or repeat
   long gates. Correct findings and obtain focused re-review.
4. Update the forester contract and report with exact accepted-input/code/test/
   documentation pins, evidence index, performed reviews, retained failures,
   limitations and future Windows API/stream delta. Commit locally and stop for
   orchestrator review; no task-code push, main merge/rebase, map install or UI
   integration. Unavailable Windows/Linux/original-runtime/GPU checks stay NOT_RUN.

**Stop:** the reviewed harvesting lifecycle produces new wood and delivers it
through the normal simulation, with the real opening, required failure cases,
active-job save/reload and fresh-process replay demonstrated. This is a first
complete harvest/production/delivery loop, not acceptance of every worker family,
other planting systems, sustainable forestry balance, full progression, multiplayer
scale or target-host performance. Further gameplay/UI work waits for its own task.

### MAP-ROAD-1 — construct and remove ordinary land roads · REVIEWED, POLICY FOLLOW-UP ABOVE

**Review checkpoint, 2026-11-03:** local docs/report `242c3433`, clean gate/code
`6e721468`. Read the [direct review](reviews/2026-11-03-map-road1.md). The
tile-object path and valid 46/46, 135/136 and 1059/1061 execution evidence are
retained with their named opt-in skips. The two unconditional road adaptations
need the bounded policy correction above before full acceptance. R0 is closed;
do not restart R0–R5. Windows' fixed input and opening packet are unchanged.

**Start:** existing Mac worktree, `task/MAP-2-original-like`, accepted code/tests
`3276a0bbef3381a348c8bd2e912fdc83b5ee8568`, documentation head
`cf78d212f64b5a69633de70341e00e2b5a724e16`. Preserve newer owned work. Fetch/read
main instructions without merging main. CARGO-3 is accepted for its local scope;
no new cargo repair round is assigned.

**Outcome:** an owned city can construct and remove an evidenced family of
ordinary land roads through `BuildCmd`/`DemolishCmd`. Roads are tile objects,
not buildings. New route searches and road-dependent market-cart delivery see
those changes. This prepares authentic local logistics after the ship-start
opening; it does not claim a new production chain or live-window integration.

**Parallel boundary:** Windows owns W4-OPENING-1 and still consumes exact
`0c401c8d084cfd9feb8aec846595291f87f22cce`. Do not touch app/render/W4 source,
its worktree or processes, or request integration of this new Mac head. No
coordinator, test-support or general test-runner work, candidate selection/install,
main merge/rebase, source-branch push, pruning or original-file change is assigned.
Original files remain read-only. No new `RoadsCmd`, fake `BuildingId`, full-road
framework, bridge/water-road/wall/plaza/upgrade feature or new pathfinder belongs
in this unit.

#### Read order and preflight

1. Read main `AGENTS.md`, `CLAUDE.md`, PLAN §§0.1–0.4, the evidence contract,
   testing README, this packet and the linked CARGO-3 review. Read
   `docs/plan/{lan-mvp,lan-endless}.md` for direction; older status/start-mode
   descriptions do not supersede the current ship-start decisions.
2. Read `docs/sim/ship-placement.md`, `docs/re/ship-placement.md` §§1–3,
   `docs/sim/state.md` §§5.9/8.4 and T33.4/T33.5, `docs/re/transport.md` §3.4
   (especially §3.4.17), `docs/re/ui-and-placement.md` and
   `docs/formats/anlagen.md`. The road flag/height branches already decoded
   there are the starting point, not a reason to restart terrain research.
3. Read `src/sim/include/anno/sim/{command,apply,placement,walk_cells,walk_search,
   transport}.hpp`, `src/sim/src/{apply,placement,walk_cells,walk_search,transport,
   create}.cpp`, tile-layer writers/clearers, registry classification and
   `src/sim_load/src/original_registry.cpp`. Follow actual current filenames.
   Inspect `check_record`, atomic scratch budgeting, `spend`, `place`, removal,
   `place_object_record` and `clear_object_record` before proposing a new seam.
4. Read `src/sim/tests/{command_build,command_apply,placement_rule,create_remove,
   walk_cells,walk_search,transport,founding_assets}_test.cpp` and
   `tools/{anlagen_roads,test_anlagen_roads,placement_ref,placement_emulate}.py`.
   Existing road-aware cart tests
   and loaded-road fixtures do not prove command construction or actual production.
5. Record current Mac CPU/RAM/disk and contention; reuse valid calibration with
   admission enabled. Keep the display awake for owned work. Give each helper
   the exact-PID/session ownership rule; no pattern kills, coordinator changes,
   overlapping heavy gates or interruption of another executor.

#### R0 — finish the one documentation correction

Correct `docs/sim/ship-cargo.md` §12.6: the `a=2,b=0` row with five records is
bounded by `min(5,2)=2`, and cannot distinguish the mutant because `a+b == a`.
Replace "bounds by records only" with this explanation. This is wording only;
do not change cargo code, tests or versions or rerun cargo gates. Check the
diff and include the disposition in the report.

#### R1 — freeze a bounded road contract before implementation

1. Inventory the real registry road families and select an ordinary land-road
   family whose behavior can be established. Define eligibility from parsed
   type/classification facts, not an ID/name/player switch or one test type.
   Include all types covered by that predicate; explicitly refuse unsupported
   classes. Retain a path for data-defined types without implementing mod loading.
2. Follow the existing command and City::Place path and the relevant original
   tile writer (`AnnoWelt.dll 0x10082040`), `RemoveAt` (`0x10028fd0`) and clear
   path (`0x10082c30`). Pin terrain/height, overlap/duplicates/replacement, costs,
   ownership, removal/refund and required neighboring-tile effects. The
   `0x1000`/`0x4000` road branches are already partly decoded. Trace only the
   remaining predicates required by the selected family.
3. Write `docs/re/road-placement.md` and `docs/sim/road-placement.md`, linking
   the existing placement evidence rather than copying it. Include addresses,
   hand-derived examples and an independent reference where it adds useful proof;
   reuse the existing inventory/placement tools where applicable. Separate
   original rules, the established server-derived-height adaptation and unknowns.
   Preserve existing stream-order/per-record behavior and `Rules::build_atomic`;
   do not invent whole-drag atomicity or an arbitrary refund rule.
4. An independent read-only reviewer checks the contract/reference and original
   evidence before R2. Fix its actionable findings and obtain focused re-review.
   Proceed after approval without another owner pause. If a required rule is
   unresolved, identify the exact predicate/address and continue independent
   preparation; do not silently substitute a guessed rule.

#### R2 — authoritative road construction through BuildCmd

1. Extend the existing checked-placement path to distinguish buildings from
   supported tile objects. Use server terrain, parsed costs and normal ownership
   checks. Tile roads must not allocate a building/list record, consume a building
   serial or return a fabricated valid BuildingId. Define truthful result semantics.
2. Preserve sequential and configured atomic policies, including mixed building/
   road batches. The scratch budget must account for occupancy, gold/goods and
   building serials according to actual record kind, in command order. A refused
   record has no partial charge/state mutation; preserve the existing whole-batch
   policy rather than changing it for the road implementation.
3. Use existing tile/walk writers and local invalidation. Do not rebuild every
   island, scan all cities or add a global connectivity cache per road tile.
   Test a data-defined second road type and another owner without code changes.

#### R3 — removal and routing across road changes

1. Implement the reviewed ordinary-road removal through DemolishCmd. Preserve
   ownership/refund rules, unrelated building records, terrain, neighboring tile
   state and serials. Verify duplicate removals and building/road mixtures.
2. A new search must observe construction, disconnection and reconstruction
   through the normal production path. Examine an already travelling cart when
   its road changes; prove the implemented safe behavior and label fidelity limits.
   Do not claim immediate replanning from a static search or introduce the
   deferred fine-route/retry subsystem to avoid stating a precise dependency.
3. Keep movement-layer distinctions: road-dependent market carts and workshop
   carriers that can cross open land must retain their respective original rules.

#### R4 — causal behavioral proof and one real-file canary

1. Through public encode/decode/apply paths, cover legal placement, authoritative
   height, wrong terrain/owner, affordability, duplicates, partial/sequential and
   atomic mixed batches, removal and save/reload. Pair failures with otherwise
   legal controls and assert exact reasons and unchanged state where required.
2. In a declared transport fixture, show a producer unreachable before road
   construction, reachable afterward, and unreachable to a new search after a
   road break. Assert entrance connectivity and actual goods movement, not just
   a tile type. A producer pre-stocked by the fixture proves delivery, not production.
3. Run one real-registry/real-terrain build/remove/rebuild canary after the accepted
   ship opening/unloading, using original supplies and public commands without
   grants or relocation. Resolve exact affordability/site constraints from data.
   Keep this separate from the synthetic transport fixture. Record/reload/replay
   the relevant changes in a fresh process and use a changed legal road command
   as a divergence control. Do not multiply full sailing variants unnecessarily.
4. Use focused negative controls for the important new boundaries: for example
   bypass the terrain predicate, allocate a building serial for a road, fail to
   update walk records, or omit payment. Derive an expected result by hand and
   check that the intended assertion fails while its legal control still runs.
   Do not create a new generic mutation framework or retest unrelated invariants.

#### R5 — finite local gate, review and stop

1. Run cheap references/contracts and focused regressions before the canary and
   focused ASan/UBSan. Freeze source/tool/binary identities before each canonical
   gate; retain exact inventories, first failures and honest executed counts.
   Update serialization/version only if the actual state/stream contract requires
   it, with independent positive and correctly sealed negative controls.
2. Every substantive package receives independent read-only review of production
   callers, test causality and raw evidence. Fix findings and obtain focused
   re-review before completion. A final integration reviewer checks placement,
   costs/ownership, removal, tile/walk lifetime, mixed atomic behavior and replay.
   Reviewers must not implement their own fixes or duplicate long campaigns.
3. Reuse unaffected evidence with a dependency explanation. No new full renderer
   corpus, GPU matrix, broad sanitizer opening campaign or infrastructure repairs.
   If a concrete regression requires broader checks, name that dependency first.
4. Commit small concerns locally and report code/docs/gate pins, coverage, known
   limits and the future Windows API delta. Do not push task code or change its
   current integration input. Windows/Linux target-host and original-runtime
   checks remain explicitly NOT_RUN when unavailable.

**Stop:** a reviewed ordinary-road contract, working public-command placement/
removal, road-dependent delivery and the real-file canary/replay are demonstrated
locally, with no blocking finding. Stop earlier only for a specific required rule
or defect, not unrelated historical unknowns. No renderer/UI integration, new
production-chain feature, trade routes or further cargo work starts here. The
subsequent candidate is MAP-ECON-1: actual production from real inputs/recipes
and delivery, not delivery of pre-stocked fixture output; it is not assigned yet.

### MAP-CARGO-3 — align cargo hold bounds and retain regression coverage · ACCEPTED LOCAL HEADLESS SCOPE

**Completion checkpoint, 2026-11-02:** code/tests and clean gate source
`3276a0bbef3381a348c8bd2e912fdc83b5ee8568`, documentation head
`cf78d212f64b5a69633de70341e00e2b5a724e16`; ordinary publication remote-verified.
Read the [direct correction review](reviews/2026-11-02-map-cargo3.md). Gates
reconcile at 44/44, 62/62 and 63/63 with zero skips/failures; honest replay is
89/89 and altered load diverges at entry 35594. F-L1 and the shared predicate/
combined reload coverage are closed. R0 above handles the one wording correction.
The packet below is acceptance history; do not repeat C1–C3. Windows' fixed
input `0c401c8d` and W4-OPENING-1 assignment are unchanged.

**Start:** existing Mac worktree/branch, accepted loading code/tests
`80f9eeacd1012cd58f3a010b8d5c6018810dedad`, documentation head
`0aed8ec0334e9bfbdbb137d3662ac9e958c83887`. Preserve newer owned work.
Read [the loading review](reviews/2026-11-02-map-cargo2.md) and current main
instructions without merging main. This packet approves the original-evidence
correction below; no additional owner decision is needed between its steps.
It does not restart CARGO-1/2 or add work to W4-OPENING-1.

**Outcome:** when navigation facts exist, both manual cargo commands address
only the first `hold_count_a` cargo slots. The sailing factor's use of `a+b`
does not establish transfer eligibility. Keep the established missing-nav
unload fallback explicitly documented as an existing adaptation; changing that
separate metadata policy or ship-class configuration is outside this packet.

#### Read order and boundaries

1. Read main `AGENTS.md`, `CLAUDE.md`, PLAN §§0.1–0.4, the evidence contract,
   testing README, this packet and linked review. Read the accepted CARGO-2
   report, `docs/re/ship-cargo.md` §§11.3/11.8, and
   `docs/sim/ship-cargo.md` unload step 9 and load steps 9–12.
2. Read `src/sim/src/ship_cargo.cpp`, its header, `unload_test.cpp`'s data-defined
   ship case, loading's boundary cases, the independent cargo vectors and the
   `run_opening` reload masks in `founding_assets_test.cpp`.
3. Keep Windows input `0c401c8d`, its work and all original files untouched.
   Use current host capacity, admission and calibrated job ceilings. Keep the
   display awake for owned work. Give every helper agent the ownership rule:
   stop only an exact recorded owned PID/session, after checking its identity.
   No `pkill -f`, `killall`, name/pattern kills or stopping another coordinator.
   Use bounded commands; record any further incident immediately.

#### C1 — fix the evidenced unload bound with a causal regression

1. Preserve a cheap before-fix result: a data-defined ship with `a=2`, `b=1`
   and at least three actual holds currently unloads index 2. The existing
   `unload data` test even expects that success. From `0x100ac476`, original
   valid cargo indices end at `a-1`. Construct otherwise-legal owner, goods,
   range and stock so `BadIndex` is the only intended refusal.
2. For types with nav facts, bound unload by `min(actual holds, hold_count_a)`.
   Correct the old test expectation rather than preserving an implementation-
   derived oracle. Pair legal index `a-1` with refused index `a`; assert exact
   transfer/conservation for the former and unchanged state for the latter.
   Cover actual holds shorter than `a`, extra records beyond `a+b`, and zero
   `a` if valid under the registry contract. No guessed meaning for the second
   group, new state field or change to sailing's denominator.
3. A focused mutant restoring `a+b` must fail the extra-group refusal while
   the legal control executes. Update the unload contract/F-L1 disposition
   and any affected independent vectors. Preserve the no-nav fallback and
   identify its limited original-parity claim; do not silently broaden it.

#### C2 — share the identical range predicate and retain the old reload cut

1. Now that unload is being touched, use the existing outline-ownership helper
   for both load and unload; remove the duplicate 24-read implementation.
   Keep range semantics, ownership, refusal order and all unrelated transfer
   behavior unchanged. Existing positive/negative outline cases for both commands
   must run through the shared production caller; do not test only the helper.
2. Retain one real opening reload variant that reloads after founding AND after
   unloading in the same run. Adjust the current masks without adding another
   full replay variant: for example change mask 5 to 7 in `{5,10,17}`. Preserve
   coverage of all resupply boundaries, ledger/hash/save comparisons and both
   altered-command checks. This is restoring prior coverage, not a new matrix.
3. Keep the disclosed ship-class limitation and open cargo arms as future work.
   No result-schema, mod runtime, loading UI or further cargo feature belongs
   in this correction.

#### C3 — focused acceptance and stop

1. Run cheap references/contracts and the affected unload/load/range/codec/
   persistence cases in Debug and ASan/UBSan via the canonical workflow. Use
   explicit selected/required inventories and fresh owned evidence paths.
2. Run the changed real-file opening case once in the final Debug selection,
   and replay the retained honest/changed-load recordings against the corrected
   binary to show the normal four-hold opening is unchanged. Do not regenerate
   a long recording unless that replay or another actual failure requires it.
   Reuse CARGO-2 L5 for unchanged code, with the dependency explanation. No new
   full-sim, full-ASan opening, renderer, GPU, coordinator or performance matrix
   is required for this bounded change. Expand only for a concrete new failure
   or changed dependency and explain why.
3. One independent read-only correction review covers C1/C2, test causality,
   shared callers, cleanup ownership and evidence. Correct findings and obtain
   narrow re-review. Record exact code/docs/workflow/binary pins and the changed
   behavior for earlier replays addressing second-group slots. Keep state and
   wire identity decisions explicit under the existing policy.
4. Commit the small correction locally, report in `REPORTS.md`, and stop for
   orchestrator review. No main merge/rebase, task-code push, pruning, candidate
   install, Windows edits or unrelated infrastructure changes. This later Mac
   pin does not replace Windows' accepted `0c401c8d` input automatically.

### MAP-CARGO-2 — load supplies from an owned warehouse onto a ship · ACCEPTED LOCAL HEADLESS SCOPE

**Completion checkpoint, 2026-11-02:** accepted code/tests
`80f9eeacd1012cd58f3a010b8d5c6018810dedad`, clean L5 gate source `abe568d6`,
docs `0aed8ec0334e9bfbdbb137d3662ac9e958c83887`. Read the
[direct loading review](reviews/2026-11-02-map-cargo2.md). The docs-only P2
handoff gap is corrected, the six L5 verdicts and real resupply replay were
independently checked. Known prior unload finding F-L1 is assigned to
MAP-CARGO-3 above. Do not repeat L1–L5. Windows continues with `0c401c8d`;
this acceptance does not expand its live-opening task.

**Orchestrator assignment, 2026-11-02, following the owner's request for the
next Mac task.** The accepted opening already sails, founds, unloads and builds.
This independent gameplay unit lets players take city supplies aboard again
for resupply and expansion. It is headless manual loading only.

**Start:** reuse the existing Mac worktree and `task/MAP-2-original-like`, based
on accepted `0c401c8d084cfd9feb8aec846595291f87f22cce`. Preserve any newer work
and explain its ancestry; do not reset to that pin. The completed CARGO-1 packet's
stop is superseded for this new assignment. Continue L1–L5 without another owner
prompt, within the rules established by L1's corrected independent review.

**Parallel boundary:** Windows continues W4-WIN-VERIFY and later consumes
exactly `0c401c8d` for W4-OPENING-1. Do not add this new loading command to its
requirements, move its accepted input or edit any app/render/W4 files. New Mac
work is a later review candidate, not part of the accepted opening until reviewed.

**Outcome:** public commands transfer a requested legal quantity from the named
owned warehouse's city into the named owned ship's hold. Actual stock debit
equals hold credit, limits and mixed/empty holds follow the reviewed original
rules or explicit hardening adaptations, and save/replay reproduce the result.
No stock grant or direct mutation can satisfy the real-file canary.

**Excluded:** automatic routes, buying/selling or foreign/allied warehouses,
crew disembarkation, UI/rendering, combat, a mod runtime, general ship-eligibility
redesign, unrelated economy/road work, coordinator and shared-runner repairs.
Do not reopen accepted placement/navigation/unloading or repeat their matrices.

#### Read order and preflight

1. Fetch `origin/main`; read its `AGENTS.md`, `CLAUDE.md`, PLAN §§0.1–0.4,
   `docs/testing/{evidence-contract,README}.md`, this packet and
   [the accepted cargo review](reviews/2026-11-02-map-cargo1.md).
   Read with `git show origin/main:<path>`; **do not merge main** into the
   task branch or copy its implementation files into yours.
2. Read the latest MAP-CARGO-1 report and
   `docs/re/{ship-cargo,ship-founding,production,fire-and-ships}.md`,
   `docs/sim/{ship-cargo,ship-navigation,ship-start-integration,state}.md`.
   The load arm is already located at AnnoWelt `0x100ac498–0x100ac8e1`;
   View's load handler is `0x1005a8d0`. These are starting evidence, not a
   complete loading contract. Reuse the existing private disassembly tooling.
3. Read the current command/codec/dispatch/replay, `ShipCargo`, `CityStock`,
   `Figure` holds, `FigureType`/NavFacts, original registry loader, CargoRules,
   state fields and `unload_test.cpp`/real opening canary. Follow actual paths
   under `src/sim/{include/anno/sim,src,tests}` and `src/sim_load`.
   Read `tools/{cargo_ref,sim_stream}.py` and cargo fixtures before deriving
   expected values from production code.
4. Confirm source/build/workflow identities, available RAM/disk/CPU load and
   tool versions. Reuse the Mac calibration when current capacity permits
   (16 compile, 8 light tests as ceilings); keep admission enabled. Keep the
   screen awake for owned work and release only your own assertion. Never
   change another executor's jobs, worktree or coordinator.

#### L1 — finish the original loading contract first

1. Trace the manual load click through the command sink to city debit and
   hold credit. Resolve the hold selector, goods echo, whole/fractional request
   units, stock rounding, per-hold versus aggregate capacity, and which type
   fields supply the limit. Read the necessary callees; do not infer loading
   by merely reversing the unload formula or using automatic-route behavior.
2. Pin ownership, eligible ship/warehouse, live range, moving/carried state,
   empty-hold goods assignment, same-goods top-up, different-goods refusal,
   duplicate holds, partial/full/zero transfers and normalization. Keep unknown
   independent arms separate. A rule required for this command must be traced
   before implementation; a precise true blocker is reported, never guessed.
3. Write a loading section in `docs/re/ship-cargo.md` and a proposed contract
   in `docs/sim/ship-cargo.md`: predicate/commit order, typed command/outcome,
   refusals, checked units, data-driven capacities/rules and original-versus-
   adaptation table. Existing range/authority semantics are reused where the
   evidence agrees. Shared visibility remains information, not permission.
4. Extend the independent Python reference and hand-derived vectors before
   engine code: empty/same/different goods, stock/hold/request limits,
   fractional stored balances, boundary values, partial transfers and competing
   requests. Keep loading and unloading expectations distinguishable. Obtain
   an independent read-only contract/reference review; correct it before L2.

#### L2 — add the authoritative loading command

1. Add one minimal typed command through existing native codec, dispatch,
   result and replay paths. Choose an unused tag without renumbering existing
   alternatives. Include independent hand-written bytes and wrong-length cases.
   Document the existing wire compatibility policy accurately.
2. Resolve the current ship/warehouse and city through existing indexes at
   apply. Validate L1's order and prepare an exact transfer plan before writes.
   The ship cannot gain more than the city loses; the city cannot lose more
   than its current available stock, and the selected hold cannot exceed its
   evidenced capacity. No wrapping, hidden clamping, whole-world clone or scan.
3. Use current registry/rules facts for capacities, goods and hold counts; no
   fixed ship 1502, warehouse 1302, player/seat, four-hold limit or building-
   material whitelist. Retain CARGO-1's disclosed ship-class limitation rather
   than starting a separate capability redesign. Explicitly refuse unsupported
   or missing required capacity facts instead of substituting a guessed limit.
4. Commit debit/credit together through suitable existing APIs, with the actual
   transferred amount in the command result. A refusal preserves serialized
   state, PRNG, gold, holds and stock; the existing host rejection notice is
   separate. Keep cargo-dependent speed updates at their established segment
   boundary. Add no transfer state unless required by the established rule.
5. New authoritative rule fields must be validated, serialized and hashed
   immediately. Bump state identity only when required; avoid gratuitous golden
   changes. Version-negative tests must reseal a valid control and assert the
   version-specific reason. Get focused independent implementation review.

#### L3 — verify conservation and meaningful refusals

1. Test through encode/decode and `World::apply`, starting from a legal nonzero
   positive control. Vary one predicate at a time and require the intended
   reason plus unchanged state. Cover L1's empty/top-up/mixed cases, holds and
   stock near bounds, partial capacity, repeat requests, target replacement,
   two ships competing for limited stock, another owner and data-defined types.
2. Assert exact debit, credit, report, untouched other holds/goods and per-goods
   conservation in a wide accumulator. Test load then unload with no Advance:
   combined goods must be conserved, though hold layout need not be restored
   unless the contract requires it. Cover stored fractional remainders without
   inventing fractional command requests.
3. Use focused counterexamples to actual write sites: credit without stock
   debit and debit beyond the hold's accepted credit. The relevant first
   transfer must fail the intended accounting assertion while its legal
   control executes. Use fresh state where later cascading failures would
   obscure the result; no broad mutation quota or repeated long voyage.
4. Verify save/load around loading, subsequent commands, replay and changed
   quantity/target divergence. Check cargo affects subsequent sailing segments
   according to the existing speed contract and crew stay carried. Independent
   review covers these tests and corrections before L4.

#### L4 — one real-file resupply canary

1. Start from the unchanged candidate/loadout through the accepted opening
   path: sail, found, unload and build. Reuse its voyage/helpers/recording where
   compatible rather than repeating sailing for every loading assertion. Any
   saved starting point must be reproducible by public commands and linked to
   its source/log identity; no machine-private cache becomes a required fixture.
2. Select legitimate remaining city goods and load them aboard through the new
   command. Exercise two different owners and at least one newly emptied hold
   and one evidenced top-up/partial case if the unchanged state supports it.
   Assert the scenario's actual policy and accounting preconditions. No grant,
   changed loadout/cost, relocation or direct transfer helper in the run.
3. Keep a ledger derived from original starting quantities and command outcomes;
   distinguish independently justified simulation consumption/production from
   transfer differences. In windows intended to have no economy changes,
   assert zero rather than absorbing unexplained differences into an observed
   remainder. A rival request must reach the intended ownership refusal.
4. Compare reload boundaries with uninterrupted state and outcomes; run one
   fresh-process replay and a changed-load negative. Reuse one complete
   recording where practical. Publish files only through checked writers and
   retain commands, hashes, status and timing. Review the actual no-grants path.

#### L5 — finite closeout and handoff

1. Freeze source/tools after focused corrections. Run the cheap references and
   contracts, affected cargo/codec/state/founding/placement/navigation/replay
   cases and ASan/UBSan, then one affected full-sim gate with original files.
   Include formats/load only if their code changed. Reuse unaffected U5/E1
   evidence with dependency notes; no render, adapter, heavy-render corpus,
   coordinator or performance campaign belongs to this unit.
2. Require exact test inventories, executed/skipped counts, diagnostic output
   and correct rejection/divergence reasons. Preserve first failures. Use fresh
   owned temp paths. A timeout allowance is disclosed per lane; a failure must
   be diagnosed before retrying, not erased by a longer global deadline.
3. One fresh independent integration reviewer follows input -> ownership/range/
   capacity -> stock debit/hold credit -> sailing/persistence/replay, checking
   the loading/unloading interaction and final canary. Correct actionable
   findings and re-review only the changed paths. Prose-only corrections do
   not reopen broad gates.
4. Report exact code/test/docs/workflow pins, the acceptance table, supported
   and unsupported rules, original facts/adaptations, source/binary/fixture
   identities, failed attempts and target-host NOT_RUN lanes. Give a small
   future Windows API delta; do not edit its UI or demand that its current
   opening task integrate loading before its existing playtest.

**Stop:** one independently reviewed local headless manual-loading candidate,
with conserved real supplies, meaningful refusals, reproducible save/replay and
the original unload path preserved. Commit small concerns on the existing Mac
branch and stop for orchestrator review. No task-code push, main merge/rebase,
force push, pruning, candidate selection/install, `game/` edit, TASKS edit or
Windows work. Preserve the accepted `0c401c8d` handoff and its evidence.

**Dispatch review:** independent read-only reviewer
`/root/review_windows_dispatch` approved this task packet with no findings.
This is documentation approval, not loading-rule or implementation acceptance.
All existing W4 packets and prior MAP acceptance/history were compared with
main `8b3deaf0` and remain byte-identical; whitespace checks pass.

### MAP-BUILD-1 — build on valid terrain after founding · ACCEPTED LOCAL HEADLESS SCOPE

**Completion checkpoint:** the required temporary-directory correction is fixed
in `9b6c547f` and independently reproduced as 12/12 concurrent passes. It is
included in the accepted Mac opening candidate `0c401c8d`; see the
[cargo review](reviews/2026-11-02-map-cargo1.md). No placement repeat is assigned.
The earlier review checkpoint and packet below remain acceptance history.

**Review checkpoint, 2026-11-02:** mechanics accepted at code
`ab5e6510952b0bbff6d1a2dbba1ab9f98f8034aa`, docs
`d9e3415affa1224341c5336d81f38786fabe9f9b`, within the supported headless
placement scope. Read the [orchestrator review](reviews/2026-11-02-map-build1.md).
The two new writer tests share fixed temporary paths across processes; fix that
under the bounded continuation below. It does not invalidate serial E1 or block
the already-running cargo trace. Do not restart A–E. The original packet below
is acceptance history; this checkpoint governs current status. Cross-platform
and live-window acceptance remain pending.

Continue the existing Mac `task/MAP-2-original-like` from reviewed head
`6ad66ac7ccf2ada673a76dfc4dd0f923572d443d`, tested code
`8ee8423bc6318a023ce4507db96e466d3f5547cb`. Preserve any newer owned work and
report its relationship to those pins. Read the
[MAP-NAV-3 review and decisions](reviews/2026-11-02-map-nav3.md).

**Outcome:** an ordinary building can be built through `World::apply` on
evidenced legal terrain with the correct placed height, while illegal terrain
is refused without charging or changing the world. The same authoritative
height reaches the building record, tiles, save/replay and object-aware navigation.
Preserve the real sailed-founding canary. If its new city cannot fund a build
without an unimplemented cargo transfer, prove building on a separately declared,
stocked scenario as specified in D4; the combined opening loop remains blocked
on unloading, not falsely complete. This is a headless gameplay unit; the
Windows executor owns the live window and renderer.

**Authorization:** finish A's bounded reporting correction, obtain its focused
independent re-review, and continue B–E without another owner confirmation.
B's corrected original-rule contract must be independently reviewed before its
dependent engine edits in C. Do not stop the whole packet for an independently
unresolved terrain class or unavailable Windows capture: identify the exact
dependency and continue authorized work whose rule is established. Do not claim
the whole outcome complete if a required ordinary-building path remains blocked.

#### Read order and ownership

1. Fetch `origin/main`; read its `AGENTS.md`, `CLAUDE.md`, PLAN §§0.1–0.4 and
   §0.7, `docs/testing/evidence-contract.md`, `docs/testing/README.md`, this
   packet and the linked review using `git show origin/main:<path>` or the
   existing read-only workflow checkout. **Do not merge main into your branch.**
2. Read your MAP-NAV-3 report and final review ledger,
   `docs/sim/{ship-navigation,ship-route-engine,ship-founding,state}.md`,
   `docs/re/{ship-founding,transport}.md` and OQ-052, OQ-NAVC-2/3 in
   `docs/open-questions.md`. Follow their latest superseding sections; historical
   statements that the whole founding terrain function is unread are obsolete.
3. Read `src/sim/include/anno/sim/{placement,site_checks,terrain_view,building,
   types,config,seams,walk_cells,island,route_cache}.hpp` where present, and
   `src/sim/src/{apply,placement,site_checks,create,founding,walk_cells,island_tick,
   route_cache,route_world,scenario_formats}.cpp`. Find the actual seam declaration
   and callers if a header moved. Read the building/placement format contract
   and original-registry loader; distinguish client bytes from server-derived data.
4. Read affected tests: `nav_scale_test.cpp`, `founding_assets_test.cpp`,
   `site_checks_test.cpp`, `terrain_view_test.cpp`, `create_remove_test.cpp`,
   `route_objects_test.cpp`, and the existing command, build/atomic-batch,
   replay and corpus tests. Inspect the independent terrain/placement fixtures
   before deriving expected values from the implementation.
5. Confirm clean source, build and workflow identities; inspect current Mac
   CPU/memory/disk contention and reuse calibrated jobs (16 compile, 8 light
   tests only while capacity permits). Keep memory admission on. Keep the screen
   awake during owned work; release only your own assertion. Do not interrupt
   someone else's process or alter coordinator/test-support semantics.

#### A — close the one evidence-writer defect, without a new campaign

1. Reproduce the review's `summary.csv`-is-a-directory false success. Preserve
   the existing good F3 gates and measured samples; this is not a request to
   rerun the navigation matrix or sailed voyage for every file-error test.
2. Check opening, writing and final flushing/closing of all three requested
   benchmark files, and both requested canary log exports. A requested failure
   must fail the case and suppress its success message. Report the failing path
   and operation. Prevent an old/partial output being presented as the current
   completed run; use fresh run paths or the smallest appropriate publication
   scheme. Do not build a general evidence framework.
3. Exercise the same actual writer with a tiny nonempty payload: a valid output
   round-trips; an unwritable/directory destination fails; a write/flush error
   fails at the narrow stream seam if native reproduction is unavailable.
   Check propagation to the caller's verdict, not only a helper's return value.
   Restore the old ignored-error behavior once and require the negative control
   to fail while the valid control remains green. Register these cheap tests.
4. Repair evidence paths: N5 measurements are under the main checkout's
   `cache/map2-nav3/n5/`, while gates/voyages are under the MAP checkout.
   Name source root and artifact root explicitly. Recover the post-sync-fix
   run's source/binary identity from existing records, or mark it incomplete;
   never manufacture identity from a current binary. Reuse the independently
   reconciled samples. Correct stale current descriptions of per-ship sync
   versus `sync_once`/`begin_advance`; keep historical results labelled historical.
5. Independent read-only reviewer checks only this delta, its controls and
   evidence dispositions. After correction and focused re-review, continue B.

**A complete:** requested file failures cannot report success through either
writer's consumer, and existing evidence is accurately located and qualified.

#### B — specify the normal-building terrain and height path

1. Produce a concise dependency table for **normal `BuildCmd`**, separate from
   `FoundCmd`: bounds, ownership, occupancy/removable scenery, terrain class,
   rotation, height result, type rewrite, payment and commit. Keep original rules,
   already approved adaptations and unknowns distinct. Do not transfer founding's
   automatic rotation trials or ship predicates to normal building commands.
2. Reuse the decoded vertex grid, coast branch and record-byte writer. Trace
   only the unresolved ordinary-land branch/callers of `0x1007c790` and the
   normal placement path needed for a non-coastal starter building. Pin module
   hashes, addresses, input domains, comparison/rounding, resulting height byte
   and refusal. Do not assume a uniform flat height or invent a sea threshold.
3. Classify the registry's relevant flags into supported ordinary-land and
   evidenced coastal cases versus unimplemented mountain, road/tile-object,
   overlay or replacement cases. State exact support conditions; unsupported
   input is explicit, never successful default validation. This packet does not
   require all 286 building types or every special terrain class to become usable.
4. Trace the narrow autonomous-growth store and immediate replacement callee
   for OQ-NAVC-2: does the item height remain, get recomputed or get overwritten?
   Record the actual store. If it remains unread, keep that limitation and block
   only its dependent fidelity claim; the established ordinary-build path proceeds.
5. Define a small read-only placement result that carries both the decision and
   authoritative height, then its commit use. Preserve existing ownership,
   charging and atomic-batch contracts. Specify missing/placeholder terrain,
   ambiguous unsupported content, integer bounds and untrusted client byte8.
   Reuse shared predicates where semantics agree; do not duplicate founding.
6. Define synthetic positive/negative pairs from the evidence: identical legal
   land versus sea, valid versus invalid coast, slopes/height boundaries, turned
   nonsquare footprint, scenery/occupancy and an alternate registry type. Check
   the contract and expectations independently before engine implementation.

**B complete:** a reviewer can explain why at least one real ordinary-land
building and the supported coast class pass or fail, and where their height
comes from. An unavailable original-runtime capture stays NOT_RUN; an available
but unread rule required by that supported path is remaining work, not permission
to implement an always-reject stub or an invented rule.

#### C — integrate authoritative placement into gameplay

1. Implement B's approved result through the existing placement/command seam.
   Validate the effective type after any upgrade rewrite, its requested rotation
   and footprint at execution time. Apply only evidenced scenery replacement;
   retain explicit refusal for unsupported branches. Keep permissions distinct
   from visibility sharing and preserve command ordering/conflict behavior.
2. Derive the accepted building's height once from validated world/content data;
   carry it through `Checked`/creation and every tile written by that placement.
   Never trust a client-provided height or silently use zero. Preserve founded
   and loaded records. Fix the growth height writer only if B established its
   rule; preserve the original clear-arm behavior where already evidenced.
3. Preserve failure atomicity, including mixed-validity multi-record commands:
   bytes/hash, PRNG, money, goods, buildings, ownership and simulation events
   remain unchanged on an atomic refusal. Dry-run decisions must account for
   earlier records in the same command. Do not introduce whole-world copying
   per placement or a full registry/island scan for a local footprint.
4. Feed the existing tile setters/dirty rectangles; retain history-independent
   route results after build and demolition. Check the first plan after a change,
   including two Advances sharing a tick. Reuse the existing invalidation design;
   no navigation rewrite or new concurrency policy is authorized.
5. Use registry facts and validated rules, with original defaults. Prove a second
   data-defined type without engine edits. No special case for player 0, type
   1302, five seats, one candidate or the fixture's height. Keep future mod policy
   replaceable without implementing a plugin system. Explain any seam signature
   migration for the Windows consumer; do not edit its app/render sources.
6. If authoritative layout/rules identity changes, allocate a new unused stream
   version and independent byte vectors at that point. If only behavior changes,
   explain the existing version/identity policy and affected replay compatibility;
   do not bump goldens blindly or reuse an incompatible committed layout.

**C complete:** supported normal commands have terrain/height validation and
commit effects consistent with B; unknown cases cannot bypass it.

#### D — prove the real command path, cheaply first

1. Run B's positive/negative pairs through `World::apply`, with adequate funds,
   valid ownership and clear footprint so the terrain predicate is decisive.
   Assert the exact outcome and resulting height. Check wrong height/client
   bytes, alternate owner/type, rotated bounds, relevant integer extremes and
   atomic batches. A terrain test must not pass because ownership or funds fail.
2. At the production call site, restore the former terrain stand-in once: the
   sea/terrain control must fail at its intended assertion. Separately force a
   zero stored height: the nonzero-height building/tile/save/route check must
   fail. These are two causal checks, not a mutation-count target. Keep controls
   passing and verify each mutation is restored before any broader gate.
3. Compare post-placement navigation with a clean rebuild and save/load at a
   site where the evidenced height changes blocking. Keep a nearby legal route
   as a control. Check demolition and any newly resolved growth write. Do not
   require a blocked path that the original height predicate would leave open.
4. First check whether a newly founded city can legitimately afford a supported
   ordinary building; ship cargo is not city stock. If so, extend the real-file
   sailed-founding canary with public build/demolish commands there. Otherwise
   retain the actual sailed-founding canary and use a **separate scenario** with
   real terrain/registry facts and declared city stock in `ScenarioSpec` before
   its initial save, then execute public build/demolish commands. This is an
   explicitly stocked placement fixture, not a completed ship-start opening loop.
   Label the combined loop BLOCKED on unloading and hand off that exact dependency.
   No post-founding grants, zero-cost type changes or transfer implementation.
   Use existing canary/site-search facilities; searches may inspect world copies,
   but the executed voyage must not relocate ships or bypass ownership/terrain.
5. For each applicable canary, record charges, outcomes, territory/information
   preservation, building/tile
   heights and final state. Verify a reload after placement and fresh-process
   replay of the recorded sequence, with one changed-command divergence. Reuse
   one voyage across these checks where possible. No candidate selection, asset
   edit, added transport feature or new long matrix is needed for this canary.

#### E — one frozen acceptance pass and handoff

1. Freeze the corrected source. Run cheap writer/reference contracts, focused
   placement/build/founding/navigation/replay cases with original files and
   ASan/UBSan. Run the full affected sim selection once because command/default
   placement behavior changed; include formats/load only to the extent touched.
   Use the canonical workflow, explicit source/build roots and skip policy.
   Preserve first failures. An unchanged renderer needs no GPU/heavy-render gate.
2. Synthetic legacy tests must state which contract they exercise. Give terrain
   tests real terrain; retain an explicit test seam only for tests deliberately
   isolating another subsystem. Never switch the entire suite back to the old
   placement stand-in to make the new default green. Justify every changed golden
   by layout or specified behavior and retain independent expected outcomes.
3. One fresh integration reviewer traces input -> decision/height -> payment and
   tiles -> routing -> save/replay. It inspects tests and actual evidence, challenges
   the two wrong production behaviors above, and checks package-review corrections.
   Fix actionable findings and narrowly re-review their deltas. Do not repeat
   broad valid gates after prose-only changes or add a new review campaign.
4. Report code/test/docs/workflow pins, selected/executed/failed/empty/skipped
   counts, artifact identities, support table, failed attempts and remaining
   external gates. List the exact API/behavior delta needed by the later Windows
   integration. Do not borrow W4's host results or call a Mac candidate target-ready.

Each substantive package uses an independent **read-only review subagent**; it
does not implement fixes or delegate its review. The author fixes findings and
obtains focused re-review before marking the package complete. No P1 alone is
not acceptance: missing assigned behavior and actionable P2 defects still count.

**Checkpoint after E at one reviewed headless placement candidate.** The explicit
two-canary alternative in D4 satisfies this packet's local acceptance, while
the combined ship-start opening loop remains BLOCKED on unloading. Owner
continuation (2026-11-02): once E's independent integration review approves the
corrected placement work, preserve its exact acceptance pin and proceed directly
to MAP-CARGO-1 below. No additional owner confirmation is required. If placement
still has actionable findings or incomplete required behavior, finish it first;
the continuation does not waive its acceptance. Commit small
concerns on the existing MAP branch; no merge, rebase, prune, task-code push,
TASKS edit, candidate selection/install, `game/` modification or Windows/W4 work.
Road building, combat, hostile avoidance, mod runtime, economic-system expansion,
new UI, coordinator and shared-test-workflow work are outside this packet.
Preserve other jobs and the owner's awake assertion. Do not wait for another
orchestrator prompt between the authorized packages.

### MAP-CARGO-1 — unload the starting ship and build from its supplies · ACCEPTED LOCAL HEADLESS SCOPE

**Completion checkpoint, 2026-11-02:** U1–U5 are accepted at code/tests
`7e5529f42378a13b3235d9d93023ee8a40c19c50`, clean U5 source
`f0c8647fb543e3e512a9fc5470a9d04c0f6ff599`, documentation/integration input
`0c401c8d084cfd9feb8aec846595291f87f22cce`. Read the
[direct cargo review](reviews/2026-11-02-map-cargo1.md) for independent evidence,
scope and retained limitations. Preserve the worktree and logs; no further Mac
implementation is assigned by this checkpoint. The exact
accepted pin is published and remote-verified. Windows consumes it only through
the conditional W4-OPENING-1 integration packet, after its verifier acceptance.
The U1-in-progress checkpoint, isolation follow-up and U1–U5 packet below are
acceptance history, superseded for current status by this completion record.

**Current checkpoint, 2026-11-02:** placement mechanics and E1 are accepted as
described above. U1 is already running; preserve its work. Its corrected
independent contract review must approve before U2. This checkpoint permits U1
to continue despite the separate test-only correction below; it does not waive
that finding or authorize an unreviewed unloading rule.

**Bounded follow-up — isolate the two evidence-writer tests.** At the next
package checkpoint, before cargo acceptance gates:

1. Read the placement review's reproduction and
   `src/sim/tests/founding_assets_test.cpp` (`sail log export: ...`) and
   `src/sim/tests/nav_scale_test.cpp` (`nav scale writer: ...`). Both currently
   remove a fixed name under the system temporary directory. Concurrent test
   processes can delete each other's output.
2. Give each test invocation an exclusively created temporary directory and
   clean up only that owned directory. Prefer an existing portable helper if
   available; otherwise keep the helper local to tests. Preserve the actual
   writer calls and all positive/negative assertions. Do not serialize whole
   suites or change the production writer, coordinator or shared runner.
3. Use the tiny selectors from the review: positive serial control, then two
   concurrent processes executing both cases, with a short bounded repetition.
   Record selected/executed counts and each process's status. The pre-fix
   reproduction is already recorded in the review; no new long voyage or
   navigation/performance matrix is required. Check the focused cases under
   ASan/UBSan and obtain one focused independent read-only re-review of the
   directory ownership/cleanup and test execution. Stop this correction there.
4. Commit the test-only correction separately and record its pin. Reuse E1 for
   unchanged placement code; include the corrected cheap cases in U5's normal
   final gates. Keep U1 research progressing. No separate owner approval or
   full MAP-BUILD-1 review cycle is needed.

**Owner instruction, 2026-11-02:** implement ship unloading after the placement
work is reviewed, so players can sail, found a warehouse and start building
without test-only resource grants. This is a continuation on the same Mac
`task/MAP-2-original-like` branch, starting from the corrected MAP-BUILD-1 E
acceptance pin. No branch switch, main merge or restart of placement is needed.

**Start condition:** MAP-BUILD-1's required placement behavior and gates are
complete, its independent integration review and focused correction reviews
approve, and its code/test/documentation pins are recorded. That review checkpoint
unlocks U1; U1's corrected contract review unlocks U2. Continue U1–U5 without
another owner prompt within the scope below. Unavailable external platform
gates remain separately NOT_RUN; they do not become invented passes.

**Outcome:** starting with an unchanged ship-start candidate and its real
expedition loadout, players issue normal simulation commands to sail, found,
unload their remaining materials into their own warehouse/city storage and build
a supported ordinary building. Goods are conserved, refusals are unchanged-world
operations, and replay/save/load preserve the result. No injected stock, modified
loadout, altered building cost, relocation or direct commit bypass is permitted
in this acceptance voyage. The stocked placement fixture remains useful for its
own tests but cannot satisfy this outcome.

**Scope:** manual ship-to-own-warehouse goods transfer through the headless
command path. Excluded: loading city goods back aboard, automatic trade routes,
buying/selling, foreign/allied warehouses, crew disembarkation, combat, new
network transport, ship UI and rendering. Shared information is not ownership
or trading permission. Peace and no alliances remain unchanged.

#### Read order

1. Re-read current main instructions, evidence contract and shared workflow at
   the package boundary, this continuation and the accepted MAP-BUILD-1 report.
   Preserve the existing host calibration, admission control and own-only awake
   process discipline; do not replace a running gate or touch Windows/W4 jobs.
2. Read MAP `docs/sim/{ship-start-integration,ship-founding,ship-navigation,
   shared-visibility,state}.md`, `docs/re/{ship-founding,ship-navigation,
   fire-and-ships,game-mechanics,production}.md` and relevant `docs/re/view.md`
   command/menu evidence. Older fire-and-ships guesses lose precedence where
   the later navigation/founding traces settled them. The manual's instruction
   to unload after founding establishes the intended flow, not exact predicates.
3. Read `src/sim/include/anno/sim/{command,figure,ship,city,production,types,
   ids,world,replay,state_fields,config}.hpp`, the command/codec/apply/replay
   implementation, `ship_orders.cpp`, `founding.cpp`, `founding_payment.cpp`,
   `production.cpp` (`CityStock::add/remove`), `city_creation.cpp`, and the
   original registry/ship-start loader. Follow the current paths if names moved.
4. Read the ship-hold, command/refusal, founding-payment, city-stock,
   navigation-health and replay tests plus the real sailed-founding canary.
   Inspect existing original-file disassembly tooling/private environments;
   do not repeat whole-home searches or install another toolchain needlessly.

#### U1 — resolve the narrow unloading contract before implementation

1. Trace the original manual-unload action from the ship/warehouse UI command
   to the authoritative ship hold and city-stock changes. Record module hashes,
   addresses, record fields, units and call boundaries in a focused
   `docs/re/ship-cargo.md`; draft `docs/sim/ship-cargo.md` and an acceptance table.
   Reuse existing cargo/stock readers and field evidence. Do not confuse a cart
   or resident-walker unload with a ship unload, or infer manual transfer rules
   solely from the automatic trade-route record.
2. Resolve the predicates needed for the opening: eligible ship and warehouse,
   owner, actual target instance/city, range/docking reference and rounding,
   moving/embarked/disabled state, goods/hold selection, requested quantity and
   allowed request granularity,
   available cargo, stock capacity, zero/partial transfer and residual hold state.
   Do not reuse founding's 12-tile click radius without unloading evidence, infer
   permissions from shared visibility, or target a city anywhere on the island.
3. Specify a minimal typed command and outcome: authenticated issuer; stable
   ship/warehouse identity; explicit requested goods or hold and quantity; actual
   transferred amount and refusal reason. Choose hold-versus-goods selection from
   the evidence and document duplicate-hold behavior and stable deduction order.
   Multiple commands may unload multiple holds; no unload-all feature is required.
4. Pin exact units across widened `ShipHold::amount_88` and city storage (both
   use 1/256-ton counts, with different integer widths). Specify checked arithmetic,
   capacity and fractional remainders. The actual debit must equal the actual
   credit; overflow, a full/over-capacity stock and an unsupported goods id cannot
   erase cargo, reduce existing stock, mint goods or wrap. Inspect the existing
   `CityStock::add` behavior before using its return value as a transfer amount.
   Hardening/refusal of malformed states is an explicit rebuild adaptation;
   preserve established ordinary-state behavior. A contradictory original rule
   needing a new gameplay policy is a narrow question, not license to guess.
5. Specify commit order, refusal atomicity, repeated/same-turn commands and
   target demolition/replacement between submission and execution. Revalidate
   live state when applying a command. Distinguish a second intentionally issued
   unload from network retransmission; do not build a new deduplication subsystem.
6. Independent read-only review must approve the exact contract and hand-derived
   positive/negative examples before U2. If a required rule is still unread,
   finish the bounded trace; if genuinely unavailable, record the exact blocker
   and continue only independent authorized work. No always-reject command or
   guessed radius can count as an unloading implementation.

#### U2 — implement the command and conservative goods accounting

1. Add the command through the existing public codec, dispatch, result and replay
   paths. Keep current native tags/variant order stable; choose an unused tag and
   handle version/compatibility according to the existing format contract. Do
   not deserialize a client-supplied payer, owner or claimed credit as authority.
2. Resolve current ship and target with existing indexes. Validate U1's predicates
   and compute a transfer plan from current cargo and capacity before mutation.
   Debit exactly what can be credited under the reviewed partial/exact policy;
   leave the rest aboard. Preserve empty-hold normalization and crew, ownership,
   territory and sharing state. No whole-world clone or per-request global scan
   is needed for a targeted transfer.
3. Commit the prepared hold and stock changes together through the appropriate
   existing APIs; honor U1's associated accounting/callback effects. A refusal
   changes no serialized bytes, PRNG, money, cargo, stock or simulation events.
   Report the actual transfer through the established command outcome mechanism.
   Never call a clamping or wrapping stock adder blindly and deduct the request.
4. Ensure the existing cargo-dependent sailing factor observes the new hold
   amounts at its documented segment boundary. Do not rewrite navigation health,
   movement or change a running segment merely to make an unload test pass.
5. Keep eligible content, hold counts, goods, capacities and range facts in the
   registry/rules. Exercise another owner and data-defined ship/warehouse type;
   no hardcoded player 0, four holds, type 1502/1302 or founding-material whitelist.
   Preserve the faithful-rule/adaptation boundary; no mod runtime is required.
6. Any new authoritative state or rules identity is serialized and hashed from
   introduction, with a new unused version when required. If existing holds and
   stock suffice, avoid gratuitous transfer state. Add independent command bytes
   and any changed state vectors; version-negative tests reseal a valid control
   and assert the version-specific reason. Review and correct U2 before U3.

#### U3 — test transfers through the actual public command

1. Start with legal owner, warehouse, distance and capacity so the positive
   control transfers a known nonzero quantity permitted by U1's reviewed request
   granularity. Separately test fractional stored balances and remainders with
   exact conservation; fractional requests succeed or receive the specific
   refusal required by that contract. Storage in 1/256-ton units does not by
   itself authorize fractional manual requests. Assert both sides,
   reported amount, unchanged unrelated goods and expected residual holds.
   Vary one predicate at a time: wrong owner, distant ship, missing/replaced
   warehouse, non-warehouse target, invalid hold/goods, zero/oversized request,
   empty hold and full storage. For each refusal assert its intended reason and
   unchanged world; earlier unrelated failures cannot satisfy the test.
2. Cover duplicate goods in holds, near-full storage, stock already over capacity,
   quantities near the two representations' bounds, and two ships unloading
   into the same nearly full city in deterministic command order. Compare the
   combined ship-plus-city quantity before/after in a wide checked accumulator.
   A partial transfer is a documented outcome, not a successful loss of excess.
3. Challenge the real consumer with two targeted wrong behaviors: crediting
   without debiting, and debiting the requested amount despite limited credit.
   Each must fail the intended conservation assertion with a valid positive
   control. Use the normal codec/apply path, not only a transfer helper.
4. Save/load immediately before and after a transfer, apply the next command,
   and compare exact state/outcomes. Verify a recorded transfer in a fresh replay
   process and a changed amount/target that diverges. Cover legitimate repeat
   requests and stock contention; no cargo creation through replay or retries.
5. Check changed cargo affects later sailing according to the existing speed
   contract, and crew remain carried. Use cheap focused worlds for these checks;
   no full voyage for every edge case. Independent review checks U3's causal
   coverage and each actionable correction before the integrated canary.

#### U4 — prove the real opening loop without fixture grants

1. Extend the existing real-file canary on an unchanged candidate (a test input,
   not candidate selection). Load the original start expedition and record all
   starting holds, gold and empty city state. Exercise two participants with
   different owners so the feature is not specific to the first seat.
2. Through public commands only: sail, arrive, found the first warehouse, move
   normally if U1's unloading position requires it, unload remaining wood/tools
   or other required goods, then build a supported legal ordinary building under
   MAP-BUILD-1. Identify a real affordable type from the registry. No altered
   candidate loadout, synthetic free type, instant ship relocation, city-stock
   injection or direct placement/transfer helper in the executed world.
3. Keep an accounting ledger: starting cargo minus founding consumption minus
   unloads equals remaining cargo; city credits minus building consumption and
   separately observed simulation changes equal city stock; gold matches the
   actual costs. Before unloading, show the same otherwise-valid build cannot
   spend cargo still aboard. After unloading it succeeds and has the correct
   terrain-derived height. Shared knowledge grants no access to a rival stock.
4. Save/reload between founding and unloading and after unloading before build;
   compare subsequent outcomes and final state with uninterrupted execution.
   Replay one complete recorded opening in a fresh process with matched
   checkpoints, then change an unload command and require divergence. Reuse the
   voyage/recording where possible; do not multiply long runs for every assertion.
5. Retain exact source, asset/candidate identities, commands, outcomes and log
   hashes. Successful writes use A's corrected evidence writer. Independent
   review checks the executed path contains no setup grants after initialization.

#### U5 — final integration acceptance and stop

1. Freeze source. Run cheap contracts/reference vectors, focused cargo/command/
   placement/founding/navigation/replay cases and ASan/UBSan, then one affected
   sim gate with the original files. Include formats/load only where changed.
   Reuse valid placement results for unchanged paths with a dependency note;
   do not repeat rendering, GPU, broad performance or coordinator suites.
2. One fresh independent integration reviewer follows source holds -> authority/
   range/capacity decision -> actual stock credit and cargo debit -> building
   payment -> save/replay. It checks package corrections and the real opening
   ledger. Fix actionable findings and narrowly re-review changed paths; prose
   corrections do not trigger another broad gate campaign.
3. Report the placement checkpoint separately from unloading completion, exact
   source/tests/docs/workflow pins, selected/executed/failed/empty/skipped counts,
   first failures, supported/unsupported rules, original facts versus adaptations
   and external NOT_RUN lanes. Supply the small command/API handoff for later
   Windows viewer integration, without editing that executor's files.

**Final stop:** one independently reviewed headless sail -> found -> unload ->
build candidate, with real conserved supplies and reproducible save/replay.
Stocked placement fixtures alone do not satisfy this stop. If an essential
unloading rule remains unresolved, report the precise partial status; never
waive it by calling the command a stub or granting resources.

The inherited independent read-only package review workflow applies throughout.
Commit small concerns on the existing MAP branch. No main merge/rebase/prune,
task-code push, TASKS edit, candidate installation/selection, original asset
changes, Windows/W4 edits or unrelated infrastructure work. Stop only owned
processes and the owned awake assertion when finished.

### MAP-NAV-3 — finish navigation integration and required scope · REVIEWED LOCAL MECHANICS; REPORTING FIX IN MAP-BUILD-1

**Start:** existing Mac `task/MAP-2-original-like` at
`4731d9a510023f090aec409978eb02ff30921239`; last broad gates cover
`eeb68b986bf64f5a9f4a67a1b195c5bc064705ac`. Preserve newer owned work.
Read the [direct MAP-NAV-2 review](reviews/2026-11-02-map-nav2.md).
The sailed-founding milestone is demonstrated; the unit is **CHANGES REQUIRED**.
This packet supersedes MAP-NAV-2's next-action instructions, which remain below
as the acceptance history. Do not restart its completed A–F work.

**Outcome:** correct live congestion inputs and carried-state invariants; restore
object-aware routing and health-dependent speed; prove production locality and
populated-state persistence. Preserve the working two-participant sailed founding
canary. Continue N0–N5 without another owner confirmation within this scope.
No movement/window wiring, new gameplay system, candidate installation or main
merge is authorized. The Windows W4-WIN-CLOSE packet is unchanged.

#### Read order and boundaries

1. Fetch current `origin/main` and read its `AGENTS.md`, `CLAUDE.md`, PLAN
   §§0.1–0.4/0.7, `docs/testing/{evidence-contract,README}.md`, this packet,
   and `docs/orchestration/reviews/2026-11-02-map-nav2.md`. Read through
   `git show origin/main:<path>` or a separate main checkout; do not merge,
   rebase or overwrite MAP source to obtain the current runner or instructions.
2. On MAP, read `docs/re/ship-navigation.md` §§7–10 and package-A amendments;
   `docs/sim/{ship-navigation,ship-route-engine,ship-route-limits}.md`;
   `docs/orchestration/evidence/map-nav1-evidence.md`; the final MAP-NAV-2
   report and actual package-review findings. Reconcile acceptance against the
   old packet, not only the latest implementation-description paragraphs.
3. Follow production paths through `src/sim/src/{ship_mover,ship_step,ship_orders,
   scheduler,figure,route_cache,route_world,route_stamps,route_coast,route_plan,
   apply,island_tick,scenario,types,invariants}.cpp` and their public headers.
   Read the parent graph, figure insertion/removal and effects paths; the loader
   and tile object/footprint/base-height representation; relevant growth and
   scenery mutations. Discover additional callers with `rg`.
4. Read `src/sim/tests/{ship_orders,ship_nav_state,ship_step,route_stamps,
   route_world,route_unit,founding_assets}_test.cpp`, their helpers, and
   `src/sim/include/anno/sim/{state_fields,config,figure,ship,types}.hpp`.
   Read `tools/{ship_nav_ref,ship_step_ref,sim_stream,figure_digest_ref}.py`,
   navigation fixtures and CMake registrations. Inspect `src/tools/sim_replay/main.cpp`.
5. Read current original-format docs for the placed-item height and figure
   health before adding either input. Reuse existing original module addresses
   and bounded evidence; no whole-home searches, new broad RE campaign or copying
   original binaries/assets into git.

Keep original files read-only and candidate/active-map bytes unchanged. Preserve
peace, shared information, no alliances, natural island differences, ship starts
and optional sharing. Shared information grants no ownership or command authority.
Use only owned processes/build directories. Inspect Mac memory, free disk, cores
and contention before expensive runs; reuse validated concurrency, keep admission
on and the screen awake, and preserve the owner's and other executors' assertions.

#### N0 — freeze the baseline and write discriminating regressions

1. Record source/tool/binary pins and a compact acceptance ledger for N1–N5.
   Preserve valid E2 gate logs and failed/invalid runs. Correct the inaccurate
   blanket "later commits are documentation only" wording; the final delta
   contains one test and a comment. Do not rerun broad gates to correct prose.
2. Reproduce the three review examples before changing production code:
   moving ship stamped 25 unit-direction tiles ahead; carried navigating child
   whose saved state cannot reload after Advance 11; ninth-level plain descendant
   left at its initial position. Record actual observations, not just test exits.
   Include a valid standalone ship and ordinary scout/donkey control.
3. For each correction name the production entry point, inspected invariant and
   nearest wrong implementation. The stamp regression must obtain its data from
   an actually moving world; a manually supplied correct `LocalShip` is insufficient.
   Test the public scenario/command/advance/save path for carried state.
4. Have a read-only reviewer check these counterexamples and the package
   boundaries. Do not repeat MAP-NAV-1 research or request a new design approval
   for corrections already authorized here.

#### N1 — coherent carriage and local derived indexes

1. Define one carried-motion invariant for every accepted figure graph. An
   embarked navigating figure must not sail independently, hold a live independent
   planning ticket, or have a published position disagreeing with its authoritative
   accumulator. Use the named effects/merge path coherently; document handling
   of any accepted carried navigation state on creation/load. Reject malformed
   states with a specific diagnostic, not an arbitrary ban on deeper valid graphs.
2. Replace the eight-edge cutoff and whole-registry mirror with iterative traversal
   of the actual carried subtree. Add a derived parent/children index or reuse an
   existing appropriate index. Update/rebuild it for creation, removal, load,
   copy/move and every supported parent mutation. Define detach/removal behavior
   only as far as the existing API requires; do not implement embark/disembark UI.
3. Replace whole-registry admission counting with deterministic derived ticket
   ordering/rank or an equivalent bounded structure. Preserve the approved fair
   rule, replacement ticket retention, quota, cancellation/removal and stable
   serial service. Persist only authoritative state; derived indexes must rebuild
   from it without changing hashes or outcomes.
4. Provide a real spatial index for ship queries. Update movement/removal/load
   and preserve (cell-z, cell-x, ascending id), radius, flags and cap semantics.
   Count the adapter's work, including discarded examined entries, not just the
   results it returns. A local-query interface backed by a fresh full scan is
   not completion. Also avoid scanning all island entries for a cell already
   known to be open sea; support partial cells with a correct bounded lookup.
5. Tests: ordinary and >8-level carried chains, navigating descendants, distinct
   scheduler batches, unrelated parent trees, mid-voyage reload, copy/move and
   removal; fair admission adversary; more than the unit cap; stale-cell removal.
   Compare derived data with a fresh reconstruction and assert valid controls
   still progress. Use deterministic counters for locality assertions, timings
   only for measurements. Independent read-only review of implementation,
   production callers, tests and changed save invariants before N1 is complete.

#### N2 — correct the stamp's physical units through the real consumer

1. Reconfirm only the narrow chain: route unit slot +0x60 -> unit +0x20,
   segment installation and `position + 25 * velocity`. Correct the misleading
   normalized-heading definition in `LocalShip`, docs and reference fixtures.
   Define exact integer scale/rounding for the current segment velocity, including
   speed and the factor captured by that segment. Do not use the next segment's
   future factor or change the deferred local avoidance scope.
2. Use sufficient fixed-point precision and checked arithmetic so truncation
   at tile boundaries has a stated bound; test positive/negative directions,
   axis/diagonal movement, cargo factor, stationary/turn state and changed speed.
   Expected values come from the documented equation, not the same adapter.
3. In the actual world-planner path, arrange a nearby moving ship so its stamp
   changes the selected goal. Observe stamp tile and route endpoint; restore
   the old normalized-heading adapter in an isolated mutation and require the
   regression to fail for the wrong stamp/endpoint. Preserve self/filter/cap tests.
4. Independent read-only review; address actionable findings with focused re-review.

#### N3 — restore object-aware, history-independent routing

1. Carry the original placed-item base-height input through the loader and the
   engine representation, explicitly and with validation; use its evidenced
   wrap/height predicate and anchor/coarse extrema rules. Preserve it through
   save/hash/copy and original-file loading. Version any incompatible committed
   stream change immediately, with matching independent transcription and goldens.
   Do not approximate every footprint as land or infer height from the wrong field.
2. Inventory all current mutations affecting the route predicate: relevant
   command placements/removals and autonomous island/scenery changes. Implement
   deterministic dirty regions/revisions or an equivalent explicit invalidation
   mechanism. Connect it to those production mutations and rebuild the necessary
   mask/shore halo. Avoid a whole-world rebuild after unrelated economic commands.
3. Enable the evidenced object predicate in the production `RouteCache`. Prove
   cached, clean-rebuilt, continuously advanced, copied and reloaded worlds agree
   for the same authoritative state. A path must actually differ when an object
   blocks versus clears a discriminating narrow passage. Removing the object
   must reopen it. Warm the cache before the change to expose stale results.
4. Replace the test that blesses ignoring objects. Include a real autonomous
   mutation as well as commands, an object crossing a coarse-block boundary,
   nonzero item base height and shore-neighbor changes. A mutant disabling object
   blocking and a mutant suppressing dirty propagation must fail distinct checks.
5. Preserve original-runtime edge/unaligned unknowns as explicit limitations;
   establish bounded deterministic behavior on supported non-64-size/partial-cell
   inputs. No unsupported parity claim, silent out-of-bounds access or arbitrary
   map rejection merely to make the candidate pass. Re-evaluate all 15 candidate
   witness routes after the corrected inputs; leave manifests and selection alone.
6. Independent read-only review of loader -> state -> mutation -> derived cache ->
   actual planner, not only pure predicate helpers. If an exact field is genuinely
   unresolved, stop that dependent subtask with its narrow evidence question and
   continue N4/independent work; do not replace it with a fabricated original rule.

#### N4 — finish health-speed input and populated persistence proof

1. Add the minimal validated current-health state/input required by the already
   evidenced speed formula. New expeditions default to the documented full health;
   supported original imports preserve a known field, and an unknown import
   remains explicitly unknown. No combat/damage/healing/sinking implementation.
   Use the loaded maximum and configurable rules, with no type/player special case.
2. Apply health and cargo terms at the evidenced segment boundary. Test full and
   partial health, cargo combinations, alternate player/type, invalid ranges,
   a mid-voyage state loaded with valid damage and save/reload equivalence.
   A full-health-only implementation must fail a specific speed/arrival assertion.
3. For every new authoritative layout, assign a distinct unused state version;
   do not reuse committed 122 for an incompatible layout. Update rules/content
   identity as needed. Keep a per-field golden-change explanation; no unexplained
   mass golden regeneration. Coordinate version changes within this branch only.
4. Add a small independent writer/vector for populated navigation state: a moving
   diagonal with nonzero carry plus a waiting order/ticket, and the new height/
   health fields where serialized. Assert exact bytes/digest against C++, then
   loaded future behavior. Round-trip alone is not the oracle. Version rejection
   starts from a valid checksummed control and asserts the version-specific error.
5. Independent read-only review of field meaning, bounds, serialization and
   production factor use. Review changed claims, not every earlier static trace.

#### N5 — close with bounded production measurements and one final gate set

1. First run cheap counterexamples/contracts, then focused Debug/sanitizer cases
   and one sailed-founding canary. Fix failures before a broad matrix. Keep the
   original trace and its deliberate mismatch as historical evidence; behavior
   corrections may legitimately require a newly recorded, separately identified log.
2. Measure the real world path with fixed local sailing work while independently
   increasing unrelated figures and islands, then vary local ship/crew density
   and pending orders. Use bounded representative tiers (for example 20, 1,000,
   10,000 valid figures; enough islands to expose a full scan). Record operation
   counts, index cold build/update cost, query/step/admission/crew time, fair-service
   delay, per-Advance p50/p95/p99/max, and coast/route/index/queue bytes. Retain raw
   samples, seed, rules, source and binary identity. Distinguish unavoidable base
   scheduler work from added navigation work and cold index construction from
   hot local queries. No late-game performance pass based on the five ships.
3. Repeat the real-file two-participant move -> actual sailing -> arrival ->
   FoundCmd canary without relocation or bypasses. Check positions, carriage,
   payments, territory and shared information. Verify a mid-voyage reload and
   fresh-process replay with a discriminating changed-target failure; preserve
   accepted-command/later-unreachable behavior. Automate this small replay boundary
   where practical, without duplicating long voyages in every selector.
4. Freeze source. Run the canonical affected sim/load/replay/formats selection
   with original files and explicit opt-in/platform skip policy; focused ASan/UBSan
   and broader sim coverage once because scheduler/state changed. Reuse unrelated
   evidence with its dependency justification. Do not run GPU/heavy-render gates
   or alter shared test infrastructure to close a headless unit.
5. One fresh final integration reviewer follows real inputs through index updates,
   admission, stamps, object-aware route, stepper, carriage, save/replay and founding.
   Give it exact revisions, the requirement ledger and raw evidence. It must
   challenge a plausible wrong production adapter, not merely rerun helper tests.
   Fix actionable P1/P2 findings and narrowly re-review the deltas. No numerical
   mutation quota, open-ended new matrix or repeat of valid broad gates after prose.
6. Report exact code/test/docs/tool pins, selected/executed/failed/empty/skipped
   counts, failed runs and corrections, original-rule versus approved-adaptation
   boundaries, raw measurement locations and external gates. Windows MSVC,
   GCC/Clang warnings/Linux sanitizer and original-runtime capture remain NOT_RUN
   until actually executed on a suitable host; do not borrow W4's results.

#### Stop and handoff

Each substantive package uses an independent **read-only review subagent** that
inspects its frozen diff, production consumer, tests and evidence; it must not
write fixes or delegate its review. A reviewer checks requirements as well as
severity: "no P1" does not waive missing work or actionable P2 defects.

Commit small concerns on the existing MAP branch. Do not merge, rebase, prune,
push task code, edit TASKS.md, select/install a candidate, change `game/`, touch
Windows/W4, or start another coordinator/test-support campaign. Preserve others'
jobs and stop only your own awake assertion when owned jobs end.

**Stop at one reviewed corrected headless candidate after N5.** Mark partial or
blocked requirements honestly and continue independent authorized packages when
one dependency is unresolved. No ship UI, local avoidance, combat, trade-route
system or new gameplay feature before this acceptance point.

### MAP-NAV-2 — first sailing implementation · REVIEWED, SUPERSEDED BY MAP-NAV-3

**Start:** existing Mac `task/MAP-2-original-like`, documentation/reference head
`a01c0539f0b134783dbd7ef9521a8789cfa53a4a`, engine code
`92d4e041083835cc08168a257a31cf8a1a1b6453`. Read the
[MAP-NAV-1 review and decisions](reviews/2026-11-01-map-nav1.md).
Preserve newer owned work and report its relationship to this pin. Fetch/read
this board from `origin/main` without merging main into the task branch.
The Windows rendering assignment above is unchanged.

**Outcome:** an unchanged candidate can load with five ship expeditions, receive
authenticated-issuer move orders, actually sail and then found through the
existing `FoundCmd`. Two participants must demonstrate this without test-only
relocation. Keep ship starts, natural island differences, optional sharing,
peace, shared information and no alliances. No candidate is selected or installed.

**Authorization:** do A first. Once an independent read-only reviewer approves
its corrected contract and discriminating tests, continue B–F without another
owner confirmation for the decisions explicitly approved in the review. Engine
implementation is now authorized on that condition. If a genuinely new gameplay
policy or unavailable observation blocks one path, stop that dependent path,
record the exact issue and continue independent authorized work. Do not invent
an original rule or restart the completed navigation investigation wholesale.

#### Read in this order

1. Fetched main `AGENTS.md`, `CLAUDE.md`, `docs/PLAN.md` §§0.1–0.4 and §0.7,
   `docs/testing/evidence-contract.md`, `docs/testing/README.md`, this packet
   and the linked review. Use the current canonical workflow without merging
   or replacing another lane's source. Keep its revision and source revision distinct.
2. MAP `docs/re/ship-navigation.md`, `docs/sim/ship-navigation.md`,
   `docs/orchestration/evidence/map-nav1-evidence.md`,
   `docs/testing/ship-navigation-capture-card.md`, and the MAP-NAV-1 report.
   Earlier first-pass notes lose precedence where the new evidence corrects them.
3. `tools/ship_nav_ref.py`, all three files under
   `fixtures/synthetic/sim/nav/`, their `CMakeLists.txt` registrations, and the
   relevant private `cache/map2-nav1/` notes/harnesses. Private cache is supporting
   material, not a prerequisite that another checkout must somehow possess.
4. `docs/sim/{ship-start-integration,ship-founding,shared-visibility,state}.md`,
   the current fixed-map ship profile, and relevant figure/terrain format specs.
5. `src/sim/include/anno/sim/{figure,command,replay,scheduler,state_fields,config}.hpp`,
   `src/sim/src/{figure,scheduler,codec,replay,types,founding}.cpp`, the existing
   numeric/registry/rules/effects helpers, figure/command/replay tests,
   `src/sim_load/src/{original_registry,ship_start}.cpp`,
   `src/sim/tests/{ship_start_load,ship_state,figure_facts}_test.cpp`,
   `tools/{sim_stream,figure_digest_ref}.py`. Follow the actual includes/callers;
   this list does not authorize copying an old interface around a newer invariant.

#### A — close the contract and oracle defects before engine edits

1. Reproduce both review counterexamples first, using temporary fixture/model
   data. For V7, substituting `E10 E10 E10 E10` must be rejected for the wall
   crossing. Expand every run into steps and check bounds, water and diagonal
   side tiles. Add narrow corruptions for corner cutting and leaving/re-entering
   the map while retaining a valid endpoint. An unchanged valid fixture must
   still pass. Keep exact-run equality, path legality and independent goal/status
   checks separate; passing one is not proof of the others. Check stamped
   endpoints rather than trusting a copied endpoint field as its own oracle.
2. Replace the unfair first-four quota with a deterministic bounded-service
   policy. Specify request eligibility, stable ordering, replacement, cancellation,
   removal, exhaustion and retry. A repeatedly commanded ship must not overtake
   an older eligible waiter indefinitely. Model the actual registry cursor/cadence,
   including 100 slots, five-tick Advances and ships in slots 1..5. Demonstrate
   a finite service bound under sustained requests, then persist/hash whatever
   fairness state affects outcomes. Quota deferral keeps an order pending and
   must not emit `RouteFailed` or silently install a long idle delay.
3. Finish only the exact route dependencies: goal callback/Offer selection and
   object-height/coarse-map contribution/invalidation. Reconcile `0x100a48c0`
   with `0x1000f260` and the existing RK note, including whether storing a
   congested candidate updates the best score. Inspect `0x10090350` and its
   object-height calls, builder extrema and update flags. Commit independent
   synthetic cases for equal-score/different-density candidates, object below/
   above the blocking threshold and shore-cost versus open-water routing.
   Do not turn every footprint into land without the evidenced predicate.
4. Pin the actual public stop/cancel representation and codec tag/mode, its
   atomic refusals, pending-admission removal and segment-trim behavior. A valid
   `(0,0)` target must remain distinguishable from stop. Resolve land-start
   candidate semantics statically if possible; otherwise explicitly use and
   label the conservative first-slice rule: a land start may escape to adjacent
   reachable water but is not itself offered as a destination candidate. Keep
   original-runtime parity of this case UNKNOWN; update the capture card.
5. Reconcile stale matrix/RE statements, accepted-command versus later route
   failure, navigability within lakes versus exterior-sea spawn policy, and
   planning-only density versus deferred segment-end collision handling.
   Define every numeric scale, residue denominator, rounding and seal bound.
   Include all future-affecting residue in the persistence contract. Correct the
   Q32 absolute/relative error wording. A proof still to be made stays a requirement.
6. Run the cheap canonical reference selection, require its exact test names,
   then obtain independent review of A's contract, fixtures and both counterexamples.
   **A stop condition:** a source-dependent route rule still unknown or a failing
   counterexample blocks dependent implementation. UI rounding and Windows-only
   capture availability do not block exact headless targets or the explicitly
   approved adaptations. Record A's reviewed revision and disposition before B.

#### B — validated navigation state and portable arithmetic foundation

1. Add optional, validated navigation facts through the existing content registry
   and original-file loader. Keep non-navigation figures on their existing paths.
   Speeds, turn timing, health/cargo inputs, navigation classes, query radii and
   rule limits need explicit representable domains, checked products and useful
   refusal diagnostics. No type-1502 or player-zero special case in engine logic.
2. Add authoritative motion and fair-admission state with a clear owner. Keep
   the ship's stepper stamp distinct from the registry stamp. Specify the relation
   between the fine accumulator and published Q24.8 coordinates and validate it.
   Crew and transient/derived spatial data must not introduce hidden state.
3. Choose the simplest portable integer representation that satisfies A's exact
   contract. Prove intermediate widths, signed behavior, divisions and carry
   before optimizing. Do not assume `__int128` exists on MSVC, or add a generic
   arithmetic framework beyond what this slice needs.
4. Version as soon as serialized state or registry/rules identity changes: reserve
   a new distinct version (122 or a higher unused version), update explicit
   serialization and the independent Python transcription together. Do not leave
   intermediate authoritative fields outside the hash until package F. Never
   reuse a version for incompatible committed layouts.
5. Check valid and invalid original/mod-style facts, alternate type/player,
   byte/digest vectors, save/read round trips and invalid-state diagnostics.
   Version-refusal tests start from a valid control, modify only the version,
   recompute the envelope checksum and assert the version-specific error.
   Obtain independent review before reporting B complete.

#### C — real terrain and bounded route queries

1. Build a shared world coast index from the existing vertex/terrain views,
   honoring zero boundary vertices, four-corner averages, object contribution,
   shore costs and island-slot order. Rebuild the relevant derived region after
   changes. Tests compare incremental updates against a clean rebuild, including
   removal and neighbors affected by shore classification. Never mutate assets.
2. Implement the coarse connectivity masks and search, fine corridor expansion,
   FIFO/discovery order, learning retries, candidate selection and run output in
   small pieces with their own fixtures. Consume actual C++ output in the tests;
   Python expected-output checks alone do not test the engine. Exercise block
   boundaries, a one-tile channel, closed diagonal gap, unreachable goal, inside-
   lake route and outside-to-lake failure separately.
3. Implement deterministic bounded local unit queries/stamps. Avoid scanning and
   sorting the entire figure registry for each route. Derived spatial data must
   update with movement, removal and load and reproduce the specified stable
   order; test more than the query cap and unrelated distant figures. Serial
   queries use current authoritative state, never a stale worker snapshot.
4. Enforce per-query work and memory limits with checked map/window dimensions.
   Account for search preparation, retries and result storage as well as node
   pops. `BudgetExceeded` is a distinct failed plan, never a partial success.
   Establish defaults on documented representative small and real-candidate
   queries with a margin; do not tune only to the existing 64×64 fixtures.
   Original window behavior is a configurable default, not a fixed engine array
   or a claim that distant goals on larger maps are supported.
5. Compare exact runs with the original-derived vectors; independently validate
   every traversed tile, status and endpoint. Add a deliberately wrong neighbor/
   tie/cost choice that reaches a legal endpoint but fails the exact-route check.
   Re-evaluate all 15 saved candidate witness routes under the new real terrain
   query and report differences without changing manifest bytes, labels or selection.
   Test query determinism and reentrancy on separate immutable inputs with
   1/2/4/7 threads; authoritative integration remains serial. Independent review.

#### D — movement, turns, speed changes and stops

1. Implement the evidenced first-heading behavior, subsequent turn phases,
   per-tile chunking, cargo/health speed factors, idle pickup and replacement/
   stop trimming. Keep the reference model independent of the C++ implementation.
2. Extend the exact-rational oracle beyond its existing axis-only integrator cases:
   diagonal axes, mixed directions, turns, speed changes with fractional leftover
   time, sub-tile starts, zero-length segments, u16 stamp wrap and boundary inputs.
   Account for every carry term. Check a long voyage's position error and arrival
   time against the stated mathematical model; do not call it original-runtime
   parity. If the proposed bound fails, correct the arithmetic or report the
   concrete discrepancy before changing the acceptance tolerance.
3. Prove no overshoot, no lost leftover time, no jump at plan commit, bounded
   progress through zero-length segments and stop/reorder during a partial segment.
   Persist/reload midway through a turn, a diagonal and a speed change and compare
   future states. Check extreme supported mod values and refusal just outside
   the domain. Run focused Debug/ASan/UBSan and obtain independent review.

#### E — commands, scheduler, fair admission and carried figures

1. Integrate real move and stop commands through `World::apply` and the command
   codec/replay path. Validate issuer ownership and all other prerequisites before
   mutation. Test non-default player/type, unknown/removed ship, crew child,
   blocked state, invalid target and valid controls. Compare full state bytes,
   PRNG cursor, motion, admission state and relevant output on atomic refusal.
2. Dispatch navigating figures through the existing registry visit/effects
   protocol; keep carts/carriers unchanged. Implement A's fair admission rule and
   test it under sustained replacements with more eligible ships than the quota,
   sparse registry slots, cancellation/removal, several Advance sizes and reload.
   Observe actual service, not merely an increasing retry counter.
3. Distinguish queued order, quota waiting, planned/moving, arrived, stopped and
   route-failed outcomes. Use stable order identity so a superseded route's result
   cannot be attributed to a later command. Keep quota wait separate from a query
   hitting its explicit budget. No wall-clock or worker-order effects.
4. Publish parent and nested crew positions through the named effects operation
   in the same step, without invalid references or independent embarked motion.
   Test ship→scout→donkey and unrelated parent chains. Founding continues to use
   the authoritative carrying ship. Do not use shared information as ownership,
   alliance, trade or permission to found.
5. Test competing same-tick orders and replacement ordering, mid-voyage save/load,
   malformed codec records and LegacyRaw/native-tag separation. Compare identical
   replay inputs on 1/2/4/7 executor threads. Obtain independent review.

#### F — sailed founding canary, measured limits and final review

1. Load an unchanged version-2 candidate with original files, five expeditions
   and no cities. Select legal approach/founding sites without claiming candidate
   selection. For two participants, issue public moves, advance until they reach
   the approaches, then issue ordinary `FoundCmd`. Assert actual intervening
   positions, final legality, payment/cargo order, territory, crew and shared
   information. No relocation, forced arrival or test-only command bypass.
2. Record a replay from the initial save. Verify checkpoints, outcomes and final
   state in fresh processes, after a mid-voyage reload and across thread counts.
   A changed move target must cause a demonstrated semantic difference and replay
   mismatch; choose a discriminating target, not a one-tile change that the six-
   tile acceptance region makes equivalent. Include a real route-failure case.
3. Measure production query/step/admission costs and peak route/coast/queued bytes
   for the five ships and a bounded, legal larger synthetic workload. Increase
   distant inactive figures/islands separately from local density; report local
   query visits, queue/service delay and per-Advance p50/p95/p99/max. Keep raw
   samples, seeds, rules, source/binary identity and actual execution counts.
   Report limitations honestly; this is not late-game economy/renderer acceptance.
4. After source freezes, use the current canonical runner for focused contracts,
   affected sim/load/replay/formats gates with game files, and focused ASan/UBSan.
   Broaden once where changed serialization/scheduler dependencies require it.
   Keep declared platform/opt-in skips separate from executed passes. Reuse valid
   unrelated evidence; no GPU or heavy rendering rerun for this headless packet.
5. Obtain a fresh final integration review of public commands → fair scheduling →
   real route output → stepper → save/replay → founding, plus test discrimination
   and evidence provenance. Correct actionable findings and narrowly re-review
   them. Report actual code/docs heads, acceptance matrix, raw logs, review
   dispositions and remaining original-runtime/Windows/Linux limits.

#### Working method and final stop

- Each substantive package gets an independent **read-only review subagent**.
  Give it the exact diff, contract and relevant tests, but require it to inspect
  production callers and challenge a plausible wrong implementation. It must
  not author fixes, delegate its review or repeat hours of broad tests. For each
  critical assertion, record the reached behavior, valid control, distinguishing
  observation and nearest wrong reason for passing. One useful targeted mutation
  or counterexample is sufficient where it discriminates; no mutation-count target.
- Re-review the changed claims and their interactions, not every prior package.
  Keep failures and invalid runs in the ledger. Do not change source under a gate,
  accept a stale executable or count a skipped/empty case as executed. Use small
  commits by concern. Treat code/docs/reference changes as different provenance.
- Inspect current Mac cores, free disk/memory and concurrent owned work before
  expensive runs; reuse validated job counts, reduce concurrency under contention,
  keep admission enabled. Preserve the owner's keep-awake process and other
  executors' jobs. Do not diagnose a long admitted calculation as a hang without
  inspecting progress and the actual admission state.
- Scope excludes rendering/window wiring, UI controls, combat/local collision
  avoidance, trade routes, discovery/fog rendering, candidate selection/installation,
  coordinator/test-support repairs and any edit to W4 or original assets. If the
  existing test infrastructure blocks a gate, record the exact failure and carry
  on with independent work; do not start another coordinator campaign.
- No merge, rebase, prune, task-code push or task-board edit. The orchestrator
  handles publication/integration. **Stop at the reviewed headless candidate
  after F**, or report the precisely blocked dependent package plus completed
  independent packages. Do not report the unit complete if ships only teleport,
  every move is refused, routes are unchecked, or founding bypasses the real sim.

### MAP-NAV-1 — ship navigation evidence, contract and fixtures · REVIEWED, SUPERSEDED BY MAP-NAV-2

Historical packet below; do not repeat it. Its original engine-implementation
stop is superseded only by MAP-NAV-2's conditional authorization above.

**Predecessor accepted:** MAP-FOUND-3 at documentation head `4480200`, code
`92d4e041083835cc08168a257a31cf8a1a1b6453`, on
`task/MAP-2-original-like`. Read the
[orchestrator's acceptance review](reviews/2026-11-01-map-found3.md).
The two previous defects are closed. Do not restart founding, replay-gate or
coordinator work. Existing platform and original-fidelity limits remain named.

**Purpose:** the next playable step is a ship receiving an order, sailing to a
legal approach and founding through the existing command. This packet establishes
the evidence and deterministic contract needed to implement that step. The
geometric exterior-4 route checker is not an original navigator. Do not copy it
into the simulation and call passage faithful.

**Host and ownership:** stay on the Mac in the existing MAP branch/worktree.
Confirm the actual head and preserve any newer work. Windows rendering continues
independently. Original-runtime observations that require Windows are a queued
capture request, not authorization to interrupt or expand that executor's current
assignment. The owner still chooses a candidate; none is selected or installed.

#### Read order

1. Current main `AGENTS.md`, `CLAUDE.md`, `docs/PLAN.md` §§0.1–0.4 and §0.7,
   `docs/testing/evidence-contract.md`, `docs/testing/README.md`, this packet and
   the linked acceptance review. Read the fetched board without merging main.
2. MAP branch `docs/re/ship-navigation.md`, `docs/re/fire-and-ships.md` §3,
   `docs/sim/ship-start-integration.md`, `docs/sim/ship-founding.md`,
   `docs/sim/shared-visibility.md`, `docs/sim/fixed-map.md`'s current ship profile,
   and `docs/research/endless-start-conditions.md`.
3. `docs/formats/figuren.md` and `docs/formats/sze.md` where available; relevant
   `FigurenHeader.dat`/SCHIFF/PRIVAT/ROUTE fields; `docs/re/transport.md` only for
   reusable numeric/search evidence, not as proof of sea routing. Later reviewed
   rules supersede older first-pass statements about float accuracy and timing.
4. `src/sim/include/anno/sim/{figure,command,replay,scheduler,tick,config}.hpp`,
   their implementations and the figure behavior seam; current founding and
   ship-start loader/tests. Inspect existing deterministic numeric helpers before
   proposing another representation. Read `tools/disasm.py`, `tools/map2_ship.py`
   and the prior ignored navigation evidence as reusable tools, not authority.

#### N1 — establish a bounded trace and dependency map

1. Inventory existing local disassembly/decompilation tools and the prior private
   evidence environment. Hash the relevant original modules and record image
   bases, tool versions and commands. Reuse working tooling; no whole-home search
   for another disassembler and no package churn without a concrete need.
2. Follow one normal ship type, initially 1502: move-order submission and dispatch,
   plan construction, segment activation, ticking, arrival/cancel/replacement,
   and the handoff back to founding. Identify actual receivers/vtables and record
   validated function boundaries. Do not read past a return and attribute the
   next function to its caller, or infer a virtual target from its slot number alone.
3. Start from the existing leads: `AnnoWelt.dll` `0x1003de40` (local replan), its
   planner object and slots `+0x50/+0x68/+0x6c/+0x74/+0x7c`, `0x1003e930`
   (segment start), `0x100428d0` (step) and `0x100427b0` (speed factor).
   Reconfirm these leads against the actual module; they are not new conclusions.
4. Produce a compact call/field ledger: read/write, authoritative/presentation,
   PRNG source and draws, file-derived values, resolved callees, remaining exact
   blockers. Follow only dependencies that can affect this first sailing slice.
   Trade automation, combat, AI strategy and decorative wake effects are excluded.
5. **N1 checkpoint:** one evidence-linked path map, with concrete unresolved
   targets. Continue available static work without asking whether to keep tracing.

#### N2 — resolve passage, clearance and route behavior

1. Trace the open-water planner far enough to establish its map representation,
   water/land tests, diagonals and corner contact, hull/shore clearance, endpoint
   acceptance and segment collision checks. Separate global routing from local
   avoidance of other ships. Identify where static coast data and moving objects
   enter; do not conflate the founding hull veto with navigation clearance.
2. Determine route ordering/ties, failed/unreachable routes, maximum search/work
   behavior, partial routes, replan triggers, cancellation and replacement. Name
   any unknown authoritative side effect rather than calling the path complete.
3. Compare the original semantics with exterior-4 on small authored examples:
   enclosed lake, diagonal-only gap, one-tile channel, wider channel, concave coast,
   invalid endpoint and ship-to-ship obstruction. Original, derived and proposed
   outcomes get separate columns. UNKNOWN is not a passing route classification.
4. Document the consequences for the three existing candidates without changing
   their placements or pass labels. Geometric witnesses remain tool-only until
   actual route clearance can be evaluated. Do not select a candidate here.
5. **N2 checkpoint:** explicit passage/query contract or a short list of precise
   missing functions/runtime observations. No sweeping claim that all sea routing
   is understood from a single replan function.

#### N3 — movement state, timing and deterministic arithmetic

1. Establish movement time units/cadence and segment distance/speed meanings,
   turn phases and 16-direction facing, arrival/overshoot handling, zero-length
   segments, stop/reorder behavior, cargo/health modifiers and relevant state flags.
   Check the role of ship `+0x3c` and its relationship to founding eligibility.
2. Inventory persisted versus derived state, including parent/crew positions and
   facing during motion and after load. Scout/donkey attachment must not turn into
   an independent moving unit or stale position that affects a founding check.
3. Derive the authoritative PRNG effects, if any, separately from CRT randomness
   used for wakes/animation. Account for order dependence between ships and commands.
4. Draft an integer-only deterministic representation using the project's existing
   position/numeric seams. Explicitly reconcile older fixed-point proposals with
   any later reviewed emulation requirements. If exact original arithmetic and
   the present representation disagree at boundaries, show concrete examples and
   propose a bounded adaptation; do not silently choose a new tolerance or format.
5. **N3 checkpoint:** state diagram and exact arithmetic/tick rules for the proposed
   first movement slice, with unknowns and adaptations separately identified.

#### N4 — independent references, fixtures and runtime capture cards

1. Create small redistributable synthetic vectors from the evidenced formulas:
   turning thresholds, reversal, axis/diagonal facing, short/zero segments,
   overshoot, cargo/health boundaries and relevant passage examples. No original
   binary, asset or extracted original level is committed.
2. Add/reuse a small read-only Python reference only where the contract is known.
   Expected answers must be hand-derived or independently transcribed; do not
   generate and verify them with one implementation. A deliberately wrong rounding,
   turn threshold, corner rule or hull clearance must fail its relevant check.
   Where the original answer is unknown, provide a capture case instead of a
   fabricated expected value. Register public-tool contract tests if a new tool
   will provide acceptance evidence.
3. Prepare a short Windows original-runtime capture card with exact setup,
   relevant module/scene/type, observed fields, expected discrimination and output
   format. Distinguish visual observation from instrumented coordinates/ticks.
   Prioritize observations that resolve a real contract decision. Do not require
   the rendering executor to perform it before its current packet ends.
4. **N4 checkpoint:** reproducible references with negative controls, a compact
   fixture catalogue, and actionable NOT_RUN capture cases. No broad renderer or
   heavy C++ gate is needed for docs/reference-only work.

#### N5 — implementation-ready contract and review stop

1. Draft `docs/sim/ship-navigation.md` and update the RE note. Define the first
   slice and its refusal contract: owner/unit validation, a move order, route/state
   progress, cancellation/replacement, failed orders leaving state unchanged,
   arrival then ordinary founding. No always-reject placeholder counts as a slice.
2. Specify bounded deterministic work for many ships on large multiplayer maps:
   reusable static coast data, local obstacle queries, per-tick planning budget,
   stable ties/order, finite queues and saved/reconstructed work semantics. Wall
   clock, core count and worker completion order must not change sim outcomes.
   This is a design requirement, not measured performance acceptance.
3. Keep type facts and relevant rules configurable/validated for future mods;
   avoid hardcoded player/type IDs or one hull size. Preserve faithful defaults,
   explicit adaptations and content/rules identity. Ship movement and authoritative
   information updates must remain headless; render interpolation is a consumer.
4. Define how movement-generated information will later enter the shared-
   visibility policy without granting alliance, trade or territorial rights.
   Do not implement fog rendering or claim contact sets are full visibility.
5. Break the later implementation into reviewable packages: validated type/state
   facts; route query; movement/turn stepper; command/scheduler integration;
   save/load/replay/versioning; then actual spawn → sailed approach → founding
   canary. Identify dependencies and acceptance cases per package, including the
   alternate type/player, rejection atomicity and thread-count independence.
6. Obtain independent read-only review of each substantive research/reference
   package and a fresh final contract review. Reviewers inspect evidence and
   challenge the relevant boundary; they do not implement or delegate their own
   review. Correct findings before reporting completion.
7. **Stop before engine implementation.** READY FOR CONTRACT REVIEW requires the
   critical path to be resolved or an explicit adaptation proposed for orchestrator
   decision; list genuinely blocking runtime questions precisely. An available
   unread static dependency is unfinished work, not an external blocker. If the
   trace expands beyond this slice, preserve findings and identify the smallest
   remaining unit instead of starting a full AI/navigation rewrite.

**Deliverables and isolation:** small commits for RE/spec/reference work on the
existing branch, an acceptance/evidence matrix and a new `REPORTS.md` entry with
code/docs heads and review dispositions. Do not change `src/` simulation behavior,
state/command versions, candidate manifests, the active map, original assets,
coordinator/test admission, W4 work, or this task board. No merge/rebase/task-code
push. Preserve keep-awake assertions, inspect Mac resource availability before
costly analysis, and keep admission enabled for any tests that use it. Do not
rerun MAP-FOUND-3's completed campaign for a documentation change. The duplicated
hull bound and the other named fidelity gaps remain follow-ups, not this packet.

### MAP-FOUND-3 — bounded corrections and five-player start policy · ACCEPTED LOCAL SCOPE

**Completed and reviewed at `4480200` (code `92d4e04`); see the acceptance review
and MAP-NAV-2 above.** The following packet is retained as the acceptance record,
not an instruction to repeat A–E.

Continue `task/MAP-2-original-like` on the **Mac** from documentation head
`f0812cde4103cc8884cf749abe6f72294dc60c83`, code
`4f22cf6495266647e8f2ac4b702eaf3d7243bd88`. Confirm the actual checkout first;
do not reset newer work. Read the orchestrator's
[critical review](reviews/2026-11-01-map2-founding.md). The existing gameplay work
is retained; two concrete findings require correction. Saved local gate counts
reconcile. This is not full fidelity or target-platform acceptance.

**Owner decision C5 is resolved:** five ship-start players begin **at peace,
with shared visibility, and without alliances**. They still race to claim their
first island; natural island differences and optional sharing remain. Do not
select/install a candidate, equalize islands, grant starting territory, move
spawns, or require multiple players to share an island. Original scenarios keep
their own setup. The new start policy is an explicit adaptation, not an original
Anno 1503 multiplayer behavior claim.

#### Read in this order

1. Current main's `AGENTS.md`, `CLAUDE.md`, `docs/PLAN.md` §§0.1–0.4,
   `docs/testing/evidence-contract.md`, `docs/testing/README.md`, this packet and
   its linked review. Use the fetched board and canonical gate from main without
   merging another lane's code into this branch.
2. On the MAP branch: `docs/orchestration/evidence/map2-founding-reviews.md`, the
   latest MAP report, `docs/sim/ship-founding.md` §§16–17, and
   `docs/re/ship-founding.md` §§15–16. Later corrections supersede historical
   assertions; do not revive the treasure-at-founding claim.
3. `docs/sim/ship-start-integration.md`, `docs/sim/fixed-map.md`,
   `docs/re/players.md`, `docs/sim/state.md`; the relation/discovery/registration
   code in `player.*`, `scenario.*`, `scenario_formats.cpp`, `founding.cpp` and
   `city_creation.cpp`; `src/sim_load/{include/anno/sim_load,src}/ship_start.*`.
4. `types.*`, `figure_veto.*`, `state_fields.hpp`, `config.hpp`,
   `src/tools/sim_replay/{main.cpp,CMakeLists.txt}` and the existing argument-check
   helper; `tools/sim_stream.py`; figure-fact, founding, original-asset founding,
   ship-start, scenario-format, state-layout and replay tests. Inspect callers
   before changing shared data contracts.

#### A — fix hull-domain safety, preserving original facts

1. Reproduce the reviewer's small counterexample: a builder-produced ship type
   with signed hull `INT64_MAX` is accepted and overflows in the nearby-figure
   veto. Exercise `INT64_MIN` too. Use a tiny focused sanitizer probe, not a corpus
   campaign; retain the first failure.
2. Define one safe hull domain for every registry construction path. Validate
   builder-supplied facts as well as decoded original data, with a useful refusal
   rather than a crash or silent wrap. Make the pure veto safe for its documented
   public inputs; a clamp performed after overflowing is not a fix.
3. Preserve absent, present zero, ordinary positive values, and the real negative
   type-391 value. Cover both sides of the chosen bounds, missing-fact refusal,
   and unchanged world bytes/PRNG on a refused founding. Include one accepted
   control through the same registry → founding path.
4. **Stop A when:** focused Debug and sanitizer cases pass and the independent
   reviewer confirms the arithmetic bound and both public entry paths.

#### B — make the replay CLI contract test the real result

1. Demonstrate that the current CTest properties accept the expected divergence
   text with exit 0. The CLI currently returns the right statuses; preserve that
   behavior. Do not call its working replay algorithm broken.
2. Add/reuse one small portable command-check helper that requires both the exact
   exit status and the expected checkpoint/diagnostic content. Founding success
   expects 0, deliberate divergence expects 1. Remove the regex-only loophole in
   the analogous registered replay-success cases using the same helper.
3. Cheap helper contracts must reject right text/wrong exit in both directions,
   right exit/wrong text, missing output, and a failed child launch. Then exercise
   the real `m2_found.log` and `m2_found_wrong.log` public commands. A helper that
   itself exits 0 unconditionally is not proof. Register and explicitly require
   its contract in the canonical acceptance gate.
4. **Stop B when:** the public positive and negative runs and the helper's
   counterexamples pass review. Do not redesign the coordinator or gate runner.

#### C — independent content/state proof before new policy state

1. Add a small independent byte/digest reference for figure facts: absent hull,
   zero, positive and shipped negative, plus at least two types to establish
   ordering. Derive signed encoding and presence from the written contract, not
   the production serializer or a copied result hash. Check a changed field
   changes the identity and a reordered input respects the documented order.
2. Include a non-empty ore discovery-set wire example with hand-derived expected
   bytes. The current Python empty-list entry does not prove non-empty encoding.
   Reuse existing field walkers/oracles where appropriate; no second full engine.
3. Any incompatible serialized-state or digest-layout change in this packet
   needs a distinct declared version, updated readers/writers and explained
   goldens. Do not silently reuse 120 again. Freeze the new layout before final
   fixture generation. Test the version guard with an otherwise valid input
   differing only at the relevant version field, plus a valid control, and check
   the specific error; a registry mismatch/truncation must not satisfy that test.
4. **Stop C when:** the independent checks discriminate the intended mistakes,
   the supported/unsupported version contract is explicit, and review passes.
   Do not add an old-save migration project.

#### D — implement peace and shared visibility without alliance side effects

1. First write a short contract and acceptance table. Distinguish player contact
   (`Player::discovery`), city/island/ore discovery, actual visibility/fog, and
   TRADE/MILIT/WAR relations. Shared visibility means mutual sharing of information
   actually revealed by the five participants, not revealing the whole map at
   start. Identify what the current headless model can represent; do not label
   a contact set as live rendered fog-of-war.
2. Trace the smallest unresolved dependencies: confirm original relation-byte
   meanings and the loader's player `DISCO` → property-7 link, with exact original
   module/address/file-field evidence. If that link is established, implement its
   loader/validation and focused byte-level tests. Otherwise retain a specific
   open dependency; do not guess or block independent corrections on it.
3. Obtain an independent contract review before implementation. The owner policy
   itself is already authorized; no additional orchestrator round is needed for
   a reviewed, bounded implementation. If original MILIT=3 couples sharing to an
   alliance, represent sharing separately. Never set an alliance merely to get
   visibility. Keep the faithful original path and this scenario policy distinct.
4. Implement the smallest deterministic scenario/rules representation and the
   supported headless discovery propagation. Apply it specifically to the five-
   ship start. Make participating players explicit data, not five hardcoded peer
   checks. Validate participant IDs and persistence; include effective policy in
   world/replay/content identity as appropriate. State separately which identities
   describe unchanged candidate geometry and which describe gameplay setup.
5. Test the real loader: all ordered pairs are at peace; no military/trade
   alliance or territorial rights arise; a reveal by one participant reaches the
   supported shared-visibility state of the others; unrelated/absent players do
   not acquire it; unexplored information stays unknown. Include an ordinary
   original scenario with sharing off, a different participant set/non-default
   player IDs, and save/load plus replay. Failed loads/commands remain atomic.
6. **Stop D when:** the policy and supported behavior are reviewed and tested, or
   a precise prerequisite prevents a particular behavior. In the latter case
   finish independent work and report that row incomplete. Do not claim rendered
   shared fog, sailing, diplomacy commands or full-map visibility if not built.
   Do not implement campaign scripts, mines or CompLogic AI to close this packet.

#### E — prove the usable gameplay path and close the packet

1. Run one small end-to-end headless canary before broad gates: load an unchanged
   candidate, verify five ships/crews and no cities, then use legal founding
   commands for more than one participant and verify payments, ownership,
   visibility-policy effects and replay checkpoints. Sailing is not implemented:
   if setup relocates ships to independently validated sites, disclose that as
   fixture setup, never as a demonstrated sailed route.
2. Preserve both distinct-island settlement and optional unowned-land sharing;
   test conflicting commands and a refused command without charging or leaking
   discovery. Retain the alternate non-square warehouse definition and non-default
   owner/city test. Do not alter the candidate manifests or install a winner.
3. Run cheap contracts → focused regressions/sanitizers → the real-asset canary
   → relevant sim/replay/formats gates on the final frozen code. Reuse unaffected
   evidence with an explicit dependency explanation. No heavy render campaign or
   repeated broad run without a changed input, failed check or unresolved cause.
4. Record exact selected/executed/failed/empty/skipped/incomplete counts, source,
   workflow, binary and fixture identities. Preserve first failures. Windows,
   Linux/WSL and original-runtime checks stay NOT_RUN on this Mac; do not claim
   the rendering executor has tested this independent branch.
5. Obtain final independent integration review, fix actionable findings, append
   `REPORTS.md` and the review ledger, and stop. State code/docs heads, acceptance
   per package, effective start policy, and remaining fidelity/platform limits.
   No merge, rebase, task-code push, candidate selection or next feature expansion.

#### Independent review, efficiency and boundaries

- Deploy an independent read-only review subagent for each substantive package;
  freeze its diff and give it callers, acceptance table and raw evidence. It must
  inspect the implementation and actively challenge one relevant counterexample,
  not approve the author's summary. It must not implement, commit or delegate its
  own review. Re-review corrections and retain dispositions before completion.
- Use a fresh final integration reviewer for A–E interactions. Review original
  rules, explicit adaptations and unknowns separately. A missing independent
  reviewer is REVIEW PENDING, not self-approval. Avoid duplicate long test runs.
- Preserve modern/mod-ready boundaries: validated registry facts, scenario-owned
  policy, deterministic ordering, explicit state and content identity, and no
  rendering dependency in `sim`. A changed warehouse or participant set must not
  require another hardcoded branch. No speculative plugin framework is requested.
- Inspect current Mac memory/disk/contention and use the existing calibrated
  concurrency with admission enabled. Own only your jobs and awake assertion.
  Do not touch `test_support`, coordinator journals/processes, W4's worktree,
  original assets or shared task assignments. Keep the screen awake while your
  work runs; preserve other assertions. Report limitations instead of changing
  another lane to make a gate pass.
- Kind-11 reuse/loaded COMP records, ore-event consumers, deposits/mining and
  sailing remain explicit follow-ups. The conservative kind-11 refusal is not
  original parity. Do not attach a full AI or campaign investigation to this unit.

---

**Round bp (2026-10-26, owner instruction):** for every task after the one an executor has in progress now, **subagents are allowed again where necessary and helpful**. §1 item 5 has the rules: the task stays the executor's, subagents' results are checked against the files before they enter a commit, resource-budgeted concurrency under round cp, the orchestrator's review stays. §1 item 11 no longer forbids them inside a sub-task.

Historical dispatch, 2026-10-31 dw (W4 status superseded by the current dispatch above) — **W4 CPU corrections remain CHANGES REQUIRED** at `3db3df3` (code `ba8f300`). Multi-worker river teardown still produces ASan use-after-free; final reclamation can occur on the window thread; failed builds lose pending convergence. Release/dense-scene and byte-budget evidence remains owed. Comprehensive overnight CPU completion assigned below; no GPU integration release. Other lanes and T10c F6's pause are unchanged.

**Round dw (2026-10-31, orchestrator): second CPU review and overnight completion assignment.**
- **Review:** branch `task/W4-3-live-buildings`, report `3db3df3`, code `ba8f300`; later commits docs-only. R2 dirty-child/synchronous freshness and R3 loader backing ownership show substantive progress; do not undo them. R1/R4 are not closed; R5/R6 are incomplete despite the report heading. Independent evidence and assessment are in the round-dw REPORTS section and main's `cache/orch-dw/`.
- **DW1, trustworthy execution:** adopt the process audit in `cache/orch-process-audit-2026-10-31/`. Repair the small runner or use per-case CTest with accurate exit/count/timeout reporting. Inventory the executor's still-unfinished gate before starting another or replacing its binaries. Preserve full logs and source/binary identity; use small focused tests before game-file integration.
- **DW2, concurrent river lifecycle (P1):** a single `FlowBuild::solving` pointer is insufficient for multiple workers. Independent two-solve ASan probe destroys a non-shared Scene while one solve is gated, then reports heap-use-after-free. Track all active ownership and make dequeue plus lifetime registration atomic with teardown. Audit lazy builder publication and temporary strong-owner destruction under its lock. Test two/four workers, two islands/scenes, queued/dequeued/active shutdown, expired shared request, faults and cancellation. Do not hide the issue by forcing one worker.
- **DW3, reclamation and failed-build convergence:** passing a shared reference to the reaper does not control final deletion. The retained-frame-owner probe still destroys a retired generation on the window thread. Guarantee final expensive destruction off-window, including later consumer releases and shutdown, without cycles or use_count polling. A one-shot build failure independently leaves `pending=0`, generation 0 and an unapplied desired building; retain/retry the newest dirty state with bounded retry/backoff and an explicit terminal error state rather than waiting for another sim edit. Test recovery with no new batch.
- **DW4, enforceable bounds and publication:** count bytes as well as items across live jobs/results/generations/backing/cache consumers/reclamation; distinguish shared static residency from incremental bytes. Keep the latest desired version recorded even if dispatch is deferred, never publish a known-superseded result, and test overloaded convergence. Remove the idle full-world building scan, avoid repeated global-vector work for local edits, and requeue stale change-set preparation instead of performing a large window-thread fallback. Reconcile contradictory old/new ownership and bound text in game-shell.md.
- **DW5, representative benchmark:** the current bench serially waits for workers each cycle, synchronously renders the whole map bounds, repeats only the initial two inhabited islands and uses arithmetic threads as a simulation proxy. Keep it labelled a microbenchmark; add bounded-viewport asynchronous and sustained-update modes with deterministic representative five-player and larger density tiers. Validate placements/counts and compare final pixels with the CPU reference. Run Release, report sample counts/percentiles, queue/byte/reclamation data, idle/local-edit scaling and real headless simulation contention separately from arithmetic load. Validate CLI parameters and bound final drain. Existing K and PLAN targets remain; displayed-frame proof awaits the later viewer task.
- **DW6, complete local evidence:** focused regressions/sanitizers, changed subsystem tests, full picture/mailbox and final non-heavy game/no-game gates, formatting. Heavy calibration only after capacity checking, one worker and 8 GiB headroom before measured 2/4-worker escalation. Fix the numeric sampler and reconcile the complete heavy log. Do not waive the two Metal timing failures or claim Windows/Linux/RTX/Radeon success from this Mac. Record remaining external gates explicitly.
- **DW7, next integration preparation only:** after CPU work, document the owning-generation path through viewer/GPU and a small replacement/stale-completion test matrix with dependencies and resource estimates. Do not implement or enable viewer/GPU, input or preview yet. There is sufficient CPU correctness, measurement and gate work for the overnight assignment; use the time for those deliverables rather than unapproved feature expansion.
- **Execution:** continue the existing W4 branch, small concern-specific commits and checkpoints; no merge/push/rebase/pruning. The detailed packet in `cache/orch-dw/executor-packet.txt` breaks each DW block into subtasks. If a block remains unresolved, record the cause and continue independent authorized blocks; do not claim the unit done or cross the GPU boundary. Stop for review after the comprehensive CPU completion report. No further owner decision is needed to run the already-required Release/dense measurements.

**Round dv (2026-10-30, orchestrator): CPU review returned for corrections; viewer/GPU work not released.**
- **Heads and scope:** continue `task/W4-3-live-buildings` from `036a192`, code `549ffc4`; host dependency `abc1f8c` remains unaccepted. No merge or push. The per-island immutable design direction stays approved, but this implementation is not accepted. Read the round-dv review in REPORTS and the detailed packet under main's `cache/orch-dv/`.
- **R1, river lifecycle:** `queue_flow` computes the shared-owner flag backwards. An ordinary non-shared scene's queued flow is skipped; an expired shared scene falls through to a dangling raw pointer. The existing game-file river test fails and the paused-queue/retired-generation ASan probe reports heap-use-after-free. Fix the ownership protocol, including shutdown for non-shared callers and recovery of a shared flow slot when a request's generation expires; flipping the boolean alone does not cover those cases.
- **R2, derived freshness:** `FrameRenderer::plan_jobs` accepts dirty cached children. The parent is stamped with the current scene generation and its mark cleared although its pixels came from stale children. Independently reproduced with one derived level. Require fresh dependencies through every derived level; synchronous publication must obey the same generation rule. Test warm caches, async/sync paths, child and parent completions in adversarial order and a fresh-scene pixel oracle.
- **R3, backing storage:** `ScenePicture::owner_` does not travel with returned/derived scenes. A generation held beyond the bridge's destruction retains dangling island/file spans; reproduced under ASan. Make the owning generation retain its complete backing data graph. Test bridge/loader teardown with retained generations and jobs still alive.
- **R4, bounded work and reclamation:** the claimed `1 + threads` bound omits object jobs, completed results, flow jobs, frame ownership and reclamation. `done` accumulates obsolete results until commit; queued island jobs can retain different generations. The reaper merely drops one shared reference, so another window-thread reference can still perform final destruction. Specify and enforce finite queue/byte accounting, newest-result coalescing, nonblocking overload behavior and off-window final reclamation. Test stalled commits, many islands, slow workers/reaper, sustained edits, shutdown and injected worker failure. Do not merely relax the assertion.
- **R5, scalability and fidelity:** preserve original painter order, shadows, overrun extents, animation, water and static/dynamic occlusion; improvements remain separate from faithful behavior. Move full-island list concatenation/sorting/diff preparation out of the window commit and bound its remaining work. Measure configurable dense five-player and larger stress workloads, simultaneous island edits, burst and sustained traffic, and competition with simulation CPU load. Report p50/p95/p99/max by stage and actual live bytes/queues; the two-building baseline and 13 ms object stage are not late-game or end-to-end acceptance. PLAN §0.3 targets remain, and future viewer/GPU work must measure actual presented frames at 1080p/4K and all zoom/water modes.
- **R6, evidence and gates:** reconcile the heavy log: it records an initial 1800 s refusal, then 16 completed items, later waits, and a CTest 3600 s timeout, not a stop at item 17 solely due to admission. All 16 measured growths are below their reservations; that correction stands, with no budget constants changed. Bind reports to exact binaries/heads and distinguish pass, skipped, admission failure and timeout. After fixes run focused regressions/sanitizers, the picture/mailbox suites and full non-heavy suite with game files on the final head. Heavy completion, target Windows/Linux and RTX/Radeon gates, Metal timing failures and host-support review remain open.
- **Delivery:** small commits by correction, updated specs/perf/report with Decisions and reasoning, a reproducible benchmark and regression logs. Stop for CPU review after R1–R6. Subsequent viewer owning-generation wiring and GPU replacement tests need this review passed; developer input/preview follows. No unrelated lane changes.

**Round du (2026-10-30, orchestrator): Mac report reviewed for continuation; object-list seam approved, no acceptance or merge.**
- **Audited heads:** main `632229e`; host support `task/MAC-1-host-support` at `abc1f8c`; W4 `task/W4-3-live-buildings` at report `e7934ee`, code `a8a8e7a`. Source and selected saved logs were inspected; this review did not rerun the suites. The next task continues the existing W4 branch. Its merges of main and host support are already present; do not repeat them.
- **Next single reviewable unit: W4.3 steps 9a–9d plus 7d, on immutable per-island object lists.** The scope question is answered. `src/render/scene.*`, `frame.*`, the necessary app bridge/viewer integration and their tests/spec are in scope. No whole-scene rebuild per building change, no sim-state changes, no terrain/water refactor, no UI.2.
  1. Update `game-shell.md`: static terrain/scenery/water stay reusable; dynamic building objects have immutable per-island generations. Preserve the original static/dynamic painter order, occlusion and sprite extents. Confirm the classification boundary for every supported building type.
  2. Build/coalesce a replacement object list off the window thread for each touched island. Jobs capture immutable inputs; commit only a current result. Old lists remain alive through ownership, never by stopping or joining the workers on the window thread.
  3. Specify and enforce the retained-generation bound. The proposal's “one extra copy per island” is not proved: several in-flight jobs may reference different generations. Coalesce pending updates, bound active/queued work and release large retired data off the window thread. Include a bounded worst-case memory argument and a repeated-update stress test.
  4. Compute old and new sprite bounds, including overhangs/shadows; invalidate only intersecting keys at all cached zoom groups and derived parents. Preserve off-screen dirtiness and valid unaffected keys. Keep old chunks visible during replacement.
  5. Add controlled tests for N completing after N+1, a second change during rebuild, mailbox resync, a delayed consumer, tall sprites crossing chunks, derived far zoom, rejected commands and pan away/back. Compare changed CPU regions to a fresh scene and assert unaffected keys are not rebuilt. Mutants accepting stale results, omitting invalidation and invalidating everything must fail distinct checks.
  6. Correct the mailbox explanation in `play.cpp` and REPORTS: between the fullness check and publication, the sole consumer can empty the mailbox; no other producer can fill it. A prepared copy may be unused. Test that interleaving explicitly, and account for allocation/destruction under the publication lock rather than claiming it covers only swaps.
  7. Measure the per-island build and 50-build burst in Release as well as the development configuration where useful. The previous numbers are Debug measurements; they justify avoiding repeated static work but do not establish a Release lower bound. Report frames and milliseconds: K=3 is 50 ms at 60 Hz, 25 ms at 120 Hz. Keep the latency target and report misses. Report for review before starting the GPU/input unit.
- **Heavy-test decision: retain the existing size-based reservation model pending comparable measurements.** `render_corpus_test.cpp` reserves **256 MiB + 36 × pixels + 1 GiB while the catalog is unloaded**, not 256 MiB total. For 16960 × 9170, this is 5,867,270,656 bytes (5.46 GiB) warm, 6,941,012,480 bytes (6.46 GiB) cold. The saved `peak_rss_kib=4933392` is 5,051,793,408 bytes (4.70 GiB), not proof of a 20× underestimate. Correct the report; do not multiply the fixed term by 20 or substitute a flat 5-GB limit.
  - Authorized calibration: one heavy process, one internal worker, `ANNO_TEST_MEASURE_ITEMS=1`, 8192 MiB host headroom and the existing 2048 MiB stop threshold. Measure requested/granted bytes, item start/peak/growth, catalog state, dimensions, process RSS/phys_footprint and pressure/swap deltas. Separate total resident process memory from additional item allocation. Existing instrumentation prints these item reservations; use it.
  - No additional owner approval is needed to run required gates in the background. First prove a representative case actually completes after the stdio fix. Then allow 2, and subsequently 4, internal workers only when aggregate reservations, observed memory and throughput support it. This is staged calibration, not a permanent one-heavy-job restriction. Retain protections and count all concurrent jobs. Do not stall independent scene work while a bounded measurement runs.
- **Mac concurrency:** accept 16 compile jobs and 8 light CTest jobs with one internal worker as current measured starting settings; refresh pressure/headroom each run. Single samples are not universal optima. GPU timing/benchmark trials should not overlap other GPU workloads.
- **Developer controls:** the proposed bindings are approved for the temporary developer tool in `--play`: B toggle; left click place/demolish, drag roads; N/P previous/next according to the documented mapping (N next, P previous); period rotate; X demolish; right click cancel; WASD/arrows pan. Tool off retains left-drag pan. Existing quit/water/river/zoom/pause/home bindings remain. Prevent a gesture from both panning and placing; ignore key-repeat for mode toggles, cancel gestures on tool-off/focus loss, and test Retina coordinates. These are not approval of final UI.2 controls or original behavior claims. Step 11 follows the CPU/GPU prerequisites; no new binding question is needed.
- **Acceptance still withheld:** two Metal timing cases remain failures, not waived or silently skipped. W4 ASan, heavy completion, Windows/Linux gates and target GPU runs remain open. The saved ASan support log ends at 19:18:42, before the 19:41:23 stdio-fix commit; rerun focused support/sanitizer checks on the final host code and bind evidence to that source. The stdout change affects the shared POSIX/Linux path, so “Linux behavior unchanged” is not established. Darwin descriptor enumeration and memory-accounting semantics still need independent review; passing support tests alone is not blanket acceptance.
- **Unchanged:** UI F2, population review and MAP.2 lanes; UI.2 dependencies; ADR approvals; T10c F6 paused. No merge/push authorization. The executor appends a report with exact heads, measured results and Decisions and reasoning.

**Round dt (2026-10-30, orchestrator): UI.1 accepted as the spec base and merged; UI.1.F2 released (lane 5).**
- **Review of `task/UI-1-ingame-ui-re` at `690d4c4`** (REPORTS "Round dt"): `ACCEPTED as the spec base`, merged as `df6dcc7`. The executor's own report is QUESTION on two acceptance lines (screens not captured; elements not traced). They are real but bounded, and none blocks UI.2's start, which waits for W4.4 and ADR 0004 anyway. Merging now gives every lane the verified spec, `data/ui/` and the oracle on main.
- **UI.1.F2 `ASSIGNED — lane 5`** (block below): the remaining graphics of the build-test surface first, then the reachable screens, then a list of what only the owner's desktop can capture.
- **UI.2** now needs UI.1.F2 part 1 (F2.1) accepted, W4.4 merged and ADR 0004.
- **Unchanged:** lane 3 W4.3 + W4.4; lane 4 MAP.2; lane 1 waits for review; T10c F6 paused.

**Round ds (2026-10-30, orchestrator): UI.1 reviewed and returned for F1 (lane 5).**
- **Review of `task/UI-1-ingame-ui-re` at `3c0ce65`** (REPORTS "Round ds"): `CHANGES REQUIRED`. Kept: the inventory (`data/ui/dat.txt`, `inc.txt`, `groups.txt`, byte-stable), the `8` suffix finding (a second size set at 800/1024, not 8-bit), the drawn status bar (746 × 35 at (0, 733) = `Data/statusbar.dat` `STATUSBAR`), the right chrome at x = 779, the Citizen-start occupancy of all eight categories, eight typed cells, 59 silent Wine-runner shots and OQ-535..538. Not met: the UI.1 acceptance (every reachable screen captured and specified; every element traced; the HUD and the open construction menu mocked from the original textures and matched to the shots within a stated tolerance).
- **UI.1.F1 `ASSIGNED — lane 5`** (block below), same branch and worktree, from `3c0ce65`.
- **Unchanged:** lane 3 W4.3 + W4.4 (with the `--play data/maps/lan5.txt` wiring, orchestrator note of 2026-10-30); lane 4 MAP.2; lane 1 waits for review; UI.2 `PLANNED` (needs UI.1 accepted, W4.4 merged, ADR 0004); T10c F6 paused.

**Round dr (2026-10-30, orchestrator): owner decisions on test resources and the in-game UI; UI.1 released.**
- **Owner decisions (2026-10-30):**
  1. **Test resources:** for testing, the developer start gets generous gold and goods. It is a developer option, not an original rule: default on for the developer start in `--play`, off for original scenarios, values configurable, and taken from the original's most generous start preset where one exists (`docs/formats/endless-presets.md`).
  2. **The owner tests building and demolishing in person, through the original's in-game UI.** That UI is reverse-engineered with high precision. Every menu and submenu exists. A feature or building that is not implemented yet is a placeholder that leads nowhere until it is implemented. A limited set of available buildings during those tests is fine.
- **Consequences:**
  - **W4.4 narrowed:** tile picking, the never-blocking command queue, the preview, the refusal feedback and the money/goods display stay (UI.2 sits on them). The developer picker becomes a minimal test tool with no polish and no owner check; the bindings owner questions are dropped. W4.3 + W4.4 end with a review stop, not an OWNER-CHECK. The test resources (decision 1) are added to W4.4.
  - **UI.1 `ASSIGNED — lane 5`** (a new executor session; block below): the reverse engineering of the original's in-game UI. Docs, data and a Python oracle; no engine code.
  - **UI.2 `PLANNED`** (block below): ADR 0004 and the in-game UI with every menu, placeholders where the sim has no feature yet. It replaces W5's HUD step for the MVP and is the owner's test surface for building and demolishing. It needs UI.1 accepted and W4.4 merged.
- **Unchanged:** lane 3 continues W4.3 (A4 onward) and the narrowed W4.4; lane 4 MAP.2; lane 1 waits for review; T10c F6 paused.

**Round dq (2026-10-30, orchestrator): W4.3 + W4.4 released (lane 3).**
- **W4.3 then W4.4 `ASSIGNED — lane 3`** (the executor that wrote W4.1 and closed T61; blocks below), one run, one branch `task/W4-3-live-buildings` from the round-dq board commit, worktree `E:\anno1503-worktrees\anno1503-W4-3`, OQ-525..534. Packet: `cache/orch-dq/`.
  - **Why this order:** MVP-A needs build, demolish and roads in the live picture (`lan-mvp.md` §5). The sim already applies `BuildCmd`/`DemolishCmd` (stage S10) and charges costs, but `PlayRunner::one_turn` sends empty turns and the chunk cache never invalidates a chunk whose island changed (`game-shell.md`). W4.3 + W4.4 give the owner the first thing to try in `--play`: place and demolish a building and see it.
  - **W4.2 deferred:** with no behaviour seam installed a figure never moves (`figure.hpp` header), and play mode installs none. Drawing figures per frame waits for the sim to move them.
- **Unchanged:** lane 4 MAP.2 (restart, round dp), lane 1 (T32.6 F1 corrections and P2.1 wait for review), T10c F6 paused.

**Round dp (2026-10-30, orchestrator): T61 F2 closure accepted; MAP.2 restarted.**
- **T61 F2 `DONE`** (REPORTS "Round dp"): accepted at `8cc9928`, merged as `d1a8c11`.
  - The orchestrator re-ran `--validate` on the accepted study in a CRLF and an LF checkout (both valid), the round-dn tamper probe (refused) and both suites (264 join, 64 snapshot).
  - Not blocking: the provenance check accepts an empty or partial `source_hashes`; fix when the tool is next touched.
- **MAP.2 restarted on lane 4:** the first session created the worktree at `6c1cdbb` and stopped without a commit. Restart prompt and preflight: `cache/orch-dp/` (the round-dm packet stays the brief; a progress section after every step).
- **Lane 1** finished the round-dk work overnight: T32.6 F1 corrections (`075ede7`) and P2.1 (`0964729`). Both wait for review.

**Round dn (2026-10-29, orchestrator): T61 F2 continuation review.**
- **T61 F2 `CHANGES REQUIRED — continuation N1–N8`** (REPORTS "Round dn"):
  - Lane 2 continued after round dh with three unreported commits up to `be08b7b`: H1 is fixed in code but untested; H4 is fixed; H2 and H3 are partial. It also ran a final study but wrote no report.
  - The orchestrator measured that the study's own `--validate` refuses the untouched final study at `be08b7b`: the provenance hashes the CRLF working file but verifies LF git blobs (`core.autocrlf=true`).
  - Recorded metrics and config are not verified.
  - Continue on the same branch from `be08b7b`, same six-file scope, N1–N8 in order.
- **Lane 1** still has not started round dk's work (no commit since `b821bbe`). Its round-dk prompt stays valid (preflight re-run: 6/6).

**Round dm (2026-10-29, orchestrator): W4.1 review; MAP.2 released.**
- **W4.1 `CHANGES REQUIRED — W4.1 F1`:**
  - The owner ran `--play` and found it good, so the owner check is passed.
  - The orchestrator's review at `5f466cd` (REPORTS "Round dm") requires F1.1–F1.9: the turn cadence drifts about 4.5% slow (measured: 19.1 s of game time in 20.0 s of real time; relative sleeps accumulate); the thread loop is untested; tests (a)/(b)/(c) are narrower than briefed (two files, a 640×360 corner, three turns); a silent 12-bit id narrowing; the design choice is unrecorded; three mutations were not run; the full gates and measurements are missing.
  - Lane 3 continues on its branch from `5f466cd`.
- **MAP.2 `ASSIGNED — lane 4`** (a new executor session): the five-player map by the owner's layout rule (block below), three candidates, the fairness rule in the spec and the tool, previews, OWNER-CHECK. Tools and data only. Branch `task/MAP-2-original-like` from the round-dm board commit, worktree `E:\anno1503-worktrees\anno1503-MAP-2`, OQ-515..524.
  - The orchestrator measured the large templates: feasible clusters within ±10% land without suffixed templates exist.
  - Equal land counts show that suffixed templates are probably the same shapes as plain ones, so shape distinctness must be proven from the height masks.
- **Concurrency:** four lanes, each in its own worktree. Lane 3's new lan5 seam test loads `data/maps/lan5.txt` by path and must not depend on its content. MAP.2 leaves `lan5.txt` unchanged until the owner's pick.

**Round dl (2026-10-29, orchestrator): MAP.1 review; the owner's map decision.**
- **MAP.1 `DONE`:** code accepted at `1665f60`, report `328f267`, merged into main with `--no-ff` (6db51eb). The orchestrator re-ran these independently:
  - the fixed-map tests on MSVC (sim 5 cases with 166 assertions, formats 4 cases with 85 assertions) and on GCC 13.3 in WSL, both with the same five-minute lan5 hash `fec6c75b10075eab`, and a clean skip without the game files;
  - `agree.py --commands fixed-map` (1 of 1 agree); `fixed_map.py check`, `fairness` and `preview`; the layout script reproduces `lan5.txt` byte for byte; the synthetic fixtures reproduce from `tools/fixed_map_fixtures.py`;
  - clang-format 23.1.3 (13 files); `check_unix_warnings.sh` (GCC 13.3 and Clang 18, 178 files, clean);
  - the registry move: the before/after `sim_replay` outputs differ only in the wall-clock line; all five mutation logs fail the named assertions.
- **Not re-run:** `#286` and `#287`, the heavy render corpus tests. The memory coordinator refused them (`wait deadline`: about 5.2 GiB available against a 6.5 GiB reservation while the other lanes ran). MAP.1 changes no render code. They stay owed by the next full gate run on main.
- **Process notes for lane 3:** the report arrived about 15 minutes after the code commits. A review started on the code alone had no fact table and no Decisions list. Next time, commit the report before telling the owner the task is done. A ctest the executor started at 19:40 still held `anno_render_tests.exe` in the MAP-1 worktree during the review. End your own test processes before reporting.
- **Owner decision (map design, `lan-mvp.md` §8 question 6):** lan5's five rotated copies are rejected as too artificial and symmetric. The direction is **original-like, fair by measure**: varied positions and templates at the density of the original endless maps, with fairness measured within tolerances rather than built by copying. `data/maps/lan5.txt` stays as the tests' placeholder until **MAP.2** (below, `PLANNED`) replaces it.
- **Lane 3 continues with W4.1:** the executor had already started `task/W4-1-play-mode` from MAP.1's report head `328f267`, an ancestor of main (spec commit `1b4d940`). That base is accepted: main adds only the merge and docs on top of it. The branch continues; no restart and no rebase. The round-dl packet adds the review notes and a detailed brief. W4.1 does not depend on the map's design.
- **OQ ranges:** MAP.2 OQ-515..524.

**Round dk (2026-10-29, orchestrator; owner decisions of the same day): the LAN MVP lanes.**
- **Owner decisions:** (1) this Claude Code session is the only orchestrator; (2) the LAN MVP: one fixed map scaled for five players, no computer players (`docs/plan/lan-mvp.md`, PLAN §0.7). The MVP's open owner questions are `lan-mvp.md` §8.
- **Integrated on main:** round dj, drafted by the previous orchestrator and committed unchanged (fde5c8f); T32.5 F1 (merge 467828d, accepted at `1fc598eb`); T60 (merge 7d8e922, accepted at `ad786453`); the MVP plan (merge ca0d5af). All four are docs only; each merge's file diff equals the accepted branch's.
- **Lane 1 (the main executor):** part A = T32.6 F1 corrections, round dj S1 and S2, unchanged scope on `b821bbe`. Part B = **P2.1**, now `READY` (its one dependency, T32.5, is integrated), with the orchestrator's decisions in the lane-1 packet. This replaces round dj's "then F6": **T10c F6 is `PAUSED`** at `216a4b9`, unaccepted; round dj C1–C4 and the inherited F6 requirements bind when a later round resumes it. Reason: the MVP critical path comes first, and the current gates run with the harness as it is.
- **Lane 2 (the second executor):** T61 F2, round dh, unchanged. No lane-2 binding text changes in this round.
- **Lane 3 (a new executor session, started by the owner):** part A = **MAP.1** (the fixed map: format, loader, first layout, developer start); part B = **W4.1** (play mode: the sim in the app), branched from MAP.1's reported head because it uses MAP.1's shared registry loader. Both blocks are below in the LAN lane.
- **`PLANNED`, not released:** W4.2 (figures drawn per frame; deferred in round dq until figures move); W4.3 and W4.4 were released in round dq; ADR 0004 and W5; W3 after the owner approves ADR 0003; the docs-only RE of ships and settling (W6.1). After the MVP: W2, W7 (T-AI.4), W8.
- **New OQ ranges:** MAP.1 OQ-490..499, W4.1 OQ-500..509, P2.1 OQ-510..514.
- **Concurrency:** the three lanes work in their own worktrees and share only append-only REPORTS sections. Heavy gates go through the memory coordinator (round cp); about two full gate runs fit at once, the rest wait.

**Round dj (2026-10-29, orchestrator): correct the T32.6 signed contract, then finish the same F6 preparation/gate work.**
- **Verified starts:** clean T32.6 `codex/T32-6-house-tick-inputs` / `E:\anno1503-worktrees\anno1503-T32-6` at `b821bbe292878cbda5711ee4f7d88b0f882fb60d`, engine source unchanged at cfd85ec. Clean F6 `codex/T10c-f5-cold-cache` / `E:\anno1503-worktrees\anno1503-T10c-F5` at `216a4b97019f4fa2ac5e57cb5c87db0b4fb9a918`, tested code `9c26cf9dbb500617b3cd58d954f33c3cc4200bca`. No acceptance/merge of either. These starts supersede round-di starts; keep its T32.6-first then F6 order and original scopes. This is closure of T32.6 F1 and T10c F6, not permission for P2 or F7.
- **S1 — signed correction is wrong in the new note:** AnnoWelt `0x1007654b` compares BP with 64 and `0x1007654f` uses signed `jge`, verified against original DLL bytes and prior review disassembly. Under the downgrade arm, resident -1 therefore falls through: trunc(-1/64)-64 = -64 is passed to the counter operation, residents is set to 64 before dispatch. The note's unsigned explanation and negative row say the opposite. Correct §2.1, §3.6, mutants and report; independently check negative/0/63/64/65 and the floor/arm predicates without inferring deferred call effects. Keep removal separate. Do not import original narrow-width limits into widened engine state or change OQ-452 policy.
- **S2 — make the numerical/domain contract implementable:** the goods-name scan ends before id 0x60 (AnnoGame `0x10014861`/`0x10014864`); use a valid 0x5b..0x5f high-id counterexample or explicitly label an unreachable abstract input. Remove the proposal to impose n<=255 and goods<=0x5f on scalable engine registries: distinguish original-format validation from widened/configurable engine data under PLAN §0.1. Resolve the note's u32 E versus negative widened E' contradiction as a supported-domain/deferred-policy boundary, not a silent signed state change. State preconditions at the actual divisions (post-smoothing E, and mood-only E==0 versus a full tick's needs loop); label out-of-range mood scores before VermehrTab indexing. Replace the placeholders at the 101/102 spawn boundary with complete numbers (for example pre-decay stocks 102/103 give 101/102 when post-smoothing E=128, Menge=12), explicit walker inputs and all intermediates. Make spawned-predicate inputs consistent between §2.1 and §2.3. Preserve the useful loader/mood reads and qualified OQ-473 next address; OQ-450 is still an owner choice. OQ-472's loader derivation is credited, but not blanket acceptance of the whole note.
- **C1 — F6 progress is narrower than R1–R3 closure:** the prefix rejection, orphans numeric check, EOF-before-exit repair and unique F4/F6 directory names are useful. Code still makes readiness depend on a matching printed line, child.running and any TCP listener; it never binds that listener's build/compat/incarnation/PID to the owned process. Add a controlled foreign listener plus a still-live forged-line process; challenge and verify the actual coordinator identity. Keep normal compatible sharing unchanged. identity_probe uses substring matching; validate its complete response and fail closed on malformed/extra conflicting identity.
- **C2 — lifecycle/deadline contract remains incomplete:** both run_bounded variants break on direct-parent exit even if descendants retain pipes. On POSIX that returns without cleaning the group; new blocking waitpid(...,0) calls on timeout/fcntl failure violate the wall-bound requirement. Windows identity probes pass a pidfile path without launching a wrapper that creates it, and cleanup still trusts raw PID text; OwnedCoord still permits an empty start token to authorize kill. No actual cleanup reserve exists; hash/manifest/publication still run synchronously without per-phase bounds, and the final publish path has no post-publication deadline check before ok=true. Add short regression/fault cases for these actual paths and fix ownership/EOF/exit/reaping/deadlines together. Preserve parent-exits-before-descendant output and use controlled helpers, never unrelated PID reuse. The orchestrator's copied-source probe was prepared but not executed because automatic approval review timed out; these new lifecycle findings are code-derived, not claimed runtime reproductions.
- **C3 — publication/isolation proof still missing:** the rename-failure hook improves a normal rollback but does not prove recovery from a killed builder between binary and identity renames. The old file is still moved away and two files are published separately; the observer only searches for PARTIAL text. Add actual process-interruption and identity-consistency observers, failed identity/restore paths, and independent invocations using their own roots. Record recovery of the previous good generation and sanitizer preservation. A directory-name assertion is not the cross-process test. Do not claim R3 complete without the inherited evidence.
- **C4 — correct diagnosis, rebuild coherently, finish independent gates:** saved budget log is 80 passed / 2 failed / 14 skipped, not a green suite. Both F3 handovers remain failures to investigate. The report says it rebuilt support tests and coordinator, not anno_budget_probe; changing hashed sources changes the exact build. First record all executable identities/timestamps/hashes and perform a coherent full build (including probe), then reproduce the two tests in bounded private setups. A stale probe is a hypothesis, not an established cause; do not mask real admission/journal defects. The saved heavy line has free 11188883456 - headroom 4294967296 - pending 2788034560 = available 4105881600; the 1073741824 request fits that remainder. Comparing available against headroom+pending again double-counts them. This is NOT proof of shortage or of an admission bug: oldest-waiter/queue/domain/freshness/CPU constraints still need the bounded diagnosis assigned in round di. Preserve that 60-s per operation / ten-minute initial limit and do not rerun a multi-hour gate without changed-condition evidence. All final-source a–g/closure mutants, restored suites, full build/cold no-game/warnings/sanitizers/format/lint/oracle gates remain required. Resolve tooling availability (pinned clang-format 23.1.3 via a verified existing path or permitted isolated install), do not substitute an old version. No game-gate blocker excuses skipping independent gates. No policy/headroom/coverage/golden relaxation, shared coordinator or T61 process control, or admission/recovery/compatibility rewrite within F6.
- **Report and parallel lane:** append corrections and a requirement-to-case-to-evidence checklist; commit by concern and stop after both reports. FINISHED requires the assigned acceptance; missing gates remain explicit. Preserve all seven round-dh T61 selectors byte-identically, all earlier first-lane blocks as history, and completed P1/T10c/T32.5/T60. No active T61 head/cleanliness reads, files/process changes or messages. New lane-scoped preflights are under cache/orch-dj; the optional second-lane preflight is additive, not a new assignment. No merge/rebase/cherry-pick, push, PR, owner/ADR policy choice or next feature.

**Round di (2026-10-29, orchestrator): release the independent T32.6 audit; F6 needs preparation closure as well as the game gate.**
- **Verified:** F6 is clean at `6f40a71275d17e76115628bac892032d8d71d359`, tested source `c79ca37fbdd200df0fee897db4d13cadc4d321cd`, existing `codex/T10c-f5-cold-cache` / `E:\anno1503-worktrees\anno1503-T10c-F5`. T32.6 is clean and unchanged at `cfd85ec59d38f986bf1f0bde7d2ddcc5e6e2db88`, existing `codex/T32-6-house-tick-inputs` / `E:\anno1503-worktrees\anno1503-T32-6`. Saved F6 no-game CTest completes 1121 listed tests with two skips; sanitizer budget is 74 passed / 607 assertions / 14 skipped. Game CTest ends at 1109/1121 (1105 passed, four wait-deadline failures), exit -1, no completion summary. No merge acceptance.
- **Scheduling/preflight correction:** the old part-B preflight compares the whole T61 task against round-dd F1 and checks the entire board against 63af432. Authorized T61 F2 evolution therefore blocked unrelated T32.6. This is an orchestration coupling defect, not an executor/task failure. Use the new lane-scoped checks. Explicitly override the old first-lane order: **first complete T32.6 F1 from cfd85ec, then resume the same T10c F6 from 6f40a71**, report each, and stop. No new feature/F7 or import of main. T32.6's F1.1–F1.5 content and two-file scope are unchanged. F6's F6.1–F6.6 content remains binding, with the concrete closure below. A game gate cannot hold the independent audit hostage.
- **R1 — artifact/readiness is not proved by labels:** on exact copied c79ca37 preparation source, a 36-byte `ELF` prefix plus expected marker with a matching manifest returns `ok=1 reused=1`; it is not an executable. Existing fake-ok tests intentionally construct the same sort of payload and assert success. Replace that false success oracle with a minimal real executable or explicitly test only a lower-level operation. Production reuse/publication must verify a loadable runnable expected coordinator/configuration under supervision, including sanitation coverage. `orphans=garbage` also passes the LISTENING parser. Validate all fields/ranges and bind readiness to the live owned process and its actual listener/incarnation, not a matching printed line. Add forged-correct-line/no-listener, print-then-exit, wrong config/build/version/port and invalid/truncated ELF regressions; preserve compatible runtime sharing.
- **R2 — deadline and lifecycle:** the same bounded Linux probe sets a 1-ms preparation bound for a 2-MiB warm artifact but returns success after 20 ms, because validation/hash/publication have no deadline checks. A child which closes stdout/stderr then sleeps 200 ms and exits zero is instead killed/classified timeout after 11 ms despite a 1-s allowance. Track process exit and pipe EOF independently; reserve cleanup within the single wall budget, bound validation/I/O/publication, and forbid success after expiry. Windows cleanup currently adds separate two-second waits after expiry; job/setup return values and POSIX spawn/action/nonblocking errors must fail closed. Audit owned descendant cleanup after parent exit, PID reuse and missing ownership evidence; Windows compiler cleanup uses a raw pidfile and test cleanup kills when the saved start token is empty. Do not demonstrate PID reuse by risking unrelated processes. Controlled owned helpers and injected ownership mismatch are required.
- **R3 — isolation/publication remains incomplete:** f6_dir is the shared temp/anno1503-f6 with fixed child names and remove_all; f4 journals/scripts also use shared fixed names or a per-process counter without process identity. Separate invocations can delete/rewrite each other's artifacts despite CTest's local RESOURCE_LOCK. Give the entire test run an owned unique namespace, inherited by only its intended children. Test independent invocations/worktrees and clean only proven-owned paths. Publication moves the old binary aside before replacing it, then publishes the identity separately; the current observer checks only for the text PARTIAL. Exercise interrupted replacement and concurrent observers across the binary/manifest pair, missing/mismatched identity, failed restore/write and preservation/recovery of the prior good artifact. Keep valid sanitizer binaries instrumented.
- **R4 — evidence closure:** regressions first on 6f40a71/c79ca37, then final-source tests and the inherited a–g matrix with exact failure/timeout classifications and owned-child cleanup evidence. Credit existing mutation failures honestly; the deadline mutant currently reaches `CHECK(got.finished)` with outer-wall timeout, which does not establish cleanup or the one-deadline contract. Rebuild restored source, verify identities, and run required gates on the final code. Diagnose the game wait with bounded read-only host/WSL physical/commit capacity, meter epochs, outstanding reservations/owner liveness, CPU slots and oldest-waiter/queue evidence. The saved capacity-before file contains only free/total and a process listing. `available > estimate` alone is insufficient: the admission code also reserves room for the oldest waiter. Neither external shortage nor an admission defect is established yet. Do not repeat multi-hour tests blindly. Preserve a bounded isolated reproducer if admission logic is implicated; do not change admission/recovery/compatibility policy within F6. Run the fresh full game gate only after recorded changed conditions or evidence resolving the blocker. If it cannot safely run, retain BLOCKED with exact unmet evidence/gates; do not lower headroom, stop shared/owner/T61 processes or discard journals.
- **Parallel lane:** T61 F2's seven round-dh packet selectors remain byte-identical to c979899, including all H1–H4 and inherited gates; its reported d0db15c progress is not acceptance. Never inspect/pin its active head/cleanliness, modify its files/processes or message it. `cache/orch-di/second-lane-preflight.ps1` is an additive revalidation option, not a new T61 assignment; it avoids global-board and moving-first-lane guards. Preserve completed P1/T10c/T32.5/T60. All earlier first-lane binding blocks are also preserved as historical instructions, overridden only by this explicit sequence/start/closure paragraph. No merge/rebase/cherry-pick, push, PR, ADR approval, owner-policy choice, T40 or P2 implementation.

**Round dh (2026-10-29, orchestrator): T61 green suites are verified; F2 acceptance is still incomplete.**
- **Reviewed:** clean `codex/T61-join-jitter` at `30964c278202bb72a26f6a45541ba0e7173ec4a4`, code `e941d64c167070a1ba71f17f1fa31a929ab65f7b`; their diff is REPORTS only. Saved logs confirm 222 join tests and 64 snapshot tests passing. Independent focused run passes 60/60 (31 legacy + 22 recovery + 7 jitter); a 1-ms worker-timeout probe returns timeout with zero live owned children, and an ordinary short worker completes. Stale callback identities and the mixed-audit regression are useful fixes. No full-suite or full-study rerun by review; no acceptance/merge yet.
- **H1 — outcome/config requirements still fail:** exact final-source probes show all validators accepting a wrong-time extra arrival, an orphan arrival with changed sid, and an outcome whose transmission is marked lost. Boolean attempt, negative epoch and missing endpoint scripts remain accepted. F2.2 requires both directions of identity conservation, not only one matching-time outcome per successful send. Keep genuine stale/retry/refused/pending traffic valid and pin these previously reported counterexamples before fixing them.
- **H2 — manifest is not an integrity check yet:** an explicit review fixture aligned to the copied source's provenance passes even after inventing result/trace hashes, dropping all metrics except validators=[], changing config rate/jitter, setting wall_seconds=NaN or 301, or removing both declared diagnostics. The original observed file was never relabeled. Independently, the untouched saved study's --validate fails at the final report head because provenance compares current HEAD to the earlier tested code head. Bind a study to its recorded immutable source/config/artifacts, allow verified report-only descendants for inspection, and reject forged/incomplete/mixed evidence. Validate finite bounded timing, full effective config, explicit complete metrics/validator status, exact diagnostic inventory, result/trace hashes against retained evidence and terminal/aggregate status. Never 'fix' provenance by rewriting the saved manifest.
- **H3 — the asymmetric diagnostic is inert:** its script targets epoch 1, while the short run emits no matching transmission. Independent run counts zero script-selected transmissions; its result AND semantic trace hashes equal the matching zero-jitter primary row. Select an actually emitted immutable identity, assert the expected nonzero delay and independently derived timing/reordering effect, and require coverage of every declared script entry (or an explicit justified unused-entry error). A label alone does not establish an asymmetric experiment. The exact-repeat diagnostic must match the declared primary projection.
- **H4 — remaining metrics/gate evidence:** maximum_lag still samples only apply events; H=100 then input_on(J1, applied=98) with no later apply returns 0 instead of 2. Sample relevant membership transitions too; complete retained-ownership and pending-traffic metric evidence. Only one regression test was added since d8b287c; no final real J1–J8 mutation/equivalence/fresh-process gate evidence or current ADR/OQ reconciliation was supplied. Existing J tests mostly alter recorded labels and cannot replace the required scheduler mutations. Finish all inherited F2.1–F2.5 and A–G/F1 requirements, not only these examples. Record bounded supervisor tests and source/config/metrics phase evidence as required.
- **Disposition/continuation:** same T61 F2 closure, starting at `30964c278202bb72a26f6a45541ba0e7173ec4a4`; same six-file scope. First add failing tests for H1–H4, then fix, pass full correctness/mutation/baseline/determinism gates, and only then produce a fresh final 72+declared-diagnostics study under unchanged 300/5400-second bounds and 4096 MiB headroom. The observed 72+2 file (SHA-256 5d81f880fd86c5998c3bce0339509b92881cf2a4dca5d092539ca2621f91888c) is historical partial evidence, not final acceptance. Report item-by-item proof or an honest blocker; no new permission is needed for listed in-scope repairs. First-lane round-de seven selectors remain byte-identical to 63af432; `cache/orch-dh/primary-preflight.ps1` updates only board validation. No active first-lane head/cleanliness checks, task messaging, merge/rebase/push/PR or policy/ADR approval. T40 stays DONE.

**Round dg (2026-10-29, orchestrator): resume the already-authorized T61 F2 work.**
- **Verified checkpoint:** clean `codex/T61-join-jitter` at `d8b287ccdf3d0b2399c4cabdf0620d2b6645a2d8`, parent `42ba698042c0ecf679ee6ab2541dc23a75e592ff`, in `E:\anno1503-worktrees\anno1503-T61`. Only a four-line checker edit and eight appended REPORTS lines changed. No new tests or other F2 repairs were committed. Independent exact-source runs confirm legacy link fixtures 31/31 and focused recovery/epoch/reassembly 22 tests / 5 failures. T61 remains incomplete and unaccepted.
- **Partial compatibility fix needs one more guard:** `_check_link` decides the legacy exception per enqueue record. In a genuine annotated zero-jitter trace, deleting jitter_ms/jitter_id from only one link_enqueue still passes ALL validators. This is not a wholly legacy trace. Reproduce and pin this counterexample before changing the predicate; apply the shared whole-trace legacy rule consistently, including supplied partial fields on sends/enqueues/outcomes. Do not revert the 31 valid legacy cases or tolerate contradictory/mixed evidence.
- **Stale identity is in scope, not a new permission question:** independently reproduced actual stale data and ACK records with lid=None and missing immutable channel/attempt/ACK-ordinal evidence. Continue F2.2 now: capture identity at the original transmission/scheduling boundary, propagate it to callbacks and stale/refused records, and match it independently against raw sends/epoch transitions. Never reconstruct it from mutable current channel state or guess among same-time packets. Distinguish stale timer/control events from data/ACK outcomes. Preserve scheduling/PRNG/state behavior and all legacy fields under the baseline projection.
- **Continuation:** same T61 F2, same six-file scope and all original A–G/F1/F2 gates, starting at d8b287c. Complete outcome conservation/config tests, worker supervision, manifest/metrics, full suites/mutants/equivalence/determinism and a fresh final study in the prescribed order. Do not request a new assignment merely because the next required implementation step or ordinary test is unfinished. A session interruption may leave a truthful resumable checkpoint; it does not create a policy blocker or require reauthorization of this scope. Stop only at completion or an evidenced dependency/access/policy conflict outside the authorized work. Preserve 300/5400-second study bounds and 4096 MiB headroom.
- **First lane preserved:** every round-de packet selector stays byte-identical to `63af432f148f15ebd0e660778c4e199df0cad3a7`. The additive continuation check is `cache/orch-dg/primary-preflight.ps1`; it excludes all active task heads/cleanliness. No first-lane restart, messaging, worktree/process change, main merge/rebase, push, PR or ADR approval. T40 remains DONE; completed P1/T10c/T32.5/T60 stay protected.

**Round df (2026-10-29, orchestrator): T61 F1 remains incomplete; finish correctness before repeating the study.**
- **Reviewed:** clean `codex/T61-join-jitter`, `E:\anno1503-worktrees\anno1503-T61`, head `42ba698042c0ecf679ee6ab2541dc23a75e592ff`; tested code `787504169f5d88990868df3aec0488aedda5294e`, starting `de5cfa9bb0cf4d0dd23ede89f4bb1890eef342da`. Only checker, runner and report changed in F1; no new tests or ADR/OQ corrections. Indexed analysis is useful progress, but F1.2–F1.5 and original A–G acceptance are not complete. No merge or acceptance.
- **Verified gates/evidence:** final full log is `cache/t61-f1/final-join.err` (814888 bytes); the reported `.txt` is empty. It records 221 tests / 21 failures in 261.213 s. Independent review reproduces 14/31 hand-written fixture failures (31/31 pass at pre-T61 `917efea`) and false missing-outcome errors on valid un-serialized stale traffic. The saved study does contain all 72 distinct primary labels (sum recorded row times 152.198 s, max 11.345 s), but its validator and provenance remain insufficient for acceptance. The 7.182 s pilot is executor-measured, not rerun by this review.
- **Remaining defects, independently checked:** `_check_link` still requires jitter fields on legacy fixtures although `_check_jitter` exempts them; un-serialized stale records lack a stable transmission identity; extra wrong-time/orphan/lost-packet outcomes can pass all validators. Boolean attempt, negative epoch and missing endpoint script keys are accepted; string range raises TypeError while formatting the diagnostic. Study validation accepts 72 fabricated rows with empty config, invented hashes, negative elapsed time and absent validator results, as well as 13 diagnostics. The declared repeat diagnostic instead passes a malformed script tuple and fails config validation. Deadlines are still checked only after the blocking row/study returns. Lag ignores input_on's applied value (a hand vector expected 3 returns 101); peak retention/pending-traffic evidence and source/config provenance remain incomplete.
- **Decision:** assign T61.F2 below on the existing branch from `42ba698`, same six-file scope. Fix and test all remaining F1/A–G requirements; do not limit the task to making the 21 failing assertions green. These are in-scope defects to repair, not grounds to stop immediately as an external blocker. Preserve valid legacy fixtures and strict invalid-evidence rejection, underlying simulation semantics, all default policies, and 300/5400-second host-wall limits. Correctness/mutants/baseline gates precede a fresh final study. Prior 72 rows remain historical performance/partial evidence and must not be relabeled final after code/validator changes.
- **First lane unchanged:** round-de plus sections 1/2 and task entries T10c.F6, T32.6.F1, T10c.F5 and T32.6 remain byte-identical to `63af432f148f15ebd0e660778c4e199df0cad3a7`. Its starts/scope/order are unchanged. `cache/orch-df/primary-preflight.ps1` provides an additive board continuation without requiring any active executor head/cleanliness. Do not inspect, restart, message or modify first-lane worktrees/processes. P1/T10c/T32.5/T60 remain protected; no merge/rebase/push/PR, ADR approval, production networking or T40 work.

**Round de (2026-10-29, orchestrator): first-lane review; T10c F6 then T32.6 F1.**
- **Verified identity:** the pasted round-cy handoff is inconsistent with local Git. Its two supplied hashes do not resolve and its branch names do not exist. Actual clean results are `codex/T10c-f5-cold-cache` at `5dc055692503dfd37facd954bcf2500bd47e05e6` in `E:\anno1503-worktrees\anno1503-T10c-F5`, and `codex/T32-6-house-tick-inputs` at `cfd85ec59d38f986bf1f0bde7d2ddcc5e6e2db88` in `E:\anno1503-worktrees\anno1503-T32-6`. Both start at `fa4820447e53a4d50468e8edb24a865a2147d15f`. These verified heads, not the pasted hashes, bind the continuation.
- **F5 disposition: F6 required, not accepted.** The saved final no-game CTest is green (1103 listed, two expected skips, 96.23 s). The sanitizer log reports 56/56 budget cases and 538 assertions, but cold Linux preparation compiles a replacement with plain `g++ -O2`, so that log does not establish sanitizer coverage of every tested coordinator. The actual open full-suite gate is game-dir CTest (1800-second admission deadlines), not the WSL suite alleged in the handoff. Source review finds unbounded compiler/lock waits, fail-open lock acquisition, weak artifact/readiness validation, and cache-mutation isolation gaps. These are correctness/acceptance defects, separate from the resource blocker.
- **T32.6 disposition: F1 required, not accepted as a complete audit.** The producer inventory, three-path distinction and partial-kernel direction are useful. However, the required loader/post-pass remains deferred to OQ-472; the note incorrectly assigns both endpoints to the post-pass and associates endpoint derivation with Force. AnnoGame `0x1001487e`–`0x100148a0` writes `+0xcc` during parsing; `0x10014b25`–`0x10014b6d` writes `+0xcd`/forced infrastructure/alias, followed by substitution and `max(cd,cc)`. The proposed kernel lacks inputs for its promised would-fire flags; the audit omits concrete numerical boundary vectors and must distinguish direct effects from transitive transition effects. OQ-450 remains open under the explicit P1 decision 4 caveat; this review does not reset exact saves or choose owner policy.
- **Assignment:** complete new entries T10c.F6 (part A) then T32.6.F1 (part B) on their existing branches, without importing main. Part B is independent and proceeds after A is either reported complete or blocked with concrete evidence. Finish both reports and stop; no P2 implementation. Original round-cy requirements remain applicable except where these explicit continuation entries supersede setup/design details.
- **Parallel lane preserved:** round-dd and the complete T61 task block remain byte-identical to `b41bdedc9af07e94efbc5dd71d9e98e5c4b03b68`. T61 F1 continues with the same scope, base and 300/5400-second limits. Do not inspect/require its active head or cleanliness, message it, or change its files/processes. If that executor repeats its preflight after this additive board change, `cache/orch-de/second-lane-preflight.ps1` verifies unchanged bindings without constraining active work. Completed P1/T10c/T32.5/T60 remain protected. No merge, rebase, push, ADR approval or T40 work.

**Round dd (2026-10-29, orchestrator): T61 partial review and optimization decision.**
- **Reviewed:** clean `codex/T61-join-jitter` at `de5cfa9bb0cf4d0dd23ede89f4bb1890eef342da`, from `917efea52215907ad41c04821226f017a0976c81`; implementation `f8164ee`, runner `d897c72`, docs `7592332`, report `de5cfa9`. Six permitted files; diff check clean. Baseline log records 215 join tests passing before changes; six new jitter tests and 64 snapshot tests are reported passing. Only five primary rows are preserved. T61 is not accepted; the study timeout is not the only remaining gate.
- **Answer to the QUESTION:** authorize a scoped optimization follow-up on the same branch, preserving simulation semantics. Keep **300 seconds host wall time per row and 5400 seconds total study wall time**, including validation/metrics/output work. Do not raise bounds, change workload/horizon, bypass validators, relax caps, alter seeds/profiles or tune policy defaults. If an optimized, correctly bounded run still cannot finish, report measured phase timings and a concrete proposed change for separate review; no owner limit revision is presumed necessary now.
- **F1.1 — avoidable quadratic validation.** `_check_jitter` repeatedly searches the whole event list for finish/arrival/outcomes for each transmission, and is bypassed for default zero jitter. Independent bounded probes show 200/400/800 transmissions taking about 0.0145/0.0462/0.2187 s; a genuine 770-event trace makes 254,639 profiled calls. This proves a quadratic hotspot, not the exact share of the six-minute run (no profile of that full row was supplied). Profile simulation, validation, metrics and hashing separately; index raw events once without losing identity, multiplicity, horizon or independent reconstruction checks. Keep the old small-run semantic traces/results for comparison.
- **F1.2 — validator regressions and incomplete identity/cardinality.** On copied exact sources, `JoinJitter LinkValidatorsOnHandWrittenLogs` runs 37 tests with 14 failures; the old 31 hand-written cases pass at `917efea`. `_check_link` now requires jitter metadata even on old zero-jitter valid fixtures. Separately, duplicating a valid ACK or arrival in a short un-serialized jitter trace passes all validators, while a real stale data arrival after a pair reset fails `_check_jitter` in both modes because the actual stale record lacks its assumed ch/sid/attempt fields. Require a unique transmission/outcome association including epoch/channel/attempt or ACK identity; missing/extraneous/duplicate outcomes, outcomes for lost packets and forged epochs must be rejected, valid stale/refused/pending outcomes accepted. Define explicit legacy zero-jitter evidence compatibility without excusing nonzero or supplied contradictory evidence. Reject non-integral range/seed values and malformed script identities (float ranges/seeds and data attempt 0 currently pass config validation).
- **F1.3 — gates not demonstrated.** New J1–J8 cases mostly edit log labels, not the named scheduler defects; J7 swaps sample labels rather than arrivals. The zero-jitter test compares two runs of the new code, not the pinned baseline. Finish the original A–D/F gates: old-source comparison across named rows, genuine named behavioral mutants with independent assertions, hand vectors, scripted boundary/reorder/epoch cases and fresh-process determinism. A deliberately invalid log is useful validator coverage but cannot alone prove a scheduler mutation is caught. Preserve every old gate; do not remove tests to obtain green.
- **F1.4 — study validation and bounds are ineffective.** Independent CLI probe: 72 identical minimal copies of one primary row are accepted as a full study, without config/hash fields. `--diagnostics` labels the unscripted repeat as a 73rd primary. Per-row time is checked only after `run()` returns, aggregate time only after all rows, and failure raises before persisting the failing result. Enforce actual host-wall deadlines with an owned worker/controller or equivalent bounded mechanism, sequentially; save timeout/error/partial status and resume provenance. Validate the exact Cartesian product once each, explicit diagnostics, required fields/full effective config, head/algorithm/source hashes and recomputable summary/artifact hashes. Hash claims need retained evidence; document the trust boundary, do not pretend a hash proves a valid trace. Never silently overwrite prior partial evidence or combine incompatible heads/configurations into final acceptance.
- **F1.5 — metrics and claims.** `maximum_lag` includes loading joiners, so the reported 1616 is not proven seated-peer lag; a tiny trace with seated P1 lag 2 and loading J1 lag 100 reports 100. Track membership/activation/drop/rejoin at each event, keep maximum versus horizon lag distinct, add required peak retention bytes/turn ownership (only final retained count exists), and distinguish pending traffic from generic event-queue length. Publish coverage as five provisional rows, not a completed 72-row result; retain BLOCKED/IN PROGRESS qualifications until all gates pass. Correct ADR/OQ current jitter references and separate planned capability from verified evidence.
- **Assignment:** T61 F1 only, existing branch/worktree from `de5cfa9`; no main merge/rebase or new worktree. Same six-file scope. Fix F1.1–F1.5, then complete all still-applicable T61 A–G requirements and stop for review. Fresh full study uses one final tested source/config manifest; old five rows are historical evidence, not automatically reusable acceptance. First-lane bindings/bases remain unchanged; `cache/orch-dd/primary-preflight.ps1` is the additive continuation check. T60/T32.5 pending integration, T40 DONE, ADR Proposed; no push or new task.
- **Review artifacts:** bounded probes, profile and exact copied sources under ignored `cache/orch-dd/`; no full problematic row rerun, no game/C++ work or task-worktree edits. Findings are direct source evidence or named bounded reproductions, not an inference that extra time would make acceptance pass.

**Round dc (2026-10-29, orchestrator): comprehensive second-executor T61 packet.**
- **Owner instruction:** provide a more comprehensive task and instruction prompt. Assign the whole T61 research-checker workflow below, not just a small documentation change: baseline and contract, failing regressions, implementation, independent validators, adversarial/mutation tests, a 72-row matched experiment, and final evidence/report. Continue through these phases without stopping after a partial implementation; stop at the final review boundary or a concrete blocker.
- **Why this task:** main's `transmit` handles labeled data delay, `flush_ack` uses zero extra delay, and `_check_link` reconstructs zero extra delay for ACK-only datagrams. Non-serialized ACKs have no `ack_send` record today. A first-class jitter model therefore needs both paths and trace validation, not merely another data-delay map. Existing `LinkContract` smoke checks pass 10/10 at `6a1f00c`; no full suite or experiment has been run by this planning pass. ADR 0003 is Proposed and OQ-371 remains open.
- **Isolation/baseline:** new `codex/T61-join-jitter` in `E:\anno1503-worktrees\anno1503-T61`, from the exact committed round-dc main/board head pinned in the packet. Names were unused at preparation. Do not branch from T60, import its whole OQ file, or change a completed worktree. T61 does not depend on unmerged T60/T32.5 or active F5/T32.6. Completed P1/T10c/T32.5/T60 heads are preflight guards; active first-lane worktrees are not.
- **Parallel scope:** first-lane round-cy/T10c.F5/T32.6 blocks, bases and order stay byte-identical to `fa4820447e53a4d50468e8edb24a865a2147d15f`. If its repeated preflight detects this addition, read this round and use `cache/orch-dc/primary-preflight.ps1`, which verifies the unchanged blocks and current board. No restarting, rebasing or messaging that executor. Only append-only task REPORTS sections share a logical path in separate worktrees; all other allowed T61 files are disjoint. Python studies run one process at a time under the existing memory/headroom policy.
- **Bounds/decision ownership:** jitter profiles are explicit synthetic experiment assumptions, not original-game facts, measured LAN properties or adopted engine limits. Preserve 300 ms default retry, existing timing/cap policies and ADR Proposed status. T61 may expose a bounded failed/incomplete outcome; do not tune limits or defaults to manufacture a pass. No production network implementation, owner-policy decision, AI/house task, game launch, Wine, merge/rebase or push. The executor reports exact heads/results and stops for review.

**Round db (2026-10-29, orchestrator): T60 F1 accepted.**
- **Exact state:** clean `codex/T60-ai-post-list` at `ad786453b9ec6181aa80ac6a78efb0376d5780a9`; corrections `b599a867`, `80b6ef51`, `1dc771b3`, report `ad786453`, all following `0e7f2ddd`. Read-only preflight passed 15/15 with protected P1/T10c/T32.5 unchanged. Three allowed files only, raw Git REPORTS blob append-only, only OQ-270 changed, diff check clean.
- **F1.1/F1.2/F1.3 accepted:** fresh original-DLL extraction agrees with all 16 documented indexed destinations. Conditional state-7 transition, state-2/3 guards, state-6/default no-op, state-0 success/failure stores and global counters match the instructions. Adjacent flag/callback functions are excluded from direct dispatcher edges. Both early helper returns, selected-record decrement, corrected LEA, separate loop/fallback conditions and unchanged quotient are verified. Exact saved extraction commands and complete return instructions satisfy the bounded evidence requirement.
- **Non-blocking report clarification:** its F1.1 phrase "indexes 0..7 to physical bodies 0,1,7,2,3,4,5,6" confuses the two orders. That sequence is the state labels in physical code order; index-to-physical-position is `0,1,3,4,5,6,7,2`. The actual RE table and evidence correctly give all destinations. This review supplies the explicit correction without rewriting the historical report or requiring another fix round. The raw Git report blobs pass the byte-prefix check; the report's CRLF caveat concerned its working-file comparison, not committed history.
- **Disposition:** T60 is accepted as bounded RE, pending integration; OQ-270 remains narrowed with transitive effects open, not resolved or implemented. Second executor stays stopped. No new T61/T-AI.4/OQ-271/OQ-272 task, merge/rebase, push or ADR approval. T40 remains DONE; T32.5 remains accepted pending integration.
- **First lane preserved:** round-cy/T10c.F5/T32.6 bindings remain byte-identical to `fa4820447e53a4d50468e8edb24a865a2147d15f`. If repeating preflight after this additive board update, read this block and use `cache/orch-db/primary-preflight.ps1` to verify those unchanged bindings and the current board. No base/order/scope change or active-worktree check. Evidence and acceptance rationale are in the round-db REPORTS section; original-file outputs remain ignored under `cache/orch-db/`.

**Round da (2026-10-29, orchestrator): T60 review and bounded F1 correction.**
- **Reviewed state:** clean `codex/T60-ai-post-list` at `0e7f2ddd489c4efa8b19d45e8eefb075d0cef96f`, following evidence commit `3c063d90b2ef25a648dfd423b2f61f1460b90bb7`, based on `9c7c5d91b4e9a9677eeb436d12f618f50d3fc060`. Three allowed files only; REPORTS append-only; diff check clean. Approved read-only preflight passed 15/15 including the three protected completed worktrees and T60. No active first-lane worktree was inspected or modified by this review.
- **Accepted:** both manager calls pass quotient then tick as object arguments; helper pointer aliases argument 2, not argument 1. Both object functions return the unchanged quotient. Keeping transitive effects open is allowed by T60's bounded scope and is not itself a finding. T60 is **not accepted overall** because its direct dispatcher contract is wrong.
- **F1.1 — decode indexed targets, not address-order bodies.** Original dword tables `CompLogic 0x1003e2ac` and `Classic 0x1003b74c` map cases 0..7 to bodies in order 0,1,3,4,5,6,7,2 by physical placement. State 1 goes to 7; state 7 calls `0x1000d5f0` / `0x1000be10` and advances to 2 only when AL is nonzero. States 2/3 contain the `+0x1ca0 == 0x10` skip guards; state 4 advances to 5; state 5 resets to 0; state 6 and out-of-range values return without a direct state write. The new docs instead shift case labels and lose the conditional transition.
- **F1.2 — separate the adjacent function and include the real global write.** The direct dispatcher ends at `ret 4` at `0x1003e2a9` / `0x1003b749`, followed by its table. The flag/callback code at `0x1003e2d0` / `0x1003b770` is not a case or direct edge from that dispatcher. Remove the claimed case-7 flag/callback effects and their unsupported place in OQ-270's direct next-call list. The actual case-0 failure path (AX = `0xffff`) increments global dword `0x100692d8` / `0x10066250`. Case 0 always stores the returned word to manager `+0x30`; success stores manager `+0x2c=1` and `+0x28`. These are caller stores, not direct stores by selector `0x1003e130` / `0x1003b5d0`.
- **F1.3 — complete the bounded helper/return evidence.** Include the selected record's `+0x10` decrement at `0x10003a0e` / `0x1000230e` and the early returns before the pointed-to slot is cleared. Correct the second CompLogic LEA citation from `0x10003ab6` (push) to `0x10003ab2`. Distinguish tick input, accumulation slot, record/count loop conditions and post-loop fallback test; do not imply a proven time unit or a Classic-only behavior from differing object offsets. Supply complete instruction ranges through returns, exact reproduction commands and ignored evidence paths.
- **Next second-lane assignment:** T60 F1 only, on the same branch/worktree from `0e7f2ddd`; do not merge/rebase main or recreate the branch. The F1 block below supersedes T60's original new-worktree setup. Read the new board through its packet without importing it into the branch. Correct all current summaries/OQ-270; append a new report explicitly superseding the old errors, preserving the historical report. Commit by concern, then stop for review. No T61/T-AI.4/OQ-271/OQ-272 expansion.
- **First lane unchanged:** round-cy, T10c.F5 and T32.6 binding blocks remain byte-identical to `fa4820447e53a4d50468e8edb24a865a2147d15f`. Its bases, order, scopes and stop boundaries remain in force. If a repeated preflight detects this additive board update, read this round-da block and use `cache/orch-da/primary-preflight.ps1`, which verifies those unchanged bindings and the updated board. Do not restart/rebase active work or require active worktrees to be clean. The earlier packets remain historical; this narrowly updated preflight does not waive other mismatches.
- **Review evidence:** fresh, read-only original-file reads and indexed table extraction under `cache/orch-da/`, reproduced by `python -X utf8 cache/orch-da/review_t60.py`. No original bytes/disassembly are committed. No build, game launch, Wine, merge, push or task-worktree edit by this review. P1/T10c remain integrated in order; T32.5 F1 accepted pending integration; T40 DONE, ADRs Proposed.

**Round cz (2026-10-29, orchestrator): independent task for a second executor.**
- **Owner instruction:** prepare another task while the first executor works, to make progress in parallel. Assign **T60 only** to the second executor: the post-list manager call and returned-value questions in OQ-270, for CompLogic and Classic. It is docs-only and needs no heavy build/corpus run, so it can proceed alongside F5 within the existing memory policy. The concrete entry points remain explicitly unread in the current RE/OQ notes.
- **First executor unchanged:** the round-cy packet, F5 and T32.6 task text, their pinned base `fa4820447e53a4d50468e8edb24a865a2147d15f`, order, file scopes and stop boundaries remain in force. The orchestrator verified those binding blocks byte-for-byte against that commit. Do not restart, rebase or switch the first executor's worktrees. T60 is not another part for that executor.
- **Additive board/preflight update:** if the first executor repeats round-cy's preflight, its global board-version check will detect this addition. Re-read this round-cz block only, then use `cache/orch-cz/primary-preflight.ps1`: it verifies unchanged round-cy/F5/T32.6 blocks and reruns preflight against the updated board while retaining the original base and protected completed-worktree heads. This authorizes only the already-assigned first lane; its packet is not rewritten. Any other mismatch still stops with the exact QUESTION.
- **Second lane isolation:** new `codex/T60-ai-post-list` / `E:\anno1503-worktrees\anno1503-T60`, from the exact round-cz board commit. It edits only complogic RE, OQ-270/necessary OQ-480..489, and its own append-only REPORTS section. F5 and T32.6 do not edit those RE/OQ files. The shared logical REPORTS path is an explicit append-only exception in separate worktrees; never edit another task's section or physical files. No checks require an active first-executor worktree to be clean or frozen at its starting head.
- **Scope stays:** T40 is DONE, OQ-240 answered. T60 does not reopen it or start T-AI.4/OQ-271/OQ-272. T61 and P2 remain later, ADRs Proposed. Each executor stops at its own packet boundary; no merges/pushes or cross-task messaging performed by this preparation.

**Round cy (2026-10-29, orchestrator): T32.5 F1 review and next packet.**
- **Verdict:** accepted at `1fc598eb0b57f0d487fb50d315d7c4ad9c0d8af2`. F1 is five commits on base `e232b02` (four concerns plus report), not one commit whose immediate parent is `e232b02`; report tip's parent is `5416fb1`. The diff is limited to the four allowed docs and whitespace-clean; original REPORTS text is an unchanged prefix. T32.5, P1 and T10c worktrees are clean; P1 remains `416993fa`, T10c `5b10ed3`. No new merge or push.
- **Fresh evidence:** reran the inspected histogram script over the original 50 scenes/373 heads: all low/packed/high/full histograms reproduce, 78 nonzero high masks and 263 full values equal 1. Read original AnnoWelt `0x1001d389`–`0x1001d3df`, `0x100296a2`–`0x100296d3`, `0x100298e0`–`0x10029917`, and the city gate `0x10029610`–`0x10029638`: mapping, unsigned comparison, skip past both OR/setter and modular timing match the correction. OQ-460/461/462 headings occur once each; 463..469 unused. No build needed for this docs-only review. Evidence under `cache/orch-cy/`; detailed review in REPORTS.
- **Allowed open items:** S9a/OQ-185, S9b/OQ-462, OQ-460 and OQ-461 retain next addresses. No claim that they are answered, no new F2. T32.5 awaits integration; P2.1 remains blocked on that integration.
- **Next executor packet, ordered parts:** **A: T10c F5**, repair self-contained cold-cache preparation/readiness for N4 and prove the descriptor regression still detects its layer. **B: T32.6**, an independent docs-only house-tick input/lifecycle audit and implementation-readiness plan. Each gets a new branch/worktree from the packet's exact round-cy main commit; B never branches from or imports F5. After A's report/commits (including a clearly reported external blocker), proceed to independent B, report/commit and stop. No other task is authorized by this packet.
- **Scope/decisions:** F5 gets fail-first cold/warm/parallel/companion checks and bounded failure/concurrency checks, not a change to memory/admission policy. T32.6 reads accepted T32.5 docs at `1fc598eb` without merging/copying them over main's P1 docs; its output is a new readiness note plus REPORTS, so the branches do not overwrite each other's spec changes. OQ-450 is resolved only by an already-binding decision/evidence or stays a concrete owner question. P2.2/P2.3, T60/T61, T34 stage 2 and T-AI.4 are not dispatched. ADRs stay Proposed; T40 stays DONE.

**Round cx (2026-10-29, orchestrator): ordered local integration and next-task preparation.**
- **Owner instruction:** merge P1 then T10c into local main, prepare an executor packet for the T32.5 corrections, and thoroughly identify the next task set. No push.
- **Integrated:** P1 `416993fa6a6f9dc821455df5d414cff5833ac7f0` merged as `b173d02217b378256a7aa04a614567c19232a4ce`; then T10c report tip `5b10ed3ecb0f0c6446ca96a4d556f82a7b00aff1` merged as `42b6738a97850784adf1cf95b3b2fcc162fa4916`. Both are normal local merge commits; task branch heads are unchanged. Only REPORTS append conflicts occurred; both histories were retained. The source, fixtures and build configuration equal the reviewed combined tree; main also retains its orchestration tools. State stream 115, PRNG timeline 2, command wire 1; coordinator compatibility 3.
- **Assignment:** T32.5 F1 only, on the existing clean branch at `e232b02d743d623679b1ae92a2a6ae7083288f77`. Four findings: full mask/high-bit corpus evidence; conditional player OR/setter; modular city timing; duplicate OQ-462. See the F1 block under T32. The generated `cache/orch-cx/packet.md` contains the binding block and preflight. Stop after its report/commits; no merge/rebase from main or follow-on task.
- **Integration follow-up:** fresh-cache CTest exposed T10c F5 (N4 launches a WSL binary that another test has not built yet); isolated warm N4 passes (1 case/5 assertions), the same case with only its cache binary absent fails, and the binary was restored. Fresh Release build, no-game CTest (1096 listed, zero failures, two expected skips), oracle and lint pass. The game-file run was interrupted for sustained memory shortage at 1079 passed / 1 N4 failure / 16 unfinished; it is not a green full gate. Its independent setup fix is identified below, not silently repaired or hidden by a warm retry.
- **New set and order:** T32.6 audits missing house-tick inputs/lifecycle and prepares the OQ-450 decision; P2.1 imports the full player mask after F1 review/integration; P2.2 adds only the audited immutable calculation inputs; P2.3 remains blocked until prerequisites and lifecycle/side-effect boundaries are settled. T60 (OQ-270 post-list AI call) and T61 (explicit jitter in the existing join checker) are independent candidates for separate packets. The detailed entries below define evidence, files, acceptance and stop boundaries; none is dispatched by the F1 packet.
- **Why this split:** code inspection finds `Player::flags` already serialized but absent from scenario import, `GoodsInfo` without BauFirst/IdSec, `HouseGroup` with its first-reader metadata intentionally omitted, and `TickKind::House` still a no-op. T32.5 alone does not make live population behavior ready. OQ-450 is a policy question; OQ-451 is an unread load-phase draw order. Neither is resolved by this task planning.
- **Excluded:** T40 is DONE, not reopened; OQ-271/272 and T-AI.4 stay later. ADRs 0002/0003 remain Proposed, so no float-walk implementation or production networking. The world-generator feel/config choices and live viewer checks remain their own owner gates. Review/integration verification is recorded in REPORTS at exact head `42b6738`; no remote verification or push.

**Round cw (2026-10-29, orchestrator): overnight packet review.**
- **Exact state:** main before review `dc265568a7f95ef29f07500f109e409231e06e2b`; clean `codex/T10c-test-memory` at `5b10ed3ecb0f0c6446ca96a4d556f82a7b00aff1`, combined tested head `ef6121a7482de79f9faa3feb070ecd37642d01ad`; unchanged P1 `416993fa6a6f9dc821455df5d414cff5833ac7f0`; clean `task/T32-5-stage-mask` at `e232b02d743d623679b1ae92a2a6ae7083288f77`. Neither task worktree was edited. T40 `862475a` is an ancestor of main; there is no live T40 row or F-fix.
- **T10c accepted:** `1104a7d..5b10ed3` changes only REPORTS and the perf note. Round cv's accepted code/gates therefore stand. The three requested perf-note fixes are present. Saved A3/B3 logs are 193.437 / 199.742 s, with two waits in B3, so that pair is excluded under the binding rule. A2/B2 is the only counted pair: 197.070 / 204.759 s, B/A = 1.039, below 1.050. Accept "no throughput regression measured" under that rule; this is one qualifying pair, not evidence of a speed-up or zero overhead.
- **P1 accepted on the combined head:** checked the payload/import/registry/stream changes, saved GCC/Clang builds and runs, six named mutation failures (including the corrected compilable mutations 3 and 5), restored-source rebuild and corpus result 3283 / 43162 / 43162 / 0. Part A CTest logs confirm 1096 listed, zero failures both ways; without game files two scenario replay tests are explicitly skipped. The orchestrator freshly ran 15 targeted house/group/import/G7 tests, 401 assertions, all passed, plus `sim_stream.py --check` (goldens agree) and `lint_sim.py` (ok). Full corpus, CTest, sanitizers and compiler gates were inspected as saved evidence, not rerun. No remaining P1 acceptance blocker found; results are not attributed to the standalone P1 branch.
- **T32.5 accepted facts:** the house reads the player's property `0x21` / `+0x90`, not city `+0x618`; the player value is restored from `SPIELER/HEAD +0x48` and `+0x5c`; the city mask starts at zero and is built by the unlock pass. The 50-file / 373-head low-bit histogram independently reproduces. S9a/S9b may remain open with next addresses, as the assignment permits.
- **T32.5 corrections required before acceptance (docs only, not a newly dispatched task):**
  1. **High bits are present in shipped files.** `population.md:693` and `state.md:696` say they are clear. Reading both fields and applying AnnoWelt `0x1001d389`–`0x1001d3df` gives 78/373 with nonzero high bits: 17 with `0x28000000`, 1 with `0x29000000`, 60 with `0x69000000`; 295 have none. Only 263 full restored masks equal 1, not 280 (280 counts only `HEAD+0x48`). Correct both docs and the report, and give both histograms. Distinguish the existing `Player::flags` field from its still-unwired scenario import; default 1 is not faithful for all shipped players. P2 may be implemented before P4, but its scenario-mask loading requirement must be decided explicitly before dispatch.
  2. **The player OR/setter is conditional too.** `population.md:678` says the failed comparison skips to the OR. The unsigned branch at `0x100296c2` targets `0x100298fe`, after OR `0x100298ee` and setter call `0x100298fb`. It skips both announcement and player update. State that guard on every summary of the player write; otherwise a later implementation would call a setter with additional side effects when the original does not.
  3. **Catch-up is a modular gate, not `tick >= 2500`.** `population.md:670`'s unconditional first-visit / <=500-tick conclusion is false even assuming `last=0`: tick 65536 has word difference zero and does not pass. Retain `(u16)(tick-last) >= 2500`, keep OQ-461's unknown initial stamp explicit, and do not state an unconditional catch-up latency from the unread initial value.
  4. **Remove the duplicate OQ-462** (`open-questions.md:1975` and `:1982` are identical entries).
- **Evidence:** read-only original-file/branch probe `cache/orch-cw-review-overnight/check_stage_mask.py` and its `stage-mask-evidence.txt`; detailed review appended to REPORTS. No original bytes or disassembly committed. No heavy rerun, merge, rebase, push, ADR approval or next feature assignment. T-AI.4 remains later; OQ-270..272 are not T40 F-fixes.

**Round cv (2026-10-29, orchestrator): T10c F4 Part A review.**
- **Exact state:** clean `codex/T10c-test-memory` at `1104a7d0a9a2422b430afdc80b1579c2835a1eaa` on `94cf3cb`, one concern per commit as round cu asked: `e4482a8` (harness moved into `budget_proc_harness.hpp`), `47ae977` (E1, compat 3), `2fc77df` (E2), `57d1241` (the layer cases, probe modes and hooks), `6cdf628` (perf note), `1104a7d` (REPORTS). P1 unchanged at `416993fa`. The orchestrator changed neither branch and ran no suites; the review reads the diffs and the executor's logs in the worktree's `cache\f4\`.
- **Accepted:**
  - **E1.** `Journal::open` yields the largest hold id of any `A` or `R` line over all budgets (`max_hold_id`). One server-wide counter, seeded at that value + 1 before the first hello (`run_budget_server`), numbers every budget's holds. No replayed id is reissued: every admission is journaled before the client hears of it, and compaction keeps only live ids, all below the seed. `kCompatVersion` is 3 with the reason in `coord_wire.hpp` and the perf note. Cases 1 and 2 fail at `94cf3cb` on `orphans=1` (`cache\f4\failfirst\01-*`, `02-*`) and pass now.
  - **E2.** `session_for` reads `cancel_gen` before it waits for `connect_mu`; after the lock, with no session and a moved generation, it returns `cancelled`. Case 3 fails at `94cf3cb` (no second `RESULT`) and passes now.
  - **E3.** Every F3 and F4 mutant fails a named case on the final head, on MSVC and in WSL; the unmutated runs pass (`cache\f4\mutants\summary-final.txt`; exact replacements in `defs.py`, the driver `run.py`). N2, N3, N4 and N4b each fail a case of their own. Decision 5 is right: the coordinator closes descriptors 3..255 at start (`coord_server.cpp:1551`, since `3450eee`), so the probe moves its leaked descriptor to 256 or above to observe the client spawn's closefrom alone.
  - **E5.** The gate table names the coordinator build and version per gate; acceptance item by item; the two-worktree paragraph; the F3 corrections; the perf-note blank line and heading.
  - **Gates at `1104a7d`:** build exit 0; CTest with game 1096/1096 (912.31 s) and without 1096/1096 (34.16 s); WSL ASan/UBSan budget 49/49 and sim 664/664 with and without game; GCC 13.3 / Clang 18.1.3 warnings clean on 173 files; clang-format 23.1.3 and `lint_sim.py` clean. Coordinator `cee55b5e74257911`, compat 3.
  - Report decisions 1–7, including the relative include `"../src/coord_wire.hpp"` that `tools/check_unix_warnings.sh` needs (decision 7).
- **The executor's question (A6), answered:**
  - The 5 % band is one-sided. It asks whether the final head is slower than F2; a faster run is no regression. "Within 5 %" on the board and in the packet was the orchestrator's wording error, and the executor was right to stop on it as written.
  - The verdict is still open. A1 and B1 each logged three budget waits (`still waiting … for 30 s`, free memory 13.3–14.2 GB at those moments), so their times measure the host's free memory, not the code. The packet's A6 says such a pair is repeated once; that was not done. The only wait-free pair, A2/B2, is +3.9 %, inside the band. One repeat pair decides it, under the rule in the continuation below.
- **Doc gaps, fixed in the continuation:**
  - The test-only hooks `ANNO_TEST_COORD_LEAK_FD` (coordinator) and `ANNO_TEST_PROBE_LEAK_FD` (probe) are missing from the perf note's "Test-only hooks" list, although the code comment says they are documented there.
  - The F3 timing paragraph still ends "the gap is host variance, not a measured effect of F3", which is the unmeasured claim of E4.
  - The F4 A/B paragraph ends in "QUESTION".
- **Open nit, deferred on purpose:** `fopen_cloexec` sits between the Journal's doc comment ("Append-only record of the open promises …") and `class Journal`, so that comment now heads the wrong function. Moving it changes the coordinator's sources and build id, which would repeat every gate for a comment. It is fixed with the next change to `coord_server.cpp`.
- **Decision:** F4 Part A is accepted except the A/B verdict. The F4 continuation below is the executor's first assignment. It changes no source, so Part A's gates at `1104a7d` stand for its head as long as `git diff --stat 1104a7d HEAD` lists only `docs/` files. P1 stays BLOCKED until Part B is accepted; then the orchestrator integrates P1 and then T10c.
- **Owner request (2026-10-29): a longer assignment, so the executor works through the night.** The orchestrator adds **T32.5** (the stage mask, static RE and docs only, below under T32) as the third part of the same packet.
  - Why T32.5: P2, the house tick, is the next critical-path stage after P1 (`docs/plan/lan-endless.md` W6). Its formulas are confirmed (`population.md` §4, §12.2). One input is unknown: the house decays a need, and counts it as the lowest, only when the player's reached-stage mask has the good's `BauFirst` bit (`0x100762c2`–`0x100762f8`). The rebuild has no such mask, and where the original's comes from (file, save, recomputation) is unread. The orchestrator cannot write P2's decisions without it.
  - T32.5 needs no build, game run or Wine, and touches only docs. It branches from main, so it does not depend on the review of P1 or T10c.
  - **Overnight order:** (1) the F4 continuation; (2) F4 Part B; (3) T32.5. If (1) ends in a QUESTION, skip (2) and go to (3). If (2) stops on a blocker, write it and go to (3). Each part has its own REPORTS section.

**Round cu (2026-10-28, orchestrator): T10c F3 review.**
- **Exact state:** clean `codex/T10c-test-memory` at `94cf3cb0a09969339401f7bcd8f4a73e4cb62375`: `ebbc024` (sources), `7ca54de` (tests, probes, F1/F2 test fixes), `94cf3cb` (perf note, `CLAUDE.md`, REPORTS) on `43cd10f`. Stacked on P1 `416993fa` (an ancestor; P1's branch unchanged). Merge-base with main `507e41a`. `git diff --check 43cd10f 94cf3cb` is clean. The orchestrator changed neither branch and did not rerun suites while the owner uses the PC; the findings below are by reading the code at `94cf3cb`.
- **Accepted:**
  - **D1 as implemented.** The hello is decided by `kCompatVersion` (`coord_server.cpp:1107-1111`); the build id is provenance; an exact build only with `ANNO_TEST_BUDGET_EXACT_BUILD=1`. A refused requester retries with backoff inside `ANNO_TEST_ADMIT_WAIT_S` and ends with the infrastructure diagnostic or `cancelled` (`budget.cpp` `session_for`). The client never falls back to a second port (`coord.cpp:790-906`). The step-aside race is safe: an incompatible hello on an idle coordinator ends the poll pass (erase, then `break`) before a later hello is served, and a served client makes the coordinator busy.
  - **D2 as implemented:** CLOEXEC sockets, `accept4`, `pipe2`, closefrom on both spawns; on Windows `WSA_FLAG_NO_HANDLE_INHERIT`, the inherit flag cleared on accepted sockets, `bInheritHandles = FALSE`. The inode-intersection criterion for the meter child is right (the child holds interop sockets of its own).
  - The C1 nit; the F1/F2 test fixes (report items 16 and 18); the `CLAUDE.md` sentence (T10c scope). The combined head's full CTest covers P1's code and is reported as the combined head (round cp).
- **Answer to the executor's question (report item 9): confirmed.** "Drained" in F3 1(c) means idle as implemented: a coordinator with open promises never steps aside, and after its death the successor counts every journaled promise of a live owner as an orphan.
- **E1 — a journaled promise can still be lost across two restarts (F3 1(c), acceptance item 2).** Found by the executor by reading (report, open items) and confirmed by the orchestrator by reading; not yet run.
  - Replay keys a hold by `(budget, hold id)` (`coord_server.cpp:522-550`); every coordinator numbers a budget's holds from 1 (`:482`, `:998`; a budget created at `:1153-1159` starts at 1 too).
  - Sequence: C1 dies with P's hold `(b,1)` open. C2 replays it as an orphan and admits Q's hold as `(b,1)` as well (`A b 1 … Q`). Then Q releases (`R b 1`), or P exits (`R b 1` from `check_orphans`, `:1411`). C2 dies. C3's replay has no `(b,1)`, although P (or Q) still holds its bytes, and C3 over-admits.
  - The F3 crash case passes only because its orphan is released before the successor admits, and nothing restarts a second time.
- **E2 — `cancel()` is lost for a second waiter (F3 1(b) "waits cancellably").** By reading. `Budget::session_for` reads `cancel_gen` after it takes `connect_mu`. With two threads waiting on one unconnected budget, `cancel()` ends the thread that holds the mutex. The other then takes the mutex, reads the new generation and waits until `ANNO_TEST_ADMIT_WAIT_S` (1800 s). No production code calls `cancel()` (tests and the probe only), but the contract says it ends the budget's waits.
- **E3 — the mutation gate is not met (F3 step 5).** The table predates the last test edits and was not re-run. N2 (server ignores the exact opt-in), N3 (no `SOCK_CLOEXEC`), N4 (the meter spawn closes nothing) and N4b (the client spawn closes nothing) survive, each masked by a second layer. Defence in depth stays, but each layer needs its own observation, or it can rot unseen.
- **E4 — the throughput statement is not evidence.** The 4-worker legacy-river case took 205.4 s without budget waits, against F2's 186.4 s (+10 %). "Host variance" was not measured. An interleaved A/B of the F2 code and the final head on one host state decides it.
- **E5 — report gaps and errors.**
  - Missing: F3 step 5's "record the coordinator build and compatibility version from each gate log"; D1/D2 acceptance item by item; step 6's "how two worktrees of different builds now share one budget".
  - Wrong: report item 3 says `silent` never accepts (item 13 changed it to accept and hold); the fail-first rows 03–06 are mislabelled (04 is the drain handover, 05 the crash handover, 06 the D2 meter case).
  - `docs/perf/README.md`: no blank line before `## T10c F3`, and its timing paragraph has no heading.
- **Process:** the commits are by area, not by concern. "Interactive staging is unsupported" is not a reason: commit each concern when it is done. F3's three commits stay as they are (integration is squash-style); F4 commits one concern at a time.
- **Decision (orchestrator, with reasoning):**
  - **`kCompatVersion` goes to 3 with the E1 fix.** The bump rule written in F3 covers "recovery semantics an older peer would not reproduce", and an unfixed successor reproduces exactly the loss. It also makes stale F2/F3 binaries refused by version, with the infrastructure diagnostic, instead of by accident.
  - F3 is accepted for D1 and D2 and not accepted overall. T10c F4 below is the only executor assignment. P1 stays BLOCKED until F4 Part B is accepted; then the orchestrator integrates P1 and then T10c. No merge, push, ADR approval or next feature task.

**Round cs (2026-10-28, orchestrator): T10c F2 review.**
- **Exact state:** clean `codex/T10c-test-memory` at `43cd10f086eb6907373608c054c7598385393605`, last code commit `c95f819`, on `af8aa01` (12 commits). P1 is unchanged at `416993fa`. `git diff --check af8aa01 43cd10f` is clean. The `CLAUDE.md` edit is within T10c's scope ("stale memory instructions"). Rewording the unpushed `2318078` once, and the report-path fix `43cd10f`, are recorded in the report. The orchestrator changed neither branch.
- **Evidence checked (executor logs in the worktree's `cache\f2\`):**
  - With game files: 1080/1080, and the log names `coordinator build=49fb6ac65eb9178a … protocol=2`. Without game files: 1080/1080.
  - GCC 13 and Clang 18 are clean (`warnings2.txt`).
  - Nine mutants are each caught, and the unmutated cases pass. M2 was caught only after its case was strengthened (`M2-temporary-b`). M1 was "caught" by hanging until it was killed after 60 s.
  - The C4 calibration samples private bytes every 5 ms, agreeing with `PeakPagefileUsage` to within 20 KB. The worst item uses 0.763 of its reservation (Single_01: 28.08 B/px measured against 36 reserved). Throughput: one worker 266.4 s, four budgeted workers 186.4 s.
  - The owner confirmed (2026-10-28) that the roughly 11.5 GB external pressure during one measurement run (`FC27`) was the owner's own application. The fail-closed wait without admission was therefore the intended behaviour.
  - The orchestrator did not rerun the full suites while the owner was using the PC.
- **Independent probes** (ignored `E:\anno1503\cache\orch-cs\`):
  - The round-cr probe and the new `probe_compat.cpp` were rebuilt from the F2 sources under WSL GCC with `-fsanitize=address,undefined`, as two builds (`bx/C5…`, `bx/B0…`, via `build2.sh`).
  - Each case used its own port (49641–49646) and journal under `/tmp`, and the driver stopped only its own processes. No sanitizer diagnostic appeared.
  - Results are in `review-results*.txt`.
- **Verified fixed:**
  - **C1:** a waiter whose owner still holds 80 of 100 waits and ends with `wait deadline`, not `exceeds capacity`. The stale reading after a release with retained bytes is not admitted (`available=0`).
  - **C2:** a blocked waiter gets `coordinator lost` at once when its coordinator dies.
  - **C3:** a replacement reports `orphans=1` from the journal. A new 80-byte request waits while the old 80 is open, and is admitted the moment the old holder exits (`run_fence.py`).
  - **C4:** accepted as calibrated, with a reasoned margin.
- **D1 — the round-cr compatibility clarification is not implemented.**
  - `coord.cpp:815-823` (`hello_once`) refuses any coordinator whose build id, a SHA-256 over the coordinator sources, differs from its own. `coord_server.cpp:1096-1112` rejects such a client; an idle server exits, and a busy one stays.
  - **Probe E:** a build-B0 coordinator with a B0 holder, and a build-C5 client with the same protocol 2. The client throws at once: "… runs build b0b0b0b0b0b0b0b0 …; it still serves other clients, so it stays".
  - That fails an ordinary test run on a source-hash mismatch alone, which `e4f1685` (F2 step 3, "Mid-task clarification") rules out. Once integrated, the orchestrator's runs on main and an executor's runs on a branch that edits `src/test_support` cannot overlap.
  - The three regressions the clarification requires are absent: two compatible builds sharing reservations, an incompatible or exact-build requester while the coordinator is busy, and safe handover. The case "C3: a coordinator from another build is not used" asserts the opposite behaviour. The report does not mention the clarification.
- **D2 — the coordinator's listening socket leaks into its meter child.**
  - The WSL coordinator spawns `powershell.exe` as its Windows meter (`coord_server.cpp:193-209`, `posix_spawn` with file actions only for stdin/stdout). `listen_on` creates the socket without `SOCK_CLOEXEC` (`coord_server.cpp:1465`).
  - After the coordinator was killed and reaped, `ss -tlnp` showed `LISTEN 127.0.0.1:49645 users:(("powershell.exe",pid=296,fd=3))` for about a second (`run_holder.py`).
  - In that window a replacement fails with `BIND-FAILED` (exit 3). A client connects to the orphaned listener and dies with an uncaught "… closed the connection or speaks another protocol" (`run_d.py`, `review-results-d.txt`). A replacement bound about 1.1 s later (`run_bind.py`).
  - The F2 replacement tests run on ports with an injected meter, so they do not exercise this real path.
- **Owner instruction (2026-10-28):** prompts must be more elaborate and precise. When in doubt, the executor checks against the original game files (or the disassembly, or a measurement for host/tool questions) rather than guessing, and writes its decisions and reasoning down. This is now §1 item 12 for every task.
- **Decision:** F2 is accepted for C1–C4 and is not accepted overall. T10c F3 below is the only executor assignment. P1 stays BLOCKED; after T10c is accepted, the orchestrator integrates P1 and then T10c. No merge, push, ADR approval or next feature task.

**Round cr (2026-10-28, orchestrator): T10c F1 review.**
- **Exact state:** clean `codex/T10c-test-memory` at `af8aa01fae68daf4d2e6afb0a6301ba1085bf7fc`; latest code `f2ded2141905d761245c0703a7be614c8cafc251`, followed by the report. Clean P1 remains `416993fa6a6f9dc821455df5d414cff5833ac7f0`. Merge-base with review main `fa3933e` is `507e41a`; no merge from main. `git diff --check 416993fa af8aa01` is clean. The orchestrator changed neither task branch and ran no heavy corpus.
- **Evidence checked:** saved final CTest logs confirm with-game 1061 passed / 6 failed of 1067, all six `exceeds capacity`; without-game 1067/1067 with the two named game-dependent replay cases skipped. Saved WSL logs confirm budget 20/20, sim without game 664/664, and the two synthetic render cases (69904 omitted-ground / 57 cropped-mask assertions). The earlier with-game ASan sim run had two coordinator-staging failures; the later scenario-corpus rerun passes 3 cases / 674 assertions. The full heavy render corpus was not run under ASan. GCC/Clang warnings are clean. These are inspected executor logs, not fresh full-suite runs by this review.
- **C1 — stale admission readings can both over-admit and falsely refuse.** `coord.cpp:749-757,956-985` stores free memory per waiting request and pumps that request on another client's release using the old reading. Small real-client reproducer: free 100, holder reserves 80, waiter queues 60; holder allocates/retains 80 (free now 20), then releases its reservation. The waiter is granted 60 with `available=100`, although only 20 is free. Output: `free=20 retained=80 new=60 admitted=1 available=100 reason=admitted`. Conversely, an active holder's residency is not refreshed merely because another client requests memory: with 80 held and free 20, a 60-byte blocking request immediately returns `exceeds capacity`; after the owner finishes, the same request succeeds. `admit_detail.hpp:48-60` treats current free plus reported residency as a permanent capacity bound. This is not a safe distinction between temporary pressure and an impossible item. The Windows and WSL readers also measure different domains (`GlobalMemoryStatusEx` versus guest `/proc/meminfo`); sharing reservation records alone does not establish one authoritative host capacity.
- **C2 — coordinator death hangs a waiter.** In `coord.cpp:1381-1390`, `transact` holds `session.mu` and calls `forget_pending`, which locks the same non-recursive mutex at line 1294. A real holder and blocked waiter were connected to an isolated coordinator; killing only that coordinator left the waiter running with no result after the bounded observation. The same nested-lock pattern exists in the nonblocking timeout branch. Holder-death tests do not cover coordinator death.
- **C3 — coordinator replacement forgets live promises.** A replacement starts with `Server server` empty (`coord.cpp:1610`), while old `Reservation` objects remain usable and their work is not fenced by `reader_main` marking the session dead. After the isolated coordinator was killed and its port became reusable, the new server granted a fresh 80-byte hold against free 100 while the original process still reported `STILL_HELD 1 80`: 160 promised against 100. The probe had not allocated those promised bytes, so an actual free-memory query alone cannot recover the lost promises. Safe refusal until old work is accounted for/drained is acceptable; transparent crash recovery is not required. Do not treat a reconnect as an empty budget.
- **C4 — completion and throughput remain unproved on F1.** The reported 530382848-byte setup plus 46 bytes/pixel is more explicit than the old estimate, but one-worker legacy-river was refused before rendering. Its 297.646-second baseline has no completed F1 successor. The six current failures include four 1-GiB requests as well as the two whole-scene digest cases. Do not label all six as unavoidable RAM shortage, infer an OOM, or reuse the pre-F1 17% speedup as a measurement of this head. Correct C1-C3 first, then measure the actual corpus admission boundary, including shared catalog lifetime, scene rebuilds when final bounds grow, and pre-admission allocations. If a large item still cannot fit safely, reduce its bounded live storage or report that measured blocker; do not lower headroom or estimates merely to turn the gate green.
- **Review evidence:** ignored `E:\anno1503\cache\orch-cr\probe.cpp`, `run_review.py`, `review-results-final.txt` and source copies from `af8aa01`. Built with WSL GCC `-fsanitize=address,undefined`; only the coordinator ports in the copied source were changed to isolated 49631/49632. Injected 100-byte counters, no host-memory exhaustion. The driver terminated only its own clients/server. C1 is deterministic via an on-wait barrier; C2 has a bounded observation and the matching nested-lock source path; C3 observes both live holds after replacement. No sanitizer diagnostic in this probe run.
- **Decision:** F1 is not accepted. T10c F2 below is the only new executor assignment; P1 stays BLOCKED on combined validation and review. Preserve the useful buffer reductions, equality tests, sanitizer route, version-115 goldens and resource-budgeted overlap. No merge/push, ADR approval or next feature task.

**Round cq (2026-10-28, orchestrator): T10c review.**
- **Exact state:** clean `codex/T10c-test-memory` at `d8c3461b9738b0955e191abdbca24ce59c740c84`, stacked on unchanged P1 `416993fa`. Merge-base with main `4959437` is `507e41a`; no merge from main. `git diff --check 416993fa d8c3461` is clean.
- **Evidence checked:** baseline/after sampler metadata and saved CTest logs; 16 budget cases / 74 assertions rerun successfully on Windows. Full CTest evidence belongs to `677a4df` (1049 non-heavy + 10 heavy, with and without game files), with warning/test-only follow-ups at `7311a12` as the report states. The legacy benchmark records 297.646 s to 246.1 s, about 17.3% less wall time, on the same 47 assertions. T10c changes no sim goldens or scenario expectations. The P1 test-only GCC fix is acceptable within gate completion.
- **R1, incorrect credit for retained process memory:** `budget.cpp:124` observes private growth from a baseline that survives released reservations and credits that growth to new holds. Reproducer: capacity/free 100, old hold 60; private grows to 60 and free falls to 40; release the hold while those bytes remain cached; two fresh unallocated requests of 40 are both admitted. Output: `free=40 retained_old=60 admitted_new=80 first=1 second=1 second_pending=0 second_resident=40`. The new work can consume twice the remaining memory. Windows private commit also must not be equated to physical residency without a matching accounting model.
- **R2, Windows/WSL do not share process identity:** `pid_alive` / `reclaim_image` (`budget.cpp:302,432`) interpret every saved PID in the caller's local OS namespace. A raw-format fixture owned by the live Windows probe PID 29988 with 80 reserved out of 100 was reclaimed by the Linux client; it admitted another 80 and cleared the Windows slot. The owner remained alive. Sharing the filename is insufficient; host availability, owner identity, exclusion and wakeups must all be coordinated across the supported clients.
- **R3, owner death can leave waiters asleep forever:** real separate Linux clients on an isolated file: holder admitted 80, waiter for 60 logged WAITING, holder was killed. Waiter did not recover in the bounded observation and was terminated by the probe driver. `wait_signal` (`budget.cpp:664`) waits indefinitely; reclaim runs only when another operation wakes it. External memory recovery and cancellation also need a recheck path, without restoring forced admission.
- **R4, estimate does not cover the same workload it measures:** `render_corpus_test.cpp:149-169` reserves from pre-scene bounds and a flat 1600x900 region before building each scene. The report acknowledges 152826560 reserved pixels versus 155523200 rendered on Single_01. The 43-byte rate was measured after substantial loading in a fresh viewer; fixed scene/setup costs and the region-only path were not calibrated to the admission boundary. Reduced peaks in the successful run do not establish the bound for every admitted item.
- **R5, validation remains incomplete:** ASan/UBSan was skipped solely for missing Ninja. WSL GCC and make are available; the orchestrator compiled and ran the small review probes with `-fsanitize=address,undefined` without Ninja. That proves an available route, not completion of project sanitizer gates. Add targeted synthetic coverage for the new probe-only/cropped-mask behavior, and run applicable project gates on the corrected head. No heavy suite was rerun during this review.
- **Evidence location:** ignored `E:\anno1503\cache\orch-cq\review-results.txt`, `budget_probe.cpp`, `process_probes.py`, `foreign_pid_probe.py`, and exact test_support source copies. Probes allocate small counters only; the foreign-PID test uses a real live Windows PID in the exact file format, rather than the Windows C++ client. Add genuine Windows/WSL client regressions in F1. The owner-death driver terminates only its own probe processes.
- **Report correction:** byte totals 8044425216 / 5226586112 / 6116012032 are approximately 7.49 / 4.87 / 5.70 GiB (8.04 / 5.23 / 6.12 decimal GB). Keep source-head/dirty-tree provenance explicit for measurements made before their source commit. Add the after-results to the performance note, not only REPORTS.
- **Decision:** T10c F1 below is the only new executor assignment. P1 remains BLOCKED on corrected combined validation and review. No merge, push, ADR approval or next feature task.

**Round cp (2026-10-28, owner decision): memory-aware parallel testing.**
- **Policy:** the owner explicitly drops the one-heavy-job-at-a-time restriction, including the exclusive heavy-job wrapper and equivalent historical board instructions. Parallel builds/tests and subagent jobs are allowed when their combined CPU and memory budgets fit. This supersedes round cn's fixed eight-worker recipe and its blanket no-restart instruction for the controlled T10c work below. It does not authorize unrestricted fan-out, disabling memory protection, changing the pagefile, or closing the owner's applications. Preserve locks needed for actual shared resources or build-directory correctness.
- **P1 status:** branch `task/P1-house-payload`, clean head `416993fa6a6f9dc821455df5d414cff5833ac7f0`, base `507e41a`. Implementation `69e0d858`, formatting-only `15bcfa3c`, report `416993fa`. Reported/recorded: six caught mutants, version-115 goldens, lint and formatting clean, with-game non-heavy 1032/1032. Heavy test 228 passed; test 229 started, and the executor stopped at 1.2 GB free RAM. Without-game and WSL gates remain outstanding. P1 is not accepted or merged.
- **Diagnosis:** render/test-support code is unchanged by P1. Whole-map CPU renders have multi-GiB temporary buffers; the harness still assumes 1 GiB per worker, does not reserve memory, and permits progress after a low-memory wait expires. T10c already records about 35 GB commit at 16 workers. Two free-RAM samples do not establish each process's share or a leak. Round cn's attribution to desktop apps and claim that the cap worked are not established for this stopped run.
- **Next assignment:** T10c below, explicitly stacked on the committed P1 head in its own branch/worktree. Keep P1's branch unchanged. This permits testing P1 plus the memory fix without merging or rebasing either task with main. Report both the P1 base and the combined tested head; results on the combined head must not be attributed to unchanged P1 alone.
- **Review boundary:** commit the memory fixes separately by concern, append REPORTS, then stop. The orchestrator reviews the combined evidence and integration order. No TASKS/AGENTS edits by the executor, no merge/push, no ADR approval, P2, T34 stage 2 or T-AI.4. Existing unrelated lanes are unchanged.

**Archive (round ct, 2026-10-28):** rounds co and older, superseded orders, finished task entries (`DONE`/`MERGED` with no open part) and the §4 review log are in [`HISTORY.md`](HISTORY.md), moved verbatim. A reference such as "round bn", "§4" or a finished task id resolves there.

## 1. Protocol for the executor

1. **Read first, every session:** `CLAUDE.md`, this file, `docs/PLAN.md` §0.1–0.3 and §7, and the spec named by the task. The rules in `CLAUDE.md` apply unchanged (evidence only, determinism, no hardcoded limits, tests before refactors, `game/` read-only, nothing from `game/` in git). **Since round ct:** when the task comes with a packet (item 13), the packet replaces reading this file. Claude Code loads `CLAUDE.md` by itself; `AGENTS.md` is the same text for Codex, so it is not read again (never edit or commit it).
2. **Pick the first task whose status is `READY`** and whose dependencies are `DONE`; tasks marked `PARALLEL-OK` may run at the same time as another task in a separate git worktree (`git worktree add ../anno1503-<id> -b task/<id>-<slug> main`). Never run two tasks that touch the same files in parallel.
3. **One branch per task:** `task/<id>-<slug>` from `main`. Small conventional commits (`feat(render): …`, `fix(sim): …`, `docs: …`, `test(...)`), one concern each, each ending with the attribution line the harness gives you. **Do not merge into `main` and do not push** unless the task says so. The orchestrator merges after review.
4. **Gates before you report a task as finished** (all of them; the owner's PC has 16 threads, use them):
   - `cmake --build --preset msvc` warning-free (warnings are errors).
   - `$env:ANNO1503_GAME_DIR="$PWD\game\app"; $env:ANNO_TEST_THREADS=16; ctest --preset msvc -j 16` green **and** the same without `ANNO1503_GAME_DIR` (skips must be clean skips). Round cp: 16 is a concurrency ceiling, not permission to allocate 16 full-map renders; use measured per-item/process budgets and configurable host headroom. The full test set and clean-skip requirement remain.
   - clang-format 23.1.3 clean: `python -m pip install --target <scratch>/cfpkg clang-format==23.1.3` then `find src -name '*.cpp' -o -name '*.hpp' | xargs <scratch>/cfpkg/bin/clang-format.exe --dry-run --Werror` (a venv fails with Python 3.14 in Git Bash).
   - For `sim`/`formats`/`test_support` changes: the GCC 13.3 `-O3 -Werror` check in WSL (`Ubuntu`) with the CI's exact flags (`-O3 -DNDEBUG -std=c++20 -Wall -Wextra -Wpedantic -Wconversion -Wsign-conversion -Wshadow -Wnon-virtual-dtor -Wold-style-cast -Wcast-align -Wnull-dereference -ffp-contract=off -Werror`, doctest from `build/msvc/_deps/doctest-src`, run with and without `ANNO1503_GAME_DIR`). For `render`/`app` changes it is enough that the MSVC build is clean and you avoided the known GCC traps (`std::swap`/copy of structs holding `std::optional<std::vector<…>>` at -O3; sign conversions).
   - `tools/agree.py` where an `anno-dump` command changed.
   - A performance task also needs its `bench_frame` / `bench_sim` numbers in `docs/perf/README.md` (p50/p95/p99/max, hardware, commit).
5. **Subagents (owner, 2026-10-26: allowed again, where necessary and helpful; from the tasks after the one in progress at that time).**
   - **Use them where they help:**
     - independent reads in parallel, for example several functions or modules of an RE task, or the corpus files of a measurement;
     - an independent `PARALLEL-OK` task in its own worktree;
     - a read-only reviewer pass on a large or risky change before you report.
   - **Rules that stay:**
     - The task is yours: you brief each subagent with the task's steps, files and the no-guessing rule; you check what it brings back against the files before it enters a commit; and the report names each subagent and what it did.
     - Subagents never merge, push or edit `E:\anno1503`, and never take a task the board has not given you.
     - **Budget combined memory and CPU use across jobs and internal workers** (round cp). Subagents share that budget; no blanket exclusive heavy-job lock. Isolate build directories and logs, and retain locks for actual shared resources.
     - The orchestrator's independent review stays.
     - You still run the gates and your own mutation checks (flip a constant or a branch, the named test must fail; list them in the report) before reporting.
   - *Earlier rules, for the record:* two read-only reviewers per task (up to round r); no reviewer subagents (2026-10-23 s); no subagents at all (round aa).
6. **Report** by appending a section to `REPORTS.md` in the format given there: what was built, evidence (test names, numbers, PNG paths under `C:\Users\altin\Pictures\anno-review\<id>\`), what the reviewers found and what you changed, what is left, open questions. Then stop that task; **do not start the next step of a task marked `OWNER-CHECK` until this file says the owner has looked**.
7. **Decisions you must not make:** anything that changes the game's look or feel (defaults of render styles, river tuning, rules options, limits) is the owner's (`docs/open-questions.md`, "Decisions for the owner"). Implement the faithful or stated default behind an option and list the question in your report. Obvious bugs you just fix.
8. **If blocked** (missing information, a gate that cannot pass for a reason outside the task, a conflict with `main`): write a `BLOCKED` entry in `REPORTS.md` with what you tried and move to the next `READY` task.
9. **Linux and macOS warnings are part of every gate, not only for sim/formats.** `cmake/AnnoWarnings.cmake` turns `-Wsign-conversion`, `-Wnull-dereference`, `-Wconversion`, `-Wshadow`, `-Wold-style-cast` into errors on GCC and Clang; MSVC does not report most of them. Before a report says "warning-free", the executor runs the GCC 13 `-O3 -Werror` build in WSL over every changed library and its tests (render and app included) and a Clang build of the same; the orchestrator does not merge a branch whose CI run on the branch (push it, open a PR) is not green. Learned 2026-10-21: T1c and T1e were merged with `main` red for a day. **GPU tests skip in CI** (until T13): a render report states which GPU tests ran locally and on which adapter(s); a GPU change without that line is not reviewed.
10. **Run what you built and look at it yourself before you report (owner instruction 2026-10-21 o).** Tests are not enough: the owner found the pixelated far zoom of T3 stage 3 in the first minute of looking. For every change that affects what is drawn: run the viewer or `--png` over the views the change touches (every zoom level it affects, both water styles, the ten `0200Single` scenarios plus the canyon island `D_256x256_0045_5B.scp` where the change is not local to one place, 1280x800 and 2560x1440, the RTX 4090 and the Radeon iGPU where the GPU path changed), open the PNGs (the Read tool shows images) and look for pixelation, doubled rows, seams, holes, pop-in, wrong colours or depth order, and read the viewer's frame log for hitches. Where the original can show the same view (T15's harness once it exists), compare against it. For simulation changes, run the original's fixtures once T15 stage 1 has them. Say in the report what you ran and what you saw; the owner's look comes after yours.

11. **Sub-tasks, one PR each (since round aa).** An open task is split on this board into numbered sub-tasks (`T30b.1`, `T30b.2`, …). Take the first sub-task whose status is `READY` and whose predecessors are `DONE` or `REVIEW`; do only that sub-task; open its PR; write its report section; then stop and take the next one. Never two sub-tasks at once; subagents may help inside one (item 5). Each sub-task's steps are ticked in order; a step's result is written down before the next step starts (a table or a list in the report, not from memory at the end). **Thoroughness over speed:** a sub-task has no time box; re-read a disassembly twice before tagging a claim D; a number is re-computed before it is reported; a claim about the original carries module + address, and the report says which instructions were read (address range) — never the disassembly text itself, never bytes of the original. If a step does not hold, write what was found and the question into REPORTS.md and stop that step (item 7 below). Before calling a stack slot "beyond the arguments" or a value "untraceable", count the pushes that are live at that instruction and read the function's only caller (round ae: `[esp + 0x14]` in `0x1000eb40` was its second argument, the world). **This-adjustment rule (round an):** every class of this engine has several vtables (interfaces at `+0`, `+4`, `+8`, class Y also at `+0x2048`); a method's `this` is the address of the vtable slot it was called through, so `[this + k]` in a method of the `+4` interface is `[Y + 4 + k]`. Before you compare a field of one method with a field of another, write both as absolute `Y + n` (first row of every function table: "this = Y + ?, because slot … of vtable … at `[Y + ?]`", or "called with `lea ecx, [reg + 4]` where reg = …"). Docs-only sub-tasks need no build; a sub-task that adds a tool under `tools/` runs it once and pastes the command and its first lines of output (counts and names only) into the report. (Two items of this list carry the number 7; "item 7" in the task sections means the steps rule below.)

7. **Steps, not judgement calls (since round u; the executor is a simpler model).** A task section lists numbered steps with the files, the functions and the commands. Do the steps in order; do not add scope, do not refactor what a step does not name, do not change a default the owner has to decide (`rules` options, OQ decisions). **Allowed without asking (owner, 2026-10-24 w), when listed in the report:** tests for the code the step changed, and fixes of sentences (docs, comments) that would otherwise contradict that code. Anything else is a question. When a step is unclear, when a file or function it names does not exist, or when a measurement contradicts the step's premise: stop that step, write the question with what you found into REPORTS.md under the task, and continue with the next step that does not depend on it. Report numbers as measured, including the ones that miss the acceptance. Every claim about the original carries module + address, every mapping in code cites its address in a comment. The acceptance list is ticked item by item in REPORTS.md with the evidence (the test name, the command's output line).

12. **Check, do not guess; write down decisions and reasoning (owner instruction, 2026-10-28, round cs).** It applies to every task.
    - **When in doubt, check the evidence before you act.** Never fill a gap with a plausible value.
      - **The game's data, formats or behaviour:** read the original files in `E:\anno1503\game\app` (read-only; nothing from them into git) or the disassembly (`tools/disasm.py`, module + address).
      - **The project's own code, tools or the host:** read the code, or run a bounded measurement.
    - **If the evidence does not settle it,** stop that step and write the question into REPORTS (item 7), or add an OQ in the task's range.
    - **Every REPORTS section has a "Decisions and reasoning" list.** Each choice the steps left to you gets one entry:
      - what you decided;
      - the alternatives;
      - the evidence (file, address, command output);
      - why.

      This covers a name, an interface shape, a bound, a test design, and anything you checked against the original files and what it showed.
    - Where a decision would change the game's look or feel, it is the owner's (item 7), not yours.

13. **Task packets and the preflight (round ct, 2026-10-28; fewer start calls).** Every new executor assignment comes as one packet file under `E:\anno1503\cache\orch-<round>\`. The orchestrator generates it with `tools/orch/packet.py`, which writes:
    - the orchestrator's detailed instructions;
    - the exact preflight command;
    - the binding board blocks, cut verbatim from this file at one commit, between `<!-- board: … -->` markers.

    **The start takes two calls:**
    1. Run the packet's preflight command (`tools/orch/preflight.py`). It checks main's head (that commit or a descendant), the allowed untracked files, that the board has not changed since the packet, and each named worktree's head, branch and cleanliness, and prints one PASS/FAIL table. Every row must be PASS; otherwise stop with a QUESTION that contains the table.
    2. Read the whole packet. Do not search this file again for the blocks the packet contains.

    **Afterwards:**
    - Open other files only when a step needs them, and batch independent reads into one parallel turn.
    - If the preflight reports that the board changed after the packet, re-read only the changed blocks on main.
    - The excerpts in the packet are the binding text; where the packet's instructions and the excerpts differ, the excerpts win.

    **History** (rounds co and older, superseded orders, finished task entries, the old §4 review log) is in [`HISTORY.md`](HISTORY.md), moved verbatim. Read it only when a step names it.

## 2. Current status (2026-10-29 cz, two independent executor lanes)

- **Local Git:** T57 (`7f9e605`), T52 F7 (`8f30605`), T59 (`1974d22`) and T58 (`723029c`, with the open point `88070b1`) are integrated within their documented scope. Nothing pushed or remotely checked; pre-existing untracked `AGENTS.md` left alone.
- **Versions on main:** state stream 115, PRNG timeline 2, command wire 1; test coordinator compatibility 3.
- **Implemented:** C++ formats/assets/viewer and deterministic simulation through S14, scenario cities and production, market/workshop transport, decoded walk search and per-tile records including named-tile removal. The renderer still consumes scenario data rather than a live simulation game loop.
- **LAN groundwork:** T36 setup, T37 generator spec, T38/T40 AI inventory and tick-entry RE, T39/T41 network ADR and budgets. No C++ networking, generator, AI behavior or playable LAN loop yet. Population implementation, float walking, ships/trade, combat, fire and win/loss remain unfinished.
- **Integrated (round cx):** P1 as `b173d02`, then T10c as `42b6738`, after the round-cw acceptance. House payload/group data/scenario residents and stocks are implemented; the House tick still changes nothing. T32.5 F1 is accepted at `1fc598eb` and awaits integration. The next packet assigns T10c F5 and independent T32.6. T40 is DONE, merged as `1fdc0ff`; OQ-240 is answered and OQ-270..272 are subsequent research.

  ADRs stay Proposed; T34 stage 2 and T-AI.4 stay later. P2 is split below into prerequisite tasks and a blocked live-update task; the first executor keeps T10c F5 then independent T32.6, and the second has only T60 in parallel.
- **Host/gates:** Windows/MSVC, GCC/Clang and sanitizers in WSL. Canonical repo `E:\anno1503`, worktrees `E:\anno1503-worktrees`, read-only originals `E:\anno1503\game\app`. Memory-budgeted concurrency under round cp; round ba's local workflow continues. See round br for actual combined checks, rather than treating older branch reports as new runs.

## 3. Task queue

Status values: `READY`, `IN PROGRESS` (set by the orchestrator when a report says so), `REVIEW` (reported, orchestrator reviewing), `OWNER-CHECK` (waits for the owner's look), `DONE`, `BLOCKED`, `LATER`.

### The LAN endless lane (`docs/plan/lan-endless.md`, owner decisions D1–D11 of 2026-10-26; round bn)

**LAN MVP (owner decision 2026-10-29): one fixed map scaled for five players, no computer players.** Plan: `docs/plan/lan-mvp.md`. Effect on this queue, as planning only: no running packet changes by this note.
- **Stays on the critical path:** T32.6 → P2.1 → P2.2 → P2.3 → P3–P5 (population), then ships and settling (W6.1).
- **After the MVP:** the generator (W2; `world-scaling.md` stays the spec), the original AI's port and T-AI.4 (W7), the pirates (W8). T60 is integrated as accepted, then the AI RE lane is parked.
- **New, `PLANNED`, released one packet at a time:** MAP (the fixed map, `lan-mvp.md` §3), W4 (the game shell), ADR 0004 and W5 (menus, HUD), W3 (the C++ `net` library, after the owner approves ADR 0003), the docs-only RE of ships and settling.
- **Open owner questions:** `lan-mvp.md` §8.

#### MAP.1 — The fixed map: format, loader, first layout and the developer start · `DONE` (round dl; merge 6db51eb, accepted at `1665f60`; the lan5 layout rejected by the owner → MAP.2) · was `ASSIGNED — lane 3 part A` (round dk) · new executor session · branch `task/MAP-1-fixed-map` from main at the round-dk board commit · worktree `E:\anno1503-worktrees\anno1503-MAP-1` · new OQs OQ-490..499
- **Why:** owner decision M1 (`docs/plan/lan-mvp.md` §1, §3): one fixed map, larger than the original's, scaled for five players, replaces the generator for the MVP. MVP-A needs it with the developer start (a warehouse per start region, `lan-endless.md` checkpoint A).
- **Facts this task builds on (orchestrator's read, 2026-10-29; re-check each before relying on it):**
  - `.sze` → `World`: `sim_load::load_scenario_with_links` (`src/sim_load/include/anno/sim_load/scenario_load.hpp:47`) = `load_scenario` (`scenario_formats.cpp:1188`) + `resolve_scenario_links` (1615) + `place_island_links` (1644); then `World::create` (`world.hpp:142`, `scenario.cpp:282`). `island_from_insel` (`scenario_formats.cpp:1013`) is file-local. `place_island_links` with a hand-built `LoadedScenario` makes no draws but overwrites the island climate with `IslandLinkSpec::climate` (default 0).
  - `WorldSettings` come from the SCENE HEAD only (`read_head`, `scenario_formats.cpp:942`); bound `kWorldEdgeBound` (`config.hpp:87`); island rectangles must be inside the world and must not overlap (`validate_island_rects`, `island.cpp:650`); unaligned x0/y0 only warn (`UnalignedIsland`).
  - Raw materials and ores reach no state today for any scenario (`LinkRawMaterials`, `LinkOresUnplaced` warnings; `IslandSpec::deposits` is never filled from a `.sze`). That gap is not this task's.
  - No code places a developer-start warehouse. A city with a building at load goes through `IslandSpec::cities` → `CitySpec` + `BuildingSpec` → `build_island` → `make_city` + `create_building` (`scenario.cpp:181`, `create.cpp:127/139`). The Kontor is PRODTYPE 35, original id 1302 (`synthetic.hpp:126`). A load-time building is refused only for `OutOfIsland`, `Occupied` or `SerialExhausted` (`scenario.cpp:163`): no coast check, because the placement stand-in does not read the type's coast flag (`placement.hpp:36-42`, OQ-052). A player can build only on tiles their city owns (`footprint_owner_rule`, `placement.hpp:26-34`: an all-neutral footprint is `TileNotOwned`), tiles get owners only from `IslandSpec::owner_runs`, and nothing in the sim derives territory from a Kontor. So the developer start must also give the start island's tiles to the player's city.
  - The registry for original data is built twice today, file-locally: `scenario_registry()` in `src/tools/sim_replay/main.cpp:323` and `original_registry` in `src/sim/tests/scenario_test.cpp:155`. Human players have kind 8, volk 1 (`scenario_formats.cpp:1314`); player 31 is the neutral party in every original scenario (`docs/formats/sze.md`).
- **Files:** a new reader `src/formats/{include/anno/formats,src}/fixed_map.*` (our own text format; `formats` rules: fixed-width integers, `Result`, no exceptions); `src/sim_load/` (the shared registry loader and `load_fixed_map`); `src/tools/sim_replay/main.cpp` and `src/sim/tests/scenario_test.cpp` only to use the shared registry loader; `src/tools/anno_dump/` (a `fixed-map` command); `tools/fixed_map.py` (independent reference: summary, validation, preview PNG) and `tools/agree.py`; `fixtures/synthetic/fixed_map/` (synthetic layouts and templates, our own bytes); the first layout under a new `data/maps/` (our own data, names templates, copies nothing); `docs/sim/fixed-map.md` (new spec); `docs/plan/lan-mvp.md` §3 status; `open-questions.md` OQ-490..499; REPORTS. No sim rules, goldens of existing scenarios, render or app.
- **Steps, in order; record each result before the next:**
  1. **Check the facts above** against the code at your base. Any that does not hold: QUESTION before building on it.
  2. **One registry loader.** Move the original-data registry build into `sim_load` (one function taking the game folder; same inputs: `Anlagen.dat`, `FigurenHeader.dat`, the AnnoWelt walk tables, `Data/BGruppen.dat`). Switch `sim_replay` and `scenario_test` to it with no behaviour change: the existing scenario corpus results and goldens stay byte-identical.
  3. **Spec first:** `docs/sim/fixed-map.md`. Line-oriented ASCII text, `#` comments, a version line, every field required, unknown keys and duplicates refused. Contents: the world edge; the base scenario (an original endless `.sze` path) whose SCENE HEAD gives every `WorldSettings` field except the world size; the seed; per island a unique slot, x, y and the template key exactly as `scan_island_templates` keys it (upper-cased relative path under `Scenes/Islands`); per start region the seat index, its island slot and the developer-start Kontor (x, y, rotation); optional start money (default: the base scenario's first human player's HEAD money, the evidence value). The **digest** is FNV-1a 64 over the parsed canonical form, so CRLF and LF checkouts agree; it goes into the handshake later (W3). Raw materials and ores: a field per island is allowed but must be documented as not applied yet (the existing gap).
  4. **Reader and dump:** `anno::formats::fixed_map` parses the text into fixed-width fields and refuses malformed input with the line number. `anno-dump fixed-map <file>` prints the canonical summary; `tools/fixed_map.py` prints the same text from its own parser; `tools/agree.py` compares them on the synthetic layouts (CI) and the real layout.
  5. **Loader:** `sim_load::load_fixed_map(layout, IslandTemplates, TypeRegistry, SimConfig, Rules, seats)` → the same `ResolvedScenario`/`LoadedScenario` shape the `.sze` path gives, so `World::create` takes it unchanged. Settings from the base scenario's HEAD with the world size replaced; islands from the named templates at the given positions with **the template's own climate** (INFO[0]); players = the seats given (2 to the layout's start-region count), kind 8, volk 1, the seat's colour index, the start money; decide with evidence whether to carry the base scenario's player 31 and record it in Decisions and reasoning. With the developer start on (a loader option, on for MVP-A), each seated player gets one city on the start island with the Kontor at the layout's position, created at tick 0 without cost. Unseated start regions stay free land. No draws.
  6. **The first layout** `data/maps/lan5.txt`, following `lan-mvp.md` §3 (proposal, owner question 6): edge 1431 by D1's formula, unless validation or alignment requires another value (then the nearest multiple of 8 with the reason); five start regions with the same templates, at the same distances from their start (positions rounded to the 8-tile grid); contested islands in the middle; climates chosen so each region holds the same set. Write it by hand or by a reproducible script (say which). `tools/fixed_map.py` validates it (inside the world, no overlap, alignment, known templates) and writes a top-down preview PNG (island rectangles coloured by climate, start markers) to `C:\Users\altin\Pictures\anno-review\MAP-1\`.
  7. **Fairness, measured once:** per start region the land tiles by climate and by island size class within the region, and the distance from the start to the middle; equal across regions by construction, reported as a table. Raw materials are listed as not applied.
  8. **Tests:** synthetic (CI): every refusal (unknown key, missing or duplicate field, duplicate slot, overlap, outside the world, unknown template, a seat without a start region, more seats than regions); a valid synthetic layout → `World::create` succeeds, players equal the seats, each developer-start city exists with its owner and a Kontor; the digest is equal for CRLF and LF. With the game files: `lan5.txt` loads, `World::create` succeeds with 2, 3, 4 and 5 seats, five simulated minutes run with `check_invariants` clean, and two runs give the same hash.
  9. **Report and stop.** OWNER-CHECK: the preview PNG and the fairness table go to the owner (`lan-mvp.md` §8 question 6) before the layout counts as final.
- **Acceptance:** the facts checked; one registry loader with byte-identical existing results; the spec; reader, dump and Python reference agree; the loader builds a `World` from `lan5.txt` for 2–5 seats with the developer start; the preview and the fairness table exist; every refusal tested; no original bytes in git (`data/maps/lan5.txt` names templates only).
- **Gates:** board §1 items 4, 9 and 12 in full (formats/sim_load/tools: MSVC, CTest with and without the game files, clang-format 23.1.3, GCC 13.3 and Clang 18 `-Werror` in WSL, `tools/agree.py`); mutations: drop the climate from the template (the climate test fails); skip the overlap check (the overlap refusal fails); hash the raw bytes instead of the canonical form (the CRLF/LF test fails). Commit by concern; report `## MAP.1 — The fixed map — <date> — status: ...` with Decisions and reasoning; then part B.

#### MAP.2 — An original-like five-player map, fair by measure · `ASSIGNED — lane 4` (round dm; a new executor session; planned in round dl) · `PARALLEL-OK` with W4.1 (tools and data only) · branch `task/MAP-2-original-like` from main at 6db51eb or later · new OQs OQ-515..524
- **Why:** the owner rejected MAP.1's lan5, five rotated copies of one region around one contested island, as too artificial and symmetric (round dl, `lan-mvp.md` §3 and §8 question 6). Wanted: a map that looks and plays like an original endless map, scaled to 1431 tiles and five players, and fair by measure.
- **Facts (orchestrator's measurement, 2026-10-29, `anno-sim-replay scenario <file> --ticks 0` over the nine `Scenes/0010SingleEndlos` files; re-measure with your own tool):**
  - The "256" maps (01, 03, 06; 1024²) have 27 islands: sizes 64×5, 80×10, 128×8, 256×4. Climates 0–5 are all present (3/3/5/4/7/5). Island rectangles cover 46% of the map. The median gap to the nearest island is 16–32 tiles; the minimum is 8.
  - The "128" maps (02, 04, 07) have 28 islands: 64×2, 80×13, 128×13, 29% coverage. `00_easyentry` has 16 islands, 19% coverage. The 800² maps have 23–25 islands.
  - lan5 has 21 islands on 1431² (1.95× the area of 1024²): 23% coverage, one template per role, five climates. At the "256" density, 1431² holds about 50 islands.
  - In the originals, link positions and sizes come from the `.sze`, and the templates are drawn by the resolver. The fixed map names its templates instead (MAP.1, `docs/sim/fixed-map.md`).
  - Available tools: `tools/fixed_map.py` (`check`, `fairness`, `preview`). Its fairness assigns each island to the nearest Kontor unless the island is closer to the map centre, in which case it counts as contested. Also `tools/fixed_map_layout.py` (lan5's script) and `tools/world_measure.py` (world-scaling §12).
- **Owner's layout rule (2026-10-29, after looking at `--play`; binding):**
  - **Five large northern islands** (NORD, the 256² class) and **five large southern islands** (DSCHUNGEL, the 256² class), spread across the map. That is the large-island rule of the original "256" maps (`03`: two NORD 256 at y 32 and 368, two DSCHUNGEL 256 at y 648 and 728), scaled to five players.
  - **North/south by the map's rows** (the original's climate bands, world-scaling §3 and §7), not the screen. Each northern island lies on or above the middle line y = edge/2 (its centre y ≤ edge/2). Each southern island lies below it (its top y ≥ edge/2), so below the northern islands.
  - **Varied, not identical:** five different templates per type, of roughly the same land (measured; the tolerance is an owner question, propose ±10% of land tiles), at irregular positions with no rotated or mirrored copies.
  - **Each player starts on a large northern island** (the developer-start Kontor on its coast). Each start has one distinct large southern island within comparable reach (an assignment of southern islands to seats; the distances are equal within the fairness tolerance).
  - **Available templates** (`game/app/Scenes/Islands`): NORD 256: 14 usable (`N_256x256_0002…0019`; leave out `_DUMMY` and `0010…scp.bug`). DSCHUNGEL 256: 18 usable (`D_256x256_0001…0045`, leave out `_DUMMY`). Templates with a people's name (`_Mongolen`, `_Piraten`, `_Azteken`, `_Afrika`, `_Suedsee`) likely carry settlements of natives or pirates. Use them only after the owner decides the natives question (`lan-mvp.md` §8 question 1); start islands never use them.
  - **The rest of the map** is original-like filler: polar and tundra in the far north, praerie and steppe in the middle bands, smaller nord and dschungel islands, all by the original's climate bands. Density follows step 1's targets.
- **Steps (outline; the orchestrator writes the full packet when it is released):**
  1. **Measure the originals:** island count per area, size mix, climate mix, nearest-gap distribution, and how evenly islands spread (for example the nearest-neighbour distances of island centres). Write them into `docs/sim/fixed-map.md` as design targets, scaled to 1431².
  2. **Define "fair by measure"** in the spec: per seat, the land per climate within reach (state the reach rule), the home island's land, and the distances from the Kontor to the centre and to the nearest rival, each with a tolerance. Propose ±5% as the default. The tolerances are an owner question. `fixed_map.py fairness` prints PASS/FAIL per measure.
  3. **Candidate layouts:** two or three, each original-like by step 1's targets (varied templates and positions, no rotated copies, contested islands spread through the middle rather than one), each passing step 2. Hand-placed or made by a reproducible offline script with a fixed seed, whose committed output it reproduces byte for byte. The engine gets no generator; the files are data.
  4. **Previews** with a labelled legend (climate names), seat numbers and Kontor marks, plus one fairness table per candidate. OWNER-CHECK: the owner picks one, which becomes `data/maps/lan5.txt`.
  5. **Tests:** the lan5 test with the chosen layout (2–5 seats, five minutes, MSVC = GCC hash). Measure the sim tick on the chosen map with five cities (`lan-mvp.md` §3).
- **Acceptance:** the targets measured and documented; the fairness rule and tolerances in the spec; the candidates pass it; the owner's pick committed; the gates of board §1 items 4, 9 and 12.

#### W4.1 — Play mode: the sim runs in the app, the scene shows the sim's world · `DONE` (round do; merge 8ea19ba, accepted at `964f922`; owner check passed) · lane 3
- **Why:** `lan-endless.md` W4 steps 1–2 and `lan-mvp.md` §4: MVP-A needs the app to run the deterministic sim at a fixed cadence and the renderer to show the sim's world. Stage 1 sets the split between static content (terrain, scenery: from the files) and dynamic content (buildings: from the sim) and the threading contract. Building changes after tick 0, figures and input are W4.2–W4.4.
- **Facts this task builds on (orchestrator's read, 2026-10-29; re-check each):**
  - The app links only `anno_render` (`src/app/CMakeLists.txt:11`); modes: `--view` plus `--png`, window loop in `Viewer::run` (`viewer.cpp:438`); `Viewer` holds `const render::Scene&`.
  - `render::Scene` is built once from `formats::insel::Island` records (`scene.cpp:207`); `objects_` never changes; `ChunkWorkers` read the scene concurrently without locks; the chunk cache (`FrameRenderer::cache_`, keyed by zoom group and screen chunk) has no invalidation path. Placed objects need `anlage`, `rotation`, `state` (height << 8 | phase), island-local `x, y`.
  - The app's link stand-in rule ("island number mod count", `loader.cpp:265-305`, OQ-028) differs from the sim's `resolve_scenario_links`, so the viewer and the sim can show different islands for one endless file.
  - The sim keeps no per-object height for scenery (`walk_cells.cpp:225`); `Building::byte0f` holds the PROD2 item height for buildings (`scenario_formats.cpp:927`).
  - Cadence data: `Timing` (`clock.hpp`): 10 real ms per step, 10 steps per turn, 100 real ms per turn; `multiplayer_step_ms(percent)` game ms per step. Session entry points: `run_turn` / `run_frame` / `Recorder` (`replay.hpp`). `World` is copyable; `World::save`/`load` exist; no consumer snapshot type exists.
- **Files:** `src/app/` (a `--play <file.sze>` mode, the sim runner, the bridge; `--view` keeps its behaviour), `src/app/CMakeLists.txt` (link `anno_sim`, `anno_sim_load`; decide whether the sim enters `anno_app_core` or a separate target and record why), `src/render/` only as far as building a scene from supplied island data needs (no change to existing scene output), app/render tests, a new `docs/app/game-shell.md`, `docs/perf/README.md`, `open-questions.md` OQ-500..509, REPORTS. No sim rules, no net, no UI widgets.
- **Steps, in order; record each result before the next:**
  1. **Check the facts above.** Any that does not hold: QUESTION.
  2. **Spec first:** `docs/app/game-shell.md`: the threads (window thread, sim thread, chunk workers), what each owns, the snapshot the sim publishes and how the window reads it without waiting, the cadence, the static/dynamic split, and that the render never receives a mutable `World`.
  3. **The sim's world in the app:** `--play <file.sze>` builds the registry (MAP.1's loader), loads with `load_scenario_with_links` and creates the `World`. The render scene uses **the islands the sim placed**: embedded `INSEL5` islands, and for each link the template the sim's resolution chose, at the link's position, in place of the stand-in rule. Keep the input as a list of (island slot, source record, position), the seam MAP.1's fixed map fills later.
  4. **Buildings from the sim:** the scene's buildings come from the `World`'s buildings (type, x, y, rotation, `byte0f` as the height byte); terrain and scenery come from the files without the building items, split by the same type classification the scenario loader uses. Add to `render` only what is needed to build a scene from supplied object lists.
  5. **The sim runner:** a sim thread advances the world in turns of the original's multiplayer cadence (from `Timing` and `multiplayer_step_ms`, through `run_turn`, the path the net layer will use) and publishes an immutable snapshot after each turn (at least: tick, game time, turn cost). The window thread only reads the latest snapshot and never waits for the sim; pause stops turns. Wall time decides only *when* a turn runs, never its content. A sim that falls behind is measured and reported, not caught up without bound.
  6. **Show it:** the window title shows the tick, the game time and the turn cost (p95 over the last second); the frame log gains the sim figures.
  7. **Tests:** (a) island set: for every `.sze` of the corpus, the play scene's islands equal the sim's (slot, rectangle, template); (b) tick 0: for the ten `0200Single` scenarios the play scene renders pixel-identical to the `--view` scene on the CPU path, except objects the sim did not place, each listed and explained (no faking); GPU = CPU as for the viewer on the adapters you have; (c) determinism: N turns through the runner give the same `World` hash as the same turns applied directly; (d) a deliberately slow sim step does not stall frames; (e) a building removed from the `World` by a `DemolishCmd` before the scene is built is absent from the play scene (its pixels differ from the `--view` scene only inside that building's screen box).
  8. **Measure** (PLAN §0.3, `docs/perf/README.md`): the turn cost (p50/p95/p99/max) on the largest endless scenario; the frame time at 1920 × 1080 and 3840 × 2160 on the RTX 4090 with the sim running versus stopped.
  9. **Run and look** (§1 item 10): `--play` on two endless scenarios and two `0200Single` scenarios at 1280 × 800 and 2560 × 1440; PNGs to `C:\Users\altin\Pictures\anno-review\W4-1\`; say what you saw. Then report and stop: **OWNER-CHECK** (the owner looks at `--play` before W4.2).
- **Acceptance:** the spec; `--play` runs a scenario with the sim on its own thread at the original's turn cadence; the scene shows the sim's islands and the sim's buildings; tests (a)–(e) pass; measurements recorded; `--view` unchanged (its existing tests pass unchanged).
- **Gates:** board §1 items 4, 9, 10 and 12 (render/app: MSVC clean, GCC 13.3 and Clang 18 `-Werror` over every changed library including `app` with and without `ANNO_VIEWER_GPU`, GPU tests named with their adapters); mutations: feed the runner wall-clock-dependent step sizes (test (c) fails); take link islands from the stand-in rule (test (a) fails); draw buildings from the file instead of the sim (test (e) fails). Commit by concern; report `## W4.1 — Play mode — <date> — status: ...` with Decisions and reasoning; stop.
#### W4.3 — Live building changes reach the picture · `ASSIGNED — lane 3 part A` (round dq) · branch `task/W4-3-live-buildings` from the round-dq board commit · worktree `E:\anno1503-worktrees\anno1503-W4-3` · OQ-525..529
- **Why:** after tick 0 the play picture never changes: `FrameRenderer` keeps a chunk in `cache_` until a new chunk is stored under the same key, and nothing invalidates a chunk when an island's buildings change (`docs/app/game-shell.md`, "What that leaves for the later steps"). A build or demolish applied by the sim must appear on screen within a bounded number of frames, without the window waiting.
- **Facts (orchestrator's read at main `88d828a`; re-check):**
  - `PlayRunner::one_turn` (`src/app/src/play.cpp:336`) builds a `sim::Turn` with no commands and calls `sim::run_turn(world_, nullptr, turn, steps_)`; `PlaySnapshot` (`src/app/include/anno/app/play.hpp:53`) carries tick, game time, turn costs, `behind_ms`, `paused`, and no world content.
  - The play scene's buildings come from `play_buildings(const sim::World&)` (`play.cpp:126`) at load; the picture re-encodes them as PROD2 records (W4.1 F1.6, `game-shell.md`).
  - `FrameRenderer` (`src/render/include/anno/render/frame.hpp:216`): `std::map<ChunkKey, ChunkPtr> cache_`, `update_chunks(scene, view, group)`, background `ChunkWorkers`, `release()` frees chunks on the worker thread. A rebuild path for a cached chunk already exists for river flow (`chunk_rebuilds`, `rebuilds_`): look at it before designing invalidation. The GPU compositor uploads chunks (`gpu.cpp`); an invalidated chunk must leave the GPU too.
  - A rejected command changes no hashed state and queues a `CommandRejected` UI event (`apply.hpp:27`, `world.hpp:239`). Build charges `cost_gold` and goods (`apply.cpp:348-362`).
- **Rules:** the window thread never touches the `World` and never waits for the sim or for a chunk; the sim never sees render types; rendering never mutates sim state. Determinism is untouched: the picture is a consumer. Static content (terrain, scenery from the files) and dynamic content (buildings from the sim) keep W4.1's split. No hardcoded limits; no new sim rules.
- **Do:** (1) spec in `game-shell.md` first: what the sim publishes about building changes (a per-turn change list or a per-island version, chosen and justified), how the picture learns which chunks are stale, and the latency bound in frames; (2) publish the change information with the snapshot (immutable, built on the sim thread); (3) the picture applies changes to its island data and invalidates exactly the affected chunks (a building's screen box can reach a neighbouring chunk; tall sprites included), rebuilt by the chunk workers while the old chunk keeps drawing; (4) a scripted command source for tests and stills: `--play ... --commands <file>` (a small text format of turn index + Build/Demolish, documented) feeding the runner's turns, the same path W4.4's input uses.
- **Tests:** (a) after a scripted Build and a scripted Demolish the CPU picture of the affected area equals a fresh scene built from the world at that tick (pixel-identical), and chunks outside the affected boxes are not rebuilt (counted); (b) GPU = CPU for the same case on the RTX 4090 (adapter named); (c) the window never waits: a slow chunk rebuild does not stall frames (measured); (d) determinism: the same command file through the runner and directly through `run_turn` gives the same `World::state_hash`; (e) a rejected command changes no pixel.
- **Acceptance:** the spec; changes visible within the stated bound; tests (a)–(e); no regression in W4.1's play tests and the viewer tests.
#### W4.4 — Developer input: Build and Demolish from the mouse, with a placement preview · `ASSIGNED — lane 3 part B` (round dq; after W4.3 on the same branch) · OQ-530..534
- **Round dr amendment (binding, overrides the text below where they differ):** the owner tests building and demolishing through UI.2, not through this developer input. Keep B1 (picking), B2 (queue), B4 (preview), B5 (refusals, money and goods) and B6. B3's picker is a minimal test tool: a key cycles the registry's buildable types, rotate, click, drag a road, demolish mode; no polish, no bindings table, no owner questions about bindings. Add **developer test resources:** generous gold and goods for the developer start (a `FixedMapOptions` or app-level option, default on for the developer start in `--play`, off for original scenarios, values configurable, defaults from the original's most generous start preset if `endless-presets.md` has one, otherwise chosen and stated); goods go where Build charges them (find that in `apply.cpp`); no rule change. The run ends with a review stop, not an OWNER-CHECK.
- **Why:** MVP-A is a LAN sandbox where players build, demolish and lay roads (`lan-mvp.md` §5). Before W5's HUD exists, a developer input lets the owner try that in `--play`. It is a developer tool, not the final UI: every binding is config, the original's controls are the default where evidence exists, and feel-changing choices go to the owner.
- **Do:** (1) tile picking: screen position → world tile under the cursor at every zoom level, including height (the inverse of the renderer's projection; tested at the three zoom groups and with continuous zoom); (2) a command queue from the window thread to the sim thread that never blocks the window (the next turn takes what is queued; the turn's command order is the queue order); (3) a developer building picker (a key cycles through the buildable types of the registry, the window title names the selected type and its cost); rotation; left click places one building as a `BuildCmd`, a drag places a road-like type as one `BuildCmd` with placements in stream order along a path (the original's path rule if documented, else a straight line and an OQ); a demolish mode sends `DemolishCmd`; (4) a placement preview drawn by the window: the selected building's sprite at the cursor, marked as accepted or refused. The window cannot ask the `World`; decide and justify what the preview knows (data published with the snapshot, or a footprint-only preview plus the sim's `CommandRejected` feedback after the turn), and show the reject reason; (5) `CommandRejected` events reach the window (snapshot) and are shown (title or an on-screen line); (6) money and goods of the player shown after each turn.
- **Tests:** tile picking round trip over a grid of tiles at each zoom group; the queue preserves order and never blocks the window (measured); a scripted input sequence (synthetic events) produces the expected commands; determinism: recorded input → commands → same state hash as the command file.
- **Owner questions to record (do not decide):** default bindings where the original's are unknown; whether the preview may show refusals the stand-in placement rule would accept (OQ-052).
- **Acceptance:** in `--play` on `data/maps/lan5.txt` with the developer start the owner can place, rotate and demolish buildings and lay a road, sees the preview and the refusals, and the picture follows. **OWNER-CHECK** at the end.
- **Gates (both parts):** board §1 items 4, 9, 10 and 12; GCC 13.3 and Clang 18 `-Werror` over every changed library including `app` with and without `ANNO_VIEWER_GPU`; mutations: no invalidation (W4.3 test (a) fails), invalidate everything every turn (the "not rebuilt" count fails), commands applied in another order than queued (the determinism test fails), picking off by one tile (the round trip fails). Report `## W4.3 + W4.4 — Live buildings and developer input — <date> — status: ...` with Decisions and reasoning; stills under `C:\Users\altin\Pictures\anno-review\W4-3\`; stop.
#### UI.1 — The original's in-game UI, reverse-engineered with high precision · `DONE` (round dt; accepted as the spec base at `690d4c4`, merge `df6dcc7`; REPORTS "Round dt"; open items in UI.1.F2) · branch `task/UI-1-ingame-ui-re` from the round-dr board commit · worktree `E:\anno1503-worktrees\anno1503-UI-1` · OQ-535..549
- **Why:** owner decision 2026-10-30 (round dr): the owner tests building and demolishing in person through the original's in-game UI, so UI.2 must rebuild that UI faithfully, every menu and submenu present. `docs/re/ui-and-placement.md` is a first pass (fractions of a 1024 × 768 frame, a partial category table, several entries marked "not verified"). UI.1 makes it exact and complete. No engine code.
- **Evidence sources (re-check each):** `Data/*.dat` texture and access tables (for example `Data/statusbar.dat`: `[FILE]`, `[COLOR]`, `[TEXTURE]` with `Access`, `Pos`, `Size`); `ToolGfx/*.tex` + `.inc` (2 431 files; `docs/formats/tex.md`); the fonts (`.zei`, `docs/formats/zei.md`); `Texte.dat` and `Profiles/Texte` (`docs/formats/data-text.md`); `help/*.htm`, `InstantHelpLinks.txt` and `Manual.pdf` (`docs/research/manual-and-help.md`); `Anlagen.dat` for the construction menu's buildings, costs and unlock fields (`docs/re/production.md`, `population.md`); the DLLs (`AnnoFrame.dll`, `AnnoDisplay.dll`, `AnnoGame.dll`) for logic not in data (module and address); the original running in the Wine runner (`AnnoHarness`, pre-authorized, silent; `docs/testing/original-wine.md`, `tools/orig/control.py`, `tools/orig/menus.json`).
- **Deliverables:** (1) `docs/re/ui.md`, the spec: the screen tree of every in-game screen, panel, menu, submenu, dialog and tooltip, and for every element its graphic (file, access name or frame), position and size in pixels at 1024 × 768 plus the anchoring and scaling rule at the other resolutions the original offers, text key, hotkey, states (normal, hover, pressed, disabled, `?` locked), click behaviour (target screen or the command it sends) and its visibility or unlock condition with the data field; (2) the construction menu tree exactly: categories in order, every cell in order with its Anlagen type, cost display and lock rule; (3) a machine-readable layout under `data/ui/` holding derived facts only (ids, positions, file and access names, text keys), never original pixels or text; (4) `tools/ui_layout.py`, a Python oracle: validates the layout against the game files (every texture, access and text key exists) and renders a mock frame from the original textures into `cache/` for a pixel comparison with a Wine-runner screenshot, with self-tests on synthetic data; (5) an implementation-status table: for every menu entry, whether the rebuild's sim has the feature (implemented / placeholder), for UI.2.
- **Order:** the HUD, the construction menu and its submenus, the building info panel, demolish, the status bar and the minimap controls first (what the owner's build tests need); then military, help/info, the options and game menu with every submenu, the settlement, statistics and trade panels, ships, diplomacy, messages and dialogs.
- **Acceptance:** every screen reachable in a running game has a captured screenshot and a spec entry; every element traces to a file entry or a disassembly address; the oracle's mock of the HUD and of the open construction menu matches Wine-runner screenshots at 1024 × 768 within a stated tolerance (differences listed and explained); unknowns are OQs, never guesses stated as fact.
- **Gates:** `tools/agree.py` unaffected; the oracle's self-tests pass; no original file, pixel, text or screenshot in git (`git status` shows nothing under `game/`; screenshots stay in `cache/`); board §1 item 12 (Decisions and reasoning). Report `## UI.1 — The original's in-game UI — <date> — status: ...`; stop for review.
#### UI.1.F1 — Complete the UI spec to the acceptance: element level, every cell, a pixel oracle, the rest of the tree · `DONE` (round dt; `690d4c4`, merge `df6dcc7`) · continue `task/UI-1-ingame-ui-re` in `E:\anno1503-worktrees\anno1503-UI-1` from `3c0ce65` · OQ-539..549 (535..538 stay open until closed)
- **Why:** round-ds review. The UI.1 run reported FINISHED, but the acceptance of UI.1 is not met. UI.2 is the owner's test surface and needs the full spec. The UI.1 block above (sources, deliverables, order, acceptance, gates) stays binding; F1 lists what is missing.
- **F1.1 — repair and guard the files.** Sections 1–4 of `docs/re/ui.md` at `3c0ce65` contain 11 control bytes and 28 broken lines from escape processing (a backtick + `r` became a line break, `\t` a TAB, `\f` a form feed: `r2_*` shot names and table rows split, `fenster.inc` became FF + `enster.inc`, `farms` and `roads` lost letters). Rewrite those sections from the shots and data, not by patching bytes. Add a test that every UI.1 file in git (`docs/re/ui.md`, `data/ui/*`, the OQ section) holds no byte below 0x20 except LF (and CR before LF), and that every Markdown table row has its table's column count. Write files with a method that does not interpret escapes. Replace the one English UI string in git (`Player-menu`) with its text key.
- **F1.2 — the whole construction menu, data first.** Every cell of all eight categories with its Anlagen type id (about 148 cells at the Citizen start; addon cells too), its cost display and its lock rule. Find where the original assigns types to cells and orders them before capturing more: candidates are `Anlagen.dat` fields, the `[ANLAGEN]` order in `Texte.dat`, `BGruppen.dat` `[BAUSTUFEN]` against `INFRAMIN`, and the construction-menu code in `AnnoFrame.dll` or `AnnoGame.dll` (RTTI `tCursorAnlage*`, `TOOLTIP:BAUMENU_*` string references; module + address). Then confirm by info-panel captures of every unlocked cell and a sample of locked cells in each category. The lock rule: which field and value draws `?`, verified at a second settlement stage if the runner can reach one (an original scenario or savegame with a higher-stage city counts; say which), otherwise data + disassembly, stated. Bind the eight `THUMBNAIL*` names and the `TOOLTIP:BAUMENU_*` sections to the banners with evidence.
- **F1.3 — element level for the build-test surface.** For the HUD, the four tabs, the construction menu (category column, grid, banner, scroll if any), the building info panel (name, preview, every cost icon and digit, upkeep, rotate arrows, cancel-X), demolish, the status bar (every segment: gold, the four `STATUSBAR_BAUMAT` materials, `STATUSBAR_ROSHTOFFE`, population, `?`) and the minimap controls (painted map rectangle, zoom plus and minus, `LUPE`, `KARTE_NORDUNG`): a measured rectangle at 1024 × 768 (from the shot, with the method; not `screens.json` fractions), the graphic (file, ACCESS, texture id and frame per state), the text key (`Texte.dat` section + index, bound by evidence), the hotkey, the states (normal, hover, pressed, disabled, locked: capture hover and pressed where the runner can hold them), the click behaviour and the visibility condition. Placement as the UI shows it: the valid and refused preview, rotation (`.` / `,`), CTRL alignment, drag for roads, each with a capture.
- **F1.4 — the oracle does what UI.1 asked.** `mock <screen>` composes a PNG in `cache/ui-mock/` from the original textures (decoded with `tools/tex.py`) at the `data/ui/` positions, for the idle HUD and the open construction menu (each category). `compare <screen> <shot>` diffs it per pixel against the Wine-runner shot over the chrome (masking the map and the live digits, masks stated in `data/ui/`), writes a difference image and a summary, and fails above a stated tolerance. `validate` also checks that every texture id and frame exists in its `.tex`, every text key in `Texte.dat` and every Anlagen id in `Anlagen.dat`. Self-tests on synthetic data cover mock, compare and the new validate checks. List and explain every difference above the tolerance in `docs/re/ui.md`.
- **F1.5 — the rest of the screen tree.** Capture and specify, in the UI.2 priority order: the selected-settlement and Kontor panels (trade), each entry of the in-panel options menu with every submenu (save/load, statistics, music, speech, quit and the rest), the help/info mode on a building, the player menu and diplomacy, the ship list (S), city list (C), rating (Q), messages, tooltips, the pause dialog. A screen the runner cannot reach is a QUESTION with the steps tried, not silence.
- **F1.6 — resolutions (OQ-535).** Capture the resolution list the original offers, then the idle HUD and the open construction menu at each in-game resolution; state the anchoring rule per element and which ToolGfx set (plain or `8`) each mode draws.
- **Gates:** the UI.1 gates; F1.1's byte and table test; `validate` on the game files; `mock` + `compare` of the idle HUD and of the open construction menu within the stated tolerance; `agree.py` unaffected; no `src/` change; no original pixels, strings or shots in git. Report `## UI.1.F1 — ... — status: FINISHED | QUESTION` with the UI.1 acceptance checked line by line; FINISHED only when every line holds, otherwise QUESTION with the open lines. Stop for review.
#### UI.1.F2 — The remaining graphics and screens of the in-game UI · `ASSIGNED — lane 5` (round dt) · new branch `task/UI-1-F2-ui-remaining` from the round-dt board commit · worktree `E:\anno1503-worktrees\anno1503-UI-1-F2` · OQ-535..549 (close them as resolved); ask for new numbers in a QUESTION
- **Why:** round-dt review. UI.1 is merged as the spec base; its own report lists what is still not captured or not traced. UI.2 draws every element from `data/ui/`, so an element without a bound graphic is one UI.2 cannot draw faithfully. The UI.1 block (sources, deliverables, rules, gates) stays binding.
- **F2.1 — every graphic of the build-test surface bound (first; UI.2 waits for this part).**
  - **The building icon in each unlocked cell.** Today it is masked in the compare and has no source in the spec. Find which texture and frame the cell gadget draws for a type: start at the cell builder `View.dll` `0x1002c650`; the record's second dword is 0 in the file, so the icon comes from elsewhere (the type's data, a per-type table, or a ToolGfx file). Record it for all 148 cells (locked cells draw `NO_KLICK`; check whether a locked cell's icon is ever shown, for example in a tooltip).
  - **The category column (OQ-540) and the wooden banner.** Start at the chrome builder `View.dll` `0x1002c360` and the gadget classes it creates; try `B_Baumenue.tex` with the colour key.
  - **The info panel's cost and upkeep icons, the zoom minus and `KARTEPLUS` 44 (OQ-542), `LUPE` and the compass (OQ-543), the portrait x positions (OQ-545), the pressed frames (OQ-544, from the gadget code: which frame a `tGadKlickGfx` shows when pressed), the pause parchment and the selected-house panel origin.**
  - Then remove the masks that only existed for unbound graphics (unlocked cells, category column, banner) and compare again.
  - **Tolerance with negative controls.** The channel 48 / fraction 0.12 tolerance was chosen after the measurements (worst 0.0525). The orchestrator's control, the public mock against the roads shot, gives 0.1817. Restate the tolerance from the measured spread, and add tests where a wrong category, a one-cell shift and a missing element must fail.
- **F2.2 — the reachable screens not yet captured.** The Kontor trade sliders (OQ-547), help on a placed building (OQ-548), the options statistics, quit confirm and the unused option icons (OQ-549), the ship panel opened from the ship list, an in-game message. Diplomacy and messages need a second player: start an endless game with computer opponents in the Wine runner (the original's AI is fine here; our MVP's "no AI" applies to the rebuild only). A later settlement stage for the lock pixels (OQ-539): try the original's scenarios, campaign starts and endless presets with a richer start, or grow a city with the runner at top speed. Each with captures and the same depth as F1.3.
- **F2.3 — what only the owner's desktop can capture.** The Wine runner cannot switch display modes, so the HUD at 800 × 600, 1152 × 864, 1280 × 960 and 1280 × 1024 (OQ-535) needs the owner's dgVoodoo2 desktop harness. Do not drive it. Write the exact capture list and steps as an owner checklist (`docs/testing/checklist.md`), so the owner can run it in one sitting, and give the expected anchoring per mode as a prediction to check.
- **Gates:** the UI.1 gates; `check-files`; `validate --game-dir`; `mock` + `compare` of the idle HUD and all eight construction categories with the reduced masks and the new negative tests; no `src/` change; no original pixels, strings or shots in git. Report `## UI.1.F2 — ... — status: FINISHED | QUESTION` with a table of every element of the build-test surface (bound / open) and every screen (captured / open); stop for review.
#### UI.2 — The in-game UI with every menu, placeholders where the sim has no feature · `PLANNED` (round dr; dependency updated round dt) · needs UI.1.F2 part F2.1 accepted, W4.4 merged and ADR 0004
- **Scope:** ADR `docs/adr/0004-ui-toolkit.md` (D4: our own small widget set drawn by the GPU compositor with the original's fonts and `ToolGfx` textures); the HUD, the construction menu and every other menu and submenu of UI.1's spec, built from `data/ui/`; implemented entries drive W4.4's command path (Build, Demolish); every other entry exists and leads nowhere yet (a placeholder, as the owner decided); pixel checks against UI.1's oracle mocks. **OWNER-CHECK:** the owner builds and demolishes through it.
#### P1 — Population stage 1: the house payload, the group table, scenario import · `DONE` (round cx; merged locally as `b173d02`, P1 branch unchanged at `416993fa`; combined tested head `ef6121a`, report tip `5b10ed3`) · the main executor · branch `task/P1-house-payload` from local `main` at the round-cm board commit or later · worktree `E:\anno1503-worktrees\anno1503-P1` · new OQs OQ-450..459
- **Why:** W6 is the LAN plan's critical path (`docs/plan/lan-endless.md` §5 W6.2, §6), and population is the first W6 stage. A house is an `Inert` payload today: the residents and stocks in the scenario files are dropped (`HouseRecordNotModelled`), and T26's three pure functions are tested but not applied. P1 gives the house its state and loads it. It adds no house tick (that is P2).
- **Spec:** `docs/sim/state.md` §4.7c "Population (DRAFT, T32)", with the **P1 decisions** below, which override the draft where they differ. Evidence: `docs/re/population.md` §1 (the loader formulas), §11.2 (the house fields), §12.1 (the `BGruppen.dat` field map). Owner decisions of 2026-10-25: §4.7c owner questions 1 and 3.
- **P1 decisions (round cm, orchestrator):**
  1. **Tags.** `PayloadTag::House = 7`, because `Market = 6` exists (the draft's "next after `Mine = 5`" is stale). `TickKind::House` is appended (value 8), because `check_consistency` (`src/sim/src/types.cpp:457`) pairs every tick kind with its payload through `payload_for_tick` (`types.cpp:361`). `production_kind_for_tick` returns nullopt for it, as for `Market`. The class-table row becomes `{C::House, T::House, P::House, 2500}` (`types.cpp:640`).
  2. **No house behaviour.** In `tick_building` (`src/sim/src/driver.cpp:217`), the `TickKind::House` case returns at once and touches nothing, `last` included, exactly like `TickKind::None` (D15). P2 fills it.
  3. **Versions.** `kStateHashVersion` goes from 114 to 115 (the draft's "today 111" is stale). `kPrngTimelineVersion` stays 2, because P1 draws nothing.
  4. **The save is exact.** `state.md` §9.1: one field list, and a field can never be saved and not hashed. So the rebuild's own save is the runtime form, and save then load gives the same state hash, a residents fraction included. The original's lossy record (integer residents, `E` not saved, the `s_j` bytes) is only **read**, on scenario import. P1 writes no original-format house record.
     - Owner question 1 (catch-up after a load) concerns `last`, which nothing reads before P2. P2 decides how loading our own save reproduces it; open an OQ in the range for it.
  5. **The reset on import is not applied.** `house_reset_applies` true means the original sets residents to the group's `Wohnmax` and draws once per forced need (`population.md` §13.1, `0x10077079`–`0x1007709c`). The loader has no draw source, and the place of that draw in the load-phase cursor order is unread.
     - So P1 follows the `ComputerNameNotDrawn` precedent: it adds the warning kind `HouseResetNotApplied`, loads that record as stored, and adds no draw site.
     - Shipped scenarios never enter that branch, so the corpus count must be 0 for every file.
     - Open an OQ in the range for the draw's place.
  6. **Widths.**
     - `std::int32_t residents_64` and `std::uint32_t e`: the original's i16 and u16, widened because CLAUDE.md requires widening when data enters the engine. Comment the original widths.
     - `std::vector<std::uint32_t> stock`: one entry per need of the house's group; T26's functions already return `u32`.
     - `std::uint16_t last`, like every other payload.
     - `mood_flags` is **omitted**: owner question 7's default, since nothing writes it yet.
     - Record in an OQ in the range whether the original's i16/u16 wrap is observable. With shipped data it is not, because `Wohnmax` is small.
  7. **The group table** is registry data: immutable, load-time, in the registry digest (`digest_registry`, `types.cpp:291`), never state.
     - Parse it from `Data/BGruppen.dat` with the `formats::text` helpers (`section_entries`, `split_records`, `fields`, `to_int`; the use in `src/formats/tests/text_zei_test.cpp:130-146`), in `src/sim`.
     - Per group, keep the fields §4.7c "Group table" lists, with the §1 loader formulas. Omit `Priority`, `BauFirst`, `BauLast`, `Hiscore` and `Wissen`.
     - Add it the way the walk tables were added (`TypeRegistry::Builder::walk_tables`, the `build` overload with `tables`). Pass the parsed file at every call site that builds the original registry from the game directory: today `src/sim/tests/scenario_test.cpp:137`, `src/tools/sim_replay/main.cpp:358` and `src/sim/tests/transport_test.cpp:886`; grep `TypeRegistry::build(` for others.
     - The table may be empty (synthetic registries); then every house has no stocks. A non-empty table must have exactly `SimConfig::house_groups` groups, or the build is refused with a clear error.
  8. **The payload at creation** is the house constructor's state (`population.md` §11.2): `residents_64 = 0x140` (5·64), `e = 0x280` (5·128), every stock 0, `last` 0.
     - The original sets `last` to the creation time through property `0x23` (`0x100733ac`). P2, the first reader of `last`, does that; P1 leaves it 0.
     - `make_payload` and `payload_problem` (`src/sim/src/building.cpp:58`, `:108`) need the group's need count: pass it in, from `house_group(cfg, type.prodtype)` and the registry's table. `payload_problem` rejects a stock vector of another size.
  9. **City side.** `BuildingIndex::house_counts` is unchanged. P1 adds no per-group resident totals on `City`: they come with their first reader (P3/P4), as derived caches with a `check_invariants` rule.
  10. **G7 in P1** is the scenario and its create hash only, as `p1_g7_create`. The growth/decay asserts and the "decay omitted" mutation of §4.7c's test list belong to P2.
- **Steps (in order; §1 item 7; each written into REPORTS before the next):**
  1. **Set up:** `git -C E:\anno1503 worktree add E:/anno1503-worktrees/anno1503-P1 -b task/P1-house-payload main`, then `cmake --preset msvc` and `cmake --build --preset msvc` in the worktree.
  2. **The group table (decision 7).**
     - Write the parser and its tests:
       - a synthetic text of two groups gives exact values;
       - a missing field, a non-integer value and a table of the wrong length each fail with a clear error;
       - the shipped `Data/BGruppen.dat` (through `anno::test::game_dir`; skip cleanly without it) gives 5 groups and 47 needs, plus the per-group values that `population.md` §12.1 states.
     - Put the table on the registry and in its digest. Update every literal that pins the registry digest (`src/sim/tests/types_test.cpp`, around `:1557`) and the independent transcription in `tools/sim_stream.py`.
     - Nothing from the file goes into the source or `fixtures/`.
  3. **The payload (decisions 1, 2, 6, 8).**
     - `struct House` in `building.hpp`; the `Payload` variant (`building.hpp:49`); `kPayloadTags` (`state_fields.hpp:676`); `fields()` with a `field_count` `static_assert`, visiting `u16 last, i32 residents_64, u32 e, seq<u32> stock`.
     - `make_payload`, `payload_name` and `payload_problem`; the class-table row, `payload_for_tick` and `production_kind_for_tick`; the `tick_building` case.
     - `tools/sim_stream.py`'s class table and version.
     - Tests:
       - the tag counts and values in `building_state_test.cpp:319-335`, extended with the House stream layout;
       - the default payload equals the constructor values, with the stock sized by the group;
       - `payload_problem` rejects a wrong stock size;
       - 20 000 ticks leave a house payload unchanged (`last` included).
  4. **Scenario import (decisions 4, 5).**
     - In the house branch of `load_scenario` (`src/sim/src/scenario_formats.cpp:824-844`):
       - Read the 8-byte header: `[1]` residents, `[2]` n, `[3]` version, then the `n` bytes `s_j` through `house_satisfaction`.
       - If `house_reset_applies(group, r, Wohnmax(group − 1))` is true: warn `HouseResetNotApplied` once per record and load as stored.
       - Set `residents_64 = r << 6` and `e = r << 7`.
       - Set `stock[j] = house_need_stock(Menge_j, s_j, r)` for `j < min(n, group needs)`, and 0 above that (the constructor zeroes all 16 slots, `0x10077240`).
       - Pass the payload to `World::create` (as the market branch does).
     - Remove `HouseRecordNotModelled`; append `HouseResetNotApplied`, and update `all_warning_kinds` and the name switch.
     - Tests:
       - v2, v0 and v1 records;
       - the reset predicate true (the warning appears, and the record is loaded as stored) and false (no warning);
       - a record with `n` above and below the group's need count;
       - save then load of a world with a house whose `residents_64` is not a multiple of 64 gives the same state hash and payload.
  5. **G7 (decision 10).** Add `synthetic::g7_scenario` as §4.7c describes it: two small synthetic groups built in code, one pioneer house, the city store. Record `p1_g7_create`.
  6. **Re-record and docs.**
     - Bump `kStateHashVersion` to 115, with every assert that pins it (`sim_test.cpp:592`, `config_test.cpp:1467`).
     - Re-record the goldens (`ANNO_UPDATE_GOLDEN=1`) and `fixtures/synthetic/sim/scenario_corpus.txt` with the game dir. In the corpus, `HouseRecordNotModelled` is gone and `HouseResetNotApplied` is 0 for every file.
     - In `src/sim/tests/scenario_test.cpp`, assert that the residents of the created houses (`residents_64 >> 6`) sum to `resident_sum`, and that a loaded `single36` house has residents and non-zero stocks.
     - Docs:
       - an "as implemented (P1)" paragraph in `state.md` §4.7c, with decisions 1–10 and the stale draft numbers corrected;
       - the house payload in §9.3's stream order;
       - OQ-165's status;
       - the new OQs;
       - the W6 status line in `docs/plan/lan-endless.md`.
  7. **Gates and mutations.**
     - All of §1 item 4 and item 9: the MSVC build; `ctest` with and without `ANNO1503_GAME_DIR`; clang-format 23.1.3; the WSL GCC 13 `-O3 -Werror` and Clang builds of `sim` and `formats` with their tests, with and without the game dir. Also `python -I tools/sim_stream.py --check` and `python -I tools/lint_sim.py`.
     - Mutation checks, each with its named test failing (list them in the report):
       - `e = r << 6` on import;
       - the stock index shifted by one (`s_{j+1}`);
       - the reset warning never added;
       - the House tick writes `last`;
       - the group table left out of the registry digest;
       - a non-empty group table of the wrong size accepted.
  8. **Report and stop.**
     - Append `## P1 — House payload, group table, scenario import — <date> — status: FINISHED` to REPORTS with the head, every gate's result, the mutation table, the corpus summary line and the acceptance item by item.
     - Commit (one concern per commit) and stop. No merge, push or TASKS/AGENTS edits.
- **Acceptance:**
  - A loaded `single36` house has residents and stocks.
  - `HouseRecordNotModelled` is gone and `HouseResetNotApplied` is 0 across the corpus.
  - The created houses' residents sum to `resident_sum`.
  - Save then load is exact.
  - The House tick changes nothing.
  - Goldens and corpus are re-recorded at version 115, with `p1_g7_create`.
  - No city-store take, no draw, no `kPrngTimelineVersion` change.
  - Every gate passes, and every listed mutation fails its test.
  - Nothing from `game/` is in git.

### Follow-on set — first lane F6/T32.6 F1; second lane T61

The first executor's round-de packet continues parts A (T10c F6) and B (T32.6 F1) on their verified existing branches, then stops. The round-cy entries below are the original requirements, superseded only by the explicit continuation entries. T60 is accepted pending integration; the second executor's round-dc packet now assigns the complete T61 workflow below, then stops. The remaining entries are candidates for later packets; READY on an unrelated row is not permission to continue. Each implementation packet pins its then-current main and versions. Population is the LAN W6 critical path; the independent research/checker lanes do not adopt an ADR or implement production AI/networking.

#### T10c.F6 — Bounded artifact preparation, verified identity and isolated cold tests · `PAUSED` (round dk; unaccepted at `216a4b9`, round dj C1–C4 bind on resumption) · was `ASSIGNED — packet part A` (round de)
- **Continue/scope:** existing `codex/T10c-f5-cold-cache`, `E:\anno1503-worktrees\anno1503-T10c-F5`, from `5dc055692503dfd37facd954bcf2500bd47e05e6`. Do not create/rebase/rename it or continue the completed T10c branch. Allowed tracked files remain `src/test_support/`, the directly affected preparation/testing contract in `docs/perf/README.md`, and append REPORTS. No admission/recovery/compatibility policy change, sim/render edits, goldens/fixtures/version changes or unrelated refactor.
- **F6.1 — bounded, fail-closed preparation.** Add failing regressions before repairs: hung compiler (including a child retaining its output pipe), a lock held by another process, lock creation/acquisition failure, nonzero compiler exit after partial output, and an unusable completed artifact. `run_captured` currently waits indefinitely in fgets/pclose; native mutex/flock are unbounded; CreateMutex/open failure proceeds unlocked; the test artifact mutex ignores the wait result. Replace these with owned subprocess supervision and checked synchronization. Use one documented, configurable preparation wall deadline covering lock, compilation, validation and cleanup, default 120 seconds (the existing Windows lock bound now bounds the whole operation). Use short injected deadlines in regression tests; do not intentionally spend two minutes per negative case. No unbounded joins/output reads/destructor waits or unbounded captured output. Preserve a bounded diagnostic tail and real exit/timeout/error classifications. Cleanup only owned descendants and temporary output; no broad name/port kills. Lock descriptors are close-on-exec; handle ownership is RAII and all errors fail closed. Validate arguments/quoting for paths with spaces rather than constructing shell strings from raw paths.
- **F6.2 — completed artifact and exact test identity.** File existence/nonzero length is insufficient. Validate the expected executable, source build, compatibility and build configuration before reuse/publication; staging names and atomic publication must be safe under concurrent processes and interrupted builders. A failed attempt must not remove a previously verified good artifact. A ready line is parsed into exact fields, not substrings: the current predicate even accepts port 147825 for requested 47825 and arbitrary build/compat values. Verify expected build/version/port and owned process/incarnation; reject wrong build, wrong version, wrong port, malformed output, early exit and a foreign listener. Keep normal compatible cross-worktree sharing intact: strict identity is for artifact preparation and private coordinator tests, not a new runtime exact-build default. Do not trust a cached PID string after process exit/reuse; cleanup needs live ownership proof.
- **F6.3 — isolate cold tests and preserve instrumentation.** Cold tests must establish actual absence, fail clearly if setup/backup fails, and restore safely on every failure path. Prefer a private per-run artifact/cache root using the same production preparation code, so tests do not rename the canonical artifact out from under F3 or another test process. A CTest RESOURCE_LOCK only coordinates one CTest invocation; it is not cross-invocation protection. Script, journal, staging and artifact names must be unique across processes/worktrees. Exercise two independent preparer processes, not just threads; include interrupted builder then recovery and concurrent observer of publication. Do not hold an outer artifact lock while waiting for children that require it. Preserve an instrumented coordinator in sanitizer runs: no plain-g++ replacement of the CMake ASan/UBSan artifact. Build mode/flags and content hash join source build/version in the evidence manifest; verify the actually launched binary's instrumentation. A missing sanitized artifact is rebuilt with matching configuration or is an explicit blocked gate. Narrow/remove broad test serialization only if isolation is proven; no unmeasured throughput claim.
- **F6.4 — regression and mutation matrix.** Retain F5 cold/warm/full-parallel/compatible-companion and descriptor-layer tests. Add the F6.1–F6.3 negative and cross-process cases, including malformed/foreign artifact, wrong identity, lock timeout/failure, compiler hang/partial output, owned cleanup and a path containing spaces. Observe the companion's live identity/reservation after the operation (not only the previously captured string). Each process test has an external wall guard, captures diagnostics and reaps only its own processes. Mutants on final source must cover: bypass preparation, trust nonempty artifact, accept wrong readiness identity, ignore lock failure, accept partial output after compiler failure, disable deadline enforcement, and remove only meter-spawn closefrom with the inheritable fd >=256 hook. Name the test and intended failing assertion/diagnostic; outer timeout is a failure, never a passing test. Restore, force rebuild, verify source/artifact hashes and rerun unmutated focused cases.
- **F6.5 — gates and resource diagnosis.** Full Release MSVC build; complete no-game CTest from a genuinely cold private artifact state; complete with-game CTest; GCC 13.3/Clang 18 warnings; WSL ASan/UBSan budget suite with instrumented child identity evidence; clang-format 23.1.3 on changed C++; lint_sim and independent stream oracle. Log exact code head, executable hash/configuration and coordinator build/compat/incarnation per relevant gate, distinguishing shared host coordinator from private test instance. Retain 4096 MiB headroom, budgeted concurrency, full coverage and original goldens. Before retrying the failed heavy gate, inspect bounded read-only host/WSL capacity, reservations and queue/owner evidence; a wait deadline alone does not prove an external shortage. Do not kill the shared coordinator, discard its journal or stop owner/T61 processes. One fresh full game gate on final code; repeat a resource-failed run only after recorded evidence of changed conditions. If an admission defect is implicated outside preparation scope, preserve a bounded reproducer and report it rather than silently expanding policy scope. An open gate remains BLOCKED and does not prevent part B.
- **F6.6 — report and stop part A.** Append `## T10c F6 — Bounded preparation and verified test artifacts — <date> — status: ...`. Record verified starting head, fail-first/final tested heads, source/config/artifact manifests, phase timings, all regressions and mutants, cleanup/companion observations, full gate outcomes, remaining blockers and Decisions and reasoning. Explicitly supersede F5's overbroad artifact/readiness/sanitizer and external-shortage claims while preserving its historical text. Commit by concern (regressions, supervision/locking, artifact identity/isolation, evidence/docs as coherent changes); report final hash. FINISHED requires all acceptance and gates; then proceed only to part B.

#### T32.6.F1 — Complete the input derivation and bounded calculation contract · `ASSIGNED — lane 1 part A` (round dk: round dj S1/S2 corrections on `b821bbe`) · was `ASSIGNED — packet part B` (round de)
- **Continue/scope:** existing `codex/T32-6-house-tick-inputs`, `E:\anno1503-worktrees\anno1503-T32-6`, from `cfd85ec59d38f986bf1f0bde7d2ddcc5e6e2db88`, independent of F6. Only edit `docs/sim/house-tick-readiness.md` and append REPORTS. Read accepted T32.5 evidence at immutable `1fc598eb0b57f0d487fb50d315d7c4ad9c0d8af2`. No source/tests/fixtures/goldens/shared OQ or spec edits, build, game launch, Wine, merge/rebase or P2 implementation. Ignored scripts/derived evidence are allowed; originals remain read-only.
- **F1.1 — finish the assigned loader evidence.** Read AnnoGame parsing `0x1001486f`–`0x100148a6` and post-pass `0x10014afc`–`0x10014bdf`, extending only enough to establish initialization, receiver/stack, iteration order and referenced goods data. State exact derivation of `+0xcc` (written in parser, not post-pass), `+0xcd`, forced-infrastructure mask, id-to-slot sentinel/duplicates, WIRTSHAUS alias and substitution pass. Force does not decide whether an infra entry extends `cd`. Compare the current parser and read-only shipped BGruppen/Waren data; give a derived per-group summary and synthetic counterexamples for non-forced infra, interleaved goods/infra, duplicates/missing aliases and ordering. Distinguish original behavior, shipped-data invariants and proposed scalable engine representation/validation. Do not infer a safe generic filter from shipped ordering. Resolve OQ-472's requested read in the note, or name the exact new unread edge after completing these reads. Implementation gaps already assigned P2.1/P2.2 are dependencies, not new unknown original facts.
- **F1.2 — precise input/effect contract.** Re-read tick `0x10076230`–`0x100765b9`, mood `0x10075fd0` through its return paths, and necessary direct transition checks. Preserve proven smoothing/stock formulas and signed truncation; document sentinel/empty/zero-denominator preconditions without inventing original fallback behavior. Give the strict growth-versus-clamp boundary, counter subtract/add/correction order, negative floor versus downgrade/removal distinction, and mood override precedence (E==0 and negative table scores matter). Separate direct stock decay from transitive effects: UpgradeCheck consults queue/type/material inputs and upgrade/downgrade dispatch has effects; absence of a direct billing xref alone does not prove that every transitive call is store-free. Identify every missing input to an output predicate. The current proposed signature lacks goods BauFirst, previous/next group, walker occupancy/type maximum and upgrade context. Either include the complete inputs with evidence or narrow outputs to independently computable numerical candidates plus an explicit deferred/not-evaluated transition boundary. Do not call an incomplete predicate 'would fire'. No silent live PRNG omission or material/store mutation.
- **F1.3 — lifecycle and policy boundary.** Cite concrete creation/property-0x23 and original-load callers sufficiently to distinguish what is observed from what is assumed about the value left in last. Keep house u16 period advance separate from city stamp. Trace exact own-save/resync through the current field visitor. P1 decision 4 explicitly preserves an OQ-450 caveat, so neither this review nor the audit may silently resolve it by quoting only 'save is exact'. State the currently implemented exact preservation, the outstanding owner choice, affected hash/resync invariants and a recommended future option with reasoning. If original-load stamping remains unread, name the exact next call/address and qualify the claim that ctor zero matches every import. Correct contradictions inside the audit only; do not reset saves or decide policy.
- **F1.4 — numerical acceptance dossier.** Replace formula-only test rows with independently worked input/intermediate/output vectors: gate 2499/2500 and u16 wrap, last+=2500 versus now, mask on/off, E smoothing with positive/negative fractional residents and truncation, stock 1/127/128, satisfaction 101/102 and caps 128/160, score-table index boundaries, WIRTSHAUS contribution to goods plus infra, absent forced goods/infra, mood precedence, signed growth crossing an integer, strict Wohnmax crossing and negative floor/transition boundary, and widened intermediate overflow. Label unsupported/modded/undefined-original inputs explicitly. For every proposed output provide all required inputs and expected number or explicit deferred result. Use a small independent ignored arithmetic script if useful; do not generate expectations with future implementation code. Map concrete mutants to vectors. Describe later G7 evolution/save-resync/multi-peer checks and version/draw consequences without authorizing them now.
- **F1.5 — decision-ready plan and report.** Produce a complete producer-to-first-reader dependency map and bounded P2.1/P2.2/partial-kernel/live-P2.3 proposal with inclusions/exclusions, remaining owner/evidence blockers and concrete next reads. Distinguish P2 calculation prerequisites from P3/P4/P5 effects. Append `## T32.6 F1 — Derived inputs and calculation contract — <date> — status: ...` with exact audited heads, module/address evidence, corpus-derived summaries, numerical checks, explicit corrections to the original report and Decisions and reasoning. Check every cited path/address, no original bytes in tracked files, only the two allowed files changed, and git diff --check. Commit derivation, contract/vectors and report by concern. FINISHED may retain specifically justified owner/unread-transitive blockers; it may not defer the loader/mood reads explicitly assigned above. Report final hash and stop the packet; no next feature task.

#### T10c.F5 — Self-contained cold-cache WSL meter-child regression · `READY — packet part A` (round cy)
- **Baseline:** new `codex/T10c-f5-cold-cache` in `E:\anno1503-worktrees\anno1503-T10c-F5`, from the exact round-cy board commit pinned in the packet. Do not continue/rewrite the merged `codex/T10c-test-memory` branch. Main's source is the integrated `42b6738` tree; the later commits are orchestration docs only.
- **Problem/evidence:** `budget_f4_test.cpp::wsl_coord_binary()` computes the current source-hash cache path, but N4 launches it directly before anything ensures the file exists. Round cx reproduced warm success (1 case/5 assertions), cold failure (missing executable, empty PID), then restored the binary. `cache/orch-cx/n4-{warm,cold}.log`, coordinator logs and `check-n4-cold.ps1` are evidence. The shell can also print `STARTED` from a `pgrep` match without a running coordinator. A compatible coordinator serving another worktree is not proof that this exact test binary is prepared.
- **Allowed files:** `src/test_support/` only as needed for preparation, process harness, N4 and its regressions; `docs/perf/README.md` for the harness/build contract and `docs/orchestration/REPORTS.md`. No unrelated refactor, sim/render source, goldens, original data, TASKS/AGENTS edits, admission/headroom policy change or compatibility change. If a real semantic change proves necessary, report it before extending scope.
- **Steps, in order; record each before continuing:**
  1. **Reproduce before fixing.** Configure/build the Windows support test/probe/coordinator targets in the new worktree. Without running another budget case first, run only `F4 (N4):*` with this worktree's exact WSL cache binary absent. Preserve exit, test output and child launch error under `cache/f5/failfirst/`. Do not remove another worktree's cache, and never delete or stop a shared coordinator. If the exact binary is already present, move only that file to a checked backup and restore it in a finally block. Verify this failure is the missing prerequisite, not a compiler/permission failure.
  2. **Design the preparation contract.** Read `coord.cpp::ensure_wsl_coordinator`, `budget_proc_harness.hpp`, `child_process.hpp` and the N4 test. Separate obtaining the exact artifact from starting its private test instance. Prefer reuse of the existing preparation logic over a second divergent compiler command. Preparation must report compiler exit/failure, be bounded, serialize concurrent builders, and expose only a completed usable artifact (failed/partial output must not count as success). Scope changes to this preparation path; explain the bounds, synchronization and artifact identity in Decisions and reasoning.
  3. **Own the process and observe readiness.** Start the prepared coordinator with the leak hook, private test journal and test port. Capture the actual spawned PID/process handle or another ownership proof and wait for the exact instance's LISTENING/ready result with build/version/port. A substring match of a shell command line is not readiness. Missing executable, compile failure, bind failure or child exit must end with a specific bounded diagnostic. Cleanup stops only the instance this test owns; do not use broad name-based killing. Keep the coordinator-has-leak and meter-does-not-have-leak assertions intact. On Windows use the real WSL meter path; Linux retains its native path.
  4. **Regressions that prove independence.** Run isolated cold N4, then warm N4, then full parallel no-game CTest starting with this worktree's WSL binary absent. Also run cold N4 while a compatible companion coordinator/holder is live on a separately owned port: record that its PID/incarnation and reservation survive. Add a bounded failed-preparation/readiness test (no false STARTED/success) and exercise two preparers contending for the same missing artifact without accepting partial output. Do not let test-order dependencies masquerade as setup. Preserve all failed output before retrying.
  5. **Mutations.** On the final source, (a) bypass preparation: the cold regression fails for the intended missing-artifact condition; (b) accept shell presence instead of actual readiness: the failed-launch regression catches it; (c) remove the meter spawn's closefrom layer while supplying a deliberately inheritable descriptor: N4 fails the meter descriptor assertion, not compilation/setup. Use the F4 N4 method and leak hook (descriptor >=256, since startup already closes 3..255); do not remove the other hygiene layers. Restore and force-rebuild affected translation units, then pass unmutated cases. Record exact tested head, coordinator build/version and named failure per mutant.
  6. **Gates.** Full Release MSVC build; complete CTest with and without original files; WSL GCC 13.3/Clang 18 warning checks for changed code/tests and ASan/UBSan budget suite; pinned clang-format 23.1.3 on changed C++ files; lint_sim and independent stream oracle stay green. Use memory-budgeted concurrency and 4096 MiB headroom, retain coverage and clean skips. WSL unavailable on this host is a blocked N4 acceptance gate, not evidence of a pass. Record actual coordinator identity from each relevant run and whether hashed coordinator inputs changed. No new throughput claim or A/B rerun is needed for a preparation-only fix; changing runtime admission/performance would exceed this assignment.
  7. **Report/commit.** `## T10c F5 — Cold-cache coordinator preparation — <date> — status: ...`, exact code/report heads, fail-first, cold/warm/parallel/companion observations, failed-launch/concurrent-preparation cases, mutations, all gates, Decisions and reasoning. FINISHED requires every acceptance item; sustained host shortage must remain a qualified BLOCKED/QUESTION gate, never solved by lowering headroom or killing owner applications. Commit by concern and stop F5 work. Part B is independent and may follow even if F5 ends with a reported external blocker; do not carry F5 edits into its branch.
- **Acceptance:** the exact coordinator is prepared independently of prior tests; no partial artifact or false readiness; the descriptor check still catches the layer mutant; cold/warm/parallel and compatible-peer checks pass; all required gates pass with identities recorded. No push, merge/rebase, ADR approval or new feature task.

#### T32.6 — House-tick inputs, lifecycle and stage boundary · `READY — packet part B` (round cy)
- **Baseline/files:** new `codex/T32-6-house-tick-inputs` in `E:\anno1503-worktrees\anno1503-T32-6`, from the same pinned round-cy main/board commit, not from F5. Write only a new `docs/sim/house-tick-readiness.md` and append `docs/orchestration/REPORTS.md`. Existing specs/OQs/code are read-only for this audit. This avoids changing files in the accepted, not-yet-integrated T32.5 branch. Read its corrected `population.md` §15, `state.md` stage-mask paragraph and OQ-460..462 at immutable head `1fc598eb0b57f0d487fb50d315d7c4ad9c0d8af2` (git show or the verified clean worktree); cite that head. Do not copy its whole files over main or merge it into the audit branch.
- **Why:** P1 deliberately leaves `TickKind::House` a no-op. `Player::flags` exists but scenario import does not supply it; `GoodsInfo` lacks BauFirst/IdSec; `HouseGroup` omits derived first-reader metadata. A broad "implement P2" would hide missing producers and OQ-450's load-policy decision. No house behavior is authorized here.
- **Steps, in order; record each before continuing:**
  1. **Inputs and existing producers.** Read main's `population.md` §§1, 11.2, 12.2–12.3, `state.md` §4.7c, `driver.cpp`, `create.cpp`, `scenario.cpp`, `scenario_formats.cpp`, `house_groups.*`, `types.*`, `player.hpp`, state visitor and the T26 pure helpers. For every tick/mood/growth input, give its original source/address, width/rounding, current engine producer, first reader and missing work. Separate full player flags, goods BauFirst, need ordering/endpoints, aliases, forced needs, E, resident fraction, stocks and mood flags. A missing input is a blocker, not permission to insert a default.
  2. **Three lifecycle paths.** Trace a house created during play, one imported from original records, and one restored from an exact own-save/resync. Establish what writes `last` in each path, where property `0x23` fits (AnnoWelt `0x100733ac`), and what the actual house gate/update does (`0x10076230`, `last += 2500`). Distinguish the city stamp/OQ-461. Check current owner answers, OQ-132, P1 exact-save decision and OQ-450 together. If an existing decision settles the policy, cite it; if not, present the precise alternatives, affected save/resync invariants and owner question. Do not decide a feel-changing policy or reset exact snapshots by assumption.
  3. **Original order and effects.** Read the house update and mood path far enough to list decay, lowest need, walker/PRNG, mood, growth, clamp/upgrade/downgrade/removal and any city/store effects in execution order. Trace only callees needed to identify these boundaries. Annotate this-adjustments and live stack arguments. Keep OQ-451's load-phase reset draw and OQ-452's narrow-width behavior separate. No copied original bytes/disassembly in git; scratch evidence under ignored cache. No game launch or Wine fixture.
  4. **Data work and safe split.** Compare missing registry data to AnnoGame loader/post-pass `0x10014ad0`–`0x10014bd0` and actual helper code. State what P2.1/P2.2 must supply and what is first read only by P3/P4/P5. Propose the smallest useful next implementation, with explicit inputs/outputs/effects and supported cases. If a pure kernel can precede live integration, name it as a partial kernel. Silently skipping reachable transitions or PRNG draws is not a faithful live tick. Do not approve P2.3 or change the board.
  5. **Reviewable acceptance plan.** Give independently calculable tests for active/inactive mask bits, modular gate/wrap, period advance versus stamp-now, E smoothing/truncation, satisfaction/mood thresholds, signed fractional growth and overflow-safe widened intermediates. Include G7 evolution, save/resync continuation and deterministic multi-peer checks for the proposed eventual integration. Identify named mutations and version/draw consequences; leave unresolved versions/policies as explicit decisions for the implementation packet. Map every excluded effect to its later stage/blocker. No new tests or implementation in this audit.
  6. **Report/stop.** `## T32.6 — House-tick readiness audit — <date> — status: ...`, exact inspected code/evidence heads, input inventory, lifecycle results, policy decision status, proposed split, acceptance plan and Decisions and reasoning. FINISHED may retain explicit owner/evidence blockers if the inventory is complete and each has a concrete next read/decision; it does not mean P2 is ready. Place any candidate new questions in the readiness document (reserved OQ-470..479), not by editing the shared OQ file. No build required; check links/addresses/consistency and `git diff --check`, with only the two allowed files changed. Commit by concern and stop the packet.
- **Acceptance:** every input has evidence and a current producer or named gap; three lifecycle paths are distinct; original facts, existing owner decisions and proposed design are separated; P2/P3/P4/P5 effects and test boundaries are explicit. OQ-450 is resolved only by already-binding evidence/decision or remains an owner question. No source, goldens, fixtures, TASKS/AGENTS edits, merge/rebase, push, ADR approval, P2 implementation, T60/T61 or T40 work.

#### P2.1 — Restore the full player mask on original-scenario import · `ASSIGNED — lane 1 part B` (round dk; T32.5 integrated as 467828d; the packet holds the version and representation decisions; new OQs OQ-510..514) · was `BLOCKED on accepted T32.5 integration`
- **Evidence/current gap:** corrected T32.5; AnnoWelt `0x1001d389`–`0x1001d3df`. `Player::flags` already exists and participates in state serialization/hash, but `ScenarioSpec::PlayerSpec`, `read_player_head` and `World::create` do not carry the imported value. Default 1 is not a substitute for the full mask (only 263/373 restored corpus masks equal 1).
- **Scope:** load low 23 bits from HEAD+0x48 and reconstruct high bits 24/27/29/30 from HEAD+0x5c bits 3/2/4/5; pass the result through the scenario spec into the existing player field. Use one documented decoding rule, fixed-width field reads, and distinguish an explicitly stored zero from an omitted synthetic/default value. If a raw format reader is extended, preserve its byte-exact contract and independent oracle. No city unlock recomputation, setter side effects or house tick.
- **Acceptance/tests:** fail-first synthetic tests for every mapped high bit, low-bit masking, ignored packed bits, explicit zero, default construction and the complete import path; hash/save-load round trip of imported flags; original 50-file/373-head low/high/full histograms and the 78 nonzero high masks. Assert resulting World players, not only parser output. Existing corpus coverage stays; explain each changed golden. The implementation packet must explicitly decide whether the import semantic change needs a state-stream version change; do not silently add a second flags field. PRNG and wire versions stay unchanged.
- **Gates/stop:** full implementation gates of §1 items 4/9, independent stream oracle, no-game skip behavior, corpus with originals, and named mutations omitting the high reconstruction and dropping the assignment to Player. Commit by concern, report exact heads, then stop. Runtime House remains a no-op.

#### P2.2 — Complete the immutable inputs needed by house calculations · `BLOCKED on reviewed T32.6 input audit`
- **Evidence/current gap:** AnnoGame `[WAREN]` records and post-pass in `population.md` §1; `GoodsInfo` lacks `BauFirst`/`IdSec`; `HouseGroup` omits lookup, goods/infra endpoints and forced-infrastructure data. The audit must distinguish what P2 actually reads from fields first needed by P3/P5.
- **Scope:** add only the audited first-reader metadata, parsed from original data with explicit defaults (including goods `BauFirst=0`), resolved by registry names/config rather than new hardcoded engine counts. Define rejection of malformed/unknown names, ordering and unsupported stage indices without undefined shifts. Derived data must have one reproducible construction and digest policy, documented in the independent stream oracle. Do not import unrelated prices, taxes or research behavior.
- **Tests:** synthetic default/explicit metadata, bad references and non-original table sizes; for any lookup included, direct need, tavern/alcohol alias, infrastructure `IdSec` alias and absent sentinel separately; endpoints with goods-only, infrastructure and empty synthetic tables; original corpus against a separate transcription of the loader rules. Mutating any behavioral input must change the registry digest; derived caches must recompute identically. Full §1 items 4/9 gates and oracle; state/registry version decision pinned in the eventual packet.
- **Stop:** registry/data work only, no live house behavior or new draws. Preserve P1 payload/save and reset warning. The task cannot claim P2 tick completion.

#### P2.3 — First house update, after its policy and effects are bounded · `BLOCKED`
- **Prerequisites:** P2.1 and needed P2.2 inputs integrated; T32.6 reviewed; OQ-450 policy resolved or an explicit approved split that leaves ambiguous paths unimplemented. The orchestrator must issue concrete P2 decisions before coding.
- **Candidate scope:** tested integer calculation kernel and the agreed driver/creation integration for E smoothing, active-need decay, satisfaction/mood and growth. Preserve exact original truncation/order, modular gate and fractional residents. Expose unresolved transitions as scope blockers; silently clamping growth or omitting a reachable upgrade/downgrade/removal/walker draw is not a faithful implementation. Any partial kernel must be named and tested as such, without claiming full house behavior.
- **Acceptance to pin in the packet:** independent synthetic expected values, threshold and 16-bit wrap boundary cases, inactive/active need discrimination, zero E/quantity behavior from evidence, negative/positive growth, widened arithmetic bounds without UB, G7 observed update sequence, save/resync continuation equivalence, deterministic multi-peer hashes and measured tick cost. Mutants for omitted decay, wrong mask, wrong truncation and stamping `last=now` rather than advancing its period must fail named tests. Full gates and explicit stream/PRNG version decisions follow the actual implemented scope.
- **Excluded pending separate stages:** P3 transitions, P4 city unlock/totals and P5 walkers/store delivery unless the readiness review proves an inseparable dependency and the orchestrator rewrites the packet. No automatic expansion.

#### T60 — AI manager post-list call and returned-budget analysis (OQ-270) · `DONE` (round dk; merged as 7d8e922, accepted at ad786453; the AI RE lane is parked for the LAN MVP) · was `ACCEPTED at ad786453 — pending integration` (round db)
- **Assignment/baseline:** second executor only, while the first continues round-cy F5 then T32.6 unchanged. New branch `codex/T60-ai-post-list`, worktree `E:\anno1503-worktrees\anno1503-T60`, from the exact round-cz main/board commit in the packet. Do not use an active or completed task's worktree. T40 is DONE (`1fdc0ff`), OQ-240 answered; this is a new bounded RE task, not a T40 F-fix or T-AI.4.
- **Files:** `docs/re/complogic.md` (new T60 evidence subsection, relevant §4/§8 summaries), `docs/open-questions.md` (OQ-270 and necessary new questions OQ-480..489 only), and append a separate T60 section to `docs/orchestration/REPORTS.md`. No population/state/perf docs, no source/tools/fixtures/goldens/PLAN/TASKS/AGENTS edits. Original modules are read-only. No build, test suite, game launch or Wine. Use existing disassembly/Ghidra evidence and bounded reads; do not launch a broad new Ghidra analysis alongside F5 unless genuinely necessary and within the shared resource budget.
- **Concurrency exception:** the only overlapping tracked path with the first executor is REPORTS.md. Its append-only task sections are permitted in separate worktrees for this packet; neither executor edits the other's section or physical checkout. The orchestrator preserves both append histories at integration. All evidence outputs go to this worktree's own ignored `cache/t60/`; no shared mutable analysis project or output file.
- **Steps, in order; record each before continuing:**
  1. **Inventory the question and module identities.** Read `complogic.md` §§4, 5, 8 and the relevant later receiver/PRNG findings; OQ-240 and OQ-270..272; the original T40 report only as historical evidence. Record the exact DLL filenames/build hashes and inspected branch head. Separate the two OQ-270 questions: effects of the post-list manager call, and the value returned/subtracted by the per-object call. Do not relabel a quotient as time, work or a scheduling budget without evidence for its units.
  2. **CompLogic post-list effects.** Recheck the call site in manager `0x1003d880`–`0x1003d8d0`, then read `0x1003e1c0` through its reachable exits. For each relevant direct callee/virtual call, identify receiver provenance and arguments, effects and unresolved target as needed to classify persistent manager/object writes, world/command-facing calls, list changes and PRNG draws. State the call's condition/order relative to empty/disabled/early-ended list walks from the actual control flow. Trace only helpers necessary to this question; do not expand into nine kind behaviors. An unread indirect target prevents a blanket "no world writes/no draws" conclusion.
  3. **Returned value and aliasing.** Re-read `CompLogic.dll 0x10003a20` through its return (`0x10003ad2` is the recorded load), the caller's pushes/subtraction and `0x10003940` with its actual arguments. Write a stack/argument map normalized to entry SP at each relevant call/return, accounting for prologue pushes, temporary pushes, callee cleanup and this-adjustments. Trace definitions and every relevant write/alias of the returned slot, including pointers passed to helpers. Explain whether the return is unchanged input, remaining amount, consumed amount or another value, only as the evidence warrants. Distinguish the returned dword from the separate loop-exit/remaining-value slot. If a helper or escape cannot be resolved, record the exact call/address and missing fact rather than guessing.
  4. **Classic counterpart.** Recheck manager `0x1003ad90`–`0x1003ade0` and post-list `0x1003b660`, then the per-object entry `0x10002320`. Locate its counterpart to `0x10003940` from actual call targets; do not copy an address/offset from CompLogic. Repeat the receiver/effect and normalized return-slot trace. Publish confirmed common behavior and concrete differences, with independent ranges for both modules. "Same shape" alone is not proof of identical effects.
  5. **Write the contract and resolve only what is proven.** Add a focused T60 subsection: entry/this/argument/return map; post-list effects in order; returned-value dataflow; direct versus unresolved transitive effects; PRNG sites/conditions; CompLogic/Classic comparison. Update §4/§8's "not read" statements with references to this evidence. Update OQ-270 with an answer for each half and each variant, or precise remaining next-address questions. Mark it resolved only when the answers are supported; unresolved transitive calls remain open. Do not change the already established once-per-Advance entry or property-9 load distinction without new contradicting evidence, which must be explicitly reported. OQ-271/272 remain outside scope; a relevant encountered edge may be noted as a next address, not followed into a new task.
  6. **Cross-check and report.** Independently revisit branch targets and stack arithmetic before tagging claims D. Every negative claim must state the inspected call-graph boundary; every unresolved call must have receiver provenance/slot or next target. Check summaries, links and OQ-480..489 heading uniqueness, `git diff --check`, and the three-file limit. Append `## T60 — AI post-list effects and returned value — <date> — status: ...` with exact heads, module hashes/address ranges, commands and ignored evidence locations, both OQ-270 verdicts per module, remaining questions and Decisions and reasoning (choice, alternatives, evidence, why). Keep original code/bytes/disassembly out of git. Commit by concern, using current harness attribution, then stop for review.
- **Acceptance/stop:** both post-list functions and both per-object return paths are read with normalized receivers/stack maps; persistent effects and return semantics are evidenced or bounded by concrete unresolved calls; no broad negative claims from a partial sweep; OQ-270 status and summaries agree; three allowed docs only, clean diff. A bounded investigation may finish with explicit unresolved targets, but cannot call OQ-270 resolved or the whole behavior implemented. No merge/rebase, push, PR, ADR approval, new runtime code, T40 changes or next task.
- **F1 continuation (round da; supersedes initial setup only):** continue existing `codex/T60-ai-post-list` in `E:\anno1503-worktrees\anno1503-T60` at `0e7f2ddd489c4efa8b19d45e8eefb075d0cef96f`. Same three-file scope and parallel isolation. No merge/rebase from main; read round-da's newer instructions from its packet. Resolve F1.1/F1.2/F1.3 above, in order, from the original modules independently. Record eight indexed destinations per module, default path, every direct branch/store/call and state transition, including conditional and no-op paths; delimit code/table/adjacent-function boundaries. Correct the reachable call inventory, receivers, direct versus unresolved effects, §4/§8 and OQ-270 together. Recheck the bounded helper writes and normalized returned-slot proof; retain justified unresolved transitive targets without expanding to nine kind behaviors. Original-case-0 selection uses virtual receivers: describe provenance and missing target, not an invented class identity. Append `## T60 F1 — Indexed dispatcher and direct-effect corrections — <date> — status: ...`, explicitly superseding old case/flag claims while preserving the original report. Include exact starting/evidence/report-parent heads, module hashes, precise address/byte ranges, actual commands, ignored evidence paths, acceptance per finding, and Decisions and reasoning. A final handoff names the final report commit (do not invent a self-referential commit hash inside it). Commit each correction concern separately, then report/stop. Check full diff from `0e7f2ddd`, append-only REPORTS, allowed OQ scope/heading uniqueness, references and `git diff --check`. No build/game/Wine or next task. FINISHED requires all three corrections; OQ-270 may remain narrowed for explicitly bounded transitive effects.

#### T61.F2 — Complete compatibility, strict validation and bounded study evidence · `DONE` (round dp; accepted at 8cc9928, merge d1a8c11; REPORTS "Round dp") · second executor (round dh; closure continuation)
- **Current closure continuation (round dh; supersedes earlier start heads):** resume `codex/T61-join-jitter` in `E:\anno1503-worktrees\anno1503-T61` at `30964c278202bb72a26f6a45541ba0e7173ec4a4`. Round-dh H1–H4 plus all F2.1–F2.5/original gates below are mandatory. Add failing regression tests first for every reproduced final-source counterexample; green old suites alone do not establish acceptance. Retain confirmed callback/mixed-metadata fixes. Complete strict conservation/config, immutable verifiable study evidence, effective diagnostics, metrics and real mutation/equivalence gates before another full study. Append a new F2 closure report explicitly superseding incomplete acceptance/diagnostic/provenance claims; preserve historical reports and report actual measured statuses. Same scope/bounds and stop boundary, no new task.
- **Current continuation (round dg; supersedes the starting head only):** resume the existing branch/worktree at `d8b287ccdf3d0b2399c4cabdf0620d2b6645a2d8`, not 42ba698. Follow round-dg findings and all F2.1–F2.5 steps below. Preserve the already passing 31 legacy fixtures while closing mixed-evidence omission. Add immutable data/ACK/stale/refused identity at scheduling and propagate it through both transport modes; stale timers are not packet outcomes. Run failing regressions first. All remaining runner, metric, manifest, full-suite, mutant, baseline/determinism and fresh-study gates remain mandatory. No further permission is needed for these listed in-scope edits; preserve checkpoint evidence if the session ends before completion.
- **Continue/scope:** existing `codex/T61-join-jitter`, `E:\anno1503-worktrees\anno1503-T61`, from `42ba698042c0ecf679ee6ab2541dc23a75e592ff`. Same six files as T61: checker, its tests, study runner, only the new experiment/current jitter references in ADR 0003, OQ-371 only, append REPORTS. No new branch, main merge/rebase, broad checker redesign or snapshot-checker edits. Main/packet provides instructions only. Preserve all still-applicable original A–G and F1 requirements. No first-lane or completed-worktree changes.
- **F2.1 — reproduce and classify before edits.** Preserve stdout AND stderr, exact command/exit/source head for the 221-test failure. Build a 21-row failure inventory: 14 hand-written link fixtures, the unmutated mutant-table gate, and six real recovery/rejoin/reassembly/accounting cases. Identify root causes, distinguishing validator regressions from actual simulation failures. Independently run small cases against `917efea` and `42ba698` in ignored copies. Add regressions for every review counterexample (including ones outside the failing 21) and run them before fixing; label unsupported API failures separately from semantic failures. Do not delete valid fixtures, relax expected success or disable validators to get green.
- **F2.2 — complete evidence compatibility and identity conservation.** Define one explicit wholly-legacy-zero-jitter evidence rule applied consistently by both link and jitter validation. Missing metadata may be accepted only under that rule; partial, contradictory, nonzero or supplied invalid evidence still fails. Give every data/ACK transmission and outcome a reconstructible immutable identity in serialized AND un-serialized modes, including stale and refused records, epoch/channel/attempt or ACK ordinal. Validate both directions: each served in-horizon transmission has exactly one legitimate outcome, and every outcome is consumed exactly once by its own transmission at the derived time. Reject extra wrong-time duplicates, orphan outcomes, outcomes for lost/rejected/pending transmissions, and forged identities even when the original valid outcome remains present. Preserve legitimate late originals/retries and pending-beyond-horizon behavior. Validate stale epoch meaning from raw transitions, not a self-certifying stale label. Index once without mutation/aliasing of shared lookup lists or quadratic rescans. Keep existing order/window/retention/queue/reassembly/grant/outage checks active. Test all config identity fields and types (bool is not an integer here), ranges and duplicates, producing deliberate ValueError diagnostics rather than incidental formatting/unpacking exceptions. Keep scheduling/PRNG/event semantics unchanged; added audit fields use an explicit legacy projection retaining every original event/value/order.
- **F2.3 — real runner supervision and verifiable manifest.** Implement an owned controller with at most one simulation worker and enforce 300 s per row and 5400 s for the whole study while it runs, including validation/metrics/hashing/output. Use injectable short deadlines and tiny stalled-worker tests; do not wait minutes to test failure. On deadline reap only the owned worker, preserve a structured partial/error record and failed evidence, stop expansion and complete independent reporting. Check the aggregate deadline before starting any next row. No worker may remain running after failure. Preserve all original workloads/horizons/profiles/seeds/defaults and 4096 MiB headroom. Declare diagnostics as metadata, never a fake jitter-script entry; exercise --diagnostics on small cases. Validate the exact Cartesian set, unique identities, 0..12 predeclared diagnostics and no unknown row kinds; complete required types, finite nonnegative durations, terminal status and actual explicit validator results. Independently derive canonical config from the expected workload/profile, include the full effective config/loss/delay/horizon, recompute hashes rather than trust labels, and require a single study/source/config manifest with Python/OS, code head, source hashes, run ID and timing provenance. Bind result/trace digests to available evidence under a documented schema; state exactly what summary-only validation can prove and retain raw evidence needed for claimed semantic revalidation. Fabricated or mixed-source/config rows must fail. Report timeout separately from protocol restart/drop/incomplete-at-horizon.
- **F2.4 — metrics and full correctness gates before study.** Reconstruct membership and applied cursors at initial seating, input_on, input_off as relevant, drop and rejoin; honor input_on.applied and sample membership transitions as well as apply events. Give independent numeric vectors for maximum seated lag versus horizon lag, loading peers and same-timestamp ordering. Add peak retained turns/bytes/ownership and distinguish queued network traffic from generic scheduler events; verify metrics against raw evidence. Run the complete join AND snapshot suites on the current code, existing mutant table unmutated and mutated, real J1–J8 behavioral/validator mutants, fresh-process determinism with differing hash seeds, and zero-jitter baseline comparisons against `917efea` across all originally named rows/rates. No claim that two new-code runs equal an old-source comparison. Fix in-scope failures and rerun affected gates; do not stop simply because the first full run finds ordinary fixable regressions. A truly out-of-scope behavior conflict requires a minimal reproducer and precise question. Source/test/doc changes must not weaken accepted transport invariants. Commit correctness, indexing equivalence, supervision/manifest/metrics and evidence by concern, not one unexplained omnibus fix.
- **F2.5 — final matched study and reporting.** Only after F2.1–F2.4 pass, commit the final tested source and run a fresh bounded 72-row matrix plus predeclared diagnostics (at most 12, include required repeat/asymmetric case). Declare whether any pilot belongs to that run; pilot/diagnostic work cannot bypass the aggregate bound. One simulation/test job at a time; no concurrent own full suite and study. Independently validate the manifest/results, compare repeat and baseline projections, retain all unfavorable valid outcomes, and record actual wall time plus per-phase timing/scaling. Any code change invalidating source/config/result provenance requires affected gates and a consistent fresh final study, not stitched rows from old heads. Update current ADR/OQ-371 claims without rewriting historical results or adopting a timer/ADR. Append `## T61 F2 — Validated outcomes and bounded study completion — <date> — status: ...` with exact baseline/fail-first/final tested/report-parent heads, source/config/artifact hashes, 21-failure disposition, every regression/mutant/gate result, phase timing, study and diagnostic inventory, evidence paths (correct stderr location), remaining blockers and Decisions and reasoning. Confirm six-file/OQ scope, append-only REPORTS, git diff --check and clean branch. FINISHED requires all A–G/F1/F2 acceptance; then stop for review, no next task or merge/push.

#### T61 — Deterministic jitter, independent arrival validation and retry study (OQ-371) · `DONE` (round dp; F2 closure accepted, merge d1a8c11)
- **Baseline/files:** new `codex/T61-join-jitter`, `E:\anno1503-worktrees\anno1503-T61`, from the packet's exact round-dc board commit. Allowed tracked edits: `tools/join_activation.py`, `tools/test_join_activation.py`, new `tools/join_jitter_study.py` for a reproducible bounded study, `docs/adr/0003-network-transport.md` only the new experiment section and directly affected current jitter cross-references, OQ-371 only in `docs/open-questions.md`, and append your own `docs/orchestration/REPORTS.md` section. No general checker rewrite, snapshot-checker edits, C++/versions/fixtures/goldens/PLAN/TASKS/AGENTS edits, accepted RE/OQ changes or policy adoption. T60/T32.5 stay unmerged; the two executor lanes share only append-only REPORTS in separate checkouts.
- **A — Baseline and contract, before implementation.** Read ADR 0003 §§3–4, 8.7–8.8, 10 and Open choices, OQ-371/372/373, `Cfg`, send/ACK/link paths, event ordering, validators and existing T58 tests/rows. Run and save baseline `python -I -B tools/test_join_activation.py -v` and `python -I -B tools/test_snapshot_catchup.py -v`, with counts/exits. Capture canonical event/result evidence for the existing named zero-jitter rows; include all un-serialized ROWS and serialized LINK_ROWS at both rates, one at a time. Record source/config hashes and preserve failed logs. Specify integral units, defaults, exact keyed sampling algorithm/version, seed and explicit scripted schedule semantics, supported identity fields and validation errors. Declare bounded nonnegative additive propagation jitter after service (not service-rate jitter), permitting network arrival reordering while preserving existing per-channel application rules. Distinguish labeled data-delay fixtures from new jitter; keep their existing meaning. Exact code/task assumptions, alternatives and reasoning go in the report before coding.
- **B — Regression-first implementation.** Add tests before changing the checker and run them against the pinned baseline in an isolated scratch copy. Distinguish unsupported-new-API failures from actual behavioral failures; import/syntax failures do not prove a scenario. Implement opt-in jitter on every data attempt (all channels, including retries and controls), ACK-only datagram, both host directions, serialized and un-serialized modes, and any direct fallback route. Keys must include directed endpoints, channel, epoch and transmission type; data keys distinguish logical datagram/sequence and attempt, ACK keys distinguish the actual per-channel ACK ordinal. A retransmission has its own sample; unrelated scheduling/order, process hash randomization or another channel must not silently change a given identity's sample. Do not use wall time, Python's randomized `hash`, mutable global RNG or a shared draw cursor for this contract. Reject malformed/negative ranges and ambiguous script identities. Document precedence if scripted values override keyed samples. Lost/rejected datagrams must retain stable identities without shifting samples of survivors; do not change queue/drop/outage semantics. Jitter-off and all-zero schedules preserve canonical legacy events/results; added audit metadata may be excluded only by a small documented projection that keeps every old event/value and ordering.
- **C — Independent validation and lifecycle.** Log sufficient immutable transmission/config identity, sample and time evidence to reconstruct every expected arrival. Independently recompute queue service, declared delay, jitter and callback rounding from input/config and raw events, never from simulator channel state or a logged expected-arrival value. The validator must not call the scheduler's sampling/arrival routine as its only oracle; use independent derivation and hand-calculated fixtures/golden vectors. Cover ACKs when serialization is off as well as on. Bind an arrival/ACK/stale/refusal outcome to its own transmission attempt, channel and epoch. Reject missing, duplicate, swapped, out-of-range or forged timing evidence; a self-consistent forged sample and arrival must fail against the declared profile. Preserve pending-beyond-horizon accounting and distinguish lost/queue-rejected transmissions from missing arrivals. Keep order/window/agreement/retention/reassembly/grant/outage validators active. Do not increase lag bounds, deadlines, caps, attempts or retry defaults to silence a counterexample; record a genuine model limitation with a minimal reproducer if it exceeds the jitter feature's scope.
- **D — Adversarial and mutation gates.** Exercise independently calculable cases: ACK-only delay across a retry boundary; data reorder with ordered delivery intact; different retry samples including late original versus retry; simultaneous events at/just before/after a resend boundary; asymmetric ingress/egress; duplicate/coalesced ACKs and channels with overlapping ordinals; stale epoch arrival after rejoin; queue rejection and lost packets; an arrival just past the horizon; invalid configuration. Include unchanged zero-jitter comparison, same-seed repeat, different-seed observable difference and two fresh Python processes with different hash seeds. On final code catch these named mutants separately: J1 ignore ACK jitter; J2 reuse attempt-1 jitter for retries; J3 omit epoch/channel from sample identity; J4 force zero jitter on the un-serialized path; J5 round a sub-millisecond serialized arrival down; J6 forge both logged jitter and arrival consistently; J7 swap two attempts' arrival identities; J8 erase an in-horizon served arrival/outcome. Each mutant must fail a named behavioral/validator assertion, not import/setup/timeout; independent protection layers may require a targeted probe, never a claim that a surviving masked mutant was caught. Run the existing mutant table too, restore all changes, then pass unmutated cases.
- **E — Matched study, not timer adoption.** Implement a checked-in runner with explicit CLI/profile/config manifest and machine-readable results; large traces stay ignored under `cache/t61/`. Exactly 72 primary rows: rates `100M` and `1M`; retry candidates 300/400/500 ms; existing workloads `one_fragment`, `endless02`, `fragment_loss`; profiles zero, keyed uniform integer [0,25] ms seed 6101, the same with seed 6102, and keyed uniform integer [0,100] ms seed 6101. These ranges/seeds are synthetic stress choices, not LAN estimates. Apply each nonzero profile to data and ACKs in both directions. Preserve existing scenario definitions, fragment identity/loss schedule and per-row horizons: 12,000 ms for short rows; endless02 250,000 ms at 100M / 650,000 ms at 1M. A change of timer is the only policy change between matched timer rows; retain profile identity and report naturally differing traffic. Add at most 12 diagnostic/repeat rows chosen before running; include exact repeats and at least one script-driven asymmetric/reordering case. Do not omit unfavorable/incomplete rows or shorten the corpus to claim completion. Print completion/activation or restart/drop reason, horizon and pending traffic, first/retry/ACK bytes, queue/loss counts, peak queue/reassembly/retention ownership, maximum seated lag over time and lag at horizon separately, and every validator result. Derive metrics from evidence; `sens_line`'s current horizon lag alone is not a maximum. Record model time separately from host wall time, Python/OS, tested head and config/result hashes. No measured-LAN or universal timer-winner claim.
- **F — Resource and verification bounds.** One simulation/test process at a time; release per-row traces before the next row; retain summaries and failed-case evidence. Preserve the existing 4096 MiB headroom policy and owner applications. Use the existing budget/monitoring approach or measured safe scheduling; do not run the study beside your own full-suite jobs. Operational bounds for the study, not engine limits: 5 minutes host wall time per primary row and 90 minutes total study wall time; no hidden horizon/validator reduction if exceeded. Mark timed-out/resource-blocked rows and stop study expansion, continue independent documentation/verification, report the unfinished gate honestly. Required final gates: complete join and snapshot Python suites; old and new mutants; zero-jitter baseline equivalence; final 72-row manifest/results with validators; deterministic fresh-process checks; `git diff --check`; documented file/OQ scope. The Python-only change requires no MSVC/CTest/ASan game build; never claim those gates were run. Run `python -I -B tools/join_jitter_study.py --help` and a small reproducible CLI invocation as usability checks.
- **G — Report, concern commits and stop.** Commit separately by coherent concern: contract/regressions, jitter scheduling/evidence, independent validators/adversarial coverage, study runner/results documentation, final report (combine only when necessary to keep a coherent working change and explain why). Append `## T61 — Deterministic join jitter and matched retry study — <date> — status: ...` with exact baseline/final tested code/report-parent heads, baseline/fail-first/gate counts, mutant-to-test outcomes, result manifest/hashes/commands, all row outcomes and blocker classifications, acceptance A–G, and Decisions and reasoning. Update ADR experiment/current jitter references and OQ-371 without rewriting historical zero-jitter results or changing Proposed/OQ-373 decisions. FINISHED requires the implementation, independent checks, mutation gates and complete study; a resource/policy blocker stays explicit and prevents a blanket FINISHED. Commit report, name its final hash in handoff, and stop. No merge/rebase, push, PR, production networking, original-game execution, ADR approval or next feature task.
- **F1 continuation (round dd; overrides original new-worktree setup):** continue `codex/T61-join-jitter` in `E:\anno1503-worktrees\anno1503-T61` at `de5cfa9bb0cf4d0dd23ede89f4bb1890eef342da`. Read the newer board via the packet, not by merging main. Reproduce F1.1–F1.5 with bounded probes before fixing. First restore zero-jitter valid-fixture coverage and add missing identity/outcome/config regressions; optimize the indexed raw-event analysis with frozen small-input equivalence, recording intentional stricter validator verdicts separately from unchanged simulation events. Then fix runner completeness, true wall-time enforcement, evidence retention and membership-aware metrics; use injected small clock/deadline tests rather than a deliberate six-minute hang. Complete original regressions/mutants/determinism/baseline compatibility gates before spending the full study budget. Pilot the formerly slow row only within the unchanged 300-second guard, then rerun all 72 primary combinations on final tested code under a fresh logged 5400-second study budget; declare diagnostics (at most 12) and whether the pilot is included before starting. Diagnostic/pilot time is not an unbounded bypass of the aggregate limit. One simulation worker at a time; retain 4096 MiB headroom. An owned controller plus one worker is permitted for hard deadlines; terminate only that worker on expiry, record partial status, and stop expansion. Finish independent docs/gates if blocked, but not FINISHED. Same six tracked files and policy restrictions. Commit correctness, performance, runner/metrics and evidence by concern; append `## T61 F1 — Bounded validation and complete jitter study — <date> — status: ...`, with exact old/final tested heads, phase profiles/scaling, unchanged-event comparisons, intentional validator corrections, full gate/mutant table, 72-row manifest/provenance, timeouts and Decisions and reasoning. Preserve original report text and explicitly supersede inaccurate metric/completion claims. Report and stop; no limit increase, merge/rebase, push or next task.


**Current executor order (round de):** part A T10c F6 from `5dc0556`, then independent part B T32.6 F1 from `cfd85ec`, on the existing branches/worktrees. An honestly reported F6 blocker does not block the audit. Finish both reports and stop; no main/task merge or rebase, push, P2 implementation, TASKS/AGENTS edits, ADR approval or extra task. T40 is DONE.

**Second executor order (round dh):** T61 F2 closure at `30964c2`, H1–H4 and all outstanding inherited requirements; final correct gates before a fresh study, then report/stop. First-lane F6/T32.6 F1 unchanged; no extra task, policy/ADR approval, merge/rebase or push.

**Waiting for the owner:**
- ADR 0003's design/default approval remains pending. T52 F7 supplies the bounded protocol oracle. T58 adds opt-in serialization; F1 published the matched 300/400/500 ms comparison and real fragment-loss coverage; F2 closed the early-arrival, retry-size and vanished-arrival holes; F3 tied the receiver-side outcome records to closure and staleness evidence and added the fragment payload rule; F4 justified the reassembly-overflow record that closure evidence relies on and stated the seat-drop and forgery boundaries; F5 made a closure record bind its receive side. T58 is merged (round cm, `723029c`); the seat-drop receive-channel path stays open in the checker (OQ-320). The retry timer is an owner choice (OQ-373), informed by that comparison. Jitter and a measured link (OQ-371), assumed-delivered asks/epoch lifetime and adoption of OQ-373 limits/policies remain open. Owner approval alone does not resolve technical gaps;
- OQ-250, the UDP port;
- the seven questions of `docs/sim/world-scaling.md` §15;
- the feel questions of `docs/sim/rules-presets.md`.

### T1f — Chunk build: profile, exact field shortcuts, progressive delivery · items 1-2 `DONE` (merged as ae8b936 on 2026-10-21, see §4) · items 2b, 3, 4 `READY after T11 stage 1 is merged` (they edit `frame.cpp` / `scene.cpp`; see the Order) · branch `task/T1f-chunk-build-speed` (continue on it after merging `main`; worktree `C:\Users\altin\anno1503-T1f`)
- **Evidence:** the T1d report measures 100-250 ms per chunk on one worker (a cold frame of 12 chunks 118-247 ms with 16 threads); the perf README of 2026-10-18 measured about 12 ms per 512² chunk on one core, and a river chunk of the canyon island 56-83 ms. Something in between (the river lattice, the enhanced sea per chunk, the apron probes, below-normal thread priority against a busy window thread, memory bandwidth with 16 builders) made chunk building about ten times slower, and no margin can hide a view that crosses the screen in a quarter of a second (the T1d report: a 5000 px/s pan still shows open sea for hundreds of frames).
- **Owner decision 2026-10-20 ("tier 3 now"):** the sprite passes move to the GPU in **T11**; this task keeps what T11 does not touch: the water field, shore distance, terrain and flow passes (orchestrator's main suspect for the ten-fold growth: island properties recomputed per chunk at chunk resolution, `water_field_from` with `blurred_alpha`, `chamfer`, `blend_rivers_into_sea`, and one `terrain_at` call per pixel) and the delivery latency. Items, one commit each: (1) the profile table (below) first, it decides the rest and feeds T11's design; (2) **redefined 2026-10-21 after the profile and the executor's memory finding** (a per-pixel field per island is about 700 MB for the canyon island at group 0: not acceptable, and PLAN 0.1 forbids a design that only works for small islands). The field is expensive only where it can change: the coast band and the river beds. So: per island and zoom group, once, in the background and tiny (tile resolution or a coarse pixel grid, a few KB per island), classify each cell as **deep land**, **deep sea** or **band** by a conservative bound on the spline and noise terms (cells whose every pixel provably gets the trivial value); the chunk builder evaluates the full per-pixel spline and noise only for band cells and writes the trivial values elsewhere. **Pixels identical, digests unchanged** (the bound must be a proof, not a tolerance: write it into `docs/render/iso.md` with the terms it covers). The same classification gives the terrain stage its shortcut (per-tile corner heights rasterised per tile instead of `terrain_at` per pixel, exact). Not option 3 (coarse precompute with interpolation): it changes pixels and would need an owner decision; not per-island storage at any group. Expected: the band is O(coast length × band width), so the field cost no longer scales with the island's area; measure the band share per island and group in the README; (3) progressive delivery: a chunk is handed over in two stages with two serials, ground and fields first, objects later, so land appears first and the T1e dissolve hides the second step; (4) a `kChunk` 256 experiment behind a config value (it is a `constexpr` today: make it a `FrameRenderer` option, then measure latency per chunk, chunks per frame, draw calls and p99 at 2560x1440 for 256 and 512); the owner picks the default from the numbers.
- **Profile first:** one chunk build per stage (`render_chunk` stages: tile cull, object cull, sprite decode, ground, objects, sea field, terrain, flow lattice, apron) on the canyon island and on `Single_24.sze`, groups 0 and 2, one thread and 16 threads, with the existing timing hooks or a `bench_frame --chunk-profile` option; write the table into `docs/perf/README.md` first, then fix the two largest items. Candidates to check, not assumptions: the sea field built per chunk from scratch, sprite decode without the read-write cache hit, the lattice bake per chunk, false sharing or allocation in the builders, builders at below-normal priority losing the CPU to the window thread's spin on a 500 Hz display.
- **Item 2 outcome (2026-10-21):** 50.1 to 29.1 ms (canyon, group 0, one thread), 44.2 to 21.4 ms (`Single_24`); both targets missed (field + terrain 33-57 % of a chunk; the 25 ms not reached). Orchestrator decision: **accepted and closed as it is.** What is left is the sea and coast pixels inside the island grids, whose depth byte needs the exact spline plus noise at every pixel: that is the per-pixel CPU design itself, and T11 section 10.1 replaces it on the GPU. No SIMD work on the band pixels: it would be thrown away with T11 and cannot be bit-exact against the scalar reference without its own proof.
- **Item 2b `DONE` (merged as 92257ae on 2026-10-21, §4).** Follow-up, the first commit of item 3: a CI test of the bounded blur and chamfer (see §4). **Item 2b as specified:** blur and chamfer only within the bounding box of the river sprite pixels plus the kernel radius, the executor's estimate 2-3 ms on river chunks (the worst chunks, up to 21 ms of river stages, are what the owner sees as late chunks). Only with the same kind of proof in `iso.md` (a finite blur kernel is exactly 0 outside the box plus its radius; for the chamfer, show what every consumer does with a distance beyond the box, e.g. a clamp, so the values it reads are identical) and the golden digest; if the chamfer proof does not close in an hour, do the blur alone and say so. Then **the 16-thread rerun** of the item 1 and item 2 tables on an idle machine (not measured yet: a game was running), folded into item 4's 8 against 16 workers measurement.
- Acceptance (revised 2026-10-21 with the profile; item 2 closed below target, see above): the per-stage table in the README before/after; the exact removal of the per-pixel field from the CPU is T11's section 10.1 (a GPU field pass from island data, after the GPU objects pass); item 3 also stops the double build of river chunks (the viewer's workers deliver a river chunk without its lattice and build it again when the flow arrives: deliver the lattice as its own stage, or wait for the flow in the worker when it is a few ms away); item 4 adds one measurement: time to a full cold view (24 chunks at 2560x1440) with 8 against 16 workers, since SMT makes each chunk 1.6-2 times slower at 16 (latency against throughput; the default worker count follows the number); the sprite stages are reported as they are (T11 takes them); the GPU = CPU tests and the digests unchanged; no new limit.

### T11 — GPU-built chunks: the sprite passes on the GPU (owner decision 2026-10-20, "tier 3 now") · stages 0-1 `DONE` (stage 1 merged as daaa849 on 2026-10-21, §4) · **stage 2a merged as 9f54fec (PR #17, 2026-10-22 f); `OWNER-CHECK`** (the viewer check), then the per-chunk dispatch timestamps (correction 2026-10-23 j: this header still said `IN PROGRESS`) · was: PR #17, reviewed 2026-10-22 c. The compute pass is bit-exact on both adapters. Open: derived levels, CPU build on atlas overflow, the pipeline created at start, the B4 measurements, the viewer check, stand-in tests with both builders, mutation evidence, G1–G4. Then OWNER-CHECK. · branch `task/T11-gpu-chunks` (continue after merging `main`; worktree `C:\Users\altin\anno1503-T11`)
- **Goal:** a chunk's ground and objects pixels are no longer composited on the CPU. The CPU produces a *draw list* per chunk (sprite frame id, position, depth order, colour table, alpha level, shadow flag, lift: a few thousand instances, microseconds), and the GPU renders the list into the chunk's ground and objects planes in a render pass from sprite atlases. Everything downstream (chunk cache keyed by `ChunkKey`, compositor, stand-ins, prefetch, dissolve) stays as it is: a chunk is still "textures on the GPU", only their producer changes. Why now (owner): chunk builds of 100-250 ms are the limit behind every pop-in; per-sprite work belongs on the GPU (CLAUDE.md); and the sim will move thousands of sprites per tick (carriers, civilians, animals, ships), which static chunks cannot hold, so one GPU sprite path serves both the static world and the moving one instead of two paths.
- **What exists and must be reused, not duplicated:** the compositor already has a growable sprite atlas (`GpuLimits::atlas_size` 2048, `max_atlas_layers`, `Impl::Atlas` in `gpu.cpp`), a sprite pipeline (`sprite_vs` / `sprite_fs` in `gpu.wgsl`) and per-frame instanced sprite draws for ships and soldiers above and below the waterline (`Single_24` draws 396 sprites, within 1 per channel of the CPU path, atlas growth tested). T11 generalises that path to the chunk content. The CPU blitter (`iso.cpp`, `docs/render/iso.md` "Blitting", `docs/re/annodisplay.md`: signed `z` depth offsets, alpha levels, 5/8 shadows that compound, `H`/`V`/`M`/`W` brushes, `BSH_RGBWZ` column expansion and the 50 % checkerboard, no width clipping, signed `i16` anchors, player colour tables, per-corner elevation) stays the reference and the oracle; whatever it does the shader does, pinned by tests.
- **Stage 0, design, written into `docs/render/gpu-chunks.md` before any code, reviewed by the orchestrator (report it as a `REVIEW` of its own):** (a) the draw list format (one fixed-width record per instance; what the ground pass needs: tile frames with per-corner heights; what the objects pass needs: painter's order key, shadow before sprite, alpha level, colour table index, water/above-water split as today); (b) atlas residency: all 28,518 building and 725,424 figure frames cannot be resident (count the bytes per group from `docs/render/iso.md` "Fidelity": about 78 M covered pixels for buildings at group 0 alone), so residency is per scene and lazy, like a glyph cache: frames referenced by the draw lists of the chunks in `keep` are resident, with an LRU and a configurable budget in MB (default from the adapter's limits, reason in the comment); a chunk whose frames are not all resident yet is built after their upload, never with holes; measure how many frames a scenario references per group (`Single_24` as the largest) and put the numbers into the design; (c) the render pass: render-to-texture into the chunk's ground and objects planes (and the `base` plane), with the painter's order realised by draw order (one instanced draw per chunk per layer, instances sorted on the CPU), shadows as a blend state or a second pass, alpha levels by instance; where the compositor needs the `terrain` and `field` planes those still come from the CPU (T1f item 2) until a later stage moves them; (d) scheduling: chunk GPU work goes into the same frame's command encoder under a budget (`GpuLimits`, chunks per frame and ms, reason stated); the window thread never waits; the CPU draw-list build runs on the existing `ChunkWorkers`; (e) the CPU reference remains: the `--png` path, the digests and `anno-dump` stay the CPU compositing; a GPU-built chunk equals the CPU chunk within 1 per channel on every plane, tested per group on `Single_01`, `Single_24` and the canyon island, plus a synthetic scene that exercises shadows over translucent sprites, the checkerboard, compounding shadows and negative anchors (the mutation survivors of the earlier compositor review); (f) the fallback: without a GPU (headless, CI, the CPU path) chunks are built as today; the switch is a `FrameRenderer` option, default GPU in the viewer once the owner accepted it; (g) limits: atlas size and layers, residency budget, chunks per frame are config with reasons; no fixed arrays, no narrowing of ids (PLAN 0.1); (h) what moves later: the sea field and terrain planes (compute shader from island data), moving units from the sim (the same draw-list records, per frame instead of per chunk), so the design names those extensions without building them.
- **Stage 1:** atlas residency and the draw-list builder on the CPU, with a test that the draw list of a chunk reproduces the CPU chunk when replayed through the CPU blitter (so the list is proven before any shader work). **Stage 2:** the GPU render pass for the ground and objects planes, GPU = CPU tests, `bench_frame --chunks gpu` with the per-stage numbers, then the **owner check** on the canyon island (pan fast, zoom with the wheel) and on `Single_24.sze`. **Stage 3:** the cache and prefetch paths take GPU-built chunks (the `serial` rule unchanged), stand-ins source from them, measurements of a 5000 px/s pan (the case T1d could not cover) and of the cold frame; `docs/perf/README.md` table: chunk latency on the CPU path against the GPU path per group, resident atlas MB, frame p50/p95/p99/max at 1280x800 and 2560x1440 with the pan-and-zoom script, on the RTX 4090 and on the Radeon iGPU.
- **Late-game stress fixture (owner question 2026-10-21, "does it hold in a large late-game multiplayer match"):** stage 3 adds a synthetic scene generator (`fixtures/` code, no original files committed; the scene is built at test time from the game dir): every island of `Single_24.sze` covered with dense city blocks (tens of thousands of buildings, every building type present), thousands of soldiers and ships, so the sprite count per chunk is several times the original scenarios' maximum of 3,608. `bench_frame` runs it at 2560x1440 with the pan-and-zoom script, CPU builder against GPU builder: chunk latency, frame p50/p95/p99/max, resident atlas MiB, on both GPUs; the perf README states the per-chunk GPU time as a function of overdraw (not of instance count). The acceptance below must hold on that scene too.
- Acceptance: a visible chunk is on screen within 20 ms of its request on the 4090 at 2560x1440 (list build + atlas upload of missing frames + the pass), within 50 ms on the iGPU; no busy frame above 8 ms; `open_sea_chunks` 0 at the 5000 px/s pan; GPU = CPU within 1 per channel on all three scenes and the synthetic one; the CPU path untouched in output; no new fixed limit.

### T14 — The compositor on weak GPUs (the Radeon iGPU) · stage 1 **`DONE`** (merged as c46bc81 on 2026-10-23, PR #27, CI green; acceptance item 3, the 8 ms iGPU frame, missed and left to stage 2; §4) · stage 2 `LATER` · was `IN PROGRESS` (round j, orchestrator subagent, worktree `C:\Users\altin\anno1503-T14`; T3 stage 3 and T11 stage 2a are merged) · was `READY after T3 stage 3` (owner decision 2026-10-21 n: "Agree with your recommendation"; it edits `gpu.cpp` / `gpu.wgsl`) · branch `task/T14-weak-gpu` from `main`
- **Stage 1 (round j) = Do items (1) and (2):** attribute the cost, then bound the area-proportional passes. Item (3), the quality tiers, is stage 2: it needs the owner's side-by-side look before a default is set.
- **Stage 1 acceptance (written before work starts):**
  1. per-pass GPU timestamps (sea shading, coast, wave simulation, rivers, chunks, resample, anything else measured) on the Radeon iGPU **and** the RTX 4090, at 1280x800 and 2560x1440, scale 1 and the T3 worst far-zoom points, both water styles, in `docs/perf/README.md` (p50/p95/p99/max, adapter, commit);
  2. every pass whose cost grows with the world area shown, not with screen pixels, named from that table and bounded at far zoom (cells per screen pixel capped, or coarser, or off below a level, with the reason); the cap is a config value with the measured default, not a literal;
  3. the same table after the change; the target is no GPU frame above 8 ms on the iGPU at 1280x800 at any zoom; if stage 1 alone misses it, say by how much and which pass is left for stage 2;
  4. the CPU reference path and the GPU still agree (the existing GPU = CPU tests green on both adapters; a new test for any changed pass); the look at scale 1 is unchanged (digests unchanged), and the far-zoom change is shown in PNGs under `C:\Users\altin\Pictures\anno-review\T14\` (before and after, both adapters, the ten `0200Single` scenarios at the far levels plus the canyon island), looked at by the subagent (§1 point 10);
  5. gates of §1 item 4 (render: MSVC clean, GCC traps avoided, `tools/check_unix_warnings.sh` over the changed files), the GPU tests named with the adapters they ran on;
  6. any visible change at far zoom is an owner question (OQ) with the before/after PNG paths; a REPORTS.md section, a PR.
- **Why:** T3's measurements (`docs/perf/README.md`, "T3 stage 3 fix 1") show the GPU frame with enhanced water at 3.6 ms (1280x800) and 13.4 ms (2560x1440) on the Radeon iGPU at scale 1, and 72-195 ms when zoomed out, against 0.1-4.7 ms on the RTX 4090. The game must run on integrated graphics (PLAN 0.3: modern hardware, not only the top end).
- **Do:** (1) a per-pass GPU timestamp breakdown (sea shading, coast, wave simulation, rivers, chunks, resample) at the T3 worst points on the iGPU, so the cost is attributed before anything changes; (2) the wave simulation window and every pass whose cost grows with the world area shown (not with pixels) bounded at far zoom (the simulation's cells per screen pixel capped, coarser cells or off below a level, with the reason); (3) a quality tier chosen from the measured cost, as the river shading tier is (`river shading: high`): cheaper sea shading on a slow adapter, the composed-pixel budget (`--zoom-max-upscale` set automatically from measured ns per pixel), each tier GPU = CPU tested; the owner sees the tiers side by side before a default is set. Acceptance: no GPU frame above 8 ms on the iGPU at 1280x800 at any zoom, and the 2560x1440 figure stated.

### T5 — Finish river flow stage S7 on `wip/rivers-s7` · `READY` · `PARALLEL-OK` (worktree; touches `src/render/src/river*`, `gpu.wgsl`, `src/tools/river_qa`, `docs/render/rivers.md`)
- **Context:** `docs/render/rivers.md` sections 11, 15, 16 and the S7 acceptance list there; `docs/open-questions.md` OQ-071…OQ-090; the stage workflow that did S4–S6 is described in `docs/PLAN.md` §7 item 0(b).
- **Do:** `git worktree add ../anno1503-T5 wip/rivers-s7`; rebase onto `main` (the gap patch `2f7b40d` touched `sea.cpp`, expect small conflicts); run the four gates; review the diff against the S7 acceptance in rivers.md; fix; split into small commits on `task/T5-rivers-s7`. Everything of S7 that changes the look must sit behind the existing options with the current defaults (the owner just approved gain 4 / median speed / gap patch 8 and must not see a different picture by default).
- **Do not touch** `src/app/src/viewer.cpp` zoom code or `frame.cpp` composition (T1 owns them); if S7 needs a change there, report it and stop.
- **Evidence:** the S7 tests (`river_extras_test.cpp`) with mutation checks by the reviewers; `river_qa` sheets for `D_256x256_0045_5B`, one Nord and one Steppe island at all three groups, before/after, under `C:\Users\altin\Pictures\anno-review\T5\`; default-picture digests unchanged (the enhanced scene digests must not change when the options are at their defaults; if they must, say why).
- **Acceptance:** rivers.md S7 acceptance met, gates green, `docs/render/rivers.md` section 17 written as "S7 as built" with deviations and the owner decisions it needs (as OQ candidates, not decided).

### T6b — Survey follow-ups · `READY` · `PARALLEL-OK` · tiny · branch `task/T6b-survey-fixes` from `main`
- `tools/river_qa_gap.py:244` has a mangled line (`... h0))             if boxes else (0, 0, w0, h0)`): reformat. `docs/render/rivers.md` 13.1 describes `E comps` as "water that touches the original's river sprites"; the code's E is `W && (R || O)` (all river-flagged water plus water under a sprite): rewrite the sentence. Add one sentence that `edge px` inherits the chunk-border bias of `near_water` (pixels outside the 512-px chunk count as water) and that the 17.5 % share double-weights identical template variants (Piraten/Indianer/…), or dedupe them in the summary.

### T6c — Gap patch that deepens only real gap cells (OQ-068, owner decision 2026-10-20: option 3) · `READY` · `PARALLEL-OK` (not at the same time as T5: both touch `src/render/src/sea.cpp` / `river*`) · branch `task/T6c-gap-patch-cells` from `main`
- **Decision:** the viewer keeps `--river-gap-patch 8` until this exists. Target: the same connectivity result as patch 8 (gap px 0, components 339 at group 1, no regression on any of the 72 templates) with the water outside the painted bed back at the patch-0 level (about 6 % at group 0, 5.8 % at group 1) and no water tails at open river ends.
- **Design before code** (write it into `docs/render/rivers.md` as a short section with the rule and its evidence, then implement): instead of deepening every trench corner of every piece, find the cells where the water test actually fails inside a painted bed (the one-corner-wide trench corners at level + 1 that the original's `z + 10` depth test accepts) and deepen only those; join pieces only where the painted beds of two pieces face each other within N tiles along the river direction (not at open ends: an end without a facing piece is a source or a cliff, and gets no tail). Keep `gap_patch_tiles` as the join distance; add the mode as a `RiverConfig` field with the current behaviour selectable (the owner approved it on the canyon island, so it must stay reproducible).
- **Evidence:** rerun the T6 survey (`python tools/river_qa.py gap-patch`) for 0, 8 and the new mode; the table goes into rivers.md 13.1; contact sheets of the canyon island and the five worst T6 islands before/after under `C:\Users\altin\Pictures\anno-review\T6c\`; the enhanced digests change only where rivers are (say which scenes). Then `OWNER-CHECK` on the canyon island and on `N_256x256_0007_2B`.

### T10c — Faster tests within a measured memory budget · `DONE` (round cx; merged locally as `42b6738` after P1; F4 continuation and Part B accepted at report tip `5b10ed3`)
- **F4 continuation (round cv; completed and accepted in round cw, historical assignment):** continue `codex/T10c-test-memory` in `E:\anno1503-worktrees\anno1503-T10c` from `1104a7d0a9a2422b430afdc80b1579c2835a1eaa`. P1's branch stays at `416993fa`. Steps 1–3 change nothing under `src/`, `tools/`, `cmake/` or any `CMakeLists.txt`.
  1. **The A/B repeat pair (F4 Part A step 5).** Run A3, then B3, with `cache\f4\ab\run_legacy_ab.ps1` and the same two binaries as A1–B2 (A: the F2 scratch build in `cache\f4\ab\f2-src`; B: this worktree's Release `anno_render_tests`), ports 47851 and 47852, `-Threads 4`, nothing else running. After each run, stop only that run's coordinator, by its port. Record the same columns as A1–B2.
     - **Verdict rule (one-sided; replaces step 5's "within 5 %"):** a pair counts when neither of its runs logged a `still waiting` line. Write "no throughput regression measured" when at least one pair counts and every counted pair has B ≤ 1.05 × A. If a counted pair has B > 1.05 × A, report all six numbers as a QUESTION and stop before Part B. If A3 or B3 waits, do not repeat again: the verdict rests on A2/B2, and the report says so.
  2. **Perf note** (`docs/perf/README.md`):
     - Add A3 and B3 to the F4 A/B table, with a column "counted". Replace "QUESTION" with the verdict and the rule.
     - End the F3 timing paragraph with a pointer to the F4 A/B, which measured the gap, instead of "host variance".
     - Add `ANNO_TEST_COORD_LEAK_FD` and `ANNO_TEST_PROBE_LEAK_FD` to the "Test-only hooks" list, each with its process, its effect and its case.
  3. **REPORTS:** a new section `## T10c F4 continuation — A/B repeat and docs — <date> — status: …` with: the heads; Decisions and reasoning (§1 item 12); the six-run A/B table and the verdict; the output of `git diff --stat 1104a7d HEAD`, which must list only `docs/` files, so that Part A's gates stand for this head.
  4. **F4 Part B** (below) on the head after step 3, only if step 1's verdict is "no throughput regression measured". This condition replaces Part B's "every Part A gate has passed". Part A's gates at `1104a7d` count as "on the combined head" for P1 while the diff from `1104a7d` lists only `docs/`; if anything else changed, re-run them first.
  5. **Commits,** one concern each: the perf note; the continuation report; then Part B's commits. No merge, rebase, push, PR, TASKS/AGENTS edits, ADR approval, P2, T34 stage 2 or T-AI.4.
- **F4 continuation acceptance:**
  - The six-run A/B table with a "counted" column, and a verdict under the one-sided rule.
  - The three perf-note fixes, and the continuation report with its diff-stat.
  - F4 Part B and its acceptance (below) on the resulting head.
- **F4 (round cu; Part A reviewed and accepted in round cv except the A/B verdict; Part B runs as continuation step 4):** continue `codex/T10c-test-memory` in `E:\anno1503-worktrees\anno1503-T10c` from `94cf3cb0a09969339401f7bcd8f4a73e4cb62375`. Read nothing of main beyond the packet; do not merge or rebase. P1's branch stays at `416993fa`. Keep everything F2 and F3 fixed (C1–C4, D1, D2) and their regressions. The packet's instructions give the files, functions, commands and expected output for every step.
  - **Part A — T10c F4.**
    1. **Fail-first regressions** (bounded; isolated ports 47820–47839 and journals; real coordinator and probe processes; no real RAM exhaustion). Each fails at `94cf3cb` for the reason named, then passes:
       - (a) **E1, orphan kept:** C1 admits P's 50 on budget b and dies (P alive); C2 replays it and admits Q's 20; Q releases; C2 dies; C3 on the same journal must count 50 pending while P lives. At `94cf3cb`: 0.
       - (b) **E1, live hold kept:** as (a), but Q keeps its 20 and P exits while C2 runs; after C2 dies, C3 must count Q's 20 while Q lives. At `94cf3cb`: 0.
       - (c) **E2:** two threads of one process wait on one unconnected file budget that a busy coordinator refuses; one `cancel()` ends both with `cancelled` within a stated bound (one connection attempt plus one pause). At `94cf3cb` the second thread does not end within the bound; the case fails at the bound with a message and never hangs.
       - (d) **E3, one observation per layer:** N2, the server alone answers an exact-build hello of another build with `kStReject` and reason `kRejectBuild` (a raw-hello probe mode, without the client's re-check); N3, the coordinator's listening and accepted sockets carry `O_CLOEXEC` (`/proc/<pid>/fdinfo`, WSL); N4, a descriptor the coordinator opened without CLOEXEC (a test-only hook) is absent from the meter child (WSL meter path); N4b, a descriptor the client process opened without CLOEXEC is absent from the coordinator that the client spawned (Linux/WSL run). These are new cases, so "fails at `94cf3cb`" applies to each with its layer removed, which is the mutation run of step 4.
    2. **E1 fix.** A successor never issues a hold id that appears in the journal it replayed (an `A` or `R` line), for any budget, and ids stay unique within one process. Choose either one server-wide counter seeded above the largest replayed id, or a seed per budget at its creation; give the reasoning. The journal format is unchanged. `kCompatVersion` = 3 (round cu decision), with the reason in its comment and in the perf note; the pin test says 3, and the "incompatible" case uses `kCompatVersion + 1`, not a literal.
    3. **E2 fix.** `session_for` reads the cancel generation before it waits for `connect_mu`, so a cancel issued while a thread waits for the mutex ends that thread too. State the cancel latency bound (one connection attempt, at most `ANNO_TEST_BUDGET_LIVENESS_MS`, plus one pause, at most the recheck period) in the perf note.
    4. **Mutation table on the final head, re-run in full:** M1–M9 and N1–N9 of F3; N2, N3, N4 and N4b each on their own (step 1(d)); the double mutants N2b and N34 as extra rows; new M-E1 (successor ids restart at 1) and M-E2 (generation read after the lock). POSIX-only mutants run in WSL with the build's exit code checked; the rest on MSVC. Each mutant fails a named case; every unmutated case passes. A mutant that survives is a finding: strengthen the case, or report it as a QUESTION with the reason, never as "masked".
    5. **E4, throughput A/B.** Build the F2 code (`43cd10f`; its code equals `c95f819`) Release `anno_render_tests` in an ignored scratch copy. Run the 4-worker legacy-river case four times, alternating F2 code and the final head (A B A B). Run each on its own coordinator port with no other test process at the same time, and stop only that run's coordinator afterwards. Record each time, each run's budget-wait lines, free memory at the start, and the coordinator build and compatibility version from its log. If both final-head runs are within 5 % of the F2 run next to them, write "no throughput regression measured". Otherwise stop after Part A and report the four numbers as a QUESTION.
    6. **E5:** the gate table gets a column with the coordinator build and compatibility version from each gate log. Add D1/D2/E1/E2 acceptance item by item, and a paragraph on how two worktrees of different builds share one budget (same version) or wait for each other (different versions). Correct report item 3 and the fail-first row labels in the F4 section (do not edit the F3 section). Fix the perf-note blank line and add a heading for its timing paragraph.
    7. **Gates on the final head:** every gate of F3 step 5 (budget suite on MSVC and under WSL ASan/UBSan; full CTest with and without game files, after a full build; GCC 13 / Clang 18 warnings; clang-format 23.1.3; `lint_sim.py`), plus steps 4 and 5 above.
    8. **Commit** one concern at a time (E1 with its cases and the version; E2 with its case; the layer cases and hooks; docs), then REPORTS `## T10c F4 …` with the §1 item 12 "Decisions and reasoning" list.
  - **Part B — P1 steps 7–8 on the combined head.** Start only when every Part A gate has passed; otherwise stop after Part A.
    1. The P1 step 7 gates that Part A does not already give on this head: `python -I tools/sim_stream.py --check`; the WSL GCC 13.3 and Clang 18 `-O3 -Werror` builds of `sim` and `formats` with their tests, run with and without the game dir. Part A's full CTest, clang-format and `lint_sim.py` on the same head count for P1; say so.
    2. P1's six mutations (P1 step 7), each with its named failing test. A survivor gets a stronger test (its own commit) and the affected gates again; if P1 code changes, the full CTest with and without game files runs again on the final head.
    3. P1 step 8: REPORTS `## P1 — House payload, group table, scenario import — <date> — status: FINISHED (validated on the combined head <sha>)`, naming P1's base `416993fa` and the combined tested head, with every gate's result, the mutation table, the corpus summary line and P1's acceptance item by item. Results on the combined head are not attributed to P1 alone (round cp).
    4. Commits go on `codex/T10c-test-memory`; P1's branch stays at `416993fa`. Then stop.
  - No merge, rebase, push, PR, TASKS/AGENTS edits, ADR approval, P2, T34 stage 2 or T-AI.4.
- **F4 acceptance:**
  - E1: a journaled promise survives two restarts in both orders (cases 1(a), 1(b)); `kCompatVersion` is 3 with its reason.
  - E2: one cancel ends every waiter of a budget within the stated bound.
  - Every mutant of step 4 fails a named case on the final head, and every unmutated case passes.
  - The A/B shows no throughput regression, or the numbers are reported as a QUESTION.
  - The report has the build and version per gate log, the acceptance item by item, the two-worktree paragraph and the corrections.
  - Every gate passes. Part B: P1's gates and six mutations on the combined head, and P1's FINISHED report.
- **F3 (round cs; reviewed in round cu, historical; D1 and D2 accepted):** continue `codex/T10c-test-memory` in `E:\anno1503-worktrees\anno1503-T10c` from `43cd10f086eb6907373608c054c7598385393605`. Read PLAN and this board on main without merging/rebasing it. P1 stays at `416993fa`. Keep everything F2 fixed (C1–C4) and its regressions.
  1. **Failing regressions first** (bounded; isolated coordinator endpoints; real clients; no real RAM exhaustion). Prove each fails at `c95f819`, then passes:
     - (a) Two builds with different build ids and the same compatibility version share one busy coordinator. The second build's client is served, and both builds' reservations count against the one budget.
     - (b) A requester of an incompatible compatibility version, or a gate that asks for the exact coordinator build, while the coordinator is busy. It waits cancellably and ends with a bounded diagnostic that names an infrastructure blocker, not a product failure. It never throws at once, never stops an active peer and never opens a second, independent budget.
     - (c) The handover after (b): once the busy coordinator has drained, the requested coordinator takes over without losing a journaled promise.
     - (d) Coordinator death with a real child process spawned the way the meter is spawned. No descriptor of the coordinator stays open in the child, and the port is free when the coordinator exits. A replacement binds at once. A client that connects during the handover waits or retries within its bound with a clear diagnostic; it never dies with "speaks another protocol".
     - The orchestrator's reproducers are references only: probe E in `E:\anno1503\cache\orch-cs\run_review_f2.py`, plus `run_holder.py` and `run_d.py`. They are built from a copy of the `43cd10f` sources by `build2.sh`. Do not edit that folder; copy what you need into your worktree's ignored `cache\`.
  2. **D1, three identities** (the round-cr clarification, F2 step 3).
     - (a) The build id stays provenance only: logged in every gate log, never a compatibility test by itself.
     - (b) Add an explicit compatibility version covering the wire format **and** the reservation, journal and recovery semantics. Write in a comment, and in the perf note, which changes must bump it. A test pins its value.
     - (c) The incarnation stays the lost-promise fence.
     - The hello carries all three, and the server decides compatibility by (b). Exact-build validation is an explicit opt-in for the gate that tests the coordinator itself; name it in the perf note. Then that gate waits for drain/handover as in 1(b).
     - Replace or rewrite the case "C3: a coordinator from another build is not used" to match.
  3. **D2, no inherited descriptors.**
     - Create every coordinator socket, listening and accepted, close-on-exec (`SOCK_CLOEXEC` / `FD_CLOEXEC`).
     - Make every `posix_spawn` of a helper close everything but its stdio: the meter in `coord_server.cpp`, and the client's start of the coordinator in `coord.cpp`.
     - Check that the Windows paths create no inheritable handles (`CreateProcessW` with `bInheritHandles = FALSE`, non-inheritable sockets).
     - A connection that is accepted but answers no hello within the liveness bound is "coordinator not ready". It is retried within the request's bounded wait, not thrown at once.
  4. **Nit:** under mutant M1, the C1 case "a release that leaves its bytes allocated does not admit on an old reading" hung until killed after 60 s. Bound its own wait so that a regression fails with a message.
  5. **Gates on the final head:**
     - the budget suite and every mutation, old and new, each caught and each unmutated case passing;
     - Windows full CTest with and without game files, under the memory-budgeted overlap;
     - WSL ASan/UBSan for the budget suite;
     - GCC 13 / Clang 18 warnings, clang-format, `lint_sim.py`.
     - Re-run the 4-worker legacy-river case once to show no throughput regression. The full one-worker comparison is not repeated, because admission arithmetic does not change.
     - Record the coordinator build and compatibility version from each gate log.
  6. **Commit and stop.**
     - Concern-sized commits, then REPORTS with: the exact heads; the fail-first and mutation results; the gate totals; D1/D2 acceptance item by item; and how two worktrees of different builds now share one budget. Add the §1 item 12 "Decisions and reasoning" list: the version's value and bump rule, the opt-in name, the retry bound, the fd-closing method, and each regression's design.
     - No task-branch merge/rebase from main, push, TASKS/AGENTS edits, ADR approval, P2, T34 stage 2 or T-AI.4.
- **F3 acceptance:**
  - Different compatible builds share one budget while the coordinator is busy.
  - An incompatible or exact-build request waits and ends with a bounded infrastructure diagnostic, then takes over after drain without losing a promise.
  - After coordinator death no child holds its port; replacement and client reconnection are bounded and never fail with "speaks another protocol".
  - C1–C4 stay fixed with their regressions.
  - Every gate passes.
- **F2 (round cr; reviewed in round cs, historical; C1–C4 accepted):** continue `codex/T10c-test-memory` in `E:\anno1503-worktrees\anno1503-T10c` from `af8aa01fae68daf4d2e6afb0a6301ba1085bf7fc`. Read PLAN and this board on main without merging/rebasing it. P1 stays at `416993fa`.
  1. **Failing regressions first.** Port the C1-C3 review probes into bounded automated tests with isolated coordinator endpoints. Cover release with retained allocations while a waiter has an old reading; an allocated owner whose residency report is stale; external pressure/recovery; conflicting host/WSL memory readings; coordinator loss with memory/CPU waiters and already admitted work; replacement while old promises remain. Distinguish permanent capacity from temporary unavailability. Prove failures on `af8aa01`, then pass with fixes. Keep the earlier retained-memory, owner-death, cancellation and cross-OS tests. Add named mutations for the new safety invariants and show the same tests pass unmutated. No real RAM exhaustion or arbitrary sleeps as the correctness oracle.
  2. **C1, make admission use coherent current capacity.** Fix `coord.cpp` (`view_for`, `pump`, release/heartbeat handling), `admit_detail.hpp` and the memory-query boundary as needed. Every grant must account for outstanding unallocated promises and current usable host memory without crediting released cached bytes. A stale per-request sample cannot authorize a grant on another client's release. Explicitly combine host physical/commit constraints and any WSL-local limit; do not substitute guest availability for host availability. Missing/stale readings fail closed or wait for refresh. A temporary shortage waits/rechecks with cancellation and a bounded, clearly reported diagnostic outcome; a truly oversized request is separately identified. No timed forced admission and no revived blanket heavy-job lock. Test both the in-memory model and real Windows/WSL clients.
  3. **C2/C3, handle coordinator failure safely.** Remove the nested mutex acquisition; pending calls and cleanup must finish within a tested bound on connection loss or a nonresponsive server. Fence a replacement from admitting against promises issued by its predecessor until those promises are recovered or the old work has safely drained/stopped. A fail-closed recovery protocol is sufficient. Include a coordinator incarnation/protocol compatibility check so a detached daemon from another build is not silently used as the tested implementation. Record the actual coordinator build in gate logs. Tests must not kill unrelated coordinators or task processes.
     **Mid-task clarification:** keep three identities separate: (a) build/source ID for provenance, (b) an explicit compatibility version covering both wire format and reservation/recovery semantics, and (c) server incarnation for lost-promise fencing. Different application/test revisions, or different coordinator builds with explicitly supported semantics, may share one host budget. A source hash mismatch alone must not fail an ordinary compatible test run. Unsafe/unknown semantic versions must not be declared compatible merely because their packet layout matches. For a genuinely incompatible version, or a gate explicitly validating an exact coordinator build, wait cancellably for safe drain/handover with a bounded diagnostic outcome; timeout reports an infrastructure blocker, not a product assertion failure. Do not terminate active peers, start independent real-memory budgets per build, or bypass compatibility. Exact-build coordinator tests must actually exercise that build; using another compatible build is allowed for ordinary client tests only with its identity recorded. Add regressions for two different compatible builds sharing reservations, an incompatible/exact-build requester while the coordinator is busy, and safe handover without lost promises. This is a clarification of C2/C3 and the parallel-testing policy, not a new task. Append the behavior and limits to REPORTS.
  4. **C4, establish a schedulable measured workload.** After synthetic safety checks pass, audit setup/catalog ownership and actual final geometry in `render_corpus_test.cpp`. Account explicitly for shared catalog lifetime when reservations change; distinguish retained process bytes from fresh task costs, and bound pre-admission allocations. Calibrate the exact corpus path, not only a fresh viewer path, with a reasoned margin. Keep 4096 MiB headroom and coverage/digests unchanged. If the remaining large job demonstrably exceeds that budget, a bounded storage reduction with exact pixel-equivalence tests is in scope; no GPU rewrite, weaker estimate, skipped scenario or golden rewrite. Report a remaining blocker rather than forcing admission.
  5. **Gates and comparison on the corrected head.** Run budget regressions/mutations, Windows full CTest with/without game files, applicable WSL ASan/UBSan (budget, changed renderer paths and P1 sim), GCC/Clang warnings, formatting, lint and oracle gates. Preserve logs/sampler traces per run. Measure the same 47-assertion legacy-river workload with one worker and budgeted overlap; report wall time, peak private/commit, minimum free memory, estimates and actual overlapping work. An isolated benchmark does not reinstate an exclusive scheduling policy. State heavy-ASan coverage/omissions explicitly. Do not repeatedly rerun a refused heavy suite without a diagnosed change.
  6. **Commit and stop.** Concern-sized commits plus performance docs and REPORTS with exact code/coordinator heads, P1 base, fail-first/mutation results, gate totals and C1-C4 acceptance. No task-branch merge/rebase from main, push, TASKS/AGENTS edits, ADR approval, P2, T34 stage 2 or T-AI.4. The orchestrator reviews before integration.
- **F1 (round cq; historical assignment, reviewed above):** continue the existing branch/worktree from `d8c3461b9738b0955e191abdbca24ce59c740c84`. No merge/rebase from main; read this board on main. P1 stays at `416993fa`.
  1. Add failing regressions for R1-R3 before the fix. Cover retained allocations between jobs, reused/foreign process identity, live Windows plus WSL clients, owner crash with blocked memory/CPU waiters, cancellation and missed/external wakeups. Use bounded deterministic synchronization and isolated coordinator files; no real RAM exhaustion. Prove the regressions fail at `d8c3461`, then pass with the fix; keep the named mutation evidence.
  2. Correct admission accounting so old retained memory is not credited to newly unallocated work. Distinguish the memory quantities actually measured, update per-client accounting safely, and preserve headroom across concurrent processes. Unknown readings must not silently authorize unbounded work. Validate both the in-memory model and real file/coordinator clients.
  3. Provide real cross-Windows/WSL coordination: owner incarnation/namespace, host memory limits, exclusion, signaling and crash recovery. A single host-owned coordinator is one option; sharing a file plus local PID checks is not sufficient. Test simultaneous real clients and owner termination. Periodic liveness/memory rechecks may wake waiters; they must never force admission when the budget does not fit. Keep budgeted overlap and genuine shared-resource locks.
  4. Correct the estimation boundary: cover scene/setup costs, actual final render bounds and small group-1 regions. Include pre-admission allocations in the accounting or bound their concurrency. Measure representative large and region workloads after the buffer changes; use a reasoned margin rather than treating the ceiling of one sampled ratio as a proof. Retain reduced buffers and exact corpus digests. Add synthetic equality coverage for omitted ground pixels and cropped masks at edges/overlaps/empty inputs so CI without game files checks the changed behavior.
  5. Run the relevant sanitizer suites in WSL, using an isolated Unix Makefiles build with `ANNO_SANITIZE=ON` if Ninja is absent, or another documented equivalent. Include test_support, changed CPU render paths and P1 sim coverage; measure/configure sanitizer memory overhead. Run required Windows with/without-game gates and Unix warning/runtime checks on the corrected combined head. Do not rerun unsafe legacy concurrency or expand into unrelated gameplay work. Report exact tested revisions and any remaining gate failure.
  6. Update performance docs and REPORTS with corrected units, source/dirty-state provenance, concurrency and peak measurements, sanitizer results and itemized R1-R5 acceptance. Commit by concern and stop for review. No merge/push, TASKS/AGENTS edits, ADR approval, P2, T34 stage 2 or T-AI.4.
- **Initial assignment (round cp, retained for context):**
- **Branch/worktree:** new `codex/T10c-test-memory` in `E:\anno1503-worktrees\anno1503-T10c`, explicitly from P1 `416993fa6a6f9dc821455df5d414cff5833ac7f0` (exception to the usual main base). Read PLAN and this updated main board first; do not merge/rebase main into the branch. P1's branch stays untouched. T10c-only commits follow the P1 base and remain independently reviewable.
- **Scope:** `src/test_support/`, CPU render allocation paths/tests under `src/render/`, CTest configuration in root/library CMake files and presets, a reproducible sampling/scheduling tool under `tools/` if needed, `docs/perf/README.md`, relevant test/render docs, stale memory instructions in `CLAUDE.md`, REPORTS. No sim behavior, golden updates, reduced coverage, game files or GPU algorithm changes.
- **Steps:**
  1. **Measure first.** Preserve the stopped P1 logs. On the unchanged base, run the named legacy-river digest case (formerly test 229) with one internal worker to establish a controlled baseline, then representative large full-map renders. A one-worker measurement is experimental isolation, not the new scheduling policy. Sample at about one second: available physical memory, system commit/headroom, test PID working set/private memory, active case/worker count, elapsed time. Use unique logs per run/worktree; record hardware, head, commands and baseline memory. Do not reproduce the unsafe eight-worker run merely to show failure.
  2. **Regressions before the scheduler fix.** With injected memory readings and deterministic synchronization, cover concurrent claims on the same free budget, differently sized jobs, reserve preservation, a job larger than capacity, exception/cancellation release and waiting without forced admission. Include cross-process coordination where implemented; avoid tests that exhaust real RAM or depend on arbitrary sleeps.
  3. **Budget before allocation.** Replace the 1-GiB assumption for full-map work with conservative per-item estimates from dimensions/live buffers, calibrated to the measured peaks. Reserve/release admission capacity atomically and leave configurable host headroom (start with 4 GiB for this PC and justify adjustments). Do not double-count already resident reservations when consulting available RAM. Remove first-worker and wait-timeout bypasses for budgeted work; an impossible job reports clearly rather than silently exceeding capacity. Admit smaller jobs when they fit while ensuring large jobs eventually progress. Coordinate process-level and internal-worker budgets: independent CTest/worktree/Windows/WSL jobs must not each claim the same host headroom. Choose a small, testable coordinator/resource scheme and document its supported scope; uncontrolled external allocations are measured, not claimed to be reserved.
  4. **Allow useful overlap.** Replace the blanket all-CPU-slot/exclusive-heavy scheduling with resource-aware admission; keep genuine GPU/shared-resource locks. Show at least two eligible jobs overlapping when their combined budget fits. Keep CPU limits configurable to avoid multiplying CTest jobs by full-size worker pools. Emit and flush admission/start/end/estimate/peak information during the run, including reasons for waits or refusals. A renewed low-memory threshold stops admissions and, if necessary, the diagnostic run; investigate before any changed rerun instead of blindly repeating it.
  5. **Reduce expensive temporary storage in a separate commit.** Start with discarded full-size ground canvases and full-size river-mask/blur buffers; measure and implement a bounded improvement where exact behavior is preserved. Consider strips/tiles only with correct filter margins and pixel-order equivalence. Preserve every existing digest and both water styles; no rebaselining or omitted scenarios. Record remaining larger redesigns rather than expanding into a GPU rewrite.
  6. **Validate correctness and speed.** Run scheduler tests/mutations, unchanged render corpus digests, and the full with-game/without-game CTest gates on the combined head. Run required MSVC, GCC/Clang, formatting, lint/oracle and applicable sanitizer checks for changed libraries and P1. Existing 1032 passes remain historical evidence; test-support/render changes require fresh relevant/full gates. Compare the same representative workload under safe isolated scheduling and budgeted concurrent scheduling: total wall time, peak private/commit use, minimum available RAM, actual overlap and waits. Report measured speedup or regression honestly; no unsafe baseline, forced three-full-run stress loop, or acceptance defined by the absence of legitimate waits. If timings are noisy, repeat the bounded benchmark, not unrelated passed gates.
  7. **Report and stop.** Separate T10c changes from P1 gate completion in REPORTS. Give P1 base, every T10c commit, final tested head, commands/totals, memory/time measurements, caught mutants and unresolved limits. P1's version-115 goldens and corpus expectations remain unchanged. Commit each concern and the report, then stop for orchestrator review; no merge, rebase of main, push, TASKS/AGENTS edits, ADR approval or next feature task.
- **Acceptance:** unchanged coverage/digests; measured peak supports estimates and headroom; no unreserved forced admission; concurrent jobs demonstrated within the shared budget; memory trace survives interruption; combined P1/T10c gates reported against their actual head; throughput comparison documented. Any unmet criterion is explicit, not relabelled as complete.

### T9 — Sim stages S10–S15 · S10 **`DONE`** (merged as 0ebfeb2, PR #25) · S11 **`DONE`** (merged as 9393718 on 2026-10-23, PR #26, CI green; §4) · S12 **`DONE`** (merged as a727eb7 on 2026-10-23, PR #28, CI green; §4) · S13 **`DONE`** (merged as 7c13f3b on 2026-10-23 n, PR #30, CI green; §4) · S14 stage 1 **`DONE`** (merged as 17a17fa on 2026-10-23 r, PR #34, CI green; §4) · S14 stage 2 **`DONE`** (merged as 4635935 on 2026-10-23 t, PR #36, head a711a2a, CI 10/10; §4; was `REVIEW → rebase` in round s) · **S15 stage 1 `READY after T29`** (round v: the 1-hour determinism run, no freeze; one subagent; below) · the S15 freeze `LATER` (after the population stages) · was S15 `LATER` (after T24)
- **S15 stage 1 — the 1-hour determinism run, no freeze (round v).** Why: M3's acceptance is a 1-hour headless run with equal hashes on Windows and Linux and across two runs (PLAN M3). With transport in, it can be checked now: the heaviest file runs an hour of game time (1 800 000 ticks at 500 ticks/s) in about 13 s. Every later stage (population, the float walk) is then checked against it. `kStateHashVersion` stays a development counter; the freeze (version 2) waits for the population stages. Branch `task/T9-s15-determinism` from `main`; worktree `C:\Users\altin\anno1503-T9s15`; new OQs from **OQ-200** (to 202).
  - **Steps (in order; §1 item 7):**
    1. **Set up**; build; the sim tests green with the game dir (the count into REPORTS.md).
    2. **Checkpoints:** if `anno-sim-replay scenario` cannot print the state hash at intermediate ticks, add `--checkpoint N` (the tick and `state_hash` every N ticks), with a CTest case without the game dir for the option's usage and refusal. Commit `feat(sim_replay): scenario --checkpoint`.
    3. **The runs:** all 50 `.sze` with `--ticks 1800000 --checkpoint 150000`, twice each on the MSVC Release build, on a WSL GCC 13.3 `-O3` build and on a WSL Clang 18 `-O3` build (the CI flags of §1 item 4): six runs per file, through the heavy-job lock, one at a time.
    4. **The table** in REPORTS.md: per file, whether the 12 checkpoint hashes are equal across the six runs (yes / no), and the run time per platform.
    5. **A mismatch:** the first checkpoint that differs; then halve the interval down to the first differing tick, and name the scope (the `state_hash` scopes show which one differs). A `BLOCKED` entry with that; no fix in this task.
    6. **Docs:** `state.md` §15 "S15 stage 1 as implemented", the run times in `docs/perf/README.md`, REPORTS.md with the acceptance ticked, PR. No `TASKS.md` edit.
  - **Acceptance:** (1) all 50 files equal at every checkpoint on all six runs, or per mismatch the first differing tick and scope; (2) the `--checkpoint` test (if added); (3) gates of §1 item 4 through the heavy-job lock; (4) no stream or timeline version change; (5) PR, not merged.
- **S13 (island sweeps, growth, drought machine)**, as written in `state.md` §15 S13, §5.4 and §5.8: `grow_chunk` (the chunk visit of the growth sweep, D `0x1008b5f0`, clipped to in-bounds tiles, A14/F20), `drought_tick` (the machine of §5.8, D `0x1008aeb0`, plateau / expanding / retreating with the catch-up gate), the `city_mask` widened to words sized from `city_slots` (O-55), the `drought_tiles` seam called once per strip with the strip's rectangle, G5. The spec names the file `island_tick.cpp`; the current `island_tick` lives in `scheduler.cpp` and may move there (one commit, no behaviour change, before the feature commits).
- **S14 stage 1 (the scenario adapter and the corpus)**, as written in `state.md` §10 (the adapter paragraph: "`scenario_formats.cpp` (stage S14) is the only adapter"), §14.5 `scenario_test` and §15 S14; evidence `docs/formats/sze.md`, `docs/formats/insel.md`, `docs/re/annoscene.md` §2, §6, §8 (`STADT2`). Nothing in it draws from the PRNG, so `kPrngTimelineVersion` stays 1; the unnamed computer players' random `COMPSPIELER_NAME` (annoscene §1, a load-time draw of the original) is listed in §6 as a timeline gap, not drawn.
  - **Do:**
    1. `src/sim/include/anno/sim/scenario_formats.hpp`, `src/sim/src/scenario_formats.cpp` (the only sim file that includes `formats::sze` / `formats::insel`, `state.md` l.33): `load_scenario(formats::Bytes file, const TypeRegistry&, const SimConfig&) -> Result<LoadedScenario, std::string>` with `LoadedScenario{ScenarioSpec spec; std::vector<Warning> warnings;}`; a `Warning` has a stable kind (an enum: `UnknownRecord`, `UndecodedCity`, `IslandLink`, `UnknownObject`, `BuildingWithoutCity`, …, data not a limit: add kinds as needed) and a path like the loader's (`islands[2].objects[17]: …`). **Rule: decode what the file carries with evidence (D or P in the specs); everything else becomes a warning with the record name and offset, never a guess.** A refusal (`Result` error) only for a file that is not a scenario or whose decoded parts contradict each other (two islands in one slot, a city number twice).
    2. The mapping, each line with its evidence: `SCENE/HEAD` → `WorldSettings` by the byte map of §10 (D `0x10009cc0`; prop14 from the root name), width/height from `[0..7]`; `SPIELER` header series → `PlayerSpec` (kind, money, name, colour per `PlayerHead`, annoscene §6; `kind == 2` is player 31, D) with research empty; `INSEL5` → `IslandSpec` (rect and number from the head; climate from `INFO[0]` as annoscene §5 step 2 reads it; `Terrain` from `HEIGHT` / `GFXID` / `FRUCHTBAR` columns, the shape of OQ-060, a missing column empty; `GELAENDE`, `FLUESSE` and the `PROD2` items that are **tile objects** (no list in the layout) → `TileObjectSpec` with `phase()`; `BERGWERK2` → `DepositSpec` with the stand-in amount and `ERZE` goods as the spec says, else a warning (O-37); the `PROD2` items that are **buildings** (a list in the layout) → `BuildingSpec` of the city that owns the tile, when a city is known); `STADT2` (sze.md: `u8` city number, `u16 x, y` on its island; the owning player is the `SPIELER` block's index) → `CitySpec{no, owner}` with no stock and no live state (O-36: `PROD` records undecoded → `UndecodedCity` warning per city); a building whose tile belongs to no known city → `BuildingWithoutCity` warning, not placed; `INSELLNK` → an `IslandLink` warning per link (stage 2 resolves them; the slot stays null); every other record → `UnknownRecord` once per name. Owner runs: from whatever the file states (if `STAEDTE` or a tile-owner record is decoded with evidence; else every tile neutral and a warning naming the record that would hold it).
    3. The corpus test `src/sim/tests/scenario_test.cpp` (`ANNO1503_GAME_DIR`, skipped otherwise with the one message): every `Scenes/**/*.sze` (50) → `load_scenario` → `World::create` → 5 simulated minutes (150 000 ticks at 100 ms advances) → `check_invariants` clean; the run twice from the same spec gives equal hashes; save/load/save identity; no `UnknownObject` warning for a building type (every `PROD2` id resolves in the registry); the count of islands with `w % chunk_edge != 0 || h % chunk_edge != 0` printed (A14/F20; expected 0, a non-zero count is a report to the owner, not a failure); a table of warning counts per kind printed and the per-file totals asserted against a committed expectations file `fixtures/synthetic/sim/scenario_corpus.txt` (file name, islands, cities, buildings, tile objects, deposits, warnings per kind: **counts only, no content of the files**), so a regression in the adapter shows as a diff; the 5 add-on and 5 base scenarios with the most buildings run 20 000 ticks more under UBSan (the asan preset; a `heavy` suite case if it needs the memory). The output prints file names and counts only.
    4. `anno-sim-replay scenario <file.sze> [--ticks N] [--step-ms M]`: loads with the game dir's registry, prints the warnings table, the counts, the hash after `create` and after N ticks, and `check_invariants`; exit 1 on a refusal. A CTest case without the game dir checks the usage and the refusal of a non-scenario (a synthetic fixture); with the game dir one scenario.
    5. Unit tests (`scenario_formats_test.cpp`, no game dir): synthetic `.sze` fixtures built in the test or in `fixtures/synthetic/sze_synthetic/` (ours; the existing ones may serve): the HEAD byte map (every byte → property, the `prop1` 0 → 1 rule), a player series, one `INSEL5` island with a `HEIGHT`/`GFXID` grid, a tile object and a building, a `STADT2` city; each warning kind reached by one fixture; a contradiction refused with its path.
    6. Stream: `ScenarioSpec` is not state, so the stream version moves **only** if a field of the world changes; if the adapter needs none, `kStateHashVersion` stays 109 and no golden moves (say so in REPORTS.md). If a `WorldSettings` or `Island` field is added for something the file carries, bump with the reason and re-baseline as before.
    7. Docs: `state.md` §10 "As implemented in S14", §6 (the `COMPSPIELER_NAME` draw as a gap), §15 "S14 stage 1 as implemented" (the corpus numbers: files, islands, cities, buildings, the A14 count, the warning table), §14.5 updated to what the test does; `docs/formats/sze.md` / `insel.md` status lines ("read by the sim adapter"); new OQs for what the corpus shows and the specs do not answer (numbers from OQ-141); PLAN M3 status; REPORTS.md; a PR.
  - **Stage 1 acceptance (written before work starts):**
    1. Do 1–7 done; the corpus case passes on this PC (50 of 50 load; name in REPORTS.md how many create a world with at least one island, how many hold only links); `scenario_corpus.txt` committed and its numbers quoted in REPORTS.md.
    2. Every mapping line in `scenario_formats.cpp` cites its evidence (D address, or P with the spec section); a byte the specs do not explain is a warning or an OQ, never a default that pretends to be a reading (grep the file for "assume" and "guess": none).
    3. Tests: the unit cases of Do 5 (each warning kind reached at least once; a mutation that drops one kind fails its case), the corpus case of Do 3, the tool case of Do 4; mutation evidence: the HEAD byte map shifted by one byte fails the unit case and the corpus expectations; a city number read as the player index fails the `STADT2` case; the A14 count forced to 0 fails nothing only if the corpus has none (then say so).
    4. No `reinterpret_cast`, no exception, `Result` errors with the path; `lint_sim.py` clean (the adapter is in `src/sim` and obeys the lint; the one float the units carry is not read); `anno_sim` still links only `anno_formats`.
    5. Gates of §1 item 4 through the heavy-job lock: MSVC build; ctest with and without the game dir (the corpus case in the foreground; its memory noted); clang-format 23.1.3; WSL GCC 13.3 and Clang 18 `-O3 -Werror`; the asan preset for the 10 UBSan scenarios (WSL Clang, or the CI job if the WSL Clang cannot run them: say which).
    6. Scalability (PLAN §0.1/§0.3): no fixed island, player or city count in the adapter; the per-island tile build uses the `Executor` that `World::create` already takes; the corpus run time per scenario in REPORTS.md (load and create separately, from the tool's own timing).
  - **Stage 2 — the `INSELLNK` resolver (`READY`, round r).** Evidence: `docs/re/annoscene.md` §1 (`ROHSTOFF` `0x1000a076`, `SCHATZ` `0x1000a158`, `ERZE` `0x1000a19f`), §2 (`INFO` bytes), §3 (the link record), §4 (the generator), §5 steps 1–10 (`0x10009550`); `docs/formats/sze.md` "Island links"; `docs/formats/insel.md` (the 444 `INSEL5` and 21 `INSEL4` templates). What is **confirmed** is implemented as read; what §5 marks **likely** (the meaning of the step-3 tables `0x10009010` / `0x1003af40`, the cache entry layout, the skipped empty link of step 10) is read instruction by instruction first (`tools/disasm.py game/app/AnnoScene.dll <va> <bytes>`) and the note updated to D before the code; a step that cannot be read in this task stays a warning (the link unresolved), never a guess.
    - **Do:**
      1. **The scene records that feed the selection**, in `scenario_formats.cpp`: `ROHSTOFF` (16 bytes → 20 `u16` raw-material counts? the byte map from `0x1000a076`, D), `SCHATZ` (`u16` treasures, `0x1000a158`), `ERZE` (10 bytes: 9 ore counts and byte 6 = the special-island count, `0x1000a19f`, D) → `LoadedScenario::links` data: `IslandLinkSpec{number, x, y, w, h, climate (annoscene §3 prefix table 0x1003b328), path}` per `INSELLNK` whose path ends in `DUMMY.scp` (case-insensitive, as `0x10027310`), plus `LinkInputs{raw_materials[20], ores[9], special_count, treasures}`. A link to a non-dummy file (§3 item 3; none in the corpus) → a warning `LinkNamesTemplate` with the path, not resolved. The `IslandLink` warning goes away; the records leave the `UnknownRecord` list.
      2. **The resolver**, new files `src/sim/include/anno/sim/island_links.hpp`, `src/sim/src/island_links.cpp`, **no file I/O in `sim`**: `resolve_island_links(const std::vector<IslandLinkSpec>&, const LinkInputs&, std::span<const TemplateSummary> candidates, DrawSource&) -> Result<ResolvedLinks{ std::vector<ResolvedLink{number, template index or none, ores[9], special, treasure}>, warnings }, std::string>` where `TemplateSummary{name (upper-cased path as the cache holds it), w, h, info[12]}` is built by the caller from the game dir (steps 1–2: the filter `.scp`, not `PIRAT`, not `DUMMY.scp`, `GetAnnoInselFileData` succeeds = `formats::insel` parses it and has `INFO`; equal w, h, `INFO[0]` = climate, `INFO[8] = INFO[9] = 0`). Steps 3–9 as §5 states them, each with its `DrawSite` (new sites, data in the enum: `LinkRawMaterial`, `LinkOre`, `LinkTemplatePick`, `LinkSpecialMark`, `LinkTreasure`, `LinkOrePlace`; `may_draw` true for them only in the load phase), the draw count per step recorded in `Effects::draws`-style telemetry of the result. `kPrngTimelineVersion` → 2 with the reason (the first load-time sites), every golden and `scenario_corpus.txt` re-baselined in that one commit (the standing note at the top of this file).
      3. **Candidate order (OWNER-CHECK, default stated):** the original takes the directory enumeration order of `InselCache.txt` (§5 step 1; filesystem-dependent, so two machines can disagree). **Default of the rebuild: the candidate list sorted by the upper-cased path, bytewise** (deterministic on every machine, so a network game needs no cache exchange; `Rules::island_candidate_order = Sorted`, the alternative `Cached` reserved and rejected by `validate` until someone needs it). Consequence stated in `state.md` §10 and OQ-new: for a given seed the rebuild picks a different island than the original would (the original's own pick is not reproducible across installs either, so no differential fixture can compare it). The owner confirms or changes the default; the work does not wait.
      4. **Placement (step 10, `0x10008490`)**: read it. The resolved template is loaded by the caller (`formats::insel` on the `.scp`), turned into an `IslandSpec` by the same function stage 1 uses for `INSEL5` (pull the island-building code of `load_scenario` into `island_from_insel(const insel::Island&, slot, x0, y0, …)`), placed at the link's position with the link's number as slot; the assigned ores go onto the island's mountain objects as the original does it (D: which objects, which draw, which amount → `DepositSpec` with `goods` from the ore kind: the 9 `ERZE` kinds → goods ids, O-37, from the table the reader uses; if the goods mapping cannot be read, the deposits become a `LinkOresUnplaced` warning with the counts). A link with no candidate left stays empty with a warning `LinkUnresolved` (the original's behaviour, likely a skip; read it).
      5. **The glue**, `scenario_formats.cpp` or a small `scenario_load.cpp` outside `sim`'s I/O rule: `anno-sim-replay scenario` and the corpus test scan `ANNO1503_GAME_DIR/Scenes/Islands/**/*.scp` once into the `TemplateSummary` list (sorted), call the resolver with the world's generator seeded as `World::create` would (the scenario's seed, §4: property 8 = cursor; the draws come **before** the world exists in the original, so the resolver takes a `DrawSource` the caller constructs from the same seed and the world then starts from the cursor the resolver left: state it in `state.md` §6 and make `World::create` take the start cursor, or document why the resolver draws from the world's own stream after create — pick what the original does, D §4 and §5), load the chosen templates, append the `IslandSpec`s, then `World::create`.
      6. **Tests:** unit cases on synthetic links and synthetic candidates (no game dir): the filter of step 2 (size, climate, INFO[8]/[9]), the capacity filter of step 4, steps 5–7 with a scripted `DrawSource` (known draw values → known picks, hand-derived from §5 before the code), the special counter, the treasure loop with repeats, an unresolved link, the draw count per site, determinism (same inputs → same result), a mutation list of at least 10 with the kill evidence (a draw site dropped, `mod count` on the wrong count, special/normal swapped, the order of steps 3a/3b swapped, …). Corpus: `scenario_corpus.txt` gains `links_resolved=`, `links_unresolved=` and `deposits` becomes non-zero for the 9 link scenarios; every resolved island passes the same create/run/invariants/save-load as stage 1; the per-file `create_hash` is now seed-dependent through the picks (the corpus uses the file's seed; state which).
      7. **Docs:** `state.md` §6 (the timeline: the six sites, version 2), §10 "As implemented in S14 stage 2", §15; `annoscene.md` §5 upgraded from likely to D where read (with addresses), `sze.md` `ROHSTOFF`/`SCHATZ`/`ERZE` byte maps; OQ-new for the candidate order (owner), OQ-new for what stays unread; the standing note (the corpus header line, `sim_stream.py`); PLAN M3; REPORTS.md; PR.
    - **Stage 2 rebase (round s, the executor itself, before T24):** PR #36 was branched from 6106c58 and `main` now holds T23 (883b33a), which edits the `PROD2` building branch that 03f73a3 moved unchanged into `island_from_insel`. Rebase (or merge `main` into the branch; the PR head must contain 883b33a) and: (a) re-apply T23's `PROD2` rule inside `island_from_insel`: a building item (a list in the layout) is kept for the city pass, not placed and not `BuildingWithoutCity` (that kind is gone); `island_from_insel` hands the island's `IslandCities` (the `BuildingItem` list, `by_origin`, `other_prod2`) back to its caller; `load_scenario` keeps them per island for `read_city` as T23 does; `place_island_links` reports every building item of a placed template as `BuildingItemWithoutEntry` (no city of the file names a link island's tiles; native settlements are OQ-151) and never places one as a building; (b) `all_warning_kinds` / `warning_kind_name`: both lists (T23's 18 and stage 2's 7 new kinds, `UndecodedCity`, `BuildingWithoutCity` and `IslandLink` all gone); (c) docs: `state.md` §10 keeps both "As implemented" blocks (T23, then stage 2) and the O-35/O-36 rows of both; `open-questions.md` OQ-144/145 then OQ-150–154, and **OQ-150 → status "decided 2026-10-23 (owner): `Sorted` stays, tested against the original later (T15); `Cached` reserved"**; `sze.md` status line with both clauses; PLAN M3 both sentences; REPORTS.md both sections plus one short paragraph under the stage 2 section ("rebased on 883b33a: …", the new corpus totals); (d) `scenario_corpus.txt` re-recorded with the game dir (`ANNO_UPDATE_GOLDEN=1`; header line 110 / 2; the lines now carry cities, buildings and links); the economy case and the link case both pass; (e) the gates of §1 item 4 through the heavy-job lock (full ctest with and without the game dir, clang-format, WSL GCC and Clang, `lint_sim`, `sim_stream.py --check`), push, CI green; then stop: the orchestrator merges. No reviewer subagent.
    - **Acceptance:** (1) the 9 link scenarios (6 link-only, 3 mixed) create their islands: `links_unresolved` 0 in the corpus unless the original also leaves a link empty for that file (then say which and why, D); the 229 links listed by outcome in REPORTS.md (counts); (2) every step of §5 either D with its address in the code comment or a named warning; `grep -ci "assume\|guess"` on the new files: 0; (3) `kPrngTimelineVersion` 2, every golden and the corpus file re-baselined in one commit with the reason, `sim_stream.py --check` agrees, the standing-note header line present; (4) `may_draw` soundness: the six sites draw only during resolution, never in `advance` (a test); (5) the mutation evidence of Do 6; (6) no filesystem access in `src/sim` (lint: `fstream`, `filesystem`, `fopen`, `FindFirstFile` absent; add them to `lint_sim.py` if not there); (7) gates of §1 item 4 through the heavy-job lock; (8) `anno-sim-replay scenario 0010SingleEndlos/03_1024_med_256.sze --ticks 150000` prints the resolved islands and runs clean; (9) scalability: no fixed link, candidate or ore-kind count (the 20/9 counts are `SimConfig` data with the original defaults and a reason).
- **S13 acceptance (written before work starts):**
  1. every test name listed in §15 S13 exists in `sweeps_test.cpp` and passes: "growth sweep visits chunks per sweep_chunks", "grow writes through the setters", "cursor wrap reading A3", "drought centre is the first market building", "drought machine timeline" (also with `drought_steps` / `drought_phases` / `chunk_edge` modded), "drought gate catches up", "drought tiles seam called per strip", "start_drought_at is the constructor the scripted door will use" (O-51), "edge chunks are clipped" (A14/F20); plus: a `city_mask` test with `city_slots` 33 and 64 (O-55: a drought starts, the loader accepts the record, the bit index is the city slot) and a save/load/hash test of a drought in each of the three states;
  2. `grow_chunk` calls the existing `grow(GrowthTile&, const ProductionType&, t)` for every in-bounds tile of the chunk whose `object` is a growing type (`types.find_growing`), row-major, and writes `object` / `phase` back only through the dirtying setters (the chunk digest changes exactly for the chunks visited); the sweep's tick count, cursor and stamp stay exactly as S7 left them (the S7 sweep tests in `scheduler_test.cpp` pass unchanged);
  3. the drought timeline is derived by hand in the spec before the code and asserted: with the defaults 11 200 ticks to expand (7 rings × 32 strips × 50), 120 000 ticks of plateau (120 counts × 1 000), 11 200 to retreat, then `I.drought` empty; the catch-up gate adds `drought_plateau_period` / `drought_strip_period` to the stamp (never `= t`), so a long step runs several strips over the following ticks; the strip rectangle handed to the seam (phase = side, step = offset along the side, ring = distance in chunks) is written down in §5.8 with the address of the loop that produced it, or marked **I** with the stand-in named;
  4. **G5** (§14.3: growth tiles, the 300 s drought gate with a market building, 1 000 000 ticks; `synthetic::g5_scenario`) committed as our own synthetic fixture (`g5.log` recorded with the `Recorder`), reproduced by the test and by `anno-sim-replay run` (CTest `anno_sim_replay_g5_run`); hand-derived asserts at least for: the number of growth steps of one tile after N visits, the tick of the first drought start (the first 300 s gate whose draw passes `% event300_modulus`, worked out from the PRNG table for the seed), the drought's states at three checkpoints, the drought gone at the end or still running (say which and why);
  5. the exit condition stated: growth tiles advance through `World::advance` (name the call path `advance → island_tick → grow_chunk → grow`), and the drought tile effects remain the seam `drought_tiles` (O-15, default none), said explicitly; nothing in S13 draws from the PRNG (O-13 stays open: state what was checked at the callees of `0x1008b5f0`, or narrow O-13 with addresses);
  6. evidence as in S12: module + address or a spec section per rule; the §5.8 machine is tagged **I** today (inferred), so S13 reads `0x1008aeb0` and the strip loop `0x1008b4f7..0x1008b5be` instruction by instruction and either confirms the pseudo-code (the tag becomes **D**) or corrects the spec first; untraced details become new OQs with the stand-in named (numbers continue after the highest on `main` at PR time, OQ-133 today; T21 runs in parallel and may take OQ-134 and up: coordinate through the orchestrator); feel changes stay faithful and become owner questions;
  7. `kStateHashVersion` bumped with the reason if the stream changes (the widened `city_mask` changes it: say how it is serialised, count + words); G1–G4 still replay or are re-recorded with the reason, and `tools/sim_stream.py` follows (its `--check` agrees);
  8. gates of §1 item 4 through the heavy-job lock: MSVC build; ctest with and without the game dir (the full with-game-dir run in the foreground; if the memory guard stops it, the sim and replay subset `-R anno_sim` and the fact stated); clang-format 23.1.3; WSL GCC 13.3 and Clang 18 `-O3 -Werror` with and without the game dir; `lint_sim.py`; mutation evidence for the in-bounds clip (an 8×8 loop without the clip fails "edge chunks are clipped"), the catch-up gate (`stamp = t` fails "drought gate catches up") and the ring bound (an expanding ring past the target that does not become the plateau fails the timeline case);
  9. `state.md` §15 "S13 as implemented", §5.4/§5.8 corrections, PLAN status, a REPORTS.md section, a PR.
- **S13** `DONE` (round n, 7c13f3b, §4). **T22** (the OQ-131 deviations) is `READY` below and runs before S14, because it moves the stream version and every golden with figures once more; S14's acceptance is written when T22 is merged.
- **S12 (world-level time; done, §4 2026-10-23 j/k)**, as written in `state.md` §15 S12.
- **S12 acceptance (written before work starts):**
  1. every test name listed in §15 S12 exists in `world_events_test.cpp` and passes:
     - the world-level cases of §14.2: FIFO, saved-next in `advance`, kind-1 events saved, timer clamp, monopoly once and the rule, event 7.5 min draws, accumulator discard vs keep, zero islands, registry count without `+1`, expiry, interim return, counter release;
     - "figure phase goods effects merge after each call (C2)";
     - "AI seam commands applied in advance and not logged";
  2. golden G4 (§14.3: players with upkeep and bankruptcy, events, timers, the 7.5 min event with the add-on flag, monopoly once; 600 000 ticks) committed as our own synthetic fixture and reproduced by the test and by `anno-sim-replay run` (a CTest like `anno_sim_replay_g3_run`);
  3. exit condition checked and stated: every subsystem of §5.3 exists, even where its body is a seam default (list each with its file and whether its body is real or a seam default);
  4. evidence as in S11: module + address or a spec section per rule; untraced details as new OQs with the stand-in named; feel changes stay faithful and become owner questions; new OQ numbers continue after the highest on `main` when the PR is opened;
  5. `kStateHashVersion` / `kPrngTimelineVersion` bumped with the reason if the stream or the draw sites change; G1–G3 still replay, or are re-recorded with the reason;
  6. gates of §1 item 4 through the heavy-job lock:
     - MSVC build;
     - ctest with and without the game dir;
     - clang-format 23.1.3;
     - WSL GCC 13.3 and Clang 18 `-O3 -Werror`, with and without the game dir;
     - `lint_sim.py`;
     - mutation evidence for the event FIFO order, the monopoly-once rule and the 7.5 min event's add-on flag (break each, a named test fails);
  7. `state.md` §15 "S12 as implemented", PLAN status, a REPORTS.md section, a PR.
- **S11 (remaining production classes; done, §4 2026-10-23 j)**, as written in `state.md` §15 S11: `driver.cpp` drives WeaponsWorkshop (research input), FurTrapper, SmallFarm (`DrawRef`) and Mine (deposits), the mine area, and removes the temporary mask.
- **S11 acceptance (written before work starts):**
  1. every test name listed in §15 S11 exists and passes (`driver_classes_test.cpp`: each class through the scheduler equals the direct step-function test at the same ticks; "weapons workshop unlocked from research"; "mine deposit by index and depletion only in the add-on"; "small farm draws once per step through the world"; "fur trapper sends nothing without prey"; "weapons workshop save load advance");
  2. golden G3 (§14.3 table: two islands, two cities, one building of every implemented class, a mine over two deposits, research bytes set, 100 000 ticks) committed as our own synthetic fixture and reproduced by the test;
  3. exit condition checked and stated: every step function of `production.hpp` is reachable from `advance` (name the call path of each), and the temporary mask is gone (grep shows it);
  4. every rule cites module + address or a `production.md`/`core.md` section; untraced details go to `open-questions.md` as new OQs with the stand-in named; anything that may change the feel stays faithful and becomes an owner question;
  5. if the state stream changes, `kStateHashVersion` is bumped with the reason in `state.md`; G1 and G2 still replay (or are re-recorded with the reason);
  6. gates of §1 item 4: MSVC build, ctest with and without the game dir, clang-format 23.1.3, the WSL GCC 13.3 `-O3 -Werror` check with and without the game dir, `lint_sim.py`; mutation evidence for the research unlock, the add-on-only depletion and the once-per-step draw (break each, a named test fails);
  7. `state.md` §15 "S11 as implemented", PLAN status, a REPORTS.md section, a PR.
- One stage per task, in the order of `docs/sim/state.md` §15; each unlocked after the previous is merged. Branch `task/T9-sim-s10-build` from `main` after the S9 merge (worktree `C:\Users\altin\anno1503-T9`); one orchestrator subagent; `src/sim`, `src/tools/sim_replay`, `fixtures/synthetic/sim`, `docs/sim` only.
- **S10 (Build, Demolish, trade commands)**, as written in `state.md` §15 S10: `apply.cpp` (Build, Demolish, SetTradeFlag, SetTradeSlot; today they answer `Unsupported`), `placement.cpp` (the stand-in rule), create with charge, `legacy_codec.cpp`, the G2 log.
- **Acceptance (written before work starts):**
  1. every test name listed in §15 S10 exists and passes (17 cases, from "build charges gold and goods" to "legacy 0x64 decode on synthetic bytes");
  2. `fixtures/synthetic/sim/g2.log` committed (our own synthetic data, no original bytes), recorded with S9's `Recorder`; `anno-sim-replay run` reproduces every checkpoint of it;
  3. free-slot reuse after Demolish matches the hand-derived order written in `state.md` before the code;
  4. every rule cites its evidence (module + address or `production.md`/`core.md` section); anything not traced goes to `open-questions.md`, not into code as fact; anything that changes the feel (e.g. the free-build cheat default) stays at the original's behaviour and is listed as an owner question;
  5. all gates of §1 item 4 including the WSL GCC -O3 check with and without the game dir, `lint_sim.py`, and mutation evidence for the charge, the atomic rule and the free-slot order (break each, the tests fail);
  6. `state.md` §15 "S10 as implemented", PLAN status, a REPORTS.md section, a PR.

### T24 — Transport stage 1: the market cart and the workshop carrier (jobs `0x34`, `0x33`): RE, spec, the first goods that move (O-8, O-59, D12, OQ-103) · **`DONE`** (merged as fed76bd on 2026-10-23 u, PR #38, head 0e00cb3, CI 10/10; §4; stage 2 = T27, the weapons carrier = T28 `LATER`) · was `READY` (round s; the executor itself) · `PARALLEL-OK` with T25 (T24: `src/sim` (`figure.*`, a new `transport.cpp`/`.hpp`, `scheduler.cpp` where the market tick sends, `seams.hpp`, `state_fields.hpp`, `world.hpp`, `synthetic.*`, the sim tests), `fixtures/synthetic/sim/` (goldens, g2–g6, `scenario_corpus.txt`), `tools/sim_stream.py`, `docs/re/production.md` §5 (or a new `docs/re/transport.md`), `docs/sim/state.md` §4.9/§5.9/§15, `open-questions.md` (OQ-103, O-8/O-59/D12, new from **OQ-160**), PLAN M3, REPORTS) · branch `task/T24-transport-carts` from `main` after the stage 2 merge · worktree `C:\Users\altin\anno1503-T24`
- **Why (the game developer's view):** T23 put 8 526 buildings into the shipped towns and the honest number came out: in `single36` 5 of 107 producers move within 150 000 ticks, the warehouse never. Nothing carries goods: no cart empties a full output store, no carrier fetches a workshop's inputs, houses do not consume. PLAN M3 orders the subsystems production → carriers/roads → population; transport is next, and S15's 1-hour run means nothing on a frozen economy. The reading is half done: `docs/re/production.md` §4.6 (the market tick sends a cart, job `0x34`, when fewer than `max_figures(0)` are out; the market has no stock, its slots forward to the city), §5.1 (sending: base slot `+0x118` = `0x10072d20`, the reachable entrance `0x10072b50`, the figure factory world `+0xa0`, the job factory `0x1003b0e0`: `0x33` vtable `0x100c45e8`, `0x34` vtable `0x100c4568`; jobs are step lists, likely), §5.2 (the cart's candidates, the road search through searcher slot `+0x7c` with node callback `0x1005a900`, the score with the 75 % rule and the 1 t minimum, the reservation `0x10068460`; D), §5.3 (city add `0x1002da50` clips at the capacity), §5.4 (capacity `+0xf2`, speed f32: likely, unit open), §8 (figure movement is float in the original: 584 `__ftol`; the determinism risk). The sim side exists: `Figure` (§4.9: `pos` Q24.8, `cargo_goods`, `cargo_88`, `home`, `home_counter`, `job`), the registry phases B/C/D with `Seams::figure_tick` (§5.9), `Effects` `Reserve`/`Deliver`/`GoodsMove`/`ReleaseCounter`/`spawns`, `send_figure` in the production ticks, `end_figure` (T22). The seam's default body today is "nothing moves" (D12's interim return behind a rule).
- **Do (RE first; the code follows the reading, each line D, likely or open):**
  1. **Read** (AnnoWelt, `tools/disasm.py`; notes with addresses into `production.md` §5 or `docs/re/transport.md`): (a) **the job objects**: the step lists the factory `0x1003b0e0` builds for `0x34` (vtable `0x100c4568`) and `0x33` (`0x100c45e8`): each step's type code, target and completion rule (the codes 7, 9, `0xb` of §5.1 are "likely"); what the cart does at the producer (the reserved amount → cargo: `min(reserved, room)`? which slot takes it from the building's store) and at the market (market `+0xc0` → city `+0x80`: what happens to cargo the capacity clip leaves), how the figure returns and ends (the walk of `0x1003af00`, the Remove `0x10034844`, the counter release `+0x2c`: T21/T22 have the end; the body before it is O-8); (b) **the workshop carrier `0x33`**: where it fetches inputs (the warehouse only, or producers; production.md "Open"), how much (the priority `0x80`/`0x60`/−1 of slot `+0xd8` = `0x10068500`), and the delivery into the workshop's input store (which slot); (c) **the road search**: the class behind searcher slot `+0x7c`, the node callback `0x1005a900` (which tiles are nodes: tile objects whose type flag word has `0x4000` road/bridge, production.md §2.3 `+0x12`? the building entrance `0x10072b50`), the cost per step, the neighbour order (tie-breaking), the bound (the market's area = footprint grown by the type's radius byte `+0x65`, §5.2), and whether the cart needs a road for the whole way or only to the entrance (**OQ-103: the owner's road rule outranks the tutorial series; record what the code does, then the owner decides the default if they differ**); (d) **movement**: the per-tick step of the unit (unit slot `+0x74`, O-8), the speed f32 (`HEAD` f32 at 4 → `+0x10`, slot `+0x78` multiplies) and its unit, the arrival test; the x87 sequence, and from it **the integer design**: Q24.8 `FixedPos` (D13) with exact arithmetic where the original's floats have an exact integer counterpart (power-of-two scales, `__ftol` truncation) and a stated deviation where not (OQ-new for the owner: the trip time may differ by rounding; never a float in state or in a hashed path); (e) the figure's save record (`TASK`/`ENTRY` `0x1005b7a0`) only as far as (a) needs it to name the fields; no save compatibility.
  2. **Spec**: `state.md` §5.9 gets the pseudo-code of both jobs and the road search as the behaviour body (D per line, I where inferred, each with its address), the new `Figure` fields (job state: step index, target, path or next node, reserved amount, whatever (a) needs; data not limits), the `Seams::figure_tick` default body = these jobs (the seam stays for the figures the AI and the scripts will drive), O-8 / O-59 / D12 narrowed (a carrier that returns by itself makes `interim_figure_return_ticks` irrelevant for it: say what the rule now covers), §14.3 **G6**.
  3. **Code** (`src/sim`, `transport.cpp`): the market cart and the workshop carrier as the default `figure_tick` body; the road search as a deterministic search over the island's tile layer per the reading (ordered containers, integer costs, the original's neighbour order or a stated D2-style deviation; bounded by the market's area as the original bounds it); movement in Q24.8 per Do 1 (d); the effects through the existing `Reserve`/`Deliver`/`GoodsMove`/`ReleaseCounter` (extend them only where the reading shows a path they cannot express); the market tick's send (job `0x34`, §4.6) through `send_figure` as the production classes do. No `Rules` option unless a feel question appears (the faithful body is the default; the owner decides OQ-103 if the code and the owner's rule differ).
  4. **Tests**: unit cases on a synthetic island, each hand-derived from the reading before the code: one market, one weaver with cloth and a road between them (the cart loads, returns, the warehouse gains, the store loses, the counter releases); a producer off the road net (never chosen, per OQ-103's reading); F11 and F12 of production.md §9 (the scores); a full warehouse (the capacity clip and the cargo left); the carrier fetching wool from the warehouse into the weaver; the search's neighbour order pinned; determinism (two runs equal; executor with 1, 2 and 8 threads equal); **G6** (`g6.log`: a market, two producers, a workshop with a carrier, 300 000 ticks; replayed by the test and by `anno-sim-replay run`); a mutation list of at least 10 with the kill evidence (the 75 % threshold, the 1 t minimum, the score division, the reservation, the capacity clip, a step skipped, the neighbour order, the speed, the counter release, the early stop of the search, …). **The corpus economy case asserts the warehouse half T23 deferred:** in the scenario with the most buildings at least one warehouse stock moves within 150 000 ticks; it prints per scenario how many producers moved (expected far more than 5 of 107 in `single36`).
  5. **Stream**: `kStateHashVersion` bump with the reason (the new `Figure` fields); `kPrngTimelineVersion` only if a job draws (name the site; D); every golden, g2–g5 and `scenario_corpus.txt` re-baselined in one commit (header line; `sim_stream.py` follows). Take the `alloc_probe.cpp` nothrow-`new` fix-up in a separate commit if it bites or if the branch is the first to touch `src/test_support`.
  6. **Docs**: production.md §5 to D where read, `state.md` §5.9 / §15 "T24 as implemented", OQ-103 (what the code requires; owner decision if it differs from the owner's rule), O-8 / O-59 / D12 narrowed, new OQs from **OQ-160**, PLAN M3 (carriers), REPORTS.md, PR.
- **Staging:** stage 1 may stop after the market cart (`0x34`) if the carrier's reading (b) does not fit the task; then acceptance (3) must still hold (carts alone move warehouses) and the workshop carrier becomes stage 2 with its own acceptance. Say in REPORTS.md which.
- **Acceptance (written before work starts):** (1) the RE note answers (a)–(d) with addresses, each line D, likely or open; nothing likely is in the code without being stated in `state.md` and an OQ; (2) the synthetic cases of Do 4 pass with hand-derived expectations; G6 replays by test and by the tool; (3) **in the scenario with the most buildings at least one warehouse stock moves within 150 000 ticks**, the moved-producer count per scenario is reported, all 50 still load/create/run/hash/save-load clean, and the per-scenario run time stays within 2× T23's (median 149 ms, max 655 ms; if transport blows that up, name where: the road search is the suspect, a per-island cache the answer, not a cap); (4) no float in state or in any hashed path (`lint_sim`), the movement integer-only with the deviation from the original's float path stated as an owner OQ; (5) the mutation evidence of Do 4 (≥ 10 killed); (6) stream and timeline bumps in one commit with goldens and corpus re-baselined, `sim_stream.py --check` agrees; (7) gates of §1 item 4 through the heavy-job lock, no reviewer subagent (the orchestrator reviews); (8) scalability: no fixed figure, job, road or path-length count; the search's work per cart bounded by the market's area as the original bounds it (state the bound and measure it on `single36`).

### T27b — The registry refuses a world whose layout would classify a type differently (owner decision 2026-10-24 w) · `READY` (round w; **the executor itself**, before T30) · `PARALLEL-OK` with T29 (T27b: `types.hpp`/`types.cpp`, `types_test.cpp`, the one test setup named below, `state.md` §5.9, OQ-155; T29: `transport.*`, `transport_test.cpp`, `scenario_test.cpp`) · branch `task/T27b-layout-check` from `main` at 0972e5c or later · worktree `C:\Users\altin\anno1503-T27b` · no new OQ number
- **Why:** `BuildingType::cls` (hashed) and `walk_class` (not hashed) are derived from the `ListLayout` the registry was built with; a world may run with another layout (`validate_against` is called with the world's config, a registry is shared between worlds), and then the transport search and the class lookups disagree with the world's lists. T27's fix-up tried a whole-layout digest compare; 10 cases failed, 7 of them only for the schedule, lookup order or roles, which do not enter `list_for` (REPORTS T27 "Fix-up (round v)" item 6).
- **Steps (in order; §1 item 7):**
  1. **Set up.** `git worktree add /c/Users/altin/anno1503-T27b -b task/T27b-layout-check origin/main`; build; `ctest --preset msvc -j 16 -R anno_sim` with the game dir green (the count into REPORTS.md). Builds and ctest **in the foreground**, one per call.
  2. **The check.** In `check_against_config` (`types.cpp`), for every type: recompute what `finish` and `type_from_entry` derive from the layout, under `cfg.layout`: the class (`class_for_prodtype(t.prodtype, cfg.layout).value_or(RuntimeClass::Plain)`, only for types whose class came from that function: read `type_from_entry` and the Builder's `add_*` to see which) and the walk class (the `finish` rule); a mismatch refuses with "type N: the configuration's ListLayout gives class/walk class X, the registry was built with Y". A registry without types accepts any layout. No member is added to `TypeRegistry`.
  3. **Tests.** New case in `types_test.cpp`, "registry refuses a config whose layout reclassifies a type": a registry with one type of PRODTYPE 1 and one of 97 under `ListLayout::original()`; a config whose `block_to_list` drops block 3 (97 loses its list) is refused naming type 97; a config that only changes `schedule` and `roles` is accepted. Mutants: the walk-class comparison removed → the case fails; the class comparison removed → name which case fails or add a sub-case that does.
  4. **The existing cases.** Run the sim suite. By reading the tests only "building scheduler visits the lists in the order of the schedule" (`scheduler_test.cpp`) should fail: its world uses `block_size = 1`, `block_to_list = {4, 5}`, under which the kit's other types have no list. Fix that case's setup so the registry is built with the world's config (the Builder derives the class with its config's layout); change nothing it asserts. If other cases fail, stop and list them (§1 item 7).
  5. **Docs.** `state.md` §5.9 (one sentence: the walk class and the class follow the registry's build layout; `validate_against` refuses a world whose layout would derive other values); OQ-155: a status note "the `HEAD2` trade-list slot counts (39 shipped cities with counts other than 4/6) are not kept; a field comes with the trade stage (owner, 2026-10-24 w)"; REPORTS.md with the acceptance ticked; PR. No `TASKS.md` edit.
- **Acceptance:** (1) the new case and the two mutants; (2) only the named test setup changed, its asserts untouched; (3) every hash unchanged (the full suite with and without the game dir, `sim_stream.py --check`, `scenario_corpus.txt` and `goldens.txt` unchanged); (4) gates of §1 item 4 through the heavy-job lock, in the foreground; (5) PR, not merged.

### T28 — Transport stage 3: the weapons workshop's carrier (its two lines, `0x1006a4a0`, `0x1006a540`, `0x10068ee0`), then the farm workers and the other jobs (O-8) · `LATER` (after T33 and T34 stage 2: the weapons carrier walks by the decoded rule and the float walk; the owner decided OQ-103/160 and OQ-162 on 2026-10-24, see the header) · was `LATER` (after T27 and the owner's OQ-103/160/162 decisions)
- One line for now: `transport_tick` returns false for a job-0x33 figure whose home is not a `Workshop`; T28 reads the weapons workshop's `+0xd4`/`+0xd8` slots (vtable `0x100c8150`) and extends `workshop_priority`, `workshop_offer`, `load`, `unload` to its lines per `transport.md` §1.4, with a two-line synthetic case. The section is written when T27 is merged.

### T29 — Why the carts do not reach: the `S01_07_01` frontier, the area cut and the profile (measurement only; follow-up of T27 step 4) · `READY after PR #39 and PR #40 are merged` (round v; **one subagent**) · `PARALLEL-OK` with T30 (T29: `src/sim` `transport.hpp`/`.cpp` (only the diagnostic `TransportStats` and its filling), `transport_test.cpp`, `scenario_test.cpp`; T30: docs and `tools/` only) · branch `task/T29-cart-reach` from `main` after the PR #40 merge · worktree `C:\Users\altin\anno1503-T29` · new OQs from **OQ-171** (to 174)
- **Why:** T27 step 4 found 43 of 55 markets in `S01_07_01` with a producer in range that the cart cannot reach, in a designed campaign town, under a stand-in that is looser than the owner's rule (any footprint tile is a goal). Either the stand-in blocks tiles a cart can use, or the search area cuts the road off, or the towns really are unconnected. T30 decodes the real rule and needs to know which tile types matter; T29 measures them. Nothing changes in what the sim does.
- **Steps (in order; §1 item 7):**
  1. **Set up.** `git fetch`; `gh pr view 39 --json state` and `gh pr view 40 --json state` both say `MERGED`; `git worktree add /c/Users/altin/anno1503-T29 -b task/T29-cart-reach origin/main` (Git Bash: forward slashes). Build (`cmake --preset msvc && cmake --build --preset msvc`); `ctest --preset msvc -j 16 -R anno_sim` with `ANNO1503_GAME_DIR=C:\Users\altin\anno1503\game\app` and `ANNO_TEST_THREADS=16` green; the count into REPORTS.md.
  2. **The diagnostic fields.** In `transport.hpp`, `TransportStats` gains (test-only, filled only when the pointer is non-null, after the search, from the search's `parent` array; what `choose_source` returns does not change): `std::map<std::uint32_t, std::uint64_t> frontier` (for a search that ends `unreached`: the object type id of every tile inside the search grid that is not passable and is an 8-neighbour of a reached tile; id 0 = no object); `edge` (searches ending `unreached` whose reached tiles touch the grid's border: the area cut); `home_no_road` (senders whose footprint has no road tile among its 4-neighbours); `goal_no_road` (candidates, each counted once per search, whose footprint has no road tile among its 4-neighbours); `corner` (diagonal steps from a reached tile to a passable, unreached tile that the corner rule refused: "no diagonal passes a blocked orthogonal neighbour"); `outside_disc` (searches ending `unreached` in which every candidate's footprint lies outside the area's disc: candidates come from `area.square()` but only tiles inside the disc are walkable, T27 review). **Over the run** (round w): a `TransportStats*` member in `TickContext` (default null; not state, not hashed, set only by the test), so the case counts every search of the 150 000 ticks, not only a snapshot; say in REPORTS.md that it is single-threaded today and must stay null if island ticks ever run in parallel (S16).
  3. **The corpus numbers.** Extend the case `transport search statistics (ANNO_SIM_TRANSPORT_STATS)` in `scenario_test.cpp`: for `S01_07_01` and `single36`, the counts over the whole run (through `TickContext`) and, as before, the snapshot of every sender at ticks 0, 30 000 and 150 000; print T27's four counts and the new fields; for `frontier`, the 15 most frequent type ids with `prodtype`, `flags_word` (hex), `walk_class` and the count. Look the names up with `anno-dump anlagen` on the `Anlagen.dat` the case's registry loads (names only into REPORTS.md).
  4. **Synthetic cases** in `transport_test.cpp`, counts hand-derived before the code: (a) a market, a producer with goods in range, between them one non-road object of a known type: `unreached`, and that type in `frontier`; (b) a road that leaves the search grid and comes back: `edge` 1. Mutant: `frontier` not filled → case (a) fails (name it).
  5. **Profile** `anno-sim-replay scenario 0100Kampagne01/S01_07_01.sze --ticks 150000`: in WSL `perf record -g` or `valgrind --tool=callgrind` on a GCC `-O2 -g` build of `anno-sim-replay` (whichever is installed or installable with `apt install`), else Windows `wpr`. The 10 functions with the most self time and their shares into REPORTS.md. If no profiler can run, write `BLOCKED` for this step (§1 item 8) and continue.
  6. **Reading** (REPORTS.md, a paragraph each, with the numbers): which of (a) a walkable type without `kRoadFlag` (a square, a bridge, a gate, …), (b) the area cut (`edge`), (c) the corner rule, (d) producers or markets with no road at all explains the `unreached` count; what the profile names as the next speed-up (no optimisation in T29). The frontier's type ids are T30's first tiles to check (say so in OQ-160).
  7. **Docs.** `state.md` §15 "T29 as implemented" (short), `transport.md` §6 one line, new OQs for what the numbers show and the specs do not answer (from OQ-171), REPORTS.md with the acceptance ticked, PR. No `TASKS.md` edit.
- **Acceptance:** (1) no hash moved (the full sim suite with and without the game dir, `sim_stream.py --check`, `scenario_corpus.txt` unchanged); (2) the per-tick numbers and the top-15 frontier table for both files in REPORTS.md; (3) the two synthetic cases and the named mutant; (4) the profile table, or the `BLOCKED` line with what was tried; (5) gates of §1 item 4 through the heavy-job lock; (6) commits one concern each; PR, not merged.
- **Not in T29:** any change to the walk rule (T33), any optimisation, any `rules` option.

### T30 — The walk rule from the original's code: the walking layers decoded and checked in the running original (OQ-160, OQ-103, OQ-170, OQ-161 in part; owner decision 2026-10-24) · **`DONE`** (merged as ad9a314 on 2026-10-24 aa, PR #43, head ce97d51, CI 20/20; PARTIAL D as reported; the orchestrator's two corrections are on `main`, §4) · was `REVIEW` (round z: PR #43, head **ce97d51**, CI 20/20; the round-z commit adds §3.6 OQ-175 deeper RE — family of three variants, 33 distinct cell-type offsets, `0xfff` dominant; was `REVIEW` round y at head e99194b: partial D, headline step 5, step 2 reached the next address, step 7 BLOCKED on a figure reader; the owner reads D claims before merge) · was `READY after PR #39 is merged` (round v; **the executor itself**) · `PARALLEL-OK` with T29 (T30: `docs/re/transport.md`, `docs/re/islands.md`, `open-questions.md` OQ-103/160/161/170, `state.md` §5.9 (one PROPOSED paragraph), `tools/xref.py` fixes if needed, `tools/orig/` read-only readers; no `src/` change) · branch `task/T30-walk-rule-re` from `main` · worktree `C:\Users\altin\anno1503-T30` · new OQs from **OQ-175** (to 179) · **no time box** (owner: "thoroughly analyzed in order to be 100 % sure"); a step that cannot reach D stops and writes its question (§1 item 7), the next independent step goes on
- **Why:** which tiles a cart and a carrier may enter decides how a player lays out a town (OQ-103). The owner wants it from the original's code, certain, not voted. T27 found the tables (`0x100d1140 + 0x1c00 · layer`) but not what their entries mean.
- **Steps (in order; §1 item 7):**
  1. **Set up.** `git fetch`; PR #39 `MERGED`; `git worktree add /c/Users/altin/anno1503-T30 -b task/T30-walk-rule-re origin/main`. `tools/xref.py` is present (T27 fix-up); if not, stop and report.
  2. **The consumer, instruction by instruction (D):** start from the reader of the table pointer, `0x1000daf0` (vtable `0x100c18b0`; reads `[engine+0x430]` at `0x1000db0e` and adds a u16 from its argument) and its twin `0x1000ed40` (vtable `0x100c1a30`), and their callers; the search functions `0x10003d90` / `0x10004890` (engine at world `+0x6aa8`, vtable `0x100c1424`, its engine at `+4`, vtable `0x100c14e4`) do not read `+0x430` themselves (T27 review) but drive the expansion (OQ-161): how a node's neighbours are expanded on the 3 × 3 sub-cell grid; how a cell's 9-bit mask (bits 16–24 of the tile record's second dword) mapped through the u16 table `0x100c1af8`, with the byte of bits 8–15 at bits 16–23 (the cell `table[mask] | byte << 16`, `0x1008ff70`, `transport.md` §3.1 (b)), index the block at `[engine+0x430]`; which entry value admits a step; the corner rule; the step costs (OQ-161); which nodes are goals (the callbacks `0x1005a900` cart, `0x1005a400` carrier, `0x10054240` the walk home). Pseudo-code with addresses into `transport.md` §3.1.
  3. **Every writer of the blocks (D):** the absolute references are settled (`xref.py --reloc`: only the two setters reference `0x100d1140`, nothing points into the blocks; T27 fix-up 2); check the other modules the same way (`--reloc` on `AnnoGame.dll`, `AnnoScene.dll`, `AnnoDisplay.dll` for `0x100d1140 0x100d9d40` — a cross-module write would need an import, so also look for an export or a pointer handed out); then every store through `[engine+0x430]` / `[+0x1030]` (read the code that uses those pointers: the sweep `xref.py <dll> 0x430 0x431` lists the `+0x430` accesses). Result: written at run time or not, with the addresses.
  4. **The rule per layer:** from steps 2–3 and the blocks as the DLL holds them, the condition under which layer L admits a cell, stated compactly if it is compact (for example "mask & M_L ≠ 0"); otherwise counts only, plus an OQ proposing that the loader reads the blocks from `AnnoWelt.dll` at run time (the game files are needed at run time anyway). **No block bytes and no disassembly text in git**: addresses, offsets, counts and rules only.
  5. **The tile side (OQ-170, OQ-171, round x).** Who writes the mask bits and the byte of a tile record: the object's `WALKMASK` (every `Anlagen.dat` entry; `anno-dump anlagen`) and the island loader's terrain set-up (`docs/re/islands.md`, the tile record's second dword). Cover water, coast, river, mountain, a field, a road, a bridge, a square, a building's footprint and its entrance tiles.
     - **Rivers** (owner, 2026-10-24 w): the river pieces are `FLUESSE` tile objects (types 2701.., P), on their footprint's top-left tile in the rebuild's state; read which of a piece's footprint cells each layer may enter, and how a bridge (a road over a river) changes that.
     - **Named roads (T29, round x; OQ-171): the headline finding.** 30 073 of 39 161 cart searches in `S01_07_01` are blocked by `Anlagen.dat` entries 2507 (Cobblestone street), 2526 and 2527 (Dirt road), whose `flags_word` is `0x1186` (no `kRoadFlag`) — the stand-in's road rule doesn't match the game's named roads. **Do:** list every `Anlagen.dat` entry whose `Texte.dat` name is a road or path, with its `flags_word`, `WALKMASK` bytes and `prodtype`, through `anno-dump texte` on `Texte.dat` section `[ANLAGEN]` and `anno-dump anlagen` on `Anlagen.dat`; then decode (from the `WALKMASK` byte and the walking-layer blocks of §3.1) which bit marks a tile walkable for layer 3 (cart). Settle **OQ-171** with the answer. A table `tile kind → mask bits → admitted by layer 0..4` in §3.1, each row D or open. T29's frontier type ids first, if T29 has reported.
  6. **The layer of each walker (D):** where a figure's layer is taken (figure type `LOGIC[0x10]`; P: cart 220 = 3, carrier 203 = 0, native cart 390 = 0) and handed to `0x1000dd20` / `0x1000f010`; every job that walks (0x33, 0x34, the farm worker, the stonemason, the forester, the services) with its layer.
  7. **The check in the running original** (Wine runner, `docs/testing/original-wine.md`; runs are pre-authorized, silent, one at a time through the heavy-job lock):
     - (a) read the five blocks from memory (module base + RVA) right after a scenario loads and again after 10 game minutes; compare with the DLL image: "equal", or the count and offsets of the differing bytes (no content in git);
     - (b) build the playbook's opening (`tools/orig/opening.py`); read the tile records of its road, market hall, weaving huts, sheep farms, foresters, open land and the nearest water tile, and apply the rule of step 4;
     - (c) read the market cart's and a weaving hut carrier's tile every game second for 5 game minutes (if no reader exists, add a read-only figure-position reader to `tools/orig/gamestate.py`, with the figure layout from `transport.md` §4): every tile they stand on must be admitted for their layer, and the carrier must reach a sheep farm off the road if the rule says it may;
     - (d) where the opening allows, in the same session: the OQ-103 video claims (a sheep farm without road inside a weaver's radius is served; the forester needs a road), **and the T29 finding** (a market cart whose only path is a dirt-road/cobblestone-street tile should reach the producer — or not: the Wine runner's figure reader at tile resolution answers directly).
     Results go into §3.1 as "verified (run id)" per row.
  8. **The owner's working rule (OQ-103, 2026-10-22), clause by clause:** houses need a road to a market; services reach a house by road; a road connects only at a building's entrance tile. Each clause confirmed, refuted or open, with the address or the run.
  9. **Docs.** `transport.md` §3.1 rewritten (every line D, verified or open); OQ-160, OQ-103, OQ-170, and OQ-161 for what step 2 read, each with its status; `state.md` §5.9 gets one paragraph **"Proposed rule (T30, for T33)"**: the rule T33 will implement, with its inputs (masks, layers, goals, corner rule, costs), not yet code; REPORTS.md with the acceptance ticked; PR. No `TASKS.md` edit.
- **Acceptance:** (1) the rule per layer, each part D with its address; (2) the run-time question of the blocks settled, D, by the static xref of the whole range and the memory compare; (3) the tile table of step 5, every row D, verified or open; (4) the run of step 7 (a)–(c) done, every observed tile admitted by the rule, or each contradiction listed (a contradiction is a finding, not something to explain away); (5) the owner's three clauses each answered; (6) nothing of the original in git (no byte dumps, no disassembly listing); (7) the PROPOSED paragraph names every input T33 needs; (8) report, PR, not merged.
- **Not in T30:** any `src/` change (T33), the float walk (T34), the weapons carrier (T28).

### T30b — The walk rule, round two: the step-2 vtable method, the memory compare in the running original, and the figure-position reader (OQ-175, follow-up of T30; owner decision 2026-10-24 w `thoroughly analyzed`) · **split into sub-tasks T30b.1–T30b.8 (round aa), one PR each, in order; T30b.1 `READY`** · was `READY after PR #43 is open` (round y; the executor itself) · `PARALLEL-OK` with T31 (T30b: `docs/re/transport.md`, `docs/re/annowelt.md` or new `annoscene.md` where the search engine lives, `tools/orig/gamestate.py` (a new read-only figure-position reader is in scope here — a committed addition, named by this task per the amended §1 item 7), one new scratch script if useful; **no `src/` change**; T31: `docs/re/monuments.md`) · branch `task/T30b-walk-rule-re-2` from `main` (needs PR #43 open; may be opened before PR #43 merges to keep context) · worktree `C:\Users\altin\anno1503-T30b` · new OQs from **OQ-176** (OQ-175 is T30's open vtable method) · **no time box**; a step that cannot reach D stops and writes its question (§1 item 7), the next independent step goes on
- **Sub-tasks (round aa; §1 item 11). Each is its own branch `task/T30b-<n>-<slug>` from `main`, its own worktree `C:\Users\altin\anno1503-T30b-<n>`, its own REPORTS.md section `T30b.<n>`, its own PR (not merged). No `src/` change in any of them. The long "Steps" list below is the reference the sub-tasks point into.**
  - **T30b.1 — The road table as a tool (`DONE`, merged as 0f981cb on 2026-10-24 ad, PR #44, head d18d234, CI 20/20; was `READY`; docs + one tool; no build, no game run).** (1) Set up: `git worktree add /c/Users/altin/anno1503-T30b-1 -b task/T30b-1-road-table origin/main`. (2) Copy the T30 step-5 script (`C:\Users\altin\AppData\Local\Temp\claude\C--Users-altin-anno1503\ec83e711-dbe1-4d88-a7ca-3c3a02a5b0c8\scratchpad\t30_walkmask.py`; if it is gone, rewrite it from `transport.md` §3.2: `tools/figuren.py` parses `Anlagen.dat`, `anno-dump texte` gives the ANLAGEN names, HEAD `flags_word` is the u16 at HEAD offset `0x1c`) to `tools/anlagen_roads.py`; make the game dir an argument (`ANNO1503_GAME_DIR` or `--game-dir`), print only ids, names, PRODTYPE strings, flag words and WALKMASK byte counts; a docstring that says what it prints and that nothing of the game is written. (3) Run it: `python tools/anlagen_roads.py --game-dir game/app`; paste the first 12 lines and the three counts (35 / 36 / 26+10) into the report. (4) `transport.md` §3.2: add one table "named-road ids by group" — columns id range, count, name, PRODTYPE, flags — one row per contiguous id group (`2500–2503`, `2505–2508`, `2509`, `2510–2513`, `2514`, `2515`, `2516`, `2517`, `2518–2519`, `2520–2524`, `2525–2528`, `2529`, `2530–2533`, `2534`, `2535`, `2536–2537`; check each range against the tool's output and correct the list if it is wrong), and one sentence under "Likely": a group of four ids with equal flags looks like one piece shape in four orientations (orchestrator's reading, round aa); which shape the `0x1000` groups are is for T30b.6 (the opening's road tiles). (5) OQ-171: one line "the tool `tools/anlagen_roads.py` reproduces the table". (6) Report with the acceptance ticked: (a) tool committed and run, (b) table in §3.2 checked row by row, (c) nothing of the original in git. Commits: `tools: anlagen_roads.py, the named-road table of T30 step 5`, `docs(re): the named-road id groups (T30b.1)`.
  - **T30b.2 — The class of `[this + 0x858]` and the method `[edx + 0xec]` (`DONE`, merged as 7558fe1 on 2026-10-24 ae, PR #45; the data right, the "untraceable" conclusion corrected by the orchestrator, §4; was `READY`; RE, docs only).** Steps 2 of the list below, taken slowly: (1) worktree `anno1503-T30b-2`, branch `task/T30b-2-vtable-method`. (2) Re-disassemble `0x1000daf0` (`python tools/disasm.py game/app/AnnoWelt.dll 1000daf0 110`) and write the argument list of the call at `0x1000db4a` into the report as a table (push address, value, meaning) — it must come out as `8`, `cell + 4`, `&buffer`, `0`, `[this + 0x85c]`; if not, stop and write what you see. Do the same for the twins `0x1000ed40` and `0x1000f9d0`. (3) Find every write of a vtable pointer into `[reg + 0x858]`: `python tools/xref.py --help` first, then `xref.py` over `AnnoWelt.dll` for displacement `0x858`; list each hit (address, instruction kind: read / write / lea) in the report. (4) For each write hit, read the surrounding function (the constructor or init) and name the vtable it installs on the object stored at `+0x858`; if no write hit exists, follow the constructor `0x1000ebb0`'s callers (`xref.py` for calls to `0x1000ebb0`) one by one and read where the argument that lands in `+0x858` comes from; stop after the chain is D or after three callers without a result, and write the addresses read. (5) With the vtable address known, read the dword at `vtable + 0xec` (`disasm.py` on the vtable address with a note that it is data; or `python -c` with `pefile` as `tools/disasm.py` does) and disassemble the method it points to, instruction by instruction, until `ret`; write per instruction group what it reads from the 4-dword buffer, from `cell + 4` and from `arg 8`, and what it returns (`eax` 0/1, or a value). (6) `transport.md` §3.3: the method as D (address, signature, pseudo-code) or exactly where the chain stopped; OQ-175: status; new OQs from OQ-176 if needed. (7) Report; acceptance: (a) the argument tables for the three readers, (b) the `+0x858` hit list, (c) the method D with address and pseudo-code, or the chain's last address and the reason it stopped, (d) nothing of the original in git.
  - **T30b.3 — Class X's position test `0x1000dd60` and the use of the layer table `[X + 0x430]` (`DONE`, merged as a76dbad on 2026-10-24 af, PR #46, head b4ded5f, CI 20/20; headline right, two documentation nits for T30b.3b to fold in; §4; was `READY`, rewritten in round ae; RE, docs only).** Read `transport.md` §3.3 "Orchestrator's read after T30b.2" first: class X is the object at world `+0x6ab0` (constructor `0x1000eb40`, primary vtable `0x100c18b4`, 47 slots), `[X + 0x430]` is the layer-table pointer (setter `0x1000dd20`, slot `+0x80`), `[X + 0x434]` / `[X + 0x438]` the window origin, `[X + 8]` / `[X + 0xc]` the window size in 8-tile blocks, `[X + 0x85c]` the world, `[X + 0x860]` the kind code. Steps: (1) worktree `anno1503-T30b-3`, branch `task/T30b-3-position-test`. (2) `python tools/disasm.py game/app/AnnoWelt.dll 1000dd60 560` — the whole method to its `ret` (it ends near `0x1000df8x`; if the listing runs past a `ret` into another function, cut there). Write a table per instruction group: address range, what it reads (which `[X + …]` field, which argument, which table), what it computes, where it jumps. Name every read of `[X + 0x430]` or of a register that was loaded from it, and every read of the 8-byte tile records of the window (the data the block fetch of §3.3 filled). State what the method returns in `eax` on each path (the `0x1000df7e` path is "outside the window"). (3) The constant at `0x100c15a8`: read its four bytes with `pefile` (as `tools/disasm.py` opens the file) and give the `float` value; it is the world-unit size of a tile or a sub-cell. (4) The kind codes: `xref.py` for calls of slot `+0x50` of `0x100c18b4` is not possible by address (virtual), so instead find the callers of `0x1000db60`'s twin on the `+0x6aac` object and the call sites that pass 7, 8 or 9 (`push 7` / `push 8` / `push 9` right before a `call [reg + 0x50]`): list them; which object gets which kind. (5) The 47 slots of `0x100c18b4` (`pefile` read of 47 dwords from `0x100c18b4`): a table slot → address → one line on what the method does (from its first 20 instructions; skip the slots that point at the shared stubs `0x10001020`–`0x10001090`, `0x10037590`, `0x100b49d0`–`0x100b4b10`). (6) Docs: `transport.md` §3.3 gets "§3.3.1 The position test `0x1000dd60`" (the table of step 2, the constant, the kinds, the slot table); OQ-175 updated: the yes/no D with address and pseudo-code, or exactly where the reading stopped and the next address; OQ-178 answered if step 4 settles it. (7) Report with the acceptance ticked: (a) `0x1000dd60` read to its `ret`, every `[X + …]` read named, the return value per path; (b) the constant's value; (c) the kind codes per object, or the search described and what it found; (d) the slot table; (e) nothing of the original in git. The `+0x434` question of the old T30b.3 is answered (the window origin, written by `0x1000db60`), so it is dropped.
  - **T30b.3b — The real `[X + 0x430]` reader `0x10003780` and the virtual `[*(ptr) + 0x128]` on the layer-table pointer (`DONE`, merged as 65907d9 on 2026-10-24 ag, PR #47, head 2568865, CI 20/20; headline right, two D-tag errors on signature and buffer-tuple corrected directly on `main` in the round-ag docs commit; §4; was `READY`, orchestrator-opened in round af; RE, docs only).** Short follow-up to close OQ-175: read the body of the function `0x1000dd60`'s sweep found, and the method it calls through the layer-table pointer. Steps: (1) worktree `anno1503-T30b-3b`, branch `task/T30b-3b-430-reader` from `main` at a76dbad or later. (2) `python tools/disasm.py game/app/AnnoWelt.dll 10003780 400` — read the whole method to its `ret`. Table per instruction group: address range, what it reads from `[esi + …]` (the big class's fields), what it reads from the layer-table pointer `[esi + 0x430]` (bytes? a vtable? both?), what it passes to the virtual call `[eax + 0x128]` at `0x100037df` and what it does with the return; name the method's return value per path. The caller `[ebp + 0x40]` / `[ebp + 0x42]` (word) looks like a position pair — note which fields are tile vs. sub-cell. (3) Read the dword at `[*(ptr) + 0x128]`: the layer-table pointer at run time is zero-initialised by `0x10006362` and set by class X's slot `+0x80` = `0x1000dd20` to `0x100d1140 + 0x1c00·L`; the layer bytes do not contain a vtable, so what the big class actually dereferences at `[+0x430]` is **not** the raw table but an object whose constructor stashed the table bytes somewhere and whose vtable has a `+0x128` slot. Find that object's vtable (`xref.py --reloc` on the known vtable values of the big class, `0x100c14e0`, `0x100c14e4`, `0x100c14dc`; or follow the big constructor `0x10006344` writing a pointer into `[+0x430]`) and dump its `+0x128` slot. If the chain is more than one method deep, stop at D and name the next address. (4) **Also address the two round-af nits in `transport.md` §3.3.1**: (a) correct the sentence "the only register-load that treats the field as a pointer is `0x100037cb`" to acknowledge `0x100011e2 add edx, [ecx + 0x430]` inside `0x100011c0` (treats `+0x430` as a byte-buffer pointer, same shape as `0x100011c0`'s use of `+0x434`; its virtual call `[+0x128]` is on `[ecx + 0x454]`, so it is a different class or a different context — note which); (b) fix the row labels so every `[esi + k]` after `add esi, 4` is translated as `[X + k + 4]` (chunk-pool row: `[X + 0x10]`/`[X + 0x14]`; free-list row and the headline paragraph: `[X + 0x424..0x42c]`, not `[X + 0x420..0x42c]`; enqueue list head `[X + 0x418]`, not `[X + 0x414..0x418]`). (5) `transport.md` §3.3.2 "The real `[X + 0x430]` reader `0x10003780`": the table of step 2, the virtual method body (D with address and pseudo-code, or next address); OQ-175 status: resolved with the pseudo-code, or narrowed with the next address. (6) Report; acceptance: (a) `0x10003780` read to its `ret` with every `[esi + …]` read named and the return per path; (b) the virtual at `[+0x128]`: its vtable and body, or the next address with the reason the chain stopped; (c) the §3.3.1 label fixes and the `0x100011e2` sentence; (d) nothing of the original in git. · `PARALLEL-OK` with T30b.4 (T30b.3b: `docs/re/transport.md` §3.3.1 (edit) and new §3.3.2, OQ-175; T30b.4: `tools/orig/layer_blocks.py` and `transport.md` §3.4) · branch `task/T30b-3b-430-reader` from `main` at a76dbad or later · worktree `C:\Users\altin\anno1503-T30b-3b`.
  - **T30b.3c — The other `[reg + 0x430]` pointer-consumer `0x100011c0` (`DONE`, merged as c0dffaf on 2026-10-24 ah, PR #48, head 55a2975, CI 20/20; headline right, one D-tag buffer-tuple ordering nit corrected directly on `main` in the round-ah docs commit; §4; was `READY`, orchestrator-opened in round ag; RE, docs only).** Short follow-up to narrow OQ-175 further. The round-ag sweep confirmed exactly six `[reg + 0x430]` matches in `AnnoWelt.dll`: two writes (zero-init `0x10006362`, class-X setter `0x1000dd36`), two red herrings (`0x1000db0e` / `0x1000ed50` at `[X+4]+0x430 = [X+0x434]`), and two pointer-reads (`0x100037cb` in `0x10003780`, and `0x100011e2` in `0x100011c0`). `0x10003780` is read; `0x100011c0` is not. Steps: (1) worktree `anno1503-T30b-3c`, branch `task/T30b-3c-100011c0-reader` from `main` at 65907d9 or later. (2) `python tools/disasm.py game/app/AnnoWelt.dll 100011c0 200` — the whole method to its `ret` (ends at `0x1000123d`, `ret 4`; verified by the orchestrator in round ag). Table per instruction group: address range, what it reads from `[ecx + …]` (which fields, including `+0x430`, `+0x434`, `+0x454`, `+0x458`), what it writes to the stack buffer at `[esp + 0x10..0x24]`, what it passes to the virtual call `[edx + 0x128]` at `0x10001232` (five pushes? three? name each), what it returns. Note that `0x100011c0` reads `[ecx + 0x430]` and `[ecx + 0x434]` as byte-offset bases (adds them to a value), NOT as object pointers — this is unlike `0x10003780` where `[esi + 0x430]` is dereferenced through `[ecx]`. (3) The receiver class of `0x100011c0`: `xref.py` for callers of `0x100011c0` by direct-call opcode (`E8` disp32 pointing to it) in all four DLLs. Which class makes this call, and is it the same as `0x10003780`'s `Y` (the "bigger class" of ctor `0x10006344`), or a different one? If the same, the round-af nit's "0x100011e2 ... inside a different (bigger) class" sentence is right; if different, correct it. (4) The virtual call target: the receiver is `[ecx + 0x454]` with vtable at `[[ecx + 0x454]]`, slot `+0x128`. If the receiver class is known from step 3, read the dword at that class's `+0x454`-field-init in its ctor to find what sort of object sits there (if any writer stores a known value). If not, name the next address. (5) `transport.md` §3.3.2 end: a one-paragraph "Also `0x100011c0`" with (a) the field list, (b) the caller found (D with address, or named open), (c) the `[ecx + 0x454]` vtable (D with address, or named open), (d) one sentence on whether the "bigger class" sentence holds; OQ-175 narrowed or kept narrowed. (6) Report; acceptance: (a) `0x100011c0` read to its `ret` with every `[ecx + …]` read named and the return per path; (b) the caller named (D with address, or named open with reason); (c) nothing of the original in git. · `PARALLEL-OK` with T30b.4 (T30b.3c: `docs/re/transport.md` §3.3.2 end only; T30b.4: `tools/orig/layer_blocks.py` and `transport.md` §3.4) · branch `task/T30b-3c-100011c0-reader` from `main` at 65907d9 or later · worktree `C:\Users\altin\anno1503-T30b-3c`.
  - **T30b.3d — Cross-module sweep: are the layer tables consumed outside `AnnoWelt.dll`? (`DONE`, merged as 2d1112e on 2026-10-24 ai, PR #49, head 2f9bb45, CI 20/20; headline right, no D-tag corrections needed, executor over-delivered to eight DLLs where the spec said three; §4; was `READY`, orchestrator-opened in round ah; RE, docs only).** With the "layer tables may not be consumed in `AnnoWelt.dll` at all" theory as the leading hypothesis after T30b.3b, settle whether they are consumed in `AnnoGame.dll`, `AnnoScene.dll`, or `AnnoDisplay.dll`. Steps: (1) worktree `anno1503-T30b-3d`, branch `task/T30b-3d-cross-module-sweep` from `main`. (2) `python tools/xref.py --reloc game/app/AnnoGame.dll 0x100d1140 0x100d9d40`, same for `game/app/AnnoScene.dll` and `game/app/AnnoDisplay.dll`: print each hit (address, value, section). (3) Also sweep the three DLLs for `mov reg, imm32` / `cmp reg, imm32` with any of the five block base addresses `0x100d1140 + 0x1c00·L` (L = 0..4), in case the modules carry the addresses as immediates compiled from the shared header: `tools/xref.py <dll> 0x100d1140 0x100d1141` for each base, each dll. (4) If any hit: list its address and whether its instruction reads or writes. If none in all three: `transport.md` §3.4 gets "**No cross-module reference to the layer blocks** (`xref.py --reloc` on `AnnoGame.dll`, `AnnoScene.dll`, `AnnoDisplay.dll` returns zero; the sweeps for the base addresses in those modules return zero)" and OQ-175 updates: the walkability yes/no is **not** in the five layer blocks; the search engine or the figure's own cells must carry it. (5) Report; acceptance: (a) three `--reloc` runs and the five base-address sweeps, each with its command and count; (b) `transport.md` §3.4 updated; (c) OQ-175 narrowed or restated; (d) nothing of the original in git. · `PARALLEL-OK` with T30b.4 (T30b.3d: `docs/re/transport.md` §3.4 only, no new file; T30b.4: `tools/orig/layer_blocks.py` and `transport.md` §3.4 — the second to merge re-records the §3.4 edit) · branch `task/T30b-3d-cross-module-sweep` from `main` · worktree `C:\Users\altin\anno1503-T30b-3d` · no new OQ number.
  - **T30b.3e — The sub-cell bit reader on the search engine (`DONE`, merged as 0a35e5f on 2026-10-24 ak, PR #51, head 57c4eba, CI 20/20, docs only; headline right — search engine = class Y, vtable `0x100c1424` dumped, slot `+0xbc` = `0x10003780` the chunk-cache mark function; one counting nit (48 slots not 47, headline unchanged); the simple `and reg, 7` / `test reg, 7` grep did not find the walkability test — three next addresses named, T30b.3f is the natural next; §4).** With cross-module sweep (T30b.3d) confirming no outside references to the layer blocks and the three chunk-fetch readers (`0x1000daf0`, `0x10003780`, `0x100011c0`) only WRITING sub-cell bits (`(v & 0x8007 or 0xfc07) | 7`), the walkability yes/no is almost certainly a reader of those sub-cell bits somewhere else, with the search engine on `world + 0x6aa8` (vtable `0x100c1424`, OQ-161) the leading candidate. Steps: (1) worktree `anno1503-T30b-3e`, branch `task/T30b-3e-sub-cell-bit-reader` from `main` (T30b.4 merged in round aj as a5f459e). (2) Dump vtable `0x100c1424` with `tools/disasm.py` or the slot-reading pattern used in round af (`pefile` to pull the dwords at RVA = `0x100c1424 - 0x10000000`); list all code slots (not placeholders). (3) `xref.py --reloc game/app/AnnoWelt.dll 0x100c1424 0x100c1428` to find who installs the vtable (expected the search engine's constructor) and `xref.py --reloc game/app/AnnoWelt.dll 0x6aa8 0x6aa9` is NOT right (6aa8 is a displacement, not an address); use `xref.py game/app/AnnoWelt.dll <search-engine-vtable-slot>` once a candidate expand/step method is identified. (4) For each promising slot (names suggest: step, expand, is_passable, cost): read the method body with `tools/disasm.py` to its `ret`; look for the pattern `and <reg>, 7` or `test <reg>, 7` (bits 0..2) on a value loaded from a chunk pointer; also look for `and <reg>, 0x8000` (bit 15, preserved by both masks) and `and <reg>, 0x7c00` (bits 10..14, preserved by `0xfc07`). The first match is a strong candidate for the walkability tester. (5) If found: `transport.md` §3.4 or new §3.5 ("The sub-cell bit reader") names the method address, the test pattern, and the walkability semantics (if `bits 0..2 == 7` means "any walk class passable", or if the test is bit-indexed by walk-class; OQ-160 and the walk-class table of T30b.1 inform this). If not found after reading the three or four most promising slots: name the next address and stop (OQ-175 narrowed, not closed). (6) Report; acceptance: (a) vtable `0x100c1424` dumped and installer named; (b) at least three expand/step/is_passable candidate slots read to their `ret` with the sub-cell-bit tests named per slot (or "none found" per slot); (c) `transport.md` updated with the result (D or the next address); (d) OQ-175 narrowed or closed; (e) nothing of the original in git. · `PARALLEL-OK` with T30b.5+ (T30b.3e: `docs/re/transport.md` §3.4/§3.5 only, no new file) · branch `task/T30b-3e-sub-cell-bit-reader` from `main` · worktree `C:\Users\altin\anno1503-T30b-3e` · new OQ number from **OQ-179** (OQ-178 is the last T30b.3-family one; OQ-176/177 are earlier T30b sub-tasks).
  - **T30b.4 — Step 7(a): the five layer blocks in the running original (`DONE`, merged as a5f459e on 2026-10-24 aj, PR #50, head d0e0d0d, CI 20/20; headline **D** — all five blocks `equal` in both passes, dead-data confirmed; no D-tag corrections needed; §4; was `READY` as of round ai; a Wine run; tool under `tools/orig/`).** (1) worktree `anno1503-T30b-4`, branch `task/T30b-4-layer-memory-compare`. (2) Read `docs/testing/original-wine.md` and `tools/orig/live.py` (`main`), `tools/orig/backend.py` (`read(addr, n)`), `tools/orig/gamestate.py` (how it finds `AnnoWelt.dll`'s base and reads). (3) Write `tools/orig/layer_blocks.py`: launches or attaches as `live.py` does, waits for a scenario to be loaded (reuse `tools/orig/opening.py`'s way of loading the opening), reads the five 7168-byte blocks at module base + RVA (`0x100d1140 − 0x10000000 + 0x1c00·L`), compares each with the bytes at the same RVA in `game/app/AnnoWelt.dll` on disk (`pefile`, as `tools/disasm.py` does), and prints per block "equal" or the count of differing bytes and the first ten offsets; writes nothing but that summary to `cache/orig/layer_blocks.txt`. Run it twice: right after the scenario loads and after 10 game minutes (the harness's speed and wait helpers in `tools/orig/`). (4) `transport.md` §3.4: the result (D: equal, or the differing offsets as counts). (5) Report; acceptance: (a) the tool committed, (b) both runs' summary lines pasted, (c) no block content in git (counts and offsets only), (d) the Wine run was silent (music and sounds off, `docs/testing/original-wine.md`).
  - **T30b.3f — Deeper read of the three search-core functions for a walkability branch on a chunk-pointer load (`READY` as of round ak — T30b.3e merged; RE, docs only; medium; live-lane next).** With T30b.3e proving the search engine on `world + 0x6aa8` IS class Y (vtable `0x100c1424`, installed by the big-class constructor `0x10006344`) and the simple `and reg, 7` / `test reg, 7` grep finding only MSVC's signed-mod-8 for coordinates in the 28 non-stub slots, the walkability yes/no must still live inside the search's own work. Three functions are named: `0x10003d90`, `0x10004890`, `0x10005890` (slot `+0xa4`). The orchestrator's re-verification already saw concrete grep targets: three `and reg, 0x80000003; or reg, 0xfffffffc` sequences in `0x10003d90` (signed `mod 4` with sign preservation — likely sub-cell x/y wraps within a chunk of 4 units), and 16-bit reads `mov cx, word[eax+0x40]`, `mov cx, word[eax+0x42]`, `mov cx, word[esi]` scattered through the three bodies. Steps: (1) worktree `anno1503-T30b-3f`, branch `task/T30b-3f-search-core-walkability` from `main` (T30b.3e merged in round ak as 0a35e5f). (2) Disassemble each of `0x10003d90`, `0x10004890`, `0x10005890` from start to its own `ret` (`tools/disasm.py` with a generous length; the T30b.3e write-up says `0x10005890` is ~700 bytes = ending near `0x10005b49`, use the same `ret N` trick to find each function's end). Record each function's length and its direct callees (`call rel32` targets) in the write-up. (3) Grep EACH body for the full x86 family of a 1-of-N tag / flag test, not just one pattern: `and reg, imm`, `test reg, imm`, `test byte ptr [reg+k], imm`, `test word ptr [reg+k], imm`, `cmp byte ptr [reg+k], imm`, `cmp word ptr [reg+k], imm`, `movzx r, byte/word[reg+k]` immediately followed by `test r, imm` or `cmp r, imm` or `and r, imm`, and conditional branches (`jz`/`jnz`/`js`/`jns`/`jb`/`ja`) on flags set by any masked load from a chunk pointer (a chunk pointer being any register last loaded from `[+0x4?8]`, from `[world + 0xec]`, or from the return value of `0x100abdb0` / `0x100a52d0`). For each hit, record address, instructions, which register is the chunk pointer, and whether the branch leads to a "cell is passable" or "cell is not passable" code path. (4) If a walkability test is found: `transport.md` §3.4.4 "The walkability bit reader in the search core" (or §3.5, pick a free number) names the function, the exact instruction window, the sub-cell field offset (`+0x40`, `+0x42`, …) and the mask semantics (`bits 0..2`? `bit 15`? `bits 10..14`?); update OQ-175 as **D** with the field layout. (5) If no walkability test is found in all three bodies: name a specific next function (one of the direct callees named in step 2, or the chunk-fetch handlers T30b.3g covers) and stop; update OQ-175 as still narrowed. (6) Report; acceptance: (a) each of the three functions disassembled end-to-end with lengths and direct callees named, (b) each grep family's hits enumerated per function (zero per family counts, with the empty count stated), (c) `transport.md` updated, (d) OQ-175 narrowed or closed, (e) nothing of the original in git. · `PARALLEL-OK` with T30b.5+ (T30b.3f: `docs/re/transport.md` §3.4/§3.5 only, no new file) · branch `task/T30b-3f-search-core-walkability` from `main` · worktree `C:\Users\altin\anno1503-T30b-3f` · new OQ number from **OQ-179** (OQ-178 is the last T30b.3-family one assigned; T30b.3e did not consume OQ-179).
  - **T30b.3g — The world chunk-fetch handlers as walkability pre-filters (`DROPPED` round an: `0x100a52d0` is the object-at-tile lookup, `0x100abdb0` the block copy; the fill chain is T30b.3k; was `READY` fallback; only run if T30b.3f draws a blank; RE, docs only; small).** The world's chunk-fetch handlers `0x100abdb0` (round ae, class X path) and `0x100a52d0` (round ag, from the virtual call at `0x100037df`) are the per-kind dispatchers the chunk-fetch readers (W/X/Y) land on through `[edx + 0x128]`. If the walkability yes/no is not inside the search core (T30b.3f), the alternative is that the chunk-fetch handlers themselves FILTER by walkability as they fill the caller's 4-dword buffer: an unwalkable cell gets a sentinel (`0xffff`? `0x8000`? `-1`?) and the search's subsequent sub-cell mark skips any cell whose fetched value matches. Steps: (1) worktree `anno1503-T30b-3g`, branch `task/T30b-3g-chunk-fetch-walk-filter` from `main`. (2) Disassemble `0x100abdb0` and `0x100a52d0` end-to-end with lengths and direct callees. (3) Grep each body for the same x86 test family T30b.3f uses, plus `mov [reg+k], imm` / `mov word[reg+k], imm` of likely-sentinel values (`0xffff`, `0xfffe`, `0x8000`, `0x7fff`, `-1` = `0xffffffff`): any store to a dword/word of the caller's output buffer that writes an unusual constant is a candidate for the "cell is not walkable" sentinel. (4) Correlate with the §3.3 / §3.3.1 / §3.3.2 chunk-fetch readers' buffer layout: the four stores land `(x0, y0, x1, y1)` — any additional field beyond those four, or an alternative value written when a condition is met, is a sign of a filter. (5) If a sentinel-write is found: `transport.md` §3.4.5 names the handler, the condition, the sentinel value, and the matching reader (which search function tests for it); OQ-175 updates. (6) If none: either T30b.3f's result closes OQ-175, or `transport.md` §3.4.5 names the next unknown and OQ-175 stays open with the field left for a later task. (7) Report; acceptance same as T30b.3f items (a)–(e). · `PARALLEL-OK` with T30b.3f (different functions, same `transport.md` sections — second to merge re-records the §3.4 edit if both touch it) · branch `task/T30b-3g-chunk-fetch-walk-filter` from `main` · worktree `C:\Users\altin\anno1503-T30b-3g` · new OQ number from the next free after T30b.3f.
  - **T30b.3i — The search's main loop and the node record, with absolute `Y` offsets · `DONE` (PR #54 merged in round ao; `transport.md` §3.4.7).** Read `transport.md` §3.4.6 first. Branch `task/T30b-3i-main-loop`, worktree `anno1503-T30b-3i`. Steps: (1) `python tools/disasm.py game/app/AnnoWelt.dll 10003d90 2800` — fn1 whole (its `ret` is at `0x10004867`); first table row: fn1's `this` = `Y + 4` (slot `+0x84` of `0x100c14e4`, installed at `[Y + 4]` by `0x10006375`), and which register holds it where (`esi` after `mov esi, ecx`; find where `edi` is loaded before the loop at `0x1000475a` and state what `edi` is at `0x1000474f`). (2) Table of fn1's fields as absolute offsets: `[this + 0x418]` (the open list's sentinel), `+0x420` (open count), `+0x424` / `+0x428` / `+0x42c` (the free list and its count), `+0x438` (the byte set to 1 at `0x10003db1`), `+0x444` (the best score, `0x10003dbf`), `+0x4f0` / `+0x4f4` (the array fn1 walks first), each as `Y + n`. (3) The node record: from fn2's writes (`0x100048f8..0x10004907`) and the tail's list operations, a table of every node field (`+0`..`+0x1c`; size 28 or 32 bytes — the allocation at `0x100048c6` pushes `0x1c`), its meaning, who writes it, who reads it. (4) The loop body as pseudo-code: the bit-10 branch (`0x10003220`, the `+0x438` test, the two exits), the code switch (`0x3ff`, `0x80..0x86`, the jump table `0x10004868` dumped as 7 dwords with `pefile`, the five targets), the common tail (`0x10001480`'s arguments, the counters). (5) What fn1 returns on each exit (`0x10004855` and the others), and what it does before the loop (`0x10003d90..0x1000474f`: the array walk at `+0x4f4`, the `0x100064b0` / fn2 calls at `0x10003e7f` / `0x10003e8d` — the start nodes?). (6) Docs: `transport.md` §3.4.7 "fn1 as pseudo-code" with the three tables; OQ-175 "the loop" paragraph; REPORTS T30b.3i; acceptance: (a) the absolute-offset table, (b) the node table, (c) the loop pseudo-code with every address, (d) fn1's pre-loop part described, (e) nothing of the original in git; PR, not merged.
  - **T30b.3j — The default expansion `0x10006a40` to its `ret`: the walk rule of an ordinary step · `DONE` (PR #55 merged in round ao; `transport.md` §3.4.8).** Branch `task/T30b-3j-default-expansion`. Steps: (1) `python tools/disasm.py game/app/AnnoWelt.dll 10006a40 900` to the `ret` (find it; the function is long); first row: its `this` (fn1 calls it with `lea ecx, [edi + 4]`; use T30b.3i's answer for `edi`) and so the absolute `Y` offset of `[this + 0x42c]` and of every other `[this + k]` it reads. (2) Table per neighbour: the four (or eight) neighbour cells it visits (`cell − 0x10` = one row up, `cell + 0x10` one row down, `cell ± 4` left / right; the chunk edges — how does it reach the neighbour chunk when `x` or `y` is 0 or 3? through `0x100064b0`?), the test on the neighbour's low u16 (`0x2111` for up; the masks for the other directions), the byte-table test (`[table + code] & dl`, what `dl` is per direction), what happens to a neighbour that passes (cost added — which constants —, `0x100064b0` + fn2 to enqueue? a score, the `+0x444` best), and what fails. (3) The top nibble of the low u16 (`bx >> 12`): every use; the meaning of its bit 3 (it chooses `dl = 0x40`). (4) Pseudo-code of the whole function with addresses. (5) Docs: `transport.md` §3.4.8 "The default expansion"; OQ-175 and OQ-161 (the search expansion) updated; `state.md` §5.9's "Proposed rule for T33" paragraph: append one DRAFT sentence per decoded part, tagged D with the address; REPORTS; acceptance: (a) `this` and the absolute offsets, (b) the per-neighbour table, (c) the costs and the enqueue, (d) pseudo-code, (e) nothing of the original in git; PR, not merged.
  - **T30b.3k — The chunk fill: from the island's tile record to the 4-byte cell · `DONE` with a correction (PR #56 merged in round ao; `transport.md` §3.4.9 and the orchestrator's §3.4.11: the fill call goes through `[Y + 8]`, slot 0 = `0x10003780`, not `[Y + 4]`; steps 1, 2 and 4 are re-opened as T30b.3m and T30b.3n).** Branch `task/T30b-3k-chunk-fill`. Steps: (1) `0x100064b0`'s fill call at `0x10006694..0x10006697` (`mov eax, [ecx]; push edi; call [eax]`, `ecx = Y + 4`): read slot 0 of the vtable at `[Y + 4]` with `pefile` (`0x100c14e4` slot 0 = `0x100b4a20` per the round-ak dump; if that is a stub, the real target is a different vtable: check what `[Y + 4]` holds — `0x10006375` installs `0x100c14e4`, but a derived constructor may overwrite it; `xref.py --reloc` for writes of a vtable into `[reg + 4]` inside class Y's constructors `0x10006344` and `0x100066c0`). Disassemble the target to its `ret`: how it turns the chunk's `(x, y)` at `+0x40` / `+0x42` into the world call (`[Y + 0x430]` = the world; which slot: `+0xec` = `0x100abdb0` with which kind, or `+0x128`, or another), and where the 64 bytes land. (2) The world handler for that kind (§3.3 lists `0x100abd10` / `0x100abc60` / `0x100abaa0`): the copy routine it calls (`0x100900c0`, `0x10090350`, or `0x100a4730`): disassemble it to its `ret`; how it reads the island's tile records and what it writes per cell. (3) The cell builder of §3.1 (b), `0x1008ff70` (`cell = table[mask] | byte << 16`, mask = 9 bits at `16..24` of the tile record's second dword through `0x100c1af8`, byte = bits `8..15`): confirm it is on the path of step 2 (or name the builder that is), and write the cell's bit map: low u16 = the mapped mask (the 33 values; which of them carry the `0x2111` bits and what the top nibble is), high byte = the tile byte (the codes `0x80..0x86` are tile bytes of which kinds?), where bit 10 of the high u16 and the mark bits come from (another builder mode? `0x10091fa0(mode, …)` of §3.1). (4) The island tile record: who writes the 9 bits at `16..24` and the byte at `8..15` of the second dword — the island loader's set-tile path (`0x10087b90`, `0x100878d0`, §3.2 "Open") and the Place path for a building: from which `Anlagen` fields (HEAD `flags_word` bits `0x4000` / `0x1000`, `WALKMASK`, PRODTYPE) they come. This is where the dirt-road question is decided: state for a road tile of type `2527` (flags `0x1186`) and one of `2520` (`0x4186`) which 9-bit mask and which byte the loader writes, D with the address, or exactly where the chain stopped. (5) Docs: `transport.md` §3.4.9 "The fill chain"; OQ-171 answered or narrowed with the address; OQ-160 the "Update" bullet; `state.md` §5.9 DRAFT sentences; REPORTS; acceptance: (a) the slot-0 target and the world call, (b) the copy routine, (c) the cell bit map with the builder's address, (d) the loader's writes for the two road types, or the stop address, (e) nothing of the original in git; PR, not merged.
  - **T30b.3l — `0x10003220` (the bit-10 branch) and the four special expansions · `DONE` (PR #57 merged in round ao; `transport.md` §3.4.10; the "likely tile kind" reading withdrawn in §3.4.11 (f): the codes are came-from marks).** Branch `task/T30b-3l-special-expansions`. Steps: (1) `0x10003220` to its `ret`: its `this` (fn1 calls it with `ecx = edi`), what it reads (`[this + 0x574]`, `[this + 0x4e8]`, the node, `[chunk + 0x40]` → it converts a position to a float at `0x10003269`: likely the goal test or the "arrived" callback of the search; say which from what it compares), what it returns. (2) The four special expansions `0x100071c0`, `0x10007770`, `0x10007b00`, `0x10007ea0` (codes `0x86`, `0x82`, `0x80`, `0x84`): each to its `ret`, a table of what differs from `0x10006a40` (which neighbours, which tests, which costs); a guess at the kind of tile each code stands for (ramp, bridge, pier, entrance) is marked "likely" and tied to T30b.3k's byte map. (3) Docs: `transport.md` §3.4.10; OQ-175; REPORTS; acceptance: (a) `0x10003220` described with its return values, (b) the four-function table, (c) nothing of the original in git; PR, not merged.
  - **T30b.3m — The fill `0x10003780` end to end, the layer table at `[Y + 0x434]`, the top nibble, and the came-from marks · `DONE` (PR #59 merged as 4cb3997 on 2026-10-25, CI 20/20; `transport.md` §3.4.12; round ap review).** Read `transport.md` §3.4.11 first (the orchestrator's correction) and §3.4.7–3.4.10. Branch `task/T30b-3m-fill-and-table` from `main` at 5e9a8fc or later, worktree `anno1503-T30b-3m`. **`this` for this sub-task (D, §3.4.11 (a)):** `0x10003780`, `0x100064b0`, `0x10006a40`, `0x10006f70`, fn2 and the four special expansions run with `this = Y + 8` (interface `[Y + 8]` = `0x100c14e0`); fn1 and `0x10003220` with `this = Y + 4`; `0x10005890` with `this = Y`. Write every field as `Y + n` from the start. Steps, each written down before the next starts:
    1. **`0x10003780` to its `ret 4` at `0x10003a91`** (`python tools/disasm.py game/app/AnnoWelt.dll 10003780 800`). Table A, the fields: `[this + 0x4e4]` / `+0x4e8` (added to `[chunk + 0x40]` / `+0x42`), `+0x430` (the island; `call [[ecx] + 0x128]` at `0x100037df`), `+0x2038` / `+0x203c` (the first two arguments), `+0x4e3` (the byte tested at `0x100037eb`), `+0x454` / `+0x458` / `+0x45c` / `+0x460` and the byte table at `+0x464` (second half) — each as `Y + 8 + k`. Table B, the rect: the four dwords written at `0x100037ae..0x100037d8` (which `[esp + ?]`, which value: `x`, `y`, `x + 4`, `y + 4` — give the order as the callee sees them). Table C, the island call's arguments in callee order (count the five pushes at `0x10003799`, `0x100037ad`, `0x100037b6`, `0x100037d1`, `0x100037de`).
    2. **Island slot `+0x128` = `0x10091fa0`** (`disasm.py … 10091fa0 900`): dump the jump table `0x10092190` as 8 dwords (`pefile`); for mode 0 (`0x10091fba`) and mode 6 (`0x100920ee`) say which stack slots become `0x1008ff70`'s three stack arguments (`[esp + 0x1c]` / `+0x20` / `+0x24` after the four pushes = the slot's arguments 2, 3, 4 = rect, dest, width). One line per other mode (1–5, 7): the callee's address only; do not read them.
    3. **`0x1008ff70` to its `ret 0xc` at `0x100900b3`.** First row: `this` = the island (`ecx`); `[this + 4]` / `+6` / `+8` / `+0xa` (the island's tile rect: say which is x, y, w, h from how `0x1008ff7a..0x1008ff9f` clips the argument rect), `[this + 0xc]` = the 8-byte tile records; the two vtable calls `[eax + 0x50]` / `[eax + 0x54]` at `0x1008ffb9` / `0x1008fffe`: name the slots' targets with `pefile` on the island vtable `0x100cac2c` and read each only to its `ret` to say what it returns (one line each). Then pseudo-code with addresses: the clip (`0x1008ff89..0x1008fffa`), the dest row pointer (`lea edx, [edx + eax*4]` at `0x1008ffcd`; `imul eax, [esp + 0x24]` + `lea` at `0x1009000e..0x1009001a`: a cell is 4 bytes, a row is `width` cells), the record pointer (`lea ecx, [ecx + eax*8]` at `0x1009002e`), the inner loop `0x10090050..0x1009007b` (`cell = u16[0x100c1af8 + ((rec >> 16) & 0x1ff)·2] | ((rec & 0xff00) << 8)`). State the fill-time cell bit map: bits 0–11 mapped mask (the 33 values, §3.4.9), 12–15 zero, 16–23 the record's byte 8–15 (= the `code` of §3.4.7–3.4.8), 24–31 zero. Say what the cells of tiles outside the island's rect hold (the `rep stosd` zero of `0x100065af`, or something the clip writes).
    4. **The second half of `0x10003780`** (`0x100037e5..0x10003a8a`) as pseudo-code with addresses: the enabling byte `Y + 0x4eb`; the two passes over rows (`0x100038ea..0x10003987` and `0x100039e5..0x10003a84`); the bounds from `Y + 0x45c` / `0x460` / `0x464` / `0x468` and the byte table `Y + 0x46c + i` (`mov bl, [edx + esi + 0x464]` at `0x1000383b`); compare them with the disc fields of §1.1 / OQ-160 (`disc_half_widths`): are they the same `Y` offsets (write §1.1's as absolute offsets if it has them); the mark `(hi & 0xfc07) | 7` on the high u16 (`0x1000393e..0x10003948`). Then, with `pefile`, read byte index 7 of each of the five layer blocks `0x100d1140 + 0x1c00·L` (L = 0..4): if all are 0, code 7 (`table[7] & dl = 0`) is "never admitted" = closed, and the disc pass closes the disc's cells when a chunk is first filled — say so, or say what the bytes are. Who writes `Y + 0x4eb` and `Y + 0x45c..0x468`: one capstone sweep of `.text` for stores to `[reg + 0x4eb]` / `[reg + 0x4e3]` / `[reg + 0x45c..0x468]` / `[reg + 0x454..0x460]` (both `this` conventions), the script's pattern and its hits in the report (addresses only).
    5. **The table `[Y + 0x434]` and the mode `[Y + 0x2044]`.** The orchestrator's sweep found `mov [esi + 0x434], edi` at `0x10002b9e`, `mov [esi + 0x203c], ecx` at `0x10002b92`, `mov [esi + 0x2040], eax` at `0x10002b98`, also `[esi + 0x434]` writes at `0x100012ce` / `0x100012d6`, and `cmp [ebp + 0x2044], 6` at `0x1000576e` (in `0x10005890`, `this = Y`). (a) Walk backwards from `0x10002b92` to the function's prologue (the previous `ret`/`int3` padding), disassemble it to its `ret`, find how it is reached (`xref.py` on its start; if zero code xrefs, `--reloc` for a vtable slot and the installer), and write its `this` as `Y + n`. With that `this`, which absolute fields are `[esi + 0x434]`, `[esi + 0x203c]`, `[esi + 0x2040]`? If `this = Y` they are the expansion's table `Y + 0x434`, and `Y + 0x203c` / `Y + 0x2040` — then `0x10003780`'s `[Y + 0x2044]` (mode) is NOT written there; say so and find the `0x2044` writer separately (the same capstone sweep for `[reg + 0x2044]` and, for `this = Y + 4` / `Y + 8` callers, `[reg + 0x2040]` / `[reg + 0x203c]` — list every hit with its function and `this`). (b) What `edi` is at `0x10002b9e`: trace it back inside that function (an argument? `[esp + ?]` — count the pushes; a global? the layer blocks are `0x100d1140 + 0x1c00·L`, the setter `0x1000dd20` of §3.1 writes `[X + 0x430]` of the X-class engine: is this the Y-class twin of that setter, and is the value one of the five block bases or the figure's `layer` times `0x1c00` plus the base?). (c) If `[Y + 0x434]` is a layer block: §3.4.8's `table[code]` is `L[code]` (bytes 0..0x3ff) and `table[0x400 + code·8 + dir]` is `L[0x400 + code·8 + dir]`; with `pefile`, for layer 3 (the market cart, `0x100d1140 + 0x5400`) and layer 0, print `L[code]` for the codes the T30b.3k mapper and the tile byte can produce (the report lists the codes seen in the five blocks' non-zero admission bytes: count per block, and the eight cost bytes of the first three non-zero codes). This answers §3.1's "meaning of an entry" (OQ-160) — write it there as D with the addresses, or stop with the question. (d) The mode: the value `[Y + 0x2044]` holds when `0x10003780` runs (0 → `0x1008ff70`; 6 → `0x100920ff`'s call of the same builder with other arguments — read mode 6's argument wiring in step 2): who writes it, D with the address, or the stop address. `tools/orig/layer_blocks.py` exists; extend it for the byte reads instead of writing a second script, and paste the command and its first lines (counts only).
    6. **The top nibble of the low u16 (the admission key).** §3.4.8: `bx = low_u16[cell] >> 12` (`0x10006a6b..0x10006a70`), `dl = (nibble & 8) << 3` for up (`0x10006a9f..0x10006aa4`), and `test byte [table + code], dl; je skip` (`0x10006ab0..0x10006ab3`). The fill leaves bits 12–15 zero (step 3), and `dl = 0` makes the `test` set ZF, so a cell with nibble 0 admits no neighbour. Find who sets bits 12–15 of a cell's low u16: (a) fn1's start-cell `or word [esi], ax` at `0x10003f73`, `ax` = the return of `0x10003aa0` (`disasm.py … 10003aa0 600` to its `ret`: its `this` and two arguments at `0x10003f6c..0x10003f6e` — the dwords of a first-array entry —, the island `+0x128` call at `0x10003b17` with a stack dest (which mode, which rect, which width), `0x1000d950`, `0x10018740`, and what ends in `ax`: which bits, is bit 12–15 among them); (b) the pre-loop `and word [eax + edx*4], cx` at `0x10004737` (the stride-`0xc` array at `Y + 0x1c0c`: what `cx` is — `[esp + 0x3c]` — and which bits it clears); (c) whether any expansion writes a neighbour's LOW u16 (§3.4.11 (f): the three marks are on the high u16 — confirm for all four directions of `0x10006a40` and for `0x10006f70` by listing every `mov word ptr [...]` store in those two functions with its cell offset `+0` or `+2`); (d) `0x10003220`'s writes (none expected) and `0x10004068` / `0x10004256` / `0x10004513` (`or byte [cell + 3], 4` = bit 10 of the high u16; do any also write the low u16?). Conclude D with addresses: "the nibble of a cell is written by … and means …", or stop with the exact question and the candidates ruled out. If the answer is that only start cells carry a nibble and the expansion reads the CURRENT cell's nibble, then the current cell is always a start cell or a cell written by …: write out that consequence and check it against the pseudo-code (does the search ever expand a non-start cell?).
    7. **The came-from reading (§3.4.11 (f)).** Confirm with the addresses: the up-mark `(hi & 0xfc86) | 0x86` at `0x10006adb..0x10006ae5` targets `cell_up + 2`; the down-mark at `0x10006cbe..0x10006cc8`; `0x10006f70`'s `0x10006fb4..0x10006fc4` (`dx = (dir + 0x80) ^ hi; dx &= 0x3ff; dx ^= hi` = replace bits 0–9 by `0x80 + dir`; what `dir` is for the left and right callers: the `push` before the call at `0x10006e6a` / `0x10006ec3` / `0x10006f1b`). Then: which bits of the high u16 survive each mark (`0xfc86` keeps bits 15–10, 7, 2, 1 — bits 7, 2, 1 are then set anyway; `0xfc82`; the `0x3ff` replacement keeps bits 15–10) — so bit 10 (the goal flag) and bits 11–15 survive every mark; say what bits 11–15 are if anything writes them. Rewrite §3.4.10's table: column "Missing" = "the direction back to the parent", with the pairing `0x86 ↔ up-step`, `0x82 ↔ down`, `0x80 ↔ left`, `0x84 ↔ right` D by the mark addresses; keep the four functions' first-test addresses. State that `0x81` / `0x83` / `0x85` are unreachable from marks (`dir` even) and what a fresh tile byte of `0x80..0x86` would do (it would be mistaken for a mark — check with step 5 (c) whether any admission byte is non-zero for codes `0x80..0x86`; if all zero, such tile bytes are never admitted and the question is moot).
    8. **Docs:** `transport.md` §3.4.12 "The fill, the layer table and the marks (T30b.3m)" with tables A–C, the two pseudo-codes, the disc pass, the table finding, the nibble finding, the came-from confirmation; §3.1's "meaning of an entry" and §3.4.10's table corrected in place (strike-through, not deletion); OQ-160 (the entry meaning), OQ-175 (status line: one line, replace), OQ-161 (the expansion order, the costs, the came-from marks); `state.md` §5.9: one DRAFT sentence per decoded part, each with its address; REPORTS T30b.3m with the steps ticked and the address ranges read. Acceptance: (a) tables A–C, (b) `0x1008ff70` pseudo-code and the fill-time bit map, (c) the disc pass and the byte-7 check, (d) the writer of `[Y + 0x434]` and whether it is a layer block, and the mode's writer (each D with the address or the stop address), (e) the nibble's source (D or the question with the candidates ruled out), (f) the came-from confirmation and the corrected §3.4.10 table, (g) nothing of the original in git (`git status` shows nothing under `game/`); PR, not merged. No `src/` change, no build.
  - **T30b.3n — The tile record's walk bits: who writes bits 8–15 and 16–24 of the second dword for a road, a building footprint, open ground and water · `DONE` (PR #61 merged as a5af265 on 2026-10-25, CI green; round ar; `transport.md` §3.4.13; OQ-171 answered: the `0x1000` roads are admitted; RE, docs only). Carry the round-ap reading of the admission groups (bits 3–6 of a byte ≥ 128 = the four orthogonal connections, P) as a hypothesis to test in step 3: for the straight, corner and junction road pieces, do the written bytes' bits 3–6 match the piece's open sides?** Branch `task/T30b-3n-tile-record-writer` from `main` after the T30b.3m merge, worktree `anno1503-T30b-3n`. Steps:
    1. **The record and its readers.** From T30b.3m step 3: the island's tile records are 8 bytes at `[island + 0xc] + (y·w + x)·8` (`w` = `[island + 8]`), and `0x1008ff70` reads the second dword (`+4`). Write the record's known layout from `docs/re/islands.md` / `docs/re/annowelt.md` / `docs/formats/insel.md` (cite the section; what the first dword is, if known) and list the other readers of the second dword that §3.1 names (`0x10090a40` for mode 2 of `0x10091fa0`, the kind-9/10/11 copies `0x100900c0` / `0x10090350` / `0x100a4730`): one line each, which bits they read.
    2. **Find the writers (sweep).** One capstone script over `AnnoWelt.dll` `.text`: for every function (split at `int3`/`ret`+padding, as `tools/xref.py` or `disasm.py` do), find a load of `[reg + 0xc]` followed within 60 instructions by an addressing with scale 8 (`[… + reg*8 …]`) and then a store (`mov`, `or`, `and`, `xor` with a memory destination) through a register derived from it; print function start, store address, operand, and the immediate masks seen in between (`0x1ff`, `0xff00`, `shl …, 0x10`, `shl …, 8`, `0x1ff0000`, `0xffff00ff`, `0xfe00ffff`). Also run the sweep with the island vtable as a filter: dump every slot of `0x100cac2c` (`pefile`, until the first non-`.text` dword) and mark which of the hits are slots. Report the hit list (addresses, masks; expect fewer than twenty). If the sweep returns nothing, loosen it (scale 8 within 120 instructions) and say so.
    3. **The object placer.** Among the hits, the function that takes an object or an `Anlagen` type and writes the second dword for the tiles of a footprint: read it to its `ret`. Name with offsets (from `docs/formats/anlagen.md`, cite the field table) which type fields it reads — HEAD `flags_word` (`0x4000` / `0x1000`), `WALKMASK` (12 bytes: how are they indexed by the footprint's `(dx, dy)` and the rotation?), PRODTYPE, the footprint size — and the bit operations that build the byte (bits 8–15) and the 9-bit mask (bits 16–24). Pseudo-code with addresses. Then the predicted record for a 1×1 road tile of type `2527` (flags `0x1186`, WALKMASK all zero) and of type `2520` (`0x4186`): the byte and the mask, D with the address of each instruction that contributes. If the writer reads a field of the live object instead of the type (a rotation, a variant), say which and what the opening's roads have.
    4. **The terrain writer.** The second dword of a tile without an object (grass, coast, water, a river piece): the island loader's loop that converts the `.scp` tile layer into the 8-byte records (`docs/formats/insel.md` for the file's tile bytes; `docs/re/islands.md` for the loader, if read): which file byte becomes the byte and the mask; D with addresses, or the stop address. The predicted record for open grass, open water and a coast tile.
    5. **The predicted cell table for T30b.3h2.** One table: tile kind (dirt road `2527`, cobblestone `2505`, the `0x4000` road `2520`, open grass, water, coast, a 2×2 house footprint tile, a market's entrance tile) → record byte, record mask, `0x100c1af8[mask]` (the low u16), code (= the byte), and — with T30b.3m step 5 (c)'s table — the admission byte `L3[code]` (layer 3, the cart) and `L0[code]` (layer 0, the carrier) and the four cost bytes. Each row D with the writer's address or "stop: <address>". From that table answer OQ-171: are the `0x1000` roads admitted for layer 3 (the dirt-road frontier of T29)? And OQ-170's tile side: do water and river tiles get a code whose admission byte is zero for every layer?
    6. **Docs:** `transport.md` §3.4.13 "The tile record's walk bits (T30b.3n)"; OQ-171 and OQ-170 status lines (one line each, replaced); OQ-160 "Update"; `state.md` §5.9 DRAFT sentences with addresses; REPORTS T30b.3n. Acceptance: (a) the sweep's hit list, (b) the object placer's pseudo-code and the two road predictions, (c) the terrain writer and its three predictions (or the stop address), (d) the predicted cell table with the admission and cost bytes, (e) OQ-171 answered or the exact stop, (f) nothing of the original in git; PR, not merged. No `src/` change, no build. `tools/anlagen_roads.py` and `tools/orig/layer_blocks.py` are extended, not duplicated, if a table is computed from the files.
  - **T30b.3o — The four special expansions to their `ret`, and the constraint mask `0x10008230` · `DONE` (PR #65 merged on 2026-10-25, CI green; round aw; `transport.md` §3.4.14 and the combined table in §3.4.10; RE, docs only).** Branch `task/T30b-3o-special-expansions`, worktree `anno1503-T30b-3o`. Read `transport.md` §3.4.8 and §3.4.12 first; `this = Y + 8` for all four (`lea ecx, [edi + 4]` in fn1), the table is `[Y + 0x434]`. Steps, each written down before the next:
    1. **`0x100071c0` (code `0x86`, reached going up) to its `ret 4` at `0x10007533`** (`disasm.py … 100071c0 900`): pseudo-code with addresses in the shape of §3.4.8 — per neighbour (left, right, up: the order the report gives) the cell address, the low-u16 bits it tests (`0x100071f3` shifts the low u16 by 9: which 3-bit group, and what each bit means against the mapper's four groups `0x111` / `0x222` / `0x444` / `0x888` of §3.4.9), the admission test (`L[code] & key`: where `key` comes from for a reached cell — the parent's direction, the cell's own low-u16 group, or a constant), the cost (`0x1000723d..0x10007253` reads two cost bytes and subtracts: write the formula, the two indices and which `dir`), the mark it writes, and the enqueue. State what makes it differ from the default besides the omitted direction.
    2. **The other three** (`0x10007770` code `0x82`, `0x10007b00` code `0x80`, `0x10007ea0` code `0x84`): the same table each; then one combined table "direction of arrival → bit group read (shift 0/3/6/9) → neighbours tried → cost formula". If the four are one pattern (rotated), say so with the four shift values; if not, list the differences.
    3. **`0x10007540`** (called by the special expansions, named in §3.4.10): to its `ret`, one paragraph (what it computes, which fields).
    4. **The constraint mask.** The stride-`0xc` array at `Y + 0x1c0c` (count `Y + 0x1c08`) whose entry `+8` is AND-ed into a cell's low u16 at `0x10004737`: its producer `0x100056e1` takes `ax` from `0x10008230` at `0x100056d7`. Read `0x10008230` to its `ret`: inputs, what the u16 means (which low-u16 bits it clears: a direction group? the nibble?), and who fills the array (the caller of `0x100056e1` — `0x10005890`'s path from the figure's position, §3.4.4). One paragraph D with addresses, or the stop address.
    5. **Docs:** `transport.md` §3.4.13 "The special expansions and the constraint mask"; §3.4.10's table replaced by the combined table of step 2; OQ-161 updated; `state.md` §5.9: one DRAFT sentence per decoded part with its address (the rule T33 implements: start cell key → default expansion; reached cell → came-from expansion with its bit group and cost formula); REPORTS T30b.3o. Acceptance: (a) four pseudo-codes, (b) the combined table, (c) `0x10007540` described, (d) the constraint mask D or its stop address, (e) nothing of the original in git; PR, not merged. No `src/` change, no build.
  - **T30b.3h2 — The Wine read of real cells · `DONE` (the orchestrator, 2026-10-25 ax, fbab166; `transport.md` §3.4.15; the executor's session cannot reach the Wine runner). Was:** (`READY`, round aw; the Wine runner `AnnoHarness`, silent, pre-authorised, no owner needed; the predicted table is `transport.md` §3.4.13's, with its conditional terrain rows, plus §3.4.14's marks for cells a search has reached (`0x80 | dir` codes, code 7 for start and disc cells, bit 10 for goals); stopped in round an–ao for lack of a predicted table, PR #58 merged as the record; a Wine run; a tool under `tools/orig/`).** With the predicted cell table of T30b.3n (step 5) in hand: in the running original (the Wine runner `AnnoHarness`, `docs/testing/original-wine.md`), after a scenario loads and a cart has searched once, read the search engine `Y` (world `+0x6aa8`), the chunk-grid dimensions `Y + 0xc` / `Y + 0x10` and the super-block table `Y + 0x1c + sb·4` (16 × 8 entries; `transport.md` §3.4.11 (b)), and dump the cells of the chunks that cover known tiles: a dirt road `2527`, a cobblestone street, a `0x4000` road, open grass, water, a building footprint, a market's entrance. Compare each cell's low u16 (mapped mask, top nibble) and high u16 (code, bit 10, the `| 7` / `0x80 | dir` marks) with T30b.3n's prediction; write the table (tile kind → predicted bits, observed bits; no memory dumps in git, values only). The reader is a read-only addition to `tools/orig/gamestate.py` (`search_cells(mem, welt_base, tiles)`), with one test in `tools/orig/test_orig.py` that skips without the harness. Acceptance: the predicted and the observed bits per tile kind, every mismatch listed, the reader and its test, nothing of the original in git.
  - **T30b.5 — The figure-position reader (`DONE`: merged locally as `c1987d3`, round bl, run by the orchestrator; was `READY after T30b.4`; a tool in `tools/orig/gamestate.py` with three tests in `tools/orig/test_orig.py`).** Step 5 of the list below. (1) worktree `anno1503-T30b-5`, branch `task/T30b-5-figure-reader`. (2) From `docs/re/annowelt.md` (the unit registry, T21: `0x10099130` growth, `0x10035be0`, the registry at world `+?`) and `docs/re/transport.md` §1 (the figure object's fields the walk uses), write down the ORIGINAL's figure table location and record layout as far as the docs have it, with addresses; what is missing is read from the registry functions T21 names (one function at a time, `disasm.py`). (3) Implement `figures(mem, welt_base)` in `gamestate.py` returning `(id, type, x, y, tile, step, layer)` per figure, read-only. (4) Tests in `test_orig.py`: (a) no scenario loaded → an empty list; (b) after `opening.py` the count of figures is > 0 and every type id is a `FigurenHeader.dat` id; (c) a figure's tile at second N and N + 1 differ by at most one tile. (5) `transport.md` §3.5: the layout as D with addresses. (6) Report; acceptance: (a) the reader, (b) the three tests run and their output lines, (c) nothing of the original in git.
  - **T30b.6 — Step 7(b) and 7(c): the opening's tiles and the cart's path against the proposed rule (`DONE`: merged locally as `58ac91d`, round bl, run by the orchestrator, item (d) NOT MET; was `READY after T30b.5`; a Wine run; docs).** Steps 6 and 7 of the list below. (1) worktree `anno1503-T30b-6`, branch `task/T30b-6-wine-walk-check`. (2) Read the tile records of the opening's buildings and the road tiles around the market (which ids: this answers the shape question of T30b.1 — write per road tile its type id and whether it is a straight, a corner or a junction, from the screenshot of the harness, kept in `cache/`, never in git). (3) Track the market cart and the weaver's carrier tick by tick for 5 game minutes with the reader of T30b.5; list the type id of every tile each one stands on. (4) Apply the proposed rule (`state.md` §5.9 "Proposed rule for T33") to each; every tile the figure stood on must be admitted; list every contradiction. (5) `transport.md` **§3.7** "The Wine run (T30b)": the tables. (6) Report; acceptance: (a) the tile-id tables, (b) the cart's and the carrier's tile lists, (c) the contradictions, or none, (d) the `0x1000` ids seen and the shapes they had, (e) nothing of the original in git.
  - **T30b.7 — Step 7(d): the owner's three clauses, plus the two range cases of OQ-103's 2026-10-25 entry · case (a) `DONE` (orchestrator, round bf, Wine: a house with no road is supplied by a stand — OQ-103); the producer cases and the owner's services clause (fire brigade, healthcare reach houses by road) remain (a house in market stand A's range with a road only to stand B whose range does not cover it; the same for a producer and two market buildings) (`READY after T30b.6`; a Wine run; docs).** Step 8 of the list below: (1) worktree `anno1503-T30b-7`, branch `task/T30b-7-owner-clauses`. (2) In one session: (i) a sheep farm without a road inside a weaver's radius — does wool arrive; (ii) a forester without a road — does wood arrive; (iii) a road at a non-entrance edge of a building — is it served. Use the harness's build macros (`tools/orig/macros.py`, `opening.json` as the pattern); read the stocks with `gamestate.py` `cities`. (3) OQ-103: each clause confirmed / refuted / open with what was seen. (4) Report; acceptance: (a) three outcomes, (b) nothing of the original in git.
  - **T30b.8 — The consolidated rule · `DONE` (PR #66 merged on 2026-10-25, CI 20/20; round ay; `state.md` §5.9, `transport.md` §3.4.16). Was:** (`READY`, round ax; docs only; the main executor). Inputs: `transport.md` §3.4.7–3.4.15 (3.4.15 adds: chunk and figure coordinates are island-relative; the cell rule confirmed live), §3.1–3.3, §1.1 (the disc); `state.md` §5.9's DRAFT sentences. Output: `state.md` §5.9 rewritten as one rule read from the top, every clause FINAL with its address where D (or P-by-running with §3.4.15), DRAFT only where open (the terrain passes for coast tiles, `Y + 0x4ec` / `+0x4f0`, the finer-route validation's inner rules); a numbered list of what T33 implements: (1) per island, cells 4×4 chunks on first use from the tile record: low 12 bits = mapper[mask], code = byte; (2) the layer block per figure layer: `L[code]` admission bits, `L[0x400 + code·8 + dir]` costs; (3) start cells: the edge-connectivity nibble of `0x10003aa0` (the 3×3 subcell flood from the figure's position), the default expansion's keys; (4) reached cells: the came-from code, the four expansions' key rows, straight cost `C[dir]`, turn cost `C[diag] − C[orth]`, u16 scores, the score tree with ties to the right; (5) the goal test and the disc closure; (6) the retry with one-transition vetoes (the four-entry cap). Then OQ-160, OQ-161, OQ-171, OQ-103, OQ-175 statuses consistent, REPORTS T30b.8. Acceptance: the orchestrator can write T33 from §5.9 alone; PR, not merged. Was: `state.md` §5.9's "Proposed rule for T33" rewritten as FINAL where D (every part with its address) or kept DRAFT with the open parts named; OQ-160, OQ-171, OQ-103, OQ-175 statuses; `transport.md` §3.3–§3.7 consistent (one read from the top); REPORTS.md T30b.8 with the list of what T33 may implement and what is still open. Acceptance: the orchestrator can write T33 from §5.9 alone.
- **Why:** T30 reached three results: (i) 11 of 37 named-road variants carry `0x1000` instead of `0x4000` (corrected in round ab; T30 wrote 10 of 36), every named-road WALKMASK is 12 zero bytes, so the stand-in under-counts roads (step 5, D); (ii) the reader `0x1000daf0` builds a 16-byte buffer and calls `[edx+0xec]` on `[this+0x858]` — that method is the yes/no (step 2, OQ-175); (iii) step 7 (the Wine run) needs a figure-position reader. T30b closes (ii) and (iii) and runs the check in the running original, which is what the owner called "100 % sure".
- **Steps (in order; §1 item 7):**
  1. **Set up.** PR #43 open; `git worktree add /c/Users/altin/anno1503-T30b -b task/T30b-walk-rule-re-2 origin/main` (Git Bash forward slashes). No build; `tools/disasm.py` and `tools/xref.py` are in. The Wine runner (`docs/testing/original-wine.md`) is pre-authorized for silent runs; the heavy-job lock applies to a Wine run as to anything else, foreground, one per call.
  2. **The vtable method `[edx+0xec]` on `[this+0x858]` (OQ-175, the next address of T30 step 2).** Find the constructor that installs the vtable with slot `+0xec` matching this call site: start from `0x1000ebb0` (which installs vtables `0x100c18b4` and `0x100c18b0` and reads `[eax+0x858]` from an argument), read its callers to see which engine class supplies the `[this+0x858]` object, then dump that engine's vtable and disassemble its `+0xec` slot instruction by instruction. The method is called with `(engine+0x85c, 0, &buffer, 8)` where `buffer` is 4 dwords (two 8-byte spans from the primary layer block and two from the secondary); the question is how those 16 bytes become a yes/no. Record every D with its address; what is likely gets a note; what is open gets the next address. Update `transport.md` §3.3 and OQ-175.
  3. **The `+0x434` secondary table writer (OQ-175, cont.).** `[this+0x430]` is set by `0x1000dd20` (D, T30 §3.1); `[this+0x434]` reads as a pointer but has no obvious setter in the sweep. Look for the writer by (a) tracking writes to `[reg + k]` where `k` could be `0x434`, (b) checking the constructor at `0x1000ebb0` and its callers (the engine's initial state might set both pointers at once from a struct), (c) `xref.py` on each of the five layer-block start addresses (`0x100d1140 + 0x1c00·L` for L in 0..4) looking for arithmetic that would produce the pointer. State what the secondary points to and what layer / mode (walk, shot) it carries. Open: the one with the clearest D-chain is the answer; if none reach D, keep as the next address under OQ-175.
  4. **Step 7(a): memory compare after a scenario loads (does not need the figure reader).** In the Wine runner (`agent.py` `read`), capture the five 7168-byte blocks `0x100d1140 + 0x1c00·L` (L = 0..4) right after a scenario loads AND after 10 game minutes; compare them byte-wise with the DLL image on disk. Record: "equal", or (if different) the count and offsets of the differing bytes — **no block content in git**. This closes the "written at run time" question T30 left as "D static but open dynamic". Add the Wine automation to `tools/orig/`: a short script that drives the agent to load one scenario (reuse `tools/orig/opening.py` or just `agent.py launch` + screen-wait), then calls `read` on the module+RVAs, then diffs; the diff output goes to `cache/orig/` as the harness already does.
  5. **The figure-position reader in `tools/orig/gamestate.py`** (owner's scope rule, round w: this task names it as a new tool). The current reader covers world, players and city stocks. The figure reader must: find the figure table off the world object (`world + ?` — the sim's own figure table lives at `world + 0x114` per `transport.md` §5.9, but the ORIGINAL's offset is open — one of T30b step 5's sub-questions); decode each figure record enough to export `(id, type, position, tile, state_step, layer)` in a stable JSON; be read-only. Named test cases of the reader: (a) with no scenario loaded, zero figures; (b) after `opening.py` runs, every figure the opening spawned is in the list (counts match the manual's spawn table); (c) its tile at tick N equals its tile at tick N+1 ± a few. **The reader does not touch sim state** (it is a Wine-only harness tool).
  6. **Step 7(b): read the opening's tile records and apply the proposed rule** (`state.md` §5.9's "Proposed rule for T33"). For each of the opening's buildings (market hall, foresters, weaving huts, sheep farms, tannery, hunting lodges), read the tile records of its footprint and the surrounding road tiles, and apply the proposed rule for layer 3 (the cart) and layer 0 (the carrier). Numbers go into REPORTS; a contradiction with step 7(c) is a finding, not something to explain away.
  7. **Step 7(c): the market cart's and the weaver's carrier's tiles tick by tick** for 5 game minutes, using the figure reader (step 5). Every tile each figure stands on must be admitted by the decoded rule for its layer; a tile it refuses must be refused by the rule too. This is the headline Wine-run acceptance.
  8. **Step 7(d): the OQ-103 three clauses** in the same session. (i) A sheep farm served without a road inside a weaver's radius (place the weaver, the sheep farm without a road, confirm wool arrives); (ii) the forester needs a road (place a forester without a road, confirm nothing arrives); (iii) a road connects only at a building's specific entrance tile (place a road at a non-entrance edge, confirm the building is unserved). Each outcome into REPORTS.
  9. **Docs.** `transport.md` §3.3 (updated with step 2's result), §3.4 (updated with step 4's memory compare), new §3.6 "The Wine run (T30b)" with the step 7 outcomes; `annoscene.md` for the search engine's class and its vtable if that is where the method lives; `open-questions.md` OQ-160 updated with the fully-D rule (or exactly what still is open), OQ-171 answered or narrowed, OQ-103 answered or narrowed, OQ-175 either closed or named the next address; `state.md` §5.9's "Proposed rule for T33" rewritten to a FINAL rule when the method is D (or clearly marked as still-draft with the open parts named); REPORTS.md T30b with the acceptance ticked; PR, not merged.
- **Acceptance:** (1) the vtable method is D with its address, or exactly what is still open and the next address; (2) the `+0x434` secondary is D with the writer address, or named open with the next address; (3) the memory compare of step 4 is D (equal / listed differences); (4) the figure reader with its three tests; (5) step 7(c) run (every observed tile admitted by the rule, or every contradiction listed); (6) step 7(d) three clauses answered; (7) nothing of the original in git (no byte dumps, no screenshots); (8) report, PR, not merged. **The owner reads D claims before merge, per the directive.**
- **Not in T30b:** any `src/` change (T33 does it from the final rule); any `rules` option; the weapons carrier (T28); other search modes than the walk (shots: not open yet).

### T31 — Monuments: how the original awards and handles them, RE only (OQ-144, OQ-166; owner decision 2026-10-24) · **split into sub-tasks T31.1–T31.3 (round aa), `READY after T30b.8`** (the executor runs one sub-task at a time, no subagents, §1 item 11; T31.1 = steps 1–3 below (the data, the manual, the community claims: `docs/re/monuments.md` with a claims table, PR), T31.2 = steps 4–5 (the code: the award check, the building, the scenario path; each D with address or open with the next address, PR), T31.3 = step 6 (OQ-144 / OQ-166 statuses, "What the sim needs", PR)) · was `READY after T29` (round x; one subagent) · `PARALLEL-OK` (docs only: a new `docs/re/monuments.md`, `open-questions.md` OQ-144/166, one status line in `docs/formats/sze.md`; no `src/` change, no build beyond `anno-dump`) · branch `task/T31-monuments-re` from `main` · worktree `C:\Users\altin\anno1503-T31` · new OQs from **OQ-180** (to 184)
- **Why:** the owner describes monuments as decorations a player earns in a running game, endless games included, for reaching a threshold (example: the population satisfied for 30 minutes in a row; not confirmed). Then they are a gameplay rule, not a loader detail: before any code, the original's award rule, what a monument is as a building, and how the state keeps it. T26 left 35 `DENKMAL` entries of the shipped scenarios uncreated; the original creates them on the scenario path (T25 (j), `0x10024010`).
- **Steps (in order; §1 item 7):**
  1. **Set up** from `main` (after PR #40); `anno-dump` from the main build.
  2. **What the data says:** the monument types in `Anlagen.dat` (`anno-dump anlagen`): ids, names, PRODTYPE (the block of `DENKMAL`), size, costs, flags, research or unlock ids, `WALKMASK`. The texts (`anno-dump texte` on `Texte.dat`): every section and key that names a monument, an award or a reward; cite them by section and key and paraphrase (names and at most a few words verbatim).
  3. **What the manual and players say:** `docs/research/manual-and-help.md`, `docs/research/playbook.md`, the owner's manual at the repo root (`Anno_1503_Manual.pdf`, untracked: read it, cite page numbers, never add it to git), community sources (cite URLs). A list of claims, each with its source: when a monument is earned (threshold, duration, per player or per city, endless games), what it costs, what it does (decoration only, or an effect on houses, satisfaction, prestige), whether it can be lost, demolished or rebuilt.
  4. **The code (D):** xref the monument type ids and their PRODTYPE (`tools/xref.py` over AnnoWelt, AnnoGame, AnnoScene, AnnoDisplay); find the check that awards a monument: the counters it reads (residents per group, satisfaction, a duration timer), the tick that runs it and how often, what it writes (a per-player earned or available count or flag, a message, the build menu) and whether that is saved. The monument as a building: its class and tick (vtable; T25 (j) says the base class in the production list), its effect if any (a radius on houses' mood?), demolition. The scenario path (`0x10024010`): whether a scenario's monument counts as earned.
  5. **The owner's description, clause by clause:** "a decoration a player earns", "for reaching a threshold in a running game, endless games too", "for example the population satisfied for 30 minutes in a row": each confirmed, corrected or open, with the address or the source.
  6. **Docs.** `docs/re/monuments.md` (new; every line D / likely / open with addresses), OQ-144 and OQ-166 status, a section "What the sim needs" (the state the award needs: counters, timers, per-player flags; the stage it belongs to: the population stages if it reads satisfaction; the scenario-path creation for T31b), REPORTS.md with the acceptance ticked, PR. No `TASKS.md` edit.
- **Acceptance:** (1) the award rule D with addresses, or exactly what is open and the next address; (2) the monument as a building D (tick, effect, save); (3) every claim of steps 2–3 listed with its source and matched against the code; (4) the owner's three clauses answered; (5) nothing of the original in git (no bytes, no text beyond names and short phrases, not the manual); (6) report, PR, not merged.
- **T31b (`LATER`, after T31 is reviewed):** monuments in the sim: the scenario-path creation (OQ-166 option 1, unless T31 shows otherwise), then the award rule with the population stages it reads from.

### T32 — The population model, stage P0: RE and the spec draft (OQ-165; owner decision 2026-10-24; PLAN M3 "population/needs") · split into **T32.1–T32.3** (round bb), one branch each, in order; T32.1 `DONE` (6e48d5c, reviewed and merged locally, round bc); T32.2 `DONE` (c008c48, merged locally, round bd); T32.3 `DONE` (dd69419, merged locally, round be: the spec is `state.md` §4.7c "Population (DRAFT, T32)"); T32.4 `DONE` (81bd204, merged locally, round bf); **T32.5 `DONE` (round dk; merged as 467828d, accepted at 1fc598eb)** (was `REVIEW — accepted, pending integration`, round cy; F1 report `1fc598eb`; its own rules are in the T32.5 bullet, which override this header's file list); P1 `DONE` (round cx, `b173d02`) · **the third executor, in parallel with T33.4** (static RE and docs only: no build, no game run, no Wine; files `docs/re/population.md`, a new section in `docs/sim/state.md` (not §5.9, which T33.4 edits), `docs/open-questions.md` new OQs from **OQ-185** (to 194) plus OQ-165's status, `REPORTS.md`; never `src/`, the goldens or `TASKS.md`) · local-only workflow of round ba: branch from local `main`, commit, report the branch and commit; no push, no PR
- **Sub-tasks (round bb; §1 items 7 and 11).** The long step list below is the reference. Read `docs/re/population.md` first (it is the existing evidence: the `BGruppen.dat` loader, the house object, the scheduling, the house update `0x10076230`, the mood `0x10075fd0`, supply and level change, money). Tools: `python tools/disasm.py game/app/<module>.dll <va-hex> [bytes]`, `python tools/xref.py [--reloc] <module>.dll <lo> <hi>`, `pefile` for vtable slots and data; `analysis/pe_symbols.json` for imports and RTTI. **This-adjustment rule (§1 item 11):** the first row of every function table is "this = <object> + n, because …" (the vtable slot it was called through and that vtable's installer, or the `lea ecx, [reg + k]` at the call); write every field as an absolute offset of its object before comparing two functions. Every claim carries module + address and the address range read; never paste disassembly text or bytes of the original. A step that cannot reach D stops with its next address and the next step goes on.
  - **T32.1 — Inventory, and the house and city fields (steps 2–3 below) · `DONE` (round bc: re-read — Load `0x10076e60` rebuilds `E = r << 7` and the resident field `r << 6` (`0x10076f01..0x10076f0b`); Save `0x10077130` writes `field >> 6` (`0x1007715e`) and each need byte `min(s, 0xa0)` (`0x100771c6`); the city save `0x1002ebb0` touches none of `+0x2ec`/`+0x2f0`/`+0x18a`. So residents (whole numbers: the 1/64 fraction is lost on every save — a faithful save must truncate the same way) and the need bytes are state; `E`, the city's group totals and the house counts are derived after a load; OQ-165 answered; OQ-185 open: who dispatches the house Save slot `+0x80`).** Branch `task/T32-1-population-fields`. Output: REPORTS T32.1 with the inventory table (every rule of `population.md` with its tag and, if not D, the address to read); `population.md` new §11 "State or derived (T32.1)": one table per object (house: every field from its constructor, `Load` `0x10076e60` and the house update, offset, type, meaning, who writes it, saved yes/no with the save/load address; city: the population fields `+0x2ec`, `+0x2f0`, the per-group counts, the demand/consumption sums, the same columns) and the conclusion per field — saved by the original → state in the rebuild, recomputed after load → derived; OQ-165's status line from that table. Acceptance: both tables D per row or open with the next address; OQ-165 answered or narrowed; nothing of the original in git.
  - **T32.2 — `BGruppen.dat` fields and the house tick (steps 4–5 below) · `DONE` (round bd: re-read — the downgrade target `id − id % 20 − 20`, else `− 19` (`0x1002ce0f..0x1002ce35`); the house tick `0x10076230` calls no city billing slot `+0x88`, it decays the house's own stocks by 127/128 (`0x1007628e..0x100762f8`); **so a house never takes goods from the city store itself: goods arrive by delivery (`0x10076610`) and the store is drawn by billing with `take = 1` from another caller, OQ-186** — T32.3's spec routes consumption that way; OQ-185 still open).** Branch `task/T32-2-house-tick`. Output: `population.md` §12 "The house tick as pseudo-code (T32.2)": which `BGruppen.dat` fields are read and where (the field map: field → `formats::text` name → reader address); the tick every 2 500 ticks in order — consumption from the city store (the formula with its units, 1 t = 256, the rounding), the stock decay, satisfaction and mood (−2 … +2), growth through `VermehrTab`, up- and downgrade (conditions, which building replaces which), and taxes every 2 000 ticks into the player's money — every PRNG draw with its site and condition; each rule D with its address, else open with the next address. Acceptance: the pseudo-code complete or every gap with its next address; the field map; the draw list.
  - **T32.3 — The scenario side and the spec draft (steps 6–8 below; `READY`, round bd; first finish OQ-186: the take is answered (round bd: the market stand, PRODTYPE 161, slot `+0xac` = `0x100702f0`, `take = 1`; chapels, churches, baths, theatres, libraries, pavilions, schools bill without taking); read the remaining path — the walker class behind house slot `+0x118`, who calls the stand's slot `+0xac`, and who calls the house's delivery slot `+0xc0` (`0x10076610`) — because the spec routes consumption city store → stand → walker → house); and test the **owner's rule** recorded in OQ-103 (2026-10-25: the stand is to houses what the market building is to producers — a road connection to a stand plus lying within the range of **any** stand): find the stand's range test and the road requirement in the code and state for each clause whether the code agrees, with addresses).** Branch `task/T32-3-population-spec`. Output: T26's three pure functions (`house_need_stock`, `house_satisfaction`, `house_reset_applies` in `src/sim`) checked against T32.1–T32.2 (read the code, do not change it; list every difference); the new `state.md` section "Population (DRAFT, T32)" (the house payload fields, the group table in the registry, the tick and its place in the city tick, the seams, the stream bump, a synthetic golden G7, the test list, stages P1… each with a proposed acceptance); owner questions for anything that may change the feel, each with the faithful default. Acceptance: the orchestrator can write P1 from the section alone.
  - **T32.4 — The stand → house path and the owner's market-stand rule (`READY`, round be; static RE and docs only; the parallel executor).** Branch `task/T32-4-stand-path` from local `main`. Read `population.md` §11–§13, `state.md` §4.7c, OQ-103 (the owner's rule of 2026-10-25), OQ-186, OQ-187. Steps: (1) **OQ-187, who calls the market stand's slot `+0xac` (`0x100702f0`):** sweep for virtual calls through slot `+0xac` whose receiver is a building of the stand's class (vtable `0x100c8fe0`, constructor `0x10070730`; check the receiver by its origin, not by the slot number alone — slot numbers repeat across classes, and `0x1005ac10` is a search callback, not a stand caller); name the caller and the object it runs on. (2) **The resident walker:** the house tick calls house slot `+0x118` (`population.md` §12, `0x100763e3`); read that slot to its end: what it spawns (figure type, job), where it sends it (which stand; how the stand is chosen — nearest, first in a list, a search with which layer), what it carries back, and who calls the house's delivery slot `+0xc0` (`0x10076610`) with the goods (`0x10057bd0` is named as the figure unload path; confirm). (3) **The range test and the road requirement for houses:** where the code decides that a house may be served by a stand — a radius around the stand (`HEAD` action radius `+0x65`, as for markets), the city's area, a walk search over roads (which walking layer) — each D with its address. (4) **The owner's rule (OQ-103, 2026-10-25)** clause by clause — (a) a road connection between stand and house is required; (b) the house lies within the range of **any** stand, not necessarily the one it is connected to by road — and the same two clauses for producers and market buildings (the market cart's area and road search, `state.md` §5.9 clauses 2 and 5, are decoded): for each, agrees / disagrees / undecided statically, with the addresses, and the exact situation in which code and rule would differ (that is the case for T30b.7's Wine check). (5) Docs: `population.md` §14 "From the stand to the house (T32.4)", OQ-187 and OQ-103 status lines, `state.md` §4.7c's P5 row with what was found, REPORTS T32.4. Acceptance: (1)–(4) each D with addresses or stopped with the next address; no `src/` change; commit on the branch, report the branch and commit.
  - **P1 — The house payload, the registry groups, Load/Save (superseded in round cm by the `#### P1` entry in the LAN lane, which is the binding text; was `READY after T33.4`, round be; `src/sim`, stream change; the main executor after T33.4).** Written from `state.md` §4.7c's P1 row, its test list and its stream bump (the next free `kStateHashVersion` after T33.4's 112). The orchestrator writes the full step list when T33.4 is merged. Owner decisions of 2026-10-25 (round be): question 1, the catch-up after a load, stays faithful (`last` not saved); question 3, the billed goods, are saved, as a `Rules` option `save_billed_goods` with default `true` (`false` = the original) — P4. The other five questions take their stated faithful defaults.
- **T32.5 F1 — Correct the stage-mask evidence and summaries (`REVIEW — completed and accepted at 1fc598eb`, round cy; historical round-cx assignment).**
  - **Start:** existing clean `task/T32-5-stage-mask` at `e232b02d743d623679b1ae92a2a6ae7083288f77`, worktree `E:\anno1503-worktrees\anno1503-T32-5`. Do not merge/rebase main into it. P1 and T10c are now integrated on main; leave their branches/worktrees untouched. Use the generated round-cx packet and its preflight (§1 item 13).
  - **Files:** only `docs/re/population.md`, `docs/sim/state.md`, `docs/open-questions.md`, `docs/orchestration/REPORTS.md`. This is documentation correction, with bounded read-only original-file/disassembly checks. No build, game run, Wine, source/golden/fixture edit, TASKS/AGENTS edit, merge, push or next task.
  - **F1.1 — Restore both pieces of the mask.** Recheck AnnoWelt `0x1001d389`–`0x1001d3df` and all original SPIELER/HEAD records. State separately the stored low field (`+0x48`, low 23 bits), packed flags (`+0x5c`) and full runtime value (`player+0x90`, property `0x21`). Packed bits 3/2/4/5 restore bits 24/27/29/30. Produce low-field, reconstructed-high and full-value histograms over the 50 files/373 heads. Expected high values: zero 295, `0x28000000` 17, `0x29000000` 1, `0x69000000` 60. Low field equals 1 for 280; full value equals 1 for 263. If evidence disagrees, stop that finding with a QUESTION and exact output, not an invented reconciliation. Correct every assertion that shipped high bits are clear, including the state summary. `Player::flags` exists; its scenario import is missing. P2 can precede P4 only after its own inputs are supplied, not by assuming all masks are 1.
  - **F1.2 — Preserve the guard on the player write.** Recheck the unsigned comparison/branch at `0x100296c2`: its target `0x100298fe` is after OR `0x100298ee` and setter call `0x100298fb`. The failed comparison skips both announcement and player update. Record operands/unsigned condition from the instructions, and correct every prose/pseudo-code summary that makes the OR unconditional. Do not implement or infer additional setter effects.
  - **F1.3 — Correct the catch-up claim.** Retain the exact modular city gate `(u16)(tick-last) >= 2500`; demonstrate the counterexample tick 65536, last 0 (difference 0), plus a passing boundary. Remove the unconditional `tick >= 2500` and first-visit/<=500-tick catch-up claim. Keep the unknown initial city stamp in OQ-461 explicit. Do not confuse the city stamp with the separate house update's `last += 2500` rule or decide OQ-450.
  - **F1.4 — One OQ-462.** Remove the duplicate entry, preserve one complete question and all valid references. Check OQ-460..469 uniqueness and the links between the evidence, draft and report. S9a/OQ-185 and S9b/OQ-462 may remain open as the original task allowed; do not expand into solving them.
  - **Evidence and reporting:** the orchestrator's ignored probe `E:\anno1503\cache\orch-cw-review-overnight\check_stage_mask.py` and `stage-mask-evidence.txt` are reproduction aids, not an oracle to trust blindly. Check its field/bit extraction against the instructions and file structure. Derived output goes under the task worktree's ignored `cache/`, never original bytes in git. Append `## T32.5 F1 — Stage-mask corrections — <date> — status: ...` with exact heads, commands/output locations, all histograms, F1.1–F1.4 verdicts, and Decisions and reasoning. Preserve the historical report and explicitly supersede its incorrect statements in the new section.
  - **Acceptance/stop:** the four current documents agree on the full mask, guarded update and modular timing; no duplicate OQ; every original-file claim has evidence; allowed unresolved questions remain labeled. `git diff --check` clean and diff limited to the four files. Commit each concern separately, append the final report, then stop for orchestrator review. No next task from the follow-on set is authorized by this packet.

- **T32.5 — The stage mask: where the reached-stage bits live, who writes them, whether they are saved (`REVIEW — accepted, pending integration`, round cy; original report `e232b02`, corrections `1fc598eb`).**
  - **Branch and files:** branch `task/T32-5-stage-mask` from local `main` at the round-cv board commit or later; worktree `E:\anno1503-worktrees\anno1503-T32-5`. Files: `docs/re/population.md` (a new §15), `docs/sim/state.md` §4.7c (one DRAFT paragraph and the P2 row), `docs/open-questions.md` (new OQs from **OQ-460**, to 469), REPORTS. Never `src/`, goldens, `fixtures/`, TASKS.md or AGENTS.md. No build, game run or Wine.
  - **Why:** P2's house tick decays a need's stock, and counts it as the lowest need, only when the player's reached-stage mask has bit `BauFirst` of the need's good (`population.md` §4 and §12.2; AnnoWelt `0x100762c2`–`0x100762f8`). The rebuild has no stage mask. Where the original's comes from (a file record, a save, or recomputation by the stage unlock `0x10029610`) is unread, and P2's decisions depend on it.
  - **Steps** (in order; §1 items 7, 11 and 12; each result written into REPORTS before the next step):
    1. **The read in the house tick** (`0x10076230`–`0x100762f8`). Name each object on the path `city +0x10` → slot `+0x1c` → slot `+0x48(0x21)` by the this-adjustment rule (first table row "this = …, because …"). Say which function slot `+0x48` is and how it maps property `0x21` to storage (a field offset, or a computed value). Then say which dword is tested ("second dword"), and the bit index (`BauFirst` = goods record `+2`).
    2. **Every writer of that storage.** The constructor's initial value (player constructor `0x10021600..0x10021836`, `players.md`); the property setter (a case `0x21`, if any, near `0x1001be0d`); the stage unlock's OR into the player (`0x10029610`, `population.md` §7); and every other site found by `tools/xref.py` (the field offset by the code sweep, the function by `--reloc`). For each, give the address, the condition and the value.
    3. **The city mask `+0x618`:** its constructor value, every writer, and every reader. Confirm that the house tick reads the player's mask, not the city's.
    4. **Saved or derived, for both masks:**
       - Does a save path write the value? Check the player's save, the city save `0x1002ebb0`–`0x1002f264` (sweep it for `+0x618`) and the world save `0x100ae700`. Does a load path read it back? Check the `SPIELER` reader through player slot `+0x54` (`players.md`, "World reader → player") and the city load.
       - If saved: name the record and field (chunk, offset, width), and check them against the shipped scenarios with our readers. Report counts and value histograms only; nothing from the files goes into git.
       - If not saved: give its value after a load, and how many ticks a loaded scenario's mask needs to catch up.
    5. **The stage unlock `0x10029610`, for the spec:**
       - the 2500-tick gate (the 16-bit difference, its `last` field and the update);
       - the loop over stages 0..30, through counter slot 8 and the `[BAUSTUFEN]` record (`+0` group, `+2` count);
       - the "low 23 bits numerically below" comparison and the OR;
       - the announcement: is there any sim-side effect or PRNG draw, or is it UI only?

       Each item D with its address, or the next address.
    6. **The answer table.** One row per mask (player property `0x21`, city `+0x618`), with the columns: object and offset; width; initial value; writers; saved (yes or no, with the address); value after loading a shipped scenario; state or derived in the rebuild. Use T32.1's rule: what the original keeps and saves is state, and what it recomputes is derived.
    7. **The DRAFT paragraph "Stage mask (T32.5)"** in `state.md` §4.7c. It covers:
       - the field or fields the rebuild needs, and where they are loaded;
       - how P2 tests the bit;
       - how, and in which stage, the stage unlock updates the mask;
       - the stream impact (a new field, and which scope);
       - each owner question, with its faithful default stated.

       Update the §4.7c P2 row so that it names the mask dependency.
    8. **Docs, report and commits.**
       - Write `population.md` §15: D with addresses and the address ranges read, never disassembly text or bytes.
       - Open OQs from OQ-460 for whatever stays open.
       - Write REPORTS `## T32.5 — The stage mask — <date> — status: …` with "Decisions and reasoning" and the acceptance item by item.
       - Commit one concern at a time (RE notes; the DRAFT; the OQs), then stop.
    9. **Only after steps 1–8:**
       - (a) OQ-185, who dispatches house Save slot `+0x80` on a world save: read world save `0x100ae700` and its building-list walk for `call [reg+0x80]`.
       - (b) Which code sets house flag bits 4/5 (`+0x0c & 0x30`, the forced bad mood); start at the city events `0x1002ba80`.

       Each is D with addresses, or the next address. Put each in its own subsection of the T32.5 report, with its own commit.
  - **Acceptance:**
    - step 6's table, D per row or the next address;
    - the DRAFT paragraph, complete enough for the orchestrator to write P2's stage-mask decision;
    - every claim with module, address and the range read;
    - no disassembly text and no bytes of the original;
    - report and commits; not merged.
- **The original step list (reference for the sub-tasks):**
- **Why (unchanged):** a house is the `Inert` payload, nothing consumes, and the files' residents are dropped (T26: `single36` 388 houses, 2 283 residents). Population is the next subsystem in PLAN M3's order after transport, and T26's pure functions (`house_need_stock`, `house_satisfaction`, `house_reset_applies`) wait for it. The owner on OQ-165: "maybe both", settled from the original's files.
- **Steps (in order; §1 item 7):**
  1. **Set up** from `main`.
  2. **Inventory:** every rule of `docs/re/population.md` with its tag (D / likely / open) in a table in REPORTS.md, with the address to read for each rule that is not D.
  3. **OQ-165, the house and the city:** (a) every field of the house object with its offset: the house base constructor (kinds 97..101, `state.md` S6 notes), `Load` `0x10076e60`, the house tick: residents (`+0x2c` / `+0x2e`), group, per-need stocks or satisfaction, mood, timers; (b) every population value on the city side (the resident counter city `+0x2ec`, the per-group counts `BuildingIndex +0x46 + 2g`, any per-need demand or consumption sums) and who writes it: incrementally in the house tick, at create and remove, or recomputed from the houses; (c) which of them the original saves (the house's and the city's save and load; the world save `0x100ae700` / load `0x100ae270`, T21) and which it recomputes after a load. The answer: what the original keeps in its objects and saves is state in the rebuild, what it recomputes is derived. A table in `population.md`, D per row.
  4. **`BGruppen.dat`** (parsed by `formats::text`: 5 groups, 47 needs): which fields the house tick reads (`Menge`, `Wohnmax`, the growth table `VermehrTab`, tax, the needs per group, …) and where. Load-time data for the registry, never state.
  5. **The house tick** (every 2 500 ticks, `population.md`): consumption from the city stock (formula, 8.8 units, rounding), satisfaction and mood (−2 … +2), growth (`VermehrTab`), up- and downgrade (the conditions, the building replaced), taxes every 2 000 ticks into the player's money, every PRNG draw (site, condition). Each D with its address, else open with the next address.
  6. **The scenario side:** T26's three pure functions checked against steps 3–5; the reset rule's draw site named.
  7. **The spec draft:** a new section in `docs/sim/state.md`, "Population (DRAFT, T32)": the house payload fields, the group table in the registry, the tick and its place in the city tick, the seams, the stream bump, a golden G7 (synthetic: two groups, consumption, growth, an upgrade, taxes), the test list, and stages P1… each with a proposed acceptance, for the orchestrator to finalise. Anything that may change the feel: an owner question with the faithful default stated.
  8. **Docs.** `population.md` updated (D with addresses), OQ-165 status (answered by step 3 or narrowed), new OQs from OQ-185, REPORTS.md with the acceptance ticked, PR. No `TASKS.md` edit.
- **Acceptance:** (1) step 3's table, D per row, and OQ-165 answered from it; (2) every rule of step 5 D, or open with the next address; (3) the `BGruppen` field map; (4) the DRAFT complete enough for the orchestrator to write P1 (every field with its evidence); (5) the owner questions listed; (6) report, PR, not merged.

### T33 — The decoded walk rule in the sim (replaces the OQ-160 stand-in) · split into **T33.1–T33.4** (round ay), one PR each, in order; T33.1 `DONE` (PR #67 merged on 2026-10-25, CI 20/20); T33.2 `DONE` (merged locally, round ba, GitHub unavailable); T33.3 `DONE` (merged locally, round bb); T33.4 `DONE` (merged locally as `acd45cd`, round bj); T33.5 stage 1 `DONE` (merged locally as `d30d20b`, round bk); **T33.5 stage 2 `READY`** (round bk) · the main executor · the rule is `docs/sim/state.md` §5.9 ("What T33 implements", clauses 0–8); the evidence is `docs/re/transport.md` §3.4.7–3.4.16 · new OQs from **OQ-200**
- **Rules for all four (read first):** `CLAUDE.md` (determinism: integers only in `sim`, no unordered containers, no global mutable state; no hardcoded limits: the table sizes and the layer count are data with the original's values as defaults, the 4×4 chunk edge is a config value; explicit serialization in `formats`: `bytes.hpp`, `Result<T, E>`, no `reinterpret_cast` of file data). Every mapping in code cites its AnnoWelt address in a comment (§5.9 gives them). The original's tables are **never** copied into the source or fixtures: tests that need them read `game/app` through `anno::test::game_dir` and skip cleanly without it; synthetic tests build their own small tables. Gates of §1 item 4 for every PR (MSVC, ctest with and without the game dir, clang-format 23.1.3, the GCC 13 `-O3 -Werror` check in WSL `Ubuntu` for `sim`/`formats`), mutation checks listed in the report (§1 item 5).
- **T33.1 — The walk tables from `AnnoWelt.dll` (`READY`, round ay; `src/formats` + `anno-dump` + Python oracle + docs).** Branch `task/T33-1-walk-tables`, worktree `anno1503-T33-1`. Steps:
    1. **Spec first:** `docs/formats/annowelt-walk-tables.md`: the minimal PE reading needed (DOS header `e_lfanew` u32 at `0x3c`; `PE\0\0`; COFF `NumberOfSections` u16 at `+6`, `SizeOfOptionalHeader` u16 at `+0x14`; optional header `ImageBase` u32 at `+0x1c` of it; section headers 40 bytes each: `VirtualAddress` u32 `+0xc`, `SizeOfRawData` u32 `+0x10`, `PointerToRawData` u32 `+0x14`), RVA → file offset through the section that contains it, and the three tables with RVA, size and meaning, each tied to its reader address: the mapper `0xc1af8`, 512 × u16 (`0x1009005f`); the layer blocks `0xd1140 + 0x1c00·L`, L = 0..4, `0x1c00` bytes each: bytes `0..0x3ff` admission, `0x400 + 8·code + j` costs (`0x1000dd20`, `0x10006ab0`, `0x10006ab9`); the goal-order table `0xc1160`, 5 rows × 9 entries × 4 bytes (u16 mask, u8 col, u8 row; `0x100036fb..0x1000371c`). Identify the DLL by its COFF `TimeDateStamp` and `SizeOfImage` (record the GOG 2.0.0.5 values from the file) and refuse a different build with a clear error — a modded DLL can be accepted later behind an explicit option (comment the path, PLAN §0.1).
    2. **`src/formats`:** `anno/formats/annowelt.hpp` / `src/annowelt.cpp`: `Result<WalkTables, WalkTablesError> read_walk_tables(std::span<const std::uint8_t> dll)` with `struct WalkTables { std::vector<std::uint16_t> mapper; std::vector<std::vector<std::uint8_t>> layers; std::vector<std::array<GoalEntry, 9>> goal_order; }` (sizes from the spec as defaults, held in vectors, not fixed arrays); field by field through `bytes.hpp`; errors: not a PE, wrong build, RVA outside every section, short file. Unit tests with a synthetic tiny PE built in the test (one section, made-up tables): round trip of every field, each error case; a corpus test that reads `game/app/AnnoWelt.dll` and checks the invariants §5.9 states (P): 512 mapper entries with 33 distinct values, every value's top nibble 0; five layers; `L[7] = 0` and `L[0x80..0x86] = 0` in every layer; `L0[2] = 0xff` and `L3[2] = 0`; `C_0(0) = 10` in layers 0 and 3; `L3[4] = 0xff` with `C_0 = 40`.
    3. **`anno-dump walk-tables <AnnoWelt.dll>`:** prints the build identity, per layer the count of admitted codes below `0x400` and the admission / eight costs of codes 0, 1, 2, 4 (counts and a few numbers only, like `tools/orig/layer_blocks.py --disk-summary`); **Python oracle** `tools/annowelt_tables.py` with the same output from `pefile`-free code (`struct` only), and `tools/agree.py` gets the command.
    4. **Docs and report:** `docs/formats/README.md` one line; REPORTS T33.1 with the gates, the corpus test's output line, `agree.py`'s line, the mutation checks (flip one admission test in the corpus test, flip the RVA of the mapper: both must fail). PR, not merged.
- **T33.2 — Cells per island and the layer lookup (`READY after T33.1`; `src/sim`, pure functions, no stream change).** Branch `task/T33-2-walk-cells`. Steps:
    1. **Where the tables live in the sim:** `WalkTables` (from T33.1) is passed in with the type registry at world creation (the same path `TypeRegistry` takes from `Anlagen.dat`; read `docs/sim/state.md` §4.4 and `src/sim/src/scenario.cpp` / the app's loader for that path) and kept immutable and shared. **Not hashed in T33.2** (round az, answer to the executor's question): the registry's digest is written into the state stream's header (`state_fields.hpp` header block), so adding the walk-table digest there changes the stream layout and every golden; in T33.2 the tables are held beside the registry and nothing is added to the header, `state_hash()` or the save — every existing hash and golden must stay byte-identical (a golden that moves is a bug of this sub-task, not something to re-record). The digest joins the header in **T33.4**, together with `kStateHashVersion` 112 and the re-recorded goldens. `SimConfig::validate` refuses a missing table when a scenario has figures with a walking layer. Synthetic worlds (`synthetic.cpp`) get small synthetic tables.
    2. **The tile record's walk bits (§5.9 clause 1):** a pure function `walk_record(const Island&, const TypeRegistry&, TileXY) -> {code, mask9}`: an object tile → its type's code (the encoder `0x100091f0` from HEAD bytes `0x2f`/`0x30`, the default 4 when both are 0, `AnnoGame 0x1000940b..0x1000942b`; the rotation getter `0x10007dc0` for the high-bit codes) and the rotated 3×3 WALKMASK sample of that footprint tile (`AnnoWelt 0x1007b5e0`, the four rotation formulas of `transport.md` §3.4.13); a tile without an object → code 2, mask 0, or mask `0x1ff` when the water test holds. **The water test:** read `0x1008ff00` (the threshold's initialiser) and `0x10092b40..0x10092c4d` (the mean of the four corner heights from `[I + 0x14]`, compared with `trunc([0x100e34dc] / [0x100ca6fc])`); write what the threshold is (a constant, a function of the island, or a value loaded from a file) into `transport.md` §3.4.13 with the addresses; then check that the sim's `Terrain::height` column is the same corner-height data (`docs/formats/insel.md`, `state.md` §4.8, OQ-060); if both hold, implement the test; if either fails, **stop this sub-step** and write the question (OQ-200) — then empty tiles are code 2 / mask 0 and the deviation is listed. The coast pass `0x10081ce0` is not implemented (round ay: documented deviation, OQ-170).
    3. **Cells:** `struct WalkCell { std::uint16_t low; std::uint16_t high; }` and a per-search cell store over the search rectangle clipped to the island (§5.9 clause 0–1: origin `max(rect.left, island.x)` / `max(rect.top, island.y)`, `0x10002b80`), filled lazily per chunk of `chunk_edge` (config, default 4) tiles on first access: `low = mapper[mask9]`, `high = code`, outside the island 0; then the disc closure (§5.9 clause 5; `0x100037e5..0x10003a84`; the disc fields from the search's area set-up, `0x10002a60`) marks the cells outside the rounded area `(hi & 0xfc07) | 7`. Ordered containers only; the store is local to one search (not state).
    4. **The layer lookup:** `admission(layer, code)` and `cost(layer, code, j)` over `WalkTables`.
    5. **Tests:** synthetic: a 3×3 island with a road, a house (code 4, a WALKMASK with a closed subcell row) at all four rotations, water; the cells' low and high words against hand-computed values. Game-dir tests (skip without it): for `S01_07_01`, the cell of a dirt road `2527` tile is `low 0xfff, code 0`, of a house tile `code 4` with the house's mapped mask; the cart's layer (3) admits code 0 and 4 and not 2; the carrier's (0) admits 0, 2 and 4. REPORTS T33.2; PR, not merged.
- **T33.3 — The search as a pure function (`READY after T33.2`; `src/sim`, no stream change).** Branch `task/T33-3-walk-search`. Implement §5.9 clauses 3–8 as `search(const WalkCells&, const WalkTables&, layer, starts, goals, callback) -> SearchResult` over the cell store of T33.2, with nothing of the stand-in reused. Steps: (1) the open list as an ordered structure keyed by `(u16 score, insertion number)` (clause 7: lowest score first, ties first-in first-out; the original's tree is not copied, its order is); (2) start cells (clause 3): the 3×3 subcell flood from the start position over the start tile's mask bits (island mode 1: `8 + bit` per subcell, `0x1009206c..0x100920e2`), its four edge keys into bits 12–15, code 7, score 0; (3) the default expansion and the four came-from expansions exactly as the table of clause 4 (keys, orders, straight cost `C_dir`, turn cost `C_diag − C_orth`, u16 wrap), the marks `0x80 | dir` with bits 10–15 kept, first discovery decides (no relaxation); (4) goals (clause 5): bit 10 on the goal points' cells, the goal subcell mask, the callback per goal subcell in the order of `goal_order[row]` (row 0..3 by the arrival code, 4 otherwise), the AND of the answers, stop; (5) the result (clause 8): the route as the chain of came-from codes back to a code-7 cell, its tiles, its cost. The finer-route retry (clause 6) is **not** implemented: a `// deviation` comment and the list entry. **Tests:** hand-built grids for each clause: a straight corridor (cost = sum of `C_dir`), an L-shaped corridor (one turn: `+ C_diag − C_orth`), a junction where two routes reach a cell and the first-pushed wins although the other is cheaper (clause 4's "first discovery"), equal scores popping FIFO, a closed code-7 cell, a disc that cuts a goal off, a goal subcell order test per arrival; property tests: the search is deterministic (two runs identical) and never enters a cell with admission 0. REPORTS T33.3; PR, not merged.
- **T33.4 — Into `choose_source`, the goldens, the measurement (`DONE`: merged locally as `acd45cd` on 2026-10-26, round bj; was `REVIEW → fix-up F5`, round bi; `src/sim`, stream change; local-only workflow of round ba).** **Fix-up F5 (round bi, review of `7ed152c`; the decision and the report defects are in round bi at the top of this file). Continue on the same branch and worktree, with new commits on top and no history rewrite. One concern per commit; each step written in REPORTS before the next.**
    1. **The per-tile walk record in the state (the original's `rec2`, §5.9 clause 1).** Add two columns to `TileLayer` (`island.hpp` / `island.cpp`): `walk_code(idx)` (u16) and `walk_mask(idx)` (u16, 9 bits used). Every tile starts at the reset value code 2, mask 0 (`0x1008c80d..0x1008c821`). Add both to the chunk digest, `TileIO` (`state_fields.hpp`), the save/stream and `tools/sim_stream.py`. `kStateHashVersion` stays **112** (not merged yet).
    2. **One writer, the original's (`transport.md` §3.4.13).** Write `write_walk_record(TileLayer&, const BuildingType&, x0, y0, rotation)`. For every tile `(dx, dy)` of `footprint(type, rotation)` it sets code = `walk_code(type, rotation)` and mask = `walk_mask(type, dx, dy, rotation)`; a type with HEAD flag `0x10` ORs the mask into the old one (`0x1008239d..0x100823b0`), every other type replaces it (`0x100825c7..0x100825d9`). Tiles outside the island are skipped. Cite the addresses in a comment.
    3. **Its callers.** (a) Scenario load: `TileObjectSpec` gets `rotation` (u8, default 0). `scenario_formats.cpp` fills it from `o.rotation`, and `scenario.cpp` calls the writer for every tile object, in file order, on its full footprint. The `object` column stays as it is (top-left tile only), so nothing else changes. An item dropped as `DuplicateObject` / `ObjectTileTaken` writes nothing (as today). (b) `create_building` (`create.cpp` step 8): the writer over the building's box at its rotation. Buildings are created after the tile objects (the existing order), so a building overwrites scenery under it; state that order in a comment and in `state.md` §5.9 "As implemented". (c) `remove_building` and the clear-tile command (`apply.cpp` ~575): reset the footprint to code 2, mask 0. **New OQ-201:** what the original writes into a demolished or cleared footprint (the reset, or the scenery from before). The reset is today's behaviour too (object 0 → code 2 / mask 0), so no tick changes. (d) Growth (`island_tick.cpp`): first a game-dir test over `Anlagen.dat`. For every growth chain (the type `find_growing` returns and the object `grow` turns it into), compare `walk_code` and `walk_mask` at all four rotations and every footprint tile. If all are equal, growth writes nothing; say so in a comment with the test's name. If any differ, **stop this sub-step** and list the type pairs in REPORTS (OQ-202).
    4. **`walk_record` reads the columns.** It returns `{tiles.walk_code(i), tiles.walk_mask(i), true}` and nothing else. Remove the loop over the cities' building lists and the rotation-invariance test. Keep the field `supported` and `complete` for the API, but nothing sets them false any more; say so in the comment that named the deviation in `choose_source`. The water stop (OQ-200) and the coast pass (OQ-170) are unchanged.
    5. **Tests:** (a) synthetic: a 2×2 tile object with an asymmetric WALKMASK at rotations 0–3, all four tiles' code and mask checked by hand; a flag-`0x10` type over another object ORs; a later overlapping object replaces; a building over scenery replaces; removal resets. (b) Game dir: the T33.2 road and house cells unchanged; on S01_07_01 the search store is `complete` for every market and workshop at tick 0 (the snapshot's `incomplete` = 0). (c) A mutation that drops the rotation (always 0) must fail (a); one that writes only the top-left tile must fail (a). List both in the report.
    6. **The measurement again, as F1 specified:** the `transport search statistics` case for S01_07_01 and single36, **on `main` (before) and on this branch (after)**. Use one table per scenario with the columns `no_candidate`, `no_goal_reached`, `low_score`, `unreached`, `incomplete`, `found`, and the total as their sum. Add the tick-0 snapshot lines (markets and workshops reached). Paste the `transport run:` lines unchanged under the table. If `found` is still below `main` for either scenario, do F2 again for the top cause: three concrete searches with the home id and tile, the candidate id and tile, and where the search stopped. Do not tune anything.
    7. **Goldens, corpus, docs, gates.** Re-record g2–g6 and `scenario_corpus.txt` in their own commit. In REPORTS, per golden and per corpus field, say what moved, by how much and why: the walk record, the route, the carrier timing of `f746712`. Update `state.md` §5.9 "As implemented in T33" (the per-tile record, the write order, the reset on removal, OQ-200/170 unchanged), close the tile-only stop in `open-questions.md` and add OQ-201 (and OQ-202 if step 3(d) stops). Full gates of §1 item 4 and round ba: the **whole** ctest suite with and without the game folder (totals pasted from `ctest-output.log`: "N tests, M failed"; a failing test is not "expected"), `tools/check_unix_warnings.sh` (GCC 13, Clang 18), clang-format 23.1.3, `lint_sim`, and the sim tests once under WSL Clang ASan/UBSan. Report the branch head and **only** this fix-up's commits. Do not merge.
  **Fix-up (round bg, orchestrator review of `bb58691`; not merged):** the report says "no scenario data changed — only the stream version line", but `scenario_corpus.txt` shows the opposite: in 27 of the 50 scenarios fewer goods move than with the stand-in (`S01_07_01` producers moved 126 → 81 of 302, warehouses 10 → 5 of 14; `single36` 16 → 10 and 4 → 2), and step 6's measurement was replaced by a run time. Do, in this order, each written down before the next: (F1) **the measurement as specified** — T29's classification for `S01_07_01` and `single36` over 150 000 ticks, before (`main`) and after (this branch): searches started, reached, not reached, and the not-reached split by cause; add the cause counters to `TransportStats` if missing (at least: no goal cell reached, search marked incomplete by an unsupported tile object, start point outside the search rectangle, goal outside the disc); a table in REPORTS. (F2) **the cause:** for the top cause, three concrete searches from `S01_07_01` (home, candidate, the tile where the search stopped or the object that made it incomplete, with type ids) and the clause of `state.md` §5.9 or the T33.2 stop that explains each. Suspects to check first: (a) T33.2's tile-object stop (`supported = false` → incomplete → counted not reached) firing on trees, bushes, rocks or other common tile objects; (b) the entrance points (the rotated `ENTRANCE` offsets as goal subcells); (c) a footprint's WALKMASK closing the way the stand-in allowed; (d) the disc. (F3) **the decision is the orchestrator's:** if the cause is a deviation of ours (a stop, a bug), fix it if it is a bug (with a test), else stop and report; if the cause is the original's rule (a faithful result), report it with the evidence — do not tune anything to raise the numbers. (F4) correct the report's "no scenario data changed" line, list per golden and per corpus field what moved and why; re-run the full gates (ctest of **all** tests, with and without the game folder: the report's 635 is the sim subset, the suite has about 1 000). Commit on the branch; report the branch and commit. **Resume point (round bb, the first executor stopped):** branch `task/T33-4-walk-integration` at **bd336ce** (`wip(sim): …`, saved by the orchestrator from its worktree): step 1 is written in REPORTS T33.4; steps 2–4 are coded (`choose_source` and `path_home` on `walk_search` for jobs `0x34`/`0x33`, the walk-table digest in the header and the replay check, `kStateHashVersion` 112, `tools/sim_stream.py`, edits in eight test files) but **not built or tested**; steps 5–6 not started. The next executor checks the branch out in a fresh worktree (`git worktree add ../anno1503-T33-4 task/T33-4-walk-integration`), reads the WIP diff (`git show bd336ce`) against the steps below, builds, fixes, and goes on from step 2's verification; it keeps the WIP commit and adds its own commits on top (no history rewrite). Originally: branch `task/T33-4-walk-integration` from local `main`. **Steps, each written down before the next:** (1) read `transport.cpp` `choose_source`, `walk_rule` and the route search and write in the report, per caller, what the stand-in is asked (which starts, which goals, what it returns, what `choose_source` does with it) and the matching `walk_search` call (the starts: the home's entrance points as subcell points; the goals: every candidate's entrance subcells; the callback: `Expand` while the job's selection goes on, the selection by `choose_source`'s existing score); (2) implement it for jobs `0x34` (layer 3) and `0x33` (layer 0) only, the other jobs unchanged; (3) a search that reports `complete = false` (an unsupported tile-only object, T33.2) is counted in `TransportStats` and treated as not reached, with a comment naming the deviation; (4) the stream: the walk-table digest in the header next to the registry digest and in the replay log's config check, `kStateHashVersion` 112, `tools/sim_stream.py` updated; (5) the goldens and the corpus re-recorded, the diff's size stated in the report (which goldens moved, how many corpus lines); (6) the measurement. Replace the stand-in in `transport.cpp` (`walk_rule`, the area search in `choose_source` and the route search) by T33.2/T33.3 for the cart (job `0x34`, layer 3) and the workshop carrier (job `0x33`, layer 0): the starts (the home's entrance points), the goals (the candidates' entrance subcells, §5.9 clause 7), the callbacks (the original's selection by score, already in `choose_source`: keep its scoring, change only reachability and route cost). `kStateHashVersion` 112; the walk-table digest added to the stream header next to the registry digest (and to the replay log's config check, `replay.hpp` `registry_digest`'s neighbour); the goldens g2–g6 and `fixtures/synthetic/sim/scenario_corpus.txt` re-recorded in the same commit with the reason; `docs/sim/state.md` §5.9 "As implemented in T33" (the deviations: no finer-route retry, the shore stand-in, the coast pass); OQ-160 / OQ-161 / OQ-171 status lines. **Measurement:** re-run T29's `S01_07_01` and `single36` classification (150 000 ticks): reached markets and producers before/after, in the report as a table; if a number gets worse, name the cause before reporting. REPORTS T33.4; PR, not merged; the orchestrator reviews against the original in the Wine runner (T30b.5–7) before the merge.
- **T33.5 — The original's tile record across an object's life (OQ-201, OQ-202) · stage 1 `DONE` (merged locally as `d30d20b`, round bk; was `READY`, round bj; RE, docs only) · stage 2 `READY` (round bk; below; the main executor) · `PARALLEL-OK` with P1 (stage 1 edits `docs/re/transport.md`, `docs/open-questions.md`, `docs/sim/state.md` §5.9 only) · branch `task/T33-5-tile-record-re` from local `main` at `acd45cd` · worktree `E:\anno1503-worktrees\anno1503-T33-5` · new OQs from OQ-203.**
  - **Why:** T33.4 stores the walk code and mask on every tile, but our `TileLayer` keeps a tile object's id only on its origin tile, and no in-footprint offset or rotation at all. So a cleared multi-tile object leaves its other tiles' walk record behind (OQ-201), and growth cannot write a successor's record whose rotation is lost (OQ-202). The original keeps all of it on every tile (island slot `+0x84` = `0x10083a90`, `annowelt.md`, D). Stage 1 reads what the original writes into that record at placement, growth, removal and clearing, so that stage 2 copies the record and its writers, not a guess.
  - **Stage 1 steps (in order; §1 items 7 and 11; each written into REPORTS before the next):**
    1. **Set up:** `git worktree add E:/anno1503-worktrees/anno1503-T33-5 -b task/T33-5-tile-record-re acd45cd`. No build (docs only). Tools: `tools/disasm.py`, `tools/xref.py --reloc`; disassembly output goes to the worktree's `cache/`, never into git.
    2. **The record, one table (D or open with the next address):** the tile stride (8 bytes: growth loop `add esi, 8` at `0x1008b714`) and every byte and bit of both dwords. For each, give the field, its writer and its reader. Sources:
       - the reader `0x10083a90`: object id word 0, byte 2 dx bits 0–2 / dy bits 3–5 / rotation bits 6–7, byte 3 phase `& 0x3f`, byte 4 height;
       - the placement writer's first-dword stores `0x10082447..0x100824c5`;
       - its second-dword stores: code `0x100825af..0x100825b7`, mask `0x100825c7..0x100825d9`, the OR branch `0x1008239d..0x100823b0`;
       - the reset `0x1008c80d..0x1008c821`.

       Name bit `0x80` of byte 3 (growth keeps it: `and byte [esi], 0x80` at `0x1008b6ca`), or leave it open with the next address. Mark every unused bit as unused, with the evidence.
    3. **Growth (OQ-202), `0x1008b5f0`:**
       - Its own stores are the phase byte (`0x1008b681`, `0x1008b6a6`, `0x1008b6ca`) and `inc word [esi − 3]` (`0x1008b6c6`). Resolve its five virtual calls: `[edx + 0x70]` at `0x1008b637`, `[edx + 0xe8]` at `0x1008b648`, `[eax + 0x80]` at `0x1008b687`, `[edx + 0x7c]` at `0x1008b6b4`, `[edx + 0x188]` at `0x1008b709`. The last has receiver `ebx`, likely the island (it reads `[ebx + 4]` / `[ebx + 6]`), with the arguments (&tile xy, `0xce`, `0`).
       - For each call, trace the receiver to the vtable that holds the slot (with the installer address), then give the target address, and say whether the target or any callee writes a tile record's byte 2 or its second dword.
       - S13 named AnnoWelt `0x1008f2d0` as the redraw notice. Confirm by the vtable that it is island slot `+0x188`. Then read its calls `[edx + 0xbc]` (`0x1008f321`), `[edx + 0xc]` (`0x1008f367`) and `0x1008cb30` (`0x1008f463`) down to their stores.
       - **Answer OQ-202:** (a) the original leaves the walk record as placement wrote it, or (b) it rewrites code and mask for the successor, and with which rotation (byte 2 bits 6–7?). Give the addresses either way.
       - State also how a multi-tile growing item grows: every footprint tile is visited and its own id word incremented.
    4. **Removal and clearing (OQ-201):**
       - Start from `RemoveAt` `0x10028fd0` and its tail to island slot `+0x7c` (state.md O-5: "tile clearing"). Then find the path the demolish tool takes for a tile object that is not a building (from the command apply function `0x100589a0`, modes 2, 6, 7, 8, 9, named in `state.md`'s command evidence line next to `RemoveAt`; first establish which module holds it, since that line lists AnnoFrame and AnnoWelt addresses side by side, and say how).
       - For each path, what does it write into the footprint's first and second dwords: the reset values of `0x1008c80d..0x1008c821`, or a record kept from before the placement (is there one)?
       - Does it find the footprint from any tile through byte 2, as `0x10083a90` does?
       - **Answer OQ-201,** or narrow it with the next address.
    5. **What stage 2 must change (docs, no code):** add a paragraph "Differences to the original's tile record (for T33.5 stage 2)" in `docs/sim/state.md` §5.9 "As implemented in T33" with two parts:
       - the differences: the object id only on a tile object's origin tile, no byte 2, and where height lives in the sim (`Terrain`, OQ-060);
       - every place in `src/sim` that reads or writes `tiles.object(` / `set_object(` or `set_phase(`, as `file:line` with one sentence on what it assumes. Find them with `git grep -n "object(\|set_object(\|set_phase(" -- src/sim`; at least `scenario.cpp` `build_island`, `island_tick.cpp` `grow_chunk`, `apply.cpp` `demolish`, `create.cpp`, `walk_cells.cpp`, `placement.cpp`.
    6. **Docs:** a new `docs/re/transport.md` §3.4.17 "The tile record across an object's life" (the table of step 2; growth; removal; clearing; each D or open with the next address); OQ-201 and OQ-202 answered or narrowed; new questions from OQ-203. REPORTS section "T33.5 stage 1", committed on the branch as the last commit; not merged, not pushed.
  - **Stage 1 acceptance:**
    1. the field table, every byte and bit D, unused with evidence, or open with the next address;
    2. all five growth calls resolved, and OQ-202 answered or narrowed;
    3. OQ-201 answered or narrowed for both removal and clearing;
    4. the §5.9 difference list with `file:line` for every reader and writer;
    5. nothing of the original in git (no listings, no bytes);
    6. report and commit on the branch, not merged.
  - **Stage 2 — the tile layer as the original's record (`READY`, round bk; `src/sim`, stream change; the main executor) · branch `task/T33-5b-tile-record` from local `main` at the round-bk board commit · worktree `E:\anno1503-worktrees\anno1503-T33-5b` · new OQs from OQ-206.** The rule is `transport.md` §3.4.17, merged as `d30d20b`. Every step below cites it; nothing beyond it is implemented.
    1. **Check the limits first (game dir, a test, no code change yet):**
       - Over `Anlagen.dat` (through `TypeRegistry::build`), report the largest footprint width and height of any type.
       - Count the scenario tile objects whose type has HEAD flag `0x10` (`flags_word & 0x10`) over the 50 corpus files, through `island_from_insel`.
       - Put both numbers in REPORTS before step 2.
       - If any footprint edge exceeds 8, **stop** and write the question as OQ-206. The original's byte 2 holds dx/dy in 3 bits each (D, `0x10082447..0x10082474`).
    2. **Three columns (`island.hpp` / `island.cpp`):**
       - `foot_dx(idx)` (u16), `foot_dy(idx)` (u16) and `foot_rotation(idx)` (u8), reset 0 (`0x1008c81b` writes byte 2 = 0).
       - They are the original's byte 2, **widened** as CLAUDE.md asks ("widen when data enters the engine"). The original keeps 3 + 3 + 2 bits; step 1 shows that no shipped type needs more. Say so in a comment with the step 1 numbers.
       - Add the columns to the chunk digest, `TileIO`, `kTileBytes` (17 → 22), `tools/sim_stream.py`, `state.md` §9.2, the hostile-load byte checks, and the `config.hpp` bound comment.
       - `kStateHashVersion` **113**.
    3. **One placement writer, the original's (`0x10082040..0x100825fe`).** Extend `write_walk_record` into `place_object_record(TileLayer&, const BuildingType&, x0, y0, rotation, phase)`.
       - **Ordinary path:** on every tile `(dx, dy)` of `footprint(type, rotation)` inside the layer it writes the object id (`0x1008244a`), `foot_dx` = dx, `foot_dy` = dy, `foot_rotation` = rotation (`0x1008245c..0x10082480`) and the phase (`0x100824c5`), then the walk code and mask as today.
       - **Overlay path (`flags_word & 0x10`, `je 0x100823ed`):** it writes **only** the code and ORs the mask. The original's overlay path does not execute the first-dword stores (§3.4.17 "Placement's two paths").
       - If step 1 found scenario tile objects with flag `0x10`: implement the overlay path anyway, and list per file how many objects lose their id on the tile. If that count is above 0, **stop before step 4** and report the list; the orchestrator decides.
       - Byte 3 bit 7 (OQ-203) and bit 6 (growth parity) are not modelled; say so in the comment.
    4. **Its callers:**
       - (a) `build_island` (`scenario.cpp`): one `place_object_record` call per tile object, in file order, replacing both the origin-only `set_object` / `set_phase` loop and the walk-record loop.
       - (b) `create_building` (`create.cpp` step 8): the same writer replaces the `set_object` box loop and the `write_walk_record` call. Buildings are still created after the tile objects.
       - (c) Nothing else writes object ids.
    5. **One clear function, the original's clear arm (`0x10082c30`):** `clear_object_record(TileLayer&, const TypeRegistry&, x, y)`.
       - If the id at `(x, y)` is 0, it writes nothing (`0x10082c6a`).
       - Otherwise the origin is `(x − foot_dx, y − foot_dy)` and the rotation is the tile's `foot_rotation` (`0x10082ca5..0x10082cda`).
       - For each tile of `footprint(type of that id, rotation)` from the origin whose id, rotation and offset match (`0x10082d98..0x10082de7`), it writes id 0, `foot_dx` / `foot_dy` / `foot_rotation` 0, keeps the phase, and writes code 2 and mask 0 (`0x10082f00..0x10082f1f`).
       - The replacement arm (the high nibble of island `+0x18`, `0x10082e43..0x10082efc`) is **not** modelled: add a `// deviation` comment naming OQ-201.
       - Callers: `remove_building` (`create.cpp`, replacing the box loop; the original reaches `0x10082c30` from `RemoveAt` through island slot `+0x7c`, `0x100292ef` → `0x1008ee70` → `0x1008eec3`), and the clear-tile branch of `demolish` (`apply.cpp`), which may now name any tile of the object.
       - The non-building command path is still open (OQ-201). Say in the comment that the clear-tile branch uses the building's clear arm as a stand-in.
    6. **Readers that change without a code change; check each, and say in REPORTS what moved:**
       - `grow_chunk` (`island_tick.cpp`): every footprint tile of a growing item now holds the id, so each tile advances on its own visit, as the original (§3.4.17, growth: "each tile that holds the id word advances on its own visit").
       - `stand_in_placement_rule` (`placement.cpp:50`): it now sees the id on every footprint tile of a tile object, so a building over any tile of a 2×2 rock is `Occupied`, not only over its origin.
       - the road-neighbour statistic (`transport.cpp:441`): roads are 1×1, so expect no change; check.
       - the `create.cpp:458` footprint check: it expects an id on every building footprint tile, which is unchanged.
    7. **Tests:**
       - (a) Synthetic, `walk_cells_test.cpp` or a new `tile_record_test.cpp`:
         - a 2×3 type at rotations 0–3: every footprint tile's id, dx, dy, rotation, phase, code and mask, by hand;
         - an overlay type over another object: the id, offset, rotation and phase of the old object stay, the code is replaced and the mask is ORed;
         - `clear_object_record` from each tile of the 2×3 object clears all six tiles and keeps their phase;
         - a tile whose id was changed by growth is skipped by the clear (id mismatch);
         - removal of a building restores code 2 / mask 0 on every footprint tile;
         - a scenario-like island where a building is placed after scenery: the building's tiles hold the building.
       - (b) Game dir:
         - S01_07_01 at tick 0: for every tile object, every footprint tile holds its id and a correct dx/dy/rotation;
         - the T33.4 tick-0 snapshot (`incomplete` 0) still holds.
       - (c) Mutations, each failing (a):
         - the writer writing only the origin;
         - the clear ignoring the offsets (clearing from the clicked tile);
         - the overlay path writing the id.
    8. **Measurement (as in F5 step 6):** the `transport search statistics` case for S01_07_01 and single36 on `main` (before) and this branch (after), one table per scenario, `transport run:` lines pasted. Then the per-file corpus changes of producers / warehouses moved, as F5's table.
    9. **Goldens, corpus, docs, gates:**
       - Re-record g2–g6, `goldens.txt` and `scenario_corpus.txt` in their own commit. In REPORTS, say per golden what moved and why. The corpus counts of buildings, tile objects and warnings must **not** change; if any does, stop and list the files.
       - `state.md` §5.9 "As implemented in T33" (the record columns, the writer's two paths, the clear arm, the deviations OQ-201 replacement arm / OQ-203 / OQ-204 / OQ-205) and §9.2.
       - OQ-201 status line: the sim now uses the building clear arm for both removal and clearing; the replacement arm and the non-building command path stay open.
       - Full gates of §1 item 4 and round ba: the whole ctest suite with and without the game folder (totals pasted), `tools/check_unix_warnings.sh`, clang-format 23.1.3, `lint_sim`, `sim_stream.py --check`, the sim tests once under ASan/UBSan.
       - REPORTS "T33.5 stage 2"; commit on the branch; not merged.
    - **Stage 2 acceptance:**
      1. step 1's two numbers;
      2. the three columns hashed, saved and mirrored, stream 113;
      3. one writer with both paths, one clear function, and the callers of steps 4–5;
      4. tests 7(a)–(c) with the mutations failing;
      5. the measurement;
      6. goldens and corpus re-recorded with the moved fields explained, and the corpus building, object and warning counts unchanged;
      7. docs;
      8. gates green with totals;
      9. report and commit, not merged.
    - **Fix-up F1 (round bm, orchestrator review of `802910d`; continue on the same branch and worktree, new commits on top, no history rewrite; each step written in REPORTS before the next).**
      - **Why:** the executor stopped correctly at step 3: by the stage-1 text, 121 600 scenario tile objects of flag-`0x10` types (rocks `FELSEN`, stones `STEIN`, cliffs `KLIPPE`, rivers `FLUSS`, waterfalls, an ice hole, mines and quarries) would get no id on any tile.
      - **The stage-1 text was wrong, and the orchestrator missed it in round bk.** The scenario loader places every item through `0x10082040`:
        - island slot `+0x78` = `0x1008ec90` builds the placement record at `0x1008962a..0x1008966c` and calls `0x10082040` at `0x1008ecbd` unless flag bit 31 is set; the loader passes `0x4000000`.
        - The overlay path has **its own first-dword stores** at `0x1008220e..0x10082286`: the id (`0x10082215`), the dx bits (`0x10082227`), the dy bits (`0x1008223d`), the rotation (`0x1008224c`), the height byte (`0x10082260`), the phase (`0x10082270`), and bit 7 (`0x10082286`).
        - It skips one tile only: one whose current id is non-zero, whose type has HEAD flag `0x10` (`0x100821e6..0x10082206`), and whose byte 3 has bit 7 set (`0x10082208..0x1008220c`).
        - Then a second loop replaces the code and ORs the mask (`0x10082342..0x100823e2`).
      - **Bit 7 (OQ-203) is the block-anchor flag.** The writer walks the footprint in world-aligned 8×8 working blocks: the block origin is `x & ~7` at `0x10082109..0x10082116`, and the blocks step by 8 at `0x1008260c` / `0x1008262e`.
        - At each block start the flag is set: `mov bl, 1` at `0x1008213b`, copied to `[esp + 0x74]` at `0x1008213d`.
        - The ordinary path clears it after every tile (`0x100824c8`).
        - The overlay path clears it only after a tile it wrote (`0x10082289`; a skipped tile jumps to `0x1008228b`, past the clear).
        - So the first written footprint tile of each 8×8 block gets bit 7: at most four per object, since step 1 found no edge above 8.
      - **Steps:**
        1. **Read it yourself first** (`python E:\anno1503-worktrees\anno1503-T33-5b\tools\disasm.py E:\anno1503\game\app\AnnoWelt.dll 10082040 1600` and `… 1008ec90 300`). Confirm or correct each address above in `E:\anno1503-worktrees\anno1503-T33-5b\docs\re\transport.md` §3.4.17: the paragraph "Placement's two paths" and the byte-3 bit-7 row. Write OQ-203's new status line, with the meaning and the readers found (the skip test is one), in `E:\anno1503-worktrees\anno1503-T33-5b\docs\open-questions.md`. Also state whether the second overlay loop (`0x10082342..0x100823e2`) writes the code and mask on skipped tiles too. If any reading differs from the above, **stop** and write the difference in REPORTS.
        2. **One more column:** `foot_anchor(idx)` (u8, 0 or 1; reset 0, `0x1008c81e` writes byte 3 = 0), hashed, saved and mirrored like step 2's columns; `kTileBytes` 23, `kStateHashVersion` stays **113**.
        3. **`place_object_record` as the original:** iterate the footprint in world-aligned 8×8 blocks with the original's loop order. Blocks go row by row, then column by column; inside a block, rows outer and columns inner, clipped to the footprint (read the order from the code; don't assume it).
           - **Ordinary path:** write the first-dword fields and the anchor (1 on the first tile of each block, then 0), then the code and mask.
           - **Overlay path:** the same first-dword fields with the skip test above, and the anchor cleared only after a written tile; then the code and mask as step 1 found.
           - The height byte is not a tile column (the sim's heights are `Terrain`, OQ-060): a `// deviation` comment.
           - The working-block edge 8 is the original's rule, not a limit: a named constant with the addresses and a comment.
        4. **Then go on** with the stage 2 entry from step 4. Step 3's stop condition is replaced: report per file how many tile writes the overlay skip test suppressed.
      - **Acceptance:**
        1. §3.4.17 and OQ-203 corrected, with every address confirmed;
        2. the anchor column and the block-ordered writer, with unit cases: a 3×3 object across a block corner has four anchors; an overlay over another overlay's anchor tile is skipped there and written elsewhere; an overlay over a non-overlay object takes its tiles;
        3. stage 2 acceptance items 2–9.
  - **Stage 2 and F1 are `DONE`:** merged locally as `c290945`, round bo.
  - **Stage 3 — removal from the tile the original passes (`DONE`, merged as `4972bd8`, round br; `src/sim`; the main executor) · branch `task/T33-5c-remove-tile` from local `main` at the round-bo board commit · worktree `E:\anno1503-worktrees\anno1503-T33-5c` · new OQs from OQ-260.**
    - **Why:** `remove_building` (`E:\anno1503\src\sim\src\create.cpp`) calls `clear_object_record` at the building's **origin** tile.
      - Mines and quarries are overlay types (flag `0x10`) placed over mountains (`FELSEN`, also overlay). Where a mountain's anchor tile lies under a mine's footprint, the overlay skip leaves the mountain's id there.
      - If that tile is the mine's origin, the clear reads the mountain's id and clears the mountain. The mine's ids then stay on tiles without a building.
      - The original's `RemoveAt` (`0x10028fd0`) passes the tile its building lookup used (`0x100292d8..0x100292ef`), not necessarily the origin.
    - **Steps:**
      1. **RE (D or open):**
         - the building lookup `[[city + 0x1c]] + 0x94` that `RemoveAt` calls at `0x10028fed`, on the in-game island vtable `0x100cac2c` (`python E:\anno1503\tools\disasm.py E:\anno1503\game\app\AnnoWelt.dll <va>`): does it find the building through the tile's id, or through the city's building lists or rectangles?
         - which (x, y) `RemoveAt` hands to island slot `+0x7c` (the stack slots read at `0x100292d8` and `0x100292e7`, traced to `RemoveAt`'s arguments);
         - the tile the demolish command (AnnoFrame `0x10058e90` mode 6, `0x10058f7a`) and the city removal (`0x10058f80`, D9) pass.

         Write it in `docs/re/transport.md` §3.4.17 (removal) and OQ-201.
      2. **Measure:** with the game folder, over the 50 corpus files after load, count the buildings with at least one footprint tile that does not hold their own id (the overlay skip under buildings), and those whose **origin** tile does not. Per file, in REPORTS.
      3. **Fix, as step 1 found:**
         - If the original clears from a tile that holds the building's id, `remove_building` and `demolish` pass such a tile: the named tile when it holds the id, else the first footprint tile in the writer's order that does. That is the faithful rule and needs no option.
         - If the original can clear another object (the mountain), that is the original's bug. Then add `Rules::removal_clear_tile` (default: the original's behaviour; the enhanced value: a tile holding the building's id), a row in `docs/sim/rules-presets.md`, and the validation as the other `Rules` options.
      4. **Tests:**
         - a synthetic mine-like overlay building over a mountain-like overlay object whose anchor tile is the building's origin: after removal no tile holds the building's id, and the mountain is as the rule of step 3 says;
         - the same with the anchor elsewhere;
         - a mutation (clear from the origin regardless) fails.
      5. **Gates** (§1 item 4, round ba); goldens and corpus only if they move (say why); REPORTS "T33.5 stage 3"; not merged.
    - **Acceptance:**
      1. step 1 D or open with next addresses;
      2. the step 2 counts;
      3. the fix with its tests and the mutation;
      4. gates green with totals;
      5. report.

### T34 — The float walk: the original's x87 walk reproduced bit-exactly (OQ-162; owner decision 2026-10-24: "float number walk") · stage 1 `DONE` (merged as `71446c9`, round br; ADR Proposed) · stage 2 `LATER` (after T33 and the approval of ADR 0002) · stage 1 `PARALLEL-OK` (docs only: `docs/re/transport.md` §4, a new `docs/adr/0002-float-walk.md`, `open-questions.md` OQ-162 and new ones) · branch `task/T34-float-walk-re` from `main` · worktree `C:\Users\altin\anno1503-T34` · new OQs from **OQ-195** (to 199)
- **Why:** the owner chose the float walk, so trips take the original's time. The ground rule stays (CLAUDE.md, `lint_sim.py`): `src/sim` computes in integers and no float enters hashed state. So the walk's floats become their bit patterns in the state, and the arithmetic becomes an integer emulation of exactly what the x87 did: the original's result on every platform. That needs the precision mode and every rounding point, or the "float" would not be the original's float.
- **Stage 1 steps (in order; §1 item 7):**
  1. **Set up** from `main`.
  2. **The precision and rounding mode the walk runs under (D):** every `fldcw` in the game modules and the control words it loads; the DirectDraw / Direct3D set-up (`SetCooperativeLevel` flags `DDSCL_FPUSETUP` 0x1000 / `DDSCL_FPUPRESERVE` 0x2000, any device-creation FPU flags); the MSVC 6 runtime start-up. State the mantissa precision (24 / 53 / 64 bits) and the rounding mode while the walk (`0x10057ae0` and its callees) runs, and how each float → int conversion rounds (MSVC 6's `_ftol` switches the control word to truncation: check every conversion in the walk).
  3. **The exact operation sequence of the walk (D):** from `transport.md` §4 (time = `u16(t − stamp) · 0.2f`; `time · speed` along f32 segments of `count / 3` tiles; diagonals × `1.41421354f`; the position three floats; the rest of the time carried to the next action): every operation in order, every load and store width (a `fstp dword` rounds to binary32, a value kept on the x87 stack does not), every constant's bit pattern, every compare and conversion. Pseudo-code with addresses in §4.
  4. **The path the walk follows** (the sub-cell segments, OQ-161, from T30 step 2): what a segment is. If T30 has not read it, write the dependency and go on.
  5. **ADR `docs/adr/0002-float-walk.md`:** the decision (the owner's) and the model: a small integer soft-float in `src/sim` (binary32 add, sub, mul, div, compare and int conversions, with the precision and exponent-range behaviour of step 2; round to nearest even unless step 2 says otherwise), the state fields as `u32` bit patterns, the lint unchanged; the rejected alternative (native `float` with strict flags: x87 and SSE differ in precision and exponent range, and the lint forbids it); the stream bump and goldens; the test plan (the soft-float against the host's IEEE `float` in the tests, which may use floats, over ≥ 10^8 random operands per operation plus the edge cases: denormals, overflow, ties; the walk against a hand-derived x87 trace; later against the original's figure positions from T30 step 7's reader); the performance budget (the walk's share of a tick before and after, `bench_sim`).
  6. **Docs.** §4, OQ-162 status ("decided 2026-10-24 (owner): the float walk; the model per ADR 0002"), REPORTS.md with the acceptance ticked, PR. No `TASKS.md` edit.
- **Stage 1 acceptance:** (1) the precision and rounding mode D with addresses, or open with what was searched; (2) the walk's operation sequence D, every rounding point named; (3) ADR 0002 with the test plan; (4) nothing of the original in git; (5) report, PR, not merged.
- **Stage 2 (one line):** the soft-float module and its tests, the walk on it, the position and time fields as bit patterns, a stream bump with every golden re-recorded, a cart's trip compared with the original's in the Wine runner.

### T35 — `Result::value()` and `error()` must not throw in Release (a policy hole found by the 2026-10-24 snapshot) · `READY after T30b.8` (round ac; **one sub-task, `src/formats` only**) · `PARALLEL-OK` (touches `src/formats/include/anno/formats/result.hpp`, one test file, `docs/formats/README.md` one sentence) · branch `task/T35-result-abort` from `main` · worktree `C:\Users\altin\anno1503-T35` · no new OQ
- **Why:** `Result<T, E>::value()` and `error()` guard the wrong arm with `assert(ok())`. Under `NDEBUG` (the Release configuration the `msvc` preset builds and tests) the assert is gone and `std::get` throws `std::bad_variant_access` out of `formats`, which CLAUDE.md forbids ("no exceptions in `formats`/`sim`"). No caller is known to misuse it; the rule must hold regardless.
- **Steps (in order; §1 items 7 and 11):**
  1. Worktree: `git worktree add /c/Users/altin/anno1503-T35 -b task/T35-result-abort origin/main`; build: `cmake --preset msvc` then `cmake --build --preset msvc` (foreground; the first build takes several minutes).
  2. In `result.hpp` replace each `assert(ok());` / `assert(!ok());` by an unconditional check that calls `std::abort()` on the wrong arm (a small private helper `require(bool)` with `[[unlikely]]`; include `<cstdlib>`; keep `<cassert>` only if still used). Keep the three `value()` overloads and `error()` as they are otherwise; no other file changes.
  3. Add a death test only if doctest supports it on all three compilers (it does not: skip) — instead add a case in `src/formats/tests/` that checks `ok()` and `operator bool` on both arms and that `value()` on an ok result and `error()` on an error result return the stored objects (the positive paths); state in the report that the abort path is not unit-tested and why.
  4. Build and run: `ctest --preset msvc -j 16 -R formats` with and without `ANNO1503_GAME_DIR`; clang-format 23.1.3 on the two files; the GCC 13 `-O3 -Werror` check in WSL (`tools/check_unix_warnings.sh`).
  5. `docs/formats/README.md`: one sentence under the `Result` contract: a misuse aborts, it never throws. REPORTS.md section T35 with the gates' output lines; PR, not merged.
- **Acceptance:** (1) no `assert` guards the variant access; a misuse aborts; (2) the positive-path test; (3) the gates of step 4 pasted; (4) the README sentence; (5) report, PR, not merged.

### T20 — The scenario script engine in C++ · **stage 1 `DONE`** (merged as d94c326 on 2026-10-23 o, PR #31, CI green; §4; was `READY` in round m: the reader, `anno-dump bigbrother`, `agree.py`) · stage 2 `LATER` (the evaluation and the step before every advance, after the OQ-123 RE and S14) · `PARALLEL-OK` with S13 (new files only: `src/formats/{include/anno/formats,src}/bigbrother.*`, `src/formats/tests/bigbrother_test.cpp`, `bigbrother_corpus_test.cpp`, `src/tools/anno_dump/dump_bigbrother.cpp`, one row each in the two `CMakeLists.txt`, `tools/agree.py`, `fixtures/synthetic/bigbrother_synthetic/`) · branch `task/T20-bigbrother-reader` from `main` · worktree `C:\Users\altin\anno1503-T20`
- **Why now:** T17 (spec and Python reference), T18 and T19 (the interpreter's reading, D) are merged; S14 (the scenario adapter) and the session's script step (O-34) need a C++ reader of the script; the reader is a `formats` port with an existing oracle (`tools/bigbrother.py`), so it fits the executor's free slot while S13 runs.
- **Stage 1, do** (spec: `docs/formats/bigbrother.md` §1–§3, §3.4 the interpreter's reading; `docs/re/annoscene.md` §12; the oracle: `tools/bigbrother.py` `check` / `dump`):
  1. `anno::formats::bigbrother`: `parse(std::span<const std::uint8_t>) -> Result<Script, Error>` (Latin-1 bytes, the tokens of §2, the six sections of §3 in order, the define lines of §3.1–§3.3 and §3.5–§3.6, the kernel grammar of §3.4), `emit(const Script&) -> std::vector<std::uint8_t>` byte-identical to the input for every accepted script (line endings, blanks, comments and touching tokens kept as read), and the interpreter's reading as functions over the tree: `grouped(condition)` (equal rank, grouped to the right; parentheses as written) and `read_on(script)` (the statements without `;` and what each swallows, §3.4, OQ-121). No evaluation, no sim link in stage 1.
  2. `anno-dump bigbrother <file.sze|.sav> [--full]` printing what `tools/bigbrother.py dump <file>` prints, byte for byte (fix the Python output first if it is unstable, in the same PR, with the reason); `tools/agree.py` gets the `bigbrother` cases (synthetic fixtures always; the 50 `.sze` with the game dir) and `ANNO_DUMP_PORTED_COMMANDS` the command.
  3. Tests (`src/formats/tests/`): the round trip on our own synthetic scripts (`fixtures/synthetic/bigbrother_synthetic/`: the grammar of §3.4, every token kind, mixed conditions, each of the four kinds of statement without `;`, touching tokens, Latin-1 bytes, both line endings; **our own text, nothing copied from the shipped scripts**); each rule of §2–§3 refused when broken with the error naming the line; the grouping against C precedence; `read_on`'s four shipped cases reproduced on synthetic copies of their shapes; the corpus test (`ANNO1503_GAME_DIR`): 50 of 50 parse and re-emit byte-identically, the counts of §3.4 (35 `INITBLOCK`s, 271 `SET`s, 308 `IF`s, 3 `ELSE`s, 382 assignments, 1 656 calls), 28 mixed conditions in 10 files, three block statements without `;`, one swallowed statement (`MINIMAP_ALERT`, S01_09_01).
  4. **No hardcoded limits:** no fixed section count or name table in the reader beyond the six section names of §3 as data (a seventh name such as `STARTUP`, OQ-122, is kept as an unknown section, not refused); the vocabulary of §4–§6 is **not** compiled into stage 1 (a call is a name plus values; a later stage binds names to handlers).
  5. Docs: `bigbrother.md` §8 ("C++ reader: `anno::formats::bigbrother`, agree.py"), the status line; PLAN M1 row for the chunk; a REPORTS.md section; a PR.
- **Stage 1 acceptance:** items 1–5 done; `agree.py --commands bigbrother` agrees with and without the game dir; corpus 50/50 byte-identical; `clang-format` 23.1.3; WSL GCC 13.3 and Clang 18 `-O3 -Werror` clean; no exception, no `reinterpret_cast`, `Result` errors with the byte offset and the line; mutation evidence: the grouping changed to left-grouped fails the grouping test and the corpus case (the 28 conditions); `read_on` stopping at the block's `}` fails the swallowed-statement case; a dropped CRLF/LF distinction fails the round trip.
- **Stage 2 (`LATER`, after the OQ-123 RE of the action and condition handlers, `0x10011ed0`–`0x10013dab`, and S14):** the evaluation (`&&` short-circuits, `||` evaluates both, THEN/ELSE/neither as in §3.4), the step before every advance (`INITBLOCK` once while `SCHUETTIINITSIG`, the round-robin `IF` cursor), the `[SIGNALS]` state, the timers through `ScriptTimers`, actions as commands or seams per the lockstep decision (§5.1), and the **owner decision 2026-10-23 (OQ-121):** an opt-in `rules` option restores statements that a missing `;` swallows (`MINIMAP_ALERT( 640, 950 )` in S01_09_01); the default keeps the original's loss. Both readings are tested. Acceptance is written when stage 2 is unlocked.

## 4. Review log (orchestrator)

Moved to [`HISTORY.md`](HISTORY.md) §4 in round ct. New review notes go into the round blocks at the top and into `REPORTS.md`.
