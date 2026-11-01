#!/usr/bin/env python3
"""dev_session_capture.py: run a scripted developer-tool session in the native Windows play window and
capture the game window while it runs (W4-WIN-4; the round-ee `screenshot` step is macOS-only).

    python tools/bench/dev_session_capture.py (needs Pillow, so not -I) <anno1503.exe> <script> <new out dir> [<size>] [<tile>]

Launches `anno1503 --play data/maps/lan5.txt --renderer gpu` with ANNO_DEV_SCRIPT=<script>,
ANNO_DEV_SESSION=<out>/record and ANNO_VIEW_TRACE=<out>/trace, then watches the window of that process
(by process id, never by title alone). Each time the title first shows `command #N: ` it waits 1.2 s
(the script holds with `wait ms` for that long) and captures, as screen pixels (GDI, so what the
desktop compositor shows; the window is raised topmost for the capture):
  <out>/shots/cmdN-client.png   the client area (the game image; the title is not in it)
  <out>/shots/cmdN-window.png   the whole window with its title bar (the DWM frame bounds)
  <out>/shots/cmdN-title.txt    the title text at the capture (GetWindowTextW)
The log (<out>/capture.log) lists every capture with the window rectangle, the client rectangle and the
DPI scale. A capture of a window that is minimised, off screen or not on top is refused and logged,
never saved as a picture of the game. Exit: the window's status, or 2 for bad arguments."""
import ctypes
import ctypes.wintypes as wt
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
        return False
    # Topmost for the capture (HWND_TOPMOST = -1; SWP_NOMOVE|SWP_NOSIZE|SWP_SHOWWINDOW = 0x43), then released.
    user32.SetWindowPos(hwnd, wt.HWND(-1), 0, 0, 0, 0, 0x43)
    time.sleep(0.4)
    title = title_of(hwnd)
    win, cl = frame_rect(hwnd), client_rect(hwnd)
    ok = True
    for suffix, box in (("client", cl), ("window", win)):
        img = ImageGrab.grab(bbox=box, all_screens=True)
        if img.size != (box[2] - box[0], box[3] - box[1]):
            ok = False
        img.save(shots / f"{name}-{suffix}.png")
    (shots / f"{name}-title.txt").write_text(title + "\n", encoding="utf-8")
    user32.SetWindowPos(hwnd, wt.HWND(-2), 0, 0, 0, 0, 0x43)  # HWND_NOTOPMOST
    log.append(f"{name}: window {win} client {cl} ({cl[2]-cl[0]}x{cl[3]-cl[1]} px) title: {title}"
               + ("" if ok else " [GRAB SIZE MISMATCH]"))
    return ok


def main(argv):
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
    with open(out / "stdout.txt", "wb") as so, open(out / "stderr.txt", "wb") as se:
        proc = subprocess.Popen([app, "--play", str(HERE / "data" / "maps" / "lan5.txt"), "--renderer", "gpu"],
                                stdout=so, stderr=se, env=env)
        log, seen, pending = [], set(), {}
        t0 = time.monotonic()
        while proc.poll() is None and time.monotonic() - t0 < 240:
            for hwnd in windows_of(proc.pid):
                m = re.search(r"command #(\d+): ", title_of(hwnd))
                if m and m.group(1) not in seen:
                    seen.add(m.group(1))
                    pending[m.group(1)] = (time.monotonic() + 1.2, hwnd)
            for n, (when, hwnd) in list(pending.items()):
                if time.monotonic() >= when:
                    capture(hwnd, f"cmd{n}", shots, log)
                    del pending[n]
            time.sleep(0.05)
        if proc.poll() is None:
            proc.kill()
        rc = proc.wait()
    (out / "capture.log").write_text("\n".join(log) + f"\nwindow_exit={rc}\n", encoding="utf-8")
    print("\n".join(log))
    return rc


if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))
