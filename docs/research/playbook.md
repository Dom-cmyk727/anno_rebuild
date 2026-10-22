# Playing ANNO 1503: a player's playbook (for driving the original)

**Status:** 2026-10-22, orchestrator. A synthesis, in our own words, of how the game is played. It is written for the tool that drives the original (`tools/orig/control.py`, see [`../testing/play-original.md`](../testing/play-original.md)) and for anyone setting up fixtures by playing.

**Sources, cited inline:**
- **[M n]** the German manual ("Handbuch", the owner's `Anno_1503_Manual.pdf`), printed page *n*. The English manual in `game/app` covers the same ground (summary on PR #16, `docs/re/game-mechanics.md`).
- **[FAQ/page]** the community FAQ (Tim Howgego), mirror https://github.com/WillemRB/anno-1602-1503-guide, `1503/<page>.html`.
- **[V n]** the tutorial videos ([`tutorial-videos.md`](tutorial-videos.md)).
- **[R]** Reddit threads ([`community-faq.md`](community-faq.md)).
- **[Owner]** the owner, 2026-10-22.
- **[P]** values read from `Anlagen.dat` / `BGruppen.dat`. They are the authority wherever a guide gives another number.

Mechanics that our RE notes already pin down are linked, not repeated.

## 1. Controls the tool uses

- **Keys** [M 70; English layout in `game-mechanics.md` §1]:
  - **Esc:** leave the current mouse mode (build, demolish), or the selection.
  - **F2 / F3 / F4:** zoom levels 3 (far) / 2 / 1 (near).
  - **F5 / F6 / F7 / F8:** speed 100 / 200 / 400 / 50 %. 400 % is single-player only; the speeds are measured, PR #16.
  - **F9 / F10:** quick load / quick save.
  - **X / Y:** rotate the map. **`.` / `,`:** rotate the object being placed.
  - **J:** jump to the active object. **Space:** jump to the last event. **H:** cycle between your own cities. **P:** pause. **M / N:** find soldiers / the next scout.
  - **0–9:** select a group; **Ctrl+0–9:** create a group (groups exist only for units and ships, [FAQ/interface]).
  - **Ctrl+D / Alt+D:** open / close all city gates.
  - **B:** construction menu. **D:** player menu. **S:** ship list. **C:** city list. **O:** options.
  - **Key differences between the manuals:** the German edition throws cargo overboard with Ctrl+H and opens the rating with Shift+H; the English one uses Ctrl+W and Q. The game copy is English, so verify in-game.
  - The add-on adds **V** (plant the fields around the active farm) and **−** (statistics) [FAQ/interface].
- **No orders while paused.** To have time to think, use F8 (50 %) [FAQ/interface].
- **Mouse** [M 11-12]:
  - left click: select, or build;
  - right click: send the selected unit or ship;
  - drag with the left button: select a group;
  - push against the screen edge or use the arrow keys: scroll.
  - The minimap's arrows rotate, and its +/− zoom.
- **Right-hand game menu** [M 12-13]:
  - the construction menu with **8 categories**, top to bottom: public buildings; squares and roads; farms and plantations; workshops; coastal buildings; mines; military; ornamental;
  - the pickaxe (demolish mode);
  - the player menu: player info / city info, diplomacy status, city list, ship list;
  - info mode ("?": click any object to see its panel);
  - options (save with a name, load, rating screen, music, which speech messages play).
- **Info bar at the bottom** [M 15]:
  - money;
  - tools, wood, bricks and marble of the selected city: white = in stock, blue = needed for the building in hand, blinking red = missing;
  - the selected city's population;
  - event icons on the left;
  - over a discovered island, the crops that grow there and their yield bar.
- **North** is the screen's top edge, not the map's; the compass in the middle of the map shows it [FAQ/interface].

## 2. Discovering and choosing an island

- **What you see by sailing:** the info bar shows the island's crops once the ship comes into sight range [M 17]. Islands come in six climates: polar, tundra, north, prairie, steppe, jungle [FAQ/climate_resources].
  - A **north** island with salt, ore and hops can carry a city to citizens on its own: hops give alcohol at the settler level, and salt is mined [FAQ/colony_planning].
  - **Southern** islands start slower (alcohol from small farms for longer), but later produce cotton, sugar cane (alcohol) and silk efficiently.
- **Mountains:** they hold the deposits (stone, marble, salt, iron ore, gold, gems; which ones depends on the climate) and cannot be crossed [M 19].
  - A **scout** finds the deposits: unload it from the ship (select the ship, click the scout's slot); walk it near a mountain and icons appear over the peak [M 17-18].
  - Deposits inside a market hall's area are also found on their own [FAQ/climate_resources].
- **Fertility:** dense green land is fertile [M 18-19]. When placing a farm, the **fertility bar** shows how well that crop grows there. On an island that offers a crop at only 50 %, the bar never exceeds 50 % [M 27]. One missing segment still yields well [V 14 30:22].
- **Settled islands:** another player's or a native settlement appears only when your ship or scout sees it. A large treeless area is a hint [M 19; V 3 07:21].
- **Bays:** good for a Kontor or shipyard [M 19], but narrow bays hinder trade ships. Several Kontore around the main island shorten routes [FAQ/colony_planning].
- **Treasure chests** are found by a scout passing close by, or by building on the spot [M 19].

## 3. Founding a settlement and placing buildings

- **Order** [M 20-23]:
  1. Kontor on the coast, or a market hall inland (built by the scout);
  2. unload the ship;
  3. roads;
  4. production chains;
  5. houses and public buildings;
  6. market stands.
- **Kontor:** with the ship near the coast, the "build Kontor" icon appears in the right menu; place it on the coast [M 20].
- **Unloading** [M 21, 39]: select the ship, open its "load/unload" panel, pick the good, choose the amount per click (1 / 5 / 10 / 50 t), then the arrow. The ship must be near the Kontor.
- **Inland market hall** [M 20-21]:
  1. load wood and tools from the ship onto the scout;
  2. walk the scout to the spot;
  3. in its panel, choose "found a city" and place the market hall;
  4. unload the scout into the market hall.
- **Claiming land cheaply** [FAQ/colony_planning]: a scout-built market hall on an outpost island costs less than a Kontor, reaches farther inland and costs 10 a minute.
- **The city's area** [M 26]: building is allowed only inside the union of the Kontore's and market halls' areas, the light area. Extend it with new market halls placed at the **edge** of that area.
  - Market halls in the middle of a chain can be demolished once their neighbours' areas overlap, to save upkeep. Each one also brings market carts, though [FAQ/colony_planning].
  - Everything stored in one market hall is available in all of them on that island [FAQ/colony_planning]: the store is per city (`production.md` §5.3).
- **Placing a building** [M 25-28]: pick it in the construction menu; a ghost follows the cursor; left click places it and stays in place mode; Esc or right click leaves it.
  - **Before placing, check:** the area (shown as a yellow outline), the build cost (blue in the info bar, red if missing), the running cost and the fertility bar.
  - Nothing can be built or demolished with enemy units near [M 27].
- **Entrances and roads** [M 23, 28; Owner]: every building has **entrance arrows** (green when selected). A building is connected only if a road meets it at an **entrance tile**, not at just any edge tile.
  - Turn the building with `.` / `,` before placing, or **hold Ctrl while placing** to snap its entrance to an existing road.
  - An entrance must stay free: a road or an empty tile in front of it. If another building blocks it, that entrance is lost. A building with several entrances needs only one free [FAQ/roads_storage].
- **Owner's rule (2026-10-22)** — houses need a road to a market, and services (the fire brigade, for example) reach a house by road. Some community posters claim otherwise; their rule is unconfirmed (OQ-103) and we play by the owner's.
- **Production links** [M 22, 26; FAQ/roads_storage, industry_planning]:
  - The raw producer must stand inside the processor's area, and the processor inside a market hall's or Kontor's area.
  - The processor's own worker walks over the fields to fetch the raw good, so a farm inside its processor's area needs no road. The forester is an exception: it delivers to the store, so it needs one [V 1 00:58].
  - The **market carts** of market halls and Kontore use roads, and carry finished goods (and raw goods meant for storage) into the store.
  - Buildings touching a market hall at an entrance need no road. Some building types (distillery, lamp-oil maker, hunting lodge, spice plantation, partly the forester) let carts pass through to a neighbour [FAQ/industry_planning].
- **Fields** [M 30; V 1 01:10; FAQ/building_industry_data]: farms need their fields planted inside their own area. Use the panel's plant button, or V in the add-on. Every field square costs 5 to plant.
  - Clicking the tree tool on an existing tree cycles the tree type [FAQ/climate_resources].
- **Mines** [M 31]: pick the mine, move it near a mountain with that deposit, and it snaps along the ridge to the best spot. A quarry needs its stonemason near, but not directly in front of it [FAQ/climate_resources].
- **Demolish** [M 28]: pickaxe, then click; dragging a frame clears only plants and roads. It cannot be undone and gives no materials back.

## 4. Where to put things: layout

- **Housing modules** [FAQ/colony_planning; V 2, V 4]: a centre with the market hall, the market stands, the tavern, the chapel and the school (later doctor, bathhouse, fire brigade), ringed by houses.
  - **Size:** one stand of each kind serves about **50 houses**; 70 strain at higher levels.
  - **Big buildings:** build one big church and one university somewhere on the island. The centre's chapels and schools turn into churches and universities by themselves (OQ-101; `community-faq.md`).
  - **Access:** leave access space around the stands. A queue at a stand means it is overloaded; add another [M 23].
- **Reach comes from the house's side** [P]:
  - houses (types 501–584) have **action radius 24**: their residents walk that far;
  - market stands 4, chapel 20, tavern 20, church 24, school 24, bathhouse 24, theatre 24, doctor 24, fire brigade 24, pavilion 4.
  
  The community reports reach as irregular at the edge (OQ-102). A double-click on a public building shows the area of every building of that kind [M 23].
- **Production** goes away from the housing, so the best land near the centre is used for houses [FAQ/colony_planning].
  - Farms around their processor, with a market hall next to the processors.
  - "Production streets": a row of processors along a road, with fields on both sides [FAQ/industry_planning].
- **Overlaps that work** [FAQ/industry_planning]: hunters share land with foresters; two foresters can stand side by side (each uses half of its 44 fields); sheep and cattle farms can overlap; one well can serve two farms.
- **Sheep pastures** do better without forest, because deer graze the grass [V 10 01:08; FAQ].
- **Don't pack too tight:** upgraded houses hold up to five times the people, and crowded streets slow every errand [M 25; FAQ/colony_planning].

## 5. Economy: chains, ratios, consumption

**Consumption.** `Menge` in `BGruppen.dat` is **tons per 100 residents per game minute** [P; `population.md` §9]. Community figures confirm the reading:
- a bakery's 6 t/min feeds 600–667 residents;
- a salt works' 6 t/min serves 3,000 pioneers (`Menge` 0.2);
- two goldsmiths' 4 t/min serve 1,334 aristocrats (`Menge` 0.3).

Per 100 residents per minute:

| Level | Food | Leather | Cloth | Salt | Tobacco | Spices | Lamp oil | Silk | Clothing | Jewellery | Wine |
|---|---|---|---|---|---|---|---|---|---|---|---|
| Pioneers | 1.0 | 0.4 | 0.4* | 0.2* | | | | | | | |
| Settlers | 0.9 | 0.3 | 0.5 | 0.2 | 0.4* | 0.4* | | | | | |
| Citizens | 0.9 | | 0.5 | 0.25 | 0.4 | 0.4 | 0.2* | 0.4* | | | |
| Merchants | 0.9 | | 0.3 | 0.25 | 0.4 | 0.4 | 0.2 | 0.6 | | | |
| Aristocrats | 0.9 | | | | | | | | 0.7 | 0.3* | 0.5* |

\* Needed only for the next level up (the "Aufstieg" needs), or optional (aristocrats' jewellery and wine).
- **Averaging:** the goods needs are averaged, so one missing good is tolerated. This is the "salt can replace spices or silk" observation [FAQ/building_industry_data; `population.md` §4.1].
- **Food and the public buildings** are hard requirements.
- **Plan for the jump:** a level change roughly doubles a house's people, so stock up before letting houses upgrade [FAQ/colony_management; M 24].

**Output at full supply** [P, `PROD3` T0; 60 / T0 = t/min]:
- Processors run at these rates when fed. Field-, animal- and deposit-bound producers can fall short of the figures below.
- **Food:** bakery 6 (≈ 600 residents); hunting lodge ~2 in practice (game-bound; "5 hunters feed 1,000") [FAQ]; fisher ~0.8 in practice; small farm ~0.9 food or ~3 alcohol.
- **Cloth:** weaving hut 1.4 (≈ 350 pioneers); weaving mill (researched) 3, from 28 % less wool per ton (≈ 750 pioneers).
- **Leather:** tannery 1.7.
- **Salt:** salt works 6 (3,000 pioneers or settlers, 2,400 citizens or merchants), fed by one salt mine (6).
- **Tobacco:** tobacco works 2 (≈ 500 settlers).
- **Spices:** spice plantation 1.5 (≈ 375).
- **Later goods:** lamp-oil maker 3 (≈ 1,500); dye works (silk) 3 (≈ 500 merchants); tailor 3 (≈ 430 aristocrats); goldsmith 2 (≈ 670 aristocrats); winery 1.5 (≈ 300 aristocrats).
- **Building materials:** forester 3.3; stonemason 10 bricks; smith 2.1 tools; smelter 4 iron.

**Ratios (combines)** [P, checked against the community rules]:

| Combine | Ratio | Source |
|---|---|---|
| Hop farm : brewery | 2 : 1 | [V 3; FAQ] |
| Sugar cane : distillery | 2 : 1 | [FAQ] |
| Sheep farm : weaving hut | 2 : 1 | |
| Sheep farm : weaving mill | 3 : 1 | |
| Cotton : weaving mill | 2 : 1 | [FAQ] |
| Grain farm : mill | 2 : 1 | |
| Mill : bakery | 2 : 1 | |
| Cattle farm : butcher | 2 : 1 | |
| Tobacco plantation : tobacco works | 2 : 1 | |
| Whaler : lamp-oil maker | 1 : 1.4 | 2 : 3 [V 12] |
| Ore smelter : smith | 1 : 2 | |
| Ore mine : smelter | 1 : 1 | small mine 5 t/min against 4.9 needed |
| Salt mine : salt works | 1 : 1 | |
| Quarry : stonemason | 1 : 2–3 | the masons take turns [FAQ; V 10] |

- **Food options:** hunters first, then grain chains, which scale best in cost and land [FAQ/industry_planning].
- **Alcohol:** don't keep small-farm alcohol longer than necessary [V 3; FAQ].

**Costs and upkeep** [P; M 27-29] (every building's numbers are in §14; the manual gives none):
- Every working building has a running cost when active and a lower one when stopped (Zzzz). Stop whole chains that are not needed.
- The panel shows the input and output stock in tons and the use in %.
- **"?" with a full barrel** = overloaded: carts can't keep up, or the road or entrance is wrong. **"raw material missing"** = under-supplied.
- **Small farm:** one line can be switched off [M 29]. The manual says the remaining line then produces more (its example: 10 t alcohol + 5 t food, or 15 t food alone). That contradicts our RE: the lines are independent in `production.md` §4.5, and `PROD3` gives about 3 t alcohol and 0.9 t food a minute. **To verify by running.**
- **Tools:** buy them from the Venetians early (a buy order at the Kontor), but build your own chain soon: ore mine → smelter (+ wood) → smith (+ wood) [V 8-11; FAQ/climate_resources].
- **Money:** the full balance comes from merchants; selling at your own stands pays best [FAQ/trade_strategy; V].
- **Building materials:** 50 tools / 50 wood are typical start stocks (read on PR #16).
- **Bankruptcy:** money at or below **−3,000** gold starts a 300 s deadline (150,000 ticks at normal speed). A player still at or below −3,000 when it ends is bankrupt (D `0x1001b900`, `docs/re/players.md`). The FAQ's "−1,000" is wrong for this build.

## 6. Settlement development (levels and unlocks)

- **Levels:** pioneers → settlers → citizens → merchants. Houses upgrade by themselves when the needs are met and the next house's materials are in store [M 24-25; `population.md` §5]:
  - to settlers: 1 tool, 4 wood;
  - to citizens: 2 tools, 2 wood, 4 bricks;
  - to merchants: 5 tools, 5 wood, 4 bricks.
- **Aristocrat houses** are placed directly from 1,900 merchants: 12 tools, 10 wood, 20 bricks, 5 marble. They collapse if their needs fail [P].
- **Building-material lock:** in the Kontor's or market hall's panel; it stops all upgrades on that island and saves bricks and tools [M 25; V 3; FAQ]. Use it to time upgrades and to keep materials for building.
- **The house panel** (info mode) [M 23-24]: a face showing the mood; coloured need bars (current needs); grey fields (needs for the next level). The first number is the house's residents, the second the city's residents at that level.
- **Unlocks** come with the residents of a level or higher (`BGruppen` stages, `Anlagen.dat` `INFRAMIN`, [P]).
  - **Pioneers:**
    - 30: small fortress;
    - 80: tavern;
    - 125: chapel, salt.
  - **Settlers:**
    - 25: hemp, ropemaker, small shipyard;
    - 50: school, quarry and stonemason, market hall II, cattle;
    - 80: ore mine, smelter, smith, tobacco, spices;
    - 240: church;
    - 360: hops and brewery.
  - **Citizens:**
    - 120: cotton;
    - 200: grain farm, mill, bakery, sugar cane, market hall III;
    - 400: university, marble, silk and dyes, clothing stand;
    - 600: bathhouse, whaler and lamp oil;
    - 1,100: tailor, fur trapper, large smelter.
  - **Merchants:**
    - 750: gold, gems, goldsmith, winery;
    - 1,500: theatre, pavilion;
    - 1,900: aristocrat house.
  - **Aristocrats:**
    - 600: cathedral;
    - 1,000: palace.
  - **Researched buildings:** well, fire brigade, doctor (and herbs), weaving mill, large ore mine, library, court and the weapon buildings.
  - The weaving hut and tannery are available from the start [P].
  - In the menu, "?" means not yet unlocked, and a dark icon means unlocked soon (hover to see the requirement) [M 27].

## 7. Ships, trade routes, buying and selling

- **Ships** [M 38-40]: built at the small or large shipyard from gold, wood, cloth and ropes; cannons are loaded there.
  - From the videos: a small trade ship takes 5 t ropes, a whaler ship 7 t [V 5, V 11].
  - Find ships with the ship list (S), or as yellow dots on the minimap. Repair at the shipyard.
  - The ship panel also offers: throw cargo overboard, white flag (surrender, or show peaceful intent), sell the ship (price slider), sink it (saves upkeep).
- **Moving and unloading:** select the ship, right-click the water near the target. Loading and unloading happen near a Kontor, in steps of 1 / 5 / 10 / 50 t.
- **Automatic trade routes** [M 46-48; V 5 04:04]:
  1. select the ship → "automatic trade routes" → "set route point" → click a Kontor (your own or a trade partner's);
  2. each route point takes up to **4 actions**: the first two load, the last two unload; pick a good and an amount for each;
  3. start or stop the route with the **anchor** icon.
  
  Rules:
  - Whatever is loaded must be unloaded somewhere, or the hold fills.
  - A repeated failed load gives a message.
  - The route buoys can be dragged to steer around danger.
  - A scout can run a land route between Kontore and market halls on one island.
  
  Community practice [FAQ/colony_management, trade_strategy]:
  - one ship per supply island;
  - a second ship offset in time when supply must be more frequent;
  - a "supply ship" carrying up to 8 goods to a new colony;
  - routes to AI Kontore are unreliable, because AI preferences change.
- **Passive trade at your own Kontor** [M 43]:
  - **Buy:** "buy goods" → add a good; the upper slider is the stock level to buy up to, the lower slider the price you'll pay per ton.
  - **Sell:** "sell goods" → the upper slider is the stock to keep, the lower slider the price per ton.
  - **Price arrow:** green = a good price, yellow = fair, red = not acceptable (too high when selling, too low when buying).
  - **Who comes:** the Venetians, with no treaty needed, and trade partners' ships [M 43-44].
  - **The Venetians** [FAQ/trade_diplomacy, trade_strategy; R]: they always sell tools, often bricks and wood, and buy food. Otherwise they relay other players' orders, on a fixed circuit, and need a coastal Kontor.
  - **Prices seen in the videos:** tools bought at 51–52 (base price 60); wood sold at 5 (base 8); salt sold at 23 (base 30) [V 2, V 3, V 6, V 10].
- **Active trade** [M 41-42]: discover the partner; sign a trade treaty; click their Kontor to see what they buy and sell; sail there and load or unload. The cost or income is booked at once.
  - Cultures barter goods for goods, and need a Kontor on the coast.
  - Pirates trade only when you are unarmed and show the white flag.
- **Balance** [M 44-45]:
  - **city balance:** the Kontor or market hall panel → "city status" (trade, stand income, running costs, military);
  - **total balance:** player menu → player info.
  - Large purchases can make the balance negative for a while [V 4].

## 8. Diplomacy

- **Getting started** [M 32-36]: discover a city first; the player then appears in the diplomacy list (not the Venetians or pirates). Their bar shows the mood: empty = neutral, green = friendly, red = hostile.
- **Treaties** cycle by clicking in "treaty status". **A wrong click can break a treaty**, so the tool must read the state before and after.
  - **Trade treaty:** needed for buying and selling with players.
  - **Peace:** the default in an endless game. The dove declares war; colours: green = peace, red = war, blue = offered.
  - **Military alliance:** needs a trade treaty and trade both ways; more trade makes it likelier. It reveals the partner's cities.
- **Tribute:** demand or pay money (slider) or goods. Goods tribute is delivered by an unarmed ship under the white flag.
- **Pirates:** protection money, and hiring them against a player, both at their Kontor.
- **Messages:** delay payment, flatter (lowers aggression; it helped get treaties signed [FAQ]), threaten war, offer a truce, insult. Too many messages backfire.
- **Behaviour** [FAQ/trade_strategy]:
  - the AI's character is chosen at game setup;
  - armed ships near its islands count as aggression;
  - each trade visit raises the bar;
  - war in someone's waters can start by accident (chasing ships).
- **Cultures** [M 36-37]: Africans, Aztecs, Bedouins, Eskimos, Native Americans, Moors, Mongols, Polynesians, Venetians. Some are hostile to neighbours, so check before settling next to them.

## 9. Research

- **Knowledge** builds up from people attending school or university, up to a **cap** per level: `Wissen` 20 / 25 / 50 / 70 / 90 [P; FAQ/research].
  - A library adds 10, once.
  - Switching research loses the points gathered. Aristocrats do not attend school.
- **Fields** [M 48-50]: weapons, military enhancements, naval, civil (well, fire brigade, doctor, library, deep ore mine).
  - A school offers only part of each field; a university offers all, and schools inside its area get all too.
  - At the start each field opens one key technology.
- **Costs** [FAQ/research_trees; unchecked against the data]:
  - well 5 knowledge / 50 gold, then fire brigade 5 / 50;
  - weaving mill 20 / 300;
  - doctor 50 / 200;
  - large ore mine 40 / 500;
  - library 60 / 2,000.
  - Community advice: the well, the fire brigade and the weaving mill first; demolish the library once its research is done [FAQ/colony_management].

## 10. Events

- **Fire:**
  - random fires hit pioneer and settler houses only (`fire-and-ships.md` §2.2);
  - fire spreads to neighbours but not across roads or empty tiles;
  - demolishing a burning house stops it;
  - the fire brigade (researched) puts out fires in its reach [V 3, V 4; FAQ].
- **Plague** starts above about 1,500 inhabitants and becomes more frequent with size.
  - The doctor (researched, uses herbs) cures houses in its area; houses out of reach lose people and can collapse [V 13, V 14; FAQ]. Baths reportedly lower the chance.
- **Riots:** from citizens up, when errands take too long. Remedies: stands close to the houses, overlapping public buildings, soldiers or the fire brigade near market halls [FAQ/colony_management].
- **Drought** spares livestock; wells limit it; keep a road to crop farms so a stock builds up [FAQ/industry_planning].
- **Volcanoes:** they erupt, and only buildings close by can catch fire [FAQ].

## 11. Combat (later)

- **Basics** [M 50-60]:
  - select units and right-click the target; Ctrl attacks your own objects or trees; Alt + right-click attacks everything on the way;
  - life bars; units have different sight and reach; your territory is marked by invisible borders, and trespassing gives messages or war threats;
  - high ground gives ranged units more reach.
- **Units:**
  - melee: crew, cavalry, lancer, pikeman, medic, scout, swordsman, musketeer;
  - ranged: crossbowman, archer, marksman.
  
  They are trained in fortresses (small / medium / large) with gold and the weapon, once researched. Soldiers gain up to 3 experience points; dismissing a soldier saves upkeep.
- **War machines:** catapult, mortar, cannon, siege tower. They need crews and can be captured.
- **Defences:** walls (drag like roads; soldiers on them need an inner stair), gates, watchtower (2 ranged units), cannon tower.
- **At sea:** warships with cannons attack ships and the coast. Six formations; attack modes passive / normal / aggressive.

## 12. A tested opening (Citizen endless game, from the videos)

1. **Wood and food:**
   - 4 foresters in a row, with a road (plant forest if it is sparse);
   - a market hall;
   - 2–3 hunting lodges at the forest edge, with roads kept outside their area;
   - 1 tannery;
   - 4 sheep farms around 2 weaving huts (no roads needed);
   - unload the ship's wood [V 1].
2. **The first houses:**
   - two rows of houses per street, grouped into a block around a market hall;
   - a cloth/leather stand in the middle (exits on all sides) and a food/salt stand;
   - leave room for the tavern, chapel and fire brigade [V 2].
3. **Upgrade to settlers:** at 80 pioneers, alcohol from small farms (plant potato fields) and a tavern next to the market hall; a chapel; then settlers appear.
   - At this point put tools on a buy order at the Kontor, about 40 t at ~52 [V 2].
4. **Settlers:**
   - quarry and stonemason (bricks) → hop farms with a brewery (2 : 1), replacing the small farms;
   - a school → research the well, then the fire brigade;
   - a scout finds salt → salt mine and salt works;
   - colonise a spice or tobacco island with a Kontor and a trade route;
   - hemp and a ropery for ships;
   - a church [V 3–V 7].
5. **Citizens:**
   - grain chains; the weaving mill;
   - your own tools (ore mine, smelter, smith);
   - the doctor and herbs;
   - whalers and lamp oil from the north; marble;
   - bathhouse and university → merchants [V 8–V 13].

## 13. Known quirks that matter for scripted play

- **Lost on reload:** a tavern's alcohol and goods on carts are not saved; after a reload an empty tavern can cause riots [FAQ]. Prefer quick-saving at calm moments.
- **Hunters near animals:** an add-on hunting lodge placed on top of an animal never works [FAQ].
- **Missed upgrades:** a burning house cannot upgrade [V 14]. Houses at the fringe of a public building's reach behave unpredictably [FAQ].
- **Aristocrats' routes:** aristocrats walk errand chains that may be long. Duplicate public buildings can make the route worse [FAQ].
- **The scoreboard** wraps at 65,535 [FAQ].

## 14. Build and running costs [P]

The manual gives no numbers. It only says how to see them [M 27-28]:
- the build cost shows in blue in the info bar once a building is selected, and in red when something is missing;
- the running cost shows in info mode on a placed building;
- stopping a building (Zzzz) lowers its running cost;
- demolishing refunds nothing.

All numbers below come from `Anlagen.dat` (the id column):
- **Build cost** comes from `HEAD`: u16 `[8]` is the gold, and bytes `[3..6]` are bricks, wood, tools and marble (`docs/formats/anlagen.md`). The affordability check reads these fields (D `0x100275d0`, `docs/sim/state.md`).
  - The in-game help (`game/app/help`) gives the same values for 86 of 94 buildings (580 of 589 values).
  - Where they differ, the data is used (`manual-and-help.md` §1.1).
- **Running cost** comes from `PROD3` u16 `0x00` and `0x02`, in gold per game minute. The game takes 1/15 of a city's total every 4 s (`docs/re/population.md` §8; loader in `docs/re/production.md` §2.3).
  - The larger value (`0x02`) is charged while the building works, and the smaller one (`0x00`) while it is stopped.
  - **Confirmed:** 50 help pages give "operating costs while active / while shut down", and all 50 equal this pairing.
  - A single number below means the stopped value is 0; these buildings cannot be stopped.
- **Houses** cost materials only and have no running cost. Settler to merchant houses are reached by upgrade, which takes these materials (§6).
- **Not in `Anlagen.dat`:** the upkeep of ships and soldiers. It sits in the figure data and is still open (`docs/sim/state.md` O-49).
- **Largest running costs:** theatre 200, university 150, library and palace 100 each, bathhouse 90, and the large ore smelter 75 a minute while working.

"–" = 0. Running cost = working / stopped, in gold per minute.

**Houses and infrastructure**

| Building | Id | Gold | Tools | Wood | Bricks | Marble | Running |
|---|---|---|---|---|---|---|---|
| Pioneer house | 501 | – | – | 3 | – | – | – |
| Settler house | 520 | – | 1 | 4 | – | – | – |
| Citizen house | 540 | – | 2 | 2 | 4 | – | – |
| Merchant house | 560 | – | 5 | 5 | 4 | – | – |
| Aristocrat house | 580 | – | 12 | 10 | 20 | 5 | – |
| Dirt road (per tile) | 2520 | 5 | – | – | – | – | – |
| Paved road (per tile) | 2500 | 10 | – | – | 1 | – | – |
| Field tile (one plant) | 700–733 | 5–6 | – | – | – | – | – |
| Wooden pier | 2529 | 5 | – | 1 | – | – | – |
| Stone pier | 2509 | 10 | – | – | 1 | – | – |
| Wooden bridge (rivers only) | 2536 | 15 | – | 1 | – | – | – |
| Stone bridge (research) | 2516 | 25 | – | – | 1 | – | – |
| Bridge | 2518 | 500 | – | – | 3 | – | – |

**Trade and supply**

| Building | Id | Gold | Tools | Wood | Bricks | Marble | Running |
|---|---|---|---|---|---|---|---|
| Kontor, level 1 | 1302 | 350 | 5 | 12 | – | – | 15 |
| Kontor, level 2 | 1304 | 500 | 5 | 12 | – | – | 25 |
| Kontor, level 3 | 1306 | 800 | 10 | 8 | 12 | – | 35 |
| Market hall, level 1 | 1301 | 250 | 3 | 7 | – | – | 10 |
| Market hall, level 2 | 1303 | 500 | 5 | 12 | – | – | 15 |
| Market hall, level 3 | 1305 | 800 | 10 | 8 | 12 | – | 30 |
| Stand: food and salt | 1310 | 50 | 1 | 1 | – | – | 5 |
| Stand: cloth and leather | 1314 | 50 | 1 | 1 | – | – | 5 |
| Stand: tobacco and spices | 1315 | 50 | 1 | 1 | – | – | 5 |
| Stand: lamp oil | 1317 | 50 | 1 | 1 | – | – | 5 |
| Stand: silk cloth and clothes | 1318 | 50 | 1 | 1 | – | – | 5 |
| Stand: jewellery | 1313 | 80 | 1 | – | 1 | – | 5 |
| Stand: wine | 1316 | 80 | 1 | 1 | – | – | 5 |

**Raw materials**

| Building | Id | Gold | Tools | Wood | Bricks | Marble | Running |
|---|---|---|---|---|---|---|---|
| Forester | 801 | 150 | 2 | – | – | – | 12 / 4 |
| Hunting lodge | 805 | 140 | 1 | 3 | – | – | 20 / 8 |
| Fisher | 810 | 180 | 2 | 6 | – | – | 20 / 12 |
| Sheep farm | 807 | 220 | 2 | 4 | – | – | 10 / 5 |
| Small farm (food and alcohol) | 809 | 250 | 2 | 4 | – | – | 20 / 8 |
| Hemp plantation | 808 | 200 | 2 | 5 | – | – | 18 / 8 |
| Grain farm | 811 | 200 | 2 | 3 | 3 | – | 10 / 5 |
| Cattle farm | 820 | 300 | 5 | 8 | 4 | – | 15 / 10 |
| Hop farm | 815 | 250 | 5 | 3 | 4 | – | 18 / 10 |
| Sugar cane plantation | 814 | 310 | 4 | 4 | 4 | – | 18 / 12 |
| Tobacco plantation | 813 | 350 | 4 | 5 | 8 | – | 30 / 15 |
| Spice plantation | 812 | 390 | 4 | 5 | 8 | – | 40 / 20 |
| Cotton plantation | 819 | 380 | 5 | 2 | 4 | – | 20 / 10 |
| Silk plantation | 817 | 300 | 5 | 5 | 6 | – | 35 / 17 |
| Dye plantation | 818 | 200 | 3 | 2 | 5 | – | 40 / 20 |
| Winery | 822 | 400 | 5 | 5 | 8 | – | 45 / 20 |
| Herb farm (research) | 823 | 200 | 3 | 5 | 5 | – | 15 / 5 |
| Fur trapper | 824 | 100 | 5 | 6 | – | – | 20 / 5 |
| Whaler | 821 | 500 | 5 | 5 | 5 | – | 20 / 10 |
| Quarry (stone) | 1201 | 300 | 3 | 3 | – | – | – |
| Marble quarry | 1218 | 400 | 3 | 3 | – | – | – |
| Salt mine | 1203 | 700 | 5 | 15 | – | – | 25 / 10 |
| Ore mine | 1205 | 1,200 | 5 | 15 | – | – | 40 / 15 |
| Large ore mine (research) | 1216 | 1,700 | 8 | 20 | 10 | – | 65 / 30 |
| Gold mine | 1206 | 1,500 | 15 | 12 | 4 | – | 50 / 20 |
| Gem mine | 1214 | 1,500 | 8 | 4 | 10 | – | 80 / 30 |

**Workshops**

| Building | Id | Gold | Tools | Wood | Bricks | Marble | Running |
|---|---|---|---|---|---|---|---|
| Stonemason (bricks) | 804 | 250 | 3 | 3 | – | – | 18 / 7 |
| Marble mason | 803 | 300 | 5 | 4 | 10 | – | 18 / 7 |
| Weaving hut | 1001 | 300 | 3 | 4 | – | – | 15 / 10 |
| Weaving mill (research) | 1005 | 500 | 4 | 2 | 8 | – | 30 / 10 |
| Tannery | 1023 | 300 | 2 | 5 | – | – | 9 / 5 |
| Mill | 1002 | 300 | 4 | 3 | 3 | – | 16 / 9 |
| Bakery | 1004 | 300 | 5 | 3 | 5 | – | 15 / 10 |
| Butcher | 1018 | 350 | 4 | 5 | 4 | – | 22 / 12 |
| Brewery (hops) | 1014 | 300 | 3 | 4 | 4 | – | 20 / 10 |
| Distillery (sugar) | 1013 | 300 | 5 | 6 | 1 | – | 20 / 5 |
| Tobacco works | 1008 | 300 | 4 | 2 | 6 | – | 16 / 8 |
| Salt works | 1022 | 400 | 5 | 5 | – | – | 30 / 12 |
| Ropery | 1030 | 400 | 3 | 5 | – | – | 16 / 6 |
| Charcoal burner | 1035 | 100 | 2 | 2 | – | – | 12 / 5 |
| Small ore smelter | 1017 | 800 | 10 | 4 | 10 | – | 40 / 20 |
| Large ore smelter (charcoal) | 1034 | 1,000 | 10 | 10 | 20 | – | 75 / 32 |
| Smith (tools) | 1019 | 500 | 3 | 4 | 8 | – | 25 / 15 |
| Tailor | 1027 | 500 | 5 | 5 | 5 | – | 30 / 15 |
| Dyer (silk and dyes → silk cloth) | 1009 | 500 | 7 | 2 | 8 | – | 40 / 25 |
| Lamp-oil maker | 1028 | 500 | 5 | 5 | 5 | – | 20 / 10 |
| Goldsmith | 1039 | 300 | 6 | 7 | 11 | – | 40 / 20 |
| Armourer | 1029 | 300 | 5 | 8 | 2 | – | 30 / 15 |
| Bow maker (research) | 1006 | 500 | 6 | 4 | 4 | – | 30 / 15 |
| Large weapon smith: axes, lances (research) | 1036 | 300 | 5 | 5 | 3 | – | 35 / 17 |
| Small weapon smith: swords (research) | 1037 | 200 | 4 | 4 | 2 | – | 40 / 15 |
| Gunsmith (research) | 1012 | 500 | 6 | 3 | 8 | – | 40 / 15 |
| War-machine builder (research) | 1010 | 500 | 8 | 6 | 2 | – | – |
| Cannon foundry (research) | 1038 | 800 | 10 | 10 | 10 | – | – |
| Small shipyard | 1020 | 1,200 | 15 | 18 | 12 | – | – |
| Large shipyard | 1021 | 2,000 | 20 | 20 | 18 | – | – |

**Public buildings**

| Building | Id | Gold | Tools | Wood | Bricks | Marble | Running |
|---|---|---|---|---|---|---|---|
| Chapel | 1320 | 700 | 6 | 10 | – | – | 15 |
| Church | 1325 | 1,600 | 9 | 15 | 20 | – | 50 |
| Cathedral | 1330 | 12,000 | 40 | 30 | 60 | 30 | 80 |
| Tavern | 1335 | 500 | 5 | 9 | – | – | 20 |
| Large tavern | 1336 | 900 | 8 | 10 | 8 | – | 50 |
| School | 1346 | 400 | 5 | 8 | 6 | – | 10 / 10 |
| University | 1379 | 2,500 | 25 | 30 | 32 | – | 150 |
| Doctor (research) | 1337 | 500 | 4 | 8 | 5 | – | 20 / 10 |
| Fire brigade (research) | 1350 | 200 | 4 | 6 | – | – | 15 / 8 |
| Court (research) | 1339 | 600 | 4 | 7 | 6 | – | 20 |
| Bathhouse | 1377 | 1,600 | 20 | 15 | 25 | 10 | 90 |
| Theatre | 1376 | 2,500 | 35 | 15 | 30 | 20 | 200 |
| Library (research) | 1378 | 2,000 | 20 | 12 | 24 | 12 | 100 |
| Pavilion (park) | 1386 | 600 | 10 | 5 | 6 | 3 | 40 |
| Gallows | 1326 | 500 | 5 | 9 | – | – | – |
| Well (research) | 1341 | 100 | 4 | 2 | 3 | – | – |
| Palace, main building | 1380 | 15,000 | 40 | 15 | 50 | 20 | 100 |
| Palace parts | 1381–1385, 1392 | 3,000–10,000 | 30–40 | 10–15 | 30–50 | 15–20 | 5–50 |
| Monuments | 1388–1391 | 1,500–3,000 | 5–15 | 0–5 | 15–30 | 10–15 | 2–5 |

**Military buildings**

| Building | Id | Gold | Tools | Wood | Bricks | Marble | Running |
|---|---|---|---|---|---|---|---|
| Small fortress | 2220 | 600 | 10 | 20 | – | – | – |
| Medium fortress | 2221 | 1,000 | 10 | 15 | 20 | – | – |
| Large fortress (castle) | 2223 | 2,500 | 20 | 15 | 30 | – | – |
| Watchtower (ranged soldiers only) | 2246 | 100 | 2 | 8 | 5 | – | – |
| Cannon tower (up to 2 cannons with crews) | 2230 | 200 | 3 | 8 | 8 | – | – |
| Defensive tower (fires over 180°, no cannons) | 2229 | 650 | 10 | 12 | 16 | – | – |
| Wall (per tile) | 2240 | 50 | – | – | 1 | – | – |
| Gate | 2234 | 150 | – | 1 | 6 | – | – |
| Large gate | 2233 | 200 | – | 2 | 10 | – | – |
