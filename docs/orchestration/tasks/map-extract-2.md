# MAP-EXTRACT-2: the original's mine lifecycle, the tools chain and the quarry

**READY / RELEASED 2026-11-07. Owner: the first Mac executor. The Claude Code orchestrator wrote this packet at the
owner's request.** The orchestrator acceptance of MAP-EXTRACT-1 stays a separate step; this packet does not replace
it and does not wait for it.

Work through the parts in order: **A → T → Q → G → E**. Re-read this packet and your latest REPORTS entries after any
context compaction, before you continue.

## Why this packet

- **Building materials are the next gap on the critical path.** A settler house costs tools (WERKZEUG); later stages
  build with tools, bricks (ZIEGEL) and marble (MAMOR). These are the build goods in `HEAD` bytes 3–6
  (`docs/re/production.md` §2). The sim already produces wood, cloth and salt, and leather with synthetic hides.
  - It produces no tools, bricks or marble, so a town stops building once its start stock is spent.
  - This is MVP-B's critical path (`docs/plan/lan-mvp.md` §4 W6, §5).
- **Your mine work carries over.** The iron, gold and gem mines share the salt mine's class (flag word `0x3096`), its
  placement path and its deposit lookup.
- **One question from your closeout is answered.** You listed what loading a save does to a mine as open. A research
  pass answered it in the original's code, and the orchestrator checked it: see part A.

**Done means:**
- iron ore from a mine the player built becomes tools in the city store, and those tools pay for a later building;
- this runs under the original's rules, with save, reload and replay;
- the mine-lifecycle questions are answered, or each is open with a precise reason;
- the quarry is either implemented with a brick proof, or its missing rule is named.

## 0. Rules for this run

### 0.1 Branch and base

- Leave `task/MAP-EXTRACT-1-x2` at `2f32437a` as it is: it is the REVIEW PENDING candidate.
- Create `task/MAP-EXTRACT-2` from `2f32437a` in your own checkout. Record that start commit in your first REPORTS
  line.
- If the MAP-EXTRACT-1 acceptance asks for changes:
  1. make them on `task/MAP-EXTRACT-1-x2` and push;
  2. then merge that branch into `task/MAP-EXTRACT-2`.

  That merge direction is authorized. No other merge is.
- **Boundaries:**
  - no merge from or into `main`, and no rebase;
  - no other lane's branch;
  - no Windows code: `src/app`, `src/render`, the UI.
- Read main's newer documents with `git show origin/main:<path>`.

### 0.2 Identity

- **The base is 135.** The second Mac holds 136 for its route state.
- **137 is reserved for this packet's behaviour change:** part T, and part Q if it lands code.
  - It builds on 135 and does not include 136.
  - The orchestrators decide the order of the combined integration.
- **Before the first commit that changes behaviour,** that is accepted-command outcomes, saved state, the content
  digest or the PRNG draws:
  1. write the exact delta into `docs/sim/`: fields, streams, digests and draw order;
  2. allocate 137 in that commit;
  3. at 137, reject 135 recordings explicitly and keep them as history, as 135 did for 134.
- **QUESTION first** for a real draw-order change, a new saved field, or a stream change beyond the delta you wrote.
- **Part A changes no behaviour** and stays at 135.

### 0.3 Don't stop: QUESTIONs

- Don't wait for a reply. If a step needs a decision from the owner or an orchestrator, write it as a **QUESTION** in
  REPORTS: what is needed, the options, and your recommendation with its evidence.
- Then skip only the work that depends on it, and continue with the next independent step.
- Stop only at E, or when nothing independent is left.
- **Who decides what:**
  - an obvious bug, you fix;
  - a change that may alter how the game feels is the owner's default to pick;
  - the owner decision open now is A6.

### 0.4 Evidence and reviews

- Read `docs/testing/evidence-contract.md` and `docs/testing/README.md` at the start and before each part closes.
- Every claim about the original needs one of: an address you read, a parsed record, or an emulation run. Anything
  else goes into `docs/open-questions.md` marked U, never into code as fact.
- **Independent read-only reviewers:**
  - one per package (A, T1, T2, T3–T5 together, Q1, Q3), never its implementer;
  - a fresh one for the final review at E.

  Reviewers check bytes and data, not only test results.
- **Mutation controls.** For every new rule, restore one wrong implementation in a scratch copy. It must fail the
  intended assertion while its legal control stays green. Compare the hashes of the restored files.

### 0.5 Host, shells and CI

- At the start, detect the host's cores, memory and load, and size jobs from that. Last time this was 4 build jobs
  and 2 test processes.
- **No wait loops without a limit.** Your 2026-11-07 closeout found two of your own left over (PIDs 6895, 6902).
  - Give every long command a time limit and a log.
  - Read its exit code and the last lines of the log before the next step.
- **CI:** for each code head you push, read the run once it finishes, with a single wait that has a limit. Record the
  run id, the head and each job's verdict. Don't poll.
- **The V3 journal failure is main's.** `budget_f5_test.cpp` failed in run 42360315619. Don't fix it. If it recurs,
  record the run and the head; the unmerged candidate fix is `e5d0b877` on `claude/dreamy-brahmagupta-10bjax`.
- **clang-format 23.1.3:** install it into a cache folder (`pip install --target <cache>/clang-format
  clang-format==23.1.3`) and run it on every C++ file you change, so `cpp-format` is no longer NOT_RUN.

### 0.6 History Edition

- The History Edition files are on the owner's Windows PC, not on the Mac. Don't search for them again.
- **Record its status as the note does:** "NOT_RUN (architecture mismatch)".
  - HE `AnnoWelt.dll` is x86-64, dated 2020-08-05.
  - HE `AnnoWeltAddOn.dll` is 32-bit x86, dated 2019-09-27, and has no GOG file of that name.
- Comparisons with HE belong to research sessions on the Windows PC.

### 0.7 Out of scope

- **Hunting and wildlife.** They stay PARTIAL, under their own contract.
- **The AI.** The MVP has no computer players (`lan-mvp.md` M2).
- **Routes and freight.** These belong to the second Mac.
- **Windows work:** the live window, the UI and the Load adapter.
- **Main's 18 real-file tests,** repaired on `main` by W4-MAIN-2 part A.
- **Claims about a balanced economy, and mod content.**

## A. The mine's lifecycle in the original (docs and tests only; stays at 135)

Read `git show origin/main:docs/re/mine-deposit-followups.md` from current `origin/main`, the version that carries
this packet, including its "Orchestrator check" section.

**A1. Re-check the finding independently,** in GOG `AnnoWelt.dll`, read-only, with your own tools. Cite every
address you read, and mark each point "confirmed" or "differs":
- **The record.** The writer `0x1006e380` and the reader `0x1006e310` (slots `+0x80` and `+0x7c` of the mine's
  function table `0x100c8990`) handle 10 bytes: `+0x18`, the `+0x20` flag and `+0x1c`. Neither touches `+0x24`.
- **Attach** `0x1006e270` (slot `+0x28`) runs the lookup `0x1006ddf0` only for an argument of kind `0x1400`.
- **The factory** `0x10024010` calls slot `+0x28` twice: at `0x1002465f`, and at `0x100246be` after the position is
  set. Every case of its class switch reaches that tail. The loader `0x100264a0` calls the factory at `0x100265b7`.
- **The lookup** stores into `+0x24` at `0x1006dea8` on every call, an empty result included.
- **The step** `0x1006def0` calls the lookup again (`0x1006dfb4`) only when the bound deposit is spent or comes up
  short (`0x1006df9a`–`0x1006dfb2`). Your §22.0 table says this already; the note's first version overstated it.

**A2. The load order.**
- What does the caller `0x1002eadb` of the loader `0x100264a0` dispatch?
- Where are the island's ore objects (the ERZSYMBOL `OCCUR` objects) created during a load?
- Do they exist before the building records are read?

If a scenario (`.sze`) and a savegame take different paths, answer for both.

**A3. The ore object's class.**
- Find its constructor and its function table, through the factory's class switch (byte table `0x1002528c`, jump
  table `0x1002521c`) or wherever ERZSYMBOL objects are built.
- Read its slots `+0x88` (the remaining amount) and `+0x8c` (the take). Does either read the discovery mask `+0x2c`?

This settles caveat 2 of OQ-553 and Q4 of the note. The container `0x100c9b10` is not the ore object.

**A4. The two attach arguments.** Which run-time object is the kind-`0x1400` argument of the factory's second attach
call (`[edi+0x54]` at `0x100246b5`), and what is kind `0x700`? Find them through the factory's own class and the code
that writes its `+0x54`.

**A5. Compare with our sim,** with file and line for each point:
- **On resume:** what happens to a bound mine, and to an unbound one?
- **The step's re-lookup:** when the bound deposit is spent or short, does `mine_step` run `find_ore_in_reach` again,
  switch to another deposit, or lose its deposit when the search finds none?
  - For salt the effect is invisible: a salt mine is deep, so it never depletes.
  - For a shallow iron mine in the depleting setting, it decides production. Part T builds on this answer.
- **"Never searches again":** does that still hold across a save and reload?

**A6. QUESTION for the owner: rebinding at load.** Give two options:
- **(A)** run the lookup again at construction on load, as the original does;
- **(B)** keep the saved deposit, as a documented deviation.

For each option, state:
- the effect on 135 and 137, the save stream (would the deposit index still need saving?), replay and the existing
  pins;
- which case the player can see differ: an unbound mine with ore in reach at load; a bound mine whose ore now has a
  larger neighbour.

Then your recommendation, with evidence.
- Write no code for this before the answer.
- If the answer comes in during part T, implement it within 137.

**A7. Fold the results into OQ-552/553/554 and §22,** citing the note and your own addresses.
- Mark the AI question **parked**, because the AI is out of the MVP.
- For whoever resumes it, name the starting point from the note: FuncB `0x10030230`, with its callers `0x10010f2d`,
  `0x10030472` and `0x100336e9`.

**A8. REPORTS entry for A,** with a "Decisions and reasoning" list. Push.

## T. Tools: iron ore to iron to tools (identity 137)

The orchestrator parsed these from the `WARE` and `ROHSTOFF` records of `Anlagen.dat` on 2026-11-07. That covers only
the first input and output of each type. Derive every input, rate and class again yourself.

| Types | PRODTYPE | Input → output |
|---|---|---|
| 1205, 1210, 1216, 1217 | `MINE` (flag word `0x3096`) | → EISENERZ |
| 1035 | `HANDWERK` | HOLZ → HOLZKOHLE |
| 1017, 1034 | `HANDWERK` | EISENERZ → EISEN (check for a second input, such as HOLZKOHLE) |
| 1019 | `HANDWERK` | EISEN → WERKZEUG |
| 3206 | `STEINMETZ` | STEINE → EISEN (unexplained: identify it) |

**Other consumers of these goods,** which matter for ledger scoping:
- 1037 and 3214 (EISEN → SCHWERT);
- 1038 `KANONENBAU` (HOLZKOHLE → KANONE);
- every `BuildCmd` that spends WERKZEUG.

**Known facts:**
- **Deep or shallow** (`+0x21`, set in attach `0x1006e270`): an EISENERZ mine is deep when its type carries research
  `0x39` (TIEFE_ERZMINE: 1216 and 1217, your §1). Every other mine is deep (`docs/re/production.md` §4.4).
- **Deposits:** 2,000 t by default, and they deplete only with the add-on edition setting. The reserve flag is open:
  OQ-035 b and c.
- **An exhausted iron mine** raises notice `0x15b` or `0x19` (`docs/sim/production.md`).

**T0. Inventory and acceptance table,** as in X0:
- **For every type above:** class, inputs, outputs, rates, capacities, carriers and costs.
- **Its stage and research.** The construction-eligibility check (`d2be23aa`) refuses stage-locked types; name the
  stage each type needs.
- **Which sim ticks exist** for these types, and what is missing.
- **The iron ore objects** (the figure type with WARE EISENERZ) on every shipped island.
- **One pilot scenario:**
  - an island with iron ore in reach of a legal mountain site;
  - consumers it can reach;
  - the stage it needs, or the developer path that reaches it.

  Disclose its imported buildings and stock, and keep preloaded EISEN and WERKZEUG apart.
- **The acceptance table:** claim, evidence, positive control, the wrong interpretation it tells apart, expected
  result, reviewer.

**T1. The contract, with hand-derived vectors, before C++.** It covers:
- **Iron deposits:**
  - the amount: does it come from the `OCCUR` u16 (400 or 2,000 t in shipped files)?
  - who clears the reserve flag (OQ-035 b, c): read it, don't guess;
  - deep against shallow, and depletion under each edition setting.
- **The step on a spent or short deposit,** and on exhaustion: rebinding to other ore in reach, or idling with the
  notice.
- **The smelter's inputs and rates,** and the charcoal burner's wood use.
- **How WERKZEUG reaches the city store.**

Everything comes per type, from the data, with no list of ids. An independent review follows, then corrections and a
re-review.

**T2. Implementation:**
- **Mine placement.** Extend the 135 mine path to every class-`0x3000` MINE type whose deposit evidence holds: iron.
  Include gold and gems only if T1 covers them; otherwise give them a typed refusal.
- **The step's re-lookup** must match the original (A5).
- **Processors** go through the existing workshop classes. Add only seams with evidence.
- **Small tests,** each refusal with a legal control beside it:
  - legal and illegal sites;
  - deep and shallow;
  - both edition settings;
  - exhaustion: rebinding, or idle;
  - a rebuild that does not refill;
  - a stage-locked refusal next to an eligible control;
  - unaffordable;
  - a dry run equal to the live apply.

**T3. The real chain on the pilot:**
- **Construction:** built by public commands at the original's costs.
- **The goods flow:** new iron ore → (charcoal) → EISEN → WERKZEUG into the city store. A later `BuildCmd` of a building
  that costs tools then spends them.
- **Ledgers scoped to the owner and city:** source, raw, in flight, store and spent, with no unexplained remainder.
- **Controls:**
  - the source disabled (no mine): the store does not rise;
  - the smelter removed;
  - no charcoal: the smelter stalls;
  - the preloaded stock kept apart.

**T4. Interruption and lifetime:**
- **Cases:**
  - a shallow deposit exhausted in the depleting setting: rebinding to a second ore, or idle with the notice;
  - full stores;
  - missing charcoal;
  - a producer or carrier removed while goods are in flight;
  - a rebuild.
- **What must hold:**
  - every debit and credit happens exactly once, and claims are released;
  - the state is valid right after the interruption, not only after a later cleanup tick;
  - timed transitions are checked at their edges, as in X4.

**T5. Continuation, replay and cost:**
- **Reloads** at extraction, smelting and tool delivery, and with a mine whose deposit is spent. Each must equal the
  uninterrupted run.
- **Fresh-process replay** of a real recording, and of a changed command that diverges.
- **Cost:** bounded numbers on a modest multi-city fixture.

**T6. The package gate and its reviews:**
- canonical Debug, and ASan on the affected tests;
- the required contracts, and `picture_matrix_contract`;
- clang-format.

## Q. The quarry: stone, bricks and marble (research first)

**Parsed:**
- **Quarries:** `STEINBRUCH` 1201 and 1208 produce STEINE; 1218 and 1220 produce MAMORSTEIN.
- **Stonemasons:** `STEINMETZ` 804 makes STEINE into ZIEGEL; 803 makes MAMORSTEIN into MAMOR.
- **Status:** `RuntimeClass::Quarry` exists, but quarries are Unsupported (OQ-554).
- **A lead:** your §2 lists ore objects for EISENERZ, GOLD, SALZSTEIN, EDELSTEINE and MAMORSTEIN, but none for STEINE.
  A stone quarry may therefore need no ore object.

**Q0. Inventory,** as in T0. Include:
- which shipped islands carry MAMORSTEIN ore objects;
- how the quarries' HIGHMAP tables differ from the mines'.

**Q1. Research (docs only).**
- Find the quarry's constructor and function table through the factory's class switch, and its step function.
- Find its production source: an ore object, the terrain, or the caller of "deposit slot `+0x80`"?
- Read its depletion and its binding.
- Record the cursor checks as UI-only:
  - `0x1008b950`: terrain and height only, one pass;
  - `AnnoFrame 0x10044f90`: unidentified.

**Q2. Contract and vectors,** with an independent review.

**Q3. Implementation, only if Q2 closes.**
- Follow T2–T5: STEINE → ZIEGEL into the store, spent by a build that costs bricks; marble too, if the same rule
  covers it. This stays within 137.
- Otherwise, a precise partial result that names the missing rule.

## G. Gold and gems (inventory only)

- **Mines:** 1206 and 1212 (GOLD); 1214 (EDELSTEINE).
- **Goldsmiths:** 1039 and 3410 (GOLD → SCHMUCK); 3010 (EDELSTEINE → SCHMUCK).
- **Stall:** 1313.

Write the inventory and the acceptance table, and name which population groups need SCHMUCK. Implement nothing.
- **Exception:** if T1's contract already covers them at no extra cost, add tests only.

## E. Final gate, review and stop

- **One fresh final reviewer:** the combined path, the ledgers, the identity, the provenance and every claim.
- **Frozen source, then:**
  - canonical Debug and affected ASan, with the original files and the exact selected inventory;
  - the required contracts by exact name: the salt fresh-process tests and the new iron ones;
  - `picture_matrix_contract`;
  - clang-format 23.1.3;
  - the GCC 13.3 `-Werror` compile of the touched files in your arm64 container, if it is available.
- **Keep:** failed first attempts, the actual skips, and NOT_RUN hosts.
  - MSVC and Windows with the game files belong to the Windows PC.
  - The owner rule's game-file run happens after a consolidation into `main`, not on your branch.
- **The REPORTS entry:**
  - a verdict per part;
  - a "Decisions and reasoning" list;
  - every QUESTION, with your recommendation;
  - evidence hashes;
  - the consumer delta for Windows: read-only facts such as `mine_facts` for iron, deep or shallow, and the exhausted
    state.
- **Then:** push, read CI once, and stop with a clean tree and no jobs running.

## If something blocks

- If T is blocked on a decision, continue with Q's research.
- If Q's contract does not close, finish T and G, and stop at E with a precise partial result.
