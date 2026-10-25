"""T15 input control of the running original (owner-agreed sessions only).

Drives the game copy at C:\\anno1503-original\\ with Windows SendInput: foreground the game window,
send key presses (speeds F5-F8, pause, menu keys) and mouse moves/clicks at window-relative
coordinates, and screenshot the window. Standard library only (ctypes; no third-party tool). This
is the `SendInput` path named in docs/testing/original-harness.md and docs/testing/m2-fixtures.md,
to be used only in a session the owner agreed to.

**Safety (this is a shared PC).** Before EVERY SendInput the tool asserts the game is the foreground
window, that the owner has not taken the mouse (the cursor is where the tool last left it), and that
a mouse target lies inside the game's client area; it aborts (exit 3) otherwise, so a stray key or
click can never land in a terminal. It never leaves the game topmost (restores HWND_NOTOPMOST), and
every action is logged to cache/orig/<session>/actions.jsonl. `launch` refuses a second instance.

The last cursor position is persisted to cache/orig/<session>/cursor.json, so the "owner took the
mouse" check also works ACROSS separate control.py calls — each call checks the cursor the previous
one left. `--session` defaults to a stable per-day name (drive-YYYYMMDD), so the cross-call guard is
on by default and never silently off; only the very first action of a day has no prior cursor to
check (nothing to compare against yet). Override with an explicit `--session` to isolate a run.

Each sub-command finds the running game (so the game is launched once, detached, and persists
across calls), foregrounds its window and acts. A perceive-act loop from the shell:

  python -I tools/orig/control.py --game C:\\anno1503-original launch
  python -I tools/orig/control.py --game C:\\anno1503-original shot --out cache/orig/drive/s1.png
  python -I tools/orig/control.py --game C:\\anno1503-original click 0.5 0.6
  python -I tools/orig/control.py --game C:\\anno1503-original key F6
  python -I tools/orig/control.py --game C:\\anno1503-original --session s1 quicksave   # F10
  python -I tools/orig/control.py --game C:\\anno1503-original --session s1 quickload   # F9
  python -I tools/orig/control.py --game C:\\anno1503-original close   # WM_CLOSE + taskkill + hashes

Coordinates for click/move are client-relative in 0..1 (0,0 = top-left of the game's client area,
1,1 = bottom-right); they are mapped to absolute virtual-desktop coordinates for SendInput.
"""

from __future__ import annotations

import argparse
import ctypes
import datetime
import json
import os
import subprocess
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
MOUSEEVENTF_WHEEL = 0x0800
SM_XVIRTUALSCREEN, SM_YVIRTUALSCREEN, SM_CXVIRTUALSCREEN, SM_CYVIRTUALSCREEN = 76, 77, 78, 79
HWND_NOTOPMOST = -2
SWP_NOMOVE_NOSIZE = 0x0001 | 0x0002
SPI_SETFOREGROUNDLOCKTIMEOUT = 0x2001
SW_SHOW, SW_RESTORE = 5, 9


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


class POINT(ctypes.Structure):
    _fields_ = [("x", wintypes.LONG), ("y", wintypes.LONG)]


def _send(*inputs: INPUT) -> None:
    arr = (INPUT * len(inputs))(*inputs)
    user32.SendInput(len(inputs), arr, ctypes.sizeof(INPUT))


# --- safety: never send input to the wrong window (shared PC) --------------
_LAST_CURSOR: tuple[int, int] | None = None   # where this process last put the cursor
_LOG_PATH: str | None = None                  # cache/orig/<session>/actions.jsonl
_CURSOR_PATH: str | None = None               # cache/orig/<session>/cursor.json (cross-call state)


def cursor_pos() -> tuple[int, int]:
    p = POINT()
    user32.GetCursorPos(ctypes.byref(p))
    return p.x, p.y


def client_box(hwnd: int) -> tuple[int, int, int, int]:
    """(left, top, width, height) of the window's CLIENT area, in screen pixels (not the window
    frame — GetClientRect + ClientToScreen, so a bordered/scaled window maps 1:1 to the capture)."""
    r = smoke.RECT()
    user32.GetClientRect(hwnd, ctypes.byref(r))
    o = POINT(0, 0)
    user32.ClientToScreen(hwnd, ctypes.byref(o))
    return o.x, o.y, r.right - r.left, r.bottom - r.top


def window_title(hwnd: int) -> str:
    if not hwnd:
        return ""
    n = user32.GetWindowTextLengthW(hwnd)
    buf = ctypes.create_unicode_buffer(n + 1)
    user32.GetWindowTextW(hwnd, buf, n + 1)
    return buf.value


def restore_notopmost(hwnd: int | None) -> None:
    if hwnd:
        user32.SetWindowPos(hwnd, HWND_NOTOPMOST, 0, 0, 0, 0, SWP_NOMOVE_NOSIZE)


def log_action(kind: str, **fields) -> None:
    if not _LOG_PATH:
        return
    rec = {"t": datetime.datetime.now().isoformat(timespec="milliseconds"), "kind": kind}
    rec.update(fields)
    try:
        with open(_LOG_PATH, "a", encoding="utf-8") as f:
            f.write(json.dumps(rec) + "\n")
    except OSError:
        pass


def set_last_cursor(pos: tuple[int, int]) -> None:
    """Record where the tool left the cursor, both in this process and in the session's
    cursor.json, so a *later* control.py call in the same `--session` can tell whether the owner
    moved the mouse in between (the single-process cursor check alone cannot; review 2026-10-22 e).
    Cross-call checking is active only when a stable `--session` is passed; with the default
    per-call timestamp session each call gets a fresh dir and no prior cursor, as before."""
    global _LAST_CURSOR
    _LAST_CURSOR = pos
    if _CURSOR_PATH:
        try:
            with open(_CURSOR_PATH, "w", encoding="utf-8") as f:
                json.dump({"screen": [pos[0], pos[1]],
                           "t": datetime.datetime.now().isoformat(timespec="milliseconds")}, f)
        except OSError:
            pass


def load_last_cursor() -> tuple[int, int] | None:
    """The cursor the previous call in this session left (None if no session file yet)."""
    if not _CURSOR_PATH or not os.path.exists(_CURSOR_PATH):
        return None
    try:
        with open(_CURSOR_PATH, encoding="utf-8") as f:
            s = json.load(f)["screen"]
        return int(s[0]), int(s[1])
    except (OSError, ValueError, KeyError, TypeError, IndexError):
        return None


def abort(hwnd: int, reason: str) -> None:
    """Refuse to send input and exit loudly — the game is not safely in focus."""
    log_action("abort", reason=reason)
    restore_notopmost(hwnd)
    print(f"ABORT (no input sent): {reason}", file=sys.stderr)
    sys.exit(3)


def guard(hwnd: int, kind: str, target_screen: tuple[int, int] | None = None) -> None:
    """Pre-condition for every SendInput: the game is the foreground window, the owner has not
    taken the mouse since the last action, and a mouse target is inside the game client area."""
    fg = user32.GetForegroundWindow()
    if fg != hwnd:
        abort(hwnd, f"game is not the foreground window (fg={fg} {window_title(fg)!r}, "
                    f"game={hwnd} {window_title(hwnd)!r}); SetForegroundWindow probably failed")
    if _LAST_CURSOR is not None:
        cx, cy = cursor_pos()
        if abs(cx - _LAST_CURSOR[0]) > 4 or abs(cy - _LAST_CURSOR[1]) > 4:
            # The game warps the cursor itself (measured 2026-10-25 with no input at all: it
            # alternates between the centre of its 800x600 surface and the screen centre about
            # once a second, and lands 20-25 px off after a click), so a moved cursor INSIDE the
            # game's client area is the game's doing, not a hand on the mouse. Only a cursor that
            # left the client area is treated as the owner taking the mouse; the foreground check
            # above and the target check below keep every input inside the game.
            left, top, w, h = client_box(hwnd)
            inside = left <= cx < left + w and top <= cy < top + h
            if not inside:
                abort(hwnd, f"cursor moved since the last action and left the game client area "
                            f"(owner took the mouse): expected {_LAST_CURSOR}, got {(cx, cy)}")
            log_action("cursor_warp", expected=list(_LAST_CURSOR), got=[cx, cy])
    if target_screen is not None:
        left, top, w, h = client_box(hwnd)
        if not (left <= target_screen[0] < left + w and top <= target_screen[1] < top + h):
            abort(hwnd, f"target {target_screen} is outside the game client rect "
                        f"(left={left}, top={top}, w={w}, h={h})")


# Virtual-key names we use; hex (0x74) is also accepted.
VK = {
    "F1": 0x70, "F2": 0x71, "F3": 0x72, "F4": 0x73, "F5": 0x74, "F6": 0x75, "F7": 0x76, "F8": 0x77,
    "F9": 0x78, "F10": 0x79, "F11": 0x7A, "F12": 0x7B,
    "ESC": 0x1B, "ENTER": 0x0D, "SPACE": 0x20, "TAB": 0x09, "PAUSE": 0x13, "BACK": 0x08,
    "LEFT": 0x25, "UP": 0x26, "RIGHT": 0x27, "DOWN": 0x28, "PLUS": 0xBB, "MINUS": 0xBD,
    "CTRL": 0x11, "ALT": 0x12, "SHIFT": 0x10, "B": 0x42, "P": 0x50, "D": 0x44, "C": 0x43,
    "S": 0x53, "Q": 0x51, "O": 0x4F, "H": 0x48, "N": 0x4E, "M": 0x4D, "J": 0x4A, "W": 0x57,
    "X": 0x58, "Y": 0x59, "DOT": 0xBE, "COMMA": 0xBC,
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


def press_key(hwnd: int, vk: int, hold: float = 0.05) -> None:
    guard(hwnd, "key")
    _send(key_event(vk, False))
    time.sleep(hold)
    _send(key_event(vk, True))
    set_last_cursor(cursor_pos())
    log_action("key", vk=vk, cursor=list(_LAST_CURSOR))


def abs_coord(screen_x: int, screen_y: int) -> tuple[int, int]:
    """Screen pixel -> 0..65535 over the virtual desktop (for MOUSEEVENTF_ABSOLUTE|VIRTUALDESK)."""
    vx = user32.GetSystemMetrics(SM_XVIRTUALSCREEN)
    vy = user32.GetSystemMetrics(SM_YVIRTUALSCREEN)
    vw = user32.GetSystemMetrics(SM_CXVIRTUALSCREEN)
    vh = user32.GetSystemMetrics(SM_CYVIRTUALSCREEN)
    ax = round((screen_x - vx) * 65535 / max(vw - 1, 1))
    ay = round((screen_y - vy) * 65535 / max(vh - 1, 1))
    return ax, ay


def move_to(hwnd: int, nx: float, ny: float) -> tuple[int, int]:
    left, top, w, h = client_box(hwnd)
    sx = left + int(round(nx * w))
    sy = top + int(round(ny * h))
    guard(hwnd, "move", (sx, sy))
    ax, ay = abs_coord(sx, sy)
    i = INPUT(type=INPUT_MOUSE)
    i.u.mi = MOUSEINPUT(dx=ax, dy=ay, mouseData=0,
                        dwFlags=MOUSEEVENTF_MOVE | MOUSEEVENTF_ABSOLUTE | MOUSEEVENTF_VIRTUALDESK,
                        time=0, dwExtraInfo=0)
    _send(i)
    time.sleep(0.02)
    set_last_cursor(cursor_pos())   # record where it actually landed
    log_action("move", nx=nx, ny=ny, screen=[sx, sy], cursor=list(_LAST_CURSOR))
    return sx, sy


def click_at(hwnd: int, nx: float, ny: float, button: str = "left", double: bool = False) -> None:
    move_to(hwnd, nx, ny)
    time.sleep(0.08)
    down = MOUSEEVENTF_RIGHTDOWN if button == "right" else MOUSEEVENTF_LEFTDOWN
    up = MOUSEEVENTF_RIGHTUP if button == "right" else MOUSEEVENTF_LEFTUP
    for _ in range(2 if double else 1):
        guard(hwnd, "click")   # the cursor must still be where move_to put it
        d = INPUT(type=INPUT_MOUSE)
        d.u.mi = MOUSEINPUT(dx=0, dy=0, mouseData=0, dwFlags=down, time=0, dwExtraInfo=0)
        u = INPUT(type=INPUT_MOUSE)
        u.u.mi = MOUSEINPUT(dx=0, dy=0, mouseData=0, dwFlags=up, time=0, dwExtraInfo=0)
        _send(d)
        time.sleep(0.05)
        _send(u)
        time.sleep(0.08)
    set_last_cursor(cursor_pos())
    log_action("click", nx=nx, ny=ny, button=button, double=double, cursor=list(_LAST_CURSOR))


def wheel(hwnd: int, notches: int) -> None:
    """Mouse wheel at the current cursor position; +in / -out notches (zoom)."""
    guard(hwnd, "wheel")
    i = INPUT(type=INPUT_MOUSE)
    i.u.mi = MOUSEINPUT(dx=0, dy=0, mouseData=(notches * 120) & 0xFFFFFFFF,
                        dwFlags=MOUSEEVENTF_WHEEL, time=0, dwExtraInfo=0)
    _send(i)
    set_last_cursor(cursor_pos())
    log_action("wheel", notches=notches, cursor=list(_LAST_CURSOR))


def drag(hwnd: int, nx0: float, ny0: float, nx1: float, ny1: float) -> None:
    """Press at (nx0,ny0), move to (nx1,ny1), release — for laying a road/path."""
    move_to(hwnd, nx0, ny0)
    time.sleep(0.1)
    guard(hwnd, "drag-down")
    d = INPUT(type=INPUT_MOUSE)
    d.u.mi = MOUSEINPUT(dx=0, dy=0, mouseData=0, dwFlags=MOUSEEVENTF_LEFTDOWN, time=0, dwExtraInfo=0)
    _send(d)
    time.sleep(0.1)
    move_to(hwnd, nx1, ny1)   # guards; updates _LAST_CURSOR to the end point
    time.sleep(0.1)
    guard(hwnd, "drag-up")
    u = INPUT(type=INPUT_MOUSE)
    u.u.mi = MOUSEINPUT(dx=0, dy=0, mouseData=0, dwFlags=MOUSEEVENTF_LEFTUP, time=0, dwExtraInfo=0)
    _send(u)
    time.sleep(0.1)
    set_last_cursor(cursor_pos())
    log_action("drag", a=[nx0, ny0], b=[nx1, ny1], cursor=list(_LAST_CURSOR))


def find_game_window_any(game_dir: str) -> tuple[int, str] | None:
    """The game's top-level window by pid + title, INCLUDING when it is minimized (iconic). A
    full-screen DirectDraw/dgVoodoo2 game minimizes itself on focus loss; smoke.find_game_window
    filters a minimized window out by its tiny off-screen rect, which would make every later call
    fail with 'game window not found'. This finder keeps it so foreground() can restore it."""
    pids = set(smoke.pids_under(game_dir))
    hits: list[tuple[int, str]] = []

    def cb(hwnd, _lp):
        pid = wintypes.DWORD()
        user32.GetWindowThreadProcessId(hwnd, ctypes.byref(pid))
        if pid.value in pids and user32.IsWindowVisible(hwnd):
            title = window_title(hwnd)
            if title:
                hits.append((hwnd, title))
        return True

    user32.EnumWindows(smoke.EnumWindowsProc(cb), 0)
    for hwnd, title in hits:
        if title.strip().lower().startswith("anno"):
            return hwnd, title
    return hits[0] if hits else None


def foreground(game_dir: str, settle: float = 0.2) -> int | None:
    """Bring the game to the foreground, restoring it if it minimized itself on a previous focus
    loss (no persistent topmost — the guard enforces focus). A full-screen DirectDraw game minimizes
    when it loses focus and refuses a plain background SetForegroundWindow, so drop the foreground-
    lock timeout and send an ALT tap first (that gives this thread the right to change the foreground
    window), then SW_RESTORE if iconic."""
    global _LAST_CURSOR
    try:
        user32.SystemParametersInfoW(SPI_SETFOREGROUNDLOCKTIMEOUT, 0, None, 0x3)
    except Exception:
        pass
    w = find_game_window_any(game_dir)
    if not w:
        return None
    hwnd = w[0]
    h = ctypes.c_void_p(hwnd)   # type the handle so the Win32 calls below take it correctly
    user32.keybd_event(0x12, 0, 0, 0)   # ALT down: unlocks SetForegroundWindow for this thread
    time.sleep(0.02)
    user32.keybd_event(0x12, 0, 2, 0)   # ALT up
    user32.ShowWindow(h, SW_RESTORE if user32.IsIconic(h) else SW_SHOW)
    user32.SetForegroundWindow(h)
    user32.BringWindowToTop(h)
    time.sleep(settle)
    _LAST_CURSOR = None   # focus just changed; don't treat the pre-focus cursor as "ours"
    return hwnd


def capture_png(game_dir: str, hwnd: int, path: str) -> dict:
    """Screenshot via smoke.capture, then restore NOTOPMOST (smoke.capture sets topmost)."""
    hwnd2 = foreground(game_dir) or hwnd
    cw, ch, bgra = smoke.capture(hwnd2)
    restore_notopmost(hwnd2)
    if not (cw and ch):
        return {"capture": "empty"}
    os.makedirs(os.path.dirname(os.path.abspath(path)), exist_ok=True)
    smoke.write_png(path, cw, ch, bgra)
    log_action("shot", file=path, size=[cw, ch])
    return {"shot": path, "size": [cw, ch], "nonblack": smoke.nonblack(bgra)}


def run_actions(hwnd: int, game_dir: str, actions: list[str], gap: float) -> None:
    """A batch of actions, one foreground, small gaps. Tokens: `key:F7`, `click:nx,ny`,
    `right:nx,ny`, `dbl:nx,ny`, `move:nx,ny`, `wheel:-3`, `drag:nx0,ny0,nx1,ny1`."""
    for a in actions:
        kind, _, rest = a.partition(":")
        if kind == "key":
            for name in rest.split(","):
                press_key(hwnd, vk_of(name), hold=0.03)
                time.sleep(0.05)
        elif kind == "wheel":
            move_to(hwnd, 0.42, 0.42)
            time.sleep(0.05)
            wheel(hwnd, int(rest))
        elif kind == "drag":
            nx0, ny0, nx1, ny1 = (float(v) for v in rest.split(","))
            drag(hwnd, nx0, ny0, nx1, ny1)
        elif kind in ("click", "right", "dbl", "move"):
            nx, ny = (float(v) for v in rest.split(","))
            if kind == "move":
                move_to(hwnd, nx, ny)
            elif kind == "dbl":
                click_at(hwnd, nx, ny, double=True)
            else:
                click_at(hwnd, nx, ny, button=("right" if kind == "right" else "left"))
        time.sleep(gap)


def main() -> int:
    global _LOG_PATH, _CURSOR_PATH, _LAST_CURSOR
    ap = argparse.ArgumentParser()
    ap.add_argument("--game", required=True)
    ap.add_argument("--launcher", default="1503Startup.exe")
    ap.add_argument("--session", default=None,
                    help="log actions to cache/orig/<session>/actions.jsonl (default: a stable "
                         "per-day name drive-YYYYMMDD, so the cross-call cursor guard is always on)")
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
    p_qs = sub.add_parser("quicksave",
                          help="press F10 (quick save). QUIRK: a tavern's alcohol and goods on "
                               "carts are not saved (playbook 13); quick-save at calm moments.")
    p_qs.add_argument("--shot", default=None)
    p_qs.add_argument("--after", type=float, default=1.0, help="seconds to wait for the save")
    p_ql = sub.add_parser("quickload", help="press F9 (quick load)")
    p_ql.add_argument("--shot", default=None)
    p_ql.add_argument("--after", type=float, default=2.0, help="seconds to wait for the load")
    p_seq = sub.add_parser("seq")
    p_seq.add_argument("actions", nargs="+",
                       help="e.g. key:F7 wheel:-4 click:0.3,0.4 right:0.3,0.4 drag:0.3,0.4,0.2,0.3")
    p_seq.add_argument("--gap", type=float, default=0.15)
    p_seq.add_argument("--shot", default=None)
    p_wheel = sub.add_parser("wheel")
    p_wheel.add_argument("notches", type=int, help="negative = zoom out")
    p_wheel.add_argument("--shot", default=None)
    p_drag = sub.add_parser("drag")
    for n in ("nx0", "ny0", "nx1", "ny1"):
        p_drag.add_argument(n, type=float)
    p_drag.add_argument("--shot", default=None)
    args = ap.parse_args()

    try:
        user32.SetProcessDpiAwarenessContext(ctypes.c_void_p(-4))
    except Exception:
        try:
            user32.SetProcessDPIAware()
        except Exception:
            pass
    game_dir = os.path.abspath(args.game)
    # A stable per-day session by default (not a per-call timestamp), so the cross-call cursor
    # guard is on by default and never silently off (orchestrator, round e). Override with --session.
    session = args.session or datetime.datetime.now().strftime("drive-%Y%m%d")
    log_dir = os.path.join("cache", "orig", session)
    os.makedirs(log_dir, exist_ok=True)
    _LOG_PATH = os.path.join(log_dir, "actions.jsonl")
    _CURSOR_PATH = os.path.join(log_dir, "cursor.json")
    log_action("cmd", argv=sys.argv[1:], game=game_dir)

    if args.cmd == "launch":
        running = smoke.pids_under(game_dir)
        if running:
            print(f"refusing to launch: an instance is already running under {game_dir} "
                  f"(pids {running}); close it first")
            return 2
        exe = os.path.join(game_dir, args.launcher)
        DETACHED = 0x00000008 | 0x00000200  # DETACHED_PROCESS | CREATE_NEW_PROCESS_GROUP
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
            restore_notopmost(w[0])
            user32.PostMessageW(w[0], smoke.WM_CLOSE, 0, 0)
        for _ in range(40):
            if not smoke.pids_under(game_dir):
                break
            time.sleep(0.5)
        for pid in smoke.pids_under(game_dir):
            subprocess.run(["taskkill", "/T", "/F", "/PID", str(pid)], capture_output=True)
        time.sleep(1.0)
        left = smoke.pids_under(game_dir)
        # Compare ALL game files against game/app by SHA-256 (not just three) and fail on a diff.
        diffs = smoke.verify_copy_unchanged(game_dir)
        log_action("close", left_running=left, copy_diffs=diffs)
        print({"left_running": left, "copy_unchanged": not diffs, "diffs": diffs})
        return 0 if (not left and not diffs) else 1

    hwnd = foreground(game_dir)
    if hwnd is None:
        print("game window not found (launch first)")
        return 1
    # Cross-call safety: adopt the cursor the previous call in this session left, so the FIRST
    # input of this call is still checked against "the owner has not moved the mouse" (the single
    # process already resets to None in foreground()). Only has an effect with a stable --session.
    prev = load_last_cursor()
    if prev is not None:
        _LAST_CURSOR = prev
        log_action("cursor_resume", screen=list(prev))
    try:
        if args.cmd == "shot":
            print(capture_png(game_dir, hwnd, args.out))
            return 0
        if args.cmd == "key":
            for name in args.keys.split(","):
                press_key(hwnd, vk_of(name), hold=args.hold)
                time.sleep(0.1)
            time.sleep(args.after)
            print({"sent": args.keys})
            return 0
        if args.cmd in ("quicksave", "quickload"):
            vk = VK["F10"] if args.cmd == "quicksave" else VK["F9"]
            press_key(hwnd, vk)
            time.sleep(args.after)   # the game writes/reads the quick-save slot
            log_action(args.cmd)
            print({args.cmd: True, "key": "F10" if args.cmd == "quicksave" else "F9"})
            if getattr(args, "shot", None):
                time.sleep(0.3)
                print(capture_png(game_dir, hwnd, args.shot))
            return 0
        if args.cmd in ("click", "dblclick", "move"):
            if args.cmd == "move":
                sx, sy = move_to(hwnd, args.nx, args.ny)
                print({"moved_to_screen": [sx, sy]})
            else:
                click_at(hwnd, args.nx, args.ny, button=args.button,
                         double=(args.cmd == "dblclick"))
                print({"clicked": [args.nx, args.ny], "button": args.button,
                       "double": args.cmd == "dblclick"})
            if getattr(args, "shot", None):
                time.sleep(0.4)
                print(capture_png(game_dir, hwnd, args.shot))
            return 0
        if args.cmd == "wheel":
            move_to(hwnd, 0.42, 0.42)
            time.sleep(0.05)
            wheel(hwnd, args.notches)
            print({"wheel": args.notches})
        elif args.cmd == "seq":
            run_actions(hwnd, game_dir, args.actions, args.gap)
            print({"ran": len(args.actions)})
        elif args.cmd == "drag":
            drag(hwnd, args.nx0, args.ny0, args.nx1, args.ny1)
            print({"drag": [args.nx0, args.ny0, args.nx1, args.ny1]})
        if getattr(args, "shot", None):
            time.sleep(0.3)
            print(capture_png(game_dir, hwnd, args.shot))
        return 0
    finally:
        restore_notopmost(hwnd)


if __name__ == "__main__":
    sys.exit(main())
