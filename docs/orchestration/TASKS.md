# Executor task board (read this first, every time)

**Who writes this file:** the orchestrator session (Codex, by owner instruction). **Who reads it:** the executor sessions. The executor never edits this file; it writes to [`REPORTS.md`](REPORTS.md). The orchestrator updates this file after every review, so re-read it at the start of every task and whenever a task ends.

**Owner rules, 2026-11-06 (a co-developer joins; every lane):**
- **Push your task branch.** Every executor pushes its own task branch to `origin` at each checkpoint and with each
  report.
  - Use an ordinary push. Never force, and never push `main`.
  - For the executor's own task branch, this replaces "no push" in the Mac dispatches. The rules on merges, rebases and
    other lanes' branches are unchanged.
- **The GitLab mirror.** The co-developer works from the private GitLab mirror (`gitlab.com/fuitable/open1503`). The
  Claude Code orchestrator on the Windows PC mirrors every GitHub branch there with `tools/orch/sync_gitlab.py` after
  each push to `main` (and after task-branch checkpoints). Use ordinary pushes only: no force and no deletion.
  The owner or that orchestrator runs the sync after this orchestrator publishes main. A host with GitLab SSH
  access through remote `gitlab` may also run `python -I tools/orch/sync_gitlab.py`.
- **GitHub is public.** `Dom-cmyk727/anno_rebuild` remains the dispatch and CI channel. Visibility is the owner's
  separate decision. Publish only the normal project flow: no original files, decoded assets, listings or
  credentials. A private GitLab mirror does not make publication on GitHub private.
- **The status page.** [`STATUS.md`](STATUS.md) says who works on what, where the work is, and what it waits on. Every
  orchestrator updates it in the same commit as each dispatch, acceptance or change of lane.

**Owner rule, 2026-11-07 (every orchestrator): main is healthy only once its game-file tests have run on the Windows
PC.**
- **Why.** Hosted CI has no original game files and skips every test that needs them, so ten green CI jobs say nothing
  about those tests. On 2026-11-05, 18 such tests broke on `main` unseen. The
  [W4-SETTLEMENT-1 review](reviews/2026-11-07-w4-settlement-1-w7.md) has the details.
- **What must run.** After every push that changes code or tests on `main`, run the whole MSVC suite on the owner's
  Windows PC, at that exact commit and in a clean checkout of it:
  - `ctest --preset msvc` with `ANNO1503_GAME_DIR` set to the original files;
  - the GPU tests on the RTX 4090 included.

  Name every failure with its test name and assertion. Attribute each to a commit. Give each one an owner and a repair.
- **Until then,** that commit's status is "CI green, game-file run pending". No acceptance, review or STATUS line may
  call that `main` healthy, green or accepted without qualification. A Mac run with the game files counts for its own
  lane, but it doesn't replace this run.
- **Who runs it.** The orchestrator that pushes to `main` adds "game-file run at `<commit>`" to STATUS's waiting list.
  The Claude Code orchestrator runs it on the Windows PC, or dispatches it to the Windows executor. It records the
  commit, the counts and every failure in the package's acceptance review, or in its own review note. Docs-only pushes
  need no run.
- **Next consolidation.** A new consolidation on `main` doesn't start while the previous code commit's game-file run is
  still pending, unless the owner says so. A run with known, named failures that already have an assigned repair does
  not block it.

**Owner rule, 2026-11-10 (every orchestrator): `main` is protected on GitHub.**
- **What GitHub enforces.** A push to `main` is accepted only for a commit on which a check named `gate` from
  GitHub Actions already passed. No force push, no deletion. A pull request must be up to date with `main` before it
  can be merged. The rule binds admins too, so it binds every session that pushes as the owner's account.
- **What `gate` means.** Every CI job passed. For a change that touches only `docs/` and root `*.md` files, the build
  jobs are skipped and `gate` passes in about a minute. Any other change, compared as a whole against `main`, runs
  the full matrix, about 30 minutes; so does a path Windows cannot check out.
- **How to land: only with `land.py`, never with GitHub's merge button.** Commit on top of `origin/main`, then
  `python -I tools/orch/land.py --rev <commit> --name <topic>`. It pushes the commit to `land/<topic>`, waits for
  `gate`, pushes `main`, deletes the landing branch and runs `sync_gitlab.py`. GitHub's own check is weaker than
  `land.py`'s: `land.py` counts only a `gate` that `ci.yml` ran for a push of that exact commit.
- **A change to `.github/` needs `--allow-ci-change`.** The commit being checked defines its own `gate`, so first
  review its workflow changes for anything that weakens the gate (a job left out of `gate`, `continue-on-error`, a
  step that writes check runs).
- **A failed `gate`** stops `land.py` with the run's link. Re-run the failed jobs (`gh run rerun <id> --failed`) and
  run `land.py` again: it waits for the re-run. Or fix and land the new commit.
- **What it does not change.** Executors still never push `main`. The game-file run after a code push (rule above)
  is still required: hosted CI has no game files, so `gate` cannot replace it. Only the owner can lift the protection
  (GitHub settings, Branches).

**Owner rule, 2026-11-12 (every packet): small packets, and the orchestrator's review is the executor's review.**
- **Why.** MAP-INTEGRATE-2 took 13 h 20 min, mostly a whole-suite gate matrix and nine reviewer runs inside the
  packet, for about 3 hours of work that carried the value
  ([review](reviews/2026-11-12-map-integrate-2-review.md) §3).
- **Size.** One concern per packet, about 2–4 hours, a packet text of about one page. Past about 6 hours, stop at a
  clean point and report what is done and what is left.
- **Reviews.** The executor starts no reviewer subagents, per part or at the end, unless its packet asks for one. The
  orchestrator's review at handover is the independent review
  ([evidence contract](../testing/evidence-contract.md)). Report READY FOR REVIEW, never "approved".
- **Gates.** The build, the tests for what changed (canonical runner, classified counts) and hosted CI green on the
  pushed head. Read CI after every push and fix red before going on. The whole suite and the game-file run belong to
  the orchestrator at the merge, unless the packet says otherwise.
- **Report.** One REPORTS entry of about 60 lines at most: what changed, the gate commands with classified counts,
  NOT_RUN, Decisions and reasoning, QUESTIONs. Logs stay in `cache/`; cite the gate log, not every log.
- **State numbers.** No packet allocates a new state number until ADR 0006 (a version per save section) is accepted.
  A packet that would need one stops at that point with a QUESTION. (ADR 0005 is the edition decision.)
- **One at a time.** At most one open packet per executor. The orchestrator dispatches the next packet after
  reviewing the last one, and only with the owner's OK.

## What moved out of this board (2026-11-10)

The board's closed history moved verbatim, nothing edited, to [`HISTORY-2.md`](HISTORY-2.md), and the older
executor reports to [`REPORTS-1.md`](REPORTS-1.md), so an agent can read the live board in one pass (owner request).
Rounds up to ct were archived the same way in [`HISTORY.md`](HISTORY.md) on 2026-10-28; where §1 points to
`HISTORY.md`, look in `HISTORY-2.md` too.
- **Current work.** [`STATUS.md`](STATUS.md) says who works on what. New dispatches are packet files in
  [`tasks/`](tasks/). [MAP-INTEGRATE-2](tasks/map-integrate-2.md) is accepted
  ([review](reviews/2026-11-12-map-integrate-2-review.md)) and on `main` as state 138. Now: [GATE-1a](tasks/gate-1a.md)
  (Windows). [WILDLIFE-RE-1a](tasks/wildlife-re-1a.md) and [WILDLIFE-RE-1b](tasks/wildlife-re-1b.md) are accepted; [WILDLIFE-RE-1c](tasks/wildlife-re-1c.md) (Mac) is released.
  - **The queue, proposed 2026-11-12** by the orchestrator from the
    [project analysis](reviews/2026-11-12-project-analysis.md) (§2.2, §4.2, §6, §7, §8) and the
    [MAP-INTEGRATE-2 review](reviews/2026-11-12-map-integrate-2-review.md) §4. Every packet is released only with
    the owner's OK, one open packet per executor, under the owner rule of 2026-11-12 above. The analysis's larger
    packets are cut to that size here.

    | Step | Who | Packet | Size |
    |---|---|---|---|
    | 0 | Orchestrator | Land MAP-INTEGRATE-2 with the analysis documents: **done**, `fa9d8bbf` | — |
    | 1 | Windows executor | **[GATE-1a](tasks/gate-1a.md), RELEASED 2026-11-12:** the WSL sanitizer gate and the `asan` preset at `-O1 -g` (analysis §2.2 step 1, D1) | ~2–3 h |
    | 2 | Windows executor | **GATE-1b:** a required contract runs once; `picture_matrix_contract` in one lane; no local lavapipe lane by default (steps 2, 4; D2) | ~2 h |
    | 3 | Windows executor | **GATE-1c:** a gate can end green: the flow-build test's cause, then its fix; named-adapter GPU tests skip as platform (step 3; D3, D4) | ~2–3 h |
    | 4 | Windows executor | **PLAY-LIVE-1a:** a "living town" rules profile for `--play` and `--ship-start` behind one flag (D7); the default is the owner's | ~2–3 h |
    | 5 | Windows executor | **PLAY-LIVE-1b:** draw the tile objects that change, roads first, after confirming D8 in the window | ~3 h |
    | 6 | Windows executor | **EDITION-1a–e:** the game-source layer and its readers; the rule-set digest; the island manifest; figure type 375; `content-id` and the parity test (analysis §6, one packet each) | ~2–4 h each |
    | 1′ | Mac executor | **[WILDLIFE-RE-1a](tasks/wildlife-re-1a.md), ACCEPTED 2026-11-12** ([review](reviews/2026-11-12-wildlife-re-1a-review.md)): the fauna sweep, the creation helper, the `HIRSCH` loader (analysis §7.3 items 1–3). Documents only. The owner confirmed option 1, the faithful port | ~3–4 h |
    | 2′ | Mac executor | **[WILDLIFE-RE-1b](tasks/wildlife-re-1b.md), ACCEPTED 2026-11-12** ([review](reviews/2026-11-12-wildlife-re-1b-review.md)): the animals' own life: their classes' overridden slots, construction and attach draws (OQ-575), the spawn record's release (OQ-573), their routes and jobs (items 4–5) | ~3–4 h |
    | 3′ | Mac executor | **[WILDLIFE-RE-1c](tasks/wildlife-re-1c.md), RELEASED 2026-11-12:** the hunter's approach, the fur trapper, the wolf and predators (items 6–8) | ~3–4 h |
    | 4′ | Mac executor | **WILDLIFE-RE-1d:** `tools/wildlife_ref.py` with hand-derived vectors from 1a–1c's draw tables | ~2–3 h |
    | — | Orchestrator, beside | **ADR 0006:** a version per save section, lighter goldens, one tool that regenerates pins (MAP-INTEGRATE-2 review E1; analysis §3.2 item 5). The owner approves | — |
    | Later | — | **COORD-F3-1** (D5, the `F3 (D1, deaf)` flake, cause first) and **TEST-MEM-1** (D6, reservations from measured peaks; seven low-memory refusals in the 2026-11-12 run) | ~2–3 h each |
    | Later | — | **PLAY-LIVE-1c** (draw figures), **NET-0** (after the owner decides ADR 0003), **TEST-FIXTURE-1** | — |
    | After ADR 0006 | — | **EDITION-2 with WILDLIFE-1** (one identity change for both), then freight through the effect queue (E2–E4) | — |
    | Parked | — | [SIM-LIMITS-1](tasks/sim-limits-1.md) (packet written), FIXTURES-TRIM-1, MAP-MATERIALS-1, MAP-FOOD-1, MAP-GOODS-1a–c: re-ranked after the wildlife port re-measures [`progression-gaps.md`](../sim/progression-gaps.md) | — |

    The orchestrator writes each packet when it is next, after reviewing the last one.
  - **The three Mac dispatches below are finished** and landed through MAP-INTEGRATE-2. Their text stays here as
    their record.
- **What stays here:**
  - the owner rules above;
  - three dispatches that exist only on this board, finished since 2026-11-11 and taken over by MAP-INTEGRATE-2:
    MAP-ROUTE-1 for the second Mac; the compatibility decision on founding accounts and salt sequencing, 134/135;
    MAP-NEEDS-CLOSE-EXTRACT-1 for the first Mac;
  - §1, the protocol for the executor;
  - §4.

  Where they say "accepted above", that section is now in `HISTORY-2.md`.
- **What moved:** MAIN-CI-1 (closed); MAP-EXPAND-1 and the state-134 prerequisite (accepted); ECON-BOOK-1 and its
  baseline (accepted); the main consolidation of 2026-11-05; "Other lane dispatches and completed integration
  history", which holds the release notes of 2026-11-07 (W4-UI-2, MAP-EXTRACT-2, W4-MAIN-2), the MAP-SCRIPT-2 packet,
  the Windows lane's W4-WIN, W4-OPENING and W4-DEPTH dispatches, the MAP-POP, ROAD, CARGO, NAV, BUILD and FOUND
  packets, and the round blocks bp and cp to dw; §2, the status of 2026-10-29 (`STATUS.md` replaces it); §3, the task
  queue (the T-series and the LAN endless lane, 2026-10-20 to 10-29).
- **Still open among the moved dispatches,** as `STATUS.md` records them: W4-DEPTH-1-CLOSE (waits on the owner's
  depth check), W4-SHORE-2 (queued, not released), W4-MAIN-2 (part D changes requested;
  [packet](tasks/w4-main-2.md)), MAP-EXTRACT-2 ([packet](tasks/map-extract-2.md); finished, an input of
  MAP-INTEGRATE-2 since 2026-11-11) and W4-OPENING-2 (STATUS lists
  `task/W4-opening-2`; its adoption `task/W4-opening-2-main` is already in `main`). Every moved heading keeps its
  last recorded status: one marked READY or CURRENT there is not cancelled, and may be stale.
- **Owner decisions recorded in the moved text.** They stay in force unless a later rule replaces them; the full text
  is in `HISTORY-2.md`:
  - 2026-10-26, round bp: subagents are allowed again where necessary and helpful (§1 item 5).
  - 2026-10-28, round cp: memory-aware parallel testing replaces one heavy job at a time.
  - 2026-10-29, round dk and the LAN MVP note in §3: the LAN MVP is one fixed map for five players, no computer
    players (round dk's other decision, a single orchestrator, was replaced later). Round dl: the map is
    original-like and fair by measure, not rotated copies.
  - 2026-10-30, round dr: generous developer test resources (a developer option: default on for the developer start in
    `--play`, off for original scenarios, values configurable); the owner tests building and demolishing through the
    original's in-game UI.
  - 2026-11-02: implement ship unloading after placement (MAP-CARGO-1); wire ship selection, sailing, founding and
    unloading into the live window for the owner's own test (W4-OPENING-1).
  - 2026-11-03: the headless harvesting, wood production and warehouse delivery loop is authorized; the owner's
    report of the floating Kontor and road, the origin of W4-DEPTH-1.
  - 2026-11-04: the Windows sequence, "Yes and merge", with `main` as its own package (W4-MAIN-1). The water level:
    `kShoreHeight = 26.5` is W4-DEPTH-1's default ("Keep 26.5 in package"); the `--shore 16.5` picture stays as its
    own pinned pass. It lives on unmerged `task/W4-depth-1`; `main`'s code and docs stay at 16.5 until that merges.
  - 2026-11-06: the WSL disk rules (W4-OPENING-CONTROLS-FLAKE step 0): Ubuntu's image lives on `E:`; gate builds go
    under `/mnt/e` or into one reused folder, never one per worktree, and the script deletes its own build tree.
  - C5 (MAP-FOUND-3, reviewed 2026-11-01): five ship-start players begin at peace, with shared visibility and without
    alliances.
  - 2026-11-07: the Windows lane repairs main's 18 real-file test failures (W4-MAIN-2 part A).
- **[`REPORTS.md`](REPORTS.md)** keeps its entries from MAP-CARGO-1 (2026-11-02) on; executors still append at the
  bottom. `tools/orch/packet.py` reads only this file, so it cannot quote a moved section.

<!-- BEGIN MAP-ROUTE-1 -->
## Second Mac — MAP-ROUTE-1: automatic freight between owned warehouses (2026-11-06)

**FINISHED, 2026-11-11 note.** R0–R7 reached a final local candidate. Its CI-green head
`claude/ci-fix-map-route-1` `49ffcee6` is an input of [MAP-INTEGRATE-2](tasks/map-integrate-2.md), which makes the
combined identity decision this dispatch asks for: **state 138**.

**RELEASED: R0–R7, one comprehensive headless gameplay bundle.** This is the
second Mac executor's current assignment and supersedes the completed expansion
packet's stop/await-next-task instruction. Independent package approvals release
the next package automatically. Return once at the final candidate or a genuine
out-of-scope dependency; do not seek routine permission between R0–R7.

### Player outcome and boundaries

The player assigns an owned ship an ordered route between owned warehouses.
The simulation sails, arrives, loads/unloads and repeats without a test driver
issuing a Move/Load/Unload for every stop. The player can suspend/cancel it and
regain manual control. Newly produced wood must reach the second settlement over
at least two complete automatic cycles, with exact ledgers and save/replay.
This extends the accepted manual expansion; it does not claim a balanced town.

First Mac owns MAP-EXTRACT-1, salt/deposit placement and reserved state135;
hunting/fauna stays outside both this package and this lane. Windows owns viewer,
UI, rendering and its active W7 verification. Do not edit their worktrees, import
their unaccepted changes or interrupt their jobs. No foreign trade, prices,
diplomacy, combat, fleet/group routing, collision avoidance, original savegame
route import, full route editor or general scripting automation is released.
The first slice supports independently assigned ships and variable-length stop
lists; two stops is the canary, not an engine-wide maximum.

### R0 — Adopt the accepted base and define the proof

1. Preserve `task/MAP-EXPAND-1-main` at published `a4617184` and all its evidence.
   In the existing owned checkout, with a clean tree and no owned jobs, fetch
   origin, verify accepted main **`e0bb6be7bd26dddd53d2f7e69ae29651fc5c49b9`**, and
   create `task/MAP-ROUTE-1-main` from that exact commit. Read this new dispatch
   from fetched `origin/main` without merging a moving main. No rebase, reset,
   pruning or cross-lane cherry-pick. Record task-board blob/published revision.
2. Read the files below in order. Inventory current navigation, arrival/order
   identity, cargo, command ingress, serialization and ownership seams. Verify
   original inputs/hashes; do not start by re-running the completed long voyage.
3. Inspect actual CPU/RAM/free disk and concurrent load; retain memory admission.
   Use owned build/cache directories and measured concurrency, one expensive
   campaign at a time. Keep the display awake only with an owned assertion if
   needed; never stop the owner's assertion or another lane's processes.
4. Write a requirement/positive/control/evidence table and a small dependency
   graph in `docs/sim/ship-trade-routes.md`. Record the first cheap gate baseline,
   intended test inventory, long-canary work bounds and current compatibility.

Read first:
- `AGENTS.md`, `CLAUDE.md`, `docs/PLAN.md`, this entire marked TASKS section,
  `docs/orchestration/STATUS.md`, `docs/testing/evidence-contract.md` and
  `docs/testing/README.md`.
- `docs/orchestration/reviews/2026-11-06-map-expand1-acceptance.md`,
  `docs/orchestration/reviews/2026-11-06-map-expand1.md`,
  `docs/sim/manual-expansion.md`, `docs/testing/manual-expansion.md` and the
  final MAP-EXPAND entries in `docs/orchestration/REPORTS.md`. Earlier OPEN rows
  in historical expansion tables are not the final acceptance verdict.
- `docs/re/ship-cargo.md` §§7,11 (especially automatic-stop `0x10055bb0` and
  shared transfer calls `0x10055d8a`/`0x10055ee3`), `docs/sim/ship-cargo.md`,
  `docs/re/ship-navigation.md`, `docs/sim/ship-navigation.md`,
  `docs/sim/ship-route-engine.md`, `docs/sim/ship-route-limits.md`.
- `docs/sim/state.md`, `docs/sim/economy-booking.md`,
  `docs/sim/build-availability.md`, `docs/sim/shared-visibility.md`.
- `src/sim/include/anno/sim/{command,ship_orders,ship_cargo,world,state_fields}.hpp`;
  `src/sim/src/{apply,codec,ship_orders,ship_cargo,ship_mover,ship_step,driver,
  scheduler,save,invariants,replay}.cpp`. Follow actual callers; these lists
  identify starting files, not permission to refactor all of them.
- `src/sim/tests/{ship_orders,ship_nav_state,load,unload,founding_assets,
  command_codec,replay,integration_stream}_test.cpp`,
  `src/sim/tests/cargo_transfer_oracle.hpp`, `src/sim/CMakeLists.txt`,
  `tools/cargo_ref.py`, `tools/sim_stream.py`,
  `tools/integration_stream_ref.py`, `src/tools/sim_replay/main.cpp`.

### R1 — Original route contract and independent reference, before runtime edits

1. Trace the original automatic-stop caller and record construction, not only
   the already traced manual cargo arm. Resolve stop fields/units, target-city
   resolution, action order, hold selection, quantity meaning (transfer amount
   versus desired stock), partial/full/empty behavior, stop advancement and
   route wrap. Identify when arrival is considered complete and what happens
   on manual interruption, invalid destination or route failure. Follow exact
   function boundaries and vtable targets; an address in an old note is a lead.
2. Record module hashes, addresses and decisive predicates in the new
   `docs/re/ship-trade-routes.md`. Keep original bytes, dumps and disassembly in
   ignored cache only. Separate established original behavior, explicit modern
   adaptations and unresolved behavior. No claim of runtime parity without an
   original-runtime observation.
3. Specify a typed route definition, assignment/control commands, authenticated
   issuer checks, ordered stops/actions, current leg/action progress, errors and
   public observations. Distinguish command acceptance, sailing arrival,
   transfer success and completion of a cycle. Define same-tick ordering with
   manual commands and prevent stale arrivals from advancing a replacement route.
4. Authorize these bounded robustness adaptations explicitly: invalid/missing/
   foreign destinations must never transfer or silently retarget; report a
   recoverable stopped/suspended state. Authority is the authenticated ship
   owner, never shared visibility. No busy loop when no progress can be made.
   Reuse accepted safe cargo arithmetic and conservation. The exact empty/full
   wait/skip policy and partial-transfer advancement must be traced and reviewed;
   do not invent a wait-until-full rule because it makes the canary pass.
5. Write a small independent transition/transfer reference and hand-derived
   synthetic vectors before C++. Pin legal progress plus competing wrong
   interpretations: request versus target amount, rounded stock, action order,
   zero versus partial transfer, wrap, duplicate notification, stale generation,
   stop/reassign, zero/small/split elapsed time, and surplus interval handling.
   Reference inputs must not contain answers it merely echoes. CLI contracts
   reject zero executed vectors, malformed/truncated input and bad expectations.
6. An independent read-only reviewer re-derives the decisive original predicates
   from actual bytes and reviews the contract/reference. Approval releases R2.
   Unsupported nonessential original arms stay explicitly excluded. An unread
   essential route predicate is remaining research, not permission to guess.
   If evidence truly cannot settle it, stop dependent engine work with the exact
   question, trace attempted and smallest owner decision needed; finish other
   independent authorized artifacts rather than calling the full packet complete.

### Identity and integration boundary (record during R1)

This branch starts from canonical **134 / ECONBK1 / PRNG5 / replay4 / command1 /
 native wrapper1**. **State136 is reserved for this route-state package; 135 stays
owned by first Mac salt.** Reservation is not main acceptance and a branch based
on134 does not contain salt merely because its number is136. Document that fact
on every compatibility/handoff record. Do not adopt135 or merge the salt branch.
Orchestrator owns the later combined integration and its final identity. That
combination must not simply take max(135,136) or amend published route-only136;
it requires a fresh compatibility decision.

Before runtime changes, review the entire saved route schema, command tags,
field widths/order, rules/digest coverage and compatibility plan. Preserve command
variant order and allocate unused tags after checking the frozen source. Retain
PRNG5: route scheduling introduces no new random draws. Existing navigation draws
and their ordering must be accounted for; an unavoidable timeline change is a
real coordination stop before implementing it. Do not silently amend a published
schema or reuse another lane's identity; seek a revised reservation if the
reviewed published layout must change. Keep the existing wire/framing versions
only if the reviewed compatibility checks actually reject incompatible peers.

New branch-native136 states/replays reject earlier134 input explicitly; no
migration or relabelling. Preserve old artifacts. Do not claim a tested135
compatibility check without a real valid artifact and the intended refusal.
Update explicit serialization, native validation, independent stream reference
and synthetic fixtures together. Baseline semantic equivalence without routes
must be checked excluding intentional identity bytes. Explain each changed
current golden; do not replace unrelated history or count C++ self-output as an
independent oracle. All existing gameplay defaults remain unchanged; worlds with
no assigned route must retain their existing behavior and draw order.

### R2 — Route data, commands and persistence

1. Implement the reviewed per-ship route state and public assign/control/query
   surfaces. Use stable typed ship/warehouse references and action indices,
   never persistent pointers. Validate lengths, arithmetic and references before
   writing. Failed commands leave serialized state, money, goods and draws intact.
2. Serialize every future-affecting cursor, pending identity and control state;
   validate impossible cursors, corrupt references and malformed command input.
   Hand-write nonempty expected bytes independently of the encoder. Test codec
   roundtrips plus independent bytes, current native load and real old refusal.
3. Keep content roles/goods/warehouse eligibility data-driven. No island, ship,
   warehouse or goods allowlist from the real canary. Test a non-original goods
   id/eligible warehouse fixture and more than two stops. Operator caps for tests
   are not engine limits; public input/work budgets must have a documented reason
   and a configuration path. No general mod framework is required.
4. Independent reviewer checks schema/identity, authority, public callers and
   refusal atomicity. Fix/re-review before scheduler integration.

### R3 — Integrate automatic sailing and cargo execution

1. Connect the route state machine to production simulation visits. Route code
   must reuse existing movement and cargo validation/commit primitives; do not
   implement a second stock arithmetic path. A narrow shared internal seam is
   allowed with parity controls for existing manual commands and failure order.
2. Wait for the matching ship/order/route revision's arrival before executing
   actions. Re-resolve warehouse/owner/range at the action, even if valid when
   assigned. Original's omitted range check is not blanket authorization for
   distant cargo. Define a checked arrival-to-transfer invariant and test it.
3. Preserve declared action order, exact partial quantities and one-time effects.
   Persist the action cursor before any subsequent visit can repeat a committed
   debit. Rejected/zero transfers do not masquerade as delivered goods or completed
   productive cycles. Apply the reviewed shortage/full/route-failed policy.
4. Bound work per visit with deterministic continuation. A zero-length leg,
   colocated stops or all-refused actions must not spin through cycles forever.
   Route work should scale with active routes/due actions rather than all
   buildings/figures. Keep integer arithmetic and deterministic ordering; no
   background thread, wall-clock timer or unrecorded external controller.
5. Test real scheduler calls with public assignment commands, same-tick command
   order, smallest supported visit and split delay. Manual Move/Stop and cargo
   interactions must follow R1's explicit ownership-of-control policy.
   Independent review approves actual integration, not just a pure model.

### R4 — Interruptions, recovery and adversarial controls

1. Cover suspend/resume/cancel/reassign while sailing, at arrival, between actions
   and after a partial transfer. No replayed or stale completion can transfer twice
   or advance the replacement route. Remove the ship and remove/rebuild a target;
   stable identity must not accidentally authorize the replacement warehouse.
2. Cover no route, malformed route, unreachable dock, source shortage, destination
   full, hold full/mismatch, wrong owner, closer rival warehouse, finite capacity,
   fractional residual and multiple actions for the same goods. Recovery uses
   public commands and the reviewed policy; no internal flag/stock repair.
3. Exercise two ships and two owners on short synthetic maps. Deterministic
   shared-stock contention follows declared simulation order, never thread timing;
   conservation must hold for each city, hold, goods and owner after every action.
4. Use a small set of precise faults: arrival bypass, wrong warehouse, credit-only,
   duplicate action, stale revision and lost cursor on reload. Each fault must
   fail its intended assertion after the legal control runs. Retain failure
   extracts, then rebuild clean binaries before any final gate. Do not multiply
   long voyage mutants or start a generic mutation infrastructure project.
5. Independent review includes alternate orderings, end/removal cleanup and
   whether a test could pass on the wrong refusal or an unrelated crash.

### R5 — Real production-backed automatic freight

1. Reuse accepted MAP-EXPAND's unchanged manifest, original loadout, source/dock
   sites, public founding/forester/road setup and receiving construction recipe.
   Booking is enabled explicitly for the canary; house updates stay off. Both
   settlements may be founded manually as setup. Label this honestly: automation
   begins on assignment, not on initial founding. No grant, stock edit, relocation,
   new permission, candidate installation or invented source resource.
2. Establish a production/stock cut that distinguishes new freight from inherited
   cargo. Arrange the canary only through legal public commands, and independently
   derive the bill/hold allocation first. After assignment, the harness issues
   Advance and observations only during the uninterrupted cycle proof: no per-stop
   Move/Load/Unload or callback that secretly completes the route. Issue the
   receiving BuildCmd after that interval, with an unchanged command shown to
   refuse before sufficient freight and succeed afterwards.
3. Prove at least two complete automatic source-to-destination cycles on the same
   route with positive transfers, new production between them, actual sailing,
   crew attachment, and causal receiving stock/construction. Account for every
   goods source/sink, residual, city bill and player's money; unexplained terms
   must be asserted zero. An inherited hold balance cannot count as new output.
4. Add a bounded no-production control from before workers/in-flight output exist,
   then one public interruption/recovery branch (e.g. suspend/resume). Choose
   deterministic tick/work bounds from a short pilot; a deadline is a failing
   bound, not a success condition. One source/destination from accepted evidence
   suffices; do not repeat a broad site search or silently extend failed timeouts.
5. Reviewer checks the ledger independently and proves the driver isn't doing
   the automatic work. Retain exact commands, counts, failure records and input
   hashes in ignored evidence. Publish summaries/our own synthetic fixtures only.

### R6 — Continuation, replay, bounded scale and Windows contract

1. Establish five meaningful, reachable public save cuts with the R1 reviewer:
   sailing, arrival before transfer, between actions, after partial effect and
   suspended are candidate phases, not requirements to expose atomic internals.
   If arrival/transfer or several actions are atomic within a visit, prove that
   atomicity with a focused test and substitute adjacent reachable cuts (for
   example after completion, on the next leg, or after reassignment). Never add
   test-only halfway transactions or delay gameplay merely to create a cut.
   Every reload executes the full remaining suffix, comparing state, route cursor,
   events/draws and terminal ledger to uninterrupted execution; a matching
   immediate hash is insufficient.
2. Register a single genuine-recording producer plus fresh-process honest and
   changed-command replay consumers in CTest with correct fixture dependencies.
   The changed route/action command must be independently legal and diverge at
   its first causal checkpoint, after matching the earlier prefix. Check artifacts,
   child exit status and stderr; preserve exact binary/source/fixture hashes.
3. Measure short Release synthetic workloads at multiple active-route counts,
   stop-list lengths and unrelated-world sizes, separately reporting operation
   counts and raw timings. Assert no new per-route whole-world cargo scan. Measure
   algorithm cost without claiming original navigation bottlenecks solved or
   multiplayer/late-game acceptance. Only fix regressions introduced by this work.
4. Write `docs/testing/automatic-freight.md`: typed command recipe, stable IDs,
   actual pending/arrived/transferred/paused/failed observations, cancel/manual
   takeover, rule/identity requirements and Windows adapter delta. Do not edit
   `src/app` or promise a live window implementation. Keep manual-expansion card.
5. Independent review verifies persistence completeness, replay causality,
   timing labels, cost bounds and the executable consumer recipe.

### R7 — One finite final gate and integration review

Freeze code, tests, references and tools before the final campaign. Use the
canonical runner with explicit owned source/build paths, required originals,
reconciled test inventory and exact required reference/CLI contract names.
Run cheap controls first, then focused Debug and ASan/UBSan for route/navigation/
manual cargo/command/state/replay/shared seams. Run affected broader regressions
where a shared scheduler/serializer change requires them. Include one current
source real automatic-voyage sanitizer run; do not rerun unrelated heavy render,
all old sailing canaries or whole corpus matrices merely to enlarge a pass count.
Run honest/changed CLI replay as registered gate consumers. Record not-run target
Windows/Linux, original-runtime and live/GPU evidence explicitly.

Each R package has an independent read-only review subagent, separate from its
implementer. Record durable reviewer identity, exact source, inspected primary
facts/callers/raw evidence, findings and focused re-review. Reviewers do not
implement their own fixes or repeat long matrices. One fresh final reviewer
checks cross-package interaction, tested-source identity and evidence reuse.
Fix blocking findings; re-run only the affected checks unless dependencies really
changed. Separate required contracts from selected case counts. Do not call a
missing prerequisite a pass or accept a pre-fix executable after mutation work.

**Completion:** automatic production-backed cycles, authority/lifecycle tests,
all five reviewed reachable continuations, genuine and causal changed replay,
affected gates and independent approvals are complete at a named source. Commit the contract,
references, tests, consumer card, review ledger and REPORTS entry. Report separate
code/test/docs pins, exact counts, preserved failures, adaptations, open limits,
compatibility delta and Windows integration needs. Ordinary push the owned task
branch at every checkpoint and final report; never force or push main. Leave
TASKS.md/STATUS.md to the orchestrator. Stop with a clean tree and no owned jobs.

If an essential predicate/identity conflict blocks a package, report PARTIAL with
its exact remaining work. Do not weaken the canary, seed supplies, enable house/
economy defaults, invent original rules or start another feature to fill time.
GitLab mirroring follows the owner rules above via the Windows orchestrator.
<!-- END MAP-ROUTE-1 -->

<!-- BEGIN GAMEPLAY-IDENTITY-134-135 -->
## Compatibility decision — founding accounts and salt sequencing (2026-11-06)

**Released to the two Mac lanes; reservations, not main acceptance.** At reservation,
main implemented state 133 / ECONBK1 / PRNG 5; 134 is now accepted above. The following distinct
successor identities prevent concurrent branches from silently redefining 133:

| Identity | Owner and prerequisite | Scope |
|---|---|---|
| 134 / ECONBK1 / PRNG 5 | Second Mac, MAP-EXPAND-1 | Preserve the already checked temporary founding City.accounts when adopting its index. No saved-layout change. |
| 135 | First Mac, MAP-EXTRACT-1; must include the independently reviewed 134 correction before claiming this identity | Reviewed salt placement/deposit behavior, following the contract and scope below. Schema/content changes must be documented; no shared number allocated by an executor. |

Main receives either identity only after orchestrator acceptance/integration.
Do not amend published 133 or combine unrelated salt and founding changes under
134. First Mac's current 133 correction/research work can continue; it must not
independently take 134. Windows remains on its assigned accepted input.

### Second Mac — founding correction is authorized now

Continue on the owned MAP-EXPAND-1 branch from checkpoint
`71af3846d6a7dda663bcb4271e88743f4b47bff1`; tested source `637da167` remains the
bounded partial evidence pin. No moving-main adoption or first-Mac merge is
required. Implement the independently reviewed correction beside index adoption:
`city.accounts = local.accounts`. Do not rescan, add compensating treasury credits,
repair old histories, alter founding payment/draw order or introduce a new flag.
Keep city_booking and house_updates default-off.

Allocate **kStateHashVersion 134**, retaining the existing **ECONBK1 schema tag,
PRNG timeline 5, replay framing 4, command wire 1 and native wrapper 1**. No field
order changes are needed. Update the independent stream oracle and current
fixtures/recordings consistently. Preserve historical 133 artifacts, including
actual snapshots with the missing history, under their original names/hashes.
Use a new current134 fixture location; never relabel an old recording as new.

Compatibility policy: new 134 builds **reject all 133 native states and replays**,
including booking-disabled states, with explicit version diagnostics. No migration,
conditional old-save acceptance or silent cost reconstruction. The old 133 binary
continues to read its own historical artifacts. Raw 133/134 bytes and hashes differ
by identity even with booking off; unchanged behavior means gameplay fields,
commands, payment and draws, not identical hashes across versions.

Required focused proof before resuming E2:
- Public Found with booking enabled retains the original 1302 contribution 15;
  the first due booking charges 1. Forester addition gives 27 and its evidenced
  inactive transition gives 19. Check no double charge and independent cities.
- Booking-disabled public Found remains zero-history with unchanged gameplay,
  gold/goods/draws. Refused founding remains unchanged under both rule settings.
- Current 134 native continuation and genuine replay pass. Real old 133 native
  and replay artifacts fail for version, not corrupt/truncated data. Include
  valid controls and distinct replay-header versus embedded-native identity
  mismatches so one earlier check cannot falsely cover the other.
- Re-derive version-only fixture changes with the independent oracle; distinguish
  expected enabled-founding changes from header-only changes. Preserve first
  failing reproducer and the proposed-patch provenance from manual-expansion.md.

Independent correction review releases E2 revalidation and the already assigned
E3–E7; no routine orchestrator pause after the fix. Publish the small reviewed
fix/identity commit hashes in REPORTS and notify this orchestrator as soon as
available so the first Mac can consume the exact prerequisite. No main merge/push
or default flip is authorized. Push the owned task branch normally at each
checkpoint under the top owner rule. Final full-expansion acceptance remains open.

The CPU-only benchmark issue is separately fixed at main `b408f311`. At an owned
clean checkpoint, a **single cherry-pick of b408f311** is authorized to remove the
local -Wno-error workaround, without pulling moving main. Rebuild bench_picture
normally and run its required contracts; no old hand-built object may establish
warning-clean acceptance. Keep its source delta separate from the founding fix.

### First Mac — salt compatibility and scope disposition

The first-Mac request at `9a34664d`, salt-extraction.md §§6,8, is answered as follows:
choose **a new identity with the narrow place-and-bind scope (Option N)**, not
loader-wide activation of all imported mines. Existing serialized Deposit/Mine
fields may be reused; a new accepted command still changes deterministic behavior
and therefore receives a new identity even if its field layout is unchanged.

Reserve **135** for this slice, but do not start or label a salt runtime candidate
as 135 until the reviewed 134 founding correction/identity dependency has been
supplied at a named checkpoint. Consume only that approved prerequisite, preserving
owned corrections/evidence; no wholesale expansion branch merge. Research,
reference, contract and fixture preparation continue independently meanwhile.
Orchestrator acceptance of that small prerequisite does not require waiting for
all expansion voyages. No alternative branch using 135 without the fix is allowed.

Once the prerequisite and X1 independent contract approval are present, X2–X6 may
continue under the existing packet. Preserve defaults and no-draw behavior where
proved. If HIGHMAP requires new digested data or saved fields, document the exact
schema/content delta before landing; 135 is reserved for that reviewed salt slice,
not authority to invent missing terrain/deposit rules. Retain PRNG timeline 5 only
if no draw-order change is established. A real draw change needs coordination.

Imported mines remain explicitly unsupported/inert under the narrow scope; do not
claim this as original loader fidelity. New built mines must share an existing
source rather than duplicate its resources; teardown/rebuild must not refill a
finite ore. Add typed refusals for unproved classes; no general quarry or wildlife
activation. Deposit quantity/depletion settings and deterministic HIGHMAP sampling
still require primary evidence and independent review. This decision grants no
float-in-sim exception and approves no unverified fixed-point approximation.

At 135, reject old 133 and 134 recordings explicitly, preserve both as history,
and generate new current fixtures only from reviewed behavior. No migration is
assigned. Keep the 134 founding regression as a dependency test on the salt lane.

The first Mac is not available through this chat's connected chat tools; this
shared TASKS entry is the durable coordination message, not a claim that it has
already acknowledged it. Read at the next natural checkpoint without interrupting
work. Second Mac receives the exact published decision through its authorized chat.
Independent read-only dispatch review: `/root/review_consolidation_plan` approved
the 134/135 reservation, strict compatibility policy and conditional salt scope.
This is not acceptance of either unimplemented successor.
<!-- END GAMEPLAY-IDENTITY-134-135 -->

<!-- BEGIN MAP-NEEDS-CLOSE-EXTRACT-1 -->
## First Mac — close MAP-NEEDS-2, then MAP-EXTRACT-1 (2026-11-06)

**FINISHED, 2026-11-11 note.** MAP-EXTRACT-1 (`task/MAP-EXTRACT-1-x2` `2f32437a`) is contained in MAP-EXTRACT-2's
CI-green head `fix/MAP-EXTRACT-2-ci` `a5a3bca3`. That head is an input of
[MAP-INTEGRATE-2](tasks/map-integrate-2.md), state 138.

**Checkpoint publication:** push your own task branch to origin at every checkpoint
and report, ordinary pushes only. Never push main or force-push. This supersedes
older no-push wording for your task branch; all merge/rebase/other-lane boundaries
remain in force. Keep public GitHub content within the top owner publication rule.

**Released:** C0–C2 corrections, followed by X0–X6 salt extraction within the
conditions below. Independent package approvals release dependent work without
routine orchestrator pauses. Hunting remains PARTIAL and outside this packet.
Second Mac owns MAP-EXPAND-1; Windows owns the live settlement window.

Read the [orchestrator review](reviews/2026-11-06-map-needs2-orchestrator.md).
`5df34b38` is a preserved candidate, not accepted main. The cloth component and
labelled-hide leather mechanics have useful runtime evidence; the scoped test
corrections and clean integration still need proof. No new wildlife policy is
implied by closing those slices. Do not port the fauna sweep or change world draws
as a side task. Retain hunting as an explicit separate lifecycle/identity contract.

### Read before work

- AGENTS.md, CLAUDE.md, PLAN §§0.1–0.3, the permanent evidence contract and testing
  README; this packet supersedes the historical MAP-NEEDS assignment.
- The review above, candidate REPORTS entry, early-needs.md §§12–16, re/hunter.md;
  cloth_town.hpp, cloth_canary_test.cpp, leather_town.hpp,
  leather_fixture_test.cpp, leather_persistence_test.cpp and tutorial_town.hpp.
- Current ECON acceptance and state133 contract, production and original placement
  evidence: docs/sim/{state,production,ship-placement,road-placement}.md,
  docs/re/{production,ui-and-placement,ship-placement,road-placement}.md,
  docs/sim/{ship-start-bootstrap,economy-booking}.md and open-questions.md.
- Locate Mine, deposits, placement classification, atomic dry-run overlay,
  BuildCmd payment/permissions, carrier collection and household supply through
  current callers with rg. Read the corresponding tests and Python references.
  Original data and binaries are read-only; follow precise primary evidence
  links rather than trusting old notes about an unread placement branch.

### C0 — preserve and adopt current main safely

Preserve task/MAP-2-needs2-main at `5df34b38` and the old state-130 branch/evidence. Fetch
origin. Create task/MAP-EXTRACT-1-main in your own clean checkout from the published
main commit containing this dispatch and bench fix b408f311. Record that exact
starting commit. The baseline is state 133 / ECONBK1 / PRNG 5; do not revert it to 132.
A local merge of your completed `5df34b38` candidate into that successor is authorized;
resolve only its integration conflicts, retaining all main CMake registrations,
ECON changes, current instructions and both reports. No main merge, force push,
reset of old branches or changes to another worktree. Inspect any intervening
accepted main code before adopting a newer tip.

Detect current host resources/contention and use calibrated jobs/admission. Do
not stop another executor's work. Use explicit source/build paths and normal CMake
builds, including bench_picture; no hand-compiled objects or warning overrides.

### C1 — close the specific review findings

1. Scope leather in-flight predicates to the intended owner, city, home and job;
   a carrier must belong to the tannery chain, delivery to the target collection
   chain, and a resident to the target houses with positive leather cargo.
   Derive any corrected cut timestamps from those predicates, not old constants.
2. Scope cloth recovery's cart/wool-cargo sums to the target chain, accounting for
   old/replacement weavers and legitimate demolition loss. Audit related helpers
   for island-only assumptions; assert them or use precise city/home membership.
3. Add small positive/negative controls: rival loaded figures cannot satisfy a cut
   or change a target ledger; an empty resident cannot satisfy a loaded cut;
   an actual target loaded figure can. Prefer existing fixture states and helpers
   to duplicating long production runs. Restore one wrong-scope implementation
   in an isolated copy and show the intended assertion fails with its control green.
4. Rename/disclose the actor-free original-data component projection; preserve
   the existing fixture and its useful proof. No new full-scenario campaign.
5. Delete the unsupported no-per-visit-scan conclusion from the scale note. Keep
   recorded timings, actual sizes and one-run limitations exactly as measured.

Independent review must inspect the predicates and caller identities, not just
passing ledger totals. Correct and re-review blocking findings.

### C2 — bounded integration proof, then continue

Keep old state-132 recordings immutable. Regenerate only current identity-dependent hash
pins/logs after proving unchanged numerical production, payment, lifecycle and
household results, except explicitly explained corrections to scoped observations.
Do not bless a changed behavioral count as a version-only update.

Freeze and run corrected cloth/leather/persistence tests and relevant existing
references in Debug and ASan/UBSan through the canonical gate with explicit build
targets and required contracts. Verify genuine, changed-legal-command, changed-
checkpoint and refused-command fresh-process replays at this source; automate
artifact producer ordering where practical, otherwise retain exact manual receipts.
A warning-clean benchmark build and picture_matrix_contract are mandatory.
Reuse unaffected broad gates with their actual pins. No full 1600-case campaign
merely to close these local corrections. Report C0–C2 independently approved as
cloth/component and synthetic leather mechanics closure, hunting still PARTIAL.
This approval releases X0–X6; do not merge or push main. Push your owned task
branch at the checkpoint under the current owner rule.

### X0 — salt dependency inventory and compatibility plan

Start from parsed shipped data: salt mine, any required processor, carrier, stall,
consumer stage, source/deposit representation, legal terrain, costs and permissions.
Identify what existing Mine and transport code already implements. Choose one real
scenario with the necessary natural source and reachable consumers. Ship-start
self-sufficiency is not required; disclose imported buildings, stock and actors.
Separate preloaded salt from newly extracted/processed/delivered salt.

Make a compact acceptance table for X1–X6: evidence, positive control, distinguishing
wrong interpretation, expected result, reviewer. Record known Unsupported classes.
Determine compatibility before changing runtime: reuse state 133 only if saved
layout and deterministic semantics remain compatible under the project contract.
Changes to accepted-command outcomes, saved state, content digest or PRNG behavior
need a recorded integration identity decision with the orchestrator before landing.
Do not silently amend 133 or allocate a shared version. Continue independent
research/reference/fixture work while any such decision is pending.

### X1 — independently reviewed extraction/placement contract

Trace only necessary mountain/coverage/deposit arms and relevant creation/removal
callers. Establish source matching, bounds, orientation, height, ownership,
replacement/occupancy, permissions, payment, source consumption/renewal, capacity,
collection and failure cleanup. Evidence must distinguish imported mine state from
what a new BuildCmd must initialize. Do not infer resource renewal from names.

Write the contract and hand-derived vectors before C++. An independent read-only
reviewer checks original bytes/data, masks/widths/units/rounding and the smallest
counterexamples. Preserve original rules, explicit approved adaptations and unknowns
separately. An unread decisive predicate is remaining research, not permission to
invent a permissive placement rule. Contract approval releases only the evidenced
salt slice; new gameplay-policy choices still require the orchestrator.

### X2 — minimum reusable placement and initialization

Implement only missing behavior needed for player-built salt extraction. Use
parsed type/deposit facts and existing typed interfaces, not a salt-id whitelist.
Preserve ordinary/coastal/road behavior, command authorization and server-derived
height. Failed placement/payment leaves the world unchanged. If atomic BuildCmd
is supported, its dry run and live application must agree on latest tile/source
state in command order. Create, load and rebuild must initialize equivalent valid
mine/source state; an imported working mine alone cannot prove player construction.

Use small tests for a legal source, wrong/missing source, terrain/rotation/bounds,
occupied/replaced footprint, rival ownership, unavailable permission, insufficient
funds and repeated placement. Pair each refusal with a legal neighbor/control.
Independent review and focused re-review before using the new path in X3.

### X3 — extraction, processing and actual delivery

Connect existing production/transport pieces; add only missing evidenced seams.
Prove new source output reaches the required processing step (if any), warehouse,
stall and actual household consumers. Maintain source/raw/output/in-flight/stock/
purchase ledgers for the chosen owner and town with zero unexplained residuals.
No direct salt credit or test-only production grant may satisfy the real arm.
Account separately for declared scenario starting inventories.

A source-disabled or producer-removed control must distinguish new production
from preloaded stock. Prove public-command construction with original costs and
terrain; if the chosen original town lacks a legal site or permission, investigate
another bounded candidate, never inject permissions or cheapen the bill. Treat
finite deposits honestly; do not add renewal just to make repeated production pass.

### X4 — interruption, isolation and lifetime boundaries

Cover exhausted/missing source if supported by the original contract, full output
or warehouse, blocked route, competing consumers, in-flight producer/home removal,
repeated end/removal and rebuild recovery. Preserve exactly-once debit/credit and
release of claims/counters. Check valid native state immediately after interrupted
jobs, not only after a later cleanup tick. Use positive rival activity and precise
home/city ownership in all observations. An alternate supported data-defined type
or goods mapping must reuse the seam without an engine id special case.
For any changed timed transition, test the smallest supported positive interval,
zero elapsed time, partitioned delays with surplus, and native continuation of
partial progress; do not rely only on the typical scheduler interval.

### X5 — continuation, replay and bounded performance

Save/reload at extraction, carried output and consumer-delivery boundaries with
actual target-chain state asserted. Compare uninterrupted outcomes, resource/money
ledgers and PRNG draws. Fresh-process replay must reproduce the genuine recording;
a changed legal command must diverge causally after matching earlier checkpoints.
Preserve format-version rejection controls separately from gameplay assertions.

Measure source lookup and active producer work with operation counts and a modest
multi-city fixture; retain timings/sample counts and host contention. Do not claim
large multiplayer scalability from a small case, and do not rewrite unrelated
scheduler/search systems. Expose read-only facts Windows will need, without editing
its UI or implementing a second command language. Name unproved iron/quarry cases.

### X6 — frozen verification, final review and stop

Run cheap controls and one pilot before the expensive real chain. Each package
gets a read-only reviewer distinct from its implementer (reviewer reuse across
packages is allowed); a fresh final reviewer checks the combined
path, scoped ledgers, identities, source/command provenance, reuse and claims.
Review corrections locally, not through repeated broad campaigns. Freeze source,
use canonical Debug and affected ASan gates with original assets and exact selected
inventories, and include current-source sanitizer coverage of the new extraction
and delivery path. Keep initial failures, actual skips and NOT_RUN target hosts.
Never mutate source during a gate or suppress warnings to manufacture a pass.

Commit separate closeout and extraction units with review records, evidence hashes,
consumer delta and REPORTS entry. Stop at a clean local candidate, no owned jobs,
no main merge/push/default flip. A genuine unresolved contract or identity decision
is a precise partial handoff after independent work is finished; no fabricated
hunting, balanced-economy, complete mining or full-scenario acceptance.
Dispatch independently approved by `/root/review_consolidation_plan`; its timing
and reviewer-reuse clarifications are incorporated. This approves the assignment,
not the unexecuted correction or salt results.
<!-- END MAP-NEEDS-CLOSE-EXTRACT-1 -->

## 1. Protocol for the executor

1. **Read first, every session:** `CLAUDE.md`, this file, `docs/PLAN.md` §0.1–0.3 and §7, and the spec named by the task. The rules in `CLAUDE.md` apply unchanged (evidence only, determinism, no hardcoded limits, tests before refactors, `game/` read-only, nothing from `game/` in git). **Since round ct:** when the task comes with a packet (item 13), the packet replaces reading this file. Claude Code loads `CLAUDE.md` by itself; `AGENTS.md` is the same text for Codex, so it is not read again (never edit or commit it).
2. **Pick the first task whose status is `READY`** and whose dependencies are `DONE`; tasks marked `PARALLEL-OK` may run at the same time as another task in a separate git worktree (`git worktree add ../anno1503-<id> -b task/<id>-<slug> main`). Never run two tasks that touch the same files in parallel.
3. **One branch per task:** `task/<id>-<slug>` from `main`. Small conventional commits (`feat(render): …`, `fix(sim): …`, `docs: …`, `test(...)`), one concern each, each ending with the attribution line the harness gives you. **Do not merge into `main` and do not push** unless the task says so. The orchestrator merges after review.
4. **Gates before you report a task as finished** (all of them; the owner's PC has 16 threads, use them):
   - `cmake --build --preset msvc` warning-free (warnings are errors).
   - `$env:ANNO1503_GAME_DIR="$PWD\game\app"; $env:ANNO_TEST_THREADS=16; ctest --preset msvc -j 16` green **and** the same without `ANNO1503_GAME_DIR` (skips must be clean skips). Round cp: 16 is a concurrency ceiling, not permission to allocate 16 full-map renders; use measured per-item/process budgets and configurable host headroom. The full test set and clean-skip requirement remain.
   - clang-format 23.1.3 clean: `python -m pip install --target <scratch>/cfpkg clang-format==23.1.3` then `find src -name '*.cpp' -o -name '*.hpp' | xargs <scratch>/cfpkg/bin/clang-format.exe --dry-run --Werror` (a venv fails with Python 3.14 in Git Bash).
   - For `sim`/`formats`/`test_support` changes: the GCC 13.3 `-O3 -Werror` check in WSL (`Ubuntu`) with the CI's exact flags (`-O3 -DNDEBUG -std=c++20 -Wall -Wextra -Wpedantic -Wconversion -Wsign-conversion -Wshadow -Wnon-virtual-dtor -Wold-style-cast -Wcast-align -Wnull-dereference -ffp-contract=off -Werror`, doctest from `build/msvc/_deps/doctest-src`, run with and without `ANNO1503_GAME_DIR`). For `render`/`app` changes it is enough that the MSVC build is clean and you avoided the known GCC traps (`std::swap`/copy of structs holding `std::optional<std::vector<…>>` at -O3; sign conversions).
   - `tools/agree.py` where an `anno-dump` command changed.
   - A performance task also needs its `bench_frame` / `bench_sim` numbers in `docs/perf/README.md` (p50/p95/p99/max, hardware, commit).
5. **Subagents (owner, 2026-10-26: allowed again, where necessary and helpful; from the tasks after the one in progress at that time).**
   - **Use them where they help:**
     - independent reads in parallel, for example several functions or modules of an RE task, or the corpus files of a measurement;
     - an independent `PARALLEL-OK` task in its own worktree;
     - a read-only reviewer pass on a large or risky change before you report.
   - **Rules that stay:**
     - The task is yours: you brief each subagent with the task's steps, files and the no-guessing rule; you check what it brings back against the files before it enters a commit; and the report names each subagent and what it did.
     - Subagents never merge, push or edit `E:\anno1503`, and never take a task the board has not given you.
     - **Budget combined memory and CPU use across jobs and internal workers** (round cp). Subagents share that budget; no blanket exclusive heavy-job lock. Isolate build directories and logs, and retain locks for actual shared resources.
     - The orchestrator's independent review stays.
     - You still run the gates and your own mutation checks (flip a constant or a branch, the named test must fail; list them in the report) before reporting.
   - *Earlier rules, for the record:* two read-only reviewers per task (up to round r); no reviewer subagents (2026-10-23 s); no subagents at all (round aa).
6. **Report** by appending a section to `REPORTS.md` in the format given there: what was built, evidence (test names, numbers, PNG paths under `C:\Users\altin\Pictures\anno-review\<id>\`), what the reviewers found and what you changed, what is left, open questions. Then stop that task; **do not start the next step of a task marked `OWNER-CHECK` until this file says the owner has looked**.
7. **Decisions you must not make:** anything that changes the game's look or feel (defaults of render styles, river tuning, rules options, limits) is the owner's (`docs/open-questions.md`, "Decisions for the owner"). Implement the faithful or stated default behind an option and list the question in your report. Obvious bugs you just fix.
8. **If blocked** (missing information, a gate that cannot pass for a reason outside the task, a conflict with `main`): write a `BLOCKED` entry in `REPORTS.md` with what you tried and move to the next `READY` task.
9. **Linux and macOS warnings are part of every gate, not only for sim/formats.** `cmake/AnnoWarnings.cmake` turns `-Wsign-conversion`, `-Wnull-dereference`, `-Wconversion`, `-Wshadow`, `-Wold-style-cast` into errors on GCC and Clang; MSVC does not report most of them. Before a report says "warning-free", the executor runs the GCC 13 `-O3 -Werror` build in WSL over every changed library and its tests (render and app included) and a Clang build of the same; the orchestrator does not merge a branch whose CI run on the branch (push it, open a PR) is not green. Learned 2026-10-21: T1c and T1e were merged with `main` red for a day. **GPU tests skip in CI** (until T13): a render report states which GPU tests ran locally and on which adapter(s); a GPU change without that line is not reviewed.
10. **Run what you built and look at it yourself before you report (owner instruction 2026-10-21 o).** Tests are not enough: the owner found the pixelated far zoom of T3 stage 3 in the first minute of looking. For every change that affects what is drawn: run the viewer or `--png` over the views the change touches (every zoom level it affects, both water styles, the ten `0200Single` scenarios plus the canyon island `D_256x256_0045_5B.scp` where the change is not local to one place, 1280x800 and 2560x1440, the RTX 4090 and the Radeon iGPU where the GPU path changed), open the PNGs (the Read tool shows images) and look for pixelation, doubled rows, seams, holes, pop-in, wrong colours or depth order, and read the viewer's frame log for hitches. Where the original can show the same view (T15's harness once it exists), compare against it. For simulation changes, run the original's fixtures once T15 stage 1 has them. Say in the report what you ran and what you saw; the owner's look comes after yours.

11. **Sub-tasks, one PR each (since round aa).** An open task is split on this board into numbered sub-tasks (`T30b.1`, `T30b.2`, …). Take the first sub-task whose status is `READY` and whose predecessors are `DONE` or `REVIEW`; do only that sub-task; open its PR; write its report section; then stop and take the next one. Never two sub-tasks at once; subagents may help inside one (item 5). Each sub-task's steps are ticked in order; a step's result is written down before the next step starts (a table or a list in the report, not from memory at the end). **Thoroughness over speed:** a sub-task has no time box; re-read a disassembly twice before tagging a claim D; a number is re-computed before it is reported; a claim about the original carries module + address, and the report says which instructions were read (address range) — never the disassembly text itself, never bytes of the original. If a step does not hold, write what was found and the question into REPORTS.md and stop that step (item 7 below). Before calling a stack slot "beyond the arguments" or a value "untraceable", count the pushes that are live at that instruction and read the function's only caller (round ae: `[esp + 0x14]` in `0x1000eb40` was its second argument, the world). **This-adjustment rule (round an):** every class of this engine has several vtables (interfaces at `+0`, `+4`, `+8`, class Y also at `+0x2048`); a method's `this` is the address of the vtable slot it was called through, so `[this + k]` in a method of the `+4` interface is `[Y + 4 + k]`. Before you compare a field of one method with a field of another, write both as absolute `Y + n` (first row of every function table: "this = Y + ?, because slot … of vtable … at `[Y + ?]`", or "called with `lea ecx, [reg + 4]` where reg = …"). Docs-only sub-tasks need no build; a sub-task that adds a tool under `tools/` runs it once and pastes the command and its first lines of output (counts and names only) into the report. (Two items of this list carry the number 7; "item 7" in the task sections means the steps rule below.)

7. **Steps, not judgement calls (since round u; the executor is a simpler model).** A task section lists numbered steps with the files, the functions and the commands. Do the steps in order; do not add scope, do not refactor what a step does not name, do not change a default the owner has to decide (`rules` options, OQ decisions). **Allowed without asking (owner, 2026-10-24 w), when listed in the report:** tests for the code the step changed, and fixes of sentences (docs, comments) that would otherwise contradict that code. Anything else is a question. When a step is unclear, when a file or function it names does not exist, or when a measurement contradicts the step's premise: stop that step, write the question with what you found into REPORTS.md under the task, and continue with the next step that does not depend on it. Report numbers as measured, including the ones that miss the acceptance. Every claim about the original carries module + address, every mapping in code cites its address in a comment. The acceptance list is ticked item by item in REPORTS.md with the evidence (the test name, the command's output line).

12. **Check, do not guess; write down decisions and reasoning (owner instruction, 2026-10-28, round cs).** It applies to every task.
    - **When in doubt, check the evidence before you act.** Never fill a gap with a plausible value.
      - **The game's data, formats or behaviour:** read the original files in `E:\anno1503\game\app` (read-only; nothing from them into git) or the disassembly (`tools/disasm.py`, module + address).
      - **The project's own code, tools or the host:** read the code, or run a bounded measurement.
    - **If the evidence does not settle it,** stop that step and write the question into REPORTS (item 7), or add an OQ in the task's range.
    - **Every REPORTS section has a "Decisions and reasoning" list.** Each choice the steps left to you gets one entry:
      - what you decided;
      - the alternatives;
      - the evidence (file, address, command output);
      - why.

      This covers a name, an interface shape, a bound, a test design, and anything you checked against the original files and what it showed.
    - Where a decision would change the game's look or feel, it is the owner's (item 7), not yours.

13. **Task packets and the preflight (round ct, 2026-10-28; fewer start calls).** Every new executor assignment comes as one packet file under `E:\anno1503\cache\orch-<round>\`. The orchestrator generates it with `tools/orch/packet.py`, which writes:
    - the orchestrator's detailed instructions;
    - the exact preflight command;
    - the binding board blocks, cut verbatim from this file at one commit, between `<!-- board: … -->` markers.

    **The start takes two calls:**
    1. Run the packet's preflight command (`tools/orch/preflight.py`). It checks main's head (that commit or a descendant), the allowed untracked files, that the board has not changed since the packet, and each named worktree's head, branch and cleanliness, and prints one PASS/FAIL table. Every row must be PASS; otherwise stop with a QUESTION that contains the table.
    2. Read the whole packet. Do not search this file again for the blocks the packet contains.

    **Afterwards:**
    - Open other files only when a step needs them, and batch independent reads into one parallel turn.
    - If the preflight reports that the board changed after the packet, re-read only the changed blocks on main.
    - The excerpts in the packet are the binding text; where the packet's instructions and the excerpts differ, the excerpts win.

    **History** (rounds co and older, superseded orders, finished task entries, the old §4 review log) is in [`HISTORY.md`](HISTORY.md), moved verbatim. Read it only when a step names it.

## 4. Review log (orchestrator)

Moved to [`HISTORY.md`](HISTORY.md) §4 in round ct. New review notes go into the round blocks at the top and into `REPORTS.md`.
