# House-tick inputs, lifecycle and stage boundary

T32.6 audit (round cy). Inspected code: this branch at `fa4820447e53a4d50468e8edb24a865a2147d15f` (main with P1 integrated). Stage-mask facts: accepted T32.5 F1 at `1fc598eb0b57f0d487fb50d315d7c4ad9c0d8af2` (`docs/re/population.md` §15, `state.md` stage-mask paragraph, OQ-460..462), read only, not copied onto this branch. No P2 implementation, no OQ-file edits, no build, no Wine.

P1 already ships `PayloadTag::House`, registry groups, scenario import of the original house record, and an exact own-save of the runtime form. `TickKind::House` is a no-op (`driver.cpp`). This note lists every tick/mood/growth input, the three lifecycle paths for `last`, original effect order, a bounded next implementation, and independently calculable tests. A missing producer is a blocker, not permission to insert a default.

## 1. Inputs and existing producers

Widths below are the original's. The rebuild widens `residents_64` (i16→i32), `E` (u16→u32) and stocks (u16→u32) on entry (P1 decision 6, OQ-452). Rounding is truncate toward zero unless named.

| Input | Original source | Width / formula | Engine producer | First reader | Missing work |
|---|---|---|---|---|---|
| House `last` | ctor `0x100734ca` = 0; property `0x23` `0x100733ac` = Create `now`; tick `0x1007625e` `last += 2500`; Load does **not** write it (`population.md` §11.2) | u16 at house `+0x0a`; gate `(u16)(tick − last) < 2500` (`0x10076230`) | Field exists on `House`. `make_payload` / import leave 0. `stamp_placement` **skips** houses (P1 decision 8, `create.cpp`). Own-save visits it. | P2 (the gate) | Stamp on play-create; policy for own-save vs original Load: OQ-450 |
| Player stage mask | `GetProperty` 0x21 copies `player +0x90` to dest+4 (`0x10020a6f`). Tick tests bit `BauFirst(goods[Ware(j)])` (`0x100762d3`–`0x100762e2`). Writers: ctor 1 (`0x10021778`); `SetProperty` 0x21 (`0x10020ec3`); `HEAD` load `0x1001d389`–`0x1001d3d5`; guarded city unlock OR `0x100298e0`–`0x100298fb`; unread OR `0x1001bda3` (OQ-460). T32.5 §15 at `1fc598eb`. | u32; bit test `1 << BauFirst` | `Player::flags` exists, default 1 (`player.hpp`). Stream visits it. **Scenario import does not set it** (`scenario.cpp` copies `flags_a0`, money, volk, research — not `HEAD +0x48` / `+0x5C`). | House tick decay (P2); city unlock (P4) | Import reconstructed mask (T32.5 histogram: 263 of 373 full values are 1; 78 have high bits). OQ-460 still unread |
| Goods `BauFirst` | Waren record `+2`; tick `0x100762d3` | u8 bit index | **Absent** from `GoodsInfo` (only `name`, `forsch`) | P2 decay skip | Parse `[WAREN]` `BauFirst` into registry data |
| Goods `IdSec` | Waren `+1`; delivery `0x1007664c` | u8 substitute id | **Absent** from `GoodsInfo` | P5 delivery / demand choice, not the tick | Parse `IdSec`; first reader is P5 |
| Need count `n` | group `+2` (derived from `Ware` lines) | u8 | `HouseGroup::needs.size()` | P2 loop | none |
| `Ware` / `Force` / `Menge` | group `+0x64+4j` / `+0x65+4j` / `+0x66+4j`; `Menge = (h·4096 + 5000) / 10000` (`population.md` §1) | u16 `Menge` integer | `HouseNeed` in `parse_house_groups` | P2 `s_j` and decay; mood uses `Force` | none for those three fields |
| Id → need slot | post-pass `0x10014ad0`–`0x10014bd0`; group `+3+id` | u8 table | **Omitted** (P1 decision 7: first reader P5 delivery) | Delivery `0x1007661b`, demand `0x100765ca` | P5; not required to start P2 if P2 iterates `j` in file order |
| End of goods needs / infra needs | group `+0xcc` / `+0xcd` | u8 indices | **Omitted** | Mood `0x10075ffe` / `0x10076077` (P2) | Derive from `Force` + goods vs infra Ware ids (incl. WIRTSHAUS 0x4c) |
| Forced-need set | `Force` bits; mood averages forced goods 1…(+0xcc−1) | — | `HouseNeed::force` stored; **no derived endpoint** | P2 mood | Same as endpoints |
| `Wohnmax` / prev `Wohnmax` | group `+0xc8`; prev record `+0xc8` at `group−0x50` (`0x10076527`) | u16 | `HouseGroup::wohnmax`; prev = previous slot in the table | P2 clamp / floor | none if groups are slot-ordered (parser returns slot order) |
| `NoUpgrade` / `NoDowngrade` | group `+1` bits 0/1 | flags | `HouseGroup` bools | P3 upgrade; P2 downgrade floor | none |
| `NahrungTab` / `BedarfTab` / `InfraTab` | 21 × i8 | i8 | `HouseGroup` arrays | P2 mood; P3 next-group tables | none |
| `VermehrTab` | 5 × i8 at `+0x10e`, mood −2…+2; `trunc(v·64)` toward 0 | i8 | `HouseGroup::vermehr` | P2 growth | none |
| Group `BauFirst`/`BauLast`/`Hiscore`/`Wissen`/`Priority` | group record | — | **Omitted on purpose** (unread in this path, §12.1) | none | none |
| `E` | ctor `0x280`; Load `residents<<7` (`0x10076f01`); tick `E·127/128 + trunc(residents_64/64)` (`0x10076277`–`0x100762a9`) | u16 orig. | `House::e`; import `r<<7`; create `0x280`; own-save exact | P2 smoothing | none as a field |
| Resident fraction | `+0x2c` i16 `residents·64`; Load `r<<6`; create `0x140` | i16 orig. | `House::residents_64` | P2 growth/clamp | none |
| Stocks | 16 × u16; Load from `s_j`; delivery `+= amount·16`; decay `·127/128` | u16 orig. | `House::stock` (one per group need); T26 `house_need_stock` / `house_satisfaction` on import | P2 decay; P5 delivery | none as a field |
| Mood flags `+0x0c` bits 4/5 | base ctor `and 0xc3`; who **sets** them unread (`0x1002ba80`, owner Q7, OQ-462 at `1fc598eb`) | u8 bits | **Omitted** (P1 decision 6 / Q7 default 0) | P2 mood override if flags ≠ 0 | Writer path is P-later; default 0 is the stated faithful default |
| City group counters | city `+0x2f0+8g` i32 | i32 | **Not stored** (P1 decision 10: derived at first reader P3/P4) | Tick add/sub around `VermehrTab`; HUD | P3/P4 cache + `check_invariants` |
| House counts | `BuildingIndex +0x46+2g` | u16 | `house_counts` exists; Create increments | P3 upgrade/downgrade | P3 must move counts |
| City store | city `+0x334+12id` | tons≪8 | `CityStock` | **Not** the house tick (`0x10076230` has no xref). P5 stall `take=1`; P4 booking | none for P2 |
| Type walker max / variants | type `+0xb8` / `+0xbc` / `+0xb4` | — | type table (Anlagen) | P5 spawn | P5 |
| World PRNG | slot `+0xb4` `0x100a1740` | — | `WorldRng` | P2 walker (if spawned); P3 variants; SetGroup | P2 kernel can omit draws 1–3 if those effects are out of stage |

T26 helpers already in tree and applied on import: `house_need_stock`, `house_satisfaction`, `house_reset_applies` (`scenario_formats.cpp`). They are not applied on the live tick.

## 2. Three lifecycle paths

Property `0x23` (`0x100733ac`, house `+0x0a`): Create writes `now`. The house tick (`0x10076230`) does `last += 2500` after the unsigned 16-bit gate. Load of the **original** house record does not write `last` (§11.2). Distinguish the **city** stamp `city +0x10` used by stage unlock `0x10029610` (`last = tick`, not `+= 2500`, T32.5 §15.4–15.5 / OQ-461): that is not the house gate.

| Path | What writes `last` today | Original | Gap |
|---|---|---|---|
| **Play create** | `make_payload` sets `last = 0`; `stamp_placement` skips `House` (`create.cpp`) | property `0x23` = Create `now` | P2 must stamp `now` (P1 decision 8 deferred it to the first reader) |
| **Original scenario / original save import** | Import builds payload with `last = 0` (constructor value); does not stamp | Load leaves ctor/property value; catch-up on first visits (`population.md` §10, owner Q1) | Matches original Load **if** play-create stamp is not applied on import. Owner Q1 (2026-10-25): keep catch-up (`last = 0` on that load) |
| **Own save / resync** | Visitor includes `u16 last` (P1 decision 4, exact runtime form) | Original does not save house `last`; reload catch-up | **OQ-450 remains.** Q1 settles original-format load, not own-save. Exact save + OQ-132 (“a save and load must not change the game”) argue **keep** `last`. Reproducing original catch-up would **reset** `last` on every own-save load and change hashes / multi-peer snapshots. Do not pick a feel-changing default here |

City unlock `last` (OQ-461) is a separate stamp. Do not reuse house `last += 2500` for it.

## 3. Original order and effects

`this` = house (`esi = ecx` at `0x10076239`, vtable slot `+0x8c`). Live stack: arg0 = tick. Range `0x10076230`–`0x100765b9`. Mood callee `this` = house (`ebx = ecx` at `0x10075fd0`).

Execution order (`population.md` §4 / §12.2, T32.5 §15.1 for the mask path):

1. Gate: if `(u16)(tick − last) < 2500` return. Else `last += 2500`.
2. Mask: house `+0x10` → city → city `+0x1c` (`0x1008c680` = owner) → player `GetProperty` 0x21 → dest+4 = `player +0x90`.
3. `E = E·127/128 + trunc(residents_64/64)`.
4. For each need `j`: if mask bit `BauFirst(Ware(j))` set, `stock[j] *= 127/128` and `lowest = min(lowest, s_j)` with `s_j = min(stock·1536/(E·Menge), 160)`.
5. If `lowest < 102` and walkers < type max: PRNG draw, spawn job `0x35` (P5).
6. Mood (`0x10075fd0`): `E==0` → 0; else Nahrung/Bedarf/Infra tables; flags `+0x0c & 0x30` force −1/−2.
7. City counter sub group, `residents_64 += VermehrTab[mood]`, counter add.
8. If mood ≥ 0: clamp to `Wohnmax·64`; maybe UpgradeCheck + city Upgrade (P3).
9. If mood < 0: floor (1 resident if `NoDowngrade` or flags `& 0x30`, else ¾ previous `Wohnmax`); maybe Downgrade (P3) or island `+0xcc` remove (likely, P5/later).

The house tick does **not** take city store, does **not** bill, does **not** call delivery. OQ-451 (load-phase reset draw) and OQ-452 (narrow wrap) stay out of this order.

## 4. Data work and safe split

AnnoGame loader/post-pass `0x10014ad0`–`0x10014bd0` fills the id→slot map and the `+0xcc`/`+0xcd` ends. `parse_house_groups` currently skips every key other than the stored fields (`house_groups.cpp`: BauFirst of the **group** unread, which matches §12.1; Waren `BauFirst`/`IdSec` are a different record and are not in `GoodsInfo`).

| Must exist before live P2 | First read only later |
|---|---|
| Player mask bits used by decay (`Player::flags` imported or a test double) | City `+0x618` unlock OR (P4) |
| Goods `BauFirst` on each `Ware(j)` | Goods `IdSec` (P5) |
| Need list, `Menge`, `Force`, tables, `Wohnmax`, `NoDowngrade` (already parsed) | Id→slot lookup (P5) |
| Derived goods/infra endpoints (or equivalent walk of `Force` + Ware class) | UpgradeCheck / type `+20` / materials `0x29`–`0x2d` (P3) |
| Play-create stamp of `last` | Walker spawn and stall take (P5) |
| Integer `E`/stock/resident formulas | Booking / taxes (P4) |

**Smallest useful next implementation (partial kernel, not live):** a pure function

`HouseTickIn → HouseTickOut`

Inputs: `tick`, `last`, `e`, `residents_64`, `stock`, group record, player mask, `mood_flags` (default 0), `now` unused except the gate. Outputs: new `last`/`e`/`residents_64`/`stock`, mood, whether the walker/upgrade/downgrade/remove **would** fire (flags only). **No** PRNG draw, **no** city counter, **no** type change, **no** city store. Call it from tests against the §12.2 formulas. Live `tick_building` stays a no-op until an implementation packet wires the kernel and the missing producers.

Silently skipping a reachable PRNG draw in a **live** tick is not faithful. The kernel may expose “would spawn” without drawing; live P2 that enables spawn must bump `kPrngTimelineVersion`. A live tick that runs decay+mood+growth but not upgrade may skip draws 2–3 only if those branches are proven not taken (UpgradeCheck false / mood ≥ 0). Document that in the implementation packet.

Do not approve P2.3 or change the board.

## 5. Reviewable acceptance plan

Independently calculable (no original process):

| Case | Calculation |
|---|---|
| Mask bit clear | `BauFirst` bit off → that need’s stock unchanged this update |
| Mask bit set | `stock' = stock · 127/128` (trunc toward 0) |
| Modular gate | `(u16)(tick − last) < 2500` → no-op; `last' = last + 2500` (u16 wrap) on take |
| Wrap counterexample | `last = 0`, tick = 65536 → difference 0, skip (same arithmetic as city unlock, T32.5 §15.5) |
| Period vs stamp-now | House uses `+= 2500`; city unlock uses `last = tick` (OQ-461). Tests must not mix them |
| `E` | `e' = e·127/128 + trunc(residents_64/64)` |
| `s_j` | `min(stock·1536 / (e·Menge), 160)` if `e·Menge ≠ 0` |
| Mood tables | `(food+4)>>3` index; mean of forced goods; min infra; `(f+b+i+1)/3` or `min(f,b,i)` |
| Flags `& 0x30` | force −1 or −2 as §12.3 |
| Growth | `residents_64 += VermehrTab[mood]` (i8, already 1/64 units) |
| Overflow | intermediates in i64; widened fields do not wrap at 16-bit (OQ-452 separate) |
| G7 | existing `synthetic::g7_scenario` / `p1_g7_create`; P2 adds N-update goldens for residents and stocks; city store unchanged |
| Own-save | hash equal after save/load **if** OQ-450 is “keep last”; if “reset last”, hash changes and a test must say so |
| Multi-peer | same kernel + same mask + same `last` ⇒ same checksum; a `last` policy that differs per peer desyncs |

Named mutations (implementation packet): skip `E` smoothing; decay a need whose mask bit is clear; treat `STARTED`/shell presence analog — here: treat gate as signed compare; apply `VermehrTab` before mood; take city store. Each must fail the named test.

Unresolved for that packet: OQ-450, OQ-451 (no draw in P2), OQ-452, OQ-460, OQ-461, OQ-462 / owner Q7, walker spawn, island `+0xcc` remove.

Excluded effects → later stage: Upgrade/Downgrade/`SetGroup` (P3); booking/taxes/city mask unlock (P4); job `0x35` and stall take (P5); `IdSec` delivery chain (P5).

## 6. Candidate questions (reserved OQ-470..479)

Not written into `docs/open-questions.md`.

| Id | Topic | Next read / decision |
|---|---|---|
| OQ-470 | Wire `[WAREN]` `BauFirst` (and later `IdSec`) into registry goods data. Without `BauFirst`, P2 cannot evaluate the decay predicate. | Parse Waren.dat against `0x10013be0` / tick `0x100762d3`; add fields to `GoodsInfo` or a side table |
| OQ-471 | When to import `SPIELER` `HEAD +0x48`/`+0x5C` into `Player::flags` (T32.5 §15.4). A live tick on a loaded scenario with default `flags = 1` only decays needs whose `BauFirst` is bit 0. | Scenario loader vs a P2.1 data task; do not invent bits |
| OQ-472 | Derive group `+0xcc`/`+0xcd` from the parsed needs (post-pass `0x10014ad0`–`0x10014bd0`) vs storing them. Mood is wrong if the WIRTSHAUS slot is mis-classified. | Re-read the post-pass; one unit test per shipped group |
| OQ-473–479 | unused | — |

OQ-450 is **not** re-asked: it stays the owner question in `open-questions.md`. Owner Q1 (2026-10-25) plus P1 exact-save do not collapse it.

## 7. Decisions and reasoning

1. **Audit P1 code on this branch, cite T32.5 at `1fc598eb`.** Alternative: copy T32.5 `population.md` over main. Rejected by the packet (would overwrite newer P1 docs / audit pre-P1 code).
2. **OQ-450 stays open.** Alternative: treat Q1 “keep catch-up” as also resetting own-save `last`. Rejected: Q1 describes original Load; P1 decision 4 and OQ-132 require exact own-save/resync. Evidence: `state.md` §4.7c Q1 vs P1 decision 4 vs `open-questions.md` OQ-450.
3. **Partial kernel before live tick.** Alternative: implement live `tick_building` in this audit. Forbidden. A pure kernel can be proposed; live wiring needs the missing mask/`BauFirst` producers.
4. **Default mood flags 0.** Alternative: guess fire/plague writers. Rejected: OQ-462 / owner Q7; unread `0x1002ba80`.
5. **No new limit, no city-store take in P2.** Evidence: no xref from `0x10076230` to billing `0x1002dac0`.
