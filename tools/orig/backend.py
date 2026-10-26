"""One backend interface for driving and reading the original (T16, original-wine.md section 4.1).

The play layer, the fixtures and the runner scripts talk to a Backend and never to Win32 or X
directly, so the same macro runs against the owner's Windows copy and the isolated Wine runner.

Interface (frame coordinates are FRACTIONS 0..1 of the game frame, as in tools/orig/screens.json):

  start() / stop()                      launch (Wine: disposable copy, own display) / stop and clean up
  key(name, hold=0)                     play-layer key names: "F10", "B", "ESC", "DOT", "CTRL+X", ...
  click(nx, ny, button=1, count=1)      move(nx, ny)    drag(nx, ny, nx2, ny2)    wheel(nx, ny, amount)
  screenshot(name) -> path              recognize() -> {"screen": ..., "scores": ...}
  quicksave(keep_as=None) / quickload()
  read(addr, n) -> bytes | None         module_bases() -> {"annowelt.dll": base, ...}
  state(what="world"|"prng"|"cities"|"players"|"all"|"figures")   (tools/orig/gamestate.py readers;
                                                                   "figures" is not in "all")

Backends:
  WineBackend     tools/orig/wine/agent.py inside the AnnoHarness WSL distro, spoken to over the
                  `wsl -e` pipe as JSON lines (no shell between Windows and the agent). The Wine
                  runner adds poll_start/poll_stop (frame-time poller), sample_start/sample_saves/
                  sample_stop (JSONL sampler) and status() with crash and audio-guard reports.
                  A command that times out here keeps running in the agent; later commands queue
                  behind it, so a timeout usually means: stop and start a new instance.
  WindowsBackend  today's ctypes code: memory and capture from tools/orig/smoke.py, input from
                  tools/orig/control.py (T15, PR #16; it carries the B1 foreground/cursor guard).
                  Until that file is on the branch, the input methods raise NotImplementedError.

Standard library only (Pillow for recognize on Windows).
"""

from __future__ import annotations

import json
import os
import queue
import subprocess
import sys
import threading
import time

_HERE = os.path.dirname(os.path.abspath(__file__))
REPO = os.path.dirname(os.path.dirname(_HERE))
DISTRO = "AnnoHarness"
CREATE_NO_WINDOW = 0x08000000      # Windows: never open a console window for wsl.exe


class BackendError(RuntimeError):
    pass


def to_wsl_path(path: str) -> str:
    """C:\\Users\\x -> /mnt/c/Users/x (the distro sees Windows drives under /mnt)."""
    p = os.path.abspath(path)
    drive, rest = os.path.splitdrive(p)
    if not drive:
        return p.replace("\\", "/")
    return f"/mnt/{drive[0].lower()}" + rest.replace("\\", "/")


class WineBackend:
    """Client of tools/orig/wine/agent.py over `wsl -d AnnoHarness -u root -e python3 agent.py serve`.
    Outputs land in cache/orig/<session>/d<display>-<start time>/ (one folder per backend)."""

    def __init__(self, display: str = ":91", session: str | None = None, run: str | None = None,
                 keep: bool = False, copy: str = "overlay", distro: str = DISTRO, timeout: float = 120.0,
                 audio: bool = False, winedebug: str | None = None):
        self.display = display
        self.session = session or time.strftime("wine-%Y%m%d")
        stamp = time.strftime("%Y%m%d-%H%M%S")
        self.out_dir = os.path.join(REPO, "cache", "orig", self.session, f"d{display.lstrip(':')}-{stamp}")
        os.makedirs(self.out_dir, exist_ok=True)
        self.timeout = timeout
        self._id = 0
        self._lines: queue.Queue = queue.Queue()
        self.noise: list[str] = []          # non-JSON lines from wsl.exe or the agent's stdout
        cmd = ["wsl.exe", "-d", distro, "-u", "root", "-e", "python3",
               to_wsl_path(os.path.join(_HERE, "wine", "agent.py")), "serve",
               "--display", display, "--out-win", to_wsl_path(self.out_dir), "--copy", copy]
        if run:
            cmd += ["--run", run]
        if keep:
            cmd += ["--keep"]
        if audio:
            cmd += ["--audio"]
        if winedebug:
            cmd += ["--winedebug", winedebug]
        self._err = open(os.path.join(self.out_dir, "agent.stderr.log"), "ab")
        flags = CREATE_NO_WINDOW if sys.platform == "win32" else 0
        self.proc = subprocess.Popen(cmd, stdin=subprocess.PIPE, stdout=subprocess.PIPE,
                                     stderr=self._err, creationflags=flags)
        threading.Thread(target=self._reader, daemon=True).start()
        try:
            hello = self._next(60.0)
        except BackendError as ex:
            raise BackendError(f"agent did not start: {ex}; output: {self.noise[-5:]}") from None
        if not hello.get("ok"):
            raise BackendError(f"agent did not start: {hello}")
        self.info = hello["result"]
        self.last_host_t = time.perf_counter()

    def _reader(self) -> None:
        for raw in iter(self.proc.stdout.readline, b""):
            self._lines.put(raw)
        self._lines.put(None)

    def _next(self, timeout: float) -> dict:
        while True:
            try:
                raw = self._lines.get(timeout=timeout)
            except queue.Empty:
                raise BackendError(f"agent gave no answer within {timeout} s") from None
            if raw is None:
                raise BackendError("agent exited (see agent.stderr.log)")
            try:
                msg = json.loads(raw.decode("utf-8"))
                if isinstance(msg, dict):
                    return msg
            except (UnicodeDecodeError, ValueError):
                pass
            # wsl.exe's own messages are UTF-16; keep a readable form for the error report
            self.noise.append(raw.decode("utf-16-le", "replace") if b"\0" in raw else raw.decode("latin-1"))

    def call(self, cmd: str, args: dict | None = None, *, timeout: float | None = None, **kw):
        """Run one agent command. Agent arguments go in `args` (or as keywords, except the names
        `cmd` and `timeout`, which are this method's); `timeout` is how long to wait here."""
        payload = dict(args or {})
        payload.update(kw)
        self._id += 1
        msg = json.dumps({"id": self._id, "cmd": cmd, "args": payload}) + "\n"
        try:
            self.proc.stdin.write(msg.encode("utf-8"))
            self.proc.stdin.flush()
        except OSError as ex:
            raise BackendError(f"{cmd}: agent pipe closed ({ex})") from None
        while True:
            rep = self._next(timeout or self.timeout)
            if rep.get("id") == self._id:
                break
        self.last_host_t = time.perf_counter()
        if not rep.get("ok"):
            raise BackendError(f"{cmd}: {rep.get('error')}")
        return rep["result"]

    # interface
    def start(self, wait_game: float = 120.0) -> dict:
        res = {"prepare": self.call("prepare"), "launch": self.call("launch")}
        res["pid"] = self.call("find_game", {"wait": wait_game}, timeout=wait_game + 10)
        if res["pid"] is None:
            raise BackendError(f"no game process within {wait_game} s (see wine.log in the run folder)")
        return res

    def stop(self) -> dict:
        res: dict = {}
        try:
            res = self.call("stop", timeout=180)
        except BackendError as ex:
            res = {"stop_error": str(ex)}
        finally:
            try:
                self.proc.stdin.close()
            except OSError:
                pass
            try:
                self.proc.wait(timeout=60)
            except subprocess.TimeoutExpired:
                self.proc.kill()
                res["agent_killed"] = True
            self._err.close()
        return res

    def key(self, name: str, hold: float = 0.0):
        return self.call("key", {"name": name, "hold": hold})

    def click(self, nx: float, ny: float, button: int = 1, count: int = 1):
        return self.call("click", {"nx": nx, "ny": ny, "button": button, "count": count})

    def move(self, nx: float, ny: float):
        return self.call("move", {"nx": nx, "ny": ny})

    def drag(self, nx: float, ny: float, nx2: float, ny2: float):
        return self.call("drag", {"nx": nx, "ny": ny, "nx2": nx2, "ny2": ny2})

    def wheel(self, nx: float, ny: float, amount: int):
        return self.call("wheel", {"nx": nx, "ny": ny, "amount": amount})

    def screenshot(self, name: str, full: bool = False, box=None, scale: int = 1) -> str:
        dest = to_wsl_path(os.path.join(self.out_dir, "shots", f"{name}.png"))
        self.call("screenshot", {"name": name, "dest": dest, "full": full, "box": box, "scale": scale})
        return os.path.join(self.out_dir, "shots", f"{name}.png")

    def recognize(self) -> dict:
        return self.call("recognize")

    def learn(self, name: str, regions=None, max_dist: int = 24) -> dict:
        return self.call("learn", {"name": name, "regions": regions, "max_dist": max_dist})

    def quicksave(self, keep_as: str | None = None) -> dict:
        return self.call("quicksave", {"keep_as": keep_as}, timeout=60)

    def quickload(self) -> dict:
        return self.call("quickload")

    def read(self, addr: int, n: int) -> bytes | None:
        h = self.call("read", {"addr": addr, "n": n})
        return bytes.fromhex(h) if h is not None else None

    def module_bases(self) -> dict:
        return {k: int(v, 16) for k, v in self.call("module_bases").items()}

    def state(self, what: str = "world") -> dict:
        return self.call("state", {"what": what}, timeout=300)

    # Wine runner extras
    def poll_start(self, label: str):
        return self.call("poll_start", {"label": label})

    def poll_stop(self):
        return self.call("poll_stop", timeout=60)

    def sample_start(self, period_game_ms: int = 60000, save_every: int = 0, name: str = "sampler",
                     saves_on: bool = True):
        return self.call("sample_start", {"period_game_ms": period_game_ms, "save_every": save_every,
                                          "name": name, "saves_on": saves_on})

    def sample_saves(self, on: bool):
        return self.call("sample_saves", {"on": on})

    def sample_stop(self):
        return self.call("sample_stop", timeout=90)

    def status(self):
        return self.call("status")

    def sync(self):
        return self.call("sync", timeout=300)

    def clock_pair(self, n: int = 9) -> dict:
        """Host perf_counter <-> guest clocks, NTP style: n pings, keep the one with the smallest
        round trip; its midpoint maps to the guest reading with an error of at most rtt/2."""
        best = None
        for _ in range(n):
            t0 = time.perf_counter()
            g = self.call("clocks")
            t1 = time.perf_counter()
            if best is None or t1 - t0 < best["rtt"]:
                best = dict(g, host=(t0 + t1) / 2, rtt=t1 - t0)
        return best


class WindowsBackend:
    """The owner's Windows copy, through the existing ctypes code (smoke.py; control.py from T15)."""

    # Highest address a 32-bit process can own on 64-bit Windows (4 GB with large-address-aware;
    # 2 GB otherwise, where VirtualQueryEx simply ends the walk earlier).
    USER_TOP = 0xFFFF0000
    # Same filter as the Wine agent's MAX_REGION: larger regions are not game heap.
    MAX_REGION = 256 << 20

    def __init__(self, game_dir: str = r"C:\anno1503-original", session: str | None = None):
        sys.path.insert(0, _HERE)
        import smoke  # noqa: PLC0415 - Windows only
        self.smoke = smoke
        try:
            import control  # noqa: PLC0415 - T15 (PR #16)
        except ImportError:
            control = None
        self.control = control
        self.game_dir = game_dir
        self.session = session or time.strftime("drive-%Y%m%d")
        self.out_dir = os.path.join(REPO, "cache", "orig", self.session)
        self.pid = None
        self.handle = None

    def _need_input(self):
        if self.control is None:
            raise NotImplementedError("input needs tools/orig/control.py from T15 (PR #16)")
        hwnd = self.control.foreground(self.game_dir)
        if not hwnd:
            raise BackendError("game window not found or not in the foreground")
        return hwnd

    def start(self) -> dict:
        pids = self.smoke.pids_under(self.game_dir)
        if not pids:
            raise BackendError(f"no running process from {self.game_dir}; start the game first")
        for pid in pids:
            if self.smoke.module_base(pid, "AnnoWelt.dll"):
                self.pid = pid
                break
        else:
            raise BackendError("no process with AnnoWelt.dll")
        self.handle = self.smoke.open_read(self.pid)
        return {"pid": self.pid}

    def stop(self) -> dict:
        if self.handle:
            self.smoke.kernel32.CloseHandle(self.handle)
        self.handle = None
        return {"detached": self.pid}

    def call(self, cmd: str, args: dict | None = None, **kw):
        raise NotImplementedError(f"{cmd}: only the Wine runner has agent commands")

    # memory (gamestate interface)
    def read(self, addr: int, n: int) -> bytes | None:
        return self.smoke.read_memory(self.handle, addr, n)

    def regions(self):
        smoke = self.smoke
        mbi = smoke.MEMORY_BASIC_INFORMATION()
        addr = 0
        while addr < self.USER_TOP:
            if not smoke.kernel32.VirtualQueryEx(self.handle, smoke.ctypes.c_void_p(addr),
                                                 smoke.ctypes.byref(mbi), smoke.ctypes.sizeof(mbi)):
                break
            base, size = int(mbi.BaseAddress or addr), int(mbi.RegionSize)
            # MEM_COMMIT and writable (PAGE_READWRITE 0x04 / PAGE_EXECUTE_READWRITE 0x40)
            if mbi.State == 0x1000 and mbi.Protect in (0x04, 0x40) and size <= self.MAX_REGION:
                yield base, base + size
            addr = base + size

    def module_bases(self) -> dict:
        return {n.lower(): b for n, b, _s in self.smoke.enum_modules(self.pid)}

    def state(self, what: str = "world") -> dict:
        import gamestate  # noqa: PLC0415
        welt = self.module_bases().get("annowelt.dll")
        res: dict = {"pid": self.pid}
        if what in ("world", "all"):
            w = gamestate.find_world(self, welt, settle=0.05)
            res["world"] = None if w is None else dict(gamestate.world_fields(self, w), addr=hex(w))
        if what in ("prng", "all"):
            res["prng"] = gamestate.prng_check(self, welt)
        if what in ("cities", "all"):
            res["cities"] = gamestate.cities(self, welt)
        if what in ("players", "all"):
            res["players"] = gamestate.players(self, welt)
        if what == "figures":       # not part of "all": hundreds of records per call
            res["figures"] = gamestate.figures(self, welt)
        if what == "camera":
            found = self.smoke.find_game_window(self.game_dir)
            frame = self.module_bases().get("annoframe.dll")
            res["camera"] = (gamestate.camera(self, found[0], frame) if found and frame else None)
        return res

    # screen
    def _capture(self):
        found = self.smoke.find_game_window(self.game_dir)
        if not found:
            raise BackendError("game window not found")
        w, h, bgra = self.smoke.capture(found[0])
        return w, h, bgra

    def screenshot(self, name: str, full: bool = False, box=None, scale: int = 1) -> str:
        w, h, bgra = self._capture()
        path = os.path.join(self.out_dir, "shots", f"{name}.png")
        os.makedirs(os.path.dirname(path), exist_ok=True)
        self.smoke.write_png(path, w, h, bgra)
        return path

    def recognize(self) -> dict:
        from PIL import Image  # noqa: PLC0415
        import screensig  # noqa: PLC0415
        w, h, bgra = self._capture()
        img = Image.frombuffer("RGBA", (w, h), bgra, "raw", "BGRA", 0, 1).convert("RGB")
        res = screensig.recognize(img)
        res["frame"] = [w, h]
        return res

    # input (T15 control.py, with its B1 guard)
    def key(self, name: str, hold: float = 0.0):
        # hold=0 is the interface's "tap"; SendInput needs a gap between down and up, so a tap is
        # sent as a 50 ms press here (on the Wine side `xdotool key` sends down and up itself).
        hwnd = self._need_input()
        self.control.press_key(hwnd, self.control.vk_of(name), hold=hold or 0.05)

    def click(self, nx: float, ny: float, button: int = 1, count: int = 1):
        hwnd = self._need_input()
        self.control.click_at(hwnd, nx, ny, button={1: "left", 3: "right"}.get(button, "left"),
                              double=count == 2)

    def move(self, nx: float, ny: float):
        self.control.move_to(self._need_input(), nx, ny)

    def drag(self, nx: float, ny: float, nx2: float, ny2: float):
        self.control.drag(self._need_input(), nx, ny, nx2, ny2)

    def wheel(self, nx: float, ny: float, amount: int):
        hwnd = self._need_input()
        self.control.move_to(hwnd, nx, ny)
        self.control.wheel(hwnd, amount)

    def quicksave(self, keep_as: str | None = None) -> dict:
        self.key("F10")
        return {"key": "F10"}

    def quickload(self) -> dict:
        self.key("F9")
        return {"key": "F9"}


def open_backend(kind: str, **kw):
    if kind == "wine":
        return WineBackend(**kw)
    if kind == "windows":
        return WindowsBackend(**kw)
    raise ValueError(f"unknown backend {kind!r} (wine | windows)")
