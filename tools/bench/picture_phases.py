#!/usr/bin/env python3
"""picture_phases.py: where an edit's latency accrues, per case, from a matrix's raw traces.

    tools/bench/picture_phases.py <matrix out_dir>

For every case (results.tsv order) and every edit (a desired event of an island and version) that a
commit reached, the phases from its events.tsv (round ec, A3), in milliseconds:

  dispatch   desired -> admitted, of the build that was committed (deferral and the window's apply)
  queue      admitted -> started (the build waits for a worker)
  worker     started -> built (the object build on a worker)
  observe    built -> committed (until a frame's commit takes the finished build)
  chunks     committed -> chunks_ready (the chunk rebuild, its queue, and the frame that sees it)
  upload     chunks_ready -> gpu_ready (GPU mode: uploads up to the submission of the frame that
             draws them)

An edit reached through a later version (coalesced) is attributed to that version's build. GPU
completion and presentation are not in any trace: they are not phases here. Percentiles are
nearest-rank with their sample counts (p95 needs 20 samples, p99 100). A trace the aggregate refuses
is not read here: run picture_table.py first. Exit 0, or 2 for bad arguments.
"""
import math
import sys
from pathlib import Path


def nearest_rank(v, p):
    rank = math.ceil(p * len(v))
    return v[max(0, min(len(v) - 1, rank - 1))]


def dist(values):
    v = sorted(values)
    if not v:
        return "n=0"
    p95 = f"{nearest_rank(v, 0.95):.1f}" if len(v) >= 20 else f"(n={len(v)})"
    p99 = f"{nearest_rank(v, 0.99):.1f}" if len(v) >= 100 else f"(n={len(v)})"
    return f"{len(v)} / {nearest_rank(v, 0.5):.1f} / {p95} / {p99} / {v[-1]:.1f}"


def phases(events):
    desired, out = [], {k: [] for k in ("dispatch", "queue", "worker", "observe", "chunks",
                                        "upload")}
    admitted, started, built, committed = {}, {}, {}, {}
    pending, gen_of, cpu, gpu = [], {}, {}, {}
    for t, kind, island, version in events:
        key = (island, version)
        if kind == "desired":
            desired.append((t, island, version))
        elif kind == "admitted":
            admitted.setdefault(key, t)
        elif kind == "started":
            started[key] = t  # the start of the build that finished (a retry restarts it)
        elif kind == "built":
            built[key] = t
        elif kind == "committed":
            committed.setdefault(key, t)
            pending.append(key)
        elif kind == "generation":
            for k in pending:
                gen_of[k] = version
            pending = []
        elif kind == "chunks_ready":
            cpu.setdefault(key, t)
        elif kind == "gpu_ready":
            gpu.setdefault(key, t)
    for t0, island, version in desired:
        # The first commit of this island at this version or later.
        reached = sorted((t, v) for (i, v), t in committed.items() if i == island and v >= version)
        if not reached:
            continue
        t_commit, v = reached[0]
        k = (island, v)
        if k in admitted:
            out["dispatch"].append(admitted[k] - t0)
            if k in started:
                out["queue"].append(started[k] - admitted[k])
        if k in started and k in built:
            out["worker"].append(built[k] - started[k])
        if k in built:
            out["observe"].append(t_commit - built[k])
        g = gen_of.get(k)
        if g is not None and (island, g) in cpu:
            out["chunks"].append(cpu[(island, g)] - t_commit)
            if (island, g) in gpu:
                out["upload"].append(gpu[(island, g)] - cpu[(island, g)])
    return out


def main():
    if len(sys.argv) != 2:
        print(__doc__, file=sys.stderr)
        return 2
    matrix = Path(sys.argv[1])
    rows = (matrix / "results.tsv").read_text().splitlines()[1:]
    print("| case | dispatch | queue | worker | observe | chunks | upload |")
    print("|---|---|---|---|---|---|---|")
    for row in rows:
        name = row.split("\t")[0]
        lines = (matrix / name / "events.tsv").read_text().splitlines()[1:]
        events = []
        for line in lines:
            if not line:
                continue
            t, kind, island, version = line.split("\t")
            events.append((float(t), kind, int(island), int(version)))
        events.sort(key=lambda e: e[0])
        p = phases(events)
        print(f"| {name} | " + " | ".join(dist(p[k]) for k in
                                         ("dispatch", "queue", "worker", "observe", "chunks",
                                          "upload")) + " |")
    return 0


if __name__ == "__main__":
    sys.exit(main())
