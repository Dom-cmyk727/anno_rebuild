"""Unattended run in the Wine runner (T16 acceptance, original-wine.md section 4.6).

  1. a disposable, silent instance; the new-game macro (Citizen);
  2. the state fixtures at the start: F6 (PRNG table) and the player's warehouse, compared with
     the Windows numbers (T15, PR #16: table 0/2048 differ; starting city 50 t, tools 50, wood 50);
  3. the JSONL sampler (one record per game minute, a quicksave kept every --save-every records);
  4. the scripted opening from the playbook (tools/orig/opening.py), each step verified in memory;
  5. the run continues until --minutes of wall time have passed, with a check every --check
     seconds: game alive, audio guard clean, outputs synced to cache/orig/<session>/;
  6. a summary (summary.json) and full cleanup.
Input reaches only the runner's own X display; nothing here calls a Win32 input API.

    python -I tools/orig/unattended.py --session t16-accept --minutes 30
"""

from __future__ import annotations

import argparse
import json
import os
import sys
import time

_HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, _HERE)
import backend  # noqa: E402
import macros  # noqa: E402

# Windows reference (T15 stage 1, PR #16; original-wine.md section 3): the player's starting city.
WINDOWS_START_CITY = {"capacity_t": 50.0, "tools_t": 50.0, "wood_t": 50.0}


def player_start_city(cities: list[dict]) -> dict | None:
    """The player's city at game start: the one without inhabitants and with the start goods
    (the AI cities of the Citizen game start populated)."""
    own = [c for c in cities if c["inhabitants"] == 0 and c["tools_t"] and c["wood_t"]]
    return own[0] if len(own) == 1 else None


def main(argv=None) -> int:
    ap = argparse.ArgumentParser(description="unattended Wine-runner run with sampler and opening")
    ap.add_argument("--session", default=time.strftime("unattended-%Y%m%d-%H%M"))
    ap.add_argument("--minutes", type=float, default=30.0, help="wall-clock length of the run")
    ap.add_argument("--save-every", type=int, default=5, help="quicksave every N sampler records (0 = never)")
    ap.add_argument("--check", type=float, default=60.0, help="seconds between health checks")
    ap.add_argument("--no-opening", action="store_true")
    ap.add_argument("--display", default=":91")
    a = ap.parse_args(argv)

    t_start = time.perf_counter()
    b = backend.WineBackend(a.display, session=a.session)
    summary: dict = {"session": a.session, "minutes": a.minutes, "checks": [], "ok": False}

    def note(kind, **kw):
        rec = {"t": round(time.perf_counter() - t_start, 1), "kind": kind}
        rec.update(kw)
        print(json.dumps(rec, default=str)[:1500], flush=True)
        return rec

    try:
        summary["start"] = b.start()
        summary["new_game"] = macros.new_game(b, "citizen")
        note("in_game", world=summary["new_game"]["in_game"]["world"])

        st = b.state("all")
        city = player_start_city(st["cities"])
        summary["fixtures"] = {
            "f6_prng": st["prng"],
            "f6_equal_windows": st["prng"].get("mismatches") == 0,
            "warehouse": city,
            "warehouse_equal_windows": bool(city) and all(city[k] == v for k, v in WINDOWS_START_CITY.items()),
            "other_cities_capacity_t": sorted(c["capacity_t"] for c in st["cities"] if c is not city),
        }
        note("fixtures", **summary["fixtures"])

        summary["sampler"] = b.sample_start(period_game_ms=60000, save_every=a.save_every)
        if not a.no_opening:
            import opening  # noqa: PLC0415
            summary["opening"] = opening.run(b, log=note)
            note("opening_done", ok=summary["opening"].get("ok"))

        end = t_start + a.minutes * 60.0
        while time.perf_counter() < end:
            time.sleep(min(a.check, max(0.0, end - time.perf_counter())))
            s = b.status()
            au = b.call("audio_check")
            w = b.state("world").get("world") if s["alive"] else None
            b.sync()
            summary["checks"].append(note("check", alive=s["alive"], sampler=s["sampler"],
                                          audio_violations=len(au["violations"]),
                                          game_min=round(w["counter"] / 60000, 2) if w else None))
            if not s["alive"]:
                raise RuntimeError("the game exited during the run")

        summary["sampler_stop"] = b.sample_stop()
        summary["final"] = b.state("all")
        summary["audio"] = b.call("audio_check")
        summary["ok"] = (summary["fixtures"]["f6_equal_windows"] and summary["fixtures"]["warehouse_equal_windows"]
                         and not summary["audio"]["violations"] and all(c["alive"] for c in summary["checks"]))
    except Exception as ex:  # noqa: BLE001 - record, then clean up
        summary["error"] = f"{type(ex).__name__}: {ex}"
        note("error", error=summary["error"])
    finally:
        summary["wall_minutes"] = round((time.perf_counter() - t_start) / 60.0, 2)
        try:
            summary["stop"] = b.stop()
        except Exception as ex:  # noqa: BLE001
            summary["stop_error"] = repr(ex)
        with open(os.path.join(b.out_dir, "summary.json"), "w", encoding="utf-8") as f:
            json.dump(summary, f, indent=1, default=str)
        note("summary", ok=summary["ok"], wall_minutes=summary["wall_minutes"], error=summary.get("error"))
    return 0 if summary["ok"] else 1


if __name__ == "__main__":
    sys.exit(main())
