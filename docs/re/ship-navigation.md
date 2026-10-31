# Ship navigation and the geometric sea model

**Status:** partial. The version-2 sea rule stays a tool model. One
investigation follows the ship replan already named in
`docs/re/fire-and-ships.md` §3.2. It does not replace that note, and it does
not implement a navigator.

## 1. The geometric model, kept as a tool

`map2-ship-start-v2` uses exterior-4 water: a water tile is `HEIGHT < 16`,
and exterior water is the 4-connected component that touches the world
border. Ships and founding approaches must lie in it. Corner contact does
not connect a lake to the sea. The route is 4-connected, one tile per step,
no diagonal step, spawn clearance only, no hull. Reports label it
`sea: exterior-4`. It is not a hashed policy field.

That model answers: "is there a 4-connected water path on this height mask?"
It does not answer: "can the original ship sail it?"

## 2. Navigation investigation

**Unknown this pass was required to resolve:** does the original ship treat
diagonal water contact as passable, and what clearance from land does the
hull use?

**Where the existing note stops.** `docs/re/fire-and-ships.md` §3.2: movement
is a list of straight float segments. Local replan is `0x1003de40`. It
collects units within a radius and calls a planner object. The grid or graph
used for open water was not found. Facing is 16 steps of 22.5° (§3.3).
Positions and the step are x87 floats (§3.5). Land carriers and carts use
other layers (`docs/re/transport.md`); they are not this navigator.

**What this pass added.** On this build `0x1003de40` is in `AnnoWelt.dll`
(image size `0xF1000`; the address is inside that module). The function
matches the documented shape: a large frame, float constants, position data
at `ebp+0x14`. Its first callee, `AnnoWelt.dll` `0x10030100`, subtracts
float coordinates (`fld` / `fsub` of a point and of `ebx+0x10` / `ebx+0x18`).
That is a float geometric test, not a read of the `HEIGHT` grid and not a
4-connected tile step.

The planner methods named in §3.2 (`+0x50`, `+0x68`, `+0x6c`, `+0x74`,
`+0x7c` on the object from slot `+0x20`) were not opened. Diagonal passage
and hull clearance therefore stay **unknown**. A channel that exterior-4
rejects, or accepts, is not classified as original ship passage.

## 3. Four meanings of a narrow channel

| Name | Meaning | Status on the candidates |
|---|---|---|
| Boundary-connected water | Exterior-4. A corner touch is closed. | Defined. The v2 checker uses it. |
| Geometric tile route | 4-connected steps on that water, cost 1, no hull. | Defined. The v2 route. |
| Original ship passage | The unread planner, plus the float segment stepper. | Unknown. |
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

**Proposal, not applied.** Leave every version-2 manifest byte as it is.
A later manifest version stores `dir16` in `0..15` and drops the overload.
Until that version exists, the only conversion used when a version-2 spawn
is loaded is the cardinal table:

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
`(1000t+500, 1000u+500)`. That integer is the proposed authoritative
position.

**Conversion contract, proposed.**

- Authoritative state is signed integer milli-tiles for x and y, and a
  separate integer for height if a later trace needs it. The original's
  float height is not copied into the checksum.
- Loading a manifest copies the milli-tile integers. It does not parse them
  through a float.
- A display conversion may compute `milli / 1000` in the renderer. That
  value is not written back.
- The original's float-to-tile tie (a coordinate exactly halfway between two
  milli-tiles, or exactly on a tile boundary in the x87 step) is
  **unverified**. The implementation picks one documented integer rounding
  for its own stepper and tests that choice. It does not label the choice
  as the original's tie break.
- Segment direction uses the `dir16` table in §4 when the source is a
  version-2 facing. A future 16-step field uses the stored index directly.

No navigator is implemented in this packet.
