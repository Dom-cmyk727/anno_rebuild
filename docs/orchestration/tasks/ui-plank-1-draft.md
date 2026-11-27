# UI-PLANK-1: the text on the construction panel's banner (`PLANKEGROSS`) — draft for review

UI lane, 2026-11-27. Item 5 of the owner's `he_01` order ("all of them", 2026-11-17, `STATUS.md`); the known gap named
in the UI-TABS-1 B review ("The plank text: `PLANKEGROSS`, a known gap"). About 3 h, one concern, two stages. OQ
numbers 904-906.

**Evidence in hand.**
- The banner is two gadgets, built after the eight category tabs in `View.dll`'s construction chrome:
  - the plank sprite: class `0xb17`, 241 × 48 at (783, 252), `PLANKEGROSS` 1019 (`0x1002bd15`; `docs/re/ui.md` 2.4);
  - a text gadget over it (read for this draft, D):
    - class `0xb1c` (`0x1002bd7e`), 241 × 39 (`push 0x27; push 0xf1`, `0x1002bdc3`);
    - property `0x1c6` = 2 (`0x1002bdd7`, the centred-box style the info panel's names use, `hud.txt` UI-TABS-1 note);
    - parent `[this+0x5c0]` (`0x1002bdee`);
    - initial text property `0x1c4` = the 3-byte constant at `0x100a8404` (`0x1002bdfa`-`0x1002be23`).

  The live text is set elsewhere (`0xb1c` is the generic text class, about 20 uses in `View.dll`).
- **HE** (`he_01` `c7`, 3:57, a building of the public category armed): the plank reads **"Öffentl. Gebäude"**. That is
  the open category's name, abbreviated, centred, in a larger font than the 18-px boxes. The GOG captures
  `r2_construction_*` show text on the plank too: sprite-sad 38.75, and the oracle masks (800, 258) 200 × 22,
  `docs/re/ui.md` 2.8.
- **The rebuild** draws the plank sprite only (the ghost-road shots of ROADS-2b show it empty). The text-box path
  exists: `ui_mock.cpp` `text_box`, `hud.txt` elements with `align centre` and method `glyph-match`
  (`tools/ui_layout.py info-compare`).

## Stage A — read (RE, docs only) — DONE 2026-11-27

Result (`docs/re/ui.md` 2.4, row "Banner text"): the text is entry *tab index* of `TOOLTIP:BAUMENU_EX`, set only by
the tab handler `0x1002bf90` (on a tab click and from one panel message, OQ-904); arming a building or the road leaves
it. GOG lines: Public buildings, Squares, Streets, Farms, Plantations, Workshops, Coastal buildings, Pits, Mines,
Military installations, Ornamentals. Font and baseline: OQ-905 (B measures them). OQ-906 stays free.

The plan as approved:

1. Who writes property `0x1c4` of this gadget, and from which string list:
   - the tab handler `0x1002bf90` stores the index at `[this+0x5f0]`;
   - candidate sources: `TOOLTIP:BAUMENUTAB` (eight lines, OQ-539), a separate short-name list, or the type's group
     text.
2. What the plank shows:
   - in every panel state (category grid; info/placement panel with a type armed; road armed; demolish mode; nothing
     open);
   - whether it ever shows the building's name instead.
3. The font, the colour and the text position (the gadget is 39 high on a 48 plank).
4. Results go to `docs/re/ui.md` 2.4 (new rows) and `docs/BUILDINGSOBJECTS.md` if a state matters there.
   Unread → OQ-904..906.

**Check:** GOG and HE files, and the zoomed captures, per the owner rule of 2026-11-14 (UI bit-identical; when in doubt
check both editions). Read the GOG string itself from the game text (`anno-dump`), since HE's is German. One GOG Wine
capture of the public grid if the existing `r2_*` shots do not settle the string (pre-authorized, silent).

## Stage B — draw (app, about 1.5 h)

- **Data and code:**
  - a `hud.txt` element `plank_text` (pos, size 241 × 39, font file, `align centre`, method `glyph-match`, the
    GOG shot) with its source addresses;
  - the string looked up from the game text by the open category (or whatever stage A finds), never typed in code;
  - drawn through the existing `text_box` path, so the CPU and GPU-overlay paths share it.
- **Tests:**
  - a unit case on the string choice per panel state;
  - glyph compare against the **GOG** shot `r2_construction_public` (the oracle's shot set; plank mask removed):
    0 differing pixels, as UI-INFO-1. HE `c7` is evidence for the behaviour only (German text, scaled window). If the
    GOG shot is not at hand, one GOG Wine capture of the public grid after "PC free";
  - a wiring script from the default start view (B, each of the eight tabs, a fact `ui.plank_text`);
  - a failure proof (wrong category index → the compare fails).
- **Owner test:**

  ```
  anno1503 --play data\maps\lan5.txt --game-dir E:\anno1503\game\app --construction-menu on --layout data\ui
  ```

  B, click through the eight tabs, arm a building, arm the road: the plank names the category as in the original.

**Not in it:** the other `PLANKEGROSS` users (warehouse city name, options page names, ship panels): each is its own
panel's packet. No HE-specific behaviour (GOG is the faithful base; HE only as evidence).

**Risk:** the GOG plank's font may differ from the info boxes' `MainSmall.zei`; stage A names it before B draws.
