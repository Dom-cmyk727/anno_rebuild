# The original on an invisible display: Wine + Xvfb in WSL (spike and the T16 runner)

**Status:**
- Spike passed 2026-10-22 (orchestrator, with the owner's agreement).
- The runner (T16) was built on 2026-10-22 on branch `task/T16-wine-runner`.
- The owner allows runs in it at any time without asking first; they must be silent.
- The idea comes from a co-developer's Anno 1602 setup (Wine + Xvfb on Linux, a driver script, memory reads), adapted here to the Windows PC through WSL. Everything lives on drive E: at the owner's request.

**Why:** the T15 harness drives the original on the owner's real desktop (dgVoodoo2 full-screen). That needs the owner present, risks input landing in other windows (the B1 guard), and allows only one instance at a time. On an invisible X display:
- the game takes no screen and makes no sound (§2.2);
- input goes to the virtual display only;
- several instances run side by side, each with its own display and copy;
- runs can be unattended.

## 1. Setup (reproducible)

On Windows, once:

```
wsl --install Ubuntu-24.04 --name AnnoHarness --location E:\wsl\AnnoHarness --no-launch
wsl -d AnnoHarness -u root -- bash /mnt/c/Users/altin/anno1503/tools/orig/wine/setup.sh
```

`tools/orig/wine/setup.sh` does the rest:
- installs Wine 9.0 with 32-bit support, Xvfb, xdotool, scrot, Pillow, and Mesa's software OpenGL (llvmpipe) in 32 and 64 bit;
- copies `game/app` into the distro (`/root/anno/game`; the source is only read, and the copy never goes into git);
- creates a 32-bit Wine prefix (`/root/anno/prefix`).

A separate distro keeps the owner's `Ubuntu` (used for the GCC checks) untouched. Disk: about 6.4 GB in `E:\wsl\AnnoHarness\ext4.vhdx`.

**The base copies stay as set up.** Every run works on an overlay of them (§4.2). One exception predates the runner: the spike ran the game directly in the base copy, and the game's first start created the default profile `SaveGame/ANNOnymous/user.dat` there. The install itself ships no `SaveGame` folder (`docs/formats/user-dat.md`). A pristine base therefore needs one start to create the profile before the in-game volumes can be set (§2.2).

## 2. Findings

### 2.1 Running the game
- **OpenGL is required.** Without 32-bit `libGL`, Wine cannot create the Direct3D device the game asks for ("the current DirectDrawRenderer does not support this"), and the game exits silently. With Mesa llvmpipe (OpenGL 4.5 on Xvfb) it runs intros, menus and games, at about **380 fps** in a game (§3).
- **The X socket:** in WSL, `/tmp/.X11-unix` is WSLg's read-only tmpfs. Xvfb cannot create its socket file there, so it serves only the abstract socket `@/tmp/.X11-unix/X<n>`, which Xlib/xcb (xdotool, Wine) try first. Readiness is read from `/proc/net/unix`.
- **Module bases** come straight from `/proc/<pid>/maps`, because Wine maps the DLL files:
  - AnnoWelt 0x580000, AnnoGame 0x680000, AnnoFrame 0x4d0000, AnnoScene 0x3a0000;
  - AnnoDisplay keeps 0x10000000, as on Windows;
  - the other bases differ from Windows, so they are resolved at run time as before.
- **Getting into a game** (owner: two intros, each skipped with ESC or a click):
  - the intros are `Videos\1000.bik` and `Videos\1001.bik`; both names sit next to the startup splash code in `AnnoFrame.dll`;
  - in virtual-desktop pixels the menus are 800 × 600 at the top left;
  - path: Singleplayer (400, 231) → New game (400, 308) → Citizen (280, 337) → OK (633, 542);
  - a running game fills 1024 × 768 with the full HUD;
  - frame fractions of all buttons are in `tools/orig/menus.json`.
- **New games differ.** They are seeded from `GetTickCount` (`tick-loop.md` §3): the computer players' cities differed between runs (OQ-109). The player's start was the same in every run: Kontor 50 t, tools 50, wood 50, money 500000 (player `+0x74`).

### 2.2 Silence (owner, 2026-10-22: "turn off the ingame music and sounds for the testing")
WSLg gives every distro a PulseAudio socket (`PULSE_SERVER=unix:/mnt/wslg/PulseServer`) that plays on the owner's speakers. The first T16 test run was audible. Every run is now silent at two levels:
1. **No audio device.** The agent removes `PULSE_*`, gives Wine a private `XDG_RUNTIME_DIR` (otherwise libpulse finds WSLg's socket there), and disables Wine's audio drivers (`WINEDLLOVERRIDES=winepulse.drv,winealsa.drv,wineoss.drv=d`).
   - An **audio guard** checks every 0.5 s that no process of the instance has a `PULSE_*` variable or an audio library mapped, and kills the instance otherwise.
   - Every run since the fix: 0 violations. The game runs normally without an audio device.
2. **The game's own volumes at minimum.** The main-menu Options are graphics only (resolution, details).
   - The in-game Options (gear tab) have **Music** (playlist and volume) and **Speech** (message categories and volume). There is no separate effects volume (the manual's option list agrees, playbook §1).
   - The game stores both sliders in the profile `user.dat` chunk `VOLUME` (0 = full, −250 = minimum).
   - The agent writes −250/−250 into each disposable copy before launch. A fresh run's Music tab shows the slider at its left end.

`--audio` turns both off for a run that needs sound.

### 2.3 Tools and shell
- **`pkill -f '<pattern>'`** can match the calling shell; use `pkill -x`.
- **Quoting:** nested quoting between PowerShell, WSL and bash breaks easily. The runner avoids it entirely: commands travel as JSON lines over the `wsl` pipe or as JSON files (`live.py`). Windows PowerShell 5.1 also mangles double quotes in arguments to native programs (git messages: use `-F <file>`).
- **xdotool `mousemove --sync`** hangs until its timeout (15.5 s measured) when the pointer is already at the target. The agent sends each gesture as one chained xdotool call without `--sync`.
- **Guest clocks:**
  - WSL's time sync slews `CLOCK_MONOTONIC` by up to 1,400 ppm within minutes (1,067–1,417 ppm in the four windows of `t16-timing3`) and steps `CLOCK_REALTIME`.
  - `CLOCK_MONOTONIC_RAW` runs at the Windows host's rate within the measurement's resolution (−0.1 to +0.5 ppm measured, ±4 ppm resolution of the clock pairs).
  - The game's clock follows RAW, not MONOTONIC (§3). That Wine's `QueryPerformanceCounter` reads `CLOCK_MONOTONIC_RAW` is likely; it was not checked in the Wine 9.0 source.
  - The spike's "+0.9 %" sampled `time.time()` (REALTIME) once a second over about 60 s. It is most likely that artefact: the T16 method finds no such offset.

### 2.4 Multiplayer (two instances, OQ-110)
- `HKCU\Software\Max-Design\Anno1503` `MULTIPLAYER` = 1 (`REG_DWORD`), set in a run's disposable prefix, shows the hidden **Multiplayer** button (LAN server, LAN client, Internet greyed out).
- Two instances on one host: the client lists the server's session and joins it; no virtual network is needed.
- The host's New game list is empty: no multiplayer scenarios ship (ISS-030). Hosting from a savegame is untried (OQ-110).

### 2.5 Savegames (OQ-020)
- **F10** (quicksave) writes `SaveGame/ANNOnymous/Lastgame.sav` in the game folder; `ANNOnymous` is the default profile. The overlay's upper layer lists it after the save, and the sampler keeps a copy per save (§4.6).
- **Container:** the `.sze` chunk format with root `ANNOADDON1` (P: seven quicksaves from two games). The children are `WELT`, `MAIN`, `GUID`, `ALIASNAME`, `WARETYPE`, `SPIELER` × n, `INSEL5` × n, `SKTIMER`, `MONOPOL2`, `LIMITS` and `BIGBROTHER`: 47 children in one game, 43 in the other.
- **`MAIN`** is 16 bytes:
  - `u32` +0 is the game time in whole seconds: 300, 600, … 1500 in the saves taken at the sampler's game minutes 5, 10, … 25;
  - `u32` +4 stays the same within a game and differs between the two games (unknown);
  - +8 and +12 are 0.
  The world's 64-bit game-ms counter (§3) is not in `MAIN`.
- **`BIGBROTHER`** keeps the scenario's script text and rewrites `[SIGNALS]` (`docs/formats/bigbrother.md`). In all seven quicksaves of two Citizen games, the script's definitions and kernel equal those of exactly one shipped scenario, `0010SingleEndlos/00_1024_easyentry_01.sze`, so the Citizen game starts from it. Its islands match none of the shipped `INSEL5` chunks; the game generates them.

### 2.6 One crash (OQ-124)
- In the first acceptance run, the game died at about 4.45 game minutes: `Unhandled page fault on read access to 016CAEA4 at address 006837E4`.
- With AnnoGame at 0x680000 that is `AnnoGame.dll 0x100037E4`, a three-instruction getter (`0x100037E0`): `mov eax,[esp+4]; mov eax,[ecx+eax*4+0x214]; ret 4`. It returns entry `i` of a dword table at object `+0x214`. The next getter (`0x100037F0`) reads a table at `+0x4098`, so the first table would hold 4001 entries if the two are adjacent.
- A caller passed an index or an object pointer that points into unmapped memory. Wine printed no register dump.
- The diagnostic rerun (12 minutes) and the second acceptance run (30 minutes, §5) did not crash; the crash is intermittent.
- Since then, Wine's crash dialog is off in every run's prefix (`HKCU\Software\Wine\WineDbg` `ShowCrashDialog` = 0). The dialog had held that run for 20 minutes. The agent's watchdog reports a crash (`status().crashed`) and ends the run.

## 3. Measurements against the Windows numbers (T15, PR #16)

| Check | Windows (dgVoodoo2) | Spike (Wine) | **T16 runner (Wine)** |
|---|---|---|---|
| PRNG table `AnnoWelt+0xcb8f8` vs MSVC `rand()` seed 1 (F6, read-only form) | 0 / 2048 differ | 0 differ | **0 differ (every run)** |
| `tick == counter >> 1` (F1's second clause) | every sample | every sample | the world object is *selected* by it; **29 of 29** sampler records of the acceptance run keep it (§5) |
| Player's start: Kontor capacity, tools, wood | 50 t, 50, 50 (= HUD) | 50 t, 50, 50 | **50 t, 50, 50 (every run)** |
| Player money at start (player `+0x74`) | — | — | **500000 (= HUD)** |
| F1: game ms per real s at 100 %, 65 s | 1000.0 | 1008.6 (wall clock) | **1000.008 ± 0.019** |
| F2: 200 % / 400 % | 1984 / 3986 | 1965 / 4495 (12 s) | **2000.011 / 3999.951 (± 19 ppm)** |
| F2: 50 % | 523 | 552 (12 s) | **593.8** (the rounding model over the measured frames: Σ steps = (Σ dt + odd frames) / 2 exactly; an earlier run at 380 fps gave 611.8) |
| F4: pause, 65 s | — | — | **0 ms, resumes** |
| Frames | — | — | **~405 fps; frame time p50 2.39, p95 3.11, p99 3.22, max 15.6 ms; dt at 100 %: 2 ms 54 %, 3 ms 46 %, ≥ 4 ms 0.5 %; none at the 100 ms cap** |

Run `t16-timing3` (2026-10-22, `cache/orig/t16-timing3/`). The ± is the rate's error from the clock pairs and the 1 ms counter over 65 s; all three speeds lie within it of the ideal (+8, +6, −12 ppm).

**What the PRNG row shows:** the table is read-only data in AnnoWelt's `.rdata`, so it equals MSVC `rand()` by construction. Matching it proves the module base and the reader are right, not that the generator behaves the same. The real F6 sets the cursor and draws, and needs memory writes (OQ-T15-WRITE).

How T16 measures (`tools/orig/timing.py`):
- windows of 65 s per speed;
- every change of the world's game-ms counter (polled about every 0.5 ms in the distro) is one frame;
- real time comes from host–guest clock pairs, NTP style (round trip 0.28 ms, error ±4 ppm);
- the window itself is measured on `CLOCK_MONOTONIC_RAW`.

At 50 % the rate depends on the frame times. `trunc(dt · 0.5 + 0.5)` gives 2 ms for a 3 ms frame, so 3 ms frames run at 67 %. The check therefore replays the measured frame times through the documented rounding instead of expecting a flat 500.

Why the Windows F2 numbers sit 0.4–0.8 % below the ideal is not known. T15 did not record their window length or the frame rate. Capped frames or the measuring method are both possible, and a Windows rerun with `timing.py`'s method would tell.

## 4. The harness (T16 as built)

The spike's scripts in `tools/orig/wine/` (`run_game.sh`, `debug_run.sh`, `measure.sh`, `wmem.py`) stay for reference; `agent.py` supersedes them.

### 4.1 One backend interface (`tools/orig/backend.py`)
The play layer, the fixtures and the runner scripts talk to a backend:
- **Methods:**
  - `start`/`stop`;
  - `key`, `click`, `move`, `drag`, `wheel` (frame fractions);
  - `screenshot` (optional crop and upscale), `recognize`;
  - `quicksave`/`quickload`;
  - `read`, `module_bases`;
  - `state("world" | "prng" | "cities" | "players" | "all")`.
- **`WineBackend`** speaks JSON lines to `tools/orig/wine/agent.py serve` over `wsl -d AnnoHarness -u root`. The agent runs in the distro as root and reads `/proc/<pid>/mem` (read-only).
- **`WindowsBackend`** reuses `smoke.py` (ReadProcessMemory, VirtualQueryEx, BitBlt capture). Input binds to T15's `control.py` with its B1 guard once PR #16 is on `main`.
- **Shared readers** (`tools/orig/gamestate.py`, the same code on both backends): world clock and tick, PRNG check, city stores, player objects (money).

### 4.2 Disposable copies
- Each run gets overlayfs views of the base game and base prefix under `/root/anno/runs/<run>/` (3 ms), plus its own Xvfb display and wineserver. A full copy (`--copy copy`) is opt-in only.
- Run ids must match `[A-Za-z0-9][A-Za-z0-9._-]*` and resolve directly inside `/root/anno/runs`. `stop` tears down only a run this agent prepared; `launch` and `reg_add` refuse without `prepare`.
- Outputs go to `<run>/out` and are copied to `cache/orig/<session>/d<display>-<start time>/` (one folder per backend) on `sync` and `stop`. The run folder is deleted only after a successful copy, and never with `--keep` (a `.keep` marker).
- The agent writes as root, so every path the controller names is checked (`within` in `agent.py`, resolved with `realpath`, so a symlink that leads out is refused, and any `..` component is refused outright):
  - `--out-win` must lie inside the repository's `cache/orig` folder, as the distro sees it (`/mnt/c/.../cache/orig`);
  - a screenshot's `dest` must lie inside the run's `out` folder (a relative `dest` is taken from there) or inside `--out-win`;
  - names that become file names (screenshot, learned screen, timing label, sampler, kept quicksave) are one component with the run-id alphabet.
- Files the game wrote (the overlay's upper layer) are listed on stop.
- **The agent stops its instance whatever ends its command loop:** `stop`, EOF, a broken pipe, SIGTERM or SIGHUP. Xvfb dies with the agent (`PR_SET_PDEATHSIG`). Each teardown step runs even if an earlier one failed.
- `agent.py cleanup` reaps runs whose agent is dead. It reads `agent.pid` and `xvfb.pid` in the run folder, kills the run's Wine processes and Xvfb, unmounts and deletes. Runs whose agent is alive, and `.keep` runs, are left alone.
- Last resort from Windows: `wsl --terminate AnnoHarness`.

### 4.3 Several instances
- One display per instance (`:91`, `:92`, …). Two instances ran side by side, both silent.
- Multiplayer: §2.4.

### 4.4 Screen recognition and the new-game macro
- **Signatures** (`tools/orig/screensig.py`, `screens_sig.json`):
  - each screen is identified by 192-bit difference hashes of boxes in frame fractions, so no picture content is in git;
  - menus use the whole frame and the button column; `in_game` uses the HUD's tab row and nameplate;
  - thresholds are calibrated as half the distance to the nearest other screen; the button column separates the menus by 64–77 bits.
- **Macros** (`tools/orig/macros.py`) check the screen before every click and verify the next one:
  - `skip_intros`: one ESC per intro, at most one spare, never on the menu;
  - `new_game`: launch to a running Citizen game in about 16 s.
- **`tools/orig/live.py`:** a file-driven live session for step-by-step work (inbox/outbox JSON).

### 4.5 Timing check
`tools/orig/timing.py` (§3).

**Verdict (2026-10-22, `t16-timing3`):** the runner's game clock runs at real time (F1 +8 ± 19 ppm; F2 +6 and −12 ± 19 ppm at 200/400 %; F4 exact), and frames never hit the 100 ms cap. Timing fixtures **F1, F2 (100/200/400 %) and F4 may run in the Wine runner**, measured against `CLOCK_MONOTONIC_RAW` or the host clock, never `CLOCK_MONOTONIC`/`REALTIME`.
- F2 at 50 % is a frame-time property on both platforms; compare it through the rounding model.
- F3 (5 fps) needs a frame limiter and is untested.
- Fixtures that compare state over time depend on the step size (OQ-109).

### 4.6 Sampler and the unattended run
- **The sampler** (agent `sample_start`) writes one JSONL record per game minute: clocks, world fields, every player's money, and city stores. Every N records it quick-saves (F10) and keeps a copy of the file the game wrote under `out/saves/`.
- **`tools/orig/opening.py`** plays step 1 of the playbook's Citizen opening (§12): 4 foresters, a road, the market hall, 2 hunting lodges, a tannery, 2 weaving huts and 4 sheep farms.
  - A placement counts only when all three of these hold:
    - the city's tools fall by exactly the building's cost;
    - the money falls by at least its gold and at most gold plus one upkeep booking;
    - the wood falls by its cost, less at most one cart load delivered meanwhile.
  - The upkeep bound comes from the city's running costs (`+0x604`/`+0x608`, booked as 1/15 every 4 s) and the player's own booking (`players.md`). With it, the (tools, gold) pair tells every building of the opening apart, so a wrong menu cell cannot pass. Roads must show at least 2 tiles of gold beyond the bound.
  - Quicksaves are off while the opening runs.
  - Menu cells and costs come from the original's construction menu and building panels, and equal `Anlagen.dat` (playbook §14; a unit test compares `opening.json` with that table).
  - Road connectivity is not verified yet; that needs the building lists.
- **`tools/orig/unattended.py`** runs it all (§5).

### 4.7 Later
- A Dockerfile of the same setup, so it can move to a Linux cloud machine unchanged.
- Lead (untested): the strings `/nl` and `/ml` sit next to the intro video names in `AnnoFrame.dll`, possibly start switches that skip the videos.

## 5. Acceptance run

**Passed** on 2026-10-22: `tools/orig/unattended.py --session t16-accept2 --minutes 30 --save-every 5`, 30.02 wall minutes, unattended. Everything ran on Xvfb `:91` inside the distro; the owner's desktop received no window, input or sound. Output: `cache/orig/t16-accept2/d91-20261022-214309/` (`summary.json`, `sampler.jsonl`, `actions.jsonl`, `saves/`; not in git).

| Step | Result |
|---|---|
| Prepare | overlay copy in 4 ms; profile volumes 0/0 → −250/−250; Wine crash dialog off |
| Start | two intros skipped (2 of 2), main menu recognised, Citizen game running (`in_game`) |
| F6 read-only form | PRNG table 0 of 2048 entries differ (= Windows) |
| Warehouse | Kontor 50 t, tools 50, wood 50, bricks 0 (= Windows); money 500000 |
| Opening (playbook §12 step 1) | 15 of 15 steps: 4 foresters, the road (3 + 4 tiles), market hall, 2 hunting lodges, tannery, 2 weaving huts, 4 sheep farms. Each step passed the tools/gold/wood check (§4.6); one sheep farm needed 3 tries. Stores afterwards: tools 21, wood 8, money 496,940 |
| Sampler | 29 records at game minutes 1.005 … 29.004, no gap; `tick == counter >> 1` in all 29 |
| Quicksaves | 5 (game minutes 5, 10, 15, 20, 25), all kept; `MAIN` holds 300 … 1500 s (§2.5) |
| Health | 29 checks, one per minute: alive, no crash, 0 audio violations over 11 processes |
| Teardown | 11 processes killed, overlay unmounted, run folder removed after the copy; the game wrote only `user.dat` and `Lastgame.sav` |

All eleven criteria of `unattended.py` are true: `no_error`, `prng_table_equal_windows`, `warehouse_equal_windows`, `opening_ok`, `checks_ran`, `never_crashed`, `audio_clean`, `sampler_no_error`, `sampler_one_record_per_minute`, `f1_tick_invariant_all_records`, `saves_kept`.

Earlier attempts:
- `t16-accept`: the game crashed at 4.45 game minutes (§2.6), and the crash dialog held the run. That run led to the crash detection and the dialog switch.
- `t16-crashdiag`: a 12-minute diagnostic run that passed every criterion.

## 6. Rules
- The game copy stays inside the distro; nothing of the original goes into git. Signatures are hashes, menu data are coordinates, screenshots stay in `cache/`.
- Memory is read-only. Memory writes stay an owner decision (OQ-T15-WRITE); in disposable copies they are a candidate for allowing.
- What a run may change is its own disposable copy and prefix: the profile volumes and registry values such as `MULTIPLAYER`.
- Runs are silent (§2.2). The owner allows them at any time; one heavy job at a time on the shared PC.
- The Windows harness stays as the native reference.
