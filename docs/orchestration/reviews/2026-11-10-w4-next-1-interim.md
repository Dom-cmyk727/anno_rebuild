# W4-NEXT-1, interim review (parts P, C, S, U so far)

**Reviewer:** the Claude Code orchestrator, 2026-11-10 evening, read-only. **Reviewed heads:** `task/W4-coord-1`
`60955b0c`, `task/W4-ui-3` `8b172760`, both from `bd760031`. The package is still running (part C's CI count, the
independent reviews and part E are open), so this is not an acceptance. It lists what must change before part E.

**Verdict: CHANGES REQUESTED** for parts C and U; part S is accepted as reported, subject to its independent review.

## What holds

- **The game-file run on `task/W4-coord-1` passed, re-classified from its own log.** The orchestrator copied
  `build/msvc/Testing/Temporary/LastTest.log` and the console log (run 16:27, 679 s, `-j 16`) to
  `E:\anno1503\cache\w4-next-1-review\coord-gamefile\` and classified them with `tools/test/ctest_verdict.py`:
  **2,359 selected; 2,347 executed and passed; 0 failed; 12 skipped** (6 opt-in; 3 platform: `F4 (N4b)`, `S3`, the
  new `budget ports` host check; 3 symlink rows without the Windows privilege); **0 required-asset**, so the game
  files were present. No stall, no memory refusal.
- **The same run also passes the 18 real-file tests** that fail on `fix/MAP-EXTRACT-2-ci`. That supports the
  attribution of those 18 to `main`'s part A (`5e7b13c6`), which `main` has and that branch lacks.
- **C-1, the port fix.** The four cherry-picked commits keep their evidence package
  (`docs/orchestration/evidence/coord-ports-1/`, 18 of 89 failed on the old ports under a narrowed range, 0 of 92 on
  the new ones). Fresh evidence agrees: `task/W4-ui-3`, which lacks the fix, failed coordinator cases twice in eight
  CI runs (42823056511 `linux-gcc-release`: `O1`, `S1`, `T2`; 42823677953 `linux-gcc`: `F3 (D1, silent)`, `O1`,
  `T2`), while every Linux job on `task/W4-coord-1` since the fix passed.
- **CI on `task/W4-coord-1`:** qualifying first attempts with all three Linux jobs green: 42816371389 (`511ca98e`),
  42816475646 (`810b7f38`), 42820583088 (`3be3aaae`); 42848576936 (`60955b0c`) was running at review time. The first
  two had a red `cpp-format` job, fixed in `3be3aaae`; say so next to the count.
- **Part S:** the hover verb, the `inspect_building` formatter tests (6 cases, hand-derived lines, a no-building
  control) and the append-only correction about `--exclude-label heavy` are what the part asked for. The new card
  `docs/testing/settlement-inspection-playtest.md` is ready for the owner once the review below is done.

## Findings to fix before part E

1. **Blocker, bug: `--view` lost its S, D and Q keys.** `viewer.cpp`: `const bool classic = hooks_ == nullptr ||
   hooks_->ui == UiSurface::Classic;`. `main.cpp` passes hooks only in `--play`, so every `--view` window counts as
   Classic: S and D no longer pan and Q no longer quits, and `--ui dev` has no effect there, although the new
   `command line` test asserts that `--view ... --ui dev` is accepted "not --play-only". ADR 0004 R2 is about the
   default in `--play` only. Fix: a window without play hooks keeps the developer keys; make the key decision a pure
   function and test it, with a case that fails on today's code.
2. **Blocker, test bug: the new `F3 (D1, deaf)` case dies with SIGPIPE on macOS.** Run 42820583088 attempt 2,
   `cpp (macos-clang)`, test #76 after 2.54 s: `SIGPIPE***Exception`, classified INCOMPLETE (evidence
   `test-evidence-macos-clang`, `test.log` line 1077). The timing fits a write after the client's admission bound
   (2 s plus one 0.3 s liveness period): most likely `client.send("q\n")` or `quit_and_wait(deaf)` writes to a child
   that has already exited. Find it, fix it the way the other F3 cases avoid it, and show it holds under repetition
   on macOS or Linux (for example `ctest -R deaf --repeat until-fail:50`). The rerun does not count toward the five,
   but its failure is evidence and stays in the report.
3. **C-2 is a client-side bound; the stall's cause is not shown.** The fix is sound as a bound: once the cancel is
   unanswered for one more liveness period, `transact()` gives up with `wait deadline, no server reply`. But the
   recorded stall does not obviously match the mechanism. `cache/main-gamefile-96de6bca/stall2-snapshot.txt`: the
   stalled test (pid 32352) had released everything in the journal (A 4195 / R 4195, C 4194 / D 4194), the coordinator's
   meter process (PowerShell pid 65816) used no CPU for 30 s, and the snapshot was 22 minutes in, inside the 3,600 s
   admission bound of that run. Either show from those records (and `coord-47631.journal.at-stall2`) that the client
   was inside `transact()` with an unjournaled pending request kept alive by unrelated frames, or report the stall's
   cause as UNPROVEN, with the bound as a mitigation. Also say why a frozen meter process would still produce frames.
4. **C-1, `F4 (N3)` and `H1`:** correctly left unexplained. The packet's step 1 asked for the failing cases to log
   the message they received; confirm that this logging exists for them now (name the lines), so the next failure
   names its cause.
5. **C-3:** not measured. That is acceptable for this package only as a NOT_RUN with its reason; the measurement is
   assigned in W4-NEXT-2 part M.
6. **Part U, rows 5 and 10 have no tests.** The packet requires for every screen: CPU then GPU drawing compared, the
   UI.1 oracle comparison with a deliberately wrong input that it rejects, and `bench_frame` before and after. None
   of these exist for the pickaxe frame swap or the pause parchment. Add them, or report the rows as not done.
7. **Part U, row 19 is not done.** Only a `data/ui/hud.txt` entry was added; nothing is drawn (OQ-565). Report it as
   NOT DONE with its OQ, not as progress.
8. **The row 18 QUESTION rests on a wrong premise.** The report says no mouse-button handling or screen-to-tile
   mapping exists in `src/app`. It does: `viewer.cpp` handles `SDL_EVENT_MOUSE_BUTTON_DOWN/UP` for the construction
   menu (around line 1416), the developer tool's clicks go to the runner (around 1091-1100), the opening selects
   ships by click (around 1508), `render::pick_geometry_of` and `PointerFrame` map the pointer, and the W4-SETTLEMENT-1
   inspection line already names the building under the pointer. Correct the entry. Option (b), a separate task, is
   still right, because selection state and the meaning of Escape are owner-facing; it is W4-NEXT-2 part K.
9. **Small:** `test_ports.hpp` documents the F3 band as 27810-27817, but `kPortDeaf` is 27818; fix the comment. The
   packet asked for evidence under the worktree's `cache\w4-next-1\`; the game-file run's logs were left only in the
   build tree, where the next run overwrites them. Keep every later gate's logs there.

## For the merge (orchestrator)

- ADR 0004 row 10 says P is free; `sdl_input.cpp` binds P to `PrevType` while the developer tool is on. Fold the
  executor's correction into the ADR at the merge.
- `--ui classic` as the `--play` default matches ADR 0004 R2's interim ("until answered: (a), default classic"); R2
  stays the owner's question. In Classic, S and D do nothing while A and W still pan, so WASD is half there: note it on
  the card for the owner.
