#!/usr/bin/env python3
"""T16 Wine runner: one instance of the original under Wine on its own invisible X display.

Runs inside the AnnoHarness WSL distro as root (docs/testing/original-wine.md). One instance =
  - a disposable copy: overlayfs views of the base game (/root/anno/game) and the base prefix
    (/root/anno/prefix), so a run never changes the base copies and starts in milliseconds
    (a full copy with --copy copy, opt-in only);
  - its own Xvfb display (:91, :92, ...) and therefore its own wineserver (one per prefix);
  - outputs in <run>/out (screenshots, timing series, sampler JSONL, quicksave copies), copied to a
    Windows folder (cache/orig/<session>/...) on `sync` and on `stop`.

Input is XTEST through xdotool on that display only; the owner's desktop is never touched. Memory is
read-only (/proc/<pid>/mem; root may read any process despite ptrace_scope=1). Runs are silent
(no audio device, an audio guard, the game's own volumes at minimum; original-wine.md section 2.2).
Wine's crash dialog is switched off in each run's prefix: a crash writes its report to wine.log, the
process ends, and the watchdog reports `crashed` at once.

Two ways to use it:
  serve   JSON lines on stdin/stdout, one command per line: {"id": n, "cmd": "...", "args": {...}}
          -> {"id": n, "ok": true, "result": ...}. tools/orig/backend.py (WineBackend) speaks this.
          The instance is stopped and cleaned up when the loop ends for any reason: `stop`, EOF on
          stdin, a broken pipe, SIGTERM/SIGHUP. Xvfb dies with the agent (PR_SET_PDEATHSIG); Wine
          processes of a run whose agent died are reaped by `cleanup` (agent.pid in the run folder).
          Last resort from Windows: `wsl --terminate AnnoHarness`.
  python  `from agent import Instance` inside the distro (the same methods, no pipe).

Rules (TASKS.md T16): the game copy stays inside the distro; read-only memory; `pkill -x` only.
"""

from __future__ import annotations

import argparse
import json
import os
import re
import shutil
import signal
import subprocess
import sys
import threading
import time
import traceback

_HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, os.path.dirname(_HERE))          # tools/orig: gamestate, screensig, userdat
import gamestate  # noqa: E402
import screensig  # noqa: E402
import userdat  # noqa: E402

BASE = "/root/anno"
BASE_GAME = f"{BASE}/game"
BASE_PREFIX = f"{BASE}/prefix"
RUNS = f"{BASE}/runs"
SIGREF = f"{BASE}/sigref"      # reference frames of learned screens (inside the distro, never in git)
DESKTOP = (1024, 768)          # the Wine virtual desktop; the game never switches display modes
MENU_FRAME = (800, 600)        # menus draw 800x600 at the desktop's top left (spike, section 2)
GAME_DLLS = ("annowelt.dll", "annogame.dll", "annoframe.dll", "annoscene.dll", "annodisplay.dll")
# Vtable scans skip mappings larger than this: they are file images or Wine's reserved areas, not
# game heap. A filter on scan hits, not a game limit; raise it here if a heap ever grows past it.
MAX_REGION = 256 << 20
AUDIO_OFF = "winepulse.drv,winealsa.drv,wineoss.drv=d"   # WINEDLLOVERRIDES: no audio driver
# The audio guard kills the instance when any of these is mapped in a silent run.
AUDIO_LIBS = ("winepulse", "winealsa", "wineoss", "libpulse", "libasound", "libpipewire", "libjack",
              "libopenal")
RUN_ID = re.compile(r"^[A-Za-z0-9][A-Za-z0-9._-]{0,99}$")
# File names the controller picks (screenshot, learned screen, timing label, sampler, kept save):
# one path component, no separators, no "..", the same alphabet as run ids.
NAME = RUN_ID
# The only place --out-win may point: the repository's cache/orig folder as the distro sees it
# (the agent runs from the Windows checkout, /mnt/c/.../tools/orig/wine/agent.py; backend.py puts
# each run in cache/orig/<session>/d<display>-<time>/). The agent writes there as root.
OUT_WIN_BASE = os.path.join(os.path.dirname(os.path.dirname(os.path.dirname(_HERE))), "cache", "orig")
XDO_TIMEOUT = 30               # seconds; a wedged X server must not hang the command loop
WATCHDOG_JOIN = 5              # seconds stop() waits for the watchdog (it checks for stop every 0.5 s)
SAVE_DIR = "SaveGame"          # the game's save folder in its copy (user.dat profile lives there too)

# play-layer key names (tools/orig/screens.json "keys") -> X keysyms
XKEYS = {"ESC": "Escape", "ENTER": "Return", "RETURN": "Return", "SPACE": "space", "TAB": "Tab",
         "DOT": "period", "COMMA": "comma", "LEFT": "Left", "RIGHT": "Right", "UP": "Up",
         "DOWN": "Down", "CTRL": "Control_L", "SHIFT": "Shift_L", "ALT": "Alt_L",
         "PGUP": "Prior", "PGDN": "Next", "HOME": "Home", "END": "End", "DEL": "Delete"}


def xkey(name: str) -> str:
    parts = [p.strip() for p in name.split("+")]
    out = []
    for p in parts:
        up = p.upper()
        if up in XKEYS:
            out.append(XKEYS[up])
        elif up.startswith("F") and up[1:].isdigit():
            out.append(up)
        elif len(p) == 1:
            out.append(p.lower())
        else:
            out.append(p)
    return "+".join(out)


_POSIX_CLOCKS = hasattr(time, "clock_gettime")   # the agent runs in the distro; its tests also on Windows


def mono() -> float:
    return time.clock_gettime(time.CLOCK_MONOTONIC) if _POSIX_CLOCKS else time.monotonic()


def raw() -> float:
    """CLOCK_MONOTONIC_RAW: not slewed by WSL's time sync; the clock the game's timer follows
    (measured, original-wine.md section 2.3)."""
    return time.clock_gettime(time.CLOCK_MONOTONIC_RAW) if _POSIX_CLOCKS else time.monotonic()


def clocks() -> dict:
    return {"mono": mono(), "raw": raw(),
            "real": time.clock_gettime(time.CLOCK_REALTIME) if _POSIX_CLOCKS else time.time()}


def cmdline(pid: int) -> str:
    try:
        return open(f"/proc/{pid}/cmdline", "rb").read().replace(b"\0", b" ").decode("latin-1").strip()
    except OSError:
        return ""


def pid_alive(pid: int | None) -> bool:
    return bool(pid) and os.path.exists(f"/proc/{pid}")


def prefix_pids(prefix: str) -> list[int]:
    """Every process whose environment has WINEPREFIX=<prefix>."""
    want = f"WINEPREFIX={prefix}".encode()
    pids = []
    for p in os.listdir("/proc"):
        if not p.isdigit():
            continue
        try:
            env = open(f"/proc/{p}/environ", "rb").read().split(b"\0")
        except OSError:
            continue
        if want in env:
            pids.append(int(p))
    return pids


def kill_pids(pids) -> list[int]:
    killed = []
    for p in pids:
        try:
            os.kill(p, signal.SIGKILL)
            killed.append(p)
        except OSError:
            pass
    return killed


def read_pid(path: str) -> int | None:
    try:
        return int(open(path).read().strip())
    except (OSError, ValueError):
        return None


def check_name(name: str, what: str = "name") -> str:
    if not isinstance(name, str) or not NAME.match(name):
        raise ValueError(f"{what} {name!r} must match {NAME.pattern}")
    return name


def within(path: str, base: str, what: str = "path") -> str:
    """The real path of `path` when it lies strictly inside `base` (both through realpath, so a
    symlink that leads out is caught); ValueError otherwise. A '..' component is refused outright,
    wherever it would lead."""
    if not isinstance(path, str) or not path:
        raise ValueError(f"{what} must be a non-empty path")
    if ".." in re.split(r"[\\/]+", path):
        raise ValueError(f"{what} {path!r} contains '..'")
    real, rbase = os.path.realpath(path), os.path.realpath(base)
    try:
        common = os.path.commonpath([real, rbase])
    except ValueError:                  # different drives (Windows)
        common = None
    if common is None or os.path.normcase(common) != os.path.normcase(rbase) \
            or os.path.normcase(real) == os.path.normcase(rbase):
        raise ValueError(f"{what} {path!r} is not inside {base}")
    return real


def _pdeathsig() -> None:
    """In the child: get SIGTERM when the parent (the agent) dies (prctl PR_SET_PDEATHSIG = 1)."""
    import ctypes
    ctypes.CDLL("libc.so.6", use_errno=True).prctl(1, signal.SIGTERM)


class ProcMem:
    """gamestate's memory interface over /proc/<pid>/mem and /proc/<pid>/maps (read-only)."""

    def __init__(self, pid: int):
        self.pid = pid
        self.f = open(f"/proc/{pid}/mem", "rb", buffering=0)

    def read(self, addr: int, n: int) -> bytes | None:
        try:
            return os.pread(self.f.fileno(), n, addr)
        except (OSError, ValueError):       # ValueError: the file was closed by another thread
            return None

    def maps(self) -> list[tuple[int, int, str, str]]:
        out = []
        with open(f"/proc/{self.pid}/maps") as f:
            for line in f:
                parts = line.split(None, 5)
                lo, hi = (int(x, 16) for x in parts[0].split("-"))
                out.append((lo, hi, parts[1], parts[5].strip() if len(parts) > 5 else ""))
        return out

    def regions(self):
        for lo, hi, perm, _path in self.maps():
            if perm.startswith("rw") and hi - lo <= MAX_REGION:
                yield lo, hi

    def close(self) -> None:
        self.f.close()


class Instance:
    def __init__(self, display: str = ":91", run_id: str | None = None, out_win: str | None = None,
                 copy: str = "overlay", keep: bool = False, sig_path: str | None = None,
                 audio: bool = False, winedebug: str = "-all", crash_dialog: bool = False):
        if not re.fullmatch(r":\d{1,4}", display):
            raise ValueError(f"display must look like :91, got {display!r}")
        self.display = display
        self.num = display.lstrip(":")
        self.run_id = run_id or time.strftime(f"run-%Y%m%d-%H%M%S-d{self.num}")
        if not RUN_ID.match(self.run_id):
            raise ValueError(f"run id {self.run_id!r} must match {RUN_ID.pattern}")
        self.run = f"{RUNS}/{self.run_id}"
        if os.path.dirname(os.path.realpath(self.run)) != os.path.realpath(RUNS):
            raise ValueError(f"run folder {self.run} is not directly inside {RUNS}")
        self.game = f"{self.run}/game"
        self.prefix = f"{self.run}/prefix"
        self.out = f"{self.run}/out"
        self.out_win = within(out_win, OUT_WIN_BASE, "--out-win") if out_win else None
        self.copy = copy
        self.keep = keep
        self.sig_path = sig_path
        self.audio = audio
        self.winedebug = winedebug
        self.crash_dialog = crash_dialog
        self.created = False           # set by prepare(); stop() tears down only what this created
        self.stopped = False
        self.stop_result: dict | None = None
        self.mounts: list[str] = []
        self.xvfb: subprocess.Popen | None = None
        self.wine: subprocess.Popen | None = None
        self.pid: int | None = None
        self.mem: ProcMem | None = None
        self.bases: dict[str, int] = {}
        self.world: int | None = None
        self.input_lock = threading.Lock()
        self.game_lock = threading.RLock()
        self.poller: dict | None = None
        self.sampler: dict | None = None
        self.log_f = None
        # The command loop, the watchdog, the sampler and the poller all log: one lock guards the
        # lazy open, every write and the close; after the close log() is a no-op, so a late line
        # never recreates <run>/out after stop() removed the run folder.
        self.log_lock = threading.Lock()
        self.log_closed = False
        self.guard_stop = threading.Event()
        self.watchdog: threading.Thread | None = None
        self.audio_killed: list | None = None
        self.crashed: dict | None = None
        self.scan_hits: list[int] | None = None

    # ---- environment -------------------------------------------------------------------------
    def env(self) -> dict:
        e = dict(os.environ)
        e.pop("WAYLAND_DISPLAY", None)
        e.pop("WSL_INTEROP", None)     # nothing in a run may start a Windows program on the desktop
        e.update(DISPLAY=self.display, WINEPREFIX=self.prefix, WINEARCH="win32", WINEDEBUG=self.winedebug)
        if not self.audio:
            # Silent runs (owner, 2026-10-22): WSLg hands every distro a PulseAudio socket
            # (PULSE_SERVER=unix:/mnt/wslg/PulseServer) that plays on the owner's speakers.
            # Without PULSE_SERVER, libpulse would still find WSLg's socket through
            # XDG_RUNTIME_DIR (/mnt/wslg/runtime-dir), so each run gets a private, empty one.
            # The audio guard (watchdog) kills the instance if any audio library loads anyway.
            for k in [k for k in e if k.startswith("PULSE_")]:
                e.pop(k)
            e["XDG_RUNTIME_DIR"] = f"{self.run}/xdg"
            e["WINEDLLOVERRIDES"] = ";".join(filter(None, [e.get("WINEDLLOVERRIDES"), AUDIO_OFF]))
        return e

    def log(self, kind: str, /, **kw) -> None:
        """One line in <run>/out/actions.jsonl. Thread-safe. Never creates a folder: before
        prepare() made out/ and after log_close() the line is dropped."""
        rec = {"t": round(mono(), 4), "kind": kind}
        for k, v in kw.items():
            rec[f"arg_{k}" if k in rec else k] = v
        line = json.dumps(rec, default=str) + "\n"
        with self.log_lock:
            if self.log_closed:
                return
            if self.log_f is None:
                if not os.path.isdir(self.out):
                    return
                self.log_f = open(f"{self.out}/actions.jsonl", "a", buffering=1)
            self.log_f.write(line)

    def log_close(self) -> None:
        """Close the action log for good; later log() calls are no-ops."""
        with self.log_lock:
            self.log_closed = True
            if self.log_f is not None:
                f, self.log_f = self.log_f, None
                f.close()

    # ---- disposable copy ---------------------------------------------------------------------
    def _overlay(self, lower: str, merged: str) -> None:
        upper, work = merged + ".upper", merged + ".work"
        for d in (upper, work, merged):
            os.makedirs(d, exist_ok=True)
        r = subprocess.run(["mount", "-t", "overlay", "overlay", "-o",
                            f"lowerdir={lower},upperdir={upper},workdir={work}", merged],
                           capture_output=True, text=True)
        if r.returncode != 0:
            raise RuntimeError(f"overlay mount of {merged} failed: {r.stderr.strip()}")
        self.mounts.append(merged)

    def prepare(self) -> dict:
        if self.created:
            raise RuntimeError("already prepared")
        if os.path.exists(self.run):
            raise RuntimeError(f"run folder {self.run} exists; pick another run id")
        os.makedirs(self.out)
        self.created = True
        with open(f"{self.run}/agent.pid", "w") as f:
            f.write(str(os.getpid()))
        if self.keep:
            open(f"{self.run}/.keep", "w").close()
        os.makedirs(f"{self.run}/xdg", mode=0o700, exist_ok=True)
        t0 = mono()
        if self.copy == "overlay":
            self._overlay(BASE_GAME, self.game)
            self._overlay(BASE_PREFIX, self.prefix)
        elif self.copy == "copy":
            subprocess.run(["cp", "-a", BASE_GAME, self.game], check=True)
            subprocess.run(["cp", "-a", BASE_PREFIX, self.prefix], check=True)
        else:
            raise ValueError(f"copy mode {self.copy!r} (overlay | copy)")
        res = {"run": self.run, "copy": self.copy, "seconds": round(mono() - t0, 3)}
        if not self.audio:
            res["profiles"] = self.silence_profiles()
        if not self.crash_dialog:
            res["crash_dialog_off"] = self._reg("HKCU\\Software\\Wine\\WineDbg", "ShowCrashDialog", "0",
                                                "REG_DWORD")["rc"] == 0
        self.log("prepare", **res)
        return res

    def silence_profiles(self) -> list[dict]:
        """In-game music and speech volume to the game's minimum in every profile of this copy
        (tools/orig/userdat.py). Writes land in the overlay's upper layer, never in the base. A
        profile that cannot be patched is reported and left as it is."""
        out = []
        root = f"{self.game}/{SAVE_DIR}"
        for name in sorted(os.listdir(root)) if os.path.isdir(root) else []:
            p = f"{root}/{name}/user.dat"
            if not os.path.isfile(p):
                continue
            data = open(p, "rb").read()
            rec = {"profile": name}
            try:
                rec["volume_before"] = userdat.read(data).get("volume")
                new = userdat.set_volume(data)
            except ValueError as ex:
                rec["error"] = str(ex)
                out.append(rec)
                continue
            with open(p, "wb") as f:
                f.write(new)
            rec["volume_after"] = userdat.read(open(p, "rb").read()).get("volume")
            out.append(rec)
        return out

    def _reg(self, key: str, name: str, value: str, kind: str) -> dict:
        r = subprocess.run(["wine", "reg", "add", key, "/v", name, "/t", kind, "/d", str(value), "/f"],
                           env=self.env(), capture_output=True, text=True, timeout=60)
        subprocess.run(["wineserver", "-w"], env=self.env(), capture_output=True, timeout=60)
        return {"key": key, "name": name, "kind": kind, "value": value, "rc": r.returncode,
                "out": (r.stdout + r.stderr).strip()[-300:]}

    def reg_add(self, key: str, name: str, value: str, kind: str = "REG_SZ") -> dict:
        """Set a registry value in THIS run's prefix (disposable; the base prefix is untouched),
        between prepare and launch. Example: the community's hidden-multiplayer switch
        HKCU\\Software\\Max-Design\\Anno1503 MULTIPLAYER=1 (community-landscape.md)."""
        if not self.created:
            raise RuntimeError("reg_add needs prepare first")
        if self.wine is not None:
            raise RuntimeError("reg_add must run between prepare and launch")
        if kind not in ("REG_SZ", "REG_DWORD"):
            raise ValueError(f"unsupported registry type {kind}")
        res = self._reg(key, name, value, kind)
        self.log("reg_add", **res)
        if res["rc"] != 0:
            raise RuntimeError(f"wine reg add failed: {res['out']}")
        return res

    def _unmount(self) -> list[str]:
        failed = []
        for m in reversed(self.mounts):
            if subprocess.run(["umount", m], capture_output=True).returncode != 0:
                if subprocess.run(["umount", "-l", m], capture_output=True).returncode != 0:
                    failed.append(m)
        self.mounts.clear()
        return failed

    # ---- launch and stop ---------------------------------------------------------------------
    def _x_listening(self) -> bool:
        """Under WSL, /tmp/.X11-unix is WSLg's read-only tmpfs: Xvfb cannot create its socket file
        there and serves only the abstract socket @/tmp/.X11-unix/X<n>, which Xlib/xcb (xdotool,
        Wine) try first. So readiness is read from /proc/net/unix, not from the file system."""
        want = f"@/tmp/.X11-unix/X{self.num}"
        with open("/proc/net/unix") as f:
            return any(line.rstrip().endswith(want) for line in f)

    def _display_free(self) -> bool:
        return not os.path.exists(f"/tmp/.X{self.num}-lock") and not self._x_listening()

    def launch(self) -> dict:
        if not self.created:
            raise RuntimeError("launch needs prepare first")
        if self.wine is not None:
            raise RuntimeError("already launched")
        if not self._display_free():
            raise RuntimeError(f"display {self.display} is in use (/tmp/.X{self.num}-lock)")
        self.xvfb = subprocess.Popen(
            ["Xvfb", self.display, "-screen", "0", f"{DESKTOP[0]}x{DESKTOP[1]}x24", "-nolisten", "tcp"],
            stdout=subprocess.DEVNULL, stderr=open(f"{self.out}/xvfb.log", "w"), preexec_fn=_pdeathsig)
        with open(f"{self.run}/xvfb.pid", "w") as f:
            f.write(str(self.xvfb.pid))
        for _ in range(200):
            if self.xvfb.poll() is not None:
                raise RuntimeError(f"Xvfb exited with {self.xvfb.returncode} (see xvfb.log)")
            if self._x_listening():
                break
            time.sleep(0.05)
        else:
            raise RuntimeError("Xvfb did not come up within 10 s (see xvfb.log)")
        time.sleep(0.1)
        if self.xvfb.poll() is not None:     # another server owned the display: it was not ours
            raise RuntimeError(f"Xvfb exited with {self.xvfb.returncode}; display {self.display} taken")
        self.wine = subprocess.Popen(
            ["wine", "explorer", f"/desktop=anno{self.num},{DESKTOP[0]}x{DESKTOP[1]}", "1503Startup.exe"],
            cwd=self.game, env=self.env(), stdout=open(f"{self.out}/wine.log", "w"),
            stderr=subprocess.STDOUT, start_new_session=True)
        self.log("launch", display=self.display, xvfb=self.xvfb.pid, wine=self.wine.pid, audio=self.audio)
        self.watchdog = threading.Thread(target=self._watchdog, daemon=True)
        self.watchdog.start()
        return {"display": self.display, "xvfb_pid": self.xvfb.pid, "audio": self.audio}

    def find_game(self, wait: float = 0.0) -> int | None:
        """The process of this instance that maps AnnoWelt.dll (the world lives there)."""
        end = mono() + wait
        while True:
            with self.game_lock:
                for pid in prefix_pids(self.prefix):
                    try:
                        maps = open(f"/proc/{pid}/maps").read().lower()
                    except OSError:
                        continue
                    if "/annowelt.dll" in maps:
                        if pid != self.pid:
                            if self.mem:
                                self.mem.close()
                            self.pid, self.mem, self.world = pid, ProcMem(pid), None
                            self.bases = self._bases()
                            self.log("game_pid", pid=pid, bases={k: hex(v) for k, v in self.bases.items()})
                        return pid
            if mono() >= end:
                return None
            time.sleep(0.25)

    def _bases(self) -> dict[str, int]:
        found: dict[str, int] = {}
        for lo, _hi, _perm, path in self.mem.maps():
            low = path.lower()
            for d in GAME_DLLS:
                if low.endswith("/" + d) and d not in found:
                    found[d] = lo
        return found

    def alive(self) -> bool:
        return pid_alive(self.pid)

    def _kill_all(self) -> dict:
        """SIGKILL first (it cannot hang), then let wineserver clean up what is left."""
        res = {"killed": kill_pids(prefix_pids(self.prefix))}
        try:
            subprocess.run(["wineserver", "-k"], env=self.env(), capture_output=True, timeout=10)
        except (OSError, subprocess.TimeoutExpired) as ex:
            res["wineserver"] = repr(ex)
        return res

    def stop(self) -> dict:
        """Tear down what prepare/launch created, step by step; every step runs even if an earlier
        one failed, and the errors are reported. The run folder is removed only after its outputs
        were synced (or when no Windows folder was given) and unless the run is kept."""
        if self.stopped:
            return self.stop_result
        if not self.created:
            self.stopped = True
            self.stop_result = {"run": self.run, "skipped": "nothing was prepared by this agent"}
            return self.stop_result
        res: dict = {"run": self.run, "errors": []}

        def step(name, fn):
            try:
                return fn()
            except Exception as ex:  # noqa: BLE001 - collect, keep tearing down
                res["errors"].append(f"{name}: {type(ex).__name__}: {ex}")
                return None

        self.guard_stop.set()
        res["watchdog_stopped"] = step("watchdog_join", self._join_watchdog)
        res["sampler"] = step("sample_stop", self.sample_stop)
        res["poller"] = step("poll_stop", lambda: self.poll_stop() and "stopped")
        res["kill"] = step("kill", self._kill_all)
        if self.wine:
            step("wine_wait", lambda: self.wine.wait(timeout=10))
        if self.xvfb and self.xvfb.poll() is None:
            self.xvfb.terminate()
            if step("xvfb_wait", lambda: self.xvfb.wait(timeout=10) is not None) is None:
                step("xvfb_kill", self.xvfb.kill)
        res["leftover_pids_killed"] = step("leftovers", lambda: kill_pids(prefix_pids(self.prefix)))
        if self.mem:
            step("mem_close", self.mem.close)
            self.mem = None
        res["crashed"] = self.crashed
        res["audio_killed"] = self.audio_killed
        res["upper_files"] = step("upper_files", self.upper_files) or []
        self.log("stop", **{k: v for k, v in res.items() if k != "upper_files"})
        step("log_close", self.log_close)
        synced = step("sync", self.sync)
        res["synced_to"] = synced
        res["unmount_failed"] = step("unmount", self._unmount)
        sync_ok = self.out_win is None or synced is not None
        if self.keep or os.path.exists(f"{self.run}/.keep"):
            res["kept"] = "--keep"
        elif not sync_ok:
            res["kept"] = "outputs not synced; the run folder stays for `cleanup`"
        elif res["unmount_failed"]:
            res["kept"] = "an overlay could not be unmounted"
        else:
            step("remove", lambda: shutil.rmtree(self.run))
            res["removed"] = not os.path.exists(self.run)
        self.stopped = True
        self.stop_result = res
        return res

    def _join_watchdog(self) -> bool | None:
        """Wait for the watchdog after guard_stop is set (it wakes every 0.5 s). True when it
        ended, None when it was never started; a watchdog still running after the timeout is an
        error, and its late lines are dropped by the closed log."""
        if self.watchdog is None:
            return None
        self.watchdog.join(timeout=WATCHDOG_JOIN)
        if self.watchdog.is_alive():
            raise RuntimeError(f"watchdog still running after {WATCHDOG_JOIN} s")
        return True

    def upper_files(self, layer: str = "game") -> list[dict]:
        """Every file the game wrote into its copy or its prefix (the overlay upper layer)."""
        upper = (self.game if layer == "game" else self.prefix) + ".upper"
        out = []
        for root, _dirs, files in os.walk(upper):
            for fn in files:
                p = os.path.join(root, fn)
                out.append({"path": os.path.relpath(p, upper), "size": os.path.getsize(p),
                            "mtime": round(os.path.getmtime(p), 3)})
        return sorted(out, key=lambda r: r["mtime"])

    def sync(self) -> str | None:
        if not self.out_win:
            return None
        with self.log_lock:
            if self.log_f is not None:
                self.log_f.flush()
        os.makedirs(self.out_win, exist_ok=True)
        shutil.copytree(self.out, self.out_win, dirs_exist_ok=True)
        return self.out_win

    # ---- watchdog: silence and crashes --------------------------------------------------------
    def audio_check(self, pids: list[int] | None = None) -> dict:
        """Every process of this instance: PULSE_* variables in its environment and audio
        libraries in its address space. Both must be empty in a silent run."""
        pids = prefix_pids(self.prefix) if pids is None else pids
        bad = []
        for pid in pids:
            try:
                env = open(f"/proc/{pid}/environ", "rb").read().split(b"\0")
                maps = open(f"/proc/{pid}/maps").read().lower()
            except OSError:
                continue
            pulse = [e.decode(errors="replace") for e in env if e.startswith(b"PULSE_")]
            libs = sorted({lib for lib in AUDIO_LIBS if lib in maps})
            if pulse or libs:
                bad.append({"pid": pid, "env": pulse, "libs": libs})
        return {"audio": self.audio, "processes": len(pids), "violations": bad, "killed": self.audio_killed}

    def _watchdog(self) -> None:
        while not self.guard_stop.wait(0.5):
            try:
                pids = prefix_pids(self.prefix)
                if not self.audio and self.audio_killed is None:
                    r = self.audio_check(pids)
                    if r["violations"]:
                        self.audio_killed = r["violations"]
                        kill_pids(prefix_pids(self.prefix))
                        self.log("audio_guard_kill", violations=r["violations"])
                        continue
                if self.crashed is None:
                    dbg = [{"pid": p, "cmd": cmdline(p)} for p in pids if "winedbg" in cmdline(p).lower()]
                    if dbg:
                        self.crashed = {"t_mono": round(mono(), 3), "winedbg": dbg}
                        self.log("crash", winedbg=dbg)
            except Exception as ex:  # noqa: BLE001 - the watchdog must keep watching
                self.log("watchdog_error", error=repr(ex))

    def status(self) -> dict:
        return {"run": self.run, "display": self.display, "pid": self.pid, "alive": self.alive(),
                "copy": self.copy, "crashed": self.crashed, "audio_killed": self.audio_killed,
                "sampler": None if not self.sampler else
                {"records": self.sampler["n"], "error": self.sampler["error"],
                 "saves_on": self.sampler["saves_on"]}}

    # ---- input (XTEST via xdotool, this display only) ----------------------------------------
    def _xdo(self, *args: str) -> None:
        r = subprocess.run(["xdotool", *args], env=self.env(), capture_output=True, text=True,
                           timeout=XDO_TIMEOUT)
        if r.returncode != 0:
            raise RuntimeError(f"xdotool {' '.join(args)}: {r.stderr.strip()}")

    def frame(self) -> dict:
        return self.frame_from(self.grab())

    def _px(self, nx: float, ny: float, fr: dict | None = None) -> tuple[int, int]:
        if not (0.0 <= nx <= 1.0 and 0.0 <= ny <= 1.0):
            raise ValueError(f"target ({nx}, {ny}) is not a frame fraction")
        fr = fr or self.frame()
        return fr["x"] + min(round(nx * fr["w"]), fr["w"] - 1), fr["y"] + min(round(ny * fr["h"]), fr["h"] - 1)

    def key(self, name: str, hold: float = 0.0) -> dict:
        k = xkey(name)
        with self.input_lock:
            if hold > 0:
                self._xdo("keydown", k)
                try:
                    time.sleep(hold)
                finally:
                    self._xdo("keyup", k)
            else:
                self._xdo("key", k)
        self.log("key", key=name, xkey=k)
        return {"key": k}

    # Each gesture is ONE xdotool invocation (one X connection, so the events stay in order).
    # No `mousemove --sync`: it waits for a motion event, and when the pointer is already at the
    # target none comes, so it hung ~15 s per click (measured 2026-10-22).
    def move_px(self, x: int, y: int) -> dict:
        with self.input_lock:
            self._xdo("mousemove", str(x), str(y))
        return {"x": x, "y": y}

    def click_px(self, x: int, y: int, button: int = 1, count: int = 1) -> dict:
        with self.input_lock:
            self._xdo("mousemove", str(x), str(y), "sleep", "0.03",
                      "click", "--repeat", str(count), "--delay", "80", str(button))
        self.log("click", x=x, y=y, button=button, count=count)
        return {"x": x, "y": y}

    def click(self, nx: float, ny: float, button: int = 1, count: int = 1) -> dict:
        x, y = self._px(nx, ny)
        return self.click_px(x, y, button, count)

    def move(self, nx: float, ny: float) -> dict:
        x, y = self._px(nx, ny)
        self.log("move", x=x, y=y)
        return self.move_px(x, y)

    def drag(self, nx: float, ny: float, nx2: float, ny2: float, steps: int = 12, button: int = 1) -> dict:
        fr = self.frame()
        (x, y), (x2, y2) = self._px(nx, ny, fr), self._px(nx2, ny2, fr)
        seq = ["mousemove", str(x), str(y), "sleep", "0.03", "mousedown", str(button)]
        for i in range(1, steps + 1):
            seq += ["sleep", "0.02", "mousemove", str(x + (x2 - x) * i // steps), str(y + (y2 - y) * i // steps)]
        seq += ["sleep", "0.03", "mouseup", str(button)]
        with self.input_lock:
            self._xdo(*seq)
        self.log("drag", frm=[x, y], to=[x2, y2])
        return {"from": [x, y], "to": [x2, y2]}

    def wheel(self, nx: float, ny: float, amount: int) -> dict:
        x, y = self._px(nx, ny)
        button = "4" if amount > 0 else "5"
        with self.input_lock:
            self._xdo("mousemove", str(x), str(y), "sleep", "0.03",
                      "click", "--repeat", str(abs(amount)), "--delay", "60", button)
        self.log("wheel", x=x, y=y, amount=amount)
        return {"x": x, "y": y, "amount": amount}

    # ---- screen ------------------------------------------------------------------------------
    def grab(self):
        from PIL import ImageGrab
        return ImageGrab.grab(xdisplay=self.display)

    def frame_from(self, img) -> dict:
        """The game frame inside the desktop: menus are 800x600 at the top left, a running game
        fills the desktop. Decided from the screen: outside the menu frame the desktop is one
        flat colour while menus show."""
        right = img.crop((MENU_FRAME[0] + 8, 8, DESKTOP[0] - 8, DESKTOP[1] - 8)).convert("L")
        lo, hi = right.getextrema()
        w, h = MENU_FRAME if hi - lo <= 2 else DESKTOP
        return {"x": 0, "y": 0, "w": w, "h": h}

    def frame_image(self):
        img = self.grab()
        fr = self.frame_from(img)
        return img.crop((fr["x"], fr["y"], fr["x"] + fr["w"], fr["y"] + fr["h"])), fr

    def screenshot(self, name: str, dest: str | None = None, full: bool = False, box=None,
                   scale: int = 1) -> dict:
        """PNG of the game frame (or the whole desktop with full=True). box = [x0, y0, x1, y1] in
        frame fractions crops it; scale enlarges a crop (nearest neighbour) for reading digits.
        dest: an extra copy, inside this run's out dir (relative paths are taken from there) or
        inside --out-win (backend.py copies each shot to the Windows folder this way)."""
        check_name(name, "screenshot name")
        dest = self.dest_path(dest) if dest else None
        img = self.grab()
        if not full:
            fr = self.frame_from(img)
            img = img.crop((fr["x"], fr["y"], fr["x"] + fr["w"], fr["y"] + fr["h"]))
        if box:
            w, h = img.size
            img = img.crop((round(box[0] * w), round(box[1] * h), round(box[2] * w), round(box[3] * h)))
        if scale > 1:
            from PIL import Image
            img = img.resize((img.size[0] * scale, img.size[1] * scale), Image.NEAREST)
        os.makedirs(f"{self.out}/shots", exist_ok=True)
        path = f"{self.out}/shots/{name}.png"
        img.save(path)
        res = {"path": path, "size": list(img.size)}
        if dest:
            os.makedirs(os.path.dirname(dest), exist_ok=True)
            shutil.copyfile(path, dest)
            res["dest"] = dest
        return res

    def dest_path(self, dest: str) -> str:
        """`dest` resolved and checked: strictly inside the run's out dir or the --out-win folder."""
        if not os.path.isabs(dest):
            dest = os.path.join(self.out, dest)
        errors = []
        for base in filter(None, (self.out, self.out_win)):
            try:
                return within(dest, base, "dest")
            except ValueError as ex:
                errors.append(str(ex))
        raise ValueError("; ".join(errors))

    def recognize(self) -> dict:
        img, fr = self.frame_image()
        res = screensig.recognize(img, screensig.load(self.sig_path))
        res["frame"] = [fr["w"], fr["h"]]
        return res

    def learn(self, name: str, regions=None, max_dist: int = screensig.DEFAULT_MAX_DIST) -> dict:
        check_name(name, "screen name")
        img, fr = self.frame_image()
        os.makedirs(SIGREF, exist_ok=True)
        img.save(f"{SIGREF}/{name}.png")
        table = screensig.load(self.sig_path)
        table.setdefault("screens", {})[name] = {"frame": [fr["w"], fr["h"]],
                                                 "regions": screensig.learn(img, regions, max_dist)}
        screensig.save(table, self.sig_path)
        return table["screens"][name]

    def calibrate(self) -> dict:
        """Thresholds from the reference frames of all learned screens (screensig.calibrate)."""
        from PIL import Image
        table = screensig.load(self.sig_path)
        refs = {n: Image.open(f"{SIGREF}/{n}.png").convert("RGB")
                for n in table.get("screens", {}) if os.path.exists(f"{SIGREF}/{n}.png")}
        report = screensig.calibrate(table, refs)
        screensig.save(table, self.sig_path)
        return {"refs": sorted(refs), "regions": report}

    def forget(self, name: str) -> dict:
        table = screensig.load(self.sig_path)
        gone = table.get("screens", {}).pop(name, None) is not None
        screensig.save(table, self.sig_path)
        return {"forgot": name, "existed": gone}

    # ---- memory (read-only) ------------------------------------------------------------------
    def need_game(self) -> None:
        with self.game_lock:
            if not self.alive():
                self.pid = None
                if not self.find_game(wait=0):
                    raise RuntimeError("game process not found")

    def read(self, addr: int, n: int) -> str | None:
        self.need_game()
        b = self.mem.read(addr, n)
        return b.hex() if b is not None else None

    def module_bases(self) -> dict:
        self.need_game()
        return {k: hex(v) for k, v in self.bases.items()}

    def scan(self, value: int, refine: bool = False, limit: int = 1 << 16) -> dict:
        """Read-only value search (RE by memory diffing): every 4-aligned u32 == value in the
        writable regions, or, with refine, only among the previous scan's hits."""
        self.need_game()
        if refine and self.scan_hits is not None:
            hits = []
            for a in self.scan_hits:
                b = self.mem.read(a, 4)
                if b and gamestate.u32(b) == value:
                    hits.append(a)
        else:
            hits = gamestate.scan_u32(self.mem, value, limit)
        self.scan_hits = hits
        return {"value": value, "count": len(hits), "truncated": not refine and len(hits) >= limit,
                "addrs": [hex(a) for a in hits[:64]]}

    def whereis(self, addr: int) -> dict:
        """Which mapping holds addr; for a module image, the module and the RVA."""
        self.need_game()
        maps = self.mem.maps()
        for lo, hi, perm, path in maps:
            if lo <= addr < hi:
                base = min(m[0] for m in maps if m[3] == path) if path else lo
                rec = {"addr": hex(addr), "perm": perm, "path": path, "region": [hex(lo), hex(hi)]}
                if path.lower().endswith((".dll", ".exe")):
                    rec.update(module=os.path.basename(path).lower(), rva=hex(addr - base))
                return rec
        return {"addr": hex(addr), "path": None}

    def find_world(self) -> int | None:
        with self.game_lock:
            self.need_game()
            welt = self.bases.get("annowelt.dll")
            if welt is None:
                return None
            if self.world is not None and gamestate.is_world(gamestate.world_fields(self.mem, self.world)):
                return self.world
            self.world = gamestate.find_world(self.mem, welt, settle=0.05)
            return self.world

    def state(self, what: str = "world", types: list | None = None) -> dict:
        self.need_game()
        welt = self.bases.get("annowelt.dll")
        res: dict = {"pid": self.pid}
        if what in ("world", "all"):
            w = self.find_world()
            res["world"] = None if w is None else dict(gamestate.world_fields(self.mem, w), addr=hex(w))
        if what in ("prng", "all"):
            res["prng"] = gamestate.prng_check(self.mem, welt)
        if what in ("cities", "all"):
            res["cities"] = gamestate.cities(self.mem, welt)
        if what in ("players", "all"):
            res["players"] = gamestate.players(self.mem, welt, self.find_world())
        if what == "figures":       # not part of "all": hundreds of records per call
            res["figures"] = gamestate.figures(self.mem, welt, self.find_world())
        if what == "walkers":       # the figures of `types` with the tile record under each
            res["walkers"] = gamestate.walkers(self.mem, welt, types or [], self.find_world())
        return res

    def clocks(self) -> dict:
        """The guest clocks now (for host/guest clock pairs, timing.py)."""
        return clocks()

    # ---- quick save / load -------------------------------------------------------------------
    def _save_snapshot(self) -> dict:
        root = f"{self.game}/{SAVE_DIR}"
        snap = {}
        for dirpath, _dirs, files in os.walk(root):
            for fn in files:
                p = os.path.join(dirpath, fn)
                try:
                    st = os.stat(p)
                except OSError:
                    continue
                snap[os.path.relpath(p, self.game)] = (st.st_mtime_ns, st.st_size)
        return snap

    def quicksave(self, keep_as: str | None = None, wait: float = 20.0) -> dict:
        """F10 (playbook section 1), then wait until new or changed files in the copy's save folder
        (the profile user.dat excluded) are stable in size; keep every one of them in out/saves."""
        if keep_as is not None:
            check_name(keep_as, "keep_as")
        before = self._save_snapshot()
        self.key("F10")
        found: list[str] = []
        end = mono() + wait
        while mono() < end and not found:
            time.sleep(0.5)
            now = self._save_snapshot()
            changed = sorted(p for p, v in now.items() if before.get(p) != v
                             and not p.lower().endswith("user.dat"))
            if changed:
                time.sleep(0.7)
                again = self._save_snapshot()
                if all(again.get(p) == now.get(p) for p in changed):
                    found = changed
        res: dict = {"saved": found, "kept": []}
        if not found:
            res["error"] = f"no new file in {SAVE_DIR} within {wait} s"
        elif keep_as:
            os.makedirs(f"{self.out}/saves", exist_ok=True)
            for p in found:
                dst = f"{self.out}/saves/{keep_as}__{os.path.basename(p)}"
                shutil.copyfile(os.path.join(self.game, p), dst)
                res["kept"].append(dst)
        self.log("quicksave", **res)
        return res

    def quickload(self) -> dict:
        self.key("F9")
        self.world = None
        return {"key": "F9"}

    # ---- frame-time poller (timing check, section 4 item 5) ----------------------------------
    def poll_start(self, label: str = "timing", interval: float = 0.0005) -> dict:
        """Poll the world's game-ms counter about every `interval` s, timestamped on
        CLOCK_MONOTONIC_RAW. In single player every frame advances the counter once by
        min(trunc(dt * speed + 0.5), 100) (tick-loop.md section 2), so each change marks a frame."""
        check_name(label, "timing label")
        if self.poller:
            raise RuntimeError("poller already running")
        w = self.find_world()
        if w is None:
            raise RuntimeError("no world object (not in a game)")
        stop, started = threading.Event(), threading.Event()
        rec = {"label": label, "world": w, "stop": stop, "series": [], "start": None,
               "fails": 0, "error": None, "interval": interval}

        def run():
            try:
                series = rec["series"]
                last = gamestate.world_counter(self.mem, w)
                rec["start"] = dict(clocks(), counter=last)
                series.append((rec["start"]["raw"], last))
                started.set()
                while not stop.is_set():
                    c = gamestate.world_counter(self.mem, w)
                    if c is None:
                        rec["fails"] += 1
                    elif c != last:
                        series.append((raw(), c))
                        last = c
                    time.sleep(interval)
            except Exception as ex:  # noqa: BLE001 - reported by poll_stop
                rec["error"] = repr(ex)
            finally:
                started.set()

        rec["thread"] = threading.Thread(target=run, daemon=True)
        rec["thread"].start()
        if not started.wait(5) or rec["start"] is None or rec["start"]["counter"] is None:
            stop.set()
            raise RuntimeError(f"poller did not start: {rec['error']}")
        self.poller = rec
        return {"label": label, "start": rec["start"]}

    def poll_stop(self) -> dict | None:
        rec = self.poller
        if not rec:
            return None
        self.poller = None
        rec["stop"].set()
        rec["thread"].join(timeout=5)
        end_counter = gamestate.world_counter(self.mem, rec["world"]) if self.mem else None
        if end_counter is None:
            rec["fails"] += 1
        e = dict(clocks(), counter=end_counter)
        s, series = rec["start"], rec["series"]
        os.makedirs(f"{self.out}/timing", exist_ok=True)
        path = f"{self.out}/timing/{rec['label']}.jsonl"
        with open(path, "w") as f:
            for t, c in series:
                f.write(json.dumps({"t_raw": round(t, 6), "counter": c}) + "\n")
        steps = [b[1] - a[1] for a, b in zip(series, series[1:])]
        gaps = [b[0] - a[0] for a, b in zip(series, series[1:])]
        hist: dict[int, int] = {}
        for x in steps:
            hist[x] = hist.get(x, 0) + 1

        def pct(v, q):
            if not v:
                return None
            v = sorted(v)
            return v[min(len(v) - 1, int(q * len(v)))]

        changes = series[1:]
        res = {"label": rec["label"], "series": path, "frames": len(steps), "read_failures": rec["fails"],
               "thread_error": rec["error"], "thread_alive": rec["thread"].is_alive(),
               "mono_s": round(e["mono"] - s["mono"], 6), "raw_s": round(e["raw"] - s["raw"], 6),
               "real_s": round(e["real"] - s["real"], 6),
               "game_ms": None if end_counter is None else end_counter - s["counter"],
               # window edges: the start read and the end read each fall inside one frame, so the
               # window total is off by at most the first and the last step
               "edge_error_ms": (steps[0] + steps[-1]) if steps else None,
               "step_hist": {str(k): v for k, v in sorted(hist.items())},
               "step_ms": {"p50": pct(steps, .5), "p95": pct(steps, .95), "max": max(steps) if steps else None,
                           "at_cap_100": sum(1 for x in steps if x >= 100)},
               "frame_ms": {k: (round(1000 * v, 3) if v is not None else None) for k, v in
                            {"p50": pct(gaps, .5), "p95": pct(gaps, .95), "p99": pct(gaps, .99),
                             "max": max(gaps) if gaps else None}.items()},
               "start": s, "end": e}
        if len(changes) >= 2:
            (t0, c0), (t1, c1) = changes[0], changes[-1]
            res["changes"] = {"first": [round(t0, 6), c0], "last": [round(t1, 6), c1],
                              "game_ms": c1 - c0, "raw_s": round(t1 - t0, 6),
                              "game_ms_per_raw_s": round((c1 - c0) / (t1 - t0), 4) if t1 > t0 else None,
                              "frames": len(changes) - 1}
        self.log("timing", **{k: v for k, v in res.items() if k not in ("start", "end", "step_hist")})
        return res

    # ---- JSONL sampler (section 4 item 6) ----------------------------------------------------
    def sample_start(self, period_game_ms: int = 60000, save_every: int = 0, name: str = "sampler",
                     saves_on: bool = True) -> dict:
        """One JSONL record per `period_game_ms` of game time (default one game minute): clocks,
        world fields, every player's money, city stores. Every `save_every` records (0 = never) a
        quicksave is taken and kept as out/saves/m<minute>__<file>, while saves are on
        (sample_saves; a macro switches them off so F10 never lands in the middle of it)."""
        check_name(name, "sampler name")
        if self.sampler:
            raise RuntimeError("sampler already running")
        w = self.find_world()
        if w is None:
            raise RuntimeError("no world object (not in a game)")
        stop = threading.Event()
        path = f"{self.out}/{name}.jsonl"
        rec = {"stop": stop, "path": path, "n": 0, "error": None, "saves_on": saves_on, "saves": 0}

        def run():
            welt = self.bases.get("annowelt.dll")
            nxt = None
            with open(path, "a", buffering=1) as f:
                plist = gamestate.players(self.mem, welt, w)   # found once; money re-read per record
                f.write(json.dumps({"event": "players", "players": plist}) + "\n")
                while not stop.is_set():
                    try:
                        if self.crashed:
                            rec["error"] = "game crashed"
                            f.write(json.dumps({"event": "crash", "crash": self.crashed}) + "\n")
                            return
                        if not self.alive():
                            rec["error"] = "game exited"
                            f.write(json.dumps({"event": "game_exited", "t_raw": round(raw(), 3)}) + "\n")
                            return
                        wf = gamestate.world_fields(self.mem, self.find_world() or w)
                        if wf and nxt is None:
                            nxt = (wf["counter"] // period_game_ms + 1) * period_game_ms
                        if wf and wf["counter"] >= nxt:
                            money = {}
                            for p in plist:
                                mb = self.mem.read(int(p["addr"], 16) + gamestate.PLAYER_MONEY, 4)
                                money[f"{p['kind'] or p['name']}"] = gamestate.i32(mb) if mb else None
                            r = {"game_min": round(wf["counter"] / 60000, 3), "t_raw": round(raw(), 3),
                                 "t_real": round(time.time(), 3), "world": wf, "money": money,
                                 "cities": gamestate.cities(self.mem, welt)}
                            rec["n"] += 1
                            if save_every and rec["n"] % save_every == 0 and rec["saves_on"]:
                                r["save"] = self.quicksave(keep_as=f"m{int(wf['counter'] // 60000):04d}")
                                rec["saves"] += 1
                            f.write(json.dumps(r) + "\n")
                            nxt += period_game_ms
                    except Exception as ex:  # noqa: BLE001 - record and continue
                        rec["error"] = repr(ex)
                        f.write(json.dumps({"event": "error", "error": repr(ex)}) + "\n")
                    stop.wait(0.5)

        rec["thread"] = threading.Thread(target=run, daemon=True)
        rec["thread"].start()
        self.sampler = rec
        return {"path": path, "saves_on": saves_on}

    def sample_saves(self, on: bool) -> dict:
        if not self.sampler:
            raise RuntimeError("no sampler running")
        self.sampler["saves_on"] = bool(on)
        self.log("sample_saves", on=bool(on))
        return {"saves_on": bool(on)}

    def sample_stop(self) -> dict | None:
        rec = self.sampler
        if not rec:
            return None
        self.sampler = None
        rec["stop"].set()
        rec["thread"].join(timeout=30)
        return {"path": rec["path"], "records": rec["n"], "saves": rec["saves"], "error": rec["error"],
                "thread_alive": rec["thread"].is_alive()}


# ---- JSON-lines server ------------------------------------------------------------------------
COMMANDS = {"prepare", "launch", "find_game", "stop", "status", "key", "click", "click_px", "move",
            "move_px", "drag", "wheel", "frame", "screenshot", "recognize", "learn", "read",
            "module_bases", "state", "quicksave", "quickload", "poll_start", "poll_stop",
            "sample_start", "sample_stop", "sample_saves", "sync", "upper_files", "audio_check",
            "calibrate", "forget", "reg_add", "clocks", "scan", "whereis"}


def serve(inst: Instance, stdin=None, stdout=None) -> int:
    """The command loop. Whatever ends it (stop, EOF, broken pipe, a signal, an error in the loop
    itself), the instance is stopped in `finally`; stop() is idempotent."""
    out = stdout or sys.stdout
    src = stdin or sys.stdin

    def reply(obj):
        try:
            out.write(json.dumps(obj, default=str) + "\n")
            out.flush()
        except (OSError, ValueError):       # the controller is gone; keep going until EOF/finally
            pass

    def on_signal(signum, _frame):
        raise SystemExit(128 + signum)

    if threading.current_thread() is threading.main_thread():
        for name in ("SIGTERM", "SIGHUP"):
            if hasattr(signal, name):
                signal.signal(getattr(signal, name), on_signal)
    try:
        reply({"id": 0, "ok": True, "result": {"hello": "anno-wine-agent", "run": inst.run,
                                               "display": inst.display, "t_mono": mono()}})
        for line in src:
            line = line.strip()
            if not line:
                continue
            cid = None
            try:
                msg = json.loads(line)
                if not isinstance(msg, dict):
                    raise ValueError("a command must be a JSON object")
                cid, cmd, args = msg.get("id"), msg.get("cmd"), msg.get("args") or {}
                if not isinstance(args, dict):
                    raise ValueError("args must be a JSON object")
            except ValueError as ex:
                reply({"id": cid, "ok": False, "error": f"bad command: {ex}"})
                continue
            if cmd not in COMMANDS:
                reply({"id": cid, "ok": False, "error": f"unknown command {cmd!r}"})
                continue
            try:
                result = getattr(inst, cmd)(**args)
                reply({"id": cid, "ok": True, "result": result, "t_mono": mono()})
                if cmd == "stop":
                    break
            except Exception as ex:  # noqa: BLE001 - every failure goes back to the controller
                reply({"id": cid, "ok": False, "error": f"{type(ex).__name__}: {ex}",
                       "trace": traceback.format_exc(limit=3)})
    finally:
        inst.stop()
    return 0


def cleanup(force: bool = False) -> dict:
    """Reap runs under /root/anno/runs whose agent is gone: kill their Wine processes and Xvfb,
    unmount their overlays, delete the folders. A run whose agent is alive is never touched; a
    run marked .keep is kept unless force. Never touches the base copies."""
    report: dict = {"removed": [], "live": [], "kept": [], "killed": {}, "errors": []}
    runs = sorted(os.listdir(RUNS)) if os.path.isdir(RUNS) else []
    mounts = [ln.split()[1] for ln in open("/proc/mounts")]
    for d in runs:
        run = f"{RUNS}/{d}"
        apid = read_pid(f"{run}/agent.pid")
        if pid_alive(apid) and "agent.py" in cmdline(apid):
            report["live"].append(d)
            continue
        if os.path.exists(f"{run}/.keep") and not force:
            report["kept"].append(d)
            continue
        killed = kill_pids(prefix_pids(f"{run}/prefix"))
        xpid = read_pid(f"{run}/xvfb.pid")
        if pid_alive(xpid) and cmdline(xpid).startswith("Xvfb"):
            killed += kill_pids([xpid])
        if killed:
            report["killed"][d] = killed
            time.sleep(0.5)
        for m in sorted((m for m in mounts if m.startswith(run + "/")), reverse=True):
            subprocess.run(["umount", "-l", m], capture_output=True)
        try:
            shutil.rmtree(run)
            report["removed"].append(d)
        except OSError as ex:
            report["errors"].append(f"{d}: {ex}")
    return report


def main(argv=None) -> int:
    ap = argparse.ArgumentParser(description=__doc__.split("\n")[0])
    ap.add_argument("mode", choices=["serve", "cleanup"])
    ap.add_argument("--display", default=":91")
    ap.add_argument("--run", default=None, help="run id (folder under /root/anno/runs)")
    ap.add_argument("--out-win", default=None, help="Windows-visible folder for outputs; must lie inside the repository's "
                         f"cache/orig ({OUT_WIN_BASE})")
    ap.add_argument("--copy", choices=["overlay", "copy"], default="overlay")
    ap.add_argument("--keep", action="store_true", help="keep the run folder after stop (.keep marker)")
    ap.add_argument("--sigs", default=None, help="signature file (default tools/orig/screens_sig.json)")
    ap.add_argument("--audio", action="store_true", help="let Wine play sound (default: silent)")
    ap.add_argument("--winedebug", default="-all", help="WINEDEBUG for the game (default -all)")
    ap.add_argument("--crash-dialog", action="store_true", help="keep Wine's crash dialog (default: off)")
    ap.add_argument("--force", action="store_true", help="cleanup: also remove runs marked .keep")
    a = ap.parse_args(argv)
    if a.mode == "cleanup":
        print(json.dumps(cleanup(a.force)))
        return 0
    inst = Instance(a.display, a.run, a.out_win, a.copy, a.keep, a.sigs, a.audio, a.winedebug,
                    a.crash_dialog)
    return serve(inst)


if __name__ == "__main__":
    sys.exit(main())
