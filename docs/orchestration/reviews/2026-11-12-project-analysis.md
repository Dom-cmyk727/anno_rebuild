# Project analysis and handover for the orchestrator (2026-11-12)

**Who asked, and for what.** The owner asked a Claude Code session on the Windows PC to find out why progress
feels stalled (long test runs, many review rounds, small gains), to list issues, bugs and improvements that bring
gameplay forward, to compare the GOG and History Edition files, and to hand the result to the orchestrator.
This document is that handover. It changes no code. Base: `origin/main` `3eef75a5`.

**How to read the evidence marks.**
- **M** measured in this session on the Windows PC (commands and raw numbers below).
- **C** read in the code or files of `main` by this session.
- **R** reported by a read-only subagent of this session from the reviews, reports or code, and spot-checked only
  where marked **C** as well. Counts of review findings by kind are a reader's classification, so approximate.
- **E** estimate.

**Companion documents (same branch `docs/analysis-2026-11-12`).**
- [`docs/research/he-vs-gog-comparison.md`](../../research/he-vs-gog-comparison.md): the edition comparison.
- [`docs/adr/0005-edition-neutral-content.md`](../../adr/0005-edition-neutral-content.md): the owner's decision on
  mixed editions.

## 1. The findings in short

1. **The hours come from one build setting.** The WSL sanitizer gate compiles without optimisation. One complete
   gate takes 4.3 to 8 hours and all three recorded ones ended red (M). The same tests under the same sanitizers
   run 5.5 to 5.9 times faster at `-O1` (M). The Windows Release run with game files takes about 13 minutes.
2. **The review loop spends its rounds on tests and paperwork.** In the Windows lane since 2026-11-04, 6 of 97
   review findings were something a player could notice (R). W4-NEXT-2 ran the full suite at least ten times in
   one day, through three review rounds, for a change that touches no file under `src/sim`, `src/render`,
   `src/formats` or `src/assets` (R).
3. **Little of the gameplay reaches the window.** The live window never switches population on and draws only
   buildings and ships (C). Salt, tools, quarry, bricks and automatic freight are finished but not on `main` (R).
   There is no network code, and ADR 0003 is still "Proposed" (C).
4. **Mixed editions are solvable and decided:** ADR 0005, accepted by the owner today.
5. **Wildlife is the same game in both editions**, and the History Edition's class names make the remaining reads
   shorter. The owner leans to the faithful port (option 1).

## 2. Where the test time goes

### 2.1 Measurements

**Complete WSL gates on this PC (M, `~/anno-gate-logs/*/wsl-*.txt` in Ubuntu):**

| Gate | Start to end | App lane | Sim lane | Render lane | Lavapipe GPU lane | Result |
|---|---|---|---|---|---|---|
| `w7fix2` (2026-11-07) | 4.3 h | 8,138 s | 3,531 s | 2,136 s | not run | rc 1 |
| `w4main2c1` (2026-11-07) | 7.5 h | 8,565 s | 4,568 s | 2,481 s | 9,532 s | rc 1 |
| `w4ui2parte` (2026-11-08) | 8.0 h | 8,410 s | 4,856 s | 4,980 s | 8,175 s | rc 1 |

**The same tests, Windows Release against the WSL gate (M; `CTestCostData.txt` of the game-file run of
2026-11-11 against `wsl-w4ui2parte-*.log`):**

| Test | MSVC Release | WSL gate (`-O0`, ASan, UBSan) | Factor |
|---|---|---|---|
| `sailed founding canary: the opening loop ...` | 57 s | 2,540 s | 44 |
| `population A original-data components ...` | 28 s | 1,666 s | 59 |
| `forester opening canary ...` | 13 s | 507 s | 38 |
| `textures, buildings, figures, units and ground match the Rust reference` | 19 s | 855 s | 46 |
| `chunk draw lists replay to the objects ...` | 307 s | 5,109 s | 17 |
| `opening_session_replay` | 135 s | 1,849 s | 14 |
| `scenario corpus ...: every .sze loads, creates and runs five minutes` | 225 s | 1,935 s | 9 |

**The optimisation level alone (M, this session).** `anno_sim_tests` built three times in WSL (GCC 13, the gate's
flags `-fsanitize=address,undefined,float-cast-overflow`, `CMAKE_BUILD_TYPE=Debug` so assertions stay), each test
run alone on the idle PC with the game files:

| Build | `forester opening canary*` | `sailed founding canary: two participants*` | Build of the target, `-j 12` |
|---|---|---|---|
| `-O0` (what `wsl_gate.sh` builds) | 419 s | 444 s | 136 s |
| `CMAKE_CXX_FLAGS_DEBUG="-O1 -g"` | 77 s | not run | 227 s |
| `CMAKE_CXX_FLAGS_DEBUG="-O2 -g"` | 79 s | 75 s | 299 s |

All runs passed with the same assertion counts (18,810 and 76). The build tree was deleted afterwards.
`docs/testing/wsl-gate-root-cause-2026-11-05.md` blamed the software renderer and called the sanitizer overhead
"normal"; the optimisation level was not looked at.

**Other costs inside one gate (M, C):**
- **Required contracts run twice.** `tools/test/run_selected.py` runs every `--require-test` alone and in series
  (lines 146-164), then again inside the selection (line 179). In the app lane of `w4ui2parte`:
  `opening_session_replay` 1,839 s and 1,849 s, `viewer_wiring_opening_controls` 210 s twice,
  `viewer_wiring_settlement_activity` 154 s twice. `picture_matrix_contract` (about 230 s) is added to every lane
  automatically (line 143) and so ran in each of the six lanes. Together about one hour per gate.
- **Three tests fail in every gate**, so no gate can end green and each needs manual adjudication:
  - `anno_render_tests.a flow build that fails leaves the legacy look, final, built once (game files)`
    (`src/render/tests/river_flow_corpus_test.cpp:1322`). Its "settled" test looks at the visible chunks only,
    while `update_chunks` also counts margin and neighbour-group chunks; the review of 2026-11-07 names this as the
    likely cause, not confirmed by a rerun.
  - `anno_render_gpu_tests.play: GPU matches the CPU path on one scene` and `..., on the Radeon iGPU`: they need a
    named adapter, and WSL has only llvmpipe.
- **The lavapipe lane costs 2.3 to 2.6 hours** per gate. Hosted CI (`linux-gcc`) already runs the GPU parity tests
  on lavapipe, and the Windows game-file run covers the RTX 4090 and the iGPU.
- **Lanes run in series at `-j 4`** on 16 threads. The app lane's tests carry `PROCESSORS 4`, so they run one at a
  time: 18,160 test-seconds in 8,410 s of wall time.
- **The Mac lanes** use the same unoptimised presets (`dev` and `asan` are `CMAKE_BUILD_TYPE=Debug`); their
  reports raise per-test limits to 2,400 and 3,600 s (R).

### 2.2 Instructions: GATE-1 (Windows executor; no gameplay)

Step 1 alone is the large gain and fits one small packet. Steps 2 to 4 are a second one. Steps 5 and 6 are
optional follow-ups. The orchestrator cuts them as the packet cap requires. Each step is its own commit.

1. **Build the sanitizer gate optimised.** In `tools/test/wsl_gate.sh` (the `cmake -S ... -B ...` line, about
   line 220) add `-DCMAKE_CXX_FLAGS_DEBUG="-O1 -g"`. Keep `CMAKE_BUILD_TYPE=Debug`. In `CMakePresets.json` give the
   `asan` preset the same cache variable, so hosted CI and the Mac get it too.
   - *Proof:* one full gate at the same commit before and after. Report every lane's seconds side by side. The
     set of passing and failing tests must be the same and `sanitizer_lines` must stay 0.
   - *Check before relying on it:* the deadlines that are scaled for `ANNO_SANITIZE` builds still hold, and no
     new `-Werror` warning appears at `-O1` (the `-O3` job already compiles the code clean).
2. **Run a required contract once.** In `run_selected.py`, leave the tests that ran as required contracts out of
   the selection run (their verdict rows are already in the log), and let a caller name the lanes in which the
   automatic `picture_matrix_contract` runs; `wsl_gate.sh` runs it in the first lane only. Extend
   `tools/test/workflow_test.py` so the contract "required and selected runs once" is checked.
3. **Make a green gate possible.**
   - Confirm the cause of the `flow build that fails ...` failure first (a rerun that shows a margin or
     neighbour-group chunk arriving in the 60-frame loop). Then make the test wait for every chunk
     `update_chunks` counts, or count only what its claim is about. Do not raise a sleep or a retry count.
   - Where the named adapter is absent, the two `play: GPU matches the CPU path` cases report a platform skip.
4. **Stop the local lavapipe lane** as a default part of package gates. Keep `--gpu-lane` for debugging. Say in
   `docs/testing/README.md` which run covers what: hosted `linux-gcc` covers lavapipe without game files, and the
   Windows game-file run covers the real adapters.
5. **Then, and only if still needed,** run the sim, formats and render lanes side by side, or raise `-j`. WSL has
   15 GiB by default; more needs a `.wslconfig` on the owner's PC, which is the owner's to change.
6. **For the Mac:** a `check` preset (Debug with `-O1 -g`) for local gates, and the same `asan` change.

*Expected (E):* 8 hours to roughly 1. This is an extrapolation from two sim tests; the render lane's factor at
`-O1` was not measured.

## 3. The review loop

### 3.1 What was found (R, from the 54 review files and `REPORTS.md`)

- **Findings by kind, all 54 reviews:** 43 player-noticeable product defects, 63 test defects, 54 evidence or
  wording findings, 28 infrastructure findings. **Windows lane and main infrastructure, 2026-11-04 to 11-11 (12
  reviews): 6, 28, 37 and 24.**
- **Seven of the 43 product defects came from the one review that followed the owner's own playtest**
  (`2026-11-03-w4-opening1-playtest.md`).
- **W4-NEXT-2:** three review rounds (F, G, H) and at least ten full game-file runs in one day; the remaining
  findings of the last round were two sentences in documents.
- **W4-SETTLEMENT-1 W7:** eight WSL gate launches, three of which completed.
- **A flaky-test fix accepted on three green reruns was later found wrong** (`viewer_wiring_opening_controls`);
  the reruns took about four hours.
- **Commits by kind, 2026-10-29 to 11-11 (M, `git log`):** 220 `docs(orchestration)`, 194 `test(sim)`, 182 `docs`,
  94 `docs(sim)`, against 69 `feat(sim)`, 51 `feat(app)` and 7 `feat(render)`. Tests are 135,567 of the 248,083
  lines under `src`.
- **State identity:** the stream went from 121 to 134 on `main`, with 135, 136 and 137 on branches and 138
  reserved. Every step invalidates saves, replays and pinned hashes. MAP-INTEGRATE-2 conflicts with `main` in 23
  paths, "mostly identity pins, goldens and the stream oracles" (its packet).
- **Branch age:** the Windows lane ran from 2026-10-30 to 11-05 unmerged (552 commits); MAP-ROUTE-1 waited five
  days, MAP-EXTRACT-1 four and a half, `task/W4-depth-1` seven and is 803 commits behind.

### 3.2 Suggested rule changes (for the owner and the orchestrator; none is made by this document)

0. **Already decided by the owner on 2026-11-12, as the orchestrator session recorded it (the packet cap):** a
   packet is one concern of about two to four hours; its gate is focused tests plus hosted CI; the orchestrator's
   handover review is the independent review; the whole-suite and game-file runs stay with the orchestrator at the
   merge; a packet that needs a state-number change waits for the save-section redesign. The items below add to
   that and do not replace it.
1. **One full sanitizer gate with game files per landing, run by the orchestrator at the merge.** A fix round runs
   the tests the fix touches. GATE-1 is what makes that one gate cost about an hour instead of a working day.
2. **The reviewer fixes wording.** A finding that is a sentence in a document, a missing REPORTS entry or a count
   that does not reconcile is corrected in the acceptance commit and listed there. It does not open a fix round.
3. **A flake is closed by its mechanism, not by reruns.** The reviews of 2026-11-07 and 2026-11-11 both reached
   this; make it the standing rule.
4. **Merge within a day or two, behind default-off flags.** Gameplay that is not accepted for the default can still
   be on `main`, as `house_updates` and `city_booking` are.
5. **One tool regenerates the pins.** Hash pins, goldens and stream oracles are rewritten by a command when the
   state identity moves, and reviewed as one diff, instead of being edited by hand in every branch. Bundle
   identity changes: two packages that both need one land together.
6. **Retire dated protocol text.** `TASKS.md` §1 still says "the executor is a simpler model" (item 7) and "a
   sub-task has no time box" (item 11). `STATUS.md`'s waiting list has stale lines (the coordinator follow-up,
   W4-MAIN-2 parts C and D).

## 4. Gameplay: what a player gets today, and what is missing

### 4.1 Found (C unless marked)

- **The window never turns population on.** `--play` creates the world with `sim::Rules{}`
  (`src/app/src/play.cpp`, the `World::create` call); `--ship-start` sets only `city_names` and the optional
  `city_booking` and says so (`src/app/src/opening.cpp`, the comment at the `rules.city_booking` line). Outside
  tests, `house_updates = true` exists only in `src/tools/sim_replay/main.cpp`. Houses are inert in every window
  session, and there is no income without `--city-booking on`.
- **The window draws buildings and ships only.** `play_buildings` lists the cities' building lists; ships are an
  overlay. No carrier, cart, resident or animal is drawn. Roads are tile objects (`src/sim/src/apply.cpp`,
  `tile_object`), which `play_buildings` does not list, so a road the sim accepts is not added to the picture.
  This is from reading the code; the window was not run to confirm it. `play.hpp:93` still says the sim cannot
  build roads.
- **The fixed-map developer start** gives each of two seats a placed Kontor, the whole island, 10,000,000 gold
  and 190 t of every good (R).
- **No network.** There is no `src/net`; `crates/net/src/lib.rs` is three lines; ADR 0003 is "Proposed" since
  2026-10-26. The sim already has what a lockstep needs: the command and turn codec, `run_turn`, the recorder and
  replay log, `state_hash` and the hash tree, and `PlayRunner` records a state hash per 100 ms turn (R).
- **Finished gameplay not on `main` (R):** `task/MAP-INTEGRATE-2` holds 230 commits: salt (135), iron and tools,
  quarry and bricks (137), automatic freight (136); about 2,900 lines of engine code and 13,700 lines of tests.
  `task/W4-depth-1` (buildings sit in the ground and the water) waits on the owner's depth check.
- **What stops a town (R, `docs/sim/progression-gaps.md` on `task/MAP-INTEGRATE-2`, 317 houses, both flags on):**
  162 houses fail first on a forced consumer good (leather needs wildlife, lamp oil the whaler, clothing pelts),
  131 on food at the upgrade instant (the fisher route is refused, the hunter is missing), 18 on upgrade
  materials, 6 on infrastructure. A city founded from the ship start has no houses and runs out of tools.
- **Other gaps in the sim (R, not re-checked):** player-level upkeep is read but never written
  (`src/sim/src/player.cpp`); bankruptcy has no defeat handling; quarry, hunter, whaler, shipyard, doctor, school,
  fire brigade and the military classes are inert on `main`; trade, fire, combat and diplomacy are absent.
- **`data/maps/lan5.txt` is the map the owner rejected** on 2026-10-29; it is still the default placeholder.

### 4.2 Suggested packets (each one concern; the orchestrator sizes and words them)

- **PLAY-LIVE-1, the living town in the window.** A named rules profile for the window (population, the Inn and
  city booking on) selectable by one flag for `--play` and `--ship-start`. Whether it is the default is the
  owner's decision ("feeling" default). Then draw what the sim already simulates: tile objects that change (roads,
  felled trees, fields) and the figures (carriers, carts, residents). Proof is the owner's playtest card, because
  the playtest is what found product defects so far.
- **NET-0, the first lockstep.** First the owner's word on ADR 0003. Then two peers in one process over a
  loopback transport, exchanging turns and comparing the per-turn state hash the runner already records, with an
  injected divergence detected. No lobby, no join-in-progress yet.
- **Land MAP-INTEGRATE-2** when it reports, with one gate, and keep later Mac packages short-lived (§3.2 item 4).
- **TEST-FIXTURE-1, saved towns for tests.** Tests that need a town load an original scenario and tick from tick
  0, up to 510,000 ticks (`economy_loop_test.cpp`, R). Build such a state once per run, keyed by the state
  identity, save it with `World::save`, and let the cases start from it. Lower priority than GATE-1.

## 5. Defects and loose ends

| # | What | Evidence | Suggested fix |
|---|---|---|---|
| D1 | The sanitizer gate builds at `-O0` | §2.1 (M) | GATE-1 step 1 |
| D2 | Required contracts run twice; `picture_matrix_contract` in every lane | `run_selected.py:141-180` (C), lane logs (M) | GATE-1 step 2 |
| D3 | `a flow build that fails leaves the legacy look ...` fails in every recorded gate | three gate logs (M); `river_flow_corpus_test.cpp:1322` (C) | GATE-1 step 3 |
| D4 | Two GPU tests cannot pass on lavapipe and are counted as failures | gate logs (M) | GATE-1 step 3 |
| D5 | Coordinator test `F3 (D1, deaf)` failed three times on 2026-11-11 and has no owner; the cause of two stalls is UNPROVEN | acceptance reviews of 2026-11-10 and 11-11 (R) | one owner, cause first, as its own packet |
| D6 | 18 tests above 1 GiB take no memory reservation; one peaks at 8,715 MiB | W4-NEXT-2 part M (R) | reservations from the measured peaks (already recommended, unscheduled) |
| D7 | No window session has living houses | §4.1 (C) | PLAY-LIVE-1 |
| D8 | Roads built in the sim are not drawn; stale comment in `play.hpp:93` | §4.1 (C, not run) | PLAY-LIVE-1; confirm in the window first |
| D9 | In a History Edition folder the loaders open the wrong `Anlagen.dat`, the German `Texte.dat` and no scenarios | comparison §6 (C, M) | EDITION-1 |
| D10 | City names from the language file are in the hashed state | `original_registry.cpp`, `state_fields.hpp` stream 120 (C); 59 of 160 names differ (M) | EDITION-2 |
| D11 | The island candidate list comes from a directory listing; the editions differ by one template | `scenario_load.cpp` `scan_island_templates` (C); comparison §3.5 (M) | EDITION-1 |
| D12 | ADR 0003's handshake hashes `FigurenHeader.dat`, which differs between editions | ADR 0003 "Handshake" (C); comparison §3.2 (M) | ADR 0005 point 3; amend ADR 0003 when it is decided |
| D13 | `tools/figuren.py frames` does not finish on the History Edition's figure type 375 | comparison §3.2 (M) | EDITION-1 step 5 |
| D14 | Dated protocol text and stale STATUS lines | §3.2 item 6 (C) | orchestrator, with the owner's word on the protocol text |
| D15 | Player upkeep never written; bankruptcy without defeat | R, not re-checked | verify, then schedule with the economy work |

## 6. Mixed editions: the approved solution and its packets

The decision is [ADR 0005](../../adr/0005-edition-neutral-content.md). The owner approved it on 2026-11-12 with
three choices: add-on rules only for multiplayer, city names as an index in the state, and the 464 shared island
templates as the multiplayer pool.

### EDITION-1: the game-source layer and the parity proof (no state change)

1. **Add the layer** (a small library next to `sim_load`, used by `sim_load`, `app`, `assets` tests and
   `test_support`). It answers, for a game folder: which edition; the path of the buildings file, the figure header
   and volumes, each `Data` table, the language text, a scenario by its folder and name, and the island template
   root. Layouts to recognise, from the comparison:

   | Logical item | GOG 2.0.0.5 | History Edition |
   |---|---|---|
   | Buildings | `Anlagen.dat` | `Anlagen.addon` (never `Anlagen.dat` or `Anlagen.classic`) |
   | Figures | `FigurenHeader.dat` + `Figuren*.dat` | the same names |
   | Data tables | `Data/*.dat` | the same names |
   | Language text | `Texte.dat` | `Languages/Texte.dat` (English), root `Texte.dat` (German) |
   | Scenarios | `Scenes/<folder>/*.sze` | `Languages/Eng/Scenes/<folder>/*.sze` (and `Ger`) |
   | Island templates | `Scenes/Islands/**` | the same |

   Detection uses the layout (for example `Anlagen.addon` and `Languages/` present), never a guess from a size.
2. **Route every reader through it.** Today's fixed names are in `src/sim_load/src/original_registry.cpp`
   (`Anlagen.dat`, `FigurenHeader.dat`, `BGruppen.dat`, `Texte.dat`), `src/sim_load/src/scenario_load.cpp`
   (`Scenes`, `Islands`), `src/app/src/loader.cpp` (`Anlagen.dat`, `FigurenHeader.dat`, `Scenes`) and
   `src/test_support/src/game_dir.cpp` (`Anlagen.dat` as the folder test). Afterwards a search for these names
   outside the layer finds nothing.
3. **Confirm the rule set.** After parsing, compare the type registry's digest (`TypeRegistry::digest()`) with
   the reference value of the add-on rules and refuse another variant with a message that names both. The
   reference value is a number in our code, not original data.
4. **The island manifest.** A file in the repository lists the 464 shared templates by relative name and digest.
   `scan_island_templates` takes its candidates from it and reports a listed template that is missing or differs.
   Check first, from the original's resolver (`AnnoScene` `0x10007c50`), whether the GOG-only template
   `Dschungel/D_080x080_0015_2Ba_Suedsee.scp` can be chosen in an endless game; write down the answer either way.
5. **Settle figure type 375.** Find why `tools/figuren.py frames` does not end on the History Edition's entry 375,
   and what differs in entries 370-376 and 453 (comparison §3.2). Decide from that whether the C++ reader needs a
   guard. Then run the frame comparison over all 368 figure types and record the result in
   `docs/research/he-vs-gog-comparison.md`.
6. **The identity command and the parity test.** `anno-dump content-id <game dir>` prints the edition and one
   digest per section (types, needs and growth tables, walk tables, island pool). A test that needs both installs
   (a second environment variable beside `ANNO1503_GAME_DIR`, skipped as "required assets" where it is unset)
   requires equal section digests, and runs one short replay with the world loaded once from each folder and equal
   state hashes at every turn. With today's code the hashes can only agree if both folders give the same city
   names, so until EDITION-2 the test loads the English text on both sides and says so.
7. **Acceptance:** the Windows game-file run with the GOG folder is unchanged in counts; the same run with
   `ANNO1503_GAME_DIR` pointed at the History Edition install is reported honestly (which tests pass, which
   assume GOG-only content); the parity test passes; no path into the game folder is built outside the layer.

### EDITION-2: language out of the state (needs a new state identity)

1. `Rules::city_names` leaves the hashed and saved rules. A city stores the index the founding code draws today
   (`src/sim/src/founding.cpp`, where `creq.names` is set) and, for a name the player typed, the text.
2. The app resolves an index through the game-source layer in the player's language.
3. The parity test of EDITION-1 then loads each folder's own language and still needs equal hashes.
4. Land it together with the next change that moves the state identity (§7), not alone.

## 7. Wildlife (hunter, hides, pelts)

### 7.1 Status

- **The question:** hunters and fur trappers chase animals that live on the island. The sim has none, so the
  hunter is inert, the trapper never sends its worker (owner, 2026-10-23: "just stay idle until the animals are
  added"), and leather and pelts have no source. That is the first blocker of the measured list (§4.1).
- **The owner's position, 2026-11-12:** leaning to option 1, the faithful port of the original's wildlife, once
  the editions were compared. The comparison is done. **Confirm with the owner when the first packet below is
  dispatched;** this document does not record a final decision.
- **Why it needed the owner:** the fauna sweep consumes draws of the world's random generator, so porting it moves
  the PRNG timeline and the state identity, and it is a new lifecycle (animals appear, move and die), not a reuse
  of farm code.

### 7.2 What the comparison adds (comparison §4 and §5)

- **Both editions have the same wildlife.** Animal, hunter and trapper data are identical. The fauna sweep (GOG
  `0x10093480`, HE86 `0x10048370`) has the identical multiset of indirect calls and the same constants. The hunter
  tick (GOG `0x10069f40`, HE86 `0x10067bb0`) matches in the same way. So GOG stays the reference and one port
  serves both editions.
- **The History Edition names the classes.** Its 32-bit `AnnoWeltAddOn.dll` has RTTI. Mapped to GOG by table size
  and call profile: `tProdJagdhaus` `0x100c82a0`, `tInselServer` `0x100cac2c`, `tRouteJaeger` `0x100c6f90` (the
  "job 0xd" object), `tHirschObject@tInsel` `0x100cac04` or `0x100cac18`, and the animal figures
  `tBewohnerWildTier` `0x100c653c`, `tBewohnerWildTierNew` `0x100c66fc`, `tBewohnerWildTierWolf` `0x100c68e0`.
  Named but not yet located in GOG: `tRouteHirsch`, `tRouteWildTier`, `tJobSteheWildTier`, `tJobWalkRandom`,
  `tJobWalkRohstWild`, `tProdPelzJaeger`. These are structural matches from one pass.
- **Already read before today** (`docs/re/hunter.md`): the hunter tick's arithmetic with 17 vectors and a
  reference (`tools/hunter_ref.py`), the target search, the kill step (the animal is removed and its type capacity
  becomes the cargo), the `HIRSCH` loader's shape, and the sweep's outline.

### 7.3 WILDLIFE-RE-1: the remaining reads (documents only, no engine code)

Method for every item: read the GOG function as the reference; read the HE86 function at the same table slot as
a second, named reading; record both addresses; report any difference in behaviour between the builds, with GOG
deciding. Write the result into a new `docs/re/wildlife.md` (leave `docs/re/hunter.md` as it is and link it).
No disassembly text and no bytes in the document.

1. **The fauna sweep `0x10093480`, in full:** what it does per chunk; how many animals it makes and when; every
   world draw with its position, its modulus and what it selects; the terrain tests (`0x46`, `0x145`); the table
   `0x100e34e8`; whether there is a cap or a count of living animals; what the divide-by-1000 computes. The
   cadence is already known (`docs/sim/state.md`, `fauna_period`).
2. **The creation helper `0x10092ca0`:** its arguments, the figure type it chooses, the position, and whether it
   draws.
3. **`tHirschObject` and the `HIRSCH` loader `0x100946c0`:** the 8-byte entry, the 16-byte object, how its type
   byte becomes a figure type, and which of the two 5-slot tables is the class.
4. **The animal figures.** List the slots `tBewohnerWildTier`, `...New` and `...Wolf` override against
   `tBewohnerBoden` by comparing the tables; read the overridden ones: the tick, movement, the two flag bits of
   `+0x8c` (bit 1 and bit 2 in the kill step), death, and every draw.
5. **Their routes and jobs:** locate `tRouteHirsch`, `tRouteWildTier`, `tJobSteheWildTier` and `tJobWalkRandom` in
   GOG; read how an animal chooses where to stand and walk. A random walk that draws from the world generator
   changes the draw order and must be counted exactly.
6. **The hunter's approach:** locate `tJobWalkRohstWild`; decode the pursuit steps' movement slots (`+0xe8`,
   `+0xd0`, `+0xc8`, `+0xcc`) far enough to state what a hunter does when its target moves, dies or is taken.
7. **The fur trapper:** `tProdPelzJaeger` derives from `tProdPlantage`, not from the hunter's class; state what
   differs from the hunter beyond the goods.
8. **Predators:** what `tBewohnerWildTierWolf` and the `RAUBTIER` figure types do to residents or soldiers. This
   decides whether predators can be left out of the first port. It is a scope question for the owner.

Deliver with it: an independent reference (`tools/wildlife_ref.py`) and hand-derived vectors for the sweep's
selection and draw order, in the style of `tools/hunter_ref.py`, and the exact list of what the engine port needs.
Stop and report when a read does not settle a rule; nothing is invented.

### 7.4 WILDLIFE-1: the port (after 7.3 is reviewed)

- **Scope:** animals created from the `HIRSCH` records at load and by the sweep; their movement and death; the
  hunter's tick (already decoded) with route and jobs; the trapper's dispatch; hides and pelts reaching the store.
- **Behind a rules flag, default off,** as `house_updates` is: the accepted goldens and replays do not move until
  the owner switches the default. It belongs in the "living town" profile of PLAY-LIVE-1.
- **Identity:** `WildlifeSweep` leaves the PRNG timeline's `missing` list, so the timeline version and the state
  identity move. **Land it together with EDITION-2**, so both pay one identity change.
- **Proof:** synthetic tests for each rule from the vectors; a real-file canary on an island with hunters,
  tanners and `HIRSCH` records and no hides at the start (`docs/sim/early-needs.md` §5 lists four); then the
  progression measurement of `docs/sim/progression-gaps.md` again, to show how many of the 162 houses move.
- **Not in it:** the whaler (lamp oil). Its classes have the same family of names (`tProdWalfang`, `tBewohnerWal`,
  `tRouteWal`, `tRouteWalfaenger`, `tOccurrenceWal`), so a later WHALE packet can reuse the method.

## 8. Suggested order

1. **GATE-1** (Windows executor): it shortens every later package.
2. **MAP-INTEGRATE-2** lands when it reports (in progress on the Mac).
3. **EDITION-1** and **WILDLIFE-RE-1** side by side: neither moves the state identity, and the second is
   documents only.
4. **PLAY-LIVE-1**, so the owner's playtests see a living town.
5. **NET-0**, as soon as the owner has decided ADR 0003.
6. **EDITION-2 with WILDLIFE-1**, one identity change for both. Both need a state-number change, so by the
   owner's rule of 2026-11-12 they queue behind the save-section redesign.

## 9. Owner decisions

**Recorded today (2026-11-12):**
- ADR 0005 accepted: the game-source layer, add-on rules only for multiplayer, the join check on parsed content,
  city names as an index, the 464 shared island templates, the parity test.
- Wildlife: leaning to option 1 (faithful port); to be confirmed at dispatch of WILDLIFE-RE-1.

**Still open, and blocking something above:**
- ADR 0003 (blocks NET-0 and with it the first LAN checkpoint).
- Whether population and income are on by default in the window (PLAY-LIVE-1).
- Predators in or out of the first wildlife port (after WILDLIFE-RE-1 item 8).
- The four pending playtests: depth (W4-DEPTH-1), the opening, the HUD and construction menu (W4-UI-2), the
  settlement card.
- The rule changes of §3.2.

## 10. What this analysis did not do

- It did not run the live window; statements about what is drawn come from the code.
- The projected gate time is an estimate from two sim tests at `-O1`; render and app tests were not measured.
- The findings-by-kind counts are one reader's classification of the review files.
- Only two installs were compared; other editions and the co-developer's build were not.
- The 64-bit History Edition modules were read for names and file references only.
- No independent reviewer has checked this document or the comparison. Under the evidence contract both are
  REVIEW PENDING.

## 11. Reproducing the measurements

- **Gate times:** `~/anno-gate-logs/<worktree>/wsl-<tag>.txt` and the per-lane `.log` files in Ubuntu.
- **Release test times:** `build/msvc/Testing/Temporary/CTestCostData.txt` of the game-file run worktree.
- **Optimisation experiment:** `git archive` of `main` unpacked in WSL, three configures of `anno_sim_tests` with
  `-DANNO_SANITIZE=ON -DANNO_BUILD_VIEWER=OFF -DANNO_BUILD_GPU=OFF` and the three `CMAKE_CXX_FLAGS_DEBUG` values
  of §2.1, each test run with `-tc=` and `ANNO1503_GAME_DIR=/mnt/e/anno1503/game/app`.
- **Edition comparison:** see the comparison document, §8.
