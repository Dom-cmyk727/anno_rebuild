# SHORE-2: the sea's edge follows the terrain at the original's plane

**Status:** RELEASED 2026-11-13 by the Claude Code orchestrator to **the Windows executor**, under the standing go,
after DEPTH-INTEGRATE-1's acceptance ([review](../reviews/2026-11-13-depth-integrate-1-review.md)).

**Base:** `origin/task/DEPTH-INTEGRATE-1` at `eff68d5e` (accepted; it lands on `main` before this packet). If it is
on `origin/main` when you start, base on `origin/main` instead. Branch `task/SHORE-2`, in a new worktree. About 3–4
hours. Never rebase or force-push.

**Rules:** the owner rule of 2026-11-12:
- one concern;
- no reviewer subagents;
- one REPORTS entry of about 60 lines, READY FOR REVIEW.

Gates follow `TASKS.md` §1 item 4: a render change, so the render and app lanes, the GPU tests on the 4090 and the
Radeon, and hosted CI.

## Why

The owner chose the original's sea plane, 26.5 `HEIGHT`, on 2026-11-13. With it, the water edge on wide beaches is
stepped in 8 × 8-tile blocks. DEPTH-INTEGRATE-1's pictures show it around the owner's Kontor.

**The cause** (`docs/render/iso.md`, the sea's "Where" row):
- **The gate:** sea sprites are drawn only in 8 × 8-tile chunks whose sea bit is set. Our approximation of that bit
  is "the chunk holds a tile whose corner average is ≤ 19", the original's land classifier `0x100900c0`.
- **The mismatch:** 19 lies below the 26.5 plane. A chunk of low beach, every tile between 19 and 26.5, gets no sea
  at all, although its terrain lies below the plane. Inside a sea chunk the per-pixel test already follows the
  terrain.
- **The original:** it draws its sea on every pixel whose cached height lies below the plane
  ([`annodisplay.md`](../../re/annodisplay.md) §9.3), inside chunks whose header bit `+0x32` is set. **How that bit
  is set has never been traced** (`docs/re/annowelt.md`, open list; OQ-026).

## Do

1. **Trace the chunk sea bit, time-boxed to about one hour.**
   - **Where:** `AnnoWelt.dll`; `0x1009f870` reads `+0x32` and calls `0x1009f4c0` per chunk. Find the writer.
   - **How:** use `tools/disasm.py` (Capstone and pefile). Install them into an ignored cache with
     `pip install --target` if they are missing. Ghidra on Windows is not an option.
   - **Record** the rule with addresses in `docs/re/annowelt.md`, and close or narrow the "Where" part of OQ-026.
   - **If it isn't found in the time-box,** write down what was searched, and use step 2's approximation, labelled as
     one.
2. **Draw the sea against the plane.** Where the sea bit allows sea, a sea pixel is drawn where the terrain seen at
   that pixel lies below `Scene::shore_height`, as today. The bit itself must follow the traced rule.
   - **The approximation, only if the trace fails:** a rule derived from the plane, never from the fixed classifier
     19. Then it can't cut sea out of terrain below the plane.
   - **Basins:** say how an inland basin below the plane, not connected to the sea, behaves, and on what evidence.
   - **Both paths:** the CPU reference and the GPU chunk build.
     - **GPU = CPU** over the real-file chunk corpus: 0 pixels differing, or a stated tolerance with the reason.
     - Keep the per-pixel depth: ADR 0007 requires it of every path.
   - **The water field's empty-cell proof** (`iso.md`, "Water field: empty cells") uses the sea bit. Keep it a proof:
     update its conditions and tests. Never turn it into a tolerance.
   - **The comparison switches keep working:** `--shore 16.5` and `--depth off`.
   - **Digests:** re-pin them once, with the reason in the commit message.
3. **Measure against the original.** Recompute W4-DEPTH-1's coverage table: the height bands, the original's covered
   share against ours. Use the same world points from the 2026-11-04 trace
   ([review](../reviews/2026-11-04-w4-depth1.md) §3, `annodisplay.md` §9.3), now with the new rule. Report the table
   before and after.
4. **River mouths.** The higher plane also takes river cells near the sea: a 24-high channel reads as sea.
   - Count, over the river corpus, the river cells or river-water pixels that the sea covers at 26.5 against 16.5.
   - Show one river mouth, before and after.

   The owner wants realistic sea mouths. This is a measurement, not a change to the rivers.
5. **Pictures,** before (`eff68d5e`) and after, the same views, under
   `C:\Users\altin\Pictures\anno-review\SHORE-2\`:
   - the owner's Kontor spot (candidate A, seat 0, `(255,138)`, as in DEPTH-INTEGRATE-1);
   - one wide beach;
   - the river mouth of item 4;
   - per pair, the box and count of changed pixels.
6. **Performance:**
   - chunk build time before and after (the method of `docs/perf/README.md`);
   - one `bench_frame --zoom-sweep` profile of ZOOM-MEASURE-1 (`lan5` legs). Readiness must not get worse.
7. **The playtest card:** say what to look at on a beach.

## Proof

- **Item 1's trace,** or its documented time-box.
- **Items 3–6:** the numbers and the pictures.
- **The gates:**
  - the render and app lanes on MSVC Release with the game files, and in the WSL `-O1` build;
  - the GPU tests on both adapters;
  - hosted CI green.

**Reporting** (from the DEPTH-INTEGRATE-1 review):
- quote each gate's `COUNTS` and `VERDICT` lines as the log prints them, with the log's path;
- rerun a test refused for memory alone through `run_selected.py --jobs 1`, with its own log, and name that log;
- don't commit while a gate runs: the tested-source check then fails the run.

**Not yours:**
- the direct renderer (ADR 0007);
- the zoom fixes (ZOOM-SMOOTH-1);
- river flow changes;
- the full gates (the orchestrator's, at the merge).

## Report

READY FOR REVIEW, about 60 lines:
- the trace or its time-box;
- the rule as built;
- GPU = CPU;
- the coverage table;
- the river mouths;
- the pictures;
- performance;
- the gates and CI;
- NOT_RUN;
- Decisions and reasoning;
- QUESTIONs.
