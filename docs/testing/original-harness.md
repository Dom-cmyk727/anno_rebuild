# T15 stage 0: running the original game for comparison (design, REVIEW)

Status: **design, static checks and the smoke run done (2026-10-21).** The game runs through dgVoodoo2 (full-screen at its native resolution) on the primary monitor, a running scenario was screenshotted by the script at 1:1, the 32-bit module bases were read and each base cross-checked against the on-disk PE header, the world clock was located and read growing at 500 ticks/s (game speed 1.0), and the game closed with nothing left and the copy unchanged. Host: Windows 11, Ryzen 9800X3D, RTX 4090, the 2560 x 1440 @ 500 Hz monitor primary at 100 % scaling (both monitors on the RTX 4090; the iGPU drives none, correction 2026-10-21 s).

## 1. The copy (done, verified)

- `game/app` is copied to `C:\anno1503-original\` (outside the repository, 63 entries). The game is never run from `game/app`: it writes profiles, settings and saves into its own folder.
- The 20 executables and DLLs (`*.exe`, `*.dll`, `*.DLL`) have the same SHA-256 in the copy and in `game/app`. The list is the table at the end of this note; the files themselves are not in git.
- The copy is run by the harness only; `game/app` stays read-only. The copy lies outside the repository, so git cannot see it.

## 2. What the static checks show (no game code run)

From `analysis/pe_symbols.json` (the import tables of the original modules):

- `1503Startup.exe` imports only KERNEL32, USER32 and GDI32 (`GetStockObject` from GDI32). It is the launcher.
- `AnnoDisplay.dll` imports `DDRAW.dll` through one function, `DirectDrawCreateEx`. The original renders through **DirectDraw 7**, not Direct3D. Its window and its mode switch are DirectDraw's.
- `binkw32.dll` (Bink video) and `goggame-1438074791.dll` (GOG) are present; neither is needed for the harness.
- Community notes (`docs/research/original-issues.md`, ISS-014, from community posts, not verified here): a `/w` command-line switch for a window, and DxWnd with "flip emulation" as a workaround for slow display on Windows 8 and later. Neither is verified on this copy yet.
- Settings: the community notes give the registry key `HKCU\Software\Max-Design\Anno1503` with `SCREENMODE` 0-4. The key is **not present** on this PC (the game has not been run here yet). Its values are an input to the smoke run, not an assumption.

**Consequence for the design:** a windowed run of the original needs one of (a) the game's own window switch, if it works on this copy (to be tested in the smoke run), or (b) a DirectDraw wrapper (a third-party tool, section 5). Until the smoke run shows which, the harness assumes the game runs in its own mode and captures the desktop around it.

## 3. How the harness works (no third-party code)

All of this uses the Python standard library (`ctypes`) and Windows calls; nothing is installed.

| Need | Method | Notes |
|---|---|---|
| Start the game | `CreateProcess` of `1503Startup.exe` from the copy, with a `cwd` in the copy | the launcher may start `Anno1503.exe`-like children; record the process tree |
| Screenshot | `PrintWindow` (or `BitBlt` of the window's DC) into a PNG under `cache/orig/<session>/` | exclusive-mode DirectDraw may not show in `PrintWindow`; the smoke run decides between that and a desktop capture |
| Module bases | 32-bit PEB loader-list walk via `ReadProcessMemory` (`EnumProcessModulesEx(LIST_MODULES_32BIT)` / `TH32CS_SNAPMODULE32` as the documented cross-check) | the RE notes assume image base `0x10000000` per module; checked at run time, the DLLs rebase |
| Memory reads | `ReadProcessMemory` (read-only handle) | the world object: vtable `0x100ccdd8` in `AnnoWelt.dll` (offset `0x000ccdd8` from the module base), tick at `+0x10c` (`docs/re/tick-loop.md`, §3 and the world object) |
| Input (camera, zoom, pause, speed) | `SendInput` or window messages to the game window | **only in an agreed session**, one action at a time, each followed by a screenshot |
| Scripting | a protocol file (JSON lines: action, expected screenshot or memory value) run by `tools/orig/` | outputs and a timestamped log in `cache/orig/<session>/` |

Failure handling: each step has a timeout; a missing window or a value outside the expected range stops the script and writes the screenshot taken at that moment.

## 4. Stage 0 smoke run (done 2026-10-21; `tools/orig/smoke.py`)

The run: `python -I tools/orig/smoke.py --game C:\anno1503-original --out cache/orig/smoke-<tag> --hold <s>`.
It launches the copy, re-finds and foregrounds the game window, screenshots it (BitBlt of the
on-screen rectangle) every `--shot-every` seconds, enumerates the 32-bit modules, scans the heap
for the world object and reads its tick, then closes the game (WM_CLOSE, then taskkill of every
process under the copy folder) and hashes the copy. It sends **no input** (the owner clicks).

**Results (owner-run, RTX 4090, primary monitor):**

1. **It renders and is captured.** `cache/orig/smoke-dgv-full/shot-114s.png` is a running
   single-player scenario at 1024 x 768 (isometric coast, a ship, a trading post, the full HUD:
   minimap, resource bar, gold 499,990) — the original game through dgVoodoo2, screenshotted by the
   script. (cnc-ddraw first produced black and then made the game exit in ~3 s; dgVoodoo2 runs it.
   See 5.1 / 5.2.) First frames are black (the Max Design intro), then content.
2. **The owner reaches a scenario** by clicking the intros and menu (the window grows from 800 x 600
   to 1024 x 768 when the scenario loads).
3. **Module bases** (32-bit, by walking the WOW64 target's PEB loader list over
   `ReadProcessMemory`. `EnumProcessModulesEx(LIST_MODULES_32BIT)` and `TH32CS_SNAPMODULE32` are the
   documented ways for a 64-bit caller to read a WOW64 target's 32-bit modules, but a
   `TH32CS_SNAPMODULE32` snapshot came back empty in the stage-0 runs (`toolhelp_snapmodule32_count`
   0; smoke.py now also records the `CreateToolhelp32Snapshot` error code), so the PEB walk is the
   primary method and the per-module PE-identity check below is the real cross-check):
   `AnnoDisplay.dll` loads at its
   preferred base **0x10000000**; `AnnoFrame.dll` (0x2300000), `AnnoScene.dll` (0x24b0000),
   `AnnoGame.dll` (0x2500000) and `AnnoWelt.dll` (0x2530000-0x2560000) are **rebased** (they all
   prefer 0x10000000 and conflict, so only the first keeps it). `binkw32.dll` 0x30000000,
   the dgVoodoo2 `DDraw.dll`/`D3DImm.dll` in the 0x6xxxxxxx range. The RE notes' "image base
   0x10000000 each" is the preferred base; at run time only AnnoDisplay gets it.
4. **The world clock reads and grows.** `ReadProcessMemory` of a module's code bytes matches the
   file on disk. The locator scans the heap for every DWORD equal to the world vtable
   (`AnnoWelt base + 0xccdd8`) and accepts only a candidate whose ms counter (+0x100) is non-zero
   and whose tick (+0x10c) == counter >> 1 (docs/re/tick-loop.md); in the unpaused run there was
   exactly one such candidate (an earlier run accepted a stray all-zero match, now rejected). Read
   four times, ~2.5 s apart, the tick grew 4063 -> 5314 -> 6563 -> 7814, i.e. **500.1 ticks per real
   second = game speed 1.0** (the note's 500 ticks/s at 1x; 1 tick = 2 ms game time). The DWORD at
   +0x108 equalled the tick (+0x10c) in every read. The candidate list, the reads with their real
   times and the rate are in the run's `log.json`.
5. **Clean close:** no process left under the copy folder; the game's 20 files stay byte-identical
   to `game/app` after every run.

Stop: the game's own close, or `taskkill` on the recorded PIDs (the script does the latter at the
end). Nothing is typed into other windows.

## 4a. Input control (SendInput; `tools/orig/control.py`)

`tools/orig/control.py` drives the game with Windows `SendInput` (standard-library ctypes, no
third-party tool): it foregrounds the game window and sends key presses (speeds F5-F8, menu keys)
and mouse moves/clicks at window-relative coordinates (0..1), and screenshots the window. Used
only in a session the owner agreed to; the owner agreed to `SendInput` control on 2026-10-22.

Sub-commands (the game is launched once, detached, and persists across calls): `launch`, `shot
--out`, `key F6` (or `ENTER,ESC`), `click nx ny [--shot]`, `dblclick`, `move`, `wheel`, `drag`,
`seq`, `quicksave` (F10), `quickload` (F9), `close` (WM_CLOSE + taskkill + re-hash). A perceive-act
loop: `launch`, `shot`, read the PNG, `click`/`key`, `shot`.

**Quick save / load (stage A).** `quicksave` presses F10 and `quickload` presses F9 (manual p4-5,
authoritative; English copy). The save state itself is not yet read from memory (stages B/C), so the
command logs and can `--shot`; verify by screenshot until the save slot can be read. Quirk
(playbook §13): a tavern's alcohol and goods on carts are **not** saved — quick-save at calm moments.

**Cross-call cursor guard (stage A).** The "owner took the mouse" safety check compares the cursor
to where the tool last left it. Within one process that is a module global; **across** separate
`control.py` calls the last cursor is now persisted to `cache/orig/<session>/cursor.json` and
re-loaded at the start of the next call, so the first input of a later call in the same session is
also checked. This is active only with a **stable `--session`** (the default per-call timestamp
session has no prior cursor, so the first input is unchecked, as before). A driving session should
therefore pass a fixed `--session <name>`.

**Screen table as data (stage A).** The HUD and construction-menu layout is `tools/orig/screens.json`
(window-fraction coordinates, resolution-independent) with a loader+validator `tools/orig/screens.py`
(`python -I tools/orig/screens.py` validates; `--missing` lists what is still to capture). Only
measured coordinates are filled in (the four main tabs, the minimap region, the category column x,
the grid region, the scripted new-Citizen-game menu path, the manual's eight construction categories,
the gameplay key map); every per-civ-level building grid cell and every panel layout is marked
`{"capture": "owner-session"}` and recorded by screenshot with the owner, not fabricated.

Verified 2026-10-22: driven entirely by the script, the game went main menu -> Singleplayer -> New
game -> Choose game (Citizen) -> OK -> a running single-player scenario (screens in
`cache/orig/drive/`, git-ignored), and in the running scenario `F7` changed the clock from 500 to
2000 ticks/s (speed 4.0) and `F5` back to 500 (1.0): mouse drives the menus and keyboard drives
the simulation. So the keyed fixtures (`m2-fixtures.md` TL-F2 speeds, TL-F4 pause) and menu/scene
setup can run without the owner at the keyboard, in an agreed session. The copy stays byte-identical
to `game/app` after a run; no process is left.

## 4b. Owner driving session 2026-10-25 (orchestrator at the keyboard, owner present)

What ran: the copy through dgVoodoo2, driven only by `tools/orig/control.py` (sessions
`drive-20261025c/d`), memory read by `tools/orig/backend.py` (`WindowsBackend.state('all')`,
from `main`) and the new `tools/orig/memdiff.py`. Findings that change how the harness is used:

1. **The game warps the cursor itself** (`docs/re/view.md` §1): the B1 "cursor moved since the
   last action" check aborted every call after the first click. It now aborts only when the cursor
   left the game's client area; a warp inside the area is logged as `cursor_warp`. The foreground
   check and the target-inside-client check are unchanged. With that, the whole path intros →
   Singleplayer → New game → Citizen → OK → a running game, F10, F9, B and the eight categories
   ran from the script with zero aborts.
2. **The full-screen game minimizes whenever the terminal takes focus** between calls (its window
   rect reads (-32000, -32000)); `foreground()` restores it (4f58386), so every call works, but
   `smoke.find_game_window` returns `None` while it is minimized: use `control.foreground` first.
3. **`P` opens a modal "Pause!" scroll** ("Continue game" at client (0.40, 0.69)); the arrow keys
   and the hovered tile are dead while it is up. Slow down with F8 (50 %) for RE, not with P.
4. **Sound off:** `tools/orig/userdat.py set_volume` on the copy's
   `SaveGame/ANNOnymous/user.dat` (music was -235, speech 0; now -250/-250). Set before the
   launch; no effects slider exists.
5. **Memory readers from `main` work on the Windows copy:** world tick, PRNG table 0/2048
   mismatches, seven players with money, five cities with stocks. After a quick load two player
   objects carry the human's vtable (the freed pre-load copy): prune by reachability.
6. **Disk:** `memdiff snap` writes 182 MiB per snapshot; C: hit 0 bytes free once. Delete mined
   snapshots at once; keep at most one set.
7. **Stage-A acceptance:** F10 wrote `Lastgame.sav` (2.7 MB) within 3 s and F9 reloaded it; the
   deliberate focus-steal test (a key refused and logged when another window is in front) is the
   one item still to be done with the owner's hand (see REPORTS).
8. **Stage B:** the hovered tile and the game's mouse position are found (stack locals,
   `docs/re/view.md`); the camera is open, to be calibrated against the minimap rectangle
   (owner's hint). The construction menu's category column and grid are measured into
   `screens.json` (eight categories, pioneer-level cells).

## 5. Third-party tools (none installed; owner decides each)

Candidates for the questions above; each needs its version, licence and reason before installation, and nothing of them goes into git:

| Tool | Purpose | Status |
|---|---|---|
| **dgVoodoo2** (ddraw wrapper) | DirectDraw on the primary monitor (full-screen at native res) | **installed and in use** (see 5.2): the game runs and renders through it |
| cnc-ddraw (ddraw wrapper) | windowed DirectDraw | **tried, removed** (see 5.1): it rendered black, then the game exited in ~3 s |
| Frida | hooks at the tick function | not installed; not needed for stage 0 |
| pywinauto | window control | not installed; not needed for stage 0 |
| mss | screenshots | not installed (BitBlt of the window rectangle works) |

### 5.1 cnc-ddraw (tried first, removed)

The original renders through DirectDraw 7, natively in exclusive full-screen on the primary
monitor, so a desktop capture of it was black. cnc-ddraw was tried first (owner's order). It
loaded (its window appeared), but **rendered black and then the game process exited within ~3 s**
(confirmed: `1503Startup.exe`, which runs the game in-process and persists natively, exits a few
seconds after start whenever cnc-ddraw's `ddraw.dll` is present). So cnc-ddraw does not run this
game; its files were **removed** from the copy. Recorded for the trail:

- **Version:** v7.1.0.0 (released 2024-12-28).
- **Download:** https://github.com/FunkyFr3sh/cnc-ddraw/releases/download/v7.1.0.0/cnc-ddraw.zip
- **SHA-256 (cnc-ddraw.zip):** `0b13ab89a64c9918189b1dadd449ef6ed3cb3b7b19cabd96d8adbd95505bb908`
- **Licence:** MIT (github.com/FunkyFr3sh/cnc-ddraw).
- **Installed files** (into `C:\anno1503-original\` only, never `game/app`, never git):
  - `ddraw.dll` SHA-256 `85e0f7d530dfda134793a57cb3e76b0287dcc96892ee57162dd68f47283b03a9`
  - `ddraw.ini` (configured, see below) and `Shaders\` (15 files, for the unused shader renderers).
- **Configuration** (the global `[ddraw]` section of `ddraw.ini`; no Anno-specific section exists):
  `windowed=true`, `width=1280`, `height=960`, `maintas=true`, `posX=100`, `posY=80` (on the
  primary monitor, `\\.\DISPLAY2`, x 0..2559), `renderer=gdi` (the most reliable for a screen
  capture), `savesettings=0` (the window stays where it is put), `resizable=false`.
- **Hash separation:** the game's 20 executables and DLLs stay byte-identical to `game/app`
  (verified after install); `ddraw.dll`, `ddraw.ini` and `Shaders\` are the only added files.

### 5.2 dgVoodoo2 (installed, in use)

dgVoodoo2 wraps DirectDraw/Direct3D over D3D11/12 far more completely than cnc-ddraw. Installed
after cnc-ddraw failed; **the game runs, renders and is captured through it.**

- **Version:** v2.87.5 (released 2026-09-15).
- **Download:** https://github.com/dege-diosg/dgVoodoo2/releases/download/v2.87.5/dgVoodoo2_87_5.zip
- **SHA-256 (dgVoodoo2_87_5.zip):** `5ffde6927f7355ca3fdd5d785b581256a8e6539fa13e395a891ade6ba1040850`
- **Licence:** the package ships no licence text file; it links the terms through `Doc\Readme.url`
  (`URL=http://dege.fw.hu/dgVoodoo2/ReadmeGeneral/`). That page, `Copyright (c) 2013-2026 Dege`,
  states its redistribution rights verbatim (section "1. Redistribution rights"):
  > You can freely ship your game or game mod with unmodified individual dgVoodoo components (files) included.
  > If you want to host or re-distribute dgVoodoo as a standalone component for any reason then you must
  > provide the original, unmodified .zip package. You cannot bundle dgVoodoo inside launchers or
  > frameworks, for general use across multiple applications.
  We ship nothing: the files stay in the local copy only, never in git, so this is use, not redistribution.
  The page states no fee and no named licence (MIT/GPL/etc.); only these redistribution rules.
- **Installed files** (into `C:\anno1503-original\` only, never `game/app`, never git), from
  `MS/x86/`: `DDraw.dll` and `D3DImm.dll` (the 32-bit DirectDraw wrappers), plus the configured
  `dgVoodoo.conf`. cnc-ddraw's `ddraw.dll`, `ddraw.ini` and `Shaders\` were removed first.
- **Configuration** (`dgVoodoo.conf`): `FullScreenMode = false` was set but did **not** window this
  title (the owner saw it full-screen); `CaptureMouse = false` (the owner keeps the cursor),
  `dgVoodooWatermark = false` (clean capture); the DirectX resolution is unforced, so the game runs
  at its own resolution (800 x 600 menu, 1024 x 768 in a scenario) full-screen, with no upscaling.
- **Hash separation:** the game's 20 files stay byte-identical to `game/app`; `DDraw.dll`,
  `D3DImm.dll` and `dgVoodoo.conf` are the only added files.
- **Scaling and 1:1 capture (stage 3 compares pixels):** the primary display is at **100 %**
  scaling (96 DPI, 2560 x 1440). dgVoodoo2's DirectX `Resolution` is `unforced`, so it presents the
  game's own back-buffer (800 x 600 at the menu, 1024 x 768 in a scenario) at that exact pixel size,
  full-screen with no stretching or filtering (a scaled full-screen would be captured at the
  monitor's 2560 x 1440 and blurred; the grab is 1024 x 768, so it is 1:1). The harness marks itself
  per-monitor-DPI-aware and captures with a `BitBlt` of the window's physical rectangle, so the PNG
  is 1:1 with the game's resolution (the scenario shot is 1024 x 768, the game's resolution; the
  run records `primary_display_scaling_percent`). If the owner ever raises the desktop scaling,
  keep dgVoodoo2's resolution unforced and the window unscaled for the pixel comparison.

## 6. Our side (what has to show the same view)

For stage 3 of T15 (the renderer comparison), the view is set in our renderer by `--png --group <g> --at <x>,<y> --size <w>x<h>` (the derived levels too, from the T3 follow-up). The camera position and zoom group of the original are taken from its memory or from the screen, and the same values go to `--at` and `--group`. The mapping between the original's camera and our tile coordinates is part of stage 3 (OQ-026).

## 7. What is settled, and what is left

Settled by the smoke run (owner-agreed, 2026-10-21): the copy at `C:\anno1503-original\` is used for
all stages; the wrapper is **dgVoodoo2** (cnc-ddraw does not run the game); the harness can
launch, screenshot at 1:1, read 32-bit modules (cross-checked against the on-disk headers) and the
world clock (growing at the expected rate), and close cleanly without touching `game/app`.

Left for stage 1 (not blockers for the design):
1. **Scripted control** (menu navigation / camera / save-load by `SendInput`): only in a session the
   owner agrees to; not needed for the fixtures that read memory.
2. The M2 fixtures themselves (F1-F9, production, population) read the world clock, PRNG cursor and
   stocks from memory while a scenario runs; the smoke run proves the reads are possible.
3. **True windowed mode is a nicety, not a need.** The game ran full-screen at its native resolution
   (the owner's note); `FullScreenMode = false` in `dgVoodoo.conf` did not window this title. The
   capture is still 1:1 (the screen was the game's native resolution, no upscaling; a scaled
   full-screen would have been captured at the monitor's 2560 x 1440 and blurred). A true window
   (less disruptive for the owner, the desktop stays put) can be chased later — a per-game CPL
   profile, or `dgVoodooCpl.exe` run once against the copy — but stage 3's pixel comparison does not
   need it.

## 8. Hashes of the copy (SHA-256, identical in `game/app`)

Recorded by `sha256sum` on 2026-10-21; the list covers the 20 executables and DLLs of the game folder.

```
0cd03e300d1f437c6066409a2961c6b4d0ced578d81dad0ef594dc081c62e9b4 *1503Startup.exe
b95f88689e04ad7f55c5d8c98b9ec9e120a2da3c24d7b655c88c56d9ebec70af *unins000.exe
e298efdb67707e61ec86ccfc105bebbeb9c74ab29d674f69ba89056ffedebe24 *AnnoDisplay.dll
dd3f36ff564d2685d10795272c4ed3c0b2da32a946c2883fb1158ff439249b91 *AnnoFrame.dll
ac721220afd27ebca5fd2ece6a8ff452289b9d7458c4e091c68adea2012db7bc *AnnoGame.dll
5b521f9541332846d32b57c93eaaee9c9d8de2ac2404cd75da0fe93e0e36fd38 *AnnoScene.dll
f220a878510f740c4cfb7224ade0a260e9a0fd7e99113e1201e7a37c0dfa6ae2 *AnnoWelt.dll
39b622b4a62d9814068619fd4254a675ed1a2a88d6b2594cb6a969c0216d11fa *CompLogic.dll
544b9135e819e6270d889a7d5e7736e3faa4d54a5cddf329452870bd1a386a57 *CompLogicClassic.dll
913fcaa08d5fc283dd4b3049bf67cea6cebbdf9655d1db1ceac71631cf3cbdf6 *DrvMgt.dll
368918b78404ac77c67c7af43b3821933ce21444f956d6cbb781090c89cadac0 *FileGame.dll
b07ffcd0af80f6b9fba09abe816ba2f0ff0d336639f1768fc317291bc635ece6 *GameuxInstallHelper.dll
2510065003d7fdde3492b6786d576bcb19ddf1b369664707cfa5ed66e6f513f9 *Max_Sound.dll
e32fa8a5ff0f3f0b00ef05618daf8309c692daaa57316147eb6cb1f3637733a1 *View.dll
892a51c4056efcb22297a3b44a3491e3f5888f28b08ed1b17030f24acffedb44 *binkw32.dll
a8cb45940d009abc091cdd05a4c4e6882e8a2bc2a04c2ed9a1315a85428d8386 *goggame-1438074791.dll
3a3fa253ce80cf55027abc7918f8984057a94d1832302fb9893402aba9d35dff *MFC42D.DLL
c24f04b2bdcd9211e49853ec874c6d5c1bc6487b09c378d185aa37cceea5c4c5 *MFCO42D.DLL
3ef37b982dc58b12f72c978e0bdc19f6af74fd2a582818788d422b6914c0698d *MSVCRT.DLL
9ccc9526a034e337c1db7f4afb60d88130ebca9327287915ed50e87c873e44f2 *MSVCRTD.DLL
```
