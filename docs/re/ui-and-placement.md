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
| **Public buildings** | house | **Pioneer house** (settler residence, cost 3 wood), a market/warehouse, a sawmill, a coin/level-I building |
| **Squares, Streets** | path | a stone **road/path**, a paved square (street placement) |
| **Farms, Plantations** | ox | sheep farm, a plantation, **grain farm**, **forester ("Forest")** (cost ~5 gold), potato farm |
| **Workshops** | anvil | mostly `?` (the weaver and other goods workshops need a higher stage) |
| **Mining** | crossed picks | `?` at this stage |
| **Military** | spear | `?` at this stage |
| (ranch / other) | animal, keg | `?` |

So the **weaver** (production fixture PR-F2) and most workshops are **not placeable at a fresh Citizen
start** — they require growing settler houses first. The farms, the forester, the pioneer house, roads
and the market are available immediately.

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
- **Forester ("Forest") tree radius.** The forester plants/chops trees within a **circle of radius 7**
  around its footprint, with the per-row half-width table `[7, 7, 7, 6, 6, 5, 4, 2]` (production.md:259),
  already emulated exactly in `src/sim`. That is the "woodcutter tree range / tree ground objects".
- **Farm fields (farm-ground objects).** A farm places its **field tiles** around the building; the field
  layout is the type's `OBJECTPOS` (the ground objects it owns, OQ-015 in production.md). Grain/potato
  farms show their field as those tiles; harvest fills the farm's raw stock (production.md §4.1, farm
  `+0x1c`).
- **Public-service buildings (church, well, …) radius.** Public services reach houses through a
  **service mask / service radius**: `BuildingType::service_mask` / `service_mask_for_goods`
  (`src/sim/types.hpp:137,361`); infrastructure "goods" `0x4c`–`0x5a` reach a house within the service
  radius (population.md:213, 237). The chapel/church is in **Public buildings** (locked until the colony
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
  3 tools / 7 wood / 250 gold / 10 upkeep; **Dirt road** 5 gold; **forester ("Forest")** ~5 gold.
  Public buildings icon 3 is the **Main market building** (the Marktplatz), not a warehouse.
- **Houses need a Main market building, not just the Kontor, to grow.** Placing pioneer houses in the
  **Kontor's** yellow range populated them (28 pioneers) but they then **declined** (city `+0x2f0`
  31 -> 21, `+0x18a` 4 -> 3, a house removed) — because there was **no Main market building** in range,
  so the houses never reached INFRA (PO-P2's precondition) and starved out. The real "warehouse/market
  connection" is: Kontor (warehouse, stores goods) -> **Main market building** (distributes to houses
  in its radius, = INFRA) -> houses. A pioneer house in a market's range grows to 8 even with no
  deliveries (PO-P2); food (NAHRUNG) only affects satisfaction/mood.
- **Road connection rule (owner, 2026-10-22).** A building joins the road network (and so the Kontor)
  by sitting on the **edge of a road** — placed **adjacent to** a road tile, **not on** the road and
  **not touching shore**. The road must itself reach the **Kontor's land entrance** to carry goods.
  Confirmed in driving: a **Pioneer house** (cheap, few requirements) placed freely, but the **Main
  market building** would only place when its clear 2x2 footprint sat beside a **Kontor-connected**
  road; on the road, on shore, on trees, or beside a disconnected road segment it did not place (gold
  not deducted). So the supply chain is Kontor (warehouse) -> road -> **Main market building** on the
  road edge -> houses on the road edge.
- **Open blocker for the growth/weaver fixtures.** On the Citizen starting island the land near the
  Kontor is **densely forested**, so the road-adjacent tiles that are also clear enough for a 2x2
  market are scarce, and placing on them blind (isometric screen->tile mapping, trees in the footprint)
  did not succeed over many tries. The road-and-edge mechanic is understood and the road was connected
  to the Kontor; the remaining need is a **clear road-adjacent 2x2**. Running the house-growth
  (PO-P2/P3), tax (PO-T) and workshop (PR-F2 weaver) fixtures end to end therefore needs: a scenario or
  landing spot with **open grassland**, **clearing trees first** (the forester), or a **placement
  helper** that moves the ghost until the game reports it valid (feedback loop) rather than blind
  clicks. The **measurement side is ready**: city `+0x2f0` (inhabitants), `+0x18a` (houses), `+0x618`
  (stage), warehouse `+0x334`/`+0x624` are all read live; per-house `+0x2c` residents need the house
  vtable (OQ-T15-HOUSEVT).

