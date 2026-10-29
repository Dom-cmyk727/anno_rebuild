#!/usr/bin/env python3
"""Reproducible synthetic T61 retry/jitter study for the join checker.

This is not a LAN measurement.  It runs one simulation at a time and writes JSONL so a
reviewer can validate both the 72 matched primary rows and the declared diagnostics.
"""
from __future__ import annotations

import argparse
import hashlib
import json
import math
import multiprocessing
import os
import platform
import subprocess
import sys
import time
from collections import Counter

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import join_activation as ja


PROFILES = {
    "zero": (0, 0, 0, ()),
    "uniform25-seed6101": (0, 25, 6101, ()),
    "uniform25-seed6102": (0, 25, 6102, ()),
    "uniform100-seed6101": (0, 100, 6101, ()),
}
RATES = ("100M", "1M")
TIMERS = (300, 400, 500)
ROWS = ("one_fragment", "endless02", "fragment_loss")


def digest(value):
    return hashlib.sha256(json.dumps(value, sort_keys=True, separators=(",", ":"), default=list).encode()).hexdigest()


def canon(value):
    """JSON form of a recorded or replayed object. Tuples become lists on disk."""
    return json.loads(json.dumps(value, sort_keys=True, default=list))


def committed_source_sha256(head, name):
    """SHA-256 of the committed blob. A CRLF or LF checkout hashes the same bytes."""
    root = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
    blob = subprocess.check_output(("git", "show", "%s:tools/%s" % (head, name)), cwd=root)
    return hashlib.sha256(blob).hexdigest()


def provenance():
    root = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
    try:
        head = subprocess.check_output(("git", "rev-parse", "HEAD"), cwd=root, text=True).strip()
        hashes = {name: committed_source_sha256(head, name)
                  for name in ("join_activation.py", "join_jitter_study.py")}
    except (OSError, subprocess.CalledProcessError):
        head, hashes = None, {}
    return {
        "code_head": head, "source_hashes": hashes,
        "python": sys.version.split()[0], "platform": platform.platform(),
    }


def recorded_provenance_valid(recorded):
    """Accept the recorded source or a report-only descendant, never current-file substitution."""
    head = recorded.get("code_head") if isinstance(recorded, dict) else None
    if not isinstance(head, str) or not isinstance(recorded.get("source_hashes"), dict):
        return False
    root = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
    try:
        if subprocess.run(("git", "merge-base", "--is-ancestor", head, "HEAD"), cwd=root).returncode:
            return False
        for name, want in recorded["source_hashes"].items():
            if committed_source_sha256(head, name) != want:
                return False
    except (OSError, subprocess.CalledProcessError):
        return False
    return True


def horizon_lag(sim):
    return max([sim.host.applied - peer.applied for peer in sim.seated] or [0])


def maximum_lag(sim):
    applied = {"H": 0}
    members = set()
    greatest = 0
    for event in sim.events:
        if event["kind"] == "seated":
            members.add(event["peer"])
            applied.setdefault(event["peer"], 0)
        elif event["kind"] == "input_on":
            members.add(event["peer"])
            applied[event["peer"]] = event["applied"]
        elif event["kind"] == "input_off":
            members.discard(event["peer"])
        elif event["kind"] == "drop_seated":
            members.discard(event["peer"])
        if event["kind"] == "apply":
            applied[event["peer"]] = event["turn"]
        host = applied.get("H", 0)
        greatest = max(greatest, max([host - applied.get(name, 0) for name in members] or [0]))
    return greatest


def peak_reassembly(sim):
    """Fragment and byte high-water marks from acquire and release records, not simulator fields."""
    owned = {}
    peak_frags, peak_bytes = 0, 0
    for event in sim.events:
        if event["kind"] == "reasm_acquire":
            owned[(event["src"], event["dst"], event["ch"], event["mid"], event["frag"])] = event["size"]
            peak_frags = max(peak_frags, len(owned))
            peak_bytes = max(peak_bytes, sum(owned.values()))
        elif event["kind"] == "reasm_release":
            for frag in event.get("frags", ()):
                index = frag[0] if isinstance(frag, (tuple, list)) else frag
                owned.pop((event["src"], event["dst"], event["ch"], event["mid"], index), None)
    return peak_frags, peak_bytes


def peak_retained(sim):
    held, peak_turns, peak_bytes = {}, 0, 0
    for event in sim.events:
        if event["kind"] == "retain":
            held[event["turn"]] = event["bytes"]
        elif event["kind"] == "release":
            held.pop(event["turn"], None)
        peak_turns = max(peak_turns, len(held))
        peak_bytes = max(peak_bytes, sum(held.values()))
    return peak_turns, peak_bytes


def metrics(sim):
    totals, peak_queue = Counter(), 0
    for direction in ("egress", "ingress"):
        stat = sim.link_stats[direction]
        for key in ("first_bytes", "retry_bytes", "ack_bytes", "drop_queue", "drop_loss", "bytes"):
            totals[key] += stat[key]
        peak_queue = max(peak_queue, stat["peak_bytes"])
    join = sim.host.joins.get("J1")
    retained_turns, retained_bytes = peak_retained(sim)
    reasm_frags, reasm_bytes = peak_reassembly(sim)
    return {
        "outcome": ja.sens_outcome(sim), "completion_ms": getattr(sim.joiners.get("J1"), "active_ms", None),
        "restart_ms": getattr(join, "restart_ms", None), "restart_reason": getattr(join, "restart_reason", None),
        "dropped": sorted(sim.dropped), "horizon_ms": sim.horizon,
        "pending_scheduler_events": len(sim.q),
        "pending_network_callbacks": sum(1 for _, _, _, fn, _ in sim.q if fn.__name__ in ("arrive", "take_ack")),
        "pending_wire_bytes": sum(link["occ"] for link in sim.links.values()),
        "first_bytes": totals["first_bytes"], "retry_bytes": totals["retry_bytes"], "ack_bytes": totals["ack_bytes"],
        "queue_rejects": totals["drop_queue"], "loss_drops": totals["drop_loss"], "peak_queue_bytes": peak_queue,
        "peak_reassembly_fragments": reasm_frags, "peak_reassembly_bytes": reasm_bytes,
        "retained_turns": len(sim.host.log_turns), "peak_retained_turns": retained_turns,
        "peak_retained_bytes": retained_bytes, "max_seated_lag": maximum_lag(sim),
        "horizon_seated_lag": horizon_lag(sim), "validators": ja.validate_sim(sim),
    }


def run(rate, timer, row, profile, script=(), diagnostic=None):
    lo, hi, seed, configured_script = PROFILES[profile]
    original = ja.link_cfg
    def config(selected_rate, **kw):
        return original(selected_rate, jitter_min_ms=lo, jitter_max_ms=hi, jitter_seed=seed,
                        jitter_script=tuple(configured_script) + tuple(script), **kw)
    ja.link_cfg = config
    try:
        sim = ja.SENS_ROWS[row][0](rate, timer)
    finally:
        ja.link_cfg = original
    cfg = config(rate, resend_ms=timer)
    result = metrics(sim)
    return {
        "kind": "primary" if diagnostic is None else "diagnostic", "diagnostic": diagnostic, "status": "complete",
        "rate": rate, "resend_ms": timer,
        "workload": row, "profile": profile, "script_hits": sum(
            e.get("jitter_source") == "script" for e in sim.events if e["kind"] in ("send", "ack_send")), "config": {
            "rate_bps": cfg.link_bps, "queue_bytes": cfg.link_queue_bytes, "jitter_min_ms": lo,
            "jitter_max_ms": hi, "jitter_seed": seed, "jitter_script": tuple(script),
        }, "metrics": result,
        "config_hash": digest((rate, timer, row, profile, tuple(script), cfg.link_bps, cfg.link_queue_bytes,
                               cfg.one_way_ms, sim.horizon)),
        "result_hash": digest((sim.events, result)), "semantic_trace_hash": digest([
            {k: v for k, v in event.items() if k not in ("jitter_ms", "jitter_id", "jitter_source")}
            for event in sim.events]),
    }


def rows(include_diagnostics=False):
    for rate in RATES:
        for timer in TIMERS:
            for workload in ROWS:
                for profile in PROFILES:
                    yield rate, timer, workload, profile, (), None
    if include_diagnostics:
        # Declared diagnostics: exact repeat and asymmetric script with a large selected delay.
        yield "100M", 300, "one_fragment", "uniform25-seed6101", (), "repeat"
        yield "100M", 300, "one_fragment", "zero", ((('data', 'H', 'J1', 1, 0, 0, 1), 100),), "asymmetric-data-delay"


def _worker(queue, args):
    begun = time.monotonic()
    try:
        result = run(*args)
        result["wall_seconds"] = round(time.monotonic() - begun, 3)
        queue.put(("ok", result))
    except BaseException as exc:
        queue.put(("error", {"status": "error", "error": "%s: %s" % (type(exc).__name__, exc),
                             "wall_seconds": round(time.monotonic() - begun, 3)}))


def controlled_run(args, row_timeout):
    """Run one owned simulation worker and reap only that child on expiry."""
    queue = multiprocessing.Queue(1)
    worker = multiprocessing.Process(target=_worker, args=(queue, args))
    worker.start()
    worker.join(row_timeout)
    if worker.is_alive():
        worker.terminate()
        worker.join()
        return {"status": "timeout", "wall_seconds": row_timeout}
    if queue.empty():
        return {"status": "error", "error": "worker exited without a result", "wall_seconds": row_timeout}
    state, value = queue.get()
    return value


def expected_config(rate, timer, workload, profile, script=()):
    lo, hi, seed, _ = PROFILES[profile]
    cfg = ja.link_cfg(rate, resend_ms=timer, jitter_min_ms=lo, jitter_max_ms=hi, jitter_seed=seed,
                      jitter_script=tuple(script))
    horizon = 250000 if workload == "endless02" and rate == "100M" else (650000 if workload == "endless02" else 12000)
    return digest((rate, timer, workload, profile, tuple(script), cfg.link_bps, cfg.link_queue_bytes,
                   cfg.one_way_ms, horizon))


def main(argv=None):
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--output", required=True, help="JSONL output path")
    parser.add_argument("--diagnostics", action="store_true", help="append the two declared diagnostic rows")
    parser.add_argument("--validate", action="store_true", help="validate an existing JSONL file instead of running")
    parser.add_argument("--limit", type=int, help="run only this many rows for a smoke check; never study evidence")
    parser.add_argument("--row-timeout", type=float, default=300, help="owned-worker wall limit (default: 300 s)")
    parser.add_argument("--study-timeout", type=float, default=5400, help="aggregate wall limit (default: 5400 s)")
    args = parser.parse_args(argv)
    if args.validate:
        with open(args.output, encoding="utf-8") as handle:
            data = [json.loads(line) for line in handle if line.strip()]
        if not data or data[0].get("kind") != "manifest" or len([x for x in data if x.get("kind") == "manifest"]) != 1:
            raise SystemExit("invalid study: require exactly one leading manifest")
        manifest, data = data[0], data[1:]
        if not recorded_provenance_valid(manifest.get("provenance")) or not isinstance(manifest.get("run_id"), str):
            raise SystemExit("invalid study: provenance or run identity does not match this source")
        primary = [row for row in data if row.get("kind") == "primary"]
        diagnostics = [row for row in data if row.get("kind") == "diagnostic"]
        if {row.get("diagnostic") for row in diagnostics} != {"repeat", "asymmetric-data-delay"}:
            raise SystemExit("invalid study: required diagnostics repeat and asymmetric-data-delay are absent")
        expected = {(rate, timer, workload, profile) for rate in RATES for timer in TIMERS
                    for workload in ROWS for profile in PROFILES}
        actual = {(row.get("rate"), row.get("resend_ms"), row.get("workload"), row.get("profile")) for row in primary}
        required = {"config", "config_hash", "result_hash", "semantic_trace_hash", "wall_seconds", "metrics", "status"}
        allowed_diagnostics = {"repeat", "asymmetric-data-delay"}
        if (len(primary) != 72 or actual != expected or len(diagnostics) != len(manifest.get("diagnostics", ()))
                or any(row.get("kind") not in ("primary", "diagnostic") for row in data)
                or any(not required <= set(row) or row.get("status") != "complete" for row in data)
                or any(not isinstance(row["wall_seconds"], (int, float)) or not math.isfinite(row["wall_seconds"])
                       or not 0 <= row["wall_seconds"] <= manifest["row_timeout_seconds"] for row in data)
                or any(not isinstance(row["metrics"].get("validators"), list) or row["metrics"]["validators"] for row in data)
                or any(row.get("diagnostic") not in allowed_diagnostics for row in diagnostics)
                or len({row["config_hash"] for row in primary}) != 72
                or any(row["config_hash"] != expected_config(row["rate"], row["resend_ms"], row["workload"], row["profile"],
                                                               tuple(row["config"].get("jitter_script", ()))) for row in data)):
            raise SystemExit("invalid study: require exactly the 72 distinct, validator-clean primary configurations")
        begun = time.monotonic()
        study_bound = min(args.study_timeout, manifest["study_timeout_seconds"])
        row_bound = min(args.row_timeout, manifest["row_timeout_seconds"])
        for row in data:
            remaining = study_bound - (time.monotonic() - begun)
            if remaining <= 0:
                raise SystemExit("invalid study: validation exceeded the study timeout")
            script = tuple((tuple(x[0]), x[1]) for x in row["config"].get("jitter_script", ()))
            replay = controlled_run(
                (row["rate"], row["resend_ms"], row["workload"], row["profile"], script, row.get("diagnostic")),
                min(row_bound, remaining))
            if replay.get("status") != "complete":
                raise SystemExit("invalid study: row replay %s" % replay.get("status"))
            if canon(row.get("metrics")) != canon(replay.get("metrics")):
                raise SystemExit("invalid study: recorded metrics do not match the replay")
            if canon(row.get("config")) != canon(replay.get("config")):
                raise SystemExit("invalid study: recorded config does not match the replay")
            if any(row.get(key) != replay.get(key) for key in ("result_hash", "semantic_trace_hash", "config_hash", "script_hits")):
                raise SystemExit("invalid study: retained row evidence does not reproduce")
        repeat = next(row for row in diagnostics if row.get("diagnostic") == "repeat")
        matches = [row for row in primary if (row["rate"], row["resend_ms"], row["workload"], row["profile"])
                   == (repeat["rate"], repeat["resend_ms"], repeat["workload"], repeat["profile"])]
        if len(matches) != 1 or any(repeat[key] != matches[0][key] for key in ("result_hash", "semantic_trace_hash")):
            raise SystemExit("invalid study: repeat diagnostic does not match its primary row")
        declared = [(["data", "H", "J1", 1, 0, 0, 1], 100)]
        asym = next(row for row in diagnostics if row.get("diagnostic") == "asymmetric-data-delay")
        if (canon(asym["config"].get("jitter_script")) != canon(declared)
                or asym.get("script_hits") != len(declared)):
            raise SystemExit("invalid study: asymmetric diagnostic did not cover its 100 ms script")
        print("valid: %d primary rows, %d diagnostics" % (len(primary), len(diagnostics)))
        return 0
    started = time.monotonic()
    with open(args.output, "w", encoding="utf-8", newline="\n") as out:
        manifest = {"kind": "manifest", "run_id": digest((time.time_ns(), os.getpid())), "provenance": provenance(),
                    "row_timeout_seconds": args.row_timeout, "study_timeout_seconds": args.study_timeout,
                    "diagnostics": ["repeat", "asymmetric-data-delay"] if args.diagnostics else []}
        out.write(json.dumps(manifest, sort_keys=True) + "\n")
        out.flush()
        for number, (rate, timer, workload, profile, script, diagnostic) in enumerate(rows(args.diagnostics), 1):
            if args.limit is not None and number > args.limit:
                break
            remaining = args.study_timeout - (time.monotonic() - started)
            if remaining <= 0:
                raise SystemExit("study exceeded aggregate wall bound before row %d" % number)
            result = controlled_run((rate, timer, workload, profile, script, diagnostic), min(args.row_timeout, remaining))
            result.update({"kind": "primary" if diagnostic is None else "diagnostic", "diagnostic": diagnostic,
                           "rate": rate, "resend_ms": timer, "workload": workload, "profile": profile})
            out.write(json.dumps(result, sort_keys=True, default=list) + "\n")
            out.flush()
            if result.get("status") != "complete" or result.get("metrics", {}).get("validators"):
                raise SystemExit("row %d failed bounds or validation: %r" % (number, result))
            print("%02d %s/%d/%s/%s %.3fs" % (number, rate, timer, workload, profile, result["wall_seconds"]))
    if time.monotonic() - started > args.study_timeout:
        raise SystemExit("study exceeded the 90 minute bound")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
