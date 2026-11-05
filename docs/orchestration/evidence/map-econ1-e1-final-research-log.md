# research-log-final.md (MAP-ECON-1 E1-FINAL, 2026-11-03)

Read-only on `/Users/dom/Dev/anno1503-worktrees/anno1503-worktrees/anno1503-MAP-2` (HEAD c82608bd) and on `game/app`. All outputs in this directory (`cache/map2-econ1-e1-final/`). Nothing under the worktree was edited, built or run (no CMake, no C++ test binary, no git write). No pkill/killall: every child was a bounded command (`perl -e 'alarm N; exec @ARGV'` wrapper, or `subprocess.Popen(...).communicate(timeout)` with `kill()` of the exact child pid).

## Inputs read
- Orchestrator review `scratchpad/REVIEW-CLOSE2.md`; `scratchpad/TASKS3.md` lines 1110-1230 (sections B, C) and 1398-1590 (E2-E6, the E5 canary).
- Engine (read only): `src/sim/src/{driver,scheduler,transport}.cpp` (consume/send_figure_job, input_store, merge steps 1-8, transport_tick, unload/StoreDelta::Input), `src/sim/src/production*.cpp` (`farm_step`), `include/anno/sim/{tick,draw,world,figure}.hpp` (Effects merge order, StoreDelta, DrawSite table, kPrngTimelineVersion = 3, kStateHashVersion = 125), `docs/sim/state.md` D12, T22, T24, 5.9, `docs/re/{annowelt,transport}.md` (figure tick 4.3, end path), `src/sim/tests/{founding_assets,unload}_test.cpp`, `docs/sim/{ship-founding,ship-cargo}.md` (the opening ledger).

## Commands (bounded)
- Interpreter: `.../anno1503-MAP-2/cache/map2-founding-evidence/venv/bin/python` (3.14.6; pefile, capstone, unicorn).
- `asm/` holds the disassembly text of the ranges read (`run.sh MODULE VA [bytes]` wraps `tools/disasm.py` with a 60 s alarm); `vt.py` reads vtable dwords, `rf.py` reads f32 constants, `tree_table.py` derives the cut transitions from `Anlagen.dat` (output `tree_table.txt`: 46 BAUM entries, 45 cuttable, 44 regrowing, 259 stump, 261 uncuttable).
- `tools/xref.py --reloc AnnoWelt.dll 1006f070 1006f071` -> farm class vtables `0x100c2bf0`, `0x100c8ae0`, `0x100c8c0c` (slot +0x2c); one targeted reloc lookup, no scanning campaign.
- Python reference: `forester_ref.py forester_vectors.txt` (61 vectors, 0 failures); `forester_ref_contract_test.py --ref forester_ref.py --vectors forester_vectors.txt` (11 checks). Reproductions of the review findings are listed below.

## Addresses read in this round (AnnoWelt)
`0x10072d20`-`0x10072ded` (send, whole), `0x100334f0` (figure slot +0xac = store job), `0x10034380` (+0x178: job arrival then run, delete when run answers 0), `0x10035ce0`/`0x1003b7c0` (constructor: remaining and rate 0.001f), `0x1003af00` (end dispatch, whole), `0x10034940` (+0x168 stop-job), `0x1003b6e0` (+0x110 release), `0x1003b730`/`0x1003b780` (home attach/detach), `0x10074540` (farm attach, first free of five slots), `0x1006f070` (farm detach), `0x1006ed78`-`0x1006ee5f` (dispatch, whole), `0x1006ea90` first lines (early exits), `0x100349c0` (hit points, whole), `0x100343c0` (route step, to the `+0x110` arm), `0x1005b740` (job abort), `0x100545c0` (step abort). Figure vtable `0x100c4950` slots read: +0x28, +0x74, +0x94, +0xa4, +0xac, +0x110, +0x168, +0x174, +0x178, +0x17c, +0x18c.

## What changed (per finding)
- B1: `docs-sim-forestry.md` 3.2 (overlay/transaction, credit rule, tick carry), E-14, E-15; reference `visit`/`visitstale`.
- B2/B3: 2.1 (faithful vs enhanced states), 3.5 (transition table), 3.6 (two Rules), 2.2 (raw width from configuration); RE note 2.13 item 7 and 4.3 corrected (hit-point path DOES abort the job: the E1 "killed figure leaks" statement is withdrawn).
- B4: RE 2.1 draw order, 4.3 full-output bullet, new 2.13; sim 3.1/3.7, E-17, E-20; reference `draws`.
- B5: sim 3.8 and RE 2.11 residual paragraph.
- B6: sim section 7 (one opening; ledger; E-19 registry-derived transitions).
- B7: sim 3.2 (StoreDelta::Input), 3.7 (DrawSite + timeline identity), 4.
- B8/C: reference and vectors (44 originals unchanged and still pass; 17 added), contract test.

## Reproductions (before the fix, with the E1 reference)
- empty file and type-only file: `0 vectors, 0 failures`, exit 0.
- `drain 4096 -> 1152 1288`: no output, killed by the 5 s alarm (exit 142).
After the fix: exit 2 with "0 vectors executed"; the drain vector passes in milliseconds. The contract test run against the OLD reference (`tools/forester_ref.py` of the worktree, read only, `--timeout 3`) fails 9 of 11 checks (empty, comments-only, type-only, missing file, unknown command, malformed line, both drain checks by timeout, bundled vectors by timeout), so it detects the defects.

## Hand derivations (before the program)
drain 4096/2944/2976/2912; the visit table (1500 + 6500 = 8000 first cut, 2000 + 6500 -> 16500 second cut, 3500 home -> 20000; 12000 -> Cut wait 4500; 3000 -> wait 5000; 18000 -> ToHome 2000 left; stale -> 768 at 22500); claims; draws = 3; candskip order. All 17 new vectors passed on the first run.

## Dead ends and cautions
- The tile scale `[0x100e0f58]` read from the file image is 0.0 (a runtime-initialised data cell); the 0.0 vs 15.0 Remove delay is therefore U-14, which only moves the stamp freeze, not the release.
- The first-job run rests on the D reading of `0x10034760` arithmetic (0.2f ticks, 0.001f micro-step) with `transport.md` 4.3; it was not executed in unicorn. The stamp value that the registry create writes was taken from `annowelt.md` (`0x10097250` argument not re-read).
- `+0x168` and `+0x110` have other callers (about 30 `+0x110` sites) that were not read; nothing is claimed about them.
- The fur trapper's tick was not read (U-13).
