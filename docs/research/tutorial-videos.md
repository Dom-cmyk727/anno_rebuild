# Community tutorial series: what it shows about the mechanics

**Status:** analysed 2026-10-22 (orchestrator session). Community source, checked claim by claim against the parsed original files (P) and our RE notes (D). New leads are filed as OQ-101 to OQ-108 in [`../open-questions.md`](../open-questions.md).

## 1. Source and method

- **Source:** "anno-michi's ANNO 1503 Anfängertutorials", a YouTube playlist: https://www.youtube.com/playlist?list=PLD00C4FEE3FE0C216. Fourteen German beginner videos, about 2 h 50 min in total. They play the easy endless game "Bürger" (much money, large islands) from the first road to aristocrats.
- **Edition:** the small ore mine runs dry in V14. In our RE that happens only with the add-on flag set (`docs/re/production.md` §4.4), so the series was recorded with the add-on edition.
- **Method:**
  - The narration was read from YouTube's German auto-captions (speech recognition, so names are sometimes garbled). The video frames were **not** analysed, so numbers that appear only on screen are not checked.
  - Each claim was checked against `Data/BGruppen.dat` (read as text) and `Anlagen.dat` (the `HEAD`, `PRODTYPE`, `WARE`, `ROHSTOFF`, `PROD3`, `INFRAMIN` and `FORSCHTYPE` records, decoded with a scratch script on top of `tools/chunks.py`). It was also checked against `docs/re/population.md`, `production.md`, `fire-and-ships.md` and `tick-loop.md`.
  - The transcripts and the scratch script are not in git.
- **Rates:** production rates below are nominal, from `PROD3`. `T0` = seconds per ton of output; `I0` = tons of input per ton of output (`production.md` §2.3). They ignore harvesting, walking and field growth.

| V | Video | Length |
|---|---|---|
| 1 | Pioneers → settlers I, https://youtu.be/4hsoLd3T2Xs | 4:57 |
| 2 | Pioneers → settlers II, https://youtu.be/qZgS069PnMc | 7:30 |
| 3 | Settlers → citizens I, https://youtu.be/oD9TMHbC-qc | 10:10 |
| 4 | Settlers → citizens II, https://youtu.be/m6ojm-VM-m0 | 10:24 |
| 5 | Settlers → citizens III, https://youtu.be/Ak8Nvga-yyE | 10:11 |
| 6 | Settlers → citizens IV, https://youtu.be/S3Jfe5NmdWc | 9:58 |
| 7 | Settlers → citizens V, https://youtu.be/FFLePamdHMk | 7:53 |
| 8 | Citizens → merchants I, https://youtu.be/ojlZP3wccDU | 10:28 |
| 9 | Citizens → merchants II, https://youtu.be/SowtFOWPsjg | 10:07 |
| 10 | Citizens → merchants III, https://youtu.be/bzhuZW6QG8k | 10:13 |
| 11 | Citizens → merchants IV, https://youtu.be/7Lri1L3HmH0 | 10:18 |
| 12 | Citizens → merchants V, https://youtu.be/fXheU2wBKgM | 10:43 |
| 13 | Citizens → merchants VI, https://youtu.be/QQ8ZAgdnkkI | 8:56 |
| 14 | Aristocrats, https://youtu.be/l3NeSTwfEsA | 49:43 |

References read "V3 08:29" = video 3 at 8 min 29 s (append `?t=509` to the link).

## 2. The progression the series plays

1. **Pioneers** (V1–V2):
   - Production: foresters with a road; hunters, which need forest nearby; a tannery; sheep farms around a weaving hut.
   - Supply: a market hall to collect goods; houses; then market stands (cloth and leather, food and salt).
   - Next: a tavern once enough pioneers live there, fed with alcohol from small farms; then a chapel. Pioneers rise to settlers without salt.
2. **Settlers** (V3–V7):
   - Hop farms and a brewery replace the small farms; a quarry and stonemason make the bricks this needs.
   - A school earns knowledge, which researches the well and then the fire brigade.
   - Spice and tobacco come from other islands, carried by automatic trade routes. Salt comes from a salt mine and a saline. A church completes the citizens' needs.
3. **Citizens** (V8–V13):
   - Food: grain farms, mills and bakeries take over.
   - Tools: the player's own ore mine, smelter and smith replace buying tools from the free trader.
   - Health: herbs and the medicus deal with plague.
   - Lamp oil: whalers and lamp-oil makers on a northern island.
   - Then marble for the bathhouse, and the university. The first merchants appear.
4. **Aristocrats** (V14):
   - Unlocked at 1,900 merchants.
   - Clothing comes from a tailor using cloth and furs from a fur trapper.
   - A chapel, bathhouse, theatre and pavilion are built in a new district. Aristocrat houses are placed directly.
   - The cathedral, and later the castle, unlock with the aristocrat count.

## 3. Claims checked

Verdicts: **agrees** = matches the file (P) or a disassembled function (D); **partly** = right result for another reason, or only part of it holds; **contradicts**; **new** = nothing in our evidence yet (an OQ).

### 3.1 Residents, needs and upgrades

| Claim | Where | Our evidence | Verdict |
|---|---|---|---|
| Pioneers become settlers with a tavern and a chapel; salt "is not important yet" | V2 01:55, V2 06:03 | Settlers' forced goods are leather, cloth, salt and the tavern. The goods score is their **mean**, while infrastructure takes the minimum (`population.md` §4.1, §5). With salt at 0 the mean is 75 %, index 12, settler `BedarfTab` 2 > 0, so the upgrade passes. The chapel is infrastructure, so it is strictly required. | agrees (D, P): salt can wait because goods are averaged, not because it is unneeded |
| Pioneers are content with food alone; more pioneer houses are fine while food lasts | V9 08:36 | Pioneer `BedarfTab` ≥ 1 everywhere and no forced infrastructure, so only food can make them unhappy (`population.md` §4.1) | agrees (D, P) |
| Settlers → citizens need salt, tobacco, spices, school, church | V3 00:51, V6 03:14 | Citizens: forced goods food, cloth, salt, tobacco, spices, tavern; forced infrastructure church and school | agrees (P) |
| Citizens → merchants need lamp oil **or** silk, plus bathhouse and university | V9 04:54, V13 03:51 | Merchants force **both** lamp oil and silk, but the goods score is the mean of 7 forced goods. One missing gives 6/7 = 86 %, index 14, `BedarfTab` 1: passes. Both missing gives 5/7 = 71 %, index 11, value 0: fails. | agrees (D, P): "either one" is an effect of the mean, not a rule in the data |
| Aristocrat houses unlock at 1,900 merchants, are placed directly, never develop, and vanish when needs fail | V13 08:31, V14 02:48 | Type 580 `INFRAMIN` = `INFRA_STUFE_4F` (1,900 merchants). `ARISTOKRATEN` has `NoUpgrade` and `NoDowngrade`. A placed house starts full (`population.md` §5 SetGroup). With `NoDowngrade` it is removed below 1 resident (§4). | agrees (P, D) |
| Aristocrats need food and clothing; jewellery and wine are optional extra income; church, bathhouse, theatre, park | V14 06:26 | `SCHMUCK` and `WEIN` are `Force: FALSE`; `KIRCHE`, `BADEHAUS`, `THEATER` and `PARK` are forced | agrees (P) |
| About 30 people per aristocrat house | V14 47:57 | `Wohnmax` 30 | agrees (P) |
| An aristocrat house costs 12 tools, 10 wood, 20 bricks, 5 marble | V14 19:38 | 580 `HEAD[3..6]`: 20 bricks, 10 wood, 12 tools, 5 marble | agrees (P) |
| Houses need building materials to upgrade; locking them stops upgrades and saves bricks and wood | V3 03:28, V10 06:33, V13 04:18 | The upgrade check needs the next house type's materials in store and removes them (`population.md` §5). Settler house 520: 1 tool, 4 wood. Citizen house 540: 2 tools, 2 wood, 4 bricks. Merchant house 560: 5 tools, 5 wood, 4 bricks. | agrees (P, D) for the cost; the **lock** is new (OQ-104) |
| Over-produce, because upgrades raise consumption | V14 02:03 | An upgrade raises residents towards the next `Wohnmax` (8, 15, 28, 42) and the consumption base follows (`population.md` §4) | agrees |
| Houses "collapse" when a good runs short after upgrading | V12 02:46 | A negative mood lowers residents. Below ¾ of the lower level's `Wohnmax` the house **downgrades**; only `NoDowngrade` houses (aristocrats) disappear. | partly: only aristocrat houses vanish |
| Tavern, chapel and school come with the population | V2 02:10, V2 06:54 | `WIRTSHAUS` at `1B` (80 pioneers), `KAPELLE` at `1C` (125), `SCHULE` at `2B` (50 settlers) | agrees (P) |
| Grain farms unlock at 200 citizens | V8 01:11 | 811 `INFRAMIN` `3B` (200 citizens); mill 1002 and bakery 1004 also `3B` | agrees (P) |
| A monument is offered at the citizen stage | V10 09:32 | `DENKMAL` 1391 at `3E` (800 citizens); others at `4A`, `4B`, `4D` | agrees (P), probably 1391 |
| The cathedral works as a church; it came between 500 and 720 aristocrats | V14 43:36 | 1330 `WARE KATHETRALE`, need mask `0x1c0` = chapel + church + cathedral (`production.md` §2.4); `INFRAMIN` `5B` = 600 aristocrats | agrees (P, D) |
| The castle is not unlocked at 500 aristocrats; probably at 1,000 | V14 40:25 | Main castle 1380 at `5C` (1,000); parts 1381, 1383, 1385, 1392 at `5D` (1,500); 1384 at `5E` (3,000). `5A` (500) unlocks only decoration 1387. | agrees (P) |
| The park need is met by a pavilion, not by trees, hedges or statues | V14 16:23 | 1386 `PAVILLION`, `WARE PARK`; the decoration types (`ZIERDE`) have `WARE UNUSED` | agrees (P) |

### 3.2 Production and chains

| Claim | Where | Our evidence | Verdict |
|---|---|---|---|
| The small farm makes food with animals; planting potato fields turns it to alcohol and the animals leave | V2 02:34 | 809 `UNIVERSAL`: line 0 alcohol from potatoes, line 1 food from grass with animal figures (`production.md` §4.5) | agrees on the two lines; what switches line 1 off is new (OQ-104) |
| The hop farm is cheaper to run and better than the small farm; it needs bricks | V3 01:18 | 815: 250 gold, 4 bricks, running cost 10/18. 809: 250 gold, no bricks, running cost 8/20. | agrees (P) |
| Two hop farms per brewery | V3 06:14 | Hops 30 s/t = 2 t/min. Brewery 1014: 30 s/t, `I0` 1.88, so it needs 3.75 t of hops per minute = 1.9 farms. | agrees (P) |
| Two sheep farms per weaving hut | V14 29:17 (V1 builds 4 : 2) | Sheep farm 807: 45 s/t = 1.33 t/min. Weaving hut 1001: 43 s/t, `I0` 1.85, so it needs 2.6 t of wool per minute = 1.9 farms. | agrees (P) |
| The (researched) weaving mill makes more cloth than the weaving hut | V6 02:32, V8 03:51 | 1005 (research `GR_WEBER`): 20 s/t (2.15× the hut) and `I0` 1.33 (28 % less wool per ton). It costs 500 instead of 300 gold and 30 instead of 15 running cost when active. | agrees (P) |
| …but V14 noticed no difference and builds the cheaper hut | V14 29:22 | See the row above | contradicts (P) |
| 2 grain farms → 1 mill, 2 mills → 1 bakery | V8 02:19 | Mill 1002: 20 s/t, `I0` 1.17. Bakery 1004: 10 s/t, `I0` 0.98, so it needs 5.9 t of flour per minute = 1.96 mills. The farm side depends on field growth (grain field 310 s, `production.md` §4.3); not checked. | agrees for mill : bakery |
| Two tobacco plantations per tobacco works | V6 07:09 | 813: 30 s/t. 1008: 30 s/t, `I0` 1.99. | agrees (P) |
| 1.5 lamp-oil makers per whaler, so 2 whalers and 3 makers | V12 03:23 | Whaler 821: 15 s/t (4 t/min nominal; whales limit it). Lamp-oil maker 1028: 20 s/t, `I0` 0.94, so it needs 2.8 t per minute. That is 1.42 makers per whaler. | agrees (P) |
| One ore smelter feeds two smiths | V14 14:01 | Smelter 1017: 15 s/t = 4 t/min. Smith 1019: 28 s/t, `I0` 0.88, so it needs 1.9 t of iron per minute. That is 2.1 smiths per smelter. | agrees (P) |
| The small ore mine runs dry; research the large ore mine, which also yields more | V14 03:28, V14 19:59 | A shallow iron mine uses 400 t of a fresh deposit, then stops (1,600 t reserve; add-on edition only). The deep mine (research `TIEFE_ERZMINE`) never empties it (`production.md` §4.4). 1216: 6.7 s/t; 1205: 12 s/t. | agrees (D, P) |
| One saline can take two salt mines, but one is enough | V7 00:25 | Salt mine 1203: 10 s/t = 6 t/min. Saline 1022: 10 s/t, `I0` 1.02, so it needs 6.1 t per minute. | partly: 1 : 1 balances; a second mine is surplus |
| Two or three stonemasons can work one quarry | V10 04:53, V14 37:50 | The quarry (`STEINBRUCH`) has no tick of its own. The stonemason is a farm-family building that harvests it within radius 12 (`production.md` §2.1, §4.2). Nothing reserves the quarry. | agrees (D) |
| The fur trapper hunts arctic foxes, hares and bears; one outpaces two tailors | V14 10:03, V14 32:44 | 824 hunts the nearest figure carrying its input good (`production.md` §4.2a): 10 s/t = 6 t/min nominal. A tailor uses 0.23 t of fur per ton at 20 s/t = 0.7 t/min. | agrees on rates; species not checked |
| The tailor eats a lot of cloth | V14 15:41 | 1027 uses 0.94 t of cloth per ton of clothing, 2.8 t per minute at full rate (more than one weaving hut makes) | agrees (P) |
| A stopped building costs less (the tailor is stopped "because it costs a lot") | V14 07:33 | `PROD3` running costs A < B, read as inactive and active (`production.md` §2.3, `population.md` §8); 1027: 15/30 | supports the **likely** reading |
| Wine is a single winery with vineyards; jewellery needs a gold mine, a gem mine and a goldsmith | V14 31:50, V14 36:54 | 822 makes `WEIN` directly. 1039 makes `SCHMUCK` from `GOLD` and `EDELSTEINE`; gold mine 1206 and gem mine 1214. | agrees (P) |
| The medicus needs herbs and research | V11 02:47, V11 06:55 | 1337 `ARZT`: input herbs, research `MEDICUS`; herb plantation 823 also needs `MEDICUS`. Its `KLINIK` good is in no group's needs, so it only fights plague. | agrees (P) |
| The court does nothing; robbers were planned and cut | V14 33:44 | `GERICHT` (1339, research `GERICHT`) and `GALGEN` (1326) are in no group's needs | agrees for needs; whether other code reads them is open |
| Market stands: cloth and leather, food and salt, tobacco and spices, lamp oil, clothing | V2 01:32, V5 05:53, V12 04:54, V14 13:04 | 1314, 1310, 1315, 1317, and 1318 (silk and clothing); also 1316 wine and 1313 jewellery. Action radius 4 (`HEAD[0x11]`). | agrees (P) |
| Building a church turns the chapels into churches, and building a university turns the school into one; later a chapel is a cheap church | V7 06:16, V13 03:10, V14 12:46 | Two chapel types: 1320 (`WARE KAPELLE`, `1C`) and 1321 (the same cost, 700 gold, `WARE KIRCHE`, `2F`). Two school types: 1346 (`SCHULE`, `2B`) and 1347 (`HOCHSCHULE`, `3C`). The church 1325 (1,600 gold, running 50) and university 1379 (2,500 gold, running 150) also exist. | agrees that the cheap types serve the higher need; the **trigger** is new (OQ-101) |

### 3.3 Money, events and the clock

| Claim | Where | Our evidence | Verdict |
|---|---|---|---|
| Spices sell for 80 apiece; tobacco for more | V5 09:40 | `GEWUERZE` `Preis` 60, plus 5 and 5 (both bonuses on `NORD`/`TUNDRA`/`PRAERIE`), plus `PreisLevel` 10 at tax level 2 = 80; at tax level 1 it would be 75. `TABAKWAREN` on `NORD`: 75 + 5 + 10 = 90. | fits only the highest tax level (frame not checked) |
| Fires keep breaking out in the outer houses; demolish a burning house without a fire brigade or the fire spreads; a brigade covering every house makes fire harmless | V3 08:29, V4 03:44, V14 28:38 | Random fires pick **only pioneer and settler houses** (`fire-and-ships.md` §2.2), and the series puts those at the edges. A flame tries to spread every 9th step (§2.3). Removing a building puts its fire out. Brigades only react. | agrees (D) |
| Plague starts above some population, comes more often the bigger the city, is cured by the medicus, and houses out of the medicus' reach fall | V13 07:02, V14 11:39, V14 20:50 | The outbreak chance grows with inhabitants above 500 (`fire-and-ships.md` §2.4). If the "255 = never" rule applies as it does for fire, the first plague needs ≥ 1,524 inhabitants; the series' first plague came after its 1,500-inhabitant message. Plague spreads to houses within 2 tiles and cures itself 25 % per cycle. "Every few seconds" in a big city is the spreading: the outbreak chance caps at 4/31 per 25 s check. | agrees (D); the medicus and the mood link are new (OQ-105) |
| F5 normal, F6 double, F7 four times | V13 02:19 | Keys `0x74`–`0x76` → 100/200/400 %, 400 % single-player only (`tick-loop.md`, "Game speed") | agrees: settles our "likely" F5–F7 mapping |

## 4. Where the series is wrong or loose

- **Weaving hut vs. weaving mill:** V14 is wrong. The mill makes 2.15× the cloth from 28 % less wool per ton, at twice the active running cost.
- **Salt mines per saline:** at nominal rates one mine exactly feeds one saline; two are not needed.
- **"A chapel works like a church":** this holds only for chapel type 1321, which the chapel slot builds from stage `2F` on. A 1320 chapel serves only the chapel need.
- **"Houses collapse":** only aristocrat houses vanish; the others downgrade.
- **"Salt is not important" and "lamp oil or silk":** both are effects of averaging the goods needs. Every missing forced good still lowers the goods score, and with it growth.
- **Roads:** the narrator says aristocrat houses can do without roads (V14 31:05). The owner's rule (2026-10-22) is that houses need roads connected to markets, that services such as the fire brigade must reach a house by road, and that a road connects only at the building's entrance tile (OQ-103, `community-faq.md` §3).
- **The church's reach** (V10 03:52): a citizen house outside the church's drawn circle still had the need met, and the narrator could not explain it. Coverage may be measured by footprint overlap or by walkers rather than the drawn circle (OQ-102).

## 5. New leads (filed as OQs)

| OQ | Topic | Evidence in the series |
|---|---|---|
| OQ-101 | What turns chapels into churches and schools into universities (the stage, or the first big building), and whether a chapel placed later directly becomes 1321 | V7 06:16, V13 03:10, V14 12:46 |
| OQ-102 | How houses get market-stand goods and infrastructure: residents buy at stands within radius 4; the innkeeper fetches alcohol from the market hall; stands have exits; coverage outside the drawn circle | V2 01:36, V2 05:41, V9 07:42, V10 03:52 |
| OQ-103 | Which walkers need roads: sheep farms need none inside a weaving hut's radius; the stonemason walks to the quarry off-road; the forester needs a road for collection | V1 00:58, V1 03:34, V3 01:58 |
| OQ-104 | Player controls missing from the command list (see §6) | V1 01:10, V3 03:28, V10 09:13, V14 22:39 |
| OQ-105 | Plague and the medicus: house flag bit 4 (forced bad mood), the medicus' targets and radius, the first outbreak threshold | V13 07:02, V14 20:50 |
| OQ-106 | Wildlife: game only near forest; deer eat sheep pasture, so pastures do better without forest | V1 02:05, V10 01:08 |
| OQ-107 | Research: settlers at school earn knowledge; the well and the fire brigade cost 5 each; civil and military research at the university; the unread `Wissen` lines in `BGruppen.dat` | V3 09:17, V14 03:59 |
| OQ-108 | Trade: buy orders filled by the free trader up to a price limit; sell orders above a stock level at a minimum price; computer players come to buy; trade improves relations up to an alliance | V2 03:24, V6 01:30, V10 02:22, V14 27:18 |

## 6. What this means for the rebuild

- **Simulation:** nothing in the series contradicts the formulas implemented in S3–S8. Several player rules of thumb fall out of the mean over forced goods. These make good synthetic scenario tests:
  - pioneers upgrade without salt;
  - merchants form with lamp oil and no silk;
  - nothing forms with both missing.
- **Commands** (S8 and later, OQ-104). The original lets the player do things our command set does not yet name:
  - lock building materials per city;
  - stop and start a building;
  - plant fields or trees in a building's radius; the small farm's line choice already exists as `SetProductionLine`;
  - Kontor buy and sell orders with a stock limit and a price;
  - automatic trade routes with load and unload per Kontor;
  - trade treaties and alliances;
  - unloading and moving a scout.
  - **House variant:** V14 22:39 shows the aristocrat house look changing each time the build tool is picked, so the player chooses a look by cycling the preview. In lockstep the chosen variant must travel in the build command, or be drawn by the simulation when the command executes. It must never be drawn by the client's preview.
- **UI** (M5):
  - selecting an island lists the crops that grow well there (V5 00:09);
  - placing a farm shows fertility bars, and one missing bar still yields well (V4 00:23, V14 30:22);
  - a double-click shows everything the player owns (V6 09:27);
  - messages at population steps and for computer players' milestones (V4 04:18, V11 03:14, V13 06:08);
  - short videos when big buildings unlock (V10 09:32, V14 43:36).
- **Computer players:** in the easy endless game they build slowly and poorly on purpose, so they leave land to the human (V5 05:23). They settle large islands (V3 07:21). A rival's city appears only once one of your ships comes near (V3 07:47), so discovery is per-player state.
- **T15 fixtures:** cheap checks for the owner sessions with the running original:
  - random fires only in pioneer and settler houses;
  - no plague below about 1,524 inhabitants;
  - the shallow iron mine stops after 400 t in the add-on edition;
  - two hop farms per brewery at full supply.
