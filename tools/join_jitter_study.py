#!/usr/bin/env python3
"""Reproducible synthetic T61 retry/jitter study for the join checker.

This is not a LAN measurement.  It runs one simulation at a time and writes JSONL so a
reviewer can validate both the 72 matched primary rows and the declared diagnostics.
"""
from __future__ import annotations

import argparse
import hashlib
import json
import os
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


def horizon_lag(sim):
    return max([sim.host.applied - peer.applied for peer in sim.seated] or [0])


def maximum_lag(sim):
    applied = {"H": 0}
    greatest = 0
    for event in sim.events:
        if event["kind"] == "apply":
            applied[event["peer"]] = event["turn"]
            host = applied.get("H", 0)
            greatest = max(greatest, max([host - value for name, value in applied.items() if name != "H"] or [0]))
    return greatest


def metrics(sim):
    totals, peak_queue = Counter(), 0
    for direction in ("egress", "ingress"):
        stat = sim.link_stats[direction]
        for key in ("first_bytes", "retry_bytes", "ack_bytes", "drop_queue", "drop_loss", "bytes"):
            totals[key] += stat[key]
        peak_queue = max(peak_queue, stat["peak_bytes"])
    join = sim.host.joins.get("J1")
    return {
        "outcome": ja.sens_outcome(sim), "completion_ms": getattr(sim.joiners.get("J1"), "active_ms", None),
        "restart_ms": getattr(join, "restart_ms", None), "restart_reason": getattr(join, "restart_reason", None),
        "dropped": sorted(sim.dropped), "horizon_ms": sim.horizon, "pending_events": len(sim.q),
        "first_bytes": totals["first_bytes"], "retry_bytes": totals["retry_bytes"], "ack_bytes": totals["ack_bytes"],
        "queue_rejects": totals["drop_queue"], "loss_drops": totals["drop_loss"], "peak_queue_bytes": peak_queue,
        "peak_reassembly_fragments": sim.peak_reasm_frags, "peak_reassembly_bytes": sim.peak_reasm_bytes,
        "retained_turns": len(sim.host.log_turns), "max_seated_lag": maximum_lag(sim),
        "horizon_seated_lag": horizon_lag(sim), "validators": ja.validate_sim(sim),
    }


def run(rate, timer, row, profile, script=()):
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
        "kind": "primary" if not script else "diagnostic", "rate": rate, "resend_ms": timer,
        "workload": row, "profile": profile, "config": {
            "rate_bps": cfg.link_bps, "queue_bytes": cfg.link_queue_bytes, "jitter_min_ms": lo,
            "jitter_max_ms": hi, "jitter_seed": seed, "jitter_script": tuple(script),
        }, "metrics": result,
        "config_hash": digest((rate, timer, row, profile, tuple(script))),
        "result_hash": digest((sim.events, result)), "semantic_trace_hash": digest([
            {k: v for k, v in event.items() if k not in ("jitter_ms", "jitter_id", "jitter_source")}
            for event in sim.events]),
    }


def rows(include_diagnostics=False):
    for rate in RATES:
        for timer in TIMERS:
            for workload in ROWS:
                for profile in PROFILES:
                    yield rate, timer, workload, profile, ()
    if include_diagnostics:
        # Declared diagnostics: exact repeat and asymmetric script with a large selected delay.
        yield "100M", 300, "one_fragment", "uniform25-seed6101", ()
        yield "100M", 300, "one_fragment", "zero", ((('data', 'H', 'J1', 1, 1, 0, 1), 100),)


def main(argv=None):
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--output", required=True, help="JSONL output path")
    parser.add_argument("--diagnostics", action="store_true", help="append the two declared diagnostic rows")
    parser.add_argument("--validate", action="store_true", help="validate an existing JSONL file instead of running")
    parser.add_argument("--limit", type=int, help="run only this many rows for a smoke check; never study evidence")
    args = parser.parse_args(argv)
    if args.validate:
        data = [json.loads(line) for line in open(args.output, encoding="utf-8") if line.strip()]
        primary = [row for row in data if row["kind"] == "primary"]
        if len(primary) != 72 or any(row["metrics"]["validators"] for row in data):
            raise SystemExit("invalid study: expected 72 validator-clean primary rows")
        print("valid: %d primary rows, %d diagnostics" % (len(primary), len(data) - len(primary)))
        return 0
    started = time.monotonic()
    with open(args.output, "w", encoding="utf-8", newline="\n") as out:
        for number, (rate, timer, workload, profile, script) in enumerate(rows(args.diagnostics), 1):
            if args.limit is not None and number > args.limit:
                break
            begun = time.monotonic()
            result = run(rate, timer, workload, profile, script)
            result["wall_seconds"] = round(time.monotonic() - begun, 3)
            if result["wall_seconds"] > 300 or result["metrics"]["validators"]:
                raise SystemExit("row %d failed bounds or validation: %r" % (number, result))
            out.write(json.dumps(result, sort_keys=True, default=list) + "\n")
            out.flush()
            print("%02d %s/%d/%s/%s %.3fs" % (number, rate, timer, workload, profile, result["wall_seconds"]))
    if time.monotonic() - started > 5400:
        raise SystemExit("study exceeded the 90 minute bound")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
