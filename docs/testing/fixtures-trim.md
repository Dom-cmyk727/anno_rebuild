# Fixture cleanup: FIXTURES-TRIM-1 (2026-11-15)

Base `01a4d540`; ADR 0006 decision 6. This is cleanup, with no stream,
behavior or pin-value change. Inventory counts tracked regular files and their
logical byte sizes (not filesystem allocation). The complete sim tree before
cleanup is **392 files / 33,476,670 bytes (31.93 MiB)**, versus ADR 0006's
older 387-file / 31.7 MB observation. Different dates are not conflicting counts.

## Coverage before removal

PINS-1 had already pointed every positive replay test at `current/`. No positive
replay test still required a numbered copy on this base. `replay_test.cpp` checks
G2–G6 against the live current schema; `founding_test.cpp`, `load_test.cpp` and
`unload_test.cpp` rederive current success/refusal logs. The `anno_sim_replay_*`
public-command tests reproduce the same current files and detect their wrong-log
controls. `scenario_test.cpp` and `economy_loop_test.cpp` check the current corpus
and loop checkpoints. `integration_stream_test.cpp` compares current populated
fields with the independent transcription. A current-schema replay does **not**
prove an old-schema refusal: those actual historical inputs remain below.

Copied semantic vectors with no numbered-path reader are covered by their active
root vectors and reference contracts. Unread source/provenance snapshots have no
active-test assertion to replace; git retains their history. The two files needed
by the independent published-128 model stay byte-identical, rather than rewriting
an oracle from current C++ code. No original game files are copied or deleted.

## Inventory and disposition

| Set | Before files | Before bytes | Deleted files | Deleted bytes | Kept proof |
|---|---:|---:|---:|---:|---|
| `(root)` | 44 | 2,190,793 | 12 | 1,836,593 | semantic/current data or historical controls listed below |
| `consolidated132` | 15 | 1,859,684 | 15 | 1,859,684 | none; current set covers active positive consumers |
| `current` | 17 | 1,881,811 | 0 | 0 | semantic/current data or historical controls listed below |
| `econbook1` | 4 | 73,232 | 0 | 0 | semantic/current data or historical controls listed below |
| `econbook1/current133` | 15 | 1,863,805 | 14 | 1,742,140 | semantic/current data or historical controls listed below |
| `econbook1/current134` | 1 | 433 | 1 | 433 | none; current set covers active positive consumers |
| `econbook1/legacy132` | 4 | 238,252 | 2 | 1,631 | semantic/current data or historical controls listed below |
| `expand1/current134` | 16 | 1,867,745 | 15 | 1,746,080 | semantic/current data or historical controls listed below |
| `expand1/legacy133` | 2 | 115,808 | 1 | 530 | semantic/current data or historical controls listed below |
| `freight` | 2 | 20,759 | 0 | 0 | semantic/current data or historical controls listed below |
| `freight/current136` | 18 | 1,872,607 | 17 | 1,750,930 | semantic/current data or historical controls listed below |
| `freight/legacy134` | 4 | 180,691 | 3 | 65,413 | semantic/current data or historical controls listed below |
| `integrate1` | 22 | 2,554,254 | 16 | 1,863,854 | semantic/current data or historical controls listed below |
| `integrate1/legacy` | 2 | 32,248 | 2 | 32,248 | none; current set covers active positive consumers |
| `integrate1/legacy/combined131` | 1 | 121,504 | 0 | 0 | semantic/current data or historical controls listed below |
| `integrate1/legacy/needs130` | 1 | 121,385 | 0 | 0 | semantic/current data or historical controls listed below |
| `integrate1/legacy/published128` | 34 | 2,110,118 | 21 | 246,653 | semantic/current data or historical controls listed below |
| `integrate1/legacy/supply129` | 35 | 2,116,964 | 34 | 1,995,579 | semantic/current data or historical controls listed below |
| `integrate1/legacy/unit130` | 67 | 5,844,684 | 65 | 5,601,713 | semantic/current data or historical controls listed below |
| `iron2/current137` | 16 | 1,866,996 | 15 | 1,745,331 | semantic/current data or historical controls listed below |
| `legacy138` | 3 | 237,406 | 0 | 0 | semantic/current data or historical controls listed below |
| `nav` | 19 | 472,239 | 0 | 0 | semantic/current data or historical controls listed below |
| `salt1/current135` | 16 | 1,869,428 | 15 | 1,747,763 | semantic/current data or historical controls listed below |
| `salt1/legacy134` | 2 | 115,749 | 2 | 115,749 | none; current set covers active positive consumers |
| `salt1/legacy135` | 1 | 115,278 | 0 | 0 | semantic/current data or historical controls listed below |
| `script1` | 16 | 1,856,108 | 14 | 1,854,906 | semantic/current data or historical controls listed below |
| `script2` | 13 | 1,865,325 | 12 | 1,853,995 | semantic/current data or historical controls listed below |
| `unit_import1` | 2 | 11,364 | 0 | 0 | semantic/current data or historical controls listed below |

**Deleted: 276 files / 26,061,225 bytes. After: 116 fixture files /
7,415,445 bytes (7.07 MiB).** Moving `current/goldens.txt` to `pins.txt`
changes neither file count nor bytes. New documentation is outside the fixture
inventory. `fisher_vectors.txt` and every other retained fixture's bytes match base.
Per-file pre-removal hashes and coverage classification are retained in ignored
`cache/fixtures-trim-1/cleanup-plan.json`; the counts above reconcile with it.

## Historical inputs kept because current cannot prove their assertions

- `integrate1/{published128,supply129,script129,unit130,needs130,combined131}.save`
  and each corresponding original G2 log used by `integration_identity_test.cpp`:
  actual native-state and replay-frame refusal boundaries, including distinct schemas
  that reused numeric versions. The SCRIPT1/2 logs are under the retained unit130 tree.
- Eleven published-128 logs under `integrate1/legacy/published128/.../sim/`:
  `script_canary_test.cpp` checks their exact SHA-256s and refusal before current parsing.
  The published-128 `tools/sim_stream.py` plus `src/sim/src/goods_table.cpp` remain
  dependencies of the independent SCRIPT1/2 and integration stream transcriptions.
- `econbook1/legacy132/g2.{save,log}`, `expand1/legacy133/g2.save`,
  `econbook1/current133/g2.log`, `expand1/current134/g2.log`,
  `freight/legacy134/g2.save`, `salt1/legacy135/g2.save`,
  `salt1/current135/g2.log`, `freight/current136/g2.log`,
  `iron2/current137/g2.log`, and `legacy138/g2.{save,log}`:
  economy/freight/integration identity tests prove actual predecessor refusal.
- Root `scenario_corpus.txt` is the historical supply/research counterfactual,
  explicitly read by `scenario_test.cpp`; its version is part of the asserted proof.
  Root `goldens_script1.txt` / `goldens_script2.txt` are historical independent
  oracle expectations, not current C++ pin sources or regen outputs.
- `script1/player_research_record_counts.txt` / `research_production_delta.txt`,
  `script2/handler_vectors.json`, semantic economy/freight/nav vectors and unit-import
  vectors are purpose-specific inputs, not copies per global state number.

## Exact input changes

1. The existing `golden.hpp` reader/writer and `pins.py` now use
   `fixtures/synthetic/sim/pins.txt`. It is a rename of the former current file,
   not a second pin mechanism. `sim_stream.py --check` follows this same path;
   highmap comparison follows its existing `goldens()` call. No formula changed.
2. `integration historical replay frame2 refuses before current parsing`
   (`script2_session_test.cpp`) reads the retained unit130/SCRIPT1 G2 log instead
   of `script1/g2.log`. Before-removal SHA-256 of **both**:
   `dbddfccc4066960cae8a60e1ba4c7e7e74b3d282f5d9bc4759f104821e7c2312`.
3. The native-134 row in `integration_identity_test.cpp` reads the already-retained
   freight legacy G2 save instead of the salt duplicate. Before-removal SHA-256 of
   **both**: `aad758490b57f78e06a895f90a1e9384ef87b4c55ce614130dd376cdabe33a37`.
4. `pins_contract_test.py` uses the same canonical location in its private fixtures
   and checks that no current/goldens.txt copy survives. No replay test was pointed
   at a newly generated file, and no old version was relabeled current.

All changed readers and retained historical inputs are covered by the full pins
selection plus disjoint fixture-consuming reference tests. The report gives the
classified terminal counts; current fixtures are checked, never regenerated here.
