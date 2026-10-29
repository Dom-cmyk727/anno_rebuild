# Plan: the LAN MVP, five players on one fixed map with no computer players (owner decision 2026-10-29)

**Status:** plan, written by the orchestrator on 2026-10-29. It narrows [`lan-endless.md`](lan-endless.md) to the first playable release. That plan stays the target after the MVP: nothing in it is withdrawn, only moved after the MVP where this file says so. Items marked **(proposal)** wait for the owner (§8).

## 1. The owner's decisions (2026-10-29)

| # | Decision |
|---|---|
| M1 | **One fixed map**, larger than the original's and scaled for **five players**. It replaces the generator (W2) for the MVP. |
| M2 | **No computer players** in the MVP. It replaces the original AI's port (W7) for the MVP. |

The purpose is a shorter path to the first LAN game the group of five can play.

## 2. What follows from M1 and M2

| Requirement or decision (`lan-endless.md` §1, §7) | In the MVP |
|---|---|
| R2 up to 6 players | The fixed map has five start regions, so it seats up to five. `SetupLimits::max_seats` stays data (default 6); the map's start-region count bounds the seats for this map. |
| R3 larger map, scaled to the players | Met by the fixed map at edge round(1280 · √(5/4)) = **1431 tiles**, D1's formula at n = 5. That is about twice the area of the largest original endless world (1024 × 1024). |
| R4, R5 island size and quantity | After the MVP (one map). |
| R6 pirates | **Out.** The original creates the pirates through the CompLogic factory (player type 15, `CompLogic.dll` `0x1003dc50`, `docs/re/annoscene.md` §11.5), so without the AI there are no pirates. |
| R10 empty seats are computer players | **Out.** (proposal) A game holds 2–5 humans; an empty seat's start region stays free land. |
| R15 two start modes | (proposal) **Spread only:** the map's five start regions. "Together" comes after the MVP. |
| D8 Venetians and natives on | **Owner question (§8 item 1).** The original creates both through the same factory as the computer players (types 14 and 16, §11.5), so they need the CompLogic port. W6.3 (trade) depends on the answer. |
| D9 a dropped seat goes to the AI after 60 s | **Replaced.** (proposal) The game pauses for the grace period (60 s, D9's value). Then it goes on with the seat idle: the player's cities keep running under the rules and issue no commands. On rejoin the player gets the seat back. |
| R7 last one remaining | Unchanged. Until combat exists, only bankruptcy eliminates (as `lan-endless.md` checkpoint C notes). |
| R8, R16 colours and recolouring | Unchanged. With five players every player must recognise the others' ships and flags. |
| R11 the multiplayer menu | Smaller: no world options and no pirate option. Seats (name, colour, ready), "Original game settings" (D11), Start. |
| R12 up to 4K | Unchanged. |
| R13, D11 enhanced by default | Unchanged. |
| R14 network: join-in-progress, host migration, short drops, hashes | Unchanged as requirements. (proposal) Order inside W3: short drops and rejoin first, because without the AI a dropped player has no stand-in; join-in-progress next, on the same snapshot path; host migration last, after the first LAN playtest of MVP-A. Whether host migration may move after the MVP is §8 item 4. |

## 3. The fixed map (new workstream MAP, size S; replaces W2 for the MVP)

- **What it is:** a layout file of our own, with no bytes of the original in it:
  - the world edge;
  - every island: position, the original template by name (`Scenes/Islands/...`), its raw materials and ores;
  - the five start regions;
  - the parties.

  The island templates are read from each player's own game folder at load time, as the scenario loader does now. The file names templates and copies nothing, so it can live in git.
- **Original-like, fair by measure (owner decision 2026-10-29, after the MAP.1 proposal):** the first proposal (five rotated copies of one region around one contested island) was rejected as too artificial and symmetric. The map should look and play like an original endless map, with varied island positions and templates and the originals' density (MAP.2 measures it from the nine endless scenarios). Fairness comes from measured equality within tolerances: land per climate within each player's reach, and the distances to the middle and to the nearest rival. It does not come from identical copies. The measures are those of [`docs/sim/world-scaling.md`](../sim/world-scaling.md) §12 (`tools/world_measure.py`) and `tools/fixed_map.py fairness`. The generator's owner questions (world-scaling §15) do not block the MVP.
- **Loading:** the layout becomes a `ScenarioSpec` (`src/sim/include/anno/sim/scenario.hpp`: settings, players, islands with their terrain), the same input an original scenario produces. The rest of the sim does not know the map is ours. When the layout names every template, the link resolver (`island_links.hpp`, S14 stage 2) is not needed. The alternative, links plus a fixed seed through that resolver, reuses more code but leaves fairness to the seed.
- **To settle in the task's first step:** where the scenario-wide records come from (goods, timers, limits; `SCENE/{WARETYPE, SKTIMER, MONOPOL2, LIMITS, ...}`). The likely answer is one original endless scenario named in the layout, read at load, with its world size, islands and players replaced. The step checks this against `scenario.cpp` before anything is built on it.
- **In the handshake and the save:** the layout's version and digest replace the generator's version, parameters and seed (`lan-endless.md` §4). Savegames store the islands, as now.
- **Measured before the map is final (PLAN §0.3):** the view frame at 4K and the sim tick on the 1431-tile map with five players' cities.
- **The start:** MVP-A keeps checkpoint A's developer start (a warehouse placed in each start region). MVP-B uses the real start: a ship with its cargo in each start region (W6.1).
- **Status (MAP.1, 2026-10-29):** the layout format, reader, dump and loader are in place, and `data/maps/lan5.txt` is a proposal (five rotations, one contested middle). The owner rejected that design the same day (too artificial and symmetric). The file stays as a placeholder for the tests until MAP.2 replaces it with an original-like layout that is fair by measure.

## 4. Workstreams for the MVP

| Workstream | MVP scope | Size | Gate before start |
|---|---|---|---|
| W1 setup | `GameSetup` names the map (layout name and digest). The world options, start mode and pirate level stay in the struct, unused by the fixed map. Into the handshake and the save. | S | none |
| **MAP** | §3. | S | none |
| W3 networking | As planned, for five peers, in the order of §2 R14. No computer seats, no AI takeover. | L | **ADR 0003 approved** (Proposed since 2026-10-26) |
| W4 game shell | Unchanged: the app runs the sim at a fixed tick, the renderer draws the live world, input becomes commands, save and load. | M | none |
| W5 menus, HUD, resolution | The smaller lobby of §2 R11, the HUD, resolution and UI scale, recolouring for the colour table. | M | ADR 0004 written (D4 already decided) |
| W6 sim content | Unchanged, and still the critical path: population P2–P5, ships and settling, trade (per §8 item 1), fire, military and combat, elimination. | XL | per stage, RE first |
| W9 tests | Five peers instead of six in `netsim`; no computer seats; the LAN playtest with five PCs. | M, runs along | — |
| W2 generator | **After the MVP.** The spec stays. | — | — |
| W7 the original AI | **After the MVP.** The RE already done stays; T-AI.4 waits. | — | — |
| W8 pirates | **After the MVP** (needs W7). | — | — |

## 5. Checkpoints

| Checkpoint | What can be tested | Needs | Missing at that point |
|---|---|---|---|
| **MVP-A — LAN sandbox** | 2–5 peers on the fixed map: build, demolish, roads, production, carts; desync detection and resync; drop and rejoin; the lobby with colours; up to 4K. | W1, MAP, W3 (core), W4, W5 (lobby, basic HUD) | Ships, population, combat. Developer start. |
| **MVP-B — Settlers** | The real start: ship, founding, population and taxes, trade per §8 item 1. | W6.1, W6.2 (P2–P5), W6.3 if in scope | Combat: only bankruptcy eliminates. |
| **MVP-C — Conflict (the MVP's acceptance)** | Fire, military and combat, taking over cities, last one remaining. | W6.4–W6.6, W9 | After the MVP: AI, pirates, generator, 6–8 players, internet. |

**The MVP's acceptance** (`lan-endless.md` "testable", adjusted):
1. **Automated:** five headless peers, all scripted by a command bot, play one hour of game time on the fixed map with simulated latency and loss. Their state hashes agree at every check. An injected divergence is detected within one check interval and recovered from a host snapshot.
2. **On the LAN:** five real PCs find the game, join, choose colours, play, and survive a disconnect and a rejoin (`docs/testing/lan-playtest.md`).
3. **Performance:** five players' cities on the fixed map at 3840 × 2160 within the PLAN §0.3 frame targets and a measured sim-tick budget.

## 6. What M1 and M2 save, and what they do not

- **Removed from the MVP:** the generator (L) becomes MAP (S); the AI port (XL); the pirates (M); the computer-seat and AI-takeover paths in W3 and W5; the world options in the lobby; the old checkpoint C.
- **Nearer:** MVP-A no longer waits for the generator. It needs W3, W4 and W5, which can run in parallel.
- **Not shorter: the critical path.** W6, the game content, was and stays the longest line. MVP-B and MVP-C still wait for population, ships, and military and combat, each RE first.

## 7. Lanes (proposal)

1. **Sim content (critical path):** T32.6 → P2.1 (after T32.5 is integrated) → P2.2 → P2.3 → P3–P5; then ships and settling.
2. **Shell:** MAP, then W4.
3. **Network:** W3 once ADR 0003 is approved.
4. **UI:** ADR 0004, then W5.
5. **RE, docs only:** ships and settling (W6.1) first, then fire (W6.4) and military (W6.5).

- **Parked during the MVP:** the AI RE lane after T60 is integrated (T-AI.4), the generator (W2), the render backlog (T5, T6b, T6c, T1f, T11, T14 stage 2), T31.
- **Filler:** T35.
- **Limits:** about two full test-gate runs fit at once on the 32 GB PC (T10c measured about 35 GB committed for one 16-worker gate; the memory coordinator queues the rest). Docs-only RE lanes need no builds. The orchestrator's review of every task bounds how many lanes pay off.

## 8. Owner questions

1. **Venetians and natives (D8):** in or out of the MVP? They need the CompLogic port (§2). Without them there is no computer party to trade with, and W6.3 shrinks accordingly.
2. **A dropped player's seat without the AI:** pause for 60 s, then go on with the seat idle, and return it on rejoin (§2 D9)?
3. **The start:** spread only, from the map's five start regions, with "together" after the MVP (§2 R15)?
4. **Host migration:** last inside W3, after the first LAN playtest of MVP-A, but still inside the MVP; or after the MVP (§2 R14)?
5. **ADR 0003:** approve it, so that the C++ `net` library (W3 step 2 on) can start.
6. **The map:** ~~five rotated copies of one designed start region around a contested middle (§3), or a different design the owner wants?~~ **Decided 2026-10-29:** original-like and fair by measure (§3), not rotated copies. Open within MAP.2: the fairness tolerances, and the choice among the candidate layouts. **Layout rule (owner, same day):** five large northern (NORD 256²) and five large southern (DSCHUNGEL 256²) islands, five different shapes each with roughly the same land, at varied positions. Northern islands lie on or above the middle line by map rows (y = edge/2, the original's climate bands); southern islands lie below it. Each player starts on a large northern island, with one large southern island in comparable reach. The rest is original-like filler by the original's climate bands. Details: MAP.2 on the board.
