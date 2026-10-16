# Glossary

German (and abbreviated) terms from the original game that this project keeps as canonical identifiers. Only terms that actually appear in the original files, chunk names or module names recorded in `docs/PLAN.md` and `docs/research/` are listed.

"Unconfirmed" means the meaning is inferred from the word only and has not been verified against parsed data, disassembly or a community source. Confirming or correcting a meaning is tracked in `docs/open-questions.md`. Update this file whenever a meaning is confirmed.

Sources abbreviated below: GFA = `docs/research/game-files-analysis.md`, CL = `docs/research/community-landscape.md`, PLAN = `docs/PLAN.md`.

## Terms

| Term | Literal English | Meaning in this project | Where it appears |
|---|---|---|---|
| Anlagen (sg. Anlage) | installations, facilities | Buildings and placeable objects; `ANLAGEN` holds 921 ENTRY records. Community classes `TAnlage*` (DatEditor). | `Anlagen.dat` (chunks COLORS, ANLAGEN, TEXTURES); `Texte.dat` section `[ANLAGEN]` (id → name) (GFA §2, PLAN M1.4) |
| Figuren (sg. Figur) | figures | Sprite graphics (people, ships, buildings' graphics per community decoder); data in `Figuren*.dat`, indexed by `FigurenHeader.dat` | `FigurenHeader.dat`, `Figuren*.dat` (16 files) (GFA §2, CL §3) |
| Welt | world | The game world; module believed to hold the simulation core (unconfirmed, inferred from exports/imports) | `AnnoWelt.dll` (exports AnnoWelt, GetAnnoInselFileData, RepairSavegame) (GFA §1) |
| Insel | island | An island: terrain grid plus island-level data | Chunks `INSEL4`, `INSEL5`, `INSELLNK`; export `GetAnnoInselFileData`; `.scp` island templates (GFA §1, §3) |
| INSELLNK | island link | Link/reference from a scenario to island data (unconfirmed) | `.sze` chunk `SCENE/INSELLNK` (GFA §3) |
| Waren (sg. Ware) | goods, wares | Tradeable goods | `BGruppen.dat` (goods/prices per GFA), chunk `WARETYPE`, View.dll goods tooltips (GFA §1–3) |
| WARETYPE | goods type | Goods type definitions/settings in a scenario (unconfirmed) | `.sze` chunk `SCENE/WARETYPE` (GFA §3) |
| BGruppen | "B-groups" | Plain-text file with goods and prices (`Id` blocks per CL). What "B" abbreviates is unconfirmed. | `Data/BGruppen.dat` (GFA §2, CL §3) |
| Farbtabelle (pl. Farbtabellen) | colour table | A 256-colour palette; the directory name in every `Anlagen.dat` `COLORS` entry name (`Farbtabellen\<file>.pcx`), i.e. the palettes were authored as PCX files | `Anlagen.dat` `COLORS/ENTRY` `NAME` (P, `docs/formats/palette.md`) |
| Gelaende (Gelände) | terrain | Per-tile terrain classification of an island (unconfirmed) | Island chunk `GELAENDE` (GFA §3) |
| Wachstum | growth | Population growth data (unconfirmed; PLAN links it with `VermehrTab`) | `Data/Wachstum.dat` (GFA §2) |
| VermehrTab | multiplication/breeding table | Growth table field in `Anlagen.dat` per easy-player-boost patch (unconfirmed for GOG build) | `Anlagen.dat` field cited from community patch (PLAN M1.4) |
| Felder (sg. Feld) | fields, tiles | Ground tiles; `Felder.tex` holds 32×32 ground tiles per drkohler | `ToolGfx/Felder.tex`, `Data/Felder.dat` (GFA §2, PLAN M1.2, CL §2) |
| wegspeed | way/road speed | Movement speed on roads/paths (plain text) | `Data/wegspeed.dat` (GFA §2) |
| Highmap | height map (sic) | Unknown; plain-text data file (unconfirmed) | `Data/Highmap.dat` (GFA §2) |
| Kontor | trading post, warehouse | Island's trading post / warehouse (unconfirmed in data) | Island chunk `KONTORID` (GFA §3) |
| Staedte (Städte) | towns, cities | Town/settlement data per island (unconfirmed) | Island chunk `STAEDTE` (GFA §3) |
| Bergwerk | mine | Mine sites on an island (unconfirmed) | Island chunk `BERGWERK2` (GFA §3) |
| Fruchtbar | fertile | Fertility (which crops grow on an island) (unconfirmed) | Island chunk `FRUCHTBAR` (GFA §3) |
| Anbau | cultivation | Cultivated/planted area or crops (unconfirmed) | Island chunk `ANBAU` (GFA §3) |
| PROD | production | Production-related island data (unconfirmed) | Island chunk `PROD2` (GFA §3) |
| Bewohner / BEWOHNTYPE | inhabitant / inhabitant type | Key in a figure header ENTRY; likely the resident class a figure belongs to (unconfirmed) | `FigurenHeader.dat` ENTRY key `BEWOHNTYPE` (GFA §3) |
| HAEUSER (Häuser) | houses | Key in a figure header ENTRY (unconfirmed) | `FigurenHeader.dat` ENTRY key (PLAN M1.3) |
| Szene / SCENE | scene | A scenario (map + rules + players); also the savegame module | `.sze` chunk `SCENE`; `AnnoScene.dll` (exports AnnoScene, SaveGame, SceneCache, AnnoBigBrother) (GFA §1, §3) |
| Spieler | player | Player definitions in a scenario | `.sze` chunk `SCENE/SPIELER` (GFA §3) |
| Rohstoff | raw material | Raw-material settings in a scenario (unconfirmed) | `.sze` chunk `SCENE/ROHSTOFF` (GFA §3) |
| Schatz | treasure | Treasure placements/settings (unconfirmed) | `.sze` chunk `SCENE/SCHATZ` (GFA §3) |
| Erze (sg. Erz) | ores | Ore deposit settings (unconfirmed) | `.sze` chunk `SCENE/ERZE` (GFA §3) |
| Monopol | monopoly | Monopoly (exclusive goods) settings (unconfirmed) | `.sze` chunk `SCENE/MONOPOL2` (GFA §3) |
| Auftrag | mission, order, task | Mission/assignment text of a scenario (unconfirmed) | `.sze` chunk `SCENE/AUFTRAGTEXT` (GFA §3) |
| COMPPROFIL | computer profile | AI opponent profile (unconfirmed) | `.sze` chunk `SCENE/COMPPROFIL2` (GFA §3) |
| CompLogic | computer logic | AI opponent module; Addon vs. Classic variant | `CompLogic.dll`, `CompLogicClassic.dll` (GFA §1) |
| BIGBROTHER | (English) | Statistics recording | `.sze` chunk `SCENE/BIGBROTHER`; export `AnnoBigBrother` (GFA §1, §3) |
| Async | (short for asynchron) | The original's term for a multiplayer desync | String "Async aufgetreten! …" in `AnnoFrame.dll` (GFA §1) |
| Texte | texts | Plain-text string table, `[SECTION]` + `id:Text` | `Texte.dat` (GFA §2) |
| Endlos | endless | Endless (sandbox) game mode, used in scenario folder names | `Scenes/0010SingleEndlos`, `0011SingleEndlosAddon`, `0500MultiEndlos` (GFA §2) |
| Kampagne | campaign | Campaign scenarios | `Scenes/0100Kampagne01` (GFA §2) |
| Klima | climate | Climate zone of island templates (Polar, Nord, Steppe, Praerie, Dschungel, …) | `Scenes/Islands/<Klima>/*.scp` (GFA §2) |
| ADDON / ANNOADDON1 | add-on | Root chunk marking add-on scenario content | `.sze` root `ANNOADDON1` (27 files) vs. `ANNO` (23) (GFA §2, CL §3) |
| zei | unconfirmed (possibly Zeichen, "characters") | Font file, root `CHARSET` | `*.zei` (11 files) (GFA §2) |

## Chunk names seen so far

Chunk container per `tools/chunks.py` (12-byte name + u32 LE size + payload). Paths are root → child. Counts from GFA §2–3.

| Chunk | Where it occurs |
|---|---|
| `ANNO` | Root of 23 `.sze` scenarios and of all 465 `.scp` islands (first 12 bytes of each file, GOG 2.0.0.5) |
| `ANNOADDON1` | Root of 27 `.sze` scenarios (add-on content) |
| `SCENE` | Child of `.sze` root |
| `HEAD`, `MAIN`, `GUID`, `ALIASNAME`, `WARETYPE`, `SKTIMER`, `MONOPOL2`, `LIMITS`, `COMPPROFIL2`, `ROHSTOFF`, `SCHATZ`, `ERZE`, `BIGBROTHER`, `AUFTRAGTEXT`, `SPIELER`, `INSEL5`, `INSELLNK` | Children of `SCENE` in `.sze` ("excerpt" per GFA, list not exhaustive) |
| `INSEL4`, `INSEL5` | Island data in `.scp` / `.sze` |
| `INFO`, `HEIGHT`, `GFXID`, `GELAENDE`, `PROD2`, `FRUCHTBAR`, `BERGWERK2`, `OCCUR`, `ANBAU`, `DISCOVER`, `KONTORID`, `STAEDTE`, `LOCKED` | Island chunks (`.scp`, island sections of `.sze`) |
| `COLORS` | `Anlagen.dat` (440 ENTRY, colour tables; layout in `docs/formats/palette.md`) |
| `ANLAGEN` | `Anlagen.dat` (921 ENTRY) |
| `TEXTURES` | `Anlagen.dat` (3×) |
| `ENTRY` | `Anlagen.dat` (inside COLORS, ANLAGEN); `FigurenHeader.dat` (does not parse generically) |
| `TEXTURE`, `COLOR` | `.tex` files (`ToolGfx`, `Textures`) |
| `ACCESS` | `.inc` files |
| `CHARSET` (children `HEAD`, `BODY`) | `.zei` fonts |

Keys inside `FigurenHeader.dat` ENTRY records (12-byte key + u32, not generic chunks): `VERSION2`, `HEAD`, `BEWOHNTYPE`, `ROUTETYPE`, `LOGIC`, `PATH`, `COLORS`, `HAEUSER`, … (PLAN M1.3, GFA §3).
