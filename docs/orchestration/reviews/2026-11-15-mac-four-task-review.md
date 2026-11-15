# The Mac's four tasks: review (2026-11-15)

**Reviewer:** the Claude Code orchestrator. Two read-only review subagents read the code: one for FISHER-1b and one
for FIXTURES-TRIM-1. The orchestrator checked their key findings in the code, and read FISHER-RE-2 and FISHER-1c
itself.

| Task | Head | CI | Verdict |
|---|---|---|---|
| FISHER-1b | `7dd95644` | 43654470180 green | ACCEPTED with findings |
| FISHER-RE-2 | `568c8788` | 43660317967 green | ACCEPTED |
| FISHER-1c | `eb40d75c` | 43660612256 green | QUESTION checkpoint accepted; the canary is NOT_RUN |
| FIXTURES-TRIM-1 | `7aaa3c85` | 43662104860 green | ACCEPTED with findings |

## FISHER-1b

**What is met:**
- Tag 1 in the header is one byte. An explicit default entry is refused, and so is an unknown tag 2 in every section.
- Flag off writes nothing, and the pins show 0 moved.
- Mode-3 is read as blocked, so usable = 0x1ff − blocked, as the QUESTION answer requires.
- The draws are unchanged, and the receipt is an unclipped u16 add.
- `7dd95644` differs from the tested `b68a7031` only in REPORTS.
- Gates: affected tests 90/90; full suite 1666 executed, 18 opt-in skips, PASS.

**Findings, for the next fisher code packet:**
1. **B1, blocked fish are dropped before the zero-candidate test** (`harvest.cpp:239`, `usable_mask == 0` before
   `candidates.insert`). The review subagent reads the original's zero-candidate check (`0x100592fc`) as counting
   every unclaimed tile-11, with the usable bits applied only later, in goal-pop (`0x100033c3`).
   - The outcome is the same (no goal, home fallback, no draw), but `search_candidates` / `search_popped` differ.
   - The "no fish" test asserts the rebuild's order, not the original's.
   - Check `0x100592fc` and match it, or cite why not. Add a test with truly no fish.
2. **B2, the rig's bay is open water everywhere.** The fishery and its entrance stand on open mode-3 cells, so a
   coastal hut whose entrance is on land is never exercised. If the search cannot start from a closed cell, the
   fishers do nothing on a real map. Add a rig with a land entrance before the canary.
3. **B3, some tests prove less than they claim.**
   - The mode-0 test compares `WalkCells` with itself; only the pins prove mode 0 unchanged.
   - The launch test uses the function under test as its oracle.
   Name these as such, or give each an independent expectation.
4. **B4, the mid-trip save test never asserts that the trip finishes,** and there is no two-runs-from-create hash
   comparison. The packet's Proof 3.7 is only partly met.
5. **B5, cosmetic:** `validate_figures`' refusal message (`figure.cpp`) leaves out job 0x2d.

## FISHER-RE-2

**Accepted.** It reads AnnoWelt `0x100928a0`–`0x10092b32`:
- an empty whole object word with a four-corner mean strictly below 16 gets resource 11;
- byte 3 bit 7 is kept, and the corners are untouched;
- the load callers and the area clear also run the fill.

`fisher_ref.py`'s `runtime_fish` and its four vectors match that rule. Two mutants are applied and refused. The C++
vector reader skips the new kind by name and still runs its 16 geometry vectors.

**The census:**
- S01_02_01 has 179,016 usable generated cells, including 28 and 64 candidates in its two farms' rectangles;
- all 50 maps and both `lan5` layouts are counted.

OQ-610 is narrowed, not closed.

**Consequence:** on real maps the fishers' goals come from this load-time fill, not from the 2,473 embedded fish, which
are all blocked. The sim does not do the fill yet. That is the next fisher code step.

## FISHER-1c (QUESTION checkpoint)

**Accepted as a checkpoint.**
- `load_scenario` imports S01_02_01's 27 islands.
- `World::create` then refuses it, with the flag off and on alike: `script offset 0: unsupported object definitions`.
  The map defines two `OBJECT_STADT` objects, and `script.cpp` rejects any nonempty object definitions.
- Nothing was bypassed.

The choice of how to unblock the canary goes to the owner (below).

**Numbering:** the branch's open question also took OQ-623, which UI-INFO-2 uses (the `.`/`,` turn direction).
UI-INFO-2 lands first, so the fisher question is renumbered **OQ-625** in the next fisher packet.

## FIXTURES-TRIM-1

**Accepted:**
- 276 files deleted, all under `fixtures/synthetic/sim` (31.93 to 7.07 MiB for that subtree; all of `fixtures/` went
  from 32.26 to 7.40 MiB).
- The only other fixture change is an exact rename, `goldens.txt` to `pins.txt`. No retained byte changed.
- No glob over `fixtures/synthetic/sim` silently matches nothing.
- The two repointed tests read identical blobs (`03600b8a`, `b1ef867f`).
- No original file entered git.

**Findings:**
1. **T1:** it keeps more per-number copies than ADR 0006 step 3 names: `econbook1`, `expand1`, `salt1`, `freight` and
   `iron2`, and parts of `integrate1/legacy`. `docs/testing/fixtures-trim.md` justifies them as refusal and oracle
   inputs. **The orchestrator accepts that reading.**
2. **T2:** stale `goldens.txt` mentions remain in comments and docs: `src/sim/CMakeLists.txt:58`, seven test files,
   `tools/sim_stream.py`, `docs/testing/pins-literals.md`, `current/README.md` and `provenance.json`. A small docs fix
   goes into the next Mac packet.

## Landing

`23885eb3` is the tested `7c7c6942` (UI-INFO-2, the ZOOM-SMOOTH-1 partial, 1b and 1c), then FISHER-1b, then
FIXTURES-TRIM-1. Both REPORTS conflicts were resolved by keeping both sides. Its game-file run is in progress. Landing
waits for the owner's OK. FISHER-RE-2 and FISHER-1c wait for the OQ renumbering.
