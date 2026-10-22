# Open checks for people (living list)

What only a person with the right machine can check. Claude Code keeps this list current; tick items off or note results next to them (date, machine, what you saw). Newest items first.

## Open

- **River flow, first look (2026-10-19, stage S5; enhanced water `E`, key `R` toggles flow/legacy):** the flow shading is drawn by both paths since stage S6 (2026-10-19): run `anno1503 --view game/app/Scenes/Islands/Dschungel/D_256x256_0045_5B.scp` (the GPU renderer, zoom group 1, press `E`; `R` switches to the old shimmer; stderr says which river tier runs, `ANNO_RIVER_QUALITY=low|medium|high` overrides it) or `--renderer cpu`. At the default strength the motion is as quiet as the old shimmer at group 0 and quieter at groups 1 and 2 (OQ-083); please compare with `R` and say whether the GPU picture also looks right on your monitor, and whether you want rivers on the integrated GPU to fall back to the old shimmer (OQ-085); `anno1503 ... --water enhanced --river-gain 4 --renderer cpu --png out.png --time 250 --at 152,97` (or `river_qa --rect ... --shade DIR`) shows it clearly. Frame sequences, light images and sheets: `C:\Users\altin\Pictures\anno-rivers\S5` (`contact_sheet_all.png`, `compare_before_after.png` with the shading before and after the review, `sheets\`, `gif\`, `before_review\`). Frame sequences, sheets and before/after GPU views of S6: `C:\Users\altin\Pictures\anno-rivers\S6`. Decisions: OQ-083 (strength), OQ-067 (rivers without sea), OQ-065/066 (speeds), OQ-085 to OQ-090 (tiers, adapter, CI, textures). Please judge whether the rivers are *visibly flowing* at the default (the review measured that they hardly are, OQ-083) and compare with the gain 1 / 3 / 4 sheets in `C:\Users\altin\Pictures\anno-rivers\S6\decision`; the three sea-less pieces of this island (OQ-067) are best looked at with `river_qa --rect ... --shade DIR --sea-less-scale 0`.
- **River gaps (2026-10-19, owner report; enhanced water `E`):** open the jungle island `Islands/Dschungel/D_256x256_0045_5B.scp` (the island of your screenshot `river_gaps.png`; `anno1503 --view <game>/Scenes/Islands/Dschungel/D_256x256_0045_5B.scp --group 1`) with `E` and look at the river network in the middle and the delta in the south-east at zoom groups 0, 1 and 2, then at a Nord or Tundra island with rivers (`Islands/Nord/N_256x256_0006_2B.scp`). Expected: every river stretch carries water from source to mouth, the water fills the painted brown bed up to its soft edge (the screenshot's brown strips along the banks and the necks where two pieces meet are gone), nothing floats outside the bed. Measured by `tools/river_qa.py` (docs/render/iso.md, "River gaps"); what only a person can judge: whether the banks look natural and whether the rivers are now too wide or too uniform (tuning constants `kBedEdge`, `kBedDepth`, `kRiverBankNoise`, listed as decision-needed there). Report anything that still looks wrong with a screenshot and the scene.
- **M3 stage S1, CI matrix (2026-10-19):** the S1 commits have been built and run on MSVC and, in WSL, on GCC 13.3 (also under ASan/UBSan without recovery); Clang and the CI jobs have not run. After pushing: `linux-gcc-release`, `linux-clang-asan` (the `asan` preset, including the CTest case `anno_sim_tests_ubsan_probe_is_caught_by_the_asan_gate`) and `macos-clang` must be green; if `linux-clang-asan` reports a finding in a suite other than `anno_sim_tests`, that suite was not yet built with `-fno-sanitize-recover=all` and the finding is old. Record the result in `docs/sim/state.md` §15 S1; stage S2 starts after that.

## Results

- 2026-10-20, owner, Windows PC (RTX 4090), `main` at 3c8876b (stand-in chunks after a zoom step, river defaults gain 4 / median speed / gap patch 8), canyon island `D_256x256_0045_5B`, enhanced water: "MUCH smoother and overall better now"; zooming with the wheel alone is "really smooth"; the rivers "look perfect now". Artifacts remain only when panning while zooming (new area without a built chunk in any group). Follow-up: T1d on the task board.

- 2026-10-18, owner, Windows PC (Ryzen 7 9800X3D, 16 threads; RTX 4090, driver 617.14; Windows 11 Pro 26H2), C++ build `msvc` preset with the game files, `ANNO_REQUIRE_GPU=1`, `ctest -j 16`, `ANNO_TEST_THREADS=16`:
  - Build clean (warnings as errors). First run without game files in `game/app`: 37 game-file tests failed with "path not found" or follow-on errors (`bad variant access`), not a code fault.
  - With the game files: **all 21 GPU tests pass on the RTX 4090 (Vulkan)** — the first run on a real Windows GPU.
  - Two Windows-only test faults, fixed: `tools/agree.py` exceeded Windows' 32,767-character command line on the corpus cases (now batched; 38 of 38 agree); `Single_16` enhanced scene digest differs on MSVC's UCRT (`exp`/`pow` rounding; Rust gives the same line there), now a per-libm table (`docs/render/iso.md`, "Fidelity"). The wave solver's UCRT digests equal Rust's bit for bit (third table). After the fixes: 115 of 115 pass.
  - Seen in the hardware report: the 2560 × 1440 @ 500 Hz monitor is attached to the CPU's integrated AMD graphics, the RTX 4090 drives the 1080p one. The tests render offscreen (unaffected); for the viewer, connect the main monitor to the 4090.

- 2026-10-18, owner, Mac (M5 Pro), C++ viewer on the GPU (`Single_01.sze`, groups 0 and 2, resizing):
  - Starts on the GPU (`GPU renderer: Apple M5 Pro (Metal)`), 120 fps at 2560 × 1600 with 0.2–0.4 ms of CPU time per frame; worst frames 26–44 ms only in the second after a zoom change (chunk uploads).
  - Lines with 8.8–70 fps: windows in which hardly anything was drawn (paused or in the background), not slow frames (their frame times are 0.2–0.4 ms).
  - Found: ships look like pictures on the water (hard hull edge) → ships in the enhanced water (`docs/render/iso.md`), item 4 below.

- 2026-10-18, owner, Mac (M5 Pro), C++ viewer `Single_01.sze`:
  - Original water: 97–120 fps at 2560 × 1600 (vsync cap 120), 3.8–4.4 ms per frame; chunks arrive in the background without hitches.
  - Enhanced water (`E`): the "sea" stage takes 12–17 ms, so 47–60 fps (groups 0 and 2). Expected: this is the CPU path; the GPU port moves it to the graphics card.
  - Minimising and restoring works: the 1.5 fps line was taken while minimised.
  - GPU decision: wgpu-native.

## For the owner (Mac), 2026-10-18

1. **CI status:** nothing to do unless it turns red. Since commit `78065e4` (2026-10-18) every job is green for the first time: C++ on Linux (GCC; Clang + ASan/UBSan), Windows (MSVC) and macOS; formatting; the Rust jobs. Before that, the C++ Linux and Windows jobs had never passed. Claude now checks CI itself through the public GitHub API. When a build or test fails, CI publishes the first errors as annotations, which anyone can read.
2. **C++ viewer on the GPU (Mac)** — new 2026-10-18; the CPU check of the last round is done (results above):
   ```
   cd ~/Dev/anno1503
   git pull
   cmake --preset release && cmake --build --preset release
   build/release/src/app/anno1503 --view game/app/Scenes/0200Single/Single_01.sze
   ```
   Claude ran it briefly (2026-10-18): it starts on the GPU and holds 120 fps at 2560 × 1600 (0.4 ms per frame). The first version quit at once on macOS ("the window surface failed (status 196609)": the window was not visible yet); fixed. If you see that message, report it.
   Check:
   - stderr prints `GPU renderer: Apple M5 Pro (Metal)` and the title shows "GPU …" instead of "CPU";
   - the picture looks as with `--renderer cpu` (original water: identical within 1 per channel by the automated test);
   - `E` (enhanced water): now the wave simulation on the GPU, as in the Rust viewer. The frame rate should stay at the vsync rate (120 fps) instead of 47–60 fps on the CPU; the stderr line every 2 s shows `uploads / sprites / encode / present` instead of the CPU stages — please paste a few lines of it;
   - waves move and break on the coasts; pause (`Space`) freezes them;
   - resize the window, minimise and restore: the picture comes back, no error in the console;
   - `--renderer cpu` still works; `--renderer gpu` fails with a message only when no GPU is usable.
   Report anything that looks different from the Rust viewer (`cargo run --release -p anno1503 -- --view game/app/Scenes/0200Single/Single_01.sze`).
3. ~~Decision: a C++ preview for the tester~~ — the zip in `share/` as before (owner, 2026-10-18): now `share/anno1503-preview-2026-10-22a.zip`, built from the C++ code by `packaging/package-preview.sh` (on the Windows PC since 2026-10-22: `packaging/package-preview-windows-host.sh`, both builds in Docker).
4. **Ships in the water** (new 2026-10-18, enhanced water): zoom in fully on a ship with `E` on. The hull bottom should sink into the water with broken foam at the waterline and rise and fall with the waves; with `E` off the ship looks as before (as the original). Close-ups were sent in the session; tell Claude what looks wrong (too much or too little sinking, foam too bright, bobbing too strong).
5. ~~Decision: GPU implementation~~ — wgpu-native (2026-10-18).
6. **Rivers into the sea, water against rocks** (new 2026-10-18, enhanced water `E`, after the tester's feedback "Übergang von Meer zu Flüssen sieht abgehakt und unnatürlich aus" and the owner's note about rocks floating on the water): look at river mouths and rocks at rivers and coasts (e.g. `Single_01.sze`, zoom 0, the river mouths of the big desert island in the middle-left). Expected: the river runs through its falls into the sea as one surface (shapes as the original's, no dark patches, no foam on rivers), rock bases sink into the water (foam only at the sea). Report what still looks wrong, with a screenshot.

## For the tester (Windows / Linux PC)

- ~~GPU tests on a Windows PC~~ — done 2026-10-18 on the owner's RTX 4090 (results above). Still open: **GPU tests on a Linux PC with a graphics card**, and a Windows PC with an AMD or Intel GPU (DirectX 12/Vulkan). Original instructions: So far they have run on the dev Mac (Metal) and on Linux with Mesa's software renderer (llvmpipe, Vulkan and GL; CI's Linux runners use it), never on a real Windows or Linux GPU (Vulkan, DirectX 12). With the game files, a C++ build (`cmake --preset release && cmake --build --preset release`, on Windows the `msvc` preset) and:
  ```
  ANNO_REQUIRE_GPU=1 ANNO1503_GAME_DIR=<game folder> ctest --preset release -j -R gpu
  ```
  (Windows PowerShell: `$env:ANNO_REQUIRE_GPU=1; $env:ANNO1503_GAME_DIR="<game folder>"; $env:ANNO_TEST_THREADS=$env:NUMBER_OF_PROCESSORS; ctest --preset msvc -j $env:NUMBER_OF_PROCESSORS -R gpu`; all hardware threads.) `ANNO_REQUIRE_GPU` makes a missing GPU a failure instead of a skip. Report the summary line and any failed test's output, and the GPU named in the first lines.
- **New preview `share/anno1503-preview-2026-10-22a.zip`** (2026-10-22, replaces 2026-10-18b; steps in the README, section "Test the preview"; what is new is in the package's `README.txt`). Please re-test: (1) **zoom**: the wheel now zooms continuously and much further out (`Home` / `0` back); turn it in and out fast, also while dragging the map, and report empty or flickering patches, areas that stay blurry, jumps, and any `frame hitch:` lines from the console (your Linux log of 2026-10-18 showed 60–103 ms frames after chunk bursts; fixed 2026-10-19); (2) **rivers with `E`**: they flow now (key `R` switches to the old look; compare `--river-gain 2` and `8` with the default 4) and should have no dry stretches; the **river-mouth re-test is still open**: look at the same mouths as in your feedback below again, now with the flow, and say whether the transition looks natural (the canyon island `Scenes/Islands/Dschungel/D_256x256_0045_5B.scp` has a long river); (3) whether it starts at all (exact error text if not), the GPU named in the window title and the first console line `GPU renderer: … (river shading: …)`, and `benchmark-results.md`; (4) with two GPUs, the list from `bench_frame --list-adapters` and whether `ANNO_GPU_ADAPTER=<number>` switches to the other one. Linux needs glibc 2.35 or newer (Ubuntu 22.04 or later). Earlier package, 2026-10-18b (2026-10-18): with `E`, river mouths (after your feedback "Übergang von Meer zu Flüssen sieht abgehakt und unnatürlich aus") and rocks in the water. Please look at the same places again and say whether the transition now looks natural. Earlier the same day: 2026-10-18a, the first build from the C++ code: steps and what to report are in the README (section "Test the preview"). New: ships sit in the water with `E`. Please also report whether it starts at all (exact error text if not), the GPU named in the window title, and `benchmark-results.md`. Linux needs glibc 2.35 or newer (Ubuntu 22.04 or later).

## Later, needs an x86 Windows machine or Wine on x86 Linux

**2026-10-21:** possible now on the owner's Windows 11 PC: task T15 (`docs/orchestration/TASKS.md`) runs these with a scripted harness against a copy of the original.

- Fixtures F1–F9 in `docs/re/tick-loop.md` (game clock, speeds, PRNG cursor, multiplayer sync) against the original game. F6 (PRNG values) is already confirmed statically (the DLL's table bytes and the draw function), not yet by running the game.
- Savegame corpus (PLAN M2.1).
