# Open checks for people (living list)

What only a person with the right machine can check. Claude Code keeps this list current; tick items off or note results next to them (date, machine, what you saw). Newest items first.

## Results

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
   Check:
   - stderr prints `GPU renderer: Apple M5 Pro (Metal)` and the title shows "GPU …" instead of "CPU";
   - the picture looks as with `--renderer cpu` (original water: identical within 1 per channel by the automated test);
   - `E` (enhanced water): now the wave simulation on the GPU, as in the Rust viewer. The frame rate should stay at the vsync rate (120 fps) instead of 47–60 fps on the CPU; the stderr line every 2 s shows `uploads / sprites / encode / present` instead of the CPU stages — please paste a few lines of it;
   - waves move and break on the coasts; pause (`Space`) freezes them;
   - resize the window, minimise and restore: the picture comes back, no error in the console;
   - `--renderer cpu` still works; `--renderer gpu` fails with a message only when no GPU is usable.
   Report anything that looks different from the Rust viewer (`cargo run --release -p anno1503 -- --view game/app/Scenes/0200Single/Single_01.sze`).
3. **Decision: a C++ preview for the tester.** CI now builds the C++ viewer for Windows and Linux. To get it to the tester, Claude can either (a) build a Linux package here in Docker and leave Windows for later, or (b) add a CI step that uploads the Windows and Linux builds. Option (b) produces GitHub artifacts, which need a GitHub login to download; a public GitHub Release is the alternative. Publishing anything is your call. Please say (a), (b), or "Release".
4. ~~Decision: GPU implementation~~ — wgpu-native (2026-10-18).

## For the tester (Windows / Linux PC)

- Nothing new to run yet: there is no packaged C++ build. The latest preview is still `share/anno1503-preview-2026-10-17g.zip` (Rust build). A C++ preview follows once CI is green on Windows and Linux.

## Later, needs an x86 Windows machine or Wine on x86 Linux

- Fixtures F1–F9 in `docs/re/tick-loop.md` (game clock, speeds, PRNG cursor, multiplayer sync) against the original game. F6 (PRNG values) is already confirmed statically (the DLL's table bytes and the draw function), not yet by running the game.
- Savegame corpus (PLAN M2.1).
