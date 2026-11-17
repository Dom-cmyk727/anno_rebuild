# Review: RENDER-DIRECT-1b (the resident atlas pages on the CPU)

Reviewer: the orchestrator, 2026-11-17. Branch `task/RENDER-DIRECT-1b`, head `c7a1e77b` (the code is at
`88cf59c5`; `c7a1e77b` adds the report). Packet: `docs/orchestration/tasks/render-direct-1b.md`.

**Verdict: CHANGES REQUESTED.**

What holds:
- The unit's design: one packer (`AtlasResidency`), slot sizes rounded to even before packing, so every slot
  origin is even under the shelf packer and each slot's mip region depends only on its own texels.
- The rounding rules: `(c*a + 127) / 255` is round to nearest for every input, because 255 is odd and no
  product lands on a half. The box rule is `(s + 2) / 4`.
- Thread independence is proven by a hash test.
- The budget default is a pure, tested function (discrete 512, otherwise 384, explicit value wins, layer clamp).
- The `play_gpu_test.cpp` message fix is its own commit.
- The dangling-buffer bugs were found and fixed in the new test code.

## F1: every timing in step 5 comes from a Debug build (blocking)

The report says "16 threads, WSL/dev build". The `dev` preset is `CMAKE_BUILD_TYPE` Debug
(`CMakePresets.json:10`). `docs/perf/README.md` measures on Release builds. These numbers are therefore Debug
figures, not costs of the shipped code:
- the 27.45 s worst build;
- the 12.8 s for `lan5.txt`;
- the "about 85 s" app-layer load;
- the collect-versus-pack finding (88-94 % collect).

For comparison, the MSVC Release viewer this morning loaded `lan5.txt` and drew its first frame about 7 s
after the process started (`zoom-session-2026-11-17/owner-run`, process start 08:23:09, first drawn frame
08:23:16, including window creation and the first chunk builds).

**Fix:** rerun `bench_frame --atlas-build` from the native `msvc` Release build on this PC, once the
orchestrator releases it, on the same 20-scene subset plus `single38.sze` and `lan5.txt`. Replace the table.
Restate the collect/pack finding from the Release figures, whatever they show. Keep the byte columns:
- they do not depend on the build type;
- they must match the Debug run exactly, which is a free consistency check.

## F2: the texel proof deviates from the packet without asking (blocking)

Step 4 asks that every frame's rectangle equal the decoder's output, on three scenes including
`data/maps/lan5.txt`. The branch does neither:

**a. It samples every 7th frame** (`atlas_pages_corpus_test.cpp`, the `i += 7` loop) instead of checking every
frame. The synthetic cases are exhaustive, but they are small and do not stand in for real frames: the
packet's point is that no real frame of any kind differs.

**b. It excludes `lan5.txt`** on a proportionality call that rests on the Debug load time (F1). `lan5.txt` is
the map RENDER-DIRECT-1c draws first, and the owner plays on it.

**Fix:**
- check every frame, and add `lan5.txt` to the game-file case (it is already in the `heavy` suite);
- report the case's wall time in the msvc Release build and in the dev build;
- if the dev-build time is a problem for the Mac lane, say so with the number, and the orchestrator decides.

A deviation from a packet's named scope goes to the orchestrator before it is built, not into the report
afterwards.

## F3: duplicate frame ids race (correctness)

`build_atlas_pages` keys `images` by `FrameId`, but it pushes one `FrameRef` per input. `AtlasResidency::ensure`
removes duplicates, but the `work` loop then calls `place(r.id)` once per ref, so a duplicated id gets two work
items with the same slot. If they fall in different chunks, two threads write the same bytes concurrently.
That is a data race, even though the values agree, and `placements` also gets a duplicate entry. The callers
deduplicate today; the function's contract does not require it.

**Fix:** skip an id already seen (first wins), or return it in a named list like `oversized`. Add a
synthetic test with a duplicated id that checks one placement and the same page hash at 1 and N threads.

## F4: the mip rectangle is one texel short for odd frame sizes (correctness)

`AtlasFramePlacement`'s comment defines the mip rectangle as the padding/2 inset of the slot's mip region. For
an odd width `w` with padding 2:
- the slot is `w + 5` wide, its mip region `(w + 5) / 2`;
- inset by 1 on each side, that leaves `(w + 1) / 2` = ceil(w/2);
- the code stores `img.width / 2`, which is floor(w/2).

Heights follow the same rule. `mip_x = place.x / 2 + padding / 2` is also only right for an even padding.

**Fix:**
- origin: `(slot_origin + padding) / 2`;
- end: ceil of `(slot_origin + padding + w) / 2`;
- size: end minus origin;
- a synthetic assertion on the odd-size frames of the existing case.

## F5: confirm the integrated budget with real pages (in the Release rerun)

The owner set 384 MiB for a non-discrete adapter from 1a's estimate of the worst adjacent pair (340 MiB). The
branch measured only all three groups together, which is the 4090's case (360 MiB, 152 MiB headroom).

**Fix:** in the F1 rerun, also report the worst adjacent pair (0+1 and 1+2) of static+water bytes per scene,
built from real pages. If any pair exceeds 384 MiB, that is a finding for the owner, not something to fix here.

## Gates still open

- **The RTX 4090 gate:** `anno_render_gpu_tests` from the msvc build with `ANNO_GPU_ADAPTER=RTX`. The log must
  name "NVIDIA GeForce RTX 4090"; a fallback adapter is no result. It runs in the same PC slot as F1.
- **CI** must be green on the final head.

No builds, tests or measurements on this PC until the orchestrator releases it (the owner's recording).
Code edits for F2-F4 can start now.

## Fix round and acceptance (added 2026-11-17)

`95a5ace6`, after two merges of `origin/main` (UNIX-WARN-1, then LAYER-CHECK-1):
- **F1:** the bench was rerun on the msvc Release build, alone on a quiet PC. All 200 byte fields equal the
  Debug run. Wall times are 19-24 times shorter; `endless02.sze` takes 1.30 s. `collect` still takes 85.3-92.4 %
  of static wall time (median 90.1 %). That finding is queued as its own packet; RENDER-DIRECT-1c does not fix
  it.
- **F2:** every frame and every texel is checked: 50,755,526 assertions on the two `.sze` scenes in
  `anno_render_tests`. `lan5.txt` is checked in `anno_app_tests` (39,965,981 assertions), so
  `anno_render_tests` links nothing above render. The checker is shared through the header-only
  `anno/test/atlas_pages_check.hpp`. A missing named file fails instead of skipping, which fixes the preflight
  failure of run 44038688654.
- **F3:** `duplicate_ids`; the first occurrence wins. A synthetic test checks one placement and equal hashes at
  1 and 8 threads.
- **F4:** the mip rectangle is computed from the slot origin with floor and ceil. Asserted on an odd-sized
  frame; no earlier test read those fields.
- **F5:** the worst adjacent pair built from real pages is 0+1 = 340.0 MiB (`endless02.sze`), 44 MiB under
  384 MiB; 1+2 is 100.0 MiB. No finding for the owner.

**The RTX 4090 gate,** msvc build with `ANNO_GPU_ADAPTER=RTX`, the log naming the 4090: 119 of 119 cases. The
"Validation Error" lines in that log also appear in `main`'s own earlier GPU gate logs; they are not new.

**One correction to the report:** the peak memory of 1,569.6 MiB (Release) against 967 MiB (Debug) compares
two platforms, not two build types. The Debug run was WSL/Linux and the Release run Windows/MSVC.

**CI green on `95a5ace6`** (run 44051159572). **ACCEPTED.** It lands under the owner's standing rule.
RENDER-DIRECT-1c is released on it.
