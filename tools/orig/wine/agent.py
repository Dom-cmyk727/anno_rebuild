#!/usr/bin/env python3
"""T16 Wine runner: one instance of the original under Wine on its own invisible X display.

Runs inside the AnnoHarness WSL distro as root (docs/testing/original-wine.md). One instance =
  - a disposable copy: overlayfs views of the base game (/root/anno/game) and the base prefix
    (/root/anno/prefix), so a run never changes the base copies and starts in milliseconds;
    falls back to a full copy (cp -a) where overlayfs is unavailable;
  - its own Xvfb display (:91, :92, ...) and therefore its own wineserver (one per prefix);
  - outputs in <run>/out (screenshots, timing series, sampler JSONL, quicksave copies), copied to a
    Windows folder (cache/orig/<session>/<instance>) on `sync` and on `stop`.

Input is XTEST through xdotool on that display only; the owner's desktop is never touched. Memory is
read-only (/proc/<pid>/mem; root may read any process despite ptrace_scope=1).

Two ways to use it:
  serve   JSON lines on stdin/stdout, one command per line: {"id": n, "cmd": "...", "args": {...}}
          -> {"id": n, "ok": true, "result": ...}. tools/orig/backend.py (WineBackend) speaks this.
          EOF on stdin stops the instance and cleans up, so a dead controller leaves nothing behind.
  python  `from agent import Instance` inside the distro (the same methods, no pipe).

Rules (TASKS.md T16): the game copy stays inside the distro; read-only memory; `pkill -x` only.
"""

from __future__ import annotations

import argparse
import json
import os
import shutil
import signal
import subprocess
import sys
import threading
import time
import traceback

_HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, os.path.dirname(_HERE))          # tools/orig: gamestate, screensig
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
MAX_REGION = 256 << 20         # skip larger mappings in vtable scans (not game heap)
AUDIO_OFF = "winepulse.drv,winealsa.drv,wineoss.drv=d"   # WINEDLLOVERRIDES: no audio driver
AUDIO_LIBS = ("winepulse", "winealsa", "wineoss", "libpulse", "libasound")   # audio guard looks for these

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


def mono() -> float:
    return time.clock_gettime(time.CLOCK_MONOTONIC)


class ProcMem:
    """gamestate's memory interface over /proc/<pid>/mem and /proc/<pid>/maps."""

    def __init__(self, pid: int):
        self.pid = pid
        self.f = open(f"/proc/{pid}/mem", "rb", buffering=0)

    def read(self, addr: int, n: int) -> bytes | None:
        try:
            return os.pread(self.f.fileno(), n, addr)
        except OSError:
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
                 audio: bool = False):
        self.display = display
        self.audio = audio
        self.num = display.lstrip(":")
        self.run_id = run_id or time.strftime(f"run-%Y%m%d-%H%M%S-d{self.num}")
        self.run = f"{RUNS}/{self.run_id}"
        self.game = f"{self.run}/game"
        self.prefix = f"{self.run}/prefix"
        self.out = f"{self.run}/out"
        self.out_win = out_win
        self.copy = copy
        self.keep = keep
        self.sig_path = sig_path
        self.mounts: list[str] = []
        self.xvfb: subprocess.Popen | None = None
        self.wine: subprocess.Popen | None = None
        self.pid: int | None = None
        self.mem: ProcMem | None = None
        self.bases: dict[str, int] = {}
        self.world: int | None = None
        self.input_lock = threading.Lock()
        self.poller: dict | None = None
        self.sampler: dict | None = None
        self.log_f = None
        self.guard_stop = threading.Event()
        self.audio_killed: list | None = None
        self.scan_hits: list[int] | None = None

    # ---- environment -------------------------------------------------------------------------
    def env(self) -> dict:
        e = dict(os.environ)
        e.pop("WAYLAND_DISPLAY", None)
        e.update(DISPLAY=self.display, WINEPREFIX=self.prefix, WINEARCH="win32",
                 WINEDEBUG=e.get("ANNO_WINEDEBUG", "-all"))
        if not self.audio:
            # Silent runs (owner, 2026-10-22): WSLg hands every distro a PulseAudio socket
            # (PULSE_SERVER=unix:/mnt/wslg/PulseServer) that plays on the owner's speakers.
            # Remove it and switch off Wine's audio drivers, so no sound leaves the distro.
            # Without PULSE_SERVER, libpulse would still find WSLg's socket through
            # XDG_RUNTIME_DIR (/mnt/wslg/runtime-dir), so each run gets a private, empty one.
            # The audio guard (launch) kills the instance if any audio library loads anyway.
            for k in [k for k in e if k.startswith("PULSE_")]:
                e.pop(k)
            e["XDG_RUNTIME_DIR"] = f"{self.run}/xdg"
            e["WINEDLLOVERRIDES"] = ";".join(filter(None, [e.get("WINEDLLOVERRIDES"), AUDIO_OFF]))
        return e

    def log(self, kind: str, **kw) -> None:
        if self.log_f is None:
            os.makedirs(self.out, exist_ok=True)
            self.log_f = open(f"{self.out}/actions.jsonl", "a", buffering=1)
        rec = {"t": round(mono(), 4), "kind": kind}
        rec.update(kw)
        self.log_f.write(json.dumps(rec) + "\n")

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
        if os.path.exists(self.run):
            raise RuntimeError(f"run folder {self.run} exists; pick another run id")
        os.makedirs(self.out, exist_ok=True)
        os.makedirs(f"{self.run}/xdg", mode=0o700, exist_ok=True)
        t0 = mono()
        mode = self.copy
        if mode == "overlay":
            try:
                self._overlay(BASE_GAME, self.game)
                self._overlay(BASE_PREFIX, self.prefix)
            except RuntimeError as e:
                self.log("overlay_failed", error=str(e))
                self._unmount()
                mode = "copy"
        if mode == "copy":
            shutil.rmtree(self.game, ignore_errors=True)
            shutil.rmtree(self.prefix, ignore_errors=True)
            subprocess.run(["cp", "-a", BASE_GAME, self.game], check=True)
            subprocess.run(["cp", "-a", BASE_PREFIX, self.prefix], check=True)
        self.copy = mode
        res = {"run": self.run, "copy": mode, "seconds": round(mono() - t0, 3)}
        if not self.audio:
            res["profiles"] = self.silence_profiles()
        self.log("prepare", **res)
        return res

    def silence_profiles(self) -> list[dict]:
        """In-game music and speech volume to the game's minimum in every profile of this copy
        (tools/orig/userdat.py). Writes land in the overlay's upper layer, never in the base."""
        out = []
        root = f"{self.game}/SaveGame"
        for name in sorted(os.listdir(root)) if os.path.isdir(root) else []:
            p = f"{root}/{name}/user.dat"
            if not os.path.isfile(p):
                continue
            data = open(p, "rb").read()
            before = userdat.read(data).get("volume")
            with open(p, "wb") as f:
                f.write(userdat.set_volume(data))
            out.append({"profile": name, "volume_before": before,
                        "volume_after": userdat.read(open(p, "rb").read()).get("volume")})
        return out

    def reg_add(self, key: str, name: str, value: str, kind: str = "REG_SZ") -> dict:
        """Set a registry value in THIS run's prefix (disposable; the base prefix is untouched).
        Call between prepare and launch. Example: the community's hidden-multiplayer switch
        HKCU\\Software\\Max-Design\\Anno1503 MULTIPLAYER=1 (community-landscape.md)."""
        if kind not in ("REG_SZ", "REG_DWORD"):
            raise ValueError(f"unsupported registry type {kind}")
        r = subprocess.run(["wine", "reg", "add", key, "/v", name, "/t", kind, "/d", str(value), "/f"],
                           env=self.env(), capture_output=True, text=True, timeout=60)
        subprocess.run(["wineserver", "-w"], env=self.env(), capture_output=True, timeout=60)
        res = {"key": key, "name": name, "kind": kind, "value": value, "rc": r.returncode,
               "out": (r.stdout + r.stderr).strip()[-300:]}
        self.log("reg_add", **res)
        if r.returncode != 0:
            raise RuntimeError(f"wine reg add failed: {res['out']}")
        return res

    def _unmount(self) -> None:
        for m in reversed(self.mounts):
            if subprocess.run(["umount", m], capture_output=True).returncode != 0:
                subprocess.run(["umount", "-l", m], capture_output=True)
        self.mounts.clear()

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
        if not self._display_free():
            raise RuntimeError(f"display {self.display} is in use (/tmp/.X{self.num}-lock)")
        self.xvfb = subprocess.Popen(
            ["Xvfb", self.display, "-screen", "0", f"{DESKTOP[0]}x{DESKTOP[1]}x24", "-nolisten", "tcp"],
            stdout=subprocess.DEVNULL, stderr=open(f"{self.out}/xvfb.log", "w"))
        for _ in range(200):
            if self._x_listening():
                break
            if self.xvfb.poll() is not None:
                raise RuntimeError(f"Xvfb exited with {self.xvfb.returncode} (see xvfb.log)")
            time.sleep(0.05)
        else:
            raise RuntimeError("Xvfb did not come up within 10 s (see xvfb.log)")
        self.wine = subprocess.Popen(
            ["wine", "explorer", f"/desktop=anno{self.num},{DESKTOP[0]}x{DESKTOP[1]}", "1503Startup.exe"],
            cwd=self.game, env=self.env(), stdout=open(f"{self.out}/wine.log", "w"),
            stderr=subprocess.STDOUT, start_new_session=True)
        self.log("launch", display=self.display, xvfb=self.xvfb.pid, wine=self.wine.pid, audio=self.audio)
        if not self.audio:
            threading.Thread(target=self._audio_guard, daemon=True).start()
        return {"display": self.display, "xvfb_pid": self.xvfb.pid, "audio": self.audio}

    # ---- audio guard (silent runs) -----------------------------------------------------------
    def audio_check(self) -> dict:
        """Every process of this instance: PULSE_* variables in its environment and audio
        libraries in its address space. Both must be empty in a silent run."""
        pids = self._prefix_pids()
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

    def _kill_all(self) -> None:
        subprocess.run(["wineserver", "-k"], env=self.env(), capture_output=True, timeout=30)
        for p in self._prefix_pids():
            try:
                os.kill(p, signal.SIGKILL)
            except OSError:
                pass

    def _audio_guard(self) -> None:
        while not self.guard_stop.is_set():
            r = self.audio_check()
            if r["violations"]:
                self.audio_killed = r["violations"]
                self._kill_all()
                self.log("audio_guard_kill", violations=r["violations"])
                return
            self.guard_stop.wait(0.5)

    def _prefix_pids(self) -> list[int]:
        want = f"WINEPREFIX={self.prefix}".encode()
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

    def find_game(self, wait: float = 0.0) -> int | None:
        """The process of this instance that maps AnnoWelt.dll (the world lives there)."""
        end = mono() + wait
        while True:
            for pid in self._prefix_pids():
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
        return bool(self.pid) and os.path.exists(f"/proc/{self.pid}")

    def stop(self) -> dict:
        self.guard_stop.set()
        self.sample_stop()
        self.poll_stop()
        res = {"run": self.run}
        subprocess.run(["wineserver", "-k"], env=self.env(), capture_output=True, timeout=30)
        if self.wine:
            try:
                self.wine.wait(timeout=15)
            except subprocess.TimeoutExpired:
                os.killpg(self.wine.pid, signal.SIGKILL)
        if self.xvfb:
            self.xvfb.terminate()
            try:
                self.xvfb.wait(timeout=10)
            except subprocess.TimeoutExpired:
                self.xvfb.kill()
        left = self._prefix_pids()
        for p in left:
            try:
                os.kill(p, signal.SIGKILL)
            except OSError:
                pass
        res["leftover_pids_killed"] = left
        if self.mem:
            self.mem.close()
            self.mem = None
        res["upper_files"] = self.upper_files()
        self.log("stop", **{k: v for k, v in res.items() if k != "upper_files"})
        res["synced_to"] = self.sync()
        self._unmount()
        if not self.keep:
            shutil.rmtree(self.run, ignore_errors=True)
            res["removed"] = True
        return res

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
        if self.log_f:
            self.log_f.flush()
        os.makedirs(self.out_win, exist_ok=True)
        shutil.copytree(self.out, self.out_win, dirs_exist_ok=True)
        return self.out_win

    # ---- input (XTEST via xdotool, this display only) ----------------------------------------
    def _xdo(self, *args: str) -> None:
        r = subprocess.run(["xdotool", *args], env=self.env(), capture_output=True, text=True)
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
                time.sleep(hold)
                self._xdo("keyup", k)
            else:
                self._xdo("key", k)
        self.log("key", key=name, xkey=k)
        return {"key": k}

    def move_px(self, x: int, y: int) -> dict:
        with self.input_lock:
            self._xdo("mousemove", "--sync", str(x), str(y))
        return {"x": x, "y": y}

    def click_px(self, x: int, y: int, button: int = 1, count: int = 1) -> dict:
        with self.input_lock:
            self._xdo("mousemove", "--sync", str(x), str(y))
            time.sleep(0.03)
            self._xdo("click", "--repeat", str(count), "--delay", "80", str(button))
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
        with self.input_lock:
            self._xdo("mousemove", "--sync", str(x), str(y))
            self._xdo("mousedown", str(button))
            for i in range(1, steps + 1):
                self._xdo("mousemove", "--sync", str(x + (x2 - x) * i // steps), str(y + (y2 - y) * i // steps))
                time.sleep(0.02)
            self._xdo("mouseup", str(button))
        self.log("drag", frm=[x, y], to=[x2, y2])
        return {"from": [x, y], "to": [x2, y2]}

    def wheel(self, nx: float, ny: float, amount: int) -> dict:
        x, y = self._px(nx, ny)
        button = "4" if amount > 0 else "5"
        with self.input_lock:
            self._xdo("mousemove", "--sync", str(x), str(y))
            self._xdo("click", "--repeat", str(abs(amount)), "--delay", "60", button)
        self.log("wheel", x=x, y=y, amount=amount)
        return {"x": x, "y": y, "amount": amount}

    # ---- screen ------------------------------------------------------------------------------
    def grab(self):
        from PIL import ImageGrab
        return ImageGrab.grab(xdisplay=self.display)

    def frame_image(self):
        img = self.grab()
        fr = self.frame_from(img)
        return img.crop((fr["x"], fr["y"], fr["x"] + fr["w"], fr["y"] + fr["h"])), fr

    def frame_from(self, img) -> dict:
        """The game frame inside the desktop: menus are 800x600 at the top left, a running game
        fills the desktop. Decided from the screen: outside the menu frame the desktop is one
        flat colour while menus show."""
        right = img.crop((MENU_FRAME[0] + 8, 8, DESKTOP[0] - 8, DESKTOP[1] - 8)).convert("L")
        lo, hi = right.getextrema()
        w, h = MENU_FRAME if hi - lo <= 2 else DESKTOP
        return {"x": 0, "y": 0, "w": w, "h": h}

    def screenshot(self, name: str, dest: str | None = None, full: bool = False, box=None,
                   scale: int = 1) -> dict:
        """PNG of the game frame (or the whole desktop with full=True). box = [x0, y0, x1, y1] in
        frame fractions crops it; scale enlarges a crop (nearest neighbour) for reading digits."""
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

    def recognize(self) -> dict:
        img, fr = self.frame_image()
        res = screensig.recognize(img, screensig.load(self.sig_path))
        res["frame"] = [fr["w"], fr["h"]]
        return res

    def learn(self, name: str, regions=None, max_dist: int = screensig.DEFAULT_MAX_DIST) -> dict:
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

    # ---- memory ------------------------------------------------------------------------------
    def need_game(self) -> None:
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
        return {"value": value, "count": len(hits), "addrs": [hex(a) for a in hits[:64]]}

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
        self.need_game()
        welt = self.bases.get("annowelt.dll")
        if welt is None:
            return None
        if self.world is not None and gamestate.is_world(gamestate.world_fields(self.mem, self.world)):
            return self.world
        self.world = gamestate.find_world(self.mem, welt)
        return self.world

    def state(self, what: str = "world") -> dict:
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
        return res

    # ---- quick save / load -------------------------------------------------------------------
    def quicksave(self, keep_as: str | None = None, wait: float = 20.0) -> dict:
        """F10 (game-mechanics.md section 1), then wait for a file in the copy's upper layer to be
        newer than the key press and stable in size; optionally keep a copy in out/saves."""
        before = mono()
        wall = time.time()
        self.key("F10")
        found = None
        end = before + wait
        while mono() < end:
            time.sleep(0.5)
            newer = [f for f in self.upper_files() if f["mtime"] >= wall - 0.5]
            if newer:
                cand = newer[-1]
                time.sleep(0.5)
                p = os.path.join(self.game + ".upper", cand["path"])
                if os.path.exists(p) and os.path.getsize(p) == cand["size"]:
                    found = cand
                    break
        res = {"saved": found}
        if found and keep_as:
            os.makedirs(f"{self.out}/saves", exist_ok=True)
            dst = f"{self.out}/saves/{keep_as}{os.path.splitext(found['path'])[1]}"
            shutil.copyfile(os.path.join(self.game + ".upper", found["path"]), dst)
            res["kept"] = dst
        self.log("quicksave", **res)
        return res

    def quickload(self) -> dict:
        self.key("F9")
        self.world = None
        return {"key": "F9"}

    # ---- frame-time poller (timing check, section 4 item 5) ----------------------------------
    def poll_start(self, label: str = "timing", interval: float = 0.0005) -> dict:
        """Poll the world's game-ms counter about every `interval` s on CLOCK_MONOTONIC. In single
        player every frame advances the counter once by round(dt * speed), capped at 100
        (tick-loop.md section 2), so each change marks a frame."""
        if self.poller:
            raise RuntimeError("poller already running")
        w = self.find_world()
        if w is None:
            raise RuntimeError("no world object (not in a game)")
        stop = threading.Event()
        rec = {"label": label, "stop": stop, "series": [], "start": None, "end": None}

        def clocks():
            return {"mono": mono(), "raw": time.clock_gettime(time.CLOCK_MONOTONIC_RAW),
                    "real": time.clock_gettime(time.CLOCK_REALTIME)}

        def run():
            series = rec["series"]
            last = gamestate.world_counter(self.mem, w)
            rec["start"] = dict(clocks(), counter=last)
            series.append((rec["start"]["mono"], last))
            while not stop.is_set():
                c = gamestate.world_counter(self.mem, w)
                if c is not None and c != last:
                    series.append((mono(), c))
                    last = c
                time.sleep(interval)
            rec["end"] = dict(clocks(), counter=last)

        rec["thread"] = threading.Thread(target=run, daemon=True)
        rec["thread"].start()
        while rec["start"] is None:
            time.sleep(0.001)
        self.poller = rec
        return {"label": label, "start": rec["start"]}

    def poll_stop(self) -> dict | None:
        rec = self.poller
        if not rec:
            return None
        rec["stop"].set()
        rec["thread"].join(timeout=5)
        self.poller = None
        series = rec["series"]
        os.makedirs(f"{self.out}/timing", exist_ok=True)
        path = f"{self.out}/timing/{rec['label']}.jsonl"
        with open(path, "w") as f:
            for t, c in series:
                f.write(json.dumps({"t": round(t, 6), "counter": c}) + "\n")
        s, e = rec["start"], rec["end"]
        steps = [b[1] - a[1] for a, b in zip(series, series[1:])]
        gaps = [b[0] - a[0] for a, b in zip(series, series[1:])]

        def pct(v, q):
            if not v:
                return None
            v = sorted(v)
            return v[min(len(v) - 1, int(q * len(v)))]

        dm = e["mono"] - s["mono"]
        res = {"label": rec["label"], "series": path, "frames": len(steps),
               "mono_s": round(dm, 4), "raw_s": round(e["raw"] - s["raw"], 4),
               "real_s": round(e["real"] - s["real"], 4), "game_ms": e["counter"] - s["counter"],
               "game_ms_per_mono_s": round((e["counter"] - s["counter"]) / dm, 2) if dm else None,
               "step_ms": {"p50": pct(steps, .5), "p95": pct(steps, .95), "p99": pct(steps, .99),
                           "max": max(steps) if steps else None,
                           "at_cap_100": sum(1 for x in steps if x >= 100)},
               "frame_ms": {k: (round(1000 * v, 2) if v is not None else None) for k, v in
                            {"p50": pct(gaps, .5), "p95": pct(gaps, .95), "p99": pct(gaps, .99),
                             "max": max(gaps) if gaps else None}.items()},
               "start": s, "end": e}
        self.log("timing", **{k: v for k, v in res.items() if k not in ("start", "end")})
        return res

    # ---- JSONL sampler (section 4 item 6) ----------------------------------------------------
    def sample_start(self, period_game_ms: int = 60000, save_every: int = 0, name: str = "sampler") -> dict:
        """One JSONL record per `period_game_ms` of game time (default one game minute): clocks,
        world fields, PRNG cursor, cities. Every `save_every` records (0 = never) a quicksave is
        taken and kept as out/saves/m<minute>."""
        if self.sampler:
            raise RuntimeError("sampler already running")
        w = self.find_world()
        if w is None:
            raise RuntimeError("no world object (not in a game)")
        stop = threading.Event()
        path = f"{self.out}/{name}.jsonl"
        rec = {"stop": stop, "path": path, "n": 0, "error": None}

        def run():
            welt = self.bases.get("annowelt.dll")
            nxt = None
            plist = gamestate.players(self.mem, welt, w)   # found once; money re-read per record
            with open(path, "a", buffering=1) as f:
                f.write(json.dumps({"event": "players", "players": plist}) + "\n")
                while not stop.is_set():
                    try:
                        if not self.alive():
                            rec["error"] = "game exited"
                            f.write(json.dumps({"event": "game_exited", "t_mono": round(mono(), 3)}) + "\n")
                            return
                        wf = gamestate.world_fields(self.mem, self.find_world() or w)
                        if wf and nxt is None:
                            nxt = (wf["counter"] // period_game_ms + 1) * period_game_ms
                        if wf and wf["counter"] >= nxt:
                            money = {}
                            for p in plist:
                                mb = self.mem.read(int(p["addr"], 16) + gamestate.PLAYER_MONEY, 4)
                                money[f"{p['kind'] or p['name']}"] = gamestate.i32(mb) if mb else None
                            r = {"game_min": round(wf["counter"] / 60000, 3), "t_mono": round(mono(), 3),
                                 "t_raw": round(time.clock_gettime(time.CLOCK_MONOTONIC_RAW), 3),
                                 "t_real": round(time.time(), 3), "world": wf, "money": money,
                                 "cities": gamestate.cities(self.mem, welt)}
                            rec["n"] += 1
                            if save_every and rec["n"] % save_every == 0:
                                r["save"] = self.quicksave(keep_as=f"m{int(wf['counter'] // 60000):04d}")
                            f.write(json.dumps(r) + "\n")
                            nxt += period_game_ms
                    except Exception as ex:  # noqa: BLE001 - record and continue
                        rec["error"] = repr(ex)
                        f.write(json.dumps({"event": "error", "error": repr(ex)}) + "\n")
                    stop.wait(0.5)

        rec["thread"] = threading.Thread(target=run, daemon=True)
        rec["thread"].start()
        self.sampler = rec
        return {"path": path}

    def sample_stop(self) -> dict | None:
        rec = self.sampler
        if not rec:
            return None
        rec["stop"].set()
        rec["thread"].join(timeout=30)
        self.sampler = None
        return {"path": rec["path"], "records": rec["n"], "error": rec["error"]}

    def clocks(self) -> dict:
        """The guest clocks now (for host/guest clock pairs, timing.py)."""
        return {"mono": mono(), "raw": time.clock_gettime(time.CLOCK_MONOTONIC_RAW),
                "real": time.clock_gettime(time.CLOCK_REALTIME)}

    def status(self) -> dict:
        return {"run": self.run, "display": self.display, "pid": self.pid, "alive": self.alive(),
                "copy": self.copy, "sampler": None if not self.sampler else
                {"records": self.sampler["n"], "error": self.sampler["error"]}}


# ---- JSON-lines server ------------------------------------------------------------------------
COMMANDS = {"prepare", "launch", "find_game", "stop", "status", "key", "click", "click_px", "move",
            "move_px", "drag", "wheel", "frame", "screenshot", "recognize", "learn", "read",
            "module_bases", "state", "quicksave", "quickload", "poll_start", "poll_stop",
            "sample_start", "sample_stop", "sync", "upper_files", "audio_check", "calibrate", "forget",
            "reg_add", "clocks", "scan", "whereis"}


def serve(inst: Instance) -> int:
    out = sys.stdout
    stopped = False

    def reply(obj):
        out.write(json.dumps(obj) + "\n")
        out.flush()

    reply({"id": 0, "ok": True, "result": {"hello": "anno-wine-agent", "run": inst.run,
                                           "display": inst.display, "t_mono": mono()}})
    for line in sys.stdin:
        line = line.strip()
        if not line:
            continue
        try:
            msg = json.loads(line)
        except ValueError:
            reply({"id": None, "ok": False, "error": "bad json"})
            continue
        cid, cmd, args = msg.get("id"), msg.get("cmd"), msg.get("args") or {}
        if cmd not in COMMANDS:
            reply({"id": cid, "ok": False, "error": f"unknown command {cmd!r}"})
            continue
        try:
            result = getattr(inst, cmd)(**args)
            reply({"id": cid, "ok": True, "result": result, "t_mono": mono()})
            if cmd == "stop":
                stopped = True
                break
        except Exception as ex:  # noqa: BLE001 - every failure goes back to the controller
            reply({"id": cid, "ok": False, "error": f"{type(ex).__name__}: {ex}",
                   "trace": traceback.format_exc(limit=3)})
    if not stopped:
        inst.stop()
    return 0


def main(argv=None) -> int:
    ap = argparse.ArgumentParser(description=__doc__.split("\n")[0])
    ap.add_argument("mode", choices=["serve", "cleanup"])
    ap.add_argument("--display", default=":91")
    ap.add_argument("--run", default=None, help="run id (folder under /root/anno/runs)")
    ap.add_argument("--out-win", default=None, help="Windows-visible folder for outputs (/mnt/c/...)")
    ap.add_argument("--copy", choices=["overlay", "copy"], default="overlay")
    ap.add_argument("--keep", action="store_true", help="keep the run folder after stop")
    ap.add_argument("--sigs", default=None, help="signature file (default tools/orig/screens_sig.json)")
    ap.add_argument("--audio", action="store_true", help="let Wine play sound (default: silent)")
    a = ap.parse_args(argv)
    if a.mode == "cleanup":
        return cleanup()
    inst = Instance(a.display, a.run, a.out_win, a.copy, a.keep, a.sigs, a.audio)
    return serve(inst)


def cleanup() -> int:
    """Remove what crashed runs left under /root/anno/runs: unmount their overlays and delete the
    folders. A run with a live Wine process (WINEPREFIX inside it) is left alone. Never touches
    the base copies."""
    live = set()
    for p in os.listdir("/proc"):
        if not p.isdigit():
            continue
        try:
            env = open(f"/proc/{p}/environ", "rb").read().split(b"\0")
        except OSError:
            continue
        for e in env:
            if e.startswith(f"WINEPREFIX={RUNS}/".encode()):
                live.add(e.decode().split("=", 1)[1][len(RUNS) + 1:].split("/")[0])
    runs = sorted(os.listdir(RUNS)) if os.path.isdir(RUNS) else []
    dead = [d for d in runs if d not in live]
    mounts = [ln.split()[1] for ln in open("/proc/mounts")
              if ln.split()[1].startswith(RUNS + "/") and ln.split()[1][len(RUNS) + 1:].split("/")[0] in dead]
    for m in sorted(mounts, reverse=True):
        subprocess.run(["umount", "-l", m], capture_output=True)
    for d in dead:
        shutil.rmtree(f"{RUNS}/{d}", ignore_errors=True)
    print(json.dumps({"unmounted": mounts, "removed": dead, "live_kept": sorted(live)}))
    return 0


if __name__ == "__main__":
    sys.exit(main())
