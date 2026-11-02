# W4-WIN-CLOSE evidence index (Windows executor)

Text-only review index for the candidate published to `origin/task/W4-3-live-buildings`. It is written at
the C0 checkpoint and extended at the end of the packet (sections marked **C-final** are filled then).
Nothing here is an acceptance claim; the originals are in the git-ignored `cache/` of the Windows checkout
(`E:\anno1503-worktrees\anno1503-W4-3`) and in the evidence archive below. The orchestrator can read the
tables, hashes and failure extracts in this file and `MANIFEST-c0.sha256`; it cannot open the logs unless
the archive is handed over (see "Archive").

## Source identities (full hashes)

| Label | Full hash | Meaning |
|---|---|---|
| pin | `a29b07949201e3ea5bf30c800e392f337c1ec598` | published rendering baseline (round-ee documentation), code `093559ce63331770ae67b1abacacd8fc5fcc4155` |
| published candidate | `9ba01f14aa1b512292410f3add480ba4f70ff0e0` | pushed (ordinary, non-force; remote was an ancestor) to `origin/task/W4-3-live-buildings`, remote hash verified with `git ls-remote` |
| last `src/` change | `4ecea073d9aff9d5fc8b86e1fda1921f9c812f4a` | the play GPU-vs-CPU case also on the Radeon (only `src/render/tests/play_gpu_test.cpp`) |
| last tool commit | `fa94bf0b645b4083e9616b7f7eab904be41737e7` | `window_case`/capture: client area on the desktop. It is **older** than `f8bc0a6` in git order |
| first report commit | `f8bc0a61c00f66279b190c6c7245e1c1a00bbb90` | the report the prior audit read |
| report correction | `9ba01f14aa1b512292410f3add480ba4f70ff0e0` | the only commit after `f8bc0a6`; docs only (`docs/orchestration/REPORTS.md`) |
| workflow (main) | `b57c6f0d0c747ecc41755b438d9377df38d2b7ac` | the checkout the canonical `tools/test/run_selected.py` ran from (sha256 `f87fc882cfe4ff2205cda6ee5cab39a1e27d975aa0b63249c8250ef1c5e4927a`); origin/main has since moved to `e37a99c3269df83beb31cca016464b5411484e7d` (read only, not merged) |

Note on the packet's wording ("later report correction and last tool fix"): by git order the last tool
commit (`fa94bf0`) precedes `f8bc0a6`; the prior audit's `f8bc0a6` boundary is a commit boundary, and the
audit did not re-read `fa94bf0`'s capture-guard change. C4 reviews `fa94bf0`, `70817ed`, `4ecea07` and
`9ba01f1` together.

### Ordered commits since the pin (23)

The exact list with subjects is `cache/gates/c0-commits.txt` (in the archive); `git log --reverse a29b079..9ba01f1`
reproduces it. `git diff --stat a29b079..9ba01f1`: 49 files, 3627 insertions, 162 deletions. Changed paths:
`git diff --name-only a29b079..9ba01f1`.

## Host and toolchain (reused intake)

`cache/win-ee/intake.md`: Ryzen 7 9800X3D (8c/16t), 31.1 GiB RAM, RTX 4090 (driver 32.0.16.1714, display
2560x1440 @ 500 Hz) and AMD Radeon iGPU (1920x1080 controller default; Windows enumerates one monitor,
2560x1440), Windows 11 Pro 10.0.26300, CMake 4.4.4, Python 3.14.6, MSVC (VS 2022 generator), WSL Ubuntu with
GCC 13.3 / Clang 18.1.3. No 4K display. Host contention at the time (EA FC27 intermittently) is stated per run.

## Binary and tool hashes (build `build/win-a29b079`, Release)

| File | sha256 |
|---|---|
| `src/app/Release/anno1503.exe` | `58c60ad2c045c9587c9cc6df6d60115a35770e16a7d41acd1ba9ef1806af7e8d` |
| `src/app/Release/anno_app_tests.exe` | `3ddfd0667efc496864119c5f9df50feadd673abf85cbb45c39d1b13b7ae5fa37` |
| `src/app/Release/anno_app_alloc_tests.exe` | `be84d564169558178bbc9ad152816470ad8427c5feb3ab4aad84356ce8731148` |
| `src/app/Release/anno_app_input_tests.exe` | `de1d29d36ba954f0fd1a5e4f1bf5e61f45777be6e12704426fbf663521573a9a` |
| `src/render/Release/anno_render_gpu_tests.exe` | `2aa777815b5b29641b0ae1b7cf0b18c863fc4cc255ca0352859d363c15eaf00f` |
| `tools/bench/window_case.py` | `4d045dd3ebb20fdc27e7aa40887ef5227595fd2077466258e835c5935cf6104e` |
| `tools/bench/window_matrix.py` | `59aeb9a6a9547c5cd23f0850084febd471148ef113c1f07025aa5c89b9c3857f` |
| `tools/bench/window_table.py` | `af215277952bc6792016334c43eb77a661f7f6ddc9835d76d76760498aba69c9` |
| `tools/bench/dev_session_capture.py` | `263215af475988dced9754700070544130db1b72f815b40af3f92a54e6d1ed43` |
| `tools/check_unix_warnings.sh` | `41f3fe7b256c00fa96c8dfe9753fb82fa1800b9093dd9b8a3152f46848d3814a` |
| `tools/bench/window_table_test.sh` | `b2c53b600622e4645c62c1d2506bc5953f580d035268a7d9437d5bbe844b6980` |
| `tools/bench/picture_matrix_test.sh` | `ade99a5eaa2dab900aecd71a979b23fa27b1323177d31f54be5a534299c9d680` |

These are the C0-time hashes (the binaries are those of the last full build of the published candidate).

## Gate runs (immutable ids = file names under `cache/gates/`)

Counts are the canonical runner's `COUNTS:` line: selected / executed / failed / empty / skipped / incomplete.

| Run id | Source head | Result | Notes |
|---|---|---|---|
| `msvc-cpu-01` | `d8ed91d5678d284c5bebcb914d3c73099b4a3aa2` | 462/460/0/0/2/0 PASS | superseded |
| `msvc-cpu-02-PASS` | `98d93298df415bb1abd64520b1ebbf5b12fdc8bb` | 462/460/0/0/2/0 PASS | superseded; its `.out` names `msvc-cpu-02-final.log` (file kept under the shown name) |
| `msvc-cpu-03-final` | `381b3e369ebc21d02dda374a6b685ec2f7d7b88e` | 463/461/0/0/2/0 PASS | the retained MSVC CPU gate. `381b3e3..9ba01f1` changes under `src/` only `src/render/tests/play_gpu_test.cpp` (GPU executable) |
| `app-02-FAILED-reaper-starved` | pre-fix | FAILED | starved-reaper family, fixed by `52dab64`; kept |
| `app-r1-r4-01-FAILED-name-preflight` | pre-`e56e686` | FAILED | `;` in a case name; ctest name preflight |
| `rtx4090-gpu-01` / `-02-final` | earlier heads (`-02-final`: `381b3e3`) | 104 / 105 PASS | superseded |
| `rtx4090-gpu-03-final` | `4ecea073d9aff9d5fc8b86e1fda1921f9c812f4a` | 106/106/0/0/0/0 PASS | its `.out` ends with the old path `gates$name.log` (chain-script quoting slip; the log was renamed) |
| `radeon-gpu-01-FAILED-source-changed` | started at `1fd82b8eaa5774ee7f331d7d1099cb8047fa86ea` | FAILED (tested source changed) | kept |
| `radeon-gpu-02-final` | `381b3e369ebc21d02dda374a6b685ec2f7d7b88e` | 105/105 PASS | superseded by `-03-final` |
| `radeon-gpu-03-final` | `70817eda019bbf9f3443d56b09ebf0511ed737d3` | 106/106/0/0/0/0 PASS | the retained Radeon gate. `70817ed` contains `4ecea07`: the Radeon variant of the play GPU-vs-CPU case ran in it (`play: GPU matches the CPU path on one scene, on the Radeon iGPU`, delta 1); both GPU gates ran the same test executable (sha256 `2aa77781...`) |
| `unix-warnings-01-FAILED-sign-conversion-shadow`, `-02`, `-03-final`, `-pin-baseline` | various | see logs | `-03-final` carries no head line |
| `wsl-asan-app-01-FAILED-empty`, `-FAILED-git-worktree-path`, `wsl-asan-build-01-FAILED-sdl-no-x11` | - | infrastructure failures | kept |
| `wsl-asan-app-02-FAILED-timeout-heavy-case` | `98d93298df415bb1abd64520b1ebbf5b12fdc8bb` | 178/175/1/0/2/0 **FAIL** | see failure A |
| `wsl-asan-final-03` | `70817eda019bbf9f3443d56b09ebf0511ed737d3` (subset) | 80/80/0/0/0/0 PASS | 80 affected cases + both shell contracts; not a substitute for the whole-app gate |

## The two failures (originals preserved; both OPEN until C3 closes them)

### A. Whole-app WSL ASan/UBSan timeout (`cache/gates/wsl-asan-app-02-FAILED-timeout-heavy-case.{log,out,log.inventory.json}`)

- Case: `anno_app_tests.play: islands are the ones the sim placed` (ctest id 102 of 178), `Timeout` at the
  per-test limit of 3400 s; total ctest wall time 4336.07 s with `-j 4`. The sibling
  `play: lan5 islands are the ones the sim placed` passed in 109.44 s in the same run.
- Source/binary: head `98d93298df415bb1abd64520b1ebbf5b12fdc8bb` (clean tree, diff sha256 of the empty diff),
  `anno_app_tests` sha256 `cb626e3fb78395e861e3bd6086ab7e9099b3a1097c67aa34f233c07344ee7f63`,
  `anno_app_alloc_tests` `c9c5a9384bf7320c2f2c09c12d6b86c4d9198a14567b8a0e8f8d8f1d9e71ae47`, build dir
  `/home/altin/anno-build/w4-asan` (ASan+UBSan, viewer OFF).
- Command: `tools/test/run_selected.py --select '^anno_app_(alloc_)?tests\.' --require-test picture_matrix_contract --require-test window_table_contract --jobs 4 --timeout 3400`.
- Last observed progress: none inside the case (ctest keeps only the final status). Admission-coordinator
  state and host load at the time were not recorded. This is a gap, not a finding.

### B. One stalled-reclaimer assertion (`cache/win-ee/app-tests-full-03-first-failure-reclaimer-stall.log`)

- Case: `picture reclaim: a stalled process reclaimer ...` in `src/app/tests/scene_picture_recovery_test.cpp`.
  Failed assertions: line 692 `reclaimer.stats().waiting - before.waiting <= limits.max_reclaim_backlog + 1`,
  line 716 `reclaimer.stats().waiting == before.waiting`, line 717 `reclaimer.stats().bytes_waiting == before.bytes_waiting`.
  Whole-binary result `165 cases, 164 passed, 1 failed`; it did not reproduce in the next two full runs.
- The host was loaded (EA FC27); not an attribution. The earlier `pending=6 commit_skipped=100
  reclaim_completed=0` terminal reproduction with 24 busy processes has **no log** and cannot be recreated.

## Other retained evidence (under `cache/win-ee/`)

`build-*.log`, `configure-01.log`, `app-tests-full-0[123]*.log`, mutation logs `mut-*` (the two
`...INVALID-unmutated-binary` logs are invalid controls and are not evidence), `contention/` (own busy
processes, `contend.py`), `window/` (canaries, dev sessions with shots and replay logs, the 13-case matrix
raw traces and `matrix.table.md`, the `REFUSED-frame-overhang` capture attempt), `intake.md`.

Unavailable / deleted: the temporary baseline worktree `anno1503-W4-3-pin` and its build logs (removed with
`--force` at the end of the first packet); the 24-process terminal output; no original assets or binaries
are in git.

## Archive

`cache/archive/w4-win-close-evidence-c0.zip` (17 720 697 bytes, 499 files, sha256
`d54b7296fe346e313d952f978a5e850c08872fcdd366d12fc059f1fd38f6603e`; deflate) with `MANIFEST.sha256`
inside; the same manifest is committed here as `MANIFEST-c0.sha256`. It holds `cache/gates` and
`cache/win-ee` only (no source, no game files, no executables); the window screenshots show rendered
original art and the archive is therefore **not** to be published. No authorized shared artifact channel
for a binary archive was available to this session, so it is retained at the path above on the Windows PC
(Windows-local, not a shared artifact). The orchestrator has not independently verified anything that
depends on it.

## C-final (frozen source head `07a7505201066a0522804527fd3abb4e8022fe8e`; documentation and evidence follow it)

Final gates (canonical runner, Release; the `-06` runs; earlier attempts kept beside them in `cache/gates/`, with `first-04/` and
`first-05/` copies): `msvc-cpu-06-final` 216/214/0/0/2 opt-in/0 PASS; `rtx4090-gpu-06-final` 106/106 PASS; `radeon-gpu-06-final` 106/106
PASS; `unix-warnings-06-final` GCC 13.3 and Clang 18.1.3 `-Werror` 234 + 234 + 55 files clean; `wsl-asan-final-06` 86/86 PASS with
`picture_matrix_contract` and `window_table_contract` passed separately. Superseded: `msvc-cpu-04-final` (1 failure: the verdict contract
found a skip phrasing), `wsl-asan-final-05` (1 failure: a use-after-free in the new hold test, found by ASan). The WSL isolated diagnostic of
the original timeout: `cache/c1/wsl-diag/` (passed alone in about 5 100-5 400 s). GPU window cases: `cache/c2/final5/`; mutation records:
`cache/c1/mut/`, `cache/c2/mut/`, `cache/c4/mut-tools-2/`; matrix re-table: `cache/c4/matrix-retable.md`.

Hashes at the final head (build `build/win-a29b079`, Release):

| File | sha256 |
|---|---|
| `src/app/Release/anno1503.exe` | `3cd70cd1cd0b60471a2e264c7495ffc99c659dc132cb24a3ae8b0251bb39f8f0` |
| `src/app/Release/anno_app_tests.exe` | `41ad411cb6346366d8efb3293053250a8f53b700dca837c4959c754dde4eb21d` |
| `src/app/Release/anno_app_alloc_tests.exe` | `4e370b378716ad4aeb007488f0476f2b96134b1a161cce87e627b7a58eef2816` |
| `src/app/Release/anno_app_input_tests.exe` | `6d2eb66a557b08886131a2dc4acacf46dd3fdc50170927ee410346fbf2e8e2c4` |
| `src/render/Release/anno_render_gpu_tests.exe` | `2fc893c123514dc5c4eebe6abfc306b6ae121bbb44cc1664cc97a946df7236d0` |
| `tools/bench/window_case.py` | `00858147c7c7f9d003195afca895d327dd582fc997e6dae1e2b5700e248787ef` |
| `tools/bench/window_table.py` | `16f7045367a73de6a6ba2249c8f2a97615add86906149e426776af26da5858d1` |
| `tools/bench/dev_session_capture.py` | `4cde3e5440dcf6e4eb43f4f6ae4123dba04f2cbf319e20410c3ecd28c3264a55` |
| `tools/test/viewer_wiring.py` | `4f32a82ac0dff8f092a4bd02b6be7be311b07b0d6afe29a152d9446648fe6484` |
| `tools/test/window_state_case.py` | `eef8844094aa0233eda2d294f3550f056f763129685278912105b3b37f7993cc` |
| `tools/test/window_tools_contract_test.py` | `bf4b9a7b1460efda0f9b37459eeeab7a3fbdae4d638c580ddba4c8960a0d4dbe` |

Final archive (logs, traces, mutation records, inventories; no source, game files or executables): `cache/archive/w4-win-close-evidence-final.zip`,
24 459 568 bytes, 1114 files, sha256 `6e33790053fccf5f719b24efef36f88d64c7220df82a8256eb732db34a39638f`; manifest `MANIFEST-final.sha256` (committed
beside this file). Windows-local, not a shared artifact; it holds screenshots of rendered original art and must not be published.
