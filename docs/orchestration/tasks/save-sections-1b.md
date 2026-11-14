# SAVE-SECTIONS-1b: the extension tag registry, the registration interface, and the proof that an extension moves no pin

**Status:** written 2026-11-14 by the Claude Code orchestrator for **the Mac executor**. It is released when the owner
pastes its prompt. It is [ADR 0006](../../adr/0006-save-sections.md) step 2, second half. SAVE-SECTIONS-1a, the first
half, gave every section a base version and an empty extension list at state 139.

**Base:** `origin/main` **after SAVE-SECTIONS-1a has landed**. Check with
`git merge-base --is-ancestor 8b732ee9 origin/main`; if that fails, stop and report. Branch `task/SAVE-SECTIONS-1b`, a
new worktree. About 3 hours. Never rebase, force-push or push `main`.

**Rules:** the owner rule of 2026-11-12 on the board ([`TASKS.md`](../TASKS.md)):
- one concern;
- no reviewer subagents (the orchestrator reviews your handover);
- one REPORTS entry of about 60 lines, READY FOR REVIEW.

**This packet takes no state number and moves no pin.** `kStateHashVersion` stays 139, and every section's base
version stays 1. A world writes exactly the bytes it writes at SAVE-SECTIONS-1a.

## Why

After this packet, a default-off feature can add state without moving any existing hash, golden or save (ADR 0006
items 1–3). The first user is FISHER-1b, the fisher behind a rules flag. Food from the fisher is the first missing
need for 131 houses in the measured towns ([`progression-gaps.md`](../../sim/progression-gaps.md)). Wildlife and
the whaler follow the same way.

## Read first

- `CLAUDE.md`;
- `docs/testing/evidence-contract.md`;
- `docs/testing/README.md`, "Pins and goldens";
- ADR 0006, items 1–3 and "Cost of the mechanism";
- `docs/sim/state.md` §9.1, §9.3–9.7 (§9.7 is 1a's framing);
- the SAVE-SECTIONS-1a review: `docs/orchestration/reviews/2026-11-14-save-sections-1a-review.md`;
- `src/sim/include/anno/sim/state_fields.hpp`:
  - `StateIO::section_version`, `StateIO::section_extensions`;
  - the ten section functions;
  - `StateIO::scopes`, `StateIO::world` (the read into temporaries, committed only after a whole, valid read);
- `src/sim/include/anno/sim/archive.hpp` (the four archives and `scope`);
- `src/sim/include/anno/sim/hash_tree.hpp`;
- `src/sim/src/save.cpp`;
- `src/sim/tests/world_state_test.cpp`: 1a's two table-driven refusal tests;
- `tools/integration_stream_ref.py`.

## Do

1. **The spec first:** a new §9.8 "Extension tags: registry and registration" in `docs/sim/state.md`. It settles the
   rules below. Names may differ; the rules may not. If a rule can't hold, stop with a QUESTION.
   - **The tag registry,** a table: tag, section, name, owner packet, status (reserved or implemented), and the payload's
     field list or a link to it.
     - Tags are reserved like OQ numbers: a packet reserves its tag on the board before it starts, and a tag is never
       reused.
     - Tag 0 is never assigned.
     - **Tags `0xF000`–`0xFFFF` are for tests only** and never appear in a production list.
     - In this packet the table has no production entry. It names the test range.
   - **The write rule.** An extension is written **only when its content differs from its default.** Entries are in
     strictly ascending tag order. The `u32` length is the exact byte length of the payload.
   - **The read rule.** Before reading, every extension this build knows for that section is reset to its default (so
     an absent tag means the default). Then each entry is checked, refusing with a message that names the section:
     - **a tag not above the one before it:** `section <name>: extension tag <n> out of order`;
     - **a tag this build doesn't know for this section:** 1a's `section <name>: unknown extension tag <n>`;
     - **a length beyond the section's remaining bytes:** an archive error;
     - **a payload not consumed exactly**, read with a reader bounded to its length: an error naming the section and
       the tag;
     - **a payload that decodes to the default:** refused, because a canonical save never writes it. This keeps
       `save(load(x)) == x` byte for byte.

     The extension's own validation follows. A failure leaves the target world unchanged, as today.
   - **The hash.** Every archive visits the same primitive sequence (§14): `u16` tag, `u32` length, then the payload
     inside a **child scope named after the extension**, so the hash tree names the feature when two peers diverge
     (ADR 0006, "Cost of the mechanism"). The length is measured by serializing the payload once before it is
     visited. A section with no extension present still contributes only its `u64` count 0.
   - **The registration interface.**
     - **Compile time only:** an ordered, compile-time list of extension types per section. There is no runtime
       registry, no static initializer and no global mutable state. A `static_assert` checks that the list is
       strictly ascending, has no duplicates and holds no test-range tag.
     - **Each extension type** names:
       - its tag and name;
       - its default;
       - one `fields(...)` function for its payload, run by all four archives (no struct dumps, no macros; "one field
         list" holds per extension);
       - its validation.
     - **Where the state lives:** the extension's state is passed to `StateIO` through an extension context. The
       production context belongs to `World`, and in this packet it holds nothing. On reading it is read into a
       temporary and committed only with the rest of the world.
     - **What a feature changes:** one type in its section's list, its state in the production context, and its row
       in the registry. FISHER-1b is the first.
   - **The schema identity for the join check.** Expose the known tags as an ordered, compile-time list of (section,
     tag, name), for ADR 0003's join check later. Do not wire it into anything.
2. **The C++.**
   - Replace 1a's always-refusing `section_extensions` with the rules above. The ten section functions keep calling
     it at the same place.
   - **Nothing production-side knows a test tag.** The test-only extension and its context are defined in
     `src/sim/tests/` only, and reach `StateIO` through the same interface a feature would use (for example, through
     `WorldTestAccess`). If that isn't possible without test code in the production library, stop with a QUESTION.
3. **A test-only synthetic extension.** One tag in the test range, on one section of your choice that has child
   scopes (for example `figures`), with a payload of at least two fields of different widths. Its default is "all
   zero".
4. **The proofs, each its own test case:**
   1. **Round trip:** a world with the synthetic extension set to a non-default value saves, loads and saves again to
      the same bytes. The loaded extension equals the one saved.
   2. **Default writes nothing:** with the extension registered and at its default, the save bytes and `state_hash()`
      equal those of the production world, where it is not registered. That is ADR 0006's "a world without it hashes
      identically before and after it is registered".
   3. **Present moves the hash:** set to a non-default value, the world hash and that section's digest change. The
      other nine section digests don't.
   4. **The hash tree names it:** the section's hash-tree node has a child named after the extension.
   5. **Refusals,** table-driven over each new message:
      - out of order;
      - a duplicate tag;
      - a payload one byte short and one byte long;
      - a present entry that decodes to the default.

      Each refused load leaves the world unchanged.
   6. **The production build still refuses the test tag:** a save written with the synthetic extension present is
      refused by the production `World::load`, with 1a's unknown-tag message naming the section and the tag.
   7. **The production lists:** sorted, no duplicates, no test-range tag (empty in this packet).
5. **The independent oracle, from the spec, not from the C++.** `tools/integration_stream_ref.py` computes the bytes
   and the hierarchical digest of the chosen section with the synthetic extension present: the section's version,
   its base bytes, count 1, then the tag, the length and the payload, with the payload as a child scope in the
   digest. The C++ test compares bytes and digest. This is a reviewed oracle edit. If it can't be written from the
   spec alone, stop with a QUESTION.
6. **Two comment fixes from the 1a review** in `world.hpp`:
   - add a history entry for 139;
   - correct "134, 135, 136 and 137 … are refused" to "134 to 138".

**Not this packet:**
- any feature, its tag reservation or its state (FISHER-1b reserves the fisher's tags);
- wiring the schema identity into a join check (NET, ADR 0003);
- FIXTURES-TRIM-1;
- any change to existing bytes, hashes or pins.

**Stop with a QUESTION if:**
- any existing byte, hash or pin would move;
- the interface needs global mutable state, a runtime registry, or test code in the production library;
- the hash-tree naming needs more than adding scopes (a change to `HashArchive`'s scope semantics).

## Proof

- **The build** with the `dev` preset, 0 warnings.
- **The tests that change.** The new and changed sim tests, `pins_contract`, `integration_stream_reference_*` and
  `population_oracles_agree`, through the canonical runner, with classified counts.
- **No pin moved.** Run `pins.py check` on the final committed head, with the game files. Then show that
  `git diff --stat <base>..HEAD -- fixtures/` is empty. Quote each `COUNTS` and `VERDICT` line as the log prints it,
  and name the log.
- **Refusals.** A test refused for memory did not run. Rerun it alone with `--jobs 1`, with its own log.
- **Hosted CI** green on the head, `gate` included.

**Not yours:** the full suite and the game-file run on the Windows PC (the orchestrator's, at the landing).

## Report

READY FOR REVIEW, about 60 lines:
- the spec section;
- the interface (the extension type's members, where its state lives, how the test reaches it);
- the synthetic extension;
- the seven proofs;
- the oracle edit;
- the `pins.py check` lines and the empty fixtures diff;
- NOT_RUN;
- Decisions and reasoning;
- QUESTIONs.
