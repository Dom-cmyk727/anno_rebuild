# Viewer and GPU handoff of the picture's generations (round dw, DW7; plan only)

Status: a plan written from reading the code at `847fe9b` (CPU candidate of round dw). Nothing here is implemented
or enabled. GPU integration, developer input and preview wait for the CPU review and an assignment.

## 1. What the code does today (read, not assumed)

- `src/app/src/viewer.cpp:345` the viewer holds the scene as a non-owning `const render::Scene& scene_`.
- `:360` the viewer owns `render::FrameRenderer frames_`.
- `:575` the viewer sets its chunk workers with `frames_.set_workers(std::make_unique<render::ChunkWorkers>(scene_, ...))`:
  the workers take a raw scene reference (`frame.hpp:173`).
- `:626` the workers are released by `frames_.set_workers(nullptr)`; `:494` the renderer is replaced when the
  river settings change.
- `:1079` the GPU path renders with `g.frame->render(frames_, scene_, view, group_, ...)`; `:1124` the CPU path
  with `frames_.render(scene_, ...)`.
- `src/render/include/anno/render/gpu.hpp:495` `GpuFrame::group_ready(const Scene& scene, group)` takes a raw scene.
- The viewer does not use `ScenePicture` yet: the play picture is not connected to the viewer. This is the work
  that comes after the CPU review.

Owning overloads that already exist: `FrameRenderer::update_chunks(const std::shared_ptr<const Scene>&, Rect,
group)` (`frame.hpp:271`) and `FrameRenderer::render(const std::shared_ptr<const Scene>&, ...)` (`frame.hpp:320`).
The chunk jobs hold their generation (`ChunkJob::scene`, `frame.hpp:156`) and the renderer holds the one it last
planned from (`job_scene_`, `frame.hpp:439`). Final destruction of any generation is the reclaimer's
(`render::SceneReclaimer`, round dw DW3a), whichever of these releases last.

## 2. Where a generation handle must travel

The unit of handoff is one `std::shared_ptr<const render::Scene>` taken from `ScenePicture::scene()` on the window
thread at the start of a frame.

1. Frame start (window): `auto gen = picture.scene();` one handle for the whole frame.
2. Chunk planning: `frames_.update_chunks(gen, view, group)` (the shared overload). Jobs created by the frame keep
   `gen` alive until they finish, so a commit during the frame cannot free what a job reads.
3. CPU composition: `frames_.render(gen, view, ...)`.
4. GPU composition: `GpuFrame::render` and `group_ready` change from `const Scene&` to the shared handle (the
   same signature change as `frame.hpp:320`). The GPU path reads the chunk pixels through `ChunkPtr`, not through
   the generation, so the handle is needed only while the frame plans and records.
5. Frame end: the handle is released by the frame (not stored in the viewer), so a generation lives only as long
   as the work that reads it.
6. Shutdown (`viewer.cpp:626`): the chunk workers are stopped first (`set_workers(nullptr)`), then the picture is
   destroyed. The picture's own queue and results are released by its reaper; the generations are destroyed by the
   reclaimer. Nothing waits on the window thread.
7. Cache retirement: the renderer's cache holds `ChunkPtr` (shared chunk pixels), not generations. A replaced
   chunk stays until its key is replaced or evicted; `invalidate(keys, generation)` marks the keys, and a stale
   completion cannot replace a newer chunk (round du/dv rules, `frame.cpp`).
8. GPU submissions: the GPU reads uploaded textures and bind groups, which copy the chunk pixels. A generation may
   be released after recording, but a `ChunkPtr` still referenced by a submitted command must stay alive until the
   submission completes. This is the one point that needs a test on the GPU, not only a reading: the chunk upload
   sync (`gpu.cpp`, `chunk_sync_ms`) must drop a GPU chunk only when no submitted command uses it.

## 3. Test matrix for the next stage (ordered, with estimates)

Each stage has a narrow regression before the complete workload matrix. Estimates are for the Mac host; the GPU
gates for RTX 4090 and Radeon iGPU run on the Windows PC.

| # | Test | Shows | Seams | Estimate |
|---|---|---|---|---|
| 1 | One replacement, CPU and GPU equality (the same chunk pixels after one commit) | the handle path and chunk replacement | `update_chunks(shared)`, `render(shared)`, `GpuFrame::render` | 1 scene load, < 1 min (game files) |
| 2 | A late stale completion (an old build finishing after a newer invalidation) is not published | stale rule on both paths | `request_chunks`, `fill_cache` (round dv R2) | synthetic, < 10 s |
| 3 | Derived zoom and an offscreen return: a chunk changed at group 2 is correct at groups 3 and 4 after the view returns | derived freshness through the viewer's zoom | `plan_jobs`, `derived_freshness` cases | 1–3 min (game files) |
| 4 | Multi-island bursts: 50 edits over 12 islands, every changed pixel equals the CPU reference | picture to chunks to GPU, under load | `bench_picture` async mode with `--verify 1` | 2–5 min per run |
| 5 | 1080p and 4K presentation latency of an edit (time from the sim's batch to the presented pixel), with and without the simulation load | the PLAN section 0.3 frame targets | viewer frame loop, presentation timing (not yet built) | depends on the viewer; several hours with the matrix |
| 6 | Dense and simulation-contention workloads on the target hosts | the same with 800 buildings per island and real simulation load | `bench_picture`, the headless simulation benchmark | hours; the Windows host |

Existing seams: #1 and #3 can use the existing pixel tests (`river_flow_corpus`, `derived_freshness`,
`scene_picture`) with the handle path added. Missing: a test that holds a generation handle across a commit on the
GPU path; a presentation-latency probe in the viewer; a real simulation-load run in the bench (only the arithmetic
proxy exists). These are listed for the next review, not implemented here.

## 4. What this plan does not decide

- The viewer's frame structure, the presentation timing, and the GPU chunk lifetime rule in section 2.8 are for the
  GPU review; the reading above is the basis, a test is the proof.
- The play options for the picture's limits are not wired. Their defaults are in `PictureLimits` with their reasons.
