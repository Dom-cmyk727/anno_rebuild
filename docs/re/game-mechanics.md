# ANNO 1503 game mechanics (from the official manual + in-game observation)

**Source:** the game's own `Manual.pdf` (EA Games English manual, 39 pages, bundled in `game/app`),
read 2026-10-22, plus the in-game driving of 2026-10-22 (`docs/re/ui-and-placement.md`). This is the
player-facing mechanics; the exact ids, offsets and formulas are in the RE notes
([`production.md`](production.md), [`population.md`](population.md), [`tick-loop.md`](tick-loop.md))
and `docs/sim/`. Summary in our own words (not a copy of the manual).

## 1. Key bindings (Command Reference, manual p4-5) — authoritative

These correct and complete `tick-loop.md` §2's guesses and the control-tool usage.

**General:** F1 Help · **F2 = Zoom level 3 (furthest out)** · **F3 = Zoom level 2** · **F4 = Zoom
level 1 (closest)** · F5 normal speed · F6 double · F7 quadruple · F8 half · F9 quick load · F10
quick save.

**Map:** X rotate map clockwise · Y rotate map anticlockwise · `.` rotate **object** clockwise · `,`
rotate object anticlockwise · J jump to active object · SPACEBAR jump to previous event · **H jump
between your cities** · **P pause** · M toggle unit identification · N jump to next scout · W fly
flag · CTRL+W throw cargo overboard.

**Groups:** 0-9 group selection (press twice = select and centre) · CTRL+0-9 create group · CTRL+D
open all city gates · ALT+D close all city gates · SHIFT+S screenshot.

**Menus:** O Options · **D Player Menu** · **B Construction Menu** · S Ship list · C City list ·
Q Rating.

Consequences for driving the original (`tools/orig/control.py`): set zoom with **F2/F3/F4** (the
owner's "furthest or one above default" = F2 or F3), not only the wheel; **pause = P**; open the
construction menu with **B**; rotate a building-in-placement with `.`/`,`.

## 2. Starting a settlement (manual p11-16)

1. **Discover (p11).** The game starts on a ship sailing an island chain. As the ship nears an
   island, the **Information Bar** shows its **climate zone** and which crops grow best; **dense
   vegetation = high fertility**, bleak land = low yield. **Mountains** carry the mineral deposits
   (Stone, Marble, Salt, Iron Ore, Gold, Gems) and cannot be crossed (a natural wall). The **Scout**
   (a military unit, moves goods by donkey) explores agriculture and, by right-clicking a mountain
   range, finds mineral deposits. Pick an **uninhabited** island for the first settlement (some
   cultures attack strangers).
2. **Establish (p12).** Build a **Warehouse** for a **coastal** settlement, or use the **Scout** to
   build a **Main Market Building** for an **inland** settlement. Then **unload the ship** to start
   building.
3. **Build the infrastructure (p13).**
   - **Streets:** extend roads from the Warehouse/Main Market Building out to the edge of its Service
     Area; link every building with streets.
   - **Production chain** (the core rule): **primary producer (e.g. Sheep Farm) -> workshop (e.g.
     Weaver) -> storage (Main Market Building or Warehouse) -> sales (a market stand)**.
   - **Market stands:** inhabitants buy their needs at market stands (which earn money); one stand
     per goods type; a **queue of customers** means you need another stand of that type.
   - **Houses and public buildings:** once roads and the building-material + food chains exist, build
     Houses (up to **30** for the first stage) and the first public buildings; link all with streets.

## 3. Placement rules (manual p9, p15-16) — this is what blocked the in-game build-up

- **To place a building:** pick it in the Construction Menu (cursor becomes the building), move to a
  spot, click. Requirements (money, tools, materials) show under the stockpile on the Information
  Bar and **blink red** if lacking. A **Fertility Indicator** over some buildings shows how well it
  will function there.
- **Building entrance and streets (the key rule):** every building has an **entrance**, shown as
  **green arrows** on its info box; its items cannot be transported unless the **entrance is aligned
  with a street**. **Press and hold CTRL while placing to auto-align the building to an existing
  street** — this is the fix for the market-placement failures in `ui-and-placement.md` (I was
  positioning by hand on a tree-cluttered road edge; CTRL snaps the entrance to the road).
- **Service Areas (p15):** a building's Service Area is its sphere of influence (a light boundary,
  shown when you click it). The settlement's total Service Area is the union of its Warehouses and
  Main Market Buildings. **Linked production-chain buildings must share a Service Area:** raw
  producers must sit in a manufacturer's Service Area, and manufacturers in a Warehouse/Main Market
  Building's Service Area. (This is the `action_radius` / service-radius of the RE notes,
  `ui-and-placement.md` §4.)
- **Overload / missing materials (p16):** a building producing faster than it ships shows a `?` + a
  full barrel (overload — from a missing street link or a mis-aligned entrance); a stalled building
  shows the Raw-Materials-Missing icon. **Storerooms** are the part of a Warehouse/Main Market
  Building that holds goods; **market wagons** auto-collect from producers to the storeroom — more
  Warehouses/Main Market Buildings = more wagons.
- **Operating costs (p15):** functional buildings cost upkeep; the **Zzzz** icon closes a building
  temporarily (lower cost). Demolish with the DEMOLISH icon; nothing can be built with enemy units
  nearby.

## 4. Growth: civilisation levels and needs (manual p13-14)

- **Five civilisation levels: Pioneers -> Settlers -> Citizens -> Merchants -> Aristocrats.** The
  first four **upgrade automatically** once the inhabitants' needs are met (houses also upgrade
  automatically if enough building material is in stock). **Aristocrats** need dedicated Aristocrat
  housing (unlocked after enough Merchant-level inhabitants); their goods are high-end and if unmet
  their houses collapse.
- **Needs:** click a house to see its needs — a **goods** field (how well goods demand is met), a
  **public-buildings** field, a **coloured** field (current needs) and a **grey** field (upcoming
  needs). Fulfilling needs advances the level. (The exact per-group goods tables are in
  `population.md`'s BGruppen data: PIONIER/SIEDLER/BUERGER/KAUFLEUTE/ARISTOKRATEN.)
- **Building availability:** a building not yet available shows a **`?`**; available-but-not-yet
  constructable shows a **grey** icon (hover it for the requirement). This is why most of the
  construction grid is `?`/grey at the Citizen start (`ui-and-placement.md` §2).

## 5. Production chains (manual p33-38)

Each chain: raw source -> primary producer -> raw good -> workshop -> finished good -> storage ->
market stand. The dependency graphs (useful for setting up fixtures and matching `production.md`):

- **Food & Salt:** Food from Hunting Lodge (Wild Game), Small Farm (Grazing Land), Fisherman's Hut
  (Coast), Bakery (Flour <- Mill <- Grain <- Grain Farm) or Butcher's Shop (Cattle <- Cattle Farm).
  Salt: Salt Works <- Rock Salt <- Salt Mine (Salt Deposit). Sold at the **Food and Salt Stand**.
- **Building material:** Tools (Toolmaker <- Iron + Wood); Wood (Forester's Hut <- Forest); Bricks
  (Stonemason <- Quarry <- Mountain); Marble (Marble Stonemason <- Marble Quarry <- Marble Deposit).
  All feed the **Main Market Building**.
- **Alcohol:** Distillery (Sugarcane), Small Farm (Potato), Brewery (Hops <- Hop Farm) -> **Tavern**.
- **Cloth & Leather:** Cloth (Weaver's Hut / Weaving Mill <- Wool <- Sheep Farm/Cotton Plantation);
  Leather (Tannery <- Hides <- Hunting Lodge). Sold at the **Cloth and Leather Stand**. Higher:
  Clothing (Tailor's Shop) and Silk Cloth (Dye Works <- Dyes/Silk) -> **Clothing Stand**.
- **Tobacco & Spices:** Tobacco Factory (Tobacco Plantation), Spice Plantation -> **Tobacco and Spice
  Stand**.
- **Jewellery:** Goldsmith <- Gold (Gold Mine) + Gems (Gem Mine) -> **Jewellery Stand**.
- **Lamp Oil:** Whale Oil Factory <- Whale Blubber <- Whaler (ship) <- Whales -> **Lamp Oil Stand**.
- **Wine:** Winery <- Vineyard -> **Wine Stand**. **Medicinal Herbs:** Medicinal Herb Plantation ->
  **Doctor**.
- **Ropes/Weapons:** Ropemaker <- Hemp (Hemp Plantation); Ropes+Wood -> War Machine Maker (War
  Machines, Siege Towers) / Bow Maker (Bows, Crossbows).
- **Weapons (metal):** Iron (Ore Smelter <- Iron Ore + Charcoal <- Charcoal Burner); Iron -> Weapon
  Smithies / Gunsmith (Muskets, Ship Cannons) / Armourer (Armour) / Cannon Foundry (Cannons,
  Mortars).

## 6. Trade, research, combat (manual p17-32), briefly

- **Ships (p17):** trading vessels (big cargo) and warships (armed); built at a Shipyard from Money,
  Wood, Cloth, Ropes (+Ship Cannons for warships). Heavily loaded/damaged ships are slower.
- **Diplomacy (p18-20):** Trade Agreements (prereq to buy/sell with a player), Peace Treaties,
  Military Alliances (need a Trade Agreement first), Tributes, Protection Money / hiring Pirates.
  The Diplomacy Status bar colour: green = good, red = hostile, blue = treaty offered.
- **Trade (p21-25):** **passive** (buy/sell at your own Warehouse/Main Market Building via sliders:
  amount in tons, price per ton), **active** (your ships/Scouts carry goods under an agreement),
  **barter** with the 10 cultures (Africans, Aztecs, Bedouins, Eskimos, Native Americans, Moors,
  Mongols, Polynesians, Venetians, Pirates) and the Venetians (free traders, no agreement needed).
  **Automatic Trading Routes** (set route points, up to 4 load/unload actions each). Balance sheets
  via the Warehouse/Main Market "City Status" tab.
- **Research (p26-27):** Knowledge from Schools/Universities; categories Weapon Production, Military
  Enhancements, Naval Technologies, Civilian Research; a Library raises the knowledge limit; one
  category at a time.
- **Combat (p28-32):** soldiers (close: Crew/Cavalry/Lancer/Pikeman/Medic/Scout/Swordsman/Musketeer;
  ranged: Crossbowman/Archer/Marksman) trained in a Fortress; War Machines (Catapults, Mortars,
  Cannons, Siege Towers) need crews; attack modes Passive/Normal/Aggressive; defences City Wall,
  Watchtower, Cannon Tower, Fortress; naval combat with warships. Territory and terrain affect
  combat.

## 7. What this changes for the rebuild work

- The control tool should prefer **F2/F3/F4** for zoom and **B**/**P**/**D**/**C** for menus, and use
  **CTRL-hold** to auto-align a building to a street when placing — that removes the blind
  tree-edge placement problem of `ui-and-placement.md` §7.
- The settlement that the population/production fixtures need is: Warehouse/Main Market Building ->
  streets -> a **food** chain (Fisherman's Hut on the coast is the simplest) and the
  building-material chain, all within one Service Area, with a **Food and Salt Stand**, then Houses
  on the streets — then pioneers grow to Settlers and the workshops (Weaver) unlock.
