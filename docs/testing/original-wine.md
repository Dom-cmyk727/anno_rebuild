# The original on an invisible display: Wine + Xvfb in WSL (spike result and harness plan)

**Status:** spike passed 2026-10-22 (orchestrator, owner's green light). The harness built on it is task T16. Idea taken from a co-developer's Anno 1602 setup (Wine + Xvfb on Linux, a driver script, memory reads); adapted here to the Windows PC through WSL, with everything on drive E: at the owner's request.

**Why:** the T15 harness drives the original on the owner's real desktop (dgVoodoo2 full-screen). That needs the owner present, risks input landing in other windows (the B1 guard), and allows one instance at a time. On an invisible X display:
- the game takes no screen;
- input goes to the virtual display only;
- several instances can run side by side (each with its own display and copy);
- runs can be unattended.

## 1. Setup (reproducible)

On Windows, once:

```
wsl --install Ubuntu-24.04 --name AnnoHarness --location E:\wsl\AnnoHarness --no-launch
wsl -d AnnoHarness -u root -- bash /mnt/c/Users/altin/anno1503/tools/orig/wine/setup.sh
```

`tools/orig/wine/setup.sh` does the rest:
- installs Wine 9.0 with 32-bit support, Xvfb, xdotool, scrot, and Mesa's software OpenGL (llvmpipe) in 32 and 64 bit;
- copies `game/app` into the distro (`/root/anno/game`; the source is only read, and the copy never goes into git);
- creates a 32-bit Wine prefix.

A separate distro keeps the owner's `Ubuntu` (used for the GCC checks) untouched. Disk: about 6.4 GB in `E:\wsl\AnnoHarness\ext4.vhdx`.

Scripts (spike quality, the start of T16), in `tools/orig/wine/`:
- `run_game.sh :91` starts Xvfb (1024×768) and the game in a Wine virtual desktop of exactly 1024×768, so the game never switches display modes. It waits for every Wine process (`wineserver -w`), because `1503Startup.exe` hands over to the game and exits.
- `wmem.py` runs as root, which may read any process's memory despite `ptrace_scope=1`. It has four commands: `info` (pid and module bases), `world --seconds N` (clock samples), `prng`, and `city`.
- `measure.sh` runs the PRNG, city and speed-key checks.
- `debug_run.sh` runs with Wine's error output.

## 2. Findings during the spike

- **OpenGL is required.** Without 32-bit `libGL`, Wine cannot create the Direct3D device the game asks for ("the current DirectDrawRenderer does not support this"), and the game exits silently. With Mesa llvmpipe (OpenGL 4.5 on Xvfb) it runs: intros, menus, the game.
- **Errors about sound** (no ALSA, no MIDI synth) are harmless.
- **Getting into a game:** Escape ×6 skips the intros. In virtual-desktop pixels, the menus are 800 × 600 at the top left: Singleplayer (400, 231) → New game (400, 308) → Citizen (280, 337) → OK (633, 542). A running game then fills 1024 × 768 with the full HUD.
- **Module bases** come straight from `/proc/<pid>/maps`, because Wine maps the DLL files:
  - AnnoWelt 0x580000, AnnoGame 0x680000, AnnoFrame 0x4d0000, AnnoScene 0x3a0000;
  - AnnoDisplay keeps 0x10000000, as on Windows;
  - the other bases differ from Windows, so they are resolved at run time as before.
- **Shell pitfalls:** `pkill -f '<pattern>'` can match the calling shell; use `pkill -x`. Nested quoting between PowerShell, WSL and bash breaks easily, so put commands into script files.

## 3. Measurements against the Windows numbers (T15, PR #16)

| Check | Windows (dgVoodoo2) | Wine + Xvfb |
|---|---|---|
| PRNG table `AnnoWelt+0xcb8f8`, 2048 entries vs MSVC `rand()` seed 1 | 0 differ | **0 differ** |
| `tick == counter >> 1` | every sample | **every sample (63)** |
| World clock at 100 %, 60 s | 500.0 ticks/s | 504.3 ticks/s (+0.9 %; F1 allows ±1 s per 60 s) |
| Starting city: capacity, tools, wood | 50 t, 50, 50 (= HUD) | **50 t, 50, 50** |
| Other cities' capacity | 70 t | **70 t** |
| Speed keys F6 / F7 / F8 (game ms per real second) | 1984 / 3986 / 523 | 1965 / 4495 / 552 (12 s windows); F5 923 |

**State checks agree; timing is noisy under Wine.** The sampler uses the wall clock, which WSL corrects now and then, and the game renders in software on about 3 cores, so frames can exceed the game's 100 ms step cap.

**Rule for now:** state fixtures (PRNG, stores, production, houses) may run under Wine; timing fixtures (F1–F4) stay on native Windows until T16 shows Wine's timing with a monotonic clock and long windows.

## 4. The harness (T16)

1. **One backend interface** for the existing tools (control, probe, fixtures, the play layer): launch and stop, screenshot, key, click, drag, wheel, quick save and load, memory read, module bases.
   - Windows backend: today's `ctypes` code.
   - Wine backend: Xvfb + xdotool/XTEST + scrot, plus `/proc/<pid>/mem` and `/proc/<pid>/maps`.
   - The play layer (`play-original.md`) and the fixtures stay the same for both.
2. **Disposable copies:** one game copy and one prefix per run inside the distro; per-run outputs under a run folder, copied to `cache/orig/<session>/` on Windows. Clean up after the run.
3. **Several instances,** one display each (`:91`, `:92`, …). This includes the two-instance multiplayer fixtures (OQ-T15-MP): two Wine instances on a virtual network.
4. **Screen recognition** by pixel signatures (main menu, choose game, in game, …), and a `new-game <name>` macro built on the menu path above.
5. **Timing check:** sample with `CLOCK_MONOTONIC` over windows of at least 60 s per speed, record frame times, and compare F1/F2 with Windows. Only then decide whether timing fixtures may use Wine.
6. **A long-running sampler** (JSONL per game minute, periodic saves) and the PR #16 tools on top.
7. **Later:** a Dockerfile of the same setup, so it can move to a Linux cloud machine unchanged.

**Rules:**
- the game copy never goes into git;
- memory writes stay an owner decision (OQ-T15-WRITE); in disposable copies they are a candidate for allowing;
- the Windows harness stays as the native reference.
