# Open checks for people (living list)

What only a person with the right machine can check. Claude Code keeps this list current; tick items off or note results next to them (date, machine, what you saw). Newest items first.

## For the owner (Mac), 2026-10-18

1. **CI status:** nothing to do unless it turns red. Since commit `78065e4` (2026-10-18) every job is green for the first time: C++ on Linux (GCC; Clang + ASan/UBSan), Windows (MSVC) and macOS; formatting; the Rust jobs. Before that, the C++ Linux and Windows jobs had never passed. Claude now checks CI itself through the public GitHub API. When a build or test fails, CI publishes the first errors as annotations, which anyone can read.
2. **C++ viewer on the Mac** (first look; the Rust viewer stays the reference until the C++ one has the GPU path):
   ```
   cd ~/Dev/anno1503
   cmake --preset release && cmake --build --preset release
   build/release/src/app/anno1503 --view game/app/Scenes/0200Single/Single_01.sze
   ```
   For comparison, the Rust viewer: `cargo run --release -p anno1503 -- --view game/app/Scenes/0200Single/Single_01.sze --renderer cpu`.
   Check:
   - islands, sea animation, ships and soldiers look the same as in the Rust viewer;
   - drag, arrow keys or WASD pan; wheel and `+`/`-` zoom, and the point under the cursor stays put;
   - `Space` pauses the water; `E` toggles enhanced water (the title says so); `Esc`/`Q` quits;
   - minimise the window and restore it: the picture comes back and the fans calm down while minimised (no frames are drawn then);
   - the title shows "CPU" and the frame rate, about 120 fps at 2560 × 1600 (measured 2026-10-18, `docs/perf/README.md`).
   Report anything that differs from the Rust viewer.
3. **Decision: a C++ preview for the tester.** CI now builds the C++ viewer for Windows and Linux. To get it to the tester, Claude can either (a) build a Linux package here in Docker and leave Windows for later, or (b) add a CI step that uploads the Windows and Linux builds. Option (b) produces GitHub artifacts, which need a GitHub login to download; a public GitHub Release is the alternative. Publishing anything is your call. Please say (a), (b), or "Release".
4. **Decision: GPU implementation** for the C++ renderer — wgpu-native (recommended: the same engine as the Rust viewer, prebuilt for Windows and Linux) or Dawn. Background: ADR 0001, "Decisions since". A one-word answer is enough.

## For the tester (Windows / Linux PC)

- Nothing new to run yet: there is no packaged C++ build. The latest preview is still `share/anno1503-preview-2026-10-17g.zip` (Rust build). A C++ preview follows once CI is green on Windows and Linux.

## Later, needs an x86 Windows machine or Wine on x86 Linux

- Fixtures F1–F9 in `docs/re/tick-loop.md` (game clock, speeds, PRNG cursor, multiplayer sync) against the original game. F6 (PRNG values) is already confirmed statically (the DLL's table bytes and the draw function), not yet by running the game.
- Savegame corpus (PLAN M2.1).
