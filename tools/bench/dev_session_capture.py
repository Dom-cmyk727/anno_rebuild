#!/usr/bin/env python3
"""dev_session_capture.py: run a scripted developer-tool session in the native Windows play window and
capture the game window while it runs (W4-WIN-4; the round-ee `screenshot` step is macOS-only).

    python tools/bench/dev_session_capture.py (needs Pillow, so not -I) [--require-captures N]
        <anno1503.exe> <script> <new out dir> [<size>] [<tile>]

Launches `anno1503 --play data/maps/lan5.txt --renderer gpu` with ANNO_DEV_SCRIPT=<script>,
ANNO_DEV_SESSION=<out>/record and ANNO_VIEW_TRACE=<out>/trace, then watches the window of that process
(by process id, never by title alone). Each time the title first shows `command #N: ` it waits 1.2 s
(the script holds with `wait ms` for that long) and captures, as screen pixels (GDI, so what the
desktop compositor shows; the window is raised topmost for the capture):
  <out>/shots/cmdN-client.png   the client area (the game image; the title is not in it)
  <out>/shots/cmdN-window.png   the whole window with its title bar (the DWM frame bounds)
  <out>/shots/cmdN-title.txt    the title text at the capture (GetWindowTextW)
The log (<out>/capture.log) lists every capture with the window rectangle, the client rectangle and the
DPI scale. A capture of a minimised window, or of one whose client area is not inside the virtual desktop, is refused
and logged, never saved as a picture of the game; a grab whose size is not the rectangle's is saved only as
<name>-<part>.invalid, never under the name of a capture. Being covered by another window is handled by
raising the window topmost for the capture, not detected.

Verdict and exit status (W4-WIN-FINAL F1): a capture is requested for every command receipt the title shows
(plus --require-captures N, at least N captures must complete). The run is COMPLETE only when every requested
capture completed: none refused, none of the wrong size, none left unfinished when the window ended, and at least
N completed. The log ends with the counts, `window_exit=<the window's own status>` (kept apart from the verdict),
`capture_verdict=complete|INCOMPLETE` and `exit=<this tool's status>`. Exit: the window's own status when it is
not 0 (preserved, whatever the captures did); else 4 when the capture set is incomplete; else 0; 2 for bad
arguments. A zero exit therefore means the child ended 0 and every requested capture was taken."""
import ctypes
import ctypes.wintypes as wt
import json
import os
import re
import subprocess
import sys
import time
from pathlib import Path

from PIL import ImageGrab

user32 = ctypes.windll.user32
dwm = ctypes.windll.dwmapi
user32.SetProcessDPIAware()
HERE = Path(__file__).resolve().parents[2]
WNDENUMPROC = ctypes.WINFUNCTYPE(wt.BOOL, wt.HWND, wt.LPARAM)


def windows_of(pid):
    found = []

    def cb(hwnd, _):
        p = wt.DWORD()
        user32.GetWindowThreadProcessId(hwnd, ctypes.byref(p))
        if p.value == pid and user32.IsWindowVisible(hwnd):
            found.append(hwnd)
        return True

    user32.EnumWindows(WNDENUMPROC(cb), 0)
    return found


def title_of(hwnd):
    n = user32.GetWindowTextLengthW(hwnd)
    buf = ctypes.create_unicode_buffer(n + 1)
    user32.GetWindowTextW(hwnd, buf, n + 1)
    return buf.value


def frame_rect(hwnd):
    r = wt.RECT()
    # DWMWA_EXTENDED_FRAME_BOUNDS = 9: the visible frame, without the invisible resize border
    dwm.DwmGetWindowAttribute(hwnd, 9, ctypes.byref(r), ctypes.sizeof(r))
    return r.left, r.top, r.right, r.bottom


def client_rect(hwnd):
    c = wt.RECT()
    user32.GetClientRect(hwnd, ctypes.byref(c))
    pt = wt.POINT(0, 0)
    user32.ClientToScreen(hwnd, ctypes.byref(pt))
    return pt.x, pt.y, pt.x + c.right, pt.y + c.bottom


def capture(hwnd, name, shots, log):
    if user32.IsIconic(hwnd):
        log.append(f"{name}: REFUSED, the window is minimised")
        return "refused"
    vx, vy = user32.GetSystemMetrics(76), user32.GetSystemMetrics(77)  # SM_X/YVIRTUALSCREEN
    vw, vh = user32.GetSystemMetrics(78), user32.GetSystemMetrics(79)  # SM_CX/CYVIRTUALSCREEN
    cx0, cy0, cx1, cy1 = client_rect(hwnd)
    if cx0 < vx or cy0 < vy or cx1 > vx + vw or cy1 > vy + vh:
        # The game image must be fully on a monitor; the frame may overhang by a border (clipped below).
        log.append(f"{name}: REFUSED, the client area {(cx0, cy0, cx1, cy1)} is not inside the desktop")
        return "refused"
    # Topmost for the capture (HWND_TOPMOST = -1; SWP_NOMOVE|SWP_NOSIZE|SWP_SHOWWINDOW = 0x43), then released.
    user32.SetWindowPos(hwnd, wt.HWND(-1), 0, 0, 0, 0, 0x43)
    time.sleep(0.4)
    title = title_of(hwnd)
    fx0, fy0, fx1, fy1 = frame_rect(hwnd)
    win = (max(fx0, vx), max(fy0, vy), min(fx1, vx + vw), min(fy1, vy + vh))  # clipped to the desktop
    cl = client_rect(hwnd)
    ok = True
    for suffix, box in (("client", cl), ("window", win)):
        img = ImageGrab.grab(bbox=box, all_screens=True)
        valid = img.size == (box[2] - box[0], box[3] - box[1])
        ok = ok and valid
        # A grab of the wrong size is kept for inspection under a name no picture glob matches.
        img.save(shots / (f"{name}-{suffix}.png" if valid else f"{name}-{suffix}.invalid"), format="PNG")
    (shots / f"{name}-title.txt").write_text(title + "\n", encoding="utf-8")
    user32.SetWindowPos(hwnd, wt.HWND(-2), 0, 0, 0, 0, 0x43)  # HWND_NOTOPMOST
    log.append(f"{name}: window {win} client {cl} ({cl[2]-cl[0]}x{cl[3]-cl[1]} px) title: {title}"
               + ("" if ok else " [GRAB SIZE MISMATCH]"))
    return "ok" if ok else "mismatch"


def verdict(rc, requested, results, required, watchdog=False):
    """(exit status, log lines) of a run: `requested` command numbers, `results` {number: "ok" | "refused" |
    "mismatch"} of the captures that were attempted (a requested one without a result is unfinished), `required`
    the minimum of completed captures, `rc` the window's own status."""
    done = sum(1 for r in results.values() if r == "ok")
    refused = sum(1 for r in results.values() if r == "refused")
    mismatch = sum(1 for r in results.values() if r == "mismatch")
    unfinished = sum(1 for n in requested if n not in results)
    complete = refused == 0 and mismatch == 0 and unfinished == 0 and done >= required
    code = rc if rc != 0 else (0 if complete else 4)
    lines = [f"captures requested={len(requested)} completed={done} refused={refused} size_mismatch={mismatch} "
             f"unfinished={unfinished} required={required}",
             f"window_exit={rc}" + (" (watchdog: the window was killed)" if watchdog else ""),
             f"capture_verdict={'complete' if complete else 'INCOMPLETE'}",
             f"exit={code}"]
    return code, lines


def main(argv):
    required = 0
    if argv[:1] == ["--require-captures"] and len(argv) > 2 and argv[1].isdigit():
        required, argv = int(argv[1]), argv[2:]
    if len(argv) < 3:
        print(__doc__, file=sys.stderr)
        return 2
    app, script, out = argv[0], argv[1], Path(argv[2])
    size = argv[3] if len(argv) > 3 else "1920x1080"
    at = argv[4] if len(argv) > 4 else "1052,444"
    if out.exists() or not os.environ.get("ANNO1503_GAME_DIR"):
        print("dev_session_capture: the out dir exists, or ANNO1503_GAME_DIR is not set", file=sys.stderr)
        return 2
    shots = out / "shots"
    shots.mkdir(parents=True)
    env = dict(os.environ, ANNO_DEV_SCRIPT=script, ANNO_DEV_SESSION=str(out / "record"),
               ANNO_VIEW_TRACE=str(out / "trace"), ANNO_VIEW_SIZE=size, ANNO_VIEW_AT=at,
               ANNO_VIEW_QUIT_S="180")
    # Test hook: ANNO_CAPTURE_APP_ARGS, a JSON list, replaces the play arguments, so a contract test can
    # watch a tiny fixture window instead of the game (tools/test/window_tools_contract_test.py).
    app_args = (json.loads(os.environ["ANNO_CAPTURE_APP_ARGS"]) if "ANNO_CAPTURE_APP_ARGS" in os.environ
                else ["--play", str(HERE / "data" / "maps" / "lan5.txt"), "--renderer", "gpu"])
    with open(out / "stdout.txt", "wb") as so, open(out / "stderr.txt", "wb") as se:
        proc = subprocess.Popen([app, *app_args], stdout=so, stderr=se, env=env)
        log, seen, pending, results = [], set(), {}, {}
        t0 = time.monotonic()
        while proc.poll() is None and time.monotonic() - t0 < 240:
            for hwnd in windows_of(proc.pid):
                m = re.search(r"command #(\d+): ", title_of(hwnd))
                if m and m.group(1) not in seen:
                    seen.add(m.group(1))
                    pending[m.group(1)] = (time.monotonic() + 1.2, hwnd)
            for n, (when, hwnd) in list(pending.items()):
                if time.monotonic() >= when:
                    results[n] = capture(hwnd, f"cmd{n}", shots, log)
                    del pending[n]
            time.sleep(0.05)
        watchdog = proc.poll() is None
        if watchdog:
            proc.kill()
        rc = proc.wait()
    code, summary = verdict(rc, sorted(seen), results, required, watchdog)
    (out / "capture.log").write_text("\n".join(log + summary) + "\n", encoding="utf-8")
    print("\n".join(log + summary))
    return code


if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))
