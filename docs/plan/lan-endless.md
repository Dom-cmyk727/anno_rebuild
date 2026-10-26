# Plan: a testable LAN multiplayer endless game (owner request 2026-10-26)

**Status:** plan, written by the orchestrator on 2026-10-26. No executor task has been written from it yet. The owner's choices in §7 come first.

**Goal:** up to six players play an endless game on one LAN. The world is generated larger than the original's and scales with the player count. Every empty seat is a computer player, and the pirates are an option. A game ends when one player remains. A custom multiplayer menu sets it all up. The display goes up to 4K. The game runs with the enhanced rules only.

**"Testable" means three things** (acceptance of the whole plan, §6 checkpoint D):
1. **Automated:** six headless peers, four humans scripted by a command bot plus two computer players, play one hour of game time on a generated world with simulated latency and loss. Their state hashes agree at every check. An injected divergence is detected within one check interval and recovered from a host snapshot.
2. **On the LAN:** two to six real PCs (Windows and Linux mixed) find the game, join, choose colours, play, and survive a disconnect and a rejoin. The checklist goes in `docs/testing/lan-playtest.md`.
3. **Performance:** with six players' cities on the largest generated world, at 3840 × 2160, the PLAN §0.3 frame targets and a measured sim-tick budget hold.

## 1. The owner's requirements (2026-10-26, as given)

| # | Requirement |
|---|---|
| R1 | A multiplayer **endless** game, testable. |
| R2 | **Up to 6 players** for now. |
| R3 | The overall **map size larger than the original's**, and the size **scaled to the number of players**. |
| R4 | **Island size**: small, medium, large. |
| R5 | **Island quantity**: less, default, many. |
| R6 | **Pirates** as an option. |
| R7 | **No special win conditions for now:** the last player remaining wins. Others follow later. |
| R8 | **More player colours:** 6 fixed colours for up to 6 players, each player choosing one. |
| R9 | **LAN only** for now (internet later). All 6 players are on the same network. |
| R10 | **Empty seats are computer players:** if only 4 of 6 players are present, the other two are in-game AI. |
| R11 | A **custom multiplayer menu**, simple but user-friendly. |
| R12 | **Resolution up to 4K.** |
| R13 | The **only playable mode is the enhanced version.** |

## 2. What this changes in earlier decisions

Each item is recorded in `docs/PLAN.md` §0.7. Each needs the owner's confirmation in §7, where it says so.

- **Enhanced only (R13) changes PLAN §0.2 and M5** ("faithful mode must remain selectable"):
  - The faithful rules stay in the code, unchanged, as the reference for tests and the differential checks against the original (CLAUDE.md "faithful core separate").
  - They are not offered in the menu.
  - The playable game uses one rules preset, `enhanced`: the `Rules` options plus the render styles.
  - A developer flag (`--rules faithful`) keeps the faithful game runnable for comparisons.
- **The world size (R3) changes PLAN §0.6 for the enhanced endless game.** §0.6 keeps a world of up to 4 players as authored and grows it only above 4. R3 makes every enhanced endless world generated and larger than the original's at every player count. §0.6's own rules still hold for the generator:
  - integer-only, seeded and versioned;
  - inputs in the handshake;
  - every island an original template, chosen as the original chooses;
  - the fairness floor per player.
- **The player count (R2):** 6 is this release's maximum. PLAN §0.6's target of 8 stays the project goal. Nothing in the engine hard-codes 6: it is the setup's default maximum (§0.1).
- **The colours (R8):** the original has eight player colours (`docs/re/annodisplay.md` §8). R8's six fixed colours are a data table the setup offers; §7 D6 proposes which six. The ships' sails and flags are not coloured yet (OQ-027). In multiplayer a player must recognise other players' ships, so that becomes part of W5.

## 3. Where the project stands (verified 2026-10-26)

| Part | State | Gap for this plan |
|---|---|---|
| **Sim core** (`src/sim`, about 57 000 lines, 635 test cases) | Deterministic: integer state, the seeded world PRNG, a hierarchical state hash, save and load, replay with a divergence finder. Done: players, islands, buildings, cities, the full tick scheduler, all production classes, the market cart and workshop carrier, scenario loading. The walk rule is the original's (T33). | Commands carry no target tick (PLAN M3). Not implemented: population, taxes, ships, trade, fire, combat, pirates, natives, Venetians, AI behaviour, win and loss conditions. |
| **Commands** (`command.hpp`) | Build, Demolish, SetProductionLine, SetTradeFlag, SetTradeSlot, SetSpeed; wire codec; host turn entries. | Ship, military, diplomacy and other commands are missing. |
| **AI** | `Seams::ai` is called every advance and draws from the world PRNG; the body is empty. The original's CompLogic factory and profiles are partly read (`docs/re/annoscene.md` §11). | No AI behaviour. |
| **Networking** | None. The original's protocol is read (`docs/re/tick-loop.md` §4–5): host turns every 100 ms, resends, keep-alives, an MD5 of a full save every 60 s. | Everything: transport, discovery, lobby, handshake, turn scheduling, hash checks, resync. |
| **World generation** | The original's island selection for `INSELLNK` links is implemented (S14 stage 2): templates by climate and size, raw materials, ores, treasures. | No generator. The spec `docs/sim/world-scaling.md` (OQ-100) is not written. |
| **Rendering** | GPU compositor (wgpu-native) with a CPU reference; enhanced water; continuous zoom. 4K measured: GPU p99 ≤ 13 ms (`docs/perf/README.md`). | The renderer draws scenario files, not live sim state. |
| **App** (`src/app`, about 3 500 lines) | A viewer: SDL3 window at a fixed 1280 × 800 logical size, resizable, high-DPI aware; pan and zoom. | It does not link the sim. No game loop, HUD, menus, input → commands, fullscreen, resolution choice or UI scale. |
| **Original's endless setup** | No options screen. Nine preset files (`Scenes/0010SingleEndlos`), each with an `.htm` description: island count (many / average / few), island size (large / normal), ores, fertility, pirates (none / not very aggressive / aggressive), money. | The descriptions are not recorded in `docs/`. The class names `tFrameStartupEndlosCustomize` and `tFrameStartupLanServer` (`analysis/pe_symbols.json`) are not analysed. |

## 4. Architecture

- **One sim on every peer.**
  - Every peer runs the same deterministic `World`, the computer players included: the AI runs inside the sim on every peer, as ISS-038's FIX says.
  - Only commands travel. A peer never sends state, except a snapshot for a resync or a late join.
- **Host-authoritative turns** (the original's model, `tick-loop.md` §4):
  - The host collects the players' commands and sends one turn per period (default 100 ms, as the original).
  - A turn carries the commands and their issuers.
  - Every peer applies a turn's commands at the same tick. A command is scheduled for turn `now + delay`; the delay is configured and measured, 2 turns by default.
  - The host is a peer that plays. There is no dedicated server in v1.
- **Desync detection better than the original's:**
  - Every K turns (default 10, i.e. 1 s) each peer sends the hash of its state at that turn's tick; the host compares.
  - The cost of a full state hash per check is measured first. The hierarchical hash with cached chunk digests makes it incremental.
  - On a mismatch the host pauses the game. Then:
    1. The diverging subtree is logged (`diff_trees`).
    2. A snapshot from the host is loaded by every peer.
    3. The game goes on.
  - The divergence report is a test artifact.
- **Handshake** (before the first tick), all of it compared, and a peer that differs is refused with the reason:
  - the content hash: `Anlagen.dat`, `FigurenHeader.dat`, the island templates, the walk tables;
  - the build version and `kStateHashVersion`;
  - the rules preset digest;
  - the generator's version, parameters and seed;
  - the setup: seats, colours, options.

  Then every peer builds the world and the world hashes are compared before tick 1.
- **Seats and drops:**
  - A seat is human (a LAN peer) or computer.
  - When a human drops, the game pauses for a grace period (configurable, 60 s by default). Then the seat goes to the AI, and the AI's commands come from the sim on every peer, so nothing changes on the wire.
  - A rejoining player gets a snapshot, the turns since it, and their seat back.
  - If the host leaves, the game ends with an autosave that the host can load again later. Host migration is not in v1.
- **Layers in the code** (dependency direction as CLAUDE.md):
  - `sim`: the rules, the AI, the generator; no I/O.
  - `net` (new library): transport, sessions, turns, hash checks; no rendering.
  - `ui` (new library): widgets, menus, HUD; it produces commands and never mutates the sim.
  - `app`: the game loop, the window, wiring.
  - `render`: draws live sim state through a read-only view.

## 5. Workstreams

Each workstream has stages. A stage becomes one or more board tasks (`docs/orchestration/TASKS.md`), each with steps, files, acceptance and gates, as for T33. The sizes are relative: S (days), M (one to two weeks), L (several weeks), XL (more).

### W1. Game setup and the enhanced preset (S) — the shared contract
1. **`GameSetup`** (sim data, hashed, part of the handshake and the save):
   - the seats: kind, name, colour, team (unused in v1);
   - the world options: size by player count, island size, island quantity;
   - pirates (none / mild / aggressive);
   - the start resources, as the presets' "money";
   - the seed;
   - the rules preset.
2. **The `enhanced` rules preset:**
   - every `Rules` option gets its enhanced value. The default is the faithful value unless a fix is listed (`docs/research/original-issues.md`, the `Rules` comments);
   - one table in `docs/sim/rules-presets.md`, with the owner's choice for every feel question.
3. **The nine endless presets:** their `.htm` descriptions recorded field by field in `docs/formats/` (no text copied). These are the original's own setting levels, which W2 calibrates against.
- **Acceptance:** `GameSetup` round-trips through save and load and goes into the handshake digest. The presets table is complete, and every row has an evidence note or an owner decision.

### W2. The enhanced world generator (L) — R3, R4, R5, R6
1. **Spec `docs/sim/world-scaling.md`** (PLAN M4.1, rewritten for R3), before any code:
   - **Size:** world edge(n) = round(B · √(n / 4)), where B is the 4-player edge. **Owner choice D1**; the original's endless worlds have 800 or 1024.
   - **Island size:** small / medium / large set the mix of link size classes. The original's links are 64, 80, 128 and 256 tiles; the mix comes from the inventory of `Scenes/Islands` templates per climate and size.
   - **Island quantity:** less / default / many set the land share of the world. The originals cover 19–46 %; the defaults are calibrated against the presets' "few / average / many".
   - **Climate bands** north to south as the originals (`annoscene.md` §5).
   - **Start regions:** one per seat, with the fairness measures of PLAN M4.1.
   - **Natives:** they scale with the islands (§0.6). **Venetians:** one party. **Pirates:** none, or one party with the chosen aggressiveness.
2. **Generator in `src/sim`:**
   - integer-only, seeded from `GameSetup`, versioned; ordered containers;
   - the links go through the original's island selection (S14 stage 2), unchanged;
   - output: a world the existing loader takes, so the rest of the sim does not know whether a world was generated.
3. **Fairness tool `world_fairness`** and property tests over many seeds, for every player count 1–6 and every option combination: the floor holds, nothing overlaps, and the world hash is equal on Windows and Linux.
- **Acceptance:** the spec approved by the owner; the generator and the tests green; the world hash in the handshake.

### W3. LAN lockstep networking (L) — R9, R10
1. **ADR `docs/adr/0003-network-transport.md`.** Owner choice D3. Options:
   - our own UDP layer, the original's design: no dependency;
   - ENet;
   - GameNetworkingSockets.

   Criteria: Windows and Linux, LAN broadcast discovery, a reliable ordered channel, code size, licence. 0002 is reserved for the float walk (T34).
2. **Commands get a target turn** (the open PLAN M3 item): `Turn { index, commands }` becomes the unit the world advances by in multiplayer. The single-player path goes through the same code.
3. **The `net` library:**
   - sessions: host and client;
   - LAN discovery: UDP broadcast on a configured port; direct IP as a fallback;
   - the handshake of §4;
   - turn relay with resend, keep-alive and a wait dialog after a timeout (the original's 2 s);
   - hash checks, snapshot resync, drop and rejoin, AI takeover.
4. **`netsim` harness:** N peers in one process over a simulated link (latency, jitter, loss, reorder, disconnect), with a command bot per human seat.
- **Acceptance (PLAN M4, adjusted to 6 players):**
  - 6 peers, 150 ms latency, 2 % loss, 10 000 turns: in sync;
  - an injected divergence detected within one check interval and recovered;
  - a peer dropped and rejoined: in sync;
  - two real PCs on the LAN find each other and play a scripted game.

### W4. The game shell (M) — the app becomes a game
1. **`app` links `sim`, `net` and `ui`.**
   - Game loop: a fixed sim cadence decoupled from the frame (turns in multiplayer, the same code in solo).
   - The render reads a const view of the world. The window thread never waits for the sim.
2. **The render bridge:** the renderer draws the live world (islands, tile objects, buildings, figures with interpolated positions, ships) instead of only scenario files. The scenario-file path stays for the viewer.
3. **Input → commands:** selection, the build cursor with the placement preview (`placement.cpp` rules), demolish, the camera. Each action becomes a command for the next turn.
4. **Save and load** of a multiplayer game: the host's autosave every N minutes and on exit; load restores `GameSetup` and the seats.
- **Acceptance:** a solo game (one human, the rest computer seats) and a 2-peer LAN game run in the same code; save, quit, load and continue.

### W5. Menus, HUD and resolution (M) — R8, R11, R12
1. **ADR `docs/adr/0004-ui-toolkit.md`.** Owner choice D4. Options:
   - our own small widget set drawn by the GPU compositor with the original's fonts (`.zei`, decoded) and `ToolGfx` textures (`fenster.tex` and others), so it looks like the game;
   - Dear ImGui: fast to build, but not the game's look.
2. **Menus** (simple, keyboard and mouse, every action reachable in two clicks):
   - **Main:** Multiplayer (LAN), Options, Quit. Solo is a LAN game with no other humans.
   - **Join:** the discovered games (host name, players, world size, ping) and "Connect to IP".
   - **Lobby** (host edits, clients see):
     - six seat rows: name, a colour swatch picker in which a taken colour is greyed, ready, and Human/AI;
     - the host can kick and switch an empty seat to AI;
     - the world options of W1;
     - pirates;
     - "Start" when every human is ready. Empty seats start as AI (R10).
   - **In game:** pause (host), save (host), leave; the wait dialog; a player dropped or taken over by the AI; the end screen (winner, last one remaining).
3. **HUD:** the original's layout (`docs/re/ui-and-placement.md`, fractions of 1024 × 768) at the UI scale: minimap, the four tabs, the build menu, the resource bar, the info panel. W4's commands are behind it.
4. **Resolution (R12):**
   - windowed, borderless fullscreen and exclusive fullscreen;
   - every mode the display reports, up to 3840 × 2160, with no fixed list;
   - **UI scale:** automatic from the window height (1.0 at 1080 lines, 2.0 at 2160 lines; integer steps keep the pixel art crisp), plus a manual setting;
   - the settings saved per machine.
   - 4K requires the GPU path. The CPU reference misses the 4K target at p99 (`docs/perf/README.md`). The setting says so.
5. **Player colours on ships and flags** (OQ-027), so that other players' ships can be told apart.
- **Acceptance:** a scripted UI test drives every menu path; the HUD checked at 1280 × 720, 1920 × 1080, 2560 × 1440 and 3840 × 2160; the owner's visual check (`docs/testing/checklist.md`).

### W6. The sim systems a game needs (XL) — the critical path
The current lane (T33.5, then P1) is part of this workstream. An endless game needs, in this order, each with the original's rules (RE first, as T33):
1. **Ships and settling:** the start ship with its cargo, moving ships, founding a warehouse on a new island from the materials on board (by ship or by scout [H/Kontor_uebernehmen]), loading and unloading. RE: `docs/re/fire-and-ships.md` and `production.md` §ships.
2. **Population, needs and taxes:** P1–P5 (`state.md` §4.7c), already specified.
3. **Trade:** the city's buy/sell lists (already read, T26) and the Venetian trader.
4. **Fire and disease:** a known desync cause in the original (ISS-033), so a priority for the hash tests.
5. **Military and combat:** soldiers, warships, attack, taking over warehouses and market buildings [H/Kontor_uebernehmen].
6. **Elimination:** a player is out when bankrupt (the existing deadline rule) or when they hold no city. **The last one remaining wins** (R7). Until combat exists only bankruptcy eliminates; §6 checkpoint C notes this.

Each system is a board task series with differential fixtures against the original where the Wine runner can produce them (T30b-style).

### W7. Computer players (L) — R10
1. **ADR `docs/adr/0005-ai.md`.** Owner choice D5. Options:
   - port the original's CompLogic (35 world-PRNG call sites, profiles `COMPPROFIL2`, parameters partly read, `annoscene.md` §11): faithful in behaviour, but a large RE effort;
   - a new deterministic AI behind the command interface, using the same `Seams::ai` and the world PRNG. It is enhanced by definition (R13 allows it), and it is faster to reach "it settles, builds chains, grows, trades".
2. **The AI emits commands exactly like a player.** The sim applies them on every peer, the same code for a filled empty seat and for a takeover.
3. **Levels:** the original's profiles give the parameter names (§11.6); an easy / normal / hard setting maps to them.
- **Acceptance:**
  - a 6-AI headless game for 2 game hours: every AI settles at least 2 islands, reaches the second civilisation level, and makes no illegal command (each one a `Reject` count);
  - the AI's command stream is equal on all peers.

### W8. Pirates (M, after W6.1 and W6.5) — R6
- **Pirate party:** player type 15, the CompLogic pirate class (`annoscene.md` §11.5), the `PIRATSCHIFF` ship type, the pirate islands, which are excluded from normal selection (`annoscene.md` §5).
- **Behaviour per W7's decision:** patrol, attack ships, demand protection money (`docs/research/manual-and-help.md`).
- **Option levels:** none / mild / aggressive, the original presets' levels; the parameters calibrated against the presets.
- **Acceptance:** with "aggressive" a pirate attacks within N minutes; with "none" no pirate party exists. In sync on all peers.

### W9. Tests, measurements and the LAN playtest (M, runs along)
- `netsim` runs in CI (headless, no game files) on synthetic worlds; with the game files, a nightly 1-hour 6-peer game.
- Determinism across Windows and Linux: the same game on both, with the hashes compared (PLAN M3 acceptance).
- Performance: the sim tick and the frame on the largest 6-player world in `docs/perf/README.md`.
- `docs/testing/lan-playtest.md`: the owner's checklist (find the game, join, colours, play, drop, rejoin, save and load, 4K).

## 6. Order and checkpoints

The critical path is W6 (the game content). W3, W2, W4 and W5 can go in parallel with it, because they meet at the command interface and the world loader, which exist.

| Checkpoint | What can be tested | Needs | Missing at that point |
|---|---|---|---|
| **A — LAN sandbox** | 2–6 peers on a generated world: build, demolish, roads, production, carts; desync detection and resync; the lobby with colours and options; up to 4K. | W1, W2 (generator v1), W3, W4, W5 (lobby, basic HUD) | Ships, population, AI, pirates. Every player starts with a warehouse that the generator places on the start island (a test start, owner choice D7). |
| **B — Settlers** | The real start: ship, founding, population and taxes, trade with the Venetians. | W6.1–W6.3 | AI, combat, pirates. |
| **C — Full seats** | Empty seats and dropped players are computer players; a solo game against the AI. | W7 | Combat: only bankruptcy eliminates. |
| **D — Threat** | Fire, combat, taking over cities, pirates; "last one remaining" with conquest. **The plan's "testable" acceptance.** | W6.4–W6.6, W8, W9 | Later: win conditions, internet play, 8 players. |

The board gets W1 and the W2 spec first (small, they unblock W2, W3 and W5). Then W3 and W4 in a second lane beside W6.

## 7. Decisions for the owner (each with a recommendation)

| # | Question | Recommendation |
|---|---|---|
| D1 | The 4-player world edge B (R3) | 1280 tiles (1.25 × the original's 1024), so 6 players get about 1568. To be checked by the fairness tool and the 4K frame. |
| D2 | Pirate levels | The original presets' three: none / not very aggressive / aggressive. |
| D3 | Network transport | Our own UDP layer after the original's design (host turns, resends). It is small, has no dependency, and LAN broadcast is simple. ENet is the fallback if it slips. |
| D4 | UI toolkit | Our own small widget set with the original's fonts and textures. It keeps the game's look, and the GPU compositor already draws these assets. |
| D5 | AI | A new deterministic AI first (checkpoint C reachable), and the CompLogic RE in the background for a "classic" level later. |
| D6 | The 6 colours | From the original's 8 (`annodisplay.md` §8): blue (0,187,255), red (255,68,0), yellow (255,244,10), turquoise (0,255,176), green (47,254,41), magenta (248,57,255). Orange (255,138,0) is too close to red, and white (255,244,222) is hard to see on sand. |
| D7 | Checkpoint A's start | A warehouse placed on each start island, until ships exist (B). This is a test start, not a game mode. |
| D8 | Venetians and natives in the enhanced endless game | Both on, as in the original's endless worlds. |
| D9 | A dropped player's seat | The AI takes over after 60 s; the player gets the seat back on rejoin. |
| D10 | The game speed in multiplayer | The host sets it. 400 % is refused in multiplayer, as the sim does now (`SimConfig::multiplayer`). |
| D11 | Confirm §2: the faithful rules out of the menu but kept for tests (`--rules faithful`), and 6 as the default maximum | Yes. |

## 8. Risks

| Risk | Handling |
|---|---|
| W6 is large: ships, population, trade, fire and combat each need RE first. | It is the critical path from now on. Checkpoints A–C give testable games before it is complete. Each system is split into board sub-tasks as T33 was. |
| A full state hash per check is too slow on a big world. | Measure first (W3 step 3). The chunk digests are cached; check the hierarchical root only, and only the changed subtrees. |
| A larger world misses the frame or tick targets at 4K with 6 players. | The generator's parameters are config; the 4K frame and the tick are measured on the largest world before the defaults are fixed (D1). |
| A new AI plays worse than the original's. | Levels and measured goals (W7 acceptance). The CompLogic RE stays available for a classic level. |
| Windows/Linux divergence. | The existing rules (integers, no float in state, `-ffp-contract=off`, sanitizers) plus a cross-platform hash test in W9. |
| GitHub is suspended: no CI. | Local gates as round ba. Push and CI when access returns, before checkpoint A. |
| Scope creep (internet, 8 players, win conditions). | Explicitly after checkpoint D. The code keeps them open (no hard-coded 6, a `team` field, the transport behind an interface). |
