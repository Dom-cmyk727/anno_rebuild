# Game files, second survey: what the shipped files still hold for the reverse engineering

**Status:** 2026-10-22 (orchestrator). The first survey is [`game-files-analysis.md`](game-files-analysis.md) (2026-10-16, inventory and container format). This one goes through every directory of GOG 2.0.0.5 again and lists what was not used yet. Evidence: P = read from the files, D = disassembly, S = strings in the binaries.

## 1. Headline findings

1. **The scenarios carry a scripting language.** The `BIGBROTHER` chunk of every `.sze` is a **plain-text script**, not a statistics record (the glossary said "statistics recording"; corrected). 39 of 50 scenarios have a script body; the longest is 29 KB (`single30.sze`). The interpreter lives in `AnnoScene.dll` (`AnnoBigBrother`, `SCRIPT_KERNEL`) and reports parse errors in German (`"Kein THEN bei Bedingung: %d"`, `"%s ist unbekannter Disastertyp bei Bedingung: %d"`). §2.
2. **The AI profiles are data.** `COMPPROFIL2` holds one 168-byte `ENTRY` per computer player: 19 `u32` parameters, a name (`Hagen V. Sayn`, `Padre Boileau`, …) and the profile page (`Profil01.htm` … `Profil12.htm`). The parameter names are in `AnnoScene.dll`: `COMPCAPS_TYP`, `COMPCAPS_HANDEL`, `COMPCAPS_KAMPFTYP`, `COMPCAPS_LAGER`, `COMPCAPS_MAXSTADT`, `COMPCAPS_MINBAUMAT`, `COMPCAPS_MINEISEN`, `COMPCAPS_PIRATEATTACKALL`, `COMPCAPS_PROZENTE`, `COMPCAPS_SUSPEND`, `COMPCAPS_TRANSPORT`, `COMPCAPS_ZIELGRUPPE`, `COMPCAPS_BAULOGICOFF`, `COMPCAPS_ADDDIPLOMATIE`, `COMPCAPS_FIXDIPLOMATIE`, `COMPCAPS_SETHELPANLAGE`. These are the script's (`SET_PROFILE`) names for numbered AI parameters. T18 mapped the columns to these parameters (D, `annoscene.md` §11). The twelve character sketches are `Profiles/Texte/Profil01..12.htm` ("The Trader", "The Warrior", "The Reckless One", …). §3.
3. **Four small `SCENE` chunks are decoded** (all 50 files, P): `WARETYPE` = the goods id table; `LIMITS` = the unit caps (ships, soldiers); `SKTIMER` = 10 script timers; `MONOPOL(2)` = 28 (goods, owner) pairs; `AUFTRAGTEXT` = the mission page file name. §4.
4. **The in-game help** (`help/*.htm`, 582 pages) was read in [`manual-and-help.md`](manual-and-help.md).
5. **The game writes derived files next to itself:** `Scenes/Scenes.cach` and `Scenes/Islands/InselCache.txt` exist in the running copy `C:\anno1503-original` but not in the pristine install; `AnnoGame.dll` also knows `Anlagen.sav`/`Anlagen.ver` and `Figuren.sav`/`Figuren.ver` (S), not seen yet. The game copy of a runner must stay writable (T16).
6. **Loose ends in the binaries** (S): a debug overlay (`[AddOn]Framerate: %3.2f …`, `Pos: %d, %d, (FIG: %d)`, `Screen%03d.tga`), the registry key `SOFTWARE\GAME_ANNO1503` (`1503Startup.exe`), launcher switches `/nl` and `/ml` (skip the intro videos? `AnnoFrame.dll`, next to `Videos\1000.bik` and `intro.ddf`), a missing `data\banner.dat`, and `Lastgame-ServerDown.sav`, written when a multiplayer server drops.

## 2. The scenario script (`BIGBROTHER`)

> Superseded in detail by [`../formats/bigbrother.md`](../formats/bigbrother.md) (T17, 2026-10-22): the grammar, all 164 script names classified, and §7 there corrects this section (37 scripts have statements, not 39; `SOLDAT_*`/`SCHIFF_ATTACK` are the combat tutorial's object names; `HELPPLAYER`/`VOLUME` are profile chunk names; `CHEATLIMIT`/`SETTRIBUTSPERRE` are `COMP` keys).

### 2.1 Shape (P, 50 files)
```
[OBJECT_DEFINE]        OBJECT_FIGUR "TOCHTER" 7          (also OBJECT_ANLAGE, OBJECT_STADT)
[AREA_DEFINE]          "BIG_WEST" 0, 915, 1025, 100      (x0, y0, x1, y1 in tiles)
[PLAYERMAPPER_DEFINE]  "HUMAN" 0  "VENEZIANER" 1 … "DUMMYPLAYER" 31
[SCRIPT_KERNEL]
  $ comment
  INITBLOCK() { … }                       runs once
  SET PART1 = TRUE;                       script variables
  IF <cond> && <cond> THEN { <actions> }  re-evaluated every step
[SIGNALS]              PART1 = TRUE …     the variables' saved state
[CAMARAPOS]            0,0
```
The engine steps once before every simulation advance (`tick-loop.md` §2, D). The `[SIGNALS]` section is the script's state, so a savegame keeps it.

### 2.2 Vocabulary (S, `AnnoScene.dll`; P, usage counts over all scripts)
- **Conditions:** `PLAYER "X" INHABITANTCOUNT "SIEDLER" > n`, `INHABITANTMOOD`, `INHABITANT_AMOUNT`, `WARECOUNT`, `MONEY`, `FINANCE_INCOME`, `FINANCE_EXPANSES`, `MAKES_PROFIT`/`MAKES_LOSS`, `BOUGHT "A@WARE"`/`SOLD`/`BUYS`/`SELLS`, `PAID_TRIBUTE_TO`, `WAR_STATUS`, `HANDELSPAKT_STATUS`, `KRIEGSPAKT_STATUS`, `SHIPCNT`, `SHIPCNT_FROM`, `SOLDIERCOUNT`, `SOLDIERCNT_FROM`, `SOLDATCOUNT_BELAGERUNGSMASCHINE`, `CONSTRUCTIONCOUNT`, `HAS_CONSTRUCTION`, `CAN_CONSTRUCTION`, `MONUMENTCOUNT`, `PLAYTIME`, `PLAYERKILLED`, `SHIPKILLED`/`SHIPLOST`, `SOLDIERKILLED`/`SOLDIERLOST`; `OBJECT "X" KILLED`/`DEAD`/`HEALTH`/`OWNER`/`MOOD`; `AREA "A" TOUCHED_BY_SHIP_FROM "P"`, `TOUCHED_BY_SOLDIER(S)_FROM`, `TOUCHED_BY_OBJECT`, `TOUCHED_BY_CONSTRUCTION`, `TOUCHED_BY_ANYCITY`/`ANYSHIP`/`ANYSOLDIER`, `CONTAINS "WARE|UNIT" > n`; `TIMER_0..4`; `SELECTED_CURSORCONSTRUCTION`.
- **Actions** (uses in the 50 files): `QUESTLOG_SHOW` 669, `DISASTER_AT` 230, `SET_UNIT_VISIBLE` 117, `PLAYVIDEO` 83, `SHOWTEXT` 72, `LOST` 55, `ASSIGN_OBJECT` 53, `QUESTLOG_DRAW` 41, `WON` 35, `SET_DIPLOMACY` 30, `ATTACK_AREA` 27, `SETCONSTRUCTION_DRAWSTATE` 25, `TIMER_SET/START/STOP`, `MINIMAP_ALERT` 23, `SET_PROFILE` 22, `PLAYWAVE` 18, `WAKEUP` 17, `GOTO_SCREEN` 14, `FREEZE_CITY` 10, `MONEY_ADD` 8, `MOVE_UNIT` 8, `SET_PIONIER_ITEM` 7, `ATTACK_OBJECT` 6, `SETWARELOG` 5, `SET_FOR_BUY` 5, `ASSIGN_ALL_SOLDIERS` 4, `SETTRIBUTE` 2, `CHEAT` 2, `ADD_SCORE` 2, `ADD_HEALTH` 2, `BEAMUP`/`BEAMDOWN` 1, `KILLPLAYER` 1, `SHOWMESSAGE` 1. Also known to the interpreter but unused: `MONEY_SET`, `SUB_HEALTH`, `ADD_DIPLOMACY`, `SET_REBELLION`, `MONOPOLY_ON`, `UPGRADE_ENABLE`, `SETTRIBUTSPERRE`, `SET_FOR_SELL`, `ASSIGN_ALL_SHIPS`, `SCHIFF_ATTACK`, `CAMERA_MEMORY`/`CAMERA_RECALL`, `CHEATLIMIT`, `LOAD`, `SAVE`, `HELPPLAYER`, `STARTSKILL`, `VOLUME`.
- **Disaster types** (`DISASTER_AT`): `FIRE`, `PLAGUE`, `DROUGHT`, `EARTHQUAKE`, `VULCAN`. The scripts call `DISASTER_AT` 230 times, mostly `FIRE` (158) and `VULCAN` (70). This is the caller of the scripted-disaster entry points that O-51 found (`AnnoScene` `0x10013870`, `0x10013a1a`).
- **Unit names** used by the script: `SOLDAT_SCHWERT`, `SOLDAT_BOGEN`, `SOLDAT_KANONE`, `SOLDAT_SANI`, `SOLDAT_PINOIER`, `SOLDAT_VICTIM_1..5`, `SOLDAT_WOUNDED`/`WOUNDER`, `LANZENTRAEGER_LEICHT`/`SCHWER`, `KANONEN`, `MUSKETEN`, …; these are the names the engine maps to figure types.

### 2.3 Why it matters
- **Faithful scenarios need the interpreter.** Every campaign and single mission is this script plus the map. PLAN M1 lists the `.sze` format; the script engine is a port target of its own (`AnnoScene`, O-16/O-34 in `docs/sim/state.md`), and its 50 shipped scripts are the test corpus.
- **The conditions name the sim's query surface:** inhabitants per level, mood, money, income and expenses, trade log per partner and good (`SETWARELOG`/`BOUGHT`), treaty status, kills, area touch tests. Whatever the sim exposes for the script must be deterministic and must exist in the state model.
- **`MAKES_PROFIT`/`FINANCE_*`** read the player balance that `population.md` §8 traces.
- **Determinism:** the script runs on the net client before each advance (`AnnoFrame` `0x1005fe10`). `PLAYVIDEO`, `GOTO_SCREEN` and `MINIMAP_ALERT` are UI; `DISASTER_AT`, `MONEY_ADD`, `SET_DIPLOMACY`, `ATTACK_AREA`, `KILLPLAYER` mutate the sim. In lockstep they must become commands (the add-on multiplayer scenarios in `0500MultiEndlos` / `0700Multi` ship without `.sze` files in this build, so the question is open).

## 3. Computer players (`COMPPROFIL2`, `Profiles/Texte`, `CompLogic.dll`)

- **`COMPPROFIL2` layout (P, 50 files; D for the meaning, T18):** tagged records. 41 files hold only one `DEF` with all 32 entries −1. The 9 endless/add-on files hold 12 or 4 `ENTRY` records (168 bytes each) and then the `DEF`. `DEF` is 32 `i32`, one per player index: the `ENTRY` used for that player, −1 = none (e.g. `01_1024_easy_256`: −1, 3, 5, 2, −1 … = players 1–3 get `ENTRY` 3, 5 and 2, counted from 0, which are Profil01, Profil03 and Profil04). The full layout is in [`../formats/sze.md`](../formats/sze.md) "AI profiles".
- **`ENTRY`:** 19 four-byte columns (column 0 holds one byte and three zero bytes), a 16-byte GUID, the name (25 bytes) and the profile page (51 bytes). The two strings are followed by `0xCC` fill, an MSVC debug-heap pattern, so the editor likely saved uninitialised memory. Columns are numbered 0–18 here.
  - *How it is used:* the game itself never loads the chunk. The single-player setup screen (`AnnoFrame.dll` `0x1001e5d0`) reads it from the scenario file, fills up to 7 opponent slots in `DEF` order and lets the player page through the profiles. At game start (`0x10028d90`) each computer player (type 12, colour still unassigned) takes the next slot, and 15 of the columns go to that player's AI object in `CompLogic.dll` as numbered parameters.
  - *Keys:* the AI's `COMP` save keys and the script's `COMPCAPS_*` names are names for the same parameter numbers (`annoscene.md` §11.5).
  - *Mapping (D):* column 1 `TYP`, 2 `FLAECHE`, 3 `LAGER`, 4 `HANDEL`, 5 `TRANSPORT`, 6 `PROZENTE`, 8 `TRIBUTSPERRE`, 9 `KAMPFTYP`, 10 `HAUPTKLIMA`, 11 `MAXSTADT`, 12 `ZIELGRUPPE`, 16 `MINBAUMAT`, 17 `MINEISEN`, 18 `SUSPEND`. Column 13 is passed as `SUSPEND` a second time (an index slip for `CHEATLIMIT` is a guess). Columns 0, 7, 14 and 15 are not used (OQ-112).
  - *Correction:* the earlier "column 7 (18–25)" and "column 16 (60 or 3)" were counted from 1. They are columns 6 (`PROZENTE`) and 15 (unknown).
- **The twelve shipped profiles** (P, read with a scratch parser over all 50 files; each profile is byte-identical in every file that holds it). Columns with the same value in all twelve: 0 = 1, 2 `FLAECHE` = 0, 4 `HANDEL` = 3, 7 = 0, 8 `TRIBUTSPERRE` = 0, 10 `HAUPTKLIMA` = 3, 12 `ZIELGRUPPE` = 3, 13 = 14 = 0, 16 `MINBAUMAT` = 0, 17 `MINEISEN` = 0, 18 `SUSPEND` = 0. The archetype is the first paragraph of the table on the profile page (`Profiles/Texte/Profil<nn>.htm`, English); the page's HTML title is a placeholder.

| Page | Archetype | Name | `TYP` (1) | `LAGER` (3) | `TRANSPORT` (5) | `PROZENTE` (6) | `KAMPFTYP` (9) | `MAXSTADT` (11) | col 15 | Used for (file: player) |
|---|---|---|---|---|---|---|---|---|---|---|
| Profil01 | The Trader | Hagen V. Sayn | 0 | 3 | 3 | 25 | 0 | 4 | 3 | `01`: 1, `02`: 1 |
| Profil02 | The Reluctant One | Padre Boileau | 1 | 3 | 3 | 23 | 1 | 4 | 3 | `02`: 2, `03`: 3 |
| Profil03 | The Upstart | Herzog Albrecht | 2 | 3 | 3 | 22 | 5 | 5 | 60 | `01`: 2, `02`: 3, `04`: 1, `05`: 3, `06`: 3 |
| Profil04 | The Just One | Duke O Leech | 3 | 2 | 3 | 20 | 7 | 5 | 60 | `01`: 3, `08`: 3 |
| Profil05 | The Introvert | V. Neidhardt | 4 | 2 | 1 | 18 | 4 | 6 | 3 | `04`: 2, `05`: 1 |
| Profil06 | The Timid One | De La Rouke | 5 | 2 | 1 | 18 | 3 | 6 | 60 | `03`: 1, `04`: 3, `05`: 2, `07`: 4 |
| Profil07 | The Warrior | Compte Duvoir | 6 | 1 | 1 | 18 | 11 | 7 | 60 | `06`: 2, `07`: 3, `08`: 2, `endless03`: 1 |
| Profil08 | The Tactician | Heinrich | 7 | 1 | 1 | 18 | 15 | 7 | 60 | `06`: 1, `07`: 1, `08`: 1, `endless03`: 10 |
| Profil09 | The Quiet One | Ramirez | 1 | 1 | 1 | 18 | 2 | 5 | 60 | `03`: 4 |
| Profil10 | The Jolly One | Kleinmuth | 3 | 3 | 1 | 20 | 9 | 6 | 60 | `03`: 2 |
| Profil11 | The Resentful One | Maximilian | 5 | 2 | 3 | 22 | 5 | 7 | 60 | `06`: 4, `endless03`: 2 |
| Profil12 | The Reckless One | Niklas V. Salach | 7 | 3 | 3 | 22 | 11 | 5 | 60 | `04`: 4, `07`: 2, `endless03`: 3 |

  `01`…`08` are the `0010SingleEndlos` games (`01_1024_easy_256` … `08_800_heavy_128`). `endless03` is in `0011SingleEndlosAddon` and holds only profiles 07, 08, 11 and 12; the other eight hold all twelve.
  - *Difficulty:* no easy game uses a profile with `KAMPFTYP` above 7. Every heavy game includes The Tactician (15) and The Warrior (11). The Trader, whose page says he "detests war", is the only profile with `KAMPFTYP` 0 (P).
  - *Names:* the player names in the scenarios' `HEAD` equal the name of the profile `DEF` gives them in some files and not in others. `02` and `05` match, apart from the spelling `De La Rourke` vs. `De La Rouke`; `04` matches for players 1–3, while player 4 is named `Duke O'Leech` and gets Niklas V. Salach's profile; `01` names its players `Admiral Schmidt`, `Fürst Heinrich`, `Hilde v. Drachenfels`. The hand-over does not pass the name (D, `annoscene.md` §11.4).
  - *Column 15:* three profiles (01, 02, 05) have 3, the others 60.
- **Twelve archetypes** (`Profiles/Texte/Profil01..12.htm`, English): Trader, Reluctant One, Upstart, Just One, Introvert, Timid One, Warrior, Tactician, Quiet One, Jolly One, Resentful One, Reckless One. Each endless game draws its opponents from these twelve.
- **`CompLogic.dll` savegame keys** (S): `BAURECORDPOS`, `DIPLPLAYER`, `FIXHAUSANZ`, `NATIVEANLAGE(N)`, `NATIVEBAUMODE`, `NATIVESTADTID`, `LOGWARE`, `EKVKWARE`, `SPEZFLAG`, `STADTOFF`, `TRIBUTSPERRE`, `HAUPTKLIMA`, `KAMPFTYP`, `LAGER`, `HANDEL`, `MAXSTADT`, `MINBAUMAT`, `MINEISEN`, `PROZENTE`, `SUSPEND`, `TRANSPORT`, `ZIELANZAHL`, `ZIELGRUPPE`, `FLAECHE`, `AREA`, `OPTIONAL`, `CHEATLIMIT`. These are the AI's persistent state and its tunables; `CompLogicClassic.dll` has the same set minus a few. Relevant to O-1 (the AI tick) and OQ-108. They are the `KEY=value` strings of each player's `SPIELER/COMP` record (decoded in `sze.md`, T18). The tunables are the numbered parameters of `annoscene.md` §11.5. The scenario editor wrote one of them as `SETTRIBUTSPERRE`, which the AI does not recognise (OQ-114).

## 4. The small `SCENE` chunks (P, all 50 files)

| Chunk | Layout | Content |
|---|---|---|
| `WARETYPE` (543 B, identical in all files) | 56 × (`u16` id, NUL-terminated name) | the goods table in id order: 0 `UNUSED`, 1 `EISENERZ`, 2 `GOLD`, 3 `SALZSTEIN`, … 53 `KANONESHIP`, 54–55 `UNUSED`. Equals the `AnnoGame` goods table (`production.md` §7), so a scenario carries the goods numbering it was saved with. |
| `LIMITS` (4 B) | `u16` ships, `u16` soldiers | the unit caps: 40 / 100 in the endless games, 20 / 100 in most missions, 2 / 100 and 4 / 10 in two campaign missions, up to 50 / 400 (`0x190`) in the add-on endless games. Confirms ISS-061 ("ground 100, ships 40, per scenario"). |
| `SKTIMER` (40 B) | 10 × `u32` | the script timers `TIMER_0..4` with their state; all zero except one mission (`0x79b` = 1947 in slot 2). Matches `ScriptTimers` / O-31. |
| `MONOPOL` / `MONOPOL2` (224 B) | 28 × (`u32` count, `i32` owner) | the monopoly table of `MonopolyTable` (O-24): 20 raw materials + 8 others; all `(0, −1)` in the shipped files. |
| `AUFTRAGTEXT` (19–27 B) | NUL-terminated file name | the mission page in the scenario folder (`1024 easy 128_91001.htm`, `auftragsingle_30000.htm`). |
| `K_DATA` (172 B) | ? | campaign files only; unread. |

## 5. Other directories

- **`Scenes/*/` side files:** `.htm` mission and quest-log texts (keys like `SZENARIO01_100010`, `CAMPAIGN_01_01_02`, used by `QUESTLOG_SHOW`), `.ddf` slide shows (`<IMAGES>`, `<IMAGEPATH>`, `<IMAGETIME>`, `<BLENDTIME>`, `<BLENDFLAGS> SMOOTH`, `<SOUND>`; played by `PLAYVIDEO("…DDF")`), `.jpg` slides and `.wav` narration; `Scenes/Tutorial/Texte` has 116 tutorial step pages. The 0010 endless folder's nine `.htm` files describe the nine endless settings (islands, ore, fertility, pirates, money).
- **`Scenes/Islands/Nord/N_256x256_0010_1B_Mongolen.scp.bug`** (2003): an island file without a `.scp` twin, left over from a bug report. Not loaded by name (`AnnoScene.dll` lists only `.SCP` names).
- **`Data/`**: `Debug.dat` is a Bink video (15 MB; a developer test clip); `Textures/debug.tex` a 4 KB debug texture; `ControlChars.bat`/`ScapeChars.bat` are the font build commands (`konvertchars verdana -h11 -w6 -s1`), which tells us the `.zei` fonts are Verdana 11 and 16 px.
- **`Speech/`** 1,277 numbered `.wav` (recount 2026-10-24, `analysis-2026-10-24.md`; an earlier count said 1,276) (the speech ids of `[SPEECHGROUP]`), **`Music/`** mood folders (`depressed`, `desperate`, `discover`, `events`, …) chosen by `AnnoMusicEvent` in `music.dat`, **`Samples/`** 364 effect files named by source (`Bergwerk1.wav`, `Axt Hit Ruestung.wav`), **`Videos/`** 10 Bink intros and menus.
- **`help/`**: see `manual-and-help.md`. Leftovers: `COMPHELP_KEINEFORSCHUNG=HS_TEST_Wilfried_24.07.doc` in a page (the help was generated from Word documents) and a German author's note "Noch nicht sicher" on the watchtower page.
- **Runtime leftovers in the game dir** (not in the install): `Scenes/Scenes.cach` (20 KB), `Scenes/Islands/InselCache.txt` (117 KB), `SaveGame/<profile>/Lastgame.sav` and `user.dat`.

## 6. What to do with it

1. **Spec the script** (new `docs/formats/bigbrother.md`): grammar from the 50 files, the vocabulary above, the `[SIGNALS]` state. A Python reference that parses all 50 is the oracle (P); the engine's error messages (S) give the syntax checks for free.
2. ~~**Read the `COMPPROFIL2` loader** in `AnnoScene.dll` to name the 19 columns with the `COMPCAPS_*` keys, then write `docs/formats/sze.md` §COMPPROFIL2.~~ Done in T18. The loader is in `AnnoFrame.dll` (the opponent setup), not in `AnnoScene.dll`. Columns are mapped in `sze.md` "AI profiles", and the hand-over to `CompLogic.dll` is in `annoscene.md` §11. Four columns reach no setter and one only through a doubtful second call (OQ-112).
3. **Sim query surface:** list the script conditions as the first external API of `anno_sim` (`docs/sim/state.md` O-16/O-34): they are what a faithful scenario needs, nothing more.
4. **T16:** the runner must copy the game dir writable (caches, `SaveGame/`), which the spike already did.
5. **Later:** `K_DATA` (campaign), `SET_PROFILE`'s effect on a running AI, and the `CHEAT` action (two missions use it) as a hint to the cheat command set of `AnnoFrame`.
