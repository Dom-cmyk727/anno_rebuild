# Monuments and ornamentals

**Status:** T31.1 (2026-10-26), data and claims only. The code is not read yet (T31.2). No `src/` change.

Tags: **P** parsed from the original files this pass; **D** already written in another note (module + address there, not re-read here); **H** the English in-game help; **M** the owner's German manual (`Anno_1503_Manual.pdf` at the repo root, untracked, not added to git). Printed page numbers are the ones in the manual's headers. Community claims cite a URL and stay community until T31.2 matches them to the code.

Tools this pass: `anno-dump` from the main build (`build/msvc/src/tools/anno_dump/Release/anno-dump.exe`), `anno-dump texte --full` on `Texte.dat` and `anno-dump anlagen` on `Anlagen.dat` (`ANLAGEN entries=921 volumes=3`). `HEAD` / `PROD3` fields come from `tools/figuren.py` at the offsets in [`production.md`](production.md) §2.3–§2.4. `Data/BGruppen.dat` was read as Latin-1 text. Scratch stayed in the worktree `cache/`.

## 1. The three production types

`state.md` Appendix C gives the numbers: 289 `DENKMAL`, 290 `TRIUMPH`, 291 `ZIERDE`. The Place path treats `PRODTYPE` ≥ 193 as a tile object and skips `Create` (`AnnoWelt` `0x10027b32`, already D in [`annowelt.md`](annowelt.md)). Scenario `Create` `0x10024010` builds `PRODTYPE` > 192 as the base class in the production list (T25 (j), already D). This note does not re-prove either path.

`Anlagen.dat` holds **55** entries of these three types (P): 28 `DENKMAL`, 1 `TRIUMPH`, 26 `ZIERDE`.

On every one of the 55 (P):

- `HEAD` is 56 bytes and `PROD3` is 68 bytes.
- The flag word `HEAD[0x1c]` is `0x0084` (bits 2 and 7). [`production.md`](production.md) §2.4 names `0x2000` coast, `0x1000` mountain and `0x4000` road. Bits 2 and 7 are not named there. What they do is open for T31.2.
- `HEAD[0x31]` (can burn) is 0.
- `FORSCHTYPE` is an empty string. None of the 55 names a research id.
- The `WARE` and `ROHSTOFF` records are present and their name strings are `UNUSED`. The public-need mask is the resolved goods id ([`production.md`](production.md) §2.4). The resolution of the name `UNUSED` is not re-read here.
- 46 of the 55 have a `WALKMASK` with at least one nonzero byte; 9 are all zero (1394, 2550, 2551, 2552, 2564, 2566–2569).

`anno-dump anlagen` record lists, flag 0, for the three that the texts single out:

- 1388 (and 1389, 1390, 1391): `VERSION2,HEAD,PRODTYPE,PROD3,INFRAMIN,WARE,ROHSTOFF,BAUSAMPLE,WALKMASK,SHOTMASK,OWNMASK,OBJECTPOS,COLORS,HAEUSER`
- 2553: the same list without `BAUSAMPLE` and without `OBJECTPOS`
- 2780: the same list without `BAUSAMPLE`

## 2. Named types

19 of the 55 have a name in `Texte.dat` section `ANLAGEN` (326 `id:name` lines; `anno-dump texte`). The `ANLAGEN` string for id 2553 is `Monument` with one trailing space. Costs below are the file bytes: goods `HEAD[3]` bricks, `[4]` wood, `[5]` tools, `[6]` marble, gold the u16 at `[8]` ([`production.md`](production.md) §2.4). The goods-cost getter returns the byte shifted left by 8; gold is the u16. Off / On are `PROD3` u16 at 0 and at 2, shut-down and active running cost in gold per minute (D, [`production.md`](production.md) §2.3, confirmed against the help in [`manual-and-help.md`](../research/manual-and-help.md) §1.2). Radius is `HEAD[0x11]`, the action radius (market search). HP is the u16 at `HEAD[0x2c]`.

| Id | Name | Type | Size | Gold | Tools | Wood | Bricks | Marble | Off | On | Radius | HP | Stage |
|---:|---|---|---:|---:|---:|---:|---:|---:|---:|---:|---:|---:|---|
| 1387 | Palace gate | ZIERDE | 4×1 | 2000 | 10 | 0 | 20 | 10 | 0 | 2 | 0 | 1000 | INFRA_STUFE_5A |
| 1388 | Hall of Generals | DENKMAL | 5×6 | 3000 | 15 | 0 | 30 | 15 | 0 | 5 | 0 | 6000 | INFRA_STUFE_4D |
| 1389 | Lions' portal | DENKMAL | 3×3 | 2000 | 5 | 0 | 20 | 10 | 0 | 2 | 0 | 1000 | INFRA_STUFE_4A |
| 1390 | Equestrian statue | DENKMAL | 2×2 | 1500 | 10 | 0 | 15 | 10 | 0 | 2 | 0 | 1000 | INFRA_STUFE_4B |
| 1391 | Fountain of Venus | DENKMAL | 2×2 | 1500 | 10 | 5 | 15 | 10 | 0 | 2 | 0 | 2000 | INFRA_STUFE_3E |
| 1393 | Ornamental garden | ZIERDE | 2×2 | 100 | 1 | 0 | 10 | 4 | 0 | 0 | 0 | 500 | INFRA_STUFE_3D |
| 1395 | Decorative flag | ZIERDE | 1×1 | 200 | 1 | 2 | 5 | 1 | 0 | 0 | 0 | 400 | INFRA_STUFE_3F |
| 2550 | Marketplace | ZIERDE | 1×1 | 10 | 0 | 0 | 1 | 0 | 0 | 0 | 16 | 0 | INFRA_STUFE_2A |
| 2551 | Marketplace | ZIERDE | 1×1 | 10 | 0 | 0 | 1 | 0 | 0 | 0 | 16 | 0 | INFRA_STUFE_2A |
| 2552 | Cobblestone square | ZIERDE | 2×2 | 40 | 0 | 0 | 4 | 0 | 0 | 0 | 16 | 0 | INFRA_STUFE_3A |
| 2553 | Monument | DENKMAL | 2×2 | 0 | 0 | 0 | 0 | 0 | 0 | 0 | 16 | 3000 | INFRA_STUFE_DENKMAL |
| 2554 | Obelisk | ZIERDE | 2×2 | 1000 | 3 | 0 | 5 | 0 | 0 | 0 | 0 | 5000 | INFRA_STUFE_5A |
| 2555 | Ornamental well | ZIERDE | 2×2 | 1000 | 5 | 0 | 10 | 0 | 0 | 0 | 20 | 3000 | INFRA_STUFE_5A |
| 2564 | Flower bed | ZIERDE | 2×2 | 20 | 0 | 0 | 0 | 0 | 0 | 0 | 0 | 0 | INFRA_STUFE_4E |
| 2566 | Ornamental square | ZIERDE | 1×1 | 10 | 0 | 0 | 1 | 0 | 0 | 0 | 0 | 0 | INFRA_STUFE_4E |
| 2567 | Ornamental square | ZIERDE | 1×1 | 10 | 0 | 0 | 1 | 0 | 0 | 0 | 0 | 0 | INFRA_STUFE_4E |
| 2568 | Ornamental square | ZIERDE | 1×1 | 10 | 0 | 0 | 1 | 0 | 0 | 0 | 0 | 0 | INFRA_STUFE_4E |
| 2569 | Ornamental square | ZIERDE | 1×1 | 10 | 0 | 0 | 1 | 0 | 0 | 0 | 0 | 0 | INFRA_STUFE_4E |
| 2780 | Triumphal arch | TRIUMPH | 5×1 | 0 | 0 | 0 | 0 | 0 | 0 | 0 | 0 | 8000 | INFRA_STUFE_TRIUMPH |

The playbook's monuments row (ids 1388–1391: gold 1,500–3,000, tools 5–15, wood 0–5, bricks 15–30, marble 10–15, running 2–5) is the range of these four `DENKMAL` rows. P, and it agrees.

The other 36 entries have no `ANLAGEN` name. 34 of them name `INFRA_EVER`. Two have no `INFRAMIN` record: 1394 (`ZIERDE`, 1×1, all costs 0, HP 0) and 3713 (`DENKMAL`). Ids: 1394, 3204, 3207, 3211, 3212, 3307, 3310, 3312, 3408, 3414, 3502, 3503, 3504, 3507, 3512, 3605, 3607, 3611, 3612, 3708, 3713, 3858, 3859, 3860, 3861, 3866, 3868, 3869, 3870, 3871, 3959, 3963, 3970, 3985, 3986, 3987. Ids 3207 and 3312 are the scenario monuments T23 already counted (not re-counted here).

## 3. The stages they name

`BGruppen.dat` `[BAUSTUFEN]` has 31 `Stufe` lines, indices 0–30 in file order (P; the same count is in [`data-text.md`](../formats/data-text.md)). The line is `Stufe: name, group, count`. Group names in `[BGRUPPE]` order: 0 `PIONIER`, 1 `SIEDLER`, 2 `BUERGER`, 3 `KAUFLEUTE`, 4 `ARISTOKRATEN`. [`population.md`](population.md) §7 already says the city check is residents of that group **or higher** against the count, every 2500 ticks, bit `i` of city mask `+0x618` (D, not re-read here). The five `Wissen` lines are stages `2B` 20, `2E` 25, `3B` 50, `3D` 70, `4C` 90. None of them is `DENKMAL` or `TRIUMPH`. [`population.md`](population.md) §1 says the `Wissen` integer's use is untraced.

Stages used by the 19 named types, with the file's line index:

| Index | Name | Group | Count | Used by |
|---:|---|---|---:|---|
| 4 | INFRA_STUFE_2A | 1 settlers | 25 | 2550, 2551 |
| 11 | INFRA_STUFE_3A | 2 citizens | 120 | 2552 |
| 14 | INFRA_STUFE_3D | 2 citizens | 600 | 1393 |
| 15 | INFRA_STUFE_3E | 2 citizens | 800 | 1391 |
| 16 | INFRA_STUFE_3F | 2 citizens | 1100 | 1395 |
| 17 | INFRA_STUFE_4A | 3 merchants | 250 | 1389 |
| 18 | INFRA_STUFE_4B | 3 merchants | 500 | 1390 |
| 20 | INFRA_STUFE_4D | 3 merchants | 1100 | 1388 |
| 21 | INFRA_STUFE_4E | 3 merchants | 1500 | 2564, 2566–2569 |
| 23 | INFRA_STUFE_5A | 4 aristocrats | 500 | 1387, 2554, 2555 |
| 29 | INFRA_STUFE_DENKMAL | 4 aristocrats | 65000 | 2553 |
| 30 | INFRA_STUFE_TRIUMPH | 4 aristocrats | 65000 | 2780 |

The file's last two lines are `INFRA_STUFE_DENKMAL, 4, 65000` and `INFRA_STUFE_TRIUMPH, 4, 65000`. There is no `Stufe` line for `INFRA_STUFE_FORSCH`. 65000 fits the u16 count field ([`population.md`](population.md) §1). The documented check would set the bit when aristocrats-or-better reach 65000.

The bit number is the name's slot in the code's stage-name table, not the file line index. [`population.md`](population.md) §1 summarises that table as 0 `INFRA_EVER` … 30 `INFRA_STUFE_DENKMAL`, 31 `INFRA_STUFE_FORSCH`, and does not mention `TRIUMPH`. The file order above puts `DENKMAL` at line 29 and `TRIUMPH` at line 30. T31.2 reads the name table (`AnnoGame` `0x10013d60`, table `0x10024bd0`) before either number is used as a bit.

## 4. Texts

`anno-dump texte --full`, English GOG `Texte.dat`, 118 sections. Positional sections are cited by 0-based index inside the section. Names and the short status phrases are quoted; the rest is paraphrased.

- `ANLAGEN` ids as in §2. The four paid monuments: 1388 "Hall of Generals", 1389 "Lions' portal", 1390 "Equestrian statue", 1391 "Fountain of Venus".
- `SPEECHGROUP` index 1: "Reward buildings" (index 0 is "Catastrophe").
- `TOOLTIP:BAUMENU_EX` index 7 and `TOOLTIP:BAUMENUTAB` index 7: the category name "Ornamentals".
- `TOOLTIP:BAUMENU_WIGWAMS_ADDON` (30 lines). Index 2 "Monument", 6 "Triumphal arch", 10 "Obelisk", 14 "Ornamental well", 29 "Palace gate" are bare labels. These carry a resident line: 18 Fountain of Venus + Citizens, 20 Flower bed + Merchants, 21 Lions' portal + Merchants, 22 Equestrian statue + Merchants, 24 Ornamental garden + Citizens, 25 "Decorative Flag" + Citizens, 26 Hall of Generals + Merchants. The resident word is the stage's group. The count is the runtime `%d0`, not a number in the string. The base-game section `TOOLTIP:BAUMENU_WIGWAMS` (20 lines) has "Statue" where the add-on has "Monument", and "Triumphal arch", "Obelisk", "Ornamental fountain", "Flower bed", with no `%d0` on any line.
- `TOOLTIP:STATUSBAR`: index 32 "Monument can be built!", 33 "Triumphal arch can be built!", 97 "The population gives you a monument as a present!", 249 "The monument has been researched!" (the previous line is the general "a new building has been researched").

The research sentence has no `FORSCHTYPE` behind it on these 55 types (§1). Who shows it is open for T31.2.

## 5. Help and the manual

Help pages (English HTML under `game/app/help/`, paraphrased):

- `B_Belohnungsbauten.htm`: reaching certain target levels rewards a building. It can then be chosen in the ornamental submenu and built once. Meeting the requirement again allows building it again.
- `Denkmal.htm`: the monument is a reward building, a symbol of success, and it beautifies the city. The page gives no duration, no count and no satisfaction rule.
- `Triumphbogen.htm`: the triumphal arch becomes available after totally defeating an enemy, and only if the capital has reached at least Citizens. Defeating a culture does not count.
- `Schloss.htm`: the palace is a reward building, available from a number of aristocrats. From 1,000: the main section and four more. After 3,000: four further sections. At 5,000: an unlimited number of further sections. Parks improve the grounds. The page says "your city".
- `Zieranlagen_und_Prunkbauten.htm`: the ornamental category is installations, statues and buildings that beautify the city, and reward buildings are found there.

Manual, printed page 31 (PDF page 29, header "Besiedeln 31"; the index on PDF page 73 also lists Zieranlagen at 31): the ornamental category holds what beautifies the city, and it also unlocks buildings the player may build as a reward for a well-run game. The example is the palace, built piece by piece. The paragraph does not name the monument, the arch, a duration, or a satisfaction timer.

Manual, printed page 12 (PDF page 10, header "Bedienung 12"): the pickaxe demolishes, the demolish cannot be undone, and a demolished building or object is rebuilt at its cost. That is the general rule, not a monument rule.

## 6. Buildings the texts group with rewards, outside the 55

Checked so the claims below are not mixed into the `DENKMAL` block (P, same `HEAD` read):

- Palace pieces are `PRODTYPE` `SCHLOSS`: 1380 `INFRA_STUFE_5C`, 1381, 1382, 1383, 1385 and 1392 `INFRA_STUFE_5D`, 1384 `INFRA_STUFE_5E`. Id 1380's gold u16 is 15000. File indices: 5C = 25, count 1000 aristocrats; 5D = 26, count 1500; 5E = 27, count 3000; 5F = 28, count 5000. The help's "main plus four at 1000, four more at 3000" is not how these seven `INFRAMIN` values are split.
- Hedges, fences and trees (2556–2563, 2565, 2672, 2673, 2676, 2677, 2778, 2779) are `PRODTYPE` `ZAUN` with `INFRA_STUFE_4E`.
- The cathedral id 1330 is `PRODTYPE` `KIRCHE` with `INFRA_STUFE_5B`. The 5B count was not copied out.

## 7. Claims

Each claim is what a source says. The files column is §2–§6. The code is T31.2; this column stays empty on purpose.

| # | Claim | Source | What the files show |
|---|---|---|---|
| C1 | A monument is a decoration a player earns, and it beautifies the city. | Owner, via OQ-166. H `Denkmal.htm`, `Zieranlagen_und_Prunkbauten.htm`. M p. 31 (the reward example there is the palace). Status text index 97: the population gives a monument as a present. | 2553 is the only `DENKMAL` with gold 0 and stage `INFRA_STUFE_DENKMAL`. 1388–1391 are paid `DENKMAL` on ordinary resident stages. |
| C2 | It is earned for a threshold in a running game, and endless games are included. | Owner, via OQ-166. | The 19 named types each name a resident stage (§3). No file read here names a game mode or an endless game. |
| C3 | Example: the population satisfied for 30 minutes in a row. | Owner, via OQ-166, given as an example and not confirmed. | No duration and no satisfaction timer in `BGruppen.dat`, the help pages in §5, or M p. 31. |
| C4 | Statues are awarded for good management. The writer's guess: keep citizens or higher happy for 30 minutes, as in 1602, and each large city may offer its own statue at about the same time. | [Tim Howgego's FAQ, colony buildings](https://anno-capsu.netlify.app/1503/colony_buildings), the 30 minutes and the per-city sentence are in brackets as the writer's own guess. | Same gap as C3. The per-city part is the city mask in [`population.md`](population.md) §7, which T31.2 has to connect to a monument. |
| C5 | A built monument counts toward the satisfaction part of the final score. Seeing the award film without building does not. | Same FAQ, quoting two players (Ravell, Balou). | No score field in these type records. Open for the code. |
| C6 | The monument is given by a city's inhabitants for a longer uninterrupted satisfaction. The triumphal arch is unlocked for destroying an opponent. The table's asterisk on the satisfaction prerequisite is not expanded on the fetched page. | [AnnoZone, Zieranlagen](https://www.annozone.de/wiki-anno1503/record/272-zieranlagen/) (page dated 2026-04-13). | 2553 and 2780 match that page's size and zero cost (2×2 and 5×1). The page gives no minute count. |
| C7 | A new triumphal arch replaces an earlier one that was not placed yet. They cannot be stockpiled. | [AnnoZone forum thread 1810](https://www.annozone.de/forum/thread/1810-triumphboegen/). | Not in the type data. |
| C8 | The arch comes after totally defeating an enemy, capital at least Citizens, and a culture does not count. | H `Triumphbogen.htm`. The FAQ states the defeat part in one sentence. | 2780's stage count is 65000 aristocrats, which is not "capital at Citizens". The Citizens test is not this `Stufe` line. |
| C9 | Reward buildings are built once per achievement, and again when it is reached again. | H `B_Belohnungsbauten.htm`. | No per-type "once" flag in `HEAD`. The flag word is `0x0084` on all 55 (§1). |
| C10 | Demolish cannot be undone, and rebuilding pays the cost again. | M p. 12, general, not monument-specific. | 2553 and 2780 have gold 0 and goods 0, so a rebuild of those two would be free if the general rule applies. Whether they can be demolished is the code. |
| C11 | The four paid monuments unlock at citizen or merchant counts: Venus 800 citizens, lions 250 merchants, equestrian statue 500 merchants, hall 1100 merchants. A monument is offered at the citizen stage. | Addon menu strings §4. [FAQ building table](https://anno-capsu.netlify.app/1503/building_industry_data) (the `^^` mark means the add-on). [`tutorial-videos.md`](../research/tutorial-videos.md): V10 09:32, probably 1391. | The four `INFRAMIN` counts in §3 are those numbers. The menu's group word matches the stage group. |
| C12 | Obelisk, ornamental well and palace gate unlock at 500 aristocrats. Flower bed, ornamental squares, hedges and trees at 1500 merchants. The FAQ's "Rewards" row also lists ornamental fence, squares and trees next to Monument and the arch. | FAQ building table. AnnoZone table. | 2554, 2555, 1387 are `INFRA_STUFE_5A` (500). 2564 and 2566–2569 are `INFRA_STUFE_4E` (1500). Hedges and trees are `ZAUN`, same stage (§6), not the `DENKMAL` stage. Their menu labels in §4 have no `%d0`. |
| C13 | Costs and sizes of the named ornamentals. | Playbook monuments row. AnnoZone table (tools, wood, bricks, marble, gold, running cost, size). | §2 matches the playbook range and the AnnoZone rows for 1388–1391, 1393, 1395, 2553, 2554, 2555, 2780. AnnoZone's palace main gold of 10000 does not match id 1380's gold 15000 (playbook also 15,000). |
| C14 | "The monument has been researched." | `TOOLTIP:STATUSBAR` index 249. | `FORSCHTYPE` is empty on all 55, and no `Wissen` line names `DENKMAL` (§3). |
| C15 | Palace: from 1000 aristocrats the main part plus four, four more at 3000, no limit at 5000, and the 5000 must be on one island. At least 1900 residents as well. Once built, palace and cathedral stay if the aristocrats leave. After a save and load, unlimited palace parts need the aristocrats again. The unlimited count is really 200. Cathedral once per game: destroy it and it cannot be rebuilt. | H `Schloss.htm` for 1000 / 3000 / 5000. The island, the 1900, the save/load and the 200 are the [FAQ](https://anno-capsu.netlify.app/1503/colony_buildings). | Palace `INFRAMIN` values are 5C, 5D and 5E per piece (§6), not one stage for the first four pieces. 5F (5000) is not the `INFRAMIN` of the seven palace ids checked. Cathedral 1330 is `KIRCHE`. |
| C16 | A scenario's monument is created on the scenario path and was left uncreated by T26. | T23, T25 (j), OQ-144, OQ-166. | 3207 and 3312 are unnamed `INFRA_EVER` entries in the 36 (§2). Whether that creation counts as "earned" is the code. |

## 8. Open for the code pass (T31.2)

- Who sets the `INFRA_STUFE_DENKMAL` and `INFRA_STUFE_TRIUMPH` bits, and which index the name table gives them (§3).
- Whether any timer of 30 minutes, or any satisfaction streak, exists (C3, C4, C6).
- What the award writes: a per-player count, a per-city bit, a message, a build-menu entry; and whether that is saved (C2, C4, C7, C9).
- The building: class, tick, effect, demolition, save. T25 (j) already has the scenario-path base class. C5 and C10 need the interactive object.
- Status text index 249 (C14) and flag bits 2 and 7.
- The owner's three clauses, which are C1, C2 and C3, answered from the code.
