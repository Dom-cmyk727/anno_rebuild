# Open checks for people (living list)

What only a person with the right machine can check. Claude Code keeps this list current; tick items off or note results next to them (date, machine, what you saw). Newest items first.

## For the owner (Mac), 2026-10-18

1. **CI status** of the latest push: GitHub → Actions → workflow "CI" → newest run on `main`. Until 2026-10-18 the C++ jobs on Linux and Windows had never passed. The Linux failures are reproduced and fixed: GCC sign-conversion errors, a missing include, SDL's XTest package. Both Linux jobs now pass in an Ubuntu 24.04 container. The Windows job failed at configure, probably because the preset pinned Visual Studio 2022; the pin is removed. That fix has not been tested: there is no Windows machine here. Claude can read the job results but not the logs (they need a login). If `cpp (windows-msvc)` is still red, open it and paste the first error lines into the chat. Expected green: `cpp (linux-gcc)`, `cpp (linux-clang-asan)`, `cpp (windows-msvc)`, `cpp (macos-clang)`, `cpp-format`, `lint`, `test`, `build-macos`.
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
3. **Decision: GPU implementation** for the C++ renderer — wgpu-native (recommended: the same engine as the Rust viewer, prebuilt for Windows and Linux) or Dawn. Background: ADR 0001, "Decisions since". A one-word answer is enough.

## For the tester (Windows / Linux PC)

- Nothing new to run yet: there is no packaged C++ build. The latest preview is still `share/anno1503-preview-2026-10-17g.zip` (Rust build). A C++ preview follows once CI is green on Windows and Linux.

## Later, needs an x86 Windows machine or Wine on x86 Linux

- Fixtures F1–F9 in `docs/re/tick-loop.md` (game clock, speeds, PRNG cursor, multiplayer sync) against the original game. F6 (PRNG values) is already confirmed statically (the DLL's table bytes and the draw function), not yet by running the game.
- Savegame corpus (PLAN M2.1).
