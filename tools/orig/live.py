"""A live session on one instance of the original, driven through files (T16).

For step-by-step work (calibrating screens, RE by memory diffing) where every next action depends
on the last screenshot. Keeps one backend open and executes command files as they appear:

    python -I tools/orig/live.py --session t16-cal            (Wine runner on :91 by default)

    cache/orig/<session>/live/inbox/<name>.json    written by the caller, one of
        {"cmd": "<backend command>", "args": {...}}       e.g. key, click, screenshot, state, learn
        {"macro": "<tools/orig/macros.py function>", "args": {...}}
        {"sleep": seconds}
        {"batch": [ ...any of the above... ]}            runs in order, stops at the first error
        {"cmd": "quit"}                                   stop the instance and exit
    cache/orig/<session>/live/outbox/<name>.json   the results (written atomically)

No shell quoting anywhere: commands are JSON files. The session stops cleanly on `quit`, on an
error that kills the backend, or after --idle seconds without a command (default 1800).
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


def run_one(b, item: dict):
    if "batch" in item:
        out = []
        for sub in item["batch"]:
            r = run_one(b, sub)
            out.append(r)
            if not r.get("ok", True):
                break
        return {"ok": all(r.get("ok", True) for r in out), "batch": out}
    t0 = time.perf_counter()
    try:
        if "sleep" in item:
            time.sleep(float(item["sleep"]))
            res = {"slept": item["sleep"]}
        elif "macro" in item:
            res = getattr(macros, item["macro"])(b, **(item.get("args") or {}))
        else:
            cmd, args = item["cmd"], item.get("args") or {}
            method = getattr(b, cmd, None)
            res = method(**args) if callable(method) and cmd not in ("call",) else b.call(cmd, **args)
        return {"ok": True, "item": item, "result": res, "s": round(time.perf_counter() - t0, 3)}
    except Exception as ex:  # noqa: BLE001 - every failure goes back to the caller
        return {"ok": False, "item": item, "error": f"{type(ex).__name__}: {ex}",
                "trace": traceback.format_exc(limit=4), "s": round(time.perf_counter() - t0, 3)}


def write_atomic(path: str, obj) -> None:
    tmp = path + ".tmp"
    with open(tmp, "w", encoding="utf-8") as f:
        json.dump(obj, f, indent=1, default=str)
    os.replace(tmp, path)


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
    if a.backend == "wine":
        b = backend.WineBackend(a.display, session=a.session)
    else:
        b = backend.WindowsBackend(session=a.session)
    ready = {"ok": True, "backend": a.backend, "inbox": inbox, "outbox": outbox}
    try:
        if not a.no_start:
            ready["start"] = b.start()
        write_atomic(os.path.join(outbox, "ready.json"), ready)
        print(json.dumps(ready), flush=True)
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
                except (OSError, ValueError):
                    time.sleep(0.3)          # still being written; retry next pass
                    continue
                os.remove(p)
                last = time.monotonic()
                if item.get("cmd") == "quit":
                    write_atomic(os.path.join(outbox, n), {"ok": True, "result": "quitting"})
                    return 0
                res = run_one(b, item)
                write_atomic(os.path.join(outbox, n), res)
                print(json.dumps({"done": n, "ok": res.get("ok")}), flush=True)
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
