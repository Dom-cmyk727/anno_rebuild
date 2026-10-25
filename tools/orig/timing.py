"""Timing check of the Wine runner against fixtures F1, F2 and F4 (T16, original-wine.md section 4.5).

Starts a game, then for each speed key holds a window of at least 60 s in which the agent polls the
world's game-ms counter about every 0.5 ms, timestamped on CLOCK_MONOTONIC_RAW (tools/orig/wine/
agent.py poll_*). In single player every frame advances the counter once by
min(trunc(dt * speed + 0.5), 100) game ms, dt = whole ms since the last frame with the
sub-millisecond rest carried over (docs/re/tick-loop.md sections 1-2), so every change of the
counter is one frame (two frames between polls merge; the frame count is a lower bound).

Clocks. Real time is the Windows host's perf_counter. Before and after each window the host and the
guest exchange NTP-style clock pairs (backend.clock_pair: the ping with the smallest round trip),
which gives the guest's RAW clock rate against the host with an error of (rtt0 + rtt1) / 2 per
window (about 4 ppm at 0.3 ms over 65 s).

Rate. The primary number is the rate between the first and the last observed frame of the window
(changes in the agent's result; error: the poll latency at both ends, about 2 x 0.5 ms over the
window, ~15 ppm). The whole-window number is kept with its edge error (one frame at each end).

Checks (verdict(), also in timing.json):
  clock_f1  at 100 %: no frame at the 100 ms cap and |rate - 1000| <= 1000/60 (F1: +-1 s per 60 s);
  speeds    200 / 400 %: |rate (+ cap cost) - speed*1000| <= speed*1000/60;
            50 %: model-free. step = trunc(dt/2 + 0.5) = ceil(dt/2), so 2*sum(steps) = sum(dt) +
            n_odd, the number of frames with odd dt; n_odd must lie in [0, frames] (with a 2 ms
            allowance for the poll latency at both ends). A rate above 500 is itself proof of the
            +0.5 rounding (truncation alone gives at most 500);
  pause_f4  no failed read, the counter unchanged over the pause window, running again afterwards.
The 100 % window's step histogram is the exact frame-time (dt) distribution: at 100 % every step
equals its frame's dt (the 50 % replay of analyse() uses poll times and is only an estimate).

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

# Speed keys (playbook section 1; tick-loop.md "Game speed"). Windows numbers: T15 (PR #16) as
# tabled in original-wine.md section 3 (F5 = F1's 500 ticks/s).
SPEEDS = {"F5": 1.0, "F6": 2.0, "F7": 4.0, "F8": 0.5}
WINDOWS_MS_PER_S = {"F5": 1000.0, "F6": 1984.0, "F7": 3986.0, "F8": 523.0}
CAP_MS = 100                      # per-frame step cap (tick-loop.md section 2)
F1_REL = 1.0 / 60.0               # F1: +-1 s per 60 s
POLL_ALLOWANCE_MS = 2             # n_odd tolerance: the poll latency at the first and last change


def load_series(path: str) -> list[tuple[float, int]]:
    with open(path, encoding="utf-8") as f:
        return [(r.get("t_raw", r.get("t")), r["counter"]) for r in map(json.loads, f)]


def analyse(series: list[tuple[float, int]], speed: float) -> dict:
    """Estimate from the poll times: the documented rounding replayed over the observed frame
    intervals, the frames at the cap and the time they cost. Poll latency smears dt between
    neighbours, so `predicted_ms` is an estimate; the exact checks are in verdict()."""
    t0 = series[0][0]
    pred, lost, capped, prev_ms = 0, 0.0, 0, 0
    for (ta, ca), (tb, cb) in zip(series, series[1:]):
        now_ms = math.floor((tb - t0) * 1000.0)
        dt = now_ms - prev_ms                    # whole ms with the rest carried (frame timer)
        prev_ms = now_ms
        step = math.trunc(dt * speed + 0.5)
        pred += min(step, CAP_MS) if step >= 1 else 0
        if cb - ca >= CAP_MS:
            capped += 1
            lost += max(0.0, (tb - ta) * 1000.0 * speed - CAP_MS)
    return {"predicted_ms": pred, "capped_frames": capped, "lost_ms": round(lost, 1)}


def half_speed_odd_frames(changes: dict) -> dict:
    """50 %: n_odd = 2 * sum(steps) - sum(dt) over the span between the first and last change;
    sum(dt) is the span in whole ms of the game's timer (RAW, which the game's timer follows)."""
    steps = changes["game_ms"]
    dt_sum = round(changes["raw_s"] * 1000.0)
    n_odd = 2 * steps - dt_sum
    frames = changes["frames"]
    return {"sum_steps": steps, "sum_dt_ms": dt_sum, "n_odd": n_odd, "frames": frames,
            "consistent": -POLL_ALLOWANCE_MS <= n_odd <= frames + POLL_ALLOWANCE_MS}


def verdict(windows: dict) -> dict:
    """The F1 / F2 / F4 checks over the measured windows (keys F5..F8, pause)."""
    out: dict = {"speeds": {}}
    for key, speed in SPEEDS.items():
        w = windows.get(key)
        if not w:
            continue
        rate = w["rate_ms_per_host_s"]
        expect = speed * 1000.0
        if speed < 1.0:
            odd = w["half_speed"]
            ok = odd["consistent"] and rate > expect
        else:
            corrected = rate + w["lost_ms"] / w["host_s"]
            ok = abs(corrected - expect) <= expect * F1_REL
            if speed == 1.0:
                out["clock_f1"] = "pass" if ok and w["capped_frames"] == 0 else "fail"
        out["speeds"][key] = ok
    p = windows.get("pause")
    if p:
        out["pause_f4"] = (p["read_failures"] == 0 and p["game_ms"] == 0 and bool(p.get("resumed")))
    return out


def window(b, label: str, seconds: float, speed: float | None) -> dict:
    p0 = b.clock_pair()
    b.poll_start(label)
    time.sleep(seconds)
    res = b.poll_stop()
    p1 = b.clock_pair()
    d_host, d_raw, d_mono = p1["host"] - p0["host"], p1["raw"] - p0["raw"], p1["mono"] - p0["mono"]
    host_per_raw = d_host / d_raw
    res["clock_pairs"] = {"rtt_ms": [round(p0["rtt"] * 1e3, 2), round(p1["rtt"] * 1e3, 2)],
                          "error_ppm": round((p0["rtt"] + p1["rtt"]) / 2 / d_host * 1e6, 1),
                          "raw_vs_host_ppm": round((d_raw / d_host - 1) * 1e6, 1),
                          "mono_vs_host_ppm": round((d_mono / d_host - 1) * 1e6, 1)}
    res["host_s"] = round(res["raw_s"] * host_per_raw, 4)
    ch = res.get("changes")
    if ch and ch.get("game_ms_per_raw_s"):
        res["rate_ms_per_host_s"] = round(ch["game_ms_per_raw_s"] / host_per_raw, 4)
        res["rate_error_ppm"] = round(2 * 0.5e-3 / ch["raw_s"] * 1e6 + res["clock_pairs"]["error_ppm"], 1)
    else:
        res["rate_ms_per_host_s"] = 0.0
    if res["game_ms"] is not None and res["raw_s"]:
        res["window_ms_per_host_s"] = round(res["game_ms"] / res["host_s"], 3)
        res["window_error_ppm"] = round((res["edge_error_ms"] or 0) / (res["host_s"] * 1000) * 1e6, 1)
    res["fps"] = round(res["frames"] / res["raw_s"], 1) if res["raw_s"] else None
    b.sync()
    if speed is not None:
        series = load_series(os.path.join(b.out_dir, "timing", f"{label}.jsonl"))
        res.update(analyse(series, speed))
        if speed < 1.0 and ch:
            res["half_speed"] = half_speed_odd_frames(ch)
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
    out = {"session": a.session, "out_dir": b.out_dir, "seconds": a.seconds, "windows": {}}
    try:
        out["start"] = b.start()
        out["new_game"] = macros.new_game(b, "citizen")
        time.sleep(5)
        for key in a.speeds.split(","):
            b.key(key)
            time.sleep(3)                      # settle after the speed change
            w = window(b, f"speed_{key}", a.seconds, SPEEDS[key])
            w["expected_flat_ms_per_s"] = SPEEDS[key] * 1000.0
            w["windows_ms_per_s"] = WINDOWS_MS_PER_S.get(key)
            out["windows"][key] = w
            print(json.dumps({k: w.get(k) for k in (
                "label", "rate_ms_per_host_s", "rate_error_ppm", "window_ms_per_host_s", "window_error_ppm",
                "predicted_ms", "half_speed", "windows_ms_per_s", "fps", "capped_frames", "frame_ms",
                "read_failures", "clock_pairs")}), flush=True)
        b.key("F5")
        if not a.no_pause:
            b.key("P")
            time.sleep(3)
            w = window(b, "pause_F4", a.seconds, None)
            b.key("P")
            time.sleep(3)
            after = b.state("world")["world"]
            w["resumed"] = bool(after) and w["end"]["counter"] is not None and after["counter"] > w["end"]["counter"]
            out["windows"]["pause"] = w
            print(json.dumps({"pause_F4": {k: w.get(k) for k in ("host_s", "game_ms", "read_failures", "resumed")}}),
                  flush=True)
        out["verdict"] = verdict(out["windows"])
        f5 = out["windows"].get("F5")
        if f5:
            out["dt_histogram_100pct"] = f5["step_hist"]
        print(json.dumps({"verdict": out["verdict"], "dt_histogram_100pct": out.get("dt_histogram_100pct")}), flush=True)
        out["status"] = b.status()
        out["audio"] = b.call("audio_check")
    finally:
        out["stop"] = b.stop()
        with open(os.path.join(b.out_dir, "timing.json"), "w", encoding="utf-8") as f:
            json.dump(out, f, indent=1, default=str)
    return 0


if __name__ == "__main__":
    sys.exit(main())
