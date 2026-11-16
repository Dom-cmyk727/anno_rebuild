# UI-STATISTICS-1 (draft): the player statistics screen, faithful base

**DRAFT 2026-11-16** (UI-PANELS-PREP). Not released. Spec: [`docs/re/ui-statistics.md`](../../re/ui-statistics.md)
(evidence letters D / P / M-HE / A / U, open questions OQ-670..679). The owner plans to replace this screen later
with a co-developer's design (`ui_5` 7:41–8:18); the faithful screen is still the base and the replacement
becomes an option on top of it (owner rule 2026-11-14/16).

Scope: `tFrameSpielerStatistik` (key `-`, options button 3) with its two views and two modes. The rating screen
(key `q`, OQ-677), the city list and the balance pages are **out of scope** (separate packets). Each stage below is
one concern of about 2–4 hours, reviewed by the orchestrator at handover (no executor-side reviewers).

## Stages

Order: S0a … S0d (evidence), S2 (static screen), the sim tasks M3 → M1 (sampler) → S1 (the UI reads the
sim's history), S3 (charts), S4 (clicks); M2 and M4 in the sim lane alongside. (Fix round after the
orchestrator's review 2026-11-16: S0 was too large; the first draft's S1 rebuilt the city tick's sampler in
the app and needed rate formulas that are unread, OQ-676: that is sim work, now the sim tasks M3 and M1.)

### S0a — Frame reads: pause, closing key, supply series (2–3 h, research only)
- Read in GOG `AnnoFrame.dll`: the frame's message slot 19 `0x10013cb0` (cases `0x10013d38`) and the frame
  manager's key path for a full frame → closing key (OQ-671) and pause (OQ-670); the supply draw `0x10014410` in
  full → series/colour mapping (OQ-672).
- **Acceptance:** each OQ answered with D evidence or narrowed with what is still missing; spec updated.

### S0b — Chart labels and the rate getters (2–3 h, research only)
- `AnnoDisplay.dll` `tDrawLayerDiagramm` → label placement and font (OQ-673). In `AnnoWelt.dll`: the farm, mine
  and base rate getters `+0x94`, the second argument of type `+0xc8(j, 0)`, city slot `+0x60` (OQ-676). M3 needs
  these.
- **Acceptance:** as S0a.

### S0c — Memory records and the saved history in the corpus (2–3 h, research only)
- The `SEL_n` reader in `AnnoWelt.dll` and the flag's values (OQ-675, meaning only: the owner decided ownership);
  corpus survey of `SEL_n` and `STST2` contents with `tools/` readers (P). M2 and M4 need these.
- **Acceptance:** as S0a, with the corpus counts written into the spec.

### S0d — Measurement: HE recording and one GOG shot set (2–3 h)
- Analyse the owner's recording (recipe below, common set-up: the frame is 1:1 at a 1024 × 768 client); add one
  GOG Wine-runner shot set of the screen (key `-`, both views, both modes, one good selected) so the GOG picture
  exists too (OQ-674, 678, 679).
- **Acceptance:** the spec's rectangles re-measured on the lossless frames (positions equal to the code
  rectangles, or the difference reported); the GOG shots named in the spec.

### S1 — The sim's statistics history in the snapshot (2–3 h; after M1)
- Add to `PlaySnapshot` per local-player city a read-only view of M1's history block (divisor, max capacity, the
  three row sets, their intervals; spec §3.1, §5.3). No sampling in the app, no state, stream, hash or draw
  change.
- **Acceptance:** unit tests: the snapshot's block equals the sim city's block byte for byte after N samples; a
  pause publishes no new sample.


### S2 — Static screen drawing (3–4 h)
- Draw the frame at 1024 × 768 (and the HE-style fit-to-height presentation, OQ-678 default) from
  `WARENSTATISTIK_BK1/BK2`, `ADDON_SPSTAT_*`, `ADDON_LAGERPROD`, `ADDON_LISTBUTTON`, the goods icon groups
  (grey/colour by selection), the city list text (`ADDONSTAT_1` line 0 + city names) and the axis captions, at
  the code rectangles of spec §2.2/§2.3. Layout as data (`data/ui/` style), not constants.
- **Acceptance:** per-element sprite SAD against the HE frames `ui_5` 7:41 (empty), 8:25 (wood), 8:38
  (supply) after the spec's conversion, each element within the tolerance the UI lane uses for HE compares;
  every goods icon id matches (the prep's SAD run identified 13/13 supply icons and the goods of all 37 stocks
  icons; one raw-goods icon, wool, scored closer to its colour twin than to its grey texture — recheck at full
  resolution); on the lossless frames of the new recording (common set-up, frame 1:1) every element matches its
  texture 1:1; GOG 1024 × 768 positions equal the code rectangles exactly.

### S3 — Charts (3 h)
- Stocks/productivity chart: axes (x 0..300, ticks 10 s; y 0..max-capacity step 10 or 0..100 step 10), red axes,
  white labels, one polyline per selected goods with the 8-colour slot table and per-sample markers, "Alle
  anzeigen" = average over cities. Supply view: 13 small charts (goods 32 skipped), two series each per OQ-672.
  Built against synthetic history blocks in the spec's layout first; live data once S1 has landed.
- **Acceptance:** HE 8:25 frame: wood line from (t 0, 50 t) to the drop to ≈ 44 t and end at t = 295 within
  ±2 px; the % frame (8:32.8): y labels 0..100, line on 0; supply frame: food and beer lines at ≈ 1 t, others on
  the axis; a synthetic two-city test proves the average and the y-max quirk.

### S4 — Clicks and keys (2–3 h)
- `-` opens (and the options button), exit button and the S0a key close, view switch, Stores/Productivity, goods
  toggle with the 8-slot limit (9th refused, icon un-pressed), memory I–VIII (selection per memory, list rebuilt
  in goods order), city list choice and scroll. The owner decided OQ-675 on 2026-11-16: the memories are
  **saved in the game**, as in the original. They are player state, saved and loaded with the game and kept in
  sync in multiplayer. Until the sim has that field (the sim lane's packet), S4 keeps them in a per-player
  store and puts it behind one interface so the field can replace it.
- **Acceptance:** scripted UI tests for each action; a replay of the HE sequence (open, select wood, %, back,
  supply, close) produces the same states as `ui_5` 7:36.88–8:50.45; the game clock does not advance while the
  screen is open if OQ-670 confirms the pause.

### Sim tasks of their own (sim lane, from spec §5; not UI stages)
- **M3** (after S0b) the measured-rate getter for every production class (`+0x94`) and per-group residents.
- **M1** (after M3) faithful sampler in the city tick (step i semantics, `City` gains the 0x2874-byte history
  and the three countdowns; ticks only, never wall time; hashed; native save stream). Its unit tests are the
  first draft's S1 tests, against hand-computed rows: stock divisor at 255/256/257 t, max capacity only grows,
  a productivity building with display rate 0 counts as 0 %, supply y max by spec §3.1
  (`m = (max + 0x80) >> 8`, then `m + (10 − m mod 10)`): 0 → 10, 9.4 t → 10, **9.6 t → 20** (the first draft
  said 10; corrected by the orchestrator's review 2026-11-16), 10 t → 20; the inn record counted as alcohol;
  the 61st sample drops the oldest; a determinism test (same tick stream → same bytes).
- **M2** (after S0c) legacy import of `STST2` (193 corpus records, 10 356 bytes) and `SEL_0..7`.
- **M4** (after S0c)
  the memories as player state (owner decision 2026-11-16, OQ-675: saved and loaded with the game, synchronised
  in multiplayer): the eight slots in the player, saved, and the commands `SetStatsGoods{player, memory, goods,
  flag}` and `SetStatsCity{player, memory, city_entry}`.

## Recipe for the owner's HE recording (closes OQ-670..674, 678, 679)

Set-up: the common one, [`ui-panels-he-recording.md`](ui-panels-he-recording.md) (window 1024 × 768, where the
statistics frame is drawn 1:1; OBS lossless; test clip first); files `HE_STAT_R1` …. A running game with
**two or more own cities** (not the tutorial) and some production (woodcutter, fisher, a farm chain), ideally
past 5 minutes of game time. Narrate each action in one short sentence; hold the mouse still for 2 s after each
click. Total about 4 minutes.

1. (0:00) Note the game time or watch a moving ship. Press **`-`**. Wait 5 s without moving the mouse.
   Press **ESC**. If the screen stays, press **`-`** again; if it stays, click the red X. Say which closed it.
2. Reopen with **`-`**. Hover, 2 s each, over: the X, button I, the left and the right big button, the wood icon,
   the food icon, the iron ore icon, the two top-right buttons, the top and bottom ornament buttons, "Alle
   anzeigen", one city name.
3. Click **wood**, **tools**, **bricks**, **food**, **cloth**, **salt**, **beer**, **wine**, then a **9th** good (say
   what happens); click **wood** again to unselect it.
4. Click the right big button (%), wait 3 s; click the left one.
5. Click **one city name**, wait 3 s; click **"Alle anzeigen"**, wait 3 s.
6. Click **II**, select **wood** only; click **I** again (does the old selection return?).
7. Click the **top-right** button (overview), wait 5 s, scroll the city list once each way, click the city, then the
   top-left button.
8. Close (whatever closed it in step 1). Open the **options tab** and click the chart button there; close again.
9. Save, load the save, press **`-`**: is the history (lines) still there, is memory II still wood?
10. Pause check: put a moving ship in view, press **`-`**, wait 30 s, close; say whether the ship (or the gold
    figure in the status bar) moved compared with the moment before opening.
11. Optional, a separate clip (OQ-678, the exception of the common set-up): the window at another size, for
    example 1920 × 1080, OBS canvas at that size; press **`-`**, hold 5 s, close.

Deliver the video plus the save from step 9; the UI lane extracts full-resolution frames (no pixels go into git).
