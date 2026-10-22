# The manual and the in-game help: what they give the reverse engineering

**Status:** 2026-10-22 (orchestrator). It continues [`tutorial-videos.md`](tutorial-videos.md), [`community-faq.md`](community-faq.md) and the player's [`playbook.md`](playbook.md). New evidence for open questions is added to OQ-023 and OQ-101 to OQ-108 in [`../open-questions.md`](../open-questions.md).

## Sources and authority

- **[M p. n]:** the owner's German manual (`Anno_1503_Manual.pdf`, 77 pages; never in git). The playbook already uses it.
- **[H/page]:** the **in-game help**, `game/app/help/*.htm`. It has 582 English HTML pages that ship with GOG 2.0.0.5, plus `InstantHelpLinks.txt`.
  - The help was listed in `game-files-analysis.md` but never read before this note.
  - It holds building costs, unit values, and short rules for most mechanics.
  - It also holds the "active help" pages `IH_*`, keyed by condition names (`COMPHELP_*`).
- **Authority:**
  - **The order:** files and disassembly (P, D), then the owner, then the shipped documents (M, H), then the community.
  - **What the shipped documents are:** the designers' statements of intent for this build. Where they differ from P or D, the code wins, and the difference becomes a "faithful vs. fix" candidate (§4).

## 1. Checked against the files

### 1.1 Building costs: the help agrees with `Anlagen.dat`
94 help pages give build costs.
- **Comparison with `HEAD`** (gold u16 `[8]`; bricks, wood, tools, marble `[3..6]`):
  - 580 of 589 values are equal;
  - 86 pages are equal in every value.
- **The 9 differences** (the data is the authority):

| Building (id) | Value | Help | `Anlagen.dat` |
|---|---|---|---|
| fisherman's hut (810) | gold | 80 | 180 |
| goldsmith (1039) | gold | 400 | 300 |
| library (1378) | gold | 200 | 2000 |
| cathedral (1330) | gold | 1200 | 12000 |
| cathedral (1330) | wood | 10 | 30 |
| large ore mine (1216) | bricks | 8 | 10 |
| deep well (1342) | tools | 4 | 5 |
| lamp-oil maker (1028) | bricks | 0 | 5 |
| cloth stand (1314) | running cost while working | 4 | 5 |

- Library and cathedral are probably dropped zeros in the help; the others may be a patch rebalance.

### 1.2 Running costs: which value is "working" is now confirmed
- **The check:** 50 help pages give both "operating costs while active" and "while shut down". In all 50, the active value equals `PROD3` u16 `0x02` and the shut-down value equals `0x00` (P).
- **What it closes:** the "likely" pairing in `production.md` §2.3 and `population.md` §8.
- **Pages with a single "operating cost"** (chapel, school, tavern, …) belong to buildings whose shut-down value is 0.

### 1.3 Names
The help corrects names in `playbook.md` §14 (now fixed):
- **2229** (PRODTYPE `KANONENTURM`, 650 gold, citizens) is the **defensive tower** ("Abwehrturm"). It fires over 180° and takes no cannons.
- **2230** (PRODTYPE `WACHTURM`, 200 gold) is the **cannon tower**. It holds up to two cannons with crews.
- **1009** is the **dyer** ("Färberei": silk + dyes → silk cloth).
- **1037** is the **small weapon smith** (swords), and **1036** the **large weapon smith** (axes, lances).

### 1.4 Units: meanings of `FigurenHeader.dat` `LOGIC` fields
- **The help values:** each unit page gives range, damage and armor.
  - On six pages the labels are swapped; the value given in "fields" is the range.
  - Range, damage and armor come from the help page.
- **The record fields:**
  - `0x12` and `0x27` are u8 values divided by 8 (stored at type `+0x14` and `+0x18`; the loader keeps `+0x18 ≥ +0x14`, `fire-and-ships.md` §3.1);
  - `0x1c` and `0x1e` are u16.

| Unit (figure) | Range | Damage | Armor | `0x12`/8 | `0x27`/8 | `0x1c` | `0x1e` |
|---|---|---|---|---|---|---|---|
| pikeman (700) | 1 | 6 | 50 | 0.75 | 0.75 | 40 | 500 |
| archer (701) | 9 | 6 | 35 | 8 | 9 | 70 | 520 |
| lancer (702) | 1 | 17 | 80 | 0.875 | 0.875 | 170 | 1050 |
| musketeer (703) | 1 | 14 | 100 | 0.75 | 0.75 | 140 | 1500 |
| swordsman (704) | 1 | 8 | 60 | 0.875 | 0.875 | 80 | 900 |
| cavalry (705) | 1 | 8 | 90 | 0.75 | 0.75 | 80 | 1100 |
| catapult (706) | 10 | 55 | 75 | 10 | 10 | 450 | 1100 |
| marksman (708) | 7 | 20 | 45 | 7 | 8 | 200 | 600 |
| crossbowman (709) | 9 | 7 | 40 | 8 | 9 | 90 | 550 |
| cannon (710) | 9 | 40 | 55 | 9 | 9 | 400 | 1100 |
| mortar (711) | 10 | 60 | 110 | 10 | 10 | 600 | 1100 |
| siege tower (712) | 5 | 0 | 250 | 5 | 5 | 0 | 3000 |
| crew (713) | 1 | 3 | 30 | 0.875 | 0.875 | 30 | 700 |
| medic (716) | 1 | 9 | 25 | 0.75 | 0.75 | 90 | 350 |

- **`0x1c` = damage × 10** (**likely**):
  - the values are equal for 10 of 14 units;
  - pikeman, archer, crossbowman and catapult differ, which a patch rebalance would explain (ISS-082);
  - the manual's "the crossbow does more damage than the bow" [M p. 51] agrees: 90 against 70.
- **`0x12` and `0x27` are ranges in 1/8 field** (**likely**):
  - every help range equals one of the two;
  - melee units have 6–7, about one field.
  - For archer, crossbowman and marksman the second value is one field longer. The range research (feathered shafts, crossbow range [H/Forschung_Waffenproduktion]) is the likely reason for the two values.
- **`0x14` = unit slots** (**likely**):
  - **War machines:** catapult 2, cannon 1, mortar 1, siege tower 2. These are the manual's crew counts exactly [M p. 56]: "catapult at least one, mortar one, cannon one, siege tower at least one, up to two".
  - **Ships:** the large warship has 12 and the large trading ship 3, which fits "warships carry more troops" [H/Kriegsschiffe]. This replaces the guess "cannons" in `fire-and-ships.md` §3.1.
- **`0x1e` (maximum hit points, D) against "armor":**
  - they are related but not equal: pikeman 500/50 and mortar 1100/110 are ×10, the rest is higher in the data;
  - so the help's armor is either another value or an older balance. **Open.**
- **`0x16` and `0x15`** (**guess**; read the getter slot `+0x94(1)` / `+0x94(2)` to confirm):
  - `0x16` is u16: soldiers 50–700, ships 1,500–4,000. It fits a training or build price.
  - `0x15` is u8: soldiers 1–10, ships 10–25. It fits a per-minute upkeep, the unit side of `upkeep_a`/`upkeep_b` (O-49).
- **Not in the data: the manual's reach claims.** The manual says lancers out-reach swordsmen and archers out-range crossbowmen [M p. 51].
  - The data gives equal reach to lancer and swordsman, and equal ranges to archer and crossbowman.
  - The community complaint that pikes and lancers do not beat cavalry (ISS-066) fits this.
  - Unless a damage-by-target-class table exists (open).

### 1.5 Bankruptcy
- **The playbook (from the FAQ):** "below −1,000 gold for some time".
- **The disassembly** (`players.md`, `0x1001b900`): money ≤ **−3,000** starts a deadline of 150,000 ticks, which is **300 s** at normal speed.
- The playbook is corrected.

### 1.6 Keys
- **Speed keys:** F5 normal, F6 ×2, F7 ×4, F8 ×½ [M p. 70, H/Spielgeschwindigkeit]. T15 measured 1984 / 3986 / 523 game-ms per second (PR #16). This closes "confirm the F5–F8 mapping" in `tick-loop.md` Open.
- **Pause and rating:** P pauses [M p. 70]. The rating screen opens with Q [H/Bewertung] or Shift+H [M p. 70].

## 2. Statements that decide between our open readings

Each one is a short scripted test for the T15 play layer (or T16 under Wine).

- **OQ-101, chapel → church and school → university:**
  - **The shipped documents:**
    - when a church is built, all chapels **in its service area** become stone chapels that count as churches [H/Kapelle, H/Kirche];
    - schools **in a university's service area** can research everything [H/Forschung, M p. 50].
    - The data has the target types: 1321 is a chapel with `WARE KIRCHE`, and 1347 a school with `HOCHSCHULE`.
  - **The FAQ says "every one on the island".**
  - **Test:** one chapel inside and one outside a new church's area.
- **OQ-103, roads:**
  - entrances must face a street;
  - without a street connection, the market wagons cannot pick up goods;
  - "a street connection shortens the way inhabitants and workers have to walk", which implies they can also walk without one [H/Strassenanschluss].
  - **Cobblestone streets:** "inhabitants and market wagons move considerably faster" on them [H/Pflasterstrasse].
    - That is the design intent. The videos call paved roads cosmetic, and the community says a developer called it a bug (ISS-093).
    - `wegspeed.dat` is a slope walk mask (every bit 1), not a speed table, and nothing reads it.
    - **Test:** time one market-wagon round trip on dirt and on cobblestone.
- **OQ-107, research:**
  - starting research **uses up all accumulated knowledge** [H/Wissen];
  - the cap depends on the level, and **one** library raises it while more libraries do not [H/Bibliothek, M p. 49];
  - research costs knowledge **and** gold;
  - each area starts with one key technology;
  - fields marked X can only be researched at a university [M p. 50].
  - **The item list:** the help lists about 36 items in four areas [H/Forschung_*]. That is the checklist for mapping the 65 research slots of the player object.
  - **Costs not found:** a search of `AnnoWelt`, `AnnoGame`, `AnnoFrame` and `AnnoScene` for two of the FAQ's cost pairs (library 60/2000, weaving mill 20/300, as u16 or u32 values close together) found no table. The costs are either in code or computed.
- **Mines (OQ-035, O-37, `production.md` Open):**
  - a deposit can be **exhausted**, and then the ore symbol disappears;
  - a small mine can also stop because it cannot reach deeper ore, while the symbol stays;
  - a large mine built on top reaches the deeper ore, which is **inexhaustible** [H/Erzmine_erschoepft].
  - The community says mines never deplete (ISS-087).
  - **Test:** run a small iron mine at 8× speed and read the deposit.
- **Storage and wagons:**
  - capacity grows with the civilization level and with each warehouse or market building in the city [H/Lagerkapazitaet_erhoehen];
  - each has up to two wagons, depending on the level [H/Marktkarren];
  - trade settings apply to every warehouse and market of the city [H/Einkauf_Waren], which matches the per-city trade lists.
  - **Test:** read city `+0x624` before and after each new market hall, and after the settler step.
- **Fire (OQ-105):**
  - **Causes** [H/Feuer]: a volcano, a chance event, war (flaming arrows), and "a large, crowded settlement of wooden buildings".
  - **Pioneer and settler density** raises the fire danger [H/Pioniere, H/Siedler]. Random fires are limited to pioneer and settler houses (`fire-and-ships.md` §2.2, D), which agrees.
  - **Still to check:** whether the start chance reads the density of neighbouring houses.
- **Plague:**
  - the doctor prevents or ends it, and needs herbs [H/Pest, H/Medikus];
  - "faster healing" lets him make more house calls at once [H/Forschung_SchnellereHeilung].
- **Small farm:** the manual says a stopped line gives its capacity to the other line, except at the hunting lodge [M p. 29]. Our RE has independent lines (`production.md` §4.5). **Test:** stop the alcohol line and read the food output.

## 3. Mechanics we had not listed

- **Taking over buildings** [H/Kontor_uebernehmen, H/MHH_uebernehmen, H/Stadt_uebernehmen]:
  - damage an enemy warehouse or market building almost to destruction;
  - then build your own on top, by ship or by scout, with the materials on board;
  - you take over every building in its service area.
  - A city is taken when all its warehouses and market buildings on the island are replaced.
  - **Likely code:** O-4's "market/Kontor in-place replacement" (`0x10027b62`).
- **Boarding** [H/Entern]: a ship with cannons and archers captures an enemy ship once the enemy's archers are dead and its damage reaches the threshold ("10 percent"). The cargo comes with the ship. It exists in the add-on content only (ISS-069).
- **Bandits** [H/Raeuber]: they climb the walls and rob inhabitants, and a district court reduces how often. ISS-091 says the court had no effect before the add-on. Look in the city event pass `0x1002ba80` (O-3).
- **Riots** [H/Aufstand]:
  - they start from a lasting lack of a consumer good or from long unhappiness;
  - rioters attack the buildings that matter to that level.
- **Earthquake** [H/Erdbeben]: it mostly damages houses, and other cultures cannot rebuild.
- **Rewards:**
  - **Reward buildings** can be built once per achievement, and again when it is reached again [H/B_Belohnungsbauten].
  - **The triumphal arch** comes after totally defeating a player while the capital is at least citizens [H/Triumphbogen].
  - **The palace** [H/Schloss]: from 1,000 aristocrats the main part plus 4 parts, 4 more at 3,000, and no limit at 5,000.
- **Pirates and Venetians:**
  - protection money loses effect when another player also pays [H/Schutzgeld];
  - the Venetians refuse an order when the buy price is too low, the sell price too high, or they cannot deliver or do not want the good [H/H_Venezianer].
- **Diplomacy** [M p. 32–36, H]:
  - every insult raises the aggression bar;
  - more than five scouts in a computer player's territory can trigger war, or even one more, depending on its personality [H/Scout, M p. 18];
  - an alliance needs a trade treaty, and its chance grows with the trade volume;
  - an attack voids all treaties;
  - a goods tribute must come on an unarmed ship without soldiers, flying the white flag.
  - These are CompLogic targets (O-1, OQ-108).
- **Buy and sell prices:** the price slider's arrow turns green, yellow or red [M p. 43, H/Verkauf_Waren]. The UI therefore computes an acceptance band per good. The function behind the colour is the shortest way to the trader's price limits (OQ-108).

## 4. Faithful vs. fix candidates (intent against code)

| Topic | Intent (M, H) | Code or community | Decide by |
|---|---|---|---|
| Cobblestone speed | faster for residents and wagons | no road speed found; ISS-093 | T15 timing |
| Court vs. bandits | the court reduces raids | no effect before the add-on (ISS-091) | `0x1002ba80` |
| Small-farm line switch | the stopped line's capacity moves over | independent lines (D) | T15 output |
| Chapel → church scope | service area | island (FAQ) | T15 two chapels |
| Unit reach | lancer > swordsman, archer > crossbow | equal in the data | combat code |

## 5. Reverse-engineering leads, in order

1. **Place side effects** (O-4: the PRODTYPE hooks `0x1002c920`, `0x1002ca80`, `0x1002d2a0`, `0x1002cf60`, and the in-place replacement `0x10027b62`) and O-28 (`0x1002c090`, `0x1002c740`): the chapel and school rewrite, and building takeover.
2. **Figure-type getters** (vtable `0x10021d10`): slot `+0x68(0..5)` for the ranges, `+0x94(1/2)` for price and upkeep, `+0x88(1)` for unit slots. They confirm §1.4.
3. **The city event pass `0x1002ba80`:** bandits and the court, riots, and the density term of fire.
4. **The mine deposit extraction** (slots `+0x88`/`+0x8c`): exhaustion and the deep ore.
5. **City capacity `+0x624`:** who writes it when a Kontor or market is built or upgraded.
6. **The price-arrow colour in `AnnoFrame.dll`** (trade menu): the acceptance band.
7. **The `COMPHELP_*` conditions** (`InstantHelpLinks.txt`, read by `AnnoFrame.dll`): wrong fields, missing road, store full, raw material missing, island with the wrong climate, and so on. They are the game's own failure checks, and the play layer's diagnostics list (`docs/testing/play-original.md`).
