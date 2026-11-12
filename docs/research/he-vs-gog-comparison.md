# GOG 2.0.0.5 versus History Edition: files, data and wildlife code (2026-11-12)

Owner request of 2026-11-12: compare the two builds of the original, write the results down, and use them for
the wildlife decision (hunter and hides) and for the question of players who own different editions.

**Evidence tier.** The project's primary evidence base stays the GOG build in `game/app` (Anno 1503 A.D. 2.0.0.5,
binaries dated 2006-07-21). Everything about the History Edition here is **HE build evidence** (Steam app 2820110),
kept separate as in `docs/render/rivers.md` §6.2. Both installs were read only; nothing from either is in git.
Marks: **H** hash of whole files compared, **P** parsed with the project's reference readers (`tools/*.py`),
**S** structural code comparison (function tables, call profiles, constants), one pass, not re-derived by a second
reader. Addresses are image addresses; `HE86` means the 32-bit module `AnnoWeltAddOn.dll` of the History Edition.

| Install | Path | Files |
|---|---|---|
| GOG 2.0.0.5 | `E:\anno1503\game\app` | 5,951 |
| History Edition | `E:\SteamLibrary\steamapps\common\Anno 1503 - History Edition` | 12,366 |

## 1. Result in short

- **The game rules data is the same.** The buildings file the History Edition loads, `Anlagen.addon`, is byte for
  byte the GOG `Anlagen.dat` (H). `BGruppen.dat`, `Wachstum.dat`, `wegspeed.dat`, `Highmap.dat` and `font.dat` are
  identical (H). All 368 figure types have identical gameplay records (P). All 50 scenarios are identical (H).
- **Three things differ and matter to a loader:**
  1. The History Edition also ships a file *named* `Anlagen.dat`, and it is a different, older variant (909
     entries against 921). A loader that opens `Anlagen.dat` in a History Edition folder gets other rules.
  2. The scenarios and the language text sit in other folders (`Languages\Eng\Scenes\...`, `Languages\Texte.dat`),
     and the root `Texte.dat` is German.
  3. One island template of the GOG build is missing in the History Edition, so the pool of island templates
     differs by one.
- **Graphics containers differ in packing; 57 of 58 sampled figure types decode identically.** `FigurenHeader.dat`
  and most `Figuren*.dat` volumes differ in bytes. One figure type (375) of the History Edition stalls the
  reference reader and eight figure types differ in non-pixel records (§3.2).
- **The History Edition has what GOG lacks:** five authored multiplayer scenarios (§3.5), and C++ class names in
  its modules (§4), which name the GOG module's unnamed classes.
- **Wildlife is the same game in both builds:** identical animal, hunter and trapper data, and the fauna sweep and
  the hunter tick are the same code by structure (§5).

## 2. Program modules (P, PE headers)

| Module | GOG | History Edition |
|---|---|---|
| Main program | `1503Startup.exe`, x86, 2006-07-21, linker 6.0 | `Anno1503.exe`, x64, 2024-03-28, linker 14.16 |
| `AnnoWelt.dll` | x86, 2006-07-21, 966,656 bytes, sha256 `f220a878...` | x64, 2020-08-05, 1,310,648 bytes, sha256 `95951b57...` |
| `AnnoGame`, `AnnoScene`, `AnnoFrame`, `View`, `CompLogic`, `CompLogicClassic`, `FileGame` | x86, 2006-07-21 (`FileGame` 2004) | x64, 2020-08-05 |
| `AnnoDisplay.dll` | x86, 344,064 bytes (DirectDraw) | x64, 2024-03-28, 30,287,384 bytes (exports name a DX11 renderer) |
| `AnnoWeltAddOn.dll`, `AnnoGameAddOn.dll`, `AnnoSceneAddOn.dll`, `AnnoFrameAddOn.dll`, `AnnoDisplayAddOn.dll` | absent | **x86**, 2019-09-27, linker 14.16; `AnnoWeltAddOn.dll` 1,698,744 bytes, sha256 `87cf9e3a...` |

- The x64 program imports only the x64 modules. The five `*AddOn.dll` files are 32-bit builds of the same code
  made with the modern compiler; a 64-bit program cannot load them. They keep the original export names and the
  image base `0x10000000`.
- **Which data file each build opens (P, strings in the modules):** GOG `AnnoFrame.dll` / `AnnoGame.dll` name
  `Anlagen.dat`; the History Edition's x64 `AnnoFrame.dll` / `AnnoGame.dll` name **`Anlagen.addon`**; its 32-bit
  `*AddOn.dll` name `Anlagen.dat`.
- **Class names.** GOG `AnnoWelt.dll` has no RTTI names (0 found). The History Edition has them: 267 type names in
  the x64 `AnnoWelt.dll`, 288 in `AnnoWeltAddOn.dll`. Four classes are named only in the 32-bit module:
  `tOccurTimeBomb`, `tProdBibliothek`, `tRouteFight`, `tRouteWalkDest`. Why the x64 module lacks them is open.

## 3. Data files

### 3.1 Buildings (`Anlagen.*`) (H, P)

| File | Size | Entries | Relation to GOG `Anlagen.dat` (921 entries) |
|---|---|---|---|
| HE `Anlagen.addon` | 205,470,957 | 921 | **byte-identical** |
| HE `Anlagen.dat` | 198,098,169 | 909 | 14 GOG entries missing (525, 526, 546, 584, 585, 1384, 1385, 1387-1393), 2 extra (510, 512); non-graphic records differ in 36 entries, of which 29 only in `VERSION2`; `PROD3` differs in 511, 513, 566, 567 |
| HE `Anlagen.classic` | 192,149,506 | 894 | 29 GOG entries missing, 2 extra; non-graphic records differ in 225 entries (`HEAD` in 139, `PROD3` in 24, among them the tannery 1023 and 1029) |
| HE `Anlagen.gfx` | 332,501 | — | colour tables and an entry list without pixel volumes; not analysed |

- `Anlagen.classic` is by its numbers the rule set before the add-on; `Anlagen.dat` in the History Edition folder
  lies between the two. Which program path uses `Anlagen.classic` was not traced.
- Hunter 805, fur trapper 824, sheep farm 807 and stall 1314 have identical gameplay records in all three variants;
  only colour ids and texture offsets differ.

### 3.2 Figures (`FigurenHeader.dat`, `Figuren*.dat`) (H, P)

- `FigurenHeader.dat` differs in bytes (27,904,566 against 27,909,219). Both hold 368 entries with the same ids.
- **The entry-level gameplay records are identical in all 368 entries:** `HEAD`, `BEWOHNTYPE`, `ROUTETYPE`, `LOGIC`,
  `WARE`. In 360 entries the per-set records (`BODY2`, `ANIM`, `SOUND2`, `GFXDATA`) are identical too.
- **Eight entries differ beyond the frame tables: 370-376 and 453.** Their `VERSION2` and `PATH` records differ
  (`PATH` is longer in the History Edition), and so do the set prefix and the hash over the per-set records. In
  the History Edition's entry 375 two sets carry the same frame-table hash, which they do not in GOG. What these
  eight figure types are and what changed in them was not decoded.
- The frame tables differ in 362 entries because the volumes are packed differently: the three zoom volumes are
  179,919,250 / 55,077,515 / 15,856,700 bytes in GOG and 179,374,409 / 54,899,642 / 15,800,617 in the History
  Edition, with 243,707 frames each in both. `Figuren000`-`003` and `Figuren100` are byte-identical, the other
  eleven volumes are not.
- **Decoded frames, a sample of 58 figure types** (every eighth entry, the eight entries above, and 108, 117, 126,
  131; `tools/figuren.py frames`, which prints size, format and a pixel hash per frame): **57 are identical in both
  builds**, among them 370-374, 376 and 453. **Entry 375 is not comparable yet:** the reference reader finishes it
  in one second on the GOG files and does not finish within 60 seconds on the History Edition's (stopped after 43
  minutes in the first run). The cause is not investigated; a loader for History Edition folders has to settle it.
- The History Edition also has `Figuren.dat`, `Figuren.addon` and `Figuren.classic` (251-279 MB each): single files
  that start like `FigurenHeader.dat`. Its modules name `Figuren.dat`. Not analysed further.

### 3.3 Plain-text data (H, text diff)

| File | Result |
|---|---|
| `Data/BGruppen.dat`, `Wachstum.dat`, `wegspeed.dat`, `Highmap.dat`, `font.dat`, `htmlpic.dat` | identical |
| `Data/Felder.dat`, `ScapeIcon.dat`, `statusbar.dat` | only the build machine's source paths differ (`\\SERVER\GFX\1503\...` against `..\Grafik\Compile\...`); `statusbar.dat` also in file names with umlauts |
| `Data/music.dat` | one path pattern corrected |
| `Data/Debug.dat` | 15 MB in GOG, 1 KB in the History Edition; not gameplay |
| HE-only: `Data/Fenster.dat`, `FensterAddOn.dat`, `Startup.dat`, `StartupAddon.dat`, `sysinfo.txt`, `config/online/storm.ini` | window layouts as text and an online configuration; not analysed |

### 3.4 Language text (`Texte.dat`) (P)

- GOG `Texte.dat` (English) and HE `Languages\Texte.dat` agree except in two sections of menu text
  (`ROUTETEXTS`, `OPTIONSUBS_TEXTE`). `[STAEDTE]` (160 city names) and `[SHIPS]` are identical.
- HE root `Texte.dat` is German: `[STAEDTE]` differs from the English file in 59 of 160 positions.
- **This reaches the simulation today:** `sim_load::load_city_names` reads `[STAEDTE]` from `<game dir>/Texte.dat`
  into `Rules::city_names`, which is part of the saved and hashed state (`state_fields.hpp`, stream 120). Two
  players with different language files therefore do not share a state hash, on the same edition too.

### 3.5 Scenarios and islands (H, P)

- All 50 GOG `.sze` exist byte-identically in the History Edition, under `Languages\Eng\Scenes\...`. The German
  tree has two more single scenarios (12 against 10 in `0200Single`).
- Island templates: 464 of the 465 GOG `.scp` are byte-identical in the History Edition. GOG-only:
  `Scenes/Islands/Dschungel/D_080x080_0015_2Ba_Suedsee.scp`. The History Edition has no template GOG lacks.
- **Multiplayer scenarios exist only in the History Edition** (the GOG folders `0500MultiEndlos` and `0700Multi`
  are empty, PLAN §0.6). All five parse with `tools/sze.py`:

| File | Kind | Player records | Island links | Link sizes | Extent of the links |
|---|---|---|---|---|---|
| `0500MultiEndlos/LastWager.sze` | endless | 6 | 24 | 2 × 256², 3 × 128², 10 × 80², 9 × 64² | 728 × 736 |
| `0500MultiEndlos/Multi01_04.sze` | endless | 14 | 28 | 13 × 128², 13 × 80², 2 × 64² | 992 × 992 |
| `0500MultiEndlos/MoralHighGround.sze` | endless | 22 | 36 | 1 × 256², 8 × 128², 17 × 80², 10 × 64² | 976 × 1000 |
| `0500MultiEndlos/NewEarth.sze` | endless | 22 | 43 | 8 × 128², 17 × 80², 18 × 64² | 960 × 1000 |
| `0700Multi/Multi001.sze` | mission | 16 | none (2 stored islands) | — | — |

  Player records include natives, pirates and traders; the number of human seats per file was not decoded. These
  are the only authored multiplayer worlds of the original and bear on `docs/plan/lan-endless.md` and the fixed map.

## 4. Class names from the History Edition, applied to the GOG module (S)

The 32-bit `AnnoWeltAddOn.dll` carries RTTI, so every function table there has a class name and a list of base
classes. GOG function tables match them in size and, slot by slot, in the profile of indirect calls:

| Class (HE name) | GOG table | HE86 table | Slots | Slots with the same indirect-call profile |
|---|---|---|---|---|
| `tProdJagdhaus` (hunter building) | `0x100c82a0` | `0x1014d408` | 75 | 70 |
| `tInselServer` (the in-game island) | `0x100cac2c` | `0x1014b00c` | 111 | 90 |
| `tRouteJaeger` (the "job 0xd" object of `docs/re/hunter.md`) | `0x100c6f90` | `0x1014f064` | 22 | 21 |
| `tHirschObject@tInsel` (the 16-byte island object of the `HIRSCH` record) | `0x100cac04` or `0x100cac18` (two 5-slot tables, not told apart) | `0x1014afdc` | 5 | 5 |
| `tBewohnerWildTier` (animal figure) | `0x100c653c` | `0x10148d90` | 101 | 92 |
| `tBewohnerWildTierNew` | `0x100c66fc` | `0x10148f30` | 104 | 93 |
| `tBewohnerWildTierWolf` (base `tBewohnerWildTierNew`) | `0x100c68e0` | `0x10149100` | 104 | 91 |
| `tBewohnerZivilist` | `0x100c6a98` | `0x101492cc` | 101 | 91 |

- Control: assigning `tBewohnerWildTier` and `tBewohnerZivilist` the other way round scores 86 of 101 each, lower
  than 92 and 91. Most slots are inherited from `tBewohnerBoden`, so only the overridden slots tell classes apart;
  the two 104-slot tables are assigned by their order. This is a structural match, not a proof.
- Other wildlife names that exist and are **not yet located in GOG**: `tRouteHirsch` (base `tRouteWildTier`),
  `tRouteWildTier`, `tJobSteheWildTier` (base `tJobStehe`), `tJobWalkRandom`, `tJobWalkRohstWild` (base
  `tJobWalkRohstProd`), `tProdPelzJaeger` (base `tProdPlantage`). In HE86 they are at `0x10148d34`, `0x1014f3c4`,
  `0x10148d08`, `0x10148cb0`, `0x10148c84`, `0x10151480`.
- Corrections this gives to existing notes: what `docs/re/hunter.md` calls the "job switch" `0x1003b0e0` creates
  **route** objects (`tRoute*`, 22 slots) whose steps are **job** objects (`tJob*`, 10 slots). The whaler has the
  same family of names (`tProdWalfang`, `tBewohnerWal`, `tRouteWal`, `tRouteWalfaenger`, `tOccurrenceWal`).

## 5. Wildlife: is it the same in both builds?

- **Data (P, H): yes.** The animal figure types (`ROUTETYPE HIRSCH`, `WARE FLEISCHTIER` / `PELZTIER` / `RAUBTIER`),
  the hunter 805 and the fur trapper 824 have identical gameplay records. The island `HIRSCH` records are in the
  scenarios, which are byte-identical.
- **Hunter tick (S): same.** GOG `0x10069f40` and HE86 `0x10067bb0` (slot 35 of `tProdJagdhaus`) call the same
  type slots (`+0x84`, `+0xa8`, `+0xb0`, `+0xb4`, `+0xe0` twice, `+0xe4` twice, `+0x118`) and use the same
  constants (`0x1f4`, `0x180`, `0x7f` six times, `0x80`, `0x40`, `0x44f`, `0xafc8`, job `0xd`). The GOG function
  has more display calls through `+0x168`; in HE86 those are direct calls.
- **Fauna sweep (S): same.** GOG `0x10093480` and HE86 `0x10048370` (slot 101 of `tInselServer`) have the
  **identical multiset of indirect calls** (twelve slots, the world draw `+0xb4` once) and the same constants
  (`0x46`, `0x145`, `0x3c`, `0x40` three times, `0xbf`, `0xc0`, `0xfffe`, the divide-by-1000 constant). 348 against
  383 instructions.
- **Growth sweep (S): not compared to a conclusion.** Slot 100 is 46 instructions in GOG and 118 in HE86, with one
  direct call fewer; inlining of the callee would explain it and was not checked.

So the wildlife rules can be read from the GOG module as the reference, with the History Edition's names as a
guide, and a port serves owners of both editions. Nothing here reads the still unread bodies: the fauna sweep's
selection rules, the creation helper `0x10092ca0`, the animals' own tick and their route and job classes
(`docs/re/hunter.md`, "Isolation of the wildlife dependency").

## 6. What a loader meets in a History Edition folder

| The rebuild opens today | In a GOG folder | In a History Edition folder |
|---|---|---|
| `Anlagen.dat` (`original_registry.cpp`, `loader.cpp`, `game_dir.cpp`) | the 921-entry add-on rules | **the 909-entry variant**; the right file is `Anlagen.addon` |
| `FigurenHeader.dat` and volumes | present | present, same gameplay records |
| `Data/BGruppen.dat` and the other `Data` files | present | present, identical where gameplay |
| `Texte.dat` (city names into the hashed rules) | English | **German**; the English file is `Languages\Texte.dat` |
| `Scenes/<folder>/*.sze` | present | **absent**; under `Languages\Eng\Scenes\` and `Languages\Ger\Scenes\` |
| `Scenes/Islands/**.scp` (the link resolver's candidate list, `scenario_load.cpp`) | 465 templates | 464 templates |

## 7. Not checked

- Whether every frame of every figure type decodes to the same pixels in both builds (a sample was compared).
- The 64-bit modules' code; only their names and file references were read.
- The History Edition's `Anlagen.gfx`, `Figuren.dat` / `.addon` / `.classic`, `Fenster*.dat`, `Startup*.dat`, the
  `ahe` archive and the online configuration.
- Human seat counts and start positions of the five multiplayer scenarios.
- Whether the GOG-only island template can be chosen by the original's link resolver in an endless game.
- Other editions (German retail, Königsedition discs, the co-developer's build).

## 8. How this was produced

Read-only scripts in a session scratch folder, not committed: a sha256 inventory of both installs; `pefile` for the
module headers and RTTI tables; `tools/figuren.py dump` / `anlagen` / `frames`, `tools/sze.py dump` and
`tools/insel.py dump` on both installs; `capstone` for function-level call and constant profiles. No disassembly
text and no bytes of either build are in this document.
