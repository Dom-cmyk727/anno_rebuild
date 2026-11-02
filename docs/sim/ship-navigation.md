# Ship navigation: the first sailing slice (contract, MAP-NAV-1; amended by MAP-NAV-2 package A)

Status: contract. **Section 12 (MAP-NAV-2 A) amends and supersedes the earlier sections where it says so; the superseded sentences below are
marked.** Evidence: `docs/re/ship-navigation.md` sections 6-12.
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
modulo 1.0 also for diagonal segments, see 12.1), then record the order (12.1: a world `order_id`, phase 0, target `(x, y)`).

## 3. Pick-up, planning and state machine

```
IDLE --order recorded--> (current segment ends, node list empty, order pending)
   -> PLAN: route query (section 5) -> failed: clear order, IDLE (+RouteFailed)
                                     -> status 2 (no runs): order consumed, IDLE
                                     -> plan: node list = turn/move segments, pos NOT moved
   -> SEGMENT-START: d>=3 TURN phases (facing +-1 immediately, f0/0.01 units each), then MOVE chunks <= 1 tile
   -> MOVING <-> SEGMENT-END (collision hook = density only here) -> next node ... -> list empty -> IDLE (arrived)
```
Pick-up latency (ORIGINAL): the order is picked up only when a segment ends and no node remains. The idle hook's segment (remaining 1.0,
speed 0.01 = 500 ticks) belongs to a ship that has **no** order; accepting an order first performs the stop semantics (12.1), which gives a
non-moving ship the short segment `remaining 0.01, speed 0.01` (1 stepper unit = 5 ticks), so pick-up follows within about one visit (a
ship in a turn phase finishes it first (the trim is a no-op there, 12.1); a moving ship sails its trimmed remainder, less than one chunk). This is the original
behaviour and is kept. A rules switch `ship_pickup_immediate` (default off, QoL, outside the faithful
core) may plan at command time later; it is not part of this slice.

The route is computed from the **start tile centre** (`ftol(x) + 0.5`), but the plan commit writes **no position**: the nodes
are velocity and length (relative), the first segment is installed with `remaining 0` and the facing set directly to the
first node's direction (no turn phase for the first heading). So the ship keeps its sub-tile offset and moves by the lattice
displacement; whether it ends within the goal tile centre or offset by that fraction is **UNKNOWN** and not asserted
(capture CAP-4 reads the position at plan time and after the first segment). The snap at `0x1003fb9a` is the local replan only
(deferred). The first heading bypasses the turn rule (ORIGINAL, `0x1003f910`) and is tested.

## 4. Type facts, state and numeric representation (integer only)

**Superseded by 12.4 and 12.5 wherever they differ:** the per-ship state list, `pos_fine`, `chunk_factor`, the Q24.8 publication formula, the width paragraph and
the rounding list below are the MAP-NAV-1 sketch; the authoritative numeric scales, rounding points and persisted fields are 12.4 and 12.5.

**Type facts** (validated at registry seal like the hull domain; all optional, absence = not a navigating type):
`speed` (HEAD f1, tiles per stepper unit), `turn_step` (HEAD f0), `nav_class` (LOGIC byte 0x10, 0..4, selects the
cost table), `max_hp`, `hold_cap`, `hold_count_a`, `hold_count_b` (the speed-factor inputs), `avoid_radius`
(the registry-query radius of the hook, type slot `+0x68(4)` = `LOGIC[0x25]/8` with 0 meaning 7.0: 16.0 tiles for a 1502,
`docs/re/ship-navigation.md` 7.4; unused until the deferred hook is ported). Further type facts from the same slots:
`stamp_radius` (slot `+0x68(5)` = `LOGIC[0x28]/8`, 0 meaning 2.0; 5.0 tiles for a 1502), used by the route query's obstacle stamps. Conversion of a file f32 to the engine's value: the f32's exact binary rational times 2^32, rounded to nearest (even) (**ADAPTATION**,
**absolute** error at most 2^-33 tile per stepper unit for every speed; the earlier "relative" wording is withdrawn; all numeric scales are
in section 12.4). Type ids, player ids and the 16-direction count are data
or config, never constants in `sim`; the alternate-type test uses a different ship type and a non-default player.

**Per-ship motion state** (new persisted state, section 8): `segment {vx, vy (Q0.32 signed), length, speed, kind}`,
`remaining`, node list, order record `{id, phase, flag, flags, target}`, `factor_percent` (V), `chunk_factor`, `last`
(u16), position accumulator **`pos_fine` in 2^-32 tile (i64 per axis)**, facing `dir16`. **Superseded by 12.4:** the "ticks scaled by 5", `distance_x5`, "floored at 2^-32 tick" and "error below 2^-32 per boundary" statements of
this paragraph; the numeric scales are those of 12.4. **Seam fit (checked against `scheduler.cpp`):** the existing figure-tick seam dispatches the whole registry visit and writes only
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
time crosses a speed change (error below 2^-32 tile per boundary), (iii) the dropped diagonal residue fraction (< 2^-32 tile) at every segment start and (iv) the floor to Q24.8 at publication. The original's
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
`pos + velocity * 25.0` (velocity = slot +0x60 = unit +0x20 = direction x speed x factor of the current segment, per stepper unit; MAP-NAV-3 N2 corrected the earlier unit-heading reading) with the 5 x 5 kernel (a stamp is only a record; the tile is the truncated point; density is evaluated per candidate tile, **no clamping**, and affects only the
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
visit order and cadence), admitted by the **fair bounded rule of section 12.2** (the first-come per-Advance quota of the
MAP-NAV-1 draft is withdrawn: it starves slot 5 of slots 1..5); (b) a query has a **work budget** in coarse-node pops and fine-node pops per query
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

**The persisted fields are those of 12.5** (`pos_u`, `res24`, `ticket`, order record, segment, nodes, `last`, world counters); `pos_fine` and "factor state" below
are the earlier names.

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
2. The orchestrator approved (review of MAP-NAV-1): atomic command refusal versus later `RouteFailed`, the exact-rational integer integrator (conditional on the
   package D arithmetic checks), the authenticated-issuer ownership check, motion/pending-admission/carry persistence under a new stream version, the
   first-slice deferral of local detours and hostile avoidance, and bounded serial deterministic planning with the fair admission of 12.2 (sections 12.1-12.5
   replace the earlier sections 2, 4, 6 and 8 where they differ).
3. x87 precision (CAP-2) and the registry cursor at load (CAP-3) are runtime-only. The coarse-map build (per island at placement, before the
   first tick) and the vertex-grid padding (zero beyond `W x H`, identical to the repo's `VertexGrid`/`kBoundaryVertexHeight = 0`, so the
   existing terrain view is reused for the land rule) are settled statically. Remaining static items that do not block this contract: the
   object-top contribution to block extrema (12.6: anchor tile only, `ftol(f / 0.04f)`, stored `>> 1`; the builder rule that creates shore type 3 from
   these records was not re-executed, so the coast-data port must read `0x100aae00` pass 2 before shore classification is implemented: a named
   dependency of package C), the setters of state bits `0x2000`/`0x4000`, the UI pick provider and group body.
4. A candidate whose witness relies on a one-tile channel or on shore tiles needs re-evaluation under the original land
   rule before selection (evidence 11); this packet selects none.


## 12. MAP-NAV-2 package A: amendments (these supersede the sentences they name)

### 12.1 Commands, status words and identities
- `MoveCmd` (tag `0xF071`, section 2) and a separate **`StopCmd`** (tag `0xF072`, payload `{FigureId ship}`) are the two public ship commands. They
  are distinct commands, so a valid `MoveCmd` target `(0, 0)` (the world corner) is never confused with a stop; the original's "x = z = 0 is a stop"
  is deliberately **not** copied (ADAPTATION). Both join the codec tag order and `is_native_wire_type`.
- Validation (atomic; bytes, PRNG cursor, motion state, tickets and counters unchanged on refusal): checks 1, 2, 3 and 6 of section 2 for both
  commands; `MoveCmd` also checks 4 (blocked-state mask) and 5 (target inside the world). `StopCmd` refuses a ship whose `state_word &
  move_blocked_mask` is non-zero too (nothing to stop; ADAPTATION, consistent with `MoveCmd`) and is otherwise accepted even when the ship is
  already idle (a no-op that still clears a pending order).
- **Accepted `MoveCmd`:** performs the stop semantics (below), then records order identity `order_id` (a world counter, u64, assigned at
  acceptance, never reused), target `(x, y)` in Q24.8, and a **ticket** (12.2): a new ticket if the ship holds none, the **existing ticket if
  it does** (a replacement keeps its place in the queue). Accepted does not mean routed.
- **Stop = a motion part and an order part (ORIGINAL slot `+0xdc`, `0x10043350`, read in full).** *Motion part* (used by both `MoveCmd` acceptance and
  `StopCmd`): clears the node list; sets the plan end to the current position; resets the speed-factor multiplier to 1.0; and, for a ship whose state bit 0
  ("moving") is set, trims `remaining` to the remainder past its whole 1.0-length chunks (`remaining -= trunc(remaining) * 1.0`: the remainder is kept and
  still sailed, then the ship idles). **Definitions used in this document:** the state bit 0 is set by a *movement segment* (kind >= 8) **and by the stepper's
  own turn phase** (`0x1003ec75`: `state |= 0x201`, facing changed by +-1, so its parity alternates between consecutive phases), and is clear in the planner's kind-3 turn nodes,
  the idle segments and terminal segments. The original's odd/even selector is **bit 0 of the ship's `dir16` facing**: in a movement segment the facing is
  `dir16` of a lattice vector (`2 * dir8`, even), so diagonal movement segments trim by 1.0 as well; in a stepper turn phase the facing is odd or even (it alternates) and either branch may run, but its `remaining` is `turn_step` (< 1 tile, validated) so `trunc(remaining * 0.7071)` and `trunc(remaining)` are both 0 and **the trim is a
  no-op on either branch**; the port therefore trims movement segments by 1.0 and leaves turn phases untouched, and refuses an odd facing in a *movement*
  segment (invalid state); an odd facing in a Turn segment is valid. A ship that is not moving (bit 0 clear) gets the idle segment (`remaining = speed =
  0.01f` = q32 `42949672`). The original's two other non-moving branches are unreachable for a ship the commands accept: state bit `0x200` without bit 0 only comes from the terminal kinds' `0x202`/`0x204`, whose bits `0x2`/`0x4` are in the blocked mask (`0x200` alone is not in `0x200e`), and a non-moving ship with state bit `0x2000` (blocked mask) returns without change; both are
  ported as "refused" for any mask containing them. The persisted segment kind (Idle / Turn-planner node / Turn-stepper phase / Move) distinguishes the stepper's turn phase (stop lets it finish) from the planner's kind-3 node (bit 0 clear: a stop replaces it by the idle segment). **ADAPTATION:** a ship whose current segment is already the idle segment with `remaining <= 0.01f` is **not
  re-armed** by a further stop or replacement (the original re-arms it), so repeated replacements cannot keep an older ticket permanently unready.
  *Order part* (only `StopCmd`; `MoveCmd` does not do this): removes the pending ticket and clears the order record; emits `Stopped{ship, order_id}` when an order
  was cancelled. `MoveCmd` acceptance performs the motion part, then records the new order and **keeps an existing ticket**. A superseded order is simply replaced; no event.
- **Outcome vocabulary (stable identity):** every routed order ends in exactly one of `RouteFailed{ship, order_id, reason}` with reason
  `Unreachable` (engine status -2), `BudgetExceeded` (a work or memory limit of 12.3), `OutsideWindow` (the target lies outside the start-centred
  window; also engine -2 in the original) ; `Arrived{ship, order_id}` (status 2, no runs, or the last segment finished); `Stopped{...}`; or it
  is `Superseded` by a later `MoveCmd` (not reported). A result is attributed to the `order_id` that was routed, never to a later order.
  Quota waiting is **not** an outcome and emits nothing.
- **Lakes and exterior sea.** Navigation only needs reachability between the start tile's water and the goal candidate: a voyage entirely inside an
  enclosed lake succeeds; an order from the sea into a lake (or the reverse) is routed and ends `RouteFailed{Unreachable}` (it is *not* an atomic
  command refusal). The v2 candidate **spawn policy** (ships start in exterior-4 sea) is a generator constraint on spawns and does not restrict
  navigation.
- **Planning-only density.** The route query uses ship density stamps (12.3, evidence 9.4) to choose among goal candidates. The original's
  segment-end collision handling (hook `0x10042210`: density-cost local detour `0x1003de40`, hostile avoidance `0x10041bd0`) is **deferred**: moving
  ships may overlap (approved for the first headless slice, not full peaceful-navigation fidelity).
- **Land start tile (conservative first-slice rule, original parity UNKNOWN, capture CAP-6c):** a start on a land tile may leave through adjacent
  reachable water, but the start tile is **not itself offered as a destination candidate**. A water start tile can be the chosen candidate
  (status 2). `passage_cases.txt` pins exactly this; CAP-6c may later replace it.

### 12.2 Fair bounded planning admission (replaces the quota of section 6(a))
State (persisted and hashed, 12.5): world `next_ticket` (u64); per navigating ship an optional `ticket` (u64). Rule:
1. A ticket is issued by an accepted `MoveCmd` only when the ship has none; **replacement keeps it**; a served order, `StopCmd`, ship removal, a refused
   plan (`RouteFailed`) or **the ship entering a blocked state** (`state_word & move_blocked_mask`, checked at its visit: the order and ticket are dropped with
   `RouteFailed{ShipBlocked}`) removes it. A ticket therefore never outlives its ship's ability to reach the pick-up point, so no ticket can reserve quota forever.
2. Per Advance `quota` plans may start (`route_plans_per_advance`, default 4). During the registry visit phase, when a ship is visited, its
   order is pending and its pick-up point is reached (segment ended, node list empty), it is **admitted iff the number of pending tickets
   older than its own is smaller than the quota left in this Advance**; admitting consumes one unit of quota and the ticket. Otherwise the
   ship is **deferred**: it stays at the pick-up point, keeps its order and ticket, emits nothing, installs no idle segment, and **its
   elapsed visit time is discarded** (`last := now`, leftover not carried; ADAPTATION, the ship is stationary), so the u16 stamp cannot wrap
   while it waits and admission later starts from that visit's own elapsed time.
3. The pending count older than a ticket is computed from the ordered set of tickets (a derived index over the figures, rebuilt on load).
4. **Finite service bound (proof to be mechanised in E):** let a ship hold the oldest pending ticket. Every ship visited earlier in the same
   Advance was admitted only if quota remained for all older tickets, so quota is left when the oldest ticket's ship is visited and it is
   admitted at its next visit after it reaches its pick-up point. A ticket with `p` older pending tickets is therefore served within
   `(p + 1) * C + R` Advances where `C = ceil(n / k)` is the registry pass length (`n` slots, `k = clamp(floor(dtick * n / 50), 1, n)` visited per
   Advance, slots visited in **cursor order with wrap-around**) and `R` is the per-ship time to reach the pick-up point after acceptance: a trimmed
   remainder shorter than one lattice step (`L_d / speed`, e.g. 39 stepper units = 196 ticks for a 1502 diagonal) or the 1-unit stop segment (5 ticks),
   i.e. at most `R = ceil(max(R_move, R_turn) / dtick_min)` Advances where, in **ticks**, `R_move = ceil(5 * 2^32 * 100 / (speed_q32 * V_min))` (one 1.0-length chunk at the slowest
   allowed factor `V_min`, 65 % in the faithful speed factor; a stepper unit is 5 ticks) and `R_turn = ceil(5 * turn_step_q32 / 42949672)` (the turn segment at speed 0.01);
   for a 1502 (speed 0.036, `f0 = 0.11`, `V_min = 65`, `dtick = 5`) that is 214 ticks = 43 Advances, not the 6 of the earlier unit-confused formula. R is a **per-type finite number**, not a constant. The derivation is
   `wait_i <= max(wait_{i-1}, R) + C`, so the bound holds for `p = 0` too (the oldest ticket must also become ready). Preconditions, all validated: `route_plans_per_advance >= 1`;
   `C` is evaluated at the current registry size `n` and the smallest Advance `dtick` (`k = clamp(floor(dtick * n / 50), 1, n)`); a visit's budget is at least the 5 ticks of
   the 1-unit stop segment (the no-re-arm rule above removes the need for it to hold with tiny registries). `tools/ship_nav_ref.py
   admission-check` models the real cursor/cadence (100 slots, 5-tick Advances, ships in slots 1..5) with hand-derived counts and a readiness model (a ticket becomes servable `ready_delay` Advances after acceptance; a blocked ship drops its ticket; the
   mutant that keeps a blocked ship's ticket starves everybody behind it): the rejected
   first-come rule serves `100 100 100 100 0` over 1000 Advances, the fair rule `100 100 100 50 50` with no ticket waiting more than 20
   Advances, a property check over random replacement/cancellation seeds, and a negative control (renewing the ticket on replacement starves
   the youngest ship).
5. `BudgetExceeded` is a query outcome after admission and is distinct from waiting for admission.

### 12.3 Route query limits (bounded work and memory)
Per query limits (`Rules`, validated): `route_coarse_budget` and `route_fine_budget` (node pops), `route_window_cells` (checked window dimensions
in 64-tile blocks, default 32 per axis = the original's), `route_memory_bytes` (coarse + fine working sets, retries, result runs counted).
Preparation and retries count toward the budget; exceeding any limit returns `BudgetExceeded` before any state change. Defaults are set in package
C from measured representative small and real-candidate queries with a margin (not from the 64 x 64 fixtures alone); the original has no cap, so
a larger default is the remedy, never a partial route. Original window behaviour is the default of a configuration value, not a fixed array.

### 12.4 Numeric scales, residues, rounding and seals (replaces the sketch of section 4)
- **Lengths and speeds** are exact integers in `q32` = 2^-32 tile: an axis step is `2^32`; a diagonal step length is `L_d = 11863283 * 2^9` q32 (= 6074000896, **above 2^32: lengths are u64**; the exact rational of the original f32 `0x3FB504F3`, 1.41421354; the integer `0x3FB504F3` itself is just the f32 bit pattern); a diagonal axis component is `c = 11863283 / 2^24` (exact rational of the original f32 `0x3F3504F3`); the diagonal displacement per
  step is therefore `2 * c^2 = 0.9999999658` tile per step (a shortfall of 3.4e-8 per step), exactly as the original's constants give (not forced to 1.0).
  A run's node length is the exact integer `steps * L_d` (the original rounds `f32(1.4142135 * steps)`; the difference is at most 2^-20 relative: ADAPTATION).
- **Type speed** `speed_q32 = round_nearest_even(f32_value * 2^32)`; **seal domain** `0 < speed_q32 < 2^28` (about 0.0625 tile per stepper unit;
  HEAD values are <= 0.05), `turn_step_q32` (HEAD f0) in `(0, 2^32)`, `V` in `1..100` (percent). Absolute conversion error <= 2^-33 tile per unit.
- **Time** is the integer tick difference `d = u16(now - last)`. One tick = 1/5 stepper unit; no division by 5 is ever made: the budget of a
  visit is `B = d * speed_q32 * V` in the scale `q32 / 500` (value < 2^16 * 2^28 * 2^7 = 2^51, u64). Remaining length is held as `rem_q32 * 500`
  (<= 2^32 * 500 per chunk, u64). A step uses `P = min(B, rem * 500)`; when the segment finishes, the unused budget `B - P` converts to the next
  segment's scale as `floor((B - P) * (s_new * V_new) / (s_old * V_old))` (one floor; none when the product is unchanged; error < 1/500 q32). The product reaches `2^51 * 2^35 = 2^86`: it needs the same portable 128-bit helper and a **128 / 64 division** (specified and unit-tested alongside it).
  Turn segments (and the stop's idle segment) use speed `0.01f` = q32 **`42949672`** (exactly; `f32(0.01) * 2^32` is an integer, not `42949673`) and `V = 100` in the budget formula; a turn segment's length is `turn_step_q32`, the idle segment's remaining is the **same `42949672`** (so a 5-tick visit at V = 100 finishes it: budget `5 * 42949672 * 100`, remaining `42949672 * 500`); a zero-length segment finishes immediately and cannot loop (every iteration of a visit spends budget, starts a node or changes the segment, so the loop terminates; `kMaxIterations` only bounds a corrupt state).
- **Residue rule (replaces the earlier sign rule):** `res24` is a non-negative fraction accumulated only inside a **diagonal movement segment**; it is **reset to 0 at every segment start and whenever a segment ends or is removed** (finished, stopped, replaced, deferred without a segment) and is 0 whenever the current segment is not a diagonal movement with `remaining > 0` (validated). The signed whole-unit displacement of an axis step is `sign * floor((P * 11863283 + res24) / 2^24)` with the magnitude's remainder written back to `res24`, so the residue is never negative and no sign has to be persisted. The fraction dropped at a segment start is below 2^-32 tile (one more rounding, listed in the error statement below).
- **Position:** per axis `pos_u` (i64) = position * 2^32 * 500 and a residue `res24` (u32 < 2^24) carrying the diagonal product: axis step
  displacement `P`; diagonal axis displacement `floor(P * 11863283 / 2^24)` with the remainder added to `res24` (a carry into `pos_u` when
  `res24 >= 2^24`). The product `P * 11863283` is < 2^75: a **portable 64 x 64 -> 128 helper** (no `__int128`) is required and its signed and
  unsigned edge cases are unit-tested. **Seal bound:** `world edge <= 2^20` tiles (checked at world creation when any navigating figure type
  exists) so `|pos_u| < 2^20 * 2^41 = 2^61` fits i64. **Published** `Figure::pos` (Q24.8) = `floor(pos_u / (500 * 2^24))` (floor toward
  negative infinity), recomputed after every step; the only rounding that reaches the observable state.
- **Error statement (a requirement still to be proven in D, not an established fact):** the position error against the mathematical model with the
  stated `speed_q32` is below `2^-32` tile per segment boundary plus the floor at publication, and the difference to the exact `f32` speed is at
  most `units * 2^-33` tile for `units` stepper units; D shows these on long voyages and does not call the result original-runtime parity.

### 12.5 Persistence contract (all future-affecting state; stream version >= 122)
World: `next_order_id`, `next_ticket`. Per navigating ship: the order record (`order_id`, phase, target Q24.8), `ticket` (optional), segment
(`kind`, direction class axis/diagonal with its dir8, `length_q32`, `rem_q32`, `speed_q32`, `V`), the node list (runs), `facing` (`dir16`), `last`
(u16), `pos_u` and `res24` for both axes, the "deferred" marker is implied by order-pending + no segment. Derived, never saved: the ticket index,
coast data, spatial index of figures. Crew children's positions are written from the parent each step (no hidden state). An original-save import
initialises every ship idle. Version 122 is reserved for this layout; any later incompatible change takes a new number.

### 12.6 Goal selection, object heights and shore (A3, pinned)
The route query's candidate selection is exactly the callback/Offer/table rule of evidence 9.4 (including that a congested store never raises the
best score and that free beats congested); the coarse pop order of the goal cells is produced by the port's own coarse search and is checked
against `offer_cases.txt` (its `pops` are the emulation's, an input to the independent reference). Tile blocking from objects uses the predicate
`((h + base) & 0xff) > 19` per object tile with `h` = `floor(f32_height * 25)` computed exactly (the 53/64-bit result; the live precision is
UNKNOWN, CAP-2, so the 29 types that differ at 24-bit precision are recorded) and no class test: **not every footprint is land, and a footprint is
not exempt from blocking**. The coast-data rebuild trigger is a data flag on building types (`coast_rebuild`), default from the original type
flag bits `0x2000`/`0x4000` and the removal group `0x141..0x160`; shore cost type 3 uses the validated class cost table (class 0: 50/69).

### 12.7 Plan nodes from the route runs (B0, ORIGINAL; executed on the real bytes, 300 random plans and all 128 facing x dir8 combinations)
The route engine's runs `(dir8, steps <= 10)` become, in `0x100a8c50`: **turn nodes only before the first run, then exactly one movement node per run**
(runs are not merged; heading changes between runs are the stepper's own turn phases). With `want = 2 * dir8(run 0)`, `diff = (want - facing) & 15` mapped to
`-7..+8` (8 stays +8): `|diff| <= 2` gives no turn node, otherwise `|diff|` turn nodes with facings `f, f+s, ..., f+(|diff|-1)s` (mod 16), `s = +1` if `diff > 0`
(so the **planner turns a 180 degree reversal in the + direction** with 8 nodes, 440 ticks for a 1502, where the stepper's own phase would turn -1 in 330). A turn
node is `velocity 0, length = turn_step, speed 0.01, kind 3` (facing set at its start, then it lasts `f0 / 0.01` units = 55 ticks for a 1502); a movement node is
`velocity = dir * speed` (diagonal `c * speed` per axis), `length = steps` (axis) or `steps * L_d` (diagonal), `speed`. The commit seeds the first node as the current
segment with `remaining 0`, sets the facing directly to `dir16` of the first **movement** vector (a turn-first plan: transient 4, DERIVED) and writes no position.
Between runs a 90 degree change costs 2 stepper phases (110 ticks, 1502), 135 degrees 4 phases, 180 degrees 6 phases (the `d >= 3` rule of 7.3). The node
builder is `plan_nodes` in `tools/ship_nav_ref.py` with `plannodes` vectors (closed form, hand-derived, equal to the emulation). The order record's state machine
(`phase` 0 -> 1 at pick-up; success keeps `1/1`, arrival clears; status 2 clears next callback without moving; failure clears at once) is the persisted `Order` of 12.5.

Persisted-state notes (12.5 addendum): `res24` is persisted but is non-zero only inside a diagonal movement segment (reset at every segment start and end); a *movement* segment's `dir16` facing
is even (an odd value there is an invalid state; a Turn segment's facing may be odd). **12.5 supersedes** the field lists of sections 4 and 8.
Reference limits (stated so a green test is not over-read): the route oracle checks path legality, status and endpoint and **accepts legal non-canonical or
non-optimal run lists** (for example `E5 E5`); exact run equality, discovery order, ties and costs are asserted only by comparing the C++ router's output with the
emulated-original run lists; the closed forms of 12.6 (`objtile`) and 12.7 (`plannodes`) are the same formulas as the reference, and their independent evidence is the
emulation recorded in the private evidence notes, not this repository.

### 12.8 Package B as implemented (stream 122) - decisions the code fixes

- **Loader policy.** A ship-class type (`BEWOHNTYPE` 113) with LOGIC hit points 0 (the shipped 343 and 1510) is not a navigating type. Every other ship-class type must have navigation facts inside the domains of 12.4 or the registry load fails with `figure <id>: navigation: <field> ...`; a value is never silently dropped. The registry seal applies the same domains to builder-supplied facts. Both hold counts 0 is valid (the shipped type 391): the cargo term of the speed factor is then 0 (no capacity, no cargo).
- **q32 conversion.** `speed_q32`, `turn_step_q32` are the round-to-nearest-even of the exact f32 value times 2^32, in integers; tests cover ties below 2^-9 (`3*2^-33 -> 2`, `5*2^-33 -> 2`, `7*2^-33 -> 4`).
- **Rules.** `NavRules` validation also requires `floor * (100 - cargo_penalty) >= 100`, so the speed factor V cannot reach 0 (V >= trunc(floor * (100 - penalty) / 100)).
- **Validated state** (`validate_navigation`): turn and idle segments have factor 100 and dir8 0; the idle segment has at most 1.0 left (the original's idle hook re-arms `remaining 1.0, speed 0.01`; a stop installs 0.01f, 12.1); a movement node is 1..`run_cap` whole steps of its direction (orthogonal `2^32`, diagonal `L_d`) with facing 0; a turn node has dir8 0; an unrouted order holds its ticket and has no nodes; a routed order holds no ticket; `Figure::pos` lies within `kWorldEdgeBound` tiles (the creation check against the configured world is stricter); `nav_class < Rules.nav.class_costs.size()` is checked when a ship of that type exists (the registry does not know the Rules). The target-inside-the-world check of 12.1 belongs to `MoveCmd` (package E).
- **Independent bytes.** `NavMotion` wire bytes are hand-transcribed in `ship_nav_state_test.cpp`; the Python transcription (`tools/sim_stream.py`) covers the ids, `Rules.nav` and the figure-type digest, not a populated figure (open: a Python writer for one populated ship).
- **Segment remaining is held x500.** `NavSegment.remaining_x500` = remaining length in q32 * 500, the integrator's budget scale (12.4), so a partial step leaves no sub-unit fraction in the persisted state. Idle after a load is `42949672 * 500`.
- **Zero holds (ADAPTATION, recorded for D).** For a type with `hold_count_a + hold_count_b == 0` (shipped 391) or capacity 0 the original's formula divides by zero (result unknown); the port uses a cargo term 0. D clamps the cargo sum to `(n0 + n1) * capacity` so `L >= 100 - cargo_penalty` and V stays >= 1 as the Rules check assumes.
- **Speed factor at segment start (ADAPTATION).** The original keeps a stored multiplier updated on hold/hit-point events and reset to 1.0 by a stop; the port derives V from the current holds at every segment start (health is not modelled: full health), so a ship never sails with a stale multiplier. Documented difference for CAP checks.

### 12.9 Package D as implemented (`ship_step.hpp/.cpp`, `tools/ship_step_ref.py`)

- **Stepper.** `nav_step` is the original loop of 7.2 in integers: `d = u16(now - last)` ticks; budget `B = d * speed_q32 * V` in q32/500 (rate per tick = `speed_q32 * factor_percent` of the *current* segment); a segment finishes when `remaining_x500 <= B` (equal finishes, as `P <= remaining`), the position advances by exactly the consumed units, the leftover budget converts to the next segment's rate with one floor (`floor(leftover * new_rate / old_rate)`, 128-bit product, 128/64 division). `last = now` after the loop ran; `d == 0` changes nothing. Position: orthogonal exact; diagonal per axis `floor((P * 11863283 + res24) / 2^24)` with the remainder in `res24`, reset at every segment end (and start). `Figure::pos = floor(pos_u / (500 * 2^24))` after every visit.
- **Segment end** (`0x10042a13` order in 7.2): nodes pending -> start the next node; else an unrouted order asks the planner (`NavPlanner`, supplied by package E): `Nodes` commits (list installed, order routed, ticket consumed, facing set directly to the first movement, a zero-length first segment so leftover time carries on, state bit 0), `Arrived` (status 2 or a plan without movement) and `Failed{reason}` clear the order with an event, `Deferred` leaves order and ticket and installs a zero-length idle segment so the next visit asks again; a routed order with nothing pending is `Arrived`; otherwise the idle segment (`remaining 1.0, speed 0.01`, 500 ticks) is installed.
- **Node start** (7.3): a planner turn node sets its facing and lasts `turn_step`; a movement node whose heading differs by `d >= 3` sixteenths turns one phase (reversal -1) and stays pending; otherwise the facing snaps, the factor V is read, and a node longer than `1.0 + f32(1e-4)` is cut: one 1.0 chunk, the rest stays. f32(1e-4) = `0xD1B717 * 2^-37` = 13743895 / 2^37 (derived from the bit pattern by `tools/ship_step_ref.py`), so the limit is `2^32 + 429496.72` q32 and the integer test is `length_q32 > 2^32 + 429496` (`kNavChunkLimitQ32`); a segment is installed whole with up to that much more than 1.0 (ADAPTATION: the sum is formed exactly, as x87 extended does; not captured on the original). The first review found this constant typed wrongly (it was f32(0.1)'s mantissa); the oracle does not chunk (chunking is invisible to positions), so the constant is pinned by the hand test at 429496/429497 only.
- **Stop** (`nav_stop_motion`): nodes cleared; a movement segment keeps `remaining mod 1.0`; stepper turn phases untouched; any other segment becomes the idle segment with `remaining = speed = 0.01f` unless already that short.
- **Speed factor** (`nav_speed_factor`): 7.5 with the clamps of 12.8 (cargo sum clamped, zero divisor = no cargo term, health = full).
- **Error against the stated model** (`tools/ship_step_ref.py`: exact rationals, no integer scales; 25 vectors with diagonals, mixed headings, turns of 90/135/180 degrees, planner reversal nodes, speed factors, speed changes with fractional leftover, sub-tile and negative starts, u16 wrap, chunk boundaries, one-tick/irregular/65535-tick visits, a 1200-tile voyage, a V = 65 plan whose turns still run at V = 100, and a rate ratio of 671 between neighbouring nodes): the position after every visit agrees within the bound `tolerance()` derives from the carry terms: entering segment i floors the converted leftover once (under one unit of that segment's rate, i.e. under `1 / r_i` ticks, which shifts all later motion by at most `max(r_j, j >= i) / r_i` units), each diagonal segment end drops a residue under one unit, plus one unit of slack; for equal ship speeds the rate ratio is the turn-to-move ratio (at most about 6) but plans with very different node speeds amplify the floor by their ratio, which is why the bound is computed per plan and not a constant (the planner uses one ship speed per plan); the measured worst error is 34 units (bound 250) on the 1200-tile voyage (1.6e-11 tile, against the 1e-6 tile target of 12.4) and 0 on every axis-only case; the arrival visit equals the model's in all 25. This is the mathematical model with the stated constants, **not** original-runtime parity (the original's f32 noise is up to +0.0178 tile at one-tick visits, 7.10).
- **Persistence.** Stopping after any visit, saving the `NavMotion` through the archive and continuing gives byte-identical motion, position, facing and state word at every later visit (split points inside turn phases, diagonal residues and speed changes).

### 12.10 Package E as implemented (`ship_orders.cpp`, `ship_mover.cpp`, `route_cache.cpp`)

- **Commands.** `MoveCmd` (`0xF071`, 16-byte payload) and `StopCmd` (`0xF072`, 8 bytes) are variant alternatives 8 and 9 and native wire types; atomic validation in the order of the table of section 2 (unknown issuer, 1 unknown figure, 2 wrong owner -> `NotShipOwner`, 3 not a navigating type -> `NotAShip`, 4 `state_word & move_blocked_mask` -> `ShipDisabled`, 5 target outside `[0, width) x [0, height)` -> `BadValue` (MoveCmd), 6 a crew child -> `ShipEmbarked`; a counter at its maximum -> `SerialExhausted`). An accepted `MoveCmd` performs the motion part of the stop (`nav_stop_motion`), records `order_id = next_order_id++` (unrouted) and keeps the ship's ticket or takes `next_ticket++`. `StopCmd` clears order and ticket and emits `ShipStopped{order_id}` if an order was cancelled. No new persisted field: stream 122 is unchanged.
- **Dispatch.** `Scheduler::run_figures` sends every figure that carries `NavMotion` to `ShipMover::visit` (before the figure-tick seam and the transport body), with the seam protocol: the ship writes only itself, everything else goes through `Effects` (events in `ui`, crew positions in the new `figure_moves`, merged in order after the spawns).
- **Mover.** Per visit: a ship with an order that is in a blocked state drops order and ticket (`RouteFailed{ShipBlocked}`); V from `nav_speed_factor`; the stepper with the world planner; events `ShipArrived{order_id}` / `ShipRouteFailed{code = NavFailReason, order_id}` (a result is attributed to the order that was routed); crew (every figure whose parent chain reaches the ship) takes the ship's position when it changed (one registry pass per moving-ship visit; a derived child index is the improvement path).
- **Planner / admission (12.2).** `quota - plans_started` is the quota left in this Advance (`plans_started` is a local of the registry pass, reset every Advance); the ship is admitted iff the number of unrouted orders with an older ticket is smaller than the quota left (one registry pass per attempt, bounded by the registry size and run only for ships at their pick-up point), otherwise `Deferred`. After admission: the route query over the derived coast index (`RouteCache`: built on first use from the islands, dropped by every successful command except Move/Stop/SetSpeed and by copying or moving the `World` (the moved-from cache too), never hashed). The index is built from TERRAIN ONLY (`ship_route_terrain_options()`): placed objects change without any command (island growth), so an index that included them would differ between a continuously running world and the same world after a save and load. ADAPTATION: the original includes the objects present at the island's placement and refreshes the coarse map only for coast and road objects (flag not modelled); modelling that flag with hashed state is the path back to object blocking, the stamps of the nearby ships (`local_ship_stamps` over one registry pass), `plan_route` with the limits and cost classes of `Rules.nav`; `Planned` becomes turn nodes + one node per run (12.7), `Arrived` and `Unreachable`/`BudgetExceeded`/`OutsideWindow` map to the stepper's outcomes.
- **Open (recorded):** object blocking is off (above) and the placed items' height byte is not carried; `RouteStatus::InvalidQuery` is reported as `Unreachable` and a `Planned` result with no runs as `BudgetExceeded` (both would indicate a bug); the registry passes above are O(registry size) per admission attempt or moving-ship visit and should become a derived spatial/ticket index before late-game scale; the route cache is rebuilt whole after a building command.

### 12.11 MAP-NAV-3 N2 - the stamp uses the segment velocity (`ShipMover::stamp_velocity_q32`)

Defect (review 2026-11-02): `RegistryShips` handed `LocalShip` a normalised direction (+-1 / +-0.7071) and the consumer multiplied it by 25 tiles; a type-1502 ship at x=5432/256 was stamped at tile 46. The original's slot +0x60 returns unit +0x20, the current segment's **velocity** (direction x speed x segment factor; RE 7.2, 7.3, 9.4), in tiles per stepper unit (5 ticks); the stamp is `trunc(x + 25 * v)`.
- `LocalShip::vel_x_q32 / vel_z_q32` = that velocity in 2^-32 tile per stepper unit (int32; |v| <= type speed <= 2^28 because the factor is <= 100). The stamp is `trunc((x_q8 * 2^24 + 25 * v) / 2^32)` toward zero in i64 (no overflow: < 2^55 + 25 * 2^28).
- `stamp_velocity_q32(seg)`: Move segments only, `unit_q24 * speed_q32 * factor_percent / (100 * 2^24)` truncated toward zero, `unit_q24` = 2^24 (axis) or 11863283 (diagonal component); the factor is the one **captured by the segment** (`seg.factor_percent`), never the next segment's. Turn, idle, stop and any other segment give {0, 0} (the stamp lies on the ship's own tile; the unit still counts). Truncation error < 2^-32 tile in v, 25 times that in the stamp offset.
- Test evidence: `ship_nav3_test.cpp` "N2 stamp" cases take the stamp from an actually moving world (`ShipMover::local_stamps`) and the planner's resulting endpoint (20,65), against (21,64) under a mutant with a far look-ahead; the expected tile comes from the documented equation, not the adapter.

