"""Unattended run in the Wine runner (T16 acceptance, original-wine.md section 4.6).

  1. a disposable, silent instance; the new-game macro (Citizen);
  2. the state checks at the start, against the Windows numbers (T15, PR #16): the PRNG table
     (0/2048 differ; the read-only form of F6) and the player's warehouse (50 t, tools 50, wood 50);
  3. the JSONL sampler (one record per game minute), with quicksaves switched OFF while the
     opening runs, so F10 never lands in the middle of a placement;
  4. the scripted opening from the playbook (tools/orig/opening.py), each step verified in memory;
     then quicksaves on (one kept every --save-every records);
  5. the run continues until --minutes of wall time have passed, with a check every --check
     seconds: game alive, no crash (Wine's crash handler), audio guard clean, outputs synced;
     a crash ends the run at the next check: the agent's watchdog marks it within 0.5 s and the
     sampler stops within another 0.5 s, but this script notices it up to --check seconds later;
  6. afterwards, from the sampler JSONL: one record per game minute, and F1's invariant
     tick == counter >> 1 in every record; a summary (summary.json) and full cleanup.
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


def sampler_report(path: str) -> dict:
    """Records per game minute and F1's invariant over the sampler JSONL."""
    recs, events = [], []
    with open(path, encoding="utf-8") as f:
        for line in f:
            r = json.loads(line)
            (events if "event" in r else recs).append(r)
    bad = [r["game_min"] for r in recs if r["world"]["tick"] != r["world"]["counter"] >> 1]
    mins = [r["game_min"] for r in recs]
    gaps = [round(b - a, 3) for a, b in zip(mins, mins[1:]) if not 0.99 <= b - a <= 1.01]
    return {"records": len(recs), "first_min": mins[0] if mins else None, "last_min": mins[-1] if mins else None,
            "minute_gaps": gaps, "f1_invariant_violations": bad,
            "events": [e["event"] for e in events], "saves": sum(1 for r in recs if "save" in r),
            "saves_ok": sum(1 for r in recs if r.get("save", {}).get("saved"))}


def main(argv=None) -> int:
    ap = argparse.ArgumentParser(description="unattended Wine-runner run with sampler and opening")
    ap.add_argument("--session", default=time.strftime("unattended-%Y%m%d-%H%M"))
    ap.add_argument("--minutes", type=float, default=30.0, help="wall-clock length of the run")
    ap.add_argument("--save-every", type=int, default=5, help="quicksave every N sampler records (0 = never)")
    ap.add_argument("--check", type=float, default=60.0, help="seconds between health checks")
    ap.add_argument("--no-opening", action="store_true")
    ap.add_argument("--display", default=":91")
    ap.add_argument("--winedebug", default=None, help="WINEDEBUG for the game (diagnostics)")
    a = ap.parse_args(argv)

    t_start = time.perf_counter()
    b = backend.WineBackend(a.display, session=a.session, winedebug=a.winedebug)
    summary: dict = {"session": a.session, "out_dir": b.out_dir, "minutes": a.minutes, "checks": [],
                     "ok": False, "criteria": {}}

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
            "prng_table": st["prng"],
            "prng_equal_windows": st["prng"].get("mismatches") == 0,
            "warehouse": city,
            "warehouse_equal_windows": bool(city) and all(city[k] == v for k, v in WINDOWS_START_CITY.items()),
            "other_cities_capacity_t": sorted(c["capacity_t"] for c in st["cities"] if c is not city),
            "players": st.get("players"),
        }
        note("fixtures", **{k: v for k, v in summary["fixtures"].items() if k != "players"})

        summary["sampler"] = b.sample_start(period_game_ms=60000, save_every=a.save_every, saves_on=False)
        if not a.no_opening:
            import opening  # noqa: PLC0415
            summary["opening"] = opening.run(b, log=note)
            note("opening_done", ok=summary["opening"]["ok"], placed=summary["opening"]["placed"],
                 failed=summary["opening"]["failed"])
        b.sample_saves(True)

        end = t_start + a.minutes * 60.0
        while time.perf_counter() < end:
            time.sleep(min(a.check, max(0.0, end - time.perf_counter())))
            s = b.status()
            au = b.call("audio_check")
            w = b.state("world").get("world") if s["alive"] and not s["crashed"] else None
            b.sync()
            summary["checks"].append(note("check", alive=s["alive"], crashed=s["crashed"], sampler=s["sampler"],
                                          audio_violations=len(au["violations"]), audio_killed=s["audio_killed"],
                                          game_min=round(w["counter"] / 60000, 2) if w else None))
            if s["crashed"]:
                raise RuntimeError(f"the game crashed (Wine's crash handler): {s['crashed']}")
            if s["audio_killed"]:
                raise RuntimeError(f"the audio guard killed the game: {s['audio_killed']}")
            if not s["alive"]:
                raise RuntimeError("the game exited during the run")

        summary["sampler_stop"] = b.sample_stop()
        summary["final"] = b.state("all")
        summary["audio"] = b.call("audio_check")
    except Exception as ex:  # noqa: BLE001 - record, then clean up
        summary["error"] = f"{type(ex).__name__}: {ex}"
        note("error", error=summary["error"])
    finally:
        summary["wall_minutes"] = round((time.perf_counter() - t_start) / 60.0, 2)
        try:
            summary["stop"] = b.stop()
        except Exception as ex:  # noqa: BLE001
            summary["stop_error"] = repr(ex)
        sp = os.path.join(b.out_dir, "sampler.jsonl")
        if os.path.exists(sp):
            summary["sampler_report"] = sampler_report(sp)
        rep = summary.get("sampler_report") or {}
        fx = summary.get("fixtures") or {}
        game_minutes = (rep.get("last_min") or 0) - (rep.get("first_min") or 0)
        c = summary["criteria"] = {
            "no_error": "error" not in summary,
            "prng_table_equal_windows": bool(fx.get("prng_equal_windows")),
            "warehouse_equal_windows": bool(fx.get("warehouse_equal_windows")),
            "opening_ok": a.no_opening or bool(summary.get("opening", {}).get("ok")),
            "checks_ran": len(summary["checks"]) > 0 and all(x["alive"] for x in summary["checks"]),
            "never_crashed": not any(x.get("crashed") for x in summary["checks"]) and not
            (summary.get("stop") or {}).get("crashed"),
            "audio_clean": not (summary.get("audio") or {}).get("violations") and not
            any(x.get("audio_killed") for x in summary["checks"]),
            "sampler_no_error": (summary.get("sampler_stop") or {}).get("error") is None and "sampler_stop" in summary,
            "sampler_one_record_per_minute": rep.get("records", 0) >= 1 and not rep.get("minute_gaps")
            and rep.get("records", 0) >= int(game_minutes),
            "f1_tick_invariant_all_records": rep.get("records", 0) >= 1 and not rep.get("f1_invariant_violations"),
            "saves_kept": a.save_every == 0 or rep.get("saves_ok", 0) == rep.get("saves", 0) > 0,
        }
        summary["ok"] = all(c.values())
        with open(os.path.join(b.out_dir, "summary.json"), "w", encoding="utf-8") as f:
            json.dump(summary, f, indent=1, default=str)
        note("summary", ok=summary["ok"], criteria=c, wall_minutes=summary["wall_minutes"], error=summary.get("error"))
    return 0 if summary["ok"] else 1


if __name__ == "__main__":
    sys.exit(main())
