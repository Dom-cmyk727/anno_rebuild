# Plain-text data files (`Texte.dat`, `Data/*.dat`)

**Status:** partial — the line grammar of every file is complete and round-trips byte-identically (P); `Texte.dat` sections and the `[ANLAGEN]` names are decoded (P); the record layouts of `BGruppen.dat`, `Wachstum.dat`, `wegspeed.dat` and `Felder.dat` are confirmed over the corpus (P). The meaning of most fields is open (OQ-029).

## Files covered

| File | Bytes | Grammar | Sections | Content (from section and key names; meaning ? OQ-029) |
|---|---|---|---|---|
| `Texte.dat` | 46,050 | texte | 118 | all UI and object texts of the installed language (English in GOG 2.0.0.5) |
| `Data/BGruppen.dat` | 12,502 | keyed | `WAREN`, `BGRUPPE`, `BAUSTUFEN`, `END` | goods with prices (`Id`, `Preis`), population groups (`BGruppeNr`, `Wohnmax`, `Ware`, `Menge`, `Priority`, `Force`, …), building stages |
| `Data/Wachstum.dat` | 43,232 | keyed | `WACHSTUM`, `END` | per ground texture (`Id`, comment names the `.bmp`): a `Size: 6, 6` grid of `Proz` percentages, `Klima`, `Bridge` |
| `Data/wegspeed.dat` | 1,245 | keyed | `WALKMASK` | `Ob`/`Un` pairs with eight `Mask` rows each (German comments: slope classes) |
| `Data/Felder.dat` | 8,372 | keyed | `FILE`, `COLOR`, `TEXTURE`, `END` | build description of `textures\felder.tex`: palette names (`Farbtabellen`), texture list |
| `Data/Highmap.dat` | 687 | keyed | `HIGHMAP` × 2, `END` × 3 | two `Id` blocks of 8 `Tab` rows |
| `Data/ScapeIcon.dat`, `htmlpic.dat`, `statusbar.dat` | 1,441 / 359 / 2,557 | keyed | `FILE`, `COLOR`, `TEXTURE`, `END` | texture build descriptions (`Access`, `Pos`, `Size`, `Segments`, `Colorkey`) |
| `Data/music.dat` | 2,711 | keyed | `GLOBAL`, `STANDARD`, `END` | music files (`Path`, `Group`) |
| `Data/font.dat` | 723 | keyed (tool script) | — | font-tool script (`PATHFONT = …`, `EndObj;`); 3 lines are not entries |
| `Data/ControlChars.bat`, `ScapeChars.bat` | 135 / 139 | keyed (lines only) | — | batch calls of a `konvertchars` tool; no entries |

`Data/Debug.dat` (15 MB) is a Bink video, not text, and is not covered.

All counts on this page: P, `crates/formats/tests/text_corpus.rs` and `anno_dump_matches_text_py_corpus`.

## Lines (all files)
- Bytes are Windows-1252 text and are kept raw. Non-ASCII bytes: `Texte.dat` 0x85 `…`, 0x92 `’`, 0x96 `–`; the `Data/` files only German umlauts in comments and paths (0xFC `ü`, 0xE4 `ä`, 0xF6 `ö`, 0xDC `Ü`).
- Every line ends with CR LF; no lone CR or LF in any file. Every file except `font.dat` ends with CR LF.
- Line splitting: at `\n`, a `\r` before it belongs to the terminator; the bytes after the last terminator form a last line (empty if the file ends with one). Joining the lines with their terminators restores every file byte-identically.

## Keyed grammar
Each line is one of (after removing a `;` comment and surrounding spaces/tabs):

| Kind | Form | Notes |
|---|---|---|
| blank | empty, whitespace, or a comment only | `;` starts a comment outside `"…"`; no `;` occurs inside quotes in the corpus, so the quote rule is unobservable there |
| section | `[NAME]` | `[END]` closes a file or block (`Highmap.dat` nests `[HIGHMAP] … [END]` blocks inside its `Id` entries) |
| entry | `Key: value` | `Key` = `[A-Za-z_][A-Za-z0-9_]*`, spaces or tabs before the colon allowed; value = the rest, trimmed |
| other | anything else | 8 in the corpus: `Felder.dat` line 48 `Pos     0, 0` (key without colon, presumably a typo the game tolerates — ? OQ-029), 3 tool-script lines in `font.dat`, 4 batch lines |

Values are comma-separated fields (split outside `"…"`, each trimmed): numbers (`100, 90`), identifiers (`EISENERZ`), quoted strings (`"textures\felder.tex"`, backslashes literal), bit strings (`11111111`).

| File | Lines | Blank | Sections | Entries | Other |
|---|---|---|---|---|---|
| `BGruppen.dat` | 615 | 178 | 4 | 433 | 0 |
| `Felder.dat` | 349 | 96 | 4 | 248 | 1 |
| `Highmap.dat` | 30 | 7 | 5 | 18 | 0 |
| `ScapeIcon.dat` | 62 | 27 | 4 | 31 | 0 |
| `Wachstum.dat` | 1,567 | 420 | 2 | 1,145 | 0 |
| `font.dat` | 27 | 14 | 0 | 10 | 3 |
| `htmlpic.dat` | 25 | 8 | 4 | 13 | 0 |
| `music.dat` | 115 | 69 | 3 | 43 | 0 |
| `statusbar.dat` | 119 | 26 | 4 | 89 | 0 |
| `wegspeed.dat` | 63 | 12 | 1 | 50 | 0 |

(Line counts include the empty last line after the final CR LF.)

## Records

A keyed section is a flat list of entries; a record is a run of entries that begins with a start key and ends before the next one. The start key is a property of the file and section (table below), read with `text::split_records`. Numbers: integers are decimal with optional `-`; decimals occur only in `BGruppen.dat` (`Menge`, `VermehrTab`), with at most two fractional digits (61 with one, 6 with two), so `text::fixed(field, 2)` reads every one exactly in hundredths — no floating point reaches the simulation.

| File, section | Start key | Layout (P) | Cross-check (P) | Meaning |
|---|---|---|---|---|
| `BGruppen.dat` `[WAREN]` | `Id` | 56 records; keys from `Id`, `IdSec`, `Forsch`, `BauFirst`, `BauLast`, `Preis`, `Bonus`, `PreisLevel`; 13 key patterns, most common `Id Preis` (25); `Preis` at most once, a positive integer, in 48 records | — | goods (`Id` = goods name such as `EISENERZ`) and their prices ? OQ-029 a |
| `BGruppen.dat` `[BGRUPPE]` | `BGruppeNr`, inside it `Ware` | 5 population groups (`PIONIER` first); header `BGruppeNr Wohnmax BauFirst BauLast [NoUpgrade] [NoDowngrade] NahrungTab BedarfTab InfraTab VermehrTab` (`VermehrTab` = 5 decimals); then needs, each exactly `Ware Menge Priority Force` (47 in total; `Menge` decimal, `Priority` `HIGH`/`NORMAL`/`LOW`, `Force` `TRUE`/`FALSE`) | — | needs per population level ? OQ-029 a |
| `BGruppen.dat` `[BAUSTUFEN]` | — (one entry each) | 31 `Stufe: <name>, <int>, <int>` (`INFRA_EVER, 0, 0` … `INFRA_STUFE_TRIUMPH, 4, 65000`), 5 `Wissen: <name>, <int>` | `BauFirst`/`BauLast` values name these stages | building stages ? OQ-029 a |
| `Wachstum.dat` `[WACHSTUM]` | `Id` | `Size: 6, 6`, then 146 records: `Id`, six `Proz` rows of six integers 0–100; 46 add two `Klima`, 5 add six `Bridge` rows | every `Id` is a `Felder.tex` texture id (all except 116); the comment above a record names that texture's `.bmp`; 6 × 6 = the ground cells of a texture (`GFXID` `u`, `v`, [`insel.md`](insel.md)) | a value per ground cell, presumably growth in % ? OQ-029 b |
| `wegspeed.dat` `[WALKMASK]` | `Ob` | 5 records `Ob Un Mask × 8`; each `Mask` exactly eight `0`/`1` digits | — | walkability by slope class (German comments) ? OQ-029 c |
| `Felder.dat` `[TEXTURE]` | `Name` | after a header (`Path Size Storename Id Typ Table`), 147 records `Name [Table]` | 147 = the textures of `Felder.tex`, in the same order (source names differ only where textures were renamed, e.g. `Wiese012.bmp` / `Wiese012a.bmp`) | build list of `Felder.tex` (tool file) |

One row breaks the comma rule: `Wachstum.dat` line 765 `Proz: 100, 100, 100   10,    20,  50` has five comma fields but six numbers. `text::ints` (commas and/or whitespace as separators) reads six; whether the original does is open (OQ-029 g).

## `Texte.dat`
- Section headers `[NAME]` on a line of their own; names may contain `:` (`[STARTUP:PROFILE]`, `[TOOLTIP:BAUMENU_WEG]`). 118 sections, all names distinct, the last is `[END]` with no lines. No lines before the first header. No comments: `;` does not occur.
- Every other line is a text, kept verbatim (2,250 lines including headers, 2,251 with the empty last one). Texts contain C format markers (`%s`) and the two characters `\n` as a line-break marker (`Version conflict!\n You can not …`).
- `[ANLAGEN]` (line 1, 326 lines): `<id>:<name>`, e.g. `104:Forest`, `501:Pioneer house`. The 326 ids are distinct and are all `Anlagen.dat` entry ids (P), i.e. this is the name table for buildings and objects; not every one of the 921 entries has a name. Ids are not sorted.
- Other sections are plain lists; presumably the game addresses them by position (? OQ-029). Name lists: `[SHIPS]` (107 ship names, e.g. `Altair`, `America`, `Escapade`), `[STAEDTE]` (160 town names), `[COMPSPIELER_NAME]`, `[NATIVE_STADT]`, `[NATIVE_SPIELER]`; goods names in `[WARE_*]`; UI texts in `[STARTUP:*]`, `[TOOLTIP:*]`, `[MESSAGEBOX]`.
- The language: this is the English GOG file. Other language editions presumably ship their own `Texte.dat` with the same sections (unverified; PLAN "Language gate").

## Reference dump

`anno-dump keyed <file>...` = `python3 tools/text.py keyed <file>...` and `anno-dump texte [--full] <Texte.dat>...` = `python3 tools/text.py texte [--full] …` print the same bytes (`crates/tools/tests/agree_python.rs`, corpus and synthetic files).

```text
# <path>
LINES count=<lines> crlf=<CR LF terminators> final=<1 if the last line is empty>
keyed, one line per non-blank line:
SECTION line=<n> name="<quoted>"
ENTRY line=<n> key="<quoted>" fields=<number of fields> value="<quoted>"
OTHER line=<n> text="<quoted, comment removed, trimmed>"
texte:
TEXTE sections=<n> preamble=<lines before the first header>
SECTION line=<n> name="<quoted>" lines=<n> hash=<FNV-1a 64 of every line followed by \n>
  ID id=<id> text="<quoted>"        each [ANLAGEN] line, or `  ID BAD text=…`
  L "<quoted>"                      with --full: every line of the section
```

Quoting: printable ASCII except `"` and `\` as is, every other byte `\xHH`. Exit code 1 if an `[ANLAGEN]` line is not `<id>:<text>`.

## Open
- Meaning of the keyed records: units (`Preis`, `Proz`, `Mask` bits), how `Ob`/`Un` and `Klima`/`Bridge` are used; the missing comma in `Wachstum.dat` line 765 — OQ-029.
- How the game indexes the plain `Texte.dat` lists — OQ-029.

## Tests
- Unit tests: `crates/formats/src/text.rs` (line round trip including LF and missing final terminator, keyed kinds, fields, records, `ints`, exact decimals, `Texte.dat` sections, `id:text`).
- Corpus tests: `crates/formats/tests/text_corpus.rs` (`keyed_corpus`, `texte_corpus`, `records_corpus`): byte-identical line round trip of all 11 files, the counts above, both anomalies, `[ANLAGEN]` ids against `Anlagen.dat`, the record layouts and their cross-checks against `Felder.tex`.
- Reference tool output: `anno_dump_matches_text_py_corpus`, `anno_dump_matches_text_py_synthetic`.
