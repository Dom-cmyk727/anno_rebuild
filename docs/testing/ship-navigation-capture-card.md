# Original-runtime capture card: ship navigation (MAP-NAV-1)

**Status: NOT_RUN.** These observations need the original game on Windows (32-bit x86; `AnnoWelt.dll`, `AnnoFrame.dll`
from the GOG "A.D." 2.0.0.5 build, hashes in `docs/re/ship-navigation.md` section 6). They are a **queued request**:
nothing here asks the rendering executor to interrupt its current assignment, and no simulation work waits for
a result unless the contract section named in "Resolves" says so. Instrumented means a debugger/hook reading the named
address; visual means a person watching the game and is only acceptable where the card says so.

Common setup: original install unmodified; a single-player scenario that contains a 1502 ship (record the scene file
and the ship's figure id; `tools/figuren.py` lists types); game speed 100 %; record the build hash of the DLLs used. The modules load at their preferred base `0x10000000` unless the debugger reports another base (rebase every address
by the difference and record it). Output for every card: one text file `capture-<id>-<date>.txt`, a first line with the DLL
hashes and the scene, then rows of **tab-separated** fields in the order named in the card (the `|` in the templates below
marks tab groups). Entry values on the stack are read at function entry: `[esp]` is the return address, `[esp+4]` the first
argument; for `0x100428d0` (`ret 4`) `now` is `[esp+4]`. Prefer a debugger with scripted breakpoints (x32dbg or WinDbg); `fnstcw`
is a read-only instruction.

| ID | Observation | Kind | Resolves | Priority |
|---|---|---|---|---|
| CAP-1 | the move order a right-click issues | instrumented | contract section 2 target rounding and group order (re 12.1) | 1 |
| CAP-2 | the x87 control word during movement | instrumented | arithmetic adaptation, contract 4 (re 12.2) | 2 |
| CAP-3 | the coarse map after load; registry cursor at load | instrumented | contract 5 (static coast data build), 6 (cadence at load) (re 12.3, 12.4) | 3 |
| CAP-4 | a straight voyage: positions at visit ticks | instrumented | contract 4 exact-rational bound | 2 |
| CAP-5 | a reversal turn: facing and ticks | instrumented | contract 3 turn timing, cadence | 3 |
| CAP-6 | channel passage witnesses | instrumented + visual | contract section 5 (no hull clearance), candidate witnesses (re 11) | 1 |

## CAP-1: what a right-click on water sends
Setup: one 1502 selected, open water; mark four click points with a one-pixel grid reference (zoom the camera to the
closest level and record the pixel of each click relative to the tile corner seen in the editor/overlay of the same
tile). Breakpoints (AnnoFrame): `0x1004feb0` (the pick callback; read the vec3 pointer at
`[esp+0xc]`), `0x100502b0` (cursor handler; kind/mods/pick at `[esp+4], [esp+8], [esp+0xc]`), `0x10059d60` (SP player command, `ret` size unknown: read the five arguments `(player, kind, buf, count, flags)` at
`[esp+4]..[esp+0x14]`, then the f32 triple at `buf`), AnnoWelt `0x10040b00` (SetOrder). Right-click the same tile at
four sub-tile positions (corner, edge, centre, off-centre) and once with the modifier held. With three ships selected
issue one order and log the ship ids in the order `0x10040b00` receives them (the group order).
Rows: `click_px_x click_px_y mods | pick_x pick_y pick_z | cmd_x cmd_y cmd_z flags | ship_id`.
Discrimination: cmd_x/cmd_z equal to the pick (raw sub-tile) versus tile centres (+0.5) versus truncated; the group id
order versus selection order. Falsifies: contract section 2 (the sim takes exact targets and the UI layer chooses the
rounding) only if the order carries something other than a point (then the target type must change). Visual observation is
not sufficient.

## CAP-2: x87 control word
Breakpoint AnnoWelt `0x100428d0` once while a ship moves; execute `fnstcw` into a scratch word (or read FPU CW from the
debugger). Row: `cw=0x????`. `0x027f` = 53-bit, `0x037f` = 64-bit, `0x007f` = 24-bit mantissa. Discrimination: which
precision the original's position accumulation really uses (the static emulation shows at most one f32 ulp difference).
Falsifies: contract section 4's statement that the exact-rational result is within one visit of the original, only if the
result is not one of the three values.

## CAP-3: coarse map and registry at load
(a) After loading a known scenario, dump `[world+0x6aa4]` (sector headers, 8 bytes each, rows `[world+0x6a9c]`,
columns `[world+0x6aa0]`) and, for sectors with a non-null pointer, the 64 dwords. Compare with a second dump after placing
a harbour-class building (or removing one) near the coast. Rows: `sector_x sector_z cell_index word` (`word` = `u16 mask | u16 type`). Falsifies: contract section 5, which
assumes static coast data is built once per island and rebuilt only for changed rectangles. Discrimination: whether
a full build happens at load and whether placement rebuilds exactly the placed rectangle (re 9.2). (b) Read `[reg+0x20]` and
`[reg+0x22]` of the unit registry (`world+0x114`) immediately before and after a load into a fresh process, and the vector
length `[reg+0x14..0x18]`. Rows: `when cursor last_tick n`. Falsifies: the contract's cadence rule for a loaded world if the cursor is not 0 after load.

## CAP-4: a straight voyage
Order a 1502 along an axis over open water for about 60 tiles. Hook AnnoWelt `0x100428d0` entry and exit: log at every
call `now` (world tick), the float position `+0x14, +0x1c`, `remaining +0x2c`, `speed +0x30`, `last +0x42`. Rows:
`now x z remaining speed last`. Repeat in single player at the default frame rate and, if possible, with an artificially
fixed frame time. Discrimination: end offset and arrival tick against the contract's exact rational result (static
emulation: +0.0178 tile at one-tick visits, below 0.0003 at 5-100). Tolerance asked of the contract: the sim arrival tick equals
the original's within one visit interval and the end position within 0.02 tile of the original's float result at the same
cadence; a larger gap falsifies contract section 4. The visit cadence per call is the key output. To fix the frame time use
the multiplayer-style fixed step (a LAN game started alone) or record the real call intervals; do not assume a rate.

## CAP-5: a reversal
Face a 1502 east, order a point due west. Same hook as CAP-4 plus the facing byte `+0x74`. Rows: `now facing[+0x74] remaining[+0x2c] speed[+0x30] kind[+0x34]`. Expected from the static analysis: facing 7, 6, 5, 4, 3, 2 then 0, six turn phases of `f0/0.01` units. Discrimination:
the turn phase count, direction (-1 for the reversal) and duration under the real cadence. Falsifies: contract section 3
(`d >= 3` turn rule, reversal direction, `f0/0.01` duration).

## CAP-6: passage witnesses (instrumented; visual only as a supplement)
Author three small scenario maps (or use the editor) with the exact land lists of `fixtures/synthetic/sim/nav/passage_cases.txt`
cases `one_tile_channel`, `diagonal_only_wall`, `land_goal_5_9_inland`/`land_goal_6_0_inland` and `start_on_land_next_to_water`,
translated into the island's tile grid so that each wall edge lies **inside** an 8 x 8 coarse cell and again once on a cell
boundary (multiples of 8) because the coarse masks differ there; record the placement. Put one 1502 at the case's start and
order a move to the goal point. Breakpoint AnnoWelt `0x10030b90` at its `ret` (the `ret` of that function; the value in `eax`
is its result) and AnnoWelt `0x100a9ff0` at its `ret` (the facade status: 0, 2, -2); also read the ship position after the
order settles. Rows: `case placement facade_status ship_x ship_z reached(Y/N)`.
Expected from the static analysis: one-tile channel status 0 and passes; diagonal-only wall -2; land goal 5.9 tiles inland status 0
ending at the nearest water tile, 6.0 tiles -2. Falsifies: evidence 9.4/9.5 (and contract section 5): a closed one-tile channel
means a hull clearance exists; an open diagonal gap means the corner rule is wrong; any other goal behaviour means the 6.0 rule
is wrong. Report such an outcome before the contract is implemented.

## CAP-6c: is a land start tile a goal candidate?
Start the ship on a land tile (editor-placed) next to water and order a move to exactly that tile centre. Rows as CAP-6. The two
possible answers are status 2 (the start tile is offered) or status 0 at the adjacent water tile (score 2240 for a neighbour at
distance 1). The contract does not assert either; the answer fixes the `start == goal` rule of the route query.
