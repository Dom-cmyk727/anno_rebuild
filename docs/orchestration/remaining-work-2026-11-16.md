# What is left, and who does what next (2026-11-16)

**What this is:** the orchestrator's assignment after a full read of the repository and its documents by a research
agent (the owner's request of 2026-11-16). The agent's plan (953 lines, with 29 area notes) stays outside git; this
page holds what the orchestrator checked, changed and assigned. Main is `ac02d4a2`.

**How to use it:** each lane works its list from the top, one open packet at a time. A task is released as before:
the Windows executor by the standing go after each review, the UI lane by the owner's go in its session, the Mac by
the owner pasting the prompt. "Packet" says whether the packet file exists yet. Nothing here changes a default, a
rule or a look without the owner.

## 1. Where the project stands

- **Done and checked against the original files:** the file formats, asset decoding, the CPU renderer and the GPU
  compositor.
- **Simulation (state 139):** deterministic, integer-only, with save, replay and a state hash. Wood, tools, bricks,
  salt, cloth, ships, founding, cargo and freight routes work. Houses, city income and the fisher exist behind
  switches that are off by default.
- **Missing in the simulation:** 16 of 25 building classes have no tick; trade with others, fire, research,
  military, diplomacy, defeat and win are absent.
- **No network code.** `src/net` does not exist and ADR 0003 is still "Proposed".
- **The application** is a viewer with a play mode started from the command line: no main menu, lobby, settings,
  named saves or audio. About one fifth of the in-game interface is built.
- **The LAN MVP has reached none of its three checkpoints.** The five-player map is still the placeholder the owner
  rejected on 2026-10-29; a town founded from a ship has never been shown to grow; none of the three acceptance
  measurements can run yet.
- **Documents are behind the code:** `PLAN.md` stops at 2026-11-10 and state 134, the README still says "no
  gameplay", `docs/sim/state.md` gives three state numbers.

## 2. The orchestrator's review of the plan

**Checked against the repository and confirmed:** state 139 and the three default-off switches; the placeholder
map; ADR 0003's status; the reject-code limit that has drifted (`freight.cpp:222` and `state_fields.hpp:1312` still
name `CityAccounting` as the last code, `reject.hpp` has one after it); the warning script that lacks a define CMake
passes; the window trace's reader that lists three columns in another place than the writer (confirmed with a real
trace of 2026-11-16: the reader would refuse it).

**Changed against the plan, with the reason:**

| Change | Reason |
|---|---|
| The Mac's order follows the critical path, not hygiene first: the town's start measured, wildlife, the network ADR, a first military read come before the limit and document clean-ups | The plan chose its first tasks by "needs no owner decision". These four need none either, and they are what the LAN target waits for longest |
| TOWN-START-1 moves from the second wave to the front | It measures where a ship-founded town stops, on the candidate map that is already a test fixture. Every later town task is ordered by its result |
| NET-ADR-1 moves to the front | Nothing of the network can start before ADR 0003 is approved, and it cannot be approved as written (its handshake contradicts ADR 0005 and 0006; it assumes six seats and a computer player) |
| A first military read (MIL-RE-1) moves from the fourth wave forward | Military is unread and the MVP's acceptance needs it. Until one read exists its size is unknown, and it is the plan's first risk |
| VIEW-TURN-0 (how the original turns the view) moves to the UI lane's first list | It is a read with no dependency, and the render lane's plan for the direct renderer should know it before the instance lists are designed |
| A new task OQ-562-1 | The same command recipe gives other numbers through the app's command path than through the sim-level harness (first founding 351 against 350 gold, final wood 1,472 t against 2,368 t). Unexplained since 2026-11-07; it has to be explained before two peers are expected to agree |
| The reader for the History Edition launcher's archive goes to the UI lane, before the main-menu study | Only the Windows PC has the files, and the archive holds what the main menu and key bindings need (menu layouts, texts, default keys) |
| DET-1a (a one-hour recording as the determinism oracle) is early in the Mac's second list | PLAN's M3 acceptance has never run; it is cheap and it is what a lockstep game stands on |

**Accepted as proposed:** SIM-LIMITS-1 goes to the Mac (all files are in `src/sim`); wildlife is ported behind an
ADR 0006 tag without waiting for EDITION-2; the network's headless core is planned for the Mac and its sockets for
the Windows executor (decided when ADR 0003 is approved).

**Not accepted without the owner:** drawing roads in play only after the direct renderer's second step
(PLAY-LIVE-1b). It avoids building it twice, but the owner then cannot see the roads he builds for weeks. Decision
D-3 below.

**A structural risk the plan understates:** almost the whole road to the LAN game (the town chain, the network
core, the sim's turn changes, the language index, the reads of fire and military) lies with one executor, the Mac.
The Windows executor is bound by the direct renderer and the UI lane by about 30 panel stages. Decision D-4 below.

## 3. Windows executor

| # | Task | What | Done when | Size | Packet |
|---|---|---|---|---|---|
| 1 | ZOOM-SMOOTH-2f | The move trigger refuses to draw from a thread that is not the window's | As the packet | 45 min | [zoom-smooth-2.md](tasks/zoom-smooth-2.md), released |
| 2 | ZOOM-SMOOTH-3c | Catch a slow `finish` call in the act | As the packet | 2 h | [zoom-smooth-3.md](tasks/zoom-smooth-3.md), queued |
| 3 | UNIX-WARN-1 | The Linux warning check ends green again | As the packet | 1 h | [unix-warn-1.md](tasks/unix-warn-1.md) |
| 4 | TRACE-COLS-1 | The window trace and its reader agree on the columns; before the bounce branch lands | As the packet | 1 h | [trace-cols-1.md](tasks/trace-cols-1.md) |
| 5 | RENDER-DIRECT-1a | Measure what the resident sprite atlases need and fix their layout (ADR 0007 step 1, first half) | One command reproduces the table; the worst scenario fits a stated budget on both GPUs, or a QUESTION | 2-3 h | to write |
| 6 | RENDER-DIRECT-1b | Pack the sprites into atlases on the CPU, exact to the pixel, behind a switch that is off | Every building and figure frame read back equals the decoder's output; no frame digest moves | 4 h | to write |
| 7 | LAYER-CHECK-1 | A build test that `sim` and `formats` include nothing from app, render, SDL or the GPU | Adding an SDL include to a sim file makes the test fail | 1-2 h | to write; any gap |

**After that, in this order unless the owner moves GATE-2a up (decision D-5):** RENDER-DIRECT-1c (the atlases on the
GPU, after GATE-MAC-1 has landed), 2a and 2b (instance lists per island; 2a also removes the 12-bit picture ids
that leave types above 4095 undrawn), GATE-2a, TEST-MEM-1, GATE-2b, GATE-SKIP-1 (one way to skip a test that needs
the game files), RENDER-DIRECT-3a to 6, VIEW-TURN-1, DET-1b (the one-hour recording replayed under MSVC, GCC and
Clang), GATE-TSAN-1 (a thread-sanitizer lane), PERF-LAN-1 (five towns at 3840 x 2160).

**To land from this lane:** ZOOM-SMOOTH-2 after 2f; ZOOM-SMOOTH-3 after 3c; the bounce branch after TRACE-COLS-1.
These land before the UI lane cuts a branch that binds keys in `viewer.cpp`.

## 4. UI Executor (interface and picture)

| # | Task | What | Done when | Size | Packet |
|---|---|---|---|---|---|
| 1 | UI-TABS-1 stage A | The sub-tabs switch and the player overview is reachable | As the packet | as packet | on `task/UI-TABS-1`, in work |
| 2 | UI-INFO-2b-1 | Read how the original draws the building preview: entrance arrows, garden, flags, the scaler for large sprites | Each of the 12 differing building types is explained by a rule with an address, or listed as open | 3 h | the lane writes it |
| 3 | VIEW-TURN-0 | Read how the original turns the view (sprite sets, draw order, camera, minimap), documents only | A note lists every rule a renderer must follow per view, with addresses | 2-3 h | the lane writes it |
| 4 | UI-PERF-1 | Remove the 73 ms first frame and the per-frame recomposition of the interface layer | Before and after numbers recorded; the target is set from the lane's own measurement first; no pixel expectation changes | 4 h | the lane writes it |
| 5 | UI-KEYS-1 | F5 to F8 speed, P pause, F10 quick-save (the owner's decision of 2026-11-16) | A window script finds the four speed commands; a paused session publishes no turn; F5 no longer saves | 3 h | draft stage S5 on `task/UI-PANELS-PREP` |
| 6 | UI-WAREHOUSE-1 stage W1a | Finish reading the warehouse and trade panel's code | OQ-651 and OQ-653 to 657 closed or narrowed with addresses | 3 h | draft on `task/UI-PANELS-PREP` |
| 7 | UI-TABS-1 stage B | Live figures in the overview | Rows equal the sim's read-only queries; compared with a History Edition frame | 3 h | after the owner's test of A |

Tasks 5 and 6 start after the orchestrator has landed UI-PANELS-PREP as documents; task 5 also after the Windows
lane's zoom branches have landed.

**After that:** UI-INFO-2b-2 (build what 2b-1 read, against History Edition frames), HE-RDA-1 (our own reader for
the History Edition launcher's archive: a spec, a Python reference, an `anno-dump` command, a corpus test), the
main-menu design study, FISHER-HE-1 step C (after the owner's clean History Edition run), UI-SELECT-1 (select a
building in a ship-start game), UI-START-VIEW-1 (decision D-7), the panel stages of the four draft packets as the
owner's recordings arrive, the lobby (needs a design from the owner).

## 5. Mac executor (simulation, reads of the original's code, network core)

| # | Task | What | Done when | Size | Packet |
|---|---|---|---|---|---|
| 1 | GATE-MAC-1 | Why three GPU timestamp tests fail on Metal, and the honest fix | As the packet | 3 h | [gate-mac-1.md](tasks/gate-mac-1.md), released |
| 2 | TOWN-START-1 | From the ship to the first houses, measured: a scripted opening through public commands on candidate A, with the `living` rules and the fisher on | A recording in which the first houses stand and upgrade, or the exact step where it stops, as a number | 3-4 h | to write |
| 3 | WILDLIFE-1a | The wildlife rules as plain functions with no state, checked against the 65 reference cases; nothing wired | 65 of 65 agree; named mutations fail; 0 pins moved | 3-4 h | to write |
| 4 | NET-ADR-1 | ADR 0003 brought in line with ADR 0005, ADR 0006 and the five-seat MVP; one page "what the owner decides" | The owner can approve or amend from one page; no contradiction with ADR 0005 or 0006 remains | 3 h | to write |
| 5 | MIL-RE-1 | A first read of the military code: the soldier class and the attack order, to size the area | A note with addresses, the list of what was not read, and an estimate of the reads still needed | 3-4 h | to write |
| 6 | ANNOSCENE-ORE-2 | How a placed ore deposit gets its tons, the marker without ore, the treasure's spot scan | Each rule has an address range and a check over our 429 templates | 2-3 h | to write |
| 7 | OQ-562-1 | Explain why the same recipe gives other numbers through the app's command path | The cause named with a test that shows it, or the two paths shown equal after a fix | 2-3 h | to write |
| 8 | SIM-LIMITS-1 | One definition for four hand-copied limits in the saved state | 0 pins moved; a new test fails when a reject code is appended without the shared definition | 2-3 h | [sim-limits-1.md](tasks/sim-limits-1.md), wording to refresh |
| 9 | DOCS-SIM-1 | One current statement of the rules switches, commands, ticking classes and empty hooks, with a check that fails when code and table disagree | The check fails on a deliberate omission and passes on main | 3-4 h | to write |

**After that:** DET-1a (the one-hour recording) and BENCH-SIM-1; WILDLIFE-1b and 1c, HUNTER-1, TRAPPER-1; the whaler
(a read first); MAP-ORES-1 (after the map is chosen); EVID-1 (citations for the newest sim code); FMT-SPEC-1 (four
gaps in our format specs); then, once ADR 0003 is approved, the network core NET-1 to NET-8, SIM-TURN-1 and
EDITION-2 (language text out of the hashed state); then fire, the player commands (tax, research, stop and start)
and the rest of the military reads.

## 6. The orchestrator's own work

| # | Task |
|---|---|
| 1 | Review UI-PANELS-PREP and land it as documents |
| 2 | Write the packets marked "to write" above before each is reached |
| 3 | Bring `PLAN.md`, the README and this board's old "Current work" section up to main `ac02d4a2` (documents only) |
| 4 | Add the landing rule of 2026-11-16 to `docs/testing/README.md` |
| 5 | Put the decisions below to the owner, each with a recommendation |

## 7. Decisions for the owner

Blocking or steering the lists above:

| # | Decision | Orchestrator's recommendation |
|---|---|---|
| D-1 | Which ship-start map is installed: A (open sea, 28 islands), B (linked channels, 38) or C (dense archipelago, 50); with filler density, ship spawns and start cargo. Open since 2026-10-31 | Look at the three in the viewer, then choose; the orchestrator prepares the three views |
| D-2 | Are living houses and city income on by default in the window? | Yes, once TOWN-START-1 shows a town growing; until then keep `original` |
| D-3 | Roads drawn in play now (about 3 hours, replaced later by the direct renderer) or only with the direct renderer's second step | Now: the owner's hands-on tests are the acceptance, and he cannot see what he builds |
| D-4 | A second executor for the simulation and network path, once ADR 0003 is approved | Yes: the road to the LAN game lies with one executor today |
| D-5 | Move GATE-2a (a test refused for memory is counted as refused and retried alone) ahead of the direct renderer? | No: since the landing split of 2026-11-16 this PC runs few large tests per landing; it stays after RENDER-DIRECT-1b |
| D-6 | The closest zoom's default: the original's 84 pixels per tile, or today's 118.8 with the original's as an option | The original's by default, the closer one as an option |
| D-7 | Does the viewer start like the History Edition (zoom level 1 on the player's ship)? | Yes |
| D-8 | The Mac's direction after its first list: the town chain (wildlife, hunter, whaler), the trade panel's groundwork, or the win and lose script actions | The town chain: 162 of 317 measured houses stop on goods that need it. This replaces the orchestrator's earlier recommendation of the trade panel |
| D-9 | ADR 0003 (network), the UDP port and the limits, after NET-ADR-1 | Decide from NET-ADR-1's one page |
| D-10 | Venetians and natives in the MVP; a dropped seat without a computer player; host migration inside or after the MVP; fish on the LAN map; whether `BuildCmd` enforces the building stage | With NET-ADR-1 and the map choice; the orchestrator prepares each with its options |

Not blocking, listed in the research plan and kept by the orchestrator: the 18 undecided rows of the enhanced
rules preset, 22 simulation decisions open since October, the four waiting playtest cards, an audio library, the
lobby's look, whether the interface becomes a library of its own.

**What the owner can do in parallel:** the History Edition recordings the panel packets ask for; one clean History
Edition run for FISHER-HE-1 step C; the four playtest cards, or a word that they are superseded.
