# W4-WIN-FINAL: commands, identities and the acceptance matrix

Text-only. `INDEX.md` lists every extract with its original and the original's sha256; the originals are
git-ignored under `cache/` of the Windows checkout. Hashes below named *blob* are git blob ids; hashes named *disk*
are sha256 of the Windows working-tree file (CRLF for text files with `core.autocrlf=true`).

## Identities

- Branch `task/W4-3-live-buildings`. Code frozen at `8367dc0e02ba0d1a1e776ba4eaee7e55a5ef5db8`; `37da91b901a0ead4ee4c45205a909e9e4b1eae8c`
  adds only a `viewer_wiring.py` UTF-8 stdout fix; every gate below records `37da91b` as the tested head (clean tree, empty diff).
  Commits after it are documentation and this evidence.
- Workflow: `tools/test/run_selected.py` from `E:\anno1503` at `b57c6f0d0c747ecc41755b438d9377df38d2b7ac`, `--source-root`/`--build-dir` explicit.
- Windows build `build/win-a29b079` (Release, MSVC, cmake cache sha256 `534a743e...`): `anno_app_tests.exe` `21fcfd77...`,
  `anno_app_alloc_tests.exe` `4e370b37...`, `anno_app_input_tests.exe` `78430b0a...` (disk sha256, prefixes).
- WSL build `~/anno-build/w4-asan-viewer` (Ubuntu, g++-13, Debug, `ANNO_SANITIZE=ON` ASan+UBSan, `ANNO_BUILD_VIEWER=ON`,
  `ANNO_BUILD_GPU=ON`, `SDL_UNIX_CONSOLE_BUILD=ON`; configure command in `docs/testing/app-sanitizer-lanes.md`):
  `anno1503` `03492596...`, `anno_app_tests` `3e257555...`, `anno_app_input_tests` `f1dd0464...`, `anno_app_alloc_tests` `beac356d...`
  (`extracts/gate-wsl-final-f.txt` has the full values).
- Tools (blob / disk-sha256 prefix): `viewer_session.py` 57d15085 / 51935f73; `viewer_wiring.py` bf47418d / d2e58166;
  `dev_session_capture.py` 3620a4eb / 58559b23; `viewer_wiring_contract_test.py` 06c7346d / 2a9bd2d0;
  `window_tools_contract_test.py` 2b6eeb1a / 739ee65e; `check_unix_warnings.sh` a75cc798 / be116cf8;
  `viewer.cpp` 9ff70b8b / 4d2921f2; `scene_picture_recovery_test.cpp` e508fbc5 / a40de7fc.
- Host: Ryzen 7 9800X3D, 31 GiB, Windows 11 (10.0.26300), RTX 4090 (Vulkan), WSL Ubuntu (15.5 GiB). Budget coordinator and `RESOURCE_LOCK gpu` on.

## Commands (as run)

```
# native gate (Windows): tools/test/run_selected.py --source-root E:\anno1503-worktrees\anno1503-W4-3 --build-dir ...\build\win-a29b079 --config Release
#   --select '^(anno_app_tests|anno_app_alloc_tests|anno_app_input_tests)\.|^viewer_wiring_|^window_tools_contract$|^test_names_are_selectable$|^test_verdict_contract$'
#   --require-test viewer_wiring_contract --require-test window_tools_contract --require-test viewer_wiring_cpu_delivery_failures
#   --require-test viewer_wiring_r1_held_publication --require-test viewer_wiring_r4_occupancy --require-test test_verdict_contract --jobs 6 --timeout 3300
# WSL ordinary lane: ... --build-dir ~/anno-build/w4-asan-viewer --exclude-label heavy (same --select, plus window_table_contract required; picture_matrix_contract added by the runner)
# WSL corpus lane:   ... --select '^anno_app_tests\.play: (islands are the ones the sim placed|tick 0 matches the viewer except objects the sim did not place)$' --label heavy --jobs 4
# warnings: JOBS=8 bash tools/check_unix_warnings.sh <_deps>   (GCC 13.3 and Clang 18.1.3, -O3 -Werror, now also GCC without ANNO_VIEWER_GPU)
```

## Results (selected / executed / failed / empty / skipped / incomplete)

| Gate | Result |
|---|---|
| Native MSVC, `msvc-cpu-f2-final` (`extracts/gate-msvc-cpu-f2-final.out`) | 220 / 218 / 0 / 0 / 2 opt-in probes / 0, PASS; includes both corpus cases natively and all six required contracts |
| WSL warnings, `unix-warnings-f-final` | GCC and Clang, viewer and GPU on, and without GPU: clean |
| WSL ordinary lane, viewer-enabled ASan+UBSan (`gate-wsl-asan-viewer-ordinary.out`) | 217 / 215 / 0 / 0 / 2 opt-in / 0, PASS, 0 sanitizer lines; `viewer_wiring_*` run against the instrumented `anno1503` |
| WSL corpus lane, same build, serial (`gate-wsl-asan-viewer-corpus.out`, `gate-corpus-progress-extract.txt`) | 2 / 2 / 0 / 0 / 0 / 0, PASS, 0 sanitizer lines: `play: islands are the ones the sim placed` 5 303.65 s (5 492/5 492 assertions, 50 scenarios, progress 106.9 s at the first, 5 303.0 s at the last), `play: tick 0 matches...` 2 374.47 s (3 478/3 478); both under `TIMEOUT 8000`; host load about 1 throughout (`gate-corpus-host-samples.txt`) |
| RTX 4090 real-window canary (`boundary_stages`, `boundary_exit_in_hold`, `--gpu`) | PASS, both session logs VALID under the final validator (`cache/f/canary`) |
| 106-case RTX and Radeon gates | reused from `07a7505` (`rtx4090-gpu-06-final`, `radeon-gpu-06-final`): `git diff 07a7505..HEAD` is empty for `src/render`, `src/sim`, `src/formats`, `src/assets`, and for the sources of `anno_app_core` (the only app code `anno_render_gpu_tests` links); the changed app files are `viewer.cpp` (executable only), `dev_script.*` (`anno_app_sdl`), tests and CMake. The rebuilt test executable's hash differs from the recorded one (MSVC relinks are not reproducible); no claim is made from the hash |

Superseded and kept: `msvc-cpu-f-attempt1-FAILED-workflow-utf8` (`extracts/gate-msvc-cpu-f-attempt1-FAILED.out`: `viewer_wiring.py`
printed the window title's em dash in the Windows code page and the gate read the log back as UTF-8; fixed in `37da91b`).

## Acceptance against the numbered requirements

- **F1.1** reproduction kept: `f1-false-pass-before.txt` (zero-exit fixture, no session: PASS) / `f1-false-pass-after-fix.txt` (FAIL, no `script.log`).
- **F1.2** execution validator: present, non-empty, every non-wait step in order up to terminal `quit`, matches the script, no FAIL, plus `session.txt`;
  nonzero child and watchdog preserved (the watchdog now ends promptly). **F1.3** contracts with a positive control first, 14 and 19 tests, registered
  and required (`f1-*-green.log`); mutations over the whole files (`f1-mutation-*`): each fails only its intended tests. **F1.4** capture verdict: exit 4,
  child status preserved and logged apart, invalid grabs saved as `*.invalid`, `--require-captures`. **F1.5** native controls (zero-exit success, minimised
  and off-desktop refused, child-9 control, short/silent/unfinished) plus mocked size-mismatch/watchdog verdict cases, labelled. **F1.6** retained sessions revalidated
  (`f1-revalidation-*.txt`); superseded sessions are not valid against the current scripts because the scripts changed, not for missing execution.
- **F2.1-F2.3** `Viewer::draw_cpu` checks the three SDL results, skips later stages, keeps the change `NotPresented`, says why on stderr, retries at the 100 ms
  back-off; the seam hands the real call a null texture/renderer. Case `cpu_delivery_failures` (`f2-cpu-delivery-session-script.log`, `f2-cpu-delivery-stderr-diagnostics.txt`);
  mutants `f2-force-success`, `f2-drop-real-returns`, `f2-drop-present-return` (stay `Visible`: the case fails at the first discriminating wait), `f2-no-backoff`
  (152 frames in a second against at most 30); `f2-ignore-results` did not compile (kept, superseded). A submit-only mutant was not run (update and present
  independently, submit by symmetry). **F2.4** the viewer-enabled ASan+UBSan build (configuration above) instruments `viewer.cpp`; **F2.5** R1, R4 and the CPU failure
  cases ran in it (ordinary lane) and natively.
- **F3** the stalled-reclaimer body is factored (`stalled_reclaimer_case`), the settling call is the one the foreign-work regression reaches through the same path;
  `f3-mutation-settle-call-removed` and `settle-returns-at-once` fail the intended final equalities with the counters logged; waits use a baseline predicate on
  `waiting` and `in_flight`; no sleep decides an order; 5 + 5 focused repeats (`f3-focused-repeats-final.log`, with command lines and binary hash). The historical
  occurrence stays an uncertainty (no counters were logged). The single-foreign-scene pin of the `in_flight` leg of settling was judged non-blocking and is not added.
- **F4** the two full-corpus cases are `heavy`, with `TIMEOUT 8000` in sanitizer builds only, all scenarios and assertions kept; inventory metadata in
  `inventory-*.tsv` (each name once, label `heavy`, `PROCESSORS 4`, `TIMEOUT 8000` in the WSL build, none in MSVC); lanes reconcile: ordinary (217 selected, heavy excluded)
  plus corpus (2) = the whole required app/input/alloc/viewer inventory.

## Limits

Headless CPU/SDL-dummy sessions say nothing about the GPU path actually rendering or about a display; the sanitizer build cannot create a GPU device. Native 4K, physical
display latency, the Radeon in a real window, scale, terrain/height, roads, UI and OQ-525 are unchanged named follow-ups. Mutation runs of F1-F3 were made at the commits named in
`INDEX.md` extracts' diffs; the F3 mutants and focused repeats were rerun at the frozen code head. `capture()` run with a real Windows GDI grab in a lock-screen session would
refuse, not pass.
