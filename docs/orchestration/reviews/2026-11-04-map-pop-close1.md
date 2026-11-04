# MAP-POP-CLOSE-1: PARTIAL retained; the missing dependency is narrower than reported

Reviewed by the Claude orchestrator session on the Windows PC, on the owner's instruction of 2026-11-04 ("Review the
Mac executors results and provide a new set of tasks").

**Identity:**
- Mac executor report of 2026-11-04;
- local head `1e65ea7d` on `task/MAP-2-original-like`, 37 commits after `135edf6f`;
- frozen gate source `719f394a`; `aed5702a` and `1e65ea7d` change docs only.

The head was unpublished when this review began. During the review, the executor pushed it at the owner's request:
a plain fast-forward of `origin/task/MAP-2-original-like` from `73e7ab1a` to `1e65ea7d`, with no force and a clean
tree. The source was then checked (§6). The gate logs and the reviewer ledger stay on the
Mac. The review checks the report against the board (`MAP-POP-CLOSE-1`, C0–C5) and checks the decisive claims against
the original files on this PC. Its `AnnoWelt.dll` has the same SHA-256 as the Mac review's, `f220a878510f740c4cfb7224ade0a260e9a0fd7e99113e1201e7a37c0dfa6ae2`.

The last accepted and published MAP pin stays `73e7ab1a` (stream 127, PRNG timeline 4). Windows' inputs are unchanged.

## 1. Verdict

**PARTIAL acknowledged, not accepted.** The stop is allowed by C4 item 6. But its stated reason is wrong in two ways
(R1, R2). And the narrow Inn supply path it names as "outside this package" was explicitly in scope under C2 item 3.

The work is retained as the executor's evidence within the limits it states:
- the F1–F4 and G1–G3 corrections;
- the P-14 and P-15 closures;
- first assignment on the public build path;
- the real-data growth from 320 to 512;
- the five save/reload cuts;
- the separately built replay;
- the gates at `719f394a`: Debug 544 selected, 543 executed, 0 failed, 1 skipped (opt-in); ASan/UBSan 343/343.

The gate counts are as reported; they were not re-read here.

The next assignment is **MAP-POP-CLOSE-2** in TASKS: the Inn supply path and the first real upgrade.

## 2. Findings

### R1: [P2] No production chain is needed; the Inn's carrier already exists

The report says tavern stock "only comes from the Inn's own tick, its alcohol carrier and a production chain", all
unported. The original data and code show a much smaller dependency.

- **The scenario already holds alcohol.** In `endless03.sze`, the city of player 4 (state block index 4) starts with
  **50 t ALKOHOL**. Its full store (`STADT2/WARE`, u16 goods number and u16 tons, names through the file's `WARETYPE`)
  is NAHRUNG 50, ALKOHOL 50, WERKZEUG 50, HOLZ 50, ZIEGEL 50.
  - Across all 50 shipped scenarios there are 272 city rows, and 58 of them hold alcohol.
  - No producer is needed while that stock lasts.
- **The "alcohol carrier" is the workshop carrier.** The Inn tick sends **job `0x33`** through the base send `+0x118`
  (`0x10072d20`). In the job factory, `0x33` is case `0x1003b11a` with vtable `0x100c45e8`. That is the workshop
  carrier (TRAEGER, route type 51 = `0x33`), already read in `docs/re/transport.md` §2 and `docs/re/production.md` §4.1, and
  implemented since S11 (`docs/sim/state.md`, "The workshop carrier job is 0x33").
  - Its source filter fetches from the warehouse and from producers.
- **The Inn itself is small and fully readable.** Its methods are listed in §4: tick, take, unload, availability,
  load and save, about 150 instructions in total.

The executor's own code says the same. At `1e65ea7d`, the comment on `struct Inn` (`src/sim/include/anno/sim/building.hpp`)
reads: "The Inn's own tick (0x100755f0, every 5000 ticks) is not ported: it only decays the statistic and sends the
alcohol fetch job 0x33." The Inn's state, its take and unload, and its persistence are already in place
(`building.hpp`, `resident.cpp:194–250`). What is missing is the tick and its dispatch of the existing job.

The Inn path is therefore an "already-scoped Inn ... narrow supplier state/availability/debit path". C2 item 3 told the
executor to finish exactly that before the canary.

### R2: [P2] A working Inn alone would not upgrade the house on `endless03`

The report presents the tavern as the one blocker. The upgrade check (`0x10075dd0`–`0x10075fc4`, read for this review)
scores **all** of the next group's forced needs. It reads them through the current group's lookup, with each value
capped at 128:

- **Goods:** the mean over the forced goods slots `1 … cc−1`, plus the forced `0x4c` (WIRTSHAUS) from the infra range.
  The mean is truncating, and it is 128 if there is none. It indexes the next `BedarfTab`.
- **Infrastructure:** the minimum over the forced infra slots, starting at 160. It indexes the next `InfraTab`.
- **Food:** slot 0 against the **next** group's NAHRUNG `Menge`. It indexes the next `NahrungTab`.
- **The test:** every index is `(x + 4) >> 3`, and all three table values must be `> 0`. Only then is city slot
  `+0x94` asked.

For SIEDLER, the shipped `BGruppen.dat` forces LEDER, STOFFE, SALZ, WIRTSHAUS and KAPELLE. The first positive table
entries give these thresholds:

| Check | Inputs | Threshold |
|---|---|---|
| Food | NAHRUNG | ≥ 108 (`NahrungTab` index 14) |
| Goods | mean of LEDER, STOFFE, SALZ and WIRTSHAUS | ≥ 68 (`BedarfTab` index 9) |
| Infrastructure | minimum of WIRTSHAUS and KAPELLE | ≥ 84 (`InfraTab` index 11) |

The city on `endless03` starts with no leather, cloth or salt in store, and the report names no producer that makes
them. Unless one does during the run, the goods mean stays at most 128 / 4 = 32, and `BedarfTab[4]` = −1. The upgrade
is then refused whatever the Inn does.

The real upgrade also needs the chapel. Its supplier is cheap: a PublicService tick (`0x1006b4e0`) only advances its
500-tick gate, and its `+0xac` (`0x1006b520`) bills with take 0. So a resident's visit is the whole supply.

**Consequence:** the progression proof needs a different original city. 28 city rows stock all five of NAHRUNG, LEDER,
STOFFE, SALZ and ALKOHOL, plus WERKZEUG, HOLZ and ZIEGEL (§5). The upgrades to settlers, citizens and merchants all need
the Inn, because each of those groups forces WIRTSHAUS; only the aristocrats do not.

### R3: [P3] The 35,000 ticks before the first delivery are not explained

The first resident delivery arrives after about 35,000 ticks, which is 70 s of game time. The report says the wait
"follows the cart precedent and was not run against the original".

The wait itself is a read. `resident_job.cpp:670–674` at `1e65ea7d` cites step B's start at `0x10056330`: the figure
type's LOGIC f32 `[4]` through figure slot `+0xe0` = `0x10035260`, rounded to `500·d` ticks (`docs/re/transport.md`
§4.7). So "not run against the original" means only that the timing is not observed in the running game.

What is still missing is the sum. Split the 35,000 ticks into their terms (spawn gate, route, walk, stall wait,
return), each with its address, so that the delay can be judged and later compared with the original.

### R4: [P3] Two report gaps

1. **The docs handoffs are not mentioned.** CLOSE-1 C5 item 6 assigned the MAP handoffs from Windows playtest review §7:
   the stale `0x10049340` founding text, the tool's land model against the renderer shore 16.5, and the BEL escapes.
   The report does not say whether they were done.
2. **The sanitizer selection may miss changed route code.** The ASan/UBSan selection omits the route suites, but P-15
   changed route validation. If that change touched shared route code (not only resident files), the route suites
   belong in the sanitizer run. The report should name the files.

### R5: Note: two counts that happen to be equal

The report counts 58 city rows with an Inn. This review counts 58 city rows with alcohol in store. These may simply be
the same towns (an Inn and an alcohol stock tend to go together), but check it cheaply against PRODTYPE 66 (WIRT).

## 3. The executor's two decision requests

**D1: stream 128 amended in place, with the `HouseFirstAssignment` draw site active inside PRNG timeline 5. Confirmed.**

Stream 128 and timeline 5 were never accepted or published. The published MAP branch is at `73e7ab1a`
(`tools/sim_stream.py` `STATE_HASH_VERSION = 127`, `kPrngTimelineVersion = 4`). Windows consumes `0c401c8d` now and
`73e7ab1a` later. An unreleased candidate identity may be amended until acceptance freezes it.

CLOSE-2 adds the Inn state under the same 128/5, again in place, with these conditions:
- No retained artifact in a superseded 128 layout is used as a test or replay input.
- `tools/sim_stream.py` describes the final 128 layout.
- The report lists each amendment.

**D2: the upgrade path. Neither option as offered.**

Do not port a production chain, and do not accept a fixture Inn with stock:
- **No production chain:** R1 shows it is unnecessary.
- **No fixture stock:** it is not original data, and the board forbids it.

Instead:
- implement the narrow Inn path (§4), reusing job `0x33`;
- confirm the chapel and stall supply for the settler needs;
- run the progression in an original city that stocks those goods (§5).

**The three accepted P3s stay accepted:**
- **Route adapter:** add the first-step-from-a-stall-into-a-wall test now. The Inn and the chapel become new endpoints.
  The later code check narrows this P3 (§6, P3 1).
- **Mod successors:** mods declare special successors explicitly.
- **Cycle check:** the quadratic cycle check stays, bounded by the goods count.

## 4. Original evidence read for this review

All addresses are in AnnoWelt. These are static reads; no game run.

| What | Address | Reads |
|---|---|---|
| Inn class (PRODTYPE 66 WIRT) | ctor `0x10075970`, vtable `0x100c96d8`, size `0x24` | `docs/sim/state.md` class table |
| Inn tick | `0x100755f0`–`0x100756c8` | gate `(u16)(t − last) ≥ 5000`, then `last += 5000`. Demand `+0x1a` = demand · 31 / 32 (truncating). Stop if flags `+0x0c & 0x30`. Count the non-zero walker slots `+0x1c`, `+0x20`; stop unless the count is below type `+0xb8(0)`. Stop if stock `+0x18` ≥ `0x100` **and** stock ≥ demand / 2 (u16 `shr 1`). Else send job `0x33` with figure type `type+0xb4(0,0)`; on success call figure `+0x84(0)`, then figure slot `+0` with `type+0xb0(0)` (likely the goods to fetch; confirm) |
| Inn take, slot `+0xac` | `0x100756e0` | `n = min(request, stock)`; demand `+= n`; stock `−= n`; city `+0x88(type+0xb0(0), n, arg3, 0)`, a bill with take 0; returns `n` |
| Inn unload, slot `+0xc0` | `0x100756d0` | stock `+= amount` (no `·16`, unlike the house's `0x10076610`) |
| Inn availability, slot `+0xa4` | `0x10075940` | returns stock |
| Inn Load / Save, slots `+0x7c` / `+0x80` | `0x10075740` / `0x100757c0` | a 10-byte class record (Load takes it when the record length is 10): u16 `[2]` = stock, u16 `[8]` = demand. The original saves tavern stock; the community note "a tavern's alcohol is not saved" does not hold for the Inn object |
| Job `0x33` | factory `0x1003b0e0` case `0x1003b11a`, vtable `0x100c45e8` | the workshop carrier (`transport.md` §2) |
| PublicService (KAPELLE …) | tick `0x1006b4e0`; `+0xac` `0x1006b520` | gate only (500); bill with take 0 |
| Upgrade check | `0x10075dd0`–`0x10075fc4` | as in R2 |

## 5. Original cities that stock the settler goods

All the values below are tons at scenario start. They come from `STADT2/WARE`, read with the container functions of
`tools/sze.py`. The scripts are not committed.

**Criteria:** every listed city holds NAHRUNG, LEDER, STOFFE, SALZ and ALKOHOL, and WERKZEUG, HOLZ and ZIEGEL.

**Not checked here:**
- buildings: an Inn, a chapel, stands and free land;
- group counts and the player mask.

The executor checks these when it picks a city (CLOSE-2 K0).

| Scenario | Player | PROD entries | Food / leather / cloth / salt / alcohol | Tools / wood / bricks |
|---|---|---|---|---|
| `0011SingleEndlosAddon/endless02.sze` | 8 | 166 | 50 / 20 / 20 / 20 / 20 | 190 / 190 / 190 |
| `0100Kampagne01/S01_02_01.sze` | 1 | 117 | 100 / 100 / 100 / 100 / 100 | 100 / 100 / 100 |
| `0100Kampagne01/S01_02_01.sze` | 2 | 98 | 100 / 100 / 100 / 100 / 100 | 100 / 100 / 100 |
| `0100Kampagne01/S01_02_01.sze` | 2 | 72 | 160 / 160 / 160 / 160 / 160 | 160 / 160 / 160 |
| `0100Kampagne01/S01_07_01.sze` | 0 | 221 | 100 / 86 / 110 / 110 / 49 | 70 / 32 / 50 |
| `0100Kampagne01/S01_07_01.sze` | 1 | 151 | 88 / 35 / 67 / 88 / 72 | 60 / 61 / 60 |
| `0100Kampagne01/S01_07_01.sze` | 6 | 94 | 87 / 88 / 24 / 86 / 55 | 68 / 70 / 54 |
| `0100Kampagne01/S01_09_01.sze` | 0 | 165 | 140 / 80 / 100 / 60 / 120 | 170 / 170 / 170 |
| `0100Kampagne01/S01_09_01.sze` | 1 | 64 | 117 / 107 / 119 / 119 / 120 | 120 / 120 / 120 |
| `0100Kampagne01/S01_10_01.sze` | 1 | 117 | 148 / 150 / 145 / 150 / 144 | 86 / 150 / 150 |
| `0100Kampagne01/S01_12_01.sze` | 2 | 91 | 154 / 170 / 166 / 169 / 169 | 170 / 170 / 170 |
| `0200Single/Single_24.sze` | 0 | 147 | 189 / 50 / 190 / 190 / 187 | 50 / 50 / 10 |
| `0201SingleAddOn/single30.sze` | 6 | 24 | 50 / 50 / 50 / 50 / 50 | 50 / 50 / 50 |
| `0201SingleAddOn/single32.sze` | 2 | 122 | 120 / 120 / 120 / 120 / 120 | 50 / 50 / 50 |
| `0201SingleAddOn/single32.sze` | 3 | 275 | 190 / 100 / 100 / 100 / 190 | 150 / 150 / 150 |
| `0201SingleAddOn/single32.sze` | 4 | 124 | 100 / 20 / 20 / 50 / 100 | 60 / 20 / 20 |
| `0201SingleAddOn/single36.sze` | 7 | 90 | 90 / 50 / 50 / 50 / 50 | 50 / 50 / 90 |
| `0201SingleAddOn/single38.sze` | 1 | 133 | 50 / 50 / 50 / 50 / 30 | 100 / 100 / 100 |
| `0201SingleAddOn/single38.sze` | 4 | 289 | 190 / 190 / 190 / 190 / 190 | 100 / 190 / 100 |
| `0201SingleAddOn/single39.sze` | 0 | 8 | 50 / 50 / 10 / 50 / 50 | 50 / 50 / 50 |
| `0201SingleAddOn/single39.sze` | 2 | 88 | 150 / 51 / 51 / 51 / 51 | 100 / 100 / 100 |
| `0201SingleAddOn/single39.sze` | 3 | 152 | 190 / 190 / 190 / 190 / 190 | 150 / 150 / 150 |
| `0201SingleAddOn/single40.sze` | 0 | 74 | 50 / 50 / 50 / 20 / 50 | 50 / 50 / 50 |
| `0201SingleAddOn/single40.sze` | 1 | 34 | 50 / 50 / 50 / 50 / 50 | 50 / 50 / 50 |
| `0201SingleAddOn/single41.sze` | 0 | 82 | 100 / 20 / 30 / 20 / 30 | 100 / 50 / 10 |
| `0201SingleAddOn/single41.sze` | 1 | 30 | 100 / 50 / 50 / 50 / 50 | 50 / 50 / 50 |
| `0201SingleAddOn/single41.sze` | 2 | 33 | 60 / 50 / 50 / 50 / 50 | 50 / 50 / 50 |
| `Tutorial/02Trade01.sze` | 2 | 24 | 50 / 30 / 30 / 50 / 50 | 50 / 50 / 30 |

A player appears twice when it has two towns.

## 6. Source checks and limits

After the push, an independent read-only reviewer checked the corrections in `135edf6f..1e65ea7d` against the code.
It found no P1 or P2. Every claimed fix holds:

| Item | Result | Where |
|---|---|---|
| F1 | `house_updates` with unsaved billing is refused at create and at load; no production path changes the rules of a live world; replay starts from a validated world | `house_tick.cpp:340`, `scenario.cpp:319`, `invariants.cpp:242` via `save.cpp:296` |
| F2 | the record re-reads the committed slot after the effects | `house_tick.cpp:181`, :238, :242 |
| F3 | the reference mirrors `player_flags_set`; rows for bits 25/27/28 tell the old and new rule apart; controls for 24, 26, a low stage and a skipped OR | `tools/house_live_ref.py:87`, `house_tick.cpp:18` |
| G1 | `BuildCmd` runs SetGroup with a null old group; the scenario import, founding, load and rule-off paths do not; the atomic dry run draws nothing | `apply.cpp:445`, :499, `create.cpp:281` |
| G2 | the registry and the delivery walk the same lookup; delivery faults before any credit | `house_groups.cpp:331`, `resident.cpp:288` |
| G3 | the domain is bounded before any draw, charge or serial | `house_transition.cpp:529` |
| P-15 | every step must be a cardinal neighbour inside the rectangle, so no jump; `transport.cpp` (`path_home`) and `harvest.cpp` are unchanged | `walk_search.cpp:403–420` |

It also found that the Inn payload is saved and hashed, and that the class payload is part of the registry digest. So
a save in a superseded 128 layout fails on the digest instead of being misread. That supports D1.

R4 item 2 is resolved: P-15 only appended `route_legal` to `walk_search.cpp`; the shared search and expansion are
unchanged.

**Three P3s, assigned in CLOSE-2:**
1. **The route exemption is wider than "leaving the stand".** On the exempt first step, `walk_search.cpp:421–425`
   skips the side check and the layer admission of `route[1]`, the ordinary tile outside the stand, not only the
   stand's own exit. I confirmed this in the code. The tile was admitted when the reverse search entered it from
   `route[2]`, so the gap only matters for a tile that admits entry from some sides and not others. No test puts such
   a tile next to a stand.
2. **The canary's floor clip is not checked against its cause.** `floor_clip` (`population_live_canary_test.cpp:539`)
   accepts any result of 64 without checking that `before + VermehrTab` actually fell below the floor.
3. **The billing bound is not tested exactly at its value.** It is tested below `u32` max and at overflow, but not at
   the bound itself.

The checks below were made by this session at `1e65ea7d`:
- **The Inn state** exists: `struct Inn`, with the tavern stock and the taken statistic widened from u16, plus load,
  save, take and unload. Its tick is not ported (R1).
- **The upgrade scoring** in `house_transition.cpp:349–403` matches the original read of R2: a mean over the forced
  goods plus WIRTSHAUS, an infra minimum starting at 160, each value capped at 128, and `(x + 4) >> 3` indices into the
  next group's tables.
- **The resident wait** is cited as D (R3).

**Limits:**
- Nothing was built or run here. The gate counts are the executor's, and the gate logs and the reviewer ledger stay on
  the Mac.
- The disassembly claims behind the code were checked only where this review names an address.
- No original-runtime run. The thresholds in R2 come from the shipped `BGruppen.dat` and the static read; they are not
  observed in the running game.
- Not run, unchanged: Windows/MSVC, Linux GCC/Clang warnings, GPU and multiplayer scale for the Mac code. The macOS
  `test_support` check stays pending until `task/W4-main-1` is published.
