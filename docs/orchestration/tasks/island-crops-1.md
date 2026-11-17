# ISLAND-CROPS-1: each island's crop list (the original's island property 11)

**Draft 2026-11-17** by the UI lane, for the orchestrator's review as a sim packet (orchestrator, 2026-11-17:
"D1 goes to your lane ... Send it to me before any code; I review it as a sim packet. One concern per
packet"). Branch `task/ISLAND-CROPS-1`, base origin/main `4dfdd9b7`, worktree
`E:\anno1503-worktrees\anno1503-ISLAND-CROPS-1`. **Approved by the orchestrator 2026-11-17 as written; 1a released 2026-11-17
in the UI lane; 1b starts after the orchestrator accepts 1a.** It is split into **1a** (format) and
**1b** (World field), one concern each. 1b starts after 1a is accepted. UI-STATUS-1's D2 (the status bar's
green bars) follows 1b.

## Why

The status bar's fertility row draws a green bar per crop: **100** when the island's crop list
(original island property 11, `+0xac`) has that crop, else **50**. There is no 0 % level (AnnoFrame
`0x100098cb`-`0x100098da`).

The rebuild has no field for that list, so UI-STATUS-1 B draws the icons without bars (a named
placeholder, OQ-702). The owner asked for the full mechanic in `ui_7.mp4` (2026-11-17).

The same list drives the original's per-tile crop test: it halves the fertility when the crop is missing,
and fields die at 50 % (OQ-706). That rule is **not** in this packet; it is D6, an owner decision raised
by the orchestrator. This packet only stores the list, so D2 and later D6 can read it.

Evidence (task/UI-STATUS-1 `0ee20000`, independently verified 2026-11-17,
`E:\anno1503-worktrees\anno1503-UI-STATUS-1\cache\ui-status-1\wf\verify-summary.txt`):
- `docs/formats/insel.md` "ANBAU": a list of NUL-terminated crop names (`GETREIDE` ... `KARTOFFEL`).
  - **Loader (AnnoWelt `0x10089871`-`0x1008995f`, D):** property 11 = `0xF009`, or 0 when the island's
    climate byte (`+0xc0`, property 7) is 0. Then, for each name found among the names of wares
    `0x38..0x44`, it ORs bit `ware − 0x38`, for at most 13 names.
  - **Templates (P, byte scan):** all 465 `Scenes/Islands` templates have an ANBAU record. 29 lack
    `KARTOFFEL` and 6 lack `KRÄUTER`; the names are in insel.md.
- **New-game link placement (AnnoScene `0x100085a2`-`0x100085cf`, D):** property 11 = `0xF009` |
  (1 << k for every raw mark k), unconditionally, polar included. Only on the INSELLNK path
  (`0x10008490`, from `0x10009550`). Scenario and save islands keep their ANBAU value.
  - The marks are drawn per link from the scenario's ROHSTOFF counts (`0x10009170`, link `+0x112`).
    The rebuild already draws them: `src/sim/src/island_links.cpp` `resolve_island_links` →
    `raw_marks`.
  - Today they only produce the `LinkRawMaterials` warning ("island property 11 ... has no field",
    `src/sim/src/scenario_formats.cpp` near line 2150).
  - That the overwrite runs after the template load inside world `+0x88` is L (annoscene.md §5
    step 10).
- **Readers (D):** the status bar `0x10009770`; the crop test (AnnoWelt `0x10081986`-`0x10081997`); the
  AI, 10 places in CompLogic, purpose not read.

## Stage 1a: decode ANBAU (formats, 2-3 h)

1. **Spec:** `docs/formats/insel.md` "ANBAU" becomes a decoded record: the byte layout (names, the NUL
   separator, any trailing bytes or padding found in the corpus, the empty case). Check the whole corpus
   before writing: every `Scenes/**/*.scp`, `*.sze` and the `Scenes/Islands` templates (`*.isd` or
   whatever `insel_corpus` already walks). Move ANBAU out of the "? OQ-024" row.
2. **Parser:** in `src/formats` (`insel.hpp/.cpp`), a field-by-field decode of the payload to the name
   list, as raw byte strings: no ware lookup in `formats`. Return `Result`, no exceptions, and give
   malformed payloads (no final NUL, if the corpus never shows one) clear errors.
3. **Writer + tests:**
   - a round-trip test (parse → write → byte-identical) on synthetic fixtures (`fixtures/synthetic/`:
     our own bytes, no original data);
   - a corpus test over every ANBAU record in `ANNO1503_GAME_DIR` (count records, check that each
     round-trips, and report the 29 and 6 template counts as a check of the scan above).
4. **Oracle:** `anno-dump insel` prints the ANBAU names. `tools/insel.py` learns the same, and
   `tools/agree.py` agrees on the `insel` commands.
5. **Gate:**
   - the formats lanes and `insel_corpus`, plus `tools/agree.py` for the insel commands;
   - `tools/check_unix_warnings.sh` in WSL, after the other lanes' runs and one at a time
     (orchestrator's rule);
   - CI green.

## Stage 1b: the island's crop list in the World (sim, 3-4 h; after 1a is accepted)

1. **Field:** `Island::crops`, the crop list as an ordered set of ware ids (ascending unique; widened
   from the original's 16-bit word). CLAUDE.md "No hardcoded limits": no fixed bit width in `sim`.
   - The original's bits are `ware − 0x38`. The crop ware range `0x38..0x44` and the base set (the
     `0xF009` bits: wares `0x38`, `0x3B`, `0x44`, and bits 13-15 = wares `0x45..0x47`) come from config
     with the original values as defaults (`SimConfig`, next to `raw_climate_mask`), each with its
     address.
   - Whether bits 13-15 name real wares is not read: keep them as listed ware ids and add an OQ (from
     OQ-708) rather than dropping them.
2. **Sources:**
   - **Template, scenario or save island with ANBAU** (`scenario_formats.cpp`, next to `DISCOVER`):
     - the base set, or empty when `climate == 0`;
     - plus each listed name resolved to a ware id through the game's ware names (find the rebuild's
       existing name → ware table; AnnoGame `0x100251f8` is L);
     - an unknown name is a load warning, not a refusal; check what the original does with it.
   - **No ANBAU record:** what the original leaves (constructor value) decides. Read it, or make it an
     OQ and leave the list empty.
   - **INSELLNK link:** the base set (unconditional) plus the drawn `raw_marks`. Each mark k → ware
     `0x38 + k`, mapped through config, not hardcoded. Replace the `LinkRawMaterials` warning with the
     real assignment, and keep a warning only for what is still not modelled.
3. **Save and hash:**
   - the field is in the island's state stream (`state_fields.hpp`, after `players_present`), with a
     stream comment naming property 11;
   - bump `kStateHashVersion`;
   - update every golden hash, replay and save fixture that changes, listing each in the report with the
     reason.
4. **Validation:** `island.cpp`'s checks (ascending, unique, in the crop range).
5. **Tests:**
   - a template island with all crops;
   - one of the 6 without `KRÄUTER`;
   - climate 0 with an empty base;
   - a link island with known marks (synthetic draw source);
   - a save round trip and the hash change;
   - a corpus pass over the shipped scenarios' islands, checking every island gets a list and the
     link warning count drops.
   - Failure proof: the link test fails without the assignment.
6. **No reader yet:** no production, AI or UI code reads the field in this stage. D2 reads it through the
   play snapshot; D6 (the dry-field rule) waits for the owner.
7. **Gate:**
   - the sim lanes (determinism, save/replay, scenario corpus) plus the changed fixtures;
   - the Mac full game-file run per the landing rules;
   - `check_unix_warnings.sh`;
   - CI green.

## Acceptance

- Every rule above traces to an address, a parsed file, or an OQ.
- The format round-trips byte-identically over the whole corpus, and `agree.py` agrees.
- 1b changes the state stream with a version bump, and every changed golden is listed with its reason.
- Nothing in this packet changes gameplay: no reader of the list exists yet.
- Evidence contract (`docs/testing/evidence-contract.md`). No original bytes in git.

## The orchestrator's answers (2026-11-17, binding)

1. **Writer:** an ANBAU writer in `formats` (CLAUDE.md: parse → write → byte-identical for every parser;
   the record-level writer would round-trip the bytes without proving the name list). Test it on synthetic
   fixtures and over the whole corpus.
2. **Representation:** the ordered set of ware ids. Add one test that rebuilds the original 16-bit word
   from the set, for a template island and an INSELLNK island, and compares it with the value the original
   computes from the same input. That keeps the widening honest.
3. **Bits 13-15:** keep them as listed ware ids from `SimConfig`, with an OQ. The OQ states that D2 must
   not draw a bar for a ware it cannot name. The faithful value stays stored; readers decide.
4. **Before 1b's code:** the constructor value for an island without ANBAU is read, or made an OQ.
5. **The unknown-name warning** cites what the original does, or an OQ.
6. **1b changes the state stream:** its landing needs the Mac's full game-file run on the candidate.

## Open points for the orchestrator's review (answered above)

- Is a `formats` writer for ANBAU wanted (needed for the round-trip test), or does the round trip use the
  existing record-level writer?
- Representation: an ordered set of ware ids (proposed) against the original's word kept verbatim plus a
  view. A set fits the no-limits rule; a word is the smaller diff.
- Ware ids `0x45..0x47` in the base set (bits 13-15): keep them as listed ids, or drop them until read.
