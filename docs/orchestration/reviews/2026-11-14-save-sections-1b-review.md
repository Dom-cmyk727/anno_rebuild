# SAVE-SECTIONS-1b, the orchestrator's review (2026-11-14)

**Verdict: ACCEPTED for landing.** Landing waits on two things:
- the game-file run of the landing candidate on the Windows PC;
- the owner's landing OK.

**Reviewer:** the Claude Code orchestrator (Opus 5.5). The packet ruled out executor-side reviewer subagents, so this is
the package's independent review.

**Reviewed head:** `task/SAVE-SECTIONS-1b` `4c1685fa`.
- It contains `main` `f54e36f0` and SAVE-SECTIONS-1a's `8b732ee9`.
- Hosted CI run 43605110976 on `4c1685fa` is green in all 12 jobs, `gate` included.

## What the orchestrator checked

- **Scope.** The diff touches 15 files: `src/sim` (two new headers, `extension_context.hpp` and `extensions.hpp`;
  `state_save.hpp`; `state_fields.hpp`, `archive.hpp`, `world.hpp`, `save.cpp`, `invariants.cpp`), `src/sim/tests`,
  `tools/integration_stream_ref.py`, `tools/lint_sim_allow.txt`, `docs/sim/state.md` and REPORTS.
  - No binary file.
  - No change under `fixtures/` (`git diff --stat` is empty).
  - `kStateHashVersion` stays 139, and every section version stays 1.
- **The spec, §9.8.** It holds every rule the packet asked for:
  - the registry table, with only the test range `0xF000`–`0xFFFF`;
  - tag 0 never assigned, and tags never reused;
  - the write rule (only non-default payloads, ascending, raw length measured once);
  - the read rule (reset to defaults; order and duplicates; unknown tags; a bounded payload read; exact consumption;
    a present default refused; then validation);
  - the payload in a child scope named after the extension, with `HashArchive` unchanged.
- **The code** (`extensions.hpp`, `extension_context.hpp`, `state_save.hpp`, the `state_fields.hpp` diff).
  - **Read.** It follows the spec in that order. `ReadArchive::subarchive` bounds the payload, so a field reader can't
    run into the next entry.
  - **Write.** It writes the count of non-default extensions, then the tag, then a length measured with a temporary
    `WriteArchive`, then the payload in its named scope. With no extension present, it writes the old `u64` count 0
    and nothing else.
  - **Registration is compile time only.**
    - `BasicExtensionList` checks with a `static_assert` that tags are nonzero, strictly ascending and in their range.
    - The test-list policy accepts only test-range tags.
    - `kKnownExtensions` exposes (section, tag, name), with a `static_assert` against test tags.
    - There is no runtime registry, static initializer or global mutable state.
  - **Staging.** Both readers stage the context and commit it only after a whole, valid read. In the native path, the
    blobs and `check_invariants` come first. `is_nothrow_move_assignable` is asserted.
  - **No test code in `anno_sim`.** The native save and load moved into templates over the context
    (`state_save.hpp`). The section functions now take the context. `SyntheticExtension` and `TestContext` exist only
    in `src/sim/tests/extensions_test.cpp`.
    - The moved save and load code is the old `save.cpp` code with the context threaded through. Read side by side,
      nothing else changed.
- **The lint allowance** (`extension_context.hpp`, `size = sizeof...(E)`). It is a derived type-pack length, not a
  state field or a content limit. Accepted.
- **The Clang 18 fix** (`4c1685fa`). It replaces nested pack-expanding lambdas with named section templates, and
  suppresses no diagnostics.
- **The proofs.** Seven test cases in `extensions_test.cpp`, one per proof.
  - The refusal table covers out of order, a duplicate, one byte short, one byte long, a present default, a
    validation failure, a length beyond the input and an unknown tag.
  - It covers both the native and the flat reader, and checks that each refusal leaves the target unchanged.
- **The oracle** (`integration_stream_ref.py`). `synthetic_extension_record()` builds the figures section from §9.8:
  - the version;
  - the empty figure base;
  - count 1, tag `0xF001`, length 6 and the payload;
  - in the digest, the payload as a child digest.

  It reads no C++ output. CMake writes its output into `build/`, and the test compares both the bytes and the digest.
  Its self-test includes a control: a digest that leaves out the child scope is different.
- **The comment fixes in `world.hpp`.** The history entry for 139 is added, and "134 to 138" is corrected.

## Notes (not blocking)

1. **Missing spaces in the text.** `state.md` §9.8, the `world.hpp` 139 history line and the REPORTS entry often drop
   the space before a number ("framing139", "versions1", "tag0xF001", "length6"). The orchestrator fixes the spec and
   the comment in a docs commit after the landing; the REPORTS entry stays as written.
2. **The canary comment** in `World::field_list_canary` no longer says to bump a version. Under ADR 0006 a new member
   goes into a section's field list, which bumps that section's version, or into the extension context. The same docs
   commit says so.
3. **The hash serializes a present payload twice:** once to measure its length, once to visit it. That is fine for
   small payloads. A feature with a large payload (wildlife herds, say) should measure the cost in its packet.
4. **`anno_sim_tests` now runs Python at build time** to write the oracle into `build/`. Every CI lane and the MSVC
   preset already find Python 3, as the green run shows.

## Game-file run of the landing candidate (2026-11-14)

**The candidate:** `orch/land-save-sections-1b` at `71b58086`. It is UI-WINDOW-1's final candidate `4317ac9b`
(`main` `f54e36f0`, then UI-WINDOW-1 `0143f045`, then the board up to `d61519db`), then `task/SAVE-SECTIONS-1b`
`4c1685fa`. It was built locally and not pushed. One run covers both packages together, as they would sit on `main`.

**The run:** the canonical call, with the GOG oracle shots and the HE captures set, 16:16–17:03.
- `COUNTS: selected=2616 executed=2587 failed=2 empty=0 skipped=27 (required-asset=0 platform=3 opt-in=21 gpu=0
  other=3) incomplete=0 sanitizer_lines=0`, `VERDICT: FAIL failed tests`
- Log sha256 `59640c093d414a4a…`.

**The two failures are 1,800 s timeouts**, spent mostly waiting for memory admission. Both are render corpus tests
this package doesn't touch:
- `chunk draw lists replay to the objects and base planes of Scene::render, every chunk`;
- `legacy river shading reproduces the previous enhanced digests` (heavy).

**Each rerun alone** with `--jobs 1`, same build, 17:04–17:13: `COUNTS: selected=1 executed=1 failed=0`,
`VERDICT: PASS` for both. Log sha256s: `c844d8d5f4a28e13…`, `f48b0d387c4f8022…`.

**Result:** every selected test that is not skipped executed and passed on the candidate.

## The landing alone (2026-11-14)

The owner said "Land both". UI-WINDOW-1 was then held again, after the owner's second retest relayed by the UI lane.
SAVE-SECTIONS-1b, covered by the same OK, lands alone. Its candidate `orch/land-save-sections-1b-only` at `f04c600f`
is `main` `f54e36f0`, then `4c1685fa`, then the board up to `ae4d8298`. Against the run above it lacks only
UI-WINDOW-1's files: no `src/sim` difference.

**Its own game-file run:** the canonical call, 17:36–18:04.
- `COUNTS: selected=2600 executed=2568 failed=5 empty=0 skipped=27 (required-asset=0 platform=3 opt-in=21 gpu=0
  other=3) incomplete=0 sanitizer_lines=0`, `VERDICT: FAIL failed tests`
- Log sha256 `b7333d0b6b59b731…`.

**The five failures are memory refusals** (`reason=low-memory stop`; another session's render tests held about
6 GB):
- two scenario-corpus tests, `every .sze loads, creates and runs five minutes` and `the ten scenarios with the most
  buildings run 20000 ticks more`;
- three render corpus tests, `render_chunk: the Cpu builder keeps the planes, GpuList keeps the list`,
  `river flow corpus: every island with rivers` and `water field: empty cells hold on every island`.

**Each rerun alone** with `--jobs 1`, 18:05–18:08: `COUNTS: selected=1 executed=1 failed=0`, `VERDICT: PASS` for all
five. Log sha256s: `f18fa7ea57804a6c…`, `bb8bee8572c3837c…`, `4dee0d6093041579…`, `874ae18ec9e7faa5…`,
`4028ac760a8f3416…`.

## Not verified by the orchestrator

- The executor's own runs: the pins check (1,653 executed, 0 failed, 18 opt-in skips), the focused runs and the
  0-warning dev build on the Mac. They were read from the report; its handoff file is on the Mac.
- MSVC and WSL locally. The landing's game-file run covers MSVC with the game files.
