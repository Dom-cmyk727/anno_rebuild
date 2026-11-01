# Ship navigation: the first sailing slice (contract, MAP-NAV-1)

Status: **contract for review; nothing is implemented.** Evidence: `docs/re/ship-navigation.md` sections 6-12.
Labels: **ORIGINAL** (evidenced rule, port faithfully), **ADAPTATION** (proposed, needs the orchestrator's decision
where marked), **UNKNOWN** (open, listed in section 11). The simulation stays headless and integer; render
interpolation is a consumer.

## 1. Goal and the first slice

A player's ship receives a move order, sails to a legal approach and founds through the existing `FoundCmd`
(`docs/sim/ship-founding.md`). The slice is complete when: a `MoveCmd` is validated and either refused with a reason
and **no state change** or accepted; the ship plans, turns and moves on the integer stepper at the figure-registry
cadence; a second order or a stop replaces the first; the ship stops within the goal acceptance; and an ordinary
`FoundCmd` then succeeds from the sailed position. An always-rejecting placeholder is not a slice. Out of scope:
trade routes, combat, AI strategy, the avoid routine for hostile owners (deferred, section 9), wakes, fog rendering,
diplomacy commands.

## 2. Command and refusal contract

`MoveCmd` (native wire tag `0xF071`, after `FoundCmd`; **ADAPTATION**: the original's player command is net `0x84`
(kind 1 = point, flags from the cursor) and its host/script command `0x6c`; neither wire format is reused): `{FigureId ship; i32 target_x_q8, target_y_q8}` where the target is a Q24.8 tile coordinate (the original sends
raw f32 world `x`, `z`; the click rounding is UNKNOWN, section 11 item 1, so the contract takes an exact fixed-point
point and the UI layer decides how to produce it). Issuer = the authenticated player. `0xF071` joins the codec's tag order and `is_native_wire_type`; a `LegacyRawCmd` of that type
becomes unrepresentable (as `0xF070` did).

Validation, in order. **ADAPTATION (atomic refusal):** in the original single-player path the stop (slot `+0xdc`) runs *before*
SetOrder, so a refused order still trims the in-flight segment; the sim refuses first and leaves bytes, PRNG cursor, the motion
state and every set unchanged (tested like `FoundCmd`, the unchanged assertion includes the new motion state):

| # | Check | Source |
|---|---|---|
| 1 | the figure exists | ORIGINAL (`0x1005d380` null check) |
| 2 | the figure's owner is the issuer | the original checks ownership in the selection layer (`0x1001a5d0`) against the *claimed* player (the player id travels in the packet and is not authenticated against the connection, evidence 8); the net `0x6c` handler has none. **ADAPTATION:** the sim checks against the authenticated issuer |
| 3 | the figure kind has a `navigation` type fact (a validated registry fact, section 4), i.e. is a sailing ship | ORIGINAL in spirit (the selection requires class group 6 or 7 and the world picks the engine by type group); expressed as a registry fact (ADAPTATION: data, not group numbers) |
| 4 | `state_word & rules.move_blocked_mask == 0`, default `0x200e` | ORIGINAL (`0x100a5f70`) |
| 5 | the target is inside the world (`0 <= x < world_w`, same for y) | ADAPTATION (original: none, the path engine decides later) |
| 6 | the ship is not a crew child (no parent) | ADAPTATION (embarked units never take orders) |

Route failure is not a command refusal in the original (the plan fails at pick-up, the order bytes are cleared and the
ship idles). **Decision proposed for the orchestrator:** keep that behaviour (accepted command, failed plan, idle)
because planning is deferred to the pick-up point; a plan failure emits a `RouteFailed` entry in the output log so
callers can observe it. This keeps the command a pure state write.

Accepted command: stop semantics first (ORIGINAL slot `+0xdc` then `+0xe4`): clear the order and the node list, set the
plan end to the current position, trim the in-flight segment to the remainder past whole lattice steps (modulo one tile,
modulo the diagonal step length for an odd facing), then record order id 1, phase 0, target `(x, y)`.

## 3. Pick-up, planning and state machine

```
IDLE --order recorded--> (current segment ends, node list empty, order pending)
   -> PLAN: route query (section 5) -> failed: clear order, IDLE (+RouteFailed)
                                     -> status 2 (no runs): order consumed, IDLE
                                     -> plan: node list = turn/move segments, pos NOT moved
   -> SEGMENT-START: d>=3 TURN phases (facing +-1 immediately, f0/0.01 units each), then MOVE chunks <= 1 tile
   -> MOVING <-> SEGMENT-END (collision hook = density only here) -> next node ... -> list empty -> IDLE (arrived)
```
Pick-up latency (ORIGINAL): the order is picked up only when a segment ends and no node remains; an idle ship has a
1.0-tile idle segment at speed 0.01 (500 ticks), so latency up to one second plus the trimmed remainder is original
behaviour and is kept (faithful default). A rules switch `ship_pickup_immediate` (default off, QoL, outside the faithful
core) may plan at command time later; it is not part of this slice.

The route is computed from the **start tile centre** (`ftol(x) + 0.5`), but the plan commit writes **no position**: the nodes
are velocity and length (relative), the first segment is installed with `remaining 0` and the facing set directly to the
first node's direction (no turn phase for the first heading). So the ship keeps its sub-tile offset and moves by the lattice
displacement; whether it ends within the goal tile centre or offset by that fraction is **UNKNOWN** and not asserted
(capture CAP-4 reads the position at plan time and after the first segment). The snap at `0x1003fb9a` is the local replan only
(deferred). The first heading bypasses the turn rule (ORIGINAL, `0x1003f910`) and is tested.

## 4. Type facts, state and numeric representation (integer only)

**Type facts** (validated at registry seal like the hull domain; all optional, absence = not a navigating type):
`speed` (HEAD f1, tiles per stepper unit), `turn_step` (HEAD f0), `nav_class` (LOGIC byte 0x10, 0..4, selects the
cost table), `max_hp`, `hold_cap`, `hold_count_a`, `hold_count_b` (the speed-factor inputs), `avoid_radius`
(the registry-query radius of the hook, type slot `+0x68(4)` = `LOGIC[0x25]/8` with 0 meaning 7.0: 16.0 tiles for a 1502,
`docs/re/ship-navigation.md` 7.4; unused until the deferred hook is ported). Further type facts from the same slots:
`stamp_radius` (slot `+0x68(5)` = `LOGIC[0x28]/8`, 0 meaning 2.0; 5.0 tiles for a 1502), used by the route query's obstacle stamps. Conversion of a file f32 to the engine's exact value: the f32's exact binary
rational, stored as a Q0.32 integer rounded to nearest (even) (**ADAPTATION**, error below 2^-33 relative to a
speed of 0.036; documented, tested against the f32 value). Type ids, player ids and the 16-direction count are data
or config, never constants in `sim`; the alternate-type test uses a different ship type and a non-default player.

**Per-ship motion state** (new persisted state, section 8): `segment {vx, vy (Q0.32 signed), length, speed, kind}`,
`remaining`, node list, order record `{id, phase, flag, flags, target}`, `factor_percent` (V), `chunk_factor`, `last`
(u16), position accumulator **`pos_fine` in 2^-32 tile (i64 per axis)**, facing `dir16`. **Seam fit (checked against `scheduler.cpp`):** the existing figure-tick seam dispatches the whole registry visit and writes only
its own figure and its `Effects`; the ship mover must therefore be installed as a **dispatch by figure kind** (navigating kinds go
to the mover, all others to the existing body, so carts and carriers are unaffected), and crew slaving and the density read need
a named `Effects` operation and a read-only registry view. `last` is the ship's own stepper stamp (`+0x42`) and is **separate**
from `Figure::stamp` (the unit's `+8`, which the registry visit uses): P1 adds it as motion state. `Figure::pos` (Q24.8) is
the published position = `pos_fine >> 24`, recomputed after every step (derived, so it cannot drift from the
accumulator). **Widths (MSVC has no `__int128`):** speed is a u32 Q0.32 below 2^28 for any real speed (HEAD f1 <= 0.05 tile per unit); one
visit consumes `ticks <= 65535`; `distance_x5 = ticks * speed_q32 < 2^44` fits i64. A diagonal axis component multiplies by
`11863283` (< 2^24) and would reach 2^68, so it is formed with a portable 64 x 64 -> 128 helper (two u64 words, no compiler
intrinsic assumed) and the position accumulator keeps the sub-2^-32 part as an exact residue (a `u32` over 2^24) so no rounding
occurs inside a segment; the only roundings are (i) the Q32 conversion of each type fact, (ii) one floor division when leftover
time crosses a speed change (error below 2^-32 tile per boundary) and (iii) the floor to Q24.8 at publication. The original's
`0.2f` is not exactly 1/5; the contract uses exactly 1/5 (part of the exact-rational adaptation). `tools/ship_nav_ref.py`
`integrate` vectors pin the exact-rational integrator (hand-derived: 50-tick visits at speed 0.04 move 0.4 tile; leftover carry
between segments; no overshoot); P3 adds the C++ check and a numeric drift bound against the 2^-32 implementation (bound to be
shown, not assumed: below 1e-6 tile over a 1000-tile voyage). Time is carried as integer *ticks scaled by 5* (the original converts `u16(now-last) * 0.2` stepper
units): `distance_x5 = ticks * speed_q32`, compared with `remaining_x5`, so no division is needed in the common
case. The final partial step of a segment uses `time = remaining / speed` (extended in the original); the leftover
time is carried as an exact integer Q32 fraction of a tick, floored at 2^-32 tick (**ADAPTATION**, bounded: below
one part in 2^32 of a tick, orders of magnitude finer than any original observation). Velocity components use the
exact rationals of the original f32 constants: axis 1, diagonal `0x3F3504F3 = 11863283 / 2^24` per axis and step
length `0x3FB504F3 = 11863283 / 2^23` (1.4142135). Q24.8 velocity per tick is rejected (it is 7.6 % to 84 % off,
`docs/re/ship-navigation.md` 7.10); the 2^-32 accumulator keeps the arrival within one visit of the float original.
At the boundaries where the original's own float behaviour is visit-cadence dependent (end offset up to +0.0178 tile
at one-tick visits), the contract follows the **exact-rational** result, not the original's f32 noise; this is an
explicit adaptation, quantified in the evidence note, and the capture card (CAP-4) can bound it on the real game.

## 5. Route query (the engine of `docs/re/ship-navigation.md` section 9)

Pure function `plan_route(static_coast, units, start_tile, goal_point, nav_class) -> {status, runs}`; integer
throughout (costs are u16, tile classes integer, scoring `2304 - ftol(64 d^2)` computed in integers on the
Q24.8 goal point with the tile centre +0.5): coarse map as in 9.2-9.3, fine search as in 9.4, learned masks (<= 8),
goal acceptance radius 6.0 (config `route_goal_radius`, default 6), run length cap 10 (config), FIFO tie order and
neighbour order exactly as evidenced. **Static coast data** is a **world-level** sector index, because a route spans open sea and several islands (the original's
`[world+0x6aa4]`: 64 x 64-tile sectors, each 8 x 8 coarse cells, a null sector = open water `0xfff`): derived state assembled from
the island rectangles in island-slot order (deterministic), rebuilt for a changed rectangle when a placed or removed object has
the original's class bits (`& 0x7000` equal to `0x2000` or `0x4000`, which becomes a data flag on the building type with the
original classes as default), never hashed, rebuilt on load (the original builds it per island at placement before the first tick, evidence 9.2). **Local obstacles** enter only as the ship-density
stamps (5 x 5 kernel, cost classes by density) from other ships near the start/goal, queried by tile from the figure
registry. The route engine's own query is evidenced (evidence 9.4): radius `stamp_radius` around the start, inclusive `d^2 <= r^2`, units with
state `0xa` clear and ship group, scan order = 16 x 16-tile cells by z then x, cap `route_unit_cap` = 0x40, each stamped at
`pos + heading * 25.0` with the 5 x 5 kernel (a stamp is only a record; the tile is the truncated point; density is evaluated per candidate tile, **no clamping**, and affects only the
goal-candidate choice, evidence 9.4; density vectors in `movement_vectors.txt`). The original's in-cell order is registry insertion
order (oldest first, re-appended only when a unit changes cell), which depends on movement history. **ADAPTATION:** the sim has no
16 x 16-tile cell lists, so it reproduces the scan order as (z-cell, x-cell, ascending figure id) computed from positions; the order
only matters when more than 64 ships are in range and is part of the rules identity. Never use wall-clock state. The nearby-figure hull veto of founding is unrelated and unchanged. The first implementation port covers the
**global** engine; the 12-unit local detour replan (section 10 of the evidence) is **deferred**: the slice accepts
that ships may overlap and that the density stamps are used only at planning time. This is an ADAPTATION noted in the
acceptance review; it removes the order dependence of the segment-end hook for the first slice and the dependent
PRNG-free but order-sensitive state.

## 6. Bounded deterministic work on large maps

Requirements (design, not measured performance): (a) planning is **per ship visit** (the registry already fixes the
visit order and cadence), at most `route_plans_per_advance` (config, default 4) plans are *started* per Advance; an
excess ship keeps its pending order, does **not** install an idle segment (it keeps what it is doing) and is retried at its
next visit, in registry order, so the outcome depends only on sim state; the per-Advance counter is not persisted (recomputed
from zero each Advance); (b) a query has a **work budget** in coarse-node pops and fine-node pops per query
(`route_coarse_budget`, `route_fine_budget`; defaults are set in P2 from measured worst cases of the fixture maps with a
documented margin and a recorded measuring procedure (not set here); the original has *no* cap and floods the window, so exceeding the budget is an ADAPTATION that yields
status `-3 BudgetExceeded` = failed plan, never a partial route; consequence stated: a route the original would complete makes the
ship idle, and large budgets are the remedy, not partial routes); (c) the window is the ORIGINAL box: centred on the **start tile only** with half-width `route_window_radius` (default 1024 = `[ship+0x6e]`
of `0x400`), clipped to the map, at most 32 x 32 blocks of 64 tiles measured from the 8-aligned origin (a larger window and a
target outside the box are failed plans in the original too; for maps up to 1024 tiles the window is the whole map); (d) static coast data is shared by all ships of a map and
read-only during planning; **planning runs serially at the ship's visit** in registry order (it reads other ships' *current*
positions for the density stamps, so no result may be computed ahead on stale state). A later optimisation may speculate on
workers only with a validation that re-runs the query when any input position changed; the sim result must not depend on core
count, wall clock or completion order (tested with 1/2/4/7 executor threads like the existing replay cases); (e) queues are finite and saved: the pending
order record is persisted state, planning itself is re-run (it is a pure function), so no half-finished work is saved.

## 7. Configuration and modding

`Rules`: `move_blocked_mask` (0x200e), `route_goal_radius` (6), `route_run_cap` (10), `route_retry_cap` (8),
`route_plans_per_advance`, `route_*_budget`, `ship_pickup_immediate` (off), `ship_speed_factor_volk` (1). Registry type
facts (section 4) carry per-type values; costs per class/density/direction are a validated table (default = the
original class table 0..4 in `docs/re/ship-navigation.md` 9.3/10.2). Faithful defaults are the original values;
improvements (immediate pick-up, hull clearance) are separate opt-in rules and change the rules digest. Content/rules
identity: the type facts enter the registry digest (a new distinct stream version, see section 8) and the rules enter
the rules digest; an unchanged candidate manifest hash is unaffected.

## 8. Persistence, save/load, replay and versioning

New persisted state needs a **new distinct stream version** (122 or later; never reuse 121) with cause-specific
old-version rejection tests. Persisted: the order record, segment, node list, remaining, `pos_fine`, `last`, factor
state and `dir16`, for every navigating ship. This is an ADAPTATION: the original saves none of the motion state and a
sailing ship stands idle after load, but a lockstep joiner and a replay must reproduce the same state, so the sim saves
it (the faithful "stand idle after load" is a documented non-goal for network/replay; a loader of an original save
must produce the idle state). `MoveCmd` joins the command codec (tag order is part of the replay format). Replay logs
and goldens move with the version; the independent Python reference moves with them (`tools/sim_stream.py`).

## 9. Hostile-owner avoidance, crew and information

- The avoid routine acts only on hostile owners (relation 2/3) and is deferred with the local detour (section 5). The
  five-ship start is at peace; the slice is correct for it.
- Crew (scout, donkey): not independent movers; their registry tick does nothing (speed 0, ORIGINAL: nothing copies the ship's
  position to them). **ADAPTATION:** the sim keeps their `Figure::pos` equal to the carrying parent's published position (written
  by the mover whenever the parent's position is published, same step, through a named Effects operation, section 4) so a
  consumer that reads a crew figure sees no stale position; `FoundCmd` reads the carrying ship's position as it does now.
- Information: movement-generated information (a ship at tile T observing terrain, other cities, ore) will use the same
  `audience` mirror as founding (`docs/sim/shared-visibility.md`): information only, never alliance, trade, or territorial
  rights. The original's sailing-driven discovery (if any; property-7 readers are UNKNOWN) is **not** part of this slice;
  no fog rendering is claimed; contact sets are not full visibility.

## 10. Implementation packages (later, each with an independent review)

| Pkg | Content | Depends | Acceptance |
|---|---|---|---|
| P1 | validated type/state facts: type facts + seal checks, `Figure` motion state, exact f32 constants, registry digest vectors (Python reference) | - | refusals at seal for out-of-domain facts; digest vectors; alternate type |
| P2 | route query: coast data, coarse map + masks, coarse/fine search, learning, goal scoring, run output; budgets | P1 | the passage catalogue (`fixtures/synthetic/sim/nav/passage_cases.txt`: reachability, goal tile and score, status; negative controls corner rule, radius, start rule), 1/2/4/7 threads identical. **Prerequisite:** route-run vectors (direction order, 10/14 costs, run cap, tie order) obtained from the original code under emulation are committed as fixtures before P2 starts; the passage catalogue does not cover them |
| P3 | stepper: segment start/turn/chunk, integrator, speed factor, stop trim, idle | P1 | `movement_vectors.txt` through C++ and the Python reference; boundary cases; drift bound vs exact |
| P4 | command/scheduler: `MoveCmd` validation, pick-up, figure-registry integration, planning budget, crew slaving | P2, P3 | rejection atomicity (bytes + PRNG), conflicting commands, replacement, non-default player/type |
| P5 | save/load/replay/version 122, `MoveCmd` codec, goldens | P4 | version guard with valid control, replay across thread counts |
| P6 | canary: unchanged candidate, spawn -> sailed approach -> `FoundCmd`, for two participants | P5 | the route is sailed, not relocated; founding legal at the arrival; payments; shared information mirror |

## 11. Open points and decisions for the orchestrator

1. Click rounding of the original UI pick (raw sub-tile versus tile centre) and the per-ship order inside a group
   command: UNKNOWN; the sim takes exact fixed-point targets and orders each ship independently (CAP-1). The cursor's
   flags (`|= 2` for a modifier, `0x10000`) reach SetOrder and enable the extra start-point set of `0x100a9e80`: not
   part of this slice (flags 0).
2. Decide the failed-plan behaviour (section 2), the exact-rational arithmetic adaptation (section 4), deferral of the
   local detour/avoid (section 5), the planning budget semantics (section 6) and persistence of motion state (section 8).
3. x87 precision (CAP-2) and the registry cursor at load (CAP-3) are runtime-only. The coarse-map build (per island at placement, before the
   first tick) and the vertex-grid padding (zero beyond `W x H`, identical to the repo's `VertexGrid`/`kBoundaryVertexHeight = 0`, so the
   existing terrain view is reused for the land rule) are settled statically. Remaining static items that do not block this contract: the
   object-top contribution to block extrema and tile type 9 (a building footprint becomes blocked; the exact object rule is read when the
   coast data is ported), the setters of state bits `0x2000`/`0x4000`, the UI pick provider and group body.
4. A candidate whose witness relies on a one-tile channel or on shore tiles needs re-evaluation under the original land
   rule before selection (evidence 11); this packet selects none.
