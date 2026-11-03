# Population readiness: prerequisites, effects, decisions and the next packages

MAP-POP-READY-1 (2026-11-03). Documentation and read-only evidence only: no engine code, test, format, golden, asset, map or version changed. The companion document is [`population-inputs.md`](population-inputs.md) (the immutable-input contract and the data-level examples). Nothing here releases P2.2, P2.3 or any live House behaviour; every package in section 6 needs the orchestrator's explicit release.

## 0. Baseline, evidence and how to read the statuses

| Item | Pin |
|---|---|
| Accepted MAP baseline (stream 126, PRNG timeline 4) | `f40a76022c609092156c457509f0c49e7905f96e` (clean tree at the start and the end of the read) |
| Assignment read from | `origin/main` `214f994dac3a940d1e31a5cc7f8217244779d8fd` (fetched, not merged) |
| P1 baseline (house payload, group table, scenario import) | `b173d02217b378256a7aa04a614567c19232a4ce`, an ancestor of the baseline |
| T32.5 F1 (stage mask), accepted | `1fc598eb0b57f0d487fb50d315d7c4ad9c0d8af2` and merge `467828db`, ancestors; text is `docs/re/population.md` section 15 |
| T32.6 F1 house-tick audit (the document this packet reconciles) | `075ede70166cc1272fd0fff9b4f2ac17d831a5d6`, **not an ancestor**; `docs/sim/house-tick-readiness.md` is read with `git show` and is absent from this branch |
| P2.1 mask import report and code | report `0964729c0c9a7d51977da3a1c27a577a695a703f`, code `62b14254f211b97b2195baaa863eb6f151f73528`; **neither is an ancestor** |
| Original modules re-read in this task | `AnnoWelt.dll` sha256 `f220a878510f740c4cfb7224ade0a260e9a0fd7e99113e1201e7a37c0dfa6ae2`, `AnnoGame.dll` `ac721220afd27ebca5fd2ece6a8ff452289b9d7458c4e091c68adea2012db7bc`, `Data/BGruppen.dat` `63e876fd3afedca214b3d9cc45f022f2c595f473839690e96f34e12ed7849e3b` |

**Status vocabulary.** *accepted* = reviewed and integrated by the orchestrator per the board; *author-reported* = an executor's report with gates, no recorded orchestrator review or integration; *unreviewed* = not reviewed by anyone independent. *Re-derived here* = I recomputed the fact from the original bytes or file in this task, not from the earlier note; the independent reviewer of this packet repeats that for the decisive rows (section 8).

**What is NOT inherited as accepted.** The board's last word on T32.6 F1 and P2.1 is "both wait for review" (TASKS.md, the "Lane 1 finished the round-dk work" entry). No acceptance of either was found in the board or `docs/orchestration/reviews/`. Their numbers are used below only where re-derived.

## 1. R0 — prerequisite inventory (actual code and bytes, not ancestry)

| # | Prerequisite | Current producer → first/actual consumer | Source / evidence pin | State | Review status | Smallest remaining action |
|---|---|---|---|---|---|---|
| 1 | House payload (P1) | `make_payload` (`building.cpp`), `consume` of a scenario house record (`scenario_formats.cpp`, Load rule at the `Class::House` branch) → `fields(House)` (`state_fields.hpp:731`), `payload_blocked` (`driver.cpp:153`); no tick reader | `b173d022`; source read at baseline. Fields: `u16 last`, `i32 residents_64`, `u32 e`, `vector<u32> stock` | present | accepted (board round cx) | none. Doc drift only: `state.md` §4.7c still says "DRAFT / Not implemented" and lists `mood_flags` and narrow widths that P1 did not take |
| 2 | Group table in the registry (P1) | `parse_house_groups` (`house_groups.cpp`) ← `src/sim_load/src/original_registry.cpp:58-60`; digested at `types.cpp:373`; consumers: importer (`Menge` → stock), `house_need_count` (`building.cpp:58`) | `b173d022`; tests `house_test.cpp` | present for `number`, flags, `Wohnmax`, needs (`ware`, `force`, `menge`), 3×21 + 5 tables; **absent**: endpoints, id→slot lookup, forced mask, per-goods `BauFirst`/`IdSec` (see 6, 7) | accepted | none for what exists |
| 3 | Full player-mask import (P2.1) | **none on this branch.** `read_player_head` (`scenario_formats.cpp:995`) reads kind, money, name, colour, `+0x4e`; `ScenarioSpec::PlayerSpec` has no flags field; `Player::flags` (`player.hpp:100`, default 1; `state_fields.hpp:391` visits it) is therefore always 1 after import | evidence **re-derived here**: AnnoWelt `0x1001d389`-`0x1001d3df` (`and eax,0x7fffff`; byte HEAD+0x5c bit3→`0x01000000`, bit2→`0x08000000`, bit4→`0x20000000`, bit5→`0x40000000`). Implementation exists only at `62b14254` | **missing** (evidence present) | T32.5 accepted; P2.1 code author-reported | package A (section 6) |
| 4 | Corrected stage-mask interpretation | the tick tests the full 32-bit dword: `movl 0x1c(%esp)` copy of property `0x21` → `testl %edx,%eax` after `shll %cl` (`0x100762db`-`0x100762e2`), `cl` = the need's goods byte `+2` | `population.md` §15 (accepted); tick instructions re-derived here. The property-`0x21` read itself (`0x10020a6f`) is T32.5's, not re-read | present as evidence; no engine reader | accepted (T32.5) | none |
| 5 | Current House dispatch | `tick_building`: `case TickKind::House: return;` (`driver.cpp:239-241`), `last` untouched; visits come from `building_scheduler` (`island_tick.cpp:85`) with `visit_count` = min(d·n/`building_div`(500) + 1, n), d = u16(t − `city.index.stamp`), so **at least one building is visited per call for each non-empty scheduled list, even at d = 0** (a given house is reached round-robin; its own gate and the u16 aliasing carry the rest) | read at baseline; G7 test "the House tick changes nothing" | present (a deliberate no-op) | accepted | none |
| 6 | Goods `BauFirst` (stage index) and `IdSec` | **none.** `GoodsInfo` = `{name, forsch}` (`types.hpp:112`); `forsch` is hard-coded in `goods_table.cpp` (OQ-034), `[WAREN]` is not parsed anywhere | original: `[WAREN]` record 26 bytes, `BauFirst` at `+2` (cleared to 0 at `[WAREN]` start, `0x10014c3b`, `population.md` §1), `IdSec` at `+1`; tick reads `goods + 26·ware + 2` (`0x100762c2`-`0x100762d3`). Shipped values **re-derived here** (23 goods have `BauFirst`, 4 have `IdSec`; section 2 of the inputs doc) | **missing** | T32.6 author-reported; values re-derived here | package B |
| 7 | Group derived data: `n`, `+0xcc`, `+0xcd`, forced mask `+0xc4`, id→slot lookup, tavern alias, `IdSec` substitution | **none** | loader `0x1001486b`-`0x100148a6`, post-pass `0x10014afc`-`0x10014bd9` re-read; the five shipped groups reproduced from the raw file by an independent script (n, `+0xcc`, `+0xcd`, mask, alias slot, substitutions all equal T32.6 §1.3) | **missing** | T32.6 author-reported; re-derived here | package C |
| 8 | Mood flags (`house +0x0c & 0x30`) | omitted from the payload; nothing writes them; mood reads them | OQ-462 (writer unread: `0x1002b700`, `0x1002b840`, fire path `0x10072a60`); owner Q7 default 0 | **open, default 0** | accepted default | none before fire/plague |
| 9 | Walker data: walker slots `+0x18..+0x24` (figure registry), type max walkers (`type +0xb8(0)`), variant count (`+0xbc(0)`), target pick (`+0xb4`), world PRNG draw `0x10076397`, job `0x35` via house slot `+0x118` | **none** for the type facts; walkers are figures | tick `0x10076341`-`0x100763e3` re-read; `BuildingType` has no walker facts | **missing** | first reader P5 | out of the proposed packages |
| 10 | Play construction stamp (`last`) | `stamp_placement` (`create.cpp:106`) deliberately skips `House` (P1 decision 8), so `last = 0` | original Create `0x10024010`: SEH prologue (3 pushes) + `sub esp,0xbc` + 4 saved registers; property `0x23` call at `0x100246b2` passes `[esp+0xe4]` = **the third stack argument** (arg1 `0xdc`, arg2 `0xe0`, arg3 `0xe4`; the out pointer written at `0x1002403a` is arg4 `0xe8`); the setter stores a u16 into `+0x0a` (`0x100733ac`, T32.6) and then calls vtable `+0x120` with the same value, which for the house resolves to `0x10037590`, a `ret 4` no-op (reviewer-verified) | **partial**: the value's origin narrowed; its callers' arguments unread | author-reported (T32.6); the arg3 result is re-derived here | evidence packet E |
| 11 | Original-scenario load | importer applies the house record: residents `<<6`, `E = residents<<7`, `stock[j] = house_need_stock(Menge, s_j, residents)`; `last` untouched (0); reset predicate warned, not applied (OQ-451) | `b173d022`; corpus oracle (3283 houses, reset 0 in all 50 files, P1 report) | present | accepted | none |
| 12 | Own save / resync | `fields(House)` writes `last`, residents, `e`, stocks; hashed; resync replays the same stream | `house_test.cpp:346` layout test; P1 decision 4 | present | accepted (policy OQ-450 open for *load semantics*, not for the stored bytes) | decision OD-1 (live effects only) |
| 13 | City resident counters and house counts | resident counters: **not yet present** (derived by design); `BuildingIndex::house_counts` counts *houses per group* (initialised `building.cpp:20`, maintained at create/remove, `create.cpp:252`, `:286`), not residents; resident counters do not exist yet | P1 decision 10, OQ-165, `population.md` §11.3-11.4 | present as *derived* | accepted | see the counter finding F-3 in section 5 |
| 14 | Stage unlock `0x10029610`, taxes `0x10029950` | `Seams::city_pass` entries are null (`seams.hpp:72`) | `population.md` §7-8 | absent | accepted evidence | out of scope (P4) |

## 2. R1 — three possible deliverables and where each input and effect is first read

Three distinct deliverables must not be confused (and none may silently stand in for another):

* **A. Immutable metadata** — registry data that a P2 calculation reads: goods `BauFirst`/`IdSec`, the group endpoints, the lookup, the forced mask, the config that names the tavern/infrastructure goods. No behaviour, no state change except the registry digest.
* **B. A bounded pure kernel** — a function from explicit inputs to labelled outputs that evaluates the numeric steps and returns "not evaluated" for every step whose inputs or whose effect belong to a later stage. It writes nothing, draws nothing and is not called from `tick_building`.
* **C. A live House tick** — `tick_building` runs the step on real houses with all of its reachable effects.

### 2.1 The taken tick, step by step, with first reader

(AnnoWelt `0x10076230`-`0x100765b9`; mood `0x10075fd0`-`0x10076224`; re-read and re-derived here.)

| Step | Inputs | Output / effect | Kind | First reader | Notes |
|---|---|---|---|---|---|
| Gate: `u16(t − last) < 2500` returns (`jb`, unsigned) | `t`, `last` | none on return | direct | B (kernel), C | `last += 2500` only on a taken tick (not `last = t`) |
| Mask read | owner's `Player::flags` | value | input | **A+package A** (the mask must be correct) | full 32-bit dword |
| `E` smoothing | `E`, `residents_64` | `E' = trunc(127E/128) + trunc(res/64)`, original stores the low 16 bits | direct state | B, C | negative sum: OD-4 |
| Need loop, masked needs only | per need: `ware`, `Menge`, goods `BauFirst`, stock | stock decays `trunc(127s/128)`; `lowest = min(lowest, min(trunc(1536·s/(E'·Menge)), 160))` | direct state + value | B, C | needs `BauFirst` (A, package B) |
| Spawn condition | `lowest < 102`, walker count, type max | boolean | value | B returns it only when walker count and type max are supplied | not a spawn |
| Spawn | variant count, **world PRNG draw `0x10076397`**, divisor, job `0x35` | a walker figure | direct effect + **PRNG** + transitive (figure registry, later deliveries) | **P5** | affects every later world draw |
| Mood | `E'`, stocks, group endpoints `+0xcc`/`+0xcd`, forced flags, tables, house flags | −2..+2 | value | B, C | needs endpoints (A, package C) |
| Counter sub/add | `trunc(res/64)` before and after | city resident counters | direct effect on a *derived* cache | P4 (first reader of the counters), see F-3 | |
| Growth | `VermehrTab[mood]` | `residents_64 += i8` | direct state | B, C | index must be in −2..+2 |
| Cap crossed (`mood ≥ 0`, `res > Wohnmax·64`, signed, strict) | `Wohnmax` | residents := `Wohnmax·64`, counter correction, then `UpgradeCheck` `0x10075dd0` → city `+0x98` | direct state + **transitive** | the number is B; the upgrade is **P3** (next group's forced needs, queue, materials `0x29`-`0x2d`, variant PRNG `0x1002ccd3`) | |
| Below floor (`mood < 0`): floor 64 if `NoDowngrade` or flags `&0x30`, else `48·previous Wohnmax` | previous group's `Wohnmax` | selects the *remove arm* (island `+0xcc`) or the *downgrade arm* (city `+0x9c`) | direct + transitive | arm selection is B; effects **P3** (downgrade type change, material move, variant PRNG `0x1002ce68`) / removal | the below-64 correction (signed `cmpw $0x40,%bp; jge`, `0x1007654b`) is a separate, B-evaluable step |

**Consequence for a live tick (C).** The spawn draw is reached whenever one masked need has satisfaction `< 102` (about 64 %) and a walker slot is free, which is the ordinary state of a town. A live tick that does not implement the walker spawn and its draw is therefore **not** a faithful live tick for real towns; it is faithful only for populations proven never to reach that branch. The cap and below-floor branches reach P3. This is why the packages below stop at the kernel and why live behaviour is a separate, later decision.

### 2.2 What each deliverable needs from the open questions

| Open item | Blocks metadata (A)? | Blocks the pure kernel (B)? | Blocks live effects (C)? |
|---|---|---|---|
| OQ-450 `last` on own-save load (OD-1) | no | no (`last` is an input) | **yes** (policy for load semantics) |
| OQ-473 Create's stamp (OD-2) | no | no | **yes** for play-created houses; and for the claim "import `last = 0` equals the original" |
| OQ-452 i16/u16 wrap (OD-3) | no | no (kernel is widened and labels the original-width store) | no unless the owner wants the wrap |
| Negative `E` sum, `E = 0` with a matched need (OD-4) | no | defines the kernel's *not evaluated* domain | **yes** if a reachable house enters it |
| OQ-462 mood flags | no | no (input, default 0) | no before fire/plague |
| OQ-460 extra OR into `player +0x90` | no (omit) | no | affects decay only if that path is reachable; unread |
| OQ-461 city unlock stamp `city +0x10` | no | no | P4 |
| Group 0 previous `Wohnmax` (OD-5) | no | defines an input | only for a modded group 0 without `NoDowngrade` |
| Infrastructure/tavern config (OD-6) | defines the config shape of package B/C | no | no |
| Counter equality (F-3) | no | no | P4 |
| Walker/figure data (P5), upgrade inputs (P3) | no | no (outputs *not evaluated*) | **yes** |

## 3. Three paths the same fields take

| Field | Original-scenario import | New construction in play | Exact own save / resync |
|---|---|---|---|
| `Player::flags` (stage mask) | original `SPIELER` head restores it (`0x1001d389`); this branch leaves 1 (**missing**, package A) | constructor value 1 (`population.md` §15; `Player::flags` default) | exact (already hashed and saved) |
| House `residents_64`, `E`, stocks | Load rule: residents `<<6`, `E = residents<<7`, stocks from `s_j` (implemented, accepted) | constructor: residents `5·64`, `E 5·128`, zero stocks (implemented) | exact (implemented) |
| House `last` | Load does not write it → whatever Create left. Importer leaves 0. Equals the original only if Create's stamp value is 0 (OD-2, open) | original: Create stamps property `0x23` (arg3); this branch: 0 (deliberate) | **exact `last`** (implemented, P1 decision 4); owner Q1 (2026-10-25) concerns the *original load*, keeping catch-up there |
| Group data, goods metadata | registry, from the game files | same | registry digest is compared in the handshake; derived data recomputes |
| City counters | derived (rebuilt) | derived | derived (never saved) |

The three paths are different rules and no one of them is allowed to "repair" another: a loaded state is never silently reset, a mask is never normalised, and a resync snapshot must be bytes-exact (a peer that reset `last` would diverge).

## 4. Reconciliation of the open questions and decisions

Labels: **E** established fact (evidence cited); **A** approved adaptation (owner or orchestrator decision on file); **P** proposal in this packet; **O** open.

| Id | Status | Statement |
|---|---|---|
| Owner Q1 (state.md §4.7c, 2026-10-25) | A | Original load keeps the catch-up (`last = 0` on that path); "option later: store `last`". |
| P1 decision 4 / OQ-450 | A for the stored bytes, **O** for load semantics | Our own save is the runtime form and stores `last` exactly. Whether loading *our own* save should reset it instead is open (OD-1). |
| OQ-451 | O | The house-reset draw (`HouseLoadReset`) position in the load phase is unread. The corpus never enters it (reset 0 in all 50 files). Not needed by A, B or by a live tick over shipped data. |
| OQ-452 / ISS-083 | O (low) | Widened arithmetic is the policy (PLAN §0.1; ISS-083 "FIX: wide integer types"); the original's 16-bit store is reported beside the widened result. ISS-083's "~65xxx per town" claim is a forum claim and does not match the house fields (the city counters are i32): unverified. |
| OQ-460 | O | The extra OR into `player +0x90` at `0x1001bda3`: case and bit unread. Omitted. |
| OQ-461 | O | `city +0x10` (unlock stamp) constructor/load value unread. P4. |
| OQ-462 | O, default A | House flag bits 4/5: writer unread; default 0 (owner Q7). |
| OQ-185 | O | Who calls house Save slot `+0x80` on a world save is unread; the record itself is E. Does not affect our own save. |
| Candidate OQ-470 (BauFirst on goods), OQ-471 (mask import) | **not registered** | These numbers exist only inside the T32.6 note ("kept in this note only") and are **not in `docs/open-questions.md`** on this branch or on `origin/main`. They are tasks (packages B and A), not open questions. |
| Candidate OQ-472 (endpoint rule) | closed by evidence | The rule is E (section 5 of the inputs doc) and re-derived here. |
| Candidate OQ-473 (Create's stamp value) | O, narrowed | New in this task: the value is Create's **third stack argument** (E, section 1 row 10); the arguments passed by Create's callers are unread. Not registered; the next read is named in OD-2. |

### 4.1 Owner/orchestrator decisions needed (each blocks only what is stated)

**OD-1 — loading our own save: keep `last` or reset it (OQ-450).**
*Behaviour:* after a load of a save made by this engine, whether houses resume their saved `last` (a house continues its 2500-tick schedule) or `last` becomes 0 (a house runs catch-up steps, one per scheduler visit, because the tick takes one step per call).
*Evidence:* the original's `Load` never writes `last` (E); the original's own save has no `last` field (E); our save stores it (implemented). A reset changes the state hash across a save/load (OQ-132) and desynchronises a peer that did not reset.
*Unknown:* what the owner wants a "load game" to feel like; no original-runtime observation.
*Options:* (a) **keep exact (recommended, implemented)**; original-format import keeps `last = 0`. (b) reset on load of our own save under a hashed `Rules` option present in the lockstep handshake, with a test that shows the hash change. (c) reset only on a user-initiated load but never on resync (needs two load kinds; a bigger design).
*Blocks:* live effects only. Not the metadata, not the kernel.

**OD-2 — what `last` a house created in play receives (candidate OQ-473, unregistered).**
*Behaviour:* the first tick of a house built by the player: `t − last` with `last = 0` versus `last` = the creation time.
*Evidence:* the original's Create passes its third stack argument to property `0x23`, which writes the u16 at house `+0x0a` (E, re-derived). Whether that argument is the world tick or a constant in each caller is unread.
*Options:* (a) read the callers of `0x10024010` (the next named read; bounded) and then follow them; (b) adopt "creation tick" as a documented adaptation now (proposal; changes first-step timing against the original only if the callers pass 0).
*Blocks:* live effects for play-created houses; the sentence "import matches the original" in section 3.

**OD-3 — keep widened arithmetic only, or add the 16-bit wrap as a rules option (OQ-452).**
*Behaviour:* whether a house whose residents, `E` or stocks exceed 16 bits wraps as the original does or keeps the exact widened value.
*Evidence:* the original wraps `residents_64` (i16), `E` and stocks (u16). With shipped data none reaches the wrap (largest shipped `Wohnmax` 42 → 2688 ≪ 32767; stock scales bounded by delivery). *Unknown:* whether any mod or the owner needs the wrap. *Recommendation:* widened only, original-width result reported beside it, no wrap option until a mod needs it. *Blocks:* nothing.

**OD-4 — the undefined-domain policy: negative `E` sum and `E = 0` with a matched need.**
*Behaviour:* (i) `E' = trunc(127E/128) + trunc(res/64)` is negative only if `res_64 ≤ −64` and `E` is small; the original stores the low 16 bits (so the next read is huge). (ii) the need loop divides by `E'·Menge`; `E' = 0` with a matched need is an **integer divide fault** in the original. Mood's own `E = 0 → 0` return is a different statement.
*Evidence:* instructions `0x10076288`-`0x100762a9` (store `movw %cx,0x2e(%esi)`), `0x10076305`-`0x10076318` (`imull`, `idivl`). In the one shipped town I ran (`single36`, 388 houses) every house record has residents `> 0`; the corpus-wide count of zero-resident house records is **not measured** (no fixture records it).
*Options:* (a) the kernel reports "not evaluated" for those inputs (this part is the proposed kernel behaviour); and, as an **unreleased** live-tick option, a live tick must not enter them (leave the house unticked and `last` unchanged: a documented adaptation); (b) clamp `E'` at 0 and skip the divide (adaptation, changes the original's fault into a value); (c) reproduce the 16-bit store under OD-3's rules option.
*Blocks:* the kernel's specification (pick (a) as the default *not evaluated* domain; harmless) and any live tick whose houses can reach the domain.

**OD-5 — group 0 without `NoDowngrade`: the previous `Wohnmax`.**
*Behaviour:* the floor for a negative mood is `48 · Wohnmax(previous group)`, read as the u16 at `group − 0x50` (`0x10076527`). For group 0 that address is **not a group record**.
*Evidence (derived here from layout, not executed):* the goods array is 96 × 26 bytes from object `+0x4`, so it ends at `+0x9c4`, exactly where the group array begins; `group0 − 0x50` is object `+0x974`, bytes 24-25 of goods record 92 (id `0x5c`, one of the ids the name scan cannot match: `0x5b`-`0x5f`), which the loader never writes. It is therefore 0 in the original (floor 0), unless a loader step not read writes it. Shipped PIONIER has `NoDowngrade`, so the shipped game never reads it.
*Unknown:* whether a loader step I did not read writes those two bytes. *Options:* (a) previous `Wohnmax` of group 0 is 0, as the layout implies (recommended; matches T32.6); (b) refuse a group-0 table without `NoDowngrade` at registry build (stricter than the original).
*Blocks:* an input definition for the kernel only for a modded table.

**OD-6 — how the engine names the infrastructure goods and the tavern (design).**
*Unknown:* which shape the owner prefers; no behaviour differs for the original data.
*Behaviour:* the original compares goods ids with the numeric constants `0x4c`-`0x5a` (infrastructure), `0x4c` (tavern, WIRTSHAUS) and `0x1d` (the alias slot, ALKOHOL).
*Evidence:* all three are direct comparisons in the loader and the mood function; they are a property of the original's goods table, not of the engine (PLAN §0.1: no hardcoded ids).
*Options:* (a) **config fields with the original values as defaults** (`infra_first`/`infra_last`/`tavern`/`tavern_alias`), hashed like `warehouse_goods`; (b) names resolved from the registry (`"WIRTSHAUS"`, `"ALKOHOL"`) plus a flag on `GoodsInfo`. *Recommendation:* (a) for the first slice (smallest, precedent `kOriginalWarehouseGoods`), with the config section noted as a future-identity change. Same decision shape as the orchestrator's CLOSE-1 route-name mapping.
*Blocks:* the exact shape of packages B and C, not their need.

## 5. Findings of this reconciliation (new or corrected against the earlier notes)

* **F-1 Candidate OQ numbers are not registered.** OQ-470..473 live only in the unintegrated T32.6 note; this packet refers to them as "candidate" and registers none (the allowed files exclude `open-questions.md`).
* **F-2 The high stage bits are unread by every shipped need.** The shipped needs' `BauFirst` indices are {0, 2, 3, 5, 6, 9, 13, 14, 19, 21} (re-derived); the maximum is 21 (`INFRA_STUFE_4E`). The player mask's high bits 24/27/29/30 therefore cannot change any shipped decay or `lowest`; they matter to a modded `BauFirst ≥ 24`, to later readers (unlock, P4), and to the mask's fidelity/hash. The tick still tests the full dword and the engine must too. Corpus: 110 of 373 shipped heads are not `1`; 78 carry a high half (T32.5 §15.4; histogram not re-run here).
* **F-3 The derived-counters shortcut has one exception.** Counters are derived from the houses (accepted, OQ-165), which equals the original's incremental counter in every arm except the **below-64 downgrade correction**: the original adds `trunc(res/64) − 64` *after* having added `trunc(res/64)` and then stores residents 64, so the incremental counter ends at `2·trunc(res/64) − 64` while a derived counter would read `trunc(64/64) = 1` per house (read from `0x10076553`-`0x1007656a`; `0xc(%ebx)` is add per `population.md` §11.3). Shipped tables cannot reach the arm from the floor (largest shipped drop 51), but a crafted or modded house of a group without `NoDowngrade` whose residents are below the largest drop (25-38), or a modded table, can (a zero-resident loaded house has `E' = 0` and meets the OD-4 divide first). Reachability from shipped data is not shown. P4 must decide derived-versus-incremental before it relies on the counters; it does not affect A, B or the kernel.
* **F-4 Spawn is reached by ordinary state** (section 2.1): a live tick is not "mostly numbers plus a rare walker".
* **F-5 Group-0 previous `Wohnmax`** is a layout accident (OD-5).
* **F-6 Create's stamp value is its third argument** (row 10); the previous audit left the frame unresolved.
* **F-7 Everything else in T32.6 §1-3 reproduced** from the raw bytes and files: the gate, `E`, decay, mood precedence, floor/cap and signed below-64 arithmetic (section 7 and the inputs doc), the loader and post-pass rules, and all five shipped group rows (n, `+0xcc`, `+0xcd`, forced mask `0x0/0x41/0x91/0xa3/0x286`, alias slots 4/6/7/7/sentinel 8, the substitutions).
* **F-8 Cadence.** The caller visits at least one building per scheduled list per call, even at zero elapsed ticks (section 1 row 5); the only partial state of the gate is `last`.

## 6. Proposed implementation packages (none released)

Order is dependency order. Each is separately reviewable. Identity effects: every package that changes the created-world bytes, the registry digest or the config serialisation changes the canonical hash and needs a `kStateHashVersion` increment (the repository convention; P2.1 did so for the same reason). Stream 126 and timeline 4 are unchanged by this task. Several packages may share one increment if the orchestrator releases them together; intermediate values would be Mac-local.

### Package A — restore the full player mask on original-scenario import (**smallest first unit**)

* **Result:** `Player::flags` of every player created from an original scenario equals the original's `[player+0x90]` after `SPIELER` load; a synthetic spec without a mask keeps the constructor 1; an explicitly stored `0` stays `0`.
* **Files:** `scenario_formats.cpp` (`read_player_head` + one decoding function), `scenario.hpp` (`PlayerSpec::flags` as `optional<u32>`), `scenario.cpp` (assign in `World::create`), tests (`scenario_formats_test.cpp`, `player_test.cpp`), `docs/re/population.md` §15.4 cross-reference, `tools/sim_stream.py` and the corpus/golden fixtures if their bytes move.
* **Inputs:** T32.5 (accepted) and the re-derived instructions above. OQ-460 stays out.
* **Supported / excluded:** original `.sze` player heads; `fixed_map.cpp:150` and `ship_start.cpp:270` build `PlayerSpec` without an original head and keep the constructor 1. Excluded: city unlock, setter side effects, the house tick, OQ-460.
* **Proof:** positive: the table rows A1-A4 (acceptance table, section 7) through the **created world's players**, plus the corpus histograms (373 heads; 263 are 1; 78 high halves). Negative controls that must fail: mutant that drops the high reconstruction (A2), mutant that never assigns to `Player` (A1).
* **Persistence/digest:** no new field (the member already visits); created players of scenarios with a mask ≠ 1 hash differently ⇒ `kStateHashVersion` 126 → 127 (proposal; the orchestrator decides). PRNG, wire unchanged. Windows delta: save/recording identity and the corpus file.
* **Review boundary:** the decoding rule, optional-versus-zero semantics, the hash/version decision and the corpus oracle (independent histogram).
* **Done / stop:** world players asserted; corpus oracle green; the House tick still a no-op; nothing else.

### Package B — goods `BauFirst` and `IdSec` from `[WAREN]` (immutable metadata)

* **Result:** each goods entry carries `bau_first` (stage index, default 0) and `id_sec` (goods id or none), parsed from `Data/BGruppen.dat`.
* **Files:** a `[WAREN]` reader beside `house_groups.cpp`; `GoodsInfo` (+2 fields) in `types.hpp`; `types.cpp` digest; `original_registry.cpp`; `goods_table.cpp` defaults; the stage-name table (32 `INFRA_*` names, `AnnoGame 0x10024bd0`, re-read here) as a compiled mapping (precedent: the accepted route-name table); `SimConfig` fields of OD-6 if (a) is chosen.
* **Inputs:** `population-inputs.md` §2; OD-6 (shape).
* **Supported / excluded:** shipped `BGruppen.dat` and synthetic tables of any goods count; excluded: prices, climates, `Forsch`/`Bonus`, `BauLast`, taxes, group-level `BauFirst`.
* **Proof:** the shipped 23 `BauFirst` and 4 `IdSec` values against an independent transcription; synthetic default/explicit/unknown-name/out-of-domain rows (inputs doc B1-B6); a mutation of any field changes the digest.
* **Digest:** registry digest changes ⇒ version increment; no state field.
* **Review boundary:** name-table transcription, unknown-name/absent behaviour, digest coverage.
* **Done / stop:** metadata only; no tick, no draw.

### Package C — derived group data (endpoints, lookup, forced mask, alias, substitution)

* **Result:** per group, computed once at registry build from `needs` + package B: `n`, `goods_end` (`+0xcc`), `infra_end` (`+0xcd`), the id→slot lookup (sentinel `n`), the forced-infrastructure mask, the tavern alias and the one-pass `IdSec` substitution, by the rules of inputs doc §3.
* **Files:** `house_groups.hpp/.cpp` (derived struct), `types.cpp`, tests.
* **Inputs:** package B, OD-6.
* **Supported / excluded:** any need list; excluded: reproducing the original's byte-wrap of `n` or its 16-slot record (the engine stores the vector; sixteen is the original record's slot count, not a limit), delivery and mood themselves.
* **Proof:** the five shipped groups against the independent script; the synthetic rows C1-C9 (duplicates, order, alias, force/endpoints, IdSec chains) with their wrong-rule rejections; recomputation equals; the digest policy (derived values not hashed, inputs hashed) demonstrated by a changed input moving the digest.
* **Digest:** none for the derived values (pure function of hashed inputs).
* **Review boundary:** the endpoint-versus-Force distinction, sentinel and order rules, the one-pass chain.
* **Done / stop:** registry/data only; no live behaviour.

### Package D — pure house kernel (named partial; not a live tick)

* **Result:** `house_kernel(inputs) → labelled outputs` as in T32.6 §2.3 *as corrected here*: `acted`, `last'`, `E'`, `stock'`, `lowest`, `spawn_condition` (only with walker count and type max), `mood`, the two truncated counter integers, `cap_crossed`, `below_floor`, `arm`, the signed below-64 result; every effect of upgrade, downgrade, removal, spawn and PRNG is **not evaluated**.
* **Files:** new `house_kernel.hpp/.cpp` (a pure function, no `World`), tests; **not** `driver.cpp`.
* **Inputs:** packages B and C (as parameters; synthetic inputs suffice for most rows), OD-4/OD-5 as the stated *not evaluated* domain.
* **Supported / excluded:** the supported domain of T32.6 §2 (non-negative `E'`, `Menge > 0`, stocks ≥ 0, `BauFirst` 0..31, mood −2..+2). Excluded: every effect; the caller. The period (2500) is an input taken from class data (`ClassInfo::gate_period` is already 2500 for `House`, `types.cpp:841`), not a literal in the kernel. Domain also requires `n ≥ 1` with need slot 0 the food need (mood reads slot 0 unconditionally; `population.md` data invariant): an empty group with `E ≠ 0` divides by `Menge` 0 and is *not evaluated*.
* **Timed behaviour (evidence contract):** the caller visits at least once per city step, with d = 0 allowed; a call with `u16(t − last) < 2500` returns unchanged (zero-time and smallest interval); a call 2500 or more behind takes exactly one step (`last += 2500`), so a house 5000 behind needs two *calls*, not one; the only partial state is `last`. The partition test is per call; the whole world is not claimed invariant under other advance schedules.
* **Proof:** acceptance rows D1-D14 (section 7); mutants that must fail: skipped smoothing, decay of an unset bit, signed gate, `last = t`, unsigned below-64, always-average mood, a kernel that reports `would_fire`.
* **Digest:** none (no state, no registry change). PRNG: none.
* **Review boundary:** each decisive row re-derived by the reviewer from the instructions; the `not evaluated` discipline.
* **Done / stop:** the function and its tests; no integration into the tick.

### Package E — evidence packet for live readiness (documentation and bounded reads; no code)

* **Result:** answers OD-2 (the arguments the callers of Create `0x10024010` pass for property `0x23`), counts zero-resident house records in the 50 shipped scenarios (OD-4), and decides whether the below-64 counter arm (F-3) is reachable from shipped data.
* **Files:** docs only. **Stop:** the three answers with addresses; no policy chosen.
* **Why separate:** these reads decide whether any live subset can be proposed. They do not gate A-D.

**Not proposed:** a live House tick (deliverable C). Section 2.1 shows it needs the walker spawn and PRNG draw (P5) and the upgrade/downgrade/removal effects (P3) for ordinary towns; P2.3 stays blocked as the board states. A constrained live subset exists only for populations proven never to reach those branches, and its proof obligations would be written in a later packet after E.

## 7. Acceptance table (decisive rules; hand-derived)

Every expected result was recomputed with a throwaway model written from the disassembly (toward-zero division as `cltd; and; add; sar`), not from the C++ or the earlier vectors; the agreement with T32.6 §3 is a check, not the proof. `res` = `residents_64`.

| Id | Rule and primary evidence | Smallest wrong reading | Distinguishing input → expected | Future assertion / reviewer disposition |
|---|---|---|---|---|
| A1 | Player mask low 23 bits: `and eax,0x7fffff` (`0x1001d397`), stored at `+0x90` | keep default 1 | HEAD+0x48 = `0x1f`, HEAD+0x5c = 0 → `0x1f` (not 1) | created world's `players[].flags` |
| A2 | High reconstruction: byte HEAD+0x5c bit3→`0x01000000`, bit2→`0x08000000`, bit4→`0x20000000`, bit5→`0x40000000` (`0x1001d39c`-`0x1001d3d5`) | low bits only; or other bit mapping | (`0x1`, `0x3c`) → `0x69000001`; (`0x1f`, `0x20`) → `0x4000001f` | mutants: high reconstruction omitted, assignment dropped |
| A3 | Ignored packed bits and bit 23 | any bit of the packed byte or of bit 23 leaks | (`0xffffffff`, `0xc3`) → `0x007fffff`; (`0x800000`, 0) → 0 | bit-by-bit |
| A4 | Explicit stored zero versus absent | "0 means unset ⇒ 1" | stored 0 → 0 (a mask with no stage bit, not even `INFRA_EVER`); omitted spec → 1 | optional semantics |
| D1 | Gate: `cmpw $0x9c4,%cx; jb` unsigned (`0x1007624b`), `last = u16(last+2500)` (`0x10076259`-`0x1007625e`) | signed compare; `last = t` | (2499,0) skip; (2500,0) take→2500; (65535,0) take→2500; (65536,0) skip; (68036,0) take→2500; (66500,64000) take→964 | one call = one step |
| D1b | Timed-transition progress (evidence contract): the caller's smallest supported interval is d = 0 (a call every city step), the smallest positive interval 1 tick | a gate that consumes time or rearms | (t 0, last 0) → skip, unchanged; (t 1, last 0) → skip; partition: last 0, t 5000 → take (last 2500), call again at t 5000 → take (last 5000), again → skip, unchanged; save/reload of `last` (hashed, saved) resumes the same | one step per call; partial state is `last` only |
| D2 | `E` smoothing `trunc(127E/128)+trunc(res/64)` (`0x10076286`-`0x100762a5`) | no smoothing; floor instead of trunc | (E 100, res 63) → 99; (128, −65) → 126; (128, −10) → 127; (640, 320) → 640 | |
| D3 | Negative sum | store a signed `E` | (E 0, res −65) → sum −1: not a widened `E` (original low 16 bits = 65535) | labelled domain (OD-4) |
| D4 | Decay only for a set stage bit: `shll %cl,%edx; testl` (`0x100762db`-`0x100762e2`), `BauFirst` at goods `+2` | decay all needs; compare numerically | SALZ (`BauFirst 1C` = index 3): mask `0x1` → clear; mask `0x9` → set | |
| D5 | Full 32-bit test | mask the dword to 23 bits | modded need `BauFirst` index 24: mask `0x01000000` set; `0x007fffff` clear | high bits matter only to modded data (F-2) |
| D6 | Satisfaction `trunc(1536·s'/(E'·Menge))` on the **decayed** stock `s' = trunc(127·stock/128)` (`0x100762f8`-`0x10076314`), cap 160, 64-bit-safe multiply | a 32-bit multiply of the product | pre-decay stock 2818221 (decays to 2796203), E' 128, Menge 12: `1536·2796203 = 4294967808 = 2³² + 512` → result 160; a u32 multiply gives 512 → s 0. (Pre-decay 2796203 decays to 2774357, product 4261412352 < 2³² but > 2³¹, so only a *signed* i32 multiply misreads it.) Stocks above 65535 are outside the original's u16 domain: an engine widening (OD-3/OQ-452) | the kernel's intermediate must be wider than 32 bits |
| D7 | Spawn threshold `cmpl $0x66; jge` (`0x10076341`), after cap 160; `type_max > walkers` (`0x1007636f`) | `≤ 102`; flag without walkers | stock 102→101 → `lowest` 101 → true; 103→102 → false (E 128, Menge 12, res 64) | boolean only with walker count and type max |
| D8 | Cap crossed: `cmpl %eax,%edx; jge` signed, strict (`0x10076468`) | `≥` | `Wohnmax 8`: 512 not crossed; 513 crossed, correction `trunc(513/64) − 8 = 0`; 576 → 1 | upgrade *not evaluated* |
| D9 | Floor: `movswl; cmpl; jge` signed strict (`0x10076540`-`0x10076545`); floor `48·prev` or 64 | unsigned or `≤` | prev 8 → floor 384: 384 not entered; 383 downgrade arm; `NoDowngrade`: 64 not entered, 63 remove arm | arm selection |
| D10 | **Below-64 correction signed** (`cmpw $0x40,%bp; jge`, `0x1007654b`/`0x1007654f`) | unsigned word | res −1, −6, 0, 63 → add `trunc(res/64) − 64`, res := 64; res 64, 65 → no correction | the unsigned reading reads −1 as 65535 and skips |
| D11 | Arithmetic of a negative result (**reachability is not shown**) | treating D10 as dead code | PIONIER res 10 + `VermehrTab[−2]` (−16) = −6 → remove arm (`NoDowngrade`); SIEDLER res 30 − 25 = 5. Imported residents are multiples of 64 and a floor-64 group removes a house below 64 first, so a negative value needs a crafted or modded state | package E decides reachability |
| D12 | Mood precedence (`0x10075fe9`, `0x100761b1`-`0x10076220`) | flags override `E = 0`; always average; `0x10` forces −1 on the min path | (E 0, any, flags `0x10`) → 0; (−1,5,5, 0) → −1; (−1,5,5, `0x10`) → −1; (−4,−4,0, `0x10`) → −4; (1,1,1, `0x10`) → −1; `0x20`/`0x30` → −2 | |
| D13 | Index `trunc((s+4)/8)`, caps 128/160 | no cap | s 159 → food index 16, infra index 20; s 3 → 0, s 4 → 1 | table has 21 entries |
| D14 | Out-of-range mood | index the table anyway | mood 3 → undefined read, not a growth | *not evaluated* |
| C* | Group-derived rows | | see `population-inputs.md` §5 | |
| B* | Goods-metadata rows | | see `population-inputs.md` §5 | |

## 8. Verification record, review and limits

**Re-derived by the author from primary sources in this task** (scripts in the ignored scratchpad, not tracked): the disassembly of the tick, mood, the `Ware` parse and the post-pass (above addresses); the 32 stage names at `AnnoGame 0x10024bd0`; the `BGruppen.dat` `[WAREN]` `BauFirst`/`IdSec` values and the five groups' endpoints (independent script); the Create argument frame; the layout arithmetic for OD-5; the numeric rows of section 7. **Not re-derived:** the property-`0x21` copy (T32.5), the HEAD buffer base for `0x1001d389` (taken from T32.5/P2.1: `esp+0xf8`; the two offsets `0x140`/`0x154` are re-read), the corpus histograms, the callers of Create, the city/upgrade code (`0x10075dd0`, `0x10029610`), fire/plague.

**One data point, not a measurement:** `scenario corpus: a shipped town's economy` (existing case, `single36`): 388 houses, all with residents > 0, sum 2283.

**Independent review:** pending in this revision; recorded in the `docs/orchestration/REPORTS.md` MAP-POP-READY-1 entry (reviewer identity, reviewed revision, scope, dispositions).

**Limits.** Windows/MSVC, Linux/WSL, original-runtime, GPU and scale checks are NOT_RUN. No C++ was built or run for this packet. The corpus-wide zero-resident count, the callers of Create and the below-64 counter reachability are open (package E). Stream 126 and PRNG timeline 4 are unchanged.
