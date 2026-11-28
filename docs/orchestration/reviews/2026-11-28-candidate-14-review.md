# Candidate 14 review (2026-11-28, orchestrator)

**Candidate:** `d3f7bd3c` on `origin/main` `93b25085`, built in `E:\anno1503-worktrees\cand12`; lands as `land/cand14`.

## 1. Contents

Six packets, each accepted at handover on its exact head with hosted CI green:

| Packet | Head | Merged through |
|---|---|---|
| RENDER-DIRECT-2c | `976271a2` | `fe4a1d59` |
| RENDER-DIRECT-3a | `fe4a1d59` (CI 46080839732) | `2f9361c6` |
| UI-KEYS-1 | `0595a0fe` | `8948714a` |
| UI-PLANK-1 | `5a08acd2` (`ui.md` restored; numstat vs `aeb65424`: 1 0) | `8948714a` |
| UI-PAUSE-1 | `8948714a` (CI 46081066719; owner viewer check 1-4 pass) | `2dc81bec` |
| FLAKE-COORD-STAGE-1 | `94139e8f` (CI 46095543235) | `d3f7bd3c` |

The three merge commits are the orchestrator's own (harness item 11: the owner reviews them):
`2f9361c6`, `2dc81bec`, `d3f7bd3c`.

## 2. Merge checks

- Conflicts only in `docs/open-questions.md` and `docs/orchestration/REPORTS.md`, all append-only: both sides kept, ours
  first. Each merge has 0 deleted lines in those files against its first parent.
- No `src/` or `tools/` file is changed by more than one of the three heads.
- Whole-candidate numstat against `origin/main`: 54 files, +3165 −100; no file with a net loss of 200 or more lines.
- No duplicate `## ` heading in REPORTS.md. `docs/open-questions.md` has OQ-096 and OQ-727 twice each, already on
  `main` before this candidate (follow-up, not this bundle).

## 3. Full game-file run on this PC

The owner's decision (2026-11-28, "Yes, PC run"): this run stands in for the Mac's.

- Release msvc build, 0 warnings (`cache/gates/cand14-build.log`).
- `run_selected.py --select . --jobs 16`, game dir set; log `cache/gates/cand14-full-01.log`. Provenance: head `d3f7bd3c`, empty status.
- `COUNTS (after retry): selected=2950 executed=2912 failed=0 refused=0 skipped=38 (platform=3 opt-in=30 other=5)`.
  5 memory refusals (heavy render cases) passed when re-run alone.
- **The verdict line reads FAIL, "prerequisite skip outside the declared gate policy",** for the same 5 `other` skips
  candidate 13's review accepted (§ Skips there): 3 `ship start:` content-path cases (Windows symlinks; they pass on
  Linux CI) and `info_panel_gog_compare`, `info_panel_rotation_gog_compare` (GOG shots not set). Accepted on the same
  grounds; nothing was changed to make the verdict pass.
- The 3 new opt-in skips are RENDER-DIRECT-2c's timing measurements (`ANNO_MEASURE_CHUNK_INSTANCE_TIME=1`).
- The bundle's own tests ran and passed: `viewer_wiring_ui_pause_gpu`, `viewer_wiring_ui_keys_gpu`,
  `viewer_wiring_ui_plank`, `ui_dynamic_draw_check`, both `plank text` cases, the four RENDER-DIRECT-3 3a proofs,
  `F4 (N4)` and `F3 (D2)` (the case that failed in candidate 13).
- GPU tests ran on the NVIDIA GeForce RTX 4090 (Vulkan).

## 4. Verdict

Accepted for landing once hosted `gate` passes on `d3f7bd3c`. The advisor's bundle review covered the merges, the run
evidence and FLAKE-COORD-STAGE-1's diff; the other five packets' code rests on their handover reviews.
