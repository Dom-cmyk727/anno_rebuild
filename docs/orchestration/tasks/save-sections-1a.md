# SAVE-SECTIONS-1a: a base version per section and empty extension lists (state 139, the last global number)

**Status:** written 2026-11-14 by the Claude Code orchestrator for **the Mac executor**. It is released when the owner
pastes its prompt. It is [ADR 0006](../../adr/0006-save-sections.md) step 2, first half; SAVE-SECTIONS-1b is the
second half.

**Base:** `origin/main` **after PINS-1 has landed**. Check with `git merge-base --is-ancestor 661cd2a6 origin/main`;
if that fails, stop and report. Branch `task/SAVE-SECTIONS-1a`, a new worktree. About 3–4 hours. Never rebase,
force-push or push `main`.

**Rules:** the owner rule of 2026-11-12 on the board ([`TASKS.md`](../TASKS.md)):
- one concern;
- no reviewer subagents (the orchestrator reviews your handover);
- one REPORTS entry of about 60 lines, READY FOR REVIEW.

**This packet takes state number 139, the last global number** (ADR 0006). No other packet takes one.

## Why

Every gameplay port that adds state is held until a default-off feature can add state without a new global
number (ADR 0006):
- the fisher (FISHER-1b), whose food is the first missing need for 131 houses in the measured towns
  ([`progression-gaps.md`](../../sim/progression-gaps.md));
- wildlife;
- the whaler.

This packet gives the stream its final shape: a base version per section, and an extension list at the end of each
section, empty for now. After it, `kStateHashVersion` stays 139. SAVE-SECTIONS-1b then adds the tag registry and
proves that an extension moves no pin.

## Read first

- `CLAUDE.md`;
- `docs/testing/evidence-contract.md`;
- `docs/testing/README.md`, "Pins and goldens" and "Deliberate state renumbering" (PINS-1's seven steps);
- ADR 0006;
- `docs/sim/state.md` §9.1–9.5;
- `src/sim/include/anno/sim/state_fields.hpp`: the header comment, `StateHeader`, and the section functions
  `header_section` … `script_section`;
- `src/sim/include/anno/sim/archive.hpp`;
- `src/sim/src/save.cpp` (the length-prefixed sections);
- `src/sim/include/anno/sim/world.hpp` (`kStateHashVersion` 138, `kSaveVersion`, `kPrngTimelineVersion`);
- `tools/sim_stream.py`;
- `tools/integration_stream_ref.py`.

## Do

1. **The spec first.** In `docs/sim/state.md` §9.3 and §9.5, and in a new §9.7 "Section versions and extensions":
   - **The ten sections:** `header`, `clock_rng`, `settings`, `ids`, `players`, `islands`, `figures`, `events`,
     `timers_monopoly` and `script`. The ADR lists nine; `script` is the tenth.
   - **Each section's scope begins with `u32` base version.** At 139 every section is version 1.
   - **Each section's scope ends with its extension list:** a count, in the form the archive already uses for
     sequence counts, then entries `(u16 tag, u32 length, payload)` with tags strictly ascending.
     - In this packet the count is always 0.
     - Reading refuses any entry, with a message that names the section and the tag:
       `section <name>: unknown extension tag <n>`.
   - **What 139 means from here:** it is the stream's format (the sections, their versions, the extension framing).
     It moves only if that framing itself changes.
   - **When a section's base version moves:** for a change to that section's base layout or default behaviour
     (ADR 0006 item 4). A load of another version is refused, with the section and both versions named.
2. **The C++.**
   - **`kSectionVersions`:** an ordered `constexpr` table of (section name, version) in `world.hpp`, read by the
     section functions. No global mutable state, and no unordered containers.
   - **Unchanged:** the header keeps `kStateHashVersion` (now 139), `script_schema` and `kPrngTimelineVersion`.
   - **Hash, save and load run the same section functions,** so each change applies to all three. The version and the
     count are part of the hash.
3. **The independent oracles, from the spec, not from the C++.** `tools/sim_stream.py` and
   `tools/integration_stream_ref.py` write the new layout: each section's version, and the empty extension list.
   - These are reviewed oracle edits, as step 3 of the README procedure says.
   - If an oracle can't be written from the spec alone, stop with a QUESTION.
4. **The renumbering 138 → 139, committed this time, by the README's seven steps:**
   1. keep `legacy138/` (`g2.log`, `g2.save`) first;
   2. bump the constant and the five tripwires;
   3. update both oracles;
   4. hand-copy the oracle-held pins;
   5. run `pins.py regen --area sim`;
   6. add 138 to the boundary tests as the newest refused predecessor;
   7. run `pins.py check`.
5. **The tests.**
   - **Section versions:** for every one of the ten sections, a save whose base version is changed is refused, and the
     message names that section and both versions. One table-driven case.
   - **Extensions:** for every section, a save with one extension entry is refused, and the message names the section
     and the tag. One table-driven case.
   - **Everything else:** the existing round-trip, replay and boundary tests stay, re-pinned by the command.

**Not this packet:**
- the tag registry, the registration interface and the synthetic-extension proofs (SAVE-SECTIONS-1b, which moves
  no pin);
- the join check's schema identity (NET, ADR 0003);
- FIXTURES-TRIM-1;
- any feature.

**Stop with a QUESTION if:**
- a section can't take its version and extension list without changing another section's bytes;
- anything would need a second number.

## Proof

- **The build** with the `dev` preset, 0 warnings.
- **The tests that change.** The sim tests that change, `pins_contract`, `population_oracles_agree`,
  `integration_stream_reference_*` and `highmap_digest_reference_agrees`, through the canonical runner, with
  classified counts.
- **The pins.** `pins.py regen`, then `pins.py check` on the final committed head, with the game files. Quote each
  `COUNTS` and `VERDICT` line as the log prints it, and name the log.
- **Refusals.** A test refused for memory did not run. Rerun it alone with `--jobs 1`, with its own log.
- **Hosted CI** green on the head, `gate` included.

**Not yours:** the full suite and the game-file run on the Windows PC (the orchestrator's, at the landing).

## Report

READY FOR REVIEW, about 60 lines:
- the spec change;
- the version table;
- the oracle edits;
- the pins moved (how many changed value, how many changed only their version) and the recordings rewritten;
- the refusal tests;
- the gate lines as printed;
- NOT_RUN;
- Decisions and reasoning;
- QUESTIONs.
