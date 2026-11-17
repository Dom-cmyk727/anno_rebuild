# FERTILITY-1: the island fertility grid and the crop test (sim queries for the fertility bar and dry fields)

**Draft 2026-11-17** by the UI lane, for the orchestrator's review as a sim packet (orchestrator 2026-11-17:
"Draft them as one packet, FERTILITY-1, one concern per stage ... Send me the draft before any code"). Branch
`task/FERTILITY-1`, base origin/main `4dfdd9b7`, worktree `E:\anno1503-worktrees\anno1503-FERTILITY-1`.
**Not released.**
- It runs after ISLAND-CROPS-1 1b, one stage at a time in this worktree.
- 1a is independent and may go first if the orchestrator allows.
- One concern per stage, each about 2-3 h.

## Why

- **The UI needs two sim queries.** UI-PRODBAR-1's placement fertility bar (accepted stage A, branch
  `task/UI-PRODBAR-1` `6b6758b9`: `docs/re/ui.md` §4.1, `production.md` §2.4 / §4.2b, OQ-715..719 and 726)
  counts the "good" tiles in a disc around a farm's ghost. A tile is good when:
  - (a) the crop test `0x100817c0` does not answer "dry", and
  - (b) the island's placement test accepts the crop object there.

  The rebuild has neither query.
- **The farm rules need the same queries.** UI-STATUS-1 stage C (`docs/re/ui.md` 5.6, `production.md`
  4.3, OQ-706) found that the crop test also decides which planted fields turn dry and never yield. That
  is the later D6 rule, an owner decision.
- **This packet adds state and queries only.** It changes no farm, AI or UI behaviour; readers come later.

Evidence base:
- task/UI-PRODBAR-1 `6b6758b9` and its verification (`cache/wf/verify-summary.txt` in that worktree);
- task/UI-STATUS-1 stage C (`docs/re/ui.md` 5.6, `production.md` 4.3; `cache/ui-status-1/wf/verify-summary.txt`);
- `docs/formats/data-text.md` (`Wachstum.dat`, line grammar already round-trips).

## Stage 1a: placement for single-tile objects (sim, about 2 h; independent)

The placement class for flag word `& 3 == 1` (single-quad tile objects such as fields and trees; flag word
`0x29` for the crop objects):
- **The height test:** the floor of the four-corner mean (AnnoGame `0x10008fd3`, `(sum) >> 2`) must be ≥ 32.
  The threshold 32 = 1.28f / 0.04f, from runtime globals set at `0x1007b340`/`0x1007b3a0` (D); put it in
  config with its source.
- **The overlap rule:** which existing tile objects may share the tile. Verify it per object in the code
  (AnnoWelt `0x1007c790`, island slot `+0x6c`); do not assume "overlappable only" (it is L,
  `ship-placement.md` §4).
- **The skipped cases:** the height test is skipped for flags `& 0x7000` ∈ {`0x2000`, `0x4000`} and for kinds
  `0x141`-`0x160`. Keep them in the code even though crop objects don't hit them.
- **Code:** extend `placement_rule` (`src/sim/include/anno/sim/placement.hpp`) and its oracle
  `tools/placement_ref.py`. Pure function, no new state.
- **Tests:** unit cases (corner heights at 31/32, an overlap allowed and refused, each skipped class), and
  a corpus pass of the oracle against the rule over the shipped islands' tiles for one crop object. Failure
  proof.

## Stage 1b: the Wachstum.dat growth table (formats + sim config, about 2 h)

- **What the file holds:** 146 records keyed by `Felder.tex` texture id, each a 6 × 6 `Proz` grid (0-100);
  46 add two `Klima` values and 5 add six `Bridge` rows (`data-text.md`). The line grammar already
  round-trips.
- **Decode:** a typed table (texture id → 6 × 6 percent cells, Klima, Bridge), with no meaning invented
  for `Klima`/`Bridge` (OQ-029).
- **The malformed line:** line 765 has five commas but six numbers. Read whether the original reads six
  numbers (AnnoGame `0x10006a80` builds the table). Otherwise keep `text::ints`'s six and leave OQ-029 g
  open.
- **Tests:** a round trip (parse → write → byte-identical) and a corpus test.
- **Oracle:** if `anno-dump` prints the table, `tools/agree.py` must agree.
- **In the sim:** a config table (the game-data defaults), exposed as game property `0x2e`'s source, as
  the original does (`production.md` 4.3).

## Stage 1c: the island fertility grid (sim, about 3 h)

- **Build** (`0x10084370`, `production.md` 4.3, D):
  - per tile, p = Proz[GFXID >> 2][cell u, v];
  - under a raw-material object (PRODTYPE 225-256) with HEAD `[0x19]` = v ≠ 0, blend p towards v:
    (p + 3v) / 4 if p < v, else (v + 3p) / 4;
  - b = (p + 5) / 10;
  - the stored nibble = min(10, Σ w·b / 12) over a 5 × 5 neighbourhood, with per-climate weights at
    `0x100ca008` (25 bytes per climate, climate byte `+0xc0`).
  - Integer only. Weights and the cap go in config with their addresses.
- **Where it lives:** `Terrain::fertility` today holds the raw `FRUCHTBAR` column (a stand-in, OQ-060). The
  rebuilt grid is a separate derived grid; don't overwrite the raw column.
- **When it is rebuilt:** OQ-726. `0x10084370` is reached only through island vtable slot `+0x1a4`. Read
  its callers first (load? raw object placed or removed? planting or felling?). If they can't be read in
  time, state the assumption "on island load and whenever a raw-material object on the island changes",
  and keep OQ-726 open.
- **Save and hash:**
  - **Proposal:** derived state, neither saved nor hashed; rebuilt after load.
  - **If** the read shows the original saves it, or the rebuild trigger makes it history-dependent, it
    enters the island stream with a `kStateHashVersion` bump, every changed golden is listed, and the
    landing needs the Mac's full game-file run.
- **Oracle:** a Python reference (`tools/`) that builds the grid from the island files and `Wachstum.dat`,
  checked against the C++ grid over every shipped island (corpus test).
- **Tests:** unit cases (the blend both ways, the 5 × 5 border at island edges, each climate's weights,
  the cap 10). Failure proof.

## Stage 1d: the crop test as a const query (sim, about 2-3 h; needs ISLAND-CROPS-1 1b and 1c)

`0x100817c0` (task/UI-STATUS-1 stage C and its verification, D):
- **The fertility value:** v = the tile's nibble (1c); 0 when the crop object's own climate mask (its
  HEAD `[0x13]`, type `+0x78`) lacks the island climate.
- **The factors**, in this order:
  - with the tile's well bit (chunk flag `0x80` via `0x1007bc00`; the name "well" is L): × 133 / 100,
    capped at 100;
  - with well bit and drought: × 2/3, then × 133/100, then the cap;
  - drought only: × 55/128, no cap.
  - Drought is ignored for trees and grass.
- **Halving:** v / 2 when the island's crop list (ISLAND-CROPS-1 1b, property 11) lacks the crop's ware.
  The goods rule is the next-id object's ware (`ROHSTWACHS`).
- **The result:** L = level, looked up in table `0x100c9c28[L][(y − y0) & 3][(x − x0) & 7]`, indexed from
  the island origin (`0x100817dd`/`0x100817e9`, `0x100819b9`-`0x100819d6`, pinned in UI-PRODBAR-1 fix
  round 1). It returns dry (1) or not (0).
- **Seams:** the well bit and the drought flag sit behind named seams ("no wells", "no drought") until
  those states exist in the sim. If S13's drought state already holds the chunk flag, use it and say so.
- **The crop-object resolver:** registry `+0x7c` (AnnoGame `0x10001500`), with the climate tree swap from
  table `0x1002137c` for a tree whose next id gives BAUM and whose own mask lacks the climate. It is a
  sim query too, so the UI holds no second copy.
- **Const query, no state change:** nothing calls it in gameplay yet. D6, the dry-field rule, is an owner
  decision.
- **Tests:**
  - the table rows (row 0 all zero, row 5 = 16 of 32 healthy);
  - the factor order;
  - the halving;
  - the climate zero;
  - the island-origin indexing on a non-8-aligned island;
  - the resolver's tree swap;
  - the polar forester halved on an ANBAU-loaded polar island (property 11 = 0), not halved on a link
    island.
  - Failure proof.

## Acceptance (each stage)

- **Evidence:** every rule cites an address or a parsed file, or is an OQ (from the next free id: ask the
  UI lane).
- **The gate:**
  - the sim lanes: determinism, save/replay when 1c touches state, the scenario corpus;
  - the formats and agree lanes for 1b;
  - `check_unix_warnings.sh`, one run at a time;
  - CI green.
- 1c, if it changes the state stream, lands only after the Mac's full run on the candidate.
- No gameplay change in any stage.

## The orchestrator's answers (2026-11-17, binding; draft approved)

1. **1c's state:** derived and unhashed only if the grid is a pure function of hashed state at every moment,
   so a load can rebuild it bit for bit.
   - Add a test that rebuilds the grid after a save/load round trip and compares it with the live grid
     after play.
   - If the OQ-726 read shows the original rebuilds only at triggers, a derived-on-demand grid would differ
     from the original's stale grid between triggers. In that case **store it**: save, hash, a version bump,
     and the Mac's full run on landing.
   - Decide from the read, and state which case it is in 1c's report.
2. **1a may run before ISLAND-CROPS-1 1b**, in its **own worktree and branch** (`task/FERTILITY-1a`), since
   one packet runs per worktree.
   - It changes what placement accepts for single-tile objects, so list every golden, replay or scenario
     fixture whose result changes, with the reason, and add a failure proof.
   - It is a faithful fix, not a rules option, unless the corpus check shows a visible change in an existing
     test map. Then the orchestrator must hear of it before landing.
3. **1d's seams:** first check whether the sim already holds a drought state with that chunk flag (grep
   `src/sim` for drought, `Duerre` and the S13 work, plus `docs/sim`).
   - If it exists, read it.
   - If not, "no wells" and "no drought" are the defaults behind named seams, each recorded as an
     ADAPTATION with the address of the state it stands in for, not stated as the original.
4. **Order:** 1a now; 1b-1d in order after ISLAND-CROPS-1 1b. Builds and tests pause during the Windows
   executor's announced quiet slot.

## Open points for the orchestrator's review (answered above)

1. 1c's state: derived and unhashed (proposed), unless the trigger read says otherwise.
2. May 1a run before ISLAND-CROPS-1 1b lands? It is independent and touches `placement.hpp` only.
3. 1d's seams: confirm "no wells" and "no drought" as the defaults until those sim states exist, or name
   the packet that owns drought (S13?) so 1d can read it.
