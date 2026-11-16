# UI-PANELS-PREP: the orchestrator's review (2026-11-16)

`task/UI-PANELS-PREP` `bd4b07e0` against main `ac02d4a2`: four specifications under `docs/re/` (warehouse and
trade, ships and diplomacy, statistics, options and main menu), four draft packets, 46 lines of open questions.
Documents only, 2,230 lines.

**How it was reviewed:** an independent reviewer (a subagent given the branch and the project's rules, not the
orchestrator's opinion) read all nine files and checked 42 concrete claims against the GOG files by disassembling
the cited addresses, parsing the data files and matching sprites on the reference shots. The orchestrator then
checked three of its refutations again (S6, T7 and W7 below); all three hold. Line numbers are the branch's.
W, S, T, O are the four specifications in the order above.

## Verdict

**ACCEPTED WITH FIXES; not landed yet.** 33 of 42 checked claims are confirmed, among them the whole treaty state
machine, the trade and ship command packers, the statistics sampler and the main menu's rectangles. Nothing
original that must come out was found (no pixels, no script text, no paragraph of game text). Eleven points are
to fix on the branch first.

## To fix before landing

1. **`docs/open-questions.md` conflicts with main.** Main appended OQ-628 to 640 where the branch appended OQ-650
   to 689; keep both. The other eight files merge cleanly.
2. **W:45-47, 207-208, 265, 291-298 (OQ-652): "both History Edition videos (1920 x 1080) draw the right panel with
   the 8 set".** It contradicts S:15, O:39-40, T:9 and main's `he-ui-scale.md` section 1. The recordings are a 0.75
   downscale of a 2542 x 1351 client, and the specification's own positions fit 0.75 times the plain-set layout
   within 2 px (tab predicted 1716.8, 251.5; measured 1716, 251). Retract it, or reduce OQ-652 to "confirm 0.75
   times the plain set". The packet builds on it (`ui-warehouse-1.md`:43, :121, :147-148).
3. **W:87, 98, 127, 208, 255, 277: `STADT_BILANZ_BK` is at (779, 301), not (779, 300).** Sprite match on
   `f15_kontor.png`: mean 11.1 at 301 against 20.1 at 300 (the specification's own "20.52"). Main's `hud.txt`
   already has `info_bk` at (779, 301). Correct the family: page origin (779, 244), the predicted
   `VKEK_HINTERGRUND` position, the offset. Say that main's `hud.txt` panel `info` already holds this page's
   background, eight tabs and deny button.
4. **O:204-206: "positions come from a layout table indexed by gadget number (vtable +0x24)".** Refuted: slot +0x24
   (`0x10019830` in both the options and the city panel's vtable) only stores the gadget pointer at that index in a
   vector. Replace with "unknown, OQ-651"; W:77-80 and S:401-402 already say unknown. The read narrows OQ-651.
5. **S:233-236 and OQ-661 (S:403): the player-info loop builds seven text gadgets, not six.** It ends
   `inc edi; cmp edi, 6; jle` (`View.dll` `0x100800c7`). The "unknown seventh row" and the implied History Edition
   difference go away.
6. **T:218 and OQ-676 (T:371): the row "64 bytes at +0x28e4 not written by the sampler" is wrong.** The loop at
   `AnnoWelt.dll` `0x1002d615` writes `[city + 0x28de + g]` for g = 0 to 69, which covers them.
7. **W:111-112: the records 0x7c and 0x8a are incomplete.** 0x7c is 13 bytes: mode at offset 3, goods at 5, bytes 7
   and 8 not written, value at 9. 0x8a is 9 bytes: bytes 3 and 4 not written, the u32 at 5. Say that these are the
   multiplayer client's packers.
8. **W:204: the History Edition id shift is misstated.** It is +5 for GOG ids 6 to 824, +4 for 825 to 1487 and +6
   from 1488, as S:90 and O:350 have it.
9. **W:100, 224, 257: the balance rule is over-claimed.** Both captures have rows 3, 4 and 5 at zero, so they show
   only row 6 = row 1 minus row 2. The other signs are a guess: move them to OQ-655. Stage W1d implements the rule
   as written (`ui-warehouse-1.md`:71-72).
10. **The open questions of three specifications lack the register's fields.** OQ-650 to 689 are all present and
    no number is reused, but in `open-questions.md` they are an index (a title and a link). Only the warehouse
    specification gives status, question, what is known and how to resolve; S:394-429, T:347-381 and O:432-466 are
    bullets. Give every entry the four fields, in the specification or the register.
11. **OQ-675 is recorded two ways.** The owner's decision (the statistics memory slots are saved state) is in the
    two packets; T:313-314, T:364-368, the index line and packet line M4 still read as undecided.

## Notes for the packets, before any stage is released

- **Four different recording set-ups** are asked of the owner (1920 x 1080 window; 1024 x 768 lossless PNG;
  1080p or larger; 2542 x 1351). Only lossless frames serve a bit-identical base. Agree on one and say it once.
- **Stages too large or with more than one concern:** ships 1a, 1d, 1h, 1i; statistics S0; options S3, S6, S7,
  S8a; warehouse W1f (it needs the goods chooser no stage draws).
- **Order gaps:** ships 1f needs OQ-669, which 1a does not list; ships 1g needs the rule of OQ-664 from the later
  stage 1h; warehouse W1e needs OQ-659, which W1a does not read; W1c opens on a Kontor click before W1d brings the
  building-to-city lookup.
- **Stale after the owner's answers:** options S3 still tests the History Edition fit although the owner declined
  it; statistics M4.
- **A wrong expected value:** `ui-statistics-1.md`:35-36 has "9.6 t gives 10"; by the formula at T:219 it gives 20
  (9.4 t gives 10).
- **Lane boundary:** statistics S1 would rebuild the city tick's sampler in the app and needs rate formulas that
  are unread (OQ-676); options S6 puts a display name inside the save; warehouse W1e needs slot counts the sim does
  not keep. These are the sim lane's, as tasks of their own.
- **Smaller:** O:225, 379 and 447 state the statistics screen pauses as fact while T:56 and OQ-670 call it
  inferred; W:220 and T:304 disagree on `City::name`; OQ-688 and half of OQ-689 repeat main's OQ-111 without
  citing it (O:274-277 is new evidence for it); O:108 the Credits text is at x 271; T:161 "16 bytes" is 32; the
  headers still say "no commits"; the evidence paths point into git-ignored caches.

## Not checked

The History Edition measurements (no video frame was opened), the pixel identity of GOG and History Edition
textures, the History Edition's text and DLL strings, the manual's page references, the statements about names in
`src/`, and about 200 further addresses.
