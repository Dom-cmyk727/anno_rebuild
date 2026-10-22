"""Timing check of the Wine runner against fixtures F1, F2 and F4 (T16, original-wine.md section 4.5).

Starts a game, then for each speed key holds a window of at least 60 s in which the agent polls the
world's game-ms counter about every 0.5 ms (tools/orig/wine/agent.py poll_*). In single player every
frame advances the counter once by min(trunc(dt * speed + 0.5), 100), dt = whole ms since the last
frame with the sub-millisecond rest carried over (docs/re/tick-loop.md sections 1-2), so every
change of the counter is one frame.

Clocks. Real time is the Windows host's perf_counter. Before and after each window the host and the
guest exchange NTP-style clock pairs (backend.clock_pair: the ping with the smallest round trip),
which gives the guest clocks' rates against the host with an error of (rtt0 + rtt1) / 2 per window.
The window itself is measured on the guest (CLOCK_MONOTONIC_RAW, the source of Wine's
QueryPerformanceCounter, and CLOCK_MONOTONIC) and converted to host seconds.

Per window: game ms per host second, per raw and per monotonic guest second; fps, frame times and
steps; frames at the 100 ms cap and the time they cost; and the rate PREDICTED from the measured
frame times by the documented rounding (this is what F2 says for 50 %: the result depends on the
frame times, so a flat 500 ms/s is not the expectation).

Verdict (written into timing.json):
  clock_f1  pass if at 100 % no frame is capped and |game ms per host s - 1000| <= 1000/60 (F1).
  speeds    per speed: |actual - predicted| <= predicted/60, and for 100/200/400 % also
            |actual (+ cap cost) - speed*1000| <= speed*1000/60.
  pause_f4  the counter is unchanged over the pause window, and it runs again afterwards.

    python -I tools/orig/timing.py --session t16-timing [--seconds 65]
"""

from __future__ import annotations

import argparse
import json
import math
import os
import sys
import time

_HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, _HERE)
import backend  # noqa: E402
import macros  # noqa: E402

# Speed keys (game-mechanics.md section 1; tick-loop.md "Game speed"). Windows numbers: T15 (PR #16)
# as tabled in original-wine.md section 3 (F5 = F1's 500 ticks/s).
SPEEDS = {"F5": 1.0, "F6": 2.0, "F7": 4.0, "F8": 0.5}
WINDOWS_MS_PER_S = {"F5": 1000.0, "F6": 1984.0, "F7": 3986.0, "F8": 523.0}
CAP_MS = 100                      # per-frame step cap (tick-loop.md section 2)
F1_REL = 1.0 / 60.0               # F1: +-1 s per 60 s


def load_series(path: str) -> list[tuple[float, int]]:
    with open(path, encoding="utf-8") as f:
        return [(r["t"], r["counter"]) for r in map(json.loads, f)]


def analyse(series: list[tuple[float, int]], speed: float) -> dict:
    """Prediction from the frame times, cap cost, and the dt histogram."""
    t0 = series[0][0]
    pred, lost, capped, hist = 0, 0.0, 0, {}
    prev_ms = 0
    for (ta, ca), (tb, cb) in zip(series, series[1:]):
        now_ms = math.floor((tb - t0) * 1000.0)
        dt = now_ms - prev_ms                    # whole ms with the rest carried (frame timer)
        prev_ms = now_ms
        hist[dt] = hist.get(dt, 0) + 1
        step = math.trunc(dt * speed + 0.5)
        pred += min(step, CAP_MS) if step >= 1 else 0
        if cb - ca >= CAP_MS:
            capped += 1
            lost += max(0.0, (tb - ta) * 1000.0 * speed - CAP_MS)
    top = sorted(hist.items(), key=lambda kv: -kv[1])[:6]
    return {"predicted_ms": pred, "capped_frames": capped, "lost_ms": round(lost, 1),
            "dt_hist_top": {str(k): v for k, v in top}}


def window(b, label: str, seconds: float, speed: float | None) -> dict:
    p0 = b.clock_pair()
    b.poll_start(label)
    time.sleep(seconds)
    res = b.poll_stop()
    p1 = b.clock_pair()
    d_host, d_raw, d_mono = p1["host"] - p0["host"], p1["raw"] - p0["raw"], p1["mono"] - p0["mono"]
    res["clock_pairs"] = {"rtt_ms": [round(p0["rtt"] * 1e3, 2), round(p1["rtt"] * 1e3, 2)],
                          "error_ppm": round((p0["rtt"] + p1["rtt"]) / 2 / d_host * 1e6, 1),
                          "raw_vs_host_ppm": round((d_raw / d_host - 1) * 1e6, 1),
                          "mono_vs_host_ppm": round((d_mono / d_host - 1) * 1e6, 1)}
    host_s = res["raw_s"] * d_host / d_raw
    res["host_s"] = round(host_s, 4)
    res["game_ms_per_host_s"] = round(res["game_ms"] / host_s, 3)
    res["game_ms_per_raw_s"] = round(res["game_ms"] / res["raw_s"], 3)
    res["fps"] = round(res["frames"] / res["mono_s"], 1)
    b.sync()
    if speed is not None:
        res.update(analyse(load_series(os.path.join(b.out_dir, "timing", f"{label}.jsonl")), speed))
    return res


def main(argv=None) -> int:
    ap = argparse.ArgumentParser(description="timing check of the Wine runner (F1, F2, F4)")
    ap.add_argument("--session", default=time.strftime("timing-%Y%m%d-%H%M"))
    ap.add_argument("--seconds", type=float, default=65.0, help="window per speed (>= 60)")
    ap.add_argument("--speeds", default="F5,F6,F7,F8")
    ap.add_argument("--no-pause", action="store_true", help="skip the F4 pause window")
    ap.add_argument("--display", default=":91")
    a = ap.parse_args(argv)
    if a.seconds < 60:
        ap.error("windows must be at least 60 s (T16 section 4.5)")

    b = backend.WineBackend(a.display, session=a.session)
    out = {"session": a.session, "seconds": a.seconds, "windows": {}}
    try:
        out["start"] = b.start()
        out["new_game"] = macros.new_game(b, "citizen")
        time.sleep(5)
        for key in a.speeds.split(","):
            speed = SPEEDS[key]
            b.key(key)
            time.sleep(3)                      # settle after the speed change
            w = window(b, f"speed_{key}", a.seconds, speed)
            expect = speed * 1000.0
            actual = w["game_ms_per_host_s"]
            predicted = w["predicted_ms"] / w["host_s"]
            w["expected_flat_ms_per_s"] = expect
            w["predicted_ms_per_host_s"] = round(predicted, 3)
            w["windows_ms_per_s"] = WINDOWS_MS_PER_S.get(key)
            ok_model = abs(actual - predicted) <= predicted * F1_REL
            ok_flat = speed < 1.0 or abs(actual + w["lost_ms"] / w["host_s"] - expect) <= expect * F1_REL
            w["pass"] = ok_model and ok_flat
            out["windows"][key] = w
            print(json.dumps({k: w.get(k) for k in (
                "label", "game_ms_per_host_s", "game_ms_per_raw_s", "predicted_ms_per_host_s",
                "expected_flat_ms_per_s", "windows_ms_per_s", "fps", "capped_frames", "lost_ms",
                "frame_ms", "dt_hist_top", "clock_pairs", "pass")}), flush=True)
        b.key("F5")
        if not a.no_pause:
            b.key("P")
            time.sleep(3)
            w = window(b, "pause_F4", a.seconds, None)
            w["counter_unchanged"] = w["game_ms"] == 0
            b.key("P")
            time.sleep(3)
            after = b.state("world")["world"]
            w["resumed"] = bool(after) and after["counter"] > w["end"]["counter"]
            out["windows"]["pause"] = w
            print(json.dumps({"pause_F4": {"host_s": w["host_s"], "game_ms": w["game_ms"],
                                           "counter_unchanged": w["counter_unchanged"],
                                           "resumed": w["resumed"]}}), flush=True)
        f5 = out["windows"].get("F5")
        if f5:
            out["verdict"] = {
                "clock_f1": "pass" if f5["capped_frames"] == 0 and
                abs(f5["game_ms_per_host_s"] - 1000.0) <= 1000.0 * F1_REL else "fail",
                "speeds": {k: w["pass"] for k, w in out["windows"].items() if k in SPEEDS},
                "pause_f4": (out["windows"].get("pause", {}).get("counter_unchanged") and
                             out["windows"].get("pause", {}).get("resumed")),
            }
            print(json.dumps({"verdict": out["verdict"]}), flush=True)
        out["audio"] = b.call("audio_check")
    finally:
        out["stop"] = b.stop()
        with open(os.path.join(b.out_dir, "timing.json"), "w", encoding="utf-8") as f:
            json.dump(out, f, indent=1, default=str)
    return 0


if __name__ == "__main__":
    sys.exit(main())
