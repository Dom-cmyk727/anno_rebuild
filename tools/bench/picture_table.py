#!/usr/bin/env python3
"""picture_table.py: the report tables of a bench_picture matrix, from its raw files.

    tools/bench/picture_table.py <bench_picture binary> <matrix out_dir>

For every case directory of the matrix (results.tsv order) it recomputes the latency summary from
the case's raw events (bench_picture --aggregate events.tsv), the frame-stage distributions from
frames.tsv (frames of the measured loop only), and reads the memory and verification lines of
summary.txt. Percentiles are nearest-rank with their sample counts; p95 needs 20 samples and p99
100, else the cell says how many there were (docs/perf/README.md, "Picture on a legal city
workload").
"""
import math
import subprocess
import sys
from pathlib import Path


def nearest_rank(sorted_values, p):
    if not sorted_values:
        return None
    rank = math.ceil(p * len(sorted_values))
    return sorted_values[max(0, min(len(sorted_values) - 1, rank - 1))]


def dist(values):
    v = sorted(values)
    n = len(v)
    if n == 0:
        return "n=0"
    p50 = nearest_rank(v, 0.50)
    p95 = f"{nearest_rank(v, 0.95):.3f}" if n >= 20 else f"(n={n})"
    p99 = f"{nearest_rank(v, 0.99):.3f}" if n >= 100 else f"(n={n})"
    return f"{n} / {p50:.3f} / {p95} / {p99} / {v[-1]:.3f}"


def read_frames(path):
    rows = path.read_text().splitlines()
    head = rows[0].split("\t")
    out = []
    for r in rows[1:]:
        f = r.split("\t")
        if len(f) == len(head):
            out.append(dict(zip(head, f)))
    return out


def kv(line):
    out = {}
    for tok in line.split():
        if "=" in tok:
            k, v = tok.split("=", 1)
            out[k] = v
    return out


def main():
    if len(sys.argv) != 3:
        print(__doc__, file=sys.stderr)
        return 2
    binary, matrix = sys.argv[1], Path(sys.argv[2])
    results = (matrix / "results.tsv").read_text().splitlines()[1:]
    print("| case | status | edits | object-ready ms n / p50 / p95 / p99 / max | visible CPU "
          "chunks ready ms n / p50 / p95 / p99 / max | max age ms | coalesced | offscreen | "
          "unfinished | worker build ms p50 / max |")
    print("|---|---:|---:|---|---|---:|---:|---:|---:|---|")
    stage_rows = []
    memory_rows = []
    progress_rows = []
    for line in results:
        name, status = line.split("\t")[:2]
        case = matrix / name
        # --aggregate exits 0 only for a complete trace of a successful run; anything else is shown
        # as such (and a malformed or missing trace stops the table: it is no evidence).
        done = subprocess.run([binary, "--aggregate", str(case / "events.tsv")],
                              capture_output=True, text=True, check=False)
        if done.returncode == 2:
            print(f"| {name} | {status} | trace unreadable: {done.stderr.strip()} |")
            continue
        agg = done.stdout.splitlines()
        run = agg[0]
        if done.returncode != 0:
            status = f"{status} ({run})"
        agg = agg[1:]
        first = kv(agg[0])
        lines = {l.split()[0]: l for l in agg if l and " n=" in l}

        def cells(key):
            d = kv(lines[key])
            return (f"{d['n']} / {float(d['p50_ms']):.1f} / {d['p95_ms']} / {d['p99_ms']} / "
                    f"{float(d['max_ms']):.1f}")
        age = kv([l for l in agg if l.startswith("max_age_ms")][0])
        build = kv(lines["build_time_worker"])
        print(f"| {name} | {status} | {first['edits']} | {cells('object_ready')} | "
              f"{cells('visible_cpu_chunks_ready')} | {float(age['max_age_ms']):.1f} | "
              f"{first['coalesced']} | {first['offscreen']} | {first['unfinished']} | "
              f"{float(build['p50_ms']):.1f} / {float(build['max_ms']):.1f} |")
        all_frames = read_frames(case / "frames.tsv")
        # One row per phase, in run order: the measured loop, the drain (a one-edit or burst case
        # commits there), and the pan and zoom of the offscreen case.
        for phase in dict.fromkeys(f["phase"] for f in all_frames):
            frames = [f for f in all_frames if f["phase"] == phase]
            publishing = [f for f in frames if int(f["published"]) > 0]
            committing = [f for f in frames if int(f["committed"]) > 0]
            stage_rows.append(
                f"| {name} | {phase} | {len(frames)} | "
                f"{dist([float(f['apply_list_ms']) for f in publishing])} | "
                f"{dist([float(f['reindex_ms']) for f in publishing])} | "
                f"{dist([float(f['dispatch_ms']) for f in publishing])} | "
                f"{dist([float(f['commit_ms']) for f in committing])} | "
                f"{dist([float(f['update_ms']) for f in frames])} |")
        summary = (case / "summary.txt").read_text().splitlines()
        mem = kv([l for l in summary if l.startswith("memory ")][0])
        stats = kv([l for l in summary if l.startswith("stats ")][0])
        verdict = [l for l in summary if l.startswith("VERIFY")]
        memory_rows.append(
            f"| {name} | {mem['exact_peak_reservation_bytes']} | {stats['result_bytes_peak']} | "
            f"{stats['estimate_shortfalls']} | {mem['exact_peak_incremental_payload']} | "
            f"{mem['sampled_peak_incremental_payload']} | "
            f"{mem['sampled_peak_unreferenced_payload']} | "
            f"{mem['sampled_peak_reclaim_queue_bytes']} | {mem['sampled_peak_rss']} | "
            f"{mem['sampled_peak_footprint']} | {mem['majflt_delta']} | "
            f"{mem['sys_pageouts_delta']} / {mem['sys_swapouts_delta']} | "
            f"{verdict[0] if verdict else 'none'} |")
        progress_rows.append(
            f"| {name} | {stats['committed']} | {first['intermediate_commits']} | "
            f"{stats['version_requeues']} | {stats['rebased']} | {stats['discarded']} | "
            f"{stats['coalesced']} | {stats['deferred']} |")
    print()
    print("Window stages per frame, by phase, ms: n / p50 / p95 / p99 / max (apply stages over "
          "the frames that published, commit over the frames that committed).")
    print()
    print("| case | phase | frames | apply: list | apply: reindex | apply: dispatch and copies | "
          "commit | chunk update |")
    print("|---|---|---:|---|---|---|---|---|")
    for r in stage_rows:
        print(r)
    print()
    print("Version progress: islands committed, of them intermediate (a version below the "
          "island's desired one when it was committed), results requeued, queued jobs moved to a "
          "new generation, results discarded, coalesced, requests deferred.")
    print()
    print("| case | committed | intermediate | requeued | rebased | discarded | coalesced | "
          "deferred |")
    print("|---|---:|---:|---:|---:|---:|---:|---:|")
    for r in progress_rows:
        print(r)
    print()
    print("Memory, bytes. Exact: the counter's own peak (reservation: the ledger; incremental: the "
          "payload counter over the process). Sampled: the largest of the per-frame samples, which "
          "can miss a shorter maximum. Unreferenced: live payload the current generation does not "
          "reference (replaced generations still held, results not committed yet, builds in "
          "flight), not proven retired-only.")
    print()
    print("| case | exact peak reservation | largest result | estimate shortfalls | exact peak "
          "incremental | sampled peak incremental | sampled peak unreferenced | sampled peak "
          "reclaim queue | sampled peak RSS | sampled peak footprint | major faults | host "
          "page-outs / swap-outs | oracle |")
    print("|---|---:|---:|---:|---:|---:|---:|---:|---:|---:|---:|---|---|")
    for r in memory_rows:
        print(r)
    return 0


if __name__ == "__main__":
    sys.exit(main())
