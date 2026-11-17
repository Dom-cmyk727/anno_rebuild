# MAC-HEAVY-1 historical measurements — packaging READY FOR REVIEW

Tested HEAD `4dfdd9b7ce51b9ad04970da0b247294345f05ac7`; tree `e218b8f5dae9e7e133e8f803618ba6e14d355286`. Original report head `ce35bb7b979eca2f1720e9ad1e04088a762449d8`. This report-only successor is `task/MAC-HEAVY-1b`, based on fetched main `4dfdd9b7ce51b9ad04970da0b247294345f05ac7`; it does not import the old evidence commit.

Independent review is pending with the orchestrator. No engine, tool, test, fixture, default, golden or pin change is included.

| Stage | Wall s | Result / scope |
|---|---:|---|
| 0 | 203.082 | Host/freeze recorded; clean source |
| 1 | 3118.817 | Full ASan/UBSan FAIL: one minimap assertion; targeted isolation PASS; 0 sanitizer reports |
| 2 | 4931.089 | Whole-process FAIL: four operations, five native case failures; one distinct known assertion |
| 3 | 6662.897 | Three full serial Release PASS; identical status/prerequisite outcomes; no missing cases |
| 4 | 861.821 | 13 native selections: 11 passes, 2 transport failures; no sanitizer reports |
| 5 | 335.062 | Both full 100-command selections FAIL on 78 loader refusals each; no invariant/crash failures; 100 exact matched outputs |
| 6 | 234.243 | Atlas exit 0; 519 loaded / 0 refused; validated TSV |

All stages stayed within their boxes. There were no memory refusals, bad_alloc retries, stage stops or timeouts. Stage 1 LSan capability probe is an expected unsupported-host result, not an engine sanitizer finding.

## Evidence

- [Verbatim canonical receipts and primary log hashes](gate-receipts.md)
- [Findings and reproduction commands](findings.md)
- [Whole-binary order matrix](order-matrix.md)
- [Opt-in timings, peak RSS and actual assertion scope](opt-ins.md)
- [Compact corpus counts and byte comparison](corpus-summary.json)
- [Methods, adaptations and logging limitations](methods.md)
- [Acceptance scope; independent disposition pending](acceptance.md)
- [Retained raw artifacts and transformed summary SHA256s](sha256-index.json)

## Atlas

Privately retained TSV (`cache/mac-heavy-1/atlas_report.tsv`): 24912 scene rows, 3 figure-catalog rows and 1 colour-eligibility row; 24925 physical lines. Full-file SHA256 `d752e056722f9654b1d3f3d11d397af2b9537c01822e0ad57c25df7d9bec3a5c`.

Combined byte-column SHA256 `f9f1cc5d0ec61c1ff58890de66c5912c3a43beb9f8f5175b58ab69c386893af6`.

Scene columns: static_bytes, water_bytes, water_shadow_bytes, enhanced_extra_bytes, figures_used_bytes, pack_bytes, rect_bytes. Then figure columns bytes, player_colourable_bytes. UTF-8 decimal strings, tabs between cells, LF per data row; headers/comments/paths/non-byte columns excluded. See stage6-atlas-summary.json for section hashes and exact schema. Windows comparison NOT_RUN.

## Publication scope and paths

Only compact findings, methods, receipts and numerical summaries are committed here. No new build, suite, corpus walk or atlas measurement ran. Original failures stay failed. The original public branch and its worktree are preserved; this packet does not delete or rewrite history.

All 1,719 original cached/publication files (262,136,937 bytes) were copied into ignored `cache/mac-heavy-1/` and hash-checked before and after. `preservation-inventory.json` lists every local artifact; `original-publication/` holds byte-identical old publication snapshots. Ordered failure prefixes and per-case matrices remain there. Raw bytes are not changed by path sanitization, and raw hashes do not describe the new summaries.

Paths in summaries are repository-relative to the original tested worktree. `build/` refers to its retained original executables, not freshly built binaries in this branch. External roots use `$ANNO1503_GAME_DIR`, `$MAC_TOOLS`, `$APPLE_TOOLCHAIN`, `$PYTHON_BIN`, `$SYSTEM_BIN`, `$BASH` and `$UI_TABS_B_WORKTREE`; set them locally. Cached files are local review evidence, not publicly downloadable artifacts. The SHA256 index distinguishes byte-identical raw records from transformed publication files; its own hash is supplied with the handover.

The corpus comparison covered 100 matching scene/mode output tuples, including refusals. Per build/mode only 11 maps reached four invariant checkpoints; 39 refused at load. No invariant failure/crash occurred in those admitted runs. This is neither 50-map gameplay coverage nor a complete-state hash comparison. Windows corpus and atlas comparison remain NOT_RUN.

NOT_RUN: any new heavy verification; LeakSanitizer (unsupported host); Windows/History Edition execution; cross-platform corpus/atlas equality; excluded diagnostic and old-coordinator probes. Transport measurement bodies never started after their setup refusal. The leather scale request reached only 8/3 then 12/0 then 12/0 farms/tanneries, not the requested 8/8, 16/16, 32/32.
