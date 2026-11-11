# MAP-INTEGRATE-2: the three finished Mac packages onto `main` as state 138, the share on the Mac, and what a town still lacks

**RELEASED 2026-11-11** by the Claude Code orchestrator at the owner's request, to **the Mac executor**: one Claude
Code session on the owner's Mac. If both Mac sessions are open, the session the owner starts with this packet owns it;
the other stays idle. The Windows executor works on W4-NEXT-2 at the same time ([packet](w4-next-2.md)).

Work the parts in order: **S → P → I0 → I1 → I2 → I3 → I4 → I5 → R → N → E**.
- **S runs alongside.** It needs the owner and the orchestrator, so don't wait on it.
- **R may run while long gates run.**
- **Each part ends with a REPORTS entry and a push** of `task/MAP-INTEGRATE-2`.
- **After a context compaction,** re-read this packet and your latest REPORTS entries before you continue.

## Why this packet

Three Mac packages are finished and independently reviewed, but none is on `main`.

| Package | State | What the player gets | Head to merge | CI on that head |
|---|---|---|---|---|
| MAP-EXTRACT-1 | 135 | Salt: a mine at a legal mountain site binds an ore deposit, mines it, and salt reaches the city | inside the head below (`2f32437a`) | — |
| MAP-EXTRACT-2 | 137 | Iron ore → iron → tools, the quarry and bricks, the walk end on the goal sub-cell (T3-Q1), the resident's walk home (T3-Q2) | `fix/MAP-EXTRACT-2-ci` **`a5a3bca3`** (contains `task/MAP-EXTRACT-2` `ce14b264` and `task/MAP-EXTRACT-1-x2` `2f32437a`) | run 42804157700, 10/10 |
| MAP-ROUTE-1 | 136 | Automatic freight between owned warehouses, with assign, suspend and cancel | `claude/ci-fix-map-route-1` **`49ffcee6`** (contains `task/MAP-ROUTE-1-main` `a736361a` plus CI fixes `525b356e` and `c6877a97`; two later port commits are reverted on it, so `git diff c6877a97 49ffcee6` is empty) | run 42360092553, success |

- **Each package started from state 134 and took its own number.** Nothing new can build on them while they stay
  apart.
- **The owner can't see them yet.** The owner tests through the in-game UI on `main` (decision M3), so tools, bricks
  and routes are invisible to the owner until they are on `main`.
- **`main` has moved about 223 commits since** (`a28005be` when this packet was written). The moves are the W4 UI,
  the test coordinator, the 18 real-file repairs, and the five `anno_app_tests` fixes that the MAP-EXTRACT-2 report
  attributes to its old base.

**Textual conflicts the orchestrator measured** (`git merge-tree`, 2026-11-11, `main` `a28005be`):

| Merge | Paths in conflict | Notes |
|---|---|---|
| `main` + `49ffcee6` | 3 | `REPORTS.md`, `world.hpp`, `src/test_support/tests/budget_f3_test.cpp` |
| `main` + `a5a3bca3` | 23 | Mostly identity pins, goldens and the stream oracles; also `world.hpp`, `cargo_rules_bytes.txt`, `ship_holds.txt`, `tools/sim_stream.py`, `tools/integration_stream_ref.py` |
| `a5a3bca3` + `49ffcee6` | 23 | Mostly the same paths, without `docs/open-questions.md` and `budget_f3_test.cpp`, plus `src/sim/src/apply.cpp` and `docs/sim/state.md` |

- **Not measured:** the real second merge, `a5a3bca3` into `main` plus `49ffcee6`. Expect about 25 paths.
- **Compiles only with a fix:** `src/app/src/play.cpp`. `main`'s `DescribePayload` and `payload_kind_name` cover
  every `Payload` alternative, and a `static_assert` stops the build when one is missing (line 797). 137 adds
  `Quarry`. See §0.1.
- A conflict-free merge is not evidence that the semantics agree.

**Done means:**
- `task/MAP-INTEGRATE-2`, branched from `main`, holds all three packages under one new identity, **state 138**.
- Each package's behaviour is preserved and proven on the combined binary.
- 134, 135, 136 and 137 artifacts are refused explicitly.
- The Mac gates pass, and CI is green on the branch head.
- Independent reviews approve every package, and a fresh reviewer approves the whole.
- **OQ-556** is answered from the original's code, or narrowed with addresses.
- `docs/sim/progression-gaps.md` gives a measured, ranked list of what still stops a town from progressing. The next
  gameplay packets come from it.
- The Mac has the shared folder, read-only.

**After you stop, the orchestrator:**
1. reviews the candidate for acceptance;
2. runs the Windows game-file run at the merge (owner rule of 2026-11-07);
3. lands it with `tools/orch/land.py`.

**You never push `main`.**

## 0. Rules for this run

### 0.1 Branch, base and the merges you may make

- **Make a new worktree in your usual worktree folder:** `git fetch origin`, then
  `git worktree add <folder>/anno1503-MAP-INTEGRATE-2 -b task/MAP-INTEGRATE-2 origin/main`. Record the `main` commit
  in your first REPORTS line.
  - A new worktree has no `game/app`. Point `ANNO1503_GAME_DIR` at the absolute `game/app` of the checkout that has
    the files.
- **Authorized merges:**
  - exactly two local merges of the two exact heads, **by hash**, `--no-ff`: first `49ffcee6`, then `a5a3bca3`;
  - later merges of `origin/main` into `task/MAP-INTEGRATE-2` at a checkpoint, when `main` moves, **only before
    I5 freezes the source**. Never rebase.
- **Not authorized:**
  - pushing `main`, force, rebase;
  - committing to the three input branches; they stay frozen;
  - merging any other branch (`task/MAP-EXTRACT-1-main`, `task/MAP-2-needs2-main`, `task/MAP-EXPAND-1-main`, any
    W4 branch);
  - another executor's worktree.
- **Windows code: `src/app`, `src/render`, `src/ui`.** Change only what the combination forces, with no behaviour
  change, and list each change in the consumer delta at E:
  - a pinned version, a fixture path, a refused-save message in a test;
  - **the `Quarry` case `play.cpp` needs to compile:** one `DescribePayload` overload and one `payload_kind_name`
    entry, plus one case in `src/app/tests/inspect_building_test.cpp`. Keep it minimal: `task/W4-ui-4` edits both
    files in other hunks, and whichever branch lands second merges them.
- **The Windows executor's branches, and where they meet yours:**
  - **`task/W4-ui-4`** touches `src/app` (`play.cpp`, `viewer.cpp`, `selection.*`), `src/test_support` (the peak
    launcher), `tools/test` and the docs. It does not touch `budget_f3_test.cpp`.
  - **`task/W4-shared-list-1`** touches `src/tools/sim_replay/CMakeLists.txt` (+12 lines near line 117) and
    `src/tools/sim_replay/main.cpp`. Your I2 rewrites the registrations in the same CMake file (lines 45–80). Keep
    your hunks apart from theirs.
  - **`budget_f3_test.cpp`:** both inputs carry a copy of `main`'s older `3f7e64d9`, since replaced by `209cb2ba`,
    `19c94567` and `3be3aaae`. Take `main`'s file.
- **Open questions:** yours are **OQ-580 to OQ-599**, reserved for this packet. `main` ends at OQ-567; `task/W4-ui-4`
  uses 568–570 and `task/W4-shared-list-1` uses 571.
- **Push `task/MAP-INTEGRATE-2`** at every checkpoint, with an ordinary push.

### 0.2 Identity: the orchestrator's compatibility decision

The MAP-ROUTE-1 dispatch requires a fresh decision for the combination (TASKS.md, MAP-ROUTE-1, "Identity and
integration boundary"). It is this:

- **The combined identity is state 138:** 134 + 135 + 137 + 136. It is reserved for this packet from now on.
  - Don't take `max()`.
  - Don't relabel any 136 or 137 artifact as 138.
  - Don't amend the published 136 or 137.
- **Keep PRNG timeline 5 (`kPrngTimelineVersion`), replay framing 4 (`kReplayLogVersion`), command wire 1
  (`kCommandWireVersion`), ECONBK1 and native wrapper 1 (`kSaveVersion`, `world.hpp`)** only if I0 shows the
  combination needs no change. Equal numbers alone prove nothing.
  - **Commands:** 136 appends Body alternatives 12/13, tags F075/F076. Prove that nothing in 135 or 137 uses those
    positions or tags.
  - **Draws:** prove that no combined change reorders draws beyond what each input's own reviewed contract records.
  - **If a real collision or a draw-order change appears,** write a QUESTION with options. Don't pick silently.
- **At 138, refuse 134, 135, 136 and 137 native states and replays explicitly,** each with its version diagnostic.
  The existing refusals of 133 and older stay.
  - Keep every old artifact as history, under its original name and hash.
  - New current fixtures go to a new `current138` location.
  - No migration.
- **Unchanged:**
  - every gameplay default (`Rules::house_updates` and `city_booking` stay off by default);
  - the owner decisions T3-Q1, T3-Q2 and Q-A as implemented;
  - a world with no route keeps its behaviour and draw order.
- **A6 is still the owner's** (rebind a mine at load).
  - Keep what the code does now, option B: the saved deposit binding survives a reload.
  - If the orchestrator relays the answer "A" before I5 freezes the source, implement it inside 138 as
    `map-extract-2.md` A6 describes. If it comes later, it is a follow-up.

### 0.3 How to work: check, don't guess; don't stop

- **When in doubt, check the evidence first.**
  - **The game's data or behaviour:** read the original files in `game/app` (read-only; nothing from them into git)
    or the disassembly (module + address).
  - **Our code or the host:** read the code, or run a bounded measurement.
  - Never fill a gap with a plausible value.
- **Don't wait for a reply.** If a step needs a decision, write a **QUESTION** in REPORTS: what is needed, the
  options, and your recommendation with its evidence.
  - Then skip only the work that depends on it, and continue with the next independent step.
  - Stop only at E, or when nothing independent is left.
- **Who decides what:** an obvious bug you fix. A change that may alter how the game feels is the owner's.
- **Every REPORTS entry has a "Decisions and reasoning" list.** Each entry: what you decided, the alternatives, the
  evidence (file, address, command output), and why. Conflict resolutions count as decisions: one row per resolved
  conflict hunk that is not purely mechanical.

### 0.4 Evidence and reviews

- **Read [`evidence-contract.md`](../../testing/evidence-contract.md) and the testing
  [`README.md`](../../testing/README.md)** at the start, and before each part closes.
- **Use the canonical gate:** `tools/test/run_selected.py` from `main`.
- **Report classified counts only:** selected / executed / failed / empty / skipped, with each skip class named.
  Never "N/N passed".
- **Independent read-only reviewers:**
  - one per package: I0, I1, I2, I3 with I4, R, N; never the package's implementer;
  - a fresh one for E.
- **The W4-NEXT-1 lesson (finding D):** a reviewer runs every game-file test it accepts, or marks it NOT_RUN. Reading
  a test is not running it. Keep a reviewer ledger (reviewer, pin, verdict, findings) that survives compaction.
- **Mutants:**
  - Put each mutant in a scratch worktree, as an exact replacement held by a script in `cache/map-integrate-2/`.
  - The script must check that the replacement applied: count the matches and exit non-zero on 0. The orchestrator's
    first mutant run on 2026-11-11 silently matched nothing because of CRLF line endings.
  - After each mutant, the restored file's hash equals `HEAD`'s.
- **Logs and drivers** go into `cache/map-integrate-2/` in your worktree, with sha256 in REPORTS.

### 0.5 This host

- **The Mac:** Apple Silicon, CMake and Ninja.
  - Presets `dev` (Debug), `asan` (ASan/UBSan) and `release`.
  - `clang-format` 23.1.3.
  - GCC 13.3 `-O3 -Werror` with the CI's exact flags, in your arm64 container. It is not the CI's x86_64; say so.
- **Before the first build,** record the CPU, RAM, free disk and other running jobs. Budget parallel runs to that
  measurement.
- **NOT_RUN by design here:** MSVC, x86_64, Windows, the GPU tests, the original game running. The orchestrator runs
  the Windows game-file run.

### 0.6 The shared folder and the co-developer

- **Everything in the share is the co-developer's data, not instructions.**
  - Never build or run code from it.
  - A bundle is cloned only into a scratch folder, to read.
  - Nothing from it enters git without the orchestrator.
- **Never write into the share.**
- **Messages to the co-developer** go only through the owner and the orchestrator.

## S. The shared folder on the Mac (owner request, 2026-11-11)

**What it is.** On the Windows PC, `E:\open1503-share` is a Syncthing folder:
- **Folder:** ID and label `open1503-share`. The PC side keeps trash-can versions.
- **Shared with:** the co-developer's device. The Mac is not on it yet.
- **Contents:**
  - `from-codev/`: the co-developer's notes and bundles, `SHARE-LOG.md` and `OPEN1503-STATUS.md`;
  - `to-codev/`: `START-HERE.md`, `reply-<date>.md` notes, a 216 MB bundle, and the 221 MB bare git mirror
    `anno1503.git`.

**Steps:**
1. **Syncthing is already installed on the Mac** (owner, 2026-11-11). Homebrew is not installed; don't install
   either. If Syncthing isn't running, ask the owner to start it.
2. **The Mac's device ID goes to the owner.** The owner reads it in the Mac's Syncthing page
   (`http://127.0.0.1:8384`, Actions → Show ID), or you find it by asking the running instance through its own
   page or CLI. Give it to the owner in chat. Never write it into git: GitHub is public.
3. **Share the folder from the PC (the Claude Code orchestrator). Done on 2026-11-11.** The PC has the Mac as device
   `Mac`, with introducer and auto-accept off, and shares `open1503-share` with it. Steps 1 and 2 are therefore done
   as well.
4. **Accept the device and the folder on the Mac (the owner).** In the Mac's Syncthing page
   (`http://127.0.0.1:8384`), accept the PC's device, then the folder offer, with these settings:
   - **Folder Path:** `~/open1503-share`;
   - **Advanced → Folder Type:** **Receive Only**;
   - **Ignore Patterns:** tick "Add ignore patterns" and save; the folder then starts paused. Enter
     `/to-codev/anno1503.git` and `/to-codev/*.bundle`, save, then resume the folder. The Mac works from GitHub, so
     it doesn't need those 437 MB.
5. **Verify (you).**
   - The folder shows "Up to Date".
   - List the files. Check the sha256 of each `from-codev/*.bundle` against its row in `SHARE-LOG.md`.
   - In REPORTS, write file counts, total size, the sync state and any mismatches. Don't copy content.
   - If the folder ever shows "Locally changed", use "Revert Local Changes". Receive Only never sends them.
6. **What it is for in this packet.** Part N may read `from-codev/OPEN1503-STATUS.md` for pointers. Cite each as
   "co-developer note, unverified", and check it against our files or the disassembly before you use it. The
   co-developer's game files are a different build from ours (different `Debug.dat`, `MainMedium.zei`,
   `MainSmall.zei`, `1006.bik`), so compare hashes before you compare findings.

S blocks nothing. If Syncthing isn't ready, continue with P.

## P. Preflight

1. **Fetch, then record:**
   - `origin/main`;
   - that the two heads exist (`git cat-file -t a5a3bca3 49ffcee6`);
   - the three merge bases: `main`∧`49ffcee6` = `e0bb6be7`, `main`∧`a5a3bca3` = `70d311cd` (re-measure both), and
     `a5a3bca3`∧`49ffcee6`;
   - the CI results named above: `gh run view <id>`; if `gh` isn't signed in on the Mac,
     `curl -s https://api.github.com/repos/Dom-cmyk727/anno_rebuild/actions/runs/<id>` (the repository is public).
2. **Read in full** `CLAUDE.md`, this packet, and TASKS.md: the owner rules at the top, the MAP-ROUTE-1 dispatch and
   §1. Read `STATUS.md`. Then read the inputs' own records, each with `git show <head>:<path>`:
   - **`a5a3bca3`:**
     - `docs/orchestration/tasks/map-extract-2.md` (on `main`);
     - the MAP-EXTRACT-2 final report (part E) in that head's `REPORTS.md`;
     - `docs/sim/salt-extraction.md`, `iron-tools.md`, `quarry-bricks.md`, `gold-gems.md`;
     - `docs/sim/windows-consumer-delta-137.md`;
     - `docs/sim/population-live.md` §27;
     - OQ-552..556.
   - **`49ffcee6`:**
     - `docs/orchestration/reviews/2026-11-06-map-route1.md`;
     - the R0–R7 and final entries in that head's `REPORTS.md`;
     - `docs/sim/ship-trade-routes.md`, `docs/re/ship-trade-routes.md`;
     - `docs/testing/automatic-freight.md`;
     - `docs/orchestration/evidence/map-route1/receipts.json`.
   - **Both:** `docs/sim/state.md`, `state_fields.hpp`, `world.hpp`, `tools/sim_stream.py`,
     `tools/integration_stream_ref.py`, and `docs/sim/gameplay-integration.md` (MAP-INTEGRATE-1's record).
3. **Run `python3 -I tools/test/run_selected.py --preflight-only` and `python3 -I tools/test/workflow_test.py`.**
   Both must pass.

**The orchestrator reviewed the MAP-EXTRACT-2 T5 fix** (the two-reserver reload test: `427b4df6`, `3f529f24`,
`a5a3bca3`): **ACCEPTED** ([review](../reviews/2026-11-11-map-extract-2-t5-fix.md)).
- **What the review ran:** the case passes with the game files, and the unsaved `Mine::reserved` mutant fails it at
  `iron_interrupt_test.cpp:418` and `:420`.
- **What that means for you:** the case is a required check in I3 step 3 and I4.

## I0. Freeze the inputs, map the semantic conflicts, write the 138 contract

1. **Inventory each input's range** from its merge base with `main`, including files changed on one side only whose
   callers changed on the other. Re-measure the conflicts with `git merge-tree --write-tree --name-only` at your
   `main`.
2. **Check these interactions in the code.** For each, write the old responsibility on each side and the combined
   one:
   1. **Saved state.**
      - 136's route state (cursors, waits, credits, generations, controls, rule fields).
      - 137's additions: the `Quarry` payload, the claim set per entrance, `HarvestState::goal_col/goal_row` for job
        `0x2c`.
      - 135's mine deposit binding.
      - Decide the combined field order in `state_fields.hpp` and `docs/sim/state.md`. Every saved field keeps one
        line in its `fields()` and in the `field_count` static_asserts.
   2. **Commands.** 136's F075/F076, against every command 135 or 137 now accepts (Build of mines and quarries); and
      the decoder's tag and variant order.
   3. **The `Building::payload` variant.** 137 adds `Quarry` as the tenth alternative.
      - Find every visitor in the whole tree: `git grep -n -E 'std::visit|get_if<|Payload' -- src`.
      - Each visitor 136 added (freight, warehouse visits) handles `Quarry`, or is shown never to reach it.
      - The same holds for `main`'s `src/app/src/play.cpp` (`DescribePayload`, `payload_kind_name`; §0.1).
   4. **Goods and cargo.** Automatic freight moving the new goods (salt, iron ore, iron, tools, bricks), the ship-hold
      rules, and the conflicts in `cargo_rules_bytes.txt` and `ship_holds.txt`.
   5. **Walks.** 137's T3-Q1 gives transport, harvest and resident walks one shared walk end. 136's production-backed
      freight uses carts to and from warehouses, so its pinned wood-cycle counts may move. Explain any move; don't
      force it.
   6. **Draws.** List every draw site each input touches, and show the combined order.
   7. **Rules and registry digests.** Every rules field each input adds enters the digest exactly once.
   8. **Goldens and pins:**
      - every `kStateHashVersion == 13x` check;
      - the `current13x` fixture folders;
      - the `src/tools/sim_replay/CMakeLists.txt` registrations (`main`'s `expand1/current134` logs);
      - `scenario_corpus.txt`, which 137 edited twice (`producers_moved`/`warehouses_moved`).
   9. **`main`'s own sim changes since the bases.** Keep them all, including the `5e7b13c6` real-file repairs and the
      commits that fix the five `anno_app_tests`.
   10. **Open questions.** The tools/quarry branch adds OQ-552..556; the route branch adds none. `main` skipped
       552..559, so no renumbering is expected. Verify that.
3. **Write a section "MAP-INTEGRATE-2 (state 138)" in `docs/sim/gameplay-integration.md`.** It holds:
   - the input table;
   - the interaction table above, with the resolution for each;
   - the identity decision and the refusal table;
   - the evidence plan;
   - an acceptance table: a legal control and the smallest counterexample for each decisive interaction.
4. **Exit:** an independent read-only reviewer approves the contract. Merge-resolution commits come after.

## I1. Merge 136, then 137, and resolve explicitly

1. **Merge the route head:** `git merge --no-ff --no-commit 49ffcee6`. Resolve, then commit.
   - Build with `dev`. Run the route's focused tests and `world.hpp`'s dependants.
   - This intermediate tree still says 136. It is a checkpoint, not evidence.
2. **Merge the tools/quarry head:** `git merge --no-ff --no-commit a5a3bca3`. Resolve, then commit.
   - The tree then holds both packages, but its version constant is whatever the resolution left. It is a checkpoint,
     not evidence, until I2 allocates 138.
3. **Resolution rules:**
   - **No wholesale ours or theirs** on a conflicted subsystem.
   - **`REPORTS.md`.** Each input carries the board's old history, about 2 MB; `main` has slimmed it
     (`REPORTS-1.md`).
     - Take your branch's file (`HEAD`): `main`'s, plus your own S, P and I0 entries.
     - Then append verbatim only the entries each input added after its merge base with `main`: the route's first,
       then the tools/quarry branch's.
   - **`TASKS.md`, `STATUS.md`, `HISTORY*.md`, `REPORTS-1.md`:** take `main`'s versions. The orchestrator owns them.
   - **`docs/open-questions.md`:** merge by OQ number.
4. **Keep these, each named in your I1 report with the test that holds it:**
   - **135:**
     - the faithful mine default: an unbound mine is idle and never searches again;
     - `Reject::NoDepositInReach` stays reserved and is never produced;
     - the `mine_site_deposit_in_reach` query;
     - imported mines stay inert.
   - **137:**
     - data-driven placement (`PlacementParams::mountain_wares`), with gold and gem mines `Unsupported`;
     - the re-lookup rule;
     - `ConstructionDenied` for stage- or research-locked types;
     - the T3-Q1 sub-cell rule and the T3-Q2 walk home with its failure branch;
     - the per-entrance claim set (no hardcoded 8) and mason job `0x2c`;
     - notice `0x15B`;
     - diagnostics stay unsaved.
   - **136:**
     - assign, suspend and cancel; manual control returns;
     - no new draws;
     - no route means no change;
     - the R4 authority recovery and the conservation ledgers.
   - **`main`:** everything since the bases.
5. **Exit:** an independent diff-and-caller review confirms that neither input lost behaviour.

## I2. State 138 and the compatibility evidence

1. **Allocate 138 in one commit,** with the exact delta written into `docs/sim/state.md` and the I0 section: fields,
   streams, digests, draw order.
2. **Refusals.** 138 refuses real 134, 135, 136 and 137 states and replays, each with its version diagnostic.
   - Next to every refusal, keep a valid 138 control.
   - Keep the replay-header mismatch and the embedded-native mismatch distinct, so one earlier check can't cover the
     other.
   - A damaged envelope must not satisfy a version-refusal test.
   - Use the preserved fixtures of each input. The route review recorded that no valid 135 artifact existed for
     its refusal test. Now one does (in the `a5a3bca3` tree), so test it.
3. **The stream oracle.** Transcribe the combined layout independently in `tools/sim_stream.py` and
   `tools/integration_stream_ref.py`.
   - Compare populated worlds, not only an empty one: a route ship in flight, a quarry with claims, a bound mine with
     two reservers.
   - A hash produced by the C++ is a rebaseline, not an oracle.
4. **Goldens.**
   - Regenerate the current fixtures into `current138`, only from reviewed behaviour.
   - Explain every changed golden in one table: file, old value, new value, cause. That includes 137's
     `scenario_corpus.txt` deltas and 136's route goldens on the combined binary.
   - Move the `sim_replay` registrations that test current behaviour to 138. Keep the historical ones as refusal
     tests.
5. **Exit:** the focused native and replay checks pass, and an independent review approves. Do this before any long
   canary.

## I3. The cross-feature proof

Build a small fixture and label it as authored: it is not an original scenario. Use public commands only.

1. **Freight of the new goods.** A route carries a good from the new chains, tools or bricks made by the real chain,
   between two owned warehouses for at least two automatic cycles.
   - The goods ledger closes exactly: made − used − in transit = the change in the stores.
   - No driver Move/Load/Unload per stop.
2. **Mine plus route.** A mine's output reaches the other settlement by route.
   - Demolish the mine mid-cycle. The route goes on and loads what is there.
   - No negative stock, no double credit, no stale reservation.
3. **One reload boundary with all three states live:**
   - a loaded route ship under way;
   - a quarry worker holding a claim;
   - a mine with two reservers (the T5 case).

   Check reload == uninterrupted after 800 advances: `state_hash`, PRNG cursor, save bytes. Then check that a run one
   advance shorter differs.
4. **Suspend and cancel a route** while the mine, quarry and cart chains run. Manual control returns, and nothing
   stays reserved.
5. **One targeted wrong reading per seam.** Each fails its assertion while the legal control passes. Examples:
   - a route visitor without the `Quarry` case;
   - the route state left out of the archive;
   - the claim set not restored;
   - an F075/F076 tag swapped.

**Exit:** the focused Debug and ASan runs pass, then an independent review. If a check fails, isolate the cause before
any long run.

## I4. The inputs' real-data results on the combined binary

1. **Re-run every accepted real-data proof whose code path the combination touches:**
   - **136:**
     - R5, the unchanged original's automatic production freight: at least two automatic cycles, exact ledgers;
     - R6: continuation, fresh-process replay, bounded scale.
   - **137:**
     - the T3–T5 iron chain on the pilot, including the two-reserver reload case;
     - Q3's brick proof;
     - T3-Q2's walk home on the 7-city fixture. The old figure was 13 of 861 failing.
   - **135:** the salt proofs.
   - **`main`:** the 18 real-file tests, and the expansion and ship-start canaries.
2. **A long run you don't repeat** is reused only with a dependency argument: name the files it depends on, and show
   that none changed.
3. **A count that changes** is explained through the combined semantics first, never forced.

## I5. Frozen gates on the Mac, then CI

1. **Freeze one clean source.** Then run:
   - **Debug, the whole suite, with `ANNO1503_GAME_DIR`.** Not a name selection: Q3 had two failing tests outside a
     name-selected gate.
   - **The same without `ANNO1503_GAME_DIR`.** Skips must be clean skips.
   - **ASan/UBSan** over `sim`, `sim_load`, the replay tools and every affected test, including I3 and the canaries.
     Say whether it covered the whole suite or the affected part.
   - **`clang-format` 23.1.3** on every changed file.
   - **GCC 13.3 `-O3 -Werror`** over every changed `.cpp`, plus a Clang build.
   - **`tools/agree.py`** where an `anno-dump` command changed.
   - **The stream references.**
2. **Push.** CI on the branch head must be green in every job, `gate` included.
   - Don't re-run a red job until it turns green without a cause.
   - A known flake: name it and the `main` commit that fixes it.
3. **A late code fix** re-runs the affected checks. A prose-only change does not reset the matrix.
4. **If `main` moves after the freeze,** don't re-run the whole matrix. Report it; the orchestrator decides.
5. **NOT_RUN by design:** MSVC with the game files, x86_64, Windows, GPU, Wine.

## R. OQ-556 from the original's code

This part is docs only. Run Ghidra on the Mac: CLAUDE.md says RE runs on macOS or Linux, never on the Windows host.

The binary is `AnnoWelt.dll`, sha256 `f220a878...`. The full hash is in
`docs/orchestration/evidence/map-econ1-e1-source-identity.txt`, line 3.

1. **Read the start flood `0x10003aa0` and the goal admission that `path_home` uses.** Record:
   - how the start keys are built for a figure standing inside a hemmed-in stand;
   - which sub-cells start;
   - how the goal is admitted.

   Name every address range you read.
2. **Apply the rule to tile (322,770) of the 7-city fixture,** by hand or with a small Python reference in `tools/`.
   Does the original's start set include sub-cells (0,1), (0,2) and (1,2), and does it reach a goal?
3. **Verdict:**
   - **The sim matches:** the ~1.5 % failures are the original's behaviour.
   - **The sim differs:** name the difference, and propose the change with its identity effect.

   Change no behaviour in this packet. A difference becomes a QUESTION and a proposal for the next package.
4. **Update OQ-556 and `population-live.md` §27,** each claim with module and address. An independent reviewer
   re-reads the addresses.

## N. What still stops a town (measurement and research; no behaviour change)

The goal: the next gameplay packets come from evidence, not guesses.

1. **Choose three towns and declare each run's game time before you start:**
   - the 7-city `endless02` fixture;
   - the settlement founded from the ship start (MAP-EXPAND-1's path);
   - an original scenario with an advanced town, such as `S01_07_01`.

   Run each on the 138 binary with `Rules::house_updates` on. Record whether city booking was on.
2. **Per house group (Pioneers → Settlers → Citizens → Merchants → Aristocrats), record:**
   - which predicate of `UpgradeCheck` fails, using the one-predicate near misses of `population-live.md` §5: food,
     goods, infrastructure, materials, next type;
   - which goods and buildings a group needs, and whether the sim supports each: implemented, a typed `Unsupported`
     refusal, or missing;
   - which build goods run out.
3. **Check every need against the original's data:** the parsed `BGruppen.dat` needs per group, and the
   `Anlagen.dat` production chains, stages and research gates. Name what only the running original could confirm (the
   Windows Wine runner).
   - **Related Windows work:** `task/W4-shared-list-1` measures some of the same things. It adds a population tool
     (`cmake/RunPopulationTool.cmake`, `src/tools/sim_replay/main.cpp`) and `docs/research/shared-test-list.md`.
     Read them with `git show`, reuse what fits, and cite them. Don't merge that branch.
4. **Write `docs/sim/progression-gaps.md`.**
   - A ranked table: rank by how many houses a gap blocks, at which stage, in the measured towns.
   - Per gap:
     - the evidence;
     - where the original's rule comes from (file, address);
     - an estimated size;
     - its dependencies, including owner decisions such as D8 (Venetians and trade, `docs/plan/lan-mvp.md` §8), the
       wildlife decision for hunting, and disasters and fire.
   - An outline for each of the top three as a next packet.
5. **Measurement code** goes into opt-in tests only, with a declared skip class. Keep it cheap.

## E. Final review, handoff, stop

1. **A fresh independent reviewer checks:**
   - both input ranges and every merge resolution;
   - the identity and refusal boundaries;
   - I3 and I4;
   - the gate logs, running the game-file tests it accepts or marking them NOT_RUN;
   - R's addresses, re-read;
   - one of N's towns, re-run.
2. **Write `docs/sim/windows-consumer-delta-138.md`.** It merges 137's delta and the route's consumer card
   (`docs/testing/automatic-freight.md`):
   - what the Windows app must handle: the `Quarry` payload, the route commands for a later UI, the refused old saves,
     the 138 pins;
   - each item cited header:line and tied to a test.
3. **Write one handoff entry in `REPORTS.md`:**
   - the pins and merge commits;
   - the conflict-decision table;
   - the identity and refusal table;
   - a verdict per part;
   - classified counts and artifact hashes;
   - the reviewer ledger;
   - the NOT_RUN list;
   - Decisions and reasoning;
   - QUESTIONs.
4. **Push, then stop.** Then the orchestrator:
   1. reviews for acceptance;
   2. runs the Windows game-file run at the merge before landing;
   3. lands with `land.py`.

**Out of scope:**
- new gameplay features, balancing, default flips;
- UI, render, audio, network;
- the coordinator;
- hunting and wildlife (waits on the owner);
- gold and gems implementation (G stays an inventory);
- a fix for OQ-556.
