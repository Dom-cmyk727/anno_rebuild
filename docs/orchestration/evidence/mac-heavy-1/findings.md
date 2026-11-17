# Findings — READY FOR REVIEW

All results concern the frozen source named in README. No engine/test repair is included.

| ID | Kind | Test / scene | First evidence | Reproducibility |
|---|---|---|---|---|
| MH-001 | sanitizer configuration / assertion only | viewer_wiring_ui_minimap_controls | `FAIL line 88 (expect): view.tile_w is '42', expected == 84` | not established under full context; isolated reproduction passed |
| MH-002 | order (known) | scene reclaim: the window's release of the last generation is destroyed on the reclaimer | `src/render/tests/scene_reclaim_test.cpp:57: ERROR: CHECK( render::SceneReclaimer::shared().stats().completed == before ) is NOT correct!` | Observed in recorded order(s); exact-command repeat NOT_RUN. Timing is not excluded as a cause. |
| MH-003 | order | publish: a frame-held intermediate generation and shutdown with work in flight are released on the reclaimer | `src/app/tests/scene_picture_publish_test.cpp:783: FATAL ERROR: REQUIRE( eventually([&] { const auto now = render::payload_counts(); return now.of(render::PayloadKind::Incremental) == before.of(render::PayloadKind::Incremental) && now.of(render::PayloadKind::Generation) == before.of(render::PayloadKind::Generation) && reclaimer.stats().in_flight == 0; }) ) is NOT correct!` | Observed in recorded order(s); exact-command repeat NOT_RUN. Timing is not excluded as a cause. |
| MH-004 | order | ownership: a frame-held generation outlives the bridge and is destroyed on the reclaimer | `src/app/tests/scene_picture_ownership_test.cpp:517: FATAL ERROR: REQUIRE( eventually([&] { const auto now = render::payload_counts(); return now.of(render::PayloadKind::Incremental) == before.of(render::PayloadKind::Incremental) && now.of(render::PayloadKind::Generation) == before.of(render::PayloadKind::Generation) && reclaimer.stats().in_flight == 0; }) ) is NOT correct!` | Observed in recorded order(s); exact-command repeat NOT_RUN. Timing is not excluded as a cause. |
| MH-005 | order | picture recovery: a result of an older version is never published after a newer change | `src/app/tests/scene_picture_recovery_test.cpp:402: ERROR: CHECK( picture.commit(frames) == 1 ) is NOT correct!` | Observed in recorded order(s); exact-command repeat NOT_RUN. Timing is not excluded as a cause. |
| MH-006 | opt-in failure | anno_sim_tests.transport search statistics (ANNO_SIM_TRANSPORT_STATS) | `scenario_test.cpp:1042: FATAL ERROR: REQUIRE( w.ok() ) is NOT correct!` | yes, Release and ASan; workload not reached in either |

## Commands and preserved context

Commands below require the environment in methods.md and their recorded cwd. Full preceding case lists are in cache/mac-heavy-1/original-publication/findings.json and cache/mac-heavy-1/original-publication/stage2-matrix.json.

### MH-001

The full ASan selection failed. One isolated ASan jobs-1 probe passed; all three Release repetitions passed this case. No sanitizer report accompanied the assertion. Full-load reproducibility and root cause remain unproved.

```json
["python3", "-I", "tools/test/run_selected.py", "--build-dir", "build/asan", "--select", ".", "--asset-policy", "required", "--allow-skip", "gpu", "--allow-skip", "other", "--jobs", "8", "--timeout", "10800", "--log", "cache/mac-heavy-1/stage1-full-01.raw.log"]
```

Log: `cache/mac-heavy-1/stage1-full-01.environment.log`; SHA256 `53f5ed05d04a4085b9a9e78c10f5c7c7b1e2868bac3bf0cb3b236d22b2ea4958`.

### MH-002

Context: `default`; cwd `build/release/src/render`.

```json
["build/release/src/render/anno_render_tests"]
```

Log: `cache/mac-heavy-1/stage2-children/whole_03_anno_render_tests_default/whole.log`; SHA256 `c3f0487a075a67816fc39239b94e25cb16f934bd116439b96f27a8ea5ea667ef`.

205 cases preceded it; complete ordered prefix retained in cache/mac-heavy-1/original-publication/findings.json.

### MH-003

Context: `default`; cwd `build/release/src/app`.

```json
["build/release/src/app/anno_app_tests"]
```

Log: `cache/mac-heavy-1/stage2-children/whole_05_anno_app_tests_default/whole.log`; SHA256 `be5b75b8dad01aee74b9e540d4627793ef9071266403a04286c914c5075409d9`.

237 cases preceded it; complete ordered prefix retained in cache/mac-heavy-1/original-publication/findings.json.

Context: `1`; cwd `build/release/src/app`.

```json
["build/release/src/app/anno_app_tests", "--order-by=rand", "--rand-seed=1", "--test-suite-exclude=heavy"]
```

Log: `cache/mac-heavy-1/stage2-children/whole_05_anno_app_tests_1/whole.log`; SHA256 `5c0f4eed8ff5e843989bdb6ec0944d14b578074ddd7bc7cd2332a795ffdb9a6f`.

61 cases preceded it; complete ordered prefix retained in cache/mac-heavy-1/original-publication/findings.json.

### MH-004

Context: `1`; cwd `build/release/src/app`.

```json
["build/release/src/app/anno_app_tests", "--order-by=rand", "--rand-seed=1", "--test-suite-exclude=heavy"]
```

Log: `cache/mac-heavy-1/stage2-children/whole_05_anno_app_tests_1/whole.log`; SHA256 `5c0f4eed8ff5e843989bdb6ec0944d14b578074ddd7bc7cd2332a795ffdb9a6f`.

37 cases preceded it; complete ordered prefix retained in cache/mac-heavy-1/original-publication/findings.json.

### MH-005

Context: `2`; cwd `build/release/src/app`.

```json
["build/release/src/app/anno_app_tests", "--order-by=rand", "--rand-seed=2", "--test-suite-exclude=heavy"]
```

Log: `cache/mac-heavy-1/stage2-children/whole_05_anno_app_tests_2/whole.log`; SHA256 `4b7717d6470dfe2ca2bf73351d3a85b3ad12ec259443af4554d9e9c79148ddbd`.

68 cases preceded it; complete ordered prefix retained in cache/mac-heavy-1/original-publication/findings.json.

### MH-006

Context: `release`; cwd `.`.

```json
["$SYSTEM_BIN/time", "-l", "python3", "-I", "tools/test/run_selected.py", "--build-dir", "build/release", "--select", "^anno_sim_tests\\.transport\\ search\\ statistics\\ \\(ANNO_SIM_TRANSPORT_STATS\\)$", "--asset-policy", "required", "--allow-skip", "gpu", "--allow-skip", "other", "--jobs", "1", "--timeout", "14400", "--log", "cache/mac-heavy-1/stage4-release-transport.raw.log", "--skip-auto-contract"]
```

Log: `cache/mac-heavy-1/stage4-release-transport.raw.log`; SHA256 `2c17771ee0473b1d8defb2a61bfd48f49cd4751a623ab7cf19fa3bb9456318a6`.

Context: `asan`; cwd `.`.

```json
["$SYSTEM_BIN/time", "-l", "python3", "-I", "tools/test/run_selected.py", "--build-dir", "build/asan", "--select", "^anno_sim_tests\\.transport\\ search\\ statistics\\ \\(ANNO_SIM_TRANSPORT_STATS\\)$", "--asset-policy", "required", "--allow-skip", "gpu", "--allow-skip", "other", "--jobs", "1", "--timeout", "14400", "--log", "cache/mac-heavy-1/stage4-asan-transport.raw.log", "--skip-auto-contract"]
```

Log: `cache/mac-heavy-1/stage4-asan-transport.raw.log`; SHA256 `e544b357e2038a25f0df82179c02a5cba79a08f0c3e465489897ffffcb785f61`.

## Non-failing coverage and diagnostic limits

- Leather scale requested K=8/16/32 but built 8/3, 12/0 and 12/0 farms/tanneries respectively; larger requested sizes were not achieved. Ledger assertions passed for actual workloads.
- The GPU cost-source MESSAGE at gpu_test.cpp:2409 prints an address, so its human-readable source label is unavailable. Timing cases executed; this is not a failed assertion.
- Stage 5 refused 39 maps per mode before checkpoints. These are capability limits, not invariant passes; every refusal is listed in corpus.md and the privately retained native outputs.
- LeakSanitizer unsupported; Windows output/atlas comparison NOT_RUN.
