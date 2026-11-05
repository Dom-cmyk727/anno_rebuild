#!/usr/bin/env python3
"""window_state_case.py: the native play window minimised, restored and covered while an edit waits
(W4-WIN-CLOSE C2; Windows only, GPU window).

    python tools/test/window_state_case.py <anno1503.exe> <new out dir> [--adapter N]

Runs tools/test/viewer_wiring/boundary_window_state.script on the GPU renderer with the window trace.
When the title first says a change is "not drawn yet" the script is holding the chunk builders for a
fixed time; this driver then, by process id: minimises the window for 2.5 s, restores it for 1.5 s,
covers it with an opaque topmost window for 2.5 s, and lets it be. It writes <out>/phases.txt (wall
times and the window's rectangle and iconic/visible state at each phase) and <out>/timeline.txt (the gaps between frame attempts, and every frame that was not presented, in trace time) and <out>/outcomes.txt (the
trace's frame outcomes in order, run-length encoded, with acquire and present statuses): what the
backend actually did, not what a minimised or covered window is assumed to do. It asserts only what
the script asserts (the change is Visible after the release and not before); it makes no claim that
a covered window does or does not present successfully. Exit: the window's status, 2 for bad
arguments."""
import ctypes
import ctypes.wintypes as wt
import os
import subprocess
import sys
import time
from pathlib import Path

HERE = Path(__file__).resolve().parents[2]
user32 = ctypes.windll.user32
user32.SetProcessDPIAware()
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


def state(hwnd):
    r = wt.RECT()
    user32.GetWindowRect(hwnd, ctypes.byref(r))
    return f"rect=({r.left},{r.top},{r.right},{r.bottom}) iconic={bool(user32.IsIconic(hwnd))} visible={bool(user32.IsWindowVisible(hwnd))}"


def cover(seconds, game):
    """An opaque topmost black window over the whole virtual desktop for `seconds`; returns what was
    observed: whether the window at the game window's centre belongs to the cover, not to the game."""
    import tkinter

    root = tkinter.Tk()
    root.overrideredirect(True)
    vx, vy = user32.GetSystemMetrics(76), user32.GetSystemMetrics(77)
    vw, vh = user32.GetSystemMetrics(78), user32.GetSystemMetrics(79)
    root.geometry(f"{vw}x{vh}+{vx}+{vy}")
    root.configure(bg="black")
    root.attributes("-topmost", True)
    root.update()
    time.sleep(0.3)
    root.update()
    r = wt.RECT()
    user32.GetWindowRect(game, ctypes.byref(r))
    centre = wt.POINT((r.left + r.right) // 2, (r.top + r.bottom) // 2)
    top = user32.GetAncestor(user32.WindowFromPoint(centre), 2)  # GA_ROOT
    on_top = bool(top) and top != game
    t0 = time.monotonic()
    while time.monotonic() - t0 < seconds:
        root.update()
        time.sleep(0.02)
    root.destroy()
    return f"the window at the game window's centre is {'the cover' if on_top else 'the GAME (the cover is NOT on top)'}"


def main(argv):
    pos = [a for a in argv if not a.startswith("--")]
    adapter = argv[argv.index("--adapter") + 1] if "--adapter" in argv else "0"
    if len(pos) < 2 or not os.environ.get("ANNO1503_GAME_DIR"):
        print(__doc__, file=sys.stderr)
        return 2
    exe, out = str(Path(pos[0]).resolve()), Path(pos[1])
    if out.exists():
        print("window_state_case: the out dir exists", file=sys.stderr)
        return 2
    out.mkdir(parents=True)
    script = HERE / "tools" / "test" / "viewer_wiring" / "boundary_window_state.script"
    env = dict(os.environ, ANNO_DEV_SCRIPT=str(script), ANNO_DEV_SESSION=str(out / "session"),
               ANNO_VIEW_TRACE=str(out / "trace"), ANNO_VIEW_SIZE="1280x720", ANNO_VIEW_AT="1052,444",
               ANNO_VIEW_QUIT_S="120", ANNO_REQUIRE_GPU="1", ANNO_GPU_ADAPTER=adapter)
    phases = []
    with open(out / "stdout.txt", "wb") as so, open(out / "stderr.txt", "wb") as se:
        proc = subprocess.Popen([exe, "--play", str(HERE / "data" / "maps" / "lan5.txt"), "--renderer", "gpu"],
                                stdout=so, stderr=se, env=env)
        t0 = time.monotonic()
        done = False
        cover_failed = False
        while proc.poll() is None and time.monotonic() - t0 < 150:
            wins = windows_of(proc.pid)
            if wins and not done and "not drawn yet" in title_of(wins[0]):
                hwnd = wins[0]
                done = True
                trigger_at = time.monotonic()
                phases.append(f"{time.monotonic() - t0:7.2f}s trigger: title has 'not drawn yet'; {state(hwnd)}")
                time.sleep(1.0)
                user32.ShowWindow(hwnd, 6)  # SW_MINIMIZE
                time.sleep(0.3)
                phases.append(f"{time.monotonic() - t0:7.2f}s minimised: {state(hwnd)}")
                time.sleep(2.2)
                user32.ShowWindow(hwnd, 9)  # SW_RESTORE
                time.sleep(0.5)
                phases.append(f"{time.monotonic() - t0:7.2f}s restored: {state(hwnd)}")
                time.sleep(1.0)
                seen = cover(2.5, hwnd)
                phases.append(f"{time.monotonic() - t0:7.2f}s covered for 2.5 s: {seen}; {state(hwnd)}")
                phases.append(f"{time.monotonic() - t0:7.2f}s uncovered: {state(hwnd)}")
                ended_at = time.monotonic()
                if "NOT on top" in seen:
                    cover_failed = True
                phases.append(f"all phases took {ended_at - trigger_at:.1f} s from the trigger; the script holds "
                              f"the builders for 12 s from just before it")
            time.sleep(0.05)
        if proc.poll() is None:
            proc.kill()
            phases.append("watchdog: the window was killed")
        rc = proc.wait()
    phases.append(f"window_exit={rc}" + ("" if done else " (the trigger never happened)")
                  + (" (the cover was NOT on top: not a covered-window result)" if cover_failed else ""))
    (out / "phases.txt").write_text("\n".join(phases) + "\n", encoding="utf-8")
    frames = out / "trace" / "frames.tsv"
    if frames.is_file():
        rows = [ln.split("\t") for ln in frames.read_text(encoding="utf-8").splitlines()]
        head = rows[0]
        col = {k: i for i, k in enumerate(head)}
        runs = []
        for r in rows[1:]:
            key = (r[col["outcome"]], r[col["acquire_status"]], r[col["present_status"]])
            if runs and runs[-1][0] == key:
                runs[-1][1] += 1
            else:
                runs.append([key, 1])
        # What the backend did in time order: every gap between frame attempts of 100 ms or more (a
        # minimised window attempts none) with the outcomes either side, and the first reconfigures.
        starts = [float(r[col["t_start_ms"]]) for r in rows[1:]]
        gaps = []
        for i in range(1, len(starts)):
            if starts[i] - starts[i - 1] >= 100.0:
                a, b = rows[i], rows[i + 1]
                gaps.append(f"gap {starts[i] - starts[i - 1]:8.1f} ms from trace t={starts[i - 1]:9.1f} ms "
                            f"(after frame {a[col['frame']]} {a[col['outcome']]}/{a[col['acquire_status']]}, "
                            f"before frame {b[col['frame']]} {b[col['outcome']]}/{b[col['acquire_status']]}, "
                            f"epoch {a[col['epoch']]} -> {b[col['epoch']]})")
        retries = [f"frame {r[col['frame']]} t={float(r[col['t_start_ms']]):.1f} ms {r[col['outcome']]} "
                   f"{r[col['acquire_status']]} epoch {r[col['epoch']]}" for r in rows[1:]
                   if r[col["outcome"]] != "presented"]
        text = (
            f"frames attempted {len(rows) - 1}; not presented {len(retries)}\n"
            + "gaps of 100 ms or more between frame attempts:\n" + ("\n".join(gaps) or "none") + "\n"
            + "frames that were not presented:\n" + ("\n".join(retries) or "none") + "\n"
        )
        (out / "timeline.txt").write_text(text, encoding="utf-8")
        (out / "outcomes.txt").write_text(
            "\n".join(f"{n:6d} x outcome={k[0]} acquire={k[1]} present={k[2]}" for k, n in runs) + "\n",
            encoding="utf-8")
    print("\n".join(phases))
    return (rc if done and not cover_failed else 1)


if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))
