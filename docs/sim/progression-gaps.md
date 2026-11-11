# State-138 town progression gaps (MAP-INTEGRATE-2 N)

Status: measurement and research only, 2026-11-12. This package changes no behavior, default,
state identity, scenario fixture or original file. Measurements use state 138 with
`Rules::house_updates = true` and `Rules::city_booking = true`; both remain default-off.

## 1. Method and declared budgets

Before either run started, the budget was fixed at **300 game-seconds = 150,000 ticks** for each
town. The probes are opt-in (`ANNO_PROGRESSION_GAPS=1`) and default-skipped. Every scenario is
loaded from the GOG 2.0.0.5 files under `game/app`; no stock, house, permission, flag or position is
patched.

| Town | Starting path | Measured clock | Booking | Scope |
|---|---|---:|---|---|
| `endless02` | Original `0011SingleEndlosAddon/endless02.sze` component; complete supported program retained | 0 -> 150,000 ticks | on | all 7 imported cities |
| founded ship start | MAP-EXPAND-1's genuine public path: sail, `FoundCmd`, unload 2 t tools, build forester, build the reviewed road | founding path ends at tick 86,000; then 150,000 measured ticks, final tick 236,000 | on | the one founded city |
| `S01_07_01` | Original `0100Kampagne01/S01_07_01.sze` component; unsupported object definitions isolated, never silently executed | 0 -> 150,000 ticks | on | all 14 imported cities |

For each existing house, the diagnostic applies the pure `UpgradeCheck` score inputs to the final
state and records the first failure in original order: food, forced goods and forced
infrastructure. It then checks whether the computed next type exists, whether its materials are
available and whether state 138 classifies it as a regular ladder successor. Missing or declared
special successors are recorded as `next_type`; a material shortage wins over a declared-special
successor because the original's city tail checks that successor's materials before its unchecked
swap. It does not invoke an upgrade. `at_cap` separately records whether residents equal the group's
`Wohnmax * 64`; the predicate projection therefore does not pretend every sampled house was at an
upgrade gate.

The original-data and ship-start opt-in tests pass independently. Original towns: selected 1 /
executed 1 / failed 0 / empty 0 / skipped 0, 7,604 assertions. Ship start: selected 1 / executed 1
/ failed 0 / empty 0 / skipped 0, 5,505 assertions. The raw logs and hashes are recorded in the N
REPORTS checkpoint.

## 2. Measured outcome

### 2.1 Existing houses

| Town / group | Houses | At cap | First failing predicate at final state |
|---|---:|---:|---|
| `endless02` Pioneers | 108 | 106 | food 13; goods 72; infrastructure 5; materials 18 |
| `endless02` Settlers | 51 | 47 | food 31; goods 20 |
| `endless02` Citizens / Merchants / Aristocrats | 0 | 0 | group absent |
| `S01_07_01` Pioneers | 50 | 50 | food 2; goods 47; infrastructure 1 |
| `S01_07_01` Settlers | 3 | 1 | food 2; goods 1 |
| `S01_07_01` Citizens | 64 | 47 | food 42; goods 22 |
| `S01_07_01` Merchants | 41 | 11 | food 41 |
| `S01_07_01` Aristocrats | 0 | 0 | group absent |

Across 317 houses, 262 are at cap. No sampled house is eligible at the final instant. Counts by
first failure are: **goods 162, food 131, materials 18, infrastructure 6, next type 0**. This is a
cross-section, not a causal claim that one correction would upgrade every row: clearing an earlier
predicate exposes the later predicates.

The zero `next_type` count does not prove that every successor is regular. It means every measured
terminal or declared-special house failed an earlier score or material predicate at this final
instant. State 138 deliberately refuses to replay the original's unchecked irregular type swap;
that compatibility boundary remains, but this measurement does not rank it above observed score
failures.

### 2.2 Founded ship-start city and build goods

After the public founding/forester/road path and the declared 300 seconds, the city has **zero
houses in every group**. Its city store has tools 0, wood 2,688 raw (10.5 t), bricks 0 and marble
0. The forester therefore closes renewable wood, while the first city-side tool allotment has been
spent. More tools remain cargo on the original ship and can be unloaded by another public player
command, so this is a **pre-house player-action/resupply boundary**, not proof of an irreversible
simulation deadlock and not a reason to invent automatic construction.

Neither original-town aggregate exhausts a real build good. `endless02` ends with tools 49,302,
wood 38,572, bricks 70,986 and marble 12,800 raw across seven cities. `S01_07_01` ends with tools
127,224, wood 180,118, bricks 113,213 and marble 22,528 raw across fourteen cities. Goods 45 is
named `UNUSED`, starts and ends at zero, and is not treated as a build-goods shortage.

## 3. Original data: needs, suppliers and current support

The need lists below are parsed from `Data/BGruppen.dat`; `force`, `Menge` and `BauFirst` are in the
opt-in registry output. Producer types, input goods, construction stage and research id are parsed
from `Anlagen.dat`. The execution status is determined from the state-138 class table and focused
tests, not inferred from the presence of a data record.

Legend: **implemented** means the relevant state-138 production/service and household path exists;
**typed refusal** names a deliberately refused route or placement; **missing** means the data is
present but the original runtime body or its prerequisite is absent.

| Group | BGruppen needs | State-138 support and remaining limits |
|---|---|---|
| Pioneers | `NAHRUNG` F 41; `LEDER` F 16; `STOFFE` 16; `SALZ` 8; `WIRTSHAUS` 16; `KAPELLE` 41 | Food has implemented small-farm, farm and workshop sources, but the fisher route 0x2d is a **typed refusal** and Hunter is **missing**. Cloth, salt, Inn/alcohol, stalls, chapel service and household delivery are implemented. Tannery onward is implemented; the real hides source is **missing** with wildlife. |
| Settlers | `NAHRUNG` F 37; `LEDER` F 12; `STOFFE` F 20; `SALZ` F 8; `TABAKWAREN` 16; `GEWUERZE` 16; `WIRTSHAUS` F 20; `KAPELLE` F 41; `SCHULE` 20; `KIRCHE` 41 | Generic plantation/farm/workshop chains for tobacco and spices are implemented; public-service and school availability is implemented for resident supply. The same food and wildlife/hides qualifications apply. |
| Citizens | `NAHRUNG` F 37; `STOFFE` F 20; `SALZ` F 10; `TABAKWAREN` F 16; `GEWUERZE` F 16; `LAMPENOEL` 8; `SEIDESTOFFE` 16; `WIRTSHAUS` F 20; `KIRCHE` F 41; `SCHULE` F 31; `BADEHAUS` 41; `HOCHSCHULE` 41 | Silk cloth's farm/workshop path is data-backed by implemented generic classes. Lamp oil's workshop exists, but its `WALSPECK` source is the unported Whaler class: **missing**. Church/bath and school/university service delivery is implemented. |
| Merchants | `NAHRUNG` F 37; `SALZ` F 10; `TABAKWAREN` F 16; `GEWUERZE` F 16; `LAMPENOEL` F 8; `STOFFE` F 12; `SEIDESTOFFE` F 25; `WIRTSHAUS` F 16; `KIRCHE` F 41; `HOCHSCHULE` F 41; `BADEHAUS` F 41 | All listed needs are forced. Generic chains and public services exist, but sustainable lamp oil is still blocked by the **missing** whaler source. The measured town fails food before these later predicates are considered. |
| Aristocrats | `NAHRUNG` F 37; `KLEIDUNG` F 29; `SCHMUCK` 12; `WEIN` 20; `KIRCHE` F 41; `BADEHAUS` F 41; `THEATER` F 41; `PARK` F 41 | Wine and the generic clothing/jewelry workshops are implemented. Sustainable clothing needs `PELZE`, whose wildlife/fur-trapper prerequisite shares the unresolved wildlife lifecycle. Gold/gem source placement remains narrower than the data inventory and must be proved for a chosen town. Church/bath/theatre/park service delivery is implemented. No Aristocrat house existed in the measured towns. |

`F` means the BGruppen need is forced. Non-forced needs still affect growth and satisfaction when
their stage is active; `UpgradeCheck` evaluates the next group's forced subsets according to
`goods_end` and `infra_end`, not this prose table.

Concrete producer facts from `Anlagen.dat` include: tannery 1023 (`TIERHAEUTE -> LEDER`), weaver
1001 (`WOLLE -> STOFFE`), saltworks 1022 (`SALZSTEIN -> SALZ`), Inns 1335/1336 (`ALKOHOL ->
WIRTSHAUS`), tobacco workshop 1008, spice farm 812, lamp-oil workshop 1028 (`WALSPECK ->
LAMPENOEL`), silk workshop 1009 (`SEIDE + FARBSTOFFE -> SEIDESTOFFE`), clothing workshop 1027
(`STOFFE + PELZE -> KLEIDUNG`), jewelry workshop 1039 (`GOLD + EDELSTEINE -> SCHMUCK`) and wine
plantation 822. Type 1005 is an alternate cloth producer gated by stage 31 and research 50; it is
not silently treated as available. The raw registry log contains every producer type, class, tick,
stage, research id and input list.

State 138 also includes the accepted renewable wood, iron/tools and quarry/bricks/marble slices.
Their presence explains why the two imported towns do not exhaust build materials in this window;
it does not establish that every new ship-start city has those chains built or supplied.

## 4. Related Windows evidence

The finished `origin/task/W4-shared-list-1` branch was read with `git show`; it was not merged. Its
population tool uses the same 500-tick/game-second mapping and samples all towns. Of its five shared
scenarios, only `01_1024_easy_256` runs: residents stay 230 and the town-store-total proxy stays
150 t at 0/10/30/60 seconds. `Single_06` refuses an unsupported `QUESTLOG_SHOW`; `Single_16`
refuses an unsupported nested unit/crew record; `S01_08_01` and `Single_24` refuse unsupported
soldier actors. The running original was **NOT_RUN** there because its Wine navigation did not yet
load a named shipped scenario. Those results support the gap inventory; they do not substitute for
the three state-138 measurements above.

## 5. Ranked progression gaps

Ranking is primarily the number of measured houses whose first failure is the gap. The pre-house
ship-start boundary is listed separately because it blocks one settlement but zero existing houses.

| Rank | Gap | Houses / stage observed | Evidence | Original rule source | Dependencies and size |
|---:|---|---|---|---|---|
| 1 | Forced consumer-goods closure | 162 houses; Pioneer through Citizen | `endless02`: 72 Pioneers + 20 Settlers; `S01_07_01`: 47 Pioneers + 1 Settler + 22 Citizens | `Data/BGruppen.dat`; `AnnoWelt.dll` `0x10075dd0`-`0x10075fc4` (forced goods mean and strict table result) | **Several slices, L.** Cloth/salt/Inn are closed; leather is sustainable only after the wildlife decision; later lamp oil needs Whaler, clothing needs pelts, and jewelry needs a proved mineral source. Trade may change the intended answer. |
| 2 | Food satisfaction at the upgrade instant | 131 houses; every present group | `endless02`: 13 Pioneers + 31 Settlers; `S01_07_01`: 2 Pioneers + 2 Settlers + 42 Citizens + 41 Merchants | `Data/BGruppen.dat`; `AnnoWelt.dll` `0x10075dd0`-`0x10075fc4` (next-group food amount and strict food-table result) | **Implementation/measurement, M-L.** Separate capacity, reach and source failures. Fisher route is typed-refused; Hunter/wildlife is owner-blocked. Prefer implemented farms/workshops before expanding scope. |
| 3 | Upgrade materials | 18 Pioneer houses in `endless02` | scores pass and a computed next type exists, but city stock minus reserve cannot pay its material vector | `Data/Anlagen.dat`; `AnnoWelt.dll` `0x1002e000` and materials check `0x1002c030`-`0x1002c08c` | **Town-specific, S-M.** State 138 supplies all four real build goods, so attribute city/house/type and reserve before adding mechanics. |
| 4 | Forced infrastructure | 6 Pioneer houses | five `endless02`, one `S01_07_01` | `Data/BGruppen.dat`; `AnnoWelt.dll` `0x10075dd0`-`0x10075fc4` (forced infrastructure minimum and strict table result) | **Town layout/service, S-M.** Inn and public-service delivery exist; measure missing building, reach, stage or stock per city. |
| 5 | Terminal/special successor disposition | zero first failures at this sampled instant | every measured terminal/special house is masked by an earlier score/material failure; state 138 still has a named compatibility boundary | `Data/Anlagen.dat`; `AnnoWelt.dll` next-type tail `0x1002e000` and unchecked swap `0x1002cbe0` | **Research/contract, M.** Inventory the houses after earlier gaps close; original-runtime spot checks. Any behavior change requires an identity decision. |
| P0 | Founded-city construction bootstrap | one founded settlement, no houses; tools 0, wood 10.5 t | genuine sail/found/forester/road path, 300 seconds | original `endless01` player/ship records; building factory `AnnoWelt.dll` `0x10024010`; construction costs in `Data/Anlagen.dat` | **Product/play-loop, S for proof.** The original ship still carries resupply, so first prove the intended unload/build sequence. Do not invent auto-build or duplicate the ship loadout. |

## 6. Top-three packet outlines

### 6.1 MAP-ADVANCED-GOODS-1

1. Attribute the 162 goods failures to the exact forced need and source chain; do not group all
   consumer goods under one generic shortage.
2. Close already-supported paths first (tobacco, spices, silk cloth, salt, cloth, Inn), with
   source-disabled controls and conservation ledgers.
3. Split unresolved prerequisites into explicit contracts: wildlife for hides/pelts, Whaler for
   lamp oil, and mineral placement/import for jewelry. No labelled synthetic stock may be counted
   as a sustainable original-data source.
4. Coordinate the intended import/trade fallback with owner question D8 before assuming Venetians,
   natives or computer-party trade.

### 6.2 MAP-FOOD-CLOSE-1

1. Add opt-in attribution for the 131 food rows: city, house/type, food score/table entry, household
   stock, city stock, reachable suppliers, producer outputs and cargo in flight.
2. Select one Settler, one Citizen and one Merchant near miss using only already implemented food
   chains; prove a causal producer-enabled/control pair through public commands or unchanged
   original data.
3. Keep fisher route 0x2d as a typed refusal unless its original body is read. Keep Hunter out
   unless the owner authorizes the wildlife lifecycle.
4. Compare at least one named town against the running original before altering a threshold,
   capacity or route rule.

### 6.3 MAP-UPGRADE-MATERIALS-1

1. Attribute the 18 rows by city, house type, computed successor, material vector, city stock and
   reserve; retain an already-affordable control from the same town.
2. Distinguish a city-local distribution shortage from missing production: state 138 already has
   wood, tools, bricks and marble chains, and aggregate stock is not the upgrade oracle.
3. Prove one shortage-to-affordable transition using only public freight or an accepted production
   chain, then observe the ordinary house tick; do not patch stock, reserve, type or residents.
4. Close spend, reload and deterministic-continuation ledgers, and keep terminal/special successor
   disposition a separate compatibility question if it becomes the next predicate.

## 7. Decisions outside this packet and original-runtime boundary

- **Wildlife:** `docs/sim/early-needs.md` leaves the fauna sweep, animal lifecycle and Hunter/job
  0xd behind an owner decision because they consume world draws and change identity. The labelled
  leather fixture proves tannery onward only; it does not produce hides.
- **Trade / D8:** `docs/plan/lan-mvp.md` section 8 asks whether Venetians and natives are in the
  MVP. They require CompLogic; without them there is no computer party to trade with. No progression
  package may assume that trade source before the owner answers.
- **Fire and disasters:** fire, plague, volcano and scripted disasters are not state-138 town
  mechanics. They affect buildings, house mood and shared draws, so their eventual implementation
  may change progression trajectories; `docs/re/fire-and-ships.md` is research, not an executed
  replacement.
- **Running original required:** only the original can confirm the 300-second distributions for
  these exact scenarios, the special-successor behavior visible to a player, unported Hunter,
  Whaler and fisher production, scenario-script consequences removed by component isolation,
  computer-party trade, and fire/disaster effects. The Mac Wine named-scenario run and the Windows
  original game-file run are **NOT_RUN** in N; none of those values is guessed.

## 8. Decisions and reasoning

- Ranked observed first failures, while preserving predicate order and the separate cap count;
  this avoids presenting a final-state projection as a one-fix upgrade forecast.
- Kept the ship-start result honest: zero city tools blocks the next city-side build, but remaining
  ship cargo makes manual resupply possible.
- Distinguished a parsed producer from an implemented source. A Workshop record for lamp oil does
  not make Whaler available; a tannery does not create wildlife hides.
- Reused the accepted W4 measurement by reading its branch only; no W4 code or moving branch was
  merged into state 138.
- Proposed contracts and causal measurements, not behavior changes, because N is explicitly a
  measurement/research package.
