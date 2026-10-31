# The game shell (W4.1)

The app can run a scenario. The window shows the sim's world. This note is the
contract for that split. Building changes after tick 0, figures, and input are
later (W4.2–W4.4).

## Threads

Three kinds of thread, and they do not share a mutable world.

- The **window thread** owns the SDL window, input, and the frame. It never
  calls `World::advance` and never waits for the sim. It reads the latest
  published snapshot and the latest scene, and it draws.
- The **sim thread** owns the `World`. It is the only thread that advances it
  or applies commands. After each turn it publishes an immutable snapshot.
- The **chunk workers** already build chunk pixels from a `const render::Scene`.
  They do not see the `World`. They keep that rule.

Pause stops the sim thread from starting another turn. The window keeps
drawing the last snapshot.

## Cadence

A turn is the original multiplayer turn (`anno::sim::Timing` in
`src/sim/include/anno/sim/clock.hpp`): 10 real milliseconds per step, 10 steps
per turn, 100 real milliseconds per turn. The game milliseconds of one step
come from `multiplayer_step_ms`. `--speed 50`, `100` (the default), or `200`
means 5, 10, or 20 game ms per fixed real-time step. The sim thread runs that
turn through `run_turn` (`replay.hpp`), which is the path the net layer will
use.

Wall time decides only **when** a turn starts. It does not change the step
size. The schedule is absolute: turn `k` is due at `t0 + k * mp_turn_real_ms`,
and the thread waits with `sleep_until` (in slices of `mp_step_real_ms`, so
pause and quit are noticed within one step). A late wake does not move later
deadlines, so timer granularity does not accumulate.

If the sim is still late when the next deadline arrives, the due turns run
back to back, up to five (`PlayRunner::set_catchup_turns`, the default). Five
turns is 500 ms at the original cadence: enough to absorb one scheduling
hitch, and short of an unbounded backlog when the sim cannot keep up. Past
that bound the schedule jumps to the first deadline strictly after now, and
the skipped real milliseconds are added to the snapshot's `behind_ms`. Those
turns are not run. Pause stops turns and shifts `t0` by the paused duration,
so the pause is not lateness. The clock and the sleep are a `PlaySchedule`;
the app uses `std::chrono::steady_clock`, and tests pass their own.

## What the snapshot is

The sim thread allocates each immutable snapshot before taking the publication
mutex, then swaps its `shared_ptr` under that mutex. The window takes the same
mutex only long enough to copy the pointer and releases it before reading or
drawing. The lock therefore never covers a turn or snapshot allocation. The
first snapshot carries at least:

- the tick;
- the game time;
- the cost of the last turn, and the p95 of turn costs over the last second
  of turns;
- `behind_ms`, the real time the schedule skipped after the catch-up bound.

The window title and the frame log show those three. The snapshot is not a
`World`. The render library never receives a `World`, mutable or otherwise,
and it gains no sim types. The bridge that copies sim buildings into render
objects lives in the app.

## Static and dynamic

Terrain and scenery come from the island files. Buildings come from the
`World`.

- Islands are the ones the sim placed: an embedded `INSEL5` island as the
  file has it, and for each link the template `resolve_scenario_links` chose,
  at the link's position. The viewer's stand-in (island number mod count,
  `loader.cpp`, OQ-028) is not used for `--play`.
- The scene's building objects are the world's buildings: type, x, y,
  rotation, and `Building::byte0f` as the height byte. The sim does not keep
  a per-object height for scenery (`walk_cells.cpp`). Scenery height stays
  the byte in the file. An Anlagen id above `0x0FFF`, or an island number
  above `0xFFFF`, does not fit a PROD2 item: the scene records a note and
  does not mask the value into the record.
- Terrain and scenery are the file's objects with the building items removed.
  The split uses the same type classification the scenario loader uses.
- At tick 0, an object the sim did not place is listed and explained. It is
  not copied back from the file to fill the gap.

`--view` is unchanged. It still builds a scene once from the file and the
stand-in rule.

## Why the scene re-encodes PROD2

The brief offered a render builder that takes object lists (Anlage id,
rotation, height, island-local position) and places sprites without writing
file records. This shell writes the world's buildings back into the island's
`PROD2` records (`apply_world_buildings`), and `render::Scene` is constructed
from those records the same way `--view` is.

Tick 0 is one scene, built before the first turn. The existing scene path
already draws `PROD2` items, and the play-versus-view checks compare that
path with itself. A second builder would have to reproduce block layout,
draw order, and the height byte, and the chunk cache would still be filled
from the same sprites. Keeping the records avoids that second path for a
picture that does not change during the run.

Two alternatives were set aside. Passing the `World` into `render` would
give the renderer sim types, which this contract forbids. An object-list
builder in `render` is the brief's shape and remains the better seam once
the picture changes after load; it is not required to draw the tick-0 scene.

What that leaves for the later steps:

- W4.2 draws figures per frame. Ships and soldiers are already
  `Scene::units`, drawn after the island chunks (the GPU compositor draws
  the ships itself). Updating that list each frame leaves the `PROD2`
  records alone, so figures do not depend on this choice.
- W4.3 has to get building changes into the chunk cache. `FrameRenderer`
  keeps chunks in `cache_` until a new chunk is stored under the same key;
  nothing invalidates a chunk when an island's objects change. Re-encoded
  `PROD2` means a building change rewrites the record and then needs that
  invalidation. An object-list builder would need the same invalidation,
  fed by a list diff instead of a record rewrite. Tick 0 never changes a
  building after the scene exists, so this gap is not exercised here.

## `--play`

`anno1503 --play <file.sze> [--speed 50|100|200]` builds the type registry with
`load_original_registry`, loads the file with `load_scenario_with_links`, and
creates the `World`. The scene is built from the islands that load placed.
The sim thread then runs turns as above.

## Building changes after tick 0 (W4.3)

Status: specification, written in round dq before the code. Numbers marked
"target" are checked by measurement (W4.3 tests A6 and the perf notes) and
change this text if they miss.

### What the sim publishes: an app-side diff, not a sim log

The sim keeps no change log of buildings, and this spec adds none: a new
counter or log in `World` would be state that the hash may see, and the
brief forbids a state or hash change. The sim thread therefore diffs the
buildings it owns before and after each turn, and publishes the difference.

- Per island slot, the sim thread keeps the building list it published last
  (ordered by the `World`'s building id). After the turn it walks the
  `World`'s current buildings of that slot and emits one delta per building
  added, removed, or changed in type, tile, rotation, or height byte. A
  changed building is one removed delta and one added delta.
- Each delta carries: the island slot, the kind (added, removed), the type,
  the tile, the rotation, the height byte, and the screen box the building
  occupied when the picture last drew it (for a removed delta, the box it had,
  so its old chunks are invalidated too).
- The turn's changes are one immutable `BuildingBatch` (turn index and deltas),
  built on the sim thread after the turn, before the publication mutex. A turn
  that changed nothing publishes no batch: no allocation, no copy.
- Delivery is a mailbox, not the snapshot. The snapshot is latest-only, so a
  batch carried in it would be lost when two turns pass between two frames. The
  runner keeps every batch published since the window last took them
  (`PlayRunner::take_building_batches`, swapped out under the publication mutex,
  O(1)); the window applies them in turn order. The snapshot stays the three
  figures and the title, as before.
- Why a diff and not a version counter: a per-island version needs a field in
  `World` (a state change, refused). Why not the applied commands: the command
  list does not cover what a turn changes on its own (buildings that finish or
  disappear in the sim's steps). The diff is the only complete source that
  needs no sim change.
- Cost, target: O(buildings in the world) comparisons per turn. A 5-player map
  carries on the order of 10³ buildings; the target is at most 0.2 ms p95 per
  turn for the diff on `06_1024_heavy_256.sze`, measured before and after
  publishing (A3). If it misses, the diff keeps a per-island hash of the last
  list and skips unchanged islands.

### How the picture learns which chunks are stale

The window thread takes every batch in the mailbox once per frame and applies
their deltas in turn order.

- For each delta and each zoom group, the screen box is the footprint diamond
  of the building grown by its sprite's extent at that group (the frame's
  anchor and its width and height, the numbers the draw list uses), so tall
  sprites and overhangs are included. A building can reach a neighbouring
  chunk; every chunk whose rectangle meets the box is stale.
- Stale keys go to `FrameRenderer` as an invalidation set. A stale key stays in
  `cache_` and is queued for a rebuild in the same priority list as a
  river-flow rebuild (`chunk_rebuilds`, `rebuilds_`, `frame.cpp:773`). When the
  worker's chunk arrives it replaces the cached one under the same key (the
  precedent at `frame.cpp:768-775`), so the old chunk draws until the new one
  is in.
- The GPU compositor replaces the uploaded planes of a key when its chunk is
  replaced. Whether `gpu.cpp` already refreshes a key on a replacement is
  checked in A4; if it does not, that path is added there and recorded.
- Tile changes other than buildings: `TileCleanup` events go to
  `seams_.tile_cleanup` (`events.cpp:29`). Play mode installs no handler, so
  today such an event changes nothing. A future handler that changes tiles
  must add a tile delta kind; it is not a silent edit. Recorded as a gap, not
  solved here.

### The immutable object list a chunk is built from

A chunk worker reads the scene while the window keeps running, so a worker
never sees an object list that changes under it. The rule is copy-on-change:

- The dynamic objects of an island (the PROD2 records of its buildings, or
  the object list the A4 choice builds) are held in a shared immutable list per
  island. A change batch builds a new list for each island it touches and swaps
  the pointer on the window thread. A chunk job holds the list it started from;
  if a delta arrives while it runs, its key is stale again and it is rebuilt
  against the new list.
- Static terrain and scenery stay in the scene built once from the file (the
  split above). Only the dynamic part is swapped.
- The cost is O(objects in that island) per batch, once per touched island, not
  per delta. A batch of 50 builds in one turn is one list build per island.

### Latency bound

Let turn N apply a change on the sim thread, and let snapshot N be published
after it.

- The first frame that takes snapshot N applies its deltas before drawing.
- The stale chunks are queued; each chunk build runs on a worker in the
  background (about a millisecond of CPU per chunk at group 0, the figure in
  `docs/perf/README.md`).
- The new chunk replaces the old one at the first frame after its build has
  finished and been uploaded.

Target **K = 3 frames**: one frame to see the snapshot and invalidate, one for
the worker, one for the upload (the GPU path). Measured in A7 as the number of
frames from the snapshot's publication to the first frame that draws the
change, over the 50-build burst. A measured K above 3 changes this text first.

### While a rebuild is in flight

The old chunk keeps drawing, always. Invalidation never removes a key from
`cache_`, and the window never waits for a worker or a chunk. A second change
to a key whose rebuild is pending keeps the key stale; the rebuild that
replaces the chunk reads the newest list, so one rebuild covers both changes.

### The command file (`--commands`)

`anno1503 --play <file.sze> --commands <path>`. One command per line; `#`
starts a comment; blank lines are ignored. A turn index counts from 0, the first
turn the runner applies. Turns must not decrease. A line that does not parse,
or names a turn already past, is an error with its line number, and the run
stops before turn 0.

```
turn <n> build <type> <x> <y> <rotation> [<x> <y> <rotation>]...
turn <n> demolish <x> <y> [<x> <y>]...
```

- `<type>` is the registry's `TypeId` value (a number); `<x> <y>` is the tile of
  the placement (`PlacementWire::x`, `y`); `<rotation>` is 0..3.
- One `build` line is one `BuildCmd` for the developer player's city on the
  island under its first placement. The file does not name a city: the runner
  resolves the `CityRef` from the world (`developer_player()`, player slot 1).
  The extra triples are more placements in stream order, in the same command.
- One `demolish` line is one `DemolishCmd` over the listed tiles, for the
  developer player's city on the island under its first tile.
- A line with no such city (no city of the developer player on that island) is
  refused at load time with its line number. No city id is invented, so the
  sim never sees a command the file made up.
- A `build` with more than 4 095 placements or a `demolish` with more than
  8 190 tiles has no wire form (`has_wire_form`, `command.hpp`); the runner
  refuses that line at load time with a message. It is not split silently; the
  window's drag splits a longer path (B3).
- The runner applies a turn's commands in file order, then runs the turn's
  steps (`Turn::commands` is applied first by `run_turn`).

The window's commands (W4.4) and the file's commands use the same queue, so
both reach a turn the same way.

### Immutable per-island object lists (W4.3, round du)

Status: implemented in the Mac continuation of round du (`render::Scene::with_islands`,
`render::FrameRenderer::invalidate`, `app::ScenePicture`; tests `scene_picture_test.cpp`).
The specification was written before the code. The measurements are in
`docs/perf/README.md` ("W4.3 round du"). Still open: the chunk rebuild and upload latency and
the GPU compositor (step 10 of round du), which need the GPU and the frame loop. The whole-scene generation measured and
rejected earlier is recorded there too (6.8 s from files, 0.47 s for the
constructor with reused parts, 0.25 s for the first sea pattern). Nothing here
changes the simulation: the picture consumes published batches.

**1. Static and dynamic.** Shared by every generation of a scene: the Anlagen
file, the ground textures, the sprite cache, the frame table, the zoom levels,
the placement layouts, the sea patterns, each island's ground grids, and, for an
island whose river pieces did not change, its water (`IslandWater`) and its river
flow. Per island and per generation, immutable: the object lists `below`,
`below_enhanced`, `above`, `above_enhanced`, `other`, `falls_below`,
`falls_above`, `river_sprites` and `rivers`. They come from the island's
`GELAENDE`, `PROD2` and `FLUESSE` records (`kObjectRecords`) through
`prepare_objects`, which returns plain values and no pointer into a file buffer.

**2. Painter order and overlap.** A building and a scenery object stay in one
draw-ordered list, as now. A change re-runs `prepare_objects` over the island's
complete record set, so each object, static or dynamic, comes out in the order a
fresh scene gives. No list is merged by hand, and no object moves to another
layer. Occlusion inside a chunk is unchanged, because the chunk replays the same
list.

**3. The classification boundary.** Every object, a building included, is
classified by one rule: its Anlagen gfx prefix (water kind) and its height against
the waterline (`prepare_objects`). The river-related Anlagen ids are river pieces
`2701, 2705, 2706, 2707`, falls `2702, 2703, 2704` and surf `2708` (`scene.hpp`,
`iso.hpp`). A building with one of those ids changes its island's rivers, so the
derived island rebuilds its water and flow instead of sharing them. Correctness
does not rest on the boundary: a share is taken only when the river list is equal.
The registry's supported building types are checked against those ids by a test
that needs the game files.

**4. Generations.** `Scene::with_islands(...)` returns a new immutable scene
(`std::shared_ptr<const Scene>`, generation `g + 1`). It shares the static parts
of the scene it derives from and carries new object lists for the islands it
replaces. A generation is never changed once another thread can see it.

**5. Off the window thread.** A touched island's object list is built on a worker
from an immutable input: the island's record set after the batch, and the
generation's Anlagen file, kept alive by the request. The window thread never runs
`prepare_objects` and never waits for a worker.

**6. Commit rule.** Each island has one request sequence number. The builder keeps
the newest request per island; a finished result whose sequence is not the newest
for its island is dropped. The window thread commits the current results of one
frame in a single derived generation.

**7. Retained-generation bound (superseded).** Round du stated `1 + threads` generations and
`(1 + threads)` times the touched islands' lists. That formula counted generations only; it missed the
jobs, results, flow requests and reaper items that hold them. The enforced contract is "Retained work and
reclamation" below (round dw, DW4f). Final destruction of a generation is the reclaimer's, whichever thread
released it last.

**8. Stale completions.** Each chunk records the generation it was built from. An
invalidation names the generation `m` that caused it and marks each affected key
with `m`. A completed chunk built from generation `g` replaces the cached one and
clears the mark only when `g >= m`. A chunk with `g < m` is discarded and the mark
stays, so the next request rebuilds the key from the current generation. A key
without a mark is never touched by this rule, so unchanged keys are reused.

**9. Invalidation.** A changed building marks every key that its old screen box and
its new one intersect, at every zoom level of the scene, derived levels included: a
derived chunk is reduced from its children, so a child's change reaches its parent.
The box is the footprint diamond of the building, grown by the largest sprite
extent of its frames at that level (anchor, width and height, shadows included),
read from the same frame data the draw list uses. A cached key stays in the cache
and keeps drawing until its replacement arrives (the river-flow precedent). Marks go
on keys whether or not they are cached, so a chunk in flight is covered too.

**10. Latency.** K frames from the frame that first takes the snapshot to the first
frame that draws the change. At 60 Hz K = 3 is 50 ms; at 120 Hz it is 25 ms. The
object-list build and the chunk rebuild add to it. The targets are measured in
Release and Debug, and each miss is reported.

**11. Rejected commands.** A refused command publishes no batch, so no key is
marked and no pixel changes.

**12. Mailbox and publication.** Only the sim thread pushes and only the window
thread takes. Between the fullness check in `PlayRunner::publish` and the push, the
window can empty the mailbox but cannot fill it, so the mailbox never becomes full
after the check. The prepared copy is then unused in that interleaving; it is
destroyed under the publication lock, which the design accepts and the report
states. The allocation is outside the lock; the destruction of an unused copy is
inside it, and bounded by the list size.

### Retained work and reclamation (W4.3, rounds dv and dw)

**State.** The bridge keeps three versions per island. `desired[i]` is the version of the newest change
to island `i`; every `apply` advances it, even when the build is deferred. `committed[i]` is the version
the committed generation has. `dirty` is the set of islands with `desired != committed`, and
`pending()` is its size: zero means converged, and a failed or deferred change is never idle. A finished
result is committed only when its version is still the desired one; an older one is dropped and the island
is built again from the current desired state. The desired state is the building list (`buildings()`); no
build command is replayed to repair the picture.

**Ownership.** A scene generation owns the bytes its islands and Anlagen.dat view (`render::Scene::Backing`),
shared by every generation derived from it. Its final destruction belongs to `render::SceneReclaimer` (one
process thread): every generation is made by `Scene::create` or derived by `with_islands`, and its deleter
hands the destruction over when the last owner of any kind releases it (the window, a frame, a chunk worker,
a flow builder or a reaper). That release only enqueues. The reclaimer counts queued, waiting, in-flight
and completed destructions and records the thread of the last one.

**Limits** (`PictureLimits`; each default has its reason; the play options are the configuration path, not
wired yet):

| Limit | Default | Reason | At the limit |
|---|---:|---|---|
| `max_outstanding` | 8 builds | Each live build holds a copy of its island's records, and its result a copy of the lists and the change set. A build counts from its start until its object is destroyed (superseded and reaped builds included). | A start beyond it is deferred (kept in order) and started by a later commit. |
| `max_outstanding_bytes` | 64 MiB (provisional) | The estimated island-record bytes of the builds in flight or finished. Provisional until DW5 measures the five-player island. | A start beyond it is deferred. A single build over the budget still starts alone, so no island is blocked for good. |
| `max_reclaim_backlog` | 4 items | A stalled release must not let work pile up. It counts the reaper's queue and the generations the process reclaimer has not destroyed (`SceneReclaimer::stats().waiting`). | A commit is skipped (returns 0) and retried by the next call. While over the bound, an island in flight is deferred rather than replaced. |
| `max_attempts` | 4 | A transient failure recovers; a persistent one must end. | After the attempts the island is failed for good: `failed_islands`, `error()`, and it stays owed (`pending()` counts it). |

Retries wait 1, 2, 4 ... commit calls (the window's frames), not timers. A change to a failed island
resets its attempts. Test seams (`build_fault`, `reclaim_gate`) are for the test suite.

**Credits and byte accounting (round dx, DX2).** Every build holds a credit: one slot and the estimated
bytes of its island's records. The credit moves with the build's object and is released when that object is
destroyed, never earlier. `outstanding` and `outstanding_bytes` are the sums over the live credits, so a
superseded build still counts while it runs, and a dropped job counts until the reaper destroys it. The
estimate is the records' payload bytes at the start of the build; it excludes the lists and the change set.
The resident shared catalogue (sprites, textures, Anlagen) is counted once, outside these bounds. Generation
bytes are reported by the reclaimer (`bytes_waiting`), not by the bridge.

| Object | Alive in | Credit released when |
|---|---|---|
| Queued job | the window's `jobs` queue | the job is started (its credit moves to the job the worker runs) |
| Running job | the worker's local job | the build returns; a failed build's credit goes with its job at the end of the worker call |
| Finished result | `done` | committed (the credit goes at the end of the commit call), or dropped |
| Superseded running build | the worker's local | the worker destroys its result and job, then counts itself done (`wait_idle` returns after this) |
| Dropped job or result | the reaper's queue | the reaper destroys it (the credit is released with it) |
| Committed result | the new generation's storage | the commit call returns; the scene holds the bytes, which the reclaimer accounts |

A start is admitted by the live credits: a build beyond `max_outstanding`, or beyond `max_outstanding_bytes`
while another build is live, is deferred. A single build over the byte budget still starts alone. The
buildings of a start are copied from the island's own entry of the per-island index, after the admission
(`copied_buildings`); the index is rebuilt for the touched islands when the list changes.

**Reclamation counters.** `reclaim_queued` and `reclaim_completed` count the bridge's own retired items
(generations as references, jobs, results); a generation's destruction is counted by the reclaimer.

**Change set.** A worker computes the objects that appear and disappear and their chunk keys, against the
island's lists when its build started. A commit uses that set only if the island's lists are still the same
object (`Scene::island_lists_handle`); otherwise the result is requeued (`version_requeues`). The window
thread never computes a change set.

**Idle commits.** A commit with nothing to start does not pass over the building list: the cost of an idle
commit does not grow with the number of buildings.

**Failure.** A build that throws is counted (`build_failures`). The committed generation is kept; the island
stays dirty and is retried from the desired state. The message of a failure for good is `error()`.

**Bound on the reaper backlog.** A commit adds at most one generation, one base per rebased job and one list
per result, so the backlog peaks at `max_reclaim_backlog + 1 + 2 * max_outstanding` (asserted by the
stalled-reaper case).

**Tests.** Synthetic (`scene_picture_recovery`): one-shot failure recovered with no new batch; persistent
failure stops and is reported; a newer change during a retry; shutdown with a retry waiting; an older
result never published after a newer change under a stalled reaper; sustained changes over 12 islands
converge; the byte budget admits one over-budget island; 1, 2 and 4 workers converge. The oracle is a fresh
build of each island from the desired state, with the loader's per-island function. Reclamation
(`scene_reclaim`): the destruction runs on the reclaimer for the window's, a chunk worker's and the
renderer's release, and the counters agree.
