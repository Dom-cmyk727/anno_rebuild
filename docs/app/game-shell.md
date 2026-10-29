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
