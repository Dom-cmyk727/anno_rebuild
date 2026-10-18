# Tick loop, timing, randomness and multiplayer sync

**Status:** first pass (2026-10-17), static analysis only. Modules (GOG 2.0.0.5, image base `0x10000000` each): `AnnoFrame.dll` (main loop, net client/server), `AnnoWelt.dll` (world clock, simulation PRNG), `AnnoScene.dll`, `AnnoGame.dll`, `CompLogic.dll`. Tools: Ghidra 12.1.4 headless (`tools/ghidra/DecompileAllPlus.java`; `CompLogic.dll` decompiled for this note), `objdump -d`, vtable owners from MSVC RTTI (`AnnoFrame.dll` only; `AnnoWelt`, `AnnoScene`, `AnnoGame` have no RTTI). Addresses are in the module named in each section. Behaviour paraphrased, no code copied.

Confidence as in [`annodisplay.md`](annodisplay.md): **confirmed** = read in the decompiled or disassembled function; **likely** = strong inference, not traced end to end; **guess** = hypothesis, repeated under "Open".

Short answer: the game loop is frame-coupled, the simulation is not stepped in fixed ticks. Each step advances a millisecond world clock by a variable amount. Single-player takes that amount from the frame time. Multiplayer takes it from a fixed 10 ms step paced by server turns (100 ms, 10 steps each). Single-player calls the world directly, without messages. The simulation PRNG is a 2048-entry table whose cursor is set from a seed the host sends. Multiplayer compares MD5 hashes of a serialized state snapshot and can resync from the last agreed snapshot.

## 1. Main loop and frame time (`AnnoFrame.dll`)

- **In-game loop** = `tFrameGame` vtable `0x10086428` slot `+0x68`, function `0x100087f0`. While no window message is pending, each pass: (1) `dt` = whole milliseconds since the previous pass from the frame timer at `+0x16d8`, (2) if `dt > 1000`, the timer is reset and `dt = 1000`, (3) `Update(dt)` (slot `+0x9c`), (4) `Draw(dt)` (slot `+0x6c`) and present. It never calls `Sleep`, so it renders as fast as it can. Pending messages are handled with `GetMessage`/`TranslateAccelerator`/`DispatchMessage`. **Confirmed.**
  - Inactive or background branch: when the flag at `+0x1708` is set and the one at `+0x1704` is clear, only `Update(dt)` runs between messages. This branch is taken only when the net client's slot `+0xfc` is non-zero (multiplayer, §4). Otherwise the loop blocks in `GetMessage`. **Confirmed** (control flow). That single-player therefore stops while the window is inactive is a **guess** (meaning of the flags not traced).
  - At game end it runs Update and Draw for another 5000 ms before returning. **Confirmed.**
- **Frame timer** (`0x10070a20`…`0x10070bf0`, identical copy in `AnnoWelt.dll` `0x100b50c0`…`0x100b5290`): a `{QPC frequency, last mark}` pair. `0x10070a60` returns the whole milliseconds since the mark and moves the mark forward by exactly that amount, so sub-millisecond remainders carry over and the clock does not drift. `0x10070b10` is the same with any unit per second (used with 1000 and 10000). **Confirmed.**
- `QueryPerformanceCounter` is the only clock that feeds the simulation. `GetTickCount` is used for UI timers (`0x1000ca90` message timeout, `0x1000ccd0`), for `srand` seeds (§3) and in `View.dll` (UI). `timeGetTime` is not imported by any module. **Confirmed** (import tables in `analysis/pe_symbols.json`, call sites).
- Startup and lobby frames use a similar loop with `Sleep(2)` per pass (`0x10019750`, `tFrameStartupBase` slot `+0x68`; `0x10020230`, `tFrameStartupKampangeBB`), also clamped at 1000 ms. **Confirmed.**

### Per-frame update (`tFrameGameFull` slot `+0x9c` = `0x1000ca90`)
Order of calls with the same `dt` (**confirmed**): UI and event objects (`+0x18b0`, `+0x18b4`, `0x1003ea80`). Then the **net server** at `+0x18ac` (slot `+0x58`), present only on a multiplayer host and stored by slot `+0x28` = `0x1000cfb0`, which accepts class id `0x1d00`. Then the **net client** at `+0x20` (slot `+0xf4`), which advances the simulation (§2). The client call is timed in 100 µs units into `+0x16f4` (profiling). Then every player object (slot `+0x11c`). After that, a 600,000 ms real-time accumulator (`+0x1e80`) calls `0x10006310` when the net client's slot `+0xfc` returns 0. `0x10006310` builds a file name from the scenario name and excludes three scenario GUIDs; this is **likely** the single-player autosave every 10 minutes of real time.

## 2. How the simulation advances

### World clock (`AnnoWelt.dll`)
World object vtable `0x100ccdd8` (base `0x100cc92c`, constructor `0x100af3e0` → `0x100a3980`):
- **`Advance(ms)` = slot `+0x84`, `0x100ae040`** (**confirmed**). It adds `ms` (sign-extended) to a 64-bit millisecond counter at `+0x100/+0x104` and sets the **world tick** `+0x10c = counter >> 1`, so **one tick = 2 ms of game time**. With that tick it runs, in order: all islands (`0x100a2e60`: every island in the table at `+0x6530`, count `+0xf4`, island slot `+0xe4`); the timed-event queue (`0x100add90`: pops entries whose due tick ≤ now, kinds 0/1); the sub-systems at `+0x114` (slot `+0x20`), `+0x118` (slot `+0x24`) and `+0x69c8` (slot `+0x14`); `0x1009d2a0(ms)`; `0x100ae140(ms)`. At the end it copies the new tick to `+0x108` (`0x100ae126`/`0x100ae12d`), so between advances `+0x108` equals `+0x10c`; only during an advance does it hold the tick from before (corrected 2026-10-18: earlier wording "stores the previous tick" was ambiguous). The ticks are stored as 32 bits (`__aullshr` low word); the step is a signed 32-bit argument. Each step is wrapped in profiler sections of the QPC profiler at `+0x124` (sections 2, 10, 5).
- World property getter, slot `+0x74` = `0x100a3380`: property 4 = tick (`+0x10c`), 7 = `tick / 500 +` offset `+0x69d4` (whole game seconds), 8 = PRNG cursor (§3). The setter is slot `+0x70` = `0x100a3230`. **Confirmed.**
- Reset (slot `+0x1c0`, `0x100af240`): clock, tick and PRNG cursor to 0. **Confirmed.**
- Integer-only scheduling seen so far (**confirmed** arithmetic): the island update `0x1008b760` sweeps its 8 × 8-tile chunks incrementally. Chunks per call = `min(chunks · Δtick / 2000 + 1, chunks)` for `0x1008b5f0`, i.e. one full sweep per 2000 ticks = **4 s** game time; and `min(chunks · Δtick / 15000 + 1, chunks)` for island slot `+0x194` (one sweep per **30 s**); no call when `chunks` = 0 (caps at `0x1008b7b3` and `0x1008b82a`, added 2026-10-18; `chunks` = `[+0x90] · [+0x8c]`, signed 32-bit arithmetic). `Δtick` is a 16-bit difference against `+0x9a`, zero-extended. `0x1008b5f0` advances per-tile item phases on a half-period toggle derived from the absolute tick (growth stages — **guess**). `0x100ae140` fires an island event every 450,000 ms of game time (7.5 min) when two PRNG draws allow it. `0x1009d2a0` re-evaluates something every 30,000 ms.

### Single-player step (`tNetClient`, vtable `0x1008914c`, slot `+0xf4` = `0x1005b110`)
**Confirmed** (disassembly):
```
game_ms = trunc(dt · speed + 0.5)        // x87: fild dt, fmul speed (+0x11c, f32), fadd 0.5 (0x10086340), __ftol
if game_ms < 1: return                   // nothing advances (pause: speed 0)
game_ms = min(game_ms, 100)
tick = world.get(4)                      // current tick, stored at +0x10
bigbrother.slot_20(tick)                 // +0x04, statistics recorder (likely AnnoBigBrother, see below)
world.Advance(game_ms)                   // +0x08 = AnnoWelt world
```
Precisely, the advance is a loop: `do { piece = min(rest, 100); bigbrother.slot_20(tick); world.Advance(piece); rest -= piece } while rest > 0` (`0x1005b13c`–`0x1005b172`); because `game_ms` is capped at 100 before (`0x1005b12b`), it runs once (added 2026-10-18).
Because `game_ms` is capped at 100, the simulation can lose time: at speed 1 a frame longer than 100 ms advances only 100 ms, at speed 4 any frame longer than 25 ms does. Because of the rounding, the effective speed at 0.5 depends on the frame time (`dt` = 1 ms → 1 ms, i.e. full speed). **Confirmed** arithmetic. `+0x04` is the object from scene slot `+0xa8` (scene `+0x15c`, created by `AnnoBigBrotherCreateInstance`, `AnnoScene.dll` `0x10002ef0`/`0x10006268`). That it is the BigBrother statistics object is **likely**.

### Multiplayer step (`tNetClientNet`, vtable `0x100893ec`, slot `+0xf4` = `0x1005fe10`)
**Confirmed** (disassembly):
```
acc += dt                                   // +0x118, real milliseconds
if not in_game (+0x6f4): acc = 0; return
if acc >= 10:
    n_turns = queued turn packets           // queue at +0x3cc
    if n_turns >= 2:                        // behind: finish the current turn at once
        run all remaining steps of the current turn; start next turn; acc = min(acc,10); return
    if n_turns == 1 and steps_left == 0: start next turn
    k = min(acc / 10, steps_left)           // steps_left at +0xa2c
    repeat k times: step()
    acc -= 10·k; steps_left -= k; acc = min(acc, 10)
start next turn  = pop packet, execute its commands (0x1005ffd0), steps_left = 10
step()           = bigbrother.slot_20(world.get(4)); world.Advance(trunc(speed · 10.0))   // 0x1008641c = 10.0f
```
So a **turn has 10 steps**, a step is due every **10 ms of local real time**, and it advances the world by `trunc(10 · speed)` game ms (5/10/20 at 50/100/200 %). The client never runs past the last received turn. If no turn arrives for more than 2000 ms (`0x100604b0`, `+0xa58 > 0x7d0`), it opens a "waiting" dialog (6/10 for clients, 6/16 for the host). **Confirmed.**

### Game speed and pause
- Speed is a float at net client `+0x11c` (1.0 at start, `0x1005b0c0`). `SetSpeed(speed, notify)` is slot `+0xb4`. **Confirmed.**
- Keys (`0x100071b0`, key-down branch): codes `0x74`→100, `0x75`→200, `0x76`→400, `0x77`→50, applied as `speed = value · 0.01` (`0x10086354`) through `0x10005f90`. 400 is refused when net client slot `+0xfc` is non-zero (`tNetClient` returns 0, `0x10009440`; `tNetClientNet` returns its connection `+0x3c4`, `0x100609c0`). **Confirmed** (code). That these are **F5 = 100 %, F6 = 200 %, F7 = 400 % (single-player only), F8 = 50 %** is **likely** (Windows `VK_F5`…`VK_F8`). Neighbouring keys `0x71`–`0x73` select 25/50/100 (zoom — **likely**).
- Pause: frame command `0x70` (`0x1000bbc0`) toggles `+0x170c` and calls `SetSpeed(0, 1)` or `SetSpeed(1.0, 1)`. **Confirmed**; which key sends `0x70` is not traced. Single-player `SetSpeed` (`0x1005a440`): speed 0 opens a dialog (ids 0x18/0x10, **likely** the pause screen) and stores speed 0; leaving speed 0 closes it. **Confirmed.**
- Multiplayer `SetSpeed` (`0x1005e230`) acts only on the host (`+0x3c8` = server). Speed 0 → server slot `+0x68(1)` = stop turns (`0x100565b0` clears `+0x3a`). Any other speed → resume, and the command `0x7a` (f32 speed) goes into the command stream. Every client applies it when the turn executes (`0x1005e210`). **Confirmed.**

## 3. Randomness

### Simulation PRNG (`AnnoWelt.dll`) — **confirmed**
- World slot `+0xb4` = `0x100a1740`: `cursor = (cursor + 1) mod len; return T[cursor]`. The cursor is the u16 at world `+0xfc`, `len` the u16 at `+0xfe` (set to `0x800` = 2048 by `0x100a3980`). `T` is a u16 table at `0x100cb8f8` (`.rdata`, 4096 bytes, SHA-256 `41bc794652bedbd69cd452d4b34a4877a92ccf1effea4c0b116d56706b45efdc`).
- **`T` is exactly the first 2048 outputs of the MSVC CRT `rand()` started from its default seed 1** (`x = x·214013 + 2531011 mod 2³²; out = (x >> 16) & 0x7fff`). Verified for all 2048 entries (2026-10-17, our script over the DLL bytes). The table therefore never has to be copied: it can be regenerated from the formula. Values are 0…32767 (min 21, max 32757, 1991 distinct).
- The generator has no other state: the whole PRNG state is the 11-bit cursor. It repeats every 2048 draws. The first draw after setting cursor `c` returns `T[(c+1) mod 2048]`.
- Seeding: the world constructor sets cursor = CRT `rand() & 0x7ff` (CRT seeded with `GetTickCount`, `0x100a3980`). It is then overwritten through world property 8 (`0x100a3230`, value mod 2048) by the scene when it creates the world (`AnnoScene.dll` `0x10002e40`, value from scene `+0x14c`, stored from the scene-load argument at `0x100025a0`/`0x100029f0`). The value comes from `AnnoFrame.dll` global `0x1009b978`: `rand() % 2048 + 1` at startup (`0x100152f0`, CRT seeded with `GetTickCount`). The host sends it to the clients in the lobby (message 0, 4 bytes; `0x1001fd10`, `0x10024240`), clients store it (`0x10019880` case 0), and it is passed to `CreateAnnoSceneInstance` (`0x10029100`). **Confirmed** chain. Whether a savegame stores the cursor (property 8 getter) is not traced.
- Consumers (static count of `slot 0xb4` calls followed by `%` or masks): `AnnoWelt` 98 call sites with slot `+0xb4` (e.g. `0x100ae140`: `r % 10`, then `r % island_count`), `AnnoScene` 19, `CompLogic` 35 (AI; e.g. `r & 7` picks one of 8 directions or offsets in CompLogic `0x100011b0` and `0x10002240`, `r & 3` in `0x10007e50`), `AnnoFrame` 21, `View` 383. Slot `+0xb4` is a common slot number, so these are upper bounds: only the `AnnoWelt` world calls and those through a world pointer are **confirmed** PRNG draws. That `AnnoScene` and `CompLogic` draw from the world PRNG is **likely**. `CompLogic.dll` has no CRT `rand`, no LCG constant and no timer import. **Confirmed.**

### CRT `rand()` — not deterministic, **confirmed**
- Every DLL links its own static MSVC CRT with a per-thread `rand` state (`_getptd()+0x14`): `AnnoWelt` `0x100b68a9` (`srand` `0x100b689c`), `AnnoFrame` `0x100785ba`, `AnnoGame` `0x10016feb`, `AnnoDisplay` `0x10030e37`, `View` `0x1008d05e`.
- `AnnoWelt` reseeds it with `GetTickCount` in several constructors (`0x10044b60`, `0x1004cea0`, `0x1004f5d0`, `0x100a3980`) and in `0x10093c10`. It has 64 call sites, in 26 functions. The sampled ones pick sound or animation variants (properties `0x148`/`0x149`, then object `0x3400`) or jitter float positions of effects (`0x1004cf20`). **Likely** cosmetic only. A rebuild must keep these draws out of the checksummed simulation and give them their own unsynchronised generator.
- `AnnoGame` `0x100113a0` uses `rand() % n` once, to pick one of `n` variants at load time (property `0x139`). **Likely** cosmetic. `AnnoFrame` has 16 call sites (UI, lobby, seed), `View` 2, `AnnoDisplay` 2.

## 4. Single-player vs. client/server (OQ-005)

- `CreateAnnoServerInstance(0x101)` (`AnnoGame.dll` `0x10013760` → `0x10007650`, 0x6418 bytes) is created for every game, single-player included (`AnnoFrame` `0x1002a190`, `0x10020100`). It is passed to the scene and the world (scene `+0x154`). It loads and saves the object data (`Anlagen.ver`/`Anlagen.sav`, `0x100076e0`/`0x10007750`). It is **likely** the object-definition provider. It is not a network server and does not mediate commands. **Confirmed** (creation), **likely** (role).
- The **network** server is `tConnectServerSock` (`AnnoFrame`, vtable `0x10088fc4`, `CreateAnnoNetServerInstance(1)` → `0x100578d0`, created only in the LAN host lobby `0x10015ef0`). Its socket thread is `0x100563c0`, which waits in `select` with a 100 ms timeout. **Confirmed.**
- Net client choice (`0x1002a190`): when no connection socket exists (`0x1009b958` = 0, single-player) the factory creates **`tNetClient`** (type 0, `0x1005b1f0`, 0x3c4 bytes). Otherwise it creates **`tNetClientNet`** (type 1, `0x10060a10`, 0xb28 bytes, derived from `tNetClient`). **Confirmed.**
- **Single-player applies player actions directly.** UI code calls net-client methods. `tNetClient` performs them on the world at once. Example: slot `+0x54` = `0x100589a0` takes 10-byte records and calls building or world methods; there is no message, queue or tick alignment. `tNetClientNet` slot `+0x54` = `0x1005cae0` instead packs the same records into a compact command (types `100`, `0x65`, `0x66`, `0x67`, `0x87`; run-length-style delta encoding of coordinates) and sends it (slot `+0x144`). When the turn arrives, the dispatcher `0x1005ffd0` → `0x1005c7f0` decodes it and calls the **same** apply function `0x100589a0`. **Confirmed.** So the command set for `crates/sim` is the `tNetClientNet` message catalogue (`0x1005ffd0` cases `0x64`…`0x97`, listed below). Single-player executes those commands between frames, not on a step boundary.
- Command types handled in a turn (`0x1005ffd0`, **confirmed** numbers; meanings not traced): `0x64–0x67, 0x87` (object placement family, via `0x1005c7f0`), `0x68–0x75`, `0x77–0x86`, `0x88–0x97`. `0x7a` = set speed (§2), `0x94` = sync snapshot or hash request (§5), `0x95` = async notification relay.

## 5. Multiplayer: turns, sync check, resync

### Turns — **confirmed** (`tConnectServerSock` update, slot `+0x58` = `0x100559c0`)
- While running (`+0x3a`), the server accumulates the host's frame `dt` and emits one turn per **100 ms** (`0x100555a0(100)`). Turn packet: `u8` type 1, `u16` turn number (`+0x38`), `u16` bitmap of recent non-empty turns (`+0x30`, shifted left each turn, bit 0 set when the packet carries commands), then the queued commands, each as `u16` type, `u16` length, payload (**no sender**: `0x100555a0` writes type and length only; the server keeps the sender at queue item `+0xc` and copies it to `+0xe00` only in recording/replay mode `+0xdea`; corrected 2026-10-18, an earlier reading had `u16` sender here). The server accepts types `0x64..0xc7` from any client (`0x100553e0`) and the dispatcher `0x1005ffd0` checks no ownership. Turns are kept for retransmission (`0x10055320`). The turn number increments every turn after a short warm-up (the counter at `+0xde8` must exceed 2 first).
- Not running (lobby or paused): a keep-alive (`0xcc`) every 333 ms. In game: a per-client keep-alive every 1000 ms; a client is dropped after 4 unanswered ones. **Confirmed.**
- Client turn queue (`0x1005b4b0`): turns are taken in `u16` sequence (`+0x326` = next expected). If a turn arrives early and the bitmap shows the missing ones were empty, those are synthesised locally. Otherwise (or for a gap > 15) the client buffers and asks for a resend (`0x1005f310`), repeating every 333 ms. **Confirmed.**
- **Command delay:** commands go to the server immediately. The server puts everything received before the next 100 ms boundary into that turn. Each client executes the turn's commands when it starts that turn and then runs its 10 steps. There is no fixed "execute at turn N + k" lead. The delay is network round trip + ≤ 100 ms batching + the client's queue. **Likely** (assembled from the confirmed pieces).
- Pace: host real time drives the turn rate, and game time per turn = `10 · trunc(10 · speed)` ms. At speed 1, multiplayer runs 1000 game ms per real second, independent of client frame rate as long as clients render ≥ 10 frames/s. Below that, a client catches up one turn per frame. **Confirmed** arithmetic.

### Sync check (desync detection) — **confirmed** unless marked
- Host client (`0x1005bb90`, in game): every **60,000 ms real time** (`+0xb1c`), if nobody is disconnected (`+0xa80` = 0), increment the check id `+0xb20` (u16, skipping 0). Then send command `0x94(id, 1)` through the turn stream (slot `+0x128` = `0x1005f540`) and tell the server `SetCheck(id)` (server slot `+0xb4` = `0x10057440`, clears the eight 16-byte report slots at `+0xe1c` and their flags at `+0xe9c`).
- Every client executes `0x94` inside the same turn (`0x1005f520` → `tNetClient` `0x10057f60(id, flags)`):
  - flag 1: serialise the whole game into a memory stream through the world's save slot `+0x7c` with mask `0x8fffd`. Append the `BIGBROTHER` statistics and a header that says whether `ANNOADDON1` is active. Keep the stream as the **candidate** snapshot (`+0x274`, size `+0x3b8`, id `+0x3bc`).
  - flag 2: compute **MD5** (`0x10076fc0` init with the standard constants, `0x10076ff0` update, `0x100770e0` final) over the candidate and send control message `0xd5` (id + 16-byte digest) to the server.
  - flag 4: promote the candidate to the **last agreed** snapshot (`+0x12c`, size `+0x270`).
- Server, every 500 ms (`0x10057470`): if a present player has not reported for the current id, request it (`0x94(id, 2)` via the host client, `+0x40` slot `+0x128`). When all have reported: if all digests are equal, send `0x94(id, 4)` and clear the id. Otherwise set the mismatch flag `+0xea4`, which makes the server broadcast `0xd6(id)` every 500 ms (`0x10057030` from `0x10055d9a`).
- Client on `0xd6` (`0x1005b990` stores it, `0x10060460` → slot `+0x12c` = `0x100582e0`): once per new id. If no agreed snapshot exists (`+0x26c` = 0), show **"Async aufgetreten! Spiel muß beendet werden !"** (`0x10096c08`). Otherwise rebuild the scene from the last agreed snapshot (`CreateAnnoSceneInstance`, re-attach world and statistics, restore player bindings). So the original has an **automatic resync to the last checkpoint**. The message is confirmed; the reload path is **likely** (decompilation of `0x100582e0` is damaged by the large stack frame).
- There are no per-turn checksums. Detection latency is up to 60 s plus the reporting round trip. **Confirmed.**

## 6. Floating point in the update path (OQ-007)
**Confirmed** x87 sites:
- Single-player step size `0x1005b110`: `dt · speed + 0.5` (f32 speed, x87 80-bit intermediate, `__ftol` truncation at `0x10077da8`, which sets round-toward-zero for the store).
- Multiplayer step size `0x1005fef9`/`0x1005ff81`: `trunc(speed · 10.0f)`. This value is part of the synchronised state, but every speed a player can set (0.5, 1, 2) gives an exact integer.
- Speed from key (`0x10007a5f`–`0x10007a6c`, in `0x100071b0` case `0x16`): `fild` the integer percentage, `fmul` by f32 0.01 (`0x10086354` = 0.0099999998), `fstp` to an f32 argument. The products 0.49999999, 0.99999998, 1.99999996, 3.99999991 round to exactly 0.5, 1.0, 2.0, 4.0 on the f32 store (round-to-nearest; our computation). So the synchronised step sizes are exact integers. **Confirmed** instructions; the rounding mode in effect is assumed to be the default (Open).
- `0x100ae140`: island centre converted int → f32 for the event payload (`0x15a`). Exact for map-sized integers.

**Confirmed integer-only:** world `Advance` `0x100ae040`, tick, PRNG `0x100a1740`, island chunk scheduler `0x1008b760`, chunk phase update `0x1008b5f0`, event queue `0x100add90`, `0x1009d2a0`. The CRT-`rand` effect code in `AnnoWelt` uses floats (`0x1004cf20` and others), **likely** cosmetic. Production, figures, ships, fire and AI (`CompLogic` has many float locals) were not surveyed; that belongs in their own notes.

## Fixture spec (to run on x86 Windows / Wine against the original)
Observable: the game clock (world property 7 = game seconds; UI clock `TIMER_UHR`) and, where a debugger is available, world `+0x100` (game ms), `+0x10c` (tick), `+0xfc` (PRNG cursor).

| # | Input | Expected |
|---|---|---|
| F1 | Single-player, speed 100 %, steady ≥ 30 fps, 60 s wall clock | game clock +60 s (±1 s); `+0x10c` = `+0x100 >> 1` at all times |
| F2 | As F1 at F6 (200 %) / F7 (400 %) / F8 (50 %) | +120 s / +240 s if fps ≥ 40 (else less, cap 100 ms per frame) / +30 s at even frame times (odd frame times round up: 3 ms → 2 ms, i.e. +40 s); at ~1000 fps 50 % runs near 100 % (rounding) |
| F3 | Single-player, frame limiter forcing 5 fps (200 ms frames), 100 % | game clock advances 100 ms per frame → +30 s per 60 s wall clock |
| F4 | Pause on, 60 s | game clock and `+0x100` unchanged |
| F5 | 2-player LAN, 100 %, 60 s | both clients +60 s; host emits 600 turns; each client 6000 `Advance(10)` calls; F7 has no effect |
| F6 | PRNG: set `+0xfc` = 0, draw 5 times via world slot `+0xb4` | 18467, 6334, 26500, 19169, 15724; with cursor 2047: 41, 18467, 6334; with cursor 1000: 1528, 26679, 7864 |
| F7 | Same seed (`0x1009b978`) and identical command log on two machines | identical MD5 at every sync check id; save files byte-identical at the same turn |
| F8 | LAN game, then patch one client's world `+0xfc` | within ≤ 60 s + 1 s: `0xd6` received; with an agreed snapshot present the game reloads it, otherwise the "Async aufgetreten" box appears |
| F9 | Real-time intervals | sync id increments every 60 s; server compare every 500 ms; waiting dialog after 2 s without turns; autosave every 10 min of real time (single-player) |

## Open
- Which key or button sends frame command `0x70` (pause); confirm F5–F8 key mapping and the meaning of the `0x80` key-flag bit.
- Whether single-player really stops while the window is inactive (`+0x1704`/`+0x1708`).
- What the world sub-systems at `+0x114`, `+0x118`, `+0x69c8` and island slots `+0x190`/`+0x194` are (figures, ships, trade?); survey them for x87 and PRNG use.
- Whether savegames store the PRNG cursor (world property 8) and the 64-bit game-ms clock; links to OQ-020 and M2.1.
- Meaning of all command types `0x68`–`0x97` (needed for the `sim` command catalogue); turn header byte layout beyond type, number and bitmap; the `+0xdea` (recording/replay?) mode in the turn builder.
- Whether `CompLogic` (AI) runs on every client or only on the host in multiplayer, and whether its 35 slot-`0xb4` calls really hit the world PRNG.
- Unit of the render/animation tick (`annowelt.md`): **guess** that it is the world tick (2 ms).
- The x87 precision-control word in effect during the update (default MSVC 53-bit; DirectDraw-only display, so probably unchanged — guess).
- Resync path `0x100582e0`: confirm the reload from the agreed snapshot instruction by instruction, and the role of `+0x26c`.
