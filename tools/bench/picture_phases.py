#!/usr/bin/env python3
"""picture_phases.py: where an edit's latency accrues, per case, from a matrix's raw traces.

    tools/bench/picture_phases.py <bench_picture binary> <matrix out_dir>

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
nearest-rank with their sample counts (p95 needs 20 samples, p99 100).

Each case's trace is first read by the aggregate (bench_picture --aggregate, the same check as
picture_table.py): a trace that is missing, unreadable, malformed, causally contradictory or
without run_end gets a row that says why, and is never tabulated (round ec review). The trace is
then read again here, strictly: the header, four fields per line, known kinds, whole numbers, a
blank line only at the end. Exit status: 0 every case usable and successful; 1 some case's evidence
is not usable; 6 all usable but some run failed; 2 bad arguments or no results.tsv.
"""
import importlib.util
import math
import re
import sys
from pathlib import Path


def load_table():
    """picture_table.py beside this file (its aggregate check; loaded by path, so -I works)."""
    spec = importlib.util.spec_from_file_location("picture_table",
                                                  Path(__file__).with_name("picture_table.py"))
    module = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(module)
    return module


KINDS = {"desired", "admitted", "deferred", "started", "built", "failed", "superseded", "dropped",
         "requeued", "committed", "generation", "chunks_ready", "no_visible_change", "retired",
         "reclaimed", "drain_start", "drain_end", "converged", "run_end", "gpu_mode", "gpu_ready",
         "batch_published", "batch_taken", "frame_submitted", "frame_presented", "frame_failed",
         "presented"}
TIME = re.compile(r"[0-9]+(\.[0-9]+)?([eE][-+]?[0-9]+)?")
WHOLE = re.compile(r"[0-9]+")


def read_events(path, unusable):
    """The events of a trace the aggregate accepted, read strictly; `unusable` on any fault."""
    try:
        lines = path.read_text().split("\n")
    except OSError as e:
        raise unusable(f"{path.name} unreadable: {e.strerror or e}") from None
    if lines and lines[-1] == "":
        lines.pop()
    if not lines or lines[0] != "t_ms\tkind\tisland\tversion":
        raise unusable(f"{path.name} line 1: not the trace header")
    events = []
    for n, line in enumerate(lines[1:], start=2):
        f = line.split("\t")
        if len(f) != 4 or not TIME.fullmatch(f[0]) or f[1] not in KINDS or \
                not WHOLE.fullmatch(f[2]) or not WHOLE.fullmatch(f[3]):
            raise unusable(f"{path.name} line {n}: not a trace event: {line!r}")
        events.append((float(f[0]), f[1], int(f[2]), int(f[3])))
    if not events or events[-1][1] != "run_end":
        raise unusable(f"{path.name}: no run_end at its end (incomplete)")
    return events


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
    if len(sys.argv) != 3:
        print(__doc__, file=sys.stderr)
        return 2
    binary, matrix = sys.argv[1], Path(sys.argv[2])
    table = load_table()
    try:
        rows = (matrix / "results.tsv").read_text().splitlines()[1:]
    except OSError as e:
        print(f"picture_phases: {matrix / 'results.tsv'}: {e.strerror or e}", file=sys.stderr)
        return 2
    print("| case | dispatch | queue | worker | observe | chunks | upload |")
    print("|---|---|---|---|---|---|---|")
    unusable, failed = [], []
    for row in rows:
        name = row.split("\t")[0]
        trace = matrix / name / "events.tsv"
        try:
            _, succeeded = table.aggregate(binary, trace)
            events = read_events(trace, table.Unusable)
        except table.Unusable as e:
            print(f"| {name} | evidence not usable: {e} |")
            print(f"picture_phases: case {name}: evidence not usable: {e}", file=sys.stderr)
            unusable.append(name)
            continue
        if not succeeded:
            failed.append(name)
        events.sort(key=lambda e: e[0])
        p = phases(events)
        print(f"| {name}{'' if succeeded else ' (run FAILED)'} | " +
              " | ".join(dist(p[k]) for k in ("dispatch", "queue", "worker", "observe", "chunks",
                                              "upload")) + " |")
    if unusable:
        return 1
    return 6 if failed else 0


if __name__ == "__main__":
    sys.exit(main())
