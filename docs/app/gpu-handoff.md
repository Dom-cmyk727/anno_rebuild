# Viewer and GPU handoff of the picture's generations (plan round dw; implemented round ea)

Status: the first owning-generation integration unit is implemented (round ea). The play window follows the
`ScenePicture`, every frame draws from one owning handle, and the GPU compositor keeps nothing of a generation
past the frame. The tests in section 4 pass on the Mac (Apple M5 Max, Metal). The Windows adapters (RTX
4090, Radeon iGPU), Windows MSVC and the Linux GCC/Clang checks are not run (section 5). This unit does not
claim the PLAN frame targets, late-game scalability or GPU acceptance.

## 1. The contract: six separate boundaries

The picture's state reaches the screen across several boundaries. Each has its own guarantee, and none of
them implies the next.

1. **A committed generation has coherent per-island object lists.** Each island of a generation holds the
   lists of one version of that island's buildings: the desired state after one batch, never a mix of two
   versions in its lists (game-shell.md, "Publication policy").
2. **Per-island committed versions move forward.** A commit never lowers an island's version. Under the
   Monotonic default an intermediate version may be committed, never one above the desired version.
3. **One owning handle protects the scene a frame and its jobs read.** The window takes
   `ScenePicture::scene()` once per frame, after the frame's batches are applied and committed. Chunk planning,
   the CPU or GPU composition and the readiness queries of that frame read only that handle. The chunk jobs
   and the background work the frame starts hold it until they finish. A commit during the frame frees
   nothing they read. This boundary is about memory safety and the inputs of new work, not about what is on
   screen.
4. **Cached chunk pixels can lag, and they update independently.** A commit marks the chunk keys of its
   change stale (`FrameRenderer::invalidate`). The cached chunk of a marked key keeps drawing, unchanged,
   until its rebuild from a new enough generation arrives. Keys are rebuilt and replaced one at a time. So
   during a transition one frame can show a newer chunk of an island beside an older one, and an older chunk
   is not newly rendered just because the frame holds a newer scene. The handle alone does not prevent
   mixed-version cached pixels within an island. Nothing waits: no island-wide staging, no synchronous
   rebuild.
5. **All affected visible chunks fresh is a separate readiness boundary.** An island's change is visibly
   ready when every key that change invalidated within the view is cached and not stale at the same moment
   (`ReadinessTracker`, picture_trace.hpp). A key seen fresh once and made stale again keeps the set waiting.
6. **GPU upload completion and presentation are further boundaries.** A fresh CPU chunk is on the GPU once
   its upload finishes (all planes made and bound; with the viewer's limits this can take several frames),
   and on screen once the frame that draws it is presented. Neither is measured yet (section 5).

Comparisons follow the boundaries:
- During a transition, the GPU is compared with the CPU composition of the same inputs: the same renderer
  and the same cached chunks, held unchanged by a gate.
- After readiness, it is compared with the fresh-scene oracle: a freshly loaded scene of the state the
  generation represents.

## 2. The implemented ownership path (round ea)

- `src/app/src/main.cpp`
  - The window takes the loader's `shared_ptr`. The scene is made by `render::Scene::create`, so its last
    release goes to the reclaimer. There is no borrowed scene and no no-op deleter.
  - In play, the classification of Anlagen ids is copied into a table before the world moves into the
    `PlayRunner`, so nothing on the window side reads the World afterwards.
  - The `ScenePicture` is made from the scene and the tick-0 buildings, sorted, with two builders.
  - `ViewerPicture` connects the runner's building mailbox (`take_building_batches`: immutable batches)
    to the window.
- `src/app/src/viewer.cpp`, frame boundary (`Viewer::follow_picture`, window thread):
  1. Take the batches and apply them.
  2. `commit(frames_)`: it invalidates the change's keys in the frame's renderer.
  3. `current()` gives the frame's handle (`gen`).
  4. `draw_gpu(gen, ...)` calls `GpuFrame::render(frames, gen, ...)`; `draw_cpu(gen, ...)` calls
     `FrameRenderer::render(gen, ...)`.
  The frame step "picture" reports the boundary's time. Pending picture work keeps frames coming, also while
  paused.
- Auxiliary paths hold a handle while they read:
  - the zoom group checks (`group_ready`), bounds and `zoom_min_tile`;
  - the river-flow prefetch (the flow builders hold the generation that asked);
  - the sea-pattern threads at start (each captures the generation it builds from).
  Zoom levels and the scene name are copied once: they are the same in every generation.
- `src/render/src/frame.cpp`
  - `ChunkWorkers(threads)` has no default scene: every job carries the handle it was submitted with.
  - `FrameRenderer::update_chunks` releases its job scene before it returns, so the renderer keeps no
    generation between frames.
  - `ChunkWorkers::set_test_gate` is the ordering seam for tests.
- `src/render/src/gpu.cpp`: the owning `GpuFrame::render` overload.
  - The frame's handle is held only while the frame records, and dropped on every way out.
  - The frame-table pointer (`cb.ft`) is cleared on every way out. Before this round it stayed set
    between frames.
  - The wave simulation's background preparation holds the owning handle in its depth function.
- Retained raw references audited:
  - `cb.ft`: now cleared at the end of every frame.
  - `levels`: the `FrameRenderer`'s, which outlives the compositor's use.
  - Sea uploads: copied in the call; no pattern pointer is kept.
  - Upload holds (`Staged::hold`): `ChunkPtr`s, released to the workers' reaper.
  - The fluid depth function: the finding below.
  The bench tools with one static scene keep the borrowed overloads by contract.

**Finding (fixed).** The wave simulation copies its depth function into a background preparation
(`GpuLimits::background_fluid`, set by the viewer's interactive limits). That function captured the scene by
reference, and the preparation runs past `render()`. Under play generations, a generation reclaimed meanwhile
would have been read after its destruction. It now captures the frame's owning handle. The regression case
fails with the old capture.

**CPU chunk buffers and GPU resources are separate lifetimes.**
- An upload copies the chunk's pixels: `wgpuQueueWriteTexture` copies at the call.
- The textures, bind groups and buffers a submission uses are kept by the device until it completes.
  Releasing a texture only drops a reference.
- Reusing a pooled plane is ordered after earlier submissions on the queue.
So a CPU chunk is not retained for the GPU. The case below frees the replaced CPU chunks while frames are
submitted and never waited for, and the GPU still shows the new pixels. GPU-side memory (chunk planes,
pools, the atlas, offscreen targets) is the compositor's own accounting (`GpuStats`, `offscreen_bytes`). It
is not part of `render::payload_counts()`, which counts CPU payload only.

**Teardown (window, then main).**
1. The window stops calling the bridge.
2. `frames_.set_workers(nullptr)` joins the chunk workers: at most one build per worker is in progress, and
   the generations their jobs held are released there.
3. The sea threads are joined.
4. The GPU finishes (`wait`) and the compositor is destroyed. Fluid preparations release their handle as
   they end.
5. `main` stops the runner (the producer).
6. `main` destroys the bridge: it joins its builders and hands what it holds to its reaper.
7. The generations are destroyed by the reclaimer.
No per-frame wait was added.

## 3. Transitional behaviour as measured

The case "picture GPU: accepted builds and an accepted demolish ..." runs two accepted builds of one island in
one commit, then one accepted demolish, on the five-city workload's map with two cities:

| | Two accepted builds, one commit | One accepted demolish |
|---|---|---|
| Affected visible keys at group 2 | 3 | 1 |
| Transition frame (all rebuilds held) | byte-equal to the GPU output before the edit; equal to the CPU composition of the same inputs within the unchanged picture's parity | same |
| Partial readiness (one key fresh, two stale) | not ready; the fresh key shows its change; the stale ones still draw their old chunks | (one key) |
| Once all keys are fresh | ready; byte-equal to a compositor of its own drawing the freshly loaded scene | same |

Parity of the unchanged picture at these views:
- **Two-build view:** max channel delta 2 at 4 pixels, among 109 per-frame water sprites.
- **Demolish view:** delta 1.

The established GPU bound of 1 ("GPU frames match the CPU path") was measured on views without such sprites.
The 2 is present before any edit and from identical inputs, so this round did not cause it. It is reported
open (section 5).

## 4. Tests (src/render/tests/picture_gpu_test.cpp, suite `picture_gpu`; game files and an adapter)

| Packet item | Case | Shows |
|---|---|---|
| 6a, 6d | accepted builds and an accepted demolish ... | the transitions above; readiness only when all keys are fresh at one poll |
| 6c | a late completion of an older generation ... | a generation-1 build that finishes after the generation-2 invalidation: discarded, the old chunk stays, the mark stays, the GPU never shows the stale building; then fresh |
| 6b, 6e | a frame's generation stays alive across a commit ... | a held chunk job owns generation 0 across the commit (the bridge and the test have let go); destroyed on the reclaimer once the job ends; the replaced CPU chunks are freed while GPU work is outstanding |
| 6f | an offscreen edit is fresh after the view returns ... | keys invalidated while not visible are rebuilt on return; group 2 and derived level 3 equal a compositor of the fresh scene exactly, and the CPU path within the derived levels' bound (max 3) |
| fluid | the wave simulation's background preparation holds ... | the finding above (fails with the old capture) |
| 6g | teardown with builds, chunk jobs and GPU work outstanding ... | every generation is destroyed on the reclaimer; the payload returns to its baseline |

Ordering is made with `ChunkWorkers::set_test_gate` and the bridge's `build_fault` seam, never with sleeps.
The CPU cases of the bridge (scene_picture*, legal_workload) are unchanged and pass.

## 5. Open, and the next units

- **NOT RUN:**
  - the RTX 4090 and Radeon iGPU adapters;
  - Windows MSVC;
  - the Linux GCC 13 / Clang 18 `-Werror` check.
  The commands are in REPORTS.md (round ea). The play GPU case `play_gpu_test` requires an RTX 4090.
- **Open: the GPU-vs-CPU parity of the per-frame water-sprite path.** It is 2 at a few pixels on Metal in a
  view with many water sprites. Earlier views had none of them, so the existing ≤1 bound never covered this
  content. The new cases bound their comparisons by the unchanged picture's parity and report it
  (`WARN`).
- **Not built:**
  - the presentation-latency probe: the time from the sim's batch to the presented pixel, at 1080p and 4K,
    with and without simulation load (PLAN §0.3);
  - a per-frame GPU-against-CPU comparison in the bench;
  - an upload-completion boundary in the trace (boundary 6).
- **Workload limits that apply to every row** (docs/perf/README.md):
  - the legal workload is five cities of about 40 buildings each, not a late-game economy;
  - roads are refused by the current sim;
  - the island's scenery caps the density a city reaches;
  - the original's placement rules are only partly resolved.
- The play options for the picture's limits are not wired. Their defaults are in `PictureLimits`, with
  their reasons.
