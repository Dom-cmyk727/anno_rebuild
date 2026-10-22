"""T15 stage 1 fixtures against the running original (docs/testing/m2-fixtures.md).

Read-only. Reuses tools/orig/smoke.py's machinery (launch, window find, screenshot, the WOW64
PEB walk, the world-object locator, ReadProcessMemory). Adds the two no-input fixtures of the first
owner session (A4):

  TL-F1  Game clock at 100%: sample the world ms counter (+0x100), +0x108 and the tick (+0x10c)
         about once a second for --seconds s. Expect tick == counter >> 1 in every sample, +0x108
         == +0x10c between advances, and the tick to grow ~500 * speed per real second.
  TL-F6  PRNG (read-only variant, no memory write): read the 2048-entry u16 table at
         AnnoWelt base + 0xcb8f8 and compare it with the first 2048 MSVC rand() outputs from
         seed 1 (tick-loop.md:70-71; this is exactly src/sim's rng table, rng.hpp). Sample the
         cursor +0xfc over the run to show it advancing as the sim draws.

The game is started by this script; the owner clicks into a single-player scenario at 100% and
leaves it unpaused. Outputs (log.json, a screenshot) go to cache/orig/<session>/ (git-ignored).
Closes the game and re-hashes the copy. Sends no input.

Run (with the owner at the PC):
  python -I tools/orig/fixtures.py --game C:\\anno1503-original --out cache/orig/f1f6-<tag> \\
         --hold 180 --seconds 60
"""

from __future__ import annotations

import argparse
import ctypes
import datetime
import json
import os
import struct
import subprocess
import sys
import time

# Reuse the stage-0 harness machinery (same directory; import, do not re-implement).
# `python -I` (isolated mode) drops the script's own directory from sys.path, so add it back
# explicitly to import the sibling module (our own repo tool, not an untrusted file).
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import smoke  # noqa: E402

# AnnoWelt PRNG table: u16[2048] at this RVA in AnnoWelt.dll (.rdata; tick-loop.md:70).
PRNG_TABLE_RVA = 0x000CB8F8
PRNG_TABLE_LEN = 2048
WORLD_PRNG_CURSOR_OFF = 0xFC  # u16 cursor (tick-loop.md:70)


def msvc_rand_seed1(n: int) -> list[int]:
    """The first n outputs of the MSVC CRT rand() from the default seed 1
    (x = x*214013 + 2531011 mod 2**32; out = (x >> 16) & 0x7fff). tick-loop.md:71."""
    out: list[int] = []
    x = 1
    for _ in range(n):
        x = (x * 214013 + 2531011) & 0xFFFFFFFF
        out.append((x >> 16) & 0x7FFF)
    return out


def read_u16(handle, addr: int) -> int | None:
    b = smoke.read_memory(handle, addr, 2)
    return struct.unpack_from("<H", b, 0)[0] if b else None


def read_prng_table(pid: int, welt_base: int) -> list[int] | None:
    handle = smoke.open_read(pid)
    if not handle:
        return None
    try:
        raw = smoke.read_memory(handle, welt_base + PRNG_TABLE_RVA, PRNG_TABLE_LEN * 2)
        if not raw:
            return None
        return list(struct.unpack_from("<%dH" % PRNG_TABLE_LEN, raw, 0))
    finally:
        smoke.kernel32.CloseHandle(handle)


def read_world_sample(pid: int, obj: int) -> dict | None:
    """One F1 + cursor sample: counter(+0x100), +0x108, tick(+0x10c), cursor(+0xfc)."""
    handle = smoke.open_read(pid)
    if not handle:
        return None
    try:
        fields = smoke.read_memory(handle, obj + 0x100, 0x10)
        cursor = read_u16(handle, obj + WORLD_PRNG_CURSOR_OFF)
        if fields is None:
            return None
        counter = struct.unpack_from("<Q", fields, 0)[0]
        at_108 = struct.unpack_from("<I", fields, 0x08)[0]
        tick = struct.unpack_from("<I", fields, 0x0C)[0]
        return {"counter": counter, "at_0x108": at_108, "tick": tick, "cursor": cursor}
    finally:
        smoke.kernel32.CloseHandle(handle)


def locate_world(game_dir: str, deadline: float) -> tuple[int, int, int] | None:
    """(pid, welt_base, world_obj) once a scenario is running, or None by deadline."""
    while time.time() < deadline:
        for pid in smoke.pids_under(game_dir):
            base = smoke.module_base(pid, "AnnoWelt.dll")
            if not base:
                continue
            _cands, accepted = smoke.find_world_candidates(pid, base[0])
            if accepted is not None:
                return pid, base[0], accepted
        time.sleep(2.0)
    return None


def run_f1f6(pid: int, welt_base: int, obj: int, t0: float, seconds: float, log: dict) -> None:
    """TL-F6 (PRNG table, read-only) + TL-F1 (clock rate and the tick==counter>>1 invariant)."""
    table = read_prng_table(pid, welt_base)
    expected = msvc_rand_seed1(PRNG_TABLE_LEN)
    f6: dict = {"table_rva": hex(PRNG_TABLE_RVA), "len": PRNG_TABLE_LEN}
    if table is None:
        f6["result"] = "could not read the table"
    else:
        diffs = [i for i in range(PRNG_TABLE_LEN) if table[i] != expected[i]]
        f6["matches_msvc_rand_seed1"] = not diffs
        f6["first_mismatch_index"] = diffs[0] if diffs else None
        f6["mismatch_count"] = len(diffs)
        f6["table_head"] = table[:6]
        f6["expected_head"] = expected[:6]
        f6["draws_after_cursor_0"] = [table[(0 + k) % PRNG_TABLE_LEN] for k in range(1, 6)]
        f6["draws_after_cursor_2047"] = [table[(2047 + k) % PRNG_TABLE_LEN] for k in range(1, 4)]
        f6["note"] = ("src/sim rng.hpp builds this same MSVC rand() seed-1 table, so "
                      "matches_msvc_rand_seed1 == matches the sim table")
    log["TL_F6"] = f6

    print(f"TL-F1: sampling the clock ~1/s for {seconds:.0f} s...", flush=True)
    samples: list[dict] = []
    end = time.time() + seconds
    while time.time() <= end:
        s = read_world_sample(pid, obj)
        if s is not None:
            s["real_s"] = round(time.time() - t0, 3)
            s["tick_eq_counter_shr1"] = s["tick"] == (s["counter"] >> 1)
            s["at108_eq_tick"] = s["at_0x108"] == s["tick"]
            samples.append(s)
        time.sleep(1.0)
    f1: dict = {"samples": samples, "n": len(samples)}
    if len(samples) >= 2:
        dt = samples[-1]["real_s"] - samples[0]["real_s"]
        dtick = samples[-1]["tick"] - samples[0]["tick"]
        dcounter = samples[-1]["counter"] - samples[0]["counter"]
        f1["real_seconds"] = round(dt, 3)
        f1["tick_delta"] = dtick
        f1["counter_delta_ms"] = dcounter
        f1["game_seconds_delta"] = round(dcounter / 1000.0, 3)
        f1["ticks_per_real_s"] = round(dtick / dt, 1) if dt > 0 else 0.0
        f1["implied_game_speed"] = round((dtick / dt) / 500.0, 3) if dt > 0 else 0.0
        f1["invariant_tick_eq_counter_shr1_all"] = all(s["tick_eq_counter_shr1"] for s in samples)
        f1["invariant_at108_eq_tick_all"] = all(s["at108_eq_tick"] for s in samples)
        cursors = [s["cursor"] for s in samples if s["cursor"] is not None]
        f1["cursor_first_last"] = [cursors[0], cursors[-1]] if cursors else None
        f1["cursor_advanced"] = (len(set(cursors)) > 1) if cursors else None
    log["TL_F1"] = f1
    log["step_size_note"] = ("1 Hz polling shows the clock rate and the tick==counter>>1 "
                             "invariant, not the per-frame dt (one Advance is sub-frame); per-frame "
                             "step needs a hook at Advance (0x100ae040) or a high-rate sampler.")


def run_rate(pid: int, obj: int, t0: float, seconds: float, interval: float, log: dict) -> None:
    """TL-F2 (speeds) + TL-F4 (pause): sample the clock continuously while the owner presses a
    scripted key sequence, then segment by the per-interval rate (game ms per real second). No
    real-time coordination: the speed of each segment is read from its rate plateau afterwards."""
    print(f"Rate mode: sampling the clock every {interval:.1f} s for {seconds:.0f} s. "
          "Press the key sequence now (100% -> F6 -> F7 -> F8 -> pause -> unpause, ~15 s each).",
          flush=True)
    samples: list[dict] = []
    end = time.time() + seconds
    while time.time() <= end:
        s = read_world_sample(pid, obj)
        if s is not None:
            s["real_s"] = round(time.time() - t0, 3)
            s["tick_eq_counter_shr1"] = s["tick"] == (s["counter"] >> 1)
            samples.append(s)
        time.sleep(interval)
    # Per-interval rate (game ms / real s); label by the nearest expected speed bucket.
    buckets = {0: "pause (0%)", 500: "50%", 1000: "100%", 2000: "200%", 4000: "400%"}
    segs: list[dict] = []
    for a, b in zip(samples, samples[1:]):
        dt = b["real_s"] - a["real_s"]
        if dt <= 0:
            continue
        rate = (b["counter"] - a["counter"]) / dt  # game ms per real s
        label = min(buckets, key=lambda k: abs(k - rate))
        name = buckets[label]
        if segs and segs[-1]["label"] == name:
            segs[-1]["end_s"] = b["real_s"]
            segs[-1]["rates"].append(round(rate, 1))
        else:
            segs.append({"label": name, "start_s": a["real_s"], "end_s": b["real_s"],
                         "rates": [round(rate, 1)]})
    for seg in segs:
        rs = seg.pop("rates")
        seg["mean_ms_per_s"] = round(sum(rs) / len(rs), 1)
        seg["duration_s"] = round(seg["end_s"] - seg["start_s"], 1)
    # Keep only segments that lasted a little, so single-sample transitions do not show as speeds.
    plateaus = [s for s in segs if s["duration_s"] >= 3.0]
    log["TL_F2_F4"] = {
        "n_samples": len(samples),
        "invariant_tick_eq_counter_shr1_all": all(s["tick_eq_counter_shr1"] for s in samples),
        "segments": plateaus,
        "all_segments": segs,
        "expected": "100% -> 1000, 200% -> 2000, 400% -> 4000, 50% -> 500, pause -> 0 ms/s",
        "note": "F4 (pause) is the 0 ms/s segment (the counter frozen); F2 the 2000/4000/500 ones. "
                "400% is single-player only (refused in MP).",
        "samples": samples,
    }


def main() -> int:
    ap = argparse.ArgumentParser()
    ap.add_argument("--game", required=True)
    ap.add_argument("--out", required=True)
    ap.add_argument("--launcher", default="1503Startup.exe")
    ap.add_argument("--wait", type=float, default=60.0, help="seconds to wait for the window")
    ap.add_argument("--hold", type=float, default=180.0,
                    help="seconds for the owner to reach a running scenario")
    ap.add_argument("--seconds", type=float, default=60.0, help="F1/rate sampling duration")
    ap.add_argument("--mode", choices=["f1f6", "rate"], default="f1f6",
                    help="f1f6 = clock + PRNG (first owner session); rate = continuous clock "
                         "sampling for TL-F2 (speeds) and TL-F4 (pause), segmented afterwards")
    ap.add_argument("--interval", type=float, default=0.5, help="rate-mode sample interval (s)")
    ap.add_argument("--arg", action="append", default=[])
    args = ap.parse_args()

    try:
        smoke.user32.SetProcessDpiAwarenessContext(ctypes.c_void_p(-4))
    except Exception:
        try:
            smoke.user32.SetProcessDPIAware()
        except Exception:
            pass

    os.makedirs(args.out, exist_ok=True)
    game_dir = os.path.abspath(args.game)
    exe = os.path.join(game_dir, args.launcher)
    log: dict = {
        "started": datetime.datetime.now().isoformat(timespec="seconds"),
        "game": game_dir,
        "launcher_sha256": smoke.sha256(exe),
        "fixtures": ["TL-F1", "TL-F6 (read-only)"],
    }
    proc = subprocess.Popen([exe, *args.arg], cwd=game_dir)
    log["launcher_pid"] = proc.pid
    t0 = time.time()

    # Wait for the window, then for the owner to reach a scenario (the world object appears).
    win = None
    while time.time() - t0 < args.wait:
        time.sleep(0.5)
        win = smoke.find_game_window(game_dir)
        if win:
            break
    if win is not None:
        log["window"] = f"{win[1]} [{win[2]}x{win[3]}]"
        dpi = smoke.user32.GetDpiForWindow(win[0]) or 96
        log["primary_display_scaling_percent"] = round(dpi * 100 / 96)

    print("Waiting for a running single-player scenario (unpaused, 100%)...", flush=True)
    located = locate_world(game_dir, t0 + args.wait + args.hold)
    if located is None:
        log["result"] = "world object not found (no running scenario within the hold window)"
    else:
        pid, welt_base, obj = located
        log["pid"] = pid
        log["welt_base"] = hex(welt_base)
        log["world_object"] = hex(obj)

        # A screenshot for the record (the scenario, paused state noted by the owner).
        w = smoke.find_game_window(game_dir)
        if w:
            cw, ch, bgra = smoke.capture(w[0])
            if cw and ch:
                shot = os.path.join(args.out, "scenario.png")
                smoke.write_png(shot, cw, ch, bgra)
                log["screenshot"] = {"file": shot, "size": [cw, ch],
                                     "nonblack": smoke.nonblack(bgra)}

        if args.mode == "rate":
            run_rate(pid, obj, t0, args.seconds, args.interval, log)
        else:
            run_f1f6(pid, welt_base, obj, t0, args.seconds, log)

    # Close and re-hash the copy.
    w = smoke.find_game_window(game_dir)
    if w:
        smoke.user32.PostMessageW(w[0], smoke.WM_CLOSE, 0, 0)
    for _ in range(40):
        if not smoke.pids_under(game_dir):
            break
        time.sleep(0.5)
    leftover = sorted(set(smoke.pids_under(game_dir)) | {proc.pid})
    for pid in leftover:
        if smoke.alive(pid):
            subprocess.run(["taskkill", "/T", "/F", "/PID", str(pid)], capture_output=True)
    time.sleep(1.0)
    log["left_running"] = [p for p in leftover if smoke.alive(p)]
    # Compare EVERY game .exe/.dll of the copy against game/app by SHA-256 (not just three) and
    # fail loudly on any difference; wrapper files have no game/app counterpart and are skipped.
    log["copy_diffs"] = smoke.verify_copy_unchanged(game_dir)
    log["copy_unchanged"] = not log["copy_diffs"]
    log["finished"] = datetime.datetime.now().isoformat(timespec="seconds")
    with open(os.path.join(args.out, "log.json"), "w", encoding="utf-8") as f:
        json.dump(log, f, indent=2)
    print(json.dumps(log, indent=2, default=str))
    if log["copy_diffs"]:
        print(f"ERROR: the copy differs from game/app: {log['copy_diffs']}", file=sys.stderr)
    return 0 if (located is not None and not log["copy_diffs"]) else 1


if __name__ == "__main__":
    sys.exit(main())
