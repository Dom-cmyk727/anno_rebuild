"""A live session on one instance of the original, driven through files (T16).

For step-by-step work (calibrating screens, RE by memory diffing) where every next action depends
on the last screenshot. Keeps one backend open and executes command files as they appear:

    python -I tools/orig/live.py --session t16-cal            (Wine runner on :91 by default)

    cache/orig/<session>/live/inbox/<name>.json    written by the caller (write <name>.tmp, then
                                                   rename to .json), one of
        {"cmd": "<command>", "args": {...}, "timeout": s}   a backend method or agent command
        {"macro": "<tools/orig/macros.py function>", "args": {...}}
        {"sleep": seconds}
        {"batch": [ ...any of the above... ]}            runs in order, stops at the first error
        {"cmd": "quit"} (or "stop")                       stop the instance and exit
    cache/orig/<session>/live/outbox/<name>.json   the results (written atomically)

No shell quoting anywhere: commands are JSON files. The session ends on `quit`, when the backend
dies or the game crashes, or after --idle seconds without a command (default 1800).
"""

from __future__ import annotations

import argparse
import json
import os
import sys
import time
import traceback

_HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, _HERE)
import backend  # noqa: E402
import macros  # noqa: E402

# Backend methods a command file may call; anything else goes to the agent as a command (the agent
# has its own whitelist). Lifecycle methods are not exposed: `quit` ends the session.
METHODS = {"key", "click", "move", "drag", "wheel", "screenshot", "recognize", "learn", "quicksave",
           "quickload", "read", "module_bases", "state", "poll_start", "poll_stop", "sample_start",
           "sample_saves", "sample_stop", "status", "sync", "clock_pair"}
BAD_JSON_RETRIES = 10          # a file still unreadable after this many passes gets an error answer


class SessionOver(Exception):
    pass


def run_one(b, item) -> dict:
    t0 = time.perf_counter()
    try:
        if not isinstance(item, dict):
            raise ValueError("a command must be a JSON object")
        if "batch" in item:
            out = []
            for sub in item["batch"]:
                r = run_one(b, sub)
                out.append(r)
                if not r.get("ok", True):
                    break
            return {"ok": all(r.get("ok", True) for r in out), "batch": out}
        if "sleep" in item:
            time.sleep(float(item["sleep"]))
            res = {"slept": item["sleep"]}
        elif "macro" in item:
            fn = getattr(macros, item["macro"], None)
            if not callable(fn) or item["macro"].startswith("_"):
                raise ValueError(f"unknown macro {item['macro']!r}")
            res = fn(b, **(item.get("args") or {}))
        else:
            cmd, args = item.get("cmd"), item.get("args") or {}
            if not isinstance(cmd, str) or not isinstance(args, dict):
                raise ValueError("cmd must be a string and args an object")
            if cmd in ("quit", "stop"):
                raise SessionOver(cmd)
            if cmd in METHODS:
                res = getattr(b, cmd)(**args)
            else:
                res = b.call(cmd, args, timeout=item.get("timeout"))
        return {"ok": True, "item": item, "result": res, "s": round(time.perf_counter() - t0, 3)}
    except SessionOver:
        raise
    except Exception as ex:  # noqa: BLE001 - every failure goes back to the caller
        return {"ok": False, "item": item, "error": f"{type(ex).__name__}: {ex}",
                "trace": traceback.format_exc(limit=4), "s": round(time.perf_counter() - t0, 3)}


def write_atomic(path: str, obj) -> None:
    tmp = path + ".tmp"
    with open(tmp, "w", encoding="utf-8") as f:
        json.dump(obj, f, indent=1, default=str)
    os.replace(tmp, path)


def backend_dead(b) -> str | None:
    """Why the session cannot go on, or None. Only the Wine runner reports crashes."""
    proc = getattr(b, "proc", None)
    if proc is not None and proc.poll() is not None:
        return "the agent exited"
    if isinstance(b, backend.WineBackend):
        try:
            st = b.status()
        except backend.BackendError as ex:
            return f"status failed: {ex}"
        if st.get("crashed"):
            return f"the game crashed: {st['crashed']}"
    return None


def main(argv=None) -> int:
    ap = argparse.ArgumentParser(description="file-driven live session on one instance of the original")
    ap.add_argument("--session", required=True)
    ap.add_argument("--backend", choices=["wine", "windows"], default="wine")
    ap.add_argument("--display", default=":91")
    ap.add_argument("--idle", type=float, default=1800.0, help="stop after this many seconds without a command")
    ap.add_argument("--no-start", action="store_true", help="do not launch the game on start")
    a = ap.parse_args(argv)

    base = os.path.join(backend.REPO, "cache", "orig", a.session, "live")
    inbox, outbox = os.path.join(base, "inbox"), os.path.join(base, "outbox")
    os.makedirs(inbox, exist_ok=True)
    os.makedirs(outbox, exist_ok=True)
    b = backend.WineBackend(a.display, session=a.session) if a.backend == "wine" \
        else backend.WindowsBackend(session=a.session)
    ready = {"ok": True, "backend": a.backend, "inbox": inbox, "outbox": outbox,
             "out_dir": getattr(b, "out_dir", None)}
    bad: dict[str, int] = {}
    try:
        if not a.no_start:
            ready["start"] = b.start()
        write_atomic(os.path.join(outbox, "ready.json"), ready)
        print(json.dumps(ready, default=str), flush=True)
        last = time.monotonic()
        while time.monotonic() - last < a.idle:
            names = sorted(n for n in os.listdir(inbox) if n.endswith(".json"))
            if not names:
                time.sleep(0.2)
                continue
            for n in names:
                p = os.path.join(inbox, n)
                try:
                    with open(p, encoding="utf-8") as f:
                        item = json.load(f)
                except (OSError, ValueError) as ex:
                    bad[n] = bad.get(n, 0) + 1
                    if bad[n] < BAD_JSON_RETRIES:
                        time.sleep(0.3)        # probably still being written; retry next pass
                        continue
                    item, err = None, f"unreadable command file: {ex}"
                else:
                    err = None
                try:
                    os.remove(p)
                except OSError:
                    continue                   # still held by the writer; retry next pass
                bad.pop(n, None)
                last = time.monotonic()
                if err:
                    write_atomic(os.path.join(outbox, n), {"ok": False, "error": err})
                    continue
                try:
                    res = run_one(b, item)
                except SessionOver:
                    write_atomic(os.path.join(outbox, n), {"ok": True, "result": "quitting"})
                    return 0
                write_atomic(os.path.join(outbox, n), res)
                print(json.dumps({"done": n, "ok": res.get("ok")}), flush=True)
                why = backend_dead(b) if not res.get("ok") or a.backend == "wine" else None
                if why:
                    write_atomic(os.path.join(outbox, "session_over.json"), {"ok": False, "error": why})
                    print(json.dumps({"session_over": why}), flush=True)
                    return 1
        print(json.dumps({"idle_timeout": a.idle}), flush=True)
        return 0
    finally:
        try:
            res = b.stop()
            print(json.dumps({"stopped": res}, default=str), flush=True)
        except Exception as ex:  # noqa: BLE001
            print(json.dumps({"stop_error": repr(ex)}), flush=True)


if __name__ == "__main__":
    sys.exit(main())
