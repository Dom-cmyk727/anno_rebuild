# Open checks for people (living list)

What only a person with the right machine can check. Claude Code keeps this list current; tick items off or note results next to them (date, machine, what you saw). Newest items first.

## Results

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
3. ~~Decision: a C++ preview for the tester~~ — the zip in `share/` as before (owner, 2026-10-18): now `share/anno1503-preview-2026-10-18b.zip`, built from the C++ code by `packaging/package-preview.sh`.
4. **Ships in the water** (new 2026-10-18, enhanced water): zoom in fully on a ship with `E` on. The hull bottom should sink into the water with broken foam at the waterline and rise and fall with the waves; with `E` off the ship looks as before (as the original). Close-ups were sent in the session; tell Claude what looks wrong (too much or too little sinking, foam too bright, bobbing too strong).
5. ~~Decision: GPU implementation~~ — wgpu-native (2026-10-18).
6. **Rivers into the sea, water against rocks** (new 2026-10-18, enhanced water `E`, after the tester's feedback "Übergang von Meer zu Flüssen sieht abgehakt und unnatürlich aus" and the owner's note about rocks floating on the water): look at river mouths and rocks at rivers and coasts (e.g. `Single_01.sze`, zoom 0, the river mouths of the big desert island in the middle-left). Expected: the river runs through its falls into the sea as one surface (shapes as the original's, no dark patches, no foam on rivers), rock bases sink into the water (foam only at the sea). Report what still looks wrong, with a screenshot.

## For the tester (Windows / Linux PC)

- **GPU tests on a Windows or Linux PC with a graphics card** (new 2026-10-18). So far they have run on the dev Mac (Metal) and on Linux with Mesa's software renderer (llvmpipe, Vulkan and GL; CI's Linux runners use it), never on a real Windows or Linux GPU (Vulkan, DirectX 12). With the game files, a C++ build (`cmake --preset release && cmake --build --preset release`, on Windows the `msvc` preset) and:
  ```
  ANNO_REQUIRE_GPU=1 ANNO1503_GAME_DIR=<game folder> ctest --preset release -j -R gpu
  ```
  (Windows PowerShell: `$env:ANNO_REQUIRE_GPU=1; $env:ANNO1503_GAME_DIR="<game folder>"; ctest --preset msvc -j 4 -R gpu`.) `ANNO_REQUIRE_GPU` makes a missing GPU a failure instead of a skip. Report the summary line and any failed test's output, and the GPU named in the first lines.
- **New preview `share/anno1503-preview-2026-10-18b.zip`** (2026-10-18): with `E`, river mouths (after your feedback "Übergang von Meer zu Flüssen sieht abgehakt und unnatürlich aus") and rocks in the water. Please look at the same places again and say whether the transition now looks natural. Earlier the same day: 2026-10-18a, the first build from the C++ code: steps and what to report are in the README (section "Test the preview"). New: ships sit in the water with `E`. Please also report whether it starts at all (exact error text if not), the GPU named in the window title, and `benchmark-results.md`. Linux needs glibc 2.35 or newer (Ubuntu 22.04 or later).

## Later, needs an x86 Windows machine or Wine on x86 Linux

- Fixtures F1–F9 in `docs/re/tick-loop.md` (game clock, speeds, PRNG cursor, multiplayer sync) against the original game. F6 (PRNG values) is already confirmed statically (the DLL's table bytes and the draw function), not yet by running the game.
- Savegame corpus (PLAN M2.1).
