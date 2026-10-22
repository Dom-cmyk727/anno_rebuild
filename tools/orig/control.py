"""T15 input control of the running original (owner-agreed sessions only).

Drives the game copy at C:\\anno1503-original\\ with Windows SendInput: foreground the game window,
send key presses (speeds F5-F8, pause, menu keys) and mouse moves/clicks at window-relative
coordinates, and screenshot the window. Standard library only (ctypes; no third-party tool). This
is the `SendInput` path named in docs/testing/original-harness.md and docs/testing/m2-fixtures.md,
to be used only in a session the owner agreed to.

Each sub-command finds the running game (so the game is launched once, detached, and persists
across calls), foregrounds its window and acts. A perceive-act loop from the shell:

  python -I tools/orig/control.py --game C:\\anno1503-original launch
  python -I tools/orig/control.py --game C:\\anno1503-original shot --out cache/orig/drive/s1.png
  python -I tools/orig/control.py --game C:\\anno1503-original click 0.5 0.6
  python -I tools/orig/control.py --game C:\\anno1503-original key F6
  python -I tools/orig/control.py --game C:\\anno1503-original close   # WM_CLOSE + taskkill + hashes

Coordinates for click/move are window-relative in 0..1 (0,0 = top-left of the game window,
1,1 = bottom-right); they are mapped to absolute virtual-desktop coordinates for SendInput.
"""

from __future__ import annotations

import argparse
import ctypes
import os
import sys
import time
from ctypes import wintypes

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import smoke  # noqa: E402  (sibling; reuse window find / capture / pids / hashes)

user32 = smoke.user32
kernel32 = smoke.kernel32

# --- SendInput plumbing ----------------------------------------------------
ULONG_PTR = ctypes.c_uint64 if ctypes.sizeof(ctypes.c_void_p) == 8 else ctypes.c_uint32
INPUT_MOUSE = 0
INPUT_KEYBOARD = 1
KEYEVENTF_KEYUP = 0x0002
MOUSEEVENTF_MOVE = 0x0001
MOUSEEVENTF_LEFTDOWN = 0x0002
MOUSEEVENTF_LEFTUP = 0x0004
MOUSEEVENTF_RIGHTDOWN = 0x0008
MOUSEEVENTF_RIGHTUP = 0x0010
MOUSEEVENTF_ABSOLUTE = 0x8000
MOUSEEVENTF_VIRTUALDESK = 0x4000
SM_XVIRTUALSCREEN, SM_YVIRTUALSCREEN, SM_CXVIRTUALSCREEN, SM_CYVIRTUALSCREEN = 76, 77, 78, 79


class KEYBDINPUT(ctypes.Structure):
    _fields_ = [("wVk", wintypes.WORD), ("wScan", wintypes.WORD), ("dwFlags", wintypes.DWORD),
                ("time", wintypes.DWORD), ("dwExtraInfo", ULONG_PTR)]


class MOUSEINPUT(ctypes.Structure):
    _fields_ = [("dx", wintypes.LONG), ("dy", wintypes.LONG), ("mouseData", wintypes.DWORD),
                ("dwFlags", wintypes.DWORD), ("time", wintypes.DWORD), ("dwExtraInfo", ULONG_PTR)]


class _IUNION(ctypes.Union):
    _fields_ = [("ki", KEYBDINPUT), ("mi", MOUSEINPUT)]


class INPUT(ctypes.Structure):
    _fields_ = [("type", wintypes.DWORD), ("u", _IUNION)]


def _send(*inputs: INPUT) -> None:
    arr = (INPUT * len(inputs))(*inputs)
    user32.SendInput(len(inputs), arr, ctypes.sizeof(INPUT))


# Virtual-key names we use; hex (0x74) is also accepted.
VK = {
    "F1": 0x70, "F2": 0x71, "F3": 0x72, "F4": 0x73, "F5": 0x74, "F6": 0x75, "F7": 0x76, "F8": 0x77,
    "F9": 0x78, "F10": 0x79, "F11": 0x7A, "F12": 0x7B,
    "ESC": 0x1B, "ENTER": 0x0D, "SPACE": 0x20, "TAB": 0x09, "PAUSE": 0x13, "BACK": 0x08,
    "LEFT": 0x25, "UP": 0x26, "RIGHT": 0x27, "DOWN": 0x28, "PLUS": 0xBB, "MINUS": 0xBD,
}
for _c in "0123456789":
    VK[_c] = 0x30 + int(_c)
for _i in range(26):
    VK[chr(ord("A") + _i)] = 0x41 + _i


def vk_of(name: str) -> int:
    name = name.strip()
    if name.lower().startswith("0x"):
        return int(name, 16)
    up = name.upper()
    if up in VK:
        return VK[up]
    raise SystemExit(f"unknown key: {name}")


def key_event(vk: int, up: bool) -> INPUT:
    i = INPUT(type=INPUT_KEYBOARD)
    i.u.ki = KEYBDINPUT(wVk=vk, wScan=0, dwFlags=(KEYEVENTF_KEYUP if up else 0), time=0,
                        dwExtraInfo=0)
    return i


def press_key(vk: int, hold: float = 0.05) -> None:
    _send(key_event(vk, False))
    time.sleep(hold)
    _send(key_event(vk, True))


def abs_coord(screen_x: int, screen_y: int) -> tuple[int, int]:
    """Screen pixel -> 0..65535 over the virtual desktop (for MOUSEEVENTF_ABSOLUTE|VIRTUALDESK)."""
    vx = user32.GetSystemMetrics(SM_XVIRTUALSCREEN)
    vy = user32.GetSystemMetrics(SM_YVIRTUALSCREEN)
    vw = user32.GetSystemMetrics(SM_CXVIRTUALSCREEN)
    vh = user32.GetSystemMetrics(SM_CYVIRTUALSCREEN)
    ax = round((screen_x - vx) * 65535 / max(vw - 1, 1))
    ay = round((screen_y - vy) * 65535 / max(vh - 1, 1))
    return ax, ay


def window_rect(hwnd: int) -> tuple[int, int, int, int]:
    r = smoke.RECT()
    user32.GetWindowRect(hwnd, ctypes.byref(r))
    return r.left, r.top, r.right - r.left, r.bottom - r.top


def move_to(hwnd: int, nx: float, ny: float) -> tuple[int, int]:
    left, top, w, h = window_rect(hwnd)
    sx = left + int(round(nx * w))
    sy = top + int(round(ny * h))
    ax, ay = abs_coord(sx, sy)
    i = INPUT(type=INPUT_MOUSE)
    i.u.mi = MOUSEINPUT(dx=ax, dy=ay, mouseData=0,
                        dwFlags=MOUSEEVENTF_MOVE | MOUSEEVENTF_ABSOLUTE | MOUSEEVENTF_VIRTUALDESK,
                        time=0, dwExtraInfo=0)
    _send(i)
    return sx, sy


def click_at(hwnd: int, nx: float, ny: float, button: str = "left", double: bool = False) -> None:
    move_to(hwnd, nx, ny)
    time.sleep(0.08)
    down = MOUSEEVENTF_RIGHTDOWN if button == "right" else MOUSEEVENTF_LEFTDOWN
    up = MOUSEEVENTF_RIGHTUP if button == "right" else MOUSEEVENTF_LEFTUP
    for _ in range(2 if double else 1):
        d = INPUT(type=INPUT_MOUSE)
        d.u.mi = MOUSEINPUT(dx=0, dy=0, mouseData=0, dwFlags=down, time=0, dwExtraInfo=0)
        u = INPUT(type=INPUT_MOUSE)
        u.u.mi = MOUSEINPUT(dx=0, dy=0, mouseData=0, dwFlags=up, time=0, dwExtraInfo=0)
        _send(d)
        time.sleep(0.05)
        _send(u)
        time.sleep(0.08)


def foreground(game_dir: str) -> int | None:
    w = smoke.find_game_window(game_dir)
    if not w:
        return None
    hwnd = w[0]
    user32.ShowWindow(hwnd, 5)
    user32.SetForegroundWindow(hwnd)
    user32.BringWindowToTop(hwnd)
    user32.SetWindowPos(hwnd, -1, 0, 0, 0, 0, 0x0001 | 0x0002)
    time.sleep(0.4)
    return hwnd


def main() -> int:
    ap = argparse.ArgumentParser()
    ap.add_argument("--game", required=True)
    ap.add_argument("--launcher", default="1503Startup.exe")
    sub = ap.add_subparsers(dest="cmd", required=True)
    sub.add_parser("launch")
    p_shot = sub.add_parser("shot")
    p_shot.add_argument("--out", required=True)
    p_key = sub.add_parser("key")
    p_key.add_argument("keys", help="comma-separated, e.g. F6 or ENTER,ESC")
    p_key.add_argument("--hold", type=float, default=0.05)
    p_key.add_argument("--after", type=float, default=0.3, help="seconds to wait after the keys")
    for name in ("click", "dblclick", "move"):
        pc = sub.add_parser(name)
        pc.add_argument("nx", type=float)
        pc.add_argument("ny", type=float)
        if name != "move":
            pc.add_argument("--button", choices=["left", "right"], default="left")
        pc.add_argument("--shot", help="optional PNG to capture after the action")
    sub.add_parser("close")
    args = ap.parse_args()

    try:
        user32.SetProcessDpiAwarenessContext(ctypes.c_void_p(-4))
    except Exception:
        try:
            user32.SetProcessDPIAware()
        except Exception:
            pass
    game_dir = os.path.abspath(args.game)

    if args.cmd == "launch":
        exe = os.path.join(game_dir, args.launcher)
        DETACHED = 0x00000008 | 0x00000200  # DETACHED_PROCESS | CREATE_NEW_PROCESS_GROUP
        import subprocess
        subprocess.Popen([exe], cwd=game_dir, creationflags=DETACHED, close_fds=True)
        t0 = time.time()
        while time.time() - t0 < 60:
            w = smoke.find_game_window(game_dir)
            if w:
                print(f"window: {w[1]} [{w[2]}x{w[3]}]")
                return 0
            time.sleep(0.5)
        print("no window within 60 s")
        return 1

    if args.cmd == "close":
        w = smoke.find_game_window(game_dir)
        if w:
            user32.PostMessageW(w[0], smoke.WM_CLOSE, 0, 0)
        for _ in range(40):
            if not smoke.pids_under(game_dir):
                break
            time.sleep(0.5)
        import subprocess
        for pid in smoke.pids_under(game_dir):
            subprocess.run(["taskkill", "/T", "/F", "/PID", str(pid)], capture_output=True)
        time.sleep(1.0)
        left = smoke.pids_under(game_dir)
        same = {n: smoke.sha256(os.path.join(game_dir, n))
                for n in ("1503Startup.exe", "AnnoWelt.dll", "AnnoFrame.dll")}
        print({"left_running": left, "sha256": same})
        return 0

    hwnd = foreground(game_dir)
    if hwnd is None:
        print("game window not found (launch first)")
        return 1

    if args.cmd == "shot":
        cw, ch, bgra = smoke.capture(hwnd)
        if not (cw and ch):
            print("capture empty")
            return 1
        os.makedirs(os.path.dirname(os.path.abspath(args.out)), exist_ok=True)
        smoke.write_png(args.out, cw, ch, bgra)
        print({"file": args.out, "size": [cw, ch], "nonblack": smoke.nonblack(bgra)})
        return 0

    if args.cmd == "key":
        for name in args.keys.split(","):
            press_key(vk_of(name), hold=args.hold)
            time.sleep(0.1)
        time.sleep(args.after)
        print({"sent": args.keys})
        return 0

    if args.cmd in ("click", "dblclick", "move"):
        if args.cmd == "move":
            sx, sy = move_to(hwnd, args.nx, args.ny)
            print({"moved_to_screen": [sx, sy]})
        else:
            click_at(hwnd, args.nx, args.ny, button=args.button, double=(args.cmd == "dblclick"))
            print({"clicked": [args.nx, args.ny], "button": args.button,
                   "double": args.cmd == "dblclick"})
        if getattr(args, "shot", None):
            time.sleep(0.4)
            hwnd2 = foreground(game_dir) or hwnd
            cw, ch, bgra = smoke.capture(hwnd2)
            if cw and ch:
                os.makedirs(os.path.dirname(os.path.abspath(args.shot)), exist_ok=True)
                smoke.write_png(args.shot, cw, ch, bgra)
                print({"shot": args.shot, "size": [cw, ch], "nonblack": smoke.nonblack(bgra)})
        return 0
    return 0


if __name__ == "__main__":
    sys.exit(main())
