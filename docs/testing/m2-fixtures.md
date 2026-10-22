# T15 stage 1 — the M2 fixtures against the running original (plan)

**Status:** plan (2026-10-22), written before any fixture runs, per the stage-1 first step
(`docs/orchestration/TASKS.md` §4, 2026-10-21 u). One row per fixture of
[`tick-loop.md`](../re/tick-loop.md), [`production.md`](../re/production.md),
[`population.md`](../re/population.md) and [`fire-and-ships.md`](../re/fire-and-ships.md). It lists,
for each: what it shows, the memory values read (module + RVA + offsets, with the RE-note line each
comes from), the game action, whether that needs input, the run time, and whether `src/sim`
implements the subsystem (then the same inputs go through the sim and the numbers are compared —
the start of `anno-diff`). No run happens until the owner agrees to that session (§4/§5 below).

The original is the copy at `C:\anno1503-original\`, driven by `tools/orig/smoke.py`'s machinery
(the stage-1 harness extends it). Nothing of the original's output goes into git; runs write to
`cache/orig/<session>/`.

## How objects are located (bases rebase at run time)

The 2006 DLLs do **not** all load at their preferred `0x10000000`: only `AnnoDisplay.dll` keeps it,
the others rebase (`original-harness.md` §3; smoke.py's PEB walk reads each base). **Every address
is `module base + RVA`, resolved at run time** — the tables below give the RVA; the harness adds the
run's base for that module.

- **World object** (`AnnoWelt.dll`): the heap is scanned for a DWORD equal to the world vtable
  `AnnoWelt base + 0xccdd8` (tick-loop.md:24); the one whose ms counter `+0x100` is non-zero and
  whose tick `+0x10c == counter >> 1` is the world (tick-loop.md:25). smoke.py already does this
  (`find_world_candidates`). All `+0x…` world offsets below are from that object.
- **Net client** (`AnnoFrame.dll`): single-player `tNetClient` vtable `AnnoFrame base + 0x8914c`
  (tick-loop.md:30), multiplayer `tNetClientNet` vtable `AnnoFrame base + 0x893ec` (tick-loop.md:43);
  located by the same vtable-value scan. The game speed is the f32 at net-client `+0x11c`
  (tick-loop.md:62). Speed is also observable without reading this object, as the tick rate
  (ticks/s = 500 × speed at ≥ ~40 fps, tick-loop.md:25/§2).
- **Type objects / buildings** (`AnnoGame.dll` registry, `AnnoWelt.dll` island objects): a type
  object is one `ANLAGEN` entry (type vtable `AnnoGame base + 0x2185c`, production.md:21); the
  BGruppen object is at the AnnoGame game object `+0x5fdc` (population.md:221). A building's
  production fields hang off a sub-object attached by class id `0x1800` (production.md:113). A single
  building has no vtable-scan anchor, so for the production/population fixtures the **building info
  panel** (UI) is the primary observable (production.md:362, population.md:217); a memory read of the
  fields below needs the selected building's address (a selected-object pointer or a hook, to be
  pinned in the owner session — recorded as OQ-T15-BLD).

## Read-only by default

Reads use `ReadProcessMemory` only. Two fixtures as written **write** game memory — TL-F6 ("set
`+0xfc` = 0") and TL-F8 ("patch a client's `+0xfc`"). Writing is a **separate owner question**
(OQ-T15-WRITE); the tables give the read-only variant instead. Multiplayer fixtures (TL-F5, TL-F7,
TL-F8, PO-T5's MP clause) run only if two instances of the original run on this PC; otherwise each
is an OQ (OQ-T15-MP). The sim side of multiplayer pacing is M4 (net), not yet built.

## What `src/sim` implements (stages S0–S8 on `main`)

| Subsystem | Header | Fixtures it can be compared against |
|---|---|---|
| World clock, tick, Advance, the 100 ms step cap | `clock.hpp`, `tick.hpp` | TL-F1–F4 |
| Simulation PRNG (2048-entry MSVC `rand()` seed-1 table, pre-increment cursor) | `rng.hpp`, `draw.hpp` | TL-F6, FS-1 |
| Production step (gate `kStepTicks = 500`), type-object integers | `production.hpp`, `types.hpp` | PR-F1–F7, PR-F10–F12 |
| Houses (update gate 2500), taxes/upkeep (period 2000), BGruppen tables | `city.hpp`, `building.hpp`, `config.hpp` | PO-P1–P6, PO-T1–T4 |
| Figures exist but **do not move until S12** | `figure.hpp` | FS-8 facing only; FS-9–F12 not yet |
| Fire / plague / disasters **not ported** (island.hpp:276 "0 until ported") | — | FS-2–F7 not yet |

Where the last column below says **yes**, A6 feeds the same start state and inputs through `src/sim`
and compares the numbers in a doctest or a small `anno-diff` tool. Where it says **no**, the original
measurement stands alone and a mismatch with a later sim stage becomes that stage's fixture.

## 1. tick-loop.md — clock, speed, PRNG, multiplayer (F1–F9)

| Fx | Shows | Memory read (module+RVA+offset · RE line) | Game action | Input? | Run | sim? |
|---|---|---|---|---|---|---|
| TL-F1 | Game clock advances real-time at 100 %; `+0x10c == +0x100 >> 1` always | AnnoWelt world `+0x100`/`+0x104` (64-bit game ms) and `+0x10c` (tick) · tick-loop.md:25; game-seconds via property-7 getter slot `+0x74` = RVA `0xa3380` · :26 | SP scenario at 100 %, left unpaused, sample ~1/s for 60 s | **no** (owner starts the scenario) | 60 s | **yes** — feed the measured wall-time/dt through `clock.hpp`, check the `>>1` invariant and the 500 ticks/s rate |
| TL-F2 | Speeds 200/400/50 % scale the clock | world `+0x100`/`+0x10c` as TL-F1; speed f32 at net-client `+0x11c` · tick-loop.md:62 (object via AnnoFrame vtable `0x8914c` · :30) | keys F6/F7/F8 (→200/400/50 %) · tick-loop.md:63 | **yes** (owner presses, or `SendInput` if agreed) | ~60 s each | **partial** — the `dt·speed+0.5` round and 100 ms cap are in `clock.hpp` (SetSpeed, S8); compare the per-step ms and the rate; 400 % is SP-only (:63) |
| TL-F3 | A 5 fps limit → 100 ms/frame → +30 s per 60 s | world `+0x100` · tick-loop.md:25 | a frame limiter forcing ~5 fps, 100 % | **config** (no keys): try dgVoodoo2's framerate-limit setting; **OQ-T15-F3** if it does not throttle the DirectDraw 2D path | 60 s | **yes** — step the sim with 200 ms frames, expect the 100 ms cap per step (tick-loop.md:35) |
| TL-F4 | Pause freezes the clock | world `+0x100`/`+0x10c` unchanged · tick-loop.md:25 | pause (frame command `0x70`, SetSpeed 0) · tick-loop.md:64 | **yes** (owner presses pause) | 60 s | **yes** — speed 0 advances nothing |
| TL-F5 | 2-player LAN: both clients +60 s, host 600 turns, 6000 `Advance(10)` | each client's world `+0x100` · tick-loop.md:25/:126 | two instances in a LAN game | **OQ-T15-MP** (needs two instances on this PC) | 60 s | **no** (M4) |
| TL-F6 | PRNG table and cursor (read-only variant) | AnnoWelt PRNG table at RVA `0xcb8f8` (2048×u16, .rdata) · tick-loop.md:70; cursor `+0xfc` (u16) and length `+0xfe` · :70; draw slot `+0xb4` = RVA `0xa1740` · :70 | SP scenario running (the cursor advances as the sim draws) | **no** (read-only; the written variant "set `+0xfc`=0" is OQ-T15-WRITE) | 30 s | **yes** — compare the 2048-entry table byte-for-byte with `rng.hpp`'s MSVC `rand()` seed-1 table and the pre-increment rule (FS-1 numbers) |
| TL-F7 | Determinism: identical MD5 and save bytes on two machines for the same seed+log | — (MD5 of the state stream; save files) · tick-loop.md:101–102/:128 | two machines, same seed `0x1009b978` and command log | **OQ-T15-MP** | — | **no** (M4) |
| TL-F8 | Desync detection and resync after patching a client | world `+0xfc` written; control msg `0xd6` · tick-loop.md:104–105/:129 | LAN game, write one client's cursor | **OQ-T15-MP + OQ-T15-WRITE** | ≤ 61 s | **no** (M4) |
| TL-F9 | Real-time intervals (sync id 60 s, server 500 ms, 2 s wait dialog, 10-min SP autosave) | — (observe files/dialogs; autosave name built in `0x10006310`) · tick-loop.md:19/:130 | SP run long enough for one autosave; MP for the rest | SP autosave **no**; MP parts **OQ-T15-MP** | 10 min (SP autosave) | **no** |

## 2. production.md — type integers and production steps (F1–F14)

Stocks are 8.8 (256 = 1 t); a step gates every 500 ticks (production.md:15; sim `kStepTicks`). Type
integers (production.md:55–65): output/step `+0x210`, input0/step `+0x202`, input1/step `+0x204`,
output-0 cap `+0x208`, per-minute display `+0x20c`; input count `+0x1fb` (production.md:46), growth
period `+0x1fe` (production.md:69). Building
production fields (sub-object, class `0x1800`, production.md:113): output stock `+0x18`, input stocks
`+0x1c`/`+0x1e`, activity `+0x20`, switched-on `+0x24` (production.md:118); farm raw stock `+0x1c`
(production.md:173); dual-line outputs `+0x18`/`+0x1a`, inputs `+0x20`/`+0x22` (production.md:152);
status at building `+0x0d` bits 5–7 (production.md:113).

| Fx | Shows | Memory read (module+RVA+offset · RE line) | Game action | Input? | Run | sim? |
|---|---|---|---|---|---|---|
| PR-F1 | 921 type objects hold the precomputed integers | type object (AnnoGame vtable RVA `0x2185c`) `+0x210`/`+0x202`/`+0x204`/`+0x208`/`+0x20c` · production.md:55–65; growth period `+0x1fe` · :69; :366 | none — static, at load | **no** | — | **yes** — `TypeRegistry` already derives these from `Anlagen.dat`; spot values (1001→6/11/0 cap 1280, 1017→17/21/13, …) are pinned in sim tests. Runtime read is optional confirmation (building address: OQ-T15-BLD) |
| PR-F2 | Weaver 1001: cloth +6/wool −11 per step; status 1 after wool runs out | prod sub-object `+0x18` cloth, `+0x1c` wool · production.md:118; status building `+0x0d` · :113; info panel · :362 | place weaver, wool 1280, switch on, no carriers; watch ≥117 steps | **yes** (build/setup via menu) | ~117 s | **yes** |
| PR-F3 | Reduced step at 50–100 % input: ratio 93 → +4/−7 | `+0x18`/`+0x1c` · production.md:118/:139 | weaver, wool 8, one step | **yes** | ~2 s | **yes** |
| PR-F4 | Output cap overshoot then status 2 ("storage full") | `+0x18` (→1285 vs cap 1280), status `+0x0d` · production.md:118/:113 | weaver, wool plenty, cloth 1279 | **yes** | ~2 s | **yes** |
| PR-F5 | Ore smelter 1017: iron+17/ore−21/wood−13; ratio step | dual-line `+0x18` iron, `+0x20`/`+0x22` inputs · production.md:152 | place smelter, ore 1280, wood 25 | **yes** | ~3 s | **yes** |
| PR-F6 | Grain farm 811: raw→corn via workers | farm `+0x1c` raw, `+0x18` corn · production.md:173 | place farm, 1 worker, raw 160 | **yes** | ~3 s | **yes** |
| PR-F7 | Workshop activity curve `a←(a·127>>7)+48` | activity `+0x20` · production.md:118/:372; panel value `a·60>>10` | weaver producing from activity 0 | **yes** | ~30 s | **yes** |
| PR-F8 | Field 712→713 after ≈155 000 t; forest 104→105 ≈240 000 t | building type object `+0x14`; counter (field growth) · production.md:373 | place a grain field, wait | **yes** | ~310 s | **partial** — field growth/conversion is edge of S-scope; mark OQ if sim lacks it |
| PR-F9 | Ore mine 1205: +21/step to ≥1792 then status 2; deposit decay (unknown rule) | `+0x18`, status `+0x0d`; deposit amount (slots `+0x88`/`+0x8c`) · production.md:374/:394 | place mine on fresh deposit | **yes** | ~10 s | **partial** — mine step yes; deposit-decay rule is an Open in production.md → OQ |
| PR-F10 | City cap 50 t clamps an add (12 700 + 300 → 12 800) | city store `+0x334+12g` (u32 stock 8.8), capacity `+0x624` (city vtable `0x100c2f40`) · production.md:325 | add goods to a city warehouse | **yes** | ~1 s | **yes** (city.hpp warehouse) |
| PR-F11 | Market cart picks the higher score (30 vs 20); ≥255 never chosen | market/producer stocks; cart choice (observe destination) · production.md:376 | market + two producers in range | **yes** | ~10 s | **yes** (market scoring) |
| PR-F12 | Score `((avail−1344)·128)/448` truncates; second chosen | producer stocks; chosen cart · production.md:377 | market + grain farm pair | **yes** | ~10 s | **yes** |
| PR-F13 | `WARETYPE` mapping: file number ↔ same-named number | save file fields · production.md:378/§7 | load a savegame | **yes** (load) | ~5 s | **yes** (goods id map) |
| PR-F14 | Timing: 116 steps ≈ 116 s at 100 % (1 step = 1 s) | world `+0x10c` deltas over PR-F2 · production.md:379 | PR-F2 timed | **yes** | ~116 s | **yes** — 500 ticks/step × 2 ms = 1 s/step (TL-F1) |

## 3. population.md — houses, residents, taxes (P1–P6, T1–T5)

House update gates every 2500 ticks, tax/upkeep every 2000 (sim `House` gate 2500, `upkeep_period`
2000). Observables (population.md:217): house residents `+0x2c` (and `+0x2e`/`+0x30…`), city stage
mask `+0x618`, player money `+0x74`; BGruppen object at AnnoGame game `+0x5fdc` (population.md:221).

| Fx | Shows | Memory read (module+RVA+offset · RE line) | Game action | Input? | Run | sim? |
|---|---|---|---|---|---|---|
| PO-P1 | BGruppen `Menge`/`VermehrTab` integers and the KAPELLE/WIRTSHAUS lookups | BGruppen object at AnnoGame game `+0x5fdc` · population.md:221 | none — static, at load | **no** | — | **yes** — `BGruppen.dat` tables are parsed; compare the dumped integers |
| PO-P2 | Pioneer house, no supply: residents 5.5→8 over 11 updates; mood falls | house `+0x2c` residents, `+0x0c`/`+0x0d` mood · population.md:217/:222 | place a pioneer house, cut supply | **yes** | ~55 s (11×2500 t) | **yes** |
| PO-P3 | Fed house: +0.5 residents/update, 8 after 6 updates | house `+0x2c` · population.md:217/:223 | pioneer house, food+leather ≥100 % | **yes** | ~30 s | **yes** |
| PO-P4 | Settler, supply cut: residents fall by mood-scaled step; downgrade <6 | house `+0x2c`, type `+0x14` · population.md:224 | settler house, cut supply | **yes** | ~60 s | **yes** |
| PO-P5 | Starvation to mood −2: −16/64 per update; removed at <1 | house `+0x2c`, mood `+0x0c` · population.md:225 | pioneer starved | **yes** | ~60 s | **yes** |
| PO-P6 | Stages 1A/1B/1C at 30/80/125 pioneers within ≤2500 t | city stage mask `+0x618` · population.md:217/:226 | grow a city past the thresholds | **yes** | minutes | **yes** |
| PO-T1 | Tax income: STEPPE tax 1, 300 NAHRUNG+100 LEDER → +88 per 2000 t | player money `+0x74` · population.md:217/:227 | run a taxed city one 2000-t period | **yes** | ~4 s | **yes** |
| PO-T2 | Same at tax 0/2 → +86/+90 | money `+0x74` · population.md:228 | change tax level | **yes** | ~4 s | **yes** |
| PO-T3 | Running costs: −8 per 2000 t (civil 100 + mil 20 /min) | money `+0x74`; city page costs · population.md:229 | city with upkeep, no sales | **yes** | ~4 s | **yes** |
| PO-T4 | Sales-revenue smoothing → 15·I (104/105 per booking) | city page "Sales revenue" · population.md:230 | constant income ≥5 game min | **yes** | ~5 min | **yes** |
| PO-T5 | "Change taxes" cycles 0→1→2→0 (SP); no-op in MP | world tax (property 10) · population.md:231/:239 | press the taxes button | **yes** (SP); MP clause **OQ-T15-MP** | ~2 s | **yes** (SP) |

## 4. fire-and-ships.md — PRNG, fire/plague, ship facing (1–12)

| Fx | Shows | Memory read (module+RVA+offset · RE line) | Game action | Input? | Run | sim? |
|---|---|---|---|---|---|---|
| FS-1 | Shared PRNG pre-increment: `s=0` → 18467,6334,26500,19169 | same as TL-F6 (table RVA `0xcb8f8`, cursor `+0xfc`) · fire-and-ships.md:173, tick-loop.md:70 | none — static | **no** | — | **yes** (same table as TL-F6) |
| FS-2 | Flame damage: 2500-step hp decay, collapse step count by `M` | building hp (`+0x…` damage record) · fire-and-ships.md:174 | set a building on fire (script) | **OQ** — fire **not ported** in sim (island.hpp:276); original-only until the fire stage | ~5 min | **no** |
| FS-3 | Flames per building from `FIREPOS` points | damage records · fire-and-ships.md:175 | ignite a 2-point house | **OQ** (fire not ported) | ~min | **no** |
| FS-4 | Spread cadence: one try per 9 steps, range 1 | neighbouring buildings · fire-and-ships.md:176 | let a fire spread | **OQ** | ~min | **no** |
| FS-5 | Random fire risk per 12 500 t (`r mod b < 4`) | city house count `s`, draw via `+0xb4` · fire-and-ships.md:177 | city of N pioneer houses | **OQ** | ~min | **no** |
| FS-6 | Plague risk and cure chance | house infection fields · fire-and-ships.md:178 | city with population `P` | **OQ** | ~min | **no** |
| FS-7 | Damage-ignition table by source class/level | damage source `+0xb0` · fire-and-ships.md:179 | apply weapon damage | **OQ** | ~min | **no** |
| FS-8 | Ship/soldier initial facing equals `attr` | figure sprite dir vs scenario `attr` · fire-and-ships.md:180 | load `0200Single/*.sze` | **no** (load, read dir) | ~5 s | **partial** — the figure's `attr`→`dir` is derivable in sim; movement (FS-9–12) is S12 |
| FS-9 | `dir16`/`dir8` from vectors | — (computed) · fire-and-ships.md:181 | n/a (formula) | **no** | — | **partial** — lookup table, compare once S12 lands |
| FS-10 | Ship turning phase snapping | ship state (float) · fire-and-ships.md:182 | order a galleon to reverse | **OQ** — movement is float, not a lockstep target (fire-and-ships.md:168); S12 | ~s | **no** |
| FS-11 | Ship speed factor from hp/cargo | ship `+0x3c` speed factor · fire-and-ships.md:183 | damage/load a ship | **OQ** (S12, float) | ~s | **no** |
| FS-12 | Route file layout stable across a save | route entries in the `.sze`/save · fire-and-ships.md:184 | save a routed ship | **OQ** (S12) | ~s | **no** |

## 5. Sequencing of the runs

1. **No input, read-only (first owner session, A4):** TL-F1 (clock rate + `>>1` invariant), TL-F6
   read-only (PRNG table + cursor growth), the PRNG cursor sampled over the same run; PR-F1 and PO-P1
   are static load-time comparisons the sim already covers (confirm, no run needed). Also one
   time-boxed (15 min) try at a true dgVoodoo2 window via `dgVoodooCpl.exe` (§4 2026-10-21 u); if it
   fails, full-screen stays and the report says so.
2. **Needs keys/building setup (second owner session, A5):** TL-F2 (speeds), TL-F4 (pause), TL-F3
   (frame-limiter config), the production steps (PR-F2…F14), houses (PO-P2…P6), taxes (PO-T1…T5).
   One action at a time, each followed by a screenshot and the memory reads.
3. **Multiplayer (only if two instances run here):** TL-F5, TL-F7, TL-F8, PO-T5's MP clause —
   otherwise OQ-T15-MP.
4. **Writing memory (separate owner yes):** TL-F6 set-cursor, TL-F8 patch — OQ-T15-WRITE.

## 6. Open questions this plan raises

- **OQ-T15-F3** — does dgVoodoo2 throttle the DirectDraw 2D path to a fixed fps (framerate-limit
  setting)? If not, F3 cannot be run with dgVoodoo2 and needs another limiter or is dropped.
- **OQ-T15-MP** — can two instances of the original run a LAN game on this one PC (loopback)? If not,
  TL-F5/F7/F8 and PO-T5-MP are deferred to a two-machine session.
- **OQ-T15-WRITE** — the owner's decision on writing game memory (`WriteProcessMemory`) for the
  TL-F6/TL-F8 written variants; the read-only variants above need no write.
- **OQ-T15-BLD** — how a specific building's object address is obtained for a memory read (the
  selected-object pointer, or a small hook at the building tick); until then production/population
  fixtures read the UI info panel (screenshot) and compare the shown numbers.
- Fire/plague (FS-2…F7) and figure movement (FS-9…F12) are measured in the original now but have no
  `src/sim` counterpart yet (fire not ported; movement is S12, float, not a lockstep target); each
  original measurement becomes the fixture for that later sim stage.
