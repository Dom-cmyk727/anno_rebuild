# Community FAQ and Reddit: mechanics evidence

**Status:** collected 2026-10-22 (orchestrator session, one research agent). It continues [`tutorial-videos.md`](tutorial-videos.md). New evidence goes into OQ-101 to OQ-108 in [`../open-questions.md`](../open-questions.md).

**Order of authority:** the files and disassembly (P, D) come first. Next come the **owner's** first-hand statements. Community posts come last: they are useful leads, and some may describe versions older than GOG 2.0.0.5.

## 1. Sources

- **Reddit r/anno.** Reddit refuses automated fetches, so the threads were read through the Arctic Shift archive API, searching titles that contain "1503" in half-year windows from 2011 to 2026. About 60 % of the windows answered, giving about 60 threads, of which 34 were read in full. Most are nostalgia, technical help or partner searches; few give numbers. Threads are cited as `r/<id>` = https://www.reddit.com/r/anno/comments/<id>/.
- **The capsu.org "Anno 1503 FAQ" by Tim Howgego** (linked from `r/hr5qad`), mirrored at https://github.com/WillemRB/anno-1602-1503-guide (pages `1503/<page>.html`, live at https://anno-capsu.netlify.app/).
  - `original-issues.md` already cites it as TH-FAQ, but used it only for bugs; this note takes its gameplay sections. Cited as `FAQ/<page>`.
  - It compiles forum posts from 2002–2004, before the A.D. release, so a claim may describe an older patch level.
- **The owner**, 2026-10-22, on roads (§3).

## 2. Checked against the files

| Finding | Source | Check | Verdict |
|---|---|---|---|
| Research points build up over time to a **cap** set by population and level: 20 / 25 / 50 / 70 / 90 at 50 settlers / 170 settlers / 200 citizens / 600 citizens / 750 merchants. A library adds 10. The number of schools does not matter, and switching research loses the points. | FAQ/research | `BGruppen.dat` `[BAUSTUFEN]`: `Wissen: 2B 20, 2E 25, 3B 50, 3D 70, 4C 90`, and those stages are 50 / 170 settlers, 200 / 600 citizens and 750 merchants | **agrees exactly (P): the `Wissen` lines are the knowledge cap per stage** (OQ-107) |
| Stall prices by island: salt 38 on northern islands and 43 elsewhere | FAQ/colony_development | `SALZ`: `Preis` 30, +5 on `STEPPE`/`TUNDRA`/`PRAERIE`/`DSCHUNGEL` (not `NORD`), `PreisLevel` 0/5/8. So 30 + 8 = 38 on `NORD`, and 30 + 5 + 8 = 43 elsewhere. | **agrees (P) at tax index 2**, like the tutorial's spice price of 80 (`tutorial-videos.md` §3.3). Two independent sources point to tax level 2 as the default, or to the price display using index 2. Check world property 10 at scenario start (T15, `population.md` T1/T5). |
| Fire does not cross streets or empty tiles; roads act as firebreaks before the fire brigade exists | FAQ/roads_storage | A flame's spread scans range 1 out from a side of the building, for buildings of every group except 6 (roads, squares) (`fire-and-ships.md` §2.3) | **agrees (D)** |
| Storage: the first warehouse gives 50 t, then +20 t per market building for the next five, then +10 t; cap 190 t (900 t in the add-on). Posters quote different numbers. | FAQ/roads_storage | T15 read city capacities of 70 t and 50 t (PR #16) | consistent so far; the rule itself is unchecked |
| F7 is only double speed on the English GOG build | `r/404a75` (one poster, 2016) | T15 measured F7 = 3,986 game-ms per real second on GOG 2.0.0.5 (PR #16) | **contradicted** by measurement |

## 3. Roads: the owner's rule (2026-10-22) and the community

- **Owner:**
  - houses need roads connected to markets;
  - services must reach a house by road (the fire brigade, for example);
  - road placement matters: a road connects only at a building's **specific entrance tile**, not at any edge tile.
- **Agrees:**
  - the manual: entrances shown as green arrows, and CTRL aligns a building to a street (`docs/re/game-mechanics.md` §3, on PR #16);
  - the FAQ: a producer must connect at the exact entrance spot; farms inside a processor's area need no road, because the processor's workers walk across the fields; only market and warehouse cartmen need roads to carry goods (FAQ/roads_storage).
- **Contradicts the owner:** FAQ posters from 2002–2004 write that houses need no roads and that the fire brigade reaches burning houses without roads (FAQ/roads_storage). One Reddit thread says residents and firefighters walk *through* buildings that have several entrances (`r/1i6v43h`).
- **Working rule for the rebuild: the owner's.** The conflict is settled by measurement, not by vote: T15 runs the same houses with and without a road to a market, and fires with and without road access (PR #16 review). The FAQ quotes may describe an older patch.
- Also from the FAQ: an entrance must stay free (a street or an empty tile in front of it); if a building has several entrances, one free entrance is enough. Cobbled roads are not faster (agrees with ISS-093), but carts can cut diagonally across squares. A market building has at most 2 carts: 1 at pioneer level, 2 from the settler upgrade (to check against `PROD3` figure-group maxima).

## 4. Other findings (unchecked unless stated)

**Buildings and upgrades**
- **Chapel → church, school → university** (OQ-101):
  - as soon as one university or church stands **on the island**, every school or chapel there is upgraded, whether or not the big building covers it;
  - demolishing the big building loses the upgrade;
  - the cathedral counts as a church.
  - Source: FAQ/colony_buildings (two posters; text checked); `r/1i6v43h` agrees.
- **Building-material lock** (OQ-104): it is per island, in the "?" panel of the market hall or warehouse (FAQ/colony_development). Upgrading houses use up tools quietly unless it is set (`r/12kxd0b`).
- **House look** (OQ-104): with the build tool, clicking on ground where nothing can be built cycles to the next design; upgrading houses choose their own (FAQ/colony_buildings). So the client picks the look, and in lockstep it must travel in the build command.
- **Palace** (FAQ/colony_buildings):
  - building it needs 1,000 aristocrats and at least 1,900 residents;
  - more parts come at 3,000 aristocrats, and the limit is lifted at 5,000 on one island;
  - the cathedral can be built once per game.
  - The stages agree with `5C`/`5E`/`5F` (P).

**Supply and residents** (OQ-102)
- **Stall coverage** (FAQ/colony_buildings, colony_planning):
  - the house's reach counts, not the stall's radius;
  - stalls sell from the island's store without any transfer;
  - at least one stall entrance must be reachable;
  - reach at the edge is irregular, which suggests walker reach rather than a drawn circle.
  - One Reddit commenter says chapel coverage counts the other way round (`r/1i6v43h`).
- **The tavern is the exception:** its keeper fetches alcohol within the tavern's radius, from the market hall or straight from a farm or brewery, through one fixed door (FAQ/colony_buildings).
- **Why houses collapse** (`r/1i6v43h`, several agree): each house sends 1–2 shoppers. Distant or crowded stalls let food run out. Rule of thumb: about 25 merchant houses per tavern, one stall of each type and 2 market halls.
- **Aristocrats** run their errands in a chain (market, home, church, home…). A long round lowers their mood (FAQ/colony_development; agrees with ISS-089).

**Research** (OQ-107)
- Costs (FAQ/research_trees), still to check against the data:
  - well 5 knowledge / 50 gold, then fire brigade 5 / 50;
  - weaving mill 20 / 300;
  - medicus 50 / 200, which also unlocks herbs;
  - fast healing 60 / 500;
  - library 60 / 2,000;
  - large ore mine 40 / 500;
  - deep well 20 / 200.
- Aristocrats do not attend school. Caps 70 and 90 need a university.

**Events** (OQ-105)
- **Medicus** (FAQ/colony_buildings, colony_development): it cures only houses in its area, and some residents always die first; "fast healing" shortens that. The in-game help says public baths lower the plague chance (unchecked).
- A burning house loses residents while it burns (FAQ).
- **Bankruptcy** comes from staying below −1,000 gold for some time (FAQ, quoting the producer). Compare with the player's bankruptcy deadline in `docs/sim/state.md`.
- **Wells** (FAQ/colony_buildings, industry_planning):
  - they raise fertility that is below 100 % and limit drought damage;
  - a well must cover part of the farm building, and a second well adds nothing;
  - drought spares livestock.

**Wildlife and fields** (OQ-106)
- Game comes from forests and grazes on grass anywhere, which supports "deer eat the sheep's pasture" (FAQ/industry_planning).
- The fur trapper takes white-furred animals, plus leopards and tigers in the jungle at about half the yield (FAQ/colony_buildings).
- A forester uses only half of its 44 fields (FAQ). About 80–85 % of a farm's own fields keep it at 100 % (`r/q7n9iu`, one poster, from memory of 1602; low confidence).

**Trade, AI, diplomacy** (OQ-108; FAQ/trade_diplomacy, trade_strategy)
- **Venetians:**
  - they always sell tools, often sell bricks and wood, and buy food;
  - otherwise they act as middlemen, passing on other players' buy and sell orders;
  - they sail a fixed circuit past every player;
  - you need a coastal warehouse to trade with them;
  - there is no Venetian island in the Citizen endless game or the campaign.
- **Trade treaty:** it needs peace; the AI trades only from its main island.
- **Relations:** every trade visit raises the relations bar, and an alliance needs a long bar. Armed ships near AI islands count as aggression.
- **AI demand:** when it founds an island, the AI asks for wood, bricks and tools.
- **Trade slider:** it sets the stock to keep.
- **Trade routes:** no "wait until full"; ships know 8 directions; a port serves about 3 ships at once.

**Controls** (OQ-104; FAQ/interface): no orders while paused; F8 half speed; the add-on adds V (plant the fields around the active farm) and "−" (statistics); H cycles your colonies, J jumps to the last event.

## 5. Bugs and quirks (candidates for "faithful vs. fix")

- The tavern's alcohol and goods on carts are not saved. After a reload the carts start again, and the empty tavern can cause riots (FAQ). This is likely a fix.
- Wells "forget" their effect after a reload (one poster).
- An add-on hunting lodge placed on top of an animal never works (FAQ).
- Scout trade routes are broken up to version 1.04.02 (FAQ).
- The History Edition changed no gameplay, according to a Ubisoft community manager (`r/ha49ku`).

## 6. Multiplayer (community)

- The History Edition's multiplayer over Ubisoft Connect works but often crashes after a few hours (`r/uwqiz3`, `r/1imdk3y`).
- **Stock multiplayer endless game** (`r/jtxvv7`):
  - islands are small and not configurable; one poster guesses island size = world size divided by the number of players (relevant to OQ-100, a guess);
  - the free trader never came, and whether a save loaded depended on who hosted.
- No computer players in multiplayer without custom maps (`r/11nrzv1`, `r/1o7fmgy`; agrees with ISS-038).
- No desync cause beyond the known fire-spread one (ISS-033).
