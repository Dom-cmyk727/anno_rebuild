# Viewer and GPU handoff of the picture's generations (plan round dw; implemented rounds ea and eb)

Status:
- **Round ea:** the play window follows the `ScenePicture`, every frame draws from one owning handle, and the
  GPU compositor keeps nothing of a generation past the frame.
- **Round eb:**
  - a paused window finishes what was published after it went idle, without input (section 2, 1A);
  - the GPU-usable state of a chunk is named and measured (boundary 6, section 6);
  - the interactive window's constrained upload path is tested (section 4).

The tests in section 4 passed on the Mac (Apple M5 Max, Metal) in rounds ea and eb and alone in round ec.
In round ec's full gate the stand-ins case (3D) failed 2 of 2 at its 120 s paused-loop deadline, beside other
test processes. It passed alone and in the GPU gate at `-j 1`; see section 5. Not run: the Windows adapters
(RTX 4090, Radeon iGPU), Windows MSVC and the Linux GCC/Clang checks (section 5). This work does not claim:
- the PLAN frame targets;
- presented-frame latency;
- late-game scalability;
- GPU acceptance;
- original-game visual parity (OQ-525).

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
6. **GPU upload completion and presentation are further boundaries.** A CPU-ready chunk is GPU-pending until
   its upload finishes; with the viewer's limits this can take several frames. It is GPU-usable (round eb,
   `GpuFrame::chunk_state`) once:
   - the compositor's copy has its serial;
   - every plane of it is made and bound;
   - it is not dissolving in over a placeholder.

   Until then the previous copy, the stand-in or the open sea keeps drawing. A rebuilt chunk is swapped whole
   when its upload ends, never plane by plane; only a chunk that replaces a placeholder dissolves. The bench
   records the frame whose submission first draws all of an island's change GPU-usable (`gpu_ready`).

   Round ec measures one more boundary: the **present call's return**. It is recorded in the play window
   (`docs/app/window-trace.md`, `presented`). Since round ed it is the first frame that draws the change
   (its keys within that frame's own view all GPU-usable at that frame's observation: `drawn`) and whose
   present call returned `WGPUStatus_Success`. Round ec took the first successful frame at or after the
   change's `gpu_ready` frame, whatever it drew; a failed present followed by a frame with the view
   elsewhere was reported presented. That rule is withdrawn.

   Two boundaries are still not measured:
   - **GPU completion:** the compositor has no non-blocking completion query, and a blocking wait would
     change what is measured;
   - **the physical display:** a present call returning is not the pixel reaching it, nor scanout.

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

**Paused and hidden windows (round eb, 1A).** The window draws while `app::FrameDemand` (play_follow.hpp)
wants a frame. Paused, it still draws while any work is pending:
- chunk builds are queued or running;
- a zoom is on its way;
- `PlayFollower::has_work()`: a batch is waiting in the mailbox (`PlayRunner::batches_waiting`, a flag set
  by the push and cleared by the take, read without copying), or a change is not committed yet;
- `GpuFrame::work_pending()`: uploads deferred by the limits, or a dissolve.

The runner's batch listener posts an SDL event, so a window waiting for events wakes when a batch arrives.
The listener is cleared before the window goes. Drawing does not run the paused sim. A hidden window draws
nothing; the mailbox keeps its bound (a collapsed resync), and the window takes it when shown.

Before round eb, a batch published by a turn in flight after the paused window's last frame waited for an
input event (review finding, introduced in round ea). Uploads deferred by the limits waited the same way:
pre-existing since the upload limits; the 3B mutant shows it.

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

Round eb added:
- the constrained cases ("picture GPU, constrained: …", Package 3);
- the early-failure teardown case (1B);
- the water reproducer (suite `picture_gpu_water`, OQ-525);
- in `src/app/tests/play_follow_test.cpp` (suite `play_follow`), the paused and hidden window cases (1A).

| Packet item | Case | Shows |
|---|---|---|
| 6a, 6d | accepted builds and an accepted demolish ... | the transitions above; readiness only when all keys are fresh at one poll |
| 6c | a late completion of an older generation ... | a generation-1 build that finishes after the generation-2 invalidation: discarded, the old chunk stays, the mark stays, the GPU never shows the stale building; then fresh |
| 6b, 6e | a frame's generation stays alive across a commit ... | a held chunk job owns generation 0 across the commit (the bridge and the test have let go); destroyed on the reclaimer once the job ends; the replaced CPU chunks are freed while GPU work is outstanding |
| 6f | an offscreen edit is fresh after the view returns ... | keys invalidated while not visible are rebuilt on return; group 2 and derived level 3 equal a compositor of the fresh scene exactly, and the CPU path within the derived levels' bound (max 3) |
| fluid | the wave simulation's background preparation holds ... | the finding above (fails with the old capture) |
| 6g | teardown with builds, chunk jobs and GPU work outstanding ... | every generation is destroyed on the reclaimer; the payload returns to its baseline |
| 1B | a failure after the chunk workers and the builder are held ... | scoped releases, declared after the owners, release both gates before the joins on every exit; a watchdog bounds the case; the payload returns to its baseline |
| 1A | play follow: a turn in flight when the window pauses ...; a hidden window ... | the late batch is shown while paused, the sim stays at its turn, the window idles again; a hidden window converges when shown |
| 3B | constrained: one upload per frame ... | a three-key edit finishes while paused; the window idles only once every key is GPU-usable; exact against a fresh compositor |
| 3C | constrained: an edit published while uploads are outstanding ... | ends on the newest state; every chunk region shown is one whole state; the replaced generation and its CPU chunks are freed |
| 3D | constrained: with stand-ins and dissolves ... | a chunk replacing its stand-in is fading (not usable) until the clock ends it; a rebuilt chunk swaps whole; old and new chunks side by side across keys |
| 3E | constrained: leaving the view ... | uploads in progress are dropped when the keys leave the view and are redone on return; closing with an upload in progress frees its CPU chunk |
| 4 | water parity: the reproducer ... | the four pixels and their cause (OQ-525); parity not accepted (`WARN`) |

Ordering is made with `ChunkWorkers::set_test_gate` and the bridge's `build_fault` seam, never with sleeps.
The CPU cases of the bridge (scene_picture*, legal_workload) are unchanged and pass.

## 5. Open, and the next units

- **Open, intermittent:** `picture GPU, constrained: with stand-ins and dissolves …` (3D) reached its 120 s
  wall-clock deadline in both round-ec full gates. Those ran at `-j 6`, beside CPU-heavy cases and a
  concurrent ASan gate. It passes alone and at `-j 1` in about 50 s. Whether the paused loop is slow or stalls
  under contention is not established.
  - Owner: the next GPU unit.
  - Acceptance: a deadline in frames, or the GPU gate at `-j 1`, passing in a full gate.
- **Round ec's intermittent `publish: one, two and four workers each hold a version ahead at a barrier, and
  the newest is published`** (anno_app_tests; failed once under ASan at workers = 2, "a held generation seen
  destroyed"). Round ed found a test defect that produces exactly that failure, and fixed it:
  - The case's observer recorded only `generation()`. Every scene lineage starts at generation 0, and
    each iteration released its base scene (generation 0) after clearing its observer and after its last
    reclaimer wait, so the shared reclaimer could destroy it after the next iteration had installed its
    own observer.
  - At workers = 2 the only generation checked was 0, and that iteration's generation 0 was the test's own
    `scene`, which it owned throughout. A "generation 0 destroyed" there could not be that scene: it can
    only be another lineage's, and the previous iteration's base is the only other one in the process.
    No identity was logged in round ec, so this is a deduction from ownership, not an observation of
    the historical run.
  - The ownership check itself was vacuous at workers = 2 for the same reason (the test owned the
    generation it checked).
  - Now: destructions are recorded by lineage (`Scene::name`) and generation (`src/app/tests/
    reclaim_identity.hpp`); each held generation is also checked through a `weak_ptr` (still owned, without
    owning it); the test owns none of the picture's generations (it compares against a reference scene of
    its own); the observer's record is shared by value, because the reclaimer calls a copy of the observer
    outside its lock, so a call can still run after the observer is cleared. The sibling case
    `accounting: one, two and four workers each hold a superseded generation at a barrier` had the same
    structure and is changed the same way.
  - The adverse ordering is a deterministic case ("publish: a held generation's destruction is told by
    its lineage …"): the reclaimer is held on a first scene, the earlier lineage's generation 0 queues
    behind it, the next observer is installed, then the reclaimer is released. The generation-only check
    reports the held generation destroyed; the identity check does not; releasing the held generation's
    owner too early is caught.
  - Mutants (`cache/ed-runs/p3-mutants-*.log`): a build that gives up its generation before it runs is
    caught by both barrier cases (expired and destroyed); a generation-only identity is caught by the
    ordering case.
- **NOT RUN:**
  - the RTX 4090 and Radeon iGPU adapters;
  - Windows MSVC;
  - the Linux GCC 13 / Clang 18 `-Werror` check.
  The commands are in REPORTS.md (round ea). The play GPU case `play_gpu_test` requires an RTX 4090.
- **Open, OQ-525: the GPU-vs-CPU parity of water objects below the waterline.**
  - The difference is 2 at four pixels of one water object, with the sea shaded over it.
  - The cause is established: the CPU reference truncates at two blends, `render::over` and
    `sea::lerp4`; the GPU rounds once.
  - Which rounding is the reference is an owner decision.
  - Parity is not accepted, and no tolerance was raised.
- **Built in round ec:** the window trace, from command to present-call return. It records:
  - the sim's acceptance;
  - the batch's publication and take;
  - the commit, CPU readiness and GPU usability;
  - every attempted frame's acquire and present status.

  It is measured at 1080p and 3456 × 1944 on the M5 Max (`docs/perf/README.md`).
- **Not built, or not run:**
  - display latency, scanout, a genuine 2160p drawable, target hosts, and the window under a second
    simulation's load;
  - GPU completion of a frame: no non-blocking completion query in the compositor;
  - a per-frame GPU-against-CPU comparison in the bench (the bench compares the final view, exactly).
- **Workload limits that apply to every row** (docs/perf/README.md):
  - the legal workload is five cities of about 40 buildings each, not a late-game economy;
  - roads are refused by the current sim;
  - the island's scenery caps the density a city reaches;
  - the original's placement rules are only partly resolved.
- The play options for the picture's limits are not wired. Their defaults are in `PictureLimits`, with
  their reasons.
