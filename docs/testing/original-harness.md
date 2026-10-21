# T15 stage 0: running the original game for comparison (design, REVIEW)

Status: **design and static checks done; the smoke run is not done** (it starts the game and takes the desktop's focus, so it needs the owner's agreement, rule (c) of T15). Date: 2026-10-21. Host: Windows 11, Ryzen 9800X3D, RTX 4090 and Radeon iGPU (the 500 Hz monitor is on the iGPU).

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
| Module bases | `EnumProcessModules` + `GetModuleInformation` | the RE notes assume image base `0x10000000` per module; check at run time, the DLLs may rebase |
| Memory reads | `ReadProcessMemory` (read-only handle) | the world object: vtable `0x100ccdd8` in `AnnoWelt.dll` (offset `0x000ccdd8` from the module base), tick at `+0x10c` (`docs/re/tick-loop.md`, §3 and the world object) |
| Input (camera, zoom, pause, speed) | `SendInput` or window messages to the game window | **only in an agreed session**, one action at a time, each followed by a screenshot |
| Scripting | a protocol file (JSON lines: action, expected screenshot or memory value) run by `tools/orig/` | outputs and a timestamped log in `cache/orig/<session>/` |

Failure handling: each step has a timeout; a missing window or a value outside the expected range stops the script and writes the screenshot taken at that moment.

## 4. Stage 0 smoke run, the plan (not run)

What runs, for how long, how to stop (rule (c)):

1. Start `C:\anno1503-original\1503Startup.exe` (no arguments) and wait for its window (at most 60 s).
2. Take one screenshot of the game's window (PNG, `cache/orig/smoke-<time>/`).
3. Read the world tick at `AnnoWelt.dll` base + `0xccdd8` pointer chain once a game is running (needs a scenario loaded; the first run uses the launcher's own menu, so the value is read only if a scenario is running), and compare it with a second read a few seconds later (it must grow).
4. Close the game with its window's close button or `taskkill /PID <pid>`; check that no child process is left.
5. Hash the copy again and compare with `game/app`.

Duration: under 5 minutes. Stop: the game's own close, or `taskkill` on the recorded PIDs. Nothing is typed into other windows; the script sends no input in this smoke run.

**Needs the owner's yes:** the smoke run takes the focus and shows the game on the desktop. I will not start it until you agree.

## 5. Third-party tools (none installed; owner decides each)

Candidates for the questions above; each needs its version, licence and reason before installation, and nothing of them goes into git:

| Tool | Purpose | Why it might be needed | Status |
|---|---|---|---|
| dgVoodoo2 (ddraw wrapper) | windowed DirectDraw | if the game's window switch does not work | not installed; licence: check |
| cnc-ddraw (ddraw wrapper) | windowed or stretched DirectDraw | alternative to the above | not installed; licence: check |
| Frida | hooks at the tick function | per-tick logs, if memory reads are not enough | not installed; not needed for stage 0 |
| pywinauto | window control | only if the scripted input of section 3 is too brittle | not installed; not needed for stage 0 |
| mss | screenshots | only if `PrintWindow` misses the DirectDraw window | not installed; not needed for stage 0 |

## 6. Our side (what has to show the same view)

For stage 3 of T15 (the renderer comparison), the view is set in our renderer by `--png --group <g> --at <x>,<y> --size <w>x<h>` (the derived levels too, from the T3 follow-up). The camera position and zoom group of the original are taken from its memory or from the screen, and the same values go to `--at` and `--group`. The mapping between the original's camera and our tile coordinates is part of stage 3 (OQ-026).

## 7. Open questions for the owner

1. May the harness use `C:\anno1503-original\` (the copy) for all stages?
2. Agree to the smoke run in section 4 now (it shows the game on the desktop, under 5 minutes)?
3. Which windowed method, if the game's own switch does not work: dgVoodoo2 or cnc-ddraw (each needs your yes)?

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
