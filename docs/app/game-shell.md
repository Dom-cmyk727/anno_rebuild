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
  refuses that line at load time with a message. It is not split, neither here nor by the
  window's drag: a drag over the limit is refused before it is sent (round ee).
- The runner applies a turn's commands in file order, then runs the turn's
  steps (`Turn::commands` is applied first by `run_turn`).

The window's commands (W4.4) and the file's commands use the same queue, so
both reach a turn the same way.

### Command outcomes (W4.4, round ee)

A command is identified from its submission to its outcome (`command_outcome.hpp`):

- **Receipt.** `PlayRunner::post_command` returns `CommandId{Window, n}`, n from 1 in submission
  order (assigned under the queue's lock). The script's commands are `{Script, 1..}` in their run
  order: `set_script` sorts them by turn, keeping file order within a turn, so a line for an
  earlier turn after a later one still runs (before round ee it was skipped). A receipt is app-side
  bookkeeping: never sent to the sim, never hashed, not a network protocol.
- **Execution.** A turn runs the script's commands, then the window's in push order, through
  `sim::run_turn`, whose loop now also returns one `ApplyResult` per command (the same loop; the
  world is unchanged by asking). The runner records, per command, the turn, its position in the
  turn, the command's status and each record's status. Nothing is correlated by content:
  identical commands get their own receipts and outcomes.
- **States.** `Queued` (issued, not run), `Applied` (the sim accepted it; a drag can be partly
  built and a demolish can find nothing on a tile, which `records` says per record), `Refused`
  (the sim's reason), `Lost` (run, but evicted from the bounded store before it was read) and
  `Unknown` (never issued). A command with no wire form is `Refused` as `Unencodable`: the sim's
  rule skips it, it is never applied, and it is not in `applied_commands()`.
- **Bound.** `OutcomeLog` keeps at most 4 096 outcomes and 65 536 per-record entries
  (`set_outcome_bounds`; about 1 MiB at most); the newest is always kept. `outcomes_since(seq)`
  returns the kept outcomes in run order with the exact range a slow reader lost; `outcome(id)`
  says `Lost` for an evicted one. A reader never has to infer acceptance from a missing refusal.
- **Wake.** The runner's wake listener (`set_wake_listener`) fires after every turn that pushed a
  building batch or ran a command, so a refusal or an accepted no-op wakes a window that waits
  for events. `PlaySnapshot::outcomes_next` is the outcome sequence after that turn.
- **Only the turn's commands:** a refusal the sim reports for a command applied during a step
  (an AI seam's, `scheduler.cpp`) belongs to no receipt; the runner drains it with the other host
  events. PlayRunner installs no AI seam today.
- **Not acceptance:** a receipt, the queue, and a picture update. A building can be accepted and
  still drawn later (the picture is asynchronous).

Tests: `src/app/tests/command_outcome_test.cpp` (suite `command_outcome`), and
`run_turn reports one result per command…` in `src/sim/tests/replay_test.cpp`.

### The developer building tool (W4.4, round ee)

A test tool in the play window (the owner's round-dr decision), not UI.2. Off by default.

- **Keys:**
  - B turns it on and off.
  - With it on: N and P cycle the buildable types, T rotates, X toggles demolish.
  - Esc cancels a press or the demolish mode; with nothing to cancel, Esc quits as before.
  - The left button presses and releases: a click places, a drag lays a road (a straight line
    along the longer axis) or demolishes a line of tiles.
  - The right button pans (the left button still pans with the tool off); the keys and the
    wheel are unchanged.
  - Losing focus, leaving the window or being hidden drops a press. Motion only moves the hover,
    never a command. A repeat of the tool's keys (B; with the tool on, N, P, T, X and Esc) does
    nothing and stays the tool's: a held Esc never reaches the window's quit.
- **The pick** (`dev_tool.hpp`, `render/picking.hpp`). The tool picks on the frame the window last
  drew, never on the camera's target:
  - SDL's window points are multiplied by the pixel density to get drawable pixels.
  - A drawable pixel maps to that frame's scene pixels: x0 + (px + 0.5) / scale, from the view's
    origin and, under continuous zoom, the Resample's offset and its scale (`PointerFrame`).
  - `render::pick_tile` then finds the world tile on the frame's own scene, with heights:
    the frontmost point where the view ray meets the ground. An earlier iteration from height 0
    picked the ground behind a raised tile.
  - Only the developer city's island is accepted. Outside the window, open sea, another island,
    a non-finite point and a point beyond ±2^30 tiles give no command and a message.
  - Commands are in world tiles, as the command file's are.
- **Data, not constants.** `dev_tool_setup` reads, at load, before the runner takes the world:
  - the registry;
  - the buildable types (`buildable_types`: list-backed and road types, in TypeId order; see
    "The tool's data and robustness");
  - the developer player's first city and its island's tile rectangle.

  The window never reads the World.
- **One adapter.** The window calls `dev_tool_step` (`sdl_input.hpp`) for every event. The tests
  feed synthetic SDL events through the same function (`src/app/tests/dev_tool_test.cpp`,
  `anno_app_input_tests`).
- **Long drags.** A drag over one record's payload bytes (65 535 tiles) is refused before its
  path is made. `sim::has_wire_form` decides the rest (4 095 placements, 8 190 tiles), and a drag
  is never split: one drag is one command.

### The tool's preview and feedback (W4.4, round ee)

- **The preview** (`dev_feedback.hpp`, `Scene::preview_sprites`):
  - It is the selected type's own sprites: its ANLAGEN entry, the selected rotation, the
    hovered tile, and the ground height under the origin tile's corner (the sim's own height for
    the building can differ: a command-built building has height byte 0 today, OQ-550). They
    come from the renderer's own `place_prepared` on the frame's scene. Checked against the
    scene's own drawing of 18 822 placed objects of Single_01 (frame and anchor, every rotation)
    and, on the GPU, against the CPU blend (Metal: 0 pixels apart by more than 1).
  - The GPU compositor draws them after everything else of the frame (an `overlay` span of
    `GpuFrame::render`); the CPU renderer blends them after its frame (`blend_preview`). So they
    lie on top of the picture: a preview, not part of the painter's order.
  - No preview at a derived zoom level (sprites exist at the built groups), in the demolish mode,
    or without a hovered tile on the city's island. When a type has no sprite to show, the line
    says why (`preview_note`): an id the picture cannot draw (`kPictureAnlageIds`: a PROD2 item
    holds 12 bits; such a building is built by the sim but not drawn), no Anlagen entry for the
    type, a derived level, or the city's island not in the scene.
  - Moving the pointer changes no object list and starts no chunk rebuild: only the overlay
    changes.
- **What it knows** (`PlacementKnowledge`), from published data only:
  - the city's island tiles;
  - the tiles the picture's published buildings cover on the city's island, kept by the deltas of
    the batches the window takes (round ee review, R4: `PlayFollower::set_batch_observer`,
    `attach_occupancy`, `PlacementKnowledge::on_batches`): a delta of another island is one
    comparison and nothing else, a delta of the city's island adds or removes its tiles
    (reference counted, so overlapping buildings stay), a resync batch reseeds from its list
    (O(the list), as the picture's own resync is), and a disabled tool does no work and is
    reseeded from the picture's per-island list (`ScenePicture::island_buildings`, O(the city))
    when it is on again (`ensure`). Never a scan per pointer move or per frame. What remains on
    the window thread for an edit on another island is the picture's own `apply` (a sorted insert
    into its building list and the per-island index, about 1 ms at 50 000 buildings (Release, Windows; 0.8 to 1.6 ms over three runs) in
    `dev_occupancy_test.cpp`'s reference row), which is not occupancy work;
  - the latest snapshot's money, goods and `costs_waived` (the free-build flag or rule) against
    the type's gold and goods costs.

  A refusal is shown only with one of those as its basis ("would be refused: occupied in the
  published picture", "… gold N needed, M held (published turn T)"). Everything else is
  unchecked, and the text names what: the sim's placement rule always, occupancy when no index
  could be built, costs without a snapshot, goods outside the published stock. The sim
  revalidates every command when it runs it; the published picture can be behind it. The sim's
  placement rule is still the stand-in of OQ-052 (inside the island, owned tiles, no overlap):
  it does not check terrain, so a building on open water inside the island's rectangle is
  applied and drawn there (lan5's developer island is a 256 x 256 rectangle that is mostly
  sea). The line says so instead of promising a terrain check.
- **The feedback line** (the window title, `feedback_line`), from left to right:
  - the tool's state: the type, its footprint and gold cost, the rotation, the hovered tile and
    the last local hint;
  - the money and the type's cost goods from the snapshot (held / needed);
  - the preview's knowledge;
  - the outcome of the tool's latest command, looked up by its receipt: Queued, Applied (with how
    many of its records did something), Refused with the sim's reason, or Lost.

  An earlier command's outcome is never shown as the latest's: while the tool is on, the title is
  `dev_title` of one snapshot (the money once), the runner's last refusal names its command
  ("refused: Occupied (window command #2)"), and a click that sent nothing clears the receipt
  shown (the tool's own reason is in the line).
- **Resources:** the developer start's test money and goods (`FixedPlayOptions`, fixed maps such
  as `lan5.txt` only) are the sim's own state, charged and refused as any other. An original
  scenario keeps its own resources.

### The tool's data and robustness (W4.4, round ee, package D)

- **The buildable list** is registry data (`buildable_types`): every type whose PRODTYPE has a
  list in the sim's layout, and every road type, in TypeId order and of any id width (TypeId is
  32 bit in the sim and on the wire). Roads are tile objects the sim does not build yet: their
  command is refused as Unsupported, which the feedback shows. A modded layout changes the list.
- **A second definition** (another id, a non-square footprint, other costs, no sprite) needs no
  code: its commands carry its id and rotation, its rotated footprint drives the island and
  occupancy checks, its costs the cost check and the line (`dev_tool_mod_test.cpp`).
- **Empty and malformed data:** with no buildable type the tool only demolishes; a listed id the
  registry lacks sends nothing and names the id; a zero footprint is refused by the registry
  builder; a footprint larger than the island is a known refusal.
- **A footprint is tested as a rectangle before any tile is made** (round ee review, R3): the
  check widens the rotated rectangle to 64 bits (`DevInput::footprint_extent`) and refuses one
  that leaves the city's island, or has no area, in O(1); what passes lies inside the island, and
  occupancy is asked with one ordered lookup per column. A registry definition of any width (a
  1,000,000 x 1 or a 4 294 967 295 square) therefore costs no list and no overflow; there is no
  building-size cap, the island is the region. The indexing of published buildings clamps each
  rectangle to the island in 64 bits. `footprint_at` (a tile list) takes the size of the region
  the caller admits and returns nothing beyond it. `alloc_test.cpp` refuses any single allocation
  above 64 KiB in the window under test, so the old enumeration fails there instead of using the
  host's memory.
- **Pointer moves rebuild nothing:** not the occupancy index (100 000 moves, one rebuild), not the
  scene or the picture (the tool has no access to either). An unpublished command changes no
  published version.
- **Per-move work** (pick, `DevTool::handle`, `make_preview`, `preview_note`, `check`,
  `feedback_line`; Apple M5 Max, debug build, `dev_tool_mod_test.cpp`): flat in the number of
  published buildings (4 to 100 004), and nearly flat in the scene's islands since the pick asks
  only the islands its ray crosses (`render::pick_ray`). One build, before and after the change
  (`cache/ee-runs/d6-pick-before-after.log`): 15.2 to 17.3 µs at one island (the crossed-island
  list), 212 to 18.8 µs at 64 islands. In the round's dev gate (G2, under load) 20.0 µs at one island
  and 21.6 µs at 64. The index rebuild, once per published change, is 0.28 ms at 100 004
  buildings. This is not large-city, complex-economy or multiplayer performance, which stays open.

### A scripted session and a picture that lags (W4.4, round ee, E3 and E4)

- **Not drawn yet** (`drawn_note`, E4; corrected in round ee review R2): the line says where an
  applied command's change is on its way to the screen (`ChangeVisibility`, `visibility.hpp`),
  from the same boundaries the window trace records and with or without a trace. In order: the
  command's turn is in the picture's desired state (`NotInPicture`, `PlayFollower::followed_turn`
  semantics: the snapshot read before the take, since a turn's batch is in the mailbox before its
  snapshot); the island's object list is committed (`Committing`); a frame attempt has looked at
  the change (`AwaitingFrame`); every chunk of the change in the view is fresh on the CPU
  (`ChunksStale`) and usable on the GPU, not dissolving (`NotOnGpu`); and a frame that draws all of
  them returned success from its present call (`NotPresented`, then `Visible`). `KeyObservation`
  (`observe_keys`) is the one definition of "drawn", shared with `FrameTracer`. A change none of
  whose chunks was in the first attempt's view after its commit is `Offscreen` (in the picture,
  not on screen, never waited for); one that was in view and left it is `OutOfView` and waits; a
  commit that invalidated no chunk is `NothingToDraw`; a turn that published no building change
  (a refused or no-op command) is `NoVisualChange`. Present-call success is not display time.
  The state is bounded: one part per commit of an island, judged and shown on its own and settled in
  version order (at most 64 per island; beyond that the two oldest merge, which only makes the wait
  stricter), at most 1 024 takes not yet shown (the two oldest merge beyond that), 1 024
  settled ones kept (an older command reads `Unknown`), at most 1 024 turns named per take (the worst case of the unshown takes is about 8 MB of turn numbers, the settled history as much again; a hidden window takes nothing, so the bound is far above a session's use). The
  sim's outcome stands either way. The old rule (followed turn and no pending island) is
  object-list completion only: with the chunk workers held it said drawn while the old chunks
  still drew (`dev_visibility_test.cpp` shows both answers on lan5 with held workers).
  `dev_barrier_test.cpp` holds every picture build at a gate: the runner's turns, the outcomes, the
  tool and the follow step go on (the slowest follow step was 0.25 ms in the dev gate, 1.2 ms under
  ASan), the line says "not drawn yet", and the open gate commits it but does not call it drawn.
- **One consistent read of the outcome and the resources** (round ee review R1): the runner
  records a turn's outcomes before it swaps in the snapshot that follows them, so the live log can
  be ahead of the snapshot for a moment. `PlayRunner::feedback` reads the snapshot first and
  answers the outcome against its `outcomes_next` watermark (`OutcomeLog::find_published`): a newer
  outcome, or an evicted one that cannot be told from an unpublished one, is withheld as Queued
  with `deferred` set, and the next read shows it with the money and goods of its own turn.
  The title and the script both read through it; `PlayRunner::outcome` is the raw log. Nothing
  waits for the sim: two short locks, one after the other.
- **A scripted session in the live window** (`dev_script.hpp`, E3): `ANNO_DEV_SCRIPT=<file>`
  pushes the script's key, mouse and wheel events into SDL's own queue, so they reach the
  window's handler as a person's input does; while it runs the window takes no other input (the
  events carry their own device id, `kScriptDevice`). It waits on frames, turns, outcomes and the
  picture (`wait drawn` ends only when an applied command's change is settled: presented, or
  classified Offscreen, NothingToDraw or NoVisualChange; `expect visibility NAME` names the stage),
  checks the title and the latest outcome, and can capture the window's own rectangle
  (macOS; the capture runs `screencapture` on the window thread, a 0.2 to 0.3 s stall per shot that
  belongs to the test harness, not to the play path). A script that never sees a drawn frame, or a wait past 30 s, fails instead of hanging.
  Window events that are not input (focus, leaving the window) still reach the tool: they can
  only drop a press, so they can fail a session but never make one pass.
  `ANNO_DEV_SESSION=<new dir>` writes the applied commands (`--commands` format), the state hash
  after each turn that ran commands and every 25th turn, every kept outcome, the invocation (map,
  speed: `session.txt`) and the script's log (`dev_session.hpp`). `--play <map> --speed <s>
  --commands <file> --replay-hashes <file>` replays them without a window at that speed and
  compares the hashes and, with the session's `outcomes.txt` next to the hash file, every
  outcome's turn, state, reason and record results (`replay_session`; tests in
  `dev_script_test.cpp`: a moved command fails on a hash, a refused command of another type on
  its outcome). A window command without a wire form (refused as Unencodable before the sim) is in
  `outcomes.txt` but not in `commands.txt`, so such a session's replay reports a difference, never a
  false agreement; only a drag over 4 095 tiles makes one. Recording hashes costs a full `state_hash` on those turns (about 0.17 s on lan5 in
  a debug build, visible in the turn p95).

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
| `max_outstanding_bytes` | 64 MiB (provisional) | The reservations of the builds holding a credit: the estimated incoming size before a build runs, its measured result after. Provisional until the legal-city builds are measured (docs/perf). | A start beyond it is deferred while another credit is held. A single build over the budget still starts alone, and nothing is admitted beside it. |
| `max_reclaim_backlog` | 4 items | A stalled release must not let work pile up. It counts the reaper's queue and the generations the process reclaimer has not destroyed (`SceneReclaimer::stats().waiting`). | A commit is skipped (returns 0) and retried by the next call. While over the bound, an island in flight is deferred rather than replaced. |
| `max_attempts` | 4 | A transient failure recovers; a persistent one must end. | After the attempts the island is failed for good: `failed_islands`, `error()`, and it stays owed (`pending()` counts it). |

Retries wait 1, 2, 4 ... commit calls (the window's frames), not timers. A change to a failed island
resets its attempts. Test seams (`build_fault`, `reclaim_gate`, `payload_destroy_hook`; the reclaimer's `set_test_gate` and
`set_test_observer`) are for the test suite.

**Four quantities (round dy, A3/A4).** They are different measures and none stands in for another:

1. **Admission reservations** (`ScenePicture::Stats::outstanding`, `outstanding_bytes`,
   `outstanding_bytes_peak`): the bridge's credits, one per build that is queued, running, superseded and
   still running, finished and uncommitted, or dropped and not yet destroyed. A build is reserved for its
   estimated incoming size before it runs (`build_estimate` in `scene_picture.cpp`: the copy of its
   desired buildings; the copy of the old records plus each rewritten block, with one 6-byte PROD2 item and
   one 6-byte block header per desired building; the new lists; the change set; the two sorted object
   copies the change set is taken from) and, once built, for the measured size of its result (record
   descriptors, storage, lists and change set; `result_bytes`). A result that measures more than its
   estimate is counted (`estimate_shortfalls`, `estimate_shortfall_max`). The estimate is provisional.
2. **Accounted payload** (`render::payload_counts()`, process-wide, by `PayloadKind`): every lists object
   and every record storage counted once, by its own allocation, from the moment it is complete until its
   destruction has finished. `Backing` is a scene family's file bytes, `Baseline` the lists of the scene
   loaded from files, `Incremental` the lists and storage built for changes, `Generation` each generation's
   own metadata (`Scene::own_bytes`: its island and record descriptors, which a derivation copies for every
   island; its island slots; its layouts). Shared storage is counted once, never per generation that
   references it (`Scene::referenced_payload_bytes` gives what one generation references).
3. **Reclamation queue** (`SceneReclaimer::Stats`): the generations waiting and the one being destroyed,
   with their own metadata bytes (`bytes_waiting`, `bytes_in_flight`). A generation leaves the waiting
   count when it is dequeued and stays in the in-flight count until its destruction has ended. The payload
   it frees as the last owner leaves the accounted payload when it is freed.
4. **Process residency** (RSS, physical footprint, paging): measured outside the code (the bench's
   samples, `tools/test/sample_memory.sh`), and the only measure that includes allocator overhead, the
   sprite and frame caches, chunk pixels and the GPU's mirror.

Sizes are capacities times element sizes plus vector headers; allocator overhead and control blocks are
not counted. Every count is released after the object it counts has been destroyed: a credit is the
first base of the job or result it accounts for (`Accounted`, `credit.hpp`), a payload token the first
member of its allocation's holder, and a generation's token its first member. Move assignment of a job
or result destroys the replaced payload before the replaced credit.

| Object | Owned by | Dominant allocations | Counted by | Released when |
|---|---|---|---|---|
| Backing (file bytes) | the scene family's assets, shared by every generation | the loaded `.scp`, `Anlagen.dat` buffers | `Backing`, once | the last generation of the family is destroyed |
| Textures, Anlagen, sprite and frame caches, sea patterns, grids, water and flow slots | the family's assets and caches | decoded sprites, frame tables, sea pixels | not counted (RSS only) | the last generation of the family is destroyed |
| Baseline lists (generation 0) | each generation that shares them | `Prepared` vectors per island | `Baseline`, once | the last generation referencing an island's baseline lists is destroyed |
| Desired state and per-island index | the window (`ScenePicture::picture_`, `by_island_`) | `SceneBuilding` vectors (twice: list and index) | not counted (O(buildings), `sizeof(SceneBuilding)` each) | the bridge is destroyed |
| Queued job | the window's job queue | the copied desired buildings of its island | its credit (estimate) | the job is started, or dropped and destroyed by the reaper |
| Running job | its worker | the copied buildings, the copy of the old records, rewritten blocks, the new lists, the change set and its sorted copies | its credit (estimate) | the job's payload is released on the worker, then the credit moves to the result |
| Superseded running build | its worker | as a running job | its credit | the worker destroys its result and job, then counts itself done (`wait_idle` returns after) |
| Finished result | `done` | record descriptors, storage, lists, change set | its credit (measured) and, for storage and lists, `Incremental` | committed (payload handed to the next generation, then the credit is released), or dropped and destroyed by the reaper |
| Current generation | the bridge (`current_`) and any frame, chunk job or flow builder holding it | its own metadata; it shares storage and lists | `Generation`; its payload by the allocations | its last owner releases it, then the reclaimer destroys it |
| Retired generation | frames, chunk jobs, flow builders, running jobs that still hold it | as a current one | `Generation` | as above; while waiting or being destroyed it is in the reclamation queue |
| Chunk pixels | the renderer's cache and chunk jobs (`ChunkPtr`) | up to 4.3 MB per land chunk | not counted here: bounded by chunk counts (visible, `spare_chunks`, the margin, `resident_level_chunks`) | evicted by `trim`, freed on the workers' reaper |
| Bridge reaper queue | the bridge's reaper | dropped jobs, results, list handles, generation references | the items' own credits and tokens; `reclaim_queued`/`reclaim_completed` count items | the reaper destroys each item |

**Bounds that are enforced.** Admission: at most `max_outstanding` credits, and at most
`max_outstanding_bytes` of reservations while more than one credit is held (one build over the budget starts
alone and nothing is admitted beside it). Reclamation: no commit and no replacement while the reaper holds
more than `max_reclaim_backlog` items or the process reclaimer holds more than `max_reclaim_backlog` waiting
generations. Retired payload is not bounded by its own byte limit: a retired generation frees what it alone
still references (the old payload of the islands the next commit replaced), and the number of retired
generations waiting is bounded as above; the bench reports retired payload as accounted payload minus what
the current generation references.

**Reclamation counters.** `reclaim_queued` and `reclaim_completed` count the bridge's own retired items
(generations as references, jobs, results); a generation's destruction is counted by the reclaimer.

**Publication policy (round dz, orchestrator direction).** The picture advances during sustained edits
instead of waiting for a build of the exact latest version. `PictureLimits::publish` selects the policy:

- `PublishPolicy::Monotonic` (the default): a finished build of island `i` at version `v` is committed when
  `committed[i] < v <= desired[i]`. A build superseded while it ran, or a finished result a newer change
  arrived for, is kept while it is newer than the committed version; a newer change does not drop it.
- `PublishPolicy::LatestOnly` (a diagnostic policy, the round-dw contract): only `v == desired[i]` with the
  island's open build at `v` is committed; a superseded build is dropped. A stream of changes to one island
  faster than one build plus the next commit then holds that island's picture until the stream stops (round
  dy, D1 case f: object-ready p50 544 ms, maximum 1040 ms at 400 edits per second; docs/perf/README.md).

Invariants of both policies (the tests are in `src/app/tests/scene_picture_publish_test.cpp`, suite
`scene_picture_publish`, for the default; the LatestOnly tests select that policy explicitly):

1. Per island the committed version only increases: a result whose version is not above the committed one is
   dropped, whenever it finishes (a late older build after a newer commit included).
2. A published version never exceeds the desired version.
3. An intermediate publication leaves the island pending: it stays dirty until its desired version is
   committed, and its newer build stays open (or is started by the next dispatch).
4. Once the changes stop and the workers can run, the picture converges to the desired state.
5. Admission is unchanged: every kept build holds its credit and its byte reservation until its object is
   destroyed or committed, and the reclamation limits gate the commits as before.
6. Stale chunk completions stay rejected: a commit invalidates its keys with its own generation, and a chunk
   built from an older generation never clears a newer mark (rule 8 above).
7. The simulation is not involved: the policy decides which finished CPU build the window publishes. Sim
   state, command validation, costs, placement, the economy and lockstep are unchanged.

What an intermediate publication holds: the island's buildings as the desired state was at that version
(the sim's state after the batch that made it). The generation's object lists of one island are never a
mixture of two versions. The cached chunk pixels a frame shows are a separate matter: they lag the
generation and update key by key (see "Consumer boundaries" below). Islands are
independent: one commit may hold island `i` at an intermediate version and island `j` at its desired one. The
real-data case "legal workload: intermediate publications under a stream of builds and a demolish ..."
compares each intermediate generation with a freshly loaded scene of the desired list at its version (PROD2
items of every island, and the chunk layers of the edited views, rendered from the generation alone and as
the bridge's own invalidated chunk cache shows them).

A result built from lists the generation no longer has (a commit of the same island happened during its
build) is requeued, never published: its change set is against lists that are gone. If it is the island's
open build, the island waits in the deferred list and the next dispatch builds it again from its current
desired state; if a newer build is open, that build continues and the requeued result is only dropped. The
cost under Monotonic: a build that runs while the island's previous version is committed is requeued, so
with one worker and a sustained stream of changes to one island every second build commits (versions 1, 3,
5, 7 of 8 in the synthetic stream case; the others are requeued). That is still progress during the stream,
where LatestOnly commits nothing; the measured cost is in docs/perf/README.md. A newer job that is still
queued when an intermediate version of its island is committed moves to the new generation with the other
queued jobs and is built from it, with no requeue.

Credits at a commit (round dz finding). The commit releases the committed results' credits before it starts
the deferred builds. Before round dz the credits were released only when the commit returned, so the
commit's own dispatch could not use them. A waiting island was then admitted only by the next `apply`,
where the islands that the call changes are considered first: under one admission, a continuously edited island kept a
waiting island out for the whole stream (the case "publish: competing islands under one admission ..."
failed with island 1 committed 0 times in 12 rounds). Now the waiting island is admitted by the commit, in
the order it waited.

**Consumer boundaries (round ea; docs/app/gpu-handoff.md).** The window follows the picture at each frame
start, on its own thread: take the mailbox's batches, apply them, commit, then take one owning handle of the
current generation for the whole frame. The guarantees are separate:
- a committed generation's per-island object lists are coherent;
- per-island committed versions move forward;
- the frame's handle keeps what the frame and the work it starts read alive;
- cached chunk pixels lag and are replaced key by key, so a frame can show an island's newer chunk beside
  an older one while the rebuilds arrive, and nothing waits for them;
- "visibly ready" is reached only when every affected visible key is fresh at the same moment;
- GPU upload completion and presentation are later boundaries.
A frame holding a newer generation does not make its cached chunks newer.

**Trace.** `PictureLimits::trace` reports every step of a build's life (desired, admitted, deferred, started,
built, failed, superseded, dropped, requeued, committed) with the island and the version, on the thread where
it happens; the bench records it with the generations, the visible chunk readiness and the reclaimer's
destructions (`picture_trace.hpp`).

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
