# Ship navigation and the geometric sea model

**Status:** MAP-NAV-1 evidence (static analysis of the original modules, 2026-11-01). The version-2 sea
rule stays a tool model. Sections 6-12 record what the original does; `docs/sim/ship-navigation.md` is the
contract derived from them. Nothing here is implemented in `src/`.

## 1. The geometric model, kept as a tool

`map2-ship-start-v2` uses exterior-4 water: a water tile is `HEIGHT < 16`,
and exterior water is the 4-connected component that touches the world
border. Ships and founding approaches must lie in it. Corner contact does
not connect a lake to the sea. The route is 4-connected, one tile per step,
no diagonal step, spawn clearance only, no hull. Reports label it
`sea: exterior-4`. It is not a hashed policy field.

That model answers: "is there a 4-connected water path on this height mask?"
It does not answer: "can the original ship sail it?"

## 2. Navigation investigation (first pass, superseded)

The first pass found `AnnoWelt.dll` `0x1003de40` and stopped at its planner object. **MAP-NAV-1 (sections
6-12) supersedes it.** Two corrections it needed: `0x1003de40` is the *local obstacle replan* run at a segment
end when other ships are near (section 10), not the route builder; a ship's route is built by a separate
two-level engine (section 9) that the first pass did not reach. Diagonal passage and clearance are now
established for that engine (section 9.5); what stays open is listed in section 12.

## 3. Four meanings of a narrow channel

| Name | Meaning | Status on the candidates |
|---|---|---|
| Boundary-connected water | Exterior-4. A corner touch is closed. | Defined. The v2 checker uses it. |
| Geometric tile route | 4-connected steps on that water, cost 1, no hull. | Defined. The v2 route. |
| Original ship passage | The two-level route engine (section 9) plus the float segment stepper and the local detour planner (section 10). | Established for the engine; see section 11 for the comparison and section 12 for the open points. |
| Future engine route | The lockstep navigator, once specified. | Not implemented. It must not copy exterior-4 silently and call it original. |

## 4. Facing

Original ship facing is property `0x16`, stored at ship `+0x74`, 16 steps,
mask `& 0xf` (`docs/re/fire-and-ships.md` §3.3). Direction from a vector:
`dir16 = (trunc(θ · 16 / 2π + 0.5) + 4) & 15`, with 0 = −x, 4 = +z, 8 = +x,
12 = −z. Scenario y is world z. Fixture 9: (1, 0) → 8, (0, 1) → 4,
(−1, 0) → 0, (0, −1) → 12. The `+ 0.5` before `trunc` is the documented
rounding for positive angles. Exact halfway cases are that truncation; they
were not re-run on the original.

Manifest version 2 stores `facing` in `0..3`: 0 north, 1 east, 2 south,
3 west, chosen toward the map centre. Map +y is south (the southern
half-plane is the high y). Those four values are cardinal authoring. They
are not property `0x16`, and they are not a warehouse door (OQ-520).

**Proposal, not applied to the files.** Leave every version-2 manifest byte
as it is. A later manifest version stores `dir16` in `0..15` and drops the
overload. Until that version exists, loading a version-2 spawn converts
facing with the cardinal table in the same step that writes the figure:

| Version-2 facing | Map direction | `dir16` |
|---|---|---|
| 0 | north, −y | 12 |
| 1 | east, +x | 8 |
| 2 | south, +y | 4 |
| 3 | west, −x | 0 |

Diagonal headings have no version-2 encoding. Do not reinterpret an existing
0..3 value as a 16-step index, and do not rehash the candidates for this.

## 5. Positions

Original ship position is three floats, world `(x, height, z)` at object
`+0x14`, `+0x18`, `+0x1c`. The step adds `direction · (step / speed)` in
x87 (`0x100428d0`). Exact float reproduction is not the lockstep goal
(§3.5).

Version-2 spawns are integer milli-tiles: tile `(t, u)` centre is
`(1000t+500, 1000u+500)`. The manifest keeps those integers. The simulation
stores positions as `FixedPos` (Q24.8). The exact conversion, and the rule
that rejects a value which is not exact, are in
`docs/sim/ship-start-integration.md`. This note does not keep a second
position type.

The original's float-to-tile tie (a coordinate exactly halfway between two
milli-tiles, or exactly on a tile boundary in the x87 step) is
**unverified**. A future stepper documents its own integer rule and tests
that rule. It does not label the choice as the original's tie break.
Segment direction uses the `dir16` table in §4 when the source is a
version-2 facing. A future 16-step field uses the stored index directly.
Facing conversion happens when the spawn is loaded, not as a later repair.

No navigator is implemented in this packet.


## 6. Evidence base (MAP-NAV-1)

Tags: **ORIGINAL** = disassembled at an exact address with a validated function end (a `ret` followed by padding
or a table end), or executed under `unicorn` on the real module bytes; **DERIVED** = reading of validated code;
**UNKNOWN** = not resolved. Addresses are `AnnoWelt.dll` unless a module is named. Tools: `capstone` 5.0.7,
`pefile` 2024.8.26, `unicorn` 2.1.4, Python 3.14.6 in the private evidence environment (not committed); one-function
disassembly with `tools/disasm.py <module> <va>`. Modules (PE32, image base `0x10000000`): `AnnoWelt.dll` size
`0xF1000` sha256 `f220a878510f740c4cfb7224ade0a260e9a0fd7e99113e1201e7a37c0dfa6ae2`; `AnnoGame.dll` `0x2E000`
`ac721220afd27ebca5fd2ece6a8ff452289b9d7458c4e091c68adea2012db7bc`; `AnnoFrame.dll` `0xAB000`
`dd3f36ff564d2685d10795272c4ed3c0b2da32a946c2883fb1158ff439249b91`; `AnnoScene.dll` `0x48000`
`5b521f9541332846d32b57c93eaaee9c9d8de2ac2404cd75da0fe93e0e36fd38`; `CompLogic.dll` `0x72000`
`39b622b4a62d9814068619fd4254a675ed1a2a88d6b2594cb6a969c0216d11fa`; `FigurenHeader.dat`
`29fa1f0528fb337d9a61e8b73169281fd05bb27efa6df65638580a4c36c468a8`. The raw dumps and the emulation harnesses are
private (derived from the originals) and are not committed; every claim below carries its address.

Corrections to `docs/re/fire-and-ships.md` §3.2 (that note stays as the first pass): the ship `+0x54` is the
**current-segment pointer**, the list object is at `+0x4c` (head `+0x4c`, tail `+0x50`, current `+0x54`, count u16
`+0x58`, plan end xyz `+0x5c..+0x64`); a node is `+0` prev, `+4` next, then six dwords (velocity xyz, length,
speed, kind). The speed factor `+0x3c` is applied at segment start in `0x1003e930`, never in the stepper. `now`
is the world tick of 2 ms.

## 7. Movement, timing and facing rules (ORIGINAL unless marked)

### 7.1 Cadence
The world Advance (`0x100ae040`) runs islands, then timed events, then the unit registry tick `0x1009a8a0` with
the tick. The registry visits `k = clamp(floor(u16(now - last) * n / 50), 1, n)` unit slots per call from a
cursor (`0x1009aa2a..0x1009aac5`), `n` = the length of the whole unit vector (empty slots included), and calls the
unit's slot `+0x74(now)`. Ships are therefore processed in vector-slot order from the cursor; the collision hook
(section 7.4) makes ship results order dependent. A fresh registry has `n = 1` (`0x10098c80`, resize(1)), growth
1, 100, 125, 156, 195 (`0x10099130`, matches `docs/sim/state.md` A7); load is `Resize(count)` (`0x100980e0`) and
does not touch cursor or last tick (so a freshly constructed world loads with cursor 0, DERIVED). The multiplayer
step is 10 ms = 5 ticks at 100 % speed (`docs/re/tick-loop.md`, not re-read): `k = max(1, floor(n/10))` per
Advance, so a ship is visited about every 50 ticks when `n` is a multiple of 10; other `n` give other cadences
(`n = 7`: 35 ticks, `n = 25`: alternately 12 and 13 Advances). Single player: visit times follow frame time.

### 7.2 Integrator `0x100428d0(now)` (vtable `0x100c4da4` slot `+0x74`; 124 instructions, executed under unicorn)
```
t = f32( u16(now - last[+0x42]) * f32(0.2) )                 // 1 unit = 5 ticks = 10 ms
while speed[+0x30] != 0 and t != 0:
    P = t * speed
    if P <= remaining[+0x2c]: step = P;         time = t
    else:                     step = remaining; time = remaining / speed        // extended, not rounded
    pos += velocity * time                                                       // per axis, stored f32
    remaining = f32(remaining - step);  t = f32(t - time)
    if not (0 < remaining):                                                      // segment finished
        if state[+0x40] & 2:  speed = 0; state |= 4; notify owner                // "final" segment
        elif slot+0x17c(now) == 0:                                               // 0x10042210 hook
            if [+0x54] != 0:   0x1003e930(now)                                   // next node
            elif [+0x148] != 0: slot+0x178(now)                                  // order handler 0x100426f0
            else:              slot+0x174(now)                                   // idle 0x1003ef30
    last = u16(now)                                                              // end of every iteration
```
The segment fields are `direction +0x20/+0x24/+0x28`, `remaining +0x2c`, `speed +0x30`, `kind +0x34` (low nibble: 0 wait, 1 terminal,
2, 3 turn, >= 8 movement), `+0x38` (reset to 1.0 per segment). `last` is written only when the loop body runs. Leftover time at a segment end carries into the next segment
inside the same call; there is no snap to the segment end; arrival is `remaining <= 0`, so the arrival tick is the
first visit at or after the exact time. Zero-length segments finish at once. Idle (no path): `0x1003ef30` installs
`dir 0, remaining 1.0, speed 0.01` (500 ticks per cycle, re-armed); after load the segment is `remaining 0.01,
speed 0.01` (`0x10040000`).

### 7.3 Segment start `0x1003e930` (kinds >= 8: movement)
`want = dir16(node.vx, node.vz)` (`0x10030e20`); `d_signed = (want - facing) & 15`, `d = min(d_signed, 16 - d_signed)`.
**Turn phase iff `d >= 3`**: one step of 1/16 turn, direction -1 iff `d_signed >= 8` (a reversal turns -1), the
facing changes **immediately**, the segment becomes `velocity 0, speed 0.01, remaining = HEAD.f0 (type slot +0x64),
kind 3`, the node is **not** popped and `0x1003e930` runs again at its end; `d - 2` turn phases follow, then the facing
snaps to `want` (`d <= 2`: no turn). Turn duration `f0 / 0.01` stepper units (1502 `f0 = 0.11`: 11 units = 55
ticks ideal). Otherwise the segment is installed with `f = f32(+0x3c) * f32(+0x38)`: `velocity = f * node.v`,
`speed = f * node.speed`; if `1.0 + f32(1e-4) < node.length` the segment is **chunked**: `node.length -= 1.0`,
`remaining = 1.0`, node kept; else `remaining = node.length`, node popped. So the factor is re-read at least once
per tile. Kind 0 = wait, kind 3 = turn (sets facing from the node), kind 1 = terminal (flags `|= 0x202`, never
cleared), kind 2 = flags `|= 0x204`. Emulated on the real bytes (1502, `f0 = 0.11`, 5-tick visits): face 8 to -x
(reversal): facing 7, 6, 5, 4, 3, 2 then snap to 0, six turn phases (330 ticks visit-rounded).

### 7.4 Segment-end hook `0x10042210` (slot `+0x17c`)
Called at the end of every non-final segment. Stamps `[+0x72]`; if `(state & 7) == 1` it calls world slot `+0xcc`
(`0x1009d9b0`, arguments `ftol(x), ftol(z), 14, 5, owner`; only this call is guarded by that test). Unconditionally it queries
the unit registry (slot `+0x14`, mask `0xf9cf`, radius `type.slot+0x68(4)`, up to 0x100 entries). Type slot `+0x68(i)` (AnnoGame `0x1000fd00`, not AnnoWelt) returns `float [type+0x14+4*i]`, filled at
AnnoGame `0x10010262..0x10010352`: (0) `LOGIC[0x12]/8`, (1) `max(LOGIC[0x27]/8, slot 0)`, (2) `LOGIC[0x24]/8`, (4) `LOGIC[0x25]/8`
(0 means 7.0), (5) `LOGIC[0x28]/8` (0 means 2.0). A 1502 (`LOGIC[0x12] = 0x40`, `[0x24] = 0`, `[0x25] = 0x80`, `[0x27] = 0x40`,
`[0x28] = 0x28`, identical for 1501..1506) therefore has slot values 8.0, 8.0, 0.0, 16.0 (index 4) and 5.0 (index 5): the hook
radius is **16.0 tiles**. With two or more units (self counts), the avoid option set (`[+0x148] ? [+0x14a] : [+0x166]`) and
`state & 0x2106 == 0` it runs the avoid routine `0x10041bd0` (section 10.3), then, if `(state & 0x2007) == 1`, the
local replan `0x1003de40`. A third path (`0x1004235f`): if `state & 0x6806 == 0x800` (replan pending) it calls the wrapper
`0x1003dc20`, i.e. `0x1003de40(0, 0)`, even with fewer than two units. A non-zero result makes the integrator skip the
next-segment start. The integrator also frees consumed nodes when `[+0x44] + 0x32 < now` (`0x10042a13`).

### 7.5 Speed factor `0x100427b0` (slot `+0xa0`)
`L = 100`; if the owner's player property 3 (the Volk number, `+0x7a`, default 1; `0x1001bea0`) equals 1:
`L = 100 - trunc(17*S / ((n0+n1) * cap))`, `S` = sum of the first `n0` holds' amounts, `cap` = type slot `+0x84`,
`n0, n1` = type slot `+0x88(0), (1)`. `V = trunc((trunc(35*hp / maxhp) + 65) * L / 100)` with `hp` = signed 16-bit
`+0x6c`, `maxhp` = type slot `+0x90` (no zero guard); `+0x3c = f32(V * f32(0.01))` (36 distinct values 65..100 when `L = 100`; the set changes with the cargo term). It
reaches movement only at segment start. It is recomputed on hold add/sub/clear, hit-point set, repair, figure
load end, and on the founding payment's hold notification (`docs/re/ship-founding.md` 14.4). Founding reads the
ship state bit `0x02`, **not** `+0x3c`.

### 7.6 Facing from a vector `0x10030e20(dx, dz)`
`theta` from +z toward +x in `[0, 2pi)`; `dir16 = (trunc(theta*16/(2pi) + 0.5) + 4) & 15`. The function rounds
`dx/len` to f32 before `asin` and uses the decimal literal `3.141592654` for pi, so it is not exact at the 11.25-degree
boundaries: emulated on the real bytes, `(1.0, 0.1989123672246933)` (the f32 nearest tan 11.25 degrees) returns **7**
where exact mathematics gives 8. Vectors built by the route engine are multiples of 45 degrees and give exactly
`2 * dir8`.

### 7.7 State words
`+0x40` (u16; every immediate write in `0x10030000..0x10044800` was scanned): `0x0001` moving (set by kind >= 8, cleared by kind
0/3 and the idle hook); `0x0002` final (set by kind 1 and by hit points reaching 0; never cleared); `0x0004` set when a final
segment ends and by kind 2 (it suppresses the replan through mask `0x2007`); `0x0008` activity byte `+0x75 == 0x13`; units with
`0x000a` set are skipped by registry queries; `0x0020` HEAD byte 1 non-negative; `0x0100` initialised from `HEAD[0xa] & 1`
(0 for a 1502, 3 for the other five ships; the HEAD record binding is an assumption) and cleared on every SetOrder when `0x8000`
is clear, while set it skips the avoid branch; `0x0200` active-segment marker (set at `0x1003ec75`, in no hook mask);
`0x0800` replan pending: set at exactly one place, `0x1003e740` (`or word [ebp+0x40], 0x801`) after `0x1003de40` installs a
detour segment, cleared at `0x1003de9b`, `0x1003fbb2`, `0x1003ff1b`, `0x10040190`, `0x100426a1`, so it is clear in a fresh
order; `0x2000` and `0x4000`: no immediate writer in the ship range (setter UNKNOWN), they appear only in masks; `0x8000` is set
when the ship has **no** crew (derived at `0x10042a8f..0x10042b5f`, and again at `0x10041023`, `0x10041a0e`, `0x10041acd` (crew attach/detach), from crew byte `+0x7a` and a type-`0x61` crew test; the earlier
gloss "units aboard" was inverted). A move order is refused by `0x100a5f70` iff `state & 0x200e` is non-zero (for type groups
`0x6c..0x70`, not ships, it additionally needs a non-empty hold). SetOrder also has an order-id `0x13` branch (flags
`& 0xf0000 == 0x30000`, non-zero target) and a plain stop when `x = z = 0`; order id 1 is not unconditional.

### 7.8 Persistence and crew
Saved: the 16-byte `SCHIFF` prefix (facing `attr`, `f32` position) and a 16-byte HEAD record (byte 0 the avoid stance `+0x166`, dword `+4` = `+0x160`, byte 8 = `+0x7a`, hit points as `max(1, hp*128/maxhp)`
(so hp 0 reloads as 1), flags, tile cell `+0x7c/+0x7e`; saver `0x10042bd0`, loader `0x10042a40`). **Not saved:** the current
segment, the segment list, `+0x38`, `+0x3c`, `last`, the registry cursor. A ship that was sailing at save time
stands idle after load. Crew slots are 12 pointers at `+0x110..+0x13c`; attach (`0x10041900`) sets the unit state
`|= 0x80`, stops the unit (`slot +0x88(4,1)`, zeroing velocity/speed/remaining and the order nibble) and calls its slot
`+0x00` (a hold counter increment: `0x10040480` or `0x10049bb0`). The stepper, segment start, hook, idle, order handlers, planner and
stop contain no direct access to the crew array: the ship's position and facing are **not** copied to the crew each tick (a kind-1
terminal segment calls ship slot `+0x110` = `0x100432c0` at `0x1003eb17`, which frees the node list and the object at `+0x68`,
notifies `[+0x144]` and walks the crew through slots `+0xb8`/`+0xb4` and crew slot `+0x110`: the release path of a destroyed
ship, not per-tick movement); the scout (`0x10035f50`) and walker
(`0x10049200`) ticks return at once while their speed is 0, so an embarked unit neither moves nor idles. Any consumer
that needs a crew position must read the carrying ship.

### 7.9 PRNG
The world random table is reached only through the world vtable slot `+0xb4` (`0x100ccdd8 + 0xb4` = `0x100a1740`; there is
no direct call), so the evidence is an **indirect** `[reg+0xb4]` scan over the transitive callees of the stepper, segment
start, hook, speed factor, order path, route facade mode 1 (point goal, ends `0x100aa8f1`), coarse and fine search, Trace,
commit and converter, and the local planner: no CRT `rand` (`0x100b68a9`) and no world draw except the hit-point setter. The
facade's other mode draws in `0x100a9950` (via `0x100a9b20`, `0x100a99ef`: `div (n+2)`), which is not on the mode-1 path. Damage draws the world table in
the hit-point setter (`0x1003da10`, up to twice); wake foam uses CRT `rand` in the draw routine (presentation only).
UNKNOWN: `0x1003d616` (argument-count mismatch with `0x100a1740`).

### 7.10 Float versus fixed point (emulated on the real bytes)
Positions and velocities are f32, `time`, `P` and sums are x87 extended; precisions 24/53/64 differ by at most one f32
ulp in these voyages. A straight 60-tile voyage ends +0.0178 tiles (4.5 Q24.8 units) from the exact end at one-tick
visits and under 0.0003 tiles at 5..100 ticks. A Q24.8 velocity per tick is unusable (true values are 1.54..2.46 units
per tick; rounding is 7.6 % fast for 1502 and 30 % for 1506, truncation 84 % slow); a per-visit floor of the
fractional part is 84 % slow at one-tick visits and 2.4 % late at five. An exact rational carry, or positions finer
than 2^-16 tile, keeps the arrival within one visit of the original.

## 8. The move order (ORIGINAL)

**Player path (right-click).** A player's right-click order does **not** use net `0x6c`. WM_RBUTTONUP (AnnoFrame
`0x10008240`, kind 4) reaches the in-game frame's slot `+0x60` (`0x10006c60`, DERIVED that the live frame is a
tFrameGame), the per-frame poll `0x100052a0` calls the cursor `slot +0x70(4, mods, pick)` (`0x10005341`); the cursor
class tCursorInfo (vtable `0x10088b30`, handler `0x100502b0`, kind 3 right press sets a bit that kind 4 requires)
builds the hit list (`0x10050030`), converts it with `0x1004f7e0` and calls the net client **slot `+0xc0`**
("player command", SP `0x10059d60`, MP `0x1005f200` = net command **`0x84`**, packet `[u8 0x84][u8 kind][u16 player]
[u32 flags][data]`, received by `0x10060408 -> 0x100601e4 -> 0x1005f120`) at `0x10050af7`. A terrain hit gives kind 1
with the buffer `{mean(hit.f32[+4]), 0.0f, mean(hit.f32[+8])}` (y always 0.0; x and z are the raw pick point, not
snapped here); a unit hit kind 2 with ids; a tile hit kind 4 or 5. Flags are `[cursor+..] | 0x1000000`, `|= 0x10000` when
modifier `8` is down and `(flags & 0xf0000) == 0`, `|= 2` when modifier `0x1000` is down. The SP path then runs
`player.slot +0xd0` (AnnoWelt `0x1001b2a0`) -> the selection's slot `+0x1c`: a single selection (`0x1001a5d0`)
**checks that the unit is owned by the selecting player (`world.slot +0x158`) and in class group 6 or 7**, then stops
the unit (slot `+0xdc`) and calls SetOrder (`+0xe4`) with the **cursor-built flags** (the `0x6c` path passes 0); a
multi selection (`0x1001a770`) forwards to a group object (`slot +0x68(mode)`, mode 1 or `0xc` for a single member
for kind 1; group capped at 30 ships); the group body, and therefore the per-ship order and the order of ships in a
group, is UNKNOWN. The screen-to-world pick provider (`pick = [tFrameGame +0x50]`, `slot +0x94`/`+0x90`) was not
identified, so tile-centre versus sub-tile rounding of the click is UNKNOWN. No water/land test is made in the cursor.

**Multiplayer authentication.** The sender (`0x1005f200`) writes the player id from the player object it is given; the server
(`0x100553e0`) accepts command types `0x64..0xc7` from any client and queues them unexamined; the turn entry has no sender field;
the receiving client (`0x1005ffd0` -> `0x100601e4` -> `0x1005f120`) takes the player from the packet's u16 (`list.slot +0xc`; a null
result drops the command) and the selection ownership check compares the ship with the **claimed player's** selection. So the
original does not authenticate the player id against the connection (a modified client can forge it, DERIVED); the ownership
check bounds a forged command to ships the forged player owns. Whether the selection-setting kinds filter by owner, and whether
the transport drops unregistered senders, are runtime observations.

**Net `0x6c` path (host/script).** Net command `0x6c`, payload kind 1: `u16 figure id | u8 kind = 1 | f32 x | f32 y | f32 z` (y ignored). Single
player: AnnoFrame `0x100591f0` calls the ship's slot `+0xdc` (stop) then slot `+0xe4` (SetOrder `0x10040b00`);
multiplayer packs in AnnoFrame `0x1005d410`, turn dispatcher `0x1005ffd0`, table entry `0x6c` handler `0x1005d380`
(looks the figure up by id, null check). **There is no owner, sender or type check** and no water, bounds or
reachability check: SetOrder calls world slot `+0x194` (`0x100a6c80`) and, for a point target, `+0x178`
(`0x100a5f70`), which fails only when `state & 0x200e`. SetOrder writes the order record (`+0x148` id 1, `+0x149`
phase, `+0x14a` flag, `+0x14c` flags, `+0x150` x, `+0x154` z) and **does not build a plan**. The integrator picks the
order up when a segment ends and the node list is empty: slot `+0x178` (`0x100426f0`, `0x100424d0` order 1 to phase 1,
`0x100423a0`) calls `0x10030b90(order, 6.0f)`, which builds two request structs (world slots `+0x118`/`+0x11c`),
a search window (ship slot `+0x164`, `0x10040940`) and calls the route facade (section 9). Return `3` or `-2` is a
failed plan; anything else is success, including `2` (no points). A failed plan clears the order bytes and the ship
idles. Commit: `0x100a92a0` then world slot `+0x120` (`0x100a8bb0`) fills the ship's list (ship slot `+0x134`, `0x100a8c20`) and calls
ship slot `+0xd8` = `0x1003f910`, which installs the first node as the current segment with `remaining = 0`, sets the facing
directly to `dir16(first node)` (**bypassing** the `d >= 3` turn rule) and `state |= 1`. **No position field is written** (the
nodes are velocity and length, relative); the position snap at `0x1003fb9a` belongs to the local replan. **Stop** (slot `+0x168(1)`, `0x10042630`) zeroes `+0x148/+0x149/+0x14a`, sets `+0x54 = 0`, clears state `0x800`
and sets the plan end to the current position; list nodes are not freed. An in-flight segment is **not** aborted: its
remaining length is cut to the remainder past whole lattice steps (modulo 1.0, modulo 1.4142 for an odd facing) and
the new order is picked up at that arrival. Latency therefore includes the idle cycle (up to 500 ticks) and the
remainder of the segment. A second order runs stop then SetOrder. The arrival leaves `+0x148/+0x149/+0x14a` = 0,
`+0x54` = NULL and the reached position; **no arrived flag** is set (the founding path reads position, state bit
`0x02`, holds and owner, `docs/re/ship-founding.md` 8.3, 8.4, 12.2). Another command, net `0x6e` (client slot `+0x8c`,
SP `0x10059220`, MP `0x1005d5c0`, packet `[0x6e][u16 id|flags<<12][u8 B][u8 yq][f32 x][f32 z]`, `yq = ftol(y/0.04)`),
special-cases type groups `0x71` and `0x61` and looks like a placement/creation command; no UI sender of `0x6c` or
`0x6e` was found (a scan of every `call [reg+0x80]` / `+0x8c` site), the right-click uses `0x84` above.

## 9. The route engine (ORIGINAL; coarse expansion executed under unicorn)

### 9.1 Identity
The ship engine is the object at `[world + 0x6ab0]` (selected by `0x100a86c0` as `world + 0x6ab0 + 4*(req[0x15] & 1)`;
`req[0x15] = 0` on the order path; carts and land units use `0x6aa8`). Constructor `0x10012030` (`ret 8`), final
vtable `0x100c197c`, size `0x27e0`; sub-objects: coarse search S at `+4` (vtable `0x100c1a3c`), coarse block pool at
`+8`, fine map R at `+0x23a4`. The facade is world slot `+0x124` = `0x100a9ff0(req1, mode = 1, &{x, 0, z}, 2, req2)`
(mode 1 = point goal, `0x100aa84e`). Type 1502 (`BEWOHNTYPE` SHIP, group `0x71`) and all 12 SHIP types (343, 391,
1501-1510) have `LOGIC` byte `0x10` = 0, so they use **cost class 0** (class table at `0x100d1140 + class*7168`;
class 4 only for animals, 1511 is class 2). The SHIP -> `BEWOHNTYPE` 113 -> group `0x71` link is taken from `fire-and-ships.md` 3.1 (113 is a type-group number, not a figure id: figure 113 is a MAEHER); the `0x71` branch of `0x100a86c0` is consistent with it.

### 9.2 Static map: terrain
One scale: a vertex byte times 0.04 world units. A tile is **water iff `floor(sum of its four corner vertex bytes / 4)
<= 19`**, land (type 9) otherwise (threshold `ftol((1.28f - 0.5f)/0.04f) = ftol(19.5) = 19`, tile rule at `0x10090350` reading `[0x100e34d0]`, initialisers `0x1008fea0`, `0x1008ff00`, `0x1008ff40..0x1008ff62`; the coarse builder family `0x100a3b40..` holds the same numbers); a tile also
becomes blocked (9) where an island tile object's footprint reaches it (`0x10090350`, exact object semantics
UNKNOWN). The repo's `HEIGHT < 16` (sea level, 0.64) is stricter than this tile-average rule. The coarse map has
**8 x 8-tile cells** in 64 x 64-tile sectors (`[world+0x6aa4]` = array of 8-byte sector headers `{u16 x_origin, u16 z_origin,
pointer to 64 cell dwords}`, `[world+0x6a9c]` = sectors per row, `[world+0x6aa0]` = rows; a null pointer = open water `0xfff`; cell
dword = `u16 mask | u16 type`). The builder is world
slot `+0xf8` = `0x100aae00` (rectangle argument): per cell the island block records give `min`/`max` (halved vertex
extremes): `19 >= 2*max` -> `0xfff` (all water), `19 < 2*min` -> `0` (all land), else mixed: the 8 x 8 tile map is
packed by `0x100a4130` into a **12-bit edge-connectivity mask**. A second pass sets cost type 3 (**shore band**) for any
cell with an 8-neighbour whose height range straddles level 32 (`T2 = ftol(1.28/0.04) = 32` on doubled half-heights).
The only types ever in the coarse map are 0 and 3. **There is no whole-map build.** The world Init (`..0x100aad49`) allocates the sector array with every pointer NULL and calls
no builder. The builder is reached through world slot `+0x1d0` (thunk `0x100aba70` -> `0x100aae00(rect)`; six callers, all world
methods): the island-attach method `0x100a2b50` (rect = the island's bounding box; its only caller is `0x100af240`, world slot
`+0x1c0` Reset, which loops over all islands) and `0x100a26b0`, `0x100a2840`, `0x100a2e60`, `0x100a1da0` (two sites). The island
place/remove methods (`0x10082040`, `0x10082b10`) rebuild only the changed rectangle, and only when their flag argument has
`& 0x7000` equal to `0x2000` or `0x4000` (call site `0x10082afd`), or, at the second site (`0x10083802`), also when the object's group
`(((type slot +0x5c) - 1) & ~0x1f) + 1 == 0x141` (`0x100837e5..0x100837f1`); the exact object rule is read when the coast data is ported; the island loader passes `0x4000000`, so
load-time placement never rebuilds. The Reset itself is **not** on the load path: its only world caller is the multiplayer resync handler (AnnoFrame `0x100584a8`
in `0x100582e0`, net `0xd6`). At load the map is built **per island at placement, before the first tick**: scenario path world slot
`+0x88` (`0x100a24d0`) -> `+0x1cc` (`0x100a1da0`) -> `+0x1d4` (rect) and `+0x1d0` (builder) at `0x100a1fac` and `0x100a20c7`; savegame
path Load `0x100ae270` -> island loader `0x100a2ea0` (call `0x100ae6cb`) -> `+0x1d4`/`+0x1d0` at `0x100a2fd7`. **Vertex grid:** island Init (`0x1008c18a..0x1008c1d0`) allocates `(H+2) x (W+1)` bytes, zero-filled, stride `W+1`; the
HEIGHT chunk copy (`0x10084870`, slot `+0xac` of island vtable `0x100ca19c`) writes only the `W x H` samples, so
`vertex(x, y) = HEIGHT[x, y]` for `x < W, y < H` and **0** for column `W` and rows `H`, `H+1` (the repo's `VertexGrid` and
`kBoundaryVertexHeight = 0` follow the same rule). Consequence (DERIVED): the tiles of the last column and row read zero
vertices, which pulls their corner average down (more water at the island border).

### 9.3 Coarse search
Mask: bit `3*s + k` means water touching source edge `s` also touches target edge `t = (s + 3 - k) mod 4`; edges
0 = E (x == 7), 1 = N (z == 0), 2 = W (x == 0), 3 = S (z == 7); a region touching one edge sets no bits. Nibble
(bits 12-15, per query): 12 = W, 13 = S, 14 = E, 15 = N; the start tile's 4-neighbour water flood inside its cell
sets a bit per touched edge, the goal-disc centre cell gets `0xf`. Open set: red-black multiset keyed by u16 cost,
**equal costs are FIFO**, pop = leftmost; **label at discovery** (labels `0x80/0x82/0x84/0x86` = arrived W/S/E/N;
first discovery wins, no relaxation, no decrease-key); **no heuristic, no node or iteration cap**: the search floods
the window until the goal cell test succeeds or the open set empties. Moves are four-way: straight costs the
entered cell's orthogonal cost, a turn to either side costs `diag - orth` of that cell (3 in class 0), so a coarse
"diagonal" is an orthogonal step plus a turn through a corner cell. Discovery order by arrival label: `0x80`: N, S, W;
`0x82`: W, E, S; `0x84`: N, S, E; `0x86`: W, E, N; start cells: N, S, W, E. Entry test: the neighbour's mask must
contain a connection ending at the edge facing the current cell or its nibble flag (masks `0x2111` N, `0x8444` S,
`0x4888` W, `0x1222` E); exit test on the current cell: its (entry edge to exit edge) bit or the nibble bit; and the
class-table mask byte must contain the direction bit (W `0x01`, S `0x04`, E `0x10`, N `0x40`). Class 0 costs: type 0
10/13 (orth/diag), type 1/2 15/20 and 20/27, type 3 (shore) 50/69. Window: built by ship slot `+0x164` = `0x10040940`
(`ret 8`): a box centred on the ship's **start tile only** (the target is not part of it), `r = u16 [ship+0x6e]` (default `0x400`, stored by the base constructor `0x100405d0` at `0x10040623`; the `0x10044080` store is
another class's slot (a building's action radius) and no loader or HEAD record changes it for ships), x range `[max(0, tx-r), min(W, tx+r+1))`,
z likewise; `SetWindow 0x1000edd0` floors x0, z0 to a multiple of 8 and **rejects** a box with more than 32 blocks of 64 tiles
per axis (2048 tiles from the aligned origin; the engine does not set the window-valid byte, block creation returns NULL and
the query fails, the failure code -2 is DERIVED). An unclipped `r = 1024` box is 2049 tiles wide (33 blocks) and is rejected: it
is accepted only when the map edge clips it, so for maps up to 1024 tiles the box is the whole map, and a target more than 1024
tiles from the start lies outside the box.

### 9.4 Fine search, goal, learning, output
The fine level is Dijkstra at one-tile resolution inside the blocks of the coarse path plus the missing corner of each
2 x 2 turn, rebuilt from live island tiles on **every** query (it sees current buildings). 8 neighbours, discovery
order **NW, NE, N, SW, SE, S, W, E** (codes 7, 5, 6, 1, 3, 2, 0, 4; code 0 = (-1,0), 1 = (-1,+1), 2 = (0,+1), 3 = (+1,+1),
4 = (+1,0), 5 = (+1,-1), 6 = (0,-1), 7 = (-1,-1); z grows southward), orthogonal cost 10, diagonal 14
(`0x10014d40`), **no corner cutting**: a diagonal needs both adjacent orthogonal tiles enterable (land at N alone
removes N, NW, NE), visited tiles block themselves and the diagonals through them, FIFO ties, label at discovery.
Goal: the facade sets the acceptance radius 6.0; a candidate water tile of the goal-disc cells (radius 1 cell = 3 x 3
cells) scores `2304 - ftol(64 d^2)` with tile centre +0.5 and is offered only if the score is higher than the best
(so `d = 6.0` is rejected, `d = 5.9` scores 77) and only when `d < 6`; `d < 0.125` stops the search; ties go to the lower local ship
congestion (unit stamps, 5 x 5 kernel at `0x100c4ba0`, density table `0x100c15b0`). **The route engine's own unit query**
(`0x100a8c50`): registry slot `+0x14` (`0x100983a0`, `ret 0x18`) around the ship start with radius `type.slot +0x68(5)` (5.0 for
ships), class mask `0x80` (class group 7, ids 113..128; other queriers use `0xf14f`) and capacity `0x40`; the registry scans 16 x 16-tile
cells by z-cell rows ascending (outer), x-cell columns ascending (inner), then the cell's list order, keeps a unit if
`dx^2 + dz^2 <= r^2` (inclusive), `(state & 0xa) == 0` and its group is in the mask, returns `{unit, d^2}` unsorted and stops at
64 entries (later units are silently dropped). Consumer, in buffer order: skip a unit whose direction pointer (`slot +0x60` =
`unit +0x20`) is NULL or whose id equals `word [req1+0x18]`, stamp at `(unit.x + dir.x * 25.0, unit.z + dir.z * 25.0)` (`0x100c2214` = 25.0)
through the route engine's stamp call (sub-object vtable `0x100c1a3c` slot `+0x6c` = `0x1000f0e0`, thunk `0x10010870`; a ship querier
stamps unconditionally with kernel `0x100cccf4` and radius R = 2, other queriers only when the unit's state lacks `0x400`, kernel
`0x100ccce0`, R = 1). The stamp call **only appends** a record `{x f32, z f32, kernel*, radius u8}` (16 bytes at `S+0x1f98`, 64 records
max, cleared at the end of `SetWindow`); nothing is converted or clamped there. The kernels are the full 5 x 5 `0 0 1 0 0 / 0 1 2 1 0 / 1 2 3 2 1
/ 0 1 2 1 0 / 0 0 1 0 0` and the 3 x 3 `1 1 1 / 1 2 1 / 1 1 1`. The density is evaluated lazily per queried tile (`0x1000f120`, `ret 4`, the only reader
of the records): the stamp point is **truncated** (`ftol`, no 0.5 offset: 40.7 -> 40), the kernel cell of tile `(tx, tz)` is
`(tx - (sx - R), tz - (sz - R))` and is used only inside `0..2R`, values combine in record order through
`state' = [[0,1,2,3],[1,1,2,3],[2,2,3,3],[3,3,3,3]][state][kernel value]` (table `0x100c15b0`; a Python reference matched the real function on
about 7400 tiles). **Stamps change neither the coarse nor the fine costs**: both callers of `0x1000f120` are inside `Offer` (`0x1000f260`),
so congestion only changes which goal-disc candidate becomes the end node. **Goal callback and Offer, pinned (MAP-NAV-2 A3; reproduced by an independent Python transcription against the real functions
under emulation: about 1200 randomised cases, 0 mismatches in the winner tile and in the best score):** the callback computes
`score = 2304 - ftol(64 * d^2)` (dx stored as f32) and offers a candidate only if `score > best` (strict; `best` starts at 0, so d >= 6.0 is never offered; a candidate whose score equals the best of an **uncongested** stored candidate is not offered, while, as long as only congested candidates are stored, `best` stays 0 and equal-score congested candidates are offered). `Offer(rec, 1)` (`0x1000f260`, `ret 8`; the `arg >= 4` branch is dead for ships) returns 1
only for an uncongested candidate (density 0), stores it and the callback then **raises `best`**, stopping the search when
`score >= 2304` (d < 0.125). A congested candidate returns 0: it is stored if nothing is stored yet or its density is strictly lower than the
stored node's (density of the stored node recomputed), and **never raises `best`**. Consequences: free beats congested at equal score in
either offer order; free versus free at equal score: the first offered wins; congested versus congested: the lowest density wins even when
offered later, equal density keeps the earlier; with no free tile in the disc score and distance play no role (the first density-1 tile in
table order wins even at d = 5.1); a low-score free tile beats congested tiles of any score. The offer order is the coarse pop order of the
goal-flag cells times the candidate table `0x100c15d0` (5 groups of 64 (column, row) pairs; group `((label + 0x80) & 0xff) >> 1` for arrival
labels `0x80..0x87`, else 4): groups 0..3 sweep columns 7..0, rows 0..7, columns 0..7, rows 7..0, each line ordered 3,4,2,5,1,6,0,7; group 4
is the centre spiral (listed in `tools/ship_nav_ref.py`). The loop does not stop early: every reachable cell of the 3 x 3 goal disc is tested
unless an exact uncongested hit stops it. 16 explicit cases are in `fixtures/synthetic/sim/nav/offer_cases.txt` (the pop order is an input).
**Object heights and blocked tiles (`0x10090350`, executed with the real AnnoGame height function `0x10007f40`):** every island tile object can block;
there is no class or flag test. A tile becomes land (9) iff `((h + rec[4]) & 0xff) > 19` (an 8-bit unsigned add: a negative `h` wraps and
blocks), `h` = the object type's height from AnnoGame slot `+0x90` (`floor(HEAD f32@0x14 * 25)` without a height map; with a map
`2 * signed byte` of the rotated map, in 0.04 units; only 11 types have a map: 1020, 1021, 2701-2708, 2740), `rec[4]` = the placed item's height
byte. The block min/max records (`0x10084190`) raise an object top only on the anchor tile (rec[3] bit 7), computed as `(ftol(f14 / 0.04f) + rec[4]) & 0xff`
(a division, unlike the exporter's `* 25`, which differs for 24 types), stored as `max >> 1`, `min >> 1`. **x87 precision matters:** at 24-bit
precision 29 types get +1 (f14 = 0.64, 1.4, 2.6, 2.8, 3.6, 4.6); the live control word is UNKNOWN (CAP-2); the sim uses the exact
floor of the f32 value times 25 (the 53/64-bit result). Which objects trigger a coarse rebuild: the type flag word (u16 at HEAD `+0x1c`) bit
`0x2000` (coast; 30 types incl. 810, 821, 1020, 1021, 1302/1304/1306, 1412-1417, 3018) or `0x4000` (road/bridge; 35 types), plus type ids
`0x141..0x160` at the removal site; mines (`0x3000`), mountains (`0x1000`), houses, farms and walls do not. The `flags` argument of the place
method at runtime is UNKNOWN (the loader passes `0x4000000`). Synthetic checks: an h = 19 object on water does not block, h = 20 does
(`objtile` vectors); an h = 20 object wall routes exactly like a terrain wall of vertex height 20. **Shore band** (type 3, class 0 cost 50 orthogonal /
69 diagonal against 10/13): a band cell costs +40 per orthogonal step and +19 per turn into it; the engine leaves the band when an open row costs
less than 40 per crossed band cell and crosses it when the detour costs more; equal costs are decided by FIFO order (cases in
`object_shore_cases.txt`; the builder rule that creates type 3 was not re-executed). Worked examples on 64 x 64 open sea (start (4.5, 20.5), goal (40.5, 20.5), radius 6): no stamp E10 E10 E10 E6; a ship exactly at
the goal NE1 E10 E10 E10 E3 (end tile (38, 19)); a ship at (38.5, 20.5) NE1 E10 E10 E10 E5; the committed fixture `route_runs.txt` holds all
of them. The registry cells are lists with the newest unit appended: within a cell the scan is oldest first, and a unit that stays
in its cell keeps its place (the update re-appends only when the cell changes, registry slot `+0x2c` = `0x10098200`). A goal on land within 6 tiles of
reachable water routes to the nearest candidate; farther returns -2. **Retry loop:** if the fine search cannot pass a coarse
cell, `0x10013fb0` clears the connectivity bit "entry edge of the previous cell to the exit edge towards the failing
cell" (the start cell clears nibble bits), up to **8** learned masks per query (list reset every query); a `-1`
result or the cap gives status -2. A start tile on land works if water is adjacent (the fine start byte is `0xa`
regardless of terrain); a start walled in by land floods nothing and ends in -2. **Output:** `Trace`
(`0x10011640`) emits runs of (direction 0..7, length <= 10) from the start tile centre; `0x100a8c50` converts them
with `0x1002fde0`: step 1.0 per unit for ships, speed `HEAD.f1 * 1.0`, diagonals `0.7071068` per axis and length
`1.4142135` per step, one kind-`0x403` turn segment per 1/16 heading change, one kind-`0x408` movement segment per
run. **Return codes** (mode 1): 0 plan committed; 2 search succeeded but Trace gave 0 runs (start == goal), no commit,
`0x10030b90` still returns success; -2 `Run()` found nothing (no candidate within 6.0, or the learn cap/-1); 3 is not
produced by the engine. There is no route cache.

### 9.5 What this settles for the candidates
Diagonal contact: a diagonal fine step is refused when either adjacent orthogonal tile is land, and a coarse cell
reaches a neighbour only through the water-connectivity masks, so two land corners touching diagonally **do not
connect** water (a diagonal-only gap is unreachable, status -2): this agrees with exterior-4 for that case. **Clearance:**
the route engine has **no hull or shore dilation** (a one-tile channel is passable); shore cells cost more (type 3:
50/69 versus 10/13 at the coarse level) but are not forbidden. The founding hull box shares no code with navigation
(it uses AnnoFrame `0x10045080`/`0x100454d0`, a registry rectangle query and hull property 5; navigation uses a radius
query, the unit-stamp kernel and thresholds 6/25/64, and never reads property 5, also not in the avoid routine
`0x10041bd0`).

## 10. The local detour planner and ship-versus-ship (ORIGINAL)

### 10.1 What it is
`0x1003de40` (`ret 8`, args: unit list, count; 695 instructions to `0x1003e784`) is a ship method with two call
sites: wrapper `0x1003dc20` and the hook `0x10042210` (section 7.4). It runs at a segment end with two or more units
near, or when the replan-pending bit `0x800` is set (7.4); there is no timer or distance trigger. It is **not** a global router: the ship's float
segment list is the route; `0x10030660` walks at most 12.0 world units of it and builds a window (box padded by 3.0).
Planner object: `ship+0x10` is the world (stored by `0x10040240` for class id `0xf00`); world slot `+0x20(0xc00, 3)`
(`0x100aee00` in vtable `0x100ccdd8`, DERIVED that this is the running world's vtable) returns `world + 0x6ab4`, final
vtable `0x100c18b4` (slots `+0x50` SetWindow `0x1000db60`, `+0x68` MarkStart `0x1000dd60`, `+0x6c` StampUnit
`0x1000e3f0`, `+0x74` MarkGoalDisc `0x1000e0c0`, `+0x7c` SetCallback, `+0x80` SetClass, `+0x84` Run `0x1000e800`,
`+0xa4` Trace `0x1000e8f0`).

### 10.2 Representation and rules
A window of at most 16 x 16 blocks of 8 x 8 tiles (128 x 128 tiles), one u16 per tile (low byte 0 free / 7 land,
building or start / `0x80|dir` parent code; bits 8-14 density 0..3 from other ships; bit 15 goal flag), blocks created
lazily from the world (`world.slot +0xec` kind 11, `0x100abaa0`); land test as in 9.2; **no hull or shore
dilation**. Other ships within 6 tiles (d^2 <= 36) of group ids 113..128 or 4, not self, at most 0x80, are stamped as a
5 x 5 kernel (`0 0 1 0 0 / 0 1 2 1 0 / 1 2 3 2 1 / 0 1 2 1 0 / 0 0 1 0 0`) through the density table; land and density 3 are
impassable. Search: 8 neighbours, discovery order NW, NE, N, SW, SE, S, W, E (executed under unicorn), costs 10/13
(class 0, density 0), 30/39 (density 1), 40/52 (density 2), no corner cutting, FIFO ties, label at discovery. Goal: the
nine tiles of a 3 x 3 square (radius 1, independent of the sub-tile position) around the point 12 units ahead on the
path; score `64 - ftol(64 d^2)`; a density-0 tile with a strictly higher score wins, score >= 64 stops. Trace
run-length-encodes the parent codes (runs capped at 5); only the **first** run is used: one straight segment from the
current float position to the centre of the tile `len` steps away. The detour is rejected if `dot(new, old) < -1e-4`.
On failure the ship stops (`remaining` 0.5 or 0.1, speed 0.01); the 0.1 branch sets `[+0x54] = 0` and cancels the route.

### 10.3 Avoid routine `0x10041bd0` (`ret 8`, ends `0x100421b9`)
Acts only on **other owners'** ships and land units (class group bits `0xc0`) whose relation code from
`owner.slot +0x7c(2, other)` is 2 or 3 (hostile, DERIVED), skipping units with `flags & 0x8100 == 0x8100`; it sorts by
distance squared, keeps at most 4 records, runs a local planner on a 25 x 25 window (stance byte 3) or 17 x 17 and
installs one run with a turn phase if needed. Stance `+0x166` defaults to 2, set by slot `+0x16c`. It does not read the
hull half-extent. So friendly and own ships are handled only by the density stamps of 10.2. UNKNOWN: the world slot
`+0xcc` effect inside islands (`island.slot +0x130(x, z, 14, 5, owner)`), the path-rejoin formula `0x10030100`.

## 11. Passage: original, derived and proposed (small authored examples)

| Case | exterior-4 (tool model) | Original (section 9; emulation) | Proposed contract |
|---|---|---|---|
| open water | reachable | reachable; runs of the fine path, status 0 | same |
| enclosed lake: sea to lake or lake to sea | not exterior | unreachable (-2) | order accepted, later `RouteFailed{Unreachable}` (not an atomic refusal) |
| voyage entirely inside an enclosed lake | not exterior | reachable (DERIVED) | succeeds |
| diagonal-only gap between two land corners | closed | unreachable (-2): the diagonal needs both orthogonals, masks do not connect | closed (same) |
| one-tile channel | open (4-connected) | passable (no hull dilation); shore cost 50/69 at coarse level | passable |
| wider channel | open | passable | passable |
| concave coast / bay | open | reachable; coarse learning may take up to 8 retries | same algorithm |
| goal on land within 6 tiles of reachable water | invalid endpoint | routes to the nearest water candidate (score 2304 - ftol(64 d^2)) | same (documented) |
| goal on land farther than 6 | invalid | -2 | -2 |
| goal = start tile | trivial | status 2 (no runs), order consumed | order accepted, no movement |
| ship-to-ship obstruction | not modelled | route engine: stamps only change which goal candidate is chosen (no cost change); the local detour planner (segment ends) uses density costs x3/x4 and blocks density 3; hostile owners trigger the avoid routine | stamps at planning time; local detour and hostile avoidance deferred |
| land definition | `HEIGHT < 16` per tile | floor(corner average) <= 19 per tile + building footprints | the original rule |
Original-column results for the coarse and end-to-end rows come from executing the original coarse expansion,
mask packer and start-nibble functions under unicorn on hand-built maps; the fine stage and goal bookkeeping in those runs
are a composition of the documented rules (DERIVED), and the fine stage was verified under original code for interior tiles and block edges (second static round). UNKNOWN is never read as a passing route.

Consequence for the three candidates: the candidates' geometric witnesses are exterior-4 tool evidence. The original
rules agree with exterior-4 on diagonal-only gaps and enclosed water, are more permissive on one-tile channels and
shore proximity (no hull clearance) and use a different land definition (tile average, not per-tile `HEIGHT < 16`), so
a witness that depends on a one-tile or shore-hugging channel, or on a coast tile whose corner average exceeds 19 while
its `HEIGHT` is below 16, must be re-evaluated with the original land rule once the route query exists. Placements and
pass labels are unchanged here; geometric witnesses remain tool-only until then.

## 12. Unresolved (with the runtime observation that settles each)

1. The screen-to-world pick (tile centre versus raw sub-tile), the group object's `slot +0x68` body (order of ships in a
   group order) and whether tCursorInfo is the active cursor in play. Settle: breakpoints AnnoFrame `0x1004feb0` (raw pick
   vec3 at `[esp+0xc]`), `0x100502b0`, `0x10059d60`, and AnnoWelt `0x10040b00` (SetOrder, log ship ids for the group
   order); right-click water at four sub-tile offsets.
2. x87 precision control at runtime: no `_controlfp`/`fldcw` setup located in the exe/DLL init. Settle: `fnstcw` at a
   breakpoint on `0x100428d0` (`0x027f` = 53 bit, `0x037f` = 64 bit, `0x007f` = 24 bit).
3. The object-top contribution to block extrema and to tile type 9; the setter of state bits `0x2000`/`0x4000`. Settle: break at
   `0x100aae00` during a scenario start and a savegame load (expect one call per island and none from Reset) and dump
   `[world+0x6aa4]` sector words. (The fine
   stage at block edges was run on the original bytes: 4400 randomised corridor cases and 600 lazy-block cases against a Python
   transcription, 0 deviations from the interior rule; the vertex-grid padding is settled in 9.2.)
4. Registry cursor and last-tick at load (read `[reg+0x20]`, `[reg+0x22]` before and after a load).
5. Contents of the goal disc table for radius 1 cell beyond the 3 x 3 result, bodies `0x10012310`, `0x10015a30`.
6. The meaning of relation codes 2/3 in the avoid routine, state bits `0x4/0x8/0x100/0x200/0x800/0x2000/0x8000`, and the
   path-rejoin formula `0x10030100`.
Windows original-runtime observations are specified in `docs/testing/ship-navigation-capture-card.md`.
