# In-game UI, construction menu and object placement (observed by running)

**Status:** first pass (2026-10-22), from driving the original (GOG 2.0.0.5) with `tools/orig/control.py`
in a single-player **Citizen** continuous game (the copy at `C:\anno1503-original\` through dgVoodoo2).
Screenshots in `cache/orig/ui/` (git-ignored). This note records what the menus and the placement UI
do, and ties the visible ranges to the `Anlagen.dat` data fields (the RE notes in
[`production.md`](production.md) and [`population.md`](population.md)); exact per-building tile ranges
come from that data, not from pixel-measuring the overlays.

## 1. HUD layout (1024 x 768 in play; coordinates below are fractions of the window)

- **Minimap** top-right (~0.78–0.98 x, 0.02–0.22 y); zoom `+`/`-` and two rotate arrows just below.
- **Four main tabs** at y ≈ 0.285: **Construction menu** (house icon, x ≈ 0.80), **Military** (bust/helmet,
  0.85), **Help/info** (`?`, 0.90), **Options/game menu** (gear, 0.95).
- **Nameplate** (player/settlement) y ≈ 0.35; below it a **statistics column** (population, balance,
  goods in/out, military, a per-minute gold balance shown red when negative, e.g. `-13`).
- **Bottom resource bar**: gold (e.g. 500000), tools (hammer), wood (planks), bricks, a civilian count,
  a soldier count, and a `?` at the right.
- Clicking a **placed building** replaces the right panel with that building's / settlement's overview
  (clicking the starting trading post showed the colony **"Plantau"** with its stat column and balance).

## 2. Construction menu (the house tab)

Opening the Construction menu shows a **category column** down the left (x ≈ 0.765) and a **building
grid** (x ≈ 0.80–0.99). A wooden **banner** names the current category. Grid cells that are **`?`** are
buildings not yet available at the current settlement stage (Citizen start unlocks only a few); they
unlock as the colony grows (pioneers → settlers → …).

Categories seen (top to bottom), with the buildings unlocked at the Citizen start:

| Category (banner) | Icon | Unlocked at Citizen start |
|---|---|---|
| **Public buildings** | house | **Pioneer house** (settler residence, cost 3 wood), the **Main market building** (1301), a coin/level-I building, and a 4th building I did not verify (NOT a "sawmill" — no such type) |
| **Squares, Streets** | path | a stone **road/path**, a paved square (street placement) |
| **Farms, Plantations** | ox | the **forester (801)** (cost **150 gold + 2 tools**) and several farm/plantation buildings; the specific ids (sheep/grain/potato) are NOT verified, and the **grain farm (811)** needs `INFRA_STUFE_3B` (200 citizens) so it is not start-available |
| **Workshops** | anvil | the **weaver (1001)** and **tannery (1023)** ARE available from the start (`INFRAMIN = INFRA_EVER`); most other goods workshops are higher-tier |
| **Mining** | crossed picks | `?` at this stage |
| **Military** | spear | `?` at this stage |
| (ranch / other) | animal, keg | `?` |

**Correction (orchestrator review, 2026-10-22):** the **weaver (1001) IS placeable at a fresh
Citizen start** (`INFRAMIN = INFRA_EVER`; the community tutorial builds weaving huts immediately),
so **PR-F2 is not blocked by colony growth**. The grid ids above were read from the info panels,
not checked against the `Anlagen.dat` `INFRA_EVER` list — treat the specific names as unconfirmed.

## 3. Placement flow

1. **Select** a building in the grid → the right panel shows its **info**: the name, an isometric
   **preview** of the building, the **build cost** (tools / wood / bricks / marble icons with numbers,
   plus a gold cost), and two **rotate arrows** (yellow, at the preview's sides) to turn the footprint.
   The cursor enters **placement mode** (a yellow **cancel-X** appears bottom-right at ~0.985, 0.95).
2. **Move over the map** → the valid-placement area and the footprint are shown (section 4). A
   **footprint ghost** (a small yellow diamond outline) follows the cursor.
3. **Left-click** places the building (and stays in placement mode to place more); **ESC**, a
   **right-click**, or the **cancel-X** leaves placement mode.

## 4. Ranges and placement logic (tied to the data)

The visible overlays correspond to `Anlagen.dat` fields the RE notes decode:

- **Settlement / warehouse connection (houses).** Placing a **Pioneer house** draws a **yellow outline**
  around the trading post (Kontor) on the map: the zone where houses may be built and reach the
  warehouse. This is the market/Kontor **action radius** — type byte `+0x65` (production.md:97), the
  "market search radius"; a market's candidate buildings are those in its footprint **grown by that
  radius** (production.md:315). So "warehouse connection" = a house (or producer) lying inside a
  market/Kontor's action-radius rectangle. (Exact tiles = the type's `+0x65` value per building.)
- **Forester (801) range.** Its range is its **action radius `HEAD[0x11]` = 5** (`types.cpp:138`,
  stored at `+0x65`), **not** the radius-7 circle of `production.md:259` — that circle is the
  **mine's deposit search**, a different building. (My earlier "radius 7" cited the wrong line.)
- **Farm fields (farm-ground objects).** A farm places its **field tiles** around the building; the field
  layout is the type's `OBJECTPOS` (the ground objects it owns, OQ-015 in production.md). Grain/potato
  farms show their field as those tiles; harvest fills the farm's raw stock (production.md §4.1, farm
  `+0x1c`).
- **Public-service buildings (church, well, …) radius.** Public services reach houses through a
  **service mask / service radius**: `BuildingType::service_mask` / `service_mask_for_goods`
  question** (`population.md:237`, OQ-T15-SERVICE), not a confirmed rule; infrastructure "goods"
  radius — but **that a public service reaches a house "within the service radius" is an OPEN
  reaches the stage that unlocks it).
- **Roads/streets** ("Squares, Streets") carry the carriers between producers, markets and the
  warehouse; a producer out of road/market reach does not deliver (the carrier path, production.md §3).

`action_radius` is also a `src/sim` type field (`types.hpp:134`), so the rebuild already carries the
range datum; this note records which on-screen overlay each one is.

## 5. Driving and reading the game (for the fixtures)

- `tools/orig/control.py` drives menus and the sim (SendInput; owner-agreed). The menu path to a game:
  main menu → **Singleplayer** (0.49, 0.39) → **New game** (0.49, 0.52) → **Choose game: Citizen**
  (0.36, 0.56) → **OK** (0.80, 0.90). The Construction tab is the house icon (0.80, 0.285); the category
  column is at x ≈ 0.765.
- `tools/orig/probe.py` reads a placed object's state by vtable scan (OQ-T15-BLD): the city/warehouse
  object (vtable `AnnoWelt +0xc2f40`, stock `+0x334+12g`, capacity `+0x624`) is already verified
  (production.md "Verified by running").

## 6. What this unblocks / what is left

- The **city/warehouse and tax fixtures** can be set up and read now (the warehouse layout is verified).
- The **production fixtures that need a workshop** (weaver PR-F2, etc.) require **growing the colony**
  first (place pioneer houses in the Kontor's range, supply them, reach the settler stage that unlocks
  the weaver), then placing and feeding it — a scripted build-up through `control.py`. A later step.
- **Precise tile ranges** per building (the action radius, the field layout) are read from `Anlagen.dat`
  (type `+0x65`, `OBJECTPOS`) rather than measured from the overlays; this note links the overlay to the
  field so a data dump gives the exact numbers.

## 7. Growth build-up: findings and the open blocker (2026-10-22)

Driving a colony build-up (control.py: zoom, place, road-drag, 400% speed) produced these findings:

- **Buildings and costs** (from the info panels): **Pioneer house** 3 wood; **Main market building**
  3 tools / 7 wood / 250 gold / 10 upkeep (Anlagen.dat 1301); **Dirt road** 5 gold; **forester (801)** 150 gold + 2 tools (my earlier "~5 gold" was wrong).
  Public buildings icon 3 is the **Main market building** (the Marktplatz), not a warehouse.
- **House removal without a market: an OPEN mismatch, NOT a finding (OQ-T15-HOUSEGROW).** Placing
  pioneer houses in the Kontor's range populated them (28 pioneers); then over ~80 game-seconds the
  city `+0x2f0` fell 31 -> 21 and `+0x18a` went 4 -> 3 (one house removed). This does **not** prove
  "no market = no growth": `INFRA_EVER` has threshold 0 (always reached), so PO-P2 predicts growth
  to 8 with no deliveries; 31 residents in 4 houses is ~7.75/house, i.e. growth; and starvation
  removal (P5) cannot happen that fast (food decays ~1/128 per 5 s; emptying a house takes ~750
  game-s, not 80). The removal may be the **tool itself** (a road drag across the house, or a
  misplaced click under the screen->tile offset). To decide it, read each house's `+0x2c` residents
  over time (house vtable, OQ-T15-HOUSEVT), the same houses with and without a road to a market, and
  feed the same start into `src/sim`. If the original really refuses growth without a market, our
  house model (`population.md` section 4) is missing that rule.
- **Road connection rule (owner, 2026-10-22).** A building joins the road network at its **specific
  entrance tile** (the green entrance arrows on its info box, manual p15), **not** at any edge tile;
  **hold CTRL while placing to auto-align the entrance to a street** (manual p15). Houses need a
  road to a market, and services (e.g. the fire brigade) reach a house by road. Supply chain:
  Kontor/Warehouse -> road -> **Main market building** (entrance on the road) -> houses (entrance on
  the road). **Still OPEN (OQ-T15-PLACEREFUSE):** does the game *refuse* placement beside an
  unconnected road, or place the building and leave it unsupplied? My blind clicks (confounded by
  trees in the footprint and the screen->tile offset) do not separate the two; it needs one
  controlled placement on clear grass once the placement helper exists.
- **Open blocker for the growth/weaver fixtures.** On the Citizen starting island the land near the
  Kontor is **densely forested**, so the road-adjacent tiles that are also clear enough for a 2x2
  market are scarce, and placing on them blind (isometric screen->tile mapping, trees in the footprint)
  did not succeed over many tries. The road-and-edge mechanic is understood and the road was connected
  to the Kontor; the remaining need is a **clear road-adjacent 2x2**. Running the house-growth
  (PO-P2/P3), tax (PO-T) and workshop (PR-F2 weaver, — available from the start, not gated) fixtures end to end therefore needs: a scenario or
  landing spot with **open grassland**, **clearing trees first** (the forester), or a **placement
  helper** that moves the ghost until the game reports it valid (feedback loop) rather than blind
  clicks. The **measurement side is ready**: city `+0x2f0` (inhabitants), `+0x18a` (houses), `+0x618`
  (stage), warehouse `+0x334`/`+0x624` are all read live; per-house `+0x2c` residents need the house
  vtable (OQ-T15-HOUSEVT).

