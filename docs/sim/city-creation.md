# City creation, registration, naming and Kontor identifiers (task MAP-2, packages B–D)

Status: contract, reviewed before the dependent implementation. It specifies the internal headless
operations a later founding command will call; it adds no command, codec alternative or `Reject` value.
Evidence tags: **D** = instructions read (AnnoWelt.dll, addresses are VAs), **P** = file data, **S** = inferred,
**ADAPT** = a stated adaptation, **OPEN** = unresolved. The research this builds on is
`docs/re/ship-founding.md` §10.x–§12 and `docs/re/islands.md`; new reads of this packet are marked *(new)*.

## 1. Effect table (original order: `0x10041760` → `0x100861c0` → `0x100967e0` → `City::Place 0x100279a0`)
| # | Address / field | Condition and order | Engine representation | Persistent or derived | Evidence | Acceptance test |
|---|---|---|---|---|---|---|
| E1 | `0x100967e0` slot choice | `number < 0`: scan the ring from `ring+0x40 + 1`, modulo the slot count, over all slots once; first empty wins; none → failure `0x80004005` (the original fails after the payment) | `Island::city_cursor`; usable slots are `0..city_slots-1` except `neutral_owner`; ADAPT: the check precedes any change | cursor persistent (stream 119) | D `0x10096816`–`0x1009684d` | allocation order, gaps, wraparound, exhaustion, neutral skip |
| E2 | `0x100967e0` city object | created (`0x1002f990`), attached to the island (`+0x28`) | `make_city(number, owner, cfg)` into `Island::cities[number]` | persistent (existing) | D `0x10096852`–`0x100968a1` | membership/index consistency |
| E3 | `0x100967e0` `ring[number]` / cursor | `ring[number] = city`; `ring+0x40 = number` (the cursor is the LAST ADDED slot, set by every add, allocation or explicit) | `Island::city_cursor = number` | persistent | D `0x100968a4`–`0x100968a7` | cursor after allocation |
| E4 | `0x100967e0` highest used | `ring+0x47` (signed byte) raised to `number` if larger; **recomputed on removal** (`0x10096990`: highest non-null slot, else −1) | derived from `Island::cities`; not stored | derived | D `0x100968b3`–`0x100968bb`, `0x10096990`–`0x100969bb` | none (no consumer found, `islands.md`) |
| E5 | `0x100967e0` city stamp | city property `0x23` := stamp; the setter writes seven words (`city+0x8,0xa,0xe,0x10,0x12,0x14,0x148`) *(new)* `0x1002f560` | `City::tick_stamp` (+8) and `BuildingIndex::stamp` (+0x148) := stamp; the other five words are not modelled (OPEN: their consumers) | persistent (existing fields) | D | stamp equality after creation |
| E6 | `0x100967e0` name | world property `0x13` yields a list; if present: **one world PRNG draw** `T[(cursor+1) mod len]`, `mod` the list size, the chosen string → city property `0x12` (`0x10096910`–`0x10096959`); no list → no draw and no name | `City::name` (stream 119); the list is an explicit input (the world property is not modelled) | persistent | D; the list is the world-level `STAEDTE` string list (`0x100dcea4`) *(new)*; empty list: the original divides by zero | draw counts, hand-derived selection |
| E7 | `0x100861c0` step 2 | player null → returns the city only; else `player.slot+0x8c(&tmp, 2, city)` adds the city to the player's kind-2 list and sets the owner | `City::owner`; the player's list is `Island::cities` filtered by owner (ONE source of truth; no second list) | persistent (existing) | D `0x10086210` | membership consistent |
| E8 | `0x100861c0` city property 0 | mask := mask `\|` `1 << owner index`, then `\|` `1 << p` for every player `p` whose `relation(kind 1, p) == 3` *(new: the owner's own bit is set first)* | **not modelled**: needs the relation table (missing) and its consumer is unknown | — | D `0x1008621f`–`0x100862a9` | none: OPEN (A1) |
| E9 | `0x100861c0` island property `0xe` | island word `+0xa6` gets the owner's bit | derived from the owners of `Island::cities` | derived | D `0x100862be`–`0x100862df` | players-present bitmask test |
| E10 | `0x100861c0` byte `[island + 0xb0 + number]` | := player property 9; **`number` here is the third argument as passed: −1 for a ring-chosen slot, so the store lands at `island+0xaf`** *(new)* | not modelled (no consumer found, `islands.md`) | — | D `0x100862e9`–`0x100862f3` | none: OPEN (consumer) |
| E11 | `0x100861c0` island `+0xaa` | `slot +0x134(1)` recomputes it from the cities' masks | not modelled (needs E8) | — | D | none: OPEN |
| E12 | `City::Place 0x100279a0` Kontor counter | PRODTYPE `0x21`..: after `Create`: `old` = island property `0x11` (word `island+0xc6`); property := `(old+1) & 0xffff`; the new building's property `0xe` (KONTID, `building+0x1e`) := `old` *(new: the original wraps at 16 bits)* | `Island::kontor_counter` (u32, default 1); `Building::kontor_id`; ADAPT: u32 with a checked increment | persistent (stream 119) | D `0x10027c63`–`0x10027c9d`; reset/ctor store 1 (`0x1008c7fd`, `0x1008cbe9`) | counter/KONTID before and after, overflow refusal |
| E13 | island loader `0x10089bb1` | record `KONTORID` (2 bytes) → `island+0xc6`; serializer `0x1008a6aa` writes it on every save *(new)* | `Island::kontor_counter` loaded from the record when present | persistent | D | loaded counter preserved |
| E14 | market loader `0x1006be00` | nested `KONTID` u16 → `building+0x1e` | `Building::kontor_id` (kept since stream 119) | persistent | D (`annowelt.md` 164) | loaded KONTID preserved |

Not part of city creation (separate units): the territory claim (`territory.hpp`), the tile writes and the two posted messages (OPEN), the payment (`founding_payment.hpp`).

## 2. Cursor identity (B2)
`ring+0x40` (dword, **allocation cursor = last added slot**) and `ring+0x46` (byte, **scheduler cursor**) are different words *(constructor `0x10096da0`: `+0x40 = -1`, `+0x46 = 0`, `+0x44 = 0`, `+0x47 = 0xff`; reset `0x100967b0` sets `+0x44` stamp, `+0x46 = 0`, `+0x47 = 0xff` and does not touch `+0x40`; the scheduler `0x100969d0` uses `+0x44`/`+0x46` only; removal `0x10096990` changes `+0x47` only)*. `Island::ring.cursor` is the scheduler's and is neither read nor written by city creation; `Island::city_cursor` is new. Removal leaves the allocation cursor untouched (the engine has no city removal yet; when it does, it must not change the cursor).

## 3. Initialization (B3)
- Empty island (fresh object): `city_cursor = -1`. A reset of an existing island does not change it (reset leaves `+0x40` alone); the engine has no island reset.
- Loaded island with cities: the loader adds each city through the same function with its explicit number, so after the load `city_cursor` = the number of the **last city created in load order** (ADAPT: spec order); gaps are legal; the cursor is NOT inferred from the highest occupied slot. An island with no city keeps `-1`.
- Highest-used metadata: derived (E4), never stored.
- Names: an absent list (no name data supplied) → no draw, `name` empty; a non-empty list → one draw; an empty list or an entry containing NUL → refusal `UnusableNameData` before any change (ADAPT; the original divides by zero on an empty list). Names loaded from a file are not read yet (no source record is decoded): `City::name` is empty for loaded cities.
- Kontor counter: default 1; loaded value = the `KONTORID` record's u16 when present.

## 4. Refusals and their order (B4)
`create_city` is `prepare` (const, no mutation, no live PRNG advance) then `commit`. Prepare checks, in this order, each with its own refusal: `UnknownIsland` → `UnknownPlayer` → `InvalidMetadata` (stamp does not fit u16 / `city_slots` unusable) → `OwnCityOnIsland` (the owner already has a city on this island: P9, same player and same island only; another player's city, here or elsewhere, never refuses) → `NoCitySlot` (no usable slot) → `UnusableNameData` (empty list or NUL) → the name draw is PEEKED on a copy of the world generator. Commit re-checks that nothing changed since prepare (`StalePlan`) and then applies E2, E3, E5, E6 and the single draw. The Kontor step (`prepare_kontor` / `commit_kontor`) is separate: `CounterOverflow` when the counter is `UINT32_MAX`; nothing is incremented by preparing, previewing, failing or by save and load.

## 5. State and PRNG timeline (stream 119, timeline 3)
State: `Island::city_cursor` (i32; with the other island scalars, after `lock`), `Island::kontor_counter` (u32), `City::name` (string), `Building::kontor_id` (u32): `kStateHashVersion` 118 -> 119. Defaults are `-1`, `1`, empty and `0`; scenario loading sets the cursor to the last created city's number (§3), the counter from `KONTORID` and `kontor_id` from the market's `KONTID`.
PRNG: the name draw is a NEW draw site, so the timeline is bumped, 2 -> 3 (`kPrngTimelineVersion`; `tools/sim_stream.py`; the replay header carries the number): enumerator `DrawSite::CityName` (D `0x10096910`: `world.slot +0xb4` = `0x100a1740`, `T[(cursor+1) mod len]`), phase `Advance` (a command applied inside the world step), ported. It draws through `GlobalDraw` (the one door to the world PRNG). A created city consumes exactly one table step when a name list is supplied and none otherwise. The unported-site list in the replay manifest does not change (the site was not in it). Goldens: worlds that create no city change only through the two version numbers and the added default fields; this is checked against `tools/sim_stream.py`, not accepted from the C++.

## 6. Cause-specific test rows (claim | valid control | intentional defect | earlier checks satisfied | expected observation | test)
| Claim | Valid control | Intentional defect | Earlier checks satisfied | Expected | Test |
|---|---|---|---|---|---|
| unknown island refused first | valid request on island 0 | island id with no island | — | `UnknownIsland`; state and PRNG unchanged | `city creation: refusals in order` |
| unknown player | same control | player id not in the table | island exists | `UnknownPlayer` | same |
| own city refuses; rival coexists | owner 1 has no city; owner 2 has one | owner 1 given a city first | island, player ok | `OwnCityOnIsland` for owner 1; owner 2's city does not refuse owner 1 | `city creation: own city and rivals` |
| full ring refuses | an island with a free slot succeeds | every usable slot occupied by other owners | island, player, no own city | `NoCitySlot`; slots and cursor unchanged; the neutral value is never allocated | `city creation: allocation exhaustion` |
| allocation order | cursor −1 → 0; cursor 5 → 6; cursor at the last usable slot wraps | gaps at 0 and 3 with cursor 2 | — | next slot = first free after the cursor, wrapping | `city creation: allocation order` |
| neutral never allocated | capacity 16 (neutral 15) | slots 0..14 occupied | — | `NoCitySlot` (not 15) | same |
| unusable name data | non-empty list succeeds | empty list; entry with NUL | all earlier checks | `UnusableNameData`; PRNG unchanged | `city creation: naming` |
| draw count | no list: 0 steps; list: 1 step | — | — | cursor advance 0 / 1; selection `T[(c+1) mod len] mod n` hand-derived | same |
| failed prepare leaves bytes | save bytes before | any refusal above | — | `World::save()` byte-identical, same `state_hash`, same PRNG cursor | each refusal row |
| counter overflow | counter `UINT32_MAX - 1` succeeds | counter `UINT32_MAX` | island exists | `CounterOverflow`; counter, building KONTID unchanged | `kontor ids` |
| KONTID is the old counter | counter 7 | — | — | building KONTID 7, counter 8; building serial counter unchanged | same |
| no increment on preview/save/load | counter 7 | prepare twice, save/load | — | counter still 7 | same |
| stale plan refused | prepare then commit | another city allocated between | prepare ok | `StalePlan`; nothing applied | `city creation: stale plan` |

## 7. Unresolved, with the exact dependency (not marked done)
E8 city property 0 / E11 island `+0xaa` (relation table and consumer), E10 the `+0xaf` store (consumer), the five other stamp words of E5, the world name list's source record, the messages, the discovery stage, and the first-warehouse virtual callees (research ledger of this packet, `docs/re/ship-founding.md` §13).
