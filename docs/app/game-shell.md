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

Status: the specification for the seam approved in round du (board round du,
`cache/orch-du`). Written before the code. The numbers are in
`docs/perf/README.md` ("W4.3 round du"). The whole-scene generation measured and
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

**7. Retained-generation bound.** A generation is alive while it is current or a
chunk job holds it. `ChunkWorkers::submit` replaces the queue, so queued jobs share
one generation, and the running jobs (at most one per thread) hold at most `threads`
others. Alive generations are at most `1 + threads`. The memory they hold beyond the
current one is the object lists of the islands they replaced: at most `(1 + threads)`
times the sum of the touched islands' lists. Pending object builds are at most one
per touched island (step 6). A retired generation is released on a background
thread: freeing its lists is not free, and the window thread must not do it.

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
