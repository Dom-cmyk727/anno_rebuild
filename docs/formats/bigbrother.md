# Scenario script (`BIGBROTHER` chunk)

**Status:** partial.
- **Grammar:** complete for the 50 shipped scripts. All parse without a fallback and re-emit byte-identically (`tools/bigbrother.py check`; T17, 2026-10-22).
- **C++ reader** (T20 stage 1, 2026-10-23): `anno::formats::bigbrother` reads the grammar and the interpreter's reading (grouping, read-on) and re-emits all 50 byte-identically; `anno-dump bigbrother` = `tools/bigbrother.py dump` (§8). No evaluation yet (stage 2).
- **Meanings:** conditions and actions are read from their names, arguments and uses. The action and condition handlers were not traced (OQ-122, OQ-123).
- **The interpreter's reading** (T19, D): the statement reader, the condition parser and the evaluation are traced in `docs/re/annoscene.md` §12. That settles the grouping of `&&`/`||` (OQ-120) and the statements without `;` (OQ-121); see §3.4.

Evidence: **P** = parsed from the 50 `.sze` files (and seven quicksaves); **S** = strings in `AnnoScene.dll` (GOG 2.0.0.5, image base `0x10000000`, addresses as virtual addresses); **D** = disassembly (`docs/re/annoscene.md` §12 and other notes).

## Files covered
- **Scenarios:** the last child of `SCENE` in every `.sze` (50 of 50, P; `sze.md`).
- **Savegames:** a direct child of the root (`ANNOADDON1`) in `SaveGame/<profile>/Lastgame.sav`. P: seven quicksaves of two games in the Wine runner (T16), not in git.
- **The interpreter:** `AnnoScene.dll` (`AnnoBigBrother`; the script ops lie in `0x10011ed0`–`0x10013dab`, `docs/sim/state.md` O-16). The net client steps it before every simulation advance (AnnoFrame `0x1005fe10`, O-34).
- **Size:** 174–29,425 bytes per script. 37 scripts have at least one statement. The kernel is empty in the nine endless games of `0010SingleEndlos`, in `endless03` and in the three tutorials.

## 1. Bytes
- **Plain text.** No terminator and no length field besides the chunk's.
- **Encoding:** the reference reads it as Latin-1, which maps every byte to one character. 236 bytes are ≥ 0x80, all in comments and strings (German umlauts).
- **Mixed line endings, written that way by the game (S):**
  - section headers and define lines end in LF; script, `[SIGNALS]` and `[CAMARAPOS]` lines end in CRLF;
  - totals: 6,257 CRLF, 1,216 LF, no bare CR, 1,590 tabs (P);
  - the format strings `"\n[SIGNALS]\n"`, `"%s = %s\r\n"` (one per signal), `"\n[CAMARAPOS]\n"` and `"%d,%d\r\n"` (`0x1003e558`, `0x1003e54c`, `0x1003e53c`, `0x1003e534`) produce exactly the endings of a savegame's `[SIGNALS]` and `[CAMARAPOS]` (P: seven quicksaves); their writer was not traced;
  - the define writers use `"[OBJECT_DEFINE]\n"`, `"OBJECT_FIGUR \"%s\" %d\n"`, `"[AREA_DEFINE]\n"`, `"[PLAYERMAPPER_DEFINE]\n"` and `"[CAMARAPOS]\n"` (`0x10040330`, `0x10040318`, `0x10040308`, `0x100402f0`, `0x100402e0`).

## 2. Tokens (P: every byte of the 50 scripts is one of these)

| Token | Form | Notes |
|---|---|---|
| newline | `\r\n` or `\n` | both kept |
| blank | spaces and tabs | |
| comment | `$` to the end of the line | the whole line or after a statement; only in the kernel (1,273 of 1,273) |
| section | `[NAME]` | a line of its own (300 of 300); the first byte of the script |
| string | `"…"` | no escapes, no line break inside |
| number | `-?[0-9]+` | negative only as `SET_DIPLOMACY`'s value (−127, −128) |
| name | `[A-Za-z_][A-Za-z0-9_]*` | keywords, variables, actions; all upper case in the corpus |
| operator | `&&` `\|\|` `==` `<` `>` `=` `(` `)` `{` `}` `,` `;` | `!=`, `<=`, `>=` and `!` never occur; the interpreter's comparison table (S, `0x1003fdc4`–`0x1003fdd0`, after `"Unbekannter Vergleichsoperator!"`) has `!=`, `==`, `>`, `<` |

- **Tokens may touch.** `AREA"NEGER"TOUCHED_BY_SHIP_FROM"HUMAN"` (endless01) has no blank between the tokens.
- **The interpreter's delimiter table** (S, `0x1003d888`–`0x1003d8a0`): `THEN`, `{`, `}`, `||`, `&&`, `;`.

## 3. Sections
- **Order:** every shipped script has six sections, always in this order (P, 50 of 50): `[OBJECT_DEFINE]`, `[AREA_DEFINE]`, `[PLAYERMAPPER_DEFINE]`, `[SCRIPT_KERNEL]`, `[SIGNALS]`, `[CAMARAPOS]`.
- **The reader's names:** it looks the sections up as `"OBJECT_DEFINE\n"` … `"SIGNALS\n"` (S, `0x1003e5c8`–`0x1003e618`).
- **`STARTUP`:** a seventh name (`0x1003e628`) that no shipped script uses (OQ-122).
- **In the readers** (the reference and the C++ reader): the six names are data, required once each and in this order. A section of any other name (its body must still be tokens of §2) is kept as read and not interpreted, so a script with `STARTUP` loads and re-emits.

### 3.1 `[OBJECT_DEFINE]`: named objects, one per line
| Form | Uses | Reading |
|---|---|---|
| `OBJECT_FIGUR "NAME" n` | 133 | a unit; `n` is likely the unit's id in its `SPIELER` record |
| `OBJECT_STADT "NAME" n` | 80 | a city; `n` not traced |
| `OBJECT_ANLAGE "NAME" x,y,t` | 37 | a building: tile x, tile y and the `Anlagen.dat` type (e.g. 2221, 2223, 1383; S01_04_01, S01_08_01, S01_12_01); likely |

- **How the kernel refers to them:** as `OBJECT "NAME" …` in conditions, and as action arguments.
- **Names the engine itself looks up (S):**
  - `SCHIFF_ATTACK` and 29 `SOLDAT_*` names stand in `AnnoScene.dll` (`0x1003b444`–`0x1003b63c`) and are the objects of the combat tutorial (`03T_Combat_01`).
  - `ERZ01` (`0x1003e4d8`, next to `"Please unload scout"`) is an area of the discovery tutorial (`01Discover01`).
  - Both tutorials have empty kernels, so their logic is likely in the code, which knows their object names.

### 3.2 `[AREA_DEFINE]`: `"NAME" a, b, c, d`
- **The numbers:** four integers in 0–1025 (P).
- **Corners, likely:** the survey reads them as x0, y0, x1, y1. The first pair is not always the smaller (`"IDOL" 195, 62, 69, 203`, endless01), so two opposite corners `(a, b)` and `(c, d)` in tiles is likely. Not traced (OQ-122).

### 3.3 `[PLAYERMAPPER_DEFINE]`: `"NAME" slot`
- **What it does:** maps the player names the kernel uses to player slots (the `SPIELER` index).
- **Slot 0** is `"HUMAN"` in 32 files, `"HUMAN01"` in 14 and the misspelt `"HUNAM01"` in 1. The other three missions give slot 0 another name: `"CG1"`, `"CG2"`, `"BERTRECHT"`.
- **Slot 31** is `"DUMMYPLAYER"` in all 50.
- **Other names:** `CG01`…, `PIRAT` (16 files), `VENEZIANER`, native peoples (P).
- **Every name once:** no define section repeats a name (P, 50 of 50).

### 3.4 `[SCRIPT_KERNEL]`
Grammar as read from the files (P):
```
kernel    = { initblock | set | if } ;
initblock = "INITBLOCK" "(" ")" block ;
set       = "SET" NAME "=" value [ ";" ] ;
if        = "IF" condition "THEN" block [ "ELSE" block ] ;
block     = "{" { assign | call } "}" ;
assign    = NAME "=" value [ ";" ] ;
call      = NAME "(" [ value { "," value } ] ")" [ ";" ]  |  NAME ";" ;
condition = term { ( "&&" | "||" ) term } ;
term      = "(" condition ")" | primary ;
primary   = ( "PLAYER" | "OBJECT" | "AREA" ) STRING QUERY [ STRING ] [ compare value ]
          | NAME [ compare value ] ;
compare   = "==" | "<" | ">" ;
value     = "TRUE" | "FALSE" | NUMBER | STRING ;
```
- **Names:** `INITBLOCK`, `SET`, `IF`, `THEN`, `ELSE`, `TRUE` and `FALSE` are never a variable, signal, action or query name (P); the reference treats them as reserved.
- **The missing `;`:** each of the four statements without one ends its line (P), and the reference accepts a missing `;` only there.
- **Define lines** (§3.1–§3.6): `OBJECT_FIGUR` and `OBJECT_STADT` take one number, `OBJECT_ANLAGE` three (P: 133, 80, 37 lines); `[CAMARAPOS]` has exactly one line. The reference rejects anything else.
- **Counts:**
  - 35 `INITBLOCK`s, 271 `SET`s, 308 `IF`s (3 with `ELSE`, all in single33);
  - 382 assignments and 1,656 calls inside blocks;
  - no nested `IF`, no `ELSE IF`, no other statement.
- **Variables:** 147 distinct names. Their values are only `TRUE` and `FALSE`; they are set by `SET` at the top level and by assignments in blocks.
- **Timers:** `TIMER_0` … `TIMER_4` are read in conditions and only compared with numbers (24 uses in 10 files). `TIMER_FLAG` (Single_08) is a `SET` variable, not a timer.
  - Actions drive them: `TIMER_SET(slot, value)`, `TIMER_START(slot)`, `TIMER_STOP(slot)`.
  - Their state is the `SKTIMER` chunk (resurvey §4, O-31).
- **Evaluation:** the scripts are written for rules that are tested again and again. The common pattern is one-shot: `IF ONETIME && … THEN { ONETIME = FALSE; … }`. This fits a step before every advance (O-34).
  - **The step** (D, AnnoScene `0x10016b70`): the `INITBLOCK` runs in the first step, before any `IF`, and only while `SCHUETTIINITSIG` is TRUE; the step then sets it FALSE.
  - **The `IF`s** are evaluated round-robin from a cursor. How many per step is open (`annoscene.md` §12).
  - **An `IF`'s condition** is evaluated left operand first. `&&` skips its right operand when the left is false; `||` always evaluates both (D, `0x10010290`).
  - **The blocks:** THEN runs when the condition is true, ELSE when it is false. Exception: a condition whose top connective is `&&` with a false left operand runs **neither** block.
  - **ELSE in the corpus:** the three shipped `ELSE`s (single33) all start with `CHKPARTn &&`, so their ELSE runs only while that flag is TRUE.
- **Irregular statements:** they ship, and the game loads these missions:
  - four statements without `;`:
    - `SET_PROFILE(…)` in S01_02_01;
    - `SET DEADEND_ABFRAGE = FALSE` in S01_08_01;
    - `SET_DIPLOMACY(…)` and `SET_FOR_BUY( "HEILKRAEUTER", "INDIANS", 30 )` in S01_09_01;
  - `QUESTLOG_DRAW;` without parentheses (single42).

  The reference accepts them as written.

  **The interpreter reads on (D, `annoscene.md` §12; OQ-121 closed):**
  - **In a block** (`INITBLOCK`, THEN, ELSE), a statement is the text up to the next `;` anywhere in the kernel text (`strpbrk`, `0x1001cbca`). That `;` can be in a later statement, in a comment or past the block's `}`.
  - **What it swallows:** the action's reader takes its arguments and ignores the rest, so every statement that starts before that `;` is lost. What lies past the block's `}` is not lost, because the reader resumes after the `}`. With no `;` left in the kernel, the interpreter refuses the whole kernel.
  - **Not modelled by the readers** (no shipped case): when that `;` stands in a comment or in a later statement's string with text after it, the interpreter's next statement starts right after the `;`. The one `;` in a comment that the corpus reads on to (S01_02_01) ends its line.
  - **At the top level**, `SET` takes its value and goes on at the next word, so a missing `;` there is harmless. `QUESTLOG_DRAW;` and `QUESTLOG_DRAW();` read the same (the action takes no argument).
  - **The four shipped cases** (P):
    - `SET_PROFILE(…)` (S01_02_01) runs on to the `;` of a commented-out line in its block: nothing lost;
    - `SET DEADEND_ABFRAGE = FALSE` (S01_08_01) is top level: nothing lost;
    - `SET_DIPLOMACY(…)` (S01_09_01) is the INITBLOCK's last statement and runs on past `}` to `SET TEIL1_1 = FALSE;`, which the reader then reads again: nothing lost;
    - `SET_FOR_BUY( "HEILKRAEUTER", "INDIANS", 30 )` (S01_09_01) swallows the next statement of its block, `MINIMAP_ALERT( 640, 950 );`. **The original never shows that minimap alert.**

  `tools/bigbrother.py` models this in `read_on` (`dump` marks the statements, `quirks` lists them). The faithful port drops the swallowed statement. Restoring the alert would be a QoL change behind a `rules` option, with the owner picking the default (CLAUDE.md).
- **Precedence:** 28 of the 308 conditions (in 10 files) mix `&&` and `||` without parentheses. Example (Single_01): `MISSION_FLAG == FALSE && (… DEAD && … DEAD && … DEAD) || PLAYER "HUMAN" INHABITANTCOUNT "BUERGER" > 2999`. The reference keeps the sequence as written.

  **The interpreter's grouping (D, `annoscene.md` §12; OQ-120 closed):**
  - **Equal rank, to the right:** `&&` and `||` have equal rank and group to the right. The first connective takes the whole rest of the condition as its right operand: `a op b op c` = `a op (b op c)`; parentheses group as written (condition parser `0x10022bf0`).
  - **The 28 shipped conditions:** all have the shape `guard && … && alt1 || alt2 …`, and the interpreter reads them as `guard && (… && (alt1 || alt2 …))`. Single_01 reads as `MISSION_FLAG == FALSE && ((… DEAD && …) || … > 2999)`.
  - **Against C precedence,** each of the 28 means something else: the alternatives after the first `||` would fire without their guard. So the interpreter's grouping is what the one-shot pattern needs; the C reading would make the rules fire again.
  - **In the reference:** `grouped`/`describe_grouped` give the grouping, `evaluate`/`branch` the evaluation; `dump` prints the grouping under each mixed condition.

### 3.5 `[SIGNALS]`: the script's state
- **Form:** one `NAME = TRUE|FALSE` per line, no `;`, CRLF.
- **In every shipped scenario** (50 of 50, P): exactly the kernel's `SET` variables with their initial values, plus `SCHUETTIINITSIG = TRUE`.
- **In a savegame** (P: seven quicksaves of two "Citizen" games in the Wine runner, T16):
  - the definitions and the kernel are byte-identical to those of exactly one scenario, `00_1024_easyentry_01.sze`, so a Citizen game starts from it (its kernel is empty; the islands are generated);
  - `[SIGNALS]` holds only `SCHUETTIINITSIG`, now `FALSE`; `[CAMARAPOS]` is still `0,0`.
- **`SCHUETTIINITSIG`:** `TRUE` in every scenario, `FALSE` once the game has run. It marks the `INITBLOCK` as pending (D, AnnoScene step `0x10016b70`):
  - the first step runs the `INITBLOCK` only while it is TRUE, then sets it FALSE. So a game loaded with it FALSE does not run the block again (likely: that `[SIGNALS]` is applied before the first step was not traced);
  - the kernel reader creates it TRUE when the script does not define it (`0x1001c9f0`).

  The quicksaves' flip fits; their script has no `INITBLOCK`.
- **Load check:** the loader reports `Signal "%s" ist nicht vorhanden!` ("signal … does not exist", S, `0x1003e574`), likely for a signal the kernel does not define. A signal name has a maximum length: `Name "%s" für Signalobject zu lang\nMax. %d Zeichen!` (S, `0x1003f93c`): 34 characters for a `SET` name (D, `0x1001cf84`).
- **What a savegame keeps of the script:**
  - the whole text (definitions and kernel; P);
  - one `NAME = value` line per signal (P for `SCHUETTIINITSIG`). That variables are saved with their current values is likely from the format strings of §1; the saved scripts have no variables, so it was not observed (OQ-123);
  - `[CAMARAPOS]` (P);
  - the timers, which live in `SKTIMER` (resurvey §4).

### 3.6 `[CAMARAPOS]`
- **Form:** `x,y`, one line; it is `0,0` in all 50 scenarios and in the seven quicksaves (P).
- **Meaning:** a camera position the game saves. Whether it is `CAMERA_MEMORY`'s slot is not traced (OQ-122).

## 4. Conditions (P)
The query names a property of the subject. An optional string narrows it, and an optional comparison tests a count or value.

| Condition | Forms after the query (count) | Uses | Files | First file |
|---|---|---|---|---|
| `variable` | — (228), == bool (201) | 429 | 37 | endless01.sze |
| `OBJECT KILLED` | — (77) | 77 | 21 | S01_01_01.sze |
| `AREA TOUCHED_BY_SHIP_FROM` | "…" (53) | 53 | 9 | endless01.sze |
| `PLAYER DEAD` | — (30) | 30 | 7 | S01_02_01.sze |
| `AREA TOUCHED_BY_CONSTRUCTION` | "…" (30) | 30 | 3 | single33.sze |
| `TIMER_n` | > number (10), == number (8), < number (6) | 24 | 10 | S01_07_01.sze |
| `AREA TOUCHED_BY_SOLDIER_FROM` | "…" (26) | 26 | 5 | endless01.sze |
| `PLAYER INHABITANTCOUNT` | "…" > number (21), "…" < number (5) | 26 | 17 | S01_01_01.sze |
| `AREA TOUCHED_BY_SOLDIERS_FROM` | "…" (22) | 22 | 11 | S01_05_01.sze |
| `AREA TOUCHED_BY_OBJECT` | "…" (19) | 19 | 5 | S01_01_01.sze |
| `PLAYER WAR_STATUS` | "…" (15) | 15 | 7 | S01_01_01.sze |
| `OBJECT INHABITANTCOUNT` | "…" > number (9), "…" < number (4) | 13 | 3 | S01_05_01.sze |
| `AREA CONTAINS` | "…" > number (8) | 8 | 2 | S01_01_01.sze |
| `AREA SHIPCNT_FROM` | "…" > number (8) | 8 | 1 | single32.sze |
| `OBJECT OWNER` | == string (7) | 7 | 2 | S01_08_01.sze |
| `OBJECT WARECOUNT` | "…" > number (6) | 6 | 1 | Single_14.sze |
| `PLAYER BOUGHT` | "…" > number (5) | 5 | 5 | S01_01_01.sze |
| `PLAYER SHIPCNT` | > number (2), == number (2), < number (1) | 5 | 4 | S01_05_01.sze |
| `PLAYER SOLDIERCOUNT` | "…" < number (3), "…" == number (2) | 5 | 3 | S01_06_01.sze |
| `OBJECT CONTAINS` | "…" > number (5) | 5 | 1 | single30.sze |
| `PLAYER WARECOUNT` | "…" > number (4) | 4 | 2 | S01_05_01.sze |
| `PLAYER INHABITANT_AMOUNT` | > number (2), < number (2) | 4 | 4 | Single_15.sze |
| `PLAYER PAID_TRIBUTE_TO` | "…" (2) | 2 | 1 | S01_04_01.sze |
| `AREA SOLDIERCNT_FROM` | "…" < number (2) | 2 | 2 | S01_07_01.sze |
| `PLAYER SOLDIERLOST` | > number (2) | 2 | 2 | single31.sze |
| `PLAYER MAKES_PROFIT` | — (1) | 1 | 1 | S01_03_01.sze |
| `OBJECT HEALTH` | < number (1) | 1 | 1 | S01_11_01.sze |
| `PLAYER SHIPLOST` | > number (1) | 1 | 1 | single38.sze |
| `AREA TOUCHED_BY_ANYCITY` | — (1) | 1 | 1 | single42.sze |

String arguments of the conditions (P):
- **Player, object and area names** from the define sections.
- **`INHABITANTCOUNT`:** the levels `ALL` (17), `BUERGER` (9), `KAUFLEUTE` (6), `SIEDLER` (3), `ADEL` (3), `PIONIER` (1).
- **`SOLDIERCOUNT`:** `ALL` (3), `MOERSER` (1), `BEDIENER` (1).
- **`CONTAINS`** (13 uses) takes units and goods: `PIONIER` (6), `BOGENSCHUETZE`, `SCHWERTKAEMPFER`, `HOLZ`, `WERKZEUG`, `NAHRUNG`, `PELZE`, `HEILKRAEUTER` (1 each). `BOGENSCHUETZE` is not in the interpreter's unit-class block, which has `BOGEN` (§6).
- **`WARECOUNT`:** `EISENERZ` (6), `FLACHS` (3), `NAHRUNG` (1).
- **`"type@player"`:** `TOUCHED_BY_CONSTRUCTION "1210@HUMAN"`, a building type of a player (single33).
- **`"player@ware"`:** `BOUGHT "HUMAN@SALZ"`.
- **`"player@ware:n"`:** `SHIPCNT_FROM "HUMAN@WERKZEUG:1"` (single32).

**Goods in the scripts** (P; conditions and actions together, uses / files): `HEILKRAEUTER` 8/4, `EISENERZ` 6/1, `FLACHS` 6/1, `EISEN` 4/2, `WERKZEUG` 3/3, `NAHRUNG` 2/2, `SALZ` 2/1, `GEWUERZE` 2/2, and once each `HOLZ`, `PELZE`, `TABAK`, `TABAKWAREN`, `SCHWERT`, `SCHMUCK`, `GOLD`, `SEILE`. They are the `WARETYPE` names (`docs/re/production.md`).

**Units in the scripts** (P): `PIONIER` (6), `BOGENSCHUETZE`, `SCHWERTKAEMPFER`, `MOERSER`, `BEDIENER` (1 each).

The interpreter's error messages (S) show some of its checks. They are shared by conditions and actions (`"… bei Bedingung: %d"` ends both):
- `"Kein THEN bei Bedingung: %d"` (no THEN);
- `"%s ist unbekannter Disastertyp bei Bedingung: %d"` (`0x1003ef2c`, next to `DISASTER_AT`'s types);
- `"Anlage mit ID %d existiert nicht bei Bedingung: %d"` (`0x1003e9b8`, next to `SETCONSTRUCTION_DRAWSTATE`);
- `"Menge muß Positiv sein bei Bedingung %d"` (count must be positive, `0x1003e634`);
- `"GOTO_SCREEN Zoom Value muß zwischen 0 und 3 sein bei Bedingung: %d"` (`0x1003f6f8`);
- `"KILLPLAYER hat keinen \"SpielerNamen\" definiert bei Bedingung: %d"` (`0x1003eaf8`);
- `"SE ScriptChunk im File ist defekt!"` (the script chunk is broken, `0x1003e598`).

## 5. Actions (P)

| Action | Argument types (count) | Uses | Files | First file |
|---|---|---|---|---|
| `QUESTLOG_SHOW` | (string, string, bool) 669 | 669 | 28 | S01_01_01.sze |
| `DISASTER_AT` | (number, number, string) 230 | 230 | 3 | single30.sze |
| `SET_UNIT_VISIBLE` | (string, bool) 117 | 117 | 7 | S01_01_01.sze |
| `PLAYVIDEO` | (string) 83 | 83 | 13 | S01_01_01.sze |
| `SHOWTEXT` | (string) 72 | 72 | 24 | S01_01_01.sze |
| `LOST` | (string) 36, (string, number) 19 | 55 | 21 | S01_01_01.sze |
| `ASSIGN_OBJECT` | (string, string) 53 | 53 | 7 | S01_08_01.sze |
| `QUESTLOG_DRAW` | () 42 | 42 | 11 | Single_10.sze |
| `WON` | (string) 29, (string, number) 6 | 35 | 35 | S01_01_01.sze |
| `SET_DIPLOMACY` | (string, string, number) 30 | 30 | 14 | S01_02_01.sze |
| `ATTACK_AREA` | (string, string) 27 | 27 | 5 | S01_02_01.sze |
| `SETCONSTRUCTION_DRAWSTATE` | (string, string, bool) 25 | 25 | 6 | endless01.sze |
| `TIMER_SET` | (number, number) 25 | 25 | 10 | S01_07_01.sze |
| `MINIMAP_ALERT` | (number, number) 23 | 23 | 7 | S01_01_01.sze |
| `SET_PROFILE` | (string, string, number) 16, (string, string, bool) 6 | 22 | 4 | S01_02_01.sze |
| `PLAYWAVE` | (string) 18 | 18 | 7 | endless01.sze |
| `TIMER_START` | (number) 17 | 17 | 10 | S01_07_01.sze |
| `WAKEUP` | (string) 17 | 17 | 3 | single30.sze |
| `TIMER_STOP` | (number) 16 | 16 | 8 | Single_08.sze |
| `GOTO_SCREEN` | (number, number, number) 13, (string, number) 1 | 14 | 13 | S01_01_01.sze |
| `FREEZE_CITY` | (string) 10 | 10 | 7 | S01_04_01.sze |
| `MONEY_ADD` | (string, number) 8 | 8 | 5 | endless01.sze |
| `MOVE_UNIT` | (string, string) 8 | 8 | 2 | S01_11_01.sze |
| `SET_PIONIER_ITEM` | (string, number, bool) 7 | 7 | 1 | single30.sze |
| `ATTACK_OBJECT` | (string, string, number) 6 | 6 | 1 | Single_24.sze |
| `SETWARELOG` | (string, string, string, string) 5 | 5 | 5 | S01_01_01.sze |
| `SET_FOR_BUY` | (string, string, number) 5 | 5 | 3 | S01_02_01.sze |
| `ASSIGN_ALL_SOLDIERS` | (string, string) 4 | 4 | 3 | single31.sze |
| `SETTRIBUTE` | (string, string, number, string, number, string, number) 2 | 2 | 1 | S01_04_01.sze |
| `CHEAT` | (string, string, number) 2 | 2 | 1 | S01_05_01.sze |
| `ADD_SCORE` | (string, number) 2 | 2 | 1 | Single_24.sze |
| `ADD_HEALTH` | (string, number) 2 | 2 | 1 | single30.sze |
| `SETCONSTRUCTION_DRAWSTATE_ALL` | (string, bool) 1 | 1 | 1 | endless01.sze |
| `BEAMUP` | (string) 1 | 1 | 1 | S01_01_01.sze |
| `BEAMDOWN` | (string) 1 | 1 | 1 | S01_02_01.sze |
| `KILLPLAYER` | (string) 1 | 1 | 1 | single30.sze |
| `SHOWMESSAGE` | (string) 1 | 1 | 1 | single41.sze |

Observed numeric arguments (P). The readings are likely; none is traced in the handlers:

| Action | Argument | Values | Reading |
|---|---|---|---|
| `DISASTER_AT(x, y, type)` | x, y | 200–756, 137–810 | tile position |
| `MINIMAP_ALERT(x, y)` | x, y | 120–970, 45–950 | tile position |
| `GOTO_SCREEN(x, y, z)` | z | 1, 2, 3 | zoom level; the interpreter allows 0–3 (S, `0x1003f6f8`) |
| `TIMER_SET(slot, value)` | slot, value | 0–4; 0–10,800 | timer slot; seconds |
| `SET_DIPLOMACY(a, b, n)` | n | −128, −127, 0, 30 | treaty or relation value |
| `SET_PROFILE(player, key, n)` | key, n | `COMPCAPS_*`; 0, 1, 2, 5 or `TRUE`/`FALSE` | an AI profile parameter of `COMPPROFIL2` (T18) |
| `MONEY_ADD(player, n)` | n | 2,000 – 10,000,000 | gold |
| `WON(player, n)`, `LOST(player, n)` | n | 8–20 | not traced |
| `SET_FOR_BUY(ware, player, n)` | n | 20–120 | price or amount |

The disaster types of `DISASTER_AT` (S, `0x1003ef60`–`0x1003ef84`):

| Type | Uses | Files |
|---|---|---|
| `FIRE` | 158 | single40 |
| `VULCAN` | 70 | single40 (68), single30 (2) |
| `EARTHQUAKE` | 2 | single41 |
| `DROUGHT`, `PLAGUE` | — | known to the interpreter, unused |

### 5.1 State versus presentation (lockstep)
Read from their names, the conditions ask about simulation state: inhabitants, goods, money, units, areas, kills, treaties and timers (likely; the handlers were not traced). The unused `SELECTED_CURSORCONSTRUCTION` is likely the exception: by its name it asks about the local player's build cursor, which is UI state (OQ-123).

The actions split by what they change. The split is likely, read from the names and arguments; the handlers (O-16) decide.

**Simulation state:**
- `DISASTER_AT`, `MONEY_ADD`, `MONEY_SET`;
- `SET_DIPLOMACY`, `ADD_DIPLOMACY`, `SETTRIBUTE`;
- `ATTACK_AREA`, `ATTACK_OBJECT`, `KILLPLAYER`;
- `ASSIGN_OBJECT`, `ASSIGN_ALL_SOLDIERS`, `ASSIGN_ALL_SHIPS`, `MOVE_UNIT`, `SET_UNIT_VISIBLE`, `BEAMUP`, `BEAMDOWN`;
- `ADD_HEALTH`, `SUB_HEALTH`, `FREEZE_CITY`, `WAKEUP`, `SET_PROFILE`;
- `SETCONSTRUCTION_DRAWSTATE(_ALL)`, `UPGRADE_ENABLE`, `SET_PIONIER_ITEM`;
- `SET_FOR_BUY`, `SET_FOR_SELL`, `SETWARELOG`, `SET_REBELLION`;
- `CHEAT`, `ADD_SCORE`;
- the timers, `WON`, `LOST`, and every variable assignment.

**Presentation:** `QUESTLOG_SHOW`, `QUESTLOG_DRAW`, `PLAYVIDEO`, `PLAYWAVE`, `SHOWTEXT`, `SHOWMESSAGE`, `GOTO_SCREEN`, `MINIMAP_ALERT`, `CAMERA_MEMORY`, `CAMERA_RECALL`.

**Session:** `LOAD`, `SAVE`.

In lockstep, two arrangements are possible:
- every client runs the script on identical state, in which case the conditions must read only synchronised state;
- or one machine runs it and turns the state-changing actions into commands.

The shipped multiplayer scenarios do not exist in this build, so the original's choice is open (resurvey §2.3).

## 6. The interpreter's names (S)
**The count.** The task's 284 names are the lines that `strings -n 4` prints for `AnnoScene.dll` (the whole file) and that match `^[A-Z][A-Z0-9_]+$`. `tools/bigbrother.py keywords` reproduces that count exactly.

**How roles are assigned:**
- **Used names:** by their use in the scripts (P).
- **Unused names:** by the block of the string table they stand in (likely). The interpreter keeps its names in contiguous blocks:
  - the reader (`0x1003e4e0`–`0x1003e628`);
  - the actions (`0x1003e84c`–`0x1003f988`, ending in `SET`, `INITBLOCK`, `ELSE`, `IF`; the `COMPCAPS_*` parameters and the disaster types sit inside it);
  - the unit classes (`0x1003f9ec`–`0x1003faa0`);
  - the conditions (`0x1003fb18`–`0x10040254`).

**Short names:** `IF`, `SET`, `ALL`, `WON` and `THEN` are shorter than 4 characters or embedded (`"-THEN"`), so the 284 miss them; they are listed with a note.

Script vocabulary: 159 of the 284, plus 5 shorter names. Used = in at least one shipped script (P); unused = only in the DLL (S).

| Name | Address | Role | Uses | Files | First file |
|---|---|---|---|---|---|
| `SIGNALS` | `0x1003e5c8` | section | 50 | 50 | 00_1024_easyentry_01.sze |
| `SCRIPT_KERNEL` | `0x1003e5d4` | section | 50 | 50 | 00_1024_easyentry_01.sze |
| `CAMARAPOS` | `0x1003e5e4` | section | 50 | 50 | 00_1024_easyentry_01.sze |
| `PLAYERMAPPER_DEFINE` | `0x1003e5f0` | section | 50 | 50 | 00_1024_easyentry_01.sze |
| `AREA_DEFINE` | `0x1003e608` | section | 50 | 50 | 00_1024_easyentry_01.sze |
| `OBJECT_DEFINE` | `0x1003e618` | section | 50 | 50 | 00_1024_easyentry_01.sze |
| `STARTUP` | `0x1003e628` | section | unused (S) |  |  |
| `THEN` | `0x1003d888` | statement (below the 4-character rule) | 308 | 37 | endless01.sze |
| `SET` | `0x1003f970` | statement (below the 4-character rule) | 271 | 37 | endless01.sze |
| `INITBLOCK` | `0x1003f974` | statement | 35 | 35 | endless01.sze |
| `ELSE` | `0x1003f980` | statement | 3 | 1 | single33.sze |
| `IF` | `0x1003f988` | statement (below the 4-character rule) | 308 | 37 | endless01.sze |
| `FALSE` | `0x1003e564` | value | 1217 | 37 | endless01.sze |
| `TRUE` | `0x1003e56c` | value | 783 | 50 | 00_1024_easyentry_01.sze |
| `SCHUETTIINITSIG` | `0x1003e4e0` | internal signal (`TRUE` in scenarios, `FALSE` in saves; §3.5) | 50 | 50 | 00_1024_easyentry_01.sze |
| `OBJECT` | `0x1004021c` | condition subject | 109 | 22 | S01_01_01.sze |
| `AREA` | `0x10040224` | condition subject | 169 | 21 | endless01.sze |
| `PLAYER` | `0x1004022c` | condition subject | 100 | 27 | S01_01_01.sze |
| `MONOPOLY_ON` | `0x1003fb18` | condition query | unused (S) |  |  |
| `MONEY` | `0x1003fb24` | condition query | unused (S) |  |  |
| `FINANCE_EXPANSES` | `0x1003fb2c` | condition query | unused (S) |  |  |
| `FINANCE_INCOME` | `0x1003fb40` | condition query | unused (S) |  |  |
| `SELECTED_CURSORCONSTRUCTION` | `0x1003fb50` | condition query | unused (S) |  |  |
| `KRIEGSPAKT_STATUS` | `0x1003fb6c` | condition query | unused (S) |  |  |
| `WAR_STATUS` | `0x1003fb80` | condition query | 15 | 7 | S01_01_01.sze |
| `HANDELSPAKT_STATUS` | `0x1003fb8c` | condition query | unused (S) |  |  |
| `DEAD` | `0x1003fba0` | condition query | 30 | 7 | S01_02_01.sze |
| `SOLDIERCOUNT` | `0x1003fba8` | condition query | 5 | 3 | S01_06_01.sze |
| `MONUMENTCOUNT` | `0x1003fbb8` | condition query | unused (S) |  |  |
| `PLAYERKILLED` | `0x1003fbc8` | condition query | unused (S) |  |  |
| `SHIPKILLED` | `0x1003fbd8` | condition query | unused (S) |  |  |
| `SHIPLOST` | `0x1003fbe4` | condition query | 1 | 1 | single38.sze |
| `SOLDIERKILLED` | `0x1003fbf0` | condition query | unused (S) |  |  |
| `SOLDIERLOST` | `0x1003fc00` | condition query | 2 | 2 | single31.sze |
| `STARTSKILL` | `0x1003fc0c` | condition query | unused (S) |  |  |
| `WARECOUNT` | `0x1003fc48` | condition query | 10 | 3 | S01_05_01.sze |
| `SELLS` | `0x1003fc54` | condition query | unused (S) |  |  |
| `BUYS` | `0x1003fc5c` | condition query | unused (S) |  |  |
| `MAKES_LOSS` | `0x1003fc64` | condition query | unused (S) |  |  |
| `MAKES_PROFIT` | `0x1003fc70` | condition query | 1 | 1 | S01_03_01.sze |
| `SHIPCNT` | `0x1003fc80` | condition query | 5 | 4 | S01_05_01.sze |
| `SOLD` | `0x1003fc88` | condition query | unused (S) |  |  |
| `BOUGHT` | `0x1003fc90` | condition query | 5 | 5 | S01_01_01.sze |
| `MOOD` | `0x1003fc98` | condition query | unused (S) |  |  |
| `PAID_TRIBUTE_TO` | `0x1003fcdc` | condition query | 2 | 1 | S01_04_01.sze |
| `INHABITANTMOOD` | `0x1003fd10` | condition query | unused (S) |  |  |
| `INHABITANTCOUNT` | `0x1003fd80` | condition query | 39 | 18 | S01_01_01.sze |
| `INHABITANT_AMOUNT` | `0x1003fd90` | condition query | 4 | 4 | Single_15.sze |
| `CONSTRUCTIONCOUNT` | `0x1003fe40` | condition query | unused (S) |  |  |
| `CAN_CONSTRUCTION` | `0x1003fe54` | condition query | unused (S) |  |  |
| `HAS_CONSTRUCTION` | `0x1003fe68` | condition query | unused (S) |  |  |
| `OWNER` | `0x1003ff64` | condition query | 7 | 2 | S01_08_01.sze |
| `CONTAINS` | `0x1003ff98` | condition query | 13 | 3 | S01_01_01.sze |
| `TOUCHED_BY_OBJECT` | `0x1003ffa4` | condition query | 19 | 5 | S01_01_01.sze |
| `TOUCHED_BY_CONSTRUCTION` | `0x1003ffb8` | condition query | 30 | 3 | single33.sze |
| `TOUCHED_BY_ANYCITY` | `0x1003ffd0` | condition query | 1 | 1 | single42.sze |
| `TOUCHED_BY_ANYSOLDIER` | `0x1003ffe4` | condition query | unused (S) |  |  |
| `TOUCHED_BY_SOLDIERS_FROM` | `0x1003fffc` | condition query | 22 | 11 | S01_05_01.sze |
| `TOUCHED_BY_SOLDIER_FROM` | `0x10040018` | condition query | 26 | 5 | endless01.sze |
| `TOUCHED_BY_SHIP_FROM` | `0x10040030` | condition query | 53 | 9 | endless01.sze |
| `SOLDIERCNT_FROM` | `0x10040048` | condition query | 2 | 2 | S01_07_01.sze |
| `SHIPCNT_FROM` | `0x10040058` | condition query | 8 | 1 | single32.sze |
| `TOUCHED_BY_ANYSHIP` | `0x10040068` | condition query | unused (S) |  |  |
| `HEALTH` | `0x1004010c` | condition query | 1 | 1 | S01_11_01.sze |
| `KILLED` | `0x10040114` | condition query | 77 | 21 | S01_01_01.sze |
| `PLAYTIME` | `0x100401e8` | condition query | unused (S) |  |  |
| `TIMER_4` | `0x100401f4` | timer | 1 | 1 | single41.sze |
| `TIMER_3` | `0x100401fc` | timer | 1 | 1 | single41.sze |
| `TIMER_2` | `0x10040204` | timer | 2 | 2 | single40.sze |
| `TIMER_1` | `0x1004020c` | timer | 13 | 4 | S01_07_01.sze |
| `TIMER_0` | `0x10040214` | timer | 7 | 6 | Single_08.sze |
| `ADEL` | `0x1003fd50` | inhabitant level (condition argument) | 3 | 1 | single42.sze |
| `KAUFLEUTE` | `0x1003fd58` | inhabitant level (condition argument) | 6 | 5 | Single_06.sze |
| `BUERGER` | `0x1003fd64` | inhabitant level (condition argument) | 9 | 9 | S01_02_01.sze |
| `SIEDLER` | `0x1003fd6c` | inhabitant level (condition argument) | 3 | 3 | S01_01_01.sze |
| `PIONIER` | `0x1003fd74` | inhabitant level (condition argument); 6 of the 7 uses are a `CONTAINS` unit | 7 | 3 | S01_01_01.sze |
| `ALL` | `0x1003fd7c` | inhabitant level (condition argument; 17 `INHABITANTCOUNT`, 3 `SOLDIERCOUNT`) (below the 4-character rule) | 20 | 9 | S01_05_01.sze |
| `MEDICUS` | `0x1003f9ec` | unit class (condition argument) | unused (S) |  |  |
| `BEDIENER` | `0x1003f9f4` | unit class (condition argument) | 1 | 1 | S01_06_01.sze |
| `MUSKETEN` | `0x1003fa00` | unit class (condition argument) | unused (S) |  |  |
| `ARMBRUST` | `0x1003fa0c` | unit class (condition argument) | unused (S) |  |  |
| `BOGEN` | `0x1003fa18` | unit class (condition argument) | unused (S) |  |  |
| `MOERSER` | `0x1003fa20` | unit class (condition argument) | 1 | 1 | S01_06_01.sze |
| `MUSKTIER` | `0x1003fa28` | unit class (condition argument) | unused (S) |  |  |
| `KANONEN` | `0x1003fa34` | unit class (condition argument) | unused (S) |  |  |
| `SOLDATCOUNT_BELAGERUNGSMASCHINE` | `0x1003fa3c` | unit class (condition argument) | unused (S) |  |  |
| `KATAPULT` | `0x1003fa5c` | unit class (condition argument) | unused (S) |  |  |
| `REITER` | `0x1003fa68` | unit class (condition argument) | unused (S) |  |  |
| `LANZENTRAEGER_SCHWER` | `0x1003fa70` | unit class (condition argument) | unused (S) |  |  |
| `LANZENTRAEGER_LEICHT` | `0x1003fa88` | unit class (condition argument) | unused (S) |  |  |
| `SCHWERTKAEMPFER` | `0x1003faa0` | unit class (condition argument) | 1 | 1 | S01_01_01.sze |
| `OBJECT_FIGUR` | `0x10040234` | object kind (OBJECT_DEFINE) | 133 | 13 | S01_01_01.sze |
| `OBJECT_STADT` | `0x10040244` | object kind (OBJECT_DEFINE) | 80 | 17 | S01_02_01.sze |
| `OBJECT_ANLAGE` | `0x10040254` | object kind (OBJECT_DEFINE) | 37 | 8 | S01_04_01.sze |
| `SET_PIONIER_ITEM` | `0x1003e84c` | action | 7 | 1 | single30.sze |
| `ASSIGN_ALL_SHIPS` | `0x1003e860` | action | unused (S) |  |  |
| `ASSIGN_ALL_SOLDIERS` | `0x1003e874` | action | 4 | 3 | single31.sze |
| `SUB_HEALTH` | `0x1003e888` | action | unused (S) |  |  |
| `ADD_HEALTH` | `0x1003e894` | action | 2 | 1 | single30.sze |
| `ASSIGN_OBJECT` | `0x1003e8a0` | action | 53 | 7 | S01_08_01.sze |
| `MOVE_UNIT` | `0x1003e918` | action | 8 | 2 | S01_11_01.sze |
| `CHEAT` | `0x1003e94c` | action | 2 | 1 | S01_05_01.sze |
| `MONEY_ADD` | `0x1003e954` | action | 8 | 5 | endless01.sze |
| `MONEY_SET` | `0x1003e960` | action | unused (S) |  |  |
| `SETCONSTRUCTION_DRAWSTATE_ALL` | `0x1003e998` | action | 1 | 1 | endless01.sze |
| `SETCONSTRUCTION_DRAWSTATE` | `0x1003ea18` | action | 25 | 6 | endless01.sze |
| `UPGRADE_ENABLE` | `0x1003eae8` | action | unused (S) |  |  |
| `KILLPLAYER` | `0x1003eb3c` | action | 1 | 1 | single30.sze |
| `ADD_DIPLOMACY` | `0x1003eb48` | action | unused (S) |  |  |
| `SET_DIPLOMACY` | `0x1003eb58` | action | 30 | 14 | S01_02_01.sze |
| `SET_PROFILE` | `0x1003ed54` | action | 22 | 4 | S01_02_01.sze |
| `FREEZE_CITY` | `0x1003edfc` | action | 10 | 7 | S01_04_01.sze |
| `SET_REBELLION` | `0x1003ee9c` | action | unused (S) |  |  |
| `LOAD` | `0x1003eee4` | action | unused (S) |  |  |
| `SAVE` | `0x1003ef24` | action | unused (S) |  |  |
| `DISASTER_AT` | `0x1003f078` | action | 230 | 3 | single30.sze |
| `MINIMAP_ALERT` | `0x1003f0ac` | action | 23 | 7 | S01_01_01.sze |
| `PLAYVIDEO` | `0x1003f0e4` | action | 83 | 13 | S01_01_01.sze |
| `PLAYWAVE` | `0x1003f118` | action | 18 | 7 | endless01.sze |
| `SHOWMESSAGE` | `0x1003f124` | action | 1 | 1 | single41.sze |
| `SHOWTEXT` | `0x1003f130` | action | 72 | 24 | S01_01_01.sze |
| `LOST` | `0x1003f13c` | action | 55 | 21 | S01_01_01.sze |
| `WON` | `0x1003f144` | action (below the 4-character rule) | 35 | 35 | S01_01_01.sze |
| `CAMERA_RECALL` | `0x1003f148` | action | unused (S) |  |  |
| `CAMERA_MEMORY` | `0x1003f158` | action | unused (S) |  |  |
| `ATTACK_OBJECT` | `0x1003f214` | action | 6 | 1 | Single_24.sze |
| `ATTACK_AREA` | `0x1003f28c` | action | 27 | 5 | S01_02_01.sze |
| `SET_UNIT_VISIBLE` | `0x1003f324` | action | 117 | 7 | S01_01_01.sze |
| `BEAMDOWN` | `0x1003f338` | action | 1 | 1 | S01_02_01.sze |
| `BEAMUP` | `0x1003f344` | action | 1 | 1 | S01_01_01.sze |
| `TIMER_STOP` | `0x1003f34c` | action | 16 | 8 | Single_08.sze |
| `TIMER_START` | `0x1003f358` | action | 17 | 10 | S01_07_01.sze |
| `TIMER_SET` | `0x1003f384` | action | 25 | 10 | S01_07_01.sze |
| `QUESTLOG_SHOW` | `0x1003f428` | action | 669 | 28 | S01_01_01.sze |
| `QUESTLOG_DRAW` | `0x1003f438` | action | 42 | 11 | Single_10.sze |
| `SETTRIBUTE` | `0x1003f4e0` | action | 2 | 1 | S01_04_01.sze |
| `SETWARELOG` | `0x1003f624` | action | 5 | 5 | S01_01_01.sze |
| `SET_FOR_BUY` | `0x1003f630` | action | 5 | 3 | S01_02_01.sze |
| `SET_FOR_SELL` | `0x1003f63c` | action | unused (S) |  |  |
| `ADD_SCORE` | `0x1003f64c` | action | 2 | 1 | Single_24.sze |
| `WAKEUP` | `0x1003f6c0` | action | 17 | 3 | single30.sze |
| `GOTO_SCREEN` | `0x1003f7b8` | action | 14 | 13 | S01_01_01.sze |
| `COMPCAPS_PIRATEATTACKALL` | `0x1003eb98` | SET_PROFILE parameter | 6 | 1 | single34.sze |
| `COMPCAPS_SUSPEND` | `0x1003ebb4` | SET_PROFILE parameter | 12 | 1 | single41.sze |
| `COMPCAPS_BAULOGICOFF` | `0x1003ebc8` | SET_PROFILE parameter | unused (S) |  |  |
| `COMPCAPS_ADDDIPLOMATIE` | `0x1003ebe0` | SET_PROFILE parameter | unused (S) |  |  |
| `COMPCAPS_FIXDIPLOMATIE` | `0x1003ebf8` | SET_PROFILE parameter | unused (S) |  |  |
| `COMPCAPS_SETHELPANLAGE` | `0x1003ec10` | SET_PROFILE parameter | unused (S) |  |  |
| `COMPCAPS_MINEISEN` | `0x1003ec28` | SET_PROFILE parameter | unused (S) |  |  |
| `COMPCAPS_MINBAUMAT` | `0x1003ec3c` | SET_PROFILE parameter | unused (S) |  |  |
| `COMPCAPS_KAMPFTYP` | `0x1003ec50` | SET_PROFILE parameter | 4 | 2 | S01_02_01.sze |
| `COMPCAPS_ZIELGRUPPE` | `0x1003ec64` | SET_PROFILE parameter | unused (S) |  |  |
| `COMPCAPS_MAXSTADT` | `0x1003ec78` | SET_PROFILE parameter | unused (S) |  |  |
| `COMPCAPS_LAGER` | `0x1003ec8c` | SET_PROFILE parameter | unused (S) |  |  |
| `COMPCAPS_PROZENTE` | `0x1003ec9c` | SET_PROFILE parameter | unused (S) |  |  |
| `COMPCAPS_TRANSPORT` | `0x1003ecb0` | SET_PROFILE parameter | unused (S) |  |  |
| `COMPCAPS_HANDEL` | `0x1003ecc4` | SET_PROFILE parameter | unused (S) |  |  |
| `COMPCAPS_TYP` | `0x1003ecd4` | SET_PROFILE parameter | unused (S) |  |  |
| `EARTHQUAKE` | `0x1003ef60` | disaster type (DISASTER_AT) | 2 | 1 | single41.sze |
| `FIRE` | `0x1003ef6c` | disaster type (DISASTER_AT) | 158 | 1 | single40.sze |
| `DROUGHT` | `0x1003ef74` | disaster type (DISASTER_AT) | unused (S) |  |  |
| `PLAGUE` | `0x1003ef7c` | disaster type (DISASTER_AT) | unused (S) |  |  |
| `VULCAN` | `0x1003ef84` | disaster type (DISASTER_AT) | 70 | 2 | single30.sze |
| `CLEAR` | `0x1003f54c` | argument value (SETWARELOG, SETTRIBUTE) | 8 | 6 | S01_01_01.sze |

Not script vocabulary (the rest of the 284):

| Block | Addresses | Count | Names |
|---|---|---|---|
| code bytes | `0x10001a3c`–`0x1002dda3` | 29 | `SUVW`, `SUV3`, `PWQV`, `RPVU`, `IQVS`, `QSUVW`, `PSUV`, `RWPS`, `RWPQ`, `QSUV`, `YDVW`, `SUVW3`, `QQSVW`, `QSVW`, `SVW3`, `NWVS`, `WVS3`, `PPPPPPPP`, `QQV3`, `FGQPS`, `VC20XC00U`, `SVWU`, `QQVW`, `YYPW`, `RPWV`, `QQSV`, `QQS3`, `PSSW`, `PPPP` |
| other data (no string table) | `0x10031150`–`0x1004616c` | 3 | `PERT`, `HMXB`, `E0S0` |
| combat-tutorial object names (03T_Combat_01 OBJECT_DEFINE) | `0x1003b444`–`0x1003b63c` | 30 | `SCHIFF_ATTACK`, `SOLDAT_BOGEN`, `SOLDAT_KAVAL`, `SOLDAT_SCHWERT`, `SOLDAT_LANZE`, `SOLDAT_SPIESS`, `SOLDAT_VICTIM_1`, `SOLDAT_VICTIM_2`, `SOLDAT_VICTIM_3`, `SOLDAT_VICTIM_4`, `SOLDAT_VICTIM_5`, `SOLDAT_PINOIER`, `SOLDAT_BEDIN_KATA1`, `SOLDAT_BEDIN_KATA2`, `SOLDAT_KATAPULT`, `SOLDAT_BEDIN_MOERSER`, `SOLDAT_MOERSER`, `SOLDAT_BEDIN_KANONE`, `SOLDAT_KANONE`, `SOLDAT_SANI`, `SOLDAT_WOUNDED`, `SOLDAT_WOUNDER`, `SOLDAT_FOR_TURM`, `SOLDAT_FEUERMACHER`, `SOLDAT_BELAGTURM`, `SOLDAT_BEDIN_BELAG1`, `SOLDAT_BEDIN_BELAG2`, `SOLDAT_SCHLEUSS1`, `SOLDAT_SCHLEUSS2`, `SOLDAT_SCHLEUSS3` |
| runtime library (KERNEL32, CRT error ids) | `0x1003bd34`–`0x1003c118` | 13 | `KERNEL32`, `R6028`, `R6027`, `R6026`, `R6025`, `R6024`, `R6019`, `R6018`, `R6017`, `R6016`, `R6009`, `R6008`, `R6002` |
| profile chunk names (P: `PROFIL`, `USDAT`, `HELPPLAYER`, `SPEECH`, `VOLUME`, `SONGS`, `USGUID` are the chunks of the profile file the game writes, `docs/formats/user-dat.md`, T16) | `0x1003d190`–`0x1003d1e4` | 8 | `PROFIL`, `USDAT`, `HELPPLAYER`, `SPEECH`, `VOLUME`, `SONGS`, `SOGUID`, `USGUID` |
| scene and player chunk names (`sze.md`) with three `Texte.dat` section names (`COMPSPIELER_NAME`, `NATIVE_SPIELER`, `NATIVE_STADT`; `data-text.md`) and `PIRAT`, the island-file filter next to `"DUMMY.scp"` (`sze.md`). `PIRAT` also names the pirate player in 16 scripts' `[PLAYERMAPPER_DEFINE]` | `0x1003d1ec`–`0x1003d478` | 25 | `COMPSPIELER_NAME`, `WELT`, `SCENE`, `ANNOADDON1`, `ANNO`, `ENTRY`, `K_DATA`, `GUID`, `ALIASNAME`, `SPIELER`, `HEAD`, `BIGBROTHER`, `INFO`, `COMPUTER`, `COMP`, `INSEL5`, `SCHATZ`, `ERZE`, `ROHSTOFF`, `INSELLNK`, `COMPPROFIL2`, `AUFTRAGTEXT`, `PIRAT`, `NATIVE_SPIELER`, `NATIVE_STADT` |
| keys of the `SPIELER`/`COMP` record (AI parameters; T18, `docs/re/annoscene.md` §11) | `0x1003d2d0`–`0x1003d37c` | 16 | `CHEATLIMIT`, `SUSPEND`, `MINEISEN`, `MINBAUMAT`, `ZIELANZAHL`, `ZIELGRUPPE`, `MAXSTADT`, `HAUPTKLIMA`, `KAMPFTYP`, `SETTRIBUTSPERRE`, `OPTIONAL`, `PROZENTE`, `TRANSPORT`, `HANDEL`, `LAGER`, `FLAECHE` |
| discovery-tutorial area name (01Discover01 AREA_DEFINE) | `0x1003e4d8`–`0x1003e4d8` | 1 | `ERZ01` |

Total: 284 names = 159 script + 125 other.

## 7. Corrections to the second survey (`docs/research/game-files-resurvey.md` §2)
- **`SOLDAT_*` and `SCHIFF_ATTACK` are not the script's unit names.** They are the combat tutorial's object names, which the engine looks up by name (§3.1).
- **`HELPPLAYER` and `VOLUME` are not unused actions.** They are chunk names of the player profile (`SaveGame/<profile>/user.dat`).
- **`CHEATLIMIT` and `SETTRIBUTSPERRE` are not unused actions.** They are keys of the `SPIELER`/`COMP` record (`0x1003d2d0`–`0x1003d37c`; T18).
- **`SOLDATCOUNT_BELAGERUNGSMASCHINE` is not a condition.** It stands among the unit classes, so it is likely a unit-class argument.
- **`MONOPOLY_ON` and `STARTSKILL` are not actions.** They stand in the condition block (likely conditions).
- **Disaster use:** 226 of the 230 `DISASTER_AT` calls are in one mission, single40 (`FIRE` 158, `VULCAN` 68). The rest are `VULCAN` 2 (single30) and `EARTHQUAKE` 2 (single41). `DROUGHT` and `PLAGUE` are unused.

## 8. Reference and checks
- `python -I tools/bigbrother.py check <files>`:
  - all 50 `.sze` files: 50/50 parsed and re-emitted byte-identically;
  - the seven quicksaves of T16: 7/7.
- `python -I -m doctest tools/bigbrother.py`: the grammar on a synthetic script of our own.
- `python -I tools/test_bigbrother.py`: the doctests, plus synthetic cases:
  - the tree of a synthetic script: values, argument kinds, areas, slots, the camera;
  - every byte is one token, and the Latin-1 round trip;
  - each rule of §2–§3 refused when broken;
  - the interpreter's reading (T19):
    - the grouping, with C precedence shown to differ;
    - `&&`'s skip and `||`'s full evaluation, and which block runs;
    - the statements without `;` that swallow the next one, swallow nothing, end in a comment's `;`, or have no `;` after them.

  With `ANNO1503_GAME_DIR` set, it also runs the corpus cases (skipped otherwise):
  - all 50 scripts re-emit byte-identically;
  - the counts of §3.4 and §4;
  - `[SIGNALS]` equals `SCHUETTIINITSIG = TRUE` followed by the `SET` variables in order;
  - `AnnoScene.dll` has 284 names;
  - the interpreter's reading of the corpus: 28 mixed conditions in 10 files, each different from the C reading; three block statements without `;`, one lost statement (`MINIMAP_ALERT`, S01_09_01).
- `dump [--full] <file>...` prints the tree, with the grouping of mixed conditions and the statements without `;` marked (the reference dump below). `vocab <files>` and `keywords <AnnoScene.dll> <files>` produce the counts and tables of §4–§6. `quirks <files>` lists the mixed conditions as grouped and the statements without `;` with what they swallow (§3.4).
- The synthetic fixtures: `fixtures/synthetic/bigbrother_synthetic/`, written by `python -I tools/bigbrother_fixtures.py` (our own text; `test_bigbrother.py` checks that the committed files are its output).

### C++ reader: `anno::formats::bigbrother` (T20 stage 1)
- `src/formats/include/anno/formats/bigbrother.hpp`:
  - `parse` returns the tree or an `Error` with the byte offset, the line (1-based, in LF bytes) and the reason;
  - `emit` gives back the bytes (every byte is in one token);
  - `find_payload` finds the chunk in a scenario or a savegame;
  - `grouped` and `read_on` are the interpreter's reading of §3.4. No evaluation.
- **No limits compiled in:** the six section names of §3, the three object kinds of §3.1 and the three subjects of §3.4 are tables; numbers are kept as written. The names of §4–§6 are not known to the reader: a call is a name plus values.
- **One limit, an option:** `Options::max_condition_depth` (default `kMaxConditionDepth` = 256; the reference's `MAX_CONDITION_DEPTH`). It bounds how deep a condition nests: along any path of its grouping, the connectives plus the parentheses.
  - **Reason:** the parser and every walk of a condition recurse once per level. So does the interpreter (`0x10022bf0` recurses on each connective and group). Without a bound, a deep modded script would overflow the stack instead of being refused.
  - **The corpus needs 21** (P).
- **Tests:** `src/formats/tests/bigbrother_test.cpp` (synthetic) and `bigbrother_corpus_test.cpp` (`ANNO1503_GAME_DIR`). The corpus case asserts:
  - 50/50 byte-identical;
  - the counts of §3.4, plus 4 statements without `;`, 1 call without `()`, 24 timer and 429 variable primaries, 1,273 comments, 147 variables and 37 scripts with statements;
  - `[SIGNALS]` as in §3.5;
  - 28 mixed conditions in 10 files, each guarded by its first term and different from C's reading;
  - the three block statements without `;` and the one lost statement.

  Its output names files and counts only.
- `tools/agree.py --commands bigbrother`: `anno-dump bigbrother [--full]` prints the same bytes as `tools/bigbrother.py dump [--full]`. It runs on the synthetic fixtures (the accepted ones, then all with the refused ones) and, with the game dir, on the 50 `.sze`.

### Reference dump
`anno-dump bigbrother [--full] <file.sze|file.sav>...` and `python3 tools/bigbrother.py dump [--full] <file>...` print:
```text
# <path>
SCRIPT bytes=<n> tokens=<n> sections=<n> crlf=<n> lf=<n>
[OBJECT_DEFINE]
  <kind> "<name>" <n>[,<n>,<n>]                     one per line
[AREA_DEFINE]
  "<name>" <a>,<b>,<c>,<d>
[PLAYERMAPPER_DEFINE]
  "<name>" <slot>
[SCRIPT_KERNEL]
  INITBLOCK
    CALL <name>(<v>, ...)[  (no ())][  (no ;)][  <read-on note>][  (LOST: read into the statement before)]
    <name> = <v>[  (no ;)][  <read-on note>][  (LOST: ...)]
  SET <name> = <v>[  (no ;)]
  IF <condition (describe): terms blank-separated, in parentheses> THEN
     grouped by the interpreter: <grouping>          only for a mixed condition
    <statements>
  ELSE
    <statements>
[SIGNALS]
  <name> = TRUE|FALSE
[CAMARAPOS]
  <x>,<y>
[<OTHER>]  (not read: <n> tokens kept)               <n> without the header
TOKEN <offset> <NEWLINE|WS|COMMENT|SECTION|STRING|NUMBER|IDENT|OP> <text>   --full: every token
```
- **Read-on notes:**
  - `(reads on to the ';' at offset <n>; loses <k> statement(s))`;
  - `(the interpreter refuses the kernel: no ';' follows)`.
- **Values:** kernel values print as written, strings with their quotes. Every define number (objects, areas, slots, the camera) prints as its decimal value (`-0`, `007` print `0`, `7`).
- **Escaping:** in every line but `# <path>` (printed as given), each byte outside printable ASCII, and `\`, prints as `\xHH`. So a script's text is ASCII on every platform.
- **Refusals:** a file whose script is refused, or that has no single `BIGBROTHER` chunk, prints `NOPARSE` (the reason goes to stderr), and the exit code is 1. A file that cannot be read prints nothing on stdout (`<path>: cannot read` on stderr), and the exit code is 1.
