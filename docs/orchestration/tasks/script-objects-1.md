# SCRIPT-OBJECTS-1 and the road to the S01_02_01 fisher canary, in small reviewed steps

**Status:**
- **Written** 2026-11-15 by the Claude Code orchestrator for **the Mac executor**.
- **The owner's decision** (2026-11-15, OQ-625 option 1a): "Option 1a, divide this into smaller substeps, that he
  reports back to you. You review them for correctness and whether they fulfill the projects guidelines."
- **Each step is released on its own** when the owner pastes its prompt. The orchestrator writes each prompt after
  reviewing the step before.

## Why

FISHER-1d ported the runtime fish fill, but the real-map canary on `S01_02_01` still cannot run. `World::create`
refuses the map's script, which needs three things the compiler lacks ([`fisher.md` §8](../../re/fisher.md)):
- two `OBJECT_STADT` definitions and their `KILLED` queries;
- five PLAYER queries;
- eleven action names.

The owner chose to do this properly, so the real-map proof is honest, in small steps that each come back for review.

## The steps

Each step is about 1–2 hours, one concern, one branch, one REPORTS entry ending in READY FOR REVIEW. **After each step,
stop.** The next step starts only when the orchestrator has accepted it and the owner pastes the next prompt.

| Step | Kind | What | Done when |
|---|---|---|---|
| **SO-1a** | read | How the original binds `OBJECT_STADT "NAME" n` and evaluates `OBJECT "NAME" KILLED`: what `n` names, when the binding happens, an absent or destroyed city, any mutable binding state | `docs/re/script-objects.md` with addresses; reference vectors in a Python oracle; or a QUESTION naming what cannot be proved |
| **SO-1b** | code | The compiler and runtime accept `OBJECT_STADT` and the `KILLED` query, exactly as SO-1a proved them; every other unsupported query or action still refuses | Synthetic bind / kill / absent / bad-id / save-reload tests that fail on `main`; `S01_02_01` now refuses at its *next* unsupported name (`QUESTLOG_SHOW`, payload offset 1339), not at the objects |
| **SL-0** | read + plan | The other 16 names `S01_02_01` uses (5 PLAYER queries, 11 actions): for each, the original handler's address, what state it reads and writes, and its class (see below) | `docs/re/script-language-plan.md`: one row per name, and an ordered list of follow-up steps of about 2 h each. **The owner reviews this plan** before any of it is built |
| **FISHER-RE-3** | read | 1d's F1: how the original fisher's walk home ends when every entrance of its hut is on land (mean ≥ 32) | An addressed note in `fisher.md` and a reference vector; or a QUESTION |
| **SL-1 … SL-n** | code | The steps SL-0 proposes, one at a time, as the owner approves them | Each its own reviewed step |
| **FISHER-1e** | canary | `S01_02_01`, loaded normally with its script, with the fisher flag on: the fishers launch, catch, return, and the farms make food; flag off, today's pins | The canary passes with nothing suppressed |

**SL-0's classes:**
- **sim:** it changes or reads game state, so it must be faithful in `src/sim`.
- **presentation:** it only shows something (quest log, text, video, screen change). The sim records it as an event for
  the UI and changes no state; the UI shows it later.
- **needs a subsystem:** it depends on something the rebuild does not have yet (for example military for
  `ATTACK_AREA`). SL-0 names the subsystem and asks a QUESTION; it does not stub it.

## Rules for every step

- **Base:** `origin/main` at the time of release (`b9b8df23` or later). New branch per step, named
  `task/<STEP>`. Never rebase, force-push or push `main`.
- **Owner rules** ([`TASKS.md`](../TASKS.md), 2026-11-12): one concern; no reviewer subagents; one REPORTS entry of
  about 60 lines; if a step passes its time, stop at the end of the current item, commit what passes, report what is
  left.
- **`CLAUDE.md` and [`evidence-contract.md`](../../testing/evidence-contract.md)**, which the orchestrator's review
  checks one by one:
  - every rule traces to an address (module + address), a parsed file or a cited source; guesses go to
    `docs/open-questions.md`;
  - the spec or oracle comes first (`docs/`, a Python reference); the code is never the only place a rule is written;
  - determinism: integer state only, ordered containers, no wall clock, no global mutable state;
  - no hardcoded limits: counts and ids are data, never fixed arrays or narrow ids in `sim`;
  - faithful core: original rules in `sim`; anything else behind a `rules` flag;
  - a new test must fail on the base and pass on the branch, and say so in the report;
  - the pins gate shows 0 pins moved unless the step says why one moves;
  - no original script text, bytes or listings in git; names, offsets and counts only.
- **The report says:** what was proved, at which addresses; what was not; NOT_RUN items; the exact gate lines;
  "Decisions and reasoning"; QUESTIONs.

## SL-0 in detail (released 2026-11-15)

**SO-1b was ACCEPTED** ([review](../reviews/2026-11-15-so-1b-review.md)). **Branch:** `task/SL-0` from the newest
`origin/main`. About 2 hours. Read and plan only: no engine code.

1. **The inventory:** every query and action name `S01_02_01`'s script still needs, from its compile refusals in
   order (the first is `QUESTLOG_SHOW` at payload offset 1339). SO-1a counted 3 distinct PLAYER query names in 5
   uses and 11 action names; confirm or correct both counts. Names, offsets and counts only, no script text.
2. **Per name:** the `AnnoScene.dll` string address, the handler's address, what game state it reads and writes,
   any RNG draw, anything it saves, and its class:
   - **sim:** it reads or changes game state, so it must be faithful in `src/sim`;
   - **presentation:** it only shows something (quest log, text, video, screen change); the sim records an event
     for the UI and changes no state;
   - **needs a subsystem:** it depends on something the rebuild lacks (for example military); name the subsystem and
     ask a QUESTION, never a stub.
3. **The plan:** `docs/re/script-language-plan.md`, one row per name with its addresses and class, then an ordered
   list of follow-up steps of about 2 h each (SL-1, SL-2, …). Group names that share a handler or state. For each
   step, say what fails before and passes after, and where `S01_02_01` refuses next.
4. **SO-1b review F1:** one addressed line in `script-objects.md` showing where the island's `+0x98` number is set
   from the scenario's island record, so the engine's `IslandSpec::slot` is proved to be the same number. If it
   can't be found in the time, say so in the note as U.
5. **If** a name's handler cannot be found or classed in the time, list it as U with what was tried. Do not guess.

**Done:** the plan, the F1 line, the REPORTS entry. **The owner reviews this plan** before any SL step is released.

## SO-1b in detail (released 2026-11-15, ACCEPTED the same day)

**SO-1a was ACCEPTED** ([review](../reviews/2026-11-15-so-1a-review.md)). **Branch:** `task/SO-1b` from `origin/main`
(the newest at release). About 2 hours. Engine code, built from `script-objects.md` and `tools/script_objects_ref.py`.

1. **The compiler** accepts `OBJECT_STADT "NAME" n`:
   - it keeps the parsed value's low 16 bits;
   - names match whole and case-folded over ASCII, and the first declaration wins;
   - nothing is looked up in the world at parse time.
2. **The query** `OBJECT "NAME" KILLED`:
   - an unknown name refuses at compile time, as today;
   - each evaluation reads island = low byte, city slot = high byte, live;
   - true when that slot is empty, false when filled; no latch, no stored result, no RNG draw.
3. **The invalid domain** (the review's decision): a missing island or a slot outside the island's city ring refuses
   at evaluation time through the existing `ScriptFault` and rollback path. It is never a guessed Boolean. Document
   it in `script-objects.md` as a difference from the original, with `AnnoScene 0x100118b8` and
   `AnnoWelt 0x100863e0`.
4. **No hardcoded limits:** the ring size comes from the engine's island data, and its source is named (review F2).
   Island and slot are widened from the file bytes into the engine's own id types.
5. **Save and reload:** the definition is program text. No new state, no new save section, no state-version bump.
   If one seems needed, stop and ask a QUESTION.
6. **Tests** that fail on `main` and pass on the branch:
   - every SO-1a vector, driven through the C++ runtime;
   - an alive city, a destroyed one, absent then founded, a bad name, a missing island, an out-of-range slot;
   - a save and reload between two evaluations giving the same answers.
7. **The canary check:** `S01_02_01` now refuses at its *next* unsupported name (`QUESTLOG_SHOW`, payload offset
   1339), not at the objects. Report the exact refusal line.
8. **Fix F1** (add evidence letter P to the legend).

**Done:** the code, the tests, the note updated, 0 pins moved, the REPORTS entry. Every other unsupported query and
action still refuses.

## SO-1a in detail (released 2026-11-15, ACCEPTED the same day)

**Branch:** `task/SO-1a` from `origin/main`. About 1.5 hours. Read only: no engine code.

1. **Start** from the name catalogue in [`bigbrother.md` §6](../../formats/bigbrother.md): the `AnnoScene.dll`
   strings `OBJECT_STADT` (`0x10040244`) and `KILLED` (`0x10040114`). Find the code that reads each.
2. **The definition `OBJECT_STADT "NAME" n`:**
   - what `n` is: a city number within an island, a global city index, a player, or something else;
   - when the binding happens: at load, at INIT, or at each evaluation;
   - what happens if no city with that `n` exists when it binds.
   Do not assume that 4 and 18 are island slots.
3. **The query `OBJECT "NAME" KILLED`:**
   - what makes it true: the city is gone, it changed owner, its last building was removed, or something else;
   - whether the result is read live each time or latched;
   - any state it keeps, and whether a save holds that state.
4. **The oracle:** add the rule to a Python reference (a new `tools/script_objects_ref.py`, or a section of an
   existing one) with hand-derived vectors: bound and alive, bound and destroyed, absent at bind, and a bad name.
5. **The note:** `docs/re/script-objects.md`, with an address for each claim, its evidence letters as in `fisher.md`,
   and what stays unproved.
6. **If** the namespace or the lifetime of `n` cannot be proved from the code within the time, stop and ask a QUESTION
   naming the exact gap. Do not guess.

**Done:** the note, the oracle and its vectors, the oracle's contract test passing, and the REPORTS entry. No sim
code, no pins moved.
