# SAVE-SECTIONS-1a review (2026-11-14)

**Verdict: ACCEPTED for landing.** The stream now has its final shape (ADR 0006 step 2, first half): each of the
ten sections begins with a `u32` base version (1) and ends with an extension list (empty), and 139 is the last
global state number.

Landing waits on two things:
- the owner's landing OK;
- the game-file run of the landing candidate on this PC.

**Reviewer:** the Claude Code orchestrator (Opus 5.5).

**Reviewed head:** `task/SAVE-SECTIONS-1a` `8b732ee9`, one commit on `main` `d2884227` (PINS-1's landing, so
`661cd2a6` is an ancestor, as the packet requires). Hosted CI run 43581689504 is green in all 12 jobs, `gate`
included.

## What the orchestrator checked

- **Scope.** 41 files:
  - the spec (`docs/sim/state.md` §9.1, §9.3–9.5, new §9.7);
  - `world.hpp`, `state_fields.hpp`, `save.cpp`, `replay.cpp`;
  - the two oracles;
  - the sim tests;
  - the synthetic fixtures.

  No game file, no feature, no PRNG draw site, no tag registry. `kSaveVersion` 1, ECONBK1, PRNG timeline 5 and
  replay frame 4 are unchanged.
- **The spec came first, and it now matches the code.** §9.7 defines the framing: the ordered version table, `u32`
  base version, the `u64` extension count with entries of (`u16` tag, `u32` length, payload), and both refusal
  messages. While there, the §9.3 table was corrected to what the code already wrote. The orchestrator checked each
  correction against `state_fields.hpp` at `d2884227`:
  - the header carries `script_schema` and the walk-table digest;
  - `prop10` is a `u32` (line 386);
  - `ids` holds `next_order_id` and `next_ticket` (lines 1120–1121);
  - `script` is row 10.
- **The code.**
  - **One pair of helpers** in `StateIO` (`section_version`, `section_extensions`) is called by all ten section
    functions. So the hash, the save and the load all run the same framing, and the version and the count are part
    of each section's digest.
  - **`kSectionVersions`** is an ordered `constexpr` table in `world.hpp`, with no mutable state.
  - **The extension reader** bounds the count against the remaining bytes (6 bytes per entry) before it reads. It
    refuses the first entry without allocating its payload.
- **The replay identity probe** (`replay.cpp`). It checks the frame's global identity first, so a 138 log is still
  refused there, with "state stream version differs". Then it reads the header section's base version, and then
  the `StateHeader` reader, which checks all three identities (state 139, ECONBK1, timeline 5). The new minimum
  header length is 28 bytes: a 4-byte version, 16 identity bytes and an 8-byte extension count.
- **The oracles were written from the spec.** Both use `section_bytes(base) = u32(1) + base + u64(0)` around the
  top-level sections only; object and registry substreams stay unframed. `integration_stream_ref.py` adds framed
  `figures` and `script` records, 12 scopes in all, and the C++ test compares bytes and hash for both.
- **The pins, recounted by the orchestrator** from `goldens.txt` at `d2884227` and `8b732ee9`.
  - 63 entries on both sides, all now at 139.
  - **50 changed value.**
  - **13 changed only their version:** the registry digests, the `oracle_core_*` values, the PRNG table and the three
    event logs. `core_hash()` keeps its five-field value by design (§9.1). The `clock_rng` section's digest now wraps
    those five fields in the version and count, and no production code equates the two.
  - This matches the report.
- **The 138 boundary.** `legacy138/g2.log` is byte-identical to the outgoing `current/g2.log` (same blob). The
  boundary test now refuses 134 to 138, native and replay, beside a valid 139 control. A replay whose header
  section claims base version 2 is refused, naming the section, and the world is left unchanged.
- **The refusal tests.** There are two table-driven cases over all ten sections, each with its own byte cursor over
  the save and a resealed checksum. Each checks the message, and that the refusal isn't the checksum:
  - base versions 0 and 2;
  - one extension entry with tag 0, 1 or 65535.

## Not verified by the orchestrator

- The Mac's local logs (`cache/save-sections-1a/`, its evidence manifest). The report quotes the final committed-head
  check: `pins_contract` 1/1 PASS, and 1664 selected, 1646 executed, 0 failed, 18 opt-in skips, PASS. The landing's
  game-file run reruns every test on the merged commit.
- Linux and macOS game-file runs.

## Notes (none blocks)

- **`world.hpp`'s history comment** has no entry for 139, and it still says that 134 to 137 "remain historical and
  are refused". 138 is now refused too. SAVE-SECTIONS-1b fixes both lines.
- **"Tags strictly ascending"** can only be tested once a tag is known. In 1a every entry is refused. SAVE-SECTIONS-1b
  tests the order with its synthetic tags.
- **The regen workflow.** The first full regen failed 65 cases. The executor fixed the replay prefix reader and the
  hand-derived offsets, then reran only those 65 cases, as the test rule says. The final committed-head check passed
  in full.

## For the landing

**The candidate:** `main` `d89d54a6`, then `task/SAVE-SECTIONS-1a` `8b732ee9`, then the board. Its game-file run is
the canonical call on this PC.
